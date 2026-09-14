#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QImage>
#include <QRect>
#include <QTimer>
#include <QWidget>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace bpr::qt {
	enum class dif_playback { still,
							  loop,
							  once };

	/// \brief Validated strip geometry and timing, independent of production asset names.
	struct dif_clip final
	{
		int          Frames{ 1 };
		int          Cell{ 256 };
		int          Gutter{ 0 };
		int          FrameMs{ 0 };
		dif_playback Playback{ dif_playback::still };
		std::size_t  Return{ 0 };
		QByteArray   Sha256;
	};

	/// \brief Immutable, explicitly shared catalog; creation decodes one injected PNG at a time.
	/// \details Index zero is validated static rest. Sources are RGBA8 PNG without compressed
	/// metadata. The composition-owned loader supplies bounded bytes and does not throw.
	/// Retained/peak accounting includes a conservative opaque Qt/allocator allowance;
	/// backend-owned paint caches require separate native measurement.
	class dif_cache final
	{
	public:
		using descriptors  = std::array<dif_clip, 10>;
		using strip_loader = std::function<QByteArray(std::size_t)>;
		/// \brief Refuses malformed descriptors, digest/geometry mismatch, or memory excess.
		static auto create(const descriptors& aClips, const strip_loader& aLoad) -> std::shared_ptr<const dif_cache>;
		auto        clip(std::size_t aIndex) const -> const dif_clip&;
		auto        image(std::size_t aIndex) const -> const QImage&;
		auto        source(std::size_t aClip, int aFrame) const -> QRect;
		auto        pixel_bytes() const -> std::uint64_t;
		auto        retained_bytes() const -> std::uint64_t;
		auto        conversion_peak_bytes() const -> std::uint64_t;

	private:
		descriptors            Clips;
		std::array<QImage, 10> Images;
		std::uint64_t          Pixels{ 0 };
		std::uint64_t          Peak{ 0 };
	};

	/// \brief One sampled frame and a relative deadline; absence means no wakeup.
	struct dif_frame final
	{
		std::size_t        Clip{ 0 };
		int                Frame{ 0 };
		std::optional<int> NextMs;
	};

	/// \brief Pure monotonic elapsed-time playback; invalid time cancels to static rest.
	class dif_sequencer final
	{
	public:
		explicit dif_sequencer(std::shared_ptr<const dif_cache> aCache);
		auto start(std::size_t aClip, std::int64_t aNow) -> bool;
		auto sample(std::int64_t aNow) -> dif_frame;
		void reset();

	private:
		std::shared_ptr<const dif_cache> Cache;
		std::size_t                      Current{ 0 };
		std::int64_t                     Started{ 0 };
		std::int64_t                     Last{ 0 };
	};

	/// \brief Presentation eligibility supplied by the owning window, in addition to Qt exposure.
	struct dif_eligibility final
	{
		bool Shown{ true };
		bool FullMotion{ false };
		bool Active{ false };
		bool Modal{ false };
		bool Suspended{ false };
	};

	/// \brief Decorative retained sprite with one single-shot timer and no per-frame image work.
	class dif_widget final : public QWidget
	{
	public:
		explicit dif_widget(std::shared_ptr<const dif_cache> aCache, QWidget* aParent = nullptr);
		~dif_widget() override;
		void set_eligibility(dif_eligibility aEligibility);
		auto play(std::size_t aClip) -> bool;
		void shutdown();

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		auto event(QEvent* aEvent) -> bool override;
		auto eventFilter(QObject* aObject, QEvent* aEvent) -> bool override;

	private:
		auto                             eligible() const -> bool;
		void                             cancel();
		void                             tick();
		std::shared_ptr<const dif_cache> Cache;
		dif_sequencer                    Sequencer;
		QTimer                           Timer;
		QElapsedTimer                    Clock;
		dif_frame                        Frame;
		dif_eligibility                  Eligibility;
		bool                             Stopped{ false };
	};
}
