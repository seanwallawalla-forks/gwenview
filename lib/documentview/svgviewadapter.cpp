// vim: set tabstop=4 shiftwidth=4 noexpandtab:
/*
Gwenview: an image viewer
Copyright 2008 Aurélien Gâteau <agateau@kde.org>

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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Cambridge, MA 02110-1301, USA.

*/
// Self
#include "svgviewadapter.moc"

// Qt
#include <QCursor>
#include <QEvent>
#include <QGraphicsWidget>
#include <QPainter>
#include <QSvgRenderer>

// KDE
#include <kdebug.h>

// Local
#include "document/documentfactory.h"

namespace Gwenview {

class SvgWidget : public QGraphicsWidget {
public:
	SvgWidget(QGraphicsItem* parent = 0)
	: QGraphicsWidget(parent)
	, mRenderer(new QSvgRenderer(this))
	, mZoom(1)
	{}

	void loadFromDocument(Document::Ptr doc) {
		mRenderer->load(doc->rawData());
		updateCache();
	}

	void updateCache() {
		mCachePix = QPixmap(defaultSize() * mZoom);
		mCachePix.fill(Qt::transparent);
		QPainter painter(&mCachePix);
		mRenderer->render(&painter, QRectF(mCachePix.rect()));
		update();
	}

	QSize defaultSize() const {
		return mRenderer->defaultSize();
	}

	virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/) {
		painter->drawPixmap(
			(size().width() - mCachePix.width()) / 2,
			(size().height() - mCachePix.height()) / 2,
			mCachePix);
	}

	QSvgRenderer* mRenderer;
	qreal mZoom;
	QPixmap mCachePix;
};

struct SvgViewAdapterPrivate {
	Document::Ptr mDocument;
	SvgWidget* mWidget;
	bool mZoomToFit;
};


SvgViewAdapter::SvgViewAdapter()
: d(new SvgViewAdapterPrivate) {
	d->mZoomToFit = true;
	d->mWidget = new SvgWidget;
	setWidget(d->mWidget);
}


void SvgViewAdapter::installEventFilterOnViewWidgets(QObject* /*object*/) {
	// FIXME: QGV
	//d->mView->viewport()->installEventFilter(object);
	// Necessary to receive key{Press,Release} events
	//d->mView->installEventFilter(object);
}


SvgViewAdapter::~SvgViewAdapter() {
	delete d;
}


QCursor SvgViewAdapter::cursor() const {
	return widget()->cursor();
}


void SvgViewAdapter::setCursor(const QCursor& cursor) {
	widget()->setCursor(cursor);
}


void SvgViewAdapter::setDocument(Document::Ptr doc) {
	d->mDocument = doc;
	connect(d->mDocument.data(), SIGNAL(loaded(KUrl)),
		SLOT(loadFromDocument()));
	loadFromDocument();
}


void SvgViewAdapter::loadFromDocument() {
	d->mWidget->loadFromDocument(d->mDocument);
	if (d->mZoomToFit) {
		setZoom(computeZoomToFit());
	}
}


Document::Ptr SvgViewAdapter::document() const {
	return d->mDocument;
}


void SvgViewAdapter::setZoomToFit(bool on) {
	if (d->mZoomToFit == on) {
		return;
	}
	d->mZoomToFit = on;
	if (d->mZoomToFit) {
		setZoom(computeZoomToFit());
	}
	zoomToFitChanged(on);
}


bool SvgViewAdapter::zoomToFit() const {
	return d->mZoomToFit;
}


qreal SvgViewAdapter::zoom() const {
	return d->mWidget->mZoom;
}


void SvgViewAdapter::setZoom(qreal zoom, const QPointF& /*center*/) {
	d->mWidget->mZoom = zoom;
	d->mWidget->updateCache();
	emit zoomChanged(zoom);
}


qreal SvgViewAdapter::computeZoomToFit() const {
	return qMin(computeZoomToFitWidth(), computeZoomToFitHeight());
}


qreal SvgViewAdapter::computeZoomToFitWidth() const {
	qreal width = d->mWidget->defaultSize().width();
	return width != 0 ? (d->mWidget->size().width() / width) : 1;
}


qreal SvgViewAdapter::computeZoomToFitHeight() const {
	qreal height = d->mWidget->defaultSize().height();
	return height != 0 ? (d->mWidget->size().height() / height) : 1;
}


bool SvgViewAdapter::eventFilter(QObject*, QEvent* event) {
	if (event->type() == QEvent::Resize) {
		if (d->mZoomToFit) {
			setZoom(computeZoomToFit());
		}
	}
	return false;
}

} // namespace
