#include "TagWidget.hpp"

TagDelegate::TagDelegate(QObject* parent) : QStyledItemDelegate(parent)
{}

QSize TagDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    QString text = index.data(Qt::DisplayRole).toString();
    QFontMetrics fm(m_font);
    int textWidth = fm.horizontalAdvance(text);
    int totalWidth = textWidth + m_padding.width() + m_horizontalSpacing;
    int totalHeight = fm.height() + m_padding.height();
    return QSize(totalWidth, totalHeight);
}

void TagDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                        const QModelIndex& index) const 
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    QRect rect = option.rect.adjusted(0, 0, -m_horizontalSpacing, 0);
    QString text = index.data(Qt::DisplayRole).toString();

    // 选择背景色和文字色（选中效果）
    QColor bgColor, textColor;
    if (option.state & QStyle::State_Selected) {
        bgColor = QColor("#3B82F6");
        textColor = Qt::white;
    } else {
        bgColor = QColor("#ffffff");
        textColor = Qt::black;
    }

    // 绘制圆角矩形背景
    painter->setBrush(bgColor);
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(rect, 13, 13);

    // 绘制文字
    painter->setPen(textColor);
    painter->setFont(m_font);
    painter->drawText(rect, Qt::AlignCenter, text);

    painter->restore();
}

TagListView::TagListView(QWidget* parent) {
    setFlow(QListView::LeftToRight);
    setWrapping(false);
    setFrameShape(QFrame::NoFrame);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setEditTriggers(QAbstractItemView::NoEditTriggers);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setStyleSheet("QListView { outline: none; }");
    m_delegate = new TagDelegate(this);
    setItemDelegate(m_delegate);

}

void TagListView::setModel(QAbstractItemModel* model) {
    if (this->model()){
        disconnect(this->model(), nullptr, this, nullptr);
    }
    QListView::setModel(model);
    if(model){
        connect(model, &QAbstractItemModel::dataChanged, this, &TagListView::adjustSizeToContent);
        connect(model, &QAbstractItemModel::rowsInserted, this, &TagListView::adjustSizeToContent);
        connect(model, &QAbstractItemModel::rowsRemoved, this, &TagListView::adjustSizeToContent);
        adjustSizeToContent();  // 初始调整
        
    }
}

void TagListView::adjustSizeToContent(){
    if (!model()) return;
    int totalWidth = 0;
    int maxHeight = 0;
    for (int row = 0; row < model()->rowCount(); ++row) {
        QModelIndex idx = model()->index(row, 0);
        QSize size = m_delegate->sizeHint(QStyleOptionViewItem(), idx);
        totalWidth += size.width();
        maxHeight = qMax(maxHeight, size.height());
    }
    if (model()->rowCount() > 0) {
        totalWidth -= m_delegate->horizontalSpacing();
    }
    int extraWidth = frameWidth() * 2 + contentsMargins().left() + contentsMargins().right();
    int extraHeight = frameWidth() * 2 + contentsMargins().top() + contentsMargins().bottom();
    setFixedSize(totalWidth + extraWidth, maxHeight + extraHeight);
}
