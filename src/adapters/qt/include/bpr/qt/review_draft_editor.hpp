#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_DRAFT_EDITOR_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_DRAFT_EDITOR_HPP

#include "bpr/core/review_ledger.hpp"
#include "bpr/core/review_round_trip.hpp"

#include <QWidget>

#include <cstdint>
#include <functional>
#include <optional>

class QAction;
class QLabel;
class QPlainTextEdit;

namespace bpr::qt {
	/// \brief Records directly observable editor activity.
	struct review_draft_editor_statistics
	{
		std::uint64_t DraftIntents   = 0U;
		std::uint64_t ReceiptIntents = 0U;
		std::uint64_t Presentations  = 0U;

		auto operator<=>(const review_draft_editor_statistics&) const = default;
	};

	/// \brief Composes a review comment and asks for it to be published.
	///
	/// \details The editor is given two dispatch callbacks and nothing else.
	/// There is no provider to hold and no endpoint to reach: the only way out
	/// of this widget is a semantic command, which is what makes "a widget
	/// never calls a mutation endpoint" true by construction rather than by
	/// review.
	///
	/// Nothing the user does changes what the editor shows. Every action emits
	/// an intent and waits; the displayed state moves only when `present` is
	/// called with an accepted projection. A widget that updated itself
	/// optimistically would show a comment as published while the request was
	/// still in flight — and would keep showing it if the request failed.
	class review_draft_editor final : public QWidget
	{
	public:
		using draft_dispatch   = std::function<void(core::review_draft_command)>;
		using receipt_dispatch = std::function<void(core::file_receipt_command)>;

		review_draft_editor(
			draft_dispatch   aDraftDispatch,
			receipt_dispatch aReceiptDispatch,
			QWidget*         aParent = nullptr);

		/// \brief Adopts one accepted draft projection.
		void present(const core::review_comment_draft& aDraft);

		/// \brief Adopts one accepted receipt projection.
		void present(const core::file_receipt& aReceipt);

		/// \brief Describes the target a publication would be sent to.
		///
		/// \details Shown before publishing rather than after, because the
		/// reviewer is agreeing to a specific pull request at a specific
		/// revision under a specific account, and cannot agree to something
		/// they were not shown.
		void describe_target(const QString& aDescription);

		[[nodiscard]] auto body_editor() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto target_label() const noexcept -> QLabel*;
		[[nodiscard]] auto publish_action() const noexcept -> QAction*;
		[[nodiscard]] auto discard_action() const noexcept -> QAction*;
		[[nodiscard]] auto mark_read_action() const noexcept -> QAction*;
		[[nodiscard]] auto accept_action() const noexcept -> QAction*;
		[[nodiscard]] auto exclude_action() const noexcept -> QAction*;

		[[nodiscard]] auto shown_draft() const noexcept
			-> const std::optional<core::review_comment_draft>&;
		[[nodiscard]] auto shown_receipt() const noexcept
			-> const std::optional<core::file_receipt>&;
		[[nodiscard]] auto statistics() const noexcept
			-> review_draft_editor_statistics;

	private:
		void emit_draft(core::review_draft_command_kind aKind);
		void emit_receipt(core::file_receipt_command_kind aKind);
		void refresh_enablement();

		draft_dispatch   DraftDispatch;
		receipt_dispatch ReceiptDispatch;

		QPlainTextEdit* Body        = nullptr;
		QLabel*         TargetLabel = nullptr;
		QAction*        Publish     = nullptr;
		QAction*        Discard     = nullptr;
		QAction*        MarkRead    = nullptr;
		QAction*        Accept      = nullptr;
		QAction*        Exclude     = nullptr;

		std::optional<core::review_comment_draft> ShownDraft;
		std::optional<core::file_receipt>         ShownReceipt;
		review_draft_editor_statistics            Statistics;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_DRAFT_EDITOR_HPP
