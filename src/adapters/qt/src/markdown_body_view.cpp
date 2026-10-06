#include "bpr/qt/markdown_body_view.hpp"

#include "markdown_document.hpp"

#include <QApplication>
#include <QClipboard>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMimeData>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTextBlock>
#include <QTextEdit>
#include <QTextFragment>
#include <QTextTable>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::qt {
	namespace {
		// Conservative source budgets bound parser and document work before Qt
		// allocates a table or recursively interprets nested markup. They count
		// literal syntax too; exceeding a budget always retains the source.
		auto rendering_limit(const std::string_view aBody) -> QString
		{
			if (aBody.size() > 65'536U) {
				return QStringLiteral("Rendered preview is limited to 65,536 UTF-8 bytes. The complete source is available.");
			}
			std::size_t lines = 0, markers = 0, pipeLines = 0, maxPipes = 0;
			std::size_t offset = 0, brackets = 0;
			while (offset < aBody.size()) {
				const auto end  = aBody.find('\n', offset);
				const auto line = aBody.substr(offset, end == std::string_view::npos ? aBody.size() - offset : end - offset);
				offset += line.size() + 1U;
				if (++lines > 2'048U || line.size() > 4'096U) {
					return QStringLiteral("Rendered preview exceeds the 2,048-line or 4,096-byte line limit. The complete source is available.");
				}
				std::size_t pipes = 0, prefix = 0, quotes = 0;
				bool        leading = true;
				for (const auto ch : line) {
					if (leading && (ch == ' ' || ch == '\t' || ch == '>')) {
						prefix += ch == '\t' ? 4U : 1U;
						quotes += ch == '>' ? 1U : 0U;
					} else {
						leading = false;
					}
					brackets += ch == '[' ? 1U : 0U;
					if (ch == ']' && brackets != 0U) {
						--brackets;
					}
					pipes += ch == '|' ? 1U : 0U;
					markers += std::string_view{ "*_~`[]<>|" }.find(ch) != std::string_view::npos ? 1U : 0U;
					if (prefix > 64U || quotes > 8U || brackets > 16U || markers > 8'192U) {
						return QStringLiteral("Rendered preview exceeds the nesting or formatting-work limit. The complete source is available.");
					}
				}
				pipeLines += pipes != 0U ? 1U : 0U;
				maxPipes = std::max(maxPipes, pipes);
				if (maxPipes > 32U || pipeLines * (maxPipes + 1U) > 1'024U) {
					return QStringLiteral("Rendered preview exceeds the table-work limit. The complete source is available.");
				}
			}
			return { };
		}

		struct image_placeholder
		{
			int     Position;
			int     Length;
			QString Alt;
			QString Target;
		};

		struct code_fence
		{
			QChar     Character;
			qsizetype Count  = 0;
			qsizetype Indent = 0;
			QString   Info;
		};

		auto fence_at(const QString& aLine) -> code_fence
		{
			code_fence fence;
			while (fence.Indent < aLine.size() && aLine[fence.Indent] == QLatin1Char{ ' ' }) {
				++fence.Indent;
			}
			if (fence.Indent > 3 || fence.Indent == aLine.size()) {
				return { };
			}
			fence.Character = aLine[fence.Indent];
			if (fence.Character != QLatin1Char{ '`' } && fence.Character != QLatin1Char{ '~' }) {
				return { };
			}
			while (fence.Indent + fence.Count < aLine.size() && aLine[fence.Indent + fence.Count] == fence.Character) {
				++fence.Count;
			}
			fence.Info = aLine.mid(fence.Indent + fence.Count).trimmed();
			return fence.Count >= 3 ? fence : code_fence{ };
		}

		auto expanded_tabs(const QString& aText) -> QString
		{
			QString result;
			for (const auto ch : aText) {
				if (ch == QLatin1Char{ '\t' }) {
					result += QString(4 - result.size() % 4, QLatin1Char{ ' ' });
				} else {
					result += ch;
				}
			}
			return result;
		}

		// Qt expands code tabs during import. Restore them only when a complete
		// top-level fenced-code sequence exactly matches the imported blocks.
		// This is a checked repair, not a second Markdown parser: any ambiguity
		// (including a tab outside those fences) retains the complete source.
		bool restore_code_tabs(QTextDocument& aDocument, const QString& aSource)
		{
			if (!aSource.contains(QLatin1Char{ '\t' })) {
				return true;
			}
			QStringList sourceLines;
			code_fence  active;
			const auto  lines = aSource.split(QLatin1Char{ '\n' });
			for (qsizetype index = 0; index < lines.size(); ++index) {
				auto line = lines[index];
				if (line.endsWith(QLatin1Char{ '\r' })) {
					line.chop(1);
				}
				const auto fence = fence_at(line);
				if (active.Count == 0) {
					if (line.contains(QLatin1Char{ '\t' })) {
						return false;
					}
					active = fence;
				} else if (fence.Character == active.Character && fence.Count >= active.Count && fence.Info.isEmpty()) {
					active = { };
				} else if (index + 1 < lines.size() || !line.isEmpty()) {
					qsizetype removed = 0;
					while (removed < active.Indent && removed < line.size() && line[removed] == QLatin1Char{ ' ' }) {
						++removed;
					}
					sourceLines.push_back(line.mid(removed));
				}
			}
			std::vector<QTextBlock> blocks;
			for (auto block = aDocument.begin(); block.isValid(); block = block.next()) {
				if (block.blockFormat().hasProperty(QTextFormat::BlockCodeFence)) {
					blocks.push_back(block);
				}
			}
			if (blocks.size() != static_cast<std::size_t>(sourceLines.size())) {
				return false;
			}
			for (std::size_t index = 0; index < blocks.size(); ++index) {
				if (blocks[index].text() != expanded_tabs(sourceLines[static_cast<qsizetype>(index)])) {
					return false;
				}
			}
			for (std::size_t index = 0; index < blocks.size(); ++index) {
				auto cursor = QTextCursor{ blocks[index] };
				cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
				cursor.insertText(sourceLines[static_cast<qsizetype>(index)]);
			}
			return true;
		}

		void replace_images(QTextDocument& aDocument)
		{
			std::vector<image_placeholder> images;
			for (auto block = aDocument.begin(); block.isValid(); block = block.next()) {
				for (auto it = block.begin(); !it.atEnd(); ++it) {
					const auto fragment = it.fragment();
					if (fragment.charFormat().isImageFormat()) {
						const auto format = fragment.charFormat().toImageFormat();
						images.push_back({ fragment.position(), fragment.length(), format.stringProperty(QTextFormat::ImageAltText), format.name() });
					}
				}
			}
			for (auto it = images.rbegin(); it != images.rend(); ++it) {
				auto cursor = QTextCursor{ &aDocument };
				cursor.setPosition(it->Position);
				cursor.setPosition(it->Position + it->Length, QTextCursor::KeepAnchor);
				auto format = QTextCharFormat{ };
				format.setAnchor(true);
				format.setAnchorHref(it->Target);
				cursor.insertText(QStringLiteral("[Image: %1 — open image link]").arg(it->Alt.isEmpty() ? QStringLiteral("no alternative text") : it->Alt), format);
			}
		}

		void style_table_header(QTextTable& aTable)
		{
			for (int column = 0; column < aTable.columns(); ++column) {
				const auto cell   = aTable.cellAt(0, column);
				auto       cursor = cell.firstCursorPosition();
				// Qt can prepend an empty paragraph to the first header cell.
				// Only remove it when another paragraph belongs to this same cell.
				if (cursor.block().text().isEmpty() && cursor.block() != cell.lastCursorPosition().block()) {
					cursor.deleteChar();
				}
				cursor = cell.firstCursorPosition();
				cursor.setPosition(cell.lastCursorPosition().position(), QTextCursor::KeepAnchor);
				auto format = QTextCharFormat{ };
				format.setFontWeight(QFont::Bold);
				cursor.mergeCharFormat(format);
			}
		}

		void style_tables(QTextFrame& aFrame, const QPalette& aPalette)
		{
			for (auto* frame : aFrame.childFrames()) {
				if (auto* table = qobject_cast<QTextTable*>(frame)) {
					auto format = table->format();
					format.setBorder(1);
					format.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
					format.setBorderBrush(aPalette.text());
					format.setCellPadding(4);
					table->setFormat(format);
					style_table_header(*table);
					// Keep each cell's text readable. Qt otherwise squeezes wide
					// tables into columns only one character across; the reader
					// already owns horizontal overflow for this bounded content.
					for (int row = 0; row < table->rows(); ++row) {
						for (int column = 0; column < table->columns(); ++column) {
							auto cursor = table->cellAt(row, column).firstCursorPosition();
							auto cell   = cursor.blockFormat();
							cell.setNonBreakableLines(true);
							cursor.setBlockFormat(cell);
						}
					}
				}
				style_tables(*frame, aPalette);
			}
		}
	} // namespace

	class markdown_text_view final : public QTextEdit
	{
	public:
		explicit markdown_text_view(QWidget* aParent)
			: QTextEdit{ aParent }
		{
			setReadOnly(true);
			setAcceptRichText(false);
			setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
			setTabChangesFocus(true);
			setMouseTracking(true);
			setDocument(new detail::markdown_document{ this });
		}

		std::function<void(const QString&)> Activate;
		std::function<void(const QString&)> Describe;
		std::function<void(bool)>           LeaveLinks;

		void reset_links()
		{
			FocusedLink = -1;
			Pressed.clear();
			Links.clear();
			for (auto block = document()->begin(); block.isValid(); block = block.next()) {
				for (auto it = block.begin(); !it.atEnd(); ++it) {
					const auto fragment = it.fragment();
					const auto target   = fragment.charFormat().anchorHref();
					if (!target.isEmpty()) {
						if (!Links.empty() && Links.back().End == fragment.position() && Links.back().Target == target) {
							Links.back().End += fragment.length();
						} else {
							Links.push_back({ fragment.position(), fragment.position() + fragment.length(), target });
						}
					}
				}
			}
		}

	protected:
		void wheelEvent(QWheelEvent* aEvent) override
		{
			if (!aEvent->pixelDelta().isNull()) {
				auto* vertical   = verticalScrollBar();
				auto* horizontal = horizontalScrollBar();
				vertical->setValue(vertical->value() - aEvent->pixelDelta().y());
				horizontal->setValue(horizontal->value() - aEvent->pixelDelta().x());
			} else {
				QTextEdit::wheelEvent(aEvent);
			}
			// A body owns scrolling even at its end; do not page its parent.
			aEvent->accept();
		}

		void mousePressEvent(QMouseEvent* aEvent) override
		{
			Pressed = aEvent->button() == Qt::LeftButton ? anchorAt(aEvent->position().toPoint()) : QString{ };
			QTextEdit::mousePressEvent(aEvent);
		}

		void mouseReleaseEvent(QMouseEvent* aEvent) override
		{
			QTextEdit::mouseReleaseEvent(aEvent);
			const auto target = anchorAt(aEvent->position().toPoint());
			if (aEvent->button() == Qt::LeftButton && !Pressed.isEmpty() && Pressed == target && !textCursor().hasSelection() && Activate) {
				Activate(target);
			}
			Pressed.clear();
		}

		void mouseMoveEvent(QMouseEvent* aEvent) override
		{
			QTextEdit::mouseMoveEvent(aEvent);
			const auto target = anchorAt(aEvent->position().toPoint());
			viewport()->setCursor(target.isEmpty() ? Qt::IBeamCursor : Qt::PointingHandCursor);
			if (Describe) {
				Describe(target);
			}
		}

		void keyPressEvent(QKeyEvent* aEvent) override
		{
			if ((aEvent->key() == Qt::Key_Return || aEvent->key() == Qt::Key_Enter) && Activate) {
				const auto target = textCursor().charFormat().anchorHref();
				if (!target.isEmpty()) {
					Activate(target);
					aEvent->accept();
					return;
				}
			}
			QTextEdit::keyPressEvent(aEvent);
		}

		bool focusNextPrevChild(bool aNext) override
		{
			const auto next = FocusedLink < 0 ? (aNext ? 0 : static_cast<int>(Links.size()) - 1) : FocusedLink + (aNext ? 1 : -1);
			if (next >= 0 && next < static_cast<int>(Links.size())) {
				FocusedLink        = next;
				const auto& link   = Links[static_cast<std::size_t>(next)];
				auto        cursor = textCursor();
				cursor.setPosition(link.Start);
				cursor.setPosition(link.End, QTextCursor::KeepAnchor);
				setTextCursor(cursor);
				ensureCursorVisible();
				if (Describe) {
					Describe(link.Target);
				}
				return true;
			}
			FocusedLink      = -1;
			const auto moved = QTextEdit::focusNextPrevChild(aNext);
			// macOS can exclude push buttons from the ordinary Tab chain. Keep
			// the source controls reachable even when this is its only text view.
			if (hasFocus() && LeaveLinks) {
				LeaveLinks(aNext);
			}
			return moved || !hasFocus();
		}

	private:
		struct link
		{
			int     Start;
			int     End;
			QString Target;
		};
		std::vector<link> Links;
		int               FocusedLink = -1;
		QString           Pressed;
	};

	markdown_body_view::markdown_body_view(QWidget* aParent)
		: QWidget{ aParent }
		, CodeFont{ QFontDatabase::systemFont(QFontDatabase::FixedFont) }
	{
		TabStopDistance = 4 * QFontMetricsF{ CodeFont }.horizontalAdvance(QLatin1Char{ ' ' });
		auto* layout    = new QVBoxLayout{ this };
		layout->setContentsMargins(0, 0, 0, 0);
		auto* controls = new QHBoxLayout;
		Toggle         = new QPushButton{ QStringLiteral("View source"), this };
		Toggle->setObjectName(QStringLiteral("markdownSourceToggle"));
		Toggle->setAccessibleName(QStringLiteral("View original Markdown source"));
		auto* copy = new QPushButton{ QStringLiteral("Copy source"), this };
		copy->setAccessibleName(QStringLiteral("Copy original Markdown source"));
		copy->setObjectName(QStringLiteral("markdownCopySource"));
		Mark = new QLabel{ this };
		Mark->setObjectName(QStringLiteral("markdownFeedbackMark"));
		Mark->hide();
		controls->addWidget(Mark);
		controls->addWidget(Toggle);
		controls->addWidget(copy);
		controls->addStretch();
		layout->addLayout(controls);
		Notice = new QLabel{ this };
		Notice->setTextFormat(Qt::PlainText);
		Notice->setWordWrap(true);
		layout->addWidget(Notice);
		Pages    = new QStackedWidget{ this };
		Rendered = new markdown_text_view{ Pages };
		Rendered->setAccessibleName(QStringLiteral("Rendered Markdown body"));
		Rendered->setAccessibleDescription(QStringLiteral("Read-only body. Tab visits links; Enter requests opening the displayed destination. Wide code and tables scroll horizontally."));
		Source = new QPlainTextEdit{ Pages };
		Source->setReadOnly(true);
		Source->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		Source->setLineWrapMode(QPlainTextEdit::NoWrap);
		Source->setTabChangesFocus(true);
		Source->setAccessibleName(QStringLiteral("Original Markdown source"));
		Pages->addWidget(Rendered);
		Pages->addWidget(Source);
		layout->addWidget(Pages, 1);
		Destination = new QLabel{ this };
		Destination->setTextFormat(Qt::PlainText);
		Destination->setWordWrap(true);
		Destination->setAccessibleName(QStringLiteral("Link destination"));
		Destination->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		layout->addWidget(Destination);
		Rendered->Describe = [this](const QString& aTarget) {
			Destination->setText(aTarget);
			Destination->setToolTip(aTarget);
		};
		Rendered->Activate = [this](const QString& aTarget) {
			Rendered->Describe(aTarget);
			if (LinkDispatch) {
				LinkDispatch(aTarget.toUtf8().toStdString());
			}
		};
		Rendered->LeaveLinks = [this, copy](const bool aNext) {
			(aNext ? Toggle : copy)->setFocus(aNext ? Qt::TabFocusReason : Qt::BacktabFocusReason);
		};
		connect(Toggle, &QPushButton::clicked, this, [this] { show_source(!source_visible()); });
		connect(copy, &QPushButton::clicked, this, &markdown_body_view::copy_source);
		setFocusProxy(Rendered);
		clear();
	}

	void markdown_body_view::set_body(std::string aBody, std::string aIdentity)
	{
		if (Body == aBody && Identity == aIdentity) {
			return;
		}
		const auto retainSource = source_visible() && Identity == aIdentity;
		Body                    = std::move(aBody);
		Identity                = std::move(aIdentity);
		Source->setPlainText(QString::fromUtf8(Body.data(), static_cast<qsizetype>(Body.size())));
		VisitedSource   = false;
		VisitedRendered = false;
		Destination->clear();
		prepare_rendered();
		show_source(retainSource || !RenderedAvailable);
	}

	void markdown_body_view::clear()
	{
		Body.clear();
		Identity.clear();
		Source->clear();
		VisitedSource   = false;
		VisitedRendered = false;
		Destination->clear();
		prepare_rendered();
		show_source(false);
	}

	void markdown_body_view::set_feedback_marks(const feedback_mark_theme& aTheme)
	{
		const auto enabled = MarkTheme.has_value();
		MarkTheme          = aTheme;
		const auto mark    = feedback_body_mark_of(Body);
		// A new tint or style redraws the mark alone. The document, its
		// position and its selection change only when what it renders does.
		if ((enabled && mark.Label == BodyMark.Label && mark.SubjectByte == BodyMark.SubjectByte) || (!enabled && !mark.Label.has_value())) {
			BodyMark = mark;
			refresh_mark();
			return;
		}
		prepare_rendered();
	}

	void markdown_body_view::refresh_mark()
	{
		if (BodyMark.Label.has_value() && MarkTheme.has_value()) {
			const auto colours = feedback_mark_colours_for(*MarkTheme, *BodyMark.Label, palette());
			Mark->setPixmap(feedback_mark_pixmap(*BodyMark.Label, BodyMark.Emphasis, colours, devicePixelRatioF()));
			Mark->setAccessibleName(BodyMark.Name);
			Mark->setToolTip(BodyMark.Name);
		}
		Mark->setVisible(BodyMark.Label.has_value());
	}

	auto markdown_body_view::feedback_mark_label() const noexcept -> QLabel*
	{
		return Mark;
	}

	void markdown_body_view::prepare_rendered()
	{
		// [[BPR-1083]]. The mark stands in for the prefix in the reading view.
		BodyMark = MarkTheme.has_value() ? feedback_body_mark_of(Body) : feedback_body_mark{ };
		refresh_mark();
		const auto shown  = std::string_view{ Body }.substr(std::min(BodyMark.SubjectByte, Body.size()));
		Fallback          = rendering_limit(shown);
		RenderedAvailable = Fallback.isEmpty();
		auto* document    = Rendered->document();
		document->setLayoutEnabled(false);
		document->clear();
		if (RenderedAvailable) {
			const auto source = QString::fromUtf8(shown.data(), static_cast<qsizetype>(shown.size()));
			document->setMarkdown(source, QTextDocument::MarkdownFeatures{ QTextDocument::MarkdownDialectGitHub } | QTextDocument::MarkdownNoHTML);
			if (restore_code_tabs(*document, source)) {
				replace_images(*document);
				refresh_formats();
			} else {
				Fallback          = QStringLiteral("This tab-bearing construct cannot be rendered without changing whitespace. The complete source is available.");
				RenderedAvailable = false;
				document->clear();
			}
		}
		document->setLayoutEnabled(true);
		// Replacing a QTextDocument directly leaves its attached view cursor
		// tracking the importer at the end. A new body starts at its beginning.
		Rendered->setTextCursor(QTextCursor{ document });
		Rendered->verticalScrollBar()->setValue(0);
		Rendered->horizontalScrollBar()->setValue(0);
		Rendered->reset_links();
		Notice->setText(!Fallback.isEmpty() ? Fallback : Body.empty() ? QStringLiteral("No body provided.")
																	  : QString{ });
		Notice->setVisible(!Notice->text().isEmpty());
		Toggle->setEnabled(RenderedAvailable);
	}

	void markdown_body_view::refresh_formats()
	{
		if (!Rendered) {
			return;
		}
		auto* document = Rendered->document();
		document->setDefaultFont(font());
		// [[BPR-0970]]. Restyling a fragment can merge it with its neighbour,
		// which invalidates a QTextBlock::iterator standing on it; the parser
		// fuzz target found `[_|_)]()` reading freed fragment nodes. Each
		// block's fragments are therefore read first and restyled after.
		struct fragment_style
		{
			int             Position = 0;
			int             Length   = 0;
			QTextCharFormat Format;
		};
		std::vector<fragment_style> fragments;
		for (auto block = document->begin(); block.isValid(); block = block.next()) {
			auto       cursor = QTextCursor{ block };
			auto       format = block.blockFormat();
			const auto code   = format.hasProperty(QTextFormat::BlockCodeFence) || format.hasProperty(QTextFormat::BlockCodeLanguage);
			if (!block.textList() && format.marker() != QTextBlockFormat::MarkerType::NoMarker) {
				format.setMarker(QTextBlockFormat::MarkerType::NoMarker);
				cursor.setBlockFormat(format);
			}
			if (code || format.intProperty(QTextFormat::BlockQuoteLevel) > 0) {
				format.setBackground(palette().window());
				format.setNonBreakableLines(code);
				cursor.setBlockFormat(format);
			}
			fragments.clear();
			for (auto it = block.begin(); !it.atEnd(); ++it) {
				const auto fragment = it.fragment();
				auto       chars    = fragment.charFormat();
				if (code || chars.fontFixedPitch()) {
					chars.setFontFamilies(CodeFont.families());
					chars.setFontFixedPitch(true);
					chars.setFontPointSize(CodeFont.pointSizeF());
					chars.setBackground(palette().window());
				}
				if (chars.isAnchor()) {
					chars.setForeground(palette().link());
					chars.setFontUnderline(true);
				}
				fragments.push_back({ .Position = fragment.position(), .Length = fragment.length(), .Format = chars });
			}
			// A character format changes no positions, so each span is still
			// the fragment it was read from.
			for (const auto& fragment : fragments) {
				cursor.setPosition(fragment.Position);
				cursor.setPosition(fragment.Position + fragment.Length, QTextCursor::KeepAnchor);
				cursor.setCharFormat(fragment.Format);
			}
		}
		style_tables(*document->rootFrame(), palette());
		Source->setFont(CodeFont);
		if (TabStopDistance > 0) {
			Source->setTabStopDistance(TabStopDistance);
			Rendered->setTabStopDistance(TabStopDistance);
		}
	}

	void markdown_body_view::set_code_font(const QFont& aFont, const qreal aTabStopDistance)
	{
		CodeFont        = aFont;
		TabStopDistance = aTabStopDistance;
		refresh_formats();
	}

	void markdown_body_view::set_link_dispatch(std::function<void(const std::string&)> aDispatch)
	{
		LinkDispatch = std::move(aDispatch);
	}

	void markdown_body_view::show_source(const bool aSource)
	{
		const auto source   = aSource || !RenderedAvailable;
		auto*      previous = source_visible() ? Source->verticalScrollBar() : Rendered->verticalScrollBar();
		const auto fraction = previous->maximum() > 0 ? static_cast<double>(previous->value()) / previous->maximum() : 0.0;
		Pages->setCurrentWidget(source ? static_cast<QWidget*>(Source) : static_cast<QWidget*>(Rendered));
		auto& visited = source ? VisitedSource : VisitedRendered;
		auto* scroll  = source ? Source->verticalScrollBar() : Rendered->verticalScrollBar();
		if (!visited) {
			scroll->setValue(static_cast<int>(fraction * scroll->maximum()));
			visited = true;
		}
		Toggle->setText(source ? QStringLiteral("Rendered") : QStringLiteral("View source"));
		Toggle->setAccessibleName(source ? QStringLiteral("Show rendered Markdown") : QStringLiteral("View original Markdown source"));
		setFocusProxy(source ? static_cast<QWidget*>(Source) : static_cast<QWidget*>(Rendered));
	}

	void markdown_body_view::copy_source() const
	{
		auto* mime = new QMimeData;
		mime->setData(QStringLiteral("text/plain"), QByteArray{ Body.data(), static_cast<qsizetype>(Body.size()) });
		QApplication::clipboard()->setMimeData(mime);
	}

	auto markdown_body_view::original_body() const noexcept -> const std::string& { return Body; }
	auto markdown_body_view::source_visible() const noexcept -> bool { return Pages->currentWidget() == Source; }
	auto markdown_body_view::rendered_available() const noexcept -> bool { return RenderedAvailable; }
	auto markdown_body_view::fallback_reason() const -> QString { return Fallback; }
	auto markdown_body_view::rendered_view() const noexcept -> QTextEdit* { return Rendered; }
	auto markdown_body_view::selected_source_bytes() const -> std::optional<std::pair<std::size_t, std::size_t>>
	{
		const auto cursor = Source->textCursor();
		if (!cursor.hasSelection()) {
			return std::nullopt;
		}
		const auto raw = QString::fromUtf8(Body.data(), static_cast<qsizetype>(Body.size()));
		if (raw.toUtf8().toStdString() != Body) {
			return std::nullopt;
		}
		QString                  normalized;
		std::vector<std::size_t> offsets{ 0 };
		std::size_t              bytes = 0;
		for (qsizetype i = 0; i < raw.size(); ++i) {
			if (raw[i] == '\r') {
				bytes += 1;
				if (i + 1 < raw.size() && raw[i + 1] == '\n') {
					++i;
					++bytes;
				}
				normalized += '\n';
				offsets.push_back(bytes);
			} else if (raw[i].isHighSurrogate() && i + 1 < raw.size() && raw[i + 1].isLowSurrogate()) {
				normalized += raw[i];
				normalized += raw[i + 1];
				offsets.push_back(bytes);
				bytes += 4;
				offsets.push_back(bytes);
				++i;
			} else {
				normalized += raw[i];
				bytes += static_cast<std::size_t>(QString{ raw[i] }.toUtf8().size());
				offsets.push_back(bytes);
			}
		}
		const auto first = static_cast<std::size_t>(cursor.selectionStart());
		const auto last  = static_cast<std::size_t>(cursor.selectionEnd());
		if (normalized != Source->toPlainText() || last >= offsets.size()
			|| (first && offsets[first] == offsets[first - 1]) || (last && offsets[last] == offsets[last - 1])) {
			return std::nullopt;
		}
		return std::pair{ offsets[first], offsets[last] };
	}

	auto markdown_body_view::source_view() const noexcept -> QPlainTextEdit* { return Source; }
	auto markdown_body_view::minimumSizeHint() const -> QSize { return { 0, 0 }; }

	void markdown_body_view::changeEvent(QEvent* aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::ApplicationPaletteChange
			|| aEvent->type() == QEvent::FontChange || aEvent->type() == QEvent::ApplicationFontChange || aEvent->type() == QEvent::StyleChange) {
			refresh_formats();
		}
	}
} // namespace bpr::qt
