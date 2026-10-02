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

#include <span>
#include <utility>
#include <vector>

namespace bpr::qt {
	namespace {
		/// Unchanged lines shown around each change in the preview.
		constexpr std::size_t preview_context = 2U;

		auto text_of(const std::string& aBytes) -> QString
		{
			return QString::fromUtf8(aBytes.data(), static_cast<qsizetype>(aBytes.size()));
		}

		auto bytes_of(const std::string& aText) -> std::span<const std::uint8_t>
		{
			return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
		}

		auto role_of(const code_role_colors& aColors, const core::appearance_role aRole) -> QColor
		{
			return aColors[static_cast<std::size_t>(aRole)];
		}

		auto status_message(const suggestion_source_status aStatus) -> QString
		{
			switch (aStatus) {
			case suggestion_source_status::current:         return { };
			case suggestion_source_status::newer_available: return QStringLiteral("A newer revision was captured. This suggestion stays on the revision you are editing.");
			case suggestion_source_status::stale:           return QStringLiteral("The code changed in the newer revision. The suggestion stays on the original bytes until you revalidate it.");
			case suggestion_source_status::ambiguous:       return QStringLiteral("The code appears more than once in the newer revision. Nothing was chosen for you.");
			case suggestion_source_status::deleted:         return QStringLiteral("The file is gone from the newer revision. The original source stays readable here.");
			}
			return { };
		}

		/// "3 changes · lines 3, 5, 7–8": where each change is in the original;
		/// an insertion names the line it goes before.
		auto summary_of(const std::string& aOriginal, const std::vector<core::cycle_edit_hunk>& aHunks) -> QString
		{
			auto places = QStringList{ };
			for (const auto& hunk : aHunks) {
				const auto span = core::project_lines(bytes_of(aOriginal), hunk.Start, hunk.End);
				if (span) {
					places.append(span->Last > span->First ? QStringLiteral("%1\u2013%2").arg(span->First).arg(span->Last) : QString::number(span->First));
				}
			}
			return aHunks.size() == 1U ? QStringLiteral("1 change · line %1").arg(places.value(0)) : QStringLiteral("%1 changes · lines %2").arg(aHunks.size()).arg(places.join(QStringLiteral(", ")));
		}
	}

	suggestion_composer::suggestion_composer(suggestion_editor& aEditor, QWidget* aParent)
		: QWidget{ aParent }
		, Editor{ &aEditor }
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

		Summary = new QLabel{ this };
		Summary->setObjectName(QStringLiteral("suggestionSummary"));
		Summary->setTextFormat(Qt::PlainText);
		Summary->setWordWrap(true);
		Summary->setAccessibleName(QStringLiteral("Suggested changes"));
		column->addWidget(Summary);

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
			if (Saved && !core::edit_hunks_problem(Hunks)) {
				Saved();
			}
		});
		connect(Cancel, &QPushButton::clicked, this, [this] {
			if (Cancelled) {
				Cancelled();
			}
		});
		setTabOrder(Illustrative, Rationale);
		setTabOrder(Rationale, Preview);
		setTabOrder(Preview, Cancel);
		setTabOrder(Cancel, Save);
	}

	auto suggestion_composer::begin(const suggestion_composer_source& aSource) -> bool
	{
		if (aSource.File.size() > core::maximum_suggestion_editor_bytes) {
			return false;
		}
		Source     = aSource;
		Texts      = { Source.File, Source.File };
		Classified = { };
		Hunks.clear();
		Location->setText(text_of(Source.Location));
		// The file already shown keeps its colours and scroll: only the caret changes.
		Editor->set_text(Source.File, Editor->model().state().TabWidth);
		Editor->set_indentation(core::indentation_for(Source.Path, Source.File));
		Editor->set_read_only(false);
		Editor->select_bytes(Source.Start, Source.End);
		Illustrative->setChecked(false);
		Rationale->clear();
		set_source_status(suggestion_source_status::current);
		refresh_preview();
		request_syntax(suggestion_syntax_side::original);
		request_syntax(suggestion_syntax_side::proposed);
		Editor->setFocus(Qt::OtherFocusReason);
		return true;
	}

	void suggestion_composer::text_changed()
	{
		Texts[1]      = Editor->text();
		Hunks         = core::derive_edit_hunks(Texts[0], Texts[1]);
		Classified[1] = Texts[1] == Texts[0] ? Classified[0] : classified_text{ };
		refresh_preview();
		if (!Classified[1].Spans) {
			request_syntax(suggestion_syntax_side::proposed);
		}
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
		Classified[index] = classify_text_lines(bytes_of(Texts[index]), std::move(aSpans));
		// The unedited file is both texts.
		if (Texts[0] == Texts[1]) {
			Classified[1 - index] = Classified[index];
		}
		apply_syntax();
		return true;
	}

	auto suggestion_composer::formats_of(const suggestion_syntax_side aSide, const std::uint64_t aBegin, const std::string& aContent) const -> QList<QTextLayout::FormatRange>
	{
		return code_syntax_format_ranges_at(Classified[static_cast<std::size_t>(aSide)], aBegin, bytes_of(aContent), SyntaxColors);
	}

	void suggestion_composer::apply_syntax()
	{
		if (Classified[1].Spans) {
			Editor->set_syntax(suggestion_syntax_lines(Classified[1], Texts[1], SyntaxColors));
		}
		// Each row after its marker and a space; a header in the diff panes' header colours.
		auto block = Preview->document()->firstBlock();
		auto bands = QList<QTextEdit::ExtraSelection>{ };
		for (std::size_t index = 0; index < PreviewRows.size() && block.isValid(); ++index, block = block.next()) {
			const auto& row     = PreviewRows[index];
			const auto  line    = core::line_span{ .Begin = static_cast<std::uint32_t>(index), .End = static_cast<std::uint32_t>(index + 1U) };
			auto        formats = QList<QTextLayout::FormatRange>{ };
			switch (row.Kind) {
			case core::cycle_edit_row_kind::header: {
				append_line_band(bands, *Preview, line, role_of(SyntaxColors, core::appearance_role::diff_hunk_header_background));
				if (const auto colour = role_of(SyntaxColors, core::appearance_role::diff_hunk_header_text); colour.isValid()) {
					auto range = QTextLayout::FormatRange{ .start = 0, .length = block.length() - 1, .format = { } };
					range.format.setForeground(colour);
					formats.append(range);
				}
				break;
			}
			case core::cycle_edit_row_kind::removed:
			case core::cycle_edit_row_kind::added:
				append_line_band(bands, *Preview, line, code_sample_row_fill(row.Kind == core::cycle_edit_row_kind::removed ? code_sample_row_kind::removed : code_sample_row_kind::added, SyntaxColors));
				[[fallthrough]];
			case core::cycle_edit_row_kind::context:
				formats = formats_of(row.Edited ? suggestion_syntax_side::proposed : suggestion_syntax_side::original, row.Begin, row.Text);
				for (auto& range : formats) {
					range.start += 2;
				}
				break;
			}
			apply_syntax_format_ranges(*Preview->document(), block, formats);
		}
		Preview->setExtraSelections(bands);
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

	auto suggestion_composer::edits() const noexcept -> const std::vector<core::cycle_edit_hunk>&
	{
		return Hunks;
	}

	auto suggestion_composer::source() const noexcept -> const suggestion_composer_source&
	{
		return Source;
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

	auto suggestion_composer::preview() const noexcept -> QPlainTextEdit*
	{
		return Preview;
	}

	auto suggestion_composer::summary_text() const -> QString
	{
		return Summary->text();
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
		PreviewRows = core::preview_edit_hunks(Texts[0], Texts[1], Hunks, preview_context);
		auto shown  = QStringList{ };
		for (const auto& row : PreviewRows) {
			switch (row.Kind) {
			case core::cycle_edit_row_kind::header:  shown.append(text_of(row.Text)); break;
			case core::cycle_edit_row_kind::context: shown.append(QStringLiteral("  ") + text_of(row.Text)); break;
			case core::cycle_edit_row_kind::removed: shown.append(QStringLiteral("- ") + text_of(row.Text)); break;
			case core::cycle_edit_row_kind::added:   shown.append(QStringLiteral("+ ") + text_of(row.Text)); break;
			}
		}
		Preview->setPlainText(shown.join(QLatin1Char('\n')));
		const auto problem = core::edit_hunks_problem(Hunks);
		if (!problem) {
			Summary->setText(summary_of(Texts[0], Hunks));
		} else {
			switch (*problem) {
			case core::cycle_edit_problem::unchanged:
				Summary->setText(QStringLiteral("Edit the file to suggest a change. Each separate change becomes one exact suggestion on this note."));
				break;
			case core::cycle_edit_problem::too_many:
				Summary->setText(QStringLiteral("%1 separate changes; one note holds at most %2. Keep fewer changes, or save them as separate suggestions.").arg(Hunks.size()).arg(core::maximum_cycle_suggestions));
				break;
			case core::cycle_edit_problem::too_large:
				Summary->setText(QStringLiteral("One change is larger than %1 KiB, the most one suggestion holds.").arg(core::maximum_cycle_suggestion_bytes / 1024U));
				break;
			}
		}
		Save->setEnabled(!problem);
		apply_syntax();
	}
}
