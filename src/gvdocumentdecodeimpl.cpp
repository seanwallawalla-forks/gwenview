// vim: set tabstop=4 shiftwidth=4 noexpandtab
/*
Gwenview - A simple image viewer for KDE
Copyright 2000-2004 Aur�lien G�teau
 
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
 
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
 
You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 
*/

// Qt
#include <qfile.h>
#include <qmemarray.h>
#include <qimage.h>
#include <qstring.h>
#include <qtimer.h>

// KDE
#include <kdebug.h>
#include <kio/netaccess.h>
#include <kurl.h>

// Local
#include "gvdocumentloadedimpl.h"
#include "gvdocumentjpegloadedimpl.h"
#include "gvdocumentdecodeimpl.moc"


const int DECODE_CHUNK_SIZE=4096;


//---------------------------------------------------------------------
//
// GVDocumentDecodeImplPrivate
//
//---------------------------------------------------------------------
class GVDocumentDecodeImplPrivate {
public:
	GVDocumentDecodeImplPrivate(GVDocumentDecodeImpl* impl)
	: mDecoder(impl) {}

	bool mUpdatedDuringLoad;
	QString mTempFilePath;
	QByteArray mRawData;
	int mReadSize;
	QImageDecoder mDecoder;
	QTimer mDecoderTimer;
};


//---------------------------------------------------------------------
//
// GVDocumentDecodeImpl
//
//---------------------------------------------------------------------
GVDocumentDecodeImpl::GVDocumentDecodeImpl(GVDocument* document) 
: GVDocumentImpl(document) {
	kdDebug() << k_funcinfo << endl;
	d=new GVDocumentDecodeImplPrivate(this);
	d->mUpdatedDuringLoad=false;

	connect(&d->mDecoderTimer, SIGNAL(timeout()), this, SLOT(loadChunk()) );
	
	QTimer::singleShot(0, this, SLOT(startLoading()) );
}


GVDocumentDecodeImpl::~GVDocumentDecodeImpl() {
	delete d;
}

void GVDocumentDecodeImpl::startLoading() {
	// FIXME: Incremental download
	if (!KIO::NetAccess::download(mDocument->url(), d->mTempFilePath)) {
		emit finished(false);
		return;
	}
	
	const char* format=QImage::imageFormat(d->mTempFilePath);
	setImageFormat(format);
	if (!format) {
		emit finished(false);
		KIO::NetAccess::removeTempFile(d->mTempFilePath);
		return;
	}
	
	// FIXME: Read file in chunks too
	QFile file(d->mTempFilePath);
	file.open(IO_ReadOnly);
	QDataStream stream(&file);
	d->mRawData.resize(file.size());
	stream.readRawBytes(d->mRawData.data(),d->mRawData.size());
	d->mReadSize=0;

	d->mDecoderTimer.start(0, false);
}


void GVDocumentDecodeImpl::loadChunk() {
	int decodedSize=d->mDecoder.decode(
		(const uchar*)(d->mRawData.data()+d->mReadSize),
		QMIN(DECODE_CHUNK_SIZE, int(d->mRawData.size())-d->mReadSize));

	// Continue loading
	if (decodedSize>0) {
		d->mReadSize+=decodedSize;
		return;
	}

	// Loading finished
	bool ok=decodedSize==0;
	// If async loading failed, try synchronous loading
	QImage image;
	if (!ok) {
		kdDebug() << k_funcinfo << " async loading failed, trying sync loading\n";
		ok=image.loadFromData(d->mRawData);
		d->mUpdatedDuringLoad=false;
	} else {
		image=d->mDecoder.image();
	}
	
	// Image can't be loaded, let's switch to an empty implementation
	if (!ok) {
		kdDebug() << k_funcinfo << " loading failed\n";
		KIO::NetAccess::removeTempFile(d->mTempFilePath);
		emit finished(false);
		switchToImpl(new GVDocumentImpl(mDocument));
		return;
	}

	kdDebug() << k_funcinfo << " loading succeded\n";

	// Convert depth if necessary
	// (32 bit depth is necessary for alpha-blending)
	if (image.depth()<32 && image.hasAlphaBuffer()) {
		image=image.convertDepth(32);
		d->mUpdatedDuringLoad=false;
	}

	// The decoder did not cause the sizeUpdated or rectUpdated signals to be
	// emitted, let's do it now
	if (!d->mUpdatedDuringLoad) {
		setImage(image);
		emit sizeUpdated(image.width(), image.height());
		emit rectUpdated( QRect(QPoint(0,0), image.size()) );
	}
	
	// Now we switch to a loaded implementation
	QCString format(mDocument->imageFormat());
	if (format=="JPEG") {
		switchToImpl(new GVDocumentJPEGLoadedImpl(mDocument, d->mRawData, d->mTempFilePath));
	} else {
		KIO::NetAccess::removeTempFile(d->mTempFilePath);
		switchToImpl(new GVDocumentLoadedImpl(mDocument));
	}
}


void GVDocumentDecodeImpl::suspendLoading() {
	d->mDecoderTimer.stop();
}

void GVDocumentDecodeImpl::resumeLoading() {
	d->mDecoderTimer.start(0, false);
}


//---------------------------------------------------------------------
//
// QImageConsumer
//
//---------------------------------------------------------------------
void GVDocumentDecodeImpl::end() {
	kdDebug() << k_funcinfo << endl;
}

void GVDocumentDecodeImpl::changed(const QRect& rect) {
	kdDebug() << k_funcinfo << " " << rect.left() << "-" << rect.top() << " " << rect.width() << "x" << rect.height() << endl;
	if (!d->mUpdatedDuringLoad) {
		setImage(d->mDecoder.image());
		d->mUpdatedDuringLoad=true;
	}
	emit rectUpdated(rect);
}

void GVDocumentDecodeImpl::frameDone() {
	kdDebug() << k_funcinfo << endl;
}

void GVDocumentDecodeImpl::frameDone(const QPoint& /*offset*/, const QRect& /*rect*/) {
	kdDebug() << k_funcinfo << endl;
}

void GVDocumentDecodeImpl::setLooping(int) {
	kdDebug() << k_funcinfo << endl;
}

void GVDocumentDecodeImpl::setFramePeriod(int /*milliseconds*/) {
	kdDebug() << k_funcinfo << endl;
}

void GVDocumentDecodeImpl::setSize(int width, int height) {
	kdDebug() << k_funcinfo << " " << width << "x" << height << endl;
	// FIXME: There must be a better way than creating an empty image
	setImage(QImage(width, height, 32));
	emit sizeUpdated(width, height);
}

