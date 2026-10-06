#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SUGGESTION_MARK_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SUGGESTION_MARK_HPP

#include "bpr/core/suggestion_applicability.hpp"

#include <QSize>
#include <QString>
#include <QWidget>

#include <string_view>

/// \file
/// \brief [[BPR-1084]]. The small mark a comment editor shows before Publish
/// when its body carries a suggested change (RFC-BPR-0048 R13, RFC-BPR-0050
/// R32): GitHub's native suggestion on this line, or text only here.
/// \details Presentation only. The decision is
/// `core::decide_suggestion_applicability`; the mark draws it, names it and
/// says why in its tooltip. A body with no suggested change shows nothing. The
/// mark never restyles the fence, applies nothing and offers no action.
namespace bpr::qt {
	/// \brief The mark's accessible name: "Suggested change, native" or
	/// "Suggested change, text only"; empty with no patch.
	[[nodiscard]] auto suggestion_mark_name(const core::suggestion_applicability_decision& aDecision) -> QString;
	/// \brief The tooltip and accessible description: what GitHub will do
	/// with the change here, and for text only why.
	[[nodiscard]] auto suggestion_mark_tooltip(const core::suggestion_applicability_decision& aDecision) -> QString;

	/// \brief A 16 px glyph: a plus over a minus in a rounded box, struck
	/// through and dashed when the change is text only, so the two states
	/// differ in shape and not only in colour. Hidden with no patch.
	class suggestion_mark final : public QWidget
	{
	public:
		explicit suggestion_mark(QWidget* aParent = nullptr);
		/// \brief Shows one decision; the same decision again changes nothing.
		void               present(const core::suggestion_applicability_decision& aDecision);
		[[nodiscard]] auto decision() const noexcept -> const core::suggestion_applicability_decision&;
		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;

	private:
		core::suggestion_applicability_decision Decision;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SUGGESTION_MARK_HPP
