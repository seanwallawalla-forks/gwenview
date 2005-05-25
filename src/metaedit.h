// vim: set tabstop=4 shiftwidth=4 noexpandtab
/*
Copyright (c) 2003 Jos van den Oever

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
#ifndef METAEDIT_H
#define METAEDIT_H

// Qt
#include <qvbox.h>

// KDE
#include <kfilemetainfo.h>
class QTextEdit;

namespace Gwenview {

class Document;

class MetaEdit : public QVBox {
Q_OBJECT
public:
	MetaEdit(QWidget *parent, Document*, const char *name="");
	~MetaEdit();
protected:
	bool eventFilter(QObject *o, QEvent *e);
private slots:
	void updateContent();
	void updateDoc();
	void setModified(bool);

private:
	bool mEmpty;
	Document* mDocument;
	QTextEdit* mCommentEdit;

	void setEmptyText();
};


} // namespace
#endif

