// vim: set tabstop=4 shiftwidth=4 noexpandtab:
/*
Gwenview: an image viewer
Copyright 2007 Aurélien Gâteau <agateau@kde.org>

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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

*/
// Self
#include "imagemetainfomodel.h"

// Qt

// KDE
#include <kdebug.h>
#include <kfileitem.h>
#include <kglobal.h>
#include <klocale.h>

// Exiv2
#include <exiv2/exif.hpp>
#include <exiv2/image.hpp>
#include <exiv2/iptc.hpp>

// Local


namespace Gwenview {


enum GroupRow {
	NoGroupSpace= -2,
	NoGroup = -1,
	GeneralGroup,
	ExifGroup,
	IptcGroup
};


class MetaInfoGroup {
public:
	enum {
		InvalidRow = -1
	};

	class Entry {
	public:
		Entry(const QString& key, const QString& label, const QString& value)
		: mKey(key), mLabel(label.trimmed()), mValue(value.trimmed())
		{}

		QString key() const { return mKey; }
		QString label() const { return mLabel; }

		QString value() const { return mValue; }
		void setValue(const QString& value) { mValue = value.trimmed(); }

		void appendValue(const QString& value) {
			if (mValue.length() > 0) {
				mValue += '\n';
			}
			mValue += value.trimmed();
		}

	private:
		QString mKey;
		QString mLabel;
		QString mValue;
	};

	MetaInfoGroup(const QString& label)
	: mLabel(label) {}


	~MetaInfoGroup() {
		qDeleteAll(mList);
	}


	void clear() {
		qDeleteAll(mList);
		mList.clear();
		mRowForKey.clear();
	}


	void addEntry(const QString& key, const QString& label, const QString& value) {
		addEntry(new Entry(key, label, value));
	}


	void addEntry(Entry* entry) {
		mList << entry;
		mRowForKey[entry->key()] = mList.size() - 1;
	}


	void getInfoForKey(const QString& key, QString* label, QString* value) const {
		Entry* entry = getEntryForKey(key);
		if (entry) {
			*label = entry->label();
			*value = entry->value();
		}
	}


	QString getKeyAt(int row) const {
		Q_ASSERT(row < mList.size());
		return mList[row]->key();
	}


	QString getLabelForKeyAt(int row) const {
		Q_ASSERT(row < mList.size());
		return mList[row]->label();
	}


	QString getValueForKeyAt(int row) const {
		Q_ASSERT(row < mList.size());
		return mList[row]->value();
	}


	void setValueForKeyAt(int row, const QString& value) {
		Q_ASSERT(row < mList.size());
		mList[row]->setValue(value);
	}


	int getRowForKey(const QString& key) const {
		return mRowForKey.value(key, InvalidRow);
	}


	int size() const {
		return mList.size();
	}


	QString label() const {
		return mLabel;
	}

	const QList<Entry*>& entryList() const {
		return mList;
	}

private:
	Entry* getEntryForKey(const QString& key) const {
		int row = getRowForKey(key);
		if (row == InvalidRow) {
			return 0;
		}
		return mList[row];
	}

	QList<Entry*> mList;
	QHash<QString, int> mRowForKey;
	QString mLabel;
};


struct ImageMetaInfoModelPrivate {
	QVector<MetaInfoGroup*> mMetaInfoGroupVector;
	ImageMetaInfoModel* mModel;


	void clearGroup(MetaInfoGroup* group, const QModelIndex& parent) {
		if (group->size() > 0) {
			mModel->beginRemoveRows(parent, 0, group->size() - 1);
			group->clear();
			mModel->endRemoveRows();
		}
	}


	void setGroupEntryValue(GroupRow groupRow, const QString& key, const QString& value) {
		MetaInfoGroup* group = mMetaInfoGroupVector[groupRow];
		int entryRow = group->getRowForKey(key);
		if (entryRow == MetaInfoGroup::InvalidRow) {
			kWarning() << "No row for key" << key;
			return;
		}
		group->setValueForKeyAt(entryRow, value);
		QModelIndex groupIndex = mModel->index(groupRow, 0);
		QModelIndex entryIndex = mModel->index(entryRow, 1, groupIndex);
		emit mModel->dataChanged(entryIndex, entryIndex);
	}


	QVariant displayData(const QModelIndex& index) const {
		if (index.internalId() == NoGroup) {
			if (index.column() != 0) {
				return QVariant();
			}
			QString label = mMetaInfoGroupVector[index.row()]->label();
			return QVariant(label);
		}

		if (index.internalId() == NoGroupSpace) {
			return QVariant(QString());
		}

		MetaInfoGroup* group = mMetaInfoGroupVector[index.internalId()];
		if (index.column() == 0) {
			return group->getLabelForKeyAt(index.row());
		} else {
			return group->getValueForKeyAt(index.row());
		}
	}


	void initGeneralGroup() {
		MetaInfoGroup* group = mMetaInfoGroupVector[GeneralGroup];
		group->addEntry("General.Name", i18nc("@item:intable Image file name", "Name"), QString());
		group->addEntry("General.Size", i18nc("@item:intable", "File Size"), QString());
		group->addEntry("General.Time", i18nc("@item:intable", "File Time"), QString());
		group->addEntry("General.ImageSize", i18nc("@item:intable", "Image Size"), QString());
	}


	template <class Container, class Iterator>
	void fillExivGroup(const QModelIndex& parent, MetaInfoGroup* group, const Container& container) {
		// key aren't always unique (for example, "Iptc.Application2.Keywords"
		// may appear multiple times) so we can't know how many rows we will
		// insert before going through them. That's why we create a hash
		// before.
		typedef QHash<QString, MetaInfoGroup::Entry*> EntryHash;
		EntryHash hash;

		Iterator
			it = container.begin(),
			end = container.end();

		if (it == end) {
			return;
		}

		for (;it != end; ++it) {
			QString key = QString::fromUtf8(it->key().c_str());
			QString label = QString::fromLocal8Bit(it->tagLabel().c_str());
			std::ostringstream stream;
			stream << *it;
			QString value = QString::fromLocal8Bit(stream.str().c_str());

			EntryHash::iterator hashIt = hash.find(key);
			if (hashIt != hash.end()) {
				hashIt.value()->appendValue(value);
			} else {
				hash.insert(key, new MetaInfoGroup::Entry(key, label, value));
			}
		}

		mModel->beginInsertRows(parent, 0, hash.size() - 1);
		Q_FOREACH(MetaInfoGroup::Entry* entry, hash) {
			group->addEntry(entry);
		}
		mModel->endInsertRows();
	}
};


ImageMetaInfoModel::ImageMetaInfoModel()
: d(new ImageMetaInfoModelPrivate) {
	d->mModel = this;
	d->mMetaInfoGroupVector.resize(3);
	d->mMetaInfoGroupVector[GeneralGroup] = new MetaInfoGroup(i18nc("@title:group General info about the image", "General"));
	d->mMetaInfoGroupVector[ExifGroup] = new MetaInfoGroup(i18nc("@title:group", "Exif"));
	d->mMetaInfoGroupVector[IptcGroup] = new MetaInfoGroup(i18nc("@title:group", "Iptc"));
	d->initGeneralGroup();
}


ImageMetaInfoModel::~ImageMetaInfoModel() {
	qDeleteAll(d->mMetaInfoGroupVector);
	delete d;
}


void ImageMetaInfoModel::setFileItem(const KFileItem& item) {
	QString sizeString = KGlobal::locale()->formatByteSize(item.size());

	d->setGroupEntryValue(GeneralGroup, "General.Name", item.name());
	d->setGroupEntryValue(GeneralGroup, "General.Size", sizeString);
	d->setGroupEntryValue(GeneralGroup, "General.Time", item.timeString());
}


void ImageMetaInfoModel::setImageSize(const QSize& size) {
	QString imageSize;
	if (size.isValid()) {
		imageSize = i18nc(
			"@item:intable %1 is image width, %2 is image height",
			"%1x%2", size.width(), size.height());

		double megaPixels = size.width() * size.height() / 1000000.;
		if (megaPixels > 0.1) {
			QString megaPixelsString = QString::number(megaPixels, 'f', 1);
			imageSize += ' ';
			imageSize + i18nc(
				"@item:intable %1 is number of millions of pixels in image",
				"(%1MP)", megaPixelsString);
		}
	} else {
		imageSize = "-";
	}
	d->setGroupEntryValue(GeneralGroup, "General.ImageSize", imageSize);
}


void ImageMetaInfoModel::setExiv2Image(const Exiv2::Image* image) {
	MetaInfoGroup* exifGroup = d->mMetaInfoGroupVector[ExifGroup];
	MetaInfoGroup* iptcGroup = d->mMetaInfoGroupVector[IptcGroup];
	QModelIndex exifIndex = index(ExifGroup, 0);
	QModelIndex iptcIndex = index(IptcGroup, 0);
	d->clearGroup(exifGroup, exifIndex);
	d->clearGroup(iptcGroup, iptcIndex);

	if (!image) {
		return;
	}

	if (image->checkMode(Exiv2::mdExif) & Exiv2::amRead) {
		const Exiv2::ExifData& exifData = image->exifData();
		d->fillExivGroup<Exiv2::ExifData, Exiv2::ExifData::const_iterator>(exifIndex, exifGroup, exifData);
	}

	if (image->checkMode(Exiv2::mdIptc) & Exiv2::amRead) {
		const Exiv2::IptcData& iptcData = image->iptcData();
		d->fillExivGroup<Exiv2::IptcData, Exiv2::IptcData::const_iterator>(iptcIndex, iptcGroup, iptcData);
	}
}


void ImageMetaInfoModel::getInfoForKey(const QString& key, QString* label, QString* value) const {
	MetaInfoGroup* group;
	if (key.startsWith(QLatin1String("General"))) {
		group = d->mMetaInfoGroupVector[GeneralGroup];
	} else if (key.startsWith(QLatin1String("Exif"))) {
		group = d->mMetaInfoGroupVector[ExifGroup];
	} else if (key.startsWith(QLatin1String("Iptc"))) {
		group = d->mMetaInfoGroupVector[IptcGroup];
	} else {
		kWarning() << "Unknown metainfo key" << key;
		return;
	}
	group->getInfoForKey(key, label, value);
}


QString ImageMetaInfoModel::getValueForKey(const QString& key) const {
	QString label, value;
	getInfoForKey(key, &label, &value);
	return value;
}


QString ImageMetaInfoModel::keyForIndex(const QModelIndex& index) const {
	if (index.internalId() == NoGroup) {
		return QString();
	}
	MetaInfoGroup* group = d->mMetaInfoGroupVector[index.internalId()];
	return group->getKeyAt(index.row());
}


QModelIndex ImageMetaInfoModel::index(int row, int col, const QModelIndex& parent) const {
	if (col < 0 || col > 1) {
		return QModelIndex();
	}
	if (!parent.isValid()) {
		// This is a group
		if (row < 0 || row >= d->mMetaInfoGroupVector.size()) {
			return QModelIndex();
		}
		return createIndex(row, col, col == 0 ? NoGroup : NoGroupSpace);
	} else {
		// This is an entry
		int group = parent.row();
		if (row < 0 || row >= d->mMetaInfoGroupVector[group]->size()) {
			return QModelIndex();
		}
		return createIndex(row, col, group);
	}
}


QModelIndex ImageMetaInfoModel::parent(const QModelIndex& index) const {
	if (!index.isValid()) {
		return QModelIndex();
	}
	if (index.internalId() == NoGroup || index.internalId() == NoGroupSpace) {
		return QModelIndex();
	} else {
		return createIndex(index.internalId(), 0, NoGroup);
	}
}


int ImageMetaInfoModel::rowCount(const QModelIndex& parent) const {
	if (!parent.isValid()) {
		return d->mMetaInfoGroupVector.size();
	} else if (parent.internalId() == NoGroup) {
		return d->mMetaInfoGroupVector[parent.row()]->size();
	} else {
		return 0;
	}
}


int ImageMetaInfoModel::columnCount(const QModelIndex& /*parent*/) const {
	return 2;
}


QVariant ImageMetaInfoModel::data(const QModelIndex& index, int role) const {
	if (!index.isValid()) {
		return QVariant();
	}

	switch (role) {
	case Qt::DisplayRole:
		return d->displayData(index);
	default:
		return QVariant();
	}
}


QVariant ImageMetaInfoModel::headerData(int section, Qt::Orientation orientation, int role) const {
	if (orientation == Qt::Vertical || role != Qt::DisplayRole) {
		return QVariant();
	}

	QString caption;
	if (section == 0) {
		caption = i18nc("@title:column", "Property");
	} else if (section == 1) {
		caption = i18nc("@title:column", "Value");
	} else {
		kWarning() << "Unknown section" << section;
	}

	return QVariant(caption);
}


} // namespace
