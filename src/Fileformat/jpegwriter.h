/**
 * This file is a part of Luminance HDR package.
 * ----------------------------------------------------------------------
 * Copyright (C) 2012 Franco Comida, Davide Anastasia
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
 * @author Franco Comida <fcomida@users.sourceforge.net>
 * Original work
 * @author Davide Anastasia <davideanastasia@users.sourceforge.net>
 * clean up memory management
 *
 */

#ifndef JPEGWRITER_H
#define JPEGWRITER_H

#include <QObject>
#include <QImage>
#include <QString>

class JpegWriter : public QObject
{
    Q_OBJECT

public:
	JpegWriter(const QImage *, QString, int);
	JpegWriter(const QImage *, int);
	~JpegWriter() {}

    //! \brief write \c QImage into Jpeg file
	bool writeQImageToJpeg();

    //! \brief return size in bytes of the file written
	int getFileSize();

private:
    const QImage *m_out_qimage;
    QString m_fname;
    int m_filesize;
    int m_quality;

};

#endif
