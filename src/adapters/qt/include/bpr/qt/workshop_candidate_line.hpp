#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WORKSHOP_CANDIDATE_LINE_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WORKSHOP_CANDIDATE_LINE_HPP

#include "bpr/core/feedback_workshop_candidate.hpp"
#include "bpr/qt/feedback_label_marks.hpp"

#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QWidget>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

/// \file
/// \brief [[BPR-1091]]. The line under the comment workshop's before and
/// after that says what accepting the candidate changes besides its wording
/// (RFC-BPR-0051 R37).
/// \details Drawn in the marks a reviewer already reads in threads, not in
/// sentences: the label as its [[BPR-1083]] mark, a change as the mark before
/// an arrow and the mark after (the empty Plain mark where a side has no
/// label), the question as a `?`, and the suggested change as the
/// [[BPR-1084]] ring. What stays the same is drawn in the faint ink with no
/// word; what changes is drawn in full ink with one word: added, removed or
/// changed. A header the shared recognizer cannot read is its token as
/// written, underlined with a wavy line in the attention colour. Each facet
/// has its own tooltip, the sentence it stands for, and the whole sentence is
/// the accessible description. Presentation only: it reads a comparison the
/// core made and dispatches nothing.
namespace bpr::qt {
	/// \brief The whole comparison in words: each facet's sentence, as in
	/// "Label changed: question → issue (blocking)" or "No longer asks a
	/// question", joined by " · ", or "Wording only" when there is none.
	[[nodiscard]] auto workshop_candidate_words(const core::workshop_candidate_change& aChange) -> QString;

	/// \brief The candidate line.
	class workshop_candidate_line final : public QWidget
	{
	public:
		/// \brief What one facet is about.
		enum class facet_kind : std::uint8_t {
			label,
			question,
			patch,
			/// Nothing but the wording changes.
			wording,
		};

		explicit workshop_candidate_line(QWidget* aParent = nullptr);

		/// \brief Shows one comparison; the same comparison again changes nothing.
		void present(const core::workshop_candidate_change& aChange);
		/// \brief Shows nothing.
		void clear();
		/// \brief The label tints and mark style on screen.
		void               set_theme(const feedback_mark_theme& aTheme);
		[[nodiscard]] auto theme() const noexcept -> const feedback_mark_theme&;

		/// \brief The comparison in words, as `workshop_candidate_words` says
		/// it; empty when nothing is shown.
		[[nodiscard]] auto text() const -> QString;
		/// \brief The facets drawn, in order.
		[[nodiscard]] auto facet_count() const noexcept -> std::size_t;
		[[nodiscard]] auto facet(std::size_t aIndex) const -> facet_kind;
		/// \brief Where one facet is drawn, in the widget.
		[[nodiscard]] auto facet_rect(std::size_t aIndex) const -> QRectF;
		/// \brief One facet's tooltip: its sentence.
		[[nodiscard]] auto facet_tooltip(std::size_t aIndex) const -> QString;
		/// \brief Whether one facet is drawn as changed (full ink and a word)
		/// rather than kept (the faint ink, no word).
		[[nodiscard]] auto facet_changed(std::size_t aIndex) const -> bool;
		/// \brief The facet under a point, if any.
		[[nodiscard]] auto facet_at(QPointF aPoint) const -> std::optional<std::size_t>;

		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;

	private:
		/// \brief One drawn piece of a facet.
		struct piece
		{
			enum class kind : std::uint8_t {
				mark,
				plain_mark,
				arrow,
				question,
				patch,
				/// Text in the faint ink: custom decorations, "Wording only".
				quiet_text,
				/// Text in full ink: the one word of a change.
				word,
				/// A header the recognizer cannot read, as written.
				unreadable,
			};
			kind                   Kind = kind::word;
			core::feedback_label   Label{ };
			feedback_mark_emphasis Emphasis;
			QString                Text;
			bool                   Faint = false;
			QRectF                 Box;
		};
		struct facet_layout
		{
			facet_kind         Kind    = facet_kind::wording;
			bool               Changed = false;
			QString            Tooltip;
			std::vector<piece> Pieces;
			QRectF             Box;
		};

		void rebuild();
		void place();
		void append_header(facet_layout& aFacet, const core::workshop_label_facts& aFacts, bool aFaint) const;

		std::optional<core::workshop_candidate_change> Change;
		feedback_mark_theme                            Theme;
		std::vector<facet_layout>                      Facets;
		QString                                        Words;
		qreal                                          Width = 0.0;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WORKSHOP_CANDIDATE_LINE_HPP
