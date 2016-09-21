#include "./downloaditemdelegate.h"

#include "../data/syncthingdownloadmodel.h"

#include <QPixmap>
#include <QPainter>
#include <QApplication>
#include <QStyle>
#include <QTextOption>
#include <QStyleOptionViewItem>
#include <QBrush>
#include <QPalette>

using namespace Data;

namespace QtGui {

inline int centerObj(int avail, int size)
{
    return (avail - size) / 2;
}

DownloadItemDelegate::DownloadItemDelegate(QObject* parent) :
    QStyledItemDelegate(parent),
    m_folderIcon(QIcon::fromTheme(QStringLiteral("folder-open"), QIcon(QStringLiteral(":/icons/hicolor/scalable/places/folder-open.svg"))).pixmap(QSize(16, 16)))
{}

void DownloadItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // use the customization only on top-level rows
    //if(!index.parent().isValid()) {
    //    QStyledItemDelegate::paint(painter, option, index);
    //} else {
        // init style options to use drawControl(), except for the text
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        opt.text.clear();
        opt.features = QStyleOptionViewItem::None;
        QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter);

        // draw progress bar
        const QAbstractItemModel *model = index.model();
        QStyleOptionProgressBar progressBarOption;
        progressBarOption.state = QStyle::State_Enabled;
        progressBarOption.direction = QApplication::layoutDirection();
        progressBarOption.rect = option.rect;
        progressBarOption.rect.setWidth(option.rect.width() - 20);
        progressBarOption.textAlignment = Qt::AlignCenter;
        progressBarOption.textVisible = true;
        progressBarOption.progress = model->data(index, SyncthingDownloadModel::ItemPercentage).toInt();
        progressBarOption.minimum = 0;
        progressBarOption.maximum = 100;
        progressBarOption.text = model->data(index, Qt::DisplayRole).toString();
        QApplication::style()->drawControl(QStyle::CE_ProgressBar, &progressBarOption, painter);

        // draw buttons
        const int buttonY = option.rect.y() + centerObj(option.rect.height(), 16);
        painter->drawPixmap(option.rect.right() - 16, buttonY, 16, 16, m_folderIcon);
    //}

}

}
