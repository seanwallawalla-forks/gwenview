// vim:set tabstop=4 shiftwidth=4 noexpandtab:
/*
Gwenview - A simple image viewer for KDE
Copyright 2000-2004 Aur�ien G�eau

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
#include <qbitmap.h>
#include <qevent.h>
#include <qlayout.h>
#include <qpainter.h>
#include <qtoolbutton.h>

// KDE
#include <kiconloader.h>
#include <kicontheme.h>

// Local
#include "gvfullscreenbar.moc"


const int FULLSCREEN_LABEL_RADIUS = 6;


class ActionButton : public QToolButton {
public:
	ActionButton(QWidget* parent, KAction* action)
	: QToolButton(parent)
	{
		setAutoRaise(true);
		setIconSet(MainBarIconSet(action->icon()));
		setTextLabel(action->plainText(), true);
		setEnabled(action->isEnabled());

		connect(this, SIGNAL(clicked()), action, SLOT(activate()) );
		connect(action, SIGNAL(enabled(bool)), this, SLOT(setEnabled(bool)) );
	}
};


static void fillMask(QPainter& painter, const QRect& rect) {
	painter.fillRect(
		rect.left(),
		rect.top(),
		rect.width() - FULLSCREEN_LABEL_RADIUS,
		rect.bottom(),
		painter.brush());

	painter.fillRect(
		rect.right() - FULLSCREEN_LABEL_RADIUS,
		rect.top(),
		FULLSCREEN_LABEL_RADIUS,
		rect.height() - FULLSCREEN_LABEL_RADIUS,
		painter.brush());

	painter.drawPie(
		rect.right() - 2*FULLSCREEN_LABEL_RADIUS,
		rect.bottom() - 2*FULLSCREEN_LABEL_RADIUS,
		FULLSCREEN_LABEL_RADIUS*2, FULLSCREEN_LABEL_RADIUS*2,
		0, -16*90);
}


static void drawBorder(QPainter& painter, const QRect& rect) {
	painter.drawLine(
		rect.right() - 1,
		rect.top(),
		rect.right() - 1,
		rect.bottom() - FULLSCREEN_LABEL_RADIUS);

	painter.drawLine(
		rect.right() - FULLSCREEN_LABEL_RADIUS,
		rect.bottom() - 1,
		rect.left(),
		rect.bottom() - 1);

	painter.drawArc(
		rect.right() - 2*FULLSCREEN_LABEL_RADIUS,
		rect.bottom() - 2*FULLSCREEN_LABEL_RADIUS,
		FULLSCREEN_LABEL_RADIUS*2, FULLSCREEN_LABEL_RADIUS*2,
		0, -16*90);
}


GVFullScreenBar::GVFullScreenBar(QWidget* parent, const KActionPtrList& actions)
: QLabel(parent) {
	QColor bg=colorGroup().highlight();
	QColor fg=colorGroup().highlightedText();
	QPalette pal(palette());
	pal.setColor(QColorGroup::Background, bg);
	pal.setColor(QColorGroup::Foreground, fg);
	pal.setColor(QColorGroup::Button, bg);
	pal.setColor(QColorGroup::ButtonText, fg);
	setPalette(pal);

	QVBoxLayout* vLayout=new QVBoxLayout(this);
	QHBoxLayout* layout=new QHBoxLayout(vLayout);
	vLayout->addSpacing(FULLSCREEN_LABEL_RADIUS);
	vLayout->setResizeMode(QLayout::Fixed);

	// Buttons
	KActionPtrList::ConstIterator
		it=actions.begin(),
		end=actions.end();
	for (; it!=end; ++it) {
		ActionButton* btn=new ActionButton(this, *it);
		layout->addWidget(btn);
	}

	// Label
	mLabel=new QLabel(this);
	layout->addWidget(mLabel);
	QFont font=mLabel->font();
	font.setWeight(QFont::Bold);
	mLabel->setFont(font);

	layout->addSpacing(FULLSCREEN_LABEL_RADIUS);
}
	

void GVFullScreenBar::resizeEvent(QResizeEvent* event) {
	QSize size=event->size();
	QRect rect=QRect(QPoint(0, 0), size);

	QPainter painter;
	// Create a mask for the text
	QBitmap mask(size,true);
	painter.begin(&mask);
	painter.setBrush(Qt::white);
	fillMask(painter, rect);
	painter.end();

	// Draw the background on a pixmap
	QPixmap pixmap(size);
	painter.begin(&pixmap, this);
	painter.eraseRect(rect);
	drawBorder(painter, rect);
	painter.end();

	// Update the label
	setPixmap(pixmap);
	setMask(mask);
}


void GVFullScreenBar::setText(const QString& text) {
	mLabel->setText(text);
}

