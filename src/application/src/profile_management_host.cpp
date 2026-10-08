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
		[[nodiscard]] auto translate(const core::profile_window_failure aFailure) -> profile_management_failure
		{
			switch (aFailure) {
			case core::profile_window_failure::unknown_profile:    return profile_management_failure::unknown_profile;
			case core::profile_window_failure::window_limit:       return profile_management_failure::window_limit;
			case core::profile_window_failure::identity_exhausted: return profile_management_failure::identity_exhausted;
			default:                                               return profile_management_failure::runtime_unavailable;
			}
		}

		struct runtime_slot
		{
			core::bpr_window_context     Context;
			profile_management_runtime   State;
			bool                         StartupPending = true;
			bool                         StartupIssued  = false;
			profile_management_operation Operation      = profile_management_operation::open;
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
		struct lifecycle_save
		{
			core::profile_window_catalog         Candidate;
			std::vector<profile_runtime_command> Commands;
			profile_management_operation         Operation;
			std::optional<core::bpr_profile_id>  Profile;
		};
		std::optional<lifecycle_save>         PendingLifecycle;
		std::vector<core::bpr_window_context> PendingFocus;
		std::vector<core::bpr_window_context> PendingRetirements;
		bool                                  Delivering = false;
		profile_registry_host                 Registry;

		implementation(profile_registry_port& aRegistry, effect_executor& aEffects,
					   decision_executor& aDecision, profile_management_projection_port& aProjection,
					   core::application_session_id aSession)
			: Projection{ aProjection }
			, Registry{ aRegistry, aEffects, aDecision, *this, aSession }
		{ }

		[[nodiscard]] auto exact_slot(const core::bpr_window_context aContext)
		{
			return std::ranges::find_if(Slots, [aContext](const auto& aSlot) { return aSlot.Context == aContext; });
		}

		[[nodiscard]] auto current(const core::bpr_window_context aContext) const noexcept -> bool
		{
			return !Closing && Catalog.accepts(aContext) && std::ranges::any_of(Slots, [aContext](const auto& aSlot) {
				return aSlot.Context == aContext && (aSlot.State == profile_management_runtime::opening || aSlot.State == profile_management_runtime::open);
			});
		}

		[[nodiscard]] auto runtime_state(const core::bpr_profile_id aProfile) const -> profile_management_runtime
		{
			auto state = profile_management_runtime::closed;
			for (const auto& slot : Slots) {
				if (slot.Context.Profile != aProfile) {
					continue;
				}
				if (slot.State == profile_management_runtime::open) {
					return profile_management_runtime::open;
				}
				if (state != profile_management_runtime::opening) {
					state = slot.State;
				}
			}
			return state;
		}

		[[nodiscard]] auto focus(const core::bpr_window_context aContext) -> bool
		{
			const auto slot = exact_slot(aContext);
			if (Closing || slot == Slots.end() || slot->State != profile_management_runtime::open || !Catalog.focus(aContext)) {
				return false;
			}
			if (PendingLifecycle) {
				std::erase(PendingFocus, aContext);
				PendingFocus.push_back(aContext);
			}
			return true;
		}

		void focus_latest_open(const core::bpr_profile_id aProfile)
		{
			const auto profile = std::ranges::find_if(Catalog.state().Profiles, [aProfile](const auto& aEntry) { return aEntry.Profile.Id == aProfile; });
			if (profile == Catalog.state().Profiles.end()) {
				return;
			}
			for (auto window = profile->Live.rbegin(); window != profile->Live.rend(); ++window) {
				if (focus({ aProfile, window->Id, window->Generation })) {
					return;
				}
			}
		}

		void emit(std::optional<profile_management_error> aFailure = { }, std::optional<profile_management_edit> aEdit = { })
		{
			auto result = profile_management_projection{ { }, Ready, !Registry.is_quiescent() || Delivering, Closing, aFailure, aEdit };
			if (HaveCatalog) {
				for (const auto& entry : Catalog.state().Profiles) {
					result.Profiles.push_back({ entry.Profile.Id, entry.Profile.Label, runtime_state(entry.Profile.Id) });
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
			if (!accepted || accepted->LastProfileId < Catalog.state().LastProfileId
				|| accepted->LastWindowId < Catalog.state().LastWindowId) {
				return false;
			}
			auto state = Catalog.state();
			state.Profiles.clear();
			state.LastProfileId = accepted->LastProfileId;
			state.LastWindowId  = accepted->LastWindowId;
			for (const auto& entry : accepted->Profiles) {
				const auto previous = std::ranges::find_if(Catalog.state().Profiles, [&entry](const auto& aPrevious) { return aPrevious.Profile.Id == entry.Profile.Id; });
				auto       profile  = core::bpr_profile_windows{ entry.Profile, { }, { } };
				if (previous != Catalog.state().Profiles.end()) {
					profile.Live = previous->Live;
					for (const auto& live : profile.Live) {
						const auto durable = std::ranges::find_if(entry.RetainedWindows, [&live](const auto& aWindow) { return aWindow.Id == live.Id; });
						if (durable != entry.RetainedWindows.end() && *durable != live) {
							return false;
						}
					}
				}
				for (const auto& window : entry.RetainedWindows) {
					if (std::ranges::find(profile.Live, window) != profile.Live.end()) {
						continue;
					}
					const auto wasRetained = previous != Catalog.state().Profiles.end()
						&& std::ranges::any_of(previous->Retained, [&window](const auto& aWindow) { return aWindow.Id == window.Id; });
					if (!HaveCatalog || wasRetained || window.Id.value() > Catalog.state().LastWindowId) {
						profile.Retained.push_back(window);
					}
				}
				state.Profiles.push_back(std::move(profile));
			}
			for (const auto& slot : Slots) {
				if (std::ranges::none_of(state.Profiles, [&slot](const auto& aEntry) { return aEntry.Profile.Id == slot.Context.Profile; })) {
					return false;
				}
			}
			auto restored = core::profile_window_catalog::restore(std::move(state));
			if (!restored) {
				return false;
			}
			Catalog = std::move(*restored);
			return true;
		}

		void deliver(const std::vector<profile_runtime_command>& aCommands, const profile_management_operation aOperation)
		{
			Delivering = true;
			std::optional<profile_management_error> failure;
			emit();
			for (const auto& command : aCommands) {
				const auto slot = exact_slot(command.Context);
				if (command.Kind != profile_runtime_command_kind::quit
					&& (slot == Slots.end() || (command.Kind == profile_runtime_command_kind::start && (Closing || slot->State != profile_management_runtime::opening)))) {
					continue;
				}
				if (command.Kind == profile_runtime_command_kind::start) {
					slot->StartupIssued = true;
				}
				try {
					Projection.perform(command);
				} catch (...) {
					DeliveryFailed = true;
					if (command.Kind == profile_runtime_command_kind::start) {
						retire(command.Context);
					}
					failure = profile_management_error{ aOperation, command.Context.Profile.value() == 0U ? std::nullopt : std::optional{ command.Context.Profile }, profile_management_failure::runtime_unavailable };
				}
			}
			Delivering = false;
			emit(failure);
		}

		void publish(const profile_registry_publication& aPublication) override
		{
			auto       lifecycle = std::exchange(PendingLifecycle, std::nullopt);
			const auto operation = lifecycle ? lifecycle->Operation : PendingOperation.value_or(profile_management_operation::reload);
			PendingOperation.reset();
			if (Closing) {
				emit();
				return;
			}
			if (aPublication.Failure) {
				Ready = false;
				fail(operation, lifecycle ? lifecycle->Profile : aPublication.Profile, translate(*aPublication.Failure));
				return;
			}
			if (lifecycle) {
				Catalog = std::move(lifecycle->Candidate);
				for (const auto context : PendingRetirements) {
					static_cast<void>(Catalog.close(context));
				}
				PendingRetirements.clear();
				for (const auto context : PendingFocus) {
					static_cast<void>(Catalog.focus(context));
				}
				PendingFocus.clear();
				Ready = true;
				for (const auto& command : lifecycle->Commands) {
					if (command.Kind == profile_runtime_command_kind::start) {
						Slots.push_back({ command.Context, profile_management_runtime::opening, true, false, operation });
					} else if (command.Kind == profile_runtime_command_kind::close) {
						const auto slot = exact_slot(command.Context);
						if (slot != Slots.end()) {
							slot->State = profile_management_runtime::closing;
						}
					} else if (command.Kind == profile_runtime_command_kind::quit) {
						Closing = true;
						Ready   = false;
						for (auto& slot : Slots) {
							slot.State = profile_management_runtime::closing;
						}
					}
				}
				deliver(lifecycle->Commands, operation);
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
			if (!Registry.is_quiescent() || Delivering) {
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
			if (PendingLifecycle && Catalog.accepts(aContext)) {
				PendingRetirements.push_back(aContext);
			}
			static_cast<void>(Catalog.close(aContext));
			const auto found = exact_slot(aContext);
			if (found != Slots.end()) {
				Slots.erase(found);
			}
		}

		[[nodiscard]] auto lifecycle_ready(const profile_management_operation aOperation, const std::optional<core::bpr_profile_id> aProfile) -> bool
		{
			if (Closing || !Ready || !Registry.is_quiescent() || Delivering) {
				fail(aOperation, aProfile, Closing ? profile_management_failure::closing : !Registry.is_quiescent() || Delivering ? profile_management_failure::busy
																																  : profile_management_failure::registry_unavailable);
				return false;
			}
			return true;
		}

		[[nodiscard]] auto save(lifecycle_save aSave) -> bool
		{
			PendingFocus.clear();
			PendingRetirements.clear();
			PendingLifecycle  = std::move(aSave);
			const auto result = Registry.save_window_state(PendingLifecycle->Candidate.state());
			if (!result) {
				const auto rejected = std::exchange(PendingLifecycle, std::nullopt);
				fail(rejected->Operation, rejected->Profile, translate(result.error()));
				return false;
			}
			if (PendingLifecycle) {
				emit();
			}
			return true;
		}

		void open(const core::bpr_profile_id aProfile, const core::profile_launch_intent aIntent, const std::optional<core::window_review_destination>& aReview = std::nullopt)
		{
			const auto operation = aIntent == core::profile_launch_intent::new_window ? profile_management_operation::new_window : profile_management_operation::open;
			if (!lifecycle_ready(operation, aProfile)) {
				return;
			}
			const auto state = runtime_state(aProfile);
			if (state == profile_management_runtime::closing
				|| (aIntent == core::profile_launch_intent::activate && state == profile_management_runtime::opening)) {
				fail(operation, aProfile, profile_management_failure::busy);
				return;
			}
			if (aIntent == core::profile_launch_intent::activate) {
				focus_latest_open(aProfile);
			}
			auto       candidate = Catalog;
			const auto launched  = candidate.launch(aProfile, aIntent);
			if (!launched) {
				fail(operation, aProfile, translate(launched.error()));
				return;
			}
			const auto profile = std::ranges::find_if(candidate.state().Profiles, [aProfile](const auto& aEntry) { return aEntry.Profile.Id == aProfile; });
			const auto label   = profile->Profile.Label;
			if (launched->Created.empty()) {
				Catalog = std::move(candidate);
				deliver({ { profile_runtime_command_kind::activate, launched->Activated, label } }, operation);
				return;
			}
			const auto reserved = static_cast<std::size_t>(std::ranges::count_if(Slots, [aProfile](const auto& aSlot) { return aSlot.Context.Profile == aProfile; }));
			if (reserved + launched->Created.size() > core::maximum_bpr_windows_per_profile) {
				fail(operation, aProfile, profile_management_failure::busy);
				return;
			}
			auto pending = lifecycle_save{ std::move(candidate), { }, operation, aProfile };
			for (const auto& created : launched->Created) {
				pending.Commands.push_back({ profile_runtime_command_kind::start, created.Context, label, created.Restore });
				if (aIntent == core::profile_launch_intent::new_window && !created.Restore) {
					pending.Commands.back().Review = aReview;
				}
			}
			static_cast<void>(save(std::move(pending)));
		}

		[[nodiscard]] auto complete_open(const core::bpr_window_context aContext, const std::expected<void, profile_management_failure> aResult) -> bool
		{
			const auto slot = exact_slot(aContext);
			if (slot == Slots.end() || !slot->StartupPending || !slot->StartupIssued) {
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
				const auto operation = slot->Operation;
				retire(aContext);
				fail(operation, aContext.Profile, aResult.error());
				return true;
			}
			slot->State = profile_management_runtime::open;
			static_cast<void>(focus(aContext));
			emit();
			const auto current = exact_slot(aContext);
			return !Closing && current != Slots.end() && current->State == profile_management_runtime::open;
		}

		[[nodiscard]] auto begin_close(const core::bpr_window_context aContext) -> bool
		{
			if (!current(aContext)) {
				return false;
			}
			if (!lifecycle_ready(profile_management_operation::close, aContext.Profile)) {
				return false;
			}
			auto candidate = Catalog;
			if (!candidate.close(aContext)) {
				return false;
			}
			return save({ std::move(candidate), { { profile_runtime_command_kind::close, aContext, { } } }, profile_management_operation::close, aContext.Profile });
		}

		[[nodiscard]] auto request_quit() -> bool
		{
			if (!lifecycle_ready(profile_management_operation::quit, { })) {
				return false;
			}
			auto candidate = Catalog;
			for (const auto& profile : Catalog.state().Profiles) {
				if (!candidate.quit(profile.Profile.Id)) {
					return false;
				}
			}
			return save({ std::move(candidate), { { profile_runtime_command_kind::quit, { }, { } } }, profile_management_operation::quit, { } });
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
			std::erase_if(Slots, [](const auto& aSlot) { return aSlot.StartupPending && !aSlot.StartupIssued; });
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
	void profile_management_host::report_runtime_admission_failure(core::bpr_profile_id aProfile, profile_management_failure aFailure)
	{
		Implementation->fail(profile_management_operation::open, aProfile, aFailure);
	}

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
			} else if constexpr (std::is_same_v<intent, new_bpr_window_intent>) {
				Implementation->open(aValue.Profile, core::profile_launch_intent::new_window, aValue.Review);
			} else {
				Implementation->open(aValue.Profile, core::profile_launch_intent::activate);
			}
		},
				   aIntent);
	}
	void profile_management_host::reload() { Implementation->edit(profile_management_operation::reload, { }, { }); }
	auto profile_management_host::complete_open(const core::bpr_window_context aContext, const std::expected<void, profile_management_failure> aResult) -> bool { return Implementation->complete_open(aContext, aResult); }
	auto profile_management_host::begin_close(const core::bpr_window_context aContext) -> bool { return Implementation->begin_close(aContext); }
	auto profile_management_host::complete_close(const core::bpr_window_context aContext) -> bool { return Implementation->complete_close(aContext); }
	auto profile_management_host::live_windows(core::bpr_profile_id aProfile) const -> std::vector<core::bpr_window_context>
	{
		auto result = std::vector<core::bpr_window_context>{ };
		for (const auto& profile : Implementation->Catalog.state().Profiles) {
			if (profile.Profile.Id != aProfile) {
				continue;
			}
			for (const auto& window : profile.Live) {
				result.push_back({ aProfile, window.Id, window.Generation });
			}
			break;
		}
		return result;
	}

	auto profile_management_host::current(const core::bpr_window_context aContext) const noexcept -> bool { return Implementation->current(aContext); }
	auto profile_management_host::focus(const core::bpr_window_context aContext) -> bool { return Implementation->focus(aContext); }

	auto profile_management_host::request_quit() -> bool { return Implementation->request_quit(); }
	void profile_management_host::shutdown() { Implementation->shutdown(); }
	auto profile_management_host::is_quiescent() const noexcept -> bool { return Implementation->Registry.is_quiescent() && Implementation->Slots.empty(); }
	auto profile_management_host::projection_delivery_failed() const noexcept -> bool { return Implementation->DeliveryFailed || Implementation->Registry.projection_delivery_failed(); }
}
