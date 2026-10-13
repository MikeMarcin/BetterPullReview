#include "bpr/application/window_review_restoration_host.hpp"
#include <algorithm>
#include <limits>
#include <utility>

namespace bpr::application {
	auto valid(const window_review_snapshot& aSnapshot) noexcept -> bool
	{
		return aSnapshot.Revision != 0 && core::valid_window_review_records(aSnapshot.Records) && core::valid_recent_reviews(aSnapshot.Recent);
	}
	namespace {
		auto translate(window_review_store_failure aFailure) -> core::window_review_failure
		{
			switch (aFailure) {
			case window_review_store_failure::cancelled:       return core::window_review_failure::cancelled;
			case window_review_store_failure::schema_mismatch: return core::window_review_failure::unsupported_schema;
			case window_review_store_failure::corrupt:         return core::window_review_failure::corrupt;
			case window_review_store_failure::conflict:        return core::window_review_failure::conflict;
			default:                                           return core::window_review_failure::unavailable;
			}
		}
	}
	struct window_review_restoration_host::implementation : std::enable_shared_from_this<implementation>
	{
		std::size_t                                Reserved;
		core::window_review_restoration            Policy;
		window_review_restoration_port&            Store;
		effect_executor&                           Effects;
		decision_executor&                         Decision;
		window_review_restoration_projection_port& Projection;
		std::optional<window_review_snapshot>      Accepted;
		std::optional<window_review_snapshot>      ConflictCurrent;
		struct job
		{
			std::uint64_t                         Request;
			std::uint64_t                         Version;
			std::optional<window_review_snapshot> Candidate;
			std::stop_source                      Stop;
		};
		std::optional<job> Active;
		struct closing_window
		{
			core::bpr_window_context Context;
			std::uint64_t            Version;
		};
		std::vector<closing_window> Closed;
		std::uint64_t               NextRequest    = 1;
		std::uint64_t               Version        = 0;
		bool                        Started        = false;
		bool                        Closing        = false;
		bool                        Orderly        = false;
		bool                        Blocked        = false;
		bool                        DeliveryFailed = false;

		implementation(core::bpr_profile_id aProfile, std::vector<core::bpr_window_context> aContexts, std::vector<std::string> aConfiguredHosts,
					   window_review_restoration_port& aStore, effect_executor& aEffects, decision_executor& aDecision, window_review_restoration_projection_port& aProjection)
			: Reserved{ aContexts.size() }
			, Policy{ aProfile, std::move(aContexts), std::move(aConfiguredHosts) }
			, Store{ aStore }
			, Effects{ aEffects }
			, Decision{ aDecision }
			, Projection{ aProjection }
		{ }
		void release_closed()
		{
			Reserved -= Closed.size();
			Closed.clear();
		}

		void fail(core::window_review_failure aReason)
		{
			if (Closing && !Orderly) {
				return;
			}
			try {
				Projection.fail({ { }, aReason });
			} catch (...) {
				DeliveryFailed = true;
			}
		}
		void notify_recent()
		{
			if (Closing) {
				return;
			}
			try {
				Projection.recent_changed();
			} catch (...) {
				DeliveryFailed = true;
			}
		}
		void deliver(const core::window_review_actions& aActions)
		{
			const auto lifetime = shared_from_this();
			if (Closing) {
				return;
			}
			for (const auto& error : aActions.Failures) {
				if (Closing) {
					return;
				}
				try {
					Projection.fail(error);
				} catch (...) {
					DeliveryFailed = true;
				}
			}
			if (aActions.Startup && !Closing && Policy.current_startup(aActions.Startup->Request)) {
				try {
					Projection.select_startup(*aActions.Startup);
				} catch (...) {
					DeliveryFailed = true;
					Policy.cancel_startup();
					fail(core::window_review_failure::unavailable);
				}
			}
			for (const auto& open : aActions.Opens) {
				if (Closing || !Policy.current_restore(open.Context, open.Request)) {
					continue;
				}
				try {
					Projection.open_review(open);
				} catch (...) {
					DeliveryFailed = true;
					deliver(Policy.complete_restore(open.Context, open.Request, std::unexpected(core::window_review_failure::unavailable)));
				}
			}
		}
		void pump()
		{
			if (Active || Blocked || !Accepted || (Closing && !Orderly)) {
				return;
			}
			if (Accepted->Records == Policy.records() && Accepted->Recent == Policy.recent()) {
				release_closed();
				return;
			}
			if (Accepted->Revision == std::numeric_limits<std::uint64_t>::max()) {
				Blocked = true;
				release_closed();
				fail(core::window_review_failure::identity_exhausted);
				return;
			}
			submit(window_review_snapshot{ Accepted->Revision + 1, Policy.records(), Policy.recent() });
		}
		void finish(std::uint64_t aRequest, window_review_store_result<window_review_snapshot> aResult)
		{
			if (!Active || Active->Request != aRequest) {
				return;
			}
			const auto job = std::exchange(Active, std::nullopt).value();
			if (Closing && !Orderly) {
				release_closed();
				return;
			}
			if (job.Stop.stop_requested()) {
				aResult = std::unexpected(window_review_store_error{ window_review_store_failure::cancelled, { } });
			}
			if (aResult && (!valid(*aResult) || (job.Candidate && *aResult != *job.Candidate))) {
				aResult = std::unexpected(window_review_store_error{ window_review_store_failure::corrupt, { } });
			}
			if (!aResult) {
				Blocked         = true;
				ConflictCurrent = aResult.error().Current && valid(*aResult.error().Current) ? aResult.error().Current : std::nullopt;
				release_closed();
				fail(translate(aResult.error().Failure));
				return;
			}
			Accepted = std::move(*aResult);
			ConflictCurrent.reset();
			Blocked = false;
			if (job.Candidate) {
				Reserved -= std::erase_if(Closed, [&job](const auto& aClosed) { return aClosed.Version <= job.Version; });
			} else {
				const auto actions = Policy.load(Accepted->Records, Accepted->Recent);
				deliver(actions);
				notify_recent();
			}
			pump();
		}
		void submit(std::optional<window_review_snapshot> aCandidate)
		{
			if (Active || (Closing && !Orderly)) {
				return;
			}
			if (NextRequest == std::numeric_limits<std::uint64_t>::max()) {
				Blocked = true;
				release_closed();
				fail(core::window_review_failure::identity_exhausted);
				return;
			}
			Active             = job{ NextRequest++, Version, aCandidate, { } };
			const auto request = Active->Request;
			const auto stop    = Active->Stop.get_token();
			const auto self    = shared_from_this();
			try {
				Effects.post([self, request, stop, candidate = std::move(aCandidate)] {
					auto result = window_review_store_result<window_review_snapshot>{ std::unexpected(window_review_store_error{ window_review_store_failure::unavailable, { } }) };
					try {
						if (stop.stop_requested()) {
							result = std::unexpected(window_review_store_error{ window_review_store_failure::cancelled, { } });
						} else {
							result = candidate ? self->Store.save(*candidate, candidate->Revision - 1, stop) : self->Store.load(stop);
						}
					} catch (...) { }
					self->Decision.post([self, request, completion = std::move(result)]() mutable { self->finish(request, std::move(completion)); });
				});
			} catch (...) {
				finish(request, std::unexpected(window_review_store_error{ window_review_store_failure::unavailable, { } }));
			}
		}
		void changed()
		{
			if (Version == std::numeric_limits<std::uint64_t>::max()) {
				Blocked = true;
				fail(core::window_review_failure::identity_exhausted);
				return;
			}
			++Version;
			pump();
		}
		void close(core::bpr_window_context aContext)
		{
			if (Closing || !Policy.close(aContext)) {
				return;
			}
			if (Version == std::numeric_limits<std::uint64_t>::max()) {
				Blocked = true;
				fail(core::window_review_failure::identity_exhausted);
				return;
			}
			++Version;
			if (!Blocked) {
				Closed.push_back({ aContext, Version });
			} else {
				--Reserved;
			}
			pump();
		}
		void shutdown(bool aOrderly)
		{
			if (Closing && (!Orderly || aOrderly)) {
				return;
			}
			Closing = true;
			Orderly = aOrderly;
			Policy.cancel_startup();
			if (!aOrderly) {
				if (Active) {
					Active->Stop.request_stop();
				}
				release_closed();
			} else {
				pump();
			}
		}
	};
	window_review_restoration_host::window_review_restoration_host(core::bpr_profile_id aProfile, std::vector<core::bpr_window_context> aContexts,
																   std::vector<std::string> aConfiguredHosts, window_review_restoration_port& aStore, effect_executor& aEffects,
																   decision_executor& aDecision, window_review_restoration_projection_port& aProjection)
		: Implementation{ std::make_shared<implementation>(aProfile, std::move(aContexts), std::move(aConfiguredHosts), aStore, aEffects, aDecision, aProjection) }
	{ }
	window_review_restoration_host::~window_review_restoration_host() { Implementation->shutdown(false); }
	void window_review_restoration_host::start()
	{
		auto& host = *Implementation;
		if (!host.Closing && !host.Started) {
			host.Started = true;
			host.submit({ });
		}
	}
	auto window_review_restoration_host::attach(core::bpr_window_context aContext) -> bool
	{
		auto& host = *Implementation;
		if (host.Closing || host.Reserved >= core::maximum_bpr_windows_per_profile || !host.Policy.attach(aContext)) {
			return false;
		}
		++host.Reserved;
		return true;
	}
	void window_review_restoration_host::mark_window_ready(core::bpr_window_context aContext)
	{
		if (!Implementation->Closing) {
			Implementation->deliver(Implementation->Policy.mark_window_ready(aContext));
		}
	}
	void window_review_restoration_host::cancel_restore(core::bpr_window_context aContext) { Implementation->Policy.cancel_restore(aContext); }
	void window_review_restoration_host::cancel_startup() { Implementation->Policy.cancel_startup(); }
	auto window_review_restoration_host::current_startup(core::request_id aRequest) const noexcept -> bool { return !Implementation->Closing && Implementation->Policy.current_startup(aRequest); }
	void window_review_restoration_host::complete_startup(core::request_id aRequest, std::expected<core::window_review_authority, core::window_review_failure> aResult)
	{
		if (!Implementation->Closing) {
			Implementation->deliver(Implementation->Policy.complete_startup(aRequest, aResult));
		}
	}
	auto window_review_restoration_host::current_restore(core::bpr_window_context aContext, core::request_id aRequest) const noexcept -> bool { return !Implementation->Closing && Implementation->Policy.current_restore(aContext, aRequest); }
	void window_review_restoration_host::complete_restore(core::bpr_window_context aContext, core::request_id aRequest, std::expected<void, core::window_review_failure> aResult)
	{
		if (!Implementation->Closing) {
			Implementation->deliver(Implementation->Policy.complete_restore(aContext, aRequest, aResult));
		}
	}
	void window_review_restoration_host::accepted_read(core::bpr_window_context aContext, const core::window_review_destination& aDestination, std::int64_t aViewedAt)
	{
		if (!Implementation->Closing && Implementation->Policy.accepted_read(aContext, aDestination, aViewedAt)) {
			Implementation->changed();
			Implementation->notify_recent();
		}
	}
	void window_review_restoration_host::accepted_title(const core::window_review_destination& aDestination, std::string_view aTitle)
	{
		if (!Implementation->Closing && Implementation->Policy.accepted_title(aDestination, aTitle)) {
			Implementation->changed();
			Implementation->notify_recent();
		}
	}
	void window_review_restoration_host::accepted_draft(const core::window_review_destination& aDestination, std::optional<bool> aDraft)
	{
		if (!Implementation->Closing && Implementation->Policy.accepted_draft(aDestination, aDraft)) {
			Implementation->changed();
			Implementation->notify_recent();
		}
	}

	void window_review_restoration_host::open_recent(core::bpr_window_context aContext, std::size_t aIndex)
	{
		if (!Implementation->Closing) {
			Implementation->deliver(Implementation->Policy.open_recent(aContext, aIndex));
		}
	}
	void window_review_restoration_host::forget_recent(const core::window_review_destination& aDestination)
	{
		if (!Implementation->Closing && Implementation->Policy.forget_recent(aDestination)) {
			Implementation->changed();
			Implementation->notify_recent();
		}
	}
	auto window_review_restoration_host::recent() const noexcept -> const std::vector<core::window_recent_review>& { return Implementation->Policy.recent(); }
	void window_review_restoration_host::close(core::bpr_window_context aContext) { Implementation->close(aContext); }
	void window_review_restoration_host::accepted_position(core::bpr_window_context aContext, const core::window_review_destination& aDestination, const core::window_review_position& aPosition)
	{
		if (!Implementation->Closing && Implementation->Policy.accepted_position(aContext, aDestination, aPosition)) {
			Implementation->changed();
			Implementation->notify_recent();
		}
	}
	void window_review_restoration_host::accepted_tabs(core::bpr_window_context aContext, const core::window_review_destination& aDestination, const core::window_review_tabs& aTabs)
	{
		if (!Implementation->Closing && Implementation->Policy.accepted_tabs(aContext, aDestination, aTabs)) {
			Implementation->changed();
			Implementation->notify_recent();
		}
	}
	auto window_review_restoration_host::window_quiescent(core::bpr_window_context aContext) const noexcept -> bool
	{
		return !Implementation->Policy.current(aContext) && std::ranges::none_of(Implementation->Closed, [aContext](const auto& aClosed) { return aClosed.Context == aContext; });
	}
	void window_review_restoration_host::retry_persistence()
	{
		auto& host = *Implementation;
		if (!host.Closing && !host.Active) {
			host.Blocked = false;
			host.Started = true;
			host.submit({ });
		}
	}
	void window_review_restoration_host::shutdown_orderly() { Implementation->shutdown(true); }
	void window_review_restoration_host::shutdown() { Implementation->shutdown(false); }
	auto window_review_restoration_host::is_quiescent() const noexcept -> bool
	{
		const auto& host = *Implementation;
		return host.Closing && !host.Active && (host.Blocked || !host.Accepted || host.Accepted->Records == host.Policy.records() || !host.Orderly);
	}
	auto window_review_restoration_host::projection_delivery_failed() const noexcept -> bool { return Implementation->DeliveryFailed; }
	auto window_review_restoration_host::durable() const noexcept -> bool
	{
		const auto& state = *Implementation;
		return !state.Active && !state.Blocked && !state.DeliveryFailed && state.Accepted
			&& state.Accepted->Records == state.Policy.records() && state.Accepted->Recent == state.Policy.recent();
	}
}
