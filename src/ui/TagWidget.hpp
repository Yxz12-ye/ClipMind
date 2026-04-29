#pragma once

#include <QWidget>
#include <QListView>
#include <QStyledItemDelegate>
#include <QSize>
#include <QPainter>

class TagDelegate;

class TagListView : public QListView
{
    Q_OBJECT
private:
    TagDelegate *m_delegate;

public:
    explicit TagListView(QWidget* parent = nullptr);
    ~TagListView()=default;

    void setModel(QAbstractItemModel *model) override;
private slots:
    void adjustSizeToContent();
};

class TagDelegate : public QStyledItemDelegate
{
    Q_OBJECT
private:
    QSize m_padding = {12, 6};
    QFont m_font = {"Microsoft YaHei", 12};
    int m_horizontalSpacing = 8;

public:
    explicit TagDelegate(QObject* parent = nullptr);
    ~TagDelegate()=default;

    int horizontalSpacing() const { return m_horizontalSpacing; }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};
