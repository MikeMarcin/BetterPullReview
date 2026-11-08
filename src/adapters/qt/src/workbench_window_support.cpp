#include "workbench_window_support.hpp"

#include "bpr/qt/keyboard_shortcuts.hpp"

#include <QAbstractItemView>
#include <QBrush>
#include <QChar>
#include <QColor>
#include <QFontMetrics>
#include <QLatin1Char>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QPlainTextEdit>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTextEdit>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	draft_excerpt_view::draft_excerpt_view(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("discussDraftExcerpt"));
		setAccessibleName(QStringLiteral("The code the draft is on"));
		setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
		hide();
	}

	void draft_excerpt_view::present(const QString& aPath, const std::uint32_t aFirstNumber, std::vector<QString> aLines,
									 const std::uint32_t aBracketFirst, const std::uint32_t aBracketLast, const QFont& aCode)
	{
		Path         = aPath;
		FirstNumber  = aFirstNumber;
		Lines        = std::move(aLines);
		BracketFirst = aBracketFirst;
		BracketLast  = aBracketLast;
		Code         = aCode;
		setAccessibleDescription(QStringLiteral("%1, %2").arg(Path, words()));
		setVisible(!Lines.empty());
		updateGeometry();
		update();
	}

	void draft_excerpt_view::clear()
	{
		Lines.clear();
		hide();
	}

	auto draft_excerpt_view::words() const -> QString
	{
		return BracketFirst == BracketLast ? QStringLiteral("Line %1").arg(BracketFirst) : QStringLiteral("Lines %1–%2").arg(BracketFirst).arg(BracketLast);
	}

	auto draft_excerpt_view::sizeHint() const -> QSize
	{
		return { 320, header() + static_cast<int>(Lines.size()) * row() + 2 };
	}

	auto draft_excerpt_view::minimumSizeHint() const -> QSize
	{
		return { 120, sizeHint().height() };
	}

	void draft_excerpt_view::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto       painter = QPainter{ this };
		const auto base    = palette().color(QPalette::Active, QPalette::Base);
		const auto ink     = palette().color(QPalette::Active, QPalette::Text);
		const auto faint   = palette().color(QPalette::Active, QPalette::PlaceholderText);
		const auto signal  = palette().color(QPalette::Active, QPalette::Highlight);
		auto       rule    = ink;
		rule.setAlphaF(0.16F);
		painter.setRenderHint(QPainter::Antialiasing, true);
		auto clip = QPainterPath{ };
		clip.addRoundedRect(QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5), 8.0, 8.0);
		painter.setPen(QPen{ rule, 1.0 });
		painter.setBrush(base);
		painter.drawPath(clip);
		painter.setClipPath(clip);
		auto bold = font();
		bold.setWeight(QFont::DemiBold);
		painter.setFont(bold);
		painter.setPen(ink);
		const auto title = QStringLiteral("%1   %2").arg(Path.section(QLatin1Char{ '/' }, -1), words());
		painter.drawText(QRect{ 10, 0, width() - 20, header() }, Qt::AlignLeft | Qt::AlignVCenter,
						 QFontMetrics{ bold }.elidedText(title, Qt::ElideMiddle, std::max(0, width() - 20)));
		painter.setPen(QPen{ rule, 1.0 });
		painter.drawLine(QPointF{ 0.0, header() - 0.5 }, QPointF{ static_cast<qreal>(width()), header() - 0.5 });
		painter.setFont(Code);
		const auto metrics = QFontMetrics{ Code };
		const auto gutter  = metrics.horizontalAdvance(QStringLiteral("00000")) + 12;
		auto       wash    = signal;
		wash.setAlphaF(0.14F);
		for (auto index = std::size_t{ 0U }; index < Lines.size(); ++index) {
			const auto number = FirstNumber + static_cast<std::uint32_t>(index);
			const auto top    = header() + static_cast<int>(index) * row();
			const auto inside = number >= BracketFirst && number <= BracketLast;
			if (inside) {
				painter.fillRect(QRect{ 0, top, width(), row() }, wash);
				painter.fillRect(QRect{ gutter - 3, top, 3, row() }, signal);
			}
			painter.setPen(inside ? signal : faint);
			painter.drawText(QRect{ 0, top, gutter - 8, row() }, Qt::AlignRight | Qt::AlignVCenter, QString::number(number));
			auto text = Lines[index];
			text.replace(QLatin1Char{ '\t' }, QStringLiteral("    "));
			painter.setPen(ink);
			painter.drawText(QRect{ gutter + 8, top, width() - gutter - 16, row() }, Qt::AlignLeft | Qt::AlignVCenter,
							 metrics.elidedText(text, Qt::ElideRight, std::max(0, width() - gutter - 16)));
		}
	}

	auto draft_excerpt_view::header() const -> int
	{
		return fontMetrics().height() + 12;
	}

	auto draft_excerpt_view::row() const -> int
	{
		return QFontMetrics{ Code }.lineSpacing() + 4;
	}

	auto reason_text(const core::workbench_reason_code aReason)
		-> QString
	{
		switch (aReason) {
		case core::workbench_reason_code::none:
			return { };
		case core::workbench_reason_code::no_active_workbench:
			return QStringLiteral("Open a pull request to review its files.");
		case core::workbench_reason_code::no_actionable_files:
			return QStringLiteral("This pull request has no actionable files.");
		// [[BPR-1328]]. The navigation arrows stay pressable at their
		// bounds; these say why a press there did nothing.
		case core::workbench_reason_code::at_first_file:
			return QStringLiteral("First file in this pull request.");
		case core::workbench_reason_code::at_last_file:
			return QStringLiteral("Last file in this pull request.");
		case core::workbench_reason_code::invalid_file_identity:
			return QStringLiteral("The selected file identity is invalid.");
		case core::workbench_reason_code::invalid_navigation_cursor:
			return QStringLiteral("The file navigation position is invalid.");
		case core::workbench_reason_code::no_open_comparison:
			return QStringLiteral("No file comparison is open.");
		case core::workbench_reason_code::no_changes:
			return QStringLiteral(
				"This comparison has no changes to navigate.");
		case core::workbench_reason_code::at_first_change:
			return QStringLiteral("First change in this file.");
		case core::workbench_reason_code::at_last_change:
			return QStringLiteral("Last change in this file.");
		case core::workbench_reason_code::invalid_change_selection:
			return QStringLiteral("The selected change is no longer available.");
		case core::workbench_reason_code::invalid_change_cursor:
			return QStringLiteral("The change navigation position is invalid.");
		case core::workbench_reason_code::overview_keeps_drawers_collapsed:
			return QStringLiteral(
				"Overview keeps every drawer collapsed. Choose Review or "
				"Discuss to open one.");
		case core::workbench_reason_code::discuss_shows_the_conversation:
			return QStringLiteral(
				"In Discuss the thread is the page. Choose Review to "
				"hide or show the drawers.");
		case core::workbench_reason_code::threads_finds_a_thread:
			return QStringLiteral(
				"In Threads the finder is the page. Choose Review to "
				"hide or show the drawers.");
		case core::workbench_reason_code::header_source_requires_review:
			return QStringLiteral("Choose Review to go to a matching header or source file.");
		case core::workbench_reason_code::header_source_no_selection:
			return QStringLiteral("Select a header or source file first.");
		case core::workbench_reason_code::header_source_unsupported_file:
			return QStringLiteral("This file has no supported header or source extension.");
		case core::workbench_reason_code::header_source_unavailable_catalogue:
			return QStringLiteral("The changed-file catalogue is unavailable. Refresh the pull request.");
		}
		// Deliberately outside the switch and with no default label, so a
		// reason the core adds later is a compile error here rather than a
		// silently generic sentence in the toolbar.
		return QStringLiteral("Review navigation is unavailable.");
	}

	auto drawer_reason_text(
		const core::workbench_reason_code aReason) -> QString
	{
		if (aReason == core::workbench_reason_code::no_active_workbench) {
			return QStringLiteral(
				"Open a pull request to use its review drawers.");
		}
		return reason_text(aReason);
	}

	auto change_reason_text(
		const core::workbench_reason_code aReason) -> QString
	{
		if (aReason == core::workbench_reason_code::no_active_workbench) {
			return QStringLiteral(
				"Open a pull request to review its changes.");
		}
		return reason_text(aReason);
	}

	auto failure_text(
		const application::workbench_host_failure aFailure) -> QString
	{
		switch (aFailure) {
		case application::workbench_host_failure::observation_unavailable:
			return QStringLiteral("The actionable file list is unavailable.");
		case application::workbench_host_failure::observation_limit:
			return QStringLiteral(
				"This pull request has more actionable files than the workbench "
				"retains.");
		case application::workbench_host_failure::settings_unavailable:
			return QStringLiteral("The saved workbench layout is unavailable.");
		case application::workbench_host_failure::selected_file_binary:
			return QStringLiteral("The selected file is binary.");
		case application::workbench_host_failure::selected_file_missing_left:
			return QStringLiteral("Comparison unavailable: the earlier selected revision has no file at this path. Pull-request status may describe a different comparison.");
		case application::workbench_host_failure::selected_file_missing_right:
			return QStringLiteral("Comparison unavailable: the later selected revision has no file at this path. Pull-request status may describe a different comparison.");
		case application::workbench_host_failure::selected_file_missing_both:
			return QStringLiteral("The selected file is absent from both revisions.");
		case application::workbench_host_failure::selected_file_ambiguous:
			return QStringLiteral(
				"The selected rename matches both paths in one revision.");
		case application::workbench_host_failure::selected_file_cancelled:
			return QStringLiteral("Loading the selected file was cancelled.");
		case application::workbench_host_failure::selected_file_comparison_unavailable:
			return QStringLiteral(
				"No exact comparison is ready for this file. Check Review history, "
				"then retry.");
		case application::workbench_host_failure::selected_file_stale:
			return QStringLiteral("The selected file changed before it could be loaded.");
		case application::workbench_host_failure::host_failure:
			return QStringLiteral("The workbench could not be updated.");
		case application::workbench_host_failure::selected_file_split_by_commit_filter:
			// RFC-BPR-0089: one exact diff cannot show it, and none is invented.
			return QStringLiteral(
				"A hidden commit changed this file between two shown commits, so no single diff shows only the shown changes. "
				"Show the hidden commit, or hide one of the others, to read it.");
		}
		return QStringLiteral("The workbench could not be updated.");
	}

	auto failure_text(
		const application::workbench_host_failure aFailure,
		const epoch_comparison_control*           aEpoch) -> QString
	{
		if (aFailure == application::workbench_host_failure::selected_file_comparison_unavailable && aEpoch != nullptr) {
			if (const auto why = aEpoch->problem_text(); !why.isEmpty()) {
				return QStringLiteral("No exact comparison is ready for this file. %1").arg(why);
			}
		}
		return failure_text(aFailure);
	}

	auto change_end_keys() -> std::array<QKeySequence, 2>
	{
		return { shortcut_key(shortcut_command::first_change), shortcut_key(shortcut_command::last_change) };
	}

	auto editable_multi_line(const QObject* const aObject) -> bool
	{
		if (const auto* const plain = qobject_cast<const QPlainTextEdit*>(aObject); plain != nullptr) {
			return !plain->isReadOnly();
		}
		if (const auto* const rich = qobject_cast<const QTextEdit*>(aObject); rich != nullptr) {
			return !rich->isReadOnly();
		}
		return false;
	}

	auto accept_file_key() -> QKeySequence
	{
		return shortcut_key(shortcut_command::accept_file);
	}

	auto takes_typed_text(const QWidget* const aWidget) -> bool
	{
		if (aWidget == nullptr || qobject_cast<const QAbstractItemView*>(aWidget) != nullptr) {
			return false;
		}
		if (qobject_cast<const QPlainTextEdit*>(aWidget) != nullptr || qobject_cast<const QTextEdit*>(aWidget) != nullptr) {
			return editable_multi_line(aWidget);
		}
		if (const auto* const line = qobject_cast<const QLineEdit*>(aWidget); line != nullptr) {
			return !line->isReadOnly();
		}
		return aWidget->testAttribute(Qt::WA_InputMethodEnabled);
	}

	auto option_character_key(const QKeyCombination aKey) -> bool
	{
		const auto modifiers = aKey.keyboardModifiers() & ~Qt::KeypadModifier;
		const auto key       = aKey.key();
		return (modifiers == Qt::AltModifier || modifiers == (Qt::AltModifier | Qt::ShiftModifier))
			&& key >= Qt::Key_Space && key < Qt::Key_Escape;
	}

	void describe_navigation_action(
		QAction* const                aAction,
		const QString&                aName,
		const bool                    aEnabled,
		const QString&                aEnabledText,
		const QString&                aDetail,
		const std::optional<QString>& aKey)
	{
		aAction->setEnabled(aEnabled);
		// [[BPR-1815]]. A View menu key that typing lifts from the menu bar
		// is named by the caller, which knows the lifted key.
		const auto key   = aKey.value_or(aAction->shortcut().toString(QKeySequence::NativeText));
		const auto title = key.isEmpty() ? aName : QStringLiteral("%1 (%2)").arg(aName, key);
		aAction->setToolTip(aDetail.isEmpty() ? title : title + QLatin1Char{ '\n' } + aDetail);
		aAction->setStatusTip(aDetail.isEmpty() ? aEnabledText : aDetail);
	}

	void describe_action(
		QAction* const aAction,
		const bool     aEnabled,
		const QString& aEnabledText,
		const QString& aDisabledText)
	{
		aAction->setEnabled(aEnabled);
		const auto description = aEnabled ? aEnabledText : aDisabledText;
		aAction->setToolTip(description);
		aAction->setStatusTip(description);
	}

	auto counted(const std::uint32_t aCount, const QString& aOne, const QString& aMany) -> QString
	{
		return aCount == 1U ? aOne : aMany.arg(aCount);
	}

	auto refresh_age_words(const core::refresh_age aAge) -> QString
	{
		switch (aAge.Unit) {
		case core::refresh_age_unit::just_now: return QStringLiteral("just now");
		case core::refresh_age_unit::minutes:  return QStringLiteral("%1 min ago").arg(aAge.Count);
		case core::refresh_age_unit::hours:    return QStringLiteral("%1 hr ago").arg(aAge.Count);
		case core::refresh_age_unit::days:     return aAge.Count == 1U ? QStringLiteral("1 day ago") : QStringLiteral("%1 days ago").arg(aAge.Count);
		}
		return { };
	}
}
