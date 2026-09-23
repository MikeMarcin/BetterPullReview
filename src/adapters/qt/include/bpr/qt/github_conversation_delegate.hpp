#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_CONVERSATION_DELEGATE_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_CONVERSATION_DELEGATE_HPP

#include <QStyledItemDelegate>

#include <memory>

namespace bpr::qt {
	/// \brief Provides the github conversation delegate contract.
	class github_conversation_delegate final : public QStyledItemDelegate
	{
	public:
		explicit github_conversation_delegate(QObject* aParent = nullptr, bool aCompact = false);
		~github_conversation_delegate() override;
		github_conversation_delegate(const github_conversation_delegate&)                    = delete;
		auto operator=(const github_conversation_delegate&) -> github_conversation_delegate& = delete;

		void paint(
			QPainter*                   aPainter,
			const QStyleOptionViewItem& aOption,
			const QModelIndex&          aIndex) const override;
		[[nodiscard]] auto sizeHint(
			const QStyleOptionViewItem& aOption,
			const QModelIndex&          aIndex) const -> QSize override;

	private:
		struct measurement_cache;

		bool                               Compact = false;
		std::unique_ptr<measurement_cache> Measured;
	};
}

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_CONVERSATION_DELEGATE_HPP
