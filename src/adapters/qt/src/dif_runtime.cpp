#include "bpr/qt/dif_runtime.hpp"

#include <QApplication>
#include <QBuffer>
#include <QCryptographicHash>
#include <QEvent>
#include <QImageReader>
#include <QPainter>

#include <algorithm>
#include <limits>

namespace bpr::qt {
	namespace {
		constexpr std::uint64_t mib = 1024 * 1024;
		// Conservative allowance for opaque Qt image data and allocator bookkeeping.
		constexpr std::uint64_t overhead = 16 * 1024;
		auto                    valid_clip(const dif_clip& aClip) -> bool
		{
			return aClip.Frames > 0 && aClip.Frames <= 128 && aClip.Cell > 0 && aClip.Cell <= 1024
				&& aClip.Gutter >= 0 && aClip.Gutter <= 32 && aClip.Return < 10 && aClip.Sha256.size() == 32
				&& ((aClip.Playback == dif_playback::still && aClip.Frames == 1 && aClip.FrameMs == 0)
					|| ((aClip.Playback == dif_playback::loop || aClip.Playback == dif_playback::once)
						&& aClip.FrameMs >= 84 && aClip.FrameMs <= 60'000));
		}
		// Reject metadata with independent decompression costs before the PNG reader.
		auto bounded_png(const QByteArray& aBytes) -> bool
		{
			if (aBytes.size() < 33 || aBytes.left(8) != QByteArray::fromHex("89504e470d0a1a0a")
				|| static_cast<unsigned char>(aBytes[24]) != 8 || static_cast<unsigned char>(aBytes[25]) != 6) {
				return false;
			}
			qsizetype offset = 8;
			while (offset <= aBytes.size() - 12) {
				std::uint32_t length = 0;
				for (qsizetype byte = 0; byte < 4; ++byte) {
					length = (length << 8) | static_cast<unsigned char>(aBytes[offset + byte]);
				}
				if (length > static_cast<std::uint64_t>(aBytes.size() - offset - 12)) {
					return false;
				}
				const auto kind = aBytes.sliced(offset + 4, 4);
				if (kind != "IHDR" && kind != "IDAT" && kind != "IEND" && kind != "sRGB"
					&& kind != "gAMA" && kind != "cHRM" && kind != "pHYs") {
					return false;
				}
				offset += static_cast<qsizetype>(length) + 12;
				if (kind == "IEND") {
					return length == 0 && offset == aBytes.size();
				}
			}
			return false;
		}

		auto transparent_gutters(const QImage& aImage, const dif_clip& aClip) -> bool
		{
			for (int x = 0; x < aImage.width(); ++x) {
				if (x % (aClip.Cell + aClip.Gutter) < aClip.Cell) {
					continue;
				}
				for (int y = 0; y < aImage.height(); ++y) {
					if (qAlpha(aImage.pixel(x, y)) != 0) {
						return false;
					}
				}
			}
			return true;
		}

	}
	auto dif_cache::create(const descriptors& aClips, const strip_loader& aLoad) -> std::shared_ptr<const dif_cache>
	{
		if (!aLoad || !std::ranges::all_of(aClips, valid_clip) || aClips[0].Playback != dif_playback::still) {
			return { };
		}
		auto cache   = std::shared_ptr<dif_cache>{ new dif_cache };
		cache->Clips = aClips;
		for (auto& clip : cache->Clips) {
			clip.Sha256 = QByteArray{ clip.Sha256.constData(), clip.Sha256.size() };
		}
		for (std::size_t index = 0; index < aClips.size(); ++index) {
			const auto& clip = aClips[index];
			if (aClips[clip.Return].Playback == dif_playback::once) {
				return { };
			}
			auto bytes = aLoad(index);
			if (!bounded_png(bytes) || bytes.size() > 8 * static_cast<qsizetype>(mib)
				|| QCryptographicHash::hash(bytes, QCryptographicHash::Sha256) != clip.Sha256) {
				return { };
			}
			QBuffer buffer{ &bytes };
			buffer.open(QIODevice::ReadOnly);
			QImageReader reader{ &buffer, "PNG" };
			const QSize  size{ clip.Frames * clip.Cell + (clip.Frames - 1) * clip.Gutter, clip.Cell };
			const auto   pixels = static_cast<std::uint64_t>(size.width()) * static_cast<std::uint64_t>(size.height()) * 4;
			const auto   peak   = cache->Pixels + pixels * 2 + static_cast<std::uint64_t>(bytes.capacity()) + sizeof(dif_cache) + overhead;
			if (reader.size() != size || cache->Pixels + pixels > 24 * mib || peak >= 40 * mib) {
				return { };
			}
			auto decoded = reader.read();
			if (decoded.isNull() || decoded.size() != size || decoded.depth() > 32 || !transparent_gutters(decoded, clip)) {
				return { };
			}
			auto retained = decoded.convertToFormat(QImage::Format_ARGB32_Premultiplied);
			if (retained.isNull()) {
				return { };
			}
			cache->Images[index] = std::move(retained);
			cache->Pixels += static_cast<std::uint64_t>(cache->Images[index].sizeInBytes());
			cache->Peak = std::max(cache->Peak, peak);
		}
		if (cache->retained_bytes() > 32 * mib) {
			return { };
		}
		return cache;
	}
	auto dif_cache::clip(std::size_t aIndex) const -> const dif_clip& { return Clips.at(aIndex); }
	auto dif_cache::image(std::size_t aIndex) const -> const QImage& { return Images.at(aIndex); }
	auto dif_cache::source(std::size_t aClip, int aFrame) const -> QRect
	{
		if (aClip >= Clips.size() || aFrame < 0 || aFrame >= Clips[aClip].Frames) {
			return { };
		}
		const auto& clip = Clips[aClip];
		return { aFrame * (clip.Cell + clip.Gutter), 0, clip.Cell, clip.Cell };
	}
	auto dif_cache::pixel_bytes() const -> std::uint64_t { return Pixels; }
	auto dif_cache::retained_bytes() const -> std::uint64_t { return Pixels + sizeof(dif_cache) + overhead; }
	auto dif_cache::conversion_peak_bytes() const -> std::uint64_t { return Peak; }

	dif_sequencer::dif_sequencer(std::shared_ptr<const dif_cache> aCache)
		: Cache{ std::move(aCache) }
	{ }
	void dif_sequencer::reset()
	{
		Current = 0;
		Started = 0;
		Last    = 0;
	}
	auto dif_sequencer::start(std::size_t aClip, std::int64_t aNow) -> bool
	{
		if (!Cache || aClip >= 10 || aNow < 0 || aNow < Last) {
			reset();
			return false;
		}
		Current = aClip;
		Started = aNow;
		Last    = aNow;
		return true;
	}
	auto dif_sequencer::sample(std::int64_t aNow) -> dif_frame
	{
		if (!Cache || aNow < 0 || aNow < Last) {
			reset();
			return { };
		}
		Last             = aNow;
		const auto& clip = Cache->clip(Current);
		if (clip.Playback == dif_playback::still) {
			return { Current, 0, { } };
		}
		const auto elapsed  = aNow - Started;
		const auto duration = static_cast<std::int64_t>(clip.Frames) * clip.FrameMs;
		if (clip.Playback == dif_playback::once && elapsed >= duration) {
			Current             = clip.Return;
			Started             = aNow;
			const auto& neutral = Cache->clip(Current);
			return { Current, 0, neutral.FrameMs ? std::optional<int>{ neutral.FrameMs } : std::nullopt };
		}
		return { Current, static_cast<int>((elapsed / clip.FrameMs) % clip.Frames), clip.FrameMs - static_cast<int>(elapsed % clip.FrameMs) };
	}

	dif_widget::dif_widget(std::shared_ptr<const dif_cache> aCache, QWidget* aParent)
		: QWidget{ aParent }
		, Cache{ std::move(aCache) }
		, Sequencer{ Cache }
	{
		setFocusPolicy(Qt::NoFocus);
		setAttribute(Qt::WA_TransparentForMouseEvents);
		setAccessibleName(QStringLiteral("Dif, BetterPullReview mascot"));
		Timer.setParent(this);
		Timer.setSingleShot(true);
		Timer.setTimerType(Qt::PreciseTimer);
		Clock.start();
		connect(&Timer, &QTimer::timeout, this, [this] { tick(); });
		qApp->installEventFilter(this);
	}
	dif_widget::~dif_widget() { shutdown(); }
	void dif_widget::shutdown()
	{
		Stopped = true;
		Timer.stop();
		disconnect(&Timer, nullptr, this, nullptr);
		qApp->removeEventFilter(this);
	}
	auto dif_widget::eligible() const -> bool
	{
		return Cache && !Stopped && isVisible() && Eligibility.Shown && Eligibility.FullMotion
			&& Eligibility.Active && !Eligibility.Modal && !Eligibility.Suspended
			&& !window()->isMinimized() && !QApplication::activeModalWidget();
	}
	void dif_widget::cancel()
	{
		Timer.stop();
		Sequencer.reset();
		if (Frame.Clip != 0 || Frame.Frame != 0) {
			Frame = { };
			update();
		}
	}
	void dif_widget::set_eligibility(dif_eligibility aEligibility)
	{
		const auto changedVisibility = Eligibility.Shown != aEligibility.Shown;
		Eligibility                  = aEligibility;
		if (!eligible()) {
			cancel();
		}
		if (changedVisibility) {
			update();
		}
	}
	auto dif_widget::play(std::size_t aClip) -> bool
	{
		if (!eligible()) {
			cancel();
			return false;
		}
		if (!Sequencer.start(aClip, Clock.elapsed())) {
			cancel();
			return false;
		}
		tick();
		return true;
	}
	void dif_widget::tick()
	{
		if (!eligible()) {
			cancel();
			return;
		}
		const auto next = Sequencer.sample(Clock.elapsed());
		if (next.Clip != Frame.Clip || next.Frame != Frame.Frame) {
			Frame = next;
			update();
		}
		Timer.stop();
		if (next.NextMs) {
			Timer.start(std::max(84, *next.NextMs));
		}
	}
	void dif_widget::paintEvent(QPaintEvent*)
	{
		if (!Cache || !Eligibility.Shown || Stopped) {
			return;
		}
		QPainter painter{ this };
		painter.drawImage(rect(), Cache->image(Frame.Clip), Cache->source(Frame.Clip, Frame.Frame));
	}
	auto dif_widget::event(QEvent* aEvent) -> bool
	{
		if (aEvent->type() == QEvent::Hide || aEvent->type() == QEvent::WindowDeactivate
			|| aEvent->type() == QEvent::WindowBlocked) {
			cancel();
		}
		return QWidget::event(aEvent);
	}
	auto dif_widget::eventFilter(QObject* aObject, QEvent* aEvent) -> bool
	{
		if ((aObject == window() && (aEvent->type() == QEvent::WindowDeactivate || aEvent->type() == QEvent::WindowBlocked || aEvent->type() == QEvent::Hide || aEvent->type() == QEvent::WindowStateChange))
			|| aEvent->type() == QEvent::ApplicationDeactivate) {
			Eligibility.Active = false;
			cancel();
		}
		return QWidget::eventFilter(aObject, aEvent);
	}
}
