#include "bpr/application/profile_management_host.hpp"

#include <algorithm>
#include <optional>
#include <type_traits>
#include <utility>

namespace bpr::application {
	namespace {
		[[nodiscard]] auto translate(const profile_registry_failure aFailure) -> profile_management_failure
		{
			switch (aFailure) {
			case profile_registry_failure::invalid_label:      return profile_management_failure::invalid_label;
			case profile_registry_failure::unknown_profile:    return profile_management_failure::unknown_profile;
			case profile_registry_failure::profile_limit:      return profile_management_failure::profile_limit;
			case profile_registry_failure::identity_exhausted: return profile_management_failure::identity_exhausted;
			case profile_registry_failure::busy:               return profile_management_failure::busy;
			case profile_registry_failure::closing:            return profile_management_failure::closing;
			case profile_registry_failure::cancelled:          return profile_management_failure::cancelled;
			case profile_registry_failure::conflict:           return profile_management_failure::conflict;
			case profile_registry_failure::schema_mismatch:
			case profile_registry_failure::corrupt:            return profile_management_failure::invalid_registry;
			default:                                           return profile_management_failure::registry_unavailable;
			}
		}
		struct runtime_slot
		{
			core::bpr_window_context   Context;
			profile_management_runtime State;
			bool                       StartupPending = true;
		};
	}

	struct profile_management_host::implementation final : profile_registry_projection_port
	{
		profile_management_projection_port&         Projection;
		core::profile_window_catalog                Catalog;
		std::vector<runtime_slot>                   Slots;
		bool                                        Ready          = false;
		bool                                        HaveCatalog    = false;
		bool                                        Closing        = false;
		bool                                        DeliveryFailed = false;
		std::optional<profile_management_operation> PendingOperation;
		profile_registry_host                       Registry;

		implementation(profile_registry_port& aRegistry, effect_executor& aEffects,
					   decision_executor& aDecision, profile_management_projection_port& aProjection,
					   core::application_session_id aSession)
			: Projection{ aProjection }
			, Registry{ aRegistry, aEffects, aDecision, *this, aSession }
		{ }

		[[nodiscard]] auto slot_for(const core::bpr_profile_id aProfile)
		{
			return std::ranges::find_if(Slots, [aProfile](const auto& aSlot) { return aSlot.Context.Profile == aProfile; });
		}
		[[nodiscard]] auto exact_slot(const core::bpr_window_context aContext)
		{
			return std::ranges::find_if(Slots, [aContext](const auto& aSlot) { return aSlot.Context == aContext; });
		}

		void emit(std::optional<profile_management_error> aFailure = { }, std::optional<profile_management_edit> aEdit = { })
		{
			auto result = profile_management_projection{ { }, Ready, !Registry.is_quiescent(), Closing, aFailure, aEdit };
			if (HaveCatalog) {
				for (const auto& entry : Catalog.state().Profiles) {
					const auto slot = slot_for(entry.Profile.Id);
					result.Profiles.push_back({ entry.Profile.Id, entry.Profile.Label,
												slot == Slots.end() ? profile_management_runtime::closed : slot->State });
				}
			}
			try {
				Projection.publish(result);
			} catch (...) {
				DeliveryFailed = true;
			}
		}

		void fail(const profile_management_operation aOperation, const std::optional<core::bpr_profile_id> aProfile,
				  const profile_management_failure aFailure)
		{
			emit(profile_management_error{ aOperation, aProfile, aFailure });
		}

		[[nodiscard]] auto synchronize() -> bool
		{
			const auto& accepted = Registry.accepted();
			if (!accepted || accepted->LastProfileId < Catalog.state().LastProfileId) {
				return false;
			}
			auto state = Catalog.state();
			for (const auto& slot : Slots) {
				if (std::ranges::none_of(accepted->Profiles, [&slot](const auto& aEntry) { return aEntry.Profile.Id == slot.Context.Profile; })) {
					return false;
				}
			}
			state.Profiles.clear();
			state.LastProfileId = accepted->LastProfileId;
			for (const auto& entry : accepted->Profiles) {
				const auto previous = std::ranges::find_if(Catalog.state().Profiles, [&entry](const auto& aPrevious) { return aPrevious.Profile.Id == entry.Profile.Id; });
				auto       profile  = previous == Catalog.state().Profiles.end() ? core::bpr_profile_windows{ entry.Profile, { }, { } } : *previous;
				profile.Profile     = entry.Profile;
				state.Profiles.push_back(std::move(profile));
			}
			auto restored = core::profile_window_catalog::restore(std::move(state));
			if (!restored) {
				return false;
			}
			Catalog = std::move(*restored);
			return true;
		}

		void publish(const profile_registry_publication& aPublication) override
		{
			const auto operation = PendingOperation.value_or(profile_management_operation::reload);
			PendingOperation.reset();
			if (Closing) {
				emit();
				return;
			}
			if (aPublication.Failure) {
				Ready = false;
				fail(operation, aPublication.Profile, translate(*aPublication.Failure));
				return;
			}
			Ready       = synchronize();
			HaveCatalog = HaveCatalog || Ready;
			if (!Ready) {
				fail(operation, aPublication.Profile, profile_management_failure::invalid_registry);
				return;
			}
			const auto edited = operation == profile_management_operation::create || operation == profile_management_operation::rename;
			emit({ }, edited && aPublication.Profile ? std::optional{ profile_management_edit{ operation, *aPublication.Profile } } : std::nullopt);
		}

		void edit(const profile_management_operation aOperation, const std::optional<core::bpr_profile_id> aProfile, const std::string_view aLabel)
		{
			if (Closing) {
				fail(aOperation, aProfile, profile_management_failure::closing);
				return;
			}
			if (!Registry.is_quiescent()) {
				fail(aOperation, aProfile, profile_management_failure::busy);
				return;
			}
			if (aOperation != profile_management_operation::reload && !Ready) {
				fail(aOperation, aProfile, profile_management_failure::registry_unavailable);
				return;
			}
			PendingOperation  = aOperation;
			const auto result = aOperation == profile_management_operation::reload ? Registry.reload()
				: aOperation == profile_management_operation::create               ? Registry.create(aLabel)
																				   : Registry.rename(*aProfile, aLabel);
			if (!result) {
				PendingOperation.reset();
				fail(aOperation, aProfile, translate(result.error()));
				return;
			}
			// Rejected effect posting can already have delivered its terminal synchronously.
			if (PendingOperation) {
				emit();
			}
		}

		void retire(const core::bpr_window_context aContext)
		{
			static_cast<void>(Catalog.close(aContext));
			const auto found = exact_slot(aContext);
			if (found != Slots.end()) {
				Slots.erase(found);
			}
		}

		void open(const core::bpr_profile_id aProfile)
		{
			constexpr auto operation = profile_management_operation::open;
			if (Closing) {
				fail(operation, aProfile, profile_management_failure::closing);
				return;
			}
			if (!Ready) {
				fail(operation, aProfile, profile_management_failure::registry_unavailable);
				return;
			}
			const auto slot = slot_for(aProfile);
			if (slot != Slots.end() && slot->State != profile_management_runtime::open) {
				fail(operation, aProfile, profile_management_failure::busy);
				return;
			}
			const auto launched = Catalog.launch(aProfile, core::profile_launch_intent::activate);
			if (!launched) {
				fail(operation, aProfile, launched.error() == core::profile_window_failure::unknown_profile ? profile_management_failure::unknown_profile : profile_management_failure::identity_exhausted);
				return;
			}
			const auto starting = !launched->Created.empty();
			if (starting) {
				Slots.push_back({ launched->Activated, profile_management_runtime::opening });
			}
			const auto profile = std::ranges::find_if(Catalog.state().Profiles, [aProfile](const auto& aEntry) { return aEntry.Profile.Id == aProfile; });
			const auto command = profile_runtime_command{ starting ? profile_runtime_command_kind::start : profile_runtime_command_kind::activate, launched->Activated, profile->Profile.Label };
			emit();
			// Reentrant status adoption may close the reservation before command delivery.
			const auto current = exact_slot(command.Context);
			if (Closing || current == Slots.end() || current->State != (starting ? profile_management_runtime::opening : profile_management_runtime::open)) {
				if (starting) {
					retire(command.Context);
					emit();
				}
				return;
			}
			try {
				Projection.perform(command);
			} catch (...) {
				DeliveryFailed = true;
				if (starting) {
					retire(command.Context);
				}
				fail(operation, aProfile, profile_management_failure::runtime_unavailable);
			}
		}

		[[nodiscard]] auto complete_open(const core::bpr_window_context aContext, const std::expected<void, profile_management_failure> aResult) -> bool
		{
			const auto slot = exact_slot(aContext);
			if (slot == Slots.end() || !slot->StartupPending) {
				return false;
			}
			slot->StartupPending = false;
			if (Closing || slot->State == profile_management_runtime::closing) {
				if (!aResult) {
					retire(aContext);
				} else {
					slot->State = profile_management_runtime::closing;
				}
				emit();
				return false;
			}
			if (!aResult) {
				retire(aContext);
				fail(profile_management_operation::open, aContext.Profile, aResult.error());
				return true;
			}
			slot->State = profile_management_runtime::open;
			emit();
			const auto current = exact_slot(aContext);
			return !Closing && current != Slots.end() && current->State == profile_management_runtime::open;
		}

		[[nodiscard]] auto begin_close(const core::bpr_window_context aContext) -> bool
		{
			const auto slot = exact_slot(aContext);
			if (slot == Slots.end() || slot->State == profile_management_runtime::closing) {
				return false;
			}
			slot->State = profile_management_runtime::closing;
			emit();
			return true;
		}
		[[nodiscard]] auto complete_close(const core::bpr_window_context aContext) -> bool
		{
			const auto slot = exact_slot(aContext);
			if (slot == Slots.end() || slot->State != profile_management_runtime::closing) {
				return false;
			}
			retire(aContext);
			emit();
			return true;
		}
		void shutdown()
		{
			if (Closing) {
				return;
			}
			Closing = true;
			Ready   = false;
			Registry.cancel();
			for (auto& slot : Slots) {
				slot.State = profile_management_runtime::closing;
			}
			emit();
		}
	};

	profile_management_host::profile_management_host(profile_registry_port& aRegistry, effect_executor& aEffects,
													 decision_executor& aDecision, profile_management_projection_port& aProjection, core::application_session_id aSession)
		: Implementation{ std::make_unique<implementation>(aRegistry, aEffects, aDecision, aProjection, aSession) }
	{ }
	profile_management_host::~profile_management_host() { Implementation->Registry.shutdown(); }
	void profile_management_host::apply(profile_management_intent aIntent)
	{
		std::visit([this](const auto& aValue) {
			using intent = std::decay_t<decltype(aValue)>;
			if constexpr (std::is_same_v<intent, reload_bpr_profiles_intent>) {
				reload();
			} else if constexpr (std::is_same_v<intent, create_bpr_profile_intent>) {
				Implementation->edit(profile_management_operation::create, { }, aValue.Label);
			} else if constexpr (std::is_same_v<intent, rename_bpr_profile_intent>) {
				Implementation->edit(profile_management_operation::rename, aValue.Profile, aValue.Label);
			} else {
				Implementation->open(aValue.Profile);
			}
		},
				   aIntent);
	}
	void profile_management_host::reload() { Implementation->edit(profile_management_operation::reload, { }, { }); }
	auto profile_management_host::complete_open(const core::bpr_window_context aContext, const std::expected<void, profile_management_failure> aResult) -> bool { return Implementation->complete_open(aContext, aResult); }
	auto profile_management_host::begin_close(const core::bpr_window_context aContext) -> bool { return Implementation->begin_close(aContext); }
	auto profile_management_host::complete_close(const core::bpr_window_context aContext) -> bool { return Implementation->complete_close(aContext); }
	void profile_management_host::shutdown() { Implementation->shutdown(); }
	auto profile_management_host::is_quiescent() const noexcept -> bool { return Implementation->Registry.is_quiescent() && Implementation->Slots.empty(); }
	auto profile_management_host::projection_delivery_failed() const noexcept -> bool { return Implementation->DeliveryFailed || Implementation->Registry.projection_delivery_failed(); }
}
