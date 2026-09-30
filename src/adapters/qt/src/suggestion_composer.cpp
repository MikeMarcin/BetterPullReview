#include "bpr/qt/suggestion_composer.hpp"

#include <QCheckBox>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <vector>

namespace bpr::qt {
	namespace {
		auto text_of(const std::string& aBytes) -> QString
		{
			return QString::fromUtf8(aBytes.data(), static_cast<qsizetype>(aBytes.size()));
		}

		/// Display lines of exact bytes: LF, CRLF and a lone CR each end a line.
		auto lines_of(const std::string& aBytes) -> std::vector<QString>
		{
			auto lines = std::vector<QString>{ };
			for (const auto& line : core::split_text_lines(aBytes)) {
				lines.push_back(text_of(line.Content));
			}
			return lines;
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
		Editor->Changed = [this] { refresh_preview(); };
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
		Location->setText(text_of(aSource.Location));
		Editor->set_text(aSource.Original);
		Illustrative->setChecked(false);
		Rationale->clear();
		set_source_status(suggestion_source_status::current);
		refresh_preview();
		Editor->setFocus(Qt::OtherFocusReason);
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
		const auto before   = lines_of(Source.Before + Source.Original + Source.After);
		const auto after    = lines_of(Source.Before + Editor->text() + Source.After);
		const auto leading  = complete_lines(Source.Before);
		const auto trailing = complete_lines(Source.After);
		auto       shown    = QString{ };
		const auto append   = [&shown](const QChar aMarker, const QString& aLine) {
			shown += aMarker;
			shown += QLatin1Char(' ');
			shown += aLine;
			shown += QLatin1Char('\n');
		};
		for (std::size_t index = 0; index < std::min(leading, before.size()); ++index) {
			append(QLatin1Char(' '), before[index]);
		}
		for (std::size_t index = leading; index + trailing < before.size(); ++index) {
			append(QLatin1Char('-'), before[index]);
		}
		for (std::size_t index = leading; index + trailing < after.size(); ++index) {
			append(QLatin1Char('+'), after[index]);
		}
		for (std::size_t index = std::max(leading, after.size() - std::min(after.size(), trailing)); index < after.size(); ++index) {
			append(QLatin1Char(' '), after[index]);
		}
		Preview->setPlainText(shown);
	}
}
