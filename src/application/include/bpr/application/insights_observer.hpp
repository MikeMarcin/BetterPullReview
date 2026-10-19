#ifndef BPR_APPLICATION_INSIGHTS_OBSERVER_HPP
#define BPR_APPLICATION_INSIGHTS_OBSERVER_HPP
#include "bpr/core/insights.hpp"
#include <memory>
namespace bpr::application {
	/// \brief Passive numeric accounting and private activity identity. Collection failure never changes an operation.
	class insights_observer
	{
	public:
		virtual ~insights_observer()                                            = default;
		virtual void               record(core::insights_event aEvent) noexcept = 0;
		virtual void               select(core::insights_scope aScope) noexcept = 0;
		[[nodiscard]] virtual auto foreground() const noexcept -> bool { return false; }
		virtual void               record_work(core::insights_work_sample) noexcept { }
		virtual void               record_energy(core::insights_energy_sample) noexcept { }
		virtual void               record_review(core::insights_review_event) noexcept { }
	};
	struct insights_child_capture
	{
		std::shared_ptr<insights_observer> Observer;
		bool                               Foreground = false;
		std::size_t                        Kind       = 0;
		void                               record(std::optional<std::uint64_t> aCpuMicroseconds) const noexcept;
	};
	[[nodiscard]] auto capture_insights_child(std::shared_ptr<insights_observer> aObserver, std::size_t aKind) -> insights_child_capture;
	/// \brief Host conversion to local Gregorian day, including the offset in force at that instant.
	[[nodiscard]] auto local_insights_day(std::int64_t aUnixSeconds) -> std::int64_t;
	/// \brief Captured exact PR facts; recording cannot change the observed operation.
	struct insights_review_capture
	{
		std::shared_ptr<insights_observer> Observer;
		core::insights_review_event        Context;
		void                               record_new(core::insights_review_kind aKind) const noexcept;
		void                               record(core::insights_review_kind aKind, std::string aIdentity, std::uint64_t aLines = 0, bool aSuggestion = false, std::uint8_t aDisposition = 2) const noexcept;
	};
}
#endif
