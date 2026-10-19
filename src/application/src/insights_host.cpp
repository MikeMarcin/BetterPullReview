#include "bpr/application/insights_host.hpp"

#include <ctime>
#include <random>
#include <utility>

namespace bpr::application {
	auto local_insights_day(const std::int64_t aUnixSeconds) -> std::int64_t
	{
		const auto timestamp = static_cast<std::time_t>(aUnixSeconds);
		std::tm    local{ };
#ifdef _WIN32
		if (localtime_s(&local, &timestamp) != 0) {
			throw std::runtime_error{ "Local date unavailable" };
		}
#else
		if (!localtime_r(&timestamp, &local)) {
			throw std::runtime_error{ "Local date unavailable" };
		}
#endif
		return std::chrono::sys_days{ std::chrono::year{ local.tm_year + 1900 } / std::chrono::month{ static_cast<unsigned>(local.tm_mon + 1) } / std::chrono::day{ static_cast<unsigned>(local.tm_mday) } }.time_since_epoch().count();
	}
	namespace {
		auto new_insights_activity_identity() -> std::string
		{
			std::random_device entropy;
			return std::to_string(entropy()) + ":" + std::to_string(entropy()) + ":" + std::to_string(entropy()) + ":" + std::to_string(entropy());
		}

	}
	void insights_review_capture::record_new(core::insights_review_kind aKind) const noexcept
	{
		try {
			if (Observer) {
				record(aKind, new_insights_activity_identity());
			}
		} catch (...) { }
	}

	void insights_review_capture::record(const core::insights_review_kind aKind, std::string aIdentity, const std::uint64_t aLines, const bool aSuggestion, const std::uint8_t aDisposition) const noexcept
	{
		try {
			if (!Observer) {
				return;
			}
			auto event        = Context;
			event.Kind        = aKind;
			event.Identity    = std::move(aIdentity);
			event.Lines       = aLines;
			event.Suggestion  = aSuggestion;
			event.Disposition = aDisposition;
			Observer->record_review(std::move(event));
		} catch (...) { }
	}

	namespace {
		class problem_log final : public diagnostic_log_port, public diagnostic_control_port
		{
		public:
			problem_log(std::shared_ptr<diagnostic_log_port> aLog, std::shared_ptr<insights_observer> aObserver)
				: Log{ std::move(aLog) }
				, Observer{ std::move(aObserver) }
				, Control{ std::dynamic_pointer_cast<diagnostic_control_port>(Log) }
			{ }
			auto record(const diagnostic_event& aEvent) noexcept -> diagnostic_log_status override
			{
				if (aEvent.Outcome == diagnostic_outcome::failed) {
					core::insights_event event;
					event.Kind = core::insights_event_kind::problem;
					switch (aEvent.Operation) {
					case diagnostic_operation::pull_request:      event.Category = core::insights_category::opening; break;
					case diagnostic_operation::review_comment:
					case diagnostic_operation::review_submission: event.Category = core::insights_category::publishing; break;
					default:                                      event.Category = core::insights_category::other; break;
					}
					switch (aEvent.PagedOperation) {
					case diagnostic_paged_operation::search_pull_requests:       event.Category = core::insights_category::inbox; break;
					case diagnostic_paged_operation::list_reviews:
					case diagnostic_paged_operation::list_review_threads:
					case diagnostic_paged_operation::list_issue_comments:
					case diagnostic_paged_operation::list_review_comments:
					case diagnostic_paged_operation::list_pull_request_timeline: event.Category = core::insights_category::conversations; break;
					default:                                                     break;
					}
					Observer->record(event);
				}
				return Log ? Log->record(aEvent) : diagnostic_log_status::unavailable;
			}
			auto status() const noexcept -> diagnostic_log_status override { return Log ? Log->status() : diagnostic_log_status::unavailable; }
			void discard_context() noexcept override
			{
				if (Log) {
					Log->discard_context();
				}
			}
			auto context_generation() const noexcept -> std::uint64_t override { return Log ? Log->context_generation() : 0; }
			auto configure(diagnostic_preferences aPreferences) noexcept -> bool override { return Control && Control->configure(aPreferences); }
			auto clear() noexcept -> diagnostic_clear_result override { return Control ? Control->clear() : diagnostic_clear_result::unavailable; }
			auto capture_state() -> diagnostic_capture_state override { return Control ? Control->capture_state() : diagnostic_capture_state{ }; }
			auto recent_context() -> std::vector<std::string> override { return Control ? Control->recent_context() : std::vector<std::string>{ }; }
			auto flush_for_export(bool aRecent) -> std::optional<diagnostic_export_capture> override { return Control ? Control->flush_for_export(aRecent) : std::nullopt; }

		private:
			std::shared_ptr<diagnostic_log_port>     Log;
			std::shared_ptr<insights_observer>       Observer;
			std::shared_ptr<diagnostic_control_port> Control;
		};
	}
	auto observe_insights_problems(std::shared_ptr<diagnostic_log_port> aLog, std::shared_ptr<insights_observer> aObserver) -> std::shared_ptr<diagnostic_log_port>
	{
		return std::make_shared<problem_log>(std::move(aLog), std::move(aObserver));
	}

	auto capture_insights_child(std::shared_ptr<insights_observer> aObserver, const std::size_t aKind) -> insights_child_capture
	{
		const auto foreground = aObserver && aObserver->foreground();
		return { std::move(aObserver), foreground, aKind };
	}
	void insights_child_capture::record(const std::optional<std::uint64_t> aCpuMicroseconds) const noexcept
	{
		try {
			if (!Observer || Kind >= 2) {
				return;
			}
			core::insights_energy_sample sample;
			sample.AtMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			sample.LocalDay       = local_insights_day(sample.AtMicroseconds / 1000000);
			sample.Foreground     = Foreground;
			sample.Children[Kind] = 1;
			if (aCpuMicroseconds) {
				sample.ChildCpuMicroseconds[Kind] = *aCpuMicroseconds;
			} else {
				sample.UnmeasuredChildren[Kind] = 1;
			}
			Observer->record_energy(sample);
		} catch (...) { }
	}

	insights_recorder::insights_recorder(effect_executor& aEffects)
		: Effects{ aEffects }
	{ }
	void insights_recorder::attach(std::shared_ptr<insights_storage_port> aStorage)
	{
		std::lock_guard lock{ Mutex };
		Storage = std::move(aStorage);
		schedule();
	}
	void insights_recorder::schedule()
	{
		if (Pending || !Storage || (Events.empty() && ReviewEvents.empty() && EnergySamples.empty() && !Selected)) {
			return;
		}
		Pending = true;
		try {
			Effects.post([self = shared_from_this()] { self->drain(); });
		} catch (...) {
			Pending = false;
		}
	}
	void insights_recorder::record(core::insights_event aEvent) noexcept
	{
		try {
			if (aEvent.UnixSeconds == 0) {
				aEvent.UnixSeconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			}
			std::lock_guard lock{ Mutex };
			if (Events.size() < 4096) {
				Events.push_back(std::move(aEvent));
			}
			schedule();
		} catch (...) { }
	}
	void insights_recorder::record_review(core::insights_review_event aEvent) noexcept
	{
		try {
			if (aEvent.UnixSeconds == 0) {
				aEvent.UnixSeconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
				aEvent.LocalDay    = local_insights_day(aEvent.UnixSeconds);
			}
			std::lock_guard lock{ Mutex };
			ReviewEvents.push_back(std::move(aEvent));
			schedule();
		} catch (...) { }
	}

	void insights_recorder::record_energy(core::insights_energy_sample aSample) noexcept
	{
		try {
			std::lock_guard lock{ Mutex };
			if (EnergySamples.size() < 4096) {
				EnergySamples.push_back(std::move(aSample));
			}
			schedule();
		} catch (...) { }
	}
	insights_energy_sampler::insights_energy_sampler(std::shared_ptr<process_metrics_port> aMetrics, std::shared_ptr<insights_recorder> aRecorder, effect_executor& aEffects)
		: Metrics{ std::move(aMetrics) }
		, Recorder{ std::move(aRecorder) }
		, Effects{ aEffects }
	{ }
	void insights_energy_sampler::observe(const bool aForeground)
	{
		if (Closing.load()) {
			return;
		}
		// Launches capture the current activation state immediately, independently of sampling latency.
		Recorder->set_foreground(aForeground);
		try {
			Effects.post([self = shared_from_this(), aForeground] { self->collect(aForeground); });
		} catch (...) { }
	}
	void insights_energy_sampler::collect(const bool aForeground)
	{
		std::optional<core::insights_process_counters> current;
		try {
			current = Metrics->read();
		} catch (...) { }
		if (current && Previous) {
			if (auto delta = core::insights_energy_delta(*Previous, *current, WasForeground, local_insights_day(current->UnixMicroseconds / 1000000))) {
				Recorder->record_energy(std::move(*delta));
			}
		}
		Previous      = current;
		WasForeground = aForeground;
	}
	void insights_energy_sampler::shutdown() noexcept
	{
		if (Closing.exchange(true)) {
			return;
		}
		try {
			Effects.post([self = shared_from_this()] { self->collect(self->WasForeground); });
		} catch (...) { }
	}

	void insights_recorder::select(core::insights_scope aScope) noexcept
	{
		try {
			std::lock_guard lock{ Mutex };
			Selected = aScope;
			schedule();
		} catch (...) { }
	}
	void insights_recorder::drain()
	{
		std::vector<core::insights_event>         events;
		std::vector<core::insights_energy_sample> energy;
		std::vector<core::insights_review_event>  reviews;
		std::optional<core::insights_scope>       selected;
		std::shared_ptr<insights_storage_port>    storage;
		{
			std::lock_guard lock{ Mutex };
			events.swap(Events);
			reviews.swap(ReviewEvents);
			energy.swap(EnergySamples);
			selected = Selected;
			Selected.reset();
			storage = Storage;
		}
		try {
			if (!events.empty() || selected) {
				storage->record(events, selected);
			}
			storage->record_reviews(reviews);
			storage->record_energy(energy);
		} catch (...) { }
		std::lock_guard lock{ Mutex };
		Pending = false;
		schedule();
	}

	insights_host::insights_host(std::shared_ptr<insights_storage_port> aStorage, effect_executor& aEffects, decision_executor& aDecisions,
								 std::function<void(const insights_snapshot&)> aPublish)
		: Storage{ std::move(aStorage) }
		, Effects{ aEffects }
		, Decisions{ aDecisions }
		, Publish{ std::move(aPublish) }
	{ }

	void insights_host::refresh(const bool aStorageVisible, std::string aBackgroundWork)
	{
		if (Closing || Pending) {
			return;
		}
		const auto now     = std::chrono::steady_clock::now();
		const auto measure = aStorageVisible && (!LastStorage || now - *LastStorage >= std::chrono::seconds{ 60 });
		Pending            = true;
		try {
			Effects.post([self = shared_from_this(), measure, now, work = std::move(aBackgroundWork)] {
				auto snapshot = insights_snapshot{ };
				if (measure) {
					try {
						snapshot = self->Storage->measure();
					} catch (...) { }
				}
				const auto built   = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
				auto       github  = core::insights_github_snapshot{ };
				auto       reviews = core::insights_reviews_snapshot{ };
				auto       energy  = core::insights_energy_snapshot{ };
				try {
					github  = self->Storage->github(built);
					reviews = self->Storage->reviews(built);
					energy  = self->Storage->energy(built);
				} catch (...) { }
				self->Decisions.post([self, measure, now, built, energySnapshot = std::move(energy), reviewsSnapshot = std::move(reviews), githubSnapshot = std::move(github), measuredSnapshot = std::move(snapshot), work]() mutable {
					self->Pending = false;
					if (self->Closing) {
						return;
					}
					if (measure) {
						self->Snapshot                    = std::move(measuredSnapshot);
						self->Snapshot.StorageUnixSeconds = built;
						self->LastStorage                 = now;
					}
					self->Snapshot.Energy           = std::move(energySnapshot);
					self->Snapshot.Reviews          = std::move(reviewsSnapshot);
					self->Snapshot.Github           = std::move(githubSnapshot);
					self->Snapshot.BuiltUnixSeconds = built;
					self->Snapshot.BackgroundWork   = work;
					self->Publish(self->Snapshot);
				});
			});
		} catch (...) {
			Pending = false;
		}
	}

	void insights_host::shutdown() noexcept { Closing = true; }
	auto insights_host::quiescent() const noexcept -> bool { return !Pending; }
}
