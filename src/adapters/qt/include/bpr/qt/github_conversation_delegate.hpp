#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_CONVERSATION_DELEGATE_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_CONVERSATION_DELEGATE_HPP

#include <QStyledItemDelegate>

namespace bpr::qt {
	/// \brief Provides the github conversation delegate contract.
	class github_conversation_delegate final : public QStyledItemDelegate
	{
	public:
		explicit github_conversation_delegate(QObject* aParent = nullptr, bool aCompact = false);

		void paint(
			QPainter*                   aPainter,
			const QStyleOptionViewItem& aOption,
			const QModelIndex&          aIndex) const override;
		[[nodiscard]] auto sizeHint(
			const QStyleOptionViewItem& aOption,
			const QModelIndex&          aIndex) const -> QSize override;

	private:
		bool Compact = false;
	};
}

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_CONVERSATION_DELEGATE_HPP
