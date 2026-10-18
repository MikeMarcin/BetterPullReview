#include "bpr/application/insights_host.hpp"

#include <utility>

namespace bpr::application {
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
		if (Pending || !Storage || (Events.empty() && !Selected)) {
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
		std::vector<core::insights_event>      events;
		std::optional<core::insights_scope>    selected;
		std::shared_ptr<insights_storage_port> storage;
		{
			std::lock_guard lock{ Mutex };
			events.swap(Events);
			selected = Selected;
			Selected.reset();
			storage = Storage;
		}
		try {
			storage->record(events, selected);
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
				const auto built  = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
				auto       github = core::insights_github_snapshot{ };
				try {
					github = self->Storage->github(built);
				} catch (...) { }
				self->Decisions.post([self, measure, now, built, githubSnapshot = std::move(github), measuredSnapshot = std::move(snapshot), work]() mutable {
					self->Pending = false;
					if (self->Closing) {
						return;
					}
					if (measure) {
						self->Snapshot                    = std::move(measuredSnapshot);
						self->Snapshot.StorageUnixSeconds = built;
						self->LastStorage                 = now;
					}
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
