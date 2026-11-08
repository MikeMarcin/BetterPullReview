#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/appearance_contrast.hpp"
#include "bpr/qt/code_sample_painter.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/line_details_pane.hpp"
#include "bpr/qt/pending_veil.hpp"
#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/suggestion_editor.hpp"
#include "bpr/qt/type_preview.hpp"
#include "diff_workbench_support.hpp"
#include "line_details_splitter.hpp"

#include <QAbstractButton>
#include <QAbstractScrollArea>
#include <QAbstractSlider>
#include <QApplication>
#include <QBoxLayout>
#include <QBrush>
#include <QChar>
#include <QColor>
#include <QEvent>
#include <QFlags>
#include <QFont>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLatin1Char>
#include <QLayout>
#include <QList>
#include <QMargins>
#include <QMetaObject>
#include <QObject>
#include <QPalette>
#include <QPlainTextDocumentLayout>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPointer>
#include <QPushButton>
#include <QRect>
#include <QRegion>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QSize>
#include <QSplitter>
#include <QSplitterHandle>
#include <QStackedLayout>
#include <QString>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFormat>
#include <QTextOption>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::qt {
	namespace {
		/// \brief [[BPR-1798]]. The colour a Find match starting at `aPosition`
		/// is drawn on: the pane's base with every row selection that covers
		/// the position composited over it in paint order, so a selected
		/// added line reads as the added fill under the selection wash.
		[[nodiscard]] auto find_ground_at(const QPlainTextEdit& aPane, const std::span<const QTextEdit::ExtraSelection> aBeneath, const int aPosition) -> core::appearance_color
		{
			const auto opaque = [](const QColor& aColour) {
				return core::appearance_color{ static_cast<std::uint8_t>(aColour.red()), static_cast<std::uint8_t>(aColour.green()), static_cast<std::uint8_t>(aColour.blue()), 255U };
			};
			auto       ground = opaque(aPane.palette().color(QPalette::Base));
			const auto block  = aPane.document()->findBlock(aPosition).blockNumber();
			for (const auto& selection : aBeneath) {
				const auto& brush = selection.format.background();
				if (brush.style() == Qt::NoBrush) {
					continue;
				}
				const auto& cursor = selection.cursor;
				const bool  covers = selection.format.boolProperty(QTextFormat::FullWidthSelection)
					? aPane.document()->findBlock(cursor.selectionStart()).blockNumber() <= block && block <= aPane.document()->findBlock(cursor.selectionEnd()).blockNumber()
					: cursor.selectionStart() <= aPosition && aPosition < cursor.selectionEnd();
				if (covers) {
					ground = core::composited(opaque(brush.color()), ground, static_cast<std::uint8_t>(brush.color().alpha()));
				}
			}
			return ground;
		}

		/// [[BPR-1264]]. Above this many lines on the two sides together a
		/// publication fills its documents over several GUI turns. Below it the
		/// single replacement is a few milliseconds on the slowest station.
		inline constexpr std::size_t staged_publication_lines = 4096U;

		/// \brief Gives a pane a filled document, keeping its typography.
		///
		/// \details QPlainTextEdit keeps its font, tab stop and margin on the
		/// document, so a new one is given the replaced one's, and the plain
		/// text layout the pane requires.
		void adopt_document(source_pane& aPane, QTextDocument& aDocument)
		{
			auto* const previous = aPane.document();
			aDocument.setDefaultFont(previous->defaultFont());
			aDocument.setDefaultTextOption(previous->defaultTextOption());
			aDocument.setDocumentMargin(previous->documentMargin());
			aDocument.setUndoRedoEnabled(previous->isUndoRedoEnabled());
			aDocument.setDocumentLayout(new QPlainTextDocumentLayout{ &aDocument });
			aDocument.setLayoutEnabled(true);
			// The pane's first document belongs to its text control, which
			// deletes it on replacement; one adopted earlier is the pane's child.
			const auto adopted = previous->parent() == &aPane;
			aPane.setDocument(&aDocument);
			if (adopted) {
				delete previous;
			}
			// Replacing the document signals no block count change, and a
			// staged file would otherwise keep the empty pane's gutter
			// ([[BPR-1335]]).
			aPane.refresh_gutter_width();
		}

		[[nodiscard]] auto reason_text(const core::diff_reason_code aReason)
			-> QString
		{
			switch (aReason) {
			case core::diff_reason_code::none:
				return QStringLiteral("Diff ready");
			case core::diff_reason_code::binary_file:
				return QStringLiteral("Binary file comparison");
			case core::diff_reason_code::missing_left_blob:
			case core::diff_reason_code::missing_right_blob:
				return QStringLiteral("A selected file version is unavailable");
			case core::diff_reason_code::decode_failure:
				return QStringLiteral("File text could not be decoded");
			case core::diff_reason_code::materialization_limit:
				return QStringLiteral("File exceeds the comparison materialization limit");
			case core::diff_reason_code::compute_limit:
				return QStringLiteral("Comparison unavailable — file exceeds the comparison work limit");
			case core::diff_reason_code::generated_file_unsupported:
				return QStringLiteral("Generated file comparison is unsupported");
			case core::diff_reason_code::cancelled:
				return QStringLiteral("Comparison cancelled");
			case core::diff_reason_code::stale:
				return QStringLiteral("Older comparison discarded");
			case core::diff_reason_code::invalid_alignment:
			case core::diff_reason_code::host_failure:
				return QStringLiteral("Comparison unavailable");
			}
			return QStringLiteral("Comparison unavailable");
		}

		/// \brief [[BPR-1212]]. What a code pane's description adds: the keys
		/// that move and extend its selected line, and the two that start a
		/// comment or a suggestion on it, named as the platform names them
		/// (Return on macOS, Enter elsewhere), as Review Cycles does.
		[[nodiscard]] auto line_keys_description() -> QString
		{
#if defined(Q_OS_MACOS)
			const auto enter = QStringLiteral("Return");
#else
			const auto enter = QStringLiteral("Enter");
#endif
			return QStringLiteral("Up and Down move the selected line; with Shift they extend the selection. %1 comments on the selected lines; Shift+%1 suggests a change to them.").arg(enter);
		}

		/// [[RFC-BPR-0090]]. Tells its owner when the watched widget resizes.
		class resize_follower final : public QObject
		{
		public:
			resize_follower(QWidget& aWatched, std::function<void()> aResized)
				: QObject{ &aWatched }
				, Resized{ std::move(aResized) }
			{
				aWatched.installEventFilter(this);
			}

		protected:
			auto eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool override
			{
				if (aEvent->type() == QEvent::Resize && Resized) {
					Resized();
				}
				return QObject::eventFilter(aWatched, aEvent);
			}

		private:
			std::function<void()> Resized;
		};
	}

	void diff_workbench::initialize_source_find()
	{
		Find         = std::make_unique<find_presentation>();
		Find->Bar    = new source_find_bar{ this };
		Find->Recent = Right;
		// [[BPR-1503]]. The find widget floats over the focused pane and is in
		// no layout, so opening it never takes height from the code.
		Find->Bar->Dispatch = [this](const source_find_gesture aGesture) { dispatch_source_find(aGesture); };
		Find->Bar->installEventFilter(this);
	}

	void diff_workbench::place_source_find()
	{
		if (!Find || Find->Bar->isHidden()) {
			return;
		}
		const QPlainTextEdit* target = Find->Target ? Find->Target.data() : OnePaneEnabled ? Unified
																						   : Right;
		if (!target->isVisible()) {
			target = OnePaneEnabled ? Unified : Right->isVisible() ? Right
																   : Left;
		}
		const auto* viewport = target->viewport();
		const auto  origin   = viewport->mapTo(this, QPoint{ 0, 0 });
		const auto  area     = QRect{ origin, viewport->size() };
		// Inset from the pane's top-right corner as VS Code's widget is. A
		// narrow pane keeps every control: the panel then extends left over
		// the neighbouring pane rather than dropping any of them.
		constexpr auto preferredInset = 8;
		const auto     least          = Find->Bar->fit_to(this->width() - 2 * preferredInset);
		// [[BPR-1527]]. Keep every control at its minimum before reserving
		// outer breathing room; a 242px surface can hold the 230px panel.
		const auto inset       = std::clamp((this->width() - least) / 2, 0, preferredInset);
		const auto wanted      = std::min(Find->Bar->preferred_width(), area.width() - 2 * inset);
		const auto panelWidth  = std::min(std::max(least, wanted), std::max(least, this->width() - 2 * inset));
		const auto panelHeight = Find->Bar->sizeHint().height();
		// Where even that runs out of room on the left, the anchor slides right
		// toward the surface's edge before any control is squeezed.
		const auto left = std::max(0, std::min(area.right() - preferredInset - panelWidth + 1, this->width() - inset - panelWidth));
		Find->Bar->setGeometry(left, area.top() + preferredInset / 2, panelWidth, panelHeight);
		Find->Bar->raise();
	}

	void diff_workbench::set_source_find_dispatch(std::function<void(const source_find_intent&)> aDispatch) { Find->Dispatch = std::move(aDispatch); }

	void diff_workbench::set_source_find_changed(std::function<void()> aChanged) { Find->Changed = std::move(aChanged); }

	auto diff_workbench::source_find() const -> source_find_bar* { return Find->Bar; }

	auto diff_workbench::source_find_available() const -> bool
	{
		if (!CurrentResult || !SemanticIndex || !isVisible()) {
			return false;
		}
		if (!OnePaneEnabled) {
			const auto* focus = QApplication::focusWidget();
			if (focus == Left) {
				return CurrentResult->LeftDocument.has_value();
			}
			if (focus == Right) {
				return CurrentResult->RightDocument.has_value();
			}
		}
		return CurrentResult->LeftDocument || CurrentResult->RightDocument;
	}

	auto diff_workbench::source_find_navigable() const -> bool { return Find->Current.has_value() || Find->ClosedNavigable; }

	auto diff_workbench::source_byte_at(QPlainTextEdit& aPane, const int aPosition) const -> std::optional<std::pair<core::diff_side, std::size_t>>
	{
		if (!CurrentResult) {
			return std::nullopt;
		}
		const auto block = aPane.document()->findBlock(aPosition);
		if (!block.isValid()) {
			return std::nullopt;
		}
		auto side = &aPane == Left ? core::diff_side::left : core::diff_side::right;
		auto line = static_cast<std::uint32_t>(block.blockNumber());
		if (&aPane == Unified) {
			if (line >= UnifiedPresentation->Rows.size()) {
				return std::nullopt;
			}
			const auto& row = UnifiedPresentation->Rows[line];
			side            = row.Head ? core::diff_side::right : core::diff_side::left;
			line            = row.Head.value_or(row.Base.value_or(0U));
		}
		const auto& document = side == core::diff_side::left ? CurrentResult->LeftDocument : CurrentResult->RightDocument;
		if (!document) {
			return std::nullopt;
		}
		if (line == document->Lines.size() && aPosition == aPane.document()->characterCount() - 1) {
			return std::pair{ side, document->Bytes.size() };
		}
		if (line >= document->Lines.size()) {
			return std::nullopt;
		}
		auto      offset = static_cast<std::size_t>(document->Lines[line].Content.Begin);
		int       units  = 0;
		const int wanted = aPosition - block.position();
		while (units < wanted && offset < document->Lines[line].Content.End) {
			const auto lead  = document->Bytes[offset];
			const auto count = (lead & 0xF8U) == 0xF0U ? 4U : (lead & 0xF0U) == 0xE0U ? 3U
				: (lead & 0xE0U) == 0xC0U                                             ? 2U
																					  : 1U;
			offset += count;
			units += count == 4U ? 2 : 1;
		}
		return units == wanted ? std::optional{ std::pair{ side, offset } } : std::nullopt;
	}

	auto diff_workbench::source_text_position(const core::diff_side aSide, const std::size_t aByte) -> std::optional<int>
	{
		if (!CurrentResult) {
			return std::nullopt;
		}
		auto& positions = Find->Positions[aSide == core::diff_side::left ? 0U : 1U];
		if (const auto saved = positions.find(aByte); saved != positions.end()) {
			return saved->second;
		}
		const auto& document = aSide == core::diff_side::left ? CurrentResult->LeftDocument : CurrentResult->RightDocument;
		if (!document || aByte > document->Bytes.size()) {
			return std::nullopt;
		}
		auto* editor = OnePaneEnabled ? Unified : aSide == core::diff_side::left ? Left
																				 : Right;
		if (aByte == document->Bytes.size() && (document->Bytes.empty() || document->HasFinalNewline)) {
			const auto end = editor->document()->characterCount() - 1;
			positions.emplace(aByte, end);
			return end;
		}

		if (document->Lines.empty()) {
			return aByte == 0U ? std::optional{ 0 } : std::nullopt;
		}
		const auto  after = std::ranges::upper_bound(document->Lines, aByte, { }, [](const core::decoded_text_line& aLine) { return aLine.Content.Begin; });
		const auto  index = after == document->Lines.begin() ? 0U : static_cast<std::size_t>(after - document->Lines.begin() - 1);
		const auto& line  = document->Lines[index];
		const auto  row   = OnePaneEnabled ? unified_row_for(aSide, static_cast<std::uint32_t>(index)) : std::optional{ static_cast<std::uint32_t>(index) };
		if (!row) {
			return std::nullopt;
		}
		const auto block = editor->document()->findBlockByNumber(static_cast<int>(*row));
		if (!block.isValid()) {
			return std::nullopt;
		}
		auto begin     = static_cast<std::size_t>(line.Content.Begin);
		auto native    = block.position();
		auto preceding = positions.upper_bound(aByte);
		if (preceding != positions.begin()) {
			--preceding;
			if (preceding->first >= line.Content.Begin && preceding->first <= line.Content.End) {
				begin  = preceding->first;
				native = preceding->second;
			}
		}
		const auto end = std::min<std::uint64_t>(aByte, line.Content.End);
		++Statistics.FindEndpointConversions;
		for (auto offset = begin; offset < end;) {
			const auto lead  = document->Bytes[offset];
			const auto count = (lead & 0xF8U) == 0xF0U ? 4U : (lead & 0xF0U) == 0xE0U ? 3U
				: (lead & 0xE0U) == 0xC0U                                             ? 2U
																					  : 1U;
			offset += count;
			if (offset > end) {
				return std::nullopt;
			}
			native += count == 4U ? 2 : 1;
		}
		positions.emplace(aByte, native);
		return native;
	}

	auto diff_workbench::source_match_fragments(const core::source_match_location& aLocation) -> const std::vector<std::pair<int, int>>&
	{
		const auto key = std::pair{ aLocation.Side, aLocation.Span };
		if (const auto saved = Find->Fragments.find(key); saved != Find->Fragments.end()) {
			return saved->second;
		}
		const auto first = source_text_position(aLocation.Side, aLocation.Span.Begin);
		const auto last  = source_text_position(aLocation.Side, aLocation.Span.End);
		if (!first || !last) {
			return Find->EmptyFragments;
		}
		if (!OnePaneEnabled || aLocation.Span.empty()) {
			return Find->Fragments.emplace(key, std::vector<std::pair<int, int>>{ { *first, *last } }).first->second;
		}
		const auto& document = aLocation.Side == core::diff_side::left ? CurrentResult->LeftDocument : CurrentResult->RightDocument;
		if (!document) {
			return Find->EmptyFragments;
		}
		auto pieces = std::vector<std::pair<int, int>>{ };
		auto line   = std::ranges::upper_bound(document->Lines, aLocation.Span.Begin, { }, [](const core::decoded_text_line& aLine) { return aLine.Content.Begin; });
		if (line != document->Lines.begin()) {
			--line;
		}
		for (; line != document->Lines.end() && line->Content.Begin < aLocation.Span.End; ++line) {
			const auto from = source_text_position(aLocation.Side, std::max(line->Content.Begin, aLocation.Span.Begin));
			const auto to   = source_text_position(aLocation.Side, std::min(line->Content.End, aLocation.Span.End));
			if (!from || !to) {
				return Find->EmptyFragments;
			}
			auto end = *to;
			if (aLocation.Span.End >= line->Terminator.End && line->Terminator.End > line->Content.End) {
				end = std::min(end + 1, Unified->document()->characterCount() - 1);
			}
			if (!pieces.empty() && pieces.back().second == *from) {
				pieces.back().second = end;
			} else {
				pieces.emplace_back(*from, end);
			}
		}
		return Find->Fragments.emplace(key, std::move(pieces)).first->second;
	}

	auto diff_workbench::source_search_seed() const -> QString
	{
		const auto* target  = OnePaneEnabled ? Unified : Find->Recent && Find->Recent->isVisible() ? Find->Recent.data()
																								   : Right;
		const auto* focused = QApplication::focusWidget();
		if (focused == Left || focused == Right || focused == Unified) {
			target = static_cast<const QPlainTextEdit*>(focused);
		}
		const auto cursor = target->textCursor();
		if (!cursor.hasSelection()) {
			return { };
		}
		if (cursor.selectionEnd() - cursor.selectionStart() > static_cast<int>(core::maximum_source_search_query_bytes)) {
			return QString{ static_cast<qsizetype>(core::maximum_source_search_query_bytes + 1U), QLatin1Char(' ') };
		}
		return cursor.selectedText().replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
	}

	void diff_workbench::open_source_find()
	{
		if (!source_find_available()) {
			return;
		}
		auto* target = OnePaneEnabled ? Unified : Find->Recent && Find->Recent->isVisible() && ((Find->Recent == Left && CurrentResult->LeftDocument) || (Find->Recent == Right && CurrentResult->RightDocument)) ? Find->Recent.data()
			: CurrentResult->RightDocument                                                                                                                                                                        ? Right
																																																				  : Left;
		if (auto* focused = QApplication::focusWidget(); focused == Left || focused == Right || focused == Unified) {
			target = static_cast<QPlainTextEdit*>(focused);
		}
		const bool rebind   = Find->Rebind;
		const bool retarget = rebind || Find->Target != target || Find->Bar->isHidden();
		Find->Rebind        = false;
		if (!retarget) {
			Find->Bar->seed({ }, false);
			return;
		}
		Find->Target = target;
		Find->Fixed.reset();
		Find->Capture.reset();
		Find->Bar->clear_selection();
		auto& intent          = Find->Intent;
		intent.Target         = target == Unified ? core::source_search_target::unified : target == Left ? core::source_search_target::left
																										 : core::source_search_target::right;
		auto       cursor     = target->textCursor();
		const auto first      = source_byte_at(*target, cursor.selectionStart());
		const auto last       = source_byte_at(*target, cursor.selectionEnd());
		bool       contiguous = first && last && first->first == last->first;
		if (contiguous && target == Unified) {
			const auto beginRow = target->document()->findBlock(cursor.selectionStart()).blockNumber();
			const auto endRow   = target->document()->findBlock(std::max(cursor.selectionStart(), cursor.selectionEnd() - 1)).blockNumber();
			auto       previous = std::optional<std::uint32_t>{ };
			for (int row = beginRow; row <= endRow && contiguous; ++row) {
				const auto& identity = UnifiedPresentation->Rows[static_cast<std::size_t>(row)];
				const auto  actual   = first->first == core::diff_side::left ? identity.Base : identity.Head;
				contiguous           = actual && (!previous || *actual == *previous + 1U);
				previous             = actual;
			}
		}
		if (cursor.hasSelection() && contiguous && last->second > first->second) {
			Find->Capture = core::source_search_range{ first->first, { first->second, last->second } };
		}
		const auto caretIndex = target == Left ? 0U : target == Right ? 1U
																	  : 2U;
		const bool largeSeed  = cursor.hasSelection() && cursor.selectionEnd() - cursor.selectionStart() > static_cast<int>(core::maximum_source_search_query_bytes);
		auto       seed       = largeSeed ? QString{ static_cast<qsizetype>(core::maximum_source_search_query_bytes + 1U), QLatin1Char(' ') } : cursor.hasSelection() ? cursor.selectedText().replace(QChar::ParagraphSeparator, QLatin1Char('\n'))
																																									  : QString{ };
		// [[BPR-1503]]. As VS Code does, the current match is the occurrence
		// the seed came from: a selection or the word under the caret is
		// searched from its start, so it is "1 of N" there rather than the
		// next occurrence after it.
		auto originPosition = Find->Recall || target->textCursor().hasSelection() ? target->textCursor().selectionStart() : target->textCursor().position();
		if (seed.isEmpty() && Find->RealCaret[caretIndex]) {
			cursor.select(QTextCursor::WordUnderCursor);
			seed = cursor.selectedText();
			if (!seed.isEmpty()) {
				originPosition = cursor.selectionStart();
			}
		}
		auto origin = source_byte_at(*target, originPosition);
		if (!Find->RealCaret[caretIndex] && !target->textCursor().hasSelection() && Interaction.Selection) {
			const auto  side     = Interaction.Selection->PreferredLine.Side;
			const auto& document = side == core::diff_side::left ? CurrentResult->LeftDocument : CurrentResult->RightDocument;
			if (document && Interaction.Selection->PreferredLine.Line < document->Lines.size()) {
				origin = std::pair{ side, document->Lines[Interaction.Selection->PreferredLine.Line].Content.Begin };
			}
		}
		intent.OriginSide = origin ? origin->first : target == Left ? core::diff_side::left
																	: core::diff_side::right;
		intent.OriginByte = origin ? origin->second : 0U;
		intent.OriginRow  = static_cast<std::uint32_t>(std::max(0, target->textCursor().blockNumber()));
		Find->Bar->set_scope(intent.Target, Find->Capture.has_value(), target == Unified ? QStringLiteral("Select a contiguous range from one actual source version") : QStringLiteral("Select source text first"));
		Find->Bar->show();
		place_source_find();
		Find->Bar->seed(Find->Recall || rebind ? QString{ } : seed, true);
		Find->Recall = false;
		dispatch_source_find(source_find_gesture::edit);
		if (Find->Changed) {
			Find->Changed();
		}
	}

	void diff_workbench::dispatch_source_find(const source_find_gesture aGesture)
	{
		if (aGesture == source_find_gesture::close && !Find->Bar->isHidden()) {
			close_source_find();
			return;
		}
		auto intent    = Find->Intent;
		intent.Gesture = aGesture;
		intent.Query   = Find->Bar->query();
		intent.Options = Find->Bar->options();
		if (aGesture == source_find_gesture::selection) {
			Find->Fixed = Find->Bar->in_selection() ? Find->Capture : std::nullopt;
		}
		intent.Selection = Find->Fixed;
		if (aGesture == source_find_gesture::next || aGesture == source_find_gesture::previous) {
			Find->ExternalSelection = false;
			Find->Navigating        = true;
		}
		if (aGesture == source_find_gesture::query_dirty) {
			Find->ExternalSelection = false;
			Find->ClosedNavigable   = false;
			Find->Records.reset();
			Find->PendingReveal.reset();
			Find->Current.reset();
			for (auto& list : Find->Decorations) {
				list.clear();
			}
			update_diff_paint();
			refresh_find_ticks();
		}
		if (Find->Dispatch) {
			Find->Dispatch(intent);
		}
	}

	void diff_workbench::navigate_source_find(const core::change_navigation_direction aDirection)
	{
		if (Find->Bar->isHidden()) {
			Find->ReopenDirection = aDirection;
			Find->Recall          = true;
			open_source_find();
			return;
		}
		dispatch_source_find(aDirection == core::change_navigation_direction::next ? source_find_gesture::next : source_find_gesture::previous);
	}

	void diff_workbench::close_source_find()
	{
		Find->ReopenDirection.reset();
		Find->ClosedNavigable = Find->Current.has_value();
		Find->PendingReveal.reset();
		Find->Bar->hide();
		dispatch_source_find(source_find_gesture::close);
		Find->Records.reset();
		Find->Current.reset();
		for (auto& list : Find->Decorations) {
			list.clear();
		}
		update_diff_paint();
		refresh_find_ticks();
		if (Find->Target && Find->Target->isVisible()) {
			Find->Target->setFocus(Qt::ShortcutFocusReason);
		}
		if (Find->Changed) {
			Find->Changed();
		}
	}

	void diff_workbench::retarget_source_find()
	{
		clear_source_find_document();
		if (Find->Rebind) {
			QTimer::singleShot(0, this, [this] {
				if (Find->Rebind && !Find->Bar->isHidden() && source_find_available()) {
					open_source_find();
				}
			});
		}
	}

	void diff_workbench::clear_source_find_document()
	{
		if (!Find) {
			return;
		}
		Find->CopyMatch.reset();
		Find->ReopenDirection.reset();
		Find->MappedRecords.reset();
		Find->ClosedNavigable = false;
		Find->Rebind          = !Find->Bar->isHidden();
		Find->PendingReveal.reset();
		Find->Fragments.clear();
		for (auto& positions : Find->Positions) {
			positions.clear();
		}
		Find->Capture.reset();
		Find->Fixed.reset();
		Find->Records.reset();
		Find->Current.reset();
		Find->RealCaret = { };
		Find->Bar->clear_selection();
		for (auto& list : Find->Decorations) {
			list.clear();
		}
		dispatch_source_find(source_find_gesture::document_changed);
		refresh_find_ticks();
		if (Find->Changed) {
			Find->Changed();
		}
	}

	auto diff_workbench::reveal_source_match(const core::source_match_location& aLocation, const bool aExternal) -> bool
	{
		if (!CurrentContext || !CurrentResult || !SemanticIndex || geometry_pending()) {
			return false;
		}
		const auto blob = aLocation.Side == core::diff_side::left ? CurrentContext->Key.LeftBlob : CurrentContext->Key.RightBlob;
		if (blob != std::optional{ aLocation.Blob }) {
			return false;
		}
		if (aExternal) {
			Find->ExternalSelection = true;
			Find->PendingReveal.reset();
		}
		const auto& pieces = source_match_fragments(aLocation);
		if (pieces.empty()) {
			return false;
		}
		const auto first = std::optional{ pieces.front().first };
		const auto last  = std::optional{ pieces.front().second };
		if (!first || !last || *last < *first) {
			return false;
		}
		const auto  line     = core::source_line_ref{ CurrentResult->Alignment.Digest, aLocation.Side, aLocation.Line };
		const auto& document = aLocation.Side == core::diff_side::left ? CurrentResult->LeftDocument : CurrentResult->RightDocument;
		const bool  terminal = document && aLocation.Span.empty() && aLocation.Span.Begin == document->Bytes.size() && aLocation.Line == document->Lines.size();
		const auto  selected = core::select_line(*SemanticIndex, line);
		if (!selected && !terminal) {
			return false;
		}
		// Qt's cursor may scroll its own pane: restore the comparison position,
		// then perform the only vertical reveal through the existing controller.
		const auto shared = Controller->position();
		auto*      editor = OnePaneEnabled ? Unified : aLocation.Side == core::diff_side::left ? Left
																							   : Right;
		auto       cursor = QTextCursor{ editor->document() };
		// Keep the caret on the first fragment that the reveal shows. A match
		// wider than its pane cannot show both ends; leaving the caret at the
		// end then scrolling to its beginning leaves the active cursor outside
		// the viewport even though the exact match is selected.
		cursor.setPosition(*last);
		cursor.setPosition(*first, QTextCursor::KeepAnchor);
		editor->setTextCursor(cursor);
		Find->CopyMatch  = aLocation;
		Find->CopyCursor = cursor;
		if (!OnePaneEnabled) {
			static_cast<void>(Controller->set_position(shared));
		}
		Interaction.Selection = selected;
		if (selected) {
			static_cast<void>(reveal_exact_line(*selected, line));
		} else if (OnePaneEnabled) {
			reveal_unified_row(static_cast<std::uint32_t>(std::max(0, editor->document()->findBlock(*first).blockNumber())));
		} else {
			Controller->end();
		}
		auto beginCursor = cursor;
		beginCursor.setPosition(*first);
		const auto native = editor->cursorRect(beginCursor);
		if (native.left() < 0) {
			editor->horizontalScrollBar()->setValue(editor->horizontalScrollBar()->value() + native.left());
		} else if (native.right() > editor->viewport()->width()) {
			editor->horizontalScrollBar()->setValue(editor->horizontalScrollBar()->value() + native.right() - editor->viewport()->width());
		}
		update_detail();
		update_diff_paint();
		emit_selection();
		return true;
	}

	void diff_workbench::observe_source_find_execution(const std::uint64_t aMatching, const std::uint64_t aCached)
	{
		Statistics.FindMatchingRuns = aMatching;
		Statistics.FindCachedRuns   = aCached;
	}

	void diff_workbench::present_source_find(const application::source_search_projection& aProjection)
	{
		if (aProjection.Scope && (!CurrentContext || !CurrentResult || aProjection.Scope->Document != std::optional{ core::source_search_document{ CurrentContext->Key.File, CurrentResult->Alignment.Digest, CurrentContext->Key.LeftBlob, CurrentContext->Key.RightBlob } })) {
			return;
		}
		const bool recordsChanged = Find->Records != aProjection.Matches;
		if (recordsChanged) {
			if (aProjection.Matches && Find->MappedRecords.lock() != aProjection.Matches) {
				Find->Fragments.clear();
				for (auto& positions : Find->Positions) {
					positions.clear();
				}
				Find->MappedRecords = aProjection.Matches;
			}
			Find->Records = aProjection.Matches;
			for (auto& list : Find->Decorations) {
				list.clear();
			}
			if (aProjection.Matches && aProjection.Scope) {
				const auto target    = aProjection.Scope->Target;
				const auto paneIndex = target == core::source_search_target::left ? 0U : target == core::source_search_target::right ? 1U
																																	 : 2U;
				auto&      list      = Find->Decorations[paneIndex];
				for (std::size_t index = 0U; index < aProjection.Matches->size(); ++index) {
					const auto& record   = (*aProjection.Matches)[index].Match;
					const auto& location = target == core::source_search_target::left ? record.Locations[0] : record.Locations[1] ? record.Locations[1]
																																  : record.Locations[0];
					if (!location) {
						continue;
					}
					for (const auto& [begin, end] : source_match_fragments(*location)) {
						list.push_back({ begin, end, 0, static_cast<std::uint32_t>(index) });
					}
				}
				std::ranges::sort(list, { }, &find_presentation::decoration::Begin);
				int maximum = 0;
				for (auto& item : list) {
					maximum        = std::max(maximum, item.End);
					item.PrefixEnd = maximum;
				}
			}
		}
		if (aProjection.CurrentMatch && Find->ReopenDirection) {
			const auto direction = *Find->ReopenDirection;
			Find->ReopenDirection.reset();
			dispatch_source_find(direction == core::change_navigation_direction::next ? source_find_gesture::next : source_find_gesture::previous);
			return;
		}
		const bool reveal = aProjection.CurrentMatch && !Find->ExternalSelection && (Find->Navigating || Find->Current != aProjection.CurrentMatch || Find->Bar->projection().Matches != aProjection.Matches);
		Find->Current     = aProjection.CurrentMatch;
		Find->Bar->present(aProjection);
		if (reveal && aProjection.Matches && *aProjection.CurrentMatch < aProjection.Matches->size()) {
			const auto& record   = (*aProjection.Matches)[*aProjection.CurrentMatch].Match;
			const auto& location = aProjection.Scope && aProjection.Scope->Target == core::source_search_target::left ? record.Locations[0] : record.Locations[1] ? record.Locations[1]
																																								  : record.Locations[0];
			if (location) {
				if (Find->Navigating) {
					Find->Intent.OriginSide = location->Side;
					Find->Intent.OriginByte = location->Span.Begin;
					Find->Intent.OriginRow  = record.UnifiedRow;
				}
				Find->PendingReveal = *location;
				if (reveal_source_match(*location)) {
					Find->PendingReveal.reset();
				}
			}
		}
		Find->Navigating = false;
		update_diff_paint();
		if (recordsChanged) {
			refresh_find_ticks();
		}
		if (Find->Changed) {
			Find->Changed();
		}
	}

	void diff_workbench::append_find_selections(QPlainTextEdit& aPane, QList<QTextEdit::ExtraSelection>& aSelections)
	{
		if (!Find) {
			return;
		}
		auto& rings = pane(&aPane)->FindRings;
		rings.clear();
		if (Find->Bar->isHidden()) {
			return;
		}
		// [[BPR-1798]]. The rows' own selections, before any match is added:
		// the ground each match lands on is read from them. Grounds repeat
		// (a pane has a handful of row fills), so each ring colour is
		// derived once per ground.
		const auto  beneath    = aSelections.size();
		auto        ringColour = std::vector<std::pair<std::pair<core::appearance_color, bool>, QColor>>{ };
		const auto& records    = Find->Decorations[&aPane == Left ? 0U : &aPane == Right ? 1U
																						 : 2U];
		const auto  visible    = paintable_line_bounds(aPane);
		const auto  start      = aPane.document()->findBlockByNumber(static_cast<int>(visible.Begin));
		const auto  finish     = aPane.document()->findBlockByNumber(static_cast<int>(visible.End));
		const auto  begin      = start.isValid() ? start.position() : 0;
		const auto  end        = finish.isValid() ? finish.position() : aPane.document()->characterCount();
		auto        at         = std::ranges::lower_bound(records, begin, { }, &find_presentation::decoration::PrefixEnd);
		for (; at != records.end() && at->Begin < end; ++at) {
			if (at->End < begin) {
				continue;
			}
			++Statistics.FindHighlightsVisited;
			auto selection   = QTextEdit::ExtraSelection{ };
			selection.cursor = QTextCursor{ aPane.document() };
			selection.cursor.setPosition(at->Begin);
			selection.cursor.setPosition(at->End, QTextCursor::KeepAnchor);
			// [[BPR-1503]]. Every match carries the theme's find wash, and the
			// current one its stronger wash, composited at the selection's
			// opacity so the contrast checks hold for both. [[BPR-1798]]. The
			// wash alone measured 1.07:1 on the dark added row, so every match
			// also gets a ring, read against the ground it actually lands on.
			const bool current = Find->Current == std::optional{ at->Match };
			auto       color   = find_wash(current);
			color.setAlpha(core::selection_wash_alpha);
			selection.format.setBackground(color);
			aSelections.push_back(selection);
			const auto key    = std::pair{ find_ground_at(aPane, std::span{ aSelections }.first(static_cast<std::size_t>(beneath)), at->Begin), current };
			auto       cached = std::ranges::find(ringColour, key, &decltype(ringColour)::value_type::first);
			if (cached == ringColour.end()) {
				cached = ringColour.insert(ringColour.end(), { key, Colors.Supplied ? to_presentation_color(core::find_match_ring(Colors.Palette, key.first, current)) : QColor{ } });
			}
			rings.push_back({ .Cursor = selection.cursor, .Colour = cached->second, .Current = current });
		}
	}

	void diff_workbench::refresh_find_ticks()
	{
		if (!Find) {
			return;
		}
		auto shared  = std::vector<qreal>{ };
		auto unified = std::vector<qreal>{ };
		if (Find->Records && !Find->Bar->isHidden()) {
			for (const auto& record : *Find->Records) {
				const auto& location = Find->Intent.Target == core::source_search_target::left ? record.Match.Locations[0] : record.Match.Locations[1] ? record.Match.Locations[1]
																																					   : record.Match.Locations[0];
				if (!location) {
					continue;
				}
				if (const auto fraction = Controller->scrollbar_fraction(location->Side, location->Line)) {
					shared.push_back(*fraction);
				} else if (location->Span.empty()) {
					shared.push_back(1.0);
				}
				if (const auto row = unified_row_for(location->Side, location->Line)) {
					unified.push_back(static_cast<qreal>(*row) / std::max(1, Unified->document()->blockCount() - 1));
				}
			}
		}
		static_cast<thread_tick_scroll_bar*>(SharedScroll)->set_find_ticks(std::move(shared));
		static_cast<thread_tick_scroll_bar*>(Unified->verticalScrollBar())->set_find_ticks(std::move(unified));
	}

	diff_workbench::diff_workbench(QWidget* const aParent)
		: QWidget{ aParent }
		, UnifiedPresentation{ std::make_unique<unified_presentation>() }
	{
		setObjectName(QStringLiteral("diffWorkbench"));
		setAccessibleName(QStringLiteral("File comparison workbench"));
		setAccessibleDescription(QStringLiteral(
			"Linked natural-layout source panes, change correspondence map, and line details"));

		auto* const root = new QVBoxLayout{ this };
		root->setContentsMargins(0, 0, 0, 0);
		root->setSpacing(6);
		Status = new QLabel{ QStringLiteral("Select a file comparison"), this };
		Status->setTextFormat(Qt::PlainText);
		Status->setWordWrap(true);
		Status->setObjectName(QStringLiteral("diffWorkbenchStatus"));
		Status->setAccessibleName(QStringLiteral("Comparison status"));
		Status->setAccessibleDescription(QStringLiteral(
			"Loading, failure, and semantic change count for the selected comparison"));
		Status->setTextInteractionFlags(Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
		root->addWidget(Status);

		Vertical = new line_details_splitter{ this };
		Vertical->setObjectName(QStringLiteral("diffWorkbenchVerticalSplitter"));
		Vertical->setChildrenCollapsible(false);
		auto* const top       = new QWidget{ Vertical };
		auto* const topLayout = new QHBoxLayout{ top };
		topLayout->setContentsMargins(0, 0, 0, 0);
		topLayout->setSpacing(0);
		Comparison = new diff_comparison_splitter{ top };
		Comparison->setObjectName(QStringLiteral("diffComparisonSplitter"));
		Comparison->setAccessibleName(QStringLiteral("Linked source comparison"));
		Comparison->setChildrenCollapsible(false);
		Left = new source_pane{ Comparison };
		Left->setObjectName(QStringLiteral("diffLeftSource"));
		Left->setAccessibleName(QStringLiteral("Earlier source"));
		Left->setAccessibleDescription(QStringLiteral("Read-only earlier file version. ") + line_keys_description());
		Right = new source_pane{ Comparison };
		Right->setObjectName(QStringLiteral("diffRightSource"));
		Right->setAccessibleName(QStringLiteral("Later source"));
		Right->setAccessibleDescription(QStringLiteral("Read-only later file version. ") + line_keys_description());
		for (auto* const source : { Left, Right }) {
			source->setReadOnly(true);
			source->setLineWrapMode(QPlainTextEdit::NoWrap);
			// [[BPR-1291]]. Lets the pane scroll until its last line is at the
			// top. Without it Qt stops a pane with its last line at the bottom.
			source->setCenterOnScroll(true);
			source->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			source->setFocusPolicy(Qt::StrongFocus);
			source->viewport()->installEventFilter(this);
		}
		Comparison->addWidget(Left);
		Comparison->addWidget(Right);
		Comparison->setSizes({ 500, 500 });
		SharedScroll = new thread_tick_scroll_bar{ Qt::Vertical, top };
		SharedScroll->setObjectName(QStringLiteral("diffSharedScrollBar"));
		SharedScroll->setAccessibleName(QStringLiteral("Shared comparison position"));
		SharedScroll->setAccessibleDescription(QStringLiteral(
			"Moves both source panes through semantic comparison space; Shift-click a thread's tick to go to that thread"));
		SharedScroll->setFocusPolicy(Qt::StrongFocus);
		topLayout->addWidget(Comparison, 1);
		topLayout->addWidget(SharedScroll);
		UnifiedMap = new one_pane_diff_link_map{ top };
		Unified    = new source_pane{ top };
		Unified->setVerticalScrollBar(new thread_tick_scroll_bar{ Qt::Vertical, Unified });
		Unified->setObjectName(QStringLiteral("diffUnifiedSource"));
		Unified->setAccessibleName(QStringLiteral("Unified source comparison"));
		Unified->setAccessibleDescription(QStringLiteral("Base and head line numbers; removed lines followed by added lines in each change. ") + line_keys_description());
		Unified->setReadOnly(true);
		Unified->setLineWrapMode(QPlainTextEdit::NoWrap);
		Unified->setCenterOnScroll(true);
		Unified->setFocusPolicy(Qt::StrongFocus);
		pane(Unified)->set_unified_rows(&UnifiedPresentation->Rows);
		Unified->viewport()->installEventFilter(this);
		Unified->installEventFilter(this);
		UnifiedMap->installEventFilter(this);
		UnifiedMap->setFocusProxy(Unified);
		topLayout->addWidget(UnifiedMap);
		topLayout->addWidget(Unified, 1);
		UnifiedMap->hide();
		Unified->hide();
		UnifiedMap->set_selection_sink([this](const diff_link_map_identity& aIdentity) { select_link(aIdentity, true); });
		connect(Unified->verticalScrollBar(), &QScrollBar::valueChanged, this, [this] {
			if (OnePaneEnabled) {
				update_unified_paint();
				++Statistics.FrameApplications;
				if (FrameObserver) {
					FrameObserver();
				}
			}
		});

		// [[BPR-1334]]. Line details: the current line, base over head, full
		// width and wrapped, under the comparison. It replaces the compact
		// detail pane in the same place rather than adding a second one.
		Detail = new line_details_pane{ Vertical };
		Detail->present_empty();
		// [[BPR-1287]]. The panes share their splitter slot with the idle
		// direction, one shown at a time, so the splitter keeps its two
		// children and the review details still open below either.
		auto* const slot = new QWidget{ Vertical };
		PanesSlot        = new QStackedLayout{ slot };
		PanesSlot->setContentsMargins(0, 0, 0, 0);
		PanesSlot->addWidget(top);
		PanesTop = top;
		// [[BPR-1576]]. The comparison's wait is said over the panes in the
		// shape of code; the status line above keeps saying the outcome.
		Veil = new pending_veil{ *this, pending_shape::code, QStringLiteral("Comparison"), top };
		Veil->set_announces(false);
		Vertical->addWidget(slot);
		Vertical->addWidget(Detail);
		// The comparison takes every change in the window's height; the strip
		// keeps the height the reader chose.
		Vertical->setStretchFactor(0, 1);
		Vertical->setStretchFactor(1, 0);
		Vertical->LineHeight = [this] { return Detail->fontMetrics().lineSpacing(); };
		Vertical->setSizes({ 500, Vertical->default_height() });
		// QSplitter names its handles after the widget they follow, so the
		// seam takes its own name once the strip is in place.
		if (auto* const seam = Vertical->handle(1); seam != nullptr) {
			seam->setObjectName(QStringLiteral("lineDetailsResize"));
		}
		root->addWidget(Vertical, 1);
		initialize_source_find();
		for (auto* const source : { Left, Right, Unified }) {
			connect(source, &QPlainTextEdit::selectionChanged, this, [this] { queue_line_details(); });
		}

		// [[BPR-1287]]. What the area says while no file comparison is chosen,
		// in place of two empty panes and a bare status.
		IdlePage = new QWidget{ slot };
		IdlePage->setObjectName(QStringLiteral("diffWorkbenchIdle"));
		IdlePage->setAccessibleName(QStringLiteral("No file open"));
		auto* const idleColumn = new QVBoxLayout{ IdlePage };
		idleColumn->setContentsMargins(24, 24, 24, 24);
		idleColumn->setSpacing(8);
		idleColumn->addStretch(2);
		// Interface text, not code: the workbench's own font is the code
		// typography, which the panes inherit.
		auto interfaceFont = QApplication::font();
		interfaceFont.setFamilies(interfaceFont.families());
		interfaceFont.setPointSizeF(interfaceFont.pointSizeF());
		interfaceFont.setStyleHint(interfaceFont.styleHint());
		interfaceFont.setWeight(interfaceFont.weight());
		IdlePage->setFont(interfaceFont);
		IdleTitle = new QLabel{ IdlePage };
		IdleTitle->setObjectName(QStringLiteral("diffWorkbenchIdleTitle"));
		IdleTitle->setTextFormat(Qt::PlainText);
		IdleTitle->setAlignment(Qt::AlignHCenter);
		auto titleFont = interfaceFont;
		titleFont.setPointSizeF(interfaceFont.pointSizeF() * 1.35);
		titleFont.setWeight(QFont::DemiBold);
		IdleTitle->setFont(titleFont);
		IdleDetail = new QLabel{ IdlePage };
		IdleDetail->setObjectName(QStringLiteral("diffWorkbenchIdleDetail"));
		IdleDetail->setTextFormat(Qt::PlainText);
		IdleDetail->setAlignment(Qt::AlignHCenter);
		IdleDetail->setWordWrap(true);
		IdleDetail->setForegroundRole(QPalette::PlaceholderText);
		idleColumn->addWidget(IdleTitle);
		idleColumn->addWidget(IdleDetail);
		auto* const idleActions = new QHBoxLayout{ };
		idleActions->setSpacing(8);
		idleActions->addStretch(1);
		IdleOpenFirst = new quiet_push_button{ QStringLiteral("Open first file"), IdlePage };
		IdleOpenFirst->setObjectName(QStringLiteral("diffWorkbenchIdleOpenFirst"));
		IdleOpenFirst->setDefault(true);
		IdleShowFiles = new quiet_push_button{ QStringLiteral("Show files"), IdlePage };
		IdleShowFiles->setObjectName(QStringLiteral("diffWorkbenchIdleShowFiles"));
		idleActions->addWidget(IdleOpenFirst);
		idleActions->addWidget(IdleShowFiles);
		idleActions->addStretch(1);
		idleColumn->addLayout(idleActions);
		idleColumn->addStretch(3);
		PanesSlot->addWidget(IdlePage);
		connect(IdleOpenFirst, &QPushButton::clicked, this, [this] {
			if (IdleDirection.OpenFirst) {
				IdleDirection.OpenFirst();
			}
		});
		connect(IdleShowFiles, &QPushButton::clicked, this, [this] {
			if (IdleDirection.ShowFiles) {
				IdleDirection.ShowFiles();
			}
		});
		set_idle_direction({ .Title = QStringLiteral("No file open"), .Detail = QStringLiteral("Choose a file to see its changes.") });
		set_idle(true);

		LinkMap = Comparison->link_map();
		LinkMap->set_selection_announcements_enabled(false);
		Controller = new comparison_scroll_controller{ *SharedScroll, { }, this };
		Controller->bind_input_surface(
			comparison_scroll_receiver::left_pane,
			*Left->viewport(),
			Left->horizontalScrollBar());
		Controller->bind_input_surface(
			comparison_scroll_receiver::right_pane,
			*Right->viewport(),
			Right->horizontalScrollBar());
		Controller->bind_input_surface(
			comparison_scroll_receiver::link_map, *LinkMap);
		Controller->bind_key_surface(
			comparison_scroll_receiver::left_pane, *Left);
		Controller->bind_key_surface(
			comparison_scroll_receiver::right_pane, *Right);
		Controller->bind_key_surface(
			comparison_scroll_receiver::link_map, *LinkMap);
		Controller->set_frame_sink(
			[this](const comparison_scroll_frame& aFrame) { apply_frame(aFrame); });
		Controller->set_repaint_sink([this] { LinkMap->update(); });
		LinkMap->set_selection_sink(
			[this](const diff_link_map_identity& aIdentity) {
				select_link(aIdentity, false);
			});
		LinkMap->set_reveal_sink(
			[this](const diff_link_map_identity& aIdentity) {
				select_link(aIdentity, true);
			});

		// [[BPR-1212]]. After the scroll controller's filters, so the line
		// cursor sees Up, Down and Return on a pane first.
		for (auto* const source : { Left, Right, Unified }) {
			source->installEventFilter(this);
		}
		// [[BPR-1336]]. ⌘-scroll and pinch over any code surface are zoom
		// gestures. The controller lets a Control-modified wheel through, so
		// this filter sees it whichever was installed first.
		LinkMap->installEventFilter(this);
		for (auto* const source : { Left, Right, Unified }) {
			pane(source)->set_font_observer([this, source] { hold_reading_position(*source); });
			// [[BPR-1724]]. A click, Shift-click or drag on the numbers.
			pane(source)->set_number_dispatch([this, source](const std::uint32_t aRow, const bool aExtend) { select_numbered_line(source, aRow, aExtend); });
		}
		pane(Unified)->set_scroll_observer([this] {
			update_unified_paint();
			if (FrameObserver) {
				FrameObserver();
			}
		});
		ZoomIndicator      = new zoom_pill{ this };
		ZoomIndicatorTimer = new QTimer{ this };
		ZoomIndicatorTimer->setSingleShot(true);
		ZoomIndicatorTimer->setInterval(1200);
		connect(ZoomIndicatorTimer, &QTimer::timeout, ZoomIndicator, &QWidget::hide);

		QWidget::setTabOrder(Left, LinkMap);
		QWidget::setTabOrder(LinkMap, Right);
		QWidget::setTabOrder(Right, SharedScroll);
		QWidget::setTabOrder(SharedScroll, Detail);
	}

	auto diff_workbench::head_edit_row(const std::uint32_t aHeadLine) const -> std::uint64_t
	{
		const auto& edit = *HeadEdit;
		if (aHeadLine < edit.First) {
			return aHeadLine;
		}
		if (aHeadLine <= edit.Last) {
			return edit.First + std::min<std::uint64_t>(aHeadLine - edit.First, edit.count() - 1U);
		}
		return std::uint64_t{ aHeadLine } + edit.count() - edit.OriginalCount;
	}

	auto diff_workbench::head_line_of_edit_row(const std::uint64_t aRow) const -> std::uint32_t
	{
		const auto& edit  = *HeadEdit;
		const auto  count = edit.count();
		if (aRow < edit.First) {
			return static_cast<std::uint32_t>(aRow);
		}
		if (aRow < edit.First + count) {
			return edit.First + static_cast<std::uint32_t>(std::min<std::uint64_t>(aRow - edit.First, edit.OriginalCount - 1U));
		}
		return static_cast<std::uint32_t>(aRow - count + edit.OriginalCount);
	}

	auto diff_workbench::begin_head_edit(const head_edit_request& aRequest) -> suggestion_editor*
	{
		end_head_edit();
		if (!CurrentResult || !CurrentResult->RightDocument.has_value() || aRequest.First > aRequest.Last) {
			return nullptr;
		}
		const auto& document = *CurrentResult->RightDocument;
		if (document.Bytes.size() > core::maximum_suggestion_editor_bytes || aRequest.Last >= document.Lines.size()) {
			return nullptr;
		}
		auto text  = std::string{ document.Bytes.begin(), document.Bytes.end() };
		auto lines = core::split_text_lines(text);
		if (aRequest.Last >= lines.size()) {
			return nullptr;
		}
		auto spanCount = static_cast<std::uint64_t>(aRequest.Last - aRequest.First) + 1U;
		if (aRequest.Span.has_value() && !aRequest.Span->empty()) {
			const auto from = lines.begin() + aRequest.First;
			lines.erase(from, from + static_cast<std::ptrdiff_t>(spanCount));
			lines.insert(lines.begin() + aRequest.First, aRequest.Span->begin(), aRequest.Span->end());
			spanCount = aRequest.Span->size();
			text      = core::join_text_lines(lines);
		}

		auto edit           = std::make_unique<head_edit>();
		edit->First         = aRequest.First;
		edit->Last          = aRequest.Last;
		edit->OriginalCount = aRequest.Last - aRequest.First + 1U;
		edit->OnePane       = OnePaneEnabled;
		auto* const host    = OnePaneEnabled ? Unified : Right;
		edit->Host          = host;
		// The head pane's own fills, per head line.
		edit->Fills.assign(document.Lines.size(), QColor{ });
		if (Colors.Supplied) {
			for (const auto& segment : CurrentResult->Alignment.Segments) {
				const auto colour = moved(segment.Id) ? Colors.Moved
					: segment.Kind == core::alignment_kind::right_only || segment.Kind == core::alignment_kind::replacement
					? Colors.Added
					: QColor{ };
				for (auto line = segment.Right.Begin; colour.isValid() && line < segment.Right.End && line < edit->Fills.size(); ++line) {
					edit->Fills[line] = colour;
				}
			}
		}

		auto* const editor = new suggestion_editor{ host };
		editor->setObjectName(QStringLiteral("diffHeadSuggestionEditor"));
		editor->setAccessibleName(QStringLiteral("Suggested change to lines %1 to %2").arg(aRequest.First + 1U).arg(aRequest.Last + 1U));
		editor->setFrameShape(QFrame::NoFrame);
		editor->set_code_font(host->font());
		editor->set_contrast_variant(Colors.Variant);
		const auto column = std::max(1.0, QFontMetricsF{ host->font() }.horizontalAdvance(QLatin1Char{ 'M' }));
		const auto tab    = static_cast<std::uint32_t>(std::max(1.0, std::round(host->tabStopDistance() / column)));
		editor->set_text(text, tab);
		editor->set_indentation(core::indentation_for({ }, text));
		editor->set_editable_lines(aRequest.First, aRequest.First + spanCount - 1U);
		if (RightSpans) {
			const auto classified = classify_text_lines(std::span{ document.Bytes }, RightSpans);
			editor->set_syntax(suggestion_syntax_lines(classified, std::string_view{ reinterpret_cast<const char*>(document.Bytes.data()), document.Bytes.size() }, Colors.Syntax));
		}
		HeadEdit                    = std::move(edit);
		const auto gutterBackground = Colors.Syntax[static_cast<std::size_t>(core::appearance_role::code_gutter_background)];
		const auto gutterText       = Colors.Syntax[static_cast<std::size_t>(core::appearance_role::code_line_number_text)];
		auto       gutterWidth      = pane(Right)->gutter_width();
		if (OnePaneEnabled || gutterWidth <= 0) {
			const auto digits = QString::number(document.Lines.size() + spanCount).size();
			gutterWidth       = qCeil(QFontMetricsF{ host->font() }.horizontalAdvance(QString(digits, QLatin1Char{ '9' }))) + 3 * GutterPadding;
		}
		editor->set_gutter(suggestion_editor::gutter_presentation{
			.Width      = gutterWidth,
			.Padding    = GutterPadding,
			.Font       = host->font(),
			.Background = gutterBackground,
			.Text       = gutterText,
			.Number     = [this](const std::uint64_t aRow) -> QString {
				if (!HeadEdit) {
					return { };
				}
				const auto& current = *HeadEdit;
				// A row the edit added has no number in the head file yet.
				if (aRow >= current.First && aRow < current.First + current.count() && aRow - current.First >= current.OriginalCount) {
					return { };
				}
				return QString::number(head_line_of_edit_row(aRow) + 1U);
			},
		});
		editor->set_row_fill([this](const std::uint64_t aRow) -> std::optional<QColor> {
			if (!HeadEdit) {
				return std::nullopt;
			}
			const auto& current = *HeadEdit;
			if (aRow >= current.First && aRow < current.First + current.count() && aRow - current.First >= current.OriginalCount) {
				return std::nullopt;
			}
			const auto line = head_line_of_edit_row(aRow);
			return line < current.Fills.size() && current.Fills[line].isValid() ? std::optional{ current.Fills[line] } : std::nullopt;
		});
		editor->set_range_colours(aRequest.Wash, aRequest.Edge, aRequest.Flash);
		// The owner's note on the drawer-link look: the code that cannot
		// change is set back on the gutter's ground.
		if (Colors.Supplied) {
			editor->set_read_only_ground(gutterBackground);
		}
		editor->set_empty_range_note(aRequest.EmptyNote);
		editor->setGeometry(host->rect());
		editor->Changed = [this, changed = aRequest.Changed] {
			sync_head_edit();
			if (changed) {
				changed();
			}
		};
		HeadEdit->Ended    = aRequest.Ended;
		HeadEdit->Editor   = editor;
		HeadEdit->Follower = new resize_follower{ *host, [this] {
													 if (HeadEdit && HeadEdit->Editor && HeadEdit->Host) {
														 HeadEdit->Editor->setGeometry(HeadEdit->Host->rect());
														 sync_head_edit();
													 }
												 } };
		if (!OnePaneEnabled) {
			// The comparison stays the one thing that scrolls; the edit follows it.
			editor->set_external_scroll(true);
			editor->Wheeled    = [this](QWheelEvent* const aEvent) { static_cast<void>(scroll_by_wheel(*aEvent)); };
			editor->Reveal     = [this](const std::uint64_t aRow) { reveal_head_edit_row(aRow); };
			HeadEdit->Scrolled = connect(Right->verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { sync_head_edit(); });
		}
		editor->horizontalScrollBar()->setValue(host->horizontalScrollBar()->value());
		editor->show();
		editor->raise();
		// One pane's map draws the interleaved rows, which the edit hides.
		if (OnePaneEnabled) {
			UnifiedMap->hide();
		}
		if (OnePaneEnabled) {
			// The range where its head rows were in the interleaved view.
			auto above = std::uint64_t{ 0U };
			if (const auto row = unified_row_for(core::diff_side::right, aRequest.First); row.has_value()) {
				const auto rect = pane(Unified)->line_row_rect(*row);
				if (!rect.isEmpty() && rect.top() > 0) {
					above = static_cast<std::uint64_t>(std::floor(static_cast<qreal>(rect.top()) / std::max<qreal>(1.0, editor->line_height())));
				}
			}
			editor->scroll_to_line(aRequest.First > above ? aRequest.First - above : 0U);
		}
		// The caret at the end of the last selected line.
		const auto last = aRequest.First + spanCount - 1U;
		static_cast<void>(editor->apply(core::place_caret{ .At = { last, editor->model().line_width(last) } }));
		sync_head_edit();
		editor->setFocus(Qt::OtherFocusReason);
		return editor;
	}

	void diff_workbench::end_head_edit()
	{
		if (!HeadEdit) {
			return;
		}
		auto edit = std::move(HeadEdit);
		QObject::disconnect(edit->Scrolled);
		if (edit->Follower) {
			delete edit->Follower.data();
		}
		const auto hadFocus = edit->Editor && edit->Editor->hasFocus();
		if (edit->Editor) {
			// Gestures may still be on the editor's stack: it goes once they return.
			edit->Editor->hide();
			edit->Editor->deleteLater();
		}
		LinkMap->set_dimmed_from(std::nullopt);
		if (edit->OnePane && OnePaneEnabled) {
			UnifiedMap->show();
		}
		if (hadFocus && edit->Host) {
			edit->Host->setFocus(Qt::OtherFocusReason);
		}
		if (edit->Ended) {
			edit->Ended();
		}
	}

	auto diff_workbench::head_editor() const noexcept -> suggestion_editor*
	{
		return HeadEdit ? HeadEdit->Editor.data() : nullptr;
	}

	auto diff_workbench::head_edit_lines() const -> std::optional<std::pair<std::uint32_t, std::uint32_t>>
	{
		return HeadEdit ? std::optional{ std::pair{ HeadEdit->First, HeadEdit->Last } } : std::nullopt;
	}

	void diff_workbench::sync_head_edit()
	{
		if (!HeadEdit || !HeadEdit->Editor || HeadEdit->OnePane) {
			return;
		}
		auto* const editor = HeadEdit->Editor.data();
		auto* const source = pane(Right);
		const auto  top    = source->top_visible_line();
		if (!top) {
			return;
		}
		const auto row = source->exact_line_top(*top);
		if (!row) {
			return;
		}
		// Where the head pane's top row is, in the editor's viewport, and the
		// head's fractional line at the editor's top edge.
		const auto pitch  = std::max<qreal>(1.0, editor->line_height());
		const auto y      = *row + static_cast<qreal>(Right->viewport()->mapTo(Right, QPoint{ }).y() - editor->viewport()->mapTo(Right, QPoint{ }).y());
		const auto head   = static_cast<qreal>(*top) - y / pitch;
		const auto first  = static_cast<qreal>(HeadEdit->First);
		const auto before = static_cast<qreal>(HeadEdit->OriginalCount);
		const auto after  = static_cast<qreal>(HeadEdit->count());
		// Rows above the range meet their head rows exactly, the range's rows
		// pass at the rate its length differs, and rows below meet theirs once
		// the range has gone above the top: the edit scrolls without a jump.
		const auto row0 = head < first ? head : head < first + before ? first + (head - first) * after / before
																	  : head + after - before;
		editor->set_vertical_offset(row0 * pitch);
		// Ribbons that touch or follow the range no longer meet their rows.
		const auto rangeRow = source->line_row_rect(HeadEdit->First);
		auto       dimFrom  = std::optional<qreal>{ };
		if (!rangeRow.isEmpty()) {
			dimFrom = static_cast<qreal>(LinkMap->mapFrom(Comparison, Right->viewport()->mapTo(Comparison, QPoint{ 0, rangeRow.top() })).y());
		} else if (*top > HeadEdit->First) {
			dimFrom = -1.0e9;
		}
		LinkMap->set_dimmed_from(dimFrom);
	}

	void diff_workbench::reveal_head_edit_row(const std::uint64_t aRow)
	{
		if (!HeadEdit || !HeadEdit->Editor || HeadEdit->OnePane) {
			return;
		}
		auto* const editor  = HeadEdit->Editor.data();
		const auto  pitch   = std::max<qreal>(1.0, editor->line_height());
		const auto  overrun = [editor, pitch, aRow] {
			const auto top    = static_cast<qreal>(aRow) * pitch - editor->vertical_offset();
			const auto height = static_cast<qreal>(editor->viewport()->height());
			return top < 0.0 ? top : top + pitch > height ? top + pitch - height
														  : 0.0;
		};
		// The comparison moves by the pixels the row is out of view. The edit
		// covers the head pane, so its viewport's own area and clip say where
		// its rows show, not what is visible of it. The reveal measures the
		// pane's origin, which can land a few pixels short: it goes again while
		// that brings the row nearer.
		for (auto attempt = 0; attempt < 3; ++attempt) {
			const auto before = overrun();
			if (std::abs(before) < 0.5 || !HeadEdit || !HeadEdit->Editor) {
				return;
			}
			const auto clip    = Right->viewport()->mask();
			const auto visible = clip.isEmpty() ? Right->viewport()->rect() : clip.boundingRect().intersected(Right->viewport()->rect());
			if (visible.isEmpty()) {
				return;
			}
			const auto shift = static_cast<int>(before < 0.0 ? std::floor(before) : std::ceil(before));
			const auto rect  = shift < 0 ? QRect{ 0, visible.top() + shift, 1, 1 } : QRect{ 0, visible.bottom() + shift, 1, 1 };
			reveal_keyboard_row(Right, rect, visible);
			if (std::abs(overrun()) >= std::abs(before)) {
				return;
			}
		}
	}

	diff_workbench::~diff_workbench() = default;

	void diff_workbench::publish(
		const application::diff_publication& aPublication)
	{
		if (
			!core::valid(aPublication.Context) || !aPublication.Result
			|| aPublication.Result->Alignment.Key != aPublication.Context.Key
			|| !CurrentContext.has_value()
			|| *CurrentContext != aPublication.Context) {
			++Statistics.RejectedStaleInput;
			return;
		}
		if (!aPublication.Index || !aPublication.Index->matches_result(*aPublication.Result)) {
			++Statistics.RejectedStaleInput;
			return;
		}
		drop_staged_publication();
		const auto lines = [](const std::optional<core::decoded_text_document>& aDocument) {
			return aDocument.has_value() ? aDocument->Lines.size() : std::size_t{ 0U };
		};
		if (lines(aPublication.Result->LeftDocument) + lines(aPublication.Result->RightDocument) <= staged_publication_lines) {
			adopt_publication(aPublication, nullptr, nullptr);
			return;
		}
		// [[BPR-1264]]. Replacing both panes' text in one turn held the GUI
		// thread for the whole file twice. The documents are filled detached,
		// a bounded batch per turn, and adopted together once complete.
		auto staged         = std::make_shared<staged_publication>();
		staged->Publication = aPublication;
		staged->Left.start(*Left, aPublication.Result->LeftDocument);
		staged->Right.start(*Right, aPublication.Result->RightDocument);
		PendingPublication = std::move(staged);
		++Statistics.StagedPublications;
		queue_publication_continuation();
	}

	void diff_workbench::adopt_publication(const application::diff_publication& aPublication,
										   QTextDocument* const aLeft, QTextDocument* const aRight)
	{
		// [[RFC-BPR-0090]]. An edit holds the text it was opened on.
		end_head_edit();
		const auto previousSelection = Interaction.Selection;
		HeldLine.reset();
		for (auto* source : { Left, Right, Unified }) {
			pane(source)->reset_pixel_gesture();
			pane(source)->set_within_line_offset(0.0);
		}
		CurrentResult = aPublication.Result;
		SemanticIndex = aPublication.Index;
		// Built once here rather than searched per segment per repaint. The
		// move table is keyed by segment identity and is unordered with
		// respect to the segment list, so the alternative is a linear search
		// inside the paint loop.
		MovedSegments.clear();
		for (const auto& move : CurrentResult->Alignment.Moves) {
			MovedSegments.insert(move.SourceDeletion);
			MovedSegments.insert(move.DestinationInsertion);
		}
		++Statistics.SemanticIndexAdoptions;
		Statistics.SemanticIndexEntries
			= SemanticIndex->statistics().IndexEntries;
		// A document's revision does not identify its content ([[BPR-1123]]).
		LeftHeights.clear();
		RightHeights.clear();
		if (aLeft != nullptr && aRight != nullptr) {
			adopt_document(*pane(Left), *aLeft);
			adopt_document(*pane(Right), *aRight);
		} else {
			Left->setPlainText(text_document(CurrentResult->LeftDocument));
			Right->setPlainText(text_document(CurrentResult->RightDocument));
		}
		rebuild_unified_document();
		++Statistics.SemanticQueries;
		static_cast<void>(core::reconcile_diff_interaction(Interaction, *SemanticIndex));
		if (!Interaction.Selection.has_value()) {
			if (!CurrentResult->Alignment.Groups.empty()) {
				++Statistics.SemanticQueries;
				static_cast<void>(core::transition(
					Interaction,
					*SemanticIndex,
					core::select_diff_group_intent{
						.Group = CurrentResult->Alignment.Groups.front().Id,
					}));
			} else if (CurrentResult->Alignment.LeftLineCount != 0U) {
				++Statistics.SemanticQueries;
				static_cast<void>(core::transition(
					Interaction,
					*SemanticIndex,
					core::select_diff_line_intent{
						.Line = {
							.Comparison = CurrentResult->Alignment.Digest,
							.Side       = core::diff_side::left,
							.Line       = 0U,
						},
					}));
			} else if (CurrentResult->Alignment.RightLineCount != 0U) {
				++Statistics.SemanticQueries;
				static_cast<void>(core::transition(
					Interaction,
					*SemanticIndex,
					core::select_diff_line_intent{
						.Line = {
							.Comparison = CurrentResult->Alignment.Digest,
							.Side       = core::diff_side::right,
							.Line       = 0U,
						},
					}));
			}
		}
		++Statistics.Publications;
		// [[BPR-0719]]: a work limit degrades the alignment; the status says so.
		const auto status = CurrentResult->Alignment.Content == core::diff_content_state::text
			? (CurrentResult->AlignmentApproximate
				   ? QStringLiteral("Diff ready — %1 semantic changes (approximate alignment)")
				   : QStringLiteral("Diff ready — %1 semantic changes"))
				  .arg(CurrentResult->Alignment.Groups.size())
			: reason_text(CurrentResult->Alignment.Diagnostic.Code);
		// [[BPR-1132]]. A ready, exact alignment is routine; a header that
		// shows the status only when it says more leaves it unsaid.
		update_status(status + (aPublication.CacheHit ? QStringLiteral(" (cached)") : QString{ }),
					  CurrentResult->Alignment.Content == core::diff_content_state::text && !CurrentResult->AlignmentApproximate);
		update_detail();
		update_selection_paint();
		schedule_geometry_rebuild(
			measured_geometry_change::accepted_diff,
			diff_geometry_rebuild_trigger::accepted_diff);
		ComparisonLoading = false;
		// [[BPR-1871]]. The file the wait named is here.
		WaitingPath.clear();
		WaitingHistory = false;
		WaitingFailure.clear();
		apply_pending();
		if (Interaction.Selection != previousSelection) {
			emit_selection();
		}
	}

	void diff_workbench::fail(
		const application::diff_failure_publication& aFailure)
	{
		if (!CurrentContext.has_value() || *CurrentContext != aFailure.Context) {
			++Statistics.RejectedStaleInput;
			return;
		}
		++Statistics.Failures;
		set_idle(false);
		clear_projection(reason_text(aFailure.Reason));
		CurrentContext.reset();
		ComparisonLoading = false;
		// [[BPR-1871]]. A named file's failed read says why in its place.
		WaitingHistory = false;
		WaitingFailure = WaitingPath.isEmpty() ? QString{ } : reason_text(aFailure.Reason);
		apply_pending();
	}

	void diff_workbench::invalidate(
		const core::pull_request_authority& aAuthority)
	{
		if (
			!CurrentContext.has_value()
			|| CurrentContext->Key.Authority != aAuthority) {
			return;
		}
		++Statistics.Invalidations;
		const auto hadSelection = Interaction.Selection.has_value();
		CurrentContext.reset();
		WaitingPath.clear();
		WaitingHistory = false;
		WaitingFailure.clear();
		set_idle(false);
		clear_projection(QStringLiteral("Comparison invalidated"));
		if (!hadSelection) {
			emit_selection_observer(false);
		}
	}

	void diff_workbench::set_loading(const core::diff_request_context& aContext, QString aPath)
	{
		if (!core::valid(aContext)) {
			return;
		}
		const auto hadSelection = Interaction.Selection.has_value();
		CurrentContext          = aContext;
		set_idle(false);
		clear_projection(aPath.isEmpty() ? QStringLiteral("Loading selected file comparison…") : QStringLiteral("Loading %1…").arg(aPath));
		WaitingPath    = std::move(aPath);
		WaitingHistory = false;
		WaitingFailure.clear();
		ComparisonLoading = true;
		apply_pending();
		if (!hadSelection) {
			emit_selection_observer(false);
		}
	}

	void diff_workbench::set_waiting(QString aPath, const bool aHistory)
	{
		if (aPath.isEmpty()) {
			return;
		}
		// Already saying so: the wait's clock keeps running and nothing flickers.
		if (!Idle && !CurrentContext.has_value() && WaitingPath == aPath && WaitingFailure.isEmpty()) {
			if (WaitingHistory != aHistory) {
				WaitingHistory = aHistory;
				apply_pending();
			}
			return;
		}
		const auto hadSelection = Interaction.Selection.has_value();
		CurrentContext.reset();
		set_idle(false);
		auto status = QStringLiteral("Loading %1…").arg(aPath);
		clear_projection(status);
		WaitingPath    = std::move(aPath);
		WaitingHistory = aHistory;
		WaitingFailure.clear();
		apply_pending();
		announce_accessible_status(std::move(status));
		if (!hadSelection) {
			emit_selection_observer(false);
		}
	}

	void diff_workbench::set_waiting_failed(QString aPath, QString aSentence)
	{
		if (aSentence.isEmpty()) {
			return;
		}
		if (!Idle && !CurrentContext.has_value() && WaitingPath == aPath && WaitingFailure == aSentence) {
			return;
		}
		const auto hadSelection = Interaction.Selection.has_value();
		CurrentContext.reset();
		set_idle(false);
		clear_projection(aSentence);
		WaitingPath    = std::move(aPath);
		WaitingHistory = false;
		WaitingFailure = aSentence;
		apply_pending();
		announce_accessible_status(std::move(aSentence));
		if (!hadSelection) {
			emit_selection_observer(false);
		}
	}

	auto diff_workbench::waiting_path() const noexcept -> const QString&
	{
		return WaitingPath;
	}

	void diff_workbench::clear(QString aStatus)
	{
		const auto hadSelection = Interaction.Selection.has_value();
		CurrentContext.reset();
		WaitingPath.clear();
		WaitingHistory = false;
		WaitingFailure.clear();
		// Only the plain "nothing chosen" is idle; a reason the host gives is
		// shown as before.
		const auto idle = aStatus.isEmpty();
		set_idle(idle);
		clear_projection(idle ? QStringLiteral("Select a file comparison") : std::move(aStatus));
		if (!hadSelection) {
			emit_selection_observer(false);
		}
	}

	void diff_workbench::set_selection_sink(selection_sink aSink)
	{
		SelectionSink = std::move(aSink);
		emit_selection();
	}

	void diff_workbench::set_line_reselection_sink(line_reselection_sink aSink)
	{
		LineReselectionSink = std::move(aSink);
	}

	void diff_workbench::set_reading_gesture_sink(std::function<void()> aSink)
	{
		ReadingGestureSink = std::move(aSink);
	}

	void diff_workbench::set_selection_observer(selection_observer aObserver)
	{
		SelectionObserver = std::move(aObserver);
		emit_selection();
	}

	auto diff_workbench::reveal_conversation_line(
		const core::diff_side aSide,
		const std::uint32_t   aLine) -> bool
	{
		if (!CurrentResult) {
			return false;
		}
		const auto requested = core::source_line_ref{
			.Comparison = CurrentResult->Alignment.Digest,
			.Side       = aSide,
			.Line       = aLine,
		};
		const auto previous = Interaction.Selection;
		if (!select_line(aSide, aLine)) {
			return false;
		}
		ConversationAnchor = requested;
		static_cast<void>(reveal_exact_line(*Interaction.Selection, requested));
		update_diff_paint();
		if (Interaction.Selection == previous) {
			emit_selection_observer(true);
		}
		return true;
	}
}
