#include "bpr/application/profile_registry.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>

namespace bpr::application {
	namespace {
		[[nodiscard]] auto catalog_of(const profile_registry_snapshot& aSnapshot)
			-> std::expected<core::profile_window_catalog, core::profile_window_failure>
		{
			auto state = core::profile_window_state{ };
			state.Profiles.clear();
			state.LastProfileId = aSnapshot.LastProfileId;
			state.LastWindowId  = aSnapshot.LastWindowId;
			for (const auto& entry : aSnapshot.Profiles) {
				state.Profiles.push_back({ entry.Profile, { }, entry.RetainedWindows });
			}
			return core::profile_window_catalog::restore(std::move(state));
		}

		[[nodiscard]] auto translate(const core::profile_window_failure aFailure) -> profile_registry_failure
		{
			switch (aFailure) {
			case core::profile_window_failure::invalid_label:      return profile_registry_failure::invalid_label;
			case core::profile_window_failure::unknown_profile:    return profile_registry_failure::unknown_profile;
			case core::profile_window_failure::profile_limit:      return profile_registry_failure::profile_limit;
			case core::profile_window_failure::identity_exhausted: return profile_registry_failure::identity_exhausted;
			default:                                               return profile_registry_failure::corrupt;
			}
		}
	}

	auto profile_root_identity(const core::bpr_profile_id aProfile) -> std::string
	{
		return aProfile == core::default_bpr_profile_id ? "legacy" : "profile-" + std::to_string(aProfile.value());
	}

	auto valid(const profile_registry_snapshot& aSnapshot) -> bool
	{
		if (aSnapshot.Revision == 0U || !aSnapshot.DefaultAdopted
			|| aSnapshot.Profiles.empty() || aSnapshot.Profiles.size() > core::maximum_bpr_profiles) {
			return false;
		}
		std::uint64_t previous = 0U;
		for (const auto& entry : aSnapshot.Profiles) {
			const auto mode = entry.Profile.Id == core::default_bpr_profile_id ? profile_root_mode::legacy : profile_root_mode::isolated;
			if (entry.Profile.Id.value() <= previous || entry.RootMode != mode
				|| entry.RootIdentity != profile_root_identity(entry.Profile.Id)) {
				return false;
			}
			previous = entry.Profile.Id.value();
		}
		return catalog_of(aSnapshot).has_value();
	}

	auto valid_profile_registry_update(const profile_registry_snapshot& aBefore, const profile_registry_snapshot& aAfter) -> bool
	{
		if (!valid(aBefore) || !valid(aAfter)
			|| aBefore.Revision == std::numeric_limits<std::uint64_t>::max()
			|| aAfter.Revision != aBefore.Revision + 1U || aAfter.Profiles.size() < aBefore.Profiles.size()) {
			return false;
		}
		for (std::size_t index = 0U; index < aBefore.Profiles.size(); ++index) {
			if (aBefore.Profiles[index].Profile.Id != aAfter.Profiles[index].Profile.Id) {
				return false;
			}
		}
		if (aAfter.LastWindowId < aBefore.LastWindowId) {
			return false;
		}
		std::map<core::bpr_window_id, std::pair<core::bpr_profile_id, core::bpr_window_generation>> previousWindows;
		for (const auto& entry : aBefore.Profiles) {
			for (const auto& window : entry.RetainedWindows) {
				previousWindows.emplace(window.Id, std::pair{ entry.Profile.Id, window.Generation });
			}
		}
		for (const auto& entry : aAfter.Profiles) {
			for (const auto& window : entry.RetainedWindows) {
				const auto previous = previousWindows.find(window.Id);
				if (previous == previousWindows.end()) {
					if (window.Id.value() <= aBefore.LastWindowId) {
						return false;
					}
				} else if (previous->second.first != entry.Profile.Id || window.Generation < previous->second.second) {
					return false;
				}
			}
		}
		if (aAfter.Profiles.size() == aBefore.Profiles.size()) {
			return aAfter.LastProfileId == aBefore.LastProfileId;
		}
		return aAfter.Profiles[aBefore.Profiles.size()].Profile.Id.value() > aBefore.LastProfileId
			&& aAfter.LastProfileId == aAfter.Profiles.back().Profile.Id.value();
	}

	struct profile_registry_host::implementation : std::enable_shared_from_this<implementation>
	{
		profile_registry_port&                   Store;
		effect_executor&                         Effects;
		decision_executor&                       Decision;
		profile_registry_projection_port&        Projection;
		core::application_session_id             Session;
		std::optional<profile_registry_snapshot> Accepted;
		std::stop_source                         Stop;
		std::uint64_t                            LastRequest    = 0U;
		bool                                     Pending        = false;
		bool                                     Closing        = false;
		bool                                     DeliveryFailed = false;

		implementation(profile_registry_port& aStore, effect_executor& aEffects,
					   decision_executor& aDecision, profile_registry_projection_port& aProjection,
					   core::application_session_id aSession)
			: Store{ aStore }
			, Effects{ aEffects }
			, Decision{ aDecision }
			, Projection{ aProjection }
			, Session{ aSession }
		{ }

		[[nodiscard]] auto ready() const -> profile_registry_result<void>
		{
			if (Closing) {
				return std::unexpected(profile_registry_failure::closing);
			}
			if (Pending) {
				return std::unexpected(profile_registry_failure::busy);
			}
			if (LastRequest == std::numeric_limits<std::uint64_t>::max()) {
				return std::unexpected(profile_registry_failure::identity_exhausted);
			}
			return { };
		}

		void finish(const core::request_id aRequest, const std::optional<core::bpr_profile_id> aProfile,
					const std::optional<profile_registry_snapshot>& aCandidate, profile_registry_result<profile_registry_snapshot> aResult)
		{
			if (!Pending || aRequest.value() != LastRequest) {
				return;
			}
			Pending = false;
			if (Closing) {
				return;
			}
			if (Stop.stop_requested()) {
				aResult = std::unexpected(profile_registry_failure::cancelled);
			} else if (aResult && (!valid(*aResult) || (aCandidate && *aCandidate != *aResult))) {
				aResult = std::unexpected(profile_registry_failure::corrupt);
			}
			auto publication = profile_registry_publication{ Session, aRequest, aProfile, { } };
			if (aResult) {
				Accepted = std::move(*aResult);
			} else {
				Accepted.reset();
				publication.Failure = aResult.error();
			}
			try {
				Projection.publish(publication);
			} catch (...) {
				DeliveryFailed = true;
			}
		}

		[[nodiscard]] auto submit(std::optional<profile_registry_snapshot>  aCandidate,
								  const std::optional<core::bpr_profile_id> aProfile) -> profile_registry_result<void>
		{
			if (auto admitted = ready(); !admitted) {
				return admitted;
			}
			Stop               = std::stop_source{ };
			Pending            = true;
			const auto request = core::request_id{ ++LastRequest };
			const auto stop    = Stop.get_token();
			const auto self    = shared_from_this();
			try {
				Effects.post([self, request, stop, candidate = aCandidate, aProfile] {
					auto result = profile_registry_result<profile_registry_snapshot>{ std::unexpected(profile_registry_failure::unavailable) };
					try {
						if (stop.stop_requested()) {
							result = std::unexpected(profile_registry_failure::cancelled);
						} else {
							result = candidate ? self->Store.save(*candidate, candidate->Revision - 1U, stop) : self->Store.load(stop);
						}
					} catch (...) {
						result = std::unexpected(profile_registry_failure::unavailable);
					}
					self->Decision.post([self, request, candidate, aProfile, completion = std::move(result)]() mutable {
						self->finish(request, aProfile, candidate, std::move(completion));
					});
				});
			} catch (...) {
				finish(request, aProfile, aCandidate, std::unexpected(profile_registry_failure::unavailable));
			}
			return { };
		}

		[[nodiscard]] auto edit(const std::optional<core::bpr_profile_id> aProfile, const std::string_view aLabel) -> profile_registry_result<void>
		{
			if (auto admitted = ready(); !admitted) {
				return admitted;
			}
			if (!Accepted) {
				return std::unexpected(profile_registry_failure::not_loaded);
			}
			if (Accepted->Revision == std::numeric_limits<std::uint64_t>::max()) {
				return std::unexpected(profile_registry_failure::identity_exhausted);
			}
			auto catalog = catalog_of(*Accepted);
			if (!catalog) {
				return std::unexpected(profile_registry_failure::corrupt);
			}
			auto id = aProfile;
			if (aProfile) {
				const auto renamed = catalog->rename_profile(*aProfile, aLabel);
				if (!renamed) {
					return std::unexpected(translate(renamed.error()));
				}
			} else {
				const auto created = catalog->create_profile(aLabel);
				if (!created) {
					return std::unexpected(translate(created.error()));
				}
				id = *created;
			}
			auto candidate = *Accepted;
			candidate.Revision += 1U;
			candidate.LastProfileId = catalog->state().LastProfileId;
			candidate.LastWindowId  = catalog->state().LastWindowId;
			candidate.Profiles.clear();
			for (const auto& profile : catalog->state().Profiles) {
				candidate.Profiles.push_back({ profile.Profile,
											   profile.Profile.Id == core::default_bpr_profile_id ? profile_root_mode::legacy : profile_root_mode::isolated,
											   profile_root_identity(profile.Profile.Id), profile.Retained });
			}
			return submit(std::move(candidate), id);
		}
	};

	profile_registry_host::profile_registry_host(profile_registry_port& aStore, effect_executor& aEffects,
												 decision_executor& aDecision, profile_registry_projection_port& aProjection, core::application_session_id aSession)
		: Implementation{ std::make_shared<implementation>(aStore, aEffects, aDecision, aProjection, aSession) }
	{
		if (aSession.value() == 0U) {
			throw std::invalid_argument("profile registry requires a nonzero session identity");
		}
	}
	profile_registry_host::~profile_registry_host() { shutdown(); }
	auto profile_registry_host::reload() -> profile_registry_result<void> { return Implementation->submit({ }, { }); }
	auto profile_registry_host::create(const std::string_view aLabel) -> profile_registry_result<void> { return Implementation->edit({ }, aLabel); }
	auto profile_registry_host::rename(const core::bpr_profile_id aProfile, const std::string_view aLabel) -> profile_registry_result<void> { return Implementation->edit(aProfile, aLabel); }
	void profile_registry_host::cancel()
	{
		Implementation->Stop.request_stop();
		Implementation->Accepted.reset();
	}
	void profile_registry_host::shutdown()
	{
		Implementation->Closing = true;
		cancel();
	}
	auto profile_registry_host::accepted() const noexcept -> const std::optional<profile_registry_snapshot>& { return Implementation->Accepted; }
	auto profile_registry_host::is_quiescent() const noexcept -> bool { return !Implementation->Pending; }
	auto profile_registry_host::projection_delivery_failed() const noexcept -> bool { return Implementation->DeliveryFailed; }
}
