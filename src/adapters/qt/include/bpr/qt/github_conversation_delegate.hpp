#pragma once

#include <QStyledItemDelegate>

namespace bpr::qt
{
class github_conversation_delegate final : public QStyledItemDelegate
{
public:
    explicit github_conversation_delegate(QObject* parent = nullptr);

    void paint(
        QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;
    [[nodiscard]] auto sizeHint(
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const -> QSize override;
};
}
