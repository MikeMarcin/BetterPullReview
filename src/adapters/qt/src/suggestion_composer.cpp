#include "bpr/qt/suggestion_composer.hpp"

#include "bpr/qt/appearance_profile.hpp"

#include <QCheckBox>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextBlock>
#include <QVBoxLayout>

#include <algorithm>
#include <span>
#include <utility>
#include <vector>

namespace bpr::qt {
	namespace {
		auto text_of(const std::string& aBytes) -> QString
		{
			return QString::fromUtf8(aBytes.data(), static_cast<qsizetype>(aBytes.size()));
		}

		/// One display line of exact bytes and where it begins in its text.
		struct located_line
		{
			std::uint64_t Begin = 0;
			std::string   Content;
		};

		/// Display lines of exact bytes that begin at `aBase` in their text:
		/// LF, CRLF and a lone CR each end a line.
		auto lines_of(const std::string& aBytes, const std::uint64_t aBase) -> std::vector<located_line>
		{
			auto lines = std::vector<located_line>{ };
			auto begin = aBase;
			for (auto& line : core::split_text_lines(aBytes)) {
				const auto length = line.Content.size() + (line.Ending == core::text_line_ending::crlf ? 2U : line.Ending == core::text_line_ending::none ? 0U
																																						  : 1U);
				lines.push_back({ .Begin = begin, .Content = std::move(line.Content) });
				begin += length;
			}
			return lines;
		}

		/// Where each classifier line begins: the classifier splits at LF only.
		auto line_starts_of(const std::string& aText) -> std::vector<std::uint64_t>
		{
			auto starts = std::vector<std::uint64_t>{ 0U };
			for (std::size_t index = aText.find('\n'); index != std::string::npos; index = aText.find('\n', index + 1U)) {
				starts.push_back(index + 1U);
			}
			return starts;
		}

		auto bytes_of(const std::string& aText) -> std::span<const std::uint8_t>
		{
			return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
		}

		/// Complete lines at the start of `aBytes`, before its last line break.
		auto complete_lines(const std::string& aBytes) -> std::size_t
		{
			return core::split_text_lines(aBytes).size() - 1U;
		}

		auto status_message(const suggestion_source_status aStatus) -> QString
		{
			switch (aStatus) {
			case suggestion_source_status::current:         return { };
			case suggestion_source_status::newer_available: return QStringLiteral("A newer revision was captured. This suggestion stays on the exact bytes you selected.");
			case suggestion_source_status::stale:           return QStringLiteral("The selected code changed in the newer revision. The suggestion stays on the original bytes until you revalidate it.");
			case suggestion_source_status::ambiguous:       return QStringLiteral("The selected code appears more than once in the newer revision. Nothing was chosen for you.");
			case suggestion_source_status::deleted:         return QStringLiteral("The file is gone from the newer revision. The original source stays readable here.");
			}
			return { };
		}
	}

	suggestion_composer::suggestion_composer(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("suggestionComposer"));
		setAccessibleName(QStringLiteral("Suggest change"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 0, 0, 0);

		Location = new QLabel{ this };
		Location->setObjectName(QStringLiteral("suggestionLocation"));
		Location->setTextFormat(Qt::PlainText);
		Location->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		column->addWidget(Location);

		StatusLabel = new QLabel{ this };
		StatusLabel->setObjectName(QStringLiteral("suggestionSourceStatus"));
		StatusLabel->setTextFormat(Qt::PlainText);
		StatusLabel->setWordWrap(true);
		StatusLabel->setAccessibleName(QStringLiteral("Suggestion source status"));
		StatusLabel->hide();
		column->addWidget(StatusLabel);

		Editor = new suggestion_editor{ this };
		column->addWidget(Editor, 2);

		Illustrative = new QCheckBox{ QStringLiteral("Illustrative only (never offered as a patch)"), this };
		Illustrative->setObjectName(QStringLiteral("suggestionIllustrative"));
		column->addWidget(Illustrative);

		Rationale = new QPlainTextEdit{ this };
		Rationale->setObjectName(QStringLiteral("suggestionRationale"));
		Rationale->setAccessibleName(QStringLiteral("Why (optional)"));
		Rationale->setPlaceholderText(QStringLiteral("Why (optional)"));
		Rationale->setTabChangesFocus(true);
		Rationale->setMaximumHeight(Rationale->fontMetrics().lineSpacing() * 4 + 12);
		column->addWidget(Rationale);

		Preview = new QPlainTextEdit{ this };
		Preview->setObjectName(QStringLiteral("suggestionPreview"));
		Preview->setAccessibleName(QStringLiteral("Before and after"));
		Preview->setReadOnly(true);
		Preview->setLineWrapMode(QPlainTextEdit::NoWrap);
		Preview->setTabChangesFocus(true);
		column->addWidget(Preview, 1);

		auto* const buttons = new QHBoxLayout{ };
		buttons->addStretch(1);
		Cancel = new QPushButton{ QStringLiteral("Cancel"), this };
		Cancel->setObjectName(QStringLiteral("suggestionCancel"));
		Save = new QPushButton{ QStringLiteral("Save suggestion"), this };
		Save->setObjectName(QStringLiteral("suggestionSave"));
		Save->setDefault(true);
		buttons->addWidget(Cancel);
		buttons->addWidget(Save);
		column->addLayout(buttons);

		set_code_font(suggestion_code_font());
		Editor->Changed = [this] { text_changed(); };
		connect(Save, &QPushButton::clicked, this, [this] {
			if (Saved) {
				Saved();
			}
		});
		connect(Cancel, &QPushButton::clicked, this, [this] {
			if (Cancelled) {
				Cancelled();
			}
		});
		setTabOrder(Editor, Illustrative);
		setTabOrder(Illustrative, Rationale);
		setTabOrder(Rationale, Preview);
		setTabOrder(Preview, Cancel);
		setTabOrder(Cancel, Save);
	}

	void suggestion_composer::begin(const suggestion_composer_source& aSource)
	{
		Source = aSource;
		// The proposal is classified inside its file when the file is the one
		// the selection was read from; otherwise inside its context alone.
		const auto context = Source.Before + Source.Original + Source.After;
		const auto inFile  = Source.Start >= Source.Before.size() && Source.Start <= Source.File.size()
			&& std::string_view{ Source.File }.substr(Source.Start - Source.Before.size()).starts_with(context);
		Texts[0]   = inFile ? Source.File : context;
		Base       = inFile ? Source.Start - Source.Before.size() : 0U;
		Selection  = Base + Source.Before.size();
		Texts[1]   = Texts[0];
		Classified = { };
		Location->setText(text_of(aSource.Location));
		Editor->set_text(aSource.Original);
		Illustrative->setChecked(false);
		Rationale->clear();
		set_source_status(suggestion_source_status::current);
		refresh_preview();
		request_syntax(suggestion_syntax_side::original);
		request_syntax(suggestion_syntax_side::proposed);
		Editor->setFocus(Qt::OtherFocusReason);
	}

	void suggestion_composer::text_changed()
	{
		Texts[1]      = core::splice_text(Texts[0], Selection, Selection + Source.Original.size(), Editor->text()).value_or(std::string{ });
		Classified[1] = { };
		refresh_preview();
		request_syntax(suggestion_syntax_side::proposed);
	}

	void suggestion_composer::request_syntax(const suggestion_syntax_side aSide)
	{
		if (SyntaxWanted) {
			SyntaxWanted({ .Side = aSide, .Path = Source.Path, .Document = Texts[static_cast<std::size_t>(aSide)] });
		}
	}

	void suggestion_composer::adopt_appearance(const appearance_profile& aProfile)
	{
		SyntaxColors = code_role_colors_of(aProfile.palette());
		apply_syntax();
	}

	auto suggestion_composer::adopt_syntax(const suggestion_syntax_side aSide, const std::string_view aDocument, application::shared_token_spans aSpans) -> bool
	{
		const auto index = static_cast<std::size_t>(aSide);
		if (!aSpans || aDocument != Texts[index]) {
			return false;
		}
		Classified[index] = { .LineStarts = line_starts_of(Texts[index]), .Spans = std::move(aSpans) };
		apply_syntax();
		return true;
	}

	auto suggestion_composer::formats_of(const suggestion_syntax_side aSide, const std::uint64_t aBegin, const std::string& aContent) const -> QList<QTextLayout::FormatRange>
	{
		// The spans of the classifier line holding this display line, cut to
		// it and counted from its start; a display line never crosses an LF.
		const auto& text = Classified[static_cast<std::size_t>(aSide)];
		if (!text.Spans || text.LineStarts.empty()) {
			return { };
		}
		const auto line  = static_cast<std::size_t>(std::ranges::upper_bound(text.LineStarts, aBegin) - text.LineStarts.begin()) - 1U;
		const auto start = text.LineStarts[line];
		const auto end   = aBegin + aContent.size();
		auto       spans = std::vector<application::token_span>{ };
		for (const auto& span : spans_of_line(*text.Spans, static_cast<std::uint32_t>(line))) {
			const auto from = std::max<std::uint64_t>(start + span.Column, aBegin);
			const auto to   = std::min<std::uint64_t>(start + span.Column + span.Length, end);
			if (from < to) {
				spans.push_back({ .Line = 0U, .Column = static_cast<std::uint32_t>(from - aBegin), .Length = static_cast<std::uint32_t>(to - from), .Role = span.Role });
			}
		}
		return code_syntax_format_ranges({ .LineBytes = bytes_of(aContent), .Spans = spans, .Colors = &SyntaxColors });
	}

	void suggestion_composer::apply_syntax()
	{
		if (Classified[1].Spans) {
			auto lines = std::vector<suggestion_line_syntax>{ };
			for (auto& line : lines_of(Editor->text(), Selection)) {
				auto formats = formats_of(suggestion_syntax_side::proposed, line.Begin, line.Content);
				lines.push_back({ .Content = std::move(line.Content), .Formats = std::move(formats) });
			}
			Editor->set_syntax(std::move(lines));
		}
		// Each preview row after its marker and a space.
		auto block = Preview->document()->firstBlock();
		for (const auto& row : PreviewRows) {
			if (!block.isValid()) {
				break;
			}
			auto formats = formats_of(row.Side, row.Begin, row.Content);
			for (auto& range : formats) {
				range.start += 2;
			}
			apply_syntax_format_ranges(*Preview->document(), block, formats);
			block = block.next();
		}
	}

	void suggestion_composer::set_source_status(const suggestion_source_status aStatus)
	{
		Status = aStatus;
		StatusLabel->setText(status_message(aStatus));
		StatusLabel->setVisible(aStatus != suggestion_source_status::current);
	}

	void suggestion_composer::set_code_font(const QFont& aFont)
	{
		Editor->set_code_font(aFont);
		Preview->setFont(aFont);
		Preview->setTabStopDistance(QFontMetricsF{ aFont }.horizontalAdvance(QLatin1Char(' ')) * core::default_suggestion_tab_width);
	}

	auto suggestion_composer::editor() const noexcept -> suggestion_editor*
	{
		return Editor;
	}

	auto suggestion_composer::replacement() const -> std::string
	{
		return Editor->text();
	}

	auto suggestion_composer::rationale() const -> std::string
	{
		const auto bytes = Rationale->toPlainText().toUtf8();
		return { bytes.constData(), static_cast<std::size_t>(bytes.size()) };
	}

	auto suggestion_composer::illustrative() const -> bool
	{
		return Illustrative->isChecked();
	}

	auto suggestion_composer::source_status() const noexcept -> suggestion_source_status
	{
		return Status;
	}

	auto suggestion_composer::preview_text() const -> QString
	{
		return Preview->toPlainText();
	}

	auto suggestion_composer::status_text() const -> QString
	{
		return StatusLabel->text();
	}

	auto suggestion_composer::save_button() const noexcept -> QPushButton*
	{
		return Save;
	}

	void suggestion_composer::refresh_preview()
	{
		// Whole lines around the selection, then its lines before and after the edit.
		const auto before   = lines_of(Source.Before + Source.Original + Source.After, Base);
		const auto after    = lines_of(Source.Before + Editor->text() + Source.After, Base);
		const auto leading  = complete_lines(Source.Before);
		const auto trailing = complete_lines(Source.After);
		auto       shown    = QString{ };
		PreviewRows.clear();
		const auto append = [&](const QChar aMarker, const suggestion_syntax_side aSide, const located_line& aLine) {
			shown += aMarker;
			shown += QLatin1Char(' ');
			shown += text_of(aLine.Content);
			shown += QLatin1Char('\n');
			PreviewRows.push_back({ .Side = aSide, .Begin = aLine.Begin, .Content = aLine.Content });
		};
		for (std::size_t index = 0; index < std::min(leading, before.size()); ++index) {
			append(QLatin1Char(' '), suggestion_syntax_side::original, before[index]);
		}
		for (std::size_t index = leading; index + trailing < before.size(); ++index) {
			append(QLatin1Char('-'), suggestion_syntax_side::original, before[index]);
		}
		for (std::size_t index = leading; index + trailing < after.size(); ++index) {
			append(QLatin1Char('+'), suggestion_syntax_side::proposed, after[index]);
		}
		for (std::size_t index = std::max(leading, after.size() - std::min(after.size(), trailing)); index < after.size(); ++index) {
			append(QLatin1Char(' '), suggestion_syntax_side::proposed, after[index]);
		}
		Preview->setPlainText(shown);
		apply_syntax();
	}
}
