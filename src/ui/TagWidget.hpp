#pragma once

#include <QWidget>
#include <QListView>
#include <QStyledItemDelegate>
#include <QSize>
#include <QPainter>
#include <QTime>

class TagDelegate;

enum class SearchMode {
    Semantics=0, // 语义搜索
    Regex, // 正则
    None // 都不是, 目前仅用于兜底标签
};

struct Tag
{
    QString tagName;
    QString rule; // 既可以语义搜索, 也能正则表达式搜索
    QColor tagNameColor; // tag标签的字体颜色, 对应ContentListItemWidget::m_badgeForeground
    QColor tagBackColor;// tag标签的背景颜色, 对应ContentListItemWidget::m_badgeBackground
    bool isSysTag;
    SearchMode mode;

    Tag(QString name, QString _rule, SearchMode _mode, 
        QColor _bg_color = {255,255,255}, 
        QColor _txt_color = {0,0,0})
        :tagName(name), rule(_rule), tagNameColor(_txt_color), tagBackColor(_bg_color)
        , mode(_mode)
        {}
};

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
