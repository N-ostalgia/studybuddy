#ifndef ACHIEVEMENTITEMDELEGATE_H
#define ACHIEVEMENTITEMDELEGATE_H

#include <QStyledItemDelegate>
#include <QPainter>
#include <QAbstractTextDocumentLayout>
#include <QTextDocument>
#include <QApplication>
#include <QDebug>
#include "achievements.h"
class AchievementItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit AchievementItemDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

};

#endif // ACHIEVEMENTITEMDELEGATE_H 
