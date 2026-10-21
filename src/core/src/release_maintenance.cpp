#include "bpr/core/release_maintenance.hpp"

#include <algorithm>
#include <chrono>
#include <limits>

namespace bpr::core {
	namespace {
		using command = release_maintenance_command;
		using reason  = release_maintenance_reason;
		using phase   = release_maintenance_phase;
		using state   = release_maintenance_state;
		using output  = release_maintenance_output;

		constexpr auto       MaximumIdentity     = std::numeric_limits<std::uint64_t>::max();
		constexpr auto       DiagnosticArtifacts = static_cast<std::uint32_t>(release_maintenance_artifact::diagnostics);
		constexpr std::array Commands{ command::preview_diagnostics, command::export_diagnostics,
									   command::prepare_clear, command::clear_diagnostics, command::prepare_purge, command::purge_profile, command::cancel };

		auto valid_authority(const release_maintenance_authority& aAuthority) noexcept -> bool
		{
			return aAuthority.Profile.value() != 0U && aAuthority.Session.value() != 0U && aAuthority.Root.value() != 0U;
		}

		auto valid_digest(const std::array<std::uint8_t, 32>& aDigest) noexcept -> bool
		{
			return std::ranges::any_of(aDigest, [](const auto aByte) { return aByte != 0U; });
		}

		auto preparing(const command aCommand) noexcept -> bool
		{
			return aCommand == command::preview_diagnostics || aCommand == command::prepare_clear || aCommand == command::prepare_purge;
		}

		auto executing(const command aCommand) noexcept -> bool
		{
			return aCommand == command::export_diagnostics || aCommand == command::clear_diagnostics || aCommand == command::purge_profile;
		}

		auto validate_snapshot(const release_diagnostic_snapshot& aSnapshot) noexcept -> reason
		{
			if (!valid_diagnostic_export_options(aSnapshot.Options) || aSnapshot.FirstUtcMs > aSnapshot.LastUtcMs || aSnapshot.RecentRecords > aSnapshot.Records || aSnapshot.CaptureRedactedRecords > aSnapshot.Records || aSnapshot.LimitOmittedRecords > aSnapshot.OmittedRecords || aSnapshot.Categories > 7U || aSnapshot.Sessions > aSnapshot.Records) {
				return reason::invalid_selection;
			}
			if (aSnapshot.Revision.value() == 0U || !valid_digest(aSnapshot.Digest)) {
				return reason::invalid_selection;
			}
			const auto permitted = release_diagnostic_default_fields
				| (aSnapshot.Options.Policy == diagnostic_export_policy::metadata ? 0U : release_diagnostic_classified_fields)
				| (aSnapshot.Options.Policy == diagnostic_export_policy::identifying ? static_cast<std::uint32_t>(release_diagnostic_field::captured_identity) : 0U);
			if (aSnapshot.Fields == 0U || (aSnapshot.Fields & ~permitted) != 0U) {
				return reason::privacy_refusal;
			}
			if (aSnapshot.Records > release_diagnostic_record_limit || aSnapshot.Bytes == 0U || aSnapshot.Bytes > release_diagnostic_byte_limit) {
				return reason::limit_exceeded;
			}
			return reason::none;
		}

		auto validate_inventory(const release_maintenance_inventory& aInventory, const command aCommand) noexcept -> reason
		{
			const auto categories = aCommand == command::prepare_clear ? DiagnosticArtifacts : release_profile_purge_categories;
			if (aInventory.Revision.value() == 0U || !valid_digest(aInventory.Digest) || aInventory.Categories != categories) {
				return reason::invalid_selection;
			}
			if (!aInventory.Complete || aInventory.UnknownEntries != 0U) {
				return reason::incomplete_inventory;
			}
			if (aInventory.Entries > release_maintenance_entry_limit) {
				return reason::limit_exceeded;
			}
			return reason::none;
		}

		auto validate_selection(const command aCommand, const release_maintenance_selection& aSelection) noexcept -> reason
		{
			if (aCommand == command::preview_diagnostics) {
				const auto* snapshot = std::get_if<release_diagnostic_snapshot>(&aSelection);
				return snapshot ? validate_snapshot(*snapshot) : reason::invalid_selection;
			}
			if (aCommand == command::prepare_clear || aCommand == command::prepare_purge) {
				const auto* inventory = std::get_if<release_maintenance_inventory>(&aSelection);
				return inventory ? validate_inventory(*inventory, aCommand) : reason::invalid_selection;
			}
			return reason::invalid_selection;
		}

		auto prepared_for(const state& aState, const command aCommand) noexcept -> reason
		{
			if (!aState.Prepared) {
				return reason::no_selection;
			}
			const auto origin   = aState.Prepared->Operation;
			const bool matching = (aCommand == command::export_diagnostics && origin == command::preview_diagnostics)
				|| (aCommand == command::clear_diagnostics && origin == command::prepare_clear)
				|| (aCommand == command::purge_profile && origin == command::prepare_purge);
			if (!matching || aState.Prepared->Authority != aState.Authority || aState.Prepared->Generation != aState.Generation) {
				return reason::selection_changed;
			}
			return validate_selection(origin, aState.Prepared->Selection);
		}

		auto command_reason(const state& aState, const command aCommand) noexcept -> reason
		{
			if (std::ranges::find(Commands, aCommand) == Commands.end()) {
				return reason::invalid_command;
			}
			if (aState.Stopped) {
				return reason::shutdown;
			}
			if (!valid_authority(aState.Authority)) {
				return reason::invalid_authority;
			}
			if (aCommand == command::cancel) {
				if (aState.PointOfNoReturn) {
					return reason::point_of_no_return;
				}
				return (!aState.Pending && !aState.Prepared) || aState.CancellationRequested ? reason::no_selection : reason::none;
			}
			if (aState.Pending) {
				return reason::busy;
			}
			if (aState.RecoveryRequired) {
				return reason::recovery_required;
			}
			if (aState.Exhausted || aState.NextRequest.value() == 0U || aState.NextRequest.value() == MaximumIdentity
				|| aState.Generation.value() == 0U || aState.Generation.value() == MaximumIdentity) {
				return reason::identity_exhausted;
			}
			const auto& capabilities = aState.Capabilities;
			switch (aCommand) {
			case command::preview_diagnostics:
				return capabilities.Diagnostics ? reason::none : reason::unavailable;
			case command::prepare_clear:
				return capabilities.Clear ? reason::none : reason::unavailable;
			case command::prepare_purge:
				return capabilities.Purge ? reason::none : reason::unavailable;
			case command::export_diagnostics:
				if (!capabilities.Export) {
					return reason::unavailable;
				}
				break;
			case command::clear_diagnostics:
				if (!capabilities.Clear) {
					return reason::unavailable;
				}
				break;
			case command::purge_profile:
				if (!capabilities.Purge) {
					return reason::unavailable;
				}
				break;
			case command::cancel:
				break;
			}
			return prepared_for(aState, aCommand);
		}

		auto reject(const reason aReason) noexcept -> output
		{
			return { .Diagnostic = aReason };
		}

		auto accepted(const bool aSelectionChanged = false) noexcept -> output
		{
			return { .Accepted = true, .StatusChanged = true, .SelectionChanged = aSelectionChanged };
		}

		void advance_generation(state& aState) noexcept
		{
			if (aState.Generation.value() == MaximumIdentity) {
				aState.Exhausted = true;
			} else {
				aState.Generation = release_maintenance_generation{ aState.Generation.value() + 1U };
			}
		}

		auto retire(state& aState, const reason aReason) noexcept -> output
		{
			auto result = accepted(aState.Prepared.has_value());
			if (aState.Pending && aState.PointOfNoReturn && aState.Pending->Operation != command::export_diagnostics) {
				aState.RecoveryRequired = true;
			}
			if (aState.Pending && !aState.CancellationRequested && !aState.PointOfNoReturn) {
				result.Request = release_maintenance_request{ *aState.Pending, true };
			}
			aState.Pending.reset();
			aState.Prepared.reset();
			aState.CancellationRequested = false;
			aState.PointOfNoReturn       = false;
			advance_generation(aState);
			aState.Phase  = phase::idle;
			aState.Reason = aReason;
			if (aState.RecoveryRequired) {
				aState.Reason = reason::recovery_required;
			}
			if (aState.Exhausted) {
				aState.Reason = reason::identity_exhausted;
			}
			result.Diagnostic = aState.Reason;
			return result;
		}

		auto matches(const state& aState, const release_maintenance_context& aContext) noexcept -> bool
		{
			return aState.Pending && *aState.Pending == aContext;
		}

		auto finish(state& aState, const phase aPhase, const reason aReason) noexcept -> output
		{
			aState.Pending.reset();
			aState.CancellationRequested = false;
			aState.PointOfNoReturn       = false;
			aState.Phase                 = aPhase;
			aState.Reason                = aReason;
			auto result                  = accepted();
			result.Diagnostic            = aReason;
			return result;
		}

		auto apply(state& aState, const release_maintenance_intent& aIntent) noexcept -> output
		{
			if (!valid_authority(aIntent.Authority) || aIntent.Authority != aState.Authority) {
				return reject(reason::invalid_authority);
			}
			if (const auto refusal = command_reason(aState, aIntent.Command); refusal != reason::none) {
				return reject(refusal);
			}
			if (aIntent.Command == command::cancel) {
				const auto& target = aState.Pending ? aState.Pending : aState.Prepared;
				if (aIntent.Target != target) {
					return reject(reason::selection_changed);
				}
				if (!aState.Pending) {
					auto result  = retire(aState, reason::cancelled);
					aState.Phase = phase::cancelled;
					return result;
				}
				aState.CancellationRequested = true;
				aState.Phase                 = phase::cancelling;
				auto result                  = accepted();
				result.Request               = release_maintenance_request{ *aState.Pending, true };
				return result;
			}
			if (executing(aIntent.Command) && aIntent.Target != aState.Prepared) {
				return reject(reason::selection_changed);
			}
			if (preparing(aIntent.Command) && aIntent.Target) {
				return reject(reason::invalid_selection);
			}
			if (aIntent.Command == command::preview_diagnostics && !valid_diagnostic_export_options(aIntent.ExportOptions)) {
				return reject(reason::invalid_selection);
			}
			if (aIntent.Command == command::export_diagnostics
				&& std::get<release_diagnostic_snapshot>(aState.Prepared->Selection).Options.Policy == diagnostic_export_policy::identifying
				&& !aIntent.IdentifyingConfirmed) {
				return reject(reason::privacy_refusal);
			}
			const auto options   = executing(aIntent.Command) ? aState.Prepared->ExportOptions : aIntent.ExportOptions;
			const auto selection = executing(aIntent.Command) ? aState.Prepared->Selection : release_maintenance_selection{ };
			auto       result    = accepted(aState.Prepared.has_value());
			advance_generation(aState);
			aState.Pending     = release_maintenance_context{ aState.Authority, aState.NextRequest, aState.Generation, aIntent.Command, selection, options };
			aState.NextRequest = request_id{ aState.NextRequest.value() + 1U };
			aState.Prepared.reset();
			aState.Phase                 = preparing(aIntent.Command) ? phase::preparing : phase::running;
			aState.Reason                = reason::none;
			aState.CancellationRequested = false;
			aState.PointOfNoReturn       = false;
			result.Request               = release_maintenance_request{ *aState.Pending, false };
			return result;
		}

		auto apply(state& aState, const release_maintenance_authority_changed& aEvent) noexcept -> output
		{
			if (aState.Stopped) {
				return reject(reason::shutdown);
			}
			const bool valid        = valid_authority(aEvent.Authority);
			auto       result       = retire(aState, valid ? reason::none : reason::invalid_authority);
			aState.Authority        = valid ? aEvent.Authority : release_maintenance_authority{ };
			aState.Capabilities     = valid ? aEvent.Capabilities : release_maintenance_capabilities{ };
			aState.RecoveryRequired = false;
			if (!aState.Exhausted) {
				aState.Reason     = valid ? reason::none : reason::invalid_authority;
				result.Diagnostic = aState.Reason;
			}
			result.AuthorityChanged = true;
			return result;
		}

		auto apply(state& aState, const release_maintenance_invalidated& aEvent) noexcept -> output
		{
			if (aState.Stopped) {
				return reject(reason::shutdown);
			}
			if (!valid_authority(aEvent.Authority) || aEvent.Authority != aState.Authority) {
				return reject(reason::invalid_authority);
			}
			return retire(aState, reason::selection_changed);
		}

		auto apply(state& aState, const release_maintenance_shutdown&) noexcept -> output
		{
			if (aState.Stopped) {
				return reject(reason::shutdown);
			}
			auto result    = retire(aState, reason::shutdown);
			aState.Stopped = true;
			aState.Phase   = phase::stopped;
			return result;
		}

		auto apply(state& aState, const release_maintenance_prepared& aEvent) noexcept -> output
		{
			if (!matches(aState, aEvent.Context)) {
				return reject(reason::stale_completion);
			}
			if (!preparing(aEvent.Context.Operation)) {
				return reject(reason::invalid_completion);
			}
			if (aState.CancellationRequested) {
				return finish(aState, phase::cancelled, reason::cancelled);
			}
			if (const auto refusal = validate_selection(aEvent.Context.Operation, aEvent.Selection); refusal != reason::none) {
				return finish(aState, phase::failed, refusal);
			}
			if (const auto* snapshot = std::get_if<release_diagnostic_snapshot>(&aEvent.Selection);
				snapshot && snapshot->Options != aEvent.Context.ExportOptions) {
				return finish(aState, phase::failed, reason::selection_changed);
			}
			aState.Prepared            = aEvent.Context;
			aState.Prepared->Selection = aEvent.Selection;
			auto result                = finish(aState, phase::ready, reason::none);
			result.SelectionChanged    = true;
			return result;
		}

		auto apply(state& aState, const release_maintenance_commit_started& aEvent) noexcept -> output
		{
			if (!matches(aState, aEvent.Context)) {
				return reject(reason::stale_completion);
			}
			if (!executing(aEvent.Context.Operation) || aState.PointOfNoReturn) {
				return reject(reason::invalid_completion);
			}
			aState.PointOfNoReturn = true;
			aState.Phase           = phase::running;
			return accepted();
		}

		auto apply(state& aState, const release_maintenance_completed& aEvent) noexcept -> output
		{
			if (!matches(aState, aEvent.Context)) {
				return reject(reason::stale_completion);
			}
			if (!executing(aEvent.Context.Operation) || !aState.PointOfNoReturn) {
				return reject(reason::invalid_completion);
			}
			const auto result = finish(aState, phase::completed, reason::none);
			if (aEvent.Context.Operation == command::purge_profile) {
				aState.Stopped = true;
			}
			return result;
		}

		auto operational_failure(const reason aReason) noexcept -> bool
		{
			return aReason == reason::unavailable || aReason == reason::io_failure || aReason == reason::selection_changed
				|| aReason == reason::privacy_refusal || aReason == reason::limit_exceeded || aReason == reason::incomplete_inventory
				|| aReason == reason::recovery_required || aReason == reason::storage_not_private;
		}

		auto apply(state& aState, const release_maintenance_failed& aEvent) noexcept -> output
		{
			if (!matches(aState, aEvent.Context)) {
				return reject(reason::stale_completion);
			}
			if (!operational_failure(aEvent.Reason)) {
				return reject(reason::invalid_completion);
			}
			aState.RecoveryRequired = aEvent.Reason == reason::recovery_required
				|| (aState.PointOfNoReturn && aEvent.Context.Operation != command::export_diagnostics);
			return finish(aState, phase::failed, aState.RecoveryRequired ? reason::recovery_required : aEvent.Reason);
		}

		auto apply(state& aState, const release_maintenance_cancelled& aEvent) noexcept -> output
		{
			if (!matches(aState, aEvent.Context)) {
				return reject(reason::stale_completion);
			}
			if (aState.PointOfNoReturn) {
				return reject(reason::point_of_no_return);
			}
			return finish(aState, phase::cancelled, reason::cancelled);
		}
	}

	auto valid_diagnostic_export_options(diagnostic_export_options aOptions) noexcept -> bool
	{
		return (aOptions.Policy == diagnostic_export_policy::metadata || aOptions.Policy == diagnostic_export_policy::anonymized || aOptions.Policy == diagnostic_export_policy::identifying)
			&& (aOptions.Scope == diagnostic_export_scope::retained_sessions || aOptions.Scope == diagnostic_export_scope::current_session)
			&& (aOptions.Policy != diagnostic_export_policy::metadata || (aOptions.Scope == diagnostic_export_scope::retained_sessions && !aOptions.IncludeRecent));
	}

	auto exportable(const release_diagnostic_field aField) noexcept -> bool
	{
		switch (aField) {
		case release_diagnostic_field::operation:
		case release_diagnostic_field::outcome:
		case release_diagnostic_field::failure_code:
		case release_diagnostic_field::counts:
			return true;
		default:
			return false;
		}
	}

	auto make_release_maintenance_state(const release_maintenance_authority    aAuthority,
										const release_maintenance_capabilities aCapabilities) noexcept -> release_maintenance_state
	{
		auto result = state{ };
		if (valid_authority(aAuthority)) {
			result.Authority    = aAuthority;
			result.Capabilities = aCapabilities;
		} else {
			result.Reason = reason::invalid_authority;
		}
		return result;
	}

	auto release_maintenance_commands(const release_maintenance_state& aState) noexcept
		-> std::array<release_maintenance_command_projection, 7>
	{
		auto result = std::array<release_maintenance_command_projection, 7>{ };
		std::ranges::transform(Commands, result.begin(), [&aState](const auto aCommand) {
			return release_maintenance_command_projection{ aCommand, command_reason(aState, aCommand) };
		});
		return result;
	}

	auto diagnostic_export_file_name(const diagnostic_export_local_time& aTime, const std::string_view aVersion) -> std::string
	{
		const auto padded = [](const unsigned aValue, const std::size_t aWidth) {
			auto text = std::to_string(aValue);
			return std::string(aWidth > text.size() ? aWidth - text.size() : 0U, '0') + text;
		};
		auto       name = std::string{ "BetterPullReview-diagnostics" };
		const auto date = std::chrono::year{ aTime.Year } / std::chrono::month{ aTime.Month } / std::chrono::day{ aTime.Day };
		if (aTime.Year >= 1 && aTime.Year <= 9999 && date.ok() && aTime.Hour < 24U && aTime.Minute < 60U) {
			name += "-" + padded(static_cast<unsigned>(aTime.Year), 4U) + "-" + padded(aTime.Month, 2U) + "-" + padded(aTime.Day, 2U) + "-" + padded(aTime.Hour, 2U) + padded(aTime.Minute, 2U);
		}
		// Dotted decimal numbers only: 0.3.1, never a suffix or other text.
		bool numbered = !aVersion.empty() && aVersion.size() <= 32U && aVersion.front() != '.' && aVersion.back() != '.' && aVersion.find("..") == std::string_view::npos;
		numbered      = numbered && std::ranges::all_of(aVersion, [](const char aCharacter) { return (aCharacter >= '0' && aCharacter <= '9') || aCharacter == '.'; });
		if (numbered) {
			name += "-";
			name += aVersion;
		}
		return name + ".txt";
	}

	auto transition(release_maintenance_state& aState, const release_maintenance_event& aEvent) noexcept -> release_maintenance_output
	{
		return std::visit([&aState](const auto& aValue) { return apply(aState, aValue); }, aEvent);
	}
}
