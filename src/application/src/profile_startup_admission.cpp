#include "bpr/application/profile_startup_admission.hpp"

#include <algorithm>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

namespace bpr::application {
	namespace {
		enum class startup_phase : std::uint8_t { idle,
												  registry,
												  root };
		[[nodiscard]] auto registry_failure(const profile_registry_failure aFailure) -> profile_startup_failure
		{
			switch (aFailure) {
			case profile_registry_failure::cancelled:       return profile_startup_failure::cancelled;
			case profile_registry_failure::schema_mismatch:
			case profile_registry_failure::corrupt:         return profile_startup_failure::invalid_registry;
			default:                                        return profile_startup_failure::registry_unavailable;
			}
		}
	}

	struct profile_startup_host::implementation : std::enable_shared_from_this<implementation>
	{
		profile_registry_port&           Registry;
		profile_startup_root_port&       Roots;
		effect_executor&                 Effects;
		decision_executor&               Decision;
		profile_startup_projection_port* Projection;
		core::application_session_id     Session;
		profile_startup_context          Context;
		std::uint64_t                    LastRequest = 0U;
		std::stop_source                 Stop;
		startup_phase                    Phase          = startup_phase::idle;
		bool                             Closing        = false;
		bool                             DeliveryFailed = false;

		implementation(profile_registry_port& aRegistry, profile_startup_root_port& aRoots,
					   effect_executor& aEffects, decision_executor& aDecision,
					   profile_startup_projection_port& aProjection, core::application_session_id aSession)
			: Registry{ aRegistry }
			, Roots{ aRoots }
			, Effects{ aEffects }
			, Decision{ aDecision }
			, Projection{ &aProjection }
			, Session{ aSession }
		{ }

		[[nodiscard]] auto matches(const profile_startup_context aContext, const startup_phase aPhase) const -> bool
		{
			return Context == aContext && Phase == aPhase;
		}

		void finish(const profile_startup_context aContext, profile_startup_result<profile_startup_admission> aResult)
		{
			if (Context != aContext || Phase == startup_phase::idle) {
				return;
			}
			Phase = startup_phase::idle;
			if (Stop.stop_requested() || Closing) {
				aResult = std::unexpected(profile_startup_failure::cancelled);
			}
			if (Projection == nullptr) {
				return;
			}
			try {
				Projection->publish({ aContext, std::move(aResult) });
			} catch (...) {
				DeliveryFailed = true;
			}
		}

		void root_completed(const profile_startup_context aContext, profile_storage_authority aAuthority,
							std::string aLabel, profile_startup_result<std::filesystem::path> aResult)
		{
			if (!matches(aContext, startup_phase::root)) {
				return;
			}
			if (!aResult) {
				finish(aContext, std::unexpected(aResult.error()));
				return;
			}
			if (aResult->empty() || !aResult->is_absolute() || *aResult != aResult->lexically_normal()) {
				finish(aContext, std::unexpected(profile_startup_failure::root_unavailable));
				return;
			}
			finish(aContext, profile_startup_admission{ std::move(aAuthority), std::move(aLabel), std::move(*aResult) });
		}

		void resolve(const profile_startup_context aContext, profile_storage_authority aAuthority, std::string aLabel)
		{
			Phase           = startup_phase::root;
			const auto self = shared_from_this();
			const auto stop = Stop.get_token();
			try {
				Effects.post([self, aContext, authority = std::move(aAuthority), label = std::move(aLabel), stop] {
					auto result = profile_startup_result<std::filesystem::path>{ std::unexpected(profile_startup_failure::root_unavailable) };
					try {
						result = stop.stop_requested() ? profile_startup_result<std::filesystem::path>{ std::unexpected(profile_startup_failure::cancelled) } : self->Roots.resolve(authority, stop);
					} catch (...) {
						result = std::unexpected(profile_startup_failure::root_unavailable);
					}
					self->Decision.post([self, aContext, authority, label, completed = std::move(result)]() mutable {
						self->root_completed(aContext, std::move(authority), std::move(label), std::move(completed));
					});
				});
			} catch (...) {
				Decision.post([self, aContext] {
					if (self->matches(aContext, startup_phase::root)) {
						self->finish(aContext, std::unexpected(profile_startup_failure::root_unavailable));
					}
				});
			}
		}

		void registry_completed(const profile_startup_context aContext, profile_registry_result<profile_registry_snapshot> aResult)
		{
			if (!matches(aContext, startup_phase::registry)) {
				return;
			}
			if (Stop.stop_requested() || Closing) {
				finish(aContext, std::unexpected(profile_startup_failure::cancelled));
				return;
			}
			if (!aResult) {
				finish(aContext, std::unexpected(registry_failure(aResult.error())));
				return;
			}
			const auto authority = profile_storage_authority::select(*aResult, aContext.Profile);
			if (!authority) {
				finish(aContext, std::unexpected(authority.error() == profile_storage_authority_failure::unknown_profile ? profile_startup_failure::unknown_profile : profile_startup_failure::invalid_registry));
				return;
			}
			const auto entry = std::ranges::find_if(aResult->Profiles, [aContext](const auto& aEntry) { return aEntry.Profile.Id == aContext.Profile; });
			resolve(aContext, *authority, entry->Profile.Label);
		}

		[[nodiscard]] auto start(const core::bpr_profile_id aProfile) -> profile_startup_result<core::request_id>
		{
			if (Closing) {
				return std::unexpected(profile_startup_failure::closing);
			}
			if (Phase != startup_phase::idle) {
				return std::unexpected(profile_startup_failure::busy);
			}
			if (LastRequest == std::numeric_limits<std::uint64_t>::max()) {
				return std::unexpected(profile_startup_failure::identity_exhausted);
			}
			Stop               = std::stop_source{ };
			Context            = { Session, core::request_id{ ++LastRequest }, aProfile };
			Phase              = startup_phase::registry;
			const auto context = Context;
			const auto self    = shared_from_this();
			const auto stop    = Stop.get_token();
			try {
				Effects.post([self, context, stop] {
					auto result = profile_registry_result<profile_registry_snapshot>{ std::unexpected(profile_registry_failure::unavailable) };
					try {
						result = stop.stop_requested() ? profile_registry_result<profile_registry_snapshot>{ std::unexpected(profile_registry_failure::cancelled) } : self->Registry.load(stop);
					} catch (...) {
						result = std::unexpected(profile_registry_failure::unavailable);
					}
					self->Decision.post([self, context, completed = std::move(result)]() mutable { self->registry_completed(context, std::move(completed)); });
				});
			} catch (...) {
				Decision.post([self, context] {
					if (self->matches(context, startup_phase::registry)) {
						self->finish(context, std::unexpected(profile_startup_failure::registry_unavailable));
					}
				});
			}
			return context.Request;
		}
	};

	profile_startup_host::profile_startup_host(profile_registry_port& aRegistry, profile_startup_root_port& aRoots,
											   effect_executor& aEffects, decision_executor& aDecision,
											   profile_startup_projection_port& aProjection, core::application_session_id aSession)
		: Implementation{ std::make_shared<implementation>(aRegistry, aRoots, aEffects, aDecision, aProjection, aSession) }
	{
		if (aSession.value() == 0U) {
			throw std::invalid_argument("profile startup requires a nonzero session identity");
		}
	}
	profile_startup_host::~profile_startup_host()
	{
		shutdown();
		Implementation->Projection = nullptr;
	}
	auto profile_startup_host::start(const core::bpr_profile_id aProfile) -> profile_startup_result<core::request_id> { return Implementation->start(aProfile); }
	void profile_startup_host::cancel() { Implementation->Stop.request_stop(); }
	void profile_startup_host::shutdown()
	{
		Implementation->Closing = true;
		cancel();
	}
	auto profile_startup_host::is_quiescent() const noexcept -> bool { return Implementation->Phase == startup_phase::idle; }
	auto profile_startup_host::projection_delivery_failed() const noexcept -> bool { return Implementation->DeliveryFailed; }
}
