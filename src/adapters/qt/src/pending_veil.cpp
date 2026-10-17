#include "bpr/qt/pending_veil.hpp"

#include "bpr/qt/home_row_delegate.hpp"

#include <QAbstractScrollArea>
#include <QAccessible>
#include <QAccessibleEvent>
#include <QAccessibleWidget>
#include <QElapsedTimer>
#include <QEvent>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPushButton>
#include <QRegion>
#include <QTimer>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace bpr::qt {
	namespace {
		/// A process-wide monotonic clock in milliseconds.
		[[nodiscard]] auto monotonic_milliseconds() -> std::int64_t
		{
			static const auto started = [] {
				auto timer = QElapsedTimer{ };
				timer.start();
				return timer;
			}();
			return started.elapsed();
		}

		/// Row lengths, as fractions of the width, that read as a list of
		/// names rather than as a bar chart: no two neighbours alike.
		constexpr auto row_lengths  = std::array{ 0.62, 0.44, 0.71, 0.53, 0.38, 0.66, 0.49, 0.58 };
		constexpr auto text_lengths = std::array{ 0.94, 0.88, 0.97, 0.61, 0.0, 0.91, 0.83, 0.47 };
		/// Code: an indent in characters and a length, line by line.
		constexpr auto code_lines = std::array{
			std::pair{ 0, 0.42 },
			std::pair{ 4, 0.55 },
			std::pair{ 4, 0.31 },
			std::pair{ 8, 0.48 },
			std::pair{ 8, 0.62 },
			std::pair{ 4, 0.18 },
			std::pair{ 0, 0.08 },
			std::pair{ 0, 0.0 },
			std::pair{ 0, 0.36 },
			std::pair{ 4, 0.51 },
			std::pair{ 4, 0.27 },
			std::pair{ 0, 0.08 },
		};
		constexpr qreal sweep_step  = 0.04;
		constexpr qreal sweep_span  = 1.3;
		constexpr qreal sweep_width = 0.3;

		class pending_veil_accessible final : public QAccessibleWidget
		{
		public:
			explicit pending_veil_accessible(pending_veil* const aVeil)
				: QAccessibleWidget{ aVeil, QAccessible::StaticText }
			{
			}

			[[nodiscard]] auto state() const -> QAccessible::State override
			{
				auto state = QAccessibleWidget::state();
				state.busy = veil().busy();
				return state;
			}

			[[nodiscard]] auto text(const QAccessible::Text aKind) const -> QString override
			{
				const auto said = veil().shown_text();
				switch (aKind) {
				case QAccessible::Name:
					return said.isEmpty() ? veil().pane_name() : veil().pane_name() + QStringLiteral(": ") + said;
				case QAccessible::Description:
					return veil().busy() ? QStringLiteral("%1 is loading.").arg(veil().pane_name()) : said;
				default:
					return QAccessibleWidget::text(aKind);
				}
			}

		private:
			[[nodiscard]] auto veil() const -> const pending_veil&
			{
				return *static_cast<const pending_veil*>(object());
			}
		};

		auto pending_veil_accessible_factory(const QString& aKey, QObject* const aObject) -> QAccessibleInterface*
		{
			static_cast<void>(aKey);
			if (auto* const veil = dynamic_cast<pending_veil*>(aObject); veil != nullptr) {
				return new pending_veil_accessible{ veil };
			}
			return nullptr;
		}

		void install_pending_veil_accessibility()
		{
			static auto installed = false;
			if (!installed) {
				QAccessible::installFactory(&pending_veil_accessible_factory);
				installed = true;
			}
		}

		[[nodiscard]] auto failing(const core::pending_look aLook) noexcept -> bool
		{
			return aLook == core::pending_look::failed || aLook == core::pending_look::failed_retained;
		}

		[[nodiscard]] auto first_load(const core::pending_look aLook) noexcept -> bool
		{
			return aLook == core::pending_look::placeholder || aLook == core::pending_look::loading || aLook == core::pending_look::loading_slow;
		}
	} // namespace

	pending_veil::pending_veil(QWidget& aTarget, const pending_shape aShape, QString aName, QWidget* const aArea)
		: QWidget{ &aTarget }
		, Target{ &aTarget }
		, Area{ aArea }
		, Shape{ aShape }
		, Name{ std::move(aName) }
	{
		install_pending_veil_accessibility();
		setObjectName(QStringLiteral("pendingVeil"));
		setAccessibleName(Name);
		setFocusPolicy(Qt::NoFocus);
		setAttribute(Qt::WA_TransparentForMouseEvents, true);
		RetryButton = new QPushButton{ QStringLiteral("Retry"), this };
		RetryButton->setObjectName(QStringLiteral("pendingVeilRetry"));
		RetryButton->setAccessibleName(QStringLiteral("Retry"));
		RetryButton->setAccessibleDescription(QStringLiteral("Read %1 again.").arg(Name.toLower()));
		RetryButton->hide();
		connect(RetryButton, &QPushButton::clicked, this, [this] {
			if (Retry) {
				Retry();
			}
		});
		Wake = new QTimer{ this };
		Wake->setSingleShot(true);
		connect(Wake, &QTimer::timeout, this, [this] { advance(); });
		Frames = new QTimer{ this };
		Frames->setInterval(frame_interval);
		connect(Frames, &QTimer::timeout, this, [this] {
			++Tick;
			// The mark turns one step every other frame, as Home's does.
			Frame = (Tick / 2) % home_open_frames;
			update();
		});
		aTarget.installEventFilter(this);
		// The area moves when any widget between it and the pane moves (a
		// splitter's handle moves the area's parent, not the area), so the
		// veil follows each of them.
		for (auto* widget = Area.data(); widget != nullptr && widget != Target; widget = widget->parentWidget()) {
			widget->installEventFilter(this);
		}
		hide();
	}

	pending_veil::~pending_veil() = default;

	void pending_veil::present(const core::pending_activity aActivity, const bool aHasContent, const bool aPartial, const pending_words& aWords)
	{
		if (aActivity == core::pending_activity::loading && Activity != core::pending_activity::loading) {
			StartedAt = now();
		}
		Activity   = aActivity;
		HasContent = aHasContent;
		Partial    = aPartial;
		Words      = aWords;
		decide();
	}

	void pending_veil::present_ready()
	{
		present(core::pending_activity::idle, true, false, Words);
	}

	void pending_veil::restart()
	{
		StartedAt = now();
		decide();
	}

	void pending_veil::set_retry(std::function<void()> aRetry)
	{
		Retry = std::move(aRetry);
		layout_controls();
	}

	void pending_veil::set_full_motion(const bool aFull)
	{
		if (FullMotion == aFull) {
			return;
		}
		FullMotion = aFull;
		decide();
	}

	void pending_veil::set_high_contrast(const bool aHigh)
	{
		HighContrast = aHigh;
		update();
	}

	void pending_veil::set_failure_ink(const QColor& aInk)
	{
		FailureInk = aInk;
		update();
	}

	void pending_veil::set_announces(const bool aAnnounces)
	{
		Announces = aAnnounces;
	}

	void pending_veil::set_row_height(const int aHeight)
	{
		RowHeight = std::max(0, aHeight);
		update();
	}

	void pending_veil::set_placeholder_count(const int aCount)
	{
		Count = std::max(0, aCount);
		update();
	}

	void pending_veil::set_clock(std::function<std::int64_t()> aClock)
	{
		Clock     = std::move(aClock);
		StartedAt = now();
	}

	void pending_veil::advance()
	{
		decide();
	}

	auto pending_veil::look() const noexcept -> core::pending_look
	{
		return Look;
	}

	auto pending_veil::words() const noexcept -> const pending_words&
	{
		return Words;
	}

	auto pending_veil::shown_text() const -> QString
	{
		switch (Look) {
		case core::pending_look::loading:
		case core::pending_look::refreshing:
			return Words.Loading;
		case core::pending_look::loading_slow:
		case core::pending_look::refreshing_slow:
			return Words.Waiting.isEmpty() ? Words.Loading : Words.Waiting;
		case core::pending_look::partial:
			return Words.Coming;
		case core::pending_look::failed:
		case core::pending_look::failed_retained:
			return Words.Failure;
		case core::pending_look::ready:
		case core::pending_look::placeholder:
		case core::pending_look::quiet:
			return { };
		}
		return { };
	}

	auto pending_veil::busy() const noexcept -> bool
	{
		return core::pending_busy(Look);
	}

	auto pending_veil::retry_button() const noexcept -> QPushButton*
	{
		return RetryButton;
	}

	auto pending_veil::full_motion() const noexcept -> bool
	{
		return FullMotion;
	}

	auto pending_veil::frame() const noexcept -> int
	{
		return Frame;
	}

	auto pending_veil::sweep() const noexcept -> qreal
	{
		return std::fmod(static_cast<qreal>(Tick) * sweep_step, sweep_span);
	}

	auto pending_veil::placeholders() const noexcept -> int
	{
		return Painted;
	}

	auto pending_veil::pane_name() const -> QString
	{
		return Name;
	}

	auto pending_veil::plate_rect() const -> QRect
	{
		const auto line = line_height();
		switch (Look) {
		case core::pending_look::failed_retained: {
			const auto height = std::max(line + 14, RetryButton->sizeHint().height() + 8);
			return QRect{ 0, 0, width(), height };
		}
		case core::pending_look::partial: {
			const auto metrics = QFontMetrics{ font() };
			const auto text    = std::min(metrics.horizontalAdvance(Words.Coming), std::max(0, width() - (4 * inset) - line));
			const auto plate   = QRect{ inset, height() - line - 12 - 8, line + 8 + text + 20, line + 12 };
			return plate;
		}
		case core::pending_look::refreshing_slow: {
			const auto metrics = QFontMetrics{ font() };
			const auto text    = std::min(metrics.horizontalAdvance(shown_text()), std::max(0, width() - (4 * inset) - line));
			const auto wide    = line + 8 + text + 20;
			return QRect{ width() - inset - wide, rule_height + 6, wide, line + 12 };
		}
		default:
			return { };
		}
	}

	void pending_veil::decide()
	{
		const auto elapsed = Activity == core::pending_activity::loading ? std::max<std::int64_t>(0, now() - StartedAt) : 0;
		const auto facts   = core::pending_facts{
			.Activity            = Activity,
			.HasContent          = HasContent,
			.Partial             = Partial,
			.ElapsedMilliseconds = static_cast<std::uint32_t>(std::min<std::int64_t>(elapsed, 0xFFFF'FFFF)),
		};
		const auto before = Look;
		Look              = core::decide_pending_look(facts);
		const auto next   = core::pending_next_change_milliseconds(facts);
		if (next != 0U) {
			Wake->start(static_cast<int>(std::min<std::uint32_t>(next, 0x7FFF'FFFFU)));
		} else {
			Wake->stop();
		}
		// An area its pane is not showing (a folded description, another page
		// of a stack) has nothing to say over.
		const auto shown = Look != core::pending_look::ready && (Area == nullptr || Target == nullptr || Area->isVisibleTo(Target));
		// Only a pane with nothing of its own to use is blocked: a first load
		// or a failure in its place. Retained content stays usable, and a
		// failure strip takes only its own row.
		const auto blocks = first_load(Look) || Look == core::pending_look::failed;
		setAttribute(Qt::WA_TransparentForMouseEvents, !blocks && Look != core::pending_look::failed_retained);
		follow_area();
		layout_controls();
		if (Look == core::pending_look::failed_retained) {
			setMask(QRegion{ plate_rect() });
		} else {
			clearMask();
		}
		if (shown == isHidden()) {
			setVisible(shown);
		}
		if (shown) {
			raise();
		}
		const auto moving = shown && animated() && (core::pending_turning(Look) || core::pending_veiled(Look) || Look == core::pending_look::refreshing_slow);
		if (moving && !Frames->isActive()) {
			Frames->start();
		} else if (!moving) {
			Frames->stop();
		}
		if (before != Look) {
			auto state = QAccessible::State{ };
			state.busy = true;
			auto event = QAccessibleStateChangeEvent{ this, state };
			QAccessible::updateAccessibility(&event);
			announce(before, Look);
		}
		update();
	}

	void pending_veil::announce(const core::pending_look aBefore, const core::pending_look aAfter)
	{
		// A pane nobody can see says nothing: its wait is said where it shows.
		if (!Announces || Target == nullptr || !Target->isVisible()) {
			return;
		}
		auto said = QString{ };
		switch (core::pending_announcement_between(aBefore, aAfter)) {
		case core::pending_announcement::none:
			return;
		case core::pending_announcement::started:
			said = shown_text();
			break;
		case core::pending_announcement::slow:
			said = Words.Waiting;
			break;
		case core::pending_announcement::finished:
			said = Words.Done;
			break;
		case core::pending_announcement::failed:
			said = Retry ? Words.Failure + QStringLiteral(" Retry is available.") : Words.Failure;
			break;
		}
		if (said.isEmpty()) {
			return;
		}
		auto event = QAccessibleAnnouncementEvent{ Target, said };
		event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
		QAccessible::updateAccessibility(&event);
	}

	void pending_veil::follow_area()
	{
		if (Target == nullptr) {
			return;
		}
		auto area = Target->rect();
		if (Area != nullptr) {
			area = QRect{ Area->mapTo(Target, QPoint{ 0, 0 }), Area->size() };
		}
		if (geometry() != area) {
			setGeometry(area);
		}
	}

	void pending_veil::layout_controls()
	{
		const auto offered = failing(Look) && static_cast<bool>(Retry);
		RetryButton->setVisible(offered);
		if (!offered) {
			return;
		}
		const auto hint = RetryButton->sizeHint();
		if (Look == core::pending_look::failed_retained) {
			const auto strip = plate_rect();
			RetryButton->setGeometry(QRect{ QPoint{ strip.right() - inset - hint.width(), strip.top() + ((strip.height() - hint.height()) / 2) }, hint });
			return;
		}
		// Under the sentence, however many lines it wraps to.
		const auto line   = words_rect();
		const auto left   = line.left() + line.height() + 8;
		const auto said   = QFontMetrics{ font() }.boundingRect(QRect{ left, line.top(), std::max(1, line.right() - left), line.height() * 3 }, Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, Words.Failure);
		const auto lowest = std::max(line.bottom(), std::min(said.bottom(), line.top() + (line.height() * 3)));
		RetryButton->setGeometry(QRect{ QPoint{ left, lowest + 8 }, hint });
	}

	auto pending_veil::now() const -> std::int64_t
	{
		return Clock ? Clock() : monotonic_milliseconds();
	}

	auto pending_veil::ground() const -> QColor
	{
		auto* widget = Area != nullptr ? Area.data() : Target.data();
		if (widget == nullptr) {
			return palette().color(QPalette::Window);
		}
		if (auto* const scroll = qobject_cast<QAbstractScrollArea*>(widget); scroll != nullptr && scroll->viewport() != nullptr) {
			widget = scroll->viewport();
		}
		return widget->palette().color(widget->backgroundRole());
	}

	auto pending_veil::ink() const -> QColor
	{
		const auto& shades = palette();
		return ground() == shades.color(QPalette::Base) ? shades.color(QPalette::Text) : shades.color(QPalette::WindowText);
	}

	auto pending_veil::failure() const -> QColor
	{
		return FailureInk.isValid() ? FailureInk : palette().color(QPalette::WindowText);
	}

	auto pending_veil::line_height() const -> int
	{
		return QFontMetrics{ font() }.height();
	}

	auto pending_veil::words_rect() const -> QRect
	{
		const auto line = line_height();
		if (Shape == pending_shape::line) {
			return QRect{ 0, std::max(0, (height() - line) / 2), width(), line };
		}
		return QRect{ inset, inset, std::max(0, width() - (2 * inset)), line };
	}

	auto pending_veil::animated() const -> bool
	{
		return FullMotion;
	}

	void pending_veil::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		Painted = 0;
		if (Look == core::pending_look::ready || Look == core::pending_look::quiet) {
			return;
		}
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto whole = rect();
		if (first_load(Look) || Look == core::pending_look::failed) {
			// Nothing of the pane's own is worth reading: its ground, so stale
			// words under it ("No pull request") are not read as content.
			painter.fillRect(whole, ground());
			if (Look != core::pending_look::failed) {
				paint_placeholders(painter, whole);
			}
			paint_words(painter);
			return;
		}
		if (core::pending_veiled(Look)) {
			if (!HighContrast) {
				auto wash = ground();
				wash.setAlphaF(static_cast<float>(wash_strength));
				painter.fillRect(whole, wash);
			}
			paint_rule(painter);
		}
		paint_plate(painter);
	}

	void pending_veil::paint_placeholders(QPainter& aPainter, const QRect& aArea)
	{
		const auto line = line_height();
		// Two lines are kept for the words, the second for what a slow wait
		// still waits on, so the placeholders never move when it is said.
		const auto top      = Shape == pending_shape::line ? 0 : words_rect().bottom() + line + 2 + (line / 2);
		const auto left     = Shape == pending_shape::line ? 0 : inset;
		const auto width    = std::max(0, aArea.width() - left - (Shape == pending_shape::line ? 0 : inset));
		const auto bar      = std::max(4, (line * 11) / 20);
		const auto strength = HighContrast ? skeleton_contrast : skeleton_strength;
		const auto tint     = mix_colour(ink(), ground(), strength);
		aPainter.setPen(Qt::NoPen);
		aPainter.setBrush(tint);
		const auto radius = bar / 2.0;
		const auto draw   = [&](const qreal aLeft, const int aTop, const qreal aWidth) {
			if (aWidth <= 0.0 || aTop + bar > aArea.bottom()) {
				return false;
			}
			aPainter.drawRoundedRect(QRectF{ aLeft, static_cast<qreal>(aTop), aWidth, static_cast<qreal>(bar) }, radius, radius);
			++Painted;
			return true;
		};
		switch (Shape) {
		case pending_shape::block:
			return;
		case pending_shape::line: {
			if (Look == core::pending_look::placeholder) {
				draw(0.0, std::max(0, (aArea.height() - bar) / 2), width * 0.5);
			}
			return;
		}
		case pending_shape::rows: {
			const auto row = RowHeight > 0 ? RowHeight : (line * 3) / 2 + 4;
			auto       y   = top;
			for (auto index = std::size_t{ 0U }; y + row <= aArea.bottom() + row && (Count == 0 || std::cmp_less(index, Count)); ++index, y += row) {
				if (!draw(left, y + ((row - bar) / 2), width * row_lengths[index % row_lengths.size()])) {
					break;
				}
			}
			return;
		}
		case pending_shape::pairs: {
			// A name at the left and a short value at the right, row by row.
			const auto row = RowHeight > 0 ? RowHeight : line + 6;
			auto       y   = top;
			for (auto index = std::size_t{ 0U }; Count == 0 || std::cmp_less(index, Count); ++index, y += row) {
				const auto name  = width * (0.34 + (0.18 * row_lengths[index % row_lengths.size()]));
				const auto value = width * 0.12;
				if (!draw(left, y + ((row - bar) / 2), name)) {
					break;
				}
				draw(left + width - value, y + ((row - bar) / 2), value);
			}
			return;
		}
		case pending_shape::text: {
			const auto step = line + (line / 3);
			auto       y    = top;
			for (auto index = std::size_t{ 0U }; index < 24U; ++index, y += step) {
				const auto length = text_lengths[index % text_lengths.size()];
				if (length != 0.0 && !draw(left, y, width * length)) {
					break;
				}
			}
			return;
		}
		case pending_shape::code: {
			// A gutter of line numbers and code lines at their indents, so the
			// pane reads as the code it is waiting for.
			const auto step    = line + 2;
			const auto gutter  = QFontMetrics{ font() }.horizontalAdvance(QStringLiteral("0000"));
			const auto indent  = QFontMetrics{ font() }.horizontalAdvance(QLatin1Char{ ' ' });
			const auto columns = std::max(0, width - gutter - 12);
			auto       y       = top;
			for (auto index = std::size_t{ 0U }; y + bar <= aArea.bottom(); ++index, y += step) {
				const auto [depth, length] = code_lines[index % code_lines.size()];
				if (!draw(left, y, gutter * 0.6)) {
					break;
				}
				if (length != 0.0) {
					draw(left + gutter + 12 + (depth * indent), y, columns * length);
				}
			}
			return;
		}
		}
	}

	void pending_veil::paint_words(QPainter& aPainter)
	{
		const auto said = shown_text();
		const auto line = words_rect();
		if (said.isEmpty() || line.isEmpty()) {
			return;
		}
		const auto mark    = QRect{ line.left(), line.top(), line.height(), line.height() };
		const auto failure = failing(Look);
		if (failure) {
			draw_home_failure_mark(aPainter, mark, font(), this->failure());
		} else {
			auto faint = ink();
			faint.setAlphaF(0.25F);
			draw_home_open_mark(aPainter, mark, animated() ? Frame : 0, ink(), faint);
		}
		const auto textLeft = mark.right() + 8;
		auto       textRect = QRect{ textLeft, line.top(), std::max(0, line.right() - textLeft), line.height() };
		aPainter.setPen(failure ? palette().color(QPalette::WindowText) : palette().color(QPalette::PlaceholderText));
		const auto metrics = QFontMetrics{ font() };
		if (Shape == pending_shape::line) {
			aPainter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, metrics.elidedText(said, Qt::ElideRight, textRect.width()));
			return;
		}
		if (core::pending_slow(Look) && !Words.Waiting.isEmpty()) {
			// The first line keeps saying what is loading; the second says what
			// it still waits on, so neither moves the other.
			aPainter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, metrics.elidedText(Words.Loading, Qt::ElideRight, textRect.width()));
			textRect.translate(0, line.height() + 2);
			aPainter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, metrics.elidedText(Words.Waiting, Qt::ElideRight, textRect.width()));
			return;
		}
		// A failure in place of the content may wrap: it is the only thing said.
		if (failure) {
			const auto wrapped = QRect{ textRect.left(), textRect.top(), textRect.width(), line.height() * 3 };
			aPainter.drawText(wrapped, Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, said);
			return;
		}
		aPainter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, metrics.elidedText(said, Qt::ElideRight, textRect.width()));
	}

	void pending_veil::paint_rule(QPainter& aPainter)
	{
		const auto accent = palette().color(QPalette::Highlight);
		auto       track  = accent;
		track.setAlphaF(0.22F);
		aPainter.fillRect(QRect{ 0, 0, width(), rule_height }, track);
		if (animated()) {
			const auto at   = (sweep() - sweep_width) * width();
			const auto span = QRectF{ at, 0.0, sweep_width * width(), static_cast<qreal>(rule_height) };
			aPainter.fillRect(span.intersected(QRectF{ rect() }), accent);
			return;
		}
		// Still, under reduced motion: a dashed rule says "under way" without
		// moving, where a solid one would read as finished.
		auto pen = QPen{ accent, static_cast<qreal>(rule_height) };
		pen.setDashPattern({ 4.0, 3.0 });
		pen.setCapStyle(Qt::FlatCap);
		aPainter.setPen(pen);
		aPainter.drawLine(QPointF{ 0.0, rule_height / 2.0 }, QPointF{ static_cast<qreal>(width()), rule_height / 2.0 });
	}

	void pending_veil::paint_plate(QPainter& aPainter)
	{
		const auto plate = plate_rect();
		if (plate.isEmpty()) {
			return;
		}
		const auto failure = Look == core::pending_look::failed_retained;
		const auto base    = palette().color(QPalette::Window);
		const auto fill    = failure ? mix_colour(this->failure(), base, failure_strength) : mix_colour(palette().color(QPalette::WindowText), base, plate_strength);
		const auto edge    = failure ? mix_colour(this->failure(), base, 0.45) : mix_colour(palette().color(QPalette::WindowText), base, 0.2);
		aPainter.setPen(QPen{ edge, 1.0 });
		aPainter.setBrush(fill);
		const auto radius = failure ? 0.0 : 6.0;
		aPainter.drawRoundedRect(QRectF{ plate }.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
		const auto line = line_height();
		const auto mark = QRect{ plate.left() + 10, plate.top() + ((plate.height() - line) / 2), line, line };
		if (failure) {
			draw_home_failure_mark(aPainter, mark, font(), this->failure());
		} else {
			auto faint = palette().color(QPalette::WindowText);
			faint.setAlphaF(0.25F);
			draw_home_open_mark(aPainter, mark, animated() ? Frame : 0, palette().color(QPalette::WindowText), faint);
		}
		const auto right = failure && RetryButton->isVisible() ? RetryButton->geometry().left() - 8 : plate.right() - 10;
		const auto text  = QRect{ mark.right() + 8, plate.top(), std::max(0, right - mark.right() - 8), plate.height() };
		aPainter.setPen(palette().color(QPalette::WindowText));
		aPainter.drawText(text, Qt::AlignVCenter | Qt::AlignLeft, QFontMetrics{ font() }.elidedText(shown_text(), Qt::ElideRight, text.width()));
	}

	auto pending_veil::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
		if (aEvent != nullptr && aWatched != nullptr && aWatched->isWidgetType()) {
			switch (aEvent->type()) {
			case QEvent::Resize:
			case QEvent::Move:
			case QEvent::Show:
			case QEvent::LayoutRequest:
				follow_area();
				layout_controls();
				if (Look == core::pending_look::failed_retained) {
					setMask(QRegion{ plate_rect() });
				}
				break;
			case QEvent::ShowToParent:
			case QEvent::HideToParent:
				if (aWatched != Target && Look != core::pending_look::ready) {
					QTimer::singleShot(0, this, [this] { decide(); });
				}
				break;
			case QEvent::ChildAdded:
				// A child the pane adds later would be drawn over the veil.
				if (isVisible()) {
					QTimer::singleShot(0, this, [this] { raise(); });
				}
				break;
			default:
				break;
			}
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}

	void pending_veil::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent != nullptr && (aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::FontChange)) {
			layout_controls();
			update();
		}
	}

	pending_mark::pending_mark(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("pendingMark"));
		setFocusPolicy(Qt::NoFocus);
		setAttribute(Qt::WA_TransparentForMouseEvents, true);
		auto policy = sizePolicy();
		policy.setRetainSizeWhenHidden(true);
		policy.setHorizontalPolicy(QSizePolicy::Fixed);
		policy.setVerticalPolicy(QSizePolicy::Fixed);
		setSizePolicy(policy);
		Frames = new QTimer{ this };
		Frames->setInterval(80);
		connect(Frames, &QTimer::timeout, this, [this] {
			Frame = (Frame + 1) % home_open_frames;
			update();
		});
		hide();
	}

	void pending_mark::set_turning(const bool aTurning)
	{
		Turning = aTurning;
		follow();
	}

	void pending_mark::set_full_motion(const bool aFull)
	{
		FullMotion = aFull;
		follow();
	}

	auto pending_mark::turning() const noexcept -> bool
	{
		return Turning;
	}

	auto pending_mark::frame() const noexcept -> int
	{
		return Frame;
	}

	auto pending_mark::sizeHint() const -> QSize
	{
		const auto side = QFontMetrics{ font() }.height();
		return { side, side };
	}

	void pending_mark::follow()
	{
		setVisible(Turning);
		if (Turning && FullMotion) {
			Frames->start();
		} else {
			Frames->stop();
		}
		update();
	}

	void pending_mark::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto       painter = QPainter{ this };
		const auto side    = std::min(width(), height());
		auto       ink     = palette().color(foregroundRole());
		auto       ring    = ink;
		ring.setAlphaF(0.25F);
		draw_home_open_mark(painter, QRect{ (width() - side) / 2, (height() - side) / 2, side, side }, FullMotion ? Frame : 0, ink, ring);
	}

	void pending_mark::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent != nullptr && aEvent->type() == QEvent::FontChange) {
			updateGeometry();
		}
	}
} // namespace bpr::qt
