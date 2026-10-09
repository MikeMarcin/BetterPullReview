#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_LINE_DETAILS_PANE_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_LINE_DETAILS_PANE_HPP

#include "bpr/application/appearance_runtime_ports.hpp"
#include "bpr/core/appearance_model.hpp"
#include "bpr/core/diff_semantics.hpp"
#include "bpr/qt/code_sample_painter.hpp"

#include <QByteArray>

#include <QColor>
#include <QPlainTextEdit>
#include <QString>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

class QPaintEvent;
class QResizeEvent;

namespace bpr::qt {
	/// \brief [[BPR-1334]]. The colours Line details paints with.
	///
	/// \details The same appearance roles the comparison panes use, so a row
	/// here looks like the line it repeats. An invalid colour paints nothing.
	/// `Syntax` is the panes' own role table: a token here is the colour it
	/// is there.
	struct line_details_colors
	{
		QColor                   Added;
		QColor                   Removed;
		QColor                   IntralineAdded;
		QColor                   IntralineRemoved;
		QColor                   GutterBackground;
		QColor                   GutterText;
		code_role_colors         Syntax;
		core::appearance_variant Variant  = core::appearance_variant::light;
		bool                     Supplied = false;

		auto operator==(const line_details_colors&) const -> bool = default;
	};

	/// \brief [[BPR-1334]]. Line details: the current line, base over head.
	///
	/// \details Beyond Compare's Text Details, under the comparison. Each row
	/// of a core `line_details_projection` is one unwrapped line: a base row
	/// above its head row, full width, with its line number and a −/+ mark in
	/// the gutter and the changed characters highlighted. A row too long for
	/// the strip scrolls sideways with every other row, under one bar, so the
	/// columns stay aligned; the gutter stays put. A new projection opens
	/// scrolled to its first changed character.
	///
	/// It decides nothing. Rows, cuts and highlight spans come from the core;
	/// this only turns them into text, bands and a gutter. A cut line and an
	/// absent side are said in the placeholder colour, so they cannot be read
	/// as code.
	///
	/// Each row is coloured as its side's pane colours that line (the owner
	/// rejected v003: "there's no syntax highlighting"): the panes' own
	/// classification of the base and the head blob, through the same
	/// `code_syntax_format_ranges`, under the change highlight. The highlight
	/// is a background, so a changed token keeps its colour, readable over it
	/// as in the panes. A cut row is coloured only as far as it is shown.
	class line_details_pane final : public QPlainTextEdit
	{
	public:
		explicit line_details_pane(QWidget* aParent = nullptr);

		/// \brief Shows `aProjection`, reading line text from `aDiff`.
		void present(const core::line_details_projection& aProjection,
					 const core::computed_file_diff&      aDiff);
		/// \brief Shows nothing; the placeholder says why.
		void present_empty();
		void set_colors(const line_details_colors& aColors);
		/// \brief Adopts the base and the head blob's classification, the
		/// ones the panes paint from. Null colours nothing on that side.
		void set_token_spans(application::shared_token_spans aBase, application::shared_token_spans aHead);

		[[nodiscard]] auto projection() const noexcept -> const core::line_details_projection&;
		/// \brief How many times the document was rebuilt; a repeat of the
		/// same projection rebuilds nothing.
		[[nodiscard]] auto presentations() const noexcept -> std::uint64_t;
		[[nodiscard]] auto gutter_width() const -> int;
		/// \brief The gutter text drawn for one block: its line number and
		/// mark, as painted. Empty for a block with no row.
		[[nodiscard]] auto gutter_text(int aBlock) const -> QString;
		/// \brief The document x of the leftmost changed character over every
		/// row: an emphasis span's start, or the start of an added or removed
		/// row. None when nothing shown is marked as changed.
		[[nodiscard]] auto first_change_x() const -> std::optional<int>;
		[[nodiscard]] auto gutter_widget() const noexcept -> QWidget*;

	protected:
		void resizeEvent(QResizeEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;
		auto viewportEvent(QEvent* aEvent) -> bool override;
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;

	private:
		class gutter;
		friend class gutter;
		void paint_gutter(QPaintEvent* aEvent);
		void place_gutter();
		/// \brief The height kept free under the rows for an overlay
		/// horizontal bar; zero for a bar that takes its own height.
		[[nodiscard]] auto overlay_bar_reserve() const -> int;
		void               repaint_bands();
		/// \brief Colours each row's tokens from its side's classification.
		void apply_syntax();
		void reveal_first_change();

		gutter*                         Gutter = nullptr;
		core::line_details_projection   Shown;
		const core::computed_file_diff* ShownDiff = nullptr;
		/// Block of each row; rows past the last are notes, not rows.
		std::size_t         RowBlocks = 0U;
		line_details_colors Colors;
		std::uint64_t       Presentations = 0U;
		bool                Empty         = true;
		/// Emphasis spans in characters, block by block, kept from the last
		/// presentation so a colour change repaints without decoding again.
		struct emphasis_span
		{
			int Block = 0;
			int From  = 0;
			int To    = 0;
		};
		std::vector<emphasis_span> Emphasis;
		/// The bytes each row shows (cut, as the core cut them), kept so a
		/// classification that arrives later colours them without the diff.
		std::vector<QByteArray>         RowBytes;
		application::shared_token_spans BaseSpans;
		application::shared_token_spans HeadSpans;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_LINE_DETAILS_PANE_HPP
