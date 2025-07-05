#include "achievementitemdelegate.h"
#include <QDebug>

AchievementItemDelegate::AchievementItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void AchievementItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->save();
    Achievement::AchievementInfo info = index.data(Qt::UserRole).value<Achievement::AchievementInfo>();

    if (!info.id.isEmpty()) {
        // Draw background
        QStyleOptionViewItem newOption = option;
        initStyleOption(&newOption, index);
        newOption.text = "";
        QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &newOption, painter, newOption.widget);

        // icon size
        QSize iconSize = QSize(32, 32);
        QRect iconRect(option.rect.x() + (option.rect.width() - iconSize.width()) / 2,
                       option.rect.y() + 5,
                       iconSize.width(), iconSize.height());

        // Draw icon
        QIcon icon = QIcon(info.iconPath);
        if (!icon.isNull()) {
            icon.paint(painter, iconRect, Qt::AlignCenter, QIcon::Normal, option.state & QStyle::State_Selected ? QIcon::On : QIcon::Off);
        }
        
        QString titleText = QString("<b>%1</b>").arg(info.name);
        QTextDocument textDocument;
        textDocument.setTextWidth(option.rect.width() - 10);
        textDocument.setHtml(titleText);

        QRect textRect(option.rect.x() + 5,
                       iconRect.bottom() + 5,
                       option.rect.width() - 10,
                       (int)textDocument.size().height());
        

        if (option.state & QStyle::State_Selected) {
            painter->setPen(option.palette.highlightedText().color());
        } else {
            painter->setPen(option.palette.text().color());
        }

        painter->translate(textRect.topLeft());
        textDocument.drawContents(painter);
        painter->translate(-textRect.topLeft());
    }

    painter->restore();
}

QSize AchievementItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Achievement::AchievementInfo info = index.data(Qt::UserRole).value<Achievement::AchievementInfo>();

    if (!info.id.isEmpty()) {
        QSize iconSize = QSize(32, 32);
        
        QString titleText = QString("<b>%1</b>").arg(info.name);

        QTextDocument textDocument;
        textDocument.setTextWidth(option.rect.width() > 0 ? option.rect.width() - 10 : 200);
        textDocument.setHtml(titleText);
        // Total height: top padding + icon height + padding + text height + bottom padding
        return QSize(option.rect.width(), iconSize.height() + (int)textDocument.size().height() + 15);
    }
    return QSize(option.rect.width(), 50); // Default size if info is empty
}
