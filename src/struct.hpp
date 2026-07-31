#pragma once

#include <QByteArray>
#include <QColor>
#include <QCryptographicHash>
#include <QDateTime>
#include <QString>

enum class SearchMode {
    Semantics = 0,  // 语义搜索
    Regex,          // 正则
    None            // 都不是
};

struct Tag  // 标签数据结构
{
    QString tagName;
    QString rule;         // 既可以语义搜索, 也能正则表达式搜索
    QColor tagNameColor;  // tag标签的字体颜色, 对应ContentListItemWidget::m_badgeForeground
    QColor tagBackColor;  // tag标签的背景颜色, 对应ContentListItemWidget::m_badgeBackground
    bool isSysTag = false;
    SearchMode mode = SearchMode::None;
    int priority = 0;  // 标签显示顺序, 越小越靠前

    Tag() = default;

    Tag(QString name, QString _rule, SearchMode _mode, QColor _bg_color = {255, 255, 255},
        QColor _txt_color = {0, 0, 0}, bool _isSysTag = false, int _priority = 0)
        : tagName(name),
          rule(_rule),
          tagNameColor(_txt_color),
          tagBackColor(_bg_color),
          isSysTag(_isSysTag),
          mode(_mode),
          priority(_priority) {}
};

struct ContentListItemData  // 每个复制文本的数据
{
    Tag tag;
    QString content;
    QDateTime copyTime;
    QDateTime updateTime;
    QByteArray hash;
    bool pinned = false;

    ContentListItemData(const Tag tg, QString _cnt, QDateTime _cpt = {}, QDateTime _upt = {})
        : tag(tg), content(_cnt), copyTime(_cpt), updateTime(_upt) {
        hash = QCryptographicHash::hash(content.toLocal8Bit(), QCryptographicHash::Sha256);
    }
};
