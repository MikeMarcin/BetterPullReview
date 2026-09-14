#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_MOTION_OBSERVER_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_MOTION_OBSERVER_HPP

#include "bpr/core/appearance_model.hpp"

#include <QObject>
#include <QPointer>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace bpr::qt {
	/// A read-only platform edge. The callback announces that a fresh read is needed.
	class motion_observation_source
	{
	public:
		virtual ~motion_observation_source()                                          = default;
		[[nodiscard]] virtual auto read() noexcept -> core::motion_observation        = 0;
		virtual void               set_change_handler(std::function<void()> aHandler) = 0;
	};

	/// One process-owned observation, fanned out to attachment-local resolution.
	class motion_observer final : public QObject
	{
	public:
		using sink = std::function<void(core::motion_observation)>;

		explicit motion_observer(
			std::unique_ptr<motion_observation_source> aSource,
			QObject*                                   aParent = nullptr);
		~motion_observer() override;
		motion_observer(const motion_observer&)                    = delete;
		auto operator=(const motion_observer&) -> motion_observer& = delete;

		/// Delivers the current fact immediately; zero means admission failed.
		[[nodiscard]] auto subscribe(QObject& aLifetime, sink aSink) -> std::uint64_t;
		void               unsubscribe(std::uint64_t aId) noexcept;
		[[nodiscard]] auto current() const noexcept -> core::motion_observation;
		void               refresh();

	private:
		struct listener
		{
			std::uint64_t     Id;
			QPointer<QObject> Lifetime;
			sink              Publish;
		};
		std::unique_ptr<motion_observation_source> Source;
		core::motion_observation                   Current = core::motion_observation::failed;
		std::vector<listener>                      Listeners;
		std::uint64_t                              NextId = 1U;
	};

	/// macOS uses AppKit on Cocoa; other selected Qt platforms say unsupported.
	[[nodiscard]] auto make_native_motion_source()
		-> std::unique_ptr<motion_observation_source>;
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_MOTION_OBSERVER_HPP
