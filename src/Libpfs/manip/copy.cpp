/*
 * This file is a part of LuminanceHDR package.
 * ----------------------------------------------------------------------
 * Copyright (C) 2012 Davide Anastasia
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
 */

//! \author Davide Anastasia <davideanastasia@users.sourceforge.net>

#include "copy.h"

#include "Libpfs/frame.h"
#include "Libpfs/domio.h"

#include <algorithm>

namespace pfs
{

void copy(const Array2D *from, Array2D *to)
{
    assert( from->getRows() == to->getRows() );
    assert( from->getCols() == to->getCols() );

    std::copy(from->begin(), from->end(), to->begin());
}

pfs::Frame *copy(const pfs::Frame *inFrame)
{
#ifdef TIMER_PROFILING
    msec_timer f_timer;
    f_timer.start();
#endif

    const int outWidth   = inFrame->getWidth();
    const int outHeight  = inFrame->getHeight();

    pfs::Frame *outFrame = pfs::DOMIO::createFrame(outWidth, outHeight);

    const ChannelContainer& channels = inFrame->getChannels();

    for ( ChannelContainer::const_iterator it = channels.begin();
          it != channels.end();
          ++it)
    {
        const pfs::Channel* inCh = *it;

        pfs::Channel *outCh = outFrame->createChannel(inCh->getName());

        const pfs::Array2D* inArray2D   = inCh->getChannelData();
        pfs::Array2D* outArray2D  = outCh->getChannelData();

        copy(inArray2D, outArray2D);
    }

    pfs::copyTags(inFrame, outFrame);

#ifdef TIMER_PROFILING
    f_timer.stop_and_update();
    std::cout << "pfscopy() = " << f_timer.get_time() << " msec" << std::endl;
#endif

    return outFrame;
}

}