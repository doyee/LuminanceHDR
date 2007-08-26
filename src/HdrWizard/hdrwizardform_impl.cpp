/**
 * This file is a part of Qtpfsgui package.
 * ---------------------------------------------------------------------- 
 * Copyright (C) 2006,2007 Giuseppe Rota
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * ---------------------------------------------------------------------- 
 *
 * @author Giuseppe Rota <grota@users.sourceforge.net>
 */

#include <QStringList>
#include <QFileDialog>
#include <QDir>
#include <QMessageBox>
#include <QProcess>
#include <QTextStream>

#if defined(__FreeBSD__) && __FreeBSD__ < 6
extern "C" {
#include "../arch/freebsd/s_exp2f.c"
}
#endif

#include "hdrwizardform_impl.h"
#include "../Fileformat/pfstiff.h"
#include "../Exif/exif_operations.h"
#include "../Fileformat/pfsindcraw.h"
#include "Alignment/alignmentdialog_impl.h"
#include "Alignment/mtb_alignment.h"
#include "../config.h"

const config_triple predef_confs[6]= {
{TRIANGULAR, GAMMA, DEBEVEC,""},
{TRIANGULAR, LINEAR,DEBEVEC,""},
{PLATEAU, LINEAR,DEBEVEC,""},
{PLATEAU, GAMMA,DEBEVEC,""},
{GAUSSIAN, LINEAR,DEBEVEC,""},
{GAUSSIAN, GAMMA,DEBEVEC,""},
};


HdrWizardForm::HdrWizardForm(QWidget *p, qtpfsgui_opts *options) : QDialog(p), curvefilename(""), expotimes(NULL), /*ldr_tiff(false),*/ opts(options), settings("Qtpfsgui", "Qtpfsgui") {
	setupUi(this);
	connect(Next_Finishbutton,SIGNAL(clicked()),this,SLOT(nextpressed()));
	connect(backbutton,SIGNAL(clicked()),this,SLOT(backpressed()));
	connect(pagestack,SIGNAL(currentChanged(int)),this,SLOT(currentPageChangedInto(int)));

	connect(checkbox_load_from_file,SIGNAL(toggled(bool)),this,SLOT(update_current_config_file_or_notfile(bool)));
	connect(comboBox_model,SIGNAL(activated(int)), this,SLOT(update_current_config_model(int)));
	connect(comboBox_gamma_lin_log,SIGNAL(activated(int)), this,SLOT(update_current_config_gamma_lin_log(int)));
	connect(comboBox_tri_gau_plateau,SIGNAL(activated(int)), this,SLOT(update_current_config_weights(int)));
	connect(comboBox_PredefConfigs,SIGNAL(activated(int)), this,SLOT(update_currentconfig(int)));
	connect(loadFileButton,SIGNAL(clicked()),this,SLOT(load_response_curve_from_file()));
	connect(loadsetButton,SIGNAL(clicked()),this,SLOT(loadfiles()));
	connect(radioButton_use_Predefined,SIGNAL(clicked()),this,SLOT(fix_gui_custom_config()));
	connect(checkBoxCallCustom,SIGNAL(toggled(bool)),this,SLOT(custom_toggled(bool)));
	connect(coboxRespCurve_Antighost,SIGNAL(activated(int)),this,SLOT(update_current_antighost_curve(int)));
	connect(radio_usecalibrate_02,SIGNAL(toggled(bool)),this,SLOT(update_current_config_calibrate()));
	connect(ShowFileloadedLineEdit,SIGNAL(textChanged(const QString&)), this, SLOT(setLoadFilename(const QString&)));

	connect(EVcomboBox, SIGNAL(activated(int)), this, SLOT(EVcomboBoxactivated(int)));
	connect(EVcomboBox, SIGNAL(highlighted(int)), this, SLOT(highlighted(int)));

	connect(listShowFiles, SIGNAL(currentRowChanged(int)), this, SLOT(fileselected(int)));
	weights_in_gui[0]=TRIANGULAR;
	weights_in_gui[1]=GAUSSIAN;
	weights_in_gui[2]=PLATEAU;
	responses_in_gui[0]=GAMMA;
	responses_in_gui[1]=LINEAR;
	responses_in_gui[2]=LOG10;
	responses_in_gui[3]=FROM_ROBERTSON;
	models_in_gui[0]=DEBEVEC;
	models_in_gui[1]=ROBERTSON;

	chosen_config=predef_confs[0];
	fillEVcombobox();
	need_to_transform_indices=false;
	enable_usability_jump_hack=true;
}

void HdrWizardForm::highlighted(int) {
	if (enable_usability_jump_hack)
		(EVcomboBox->view())->scrollTo((EVcomboBox->model())->index(29,0));
	enable_usability_jump_hack=false;
}

void HdrWizardForm::loadfiles() {
    QString filetypes;
    filetypes += tr("All formats (*.jpeg *.jpg *.tiff *.tif *.crw *.cr2 *.nef *.dng *.mrw *.orf *.kdc *.dcr *.arw *.raf *.ptx *.pef *.x3f *.raw);;");
    filetypes += tr("JPEG (*.jpeg *.jpg);;");
    filetypes += tr("TIFF Images (*.tiff *.tif);;");
    filetypes += tr("RAW Images (*.crw *.cr2 *.nef *.dng *.mrw *.orf *.kdc *.dcr *.arw *.raf *.ptx *.pef *.x3f *.raw)");
    QString RecentDirInputLDRs = settings.value(KEY_RECENT_PATH_LOAD_LDRs_FOR_HDR, QDir::currentPath()).toString();
    QStringList files = QFileDialog::getOpenFileNames(this, tr("Select the input images"), RecentDirInputLDRs, filetypes );
    if (!files.isEmpty() ) {
	//QStringList passed to alignment dialog
	fileStringList=files;
	QFileInfo qfi(files.at(0));
	// if the new dir, the one just chosen by the user, is different from the one stored in the settings, update the settings.
	if (RecentDirInputLDRs != qfi.path()) {
		// update internal field variable
		RecentDirInputLDRs=qfi.path();
		settings.setValue(KEY_RECENT_PATH_LOAD_LDRs_FOR_HDR, RecentDirInputLDRs);
	}

	numberinputfiles=files.count();
	progressBar->setMaximum(numberinputfiles);
	progressBar->setValue(0);
	expotimes=new float[numberinputfiles];
	int index_expotimes=0;
	QStringList::Iterator files_stringlistiterator = files.begin();
	
	int previous_sizex=-1; int previous_sizey=-1;
	int current_sizex=-1; int current_sizey=-1;
	while( files_stringlistiterator != files.end() ) {
		QFileInfo qfi(*files_stringlistiterator);

		//fill array of exposure times, -1 is error
		expotimes[index_expotimes]=ExifOperations::obtain_expotime(qfi.filePath().toStdString());

		listShowFiles->addItem(qfi.fileName()); //fill graphical list
		QString extension=qfi.suffix().toUpper(); //get filename extension

		//now go and fill the list of image data (real payload)
		// check for extension: if JPEG:
		if (extension.startsWith("JP")) {
			QImage *newimage=new QImage(qfi.filePath());
			current_sizex=newimage->width();
			current_sizey=newimage->height();
			// fill with image data
			ImagePtrList.append( newimage );
			ldr_tiff_input.append(false);
		//if tiff
		} else if(extension.startsWith("TIF")) {
			TiffReader reader(qfi.filePath().toUtf8().constData());
			//if 8bit ldr tiff
			if (reader.is8bitTiff()) {
				QImage *newimage=reader.readIntoQImage();
				current_sizex=newimage->width();
				current_sizey=newimage->height();
				ImagePtrList.append( newimage );
				ldr_tiff_input.append(true);
			//if 16bit (tiff) treat as hdr
			} else if (reader.is16bitTiff()) {
				pfs::Frame *framepointer=reader.readIntoPfsFrame();
				pfs::Channel *R, *G, *B;
				framepointer->getRGBChannels( R, G, B );
				listhdrR.push_back(R);
				listhdrG.push_back(G);
				listhdrB.push_back(B);
				current_sizex=R->getWidth();
				current_sizey=R->getHeight();
			//error if unknown tiff type
			} else {
				listShowFiles->clear();
				progressBar->setValue(0);
				clearlists();
				delete expotimes; expotimes=NULL;
				QMessageBox::critical(this,tr("Tiff error"), QString(tr("The file<br>%1<br> is not a 8 bit or 16 bit tiff")).arg(qfi.fileName()));
				return;
			}
		//not a jpeg of tiff file, so it's raw input (hdr)
		} else {
			pfs::Frame *framepointer=readRAWfile(qfi.filePath().toUtf8().constData(), & opts->dcraw_options);
			pfs::Channel *R, *G, *B;
			framepointer->getRGBChannels( R, G, B );
			listhdrR.push_back(R);
			listhdrG.push_back(G);
			listhdrB.push_back(B);
			current_sizex=R->getWidth();
			current_sizey=R->getHeight();
		}
		//check if the image has the same size as the one before
		if (!check_same_size(previous_sizex,previous_sizey,current_sizex,current_sizey)) {
			QMessageBox::critical(this,tr("Error..."), QString(tr("All the images must have the same size.")));
			listShowFiles->clear();
			progressBar->setValue(0);
			clearlists();
			delete expotimes; expotimes=NULL;
			return;
		}
		files_stringlistiterator++;
		index_expotimes++;
		progressBar->setValue(progressBar->value()+1); // increment progressbar
	}

	//check if at least one image doesn't contain (the required) exif tags
	bool all_ok=true;
	QStringList files_lacking_exif;
	for (int i=0; i<numberinputfiles; i++) {
		if (expotimes[i]==-1) {
			files_lacking_exif+="<li><font color=\"#FF0000\"><i><b>"+ listShowFiles->item(i)->text()+ "</b></i></font></li>\n";
			all_ok=false;
		}
	}

	if (all_ok) {
		Next_Finishbutton->setEnabled(TRUE);
		confirmloadlabel->setText(tr("<center><font color=\"#008400\"><h3><b>Images Loaded.</b></h3></font></center>"));
	} else {
		//if we didn't find the exif data for at least one image, we need to set all the values to -1, so that the user has to fill *all* the values by hand.
		for (int j=0;j<numberinputfiles;j++) {
			expotimes[j]=-1;
		}
		EVcomboBox->setEnabled(TRUE);
		label_EV->setEnabled(TRUE);
		QString warning_message=(QString(tr("<font color=\"#FF0000\"><h3><b>WARNING:</b></h3></font>\
Qtpfsgui was not able to find the relevant <i>EXIF</i> tags\nfor the following images:\n <ul>\
%1</ul>\
<hr>You can still proceed creating an Hdr. To do so you have to insert <b>manually</b> the EV (exposure values) or stop difference values for each one of your images.\
<hr>If you want Qtfsgui to do this <b>automatically</b>, you have to load images that have at least\nthe following exif data: \
<ul><li>Shutter Speed (seconds)</li>\
<li>Aperture (f-number)</li></ul>\
<hr><b>HINT:</b> Losing EXIF data usually happens when you preprocess your pictures.<br>\
You can perform a <b>one-to-one copy of the exif data</b> between two sets of images via the <i><b>\"Tools->Copy Exif Data...\"</b></i> menu item."))).arg(files_lacking_exif.join(""));
		QMessageBox::warning(this,tr("EXIF data not found"),warning_message);
		confirmloadlabel->setText(QString(tr("<center><h3><b>To proceed you need to manually set the exposure values.<br><font color=\"#FF0000\">%1</font> values still required.</b></h3></center>")).arg(numberinputfiles));
	}
	loadsetButton->setEnabled(FALSE);
	if (ImagePtrList.size() >= 2) {
		alignCheckBox->setEnabled(true);
		alignGroupBox->setEnabled(true);
	}
    }
}

bool HdrWizardForm::check_same_size(int &px,int &py,int cx,int cy) {
	if (px==-1||py==-1) {
		px=cx; py=cy;
		return true;
	} else {
		if (px==cx && py==cy)
			return true;
		else
			return false;
	}
}

void HdrWizardForm::custom_toggled(bool want_custom) {
	if (!want_custom) {
		if (!checkBoxAntighosting->isChecked()) {
			label_RespCurve_Antighost->setDisabled(TRUE);
			coboxRespCurve_Antighost->setDisabled(TRUE);
			label_Iterations->setDisabled(TRUE);
			spinBoxIterations->setDisabled(TRUE);
		}
		else {
			label_predef_configs->setDisabled(TRUE);
			comboBox_PredefConfigs->setDisabled(TRUE);
			label_weights->setDisabled(TRUE);
			lineEdit_showWeight->setDisabled(TRUE);
			label_resp->setDisabled(TRUE);
			lineEdit_show_resp->setDisabled(TRUE);
			label_model->setDisabled(TRUE);
			lineEdit_showmodel->setDisabled(TRUE);
		}
		update_currentconfig(comboBox_PredefConfigs->currentIndex());
		Next_Finishbutton->setText(tr("&Finish"));
	} else {
		Next_Finishbutton->setText(tr("&Next >"));
	}
}

void HdrWizardForm::fix_gui_custom_config() {
    //ENABLE load_curve_button and lineedit when "load from file" is checked.
    if (!checkbox_load_from_file->isChecked()) {
	//qDebug("not checked, disabling Load Button");
	loadFileButton->setEnabled(false);
	ShowFileloadedLineEdit->setEnabled(false);
    }
    update_current_config_file_or_notfile(checkbox_load_from_file->isChecked());
}

void HdrWizardForm::update_current_config_file_or_notfile( bool checkedfile ) {
    //if checkbox is checked AND we have a valid filename
    if (checkedfile && curvefilename != "") {
	//update chosen config
	chosen_config.response_curve=FROM_FILE;
	chosen_config.CurveFilename=curvefilename;
	//and ENABLE nextbutton
	Next_Finishbutton->setEnabled(true);
    }
    //if checkbox is checked AND no valid filename
    else  if (checkedfile && curvefilename == "") {
	// DISABLE nextbutton until situation is fixed
	Next_Finishbutton->setEnabled(false);
    }
    //checkbox not checked
    else {
	// update chosen config
	chosen_config.response_curve=responses_in_gui[comboBox_gamma_lin_log->currentIndex()];
	chosen_config.CurveFilename="";
	//and ENABLE nextbutton
	Next_Finishbutton->setEnabled(true);
    }
}

void HdrWizardForm::currentPageChangedInto(int newindex) {
	//predefined configs page
	if (newindex==1) {
		Next_Finishbutton->setText(tr("&Finish"));
		//when at least 2 LDR inputs
		if (ImagePtrList.size()>=2) {
			//manual alignment always done.
			//here ImagePtrList CAN already contain the pre-aligned LDR images.
			this->setDisabled(true);
			AlignmentDialog *alignmentdialog= new AlignmentDialog(0,ImagePtrList,ldr_tiff_input,fileStringList,(Qt::ToolButtonStyle)settings.value(KEY_TOOLBAR_MODE,Qt::ToolButtonTextUnderIcon).toInt());
			if (alignmentdialog->exec() == QDialog::Accepted) {
				this->setDisabled(false);
			} else {
				reject();
			}
			delete alignmentdialog;
		}
	}
	else if (newindex==2) { //custom config
		update_currentconfig(0);
		backbutton->setEnabled(TRUE);
		Next_Finishbutton->setText(tr("&Finish"));
		return;
	}
}

void HdrWizardForm::nextpressed() {
	int currentpage=pagestack->currentIndex();
	switch (currentpage) {
	case 0:
		if (need_to_transform_indices)
			transform_indices_into_values();
		//now align, if requested
		if (alignCheckBox->isChecked()) {
			QApplication::setOverrideCursor(QCursor(Qt::BusyCursor));
			confirmloadlabel->setText(tr("<center><h3><b>Aligning...</b></h3></center>"));
			QStringList env;
			Next_Finishbutton->setDisabled(TRUE);
			switch (alignmentEngineCB->currentIndex()) {
				case 0: //Hugin's align_image_stack
				ais=new QProcess(0);
				ais->setWorkingDirectory(opts->tempfilespath);
				env = QProcess::systemEnvironment();
				env.replaceInStrings(QRegExp("^PATH=(.*)", Qt::CaseInsensitive), "PATH=\\1;"+QCoreApplication::applicationDirPath());
				ais->setEnvironment(env);
				connect(ais, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(ais_finished(int,QProcess::ExitStatus)));
				connect(ais, SIGNAL(error(QProcess::ProcessError)), this, SLOT(ais_failed(QProcess::ProcessError)));
				ais->start("align_image_stack",QStringList() << QString("-a") << fileStringList);
				break;
				case 1:
				mtb_alignment(ImagePtrList,ldr_tiff_input);
				Next_Finishbutton->setEnabled(TRUE);
				QApplication::restoreOverrideCursor();
				pagestack->setCurrentIndex(1);
				break;
			}
			return;
		}
		pagestack->setCurrentIndex(1);
		break;
	case 1:
		if(!checkBoxCallCustom->isChecked()) {
			currentpage=2;
		} else {
			pagestack->setCurrentIndex(2);
			break;
		}
	case 2:
		QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
		//CREATE THE HDR
		if (ImagePtrList.size() != 0)
			PfsFrameHDR=createHDR(expotimes,&chosen_config,checkBoxAntighosting->isChecked(),spinBoxIterations->value(),true,&ImagePtrList);
		else
			PfsFrameHDR=createHDR(expotimes,&chosen_config,checkBoxAntighosting->isChecked(),spinBoxIterations->value(),false,&listhdrR,&listhdrG,&listhdrB);
		QApplication::restoreOverrideCursor();
		accept();
		return;
	}
}

void HdrWizardForm::update_current_antighost_curve(int fromgui) {
	update_current_config_gamma_lin_log(fromgui);
}

void HdrWizardForm::backpressed() {
	if (pagestack->currentIndex()==2 && !checkBoxCallCustom->isChecked())
		pagestack->setCurrentIndex(1);
}

void HdrWizardForm::load_response_curve_from_file() {
	curvefilename = QFileDialog::getOpenFileName(
			this,
			tr("Load a camera response curve file"),
			QDir::currentPath(),
			tr("Camera response curve (*.m);;All Files (*)") );
	if (curvefilename !="")  {
		ShowFileloadedLineEdit->setText(curvefilename);
		update_current_config_file_or_notfile(checkbox_load_from_file->isChecked());
	}
}

void HdrWizardForm::update_currentconfig( int index_from_gui ) {
// 	qDebug("updating config to %d", index_from_gui);
	chosen_config=predef_confs[index_from_gui];
	lineEdit_showWeight->setText(getQStringFromConfig(1));
	lineEdit_show_resp->setText(getQStringFromConfig(2));
	lineEdit_showmodel->setText(getQStringFromConfig(3));
}

void HdrWizardForm::update_current_config_weights(int from_gui) {
	chosen_config.weights=weights_in_gui[from_gui];
}

void HdrWizardForm::update_current_config_gamma_lin_log(int from_gui) {
	chosen_config.response_curve=responses_in_gui[from_gui];
}

void HdrWizardForm::update_current_config_model(int from_gui) {
	chosen_config.model=models_in_gui[from_gui];
}

void HdrWizardForm::update_current_config_calibrate() {
	chosen_config.response_curve=FROM_ROBERTSON;;
}

void HdrWizardForm::setLoadFilename( const QString & filename_from_gui) {
	if (filename_from_gui!="") {
		chosen_config.response_curve=FROM_FILE;
		chosen_config.CurveFilename=filename_from_gui;
	}
}

pfs::Frame* HdrWizardForm::getPfsFrameHDR() {
	return PfsFrameHDR;
}

QString HdrWizardForm::getCaptionTEXT() {
	return tr("(*) Weights: ")+getQStringFromConfig(1) + tr(" - Response curve: ") + getQStringFromConfig(2) + tr(" - Model: ") + getQStringFromConfig(3);
}

QString HdrWizardForm::getQStringFromConfig( int type ) {
    if (type==1) { //return String for weights
	switch (chosen_config.weights) {
	case TRIANGULAR:
	    return tr("Triangular");
	case PLATEAU:
	    return tr("Plateau");
	case GAUSSIAN:
	    return tr("Gaussian");
	}
    } else if (type==2) {   //return String for response curve
	switch (chosen_config.response_curve) {
	case LINEAR:
	    return tr("Linear");
	case GAMMA:
	    return tr("Gamma");
	case LOG10:
	    return tr("Logarithmic");
	case FROM_ROBERTSON:
	    return tr("From Calibration");
	case FROM_FILE:
	    return tr("From File");
	}
    } else if (type==3) {   //return String for model
	switch (chosen_config.model) {
	case DEBEVEC:
	    return tr("Debevec");
	case ROBERTSON:
	    return tr("Robertson");
	}
    } else return "";
return "";
}

void HdrWizardForm::fillEVcombobox() {
	for (int i=-6*4;i<=6*4;i++){
		float ev_value=-100;
		QString stop_string="";
		switch (abs(i%4)) {
			case 0:
			ev_value=0;
			stop_string=((i>0)? "+":"") + QString("%1").arg((i/4));
			break;
			case 1:
			ev_value= (i>0)? +0.3 : -0.3;
			stop_string=((i>0)? "+":"") + QString("%1/3").arg(( (i/4)*3 + ((i>0)? +1:-1) ));
			break;
			case 2:
			ev_value= (i>0)? +0.5 : -0.5;
			stop_string=((i>0)? "+":"") + QString("%1/2").arg(( (i/4)*2 + ((i>0)? +1:-1) ));
			break;
			case 3:
			ev_value= (i>0)? +0.7 : -0.7;
			stop_string=((i>0)? "+":"") + QString("%1/3").arg(( (i/4)*3 + ((i>0)? +2:-2) ));
			break;
		}
		ev_value+=i/4;
		QString result;
		QTextStream ts(&result);
		ts << "EV "<< qSetFieldWidth(4) << left << forcesign << ev_value << " || stops " << qSetFieldWidth(4) << forcesign << left << stop_string;
		EVcomboBox->addItem(result);
	}
	EVcomboBox->setCurrentIndex(-1);
}

void HdrWizardForm::transform_indices_into_values() {
	qDebug("HdrWizardForm:: transforming indices into values");
//for precise values I cannot parse back what I put in the combobox, I have to recompute the values. In other words 4/3 != 1.3
	for (int i=0; i<numberinputfiles; i++) {
		int ni=(int)expotimes[i]-(6*4);
		float v=-100;
		switch (abs(ni%4)) {
			case 0:
				v=ni/4;
				break;
			case 1:
				v=float((ni/4)*3 + ((ni>0)? +1:-1)) / 3.0f;
				break;
			case 2:
				v=float((ni/4)*2 + ((ni>0)? +1:-1)) / 2.0f;
				break;
			case 3:
				v=float((ni/4)*3 + ((ni>0)? +2:-2)) / 3.0f;
				break;
		}
		assert(v!=-100);
		expotimes[i]=exp2f(v);
	}
}

void HdrWizardForm::EVcomboBoxactivated(int i) {
	enable_usability_jump_hack=true;
	assert(listShowFiles->count()==numberinputfiles);
	//for the time being expotimes contains the indices into the combobox (from 0 to 48)
	expotimes[listShowFiles->currentRow()]=i;
// 	qDebug("setting expotimes[%d]=%d",listShowFiles->currentRow(),i);
	bool all_ok=true;
	int files_unspecified=0;
	for (int i=0; i<numberinputfiles; i++) {
		if (expotimes[i]==-1) {
			all_ok=false;
			files_unspecified++;
		}
	}
	if (all_ok) {
		Next_Finishbutton->setEnabled(TRUE);
		confirmloadlabel->setText(tr("<center><font color=\"#008400\"><h3><b>All values have been set!</b></h3></font></center>"));
		need_to_transform_indices=true;
	} else {
		confirmloadlabel->setText( QString(tr("<center><h3><b>To proceed you need to manually set the exposure values.<br><font color=\"#FF0000\">%1</font> values still required.</b></h3></center>")).arg(files_unspecified) );
	}
}

void HdrWizardForm::fileselected(int i) {
	assert(listShowFiles->count()==numberinputfiles);
	//right now expotimes contains the indices into the combobox (from 0 to 48)
	EVcomboBox->setCurrentIndex((int)expotimes[i]); //works with -1 (value not set) as well.
}

HdrWizardForm::~HdrWizardForm() {
	qDebug("~HdrWizardForm");
	// here PfsFrameHDR is not free-ed because we want to get it outside of this class via the getPfsFrameHDR() method.
	if (expotimes) delete [] expotimes;
	clearlists();
}

void HdrWizardForm::clearlists() {
	if (ImagePtrList.size() != 0) {
		qDebug("HdrWizardForm::clearlists: cleaning LDR exposures list");
		for(int i=0;i<ImagePtrList.size();i++) {
			if (ldr_tiff_input[i]) {
				qDebug("HdrWizardForm::clearlists: freeing ldr tiffs' payload.");
				delete [] ImagePtrList[i]->bits();
			}
		}
		qDeleteAll(ImagePtrList);
		ImagePtrList.clear();
		ldr_tiff_input.clear();
	}
	if (listhdrR.size()!=0 && listhdrG.size()!=0 && listhdrB.size()!=0) {
// 		qDebug("cleaning HDR exposures list");
		Array2DList::iterator itR=listhdrR.begin(), itG=listhdrG.begin(), itB=listhdrB.begin();
		for (; itR!=listhdrR.end(); itR++,itG++,itB++ ){
			delete *itR; delete *itG; delete *itB;
		}
		listhdrR.clear(); listhdrG.clear(); listhdrB.clear();
	}
}

void HdrWizardForm::ais_finished(int exitcode, QProcess::ExitStatus) {
	if (exitcode==QProcess::CrashExit) {
		ais_failed(QProcess::Crashed);
		return;
	}
	if (exitcode==0 && pagestack->currentIndex()==0) {
		qDebug("align_image_stack successfully terminated");
		clearlists();
		for (int i=0;i<numberinputfiles;i++) {
			//align_image_stack can only output tiff files
			const char* fname=QString(opts->tempfilespath + "/aligned_" + QString("%1").arg(i,4,10,QChar('0'))+".tif").toUtf8().constData();
			qDebug("Loading back file name=%s", fname);
			TiffReader reader(fname);
			ImagePtrList.append( reader.readIntoQImage() );
			QFile::remove(fname);
		}
		QFile::remove(QString(opts->tempfilespath + "/hugin_debug_optim_results.txt"));
		QApplication::restoreOverrideCursor();
		Next_Finishbutton->setEnabled(TRUE);
		pagestack->setCurrentIndex(1);
	}
}

void HdrWizardForm::ais_failed(QProcess::ProcessError e) {
	switch (e) {
	case QProcess::FailedToStart:
		QMessageBox::warning(this,tr("Error..."),tr("Failed to start the external process \"<em>align_image_stack</em>\". Read the webpage <a href=\"http://qtpfsgui.wiki.sourceforge.net/align_image_stack\">http://qtpfsgui.wiki.sourceforge.net/align_image_stack</a> for more information."));
	break;
	case QProcess::Crashed:
		QMessageBox::warning(this,tr("Error..."),tr("The external process \"<em>align_image_stack</em>\" crashed..."));
	break;
	case QProcess::Timedout:
	case QProcess::ReadError:
	case QProcess::WriteError:
	case QProcess::UnknownError:
		QMessageBox::warning(this,tr("Error..."),tr("An unknown error occurred while executing the \"<em>align_image_stack</em>\" process..."));
	break;
	}
	QApplication::restoreOverrideCursor();
	Next_Finishbutton->setEnabled(TRUE);
	if (pagestack->currentIndex()==0)
		pagestack->setCurrentIndex(1);
}
