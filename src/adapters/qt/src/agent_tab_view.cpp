#include "bpr/qt/agent_tab_view.hpp"

#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/quiet_tool_button.hpp"

#include "bpr/core/agent_finding.hpp"
#include "bpr/core/defect_workflow.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_setup_view.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/code_sample_painter.hpp"
#include "bpr/qt/disclosure_button.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/finding_queue_view.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/side_chat_panel.hpp"

#include <QAbstractItemModel>
#include <QAction>
#include <QActionGroup>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QEnterEvent>
#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStyleOptionToolButton>
#include <QStylePainter>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	namespace {
		/// A reply card's side margins; its answer is this much narrower.
		constexpr auto reply_left_margin  = 12;
		constexpr auto reply_right_margin = 10;

		[[nodiscard]] auto mixed(const QColor& aFrom, const QColor& aTo, const qreal aShare) -> QColor
		{
			const auto keep = 1.0 - aShare;
			return QColor::fromRgbF(static_cast<float>(aFrom.redF() * keep + aTo.redF() * aShare), static_cast<float>(aFrom.greenF() * keep + aTo.greenF() * aShare),
									static_cast<float>(aFrom.blueF() * keep + aTo.blueF() * aShare));
		}

		[[nodiscard]] auto dark(const QPalette& aPalette) -> bool
		{
			return aPalette.color(QPalette::Window).lightness() < 128;
		}

		/// \brief A frame that draws a rounded card in the palette: the
		/// banner's callout, a finding's card, the question's field.
		class rounded final : public QWidget
		{
		public:
			enum class look : std::uint8_t {
				card,
				callout,
				danger,
				field,
				ready,
			};
			rounded(const look aLook, QWidget* aParent)
				: QWidget{ aParent }
				, Look{ aLook }
			{
			}
			look Look;
			/// The severity's edge, on a finding's card.
			QColor Edge;
			bool   Selected = false;

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto ink    = palette().color(QPalette::Active, QPalette::WindowText);
				const auto base   = palette().color(QPalette::Active, QPalette::Base);
				const auto window = palette().color(QPalette::Active, QPalette::Window);
				const auto box    = QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
				auto       fill   = base;
				auto       line   = mixed(base, ink, 0.16);
				if (Look == look::callout) {
					fill = mixed(window, palette().color(QPalette::Active, QPalette::Highlight), 0.10);
					line = mixed(window, palette().color(QPalette::Active, QPalette::Highlight), 0.30);
				} else if (Look == look::ready) {
					const auto green = dark(palette()) ? QColor{ 0x3F, 0xB9, 0x50 } : QColor{ 0x1A, 0x7F, 0x37 };
					fill             = mixed(window, green, 0.10);
					line             = mixed(window, green, 0.35);
				} else if (Look == look::danger) {
					const auto red = dark(palette()) ? QColor{ 0xF8, 0x51, 0x49 } : QColor{ 0xCF, 0x22, 0x2E };
					fill           = mixed(window, red, 0.10);
					line           = mixed(window, red, 0.35);
				}
				if (Selected) {
					line = dark(palette()) ? QColor{ 0xE3, 0xB3, 0x41 } : QColor{ 0xC3, 0x8A, 0x10 };
				}
				painter.setPen(QPen{ line, 1.0 });
				painter.setBrush(fill);
				painter.drawRoundedRect(box, 8.0, 8.0);
				if (Edge.isValid()) {
					painter.setPen(Qt::NoPen);
					painter.setBrush(Edge);
					painter.drawRoundedRect(QRectF{ box.left(), box.top() + 6.0, 3.0, box.height() - 12.0 }, 1.5, 1.5);
				}
			}

		private:
		};

		/// \brief A tool button whose menu opens on a press and draws no
		/// arrow: the word or the dots are the affordance in a narrow drawer.
		class menu_word final : public QToolButton
		{
		public:
			explicit menu_word(QWidget* aParent)
				: QToolButton{ aParent }
			{
				setAutoRaise(true);
				setPopupMode(QToolButton::InstantPopup);
			}

			/// Owner look v003: a word that may shorten, but never below a few
			/// letters, so a narrow row keeps every other control whole.
			[[nodiscard]] auto minimumSizeHint() const -> QSize override
			{
				const auto hint = QToolButton::sizeHint();
				return { std::min(hint.width(), fontMetrics().horizontalAdvance(QStringLiteral("Mmmm…")) + 12), hint.height() };
			}

			/// Drawn as a rounded pill in the field's own colours, as the
			/// agent drawers draw their model chip.
			bool Pill = false;

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				if (Pill) {
					auto       painter = QPainter{ this };
					const auto ink     = palette().color(QPalette::Active, QPalette::WindowText);
					const auto base    = palette().color(QPalette::Active, QPalette::Base);
					painter.setRenderHint(QPainter::Antialiasing, true);
					painter.setPen(Qt::NoPen);
					painter.setBrush(mixed(base, ink, underMouse() || isDown() ? 0.16 : 0.08));
					const auto box = QRectF{ rect() }.adjusted(0.5, 1.5, -0.5, -1.5);
					painter.drawRoundedRect(box, box.height() / 2.0, box.height() / 2.0);
					if (!icon().isNull() && text().isEmpty()) {
						const auto side = std::min(16, height() - 6);
						icon().paint(&painter, QRect{ (width() - side) / 2, (height() - side) / 2, side, side });
						return;
					}
					painter.setPen(ink);
					painter.drawText(rect().adjusted(10, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft, fontMetrics().elidedText(text(), Qt::ElideRight, std::max(0, width() - 20)));
					return;
				}
				auto painter = QStylePainter{ this };
				auto option  = QStyleOptionToolButton{ };
				initStyleOption(&option);
				option.features &= ~QStyleOptionToolButton::HasMenu;
				option.text = fontMetrics().elidedText(text(), Qt::ElideRight, std::max(0, width() - 12));
				painter.drawComplexControl(QStyle::CC_ToolButton, option);
			}

			[[nodiscard]] auto sizeHint() const -> QSize override
			{
				const auto hint = QToolButton::sizeHint();
				if (!Pill) {
					return hint;
				}
				const auto wide = text().isEmpty() ? 28 : fontMetrics().horizontalAdvance(text()) + 22;
				return { wide, std::max(hint.height(), fontMetrics().height() + 8) };
			}

			void enterEvent(QEnterEvent* aEvent) override
			{
				QToolButton::enterEvent(aEvent);
				update();
			}
			void leaveEvent(QEvent* aEvent) override
			{
				QToolButton::leaveEvent(aEvent);
				update();
			}
		};

		/// \brief A thin rule inside the question field, in its outline's colour.
		class rule_line final : public QWidget
		{
		public:
			explicit rule_line(QWidget* aParent)
				: QWidget{ aParent }
			{
				setFixedHeight(1);
			}

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto painter = QPainter{ this };
				painter.fillRect(rect(), mixed(palette().color(QPalette::Active, QPalette::Base), palette().color(QPalette::Active, QPalette::WindowText), 0.14));
			}
		};

		/// \brief Owner look v003 ("Timestamp would also be nice"): a time
		/// as near words, the whole of it on hover.
		[[nodiscard]] auto relative_time(const QDateTime& aWhen) -> QString
		{
			if (!aWhen.isValid()) {
				return { };
			}
			const auto now     = QDateTime::currentDateTime();
			const auto seconds = aWhen.secsTo(now);
			if (seconds < 60) {
				return QObject::tr("just now");
			}
			if (seconds < 3600) {
				return QObject::tr("%n min ago", nullptr, static_cast<int>(seconds / 60));
			}
			const auto local = aWhen.toLocalTime();
			if (local.date() == now.date()) {
				return QLocale{ }.toString(local.time(), QLocale::ShortFormat);
			}
			return QLocale{ }.toString(local, QLocale::ShortFormat);
		}

		void show_time(QLabel& aLabel, const QDateTime& aWhen)
		{
			aLabel.setText(relative_time(aWhen));
			aLabel.setToolTip(aWhen.isValid() ? QLocale{ }.toString(aWhen.toLocalTime(), QLocale::LongFormat) : QString{ });
			aLabel.setVisible(aWhen.isValid());
		}

		/// \brief A quiet chip: the agent's head, the scope, a context.
		[[nodiscard]] auto chip_label(QWidget* aParent, const QString& aName) -> QLabel*
		{
			auto* const chip = new QLabel{ aParent };
			chip->setObjectName(aName);
			chip->setTextFormat(Qt::PlainText);
			chip->setMargin(3);
			chip->setForegroundRole(QPalette::WindowText);
			return chip;
		}

		[[nodiscard]] auto severity_colour(const core::defect_severity aSeverity, const QPalette& aPalette) -> QColor
		{
			const auto night = dark(aPalette);
			switch (aSeverity) {
			case core::defect_severity::critical:
			case core::defect_severity::major:    return night ? QColor{ 0xF8, 0x51, 0x49 } : QColor{ 0xCF, 0x22, 0x2E };
			case core::defect_severity::minor:    return night ? QColor{ 0xE3, 0xB3, 0x41 } : QColor{ 0xC3, 0x8A, 0x10 };
			case core::defect_severity::trivial:  return aPalette.color(QPalette::Active, QPalette::PlaceholderText);
			}
			return aPalette.color(QPalette::Active, QPalette::PlaceholderText);
		}

		[[nodiscard]] auto confidence_words(const core::agent_finding_confidence aConfidence) -> QString
		{
			switch (aConfidence) {
			case core::agent_finding_confidence::high:     return QObject::tr("likely");
			case core::agent_finding_confidence::medium:   return QObject::tr("possible");
			case core::agent_finding_confidence::low:      return QObject::tr("unsure");
			case core::agent_finding_confidence::unstated: break;
			}
			return QObject::tr("unrated");
		}

		[[nodiscard]] auto capitalised(std::string_view aName) -> QString
		{
			auto words = QString::fromUtf8(aName.data(), static_cast<qsizetype>(aName.size()));
			words.replace(QLatin1Char{ '_' }, QLatin1Char{ ' ' });
			if (!words.isEmpty()) {
				words[0] = words[0].toUpper();
			}
			return words;
		}
	} // namespace

	auto agent_model_display_name(const QString& aModel) -> QString
	{
		auto       name    = aModel.trimmed();
		auto       context = QString{ };
		const auto bracket = name.indexOf(QLatin1Char{ '[' });
		if (bracket > 0 && name.endsWith(QLatin1Char{ ']' })) {
			// `claude-opus-5-5[1m]`: the window the agent asked for.
			context = name.mid(bracket + 1, name.size() - bracket - 2).toUpper();
			name    = name.left(bracket);
		}
		const auto        family  = [](const QString& aWord) { return aWord.left(1).toUpper() + aWord.mid(1).toLower(); };
		const auto        with    = [&context](QString aShown) { return context.isEmpty() ? aShown : aShown + QStringLiteral(" (") + context + QLatin1Char{ ')' }; };
		static const auto aliases = QStringList{ QStringLiteral("opus"), QStringLiteral("sonnet"), QStringLiteral("haiku"), QStringLiteral("fable") };
		if (aliases.contains(name.toLower())) {
			return with(QObject::tr("%1 (newest)").arg(family(name)));
		}
		// `claude-<family>-<major>[-<minor>][-<date>]`.
		auto parts = name.split(QLatin1Char{ '-' });
		if (parts.size() >= 3 && parts.front().compare(QStringLiteral("claude"), Qt::CaseInsensitive) == 0) {
			if (parts.size() >= 4 && parts.back().size() == 8) {
				parts.removeLast();
			}
			auto       majorOk = false;
			const auto major   = parts[2].toUInt(&majorOk);
			if (majorOk && parts[1].size() > 1 && parts.size() <= 4) {
				auto version = QString::number(major);
				if (parts.size() == 4) {
					auto       minorOk = false;
					const auto minor   = parts[3].toUInt(&minorOk);
					if (!minorOk || parts[3].size() > 2) {
						return aModel.trimmed();
					}
					version += QLatin1Char{ '.' } + QString::number(minor);
				}
				return with(family(parts[1]) + QLatin1Char{ ' ' } + version);
			}
		}
		return aModel.trimmed();
	}

	auto agent_line_link(const QString& aPath, const std::uint32_t aLine, const bool aBefore) -> QString
	{
		return aPath + (aBefore ? QStringLiteral("#B") : QStringLiteral("#L")) + QString::number(aLine);
	}

	auto agent_line_link_target(const QString& aLink) -> std::optional<std::pair<QString, std::pair<std::uint32_t, bool>>>
	{
		const auto hash = aLink.lastIndexOf(QLatin1Char{ '#' });
		if (hash <= 0 || hash + 2 >= aLink.size() || (aLink[hash + 1] != QLatin1Char{ 'L' } && aLink[hash + 1] != QLatin1Char{ 'B' })) {
			return std::nullopt;
		}
		auto       number = false;
		const auto line   = aLink.mid(hash + 2).toUInt(&number);
		if (!number || line == 0U || aLink.contains(QStringLiteral("://"))) {
			return std::nullopt;
		}
		return std::pair{ aLink.left(hash), std::pair{ static_cast<std::uint32_t>(line), aLink[hash + 1] == QLatin1Char{ 'B' } } };
	}

	auto agent_line_references(const QString& aText, const agent_file_resolver& aResolve, const QString& aContextFile) -> std::vector<agent_line_reference>
	{
		auto found = std::vector<agent_line_reference>{ };
		if (!aResolve) {
			return found;
		}
		const auto taken = [&found](const qsizetype aBegin, const qsizetype aEnd) {
			return std::ranges::any_of(found, [&](const agent_line_reference& aKnown) { return aBegin < aKnown.Begin + aKnown.Length && aKnown.Begin < aEnd; });
		};
		const auto add = [&](const QRegularExpressionMatch& aMatch, const QString& aPath, const int aFirst, const int aLast, const bool aBefore) {
			auto       firstOk = false;
			auto       lastOk  = false;
			const auto first   = aMatch.captured(aFirst).toUInt(&firstOk);
			const auto last    = aLast > 0 && aMatch.hasCaptured(aLast) ? aMatch.captured(aLast).toUInt(&lastOk) : first;
			if (!firstOk || first == 0U || taken(aMatch.capturedStart(), aMatch.capturedEnd())) {
				return;
			}
			found.push_back({ .Begin  = aMatch.capturedStart(),
							  .Length = aMatch.capturedLength(),
							  .Path   = aPath,
							  .First  = static_cast<std::uint32_t>(first),
							  .Last   = static_cast<std::uint32_t>(std::max(first, aLast > 0 && aMatch.hasCaptured(aLast) && lastOk ? last : first)),
							  .Before = aBefore });
		};
		// A path: words of letters, digits, `.`, `_` and `-` between slashes,
		// the last with an extension.
		static const auto path  = QStringLiteral(R"((?<![\w./\\-])((?:[\w.-]+/)*[\w-][\w.-]*\.[A-Za-z0-9_+]+))");
		static const auto named = QRegularExpression{ path + QStringLiteral(R"( ([Ll]ines?) (\d+)(?:\s*(?:[-–]|to)\s*(\d+))?( before the change)?(?!\d))") };
		static const auto colon = QRegularExpression{ path + QStringLiteral(R"(:(\d+)(?:[-–](\d+))?(?![\d\w]))") };
		static const auto bare  = QRegularExpression{ QStringLiteral(R"((?<![\w/.-])[Ll]ines? (\d+)(?:\s*(?:[-–]|to|and)\s*(\d+))?( before the change)?(?!\d))") };
		for (auto it = named.globalMatch(aText); it.hasNext();) {
			const auto match = it.next();
			if (const auto resolved = aResolve(match.captured(1))) {
				add(match, *resolved, 3, 4, match.hasCaptured(5));
			}
		}
		for (auto it = colon.globalMatch(aText); it.hasNext();) {
			const auto match = it.next();
			if (const auto resolved = aResolve(match.captured(1))) {
				add(match, *resolved, 2, 3, false);
			}
		}
		if (!aContextFile.isEmpty()) {
			for (auto it = bare.globalMatch(aText); it.hasNext();) {
				const auto match = it.next();
				add(match, aContextFile, 1, 2, match.hasCaptured(3));
			}
		}
		std::ranges::sort(found, { }, &agent_line_reference::Begin);
		return found;
	}

	namespace {
		/// \brief Where inline code spans and existing links are in one line
		/// of Markdown: a reference is never made a link inside either.
		struct markdown_spans
		{
			/// Code spans: from the first backtick to after the last.
			std::vector<std::pair<qsizetype, qsizetype>> Code;
			std::vector<std::pair<qsizetype, qsizetype>> Links;
		};

		[[nodiscard]] auto spans_of(const QString& aLine) -> markdown_spans
		{
			auto spans = markdown_spans{ };
			for (qsizetype at = 0; at < aLine.size();) {
				if (aLine[at] != QLatin1Char{ '`' }) {
					++at;
					continue;
				}
				auto run = qsizetype{ 0 };
				while (at + run < aLine.size() && aLine[at + run] == QLatin1Char{ '`' }) {
					++run;
				}
				// The closing run is the next of exactly the same length.
				auto close = at + run;
				auto shut  = qsizetype{ -1 };
				while (close < aLine.size()) {
					if (aLine[close] != QLatin1Char{ '`' }) {
						++close;
						continue;
					}
					auto length = qsizetype{ 0 };
					while (close + length < aLine.size() && aLine[close + length] == QLatin1Char{ '`' }) {
						++length;
					}
					if (length == run) {
						shut = close + length;
						break;
					}
					close += length;
				}
				if (shut < 0) {
					at += run;
					continue;
				}
				spans.Code.emplace_back(at, shut);
				at = shut;
			}
			static const auto links = QRegularExpression{ QStringLiteral(R"(\[[^\]]*\]\([^)]*\)|<[^>\s]+>|https?://\S+)") };
			for (auto it = links.globalMatch(aLine); it.hasNext();) {
				const auto match = it.next();
				spans.Links.emplace_back(match.capturedStart(), match.capturedEnd());
			}
			return spans;
		}

		[[nodiscard]] auto inside(const std::vector<std::pair<qsizetype, qsizetype>>& aSpans, const qsizetype aBegin, const qsizetype aEnd) -> bool
		{
			return std::ranges::any_of(aSpans, [&](const auto& aSpan) { return aBegin < aSpan.second && aSpan.first < aEnd; });
		}
	} // namespace

	auto agent_markdown_with_line_links(const QString& aMarkdown, const agent_file_resolver& aResolve, const QString& aContextFile) -> QString
	{
		if (!aResolve) {
			return aMarkdown;
		}
		auto lines = aMarkdown.split(QLatin1Char{ '\n' });
		auto fence = QString{ };
		// A fence: up to three spaces, then three or more backticks or tildes.
		const auto fenceOf = [](const QString& aLine) {
			auto indent = qsizetype{ 0 };
			while (indent < aLine.size() && aLine[indent] == QLatin1Char{ ' ' }) {
				++indent;
			}
			if (indent > 3 || indent == aLine.size() || (aLine[indent] != QLatin1Char{ '`' } && aLine[indent] != QLatin1Char{ '~' })) {
				return QString{ };
			}
			auto run = qsizetype{ 0 };
			while (indent + run < aLine.size() && aLine[indent + run] == aLine[indent]) {
				++run;
			}
			return run >= 3 ? aLine.mid(indent, run) : QString{ };
		};
		for (auto& line : lines) {
			const auto marker = fenceOf(line);
			if (!fence.isEmpty()) {
				if (!marker.isEmpty() && marker[0] == fence[0] && marker.size() >= fence.size() && line.trimmed() == marker) {
					fence.clear();
				}
				continue;
			}
			if (!marker.isEmpty()) {
				fence = marker;
				continue;
			}
			const auto spans = spans_of(line);
			auto       edits = std::vector<std::pair<std::pair<qsizetype, qsizetype>, QString>>{ };
			// A code span that is wholly one reference becomes a link around it.
			for (const auto& [begin, end] : spans.Code) {
				auto run = qsizetype{ 0 };
				while (begin + run < end && line[begin + run] == QLatin1Char{ '`' }) {
					++run;
				}
				const auto content = line.mid(begin + run, end - begin - 2 * run).trimmed();
				const auto refs    = agent_line_references(content, aResolve, aContextFile);
				if (refs.size() == 1U && refs.front().Begin == 0 && refs.front().Length == content.size() && !inside(spans.Links, begin, end)) {
					edits.push_back({ { begin, end }, QStringLiteral("[") + line.mid(begin, end - begin) + QStringLiteral("](") + agent_line_link(refs.front().Path, refs.front().First, refs.front().Before) + QLatin1Char{ ')' } });
				}
			}
			for (const auto& ref : agent_line_references(line, aResolve, aContextFile)) {
				if (inside(spans.Code, ref.Begin, ref.Begin + ref.Length) || inside(spans.Links, ref.Begin, ref.Begin + ref.Length)) {
					continue;
				}
				edits.push_back({ { ref.Begin, ref.Begin + ref.Length }, QStringLiteral("[") + line.mid(ref.Begin, ref.Length) + QStringLiteral("](") + agent_line_link(ref.Path, ref.First, ref.Before) + QLatin1Char{ ')' } });
			}
			std::ranges::sort(edits, std::ranges::greater{ }, [](const auto& aEdit) { return aEdit.first.first; });
			for (const auto& [range, text] : edits) {
				line.replace(range.first, range.second - range.first, text);
			}
		}
		return lines.join(QLatin1Char{ '\n' });
	}

	agent_tab_view::agent_tab_view(agent_launch_panel& aLaunch, side_chat_panel& aChat, finding_queue_view& aFindings, QWidget& aHistory, QWidget* aParent)
		: QWidget{ aParent }
		, Launch{ aLaunch }
		, Chat{ aChat }
		, Findings{ aFindings }
		, HistoryPanel{ aHistory }
		, LaunchAlive{ static_cast<QWidget*>(&aLaunch) }
		, ChatAlive{ static_cast<QWidget*>(&aChat) }
		, FindingsAlive{ static_cast<QWidget*>(&aFindings) }
	{
		setObjectName(QStringLiteral("agentTab"));
		setAccessibleName(tr("Agent"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(0);

		// The context row: the agent, the head it reads, its scope, History.
		auto* const context = new QWidget{ this };
		context->setObjectName(QStringLiteral("agentContextRow"));
		auto* const row = new QHBoxLayout{ context };
		row->setContentsMargins(10, 8, 8, 8);
		row->setSpacing(5);
		auto* const profiles = Launch.profile_selector();
		Borrowed.emplace_back(profiles, profiles->parentWidget());
		profiles->setParent(context);
		profiles->setToolTip(tr("The agent this tab asks"));
		profiles->setSizeAdjustPolicy(QComboBox::AdjustToContents);
		row->addWidget(profiles);
		// Owner amendment 2026-10-23: the agent is switched from the chip in
		// the question field; the selector stays the one choice, unseen.
		profiles->hide();
		// Owner look v003 and the VS Code agent drawers: the session's title,
		// then History and New session as icons.
		SessionTitle = new QLabel{ tr("Agent"), context };
		SessionTitle->setObjectName(QStringLiteral("agentSessionTitle"));
		SessionTitle->setTextFormat(Qt::PlainText);
		SessionTitle->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		auto titleFont = SessionTitle->font();
		titleFont.setWeight(QFont::DemiBold);
		SessionTitle->setFont(titleFont);
		row->addWidget(SessionTitle, 1);
		Head = chip_label(context, QStringLiteral("agentHeadChip"));
		Head->setToolTip(tr("The head the session reads"));
		Head->setForegroundRole(QPalette::PlaceholderText);
		row->addWidget(Head);
		History = make_drawer_tool(context, drawer_icon(drawer_glyph::history, palette()), tr("History: sessions, earlier heads and private runs"), QStringLiteral("agentHistory"));
		History->setAccessibleName(tr("History"));
		History->setPopupMode(QToolButton::InstantPopup);
		auto* const historyMenu = new QMenu{ History };
		HistoryMenu             = historyMenu;
		historyMenu->setObjectName(QStringLiteral("agentHistoryMenu"));
		auto* const clear = historyMenu->addAction(tr("Clear session"));
		clear->setObjectName(QStringLiteral("agentClearSessionAction"));
		connect(clear, &QAction::triggered, this, [this] { clear_session(); });
		connect(historyMenu, &QMenu::aboutToShow, this, [this, clear] { clear->setEnabled(!Turns.empty() && State != agent_tab_state::working); });
		historyMenu->addAction(Chat.new_session_action());
		historyMenu->addAction(Chat.continue_old_revision_action());
		historyMenu->addSeparator();
		auto* const privateRuns = historyMenu->addAction(tr("Private history and runs"));
		privateRuns->setObjectName(QStringLiteral("agentPrivateHistoryAction"));
		connect(privateRuns, &QAction::triggered, this, [this] { show_history(true); });
		auto* const sessions = historyMenu->addAction(tr("Sessions"));
		sessions->setObjectName(QStringLiteral("agentSessionsAction"));
		sessions->setEnabled(false);
		connect(historyMenu, &QMenu::aboutToShow, this, [this, sessions] { sessions->setText(Launch.session_catalog_text()); });
		History->setMenu(historyMenu);
		row->addWidget(History);
		// Owner decision (b) 2026-10-23: a new session clears the current
		// agent's thread.
		ClearSession = make_drawer_tool(context, drawer_icon(drawer_glyph::new_session, palette()), tr("New session: clear this agent's thread and start the next question afresh"),
										QStringLiteral("agentClearSession"));
		ClearSession->setAccessibleName(tr("Clear session"));
		connect(ClearSession, &QToolButton::clicked, this, [this] { clear_session(); });
		row->addWidget(ClearSession);
		column->addWidget(context);

		Pages = new QStackedWidget{ this };
		column->addWidget(Pages, 1);

		// Not set up: one sentence and one action.
		auto* const setup = new QAction{ tr("Set up an agent"), this };
		setup->setObjectName(QStringLiteral("agentSetUpAction"));
		connect(setup, &QAction::triggered, this, [this] {
			if (Setup) {
				Setup();
			}
		});
		Empty = new drawer_empty_state{ drawer_icon(drawer_glyph::agent, palette()), tr("Ask an agent about this pull request. Nothing it says is published unless you make it a comment."), setup, Pages };
		Empty->setObjectName(QStringLiteral("agentEmpty"));
		Pages->addWidget(Empty);

		// The conversation: the state's callout, the replies, the findings.
		Conversation     = new QWidget{ Pages };
		auto* const talk = new QVBoxLayout{ Conversation };
		talk->setContentsMargins(0, 0, 0, 0);
		talk->setSpacing(0);
		auto* const scroll = new QScrollArea{ Conversation };
		scroll->setObjectName(QStringLiteral("agentConversationScroll"));
		scroll->setWidgetResizable(true);
		scroll->setFrameShape(QFrame::NoFrame);
		scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		// The cards fit the drawer's width, not the other way round.
		Viewport = scroll->viewport();
		Viewport->installEventFilter(this);
		auto* const content    = new QWidget{ scroll };
		auto* const contentCol = new QVBoxLayout{ content };
		contentCol->setContentsMargins(12, 8, 12, 12);
		contentCol->setSpacing(10);
		Banner = new rounded{ rounded::look::callout, content };
		Banner->setObjectName(QStringLiteral("agentStateBanner"));
		auto* const bannerCol = new QVBoxLayout{ Banner };
		bannerCol->setContentsMargins(12, 10, 12, 10);
		bannerCol->setSpacing(4);
		BannerTitle = new QLabel{ Banner };
		BannerTitle->setObjectName(QStringLiteral("agentStateTitle"));
		auto bold = BannerTitle->font();
		bold.setWeight(QFont::DemiBold);
		BannerTitle->setFont(bold);
		BannerTitle->setWordWrap(true);
		bannerCol->addWidget(BannerTitle);
		BannerText = new QLabel{ Banner };
		BannerText->setObjectName(QStringLiteral("agentStateText"));
		BannerText->setWordWrap(true);
		BannerText->setForegroundRole(QPalette::PlaceholderText);
		bannerCol->addWidget(BannerText);
		auto* const bannerRow = new QHBoxLayout{ };
		bannerRow->setSpacing(6);
		BannerFirst = new QPushButton{ Banner };
		BannerFirst->setObjectName(QStringLiteral("agentStateFirst"));
		BannerSecond = new quiet_push_button{ Banner };
		BannerSecond->setObjectName(QStringLiteral("agentStateSecond"));
		BannerSecond->setFlat(true);
		bannerRow->addWidget(BannerFirst);
		bannerRow->addWidget(BannerSecond);
		bannerRow->addStretch(1);
		bannerCol->addLayout(bannerRow);
		Log = new QLabel{ Banner };
		Log->setObjectName(QStringLiteral("agentStateLog"));
		Log->setWordWrap(true);
		Log->setTextInteractionFlags(Qt::TextSelectableByMouse);
		Log->hide();
		bannerCol->addWidget(Log);
		contentCol->addWidget(Banner);
		// [[BPR-1753]]. The ready line once setup finishes, and what
		// the agent gets with each question.
		ReadyCard = new rounded{ rounded::look::ready, content };
		ReadyCard->setObjectName(QStringLiteral("agentReadyCard"));
		auto* const readyCol = new QVBoxLayout{ ReadyCard };
		readyCol->setContentsMargins(12, 10, 12, 10);
		readyCol->setSpacing(4);
		auto* const readyTop = new QHBoxLayout{ };
		ReadyTitle           = new QLabel{ ReadyCard };
		ReadyTitle->setObjectName(QStringLiteral("agentReadyTitle"));
		ReadyTitle->setFont(bold);
		ReadyTitle->setWordWrap(true);
		readyTop->addWidget(ReadyTitle, 1);
		auto* const dismiss = new QToolButton{ ReadyCard };
		dismiss->setObjectName(QStringLiteral("agentReadyDismiss"));
		dismiss->setAutoRaise(true);
		dismiss->setText(QString{ QChar{ 0x2715 } });
		dismiss->setAccessibleName(tr("Dismiss"));
		connect(dismiss, &QToolButton::clicked, ReadyCard, &QWidget::hide);
		readyTop->addWidget(dismiss, 0, Qt::AlignTop);
		readyCol->addLayout(readyTop);
		// Owner look v001: the ready line alone; the check's details and what
		// the agent gets wait, folded, under one disclosure.
		auto* const gets = new disclosure_button{ ReadyCard };
		gets->setObjectName(QStringLiteral("agentReadyGetsToggle"));
		gets->setText(tr("Details and what the agent gets"));
		gets->setCheckable(true);
		gets->setChecked(false);
		readyCol->addWidget(gets, 0, Qt::AlignLeft);
		ReadyText = new QLabel{ ReadyCard };
		ReadyText->setObjectName(QStringLiteral("agentReadyText"));
		ReadyText->setWordWrap(true);
		ReadyText->setTextFormat(Qt::PlainText);
		ReadyText->hide();
		readyCol->addWidget(ReadyText);
		ReadyGets = new QLabel{ ReadyCard };
		ReadyGets->setObjectName(QStringLiteral("agentReadyGets"));
		ReadyGets->setWordWrap(true);
		ReadyGets->setTextFormat(Qt::PlainText);
		ReadyGets->setTextInteractionFlags(Qt::TextSelectableByMouse);
		ReadyGets->hide();
		readyCol->addWidget(ReadyGets);
		connect(gets, &QToolButton::toggled, this, [this](const bool aOpen) {
			ReadyText->setVisible(aOpen);
			ReadyGets->setVisible(aOpen);
		});
		ReadyCard->hide();
		contentCol->addWidget(ReadyCard);
		// Owner look v003: with nothing asked, an icon and a line, as the
		// agent drawers show it; the suggested prompts wait above the field.
		EmptyIntro              = new QWidget{ content };
		auto* const introColumn = new QVBoxLayout{ EmptyIntro };
		introColumn->setContentsMargins(0, 36, 0, 12);
		introColumn->setSpacing(6);
		auto* const introIcon = new QLabel{ EmptyIntro };
		introIcon->setObjectName(QStringLiteral("agentEmptyIcon"));
		introIcon->setPixmap(drawer_icon(drawer_glyph::agent, palette()).pixmap(32, 32));
		introColumn->addWidget(introIcon, 0, Qt::AlignHCenter);
		auto* const introTitle = new QLabel{ tr("Ask about this pull request"), EmptyIntro };
		introTitle->setObjectName(QStringLiteral("agentEmptyTitle"));
		introTitle->setFont(bold);
		introColumn->addWidget(introTitle, 0, Qt::AlignHCenter);
		auto* const introText = new QLabel{ tr("The agent reads the pull request at its head. Nothing it says is published unless you make it a comment."), EmptyIntro };
		introText->setWordWrap(true);
		introText->setAlignment(Qt::AlignHCenter);
		introText->setForegroundRole(QPalette::PlaceholderText);
		introColumn->addWidget(introText);
		EmptyIntro->hide();
		contentCol->addWidget(EmptyIntro);
		// The thread: each question and its answer, oldest first.
		Thread = new QWidget{ content };
		Thread->setObjectName(QStringLiteral("agentThread"));
		ThreadColumn = new QVBoxLayout{ Thread };
		ThreadColumn->setContentsMargins(0, 0, 0, 0);
		ThreadColumn->setSpacing(10);
		Thread->hide();
		contentCol->addWidget(Thread);
		auto* const replies = Chat.replies_list();
		Borrowed.emplace_back(replies, replies->parentWidget());
		replies->setParent(content);
		replies->setFrameShape(QFrame::NoFrame);
		replies->setWordWrap(true);
		replies->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
		replies->viewport()->setAutoFillBackground(false);
		contentCol->addWidget(replies);
		replies->hide();
		Cards      = new QWidget{ content };
		CardColumn = new QVBoxLayout{ Cards };
		CardColumn->setContentsMargins(0, 0, 0, 0);
		CardColumn->setSpacing(8);
		contentCol->addWidget(Cards);
		// Owner decision (a) 2026-10-23: suggested prompts, each sent as
		// written. Owner look v005 ("I want the suggestions to draw more like
		// they're in the chat window rather than attached to the bottom pane.
		// That way they show up after the last chat message not overtop of
		// it"): they are the thread's last item and scroll with it.
		Suggestions = new QWidget{ content };
		Suggestions->setObjectName(QStringLiteral("agentSuggestions"));
		SuggestionColumn = new QVBoxLayout{ Suggestions };
		SuggestionColumn->setContentsMargins(0, 2, 0, 0);
		SuggestionColumn->setSpacing(6);
		auto* const suggestLine = new QLabel{ tr("Ask about this pull request, or try:"), Suggestions };
		SuggestionsLine         = suggestLine;
		suggestLine->setObjectName(QStringLiteral("agentSuggestionsLine"));
		suggestLine->setForegroundRole(QPalette::PlaceholderText);
		suggestLine->setWordWrap(true);
		SuggestionColumn->addWidget(suggestLine);
		contentCol->addWidget(Suggestions);
		contentCol->addStretch(1);
		scroll->setWidget(content);
		talk->addWidget(scroll, 1);

		// The question field: the words, its context chips, Context and Ask.
		auto* const ask = new QWidget{ Conversation };
		ask->setObjectName(QStringLiteral("agentQuestion"));
		auto* const askCol = new QVBoxLayout{ ask };
		askCol->setContentsMargins(10, 8, 10, 10);
		askCol->setSpacing(6);
		// Owner look v003 ("This bottom menu area is a big mess") and the
		// Claude Code and Codex composers: one rounded field. Any added
		// context, the words, a thin rule, and one row: Context and
		// Suggestions as icons and what the agent may read on the left; the
		// agent · model · effort pill and Send on the right.
		auto* const field    = new rounded{ rounded::look::field, ask };
		Field                = field;
		auto* const fieldCol = new QVBoxLayout{ field };
		fieldCol->setContentsMargins(8, 6, 6, 6);
		fieldCol->setSpacing(4);
		field->installEventFilter(this);
		Chips = chip_label(field, QStringLiteral("agentContextChips"));
		Chips->setForegroundRole(QPalette::PlaceholderText);
		Chips->setWordWrap(true);
		Chips->hide();
		fieldCol->addWidget(Chips);
		// [[BPR-1773]]. The conversation the next questions are about: what
		// it is, and a close button. It stays until it is closed, so a
		// follow-up is about the same conversation.
		ConversationChip = new rounded{ rounded::look::card, field };
		ConversationChip->setObjectName(QStringLiteral("agentConversationChip"));
		auto* const chipRow = new QHBoxLayout{ ConversationChip };
		chipRow->setContentsMargins(8, 3, 2, 3);
		chipRow->setSpacing(4);
		ConversationText = new QLabel{ ConversationChip };
		ConversationText->setObjectName(QStringLiteral("agentConversationChipText"));
		ConversationText->setTextFormat(Qt::PlainText);
		ConversationText->setWordWrap(true);
		chipRow->addWidget(ConversationText, 1);
		ConversationClose = make_drawer_tool(ConversationChip, drawer_icon(drawer_glyph::close, palette()), tr("Stop asking about this conversation"), QStringLiteral("agentConversationChipClose"));
		connect(ConversationClose, &QToolButton::clicked, this, [this] {
			if (ConversationDismiss) {
				ConversationDismiss();
			}
		});
		chipRow->addWidget(ConversationClose, 0, Qt::AlignTop);
		ConversationChip->hide();
		fieldCol->addWidget(ConversationChip);
		auto* const composer = Chat.composer();
		Borrowed.emplace_back(composer, composer->parentWidget());
		composer->setParent(field);
		composer->setFrameShape(QFrame::NoFrame);
		composer->setPlaceholderText(tr("Ask about this pull request…"));
		composer->setMaximumHeight(90);
		composer->viewport()->setAutoFillBackground(false);
		fieldCol->addWidget(composer);
		Notice = new QLabel{ field };
		Notice->setObjectName(QStringLiteral("agentComposerNotice"));
		Notice->setWordWrap(true);
		Notice->setForegroundRole(QPalette::PlaceholderText);
		Notice->hide();
		fieldCol->addWidget(Notice);
		auto* const rule = new rule_line{ field };
		rule->setObjectName(QStringLiteral("agentComposerRule"));
		fieldCol->addWidget(rule);
		auto* const askRow = new QHBoxLayout{ };
		askRow->setSpacing(2);
		Context = make_drawer_tool(field, drawer_icon(drawer_glyph::plus, palette()), Chat.add_context_action()->toolTip().isEmpty() ? tr("Add the lines selected in the code") : Chat.add_context_action()->toolTip(),
								   QStringLiteral("agentAddContext"));
		connect(Context, &QToolButton::clicked, Chat.add_context_action(), &QAction::trigger);
		askRow->addWidget(Context);
		// Owner look v004 ("I would like the suggestions to come back where
		// they were in the chat window... maybe turn the suggestion prompts
		// into a toggle to show/hide them instead of a drop down"): the
		// prompts stay above the field, and this shows or hides them; the
		// host keeps the choice.
		SuggestionsMenu = make_drawer_tool(field, drawer_icon(drawer_glyph::agent, palette()), tr("Hide the suggested prompts"), QStringLiteral("agentSuggestionsToggle"));
		SuggestionsMenu->setCheckable(true);
		SuggestionsMenu->setChecked(true);
		SuggestionsMenu->setAccessibleName(tr("Suggested prompts"));
		connect(SuggestionsMenu, &QToolButton::toggled, this, [this](const bool aShown) {
			SuggestionsMenu->setToolTip(aShown ? tr("Hide the suggested prompts") : tr("Show the suggested prompts"));
			refresh_suggestions();
			if (SuggestionsObserver) {
				SuggestionsObserver(aShown);
			}
		});
		askRow->addWidget(SuggestionsMenu);
		auto* const scope = Chat.scope_selector();
		Borrowed.emplace_back(scope, scope->parentWidget());
		scope->setParent(field);
		scope->setToolTip(tr("What the agent may read"));
		scope->setSizeAdjustPolicy(QComboBox::AdjustToContents);
		scope->setFrame(false);
		askRow->addWidget(scope);
		askRow->addStretch(1);
		// Owner decision (c) and amendment 2026-10-23: the agent, model and
		// effort as a pill in the field; an icon alone when the field is narrow.
		RunSettings = new menu_word{ field };
		RunSettings->setObjectName(QStringLiteral("agentRunSettings"));
		RunSettings->setAccessibleName(tr("Agent, model and effort"));
		RunSettings->setMenu(new QMenu{ RunSettings });
		RunSettings->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
		static_cast<menu_word*>(RunSettings)->Pill = true;
		connect(RunSettings->menu(), &QMenu::aboutToShow, this, [this] {
			// Owner look v006: the harness is asked what it offers when the
			// reviewer opens its menu, once per window.
			if (const auto* const settings = current_run_settings(); settings != nullptr && CatalogDispatch) {
				CatalogDispatch(settings->Profile);
			}
			present_run_settings_button();
		});
		connect(RunSettings->menu(), &QMenu::aboutToHide, this, [this] {
			if (MenuStale) {
				QTimer::singleShot(0, this, [this] { present_run_settings_button(); });
			}
		});
		askRow->addWidget(RunSettings);
		Ask = new QPushButton{ field };
		Ask->setObjectName(QStringLiteral("agentAsk"));
		Ask->setDefault(true);
		Ask->setIcon(drawer_icon(drawer_glyph::send, palette()));
		Ask->setAccessibleName(tr("Send"));
		Ask->setToolTip(tr("Send the question to the agent (%1)").arg(QKeySequence{ Qt::CTRL | Qt::Key_Return }.toString(QKeySequence::NativeText)));
		// Send, or Stop while the agent works: one place, always in reach.
		connect(Ask, &QPushButton::clicked, this, [this] {
			if (State == agent_tab_state::working) {
				Launch.cancel_action()->trigger();
				return;
			}
			this->ask();
		});
		// Return with Command or Control asks from the field, as Send does,
		// so a slash command is read the same way from either.
		composer->installEventFilter(this);
		// What was asked is read when it is sent, by Send or by the keyboard.
		connect(Chat.send_action(), &QAction::triggered, this, [this] {
			PendingQuestion = Chat.composer()->toPlainText().trimmed();
			PendingReview   = false;
			PendingAbout    = ConversationShown;
			// [[BPR-1773]]. Where its answer's Draft comment would go is
			// decided now, while what it was asked on is on screen.
			PendingDraftTarget = DraftTargetSource ? std::optional{ DraftTargetSource() } : std::nullopt;
		});
		askRow->addWidget(Ask);
		fieldCol->addLayout(askRow);
		askCol->addWidget(field);
		talk->addWidget(ask);
		Pages->addWidget(Conversation);

		// History: the private history, with the way back.
		HistoryPage         = new QWidget{ Pages };
		auto* const histCol = new QVBoxLayout{ HistoryPage };
		histCol->setContentsMargins(0, 0, 0, 0);
		auto* const back = new quiet_push_button{ tr("Back to the conversation"), HistoryPage };
		back->setObjectName(QStringLiteral("agentHistoryBack"));
		back->setFlat(true);
		connect(back, &QPushButton::clicked, this, [this] { show_history(false); });
		histCol->addWidget(back, 0, Qt::AlignLeft);
		Borrowed.emplace_back(&HistoryPanel, HistoryPanel.parentWidget());
		HistoryPanel.setParent(HistoryPage);
		histCol->addWidget(&HistoryPanel, 1);
		HistoryPanel.show();
		Pages->addWidget(HistoryPage);

		// What the panels say moves this tab: their lists and actions.
		const auto again = [this] { refresh(); };
		connect(Findings.findings_list()->model(), &QAbstractItemModel::modelReset, this, [this] { ++FindingsPresented; });
		connect(Findings.findings_list()->model(), &QAbstractItemModel::rowsInserted, this, [this] { ++FindingsPresented; });
		connect(Findings.findings_list()->model(), &QAbstractItemModel::modelReset, this, again);
		connect(Findings.findings_list()->model(), &QAbstractItemModel::rowsInserted, this, again);
		connect(Findings.findings_list()->model(), &QAbstractItemModel::rowsRemoved, this, again);
		connect(Chat.replies_list()->model(), &QAbstractItemModel::rowsInserted, this, again);
		connect(Chat.replies_list()->model(), &QAbstractItemModel::modelReset, this, again);
		for (auto* const action : { Launch.start_action(), Launch.cancel_action(), Chat.continue_old_revision_action(), Chat.new_session_action(), Chat.send_action() }) {
			connect(action, &QAction::changed, this, again);
		}
		connect(profiles, &QComboBox::currentIndexChanged, this, again);
		// [[BPR-1753]], owner look v001 ("no indication anything
		// happened"). The working line follows the request as it moves, with
		// the time it has taken, so a long run never looks like nothing.
		Launch.add_progress_observer([this] { progress(); });
		// The turns' times read as near words; a minute refreshes them.
		auto* const minutes = new QTimer{ this };
		minutes->setInterval(60'000);
		connect(minutes, &QTimer::timeout, this, [this] { refresh_times(); });
		minutes->start();
		Ticker = new QTimer{ this };
		Ticker->setInterval(1000);
		connect(Ticker, &QTimer::timeout, this, [this] {
			present_banner();
			update_turn();
		});
		refresh();
	}

	agent_tab_view::~agent_tab_view()
	{
		Dying = true;
		// The panels hold these controls by pointer and outlive this tab; the
		// tab never deletes what it only shows.
		for (auto& [widget, owner] : Borrowed) {
			if (widget != nullptr && owner != nullptr) {
				widget->hide();
				widget->setParent(owner);
			}
		}
	}

	void agent_tab_view::set_setup_dispatch(std::function<void()> aDispatch) { Setup = std::move(aDispatch); }

	void agent_tab_view::focus_question()
	{
		if (!panels_alive()) {
			return;
		}
		show_history(false);
		if (Chat.composer()->isVisible()) {
			Chat.composer()->setFocus(Qt::OtherFocusReason);
		}
	}

	void agent_tab_view::show_history(const bool aShown)
	{
		if (aShown) {
			Pages->setCurrentWidget(HistoryPage);
			return;
		}
		Pages->setCurrentWidget(State == agent_tab_state::setting_up       ? static_cast<QWidget*>(SetupView)
									: State == agent_tab_state::not_set_up ? static_cast<QWidget*>(Empty)
																		   : Conversation);
	}

	void agent_tab_view::adopt_setup(agent_setup_view& aSetup, std::function<void()> aCheckAgain)
	{
		SetupView  = &aSetup;
		CheckAgain = std::move(aCheckAgain);
		Pages->addWidget(SetupView);
		SetupView->set_change_observer([this] { refresh(); });
		HistoryMenu->addSeparator();
		// Owner look v006 ("I dont think we should have this option to go back
		// to the wizard, actually we can keep it if it's a "reset agents"
		// which clears all settings and goes back to the wizard fresh").
		auto* const reset = HistoryMenu->addAction(tr("Reset agents…"));
		reset->setObjectName(QStringLiteral("agentResetAgentsAction"));
		connect(reset, &QAction::triggered, this, [this] { reset_agents(); });
		refresh();
	}

	void agent_tab_view::present_ready(const QString& aTitle, const QString& aText, const QString& aGets)
	{
		ReadyTitle->setText(aTitle);
		ReadyText->setText(aText);
		ReadyGets->setText(aGets);
		ReadyCard->setVisible(!aTitle.isEmpty());
	}

	void agent_tab_view::refresh()
	{
		if (!panels_alive()) {
			return;
		}
		const auto setUp      = Launch.profile_selector()->count() > 0;
		const auto running    = Launch.cancel_action()->isEnabled();
		const auto status     = Launch.status_text();
		const auto failed     = status.startsWith(QStringLiteral("Finished:")) && !status.contains(QStringLiteral("success"));
		const auto moved      = !Chat.offered_continuations().empty();
		const auto setting    = SetupView != nullptr && SetupView->needed();
		const auto wasWorking = State == agent_tab_state::working;
		State                 = setting ? agent_tab_state::setting_up
			: !setUp                    ? agent_tab_state::not_set_up
			: running                   ? agent_tab_state::working
			: moved                     ? agent_tab_state::head_moved
			: failed                    ? agent_tab_state::stopped
										: agent_tab_state::idle;
		if (State == agent_tab_state::working && !wasWorking) {
			Started.start();
			Ticker->start();
			ReadyCard->hide();
		} else if (State != agent_tab_state::working && wasWorking) {
			Ticker->stop();
			// A request that just ended says so, whatever it found.
			Finished = Launch.last_outcome() == application::agent_run_outcome::success;
		}
		if (State == agent_tab_state::working) {
			Finished = false;
		}
		if (Pages->currentWidget() != HistoryPage || setting) {
			Pages->setCurrentWidget(setting ? static_cast<QWidget*>(SetupView) : State == agent_tab_state::not_set_up ? static_cast<QWidget*>(Empty)
																													  : Conversation);
		}
		// The head the session reads, short; its state in the tooltip.
		auto head = Chat.head_oid_text().trimmed();
		if (head.contains(QLatin1Char{ ':' })) {
			head = head.section(QLatin1Char{ ':' }, -1).trimmed();
		}
		Head->setText(head.left(7));
		Head->setVisible(!head.isEmpty() && head != QStringLiteral("—"));
		Head->setToolTip(tr("The head the session reads. %1").arg(Chat.session_state_text()));
		// The added context, above the words, only when there is some.
		Chips->setText(Chat.chips_text());
		Chips->setVisible(Chat.chip_count() > 0U);
		// Send, or Stop while the agent works.
		if (State == agent_tab_state::working) {
			Ask->setIcon({ });
			Ask->setText(tr("Stop"));
			Ask->setAccessibleName(tr("Stop"));
			Ask->setToolTip(tr("Stop the agent"));
			Ask->setEnabled(Launch.cancel_action()->isEnabled());
		} else {
			Ask->setText({ });
			Ask->setIcon(drawer_icon(drawer_glyph::send, palette()));
			Ask->setAccessibleName(tr("Send"));
			Ask->setToolTip(tr("Send the question to the agent (%1)").arg(QKeySequence{ Qt::CTRL | Qt::Key_Return }.toString(QKeySequence::NativeText)));
			Ask->setEnabled(Chat.send_action()->isEnabled());
		}
		present_banner();
		rebuild_findings();
		Thread->setVisible(!Turns.empty());
		EmptyIntro->setVisible(Turns.empty() && (State == agent_tab_state::idle || State == agent_tab_state::stopped) && !ReadyCard->isVisible());
		ClearSession->setVisible(!Turns.empty());
		ClearSession->setEnabled(State != agent_tab_state::working);
		// Owner look v006: the agent shown is asked what it offers, so its
		// models are there when its menu opens.
		if (isVisible() && CatalogDispatch && State != agent_tab_state::setting_up && State != agent_tab_state::not_set_up) {
			if (const auto profile = Launch.selected_profile()) {
				CatalogDispatch(profile->value());
			}
		}
		refresh_suggestions();
		update_turn();
	}

	void agent_tab_view::progress()
	{
		if (!panels_alive()) {
			return;
		}
		const auto phase = static_cast<int>(Launch.current_phase());
		if (phase != LastPhase) {
			if (LastPhase == static_cast<int>(agent_launch_panel::phase::idle) && phase == static_cast<int>(agent_launch_panel::phase::preparing)) {
				begin_turn();
			}
			LastPhase = phase;
			refresh();
			return;
		}
		present_banner();
		update_turn();
	}

	void agent_tab_view::begin_turn()
	{
		auto next = turn{ };
		if (PendingReview || !PendingQuestion) {
			next.Review   = true;
			next.Question = tr("Review this pull request");
		} else {
			next.Question    = *PendingQuestion;
			next.About       = PendingAbout;
			next.DraftTarget = PendingDraftTarget;
			// Sent: the field is ready for the next question.
			Chat.composer()->clear();
		}
		PendingQuestion.reset();
		PendingAbout.reset();
		PendingDraftTarget.reset();
		PendingReview  = false;
		FindingsAtTurn = FindingsPresented;
		next.AskedAt   = QDateTime::currentDateTime();
		add_turn_widgets(next);
		Turns.push_back(std::move(next));
		Thread->show();
	}

	void agent_tab_view::add_turn_widgets(turn& aTurn)
	{
		aTurn.Serial    = NextSerial++;
		auto* const box = new QWidget{ Thread };
		box->setObjectName(QStringLiteral("agentTurn"));
		auto* const column = new QVBoxLayout{ box };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(4);
		// Owner look v003 and the agent drawers: the question in an outlined
		// bubble, its time right-aligned above it.
		auto* const askedTime = new QLabel{ box };
		askedTime->setObjectName(QStringLiteral("agentTurnAskedTime"));
		askedTime->setForegroundRole(QPalette::PlaceholderText);
		askedTime->setAlignment(Qt::AlignRight);
		column->addWidget(askedTime);
		show_time(*askedTime, aTurn.AskedAt);
		auto* const asked = new rounded{ rounded::look::field, box };
		asked->setObjectName(QStringLiteral("agentTurnQuestion"));
		auto* const askedColumn = new QVBoxLayout{ asked };
		askedColumn->setContentsMargins(10, 6, 10, 6);
		// [[BPR-1773]], owner look 2026-10-27 ("I don't even understand what
		// the question I asked was"). A question about a conversation names
		// it first, in the question's own colour and a heavier weight, so the
		// bubble reads "About <conversation>: <question>".
		if (aTurn.About) {
			auto* const about = new QLabel{ tr("About %1").arg(aTurn.About->Text), asked };
			about->setObjectName(QStringLiteral("agentTurnAbout"));
			about->setTextFormat(Qt::PlainText);
			about->setWordWrap(true);
			auto strong = about->font();
			strong.setWeight(QFont::DemiBold);
			about->setFont(strong);
			about->setToolTip(aTurn.About->Detail);
			askedColumn->addWidget(about);
		}
		asked->setAccessibleName(aTurn.About ? tr("About %1: %2").arg(aTurn.About->Text, aTurn.Question) : aTurn.Question);
		auto* const question = new QLabel{ asked };
		question->setObjectName(QStringLiteral("agentTurnQuestionText"));
		question->setWordWrap(true);
		question->setOpenExternalLinks(false);
		// Owner look v004: a file and line the question names is a link to it.
		connect(question, &QLabel::linkActivated, this, [this](const QString& aLink) { follow_link(aLink); });
		aTurn.QuestionLabel = question;
		show_question(aTurn);
		if (aTurn.Review) {
			auto italic = question->font();
			italic.setItalic(true);
			question->setFont(italic);
		}
		askedColumn->addWidget(question);
		column->addWidget(asked);
		// Owner look v003 ("the response contained in something we could
		// copy"): the reply in its own card, its footer as the agent apps
		// have it: Copy and Save as note, then who answered and when.
		auto* const reply = new rounded{ rounded::look::card, box };
		reply->setObjectName(QStringLiteral("agentTurnReply"));
		auto* const replyColumn = new QVBoxLayout{ reply };
		replyColumn->setContentsMargins(reply_left_margin, 8, reply_right_margin, 6);
		replyColumn->setSpacing(6);
		// Owner decision (a): the answer reads as the agent wrote it, its
		// Markdown rendered in place, its code coloured as comments' are;
		// links and images do nothing.
		auto* const answer = new markdown_body_view{ reply };
		answer->setObjectName(QStringLiteral("agentTurnAnswer"));
		answer->set_in_place(true);
		answer->set_height_observer([this, answer] { fit_answer(*answer); });
		// A reference the answer makes a link opens its file at its line;
		// any other link does nothing.
		answer->set_link_dispatch([this](const std::string& aLink) { follow_link(QString::fromStdString(aLink)); });
		dress_answer(*answer);
		answer->hide();
		replyColumn->addWidget(answer);
		// Owner decision 2026-10-23: the turn's findings, as cards, under its answer.
		auto* const cards = new QVBoxLayout{ };
		cards->setSpacing(8);
		replyColumn->addLayout(cards);
		// While the agent works, and how it ended: one quiet line.
		auto* const footer = new QLabel{ reply };
		footer->setObjectName(QStringLiteral("agentTurnFooter"));
		footer->setForegroundRole(QPalette::PlaceholderText);
		footer->setWordWrap(true);
		replyColumn->addWidget(footer);
		auto* const footerRow = new QHBoxLayout{ };
		footerRow->setSpacing(2);
		auto* const copy = make_drawer_tool(reply, drawer_icon(drawer_glyph::copy, palette()), tr("Copy the response as Markdown"), QStringLiteral("agentTurnCopy"));
		copy->hide();
		footerRow->addWidget(copy);
		// Save as note: the whole reply, or the part selected in it.
		auto* const save = new quiet_push_button{ tr("Save as note"), reply };
		save->setObjectName(QStringLiteral("agentTurnSaveNote"));
		save->setFlat(true);
		save->setToolTip(tr("Keep this reply, or the part of it you selected, as a private note. Nothing is published."));
		save->hide();
		footerRow->addWidget(save);
		// [[BPR-1773]], owner look 2026-10-27 ("let you turn anything into a
		// note or draft comment directly"). Draft comment, beside Save as
		// note and in its style: the whole reply, or the part selected, as a
		// private draft where the host decided when it was asked (the
		// conversation's reply, the lines asked on, or the pull request).
		// Nothing is published. Once drafted it goes to the draft.
		auto* const draft = new quiet_push_button{ tr("Draft comment"), reply };
		draft->setObjectName(QStringLiteral("agentTurnDraftComment"));
		draft->setFlat(true);
		draft->setToolTip(aTurn.DraftTarget && !aTurn.DraftTarget->Tip.isEmpty() ? aTurn.DraftTarget->Tip
																				 : tr("Draft this reply, or the part of it you selected, as a comment on the pull request. Nothing is posted until you post it."));
		draft->hide();
		footerRow->addWidget(draft);
		footerRow->addStretch(1);
		auto* const answeredTime = new QLabel{ reply };
		answeredTime->setObjectName(QStringLiteral("agentTurnAnsweredTime"));
		answeredTime->setForegroundRole(QPalette::PlaceholderText);
		answeredTime->hide();
		footerRow->addWidget(answeredTime);
		replyColumn->addLayout(footerRow);
		column->addWidget(reply);
		ThreadColumn->addWidget(box);
		aTurn.Widget       = box;
		aTurn.Answer       = answer;
		aTurn.Footer       = footer;
		aTurn.SaveNote     = save;
		aTurn.CardColumn   = cards;
		aTurn.Copy         = copy;
		aTurn.AskedTime    = askedTime;
		aTurn.AnsweredTime = answeredTime;
		aTurn.Draft        = draft;
		aTurn.Asked        = asked;
		connect(draft, &QPushButton::clicked, this, [this, draft] {
			for (auto& kept : Turns) {
				if (kept.Draft != draft) {
					continue;
				}
				const auto key = kept.DraftTarget ? kept.DraftTarget->Key : std::string{ };
				// As Go to note does: once drafted, the button opens the draft.
				if (kept.Drafted) {
					if (DraftNavigate) {
						DraftNavigate(key);
					}
					return;
				}
				if (!DraftDispatch) {
					return;
				}
				auto       text     = kept.AnswerText;
				const auto selected = kept.Answer != nullptr ? kept.Answer->rendered_view()->textCursor().selectedText() : QString{ };
				if (!selected.trimmed().isEmpty()) {
					text = selected;
					text.replace(QChar::ParagraphSeparator, QLatin1Char{ '\n' });
				}
				draft->setEnabled(false);
				DraftDispatch(key, text, [this, guarded = QPointer{ draft }](const bool aDrafted, const QString& aWhy) {
					if (guarded == nullptr) {
						return;
					}
					if (!aDrafted) {
						guarded->setText(tr("Not drafted: %1").arg(aWhy));
						guarded->setEnabled(true);
						return;
					}
					for (auto& turn : Turns) {
						if (turn.Draft == guarded) {
							turn.Drafted = true;
						}
					}
					const auto navigable = static_cast<bool>(DraftNavigate);
					guarded->setText(navigable ? tr("Go to draft") : tr("Drafted"));
					guarded->setToolTip(navigable ? tr("Open the draft this reply was written into") : QString{ });
					guarded->setEnabled(navigable);
				});
				return;
			}
		});
		connect(copy, &QToolButton::clicked, this, [this, copy] {
			for (const auto& kept : Turns) {
				if (kept.Copy == copy) {
					QGuiApplication::clipboard()->setText(kept.AnswerText);
					copy->setToolTip(tr("Copied"));
				}
			}
		});
		connect(save, &QPushButton::clicked, this, [this, save] {
			for (auto& kept : Turns) {
				if (kept.SaveNote != save) {
					continue;
				}
				// Owner look v004: once saved, the button goes to the note.
				if (kept.Note != 0U) {
					if (NoteNavigate) {
						NoteNavigate(kept.Note);
					}
					return;
				}
				if (!NoteDispatch) {
					return;
				}
				auto       text     = kept.AnswerText;
				const auto selected = kept.Answer != nullptr ? kept.Answer->rendered_view()->textCursor().selectedText() : QString{ };
				if (!selected.trimmed().isEmpty()) {
					text = selected;
					text.replace(QChar::ParagraphSeparator, QLatin1Char{ '\n' });
				}
				save->setEnabled(false);
				NoteDispatch(text, [this, guarded = QPointer{ save }](const bool aSaved, const QString& aWhy, const std::uint64_t aNote) {
					if (guarded == nullptr) {
						return;
					}
					if (!aSaved) {
						guarded->setText(tr("Not saved: %1").arg(aWhy));
						guarded->setEnabled(true);
						return;
					}
					for (auto& turn : Turns) {
						if (turn.SaveNote == guarded) {
							turn.Note = aNote;
						}
					}
					const auto navigable = aNote != 0U && NoteNavigate;
					guarded->setText(navigable ? tr("Go to note") : tr("Saved as a note"));
					guarded->setToolTip(navigable ? tr("Open the private note this reply was saved as") : QString{ });
					guarded->setEnabled(navigable);
				});
				return;
			}
		});
	}

	void agent_tab_view::set_answer(turn& aTurn, const QString& aText)
	{
		if (aTurn.Answer == nullptr || (aText == aTurn.AnswerText && aTurn.Answer->isHidden() == aText.isEmpty())) {
			return;
		}
		aTurn.AnswerText = aText;
		// The findings block is for the cards; the reply shows without it.
		if (const auto fence = aTurn.AnswerText.lastIndexOf(QStringLiteral("```bpr-findings")); fence >= 0) {
			const auto closed = aTurn.AnswerText.indexOf(QStringLiteral("```"), fence + 3);
			aTurn.AnswerText.remove(fence, closed < 0 ? aTurn.AnswerText.size() - fence : closed + 3 - fence);
			aTurn.AnswerText = aTurn.AnswerText.trimmed();
		}
		show_answer(aTurn);
	}

	void agent_tab_view::show_answer(turn& aTurn)
	{
		if (aTurn.Answer == nullptr) {
			return;
		}
		// Owner look v004: the files and lines the answer names are links;
		// Copy and Save as note keep the answer as the agent wrote it.
		const auto shown = LineNavigate ? agent_markdown_with_line_links(aTurn.AnswerText, LineResolve, aTurn.ContextFile) : aTurn.AnswerText;
		aTurn.Answer->set_body(shown.toStdString(), "agent-turn-" + std::to_string(aTurn.Serial));
		aTurn.Answer->setVisible(!aTurn.AnswerText.isEmpty());
		fit_answer(*aTurn.Answer);
	}

	void agent_tab_view::show_question(turn& aTurn)
	{
		if (aTurn.QuestionLabel == nullptr) {
			return;
		}
		const auto refs = LineNavigate ? agent_line_references(aTurn.Question, LineResolve) : std::vector<agent_line_reference>{ };
		// The one file the question names is where its answer's bare lines are.
		aTurn.ContextFile.clear();
		for (const auto& ref : refs) {
			if (aTurn.ContextFile.isEmpty()) {
				aTurn.ContextFile = ref.Path;
			} else if (aTurn.ContextFile != ref.Path) {
				aTurn.ContextFile.clear();
				break;
			}
		}
		auto html = QString{ };
		auto at   = qsizetype{ 0 };
		for (const auto& ref : refs) {
			html += aTurn.Question.mid(at, ref.Begin - at).toHtmlEscaped();
			html += QStringLiteral("<a href=\"%1\">%2</a>").arg(agent_line_link(ref.Path, ref.First, ref.Before).toHtmlEscaped(), aTurn.Question.mid(ref.Begin, ref.Length).toHtmlEscaped());
			at = ref.Begin + ref.Length;
		}
		html += aTurn.Question.mid(at).toHtmlEscaped();
		html.replace(QLatin1Char{ '\n' }, QStringLiteral("<br>"));
		aTurn.QuestionLabel->setTextFormat(refs.empty() ? Qt::PlainText : Qt::RichText);
		aTurn.QuestionLabel->setText(refs.empty() ? aTurn.Question : html);
		aTurn.QuestionLabel->setTextInteractionFlags(refs.empty() ? Qt::TextSelectableByMouse : Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
		aTurn.QuestionLabel->setAccessibleName(aTurn.Question);
		aTurn.QuestionLabel->setToolTip(refs.empty() ? QString{ } : tr("Click a file and line to open it in Review"));
	}

	void agent_tab_view::follow_link(const QString& aLink)
	{
		const auto target = agent_line_link_target(aLink);
		if (!target || !LineNavigate || !LineResolve) {
			return;
		}
		const auto path = LineResolve(target->first);
		if (path) {
			LineNavigate(*path, target->second.first, target->second.second);
		}
	}

	void agent_tab_view::set_note_navigation(agent_note_navigation aNavigate) { NoteNavigate = std::move(aNavigate); }

	void agent_tab_view::set_catalog_dispatch(std::function<void(std::uint64_t)> aDispatch) { CatalogDispatch = std::move(aDispatch); }

	void agent_tab_view::set_reset_dispatch(std::function<void()> aReset, std::function<bool()> aConfirm)
	{
		ResetDispatch = std::move(aReset);
		ResetConfirm  = std::move(aConfirm);
	}

	void agent_tab_view::reset_agents()
	{
		if (!panels_alive() || State == agent_tab_state::working || !ResetDispatch) {
			return;
		}
		const auto confirmed = ResetConfirm ? ResetConfirm()
											: QMessageBox::warning(this, tr("Reset agents?"),
																   tr("This removes every agent you added and its model, effort and check, the clone folders and the clones linked to repositories, "
																	  "and your edited review skill (a copy is saved to Downloads first). Notes, drafts and agent conversations stay. Setup then starts again."),
																   QMessageBox::Reset | QMessageBox::Cancel, QMessageBox::Cancel)
				== QMessageBox::Reset;
		if (!confirmed) {
			return;
		}
		ResetDispatch();
		if (SetupView != nullptr) {
			SetupView->begin(core::setup_step::agent);
		}
		refresh();
	}

	void agent_tab_view::set_line_links(agent_file_resolver aResolve, agent_line_navigation aNavigate)
	{
		LineResolve  = std::move(aResolve);
		LineNavigate = std::move(aNavigate);
		for (auto& kept : Turns) {
			show_question(kept);
			show_answer(kept);
		}
	}

	void agent_tab_view::set_inline_code_file(const QString& aFileName)
	{
		if (InlineCodeFile == aFileName) {
			return;
		}
		InlineCodeFile = aFileName;
		for (const auto& kept : Turns) {
			if (kept.Answer != nullptr) {
				kept.Answer->set_inline_code_selector(InlineCodeFile.toStdString());
			}
		}
	}

	auto agent_tab_view::turn_question_label(const std::size_t aTurn) const -> QLabel* { return aTurn < Turns.size() ? Turns[aTurn].QuestionLabel.data() : nullptr; }

	auto agent_tab_view::turn_answer_view(const std::size_t aTurn) const -> markdown_body_view* { return aTurn < Turns.size() ? Turns[aTurn].Answer.data() : nullptr; }

	void agent_tab_view::fit_answer(markdown_body_view& aAnswer) const
	{
		// The answer is the viewport less the conversation's margins and its
		// reply card's. Owner look v004: measured without the card's, an
		// answer was given the height of a wider one and lost its last line.
		constexpr auto margins = 12 + 12 + reply_left_margin + reply_right_margin;
		const auto     width   = Viewport == nullptr ? aAnswer.width() : Viewport->width() - margins;
		aAnswer.setFixedHeight(aAnswer.isHidden() ? 0 : aAnswer.heightForWidth(std::max(1, width)));
	}

	void agent_tab_view::present_thread(const std::vector<agent_thread_turn>& aTurns)
	{
		for (auto& kept : Turns) {
			if (kept.Widget != nullptr) {
				kept.Widget->hide();
				ThreadColumn->removeWidget(kept.Widget);
				kept.Widget->deleteLater();
			}
		}
		Turns.clear();
		// A turn's cards went with its widgets.
		std::erase_if(CardEntries, [](const card_entry& aEntry) { return aEntry.Turn.has_value(); });
		for (const auto& shown : aTurns) {
			auto next       = turn{ };
			next.Question   = shown.Question;
			next.Done       = true;
			next.AskedAt    = shown.AskedAt;
			next.AnsweredAt = shown.AnsweredAt;
			add_turn_widgets(next);
			set_answer(next, shown.Answer);
			next.Footer->setText(shown.Footer);
			settle_reply_footer(next, true);
			Turns.push_back(std::move(next));
		}
		Finished = false;
		refresh();
	}

	void agent_tab_view::set_thread_observer(std::function<void(std::vector<agent_thread_turn>)> aObserver) { ThreadObserver = std::move(aObserver); }

	auto agent_tab_view::thread_turns() const -> std::vector<agent_thread_turn>
	{
		auto result = std::vector<agent_thread_turn>{ };
		for (const auto& kept : Turns) {
			if (kept.Done) {
				result.push_back({ .Question   = kept.Question,
								   .Answer     = kept.AnswerText,
								   .Footer     = kept.Footer != nullptr ? kept.Footer->text() : QString{ },
								   .AskedAt    = kept.AskedAt,
								   .AnsweredAt = kept.AnsweredAt });
			}
		}
		return result;
	}

	void agent_tab_view::set_clear_session_dispatch(std::function<void()> aDispatch) { ClearDispatch = std::move(aDispatch); }

	void agent_tab_view::set_clear_confirmation(std::function<bool(std::size_t)> aConfirm) { ClearConfirm = std::move(aConfirm); }

	void agent_tab_view::clear_session()
	{
		if (!panels_alive() || State == agent_tab_state::working) {
			return;
		}
		// Owner decision (b): a confirmation only when turns would leave the
		// thread; an empty one is cleared at once.
		if (!Turns.empty()) {
			const auto count     = Turns.size();
			const auto confirmed = ClearConfirm ? ClearConfirm(count)
												: QMessageBox::question(this, tr("Clear session?"),
																		count == 1U ? tr("The thread's one turn leaves it, and the next question starts a new session that does not know it.")
																					: tr("The thread's %1 turns leave it, and the next question starts a new session that does not know them.").arg(count),
																		QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
					== QMessageBox::Yes;
			if (!confirmed) {
				return;
			}
		}
		present_thread({ });
		if (ClearDispatch) {
			ClearDispatch();
		}
	}

	void agent_tab_view::set_suggestion_context(std::function<agent_suggestion_context()> aContext)
	{
		SuggestionContext = std::move(aContext);
		refresh_suggestions();
	}

	auto agent_tab_view::suggestions() const -> QStringList { return Prompts; }

	auto agent_tab_view::suggestion_buttons() const -> std::vector<QPushButton*>
	{
		auto result = std::vector<QPushButton*>{ };
		for (const auto& button : PromptButtons) {
			if (button != nullptr) {
				result.push_back(button);
			}
		}
		return result;
	}

	void agent_tab_view::send_prompt(const QString& aPrompt)
	{
		if (!panels_alive() || !Chat.send_action()->isEnabled()) {
			return;
		}
		Chat.composer()->setPlainText(aPrompt);
		ask();
	}

	void agent_tab_view::refresh_suggestions()
	{
		if (!panels_alive() || Suggestions == nullptr) {
			return;
		}
		// Owner decision (a): the pull request, the open file when one is
		// open, what the change could break, and the selected lines when
		// lines are selected.
		const auto context = SuggestionContext ? SuggestionContext() : agent_suggestion_context{ };
		auto       prompts = QStringList{ tr("Review this pull request") };
		if (ConversationShown) {
			// [[BPR-1773]]. While a conversation is attached the prompts are
			// about it; a reply is offered only where one can be written.
			prompts = QStringList{ tr("Summarize this conversation"), tr("Is the concern raised here right?") };
			if (ConversationShown->Replies) {
				prompts << tr("Draft a reply to this conversation");
			} else {
				prompts << tr("Explain the code this note is on");
			}
		} else {
			if (!context.OpenFile.isEmpty()) {
				prompts << tr("Review %1").arg(context.OpenFile);
			}
			prompts << tr("What could this change break?");
			if (context.Lines) {
				// Owner look v004 ("in an hour I wont remember which file line 34
				// was in"): the prompt names the file, and the thread links it.
				// Owner look v005 ("should change to explain selected block or
				// something when there's a selection, especially a multi-line
				// selection"): a block says so, and still names its file and lines.
				const auto [first, last] = *context.Lines;
				if (context.OpenFile.isEmpty()) {
					prompts << (first == last ? tr("Explain the selected line (%1)").arg(first) : tr("Explain the selected block (lines %1–%2)").arg(first).arg(last));
				} else {
					prompts << (first == last ? tr("Explain %1 line %2").arg(context.OpenFile).arg(first) : tr("Explain the selected block (%1 lines %2–%3)").arg(context.OpenFile).arg(first).arg(last));
				}
			}
		}
		const auto usable = Chat.send_action()->isEnabled() && State != agent_tab_state::working;
		if (prompts != Prompts) {
			Prompts = prompts;
			for (auto& button : PromptButtons) {
				if (button != nullptr) {
					button->hide();
					SuggestionColumn->removeWidget(button);
					button->deleteLater();
				}
			}
			PromptButtons.clear();
			for (const auto& prompt : Prompts) {
				auto* const button = new quiet_push_button{ Suggestions };
				button->setObjectName(QStringLiteral("agentSuggestion"));
				// A long path is shortened in its middle; the prompt sent is whole.
				button->setText(button->fontMetrics().elidedText(prompt, Qt::ElideMiddle, 320));
				button->setToolTip(prompt);
				button->setAccessibleName(prompt);
				button->setProperty("agentPrompt", prompt);
				connect(button, &QPushButton::clicked, this, [this, prompt] { send_prompt(prompt); });
				SuggestionColumn->addWidget(button, 0, Qt::AlignLeft);
				PromptButtons.emplace_back(button);
			}
		}
		for (const auto& button : PromptButtons) {
			if (button != nullptr) {
				button->setEnabled(usable);
			}
		}
		const auto conversing = State != agent_tab_state::not_set_up && State != agent_tab_state::setting_up;
		SuggestionsLine->setVisible(Turns.empty());
		// [[BPR-1755]], the owner's accept of look v007 ("Hide
		// suggestions until the the agent is done thinking"): none while a
		// turn runs; they return when it has finished.
		Suggestions->setVisible(conversing && SuggestionsMenu->isChecked() && State != agent_tab_state::working);
		SuggestionsMenu->setVisible(conversing);
	}

	void agent_tab_view::set_suggestions_shown(const bool aShown)
	{
		// The host's choice is not a new one: it is not said back.
		const auto quiet = QSignalBlocker{ SuggestionsMenu };
		SuggestionsMenu->setChecked(aShown);
		SuggestionsMenu->setToolTip(aShown ? tr("Hide the suggested prompts") : tr("Show the suggested prompts"));
		refresh_suggestions();
	}

	void agent_tab_view::set_suggestions_observer(std::function<void(bool)> aObserver) { SuggestionsObserver = std::move(aObserver); }

	auto agent_tab_view::suggestions_shown() const -> bool { return SuggestionsMenu->isChecked(); }

	void agent_tab_view::update_turn()
	{
		if (Turns.empty() || Turns.back().Done || Turns.back().Answer == nullptr || Turns.back().Footer == nullptr) {
			return;
		}
		auto&      current = Turns.back();
		const auto agent   = Launch.profile_selector()->currentText();
		const auto seconds = Started.isValid() ? Started.elapsed() / 1000 : 0;
		const auto elapsed = QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char{ '0' });
		const auto written = Launch.output_text().trimmed();
		switch (Launch.current_phase()) {
		case agent_launch_panel::phase::preparing:
			set_answer(current, written);
			current.Footer->setText(QStringLiteral("● ") + tr("%1 is making its workspace at the head, %2.").arg(agent, elapsed));
			return;
		case agent_launch_panel::phase::running: {
			set_answer(current, written);
			auto line = written.isEmpty() ? tr("%1 is thinking, %2.").arg(agent, elapsed) : tr("%1 is writing, %2.").arg(agent, elapsed);
			if (const auto latest = Launch.latest_output_line(); written.isEmpty() && !latest.isEmpty()) {
				line += QStringLiteral(" ") + latest.left(120);
			}
			current.Footer->setText(QStringLiteral("● ") + line);
			return;
		}
		case agent_launch_panel::phase::idle:
			break;
		}
		// The request ended: say what came of it.
		current.Done          = true;
		const auto findings   = Findings.finding_count();
		const auto succeeded  = Launch.last_outcome() == application::agent_run_outcome::success;
		const auto stopReason = Launch.stop_reason_text();
		if (succeeded) {
			set_answer(current, written.isEmpty() ? tr("%1 finished without writing an answer.").arg(agent) : written);
		} else {
			set_answer(current, !stopReason.isEmpty() ? stopReason : tr("%1 stopped before answering: %2.").arg(agent, Launch.status_text()));
		}
		auto ran = agent;
		if (const auto reported = Launch.last_reported_model(); succeeded && !reported.isEmpty()) {
			ran = tr("%1 (%2)").arg(agent, agent_model_display_name(reported));
			if (const auto profile = Launch.selected_profile()) {
				ReportedModels[profile->value()] = reported;
				present_run_settings_button();
			}
		}
		auto footer = succeeded ? tr("%1 answered in %2").arg(ran, elapsed) : tr("Stopped after %1").arg(elapsed);
		footer += Launch.last_reported_cost().isEmpty() ? QStringLiteral(".") : tr(" for $%1.").arg(Launch.last_reported_cost());
		if (succeeded && findings > 0U && FindingsPresented != FindingsAtTurn) {
			footer += QStringLiteral(" ") + (findings == 1U ? tr("1 finding, above.") : tr("%1 findings, above.").arg(findings));
		}
		current.Footer->setText(footer);
		current.AnsweredAt = QDateTime::currentDateTime();
		settle_reply_footer(current, succeeded);
		if (succeeded && FindingsPresented != FindingsAtTurn) {
			claim_findings(Turns.size() - 1U);
		}
		if (current.SaveNote != nullptr) {
			current.SaveNote->setVisible(succeeded && !current.AnswerText.isEmpty());
		}
		if (current.Draft != nullptr) {
			current.Draft->setVisible(succeeded && !current.AnswerText.isEmpty());
		}
		ClearSession->setVisible(true);
		if (ThreadObserver) {
			ThreadObserver(thread_turns());
		}
	}

	void agent_tab_view::settle_reply_footer(turn& aTurn, const bool aSucceeded)
	{
		// Stopped reads as the agent drawers' Interrupted: in italics.
		if (aTurn.Footer != nullptr) {
			auto font = aTurn.Footer->font();
			font.setItalic(!aSucceeded);
			aTurn.Footer->setFont(font);
		}
		if (aTurn.Copy != nullptr) {
			aTurn.Copy->setVisible(!aTurn.AnswerText.isEmpty());
		}
		if (aTurn.SaveNote != nullptr) {
			aTurn.SaveNote->setVisible(aSucceeded && !aTurn.AnswerText.isEmpty());
		}
		if (aTurn.Draft != nullptr) {
			aTurn.Draft->setVisible(aSucceeded && !aTurn.AnswerText.isEmpty());
		}
		if (aTurn.AnsweredTime != nullptr) {
			show_time(*aTurn.AnsweredTime, aTurn.AnsweredAt);
		}
	}

	void agent_tab_view::refresh_times()
	{
		for (const auto& kept : Turns) {
			if (kept.AskedTime != nullptr) {
				show_time(*kept.AskedTime, kept.AskedAt);
			}
			if (kept.AnsweredTime != nullptr && kept.Done) {
				show_time(*kept.AnsweredTime, kept.AnsweredAt);
			}
		}
	}

	auto agent_tab_view::turn_copy_button(const std::size_t aTurn) const -> QToolButton* { return aTurn < Turns.size() ? Turns[aTurn].Copy.data() : nullptr; }

	auto agent_tab_view::turn_time(const std::size_t aTurn) const -> QLabel* { return aTurn < Turns.size() ? Turns[aTurn].AnsweredTime.data() : nullptr; }

	void agent_tab_view::set_session_title(const QString& aTitle)
	{
		SessionTitle->setText(aTitle.isEmpty() ? tr("Agent") : aTitle);
		SessionTitle->setToolTip(aTitle);
	}

	void agent_tab_view::dress_answer(markdown_body_view& aAnswer) const
	{
		if (CodeFont) {
			aAnswer.set_code_font(*CodeFont, 4.0 * QFontMetricsF{ *CodeFont }.horizontalAdvance(QLatin1Char{ ' ' }));
		}
		if (CodeTheme) {
			aAnswer.set_code_theme(*CodeTheme);
		}
		aAnswer.set_inline_code_selector(InlineCodeFile.toStdString());
		if (CodeDispatch) {
			aAnswer.set_code_syntax_dispatch(CodeDispatch);
		}
	}

	void agent_tab_view::set_code_syntax_dispatch(std::function<void(std::vector<application::markdown_code_syntax_request>)> aDispatch)
	{
		CodeDispatch = std::move(aDispatch);
		for (const auto& kept : Turns) {
			if (kept.Answer != nullptr) {
				kept.Answer->set_code_syntax_dispatch(CodeDispatch);
				kept.Answer->request_code_syntax();
			}
		}
	}

	void agent_tab_view::apply_code_syntax(const application::markdown_code_syntax_publication& aSpans)
	{
		for (const auto& kept : Turns) {
			if (kept.Answer != nullptr) {
				kept.Answer->apply_code_syntax(aSpans);
			}
		}
	}

	void agent_tab_view::adopt_appearance(const appearance_profile& aProfile)
	{
		CodeFont  = aProfile.code_font();
		CodeTheme = markdown_code_theme{ .Colors = code_role_colors_of(aProfile.palette()), .Variant = aProfile.variant() };
		for (const auto& kept : Turns) {
			if (kept.Answer != nullptr) {
				dress_answer(*kept.Answer);
			}
		}
	}

	auto agent_tab_view::turn_answer(const std::size_t aTurn) const -> QString
	{
		return aTurn < Turns.size() ? Turns[aTurn].AnswerText : QString{ };
	}

	auto agent_tab_view::turn_question(const std::size_t aTurn) const -> QString { return aTurn < Turns.size() ? Turns[aTurn].Question : QString{ }; }

	auto agent_tab_view::turn_footer(const std::size_t aTurn) const -> QString
	{
		return aTurn < Turns.size() && Turns[aTurn].Footer != nullptr ? Turns[aTurn].Footer->text() : QString{ };
	}

	void agent_tab_view::present_banner()
	{
		if (!panels_alive()) {
			return;
		}
		auto* const callout = static_cast<rounded*>(Banner);
		callout->Look       = State == agent_tab_state::stopped ? rounded::look::danger : rounded::look::callout;
		BannerFirst->disconnect();
		BannerSecond->disconnect();
		BannerFirst->show();
		BannerSecond->show();
		Log->setVisible(LogShown && State == agent_tab_state::stopped);
		switch (State) {
		case agent_tab_state::not_set_up:
		case agent_tab_state::setting_up:
			Banner->hide();
			return;
		case agent_tab_state::idle: {
			if (!Finished || !Turns.empty()) {
				Banner->hide();
				return;
			}
			callout->Look    = rounded::look::ready;
			const auto count = Findings.finding_count();
			BannerTitle->setText(tr("%1 finished").arg(Launch.profile_selector()->currentText()));
			BannerText->setText(count > 0U                             ? (count == 1U ? tr("1 finding below. Draft a comment from it, or ask a follow-up.") : tr("%1 findings below. Draft a comment from one, or ask a follow-up.").arg(count))
									: Chat.replies_list()->count() > 0 ? tr("Its answer is below. Ask a follow-up in the question field.")
																	   : tr("It found nothing to report. Ask a question below."));
			BannerFirst->hide();
			BannerSecond->hide();
			Banner->show();
			Banner->update();
			return;
		}
		case agent_tab_state::working: {
			// Owner look v003: the turn's own quiet line says it works, and
			// Stop is the field's Send; a turn shown makes the banner needless.
			if (!Turns.empty()) {
				Banner->hide();
				return;
			}
			BannerTitle->setText(tr("%1 is working").arg(Launch.profile_selector()->currentText()));
			const auto seconds = Started.isValid() ? Started.elapsed() / 1000 : 0;
			const auto elapsed = QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char{ '0' });
			auto       line    = Launch.current_phase() == agent_launch_panel::phase::running
				? tr("Running, %1. The answer and any findings appear here when it finishes.").arg(elapsed)
				: tr("Making the agent's workspace at the head, %1.").arg(elapsed);
			const auto latest  = Launch.latest_output_line();
			if (!latest.isEmpty()) {
				line += QStringLiteral("\n") + tr("Latest: %1").arg(latest.left(160));
			}
			BannerText->setText(line);
			BannerFirst->setText(tr("Stop"));
			BannerFirst->show();
			connect(BannerFirst, &QPushButton::clicked, Launch.cancel_action(), &QAction::trigger);
			BannerSecond->hide();
			break;
		}
		case agent_tab_state::head_moved:
			BannerTitle->setText(tr("This session read %1").arg(Head->text()));
			BannerText->setText(tr("The pull request has moved on. Its findings stay anchored to the head it read."));
			BannerFirst->setText(Chat.new_session_action()->text().isEmpty() ? tr("Start a new session") : Chat.new_session_action()->text());
			connect(BannerFirst, &QPushButton::clicked, Chat.new_session_action(), &QAction::trigger);
			BannerSecond->setText(Chat.continue_old_revision_action()->text().isEmpty() ? tr("Continue on the old head") : Chat.continue_old_revision_action()->text());
			connect(BannerSecond, &QPushButton::clicked, Chat.continue_old_revision_action(), &QAction::trigger);
			break;
		case agent_tab_state::stopped:
			BannerTitle->setText(tr("%1 stopped before answering").arg(Launch.profile_selector()->currentText()));
			// [[BPR-1738]]. A stop with a reason says it and what to do;
			// the log keeps the raw text.
			BannerText->setText(Launch.stop_reason_text().isEmpty() ? tr("%1. Your question is kept.").arg(Launch.status_text())
																	: tr("%1 Your question is kept.").arg(Launch.stop_reason_text()));
			BannerFirst->setText(tr("Ask again"));
			connect(BannerFirst, &QPushButton::clicked, Launch.start_action(), &QAction::trigger);
			// [[BPR-1753]], RFC-BPR-0092 Partly set up. An Ask that ended
			// for a setup reason offers that check again, then the setup's
			// words, rather than only asking again. The question is kept.
			if (SetupView != nullptr && CheckAgain
				&& (Launch.last_outcome() == application::agent_run_outcome::workspace_unavailable || Launch.last_outcome() == application::agent_run_outcome::failed_to_start)) {
				BannerFirst->disconnect();
				BannerFirst->setText(tr("Check again"));
				connect(BannerFirst, &QPushButton::clicked, this, [this] {
					SetupView->begin(core::setup_step::check);
					CheckAgain();
				});
			}
			BannerSecond->setText(LogShown ? tr("Hide the log") : tr("Show the log"));
			connect(BannerSecond, &QPushButton::clicked, this, [this] {
				LogShown = !LogShown;
				Log->setText(Launch.output_text().isEmpty() ? tr("The agent wrote nothing.") : Launch.output_text());
				present_banner();
			});
			break;
		}
		Banner->show();
		Banner->update();
	}

	auto agent_tab_view::finding_cards() const -> std::vector<QWidget*>
	{
		auto result = std::vector<QWidget*>{ };
		for (const auto& entry : CardEntries) {
			if (entry.Widget != nullptr) {
				result.push_back(entry.Widget);
			}
		}
		return result;
	}

	auto agent_tab_view::turn_cards(const std::size_t aTurn) const -> std::vector<QWidget*>
	{
		auto result = std::vector<QWidget*>{ };
		for (const auto& entry : CardEntries) {
			if (entry.Turn == std::optional{ aTurn } && entry.Widget != nullptr) {
				result.push_back(entry.Widget);
			}
		}
		return result;
	}

	auto agent_tab_view::finding_card_state(const std::size_t aCard) const -> agent_finding_card_state
	{
		return aCard < CardEntries.size() ? CardEntries[aCard].State : agent_finding_card_state::open;
	}

	void agent_tab_view::set_finding_card_dispatch(agent_finding_card_dispatch aDispatch) { CardDispatch = std::move(aDispatch); }

	void agent_tab_view::set_answer_note_dispatch(agent_answer_note_dispatch aDispatch) { NoteDispatch = std::move(aDispatch); }

	void agent_tab_view::set_conversation_chip(std::optional<agent_conversation_chip> aChip)
	{
		if (aChip == ConversationShown) {
			return;
		}
		ConversationShown = std::move(aChip);
		ConversationChip->setVisible(ConversationShown.has_value());
		ConversationText->setText(ConversationShown ? ConversationShown->Text : QString{ });
		ConversationChip->setToolTip(ConversationShown ? ConversationShown->Detail : QString{ });
		Chat.composer()->setPlaceholderText(ConversationShown ? tr("Ask about this conversation…") : tr("Ask about this pull request…"));
		refresh_suggestions();
	}

	void agent_tab_view::set_conversation_dismiss(std::function<void()> aDismiss)
	{
		ConversationDismiss = std::move(aDismiss);
	}

	auto agent_tab_view::conversation_chip_text() const -> QString
	{
		return ConversationShown ? ConversationShown->Text : QString{ };
	}

	void agent_tab_view::set_answer_draft_target(std::function<agent_answer_draft_target()> aTarget) { DraftTargetSource = std::move(aTarget); }

	void agent_tab_view::set_answer_draft_dispatch(agent_answer_draft_dispatch aDispatch) { DraftDispatch = std::move(aDispatch); }

	void agent_tab_view::set_draft_navigation(agent_answer_draft_navigation aNavigate) { DraftNavigate = std::move(aNavigate); }

	auto agent_tab_view::turn_draft_button(const std::size_t aTurn) const -> QPushButton* { return aTurn < Turns.size() ? Turns[aTurn].Draft.data() : nullptr; }

	auto agent_tab_view::turn_asked(const std::size_t aTurn) const -> QString
	{
		if (aTurn >= Turns.size()) {
			return { };
		}
		return Turns[aTurn].Asked != nullptr ? Turns[aTurn].Asked->accessibleName() : Turns[aTurn].Question;
	}

	auto agent_tab_view::turn_about(const std::size_t aTurn) const -> QString
	{
		return aTurn < Turns.size() && Turns[aTurn].About ? Turns[aTurn].About->Text : QString{ };
	}

	auto agent_tab_view::turn_note_button(const std::size_t aTurn) const -> QPushButton* { return aTurn < Turns.size() ? Turns[aTurn].SaveNote.data() : nullptr; }

	void agent_tab_view::present_finding_card_state(const std::uint64_t aSession, const std::size_t aOrdinal, const agent_finding_card_state aState, const QString& aWhy, const std::uint64_t aNote)
	{
		for (auto& entry : CardEntries) {
			if (entry.Session == aSession && entry.Ordinal == aOrdinal) {
				entry.State = aState;
				entry.Why   = aWhy;
				entry.Note  = aState == agent_finding_card_state::noted ? aNote : 0U;
				show_card_state(entry);
			}
		}
	}

	void agent_tab_view::rebuild_findings()
	{
		if (!panels_alive()) {
			return;
		}
		// The cards no turn brought: a delivery shown before any turn, or one
		// a turn has not claimed. Cards under a turn stay with it.
		for (auto entry = CardEntries.begin(); entry != CardEntries.end();) {
			if (entry->Turn) {
				++entry;
				continue;
			}
			if (entry->Widget != nullptr) {
				// Gone from view now; deleted once any press on it returns.
				entry->Widget->hide();
				CardColumn->removeWidget(entry->Widget);
				entry->Widget->deleteLater();
			}
			entry = CardEntries.erase(entry);
		}
		const auto* const delivery = Findings.shown_delivery();
		if (delivery == nullptr || ClaimedSession == std::optional{ delivery->Session.value() }) {
			Cards->hide();
			return;
		}
		for (std::size_t ordinal = 0U; ordinal < delivery->Findings.size(); ++ordinal) {
			auto entry = card_entry{
				.Session  = delivery->Session.value(),
				.Ordinal  = ordinal,
				.Finding  = delivery->Findings[ordinal].Finding,
				.Anchored = delivery->Findings[ordinal].Verdict.State == core::agent_finding_anchor_state::anchored,
			};
			CardEntries.push_back(std::move(entry));
			auto* const card = make_card(CardEntries.back(), Cards);
			CardColumn->addWidget(card);
		}
		Cards->setVisible(!delivery->Findings.empty());
	}

	void agent_tab_view::claim_findings(const std::size_t aTurn)
	{
		const auto* const delivery = Findings.shown_delivery();
		if (delivery == nullptr || aTurn >= Turns.size() || Turns[aTurn].CardColumn == nullptr || delivery->Findings.empty()) {
			return;
		}
		ClaimedSession = delivery->Session.value();
		// The loose cards were this delivery's; they move under the turn.
		rebuild_findings();
		for (std::size_t ordinal = 0U; ordinal < delivery->Findings.size(); ++ordinal) {
			auto entry = card_entry{
				.Session  = delivery->Session.value(),
				.Ordinal  = ordinal,
				.Finding  = delivery->Findings[ordinal].Finding,
				.Anchored = delivery->Findings[ordinal].Verdict.State == core::agent_finding_anchor_state::anchored,
				.Turn     = aTurn,
			};
			CardEntries.push_back(std::move(entry));
			auto* const card = make_card(CardEntries.back(), Turns[aTurn].Widget);
			Turns[aTurn].CardColumn->addWidget(card);
		}
	}

	auto agent_tab_view::make_card(card_entry& aEntry, QWidget* aParent) -> QWidget*
	{
		const auto& finding = aEntry.Finding;
		auto* const card    = new rounded{ rounded::look::card, aParent };
		card->setObjectName(QStringLiteral("agentFindingCard"));
		card->Edge      = severity_colour(finding.Severity, palette());
		auto* const col = new QVBoxLayout{ card };
		col->setContentsMargins(14, 10, 12, 10);
		col->setSpacing(5);
		auto* const meta = new QHBoxLayout{ };
		meta->setSpacing(6);
		auto* const severity = new QLabel{ capitalised(core::name(finding.Severity)), card };
		auto        sevFont  = severity->font();
		sevFont.setWeight(QFont::DemiBold);
		severity->setFont(sevFont);
		auto sevPalette = severity->palette();
		sevPalette.setColor(QPalette::WindowText, severity_colour(finding.Severity, palette()));
		severity->setPalette(sevPalette);
		meta->addWidget(severity);
		auto* const kind = new QLabel{ QStringLiteral("%1 · %2").arg(confidence_words(finding.Confidence), QString::fromUtf8(core::name(finding.Category).data(), static_cast<qsizetype>(core::name(finding.Category).size()))), card };
		kind->setForegroundRole(QPalette::PlaceholderText);
		// The kind gives way to the anchor when the drawer is narrow.
		kind->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		kind->setToolTip(kind->text());
		meta->addWidget(kind, 1);
		// Lines are shown one-based, first and last, as an editor numbers them.
		const auto  first = finding.Lines.Begin + 1U;
		const auto  last  = std::max(first, finding.Lines.End);
		auto* const where = new QLabel{ first == last ? QStringLiteral("%1:%2").arg(QString::fromStdString(finding.Path).section(QLatin1Char{ '/' }, -1)).arg(first)
													  : QStringLiteral("%1:%2–%3").arg(QString::fromStdString(finding.Path).section(QLatin1Char{ '/' }, -1)).arg(first).arg(last),
										card };
		where->setObjectName(QStringLiteral("agentFindingAnchor"));
		auto code = QFont{ QStringLiteral("Menlo") };
		code.setStyleHint(QFont::Monospace);
		code.setPointSizeF(std::max(1.0, font().pointSizeF() * 0.86));
		code.setWeight(QFont::DemiBold);
		where->setFont(code);
		where->setToolTip(aEntry.Anchored ? QString::fromStdString(finding.Path) : tr("%1 (not the file open when the agent answered)").arg(QString::fromStdString(finding.Path)));
		meta->addWidget(where);
		col->addLayout(meta);
		auto* const claim = new QLabel{ QString::fromStdString(finding.Claim), card };
		claim->setWordWrap(true);
		claim->setFont(sevFont);
		claim->setTextFormat(Qt::PlainText);
		col->addWidget(claim);
		if (!finding.Evidence.empty()) {
			auto* const evidence = new QLabel{ QString::fromStdString(finding.Evidence), card };
			evidence->setWordWrap(true);
			evidence->setTextFormat(Qt::PlainText);
			evidence->setForegroundRole(QPalette::PlaceholderText);
			col->addWidget(evidence);
		}
		if (finding.SuggestedComment && !finding.SuggestedComment->empty()) {
			auto* const comment = new QLabel{ QString::fromStdString(*finding.SuggestedComment), card };
			comment->setObjectName(QStringLiteral("agentFindingComment"));
			comment->setWordWrap(true);
			comment->setTextFormat(Qt::PlainText);
			comment->setTextInteractionFlags(Qt::TextSelectableByMouse);
			auto italic = comment->font();
			italic.setItalic(true);
			comment->setFont(italic);
			col->addWidget(comment);
		}
		auto* const actions = new QHBoxLayout{ };
		actions->setSpacing(6);
		aEntry.Id         = NextCardId++;
		const auto  index = aEntry.Id;
		auto* const draft = new quiet_push_button{ tr("Add as draft comment"), card };
		draft->setObjectName(QStringLiteral("agentFindingDraftComment"));
		draft->setToolTip(tr("A private draft at its lines, from its suggested comment. Nothing is published."));
		connect(draft, &QPushButton::clicked, this, [this, index] { act_on_card(index, agent_finding_card_action::draft_comment); });
		actions->addWidget(draft);
		auto* const note = new quiet_push_button{ tr("Save as note"), card };
		note->setObjectName(QStringLiteral("agentFindingSaveNote"));
		note->setToolTip(tr("A private note on its lines, never published"));
		connect(note, &QPushButton::clicked, this, [this, index] { act_on_card(index, agent_finding_card_action::save_note); });
		actions->addWidget(note);
		auto* const dismiss = new quiet_push_button{ tr("Dismiss"), card };
		dismiss->setObjectName(QStringLiteral("agentFindingDismiss"));
		dismiss->setFlat(true);
		connect(dismiss, &QPushButton::clicked, this, [this, index] { act_on_card(index, agent_finding_card_action::dismiss); });
		actions->addWidget(dismiss);
		auto* const state = new QLabel{ card };
		state->setObjectName(QStringLiteral("agentFindingState"));
		state->setForegroundRole(QPalette::PlaceholderText);
		state->setWordWrap(true);
		state->hide();
		actions->addWidget(state, 1);
		auto* const undo = new quiet_push_button{ tr("Undo"), card };
		undo->setObjectName(QStringLiteral("agentFindingUndo"));
		undo->setFlat(true);
		undo->hide();
		connect(undo, &QPushButton::clicked, this, [this, index] { act_on_card(index, agent_finding_card_action::undo_dismiss); });
		actions->addWidget(undo);
		// Owner look v004: a finding saved as a note goes to it.
		auto* const goToNote = new quiet_push_button{ tr("Go to note"), card };
		goToNote->setObjectName(QStringLiteral("agentFindingGoToNote"));
		goToNote->setToolTip(tr("Open the private note this finding was saved as"));
		goToNote->hide();
		connect(goToNote, &QPushButton::clicked, this, [this, index] {
			const auto found = std::ranges::find(CardEntries, index, &card_entry::Id);
			if (found != CardEntries.end() && found->Note != 0U && NoteNavigate) {
				NoteNavigate(found->Note);
			}
		});
		actions->addWidget(goToNote);
		actions->addStretch(1);
		col->addLayout(actions);
		// Choosing a card selects its finding and shows its lines.
		card->installEventFilter(this);
		card->setProperty("agentFindingCard", QVariant::fromValue<qulonglong>(index));
		card->setCursor(Qt::PointingHandCursor);
		aEntry.Widget = card;
		show_card_state(aEntry);
		return card;
	}

	void agent_tab_view::show_card_state(card_entry& aEntry)
	{
		if (aEntry.Widget == nullptr) {
			return;
		}
		auto* const card  = aEntry.Widget.data();
		auto* const state = card->findChild<QLabel*>(QStringLiteral("agentFindingState"));
		auto* const undo  = card->findChild<QPushButton*>(QStringLiteral("agentFindingUndo"));
		const auto  open  = aEntry.State == agent_finding_card_state::open || aEntry.State == agent_finding_card_state::refused;
		for (const auto* const name : { "agentFindingDraftComment", "agentFindingSaveNote", "agentFindingDismiss" }) {
			if (auto* const button = card->findChild<QPushButton*>(QString::fromLatin1(name)); button != nullptr) {
				button->setVisible(open);
			}
		}
		undo->setVisible(aEntry.State == agent_finding_card_state::dismissed);
		if (auto* const goToNote = card->findChild<QPushButton*>(QStringLiteral("agentFindingGoToNote")); goToNote != nullptr) {
			goToNote->setVisible(aEntry.State == agent_finding_card_state::noted && aEntry.Note != 0U && NoteNavigate);
		}
		switch (aEntry.State) {
		case agent_finding_card_state::open:      state->clear(); break;
		case agent_finding_card_state::working:   state->setText(tr("Working…")); break;
		case agent_finding_card_state::drafted:   state->setText(tr("Drafted. It is a private draft until you publish it.")); break;
		case agent_finding_card_state::noted:     state->setText(tr("Saved as a private note.")); break;
		case agent_finding_card_state::dismissed: state->setText(tr("Dismissed.")); break;
		case agent_finding_card_state::refused:   state->setText(aEntry.Why.isEmpty() ? tr("That could not be done.") : aEntry.Why); break;
		}
		state->setVisible(!state->text().isEmpty());
		card->setProperty("agentFindingStateName", static_cast<int>(aEntry.State));
		card->setEnabled(true);
		card->update();
	}

	void agent_tab_view::act_on_card(const std::uint64_t aCard, const agent_finding_card_action aAction)
	{
		const auto found = std::ranges::find(CardEntries, aCard, &card_entry::Id);
		if (found == CardEntries.end() || !panels_alive()) {
			return;
		}
		auto& entry = *found;
		if (aAction == agent_finding_card_action::undo_dismiss) {
			// A dismissal made nothing, so taking it back is the card's alone.
			entry.State = agent_finding_card_state::open;
			show_card_state(entry);
			if (CardDispatch) {
				CardDispatch({ .Session = entry.Session, .Ordinal = entry.Ordinal, .Finding = entry.Finding, .Action = aAction });
			}
			return;
		}
		if (CardDispatch) {
			entry.State = aAction == agent_finding_card_action::dismiss ? agent_finding_card_state::dismissed : agent_finding_card_state::working;
			show_card_state(entry);
			CardDispatch({ .Session = entry.Session, .Ordinal = entry.Ordinal, .Finding = entry.Finding, .Action = aAction });
			return;
		}
		// Without a host the queue's own actions are asked, as before.
		const auto* const delivery = Findings.shown_delivery();
		if (delivery == nullptr || delivery->Session.value() != entry.Session) {
			return;
		}
		Findings.select(entry.Ordinal);
		const auto queued = aAction == agent_finding_card_action::draft_comment ? finding_queue_action::convert_to_draft_comment
			: aAction == agent_finding_card_action::save_note                   ? finding_queue_action::add_to_private_notes
																				: finding_queue_action::dismiss;
		if (auto* const action = Findings.action(queued); action != nullptr) {
			action->trigger();
		}
		entry.State = aAction == agent_finding_card_action::draft_comment ? agent_finding_card_state::drafted
			: aAction == agent_finding_card_action::save_note             ? agent_finding_card_state::noted
																		  : agent_finding_card_state::dismissed;
		show_card_state(entry);
	}

	void agent_tab_view::ask()
	{
		if (!panels_alive()) {
			return;
		}
		if (run_slash_command(Chat.composer()->toPlainText().trimmed())) {
			return;
		}
		Notice->hide();
		Chat.send_action()->trigger();
	}

	void agent_tab_view::present_run_settings(std::vector<agent_run_settings> aSettings)
	{
		RunSettingsShown = std::move(aSettings);
		present_run_settings_button();
	}

	void agent_tab_view::set_run_settings_dispatch(agent_run_settings_dispatch aDispatch) { RunSettingsDispatch = std::move(aDispatch); }

	auto agent_tab_view::current_run_settings() const -> const agent_run_settings*
	{
		const auto profile = Launch.selected_profile();
		if (!profile) {
			return nullptr;
		}
		const auto found = std::ranges::find_if(RunSettingsShown, [&profile](const agent_run_settings& aSettings) { return aSettings.Profile == profile->value(); });
		return found == RunSettingsShown.end() ? nullptr : &*found;
	}

	auto agent_tab_view::run_settings_text() const -> QString { return RunSettings->property("agentRunSettingsText").toString(); }

	void agent_tab_view::fit_run_settings()
	{
		// A narrow field keeps the pill as an icon with its words in the
		// tooltip, as the Codex composer does.
		const auto full   = RunSettings->property("agentRunSettingsText").toString();
		const auto narrow = Field != nullptr && Field->width() > 0 && Field->width() < 300;
		RunSettings->setText(narrow ? QString{ } : full);
		RunSettings->setIcon(narrow ? drawer_icon(drawer_glyph::agent, palette()) : QIcon{ });
		RunSettings->setToolTip(narrow ? tr("Choose the agent and model: %1").arg(full) + QStringLiteral("\n") + RunSettingsTip : RunSettingsTip);
		RunSettings->updateGeometry();
	}

	void agent_tab_view::present_run_settings_button()
	{
		const auto* const settings = current_run_settings();
		RunSettings->setVisible(settings != nullptr);
		if (settings == nullptr) {
			return;
		}
		// Short enough for the field's row: the model, or "Default model",
		// and the effort when one is chosen.
		// Owner look v004: a model by its name and version. The agent's own
		// default is named by what it last reported running, when it did.
		const auto reported = ReportedModels.find(settings->Profile);
		const auto ranAs    = reported == ReportedModels.end() ? QString{ } : agent_model_display_name(reported->second);
		const auto model    = settings->Model.empty() ? tr("default model") : agent_model_display_name(QString::fromStdString(settings->Model));
		auto       shown    = !settings->Model.empty() ? model : ranAs.isEmpty() ? tr("Default model")
																				 : ranAs;
		if (!settings->Effort.empty()) {
			shown += QStringLiteral(" · ") + tr("%1 effort").arg(QString::fromStdString(settings->Effort));
		}
		// Owner amendment 2026-10-23: agent · model · effort.
		const auto full = settings->Agent + QStringLiteral(" · ") + (!(settings->ModelChoosable || settings->EffortChoosable) ? tr("its own model") : shown);
		RunSettings->setProperty("agentRunSettingsText", full);
		// The whole words, shortened when the row is narrow (menu_word).
		RunSettings->setAccessibleName(tr("Agent, model and effort: %1").arg(full));
		fit_run_settings();
		auto tip = tr("%1 runs %2").arg(settings->Agent, settings->Program.isEmpty() ? tr("the program setup found") : settings->Program);
		if (settings->ModelChoosable) {
			tip += QStringLiteral("\n") + (settings->Model.empty() ? tr("Model: the agent's own default") : tr("Model: %1").arg(model));
		}
		if (settings->EffortChoosable) {
			tip += QStringLiteral("\n") + (settings->Effort.empty() ? tr("Effort: the agent's own default") : tr("Effort: %1").arg(QString::fromStdString(settings->Effort)));
		}
		if (!settings->Note.isEmpty()) {
			tip += QStringLiteral("\n") + settings->Note;
		}
		if (!ranAs.isEmpty()) {
			tip += QStringLiteral("\n") + tr("Last answer ran %1 (%2)").arg(ranAs, reported->second);
		}
		RunSettingsTip = tip;
		fit_run_settings();
		auto* const menu = RunSettings->menu();
		// A menu open now is rebuilt once it closes, never under the pointer.
		if (menu->isVisible()) {
			MenuStale = true;
			return;
		}
		MenuStale = false;
		// `clear` keeps the Model and Effort menus it made; each rebuild
		// makes its own, so the old ones go.
		for (auto* const submenu : menu->findChildren<QMenu*>(Qt::FindDirectChildrenOnly)) {
			submenu->deleteLater();
		}
		menu->clear();
		// Every agent added, one press away; each keeps its own conversation
		// about this pull request (owner decision 2026-10-23).
		auto* const agents = new QActionGroup{ menu };
		for (const auto& offered : RunSettingsShown) {
			auto* const item = menu->addAction(offered.Agent);
			item->setObjectName(QStringLiteral("agentRunSettingsAgent"));
			item->setCheckable(true);
			item->setChecked(offered.Profile == settings->Profile);
			agents->addAction(item);
			connect(item, &QAction::triggered, this, [this, profile = offered.Profile] { choose_agent(profile); });
		}
		menu->addSeparator();
		auto* const what = menu->addAction(tr("%1 runs %2").arg(settings->Agent, settings->Program.isEmpty() ? tr("the program setup found") : settings->Program));
		what->setEnabled(false);
		if (!settings->Note.isEmpty()) {
			menu->addAction(settings->Note)->setEnabled(false);
		}
		const auto profile = settings->Profile;
		const auto apply   = [this, profile](std::optional<std::string> aModel, std::optional<std::string> aEffort) {
			if (RunSettingsDispatch && !RunSettingsDispatch(profile, std::move(aModel), std::move(aEffort))) {
				Notice->setText(tr("The agent doesn't take that setting."));
				Notice->show();
			}
		};
		if (settings->ModelChoosable) {
			auto* const models = menu->addMenu(tr("Model"));
			models->setObjectName(QStringLiteral("agentRunSettingsModels"));
			auto* const group = new QActionGroup{ models };
			const auto  add   = [&](const std::string& aValue, const QString& aLabel) {
				auto* const action = models->addAction(aLabel);
				action->setCheckable(true);
				action->setChecked(settings->Model == aValue);
				group->addAction(action);
				connect(action, &QAction::triggered, this, [apply, aValue] { apply(aValue, std::nullopt); });
			};
			add({ }, ranAs.isEmpty() ? tr("Default (the agent's own setting)") : tr("Default (the agent's own; last ran %1)").arg(ranAs));
			if (settings->Models.empty() && settings->Listing) {
				models->addAction(tr("Asking %1 for its models…").arg(settings->Agent))->setEnabled(false);
			}
			for (const auto& [value, label] : settings->Models) {
				add(value, label);
			}
			const auto known = settings->Model.empty() || std::ranges::any_of(settings->Models, [&settings](const auto& aModel) { return aModel.first == settings->Model; });
			if (!known) {
				add(settings->Model, agent_model_display_name(QString::fromStdString(settings->Model)));
			}
			connect(models->addAction(tr("Other…")), &QAction::triggered, this, [this, apply, current = settings->Model] {
				auto       accepted = false;
				const auto name     = QInputDialog::getText(this, tr("Model"), tr("The model's name, as the agent's --model takes it:"), QLineEdit::Normal, QString::fromStdString(current), &accepted).trimmed();
				if (accepted) {
					apply(name.toStdString(), std::nullopt);
				}
			});
		}
		if (settings->EffortChoosable) {
			auto* const efforts = menu->addMenu(tr("Effort"));
			efforts->setObjectName(QStringLiteral("agentRunSettingsEfforts"));
			auto* const group = new QActionGroup{ efforts };
			const auto  add   = [&](const std::string& aValue, const QString& aLabel) {
				auto* const action = efforts->addAction(aLabel);
				action->setCheckable(true);
				action->setChecked(settings->Effort == aValue);
				group->addAction(action);
				connect(action, &QAction::triggered, this, [apply, aValue] { apply(std::nullopt, aValue); });
			};
			add({ }, tr("Default (the agent's own setting)"));
			for (const auto& level : settings->Efforts) {
				add(level, QString::fromStdString(level));
			}
		}
		menu->addSeparator();
		menu->addAction(tr("Type /model NAME or /effort LEVEL in the question field"))->setEnabled(false);
		// Owner look v006 ("This model picker dropdown should have a link to
		// open the agent settings page").
		auto* const open = menu->addAction(tr("Agent settings…"));
		open->setObjectName(QStringLiteral("agentRunSettingsOpenSettings"));
		connect(open, &QAction::triggered, this, [this] {
			if (Setup) {
				Setup();
			}
		});
	}

	auto agent_tab_view::choose_agent(const std::uint64_t aProfile) -> bool
	{
		if (!panels_alive() || State == agent_tab_state::working) {
			return false;
		}
		auto* const selector = Launch.profile_selector();
		const auto  index    = selector->findData(QVariant::fromValue<qulonglong>(aProfile));
		if (index < 0) {
			return false;
		}
		selector->setCurrentIndex(index);
		present_run_settings_button();
		return true;
	}

	auto agent_tab_view::run_slash_command(const QString& aText) -> bool
	{
		if (!aText.startsWith(QLatin1Char{ '/' })) {
			return false;
		}
		const auto words   = aText.mid(1).split(QLatin1Char{ ' ' }, Qt::SkipEmptyParts);
		const auto command = words.isEmpty() ? QString{ } : words.front().toLower();
		if (command != QStringLiteral("model") && command != QStringLiteral("effort")) {
			return false;
		}
		const auto* const settings = current_run_settings();
		const auto        isModel  = command == QStringLiteral("model");
		Notice->show();
		if (settings == nullptr || (isModel ? !settings->ModelChoosable : !settings->EffortChoosable)) {
			Notice->setText(isModel ? tr("This agent chooses its own model.") : tr("This agent has no effort setting."));
			return true;
		}
		if (words.size() < 2) {
			RunSettings->showMenu();
			Notice->setText(isModel ? tr("Choose a model, or type /model NAME.") : tr("Choose an effort, or type /effort LEVEL."));
			return true;
		}
		auto value = words.at(1).toStdString();
		if (value == "default") {
			value.clear();
		}
		// An effort is one the harness offers, as its menu lists them.
		const auto offered = isModel || value.empty() || settings->Efforts.empty() || std::ranges::contains(settings->Efforts, value);
		const auto taken   = offered && RunSettingsDispatch && (isModel ? RunSettingsDispatch(settings->Profile, value, std::nullopt) : RunSettingsDispatch(settings->Profile, std::nullopt, value));
		if (!taken) {
			Notice->setText(isModel ? tr("%1 isn't a model name the agent takes.").arg(words.at(1)) : tr("%1 isn't an effort this agent takes. It takes %2.").arg(words.at(1), QString::fromStdString([&settings] {
																																									  auto list = std::string{ };
																																									  for (const auto& effort : settings->Efforts) {
																																										  list += (list.empty() ? "" : ", ") + effort;
																																									  }
																																									  return list;
																																								  }())));
			return true;
		}
		Chat.composer()->clear();
		Notice->setText(isModel ? (value.empty() ? tr("The next question runs on the agent's own model.") : tr("The next question runs on %1.").arg(QString::fromStdString(value)))
								: (value.empty() ? tr("The next question runs at the agent's own effort.") : tr("The next question runs at %1 effort.").arg(QString::fromStdString(value))));
		return true;
	}

	auto agent_tab_view::eventFilter(QObject* aWatched, QEvent* aEvent) -> bool
	{
		if (aEvent->type() == QEvent::KeyPress && aWatched == Chat.composer()) {
			const auto* const key = static_cast<QKeyEvent*>(aEvent);
			if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) && (key->modifiers() & (Qt::ControlModifier | Qt::MetaModifier)) != 0) {
				ask();
				return true;
			}
		}
		if (aEvent->type() == QEvent::Resize && aWatched == Field) {
			fit_run_settings();
			return false;
		}
		if (aEvent->type() == QEvent::Resize && aWatched == Viewport) {
			for (const auto& kept : Turns) {
				if (kept.Answer != nullptr) {
					fit_answer(*kept.Answer);
				}
			}
			return false;
		}
		if (aEvent->type() == QEvent::MouseButtonRelease && aWatched->objectName() == QStringLiteral("agentFindingCard")) {
			const auto id    = static_cast<std::uint64_t>(aWatched->property("agentFindingCard").toULongLong());
			const auto found = std::ranges::find(CardEntries, id, &card_entry::Id);
			// Its lines are shown while its delivery is the one the queue holds.
			if (const auto* const delivery = Findings.shown_delivery(); found != CardEntries.end() && delivery != nullptr && delivery->Session.value() == found->Session) {
				Findings.select(found->Ordinal);
				if (auto* const action = Findings.action(finding_queue_action::open_evidence); action != nullptr) {
					action->trigger();
				}
			}
			for (const auto& entry : CardEntries) {
				if (entry.Widget != nullptr) {
					static_cast<rounded*>(entry.Widget.data())->Selected = entry.Widget == aWatched;
					entry.Widget->update();
				}
			}
			return true;
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}

	void agent_tab_view::showEvent(QShowEvent* aEvent)
	{
		QWidget::showEvent(aEvent);
		if (FirstShown) {
			const auto observer = std::exchange(FirstShown, { });
			observer();
		}
		refresh();
	}

	void agent_tab_view::set_first_shown(std::function<void()> aObserver)
	{
		FirstShown = std::move(aObserver);
		if (FirstShown && isVisible()) {
			const auto observer = std::exchange(FirstShown, { });
			observer();
		}
	}

	void agent_tab_view::changeEvent(QEvent* aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange && Empty != nullptr) {
			rebuild_findings();
		}
	}
} // namespace bpr::qt
