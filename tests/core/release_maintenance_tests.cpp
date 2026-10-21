#include "bpr/core/release_maintenance.hpp"
#include "bpr/core/review_cycle_zip.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {
	using namespace bpr::core;
	using command = release_maintenance_command;
	using reason  = release_maintenance_reason;
	using phase   = release_maintenance_phase;
	using state   = release_maintenance_state;
	using context = release_maintenance_context;

	static_assert(std::is_trivially_copyable_v<state>);
	static_assert(sizeof(state) <= 512U);
	constexpr release_maintenance_capabilities Available{ true, true, true, true };

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto authority() -> release_maintenance_authority
	{
		return { bpr_profile_id{ 2U }, application_session_id{ 3U }, release_maintenance_root_id{ 4U } };
	}

	auto current() -> state
	{
		return make_release_maintenance_state(authority(), Available);
	}

	auto snapshot() -> release_diagnostic_snapshot
	{
		return { .Revision = release_maintenance_revision{ 5U }, .Digest = { 6U }, .Records = 2U, .Bytes = 128U, .OmittedRecords = 7U };
	}

	auto inventory(const command aCommand = command::prepare_purge) -> release_maintenance_inventory
	{
		return { .Revision = release_maintenance_revision{ 5U }, .Digest = { 6U }, .Categories = aCommand == command::prepare_clear ? 1U : release_profile_purge_categories, .Entries = 12U, .Complete = true };
	}

	auto gesture(state& aState, const command aCommand) -> release_maintenance_output
	{
		const bool execute = aCommand == command::export_diagnostics || aCommand == command::clear_diagnostics || aCommand == command::purge_profile;
		if (aCommand == command::cancel) {
			return transition(aState, release_maintenance_intent{ aState.Authority, aCommand, aState.Pending ? aState.Pending : aState.Prepared });
		}
		return transition(aState, release_maintenance_intent{ aState.Authority, aCommand, execute ? aState.Prepared : std::nullopt });
	}

	auto begin(state& aState, const command aCommand) -> context
	{
		const auto output = gesture(aState, aCommand);
		expect(output.Accepted && output.Request && !output.Request->Cancel && aState.Pending == output.Request->Context, "gesture starts exactly one identity-bound request");
		return output.Request->Context;
	}

	auto prepare(state& aState, const command aCommand, const release_maintenance_selection& aSelection) -> context
	{
		const auto pending = begin(aState, aCommand);
		expect(std::holds_alternative<std::monostate>(pending.Selection), "preparation has no executable selection");
		const auto output = transition(aState, release_maintenance_prepared{ pending, aSelection });
		expect(output.Accepted && output.SelectionChanged && !output.Request && !aState.Pending && aState.Prepared && aState.Phase == phase::ready, "preparation only publishes a bounded selection");
		expect(aState.Prepared->Authority == pending.Authority && aState.Prepared->Request == pending.Request
				   && aState.Prepared->Generation == pending.Generation && aState.Prepared->Selection == aSelection,
			   "preview retains origin and exact selection");
		return *aState.Prepared;
	}

	void reject_unchanged(state& aState, const release_maintenance_event& aEvent, const reason aReason)
	{
		const auto before = aState;
		const auto output = transition(aState, aEvent);
		expect(!output.Accepted && !output.Request && output.Diagnostic == aReason && aState == before, "refused event cannot change state or request effects");
	}

	void test_safe_defaults_and_closed_fields()
	{
		auto unavailable = make_release_maintenance_state(authority(), { });
		for (const auto candidate : { command::preview_diagnostics, command::prepare_clear, command::prepare_purge }) {
			reject_unchanged(unavailable, release_maintenance_intent{ authority(), candidate, { } }, reason::unavailable);
		}
		auto invalid = make_release_maintenance_state({ }, Available);
		expect(invalid.Authority == release_maintenance_authority{ } && invalid.Capabilities == release_maintenance_capabilities{ }, "invalid root admission disables every capability");
		reject_unchanged(invalid, release_maintenance_intent{ { }, command::prepare_purge, { } }, reason::invalid_authority);
		for (unsigned bit = 0U; bit < 32U; ++bit) {
			expect(exportable(static_cast<release_diagnostic_field>(1U << bit)) == (bit < 4U), "only four fixed metadata field classes can export");
		}
		expect(!exportable(static_cast<release_diagnostic_field>(0U)) && !exportable(static_cast<release_diagnostic_field>(3U)), "unknown and combined field labels do not classify raw bytes");
		auto active = current();
		for (unsigned value = 7U; value < 256U; ++value) {
			reject_unchanged(active, release_maintenance_intent{ authority(), static_cast<command>(value), { } }, reason::invalid_command);
		}
		for (const auto action : release_maintenance_commands(active)) {
			const bool enabled = action.Command == command::preview_diagnostics || action.Command == command::prepare_clear || action.Command == command::prepare_purge;
			expect(action.enabled() == enabled, "safe defaults require explicit preparation before export or destructive confirmation");
		}
	}

	void test_public_two_phase_courses()
	{
		constexpr std::array courses{
			std::pair{ command::preview_diagnostics, command::export_diagnostics },
			std::pair{ command::prepare_clear, command::clear_diagnostics },
			std::pair{ command::prepare_purge, command::purge_profile }
		};
		for (const auto& [preparation, execution] : courses) {
			auto active = current();
			reject_unchanged(active, release_maintenance_intent{ authority(), execution, { } }, reason::no_selection);
			const auto selection = preparation == command::preview_diagnostics ? release_maintenance_selection{ snapshot() } : release_maintenance_selection{ inventory(preparation) };
			const auto prepared  = prepare(active, preparation, selection);
			reject_unchanged(active, release_maintenance_intent{ authority(), execution, { } }, reason::selection_changed);
			const auto pending = begin(active, execution);
			expect(pending.Selection == selection && pending.Request != prepared.Request && pending.Generation != prepared.Generation
					   && !active.Prepared,
				   "execute consumes the exact preview with a fresh operation identity");
			reject_unchanged(active, release_maintenance_intent{ authority(), execution, prepared }, reason::busy);
			reject_unchanged(active, release_maintenance_prepared{ pending, selection }, reason::invalid_completion);
			reject_unchanged(active, release_maintenance_completed{ pending }, reason::invalid_completion);
			expect(transition(active, release_maintenance_commit_started{ pending }).Accepted, "host reports exact irreversible operation");
			reject_unchanged(active, release_maintenance_commit_started{ pending }, reason::invalid_completion);
			reject_unchanged(active, release_maintenance_cancelled{ pending }, reason::point_of_no_return);
			reject_unchanged(active, release_maintenance_intent{ authority(), command::cancel, { } }, reason::point_of_no_return);
			expect(transition(active, release_maintenance_completed{ pending }).Accepted && active.Phase == phase::completed && !active.Pending, "durable success is the only successful effect terminal");
			expect(active.Stopped == (execution == command::purge_profile), "purged runtime cannot resume storage work");
			reject_unchanged(active, release_maintenance_completed{ pending }, reason::stale_completion);
			reject_unchanged(active, release_maintenance_failed{ pending, reason::io_failure }, reason::stale_completion);
			reject_unchanged(active, release_maintenance_cancelled{ pending }, reason::stale_completion);
		}
	}

	void snapshot_result(const release_diagnostic_snapshot& aSnapshot, const reason aExpected)
	{
		auto       active  = current();
		const auto pending = begin(active, command::preview_diagnostics);
		const auto output  = transition(active, release_maintenance_prepared{ pending, aSnapshot });
		expect(output.Accepted && !output.Request && !active.Pending && output.Diagnostic == aExpected, "snapshot observation is bounded and never executes an effect");
		expect(active.Prepared.has_value() == (aExpected == reason::none), "refused snapshot is not previewable or exportable");
	}

	void test_snapshot_privacy_and_bounds()
	{
		auto candidate    = snapshot();
		candidate.Records = release_diagnostic_record_limit;
		candidate.Bytes   = release_diagnostic_byte_limit;
		snapshot_result(candidate, reason::none);
		++candidate.Records;
		snapshot_result(candidate, reason::limit_exceeded);
		candidate       = snapshot();
		candidate.Bytes = release_diagnostic_byte_limit + 1U;
		snapshot_result(candidate, reason::limit_exceeded);
		candidate.Bytes = 0U;
		snapshot_result(candidate, reason::limit_exceeded);
		candidate.Bytes   = 1U;
		candidate.Records = 0U;
		snapshot_result(candidate, reason::none);
		candidate.Revision = { };
		snapshot_result(candidate, reason::invalid_selection);
		candidate        = snapshot();
		candidate.Digest = { };
		snapshot_result(candidate, reason::invalid_selection);
		for (unsigned bit = 0U; bit < 32U; ++bit) {
			candidate        = snapshot();
			candidate.Fields = 1U << bit;
			snapshot_result(candidate, bit < 4U ? reason::none : reason::privacy_refusal);
		}
		candidate.Fields = 0U;
		snapshot_result(candidate, reason::privacy_refusal);
		candidate.Fields = release_diagnostic_default_fields | static_cast<std::uint32_t>(release_diagnostic_field::summary);
		snapshot_result(candidate, reason::privacy_refusal);
		auto       active  = current();
		const auto pending = begin(active, command::preview_diagnostics);
		expect(transition(active, release_maintenance_prepared{ pending, inventory() }).Diagnostic == reason::invalid_selection && !active.Prepared, "inventory cannot masquerade as export metadata");
	}

	void inventory_result(const release_maintenance_inventory& aInventory, const command aCommand, const reason aExpected)
	{
		auto       active  = current();
		const auto pending = begin(active, aCommand);
		const auto output  = transition(active, release_maintenance_prepared{ pending, aInventory });
		expect(output.Accepted && !output.Request && output.Diagnostic == aExpected && !active.Pending, "inventory yields preparation or a reason-coded refusal");
		expect(active.Prepared.has_value() == (aExpected == reason::none), "only complete exact inventories can reach confirmation");
	}

	void test_export_selection_and_confirmation()
	{
		auto                       active = current();
		release_maintenance_intent preview{ authority(), command::preview_diagnostics, { } };
		preview.ExportOptions = { diagnostic_export_policy::identifying, diagnostic_export_scope::current_session, true };
		const auto start      = transition(active, preview);
		expect(start.Accepted && start.Request->Context.ExportOptions == preview.ExportOptions, "exact export options reach preparation");
		auto selected    = snapshot();
		selected.Options = preview.ExportOptions;
		expect(transition(active, release_maintenance_prepared{ *active.Pending, selected }).Accepted && active.Prepared, "rich snapshot prepared");
		release_maintenance_intent save{ authority(), command::export_diagnostics, active.Prepared };
		reject_unchanged(active, save, reason::privacy_refusal);
		save.IdentifyingConfirmed = true;
		const auto exported       = transition(active, save);
		expect(exported.Accepted && exported.Request->Context.ExportOptions == preview.ExportOptions, "identifying confirmation keeps exact selected scope");
		active = current();
		static_cast<void>(transition(active, preview));
		selected.Options.Policy = diagnostic_export_policy::anonymized;
		expect(transition(active, release_maintenance_prepared{ *active.Pending, selected }).Diagnostic == reason::selection_changed && !active.Prepared, "mismatched preview policy cannot be substituted");
		active                       = current();
		preview.ExportOptions.Policy = static_cast<diagnostic_export_policy>(255);
		reject_unchanged(active, preview, reason::invalid_selection);
	}

	void test_purge_scope_and_bounds()
	{
		for (const auto operation : { command::prepare_clear, command::prepare_purge }) {
			auto candidate    = inventory(operation);
			candidate.Entries = 0U;
			inventory_result(candidate, operation, reason::none);
			candidate.Entries = release_maintenance_entry_limit;
			inventory_result(candidate, operation, reason::none);
			++candidate.Entries;
			inventory_result(candidate, operation, reason::limit_exceeded);
			candidate          = inventory(operation);
			candidate.Complete = false;
			inventory_result(candidate, operation, reason::incomplete_inventory);
			candidate.Complete       = true;
			candidate.UnknownEntries = 1U;
			inventory_result(candidate, operation, reason::incomplete_inventory);
			candidate        = inventory(operation);
			candidate.Digest = { };
			inventory_result(candidate, operation, reason::invalid_selection);
			candidate          = inventory(operation);
			candidate.Revision = { };
			inventory_result(candidate, operation, reason::invalid_selection);
			for (unsigned bit = 0U; bit < 32U; ++bit) {
				candidate = inventory(operation);
				candidate.Categories ^= 1U << bit;
				inventory_result(candidate, operation, reason::invalid_selection);
			}
		}
		inventory_result(inventory(), command::prepare_clear, reason::invalid_selection);
		auto       active  = current();
		const auto pending = begin(active, command::prepare_purge);
		expect(transition(active, release_maintenance_prepared{ pending, snapshot() }).Diagnostic == reason::invalid_selection, "a diagnostic snapshot never authorizes purge");
	}

	auto changed_contexts(const context& aContext) -> std::vector<context>
	{
		auto variants                 = std::vector<context>(7U, aContext);
		variants[0].Authority.Profile = bpr_profile_id{ 99U };
		variants[1].Authority.Session = application_session_id{ 99U };
		variants[2].Authority.Root    = release_maintenance_root_id{ 99U };
		variants[3].Request           = request_id{ 99U };
		variants[4].Generation        = release_maintenance_generation{ 99U };
		variants[5].Operation         = command::cancel;
		variants[6].Selection         = inventory();
		return variants;
	}

	void test_exact_preview_and_completion_identity()
	{
		auto       active   = current();
		const auto prepared = prepare(active, command::preview_diagnostics, snapshot());
		for (const auto& variant : changed_contexts(prepared)) {
			reject_unchanged(active, release_maintenance_intent{ authority(), command::export_diagnostics, variant }, reason::selection_changed);
		}
		auto variants                                                         = std::vector<context>(6U, prepared);
		std::get<release_diagnostic_snapshot>(variants[0].Selection).Revision = release_maintenance_revision{ 99U };
		std::get<release_diagnostic_snapshot>(variants[1].Selection).Digest[31] ^= 1U;
		++std::get<release_diagnostic_snapshot>(variants[2].Selection).Records;
		++std::get<release_diagnostic_snapshot>(variants[3].Selection).Bytes;
		++std::get<release_diagnostic_snapshot>(variants[4].Selection).OmittedRecords;
		std::get<release_diagnostic_snapshot>(variants[5].Selection).Fields = 1U;
		for (const auto& variant : variants) {
			reject_unchanged(active, release_maintenance_intent{ authority(), command::export_diagnostics, variant }, reason::selection_changed);
		}
		reject_unchanged(active, release_maintenance_intent{ authority(), command::purge_profile, prepared }, reason::selection_changed);
		const auto pending = begin(active, command::export_diagnostics);
		for (const auto& variant : changed_contexts(pending)) {
			reject_unchanged(active, release_maintenance_intent{ authority(), command::cancel, variant }, reason::selection_changed);
			reject_unchanged(active, release_maintenance_prepared{ variant, snapshot() }, reason::stale_completion);
			reject_unchanged(active, release_maintenance_commit_started{ variant }, reason::stale_completion);
			reject_unchanged(active, release_maintenance_completed{ variant }, reason::stale_completion);
			reject_unchanged(active, release_maintenance_failed{ variant, reason::io_failure }, reason::stale_completion);
			reject_unchanged(active, release_maintenance_cancelled{ variant }, reason::stale_completion);
		}
		auto exportChanged = pending;
		++std::get<release_diagnostic_snapshot>(exportChanged.Selection).Bytes;
		reject_unchanged(active, release_maintenance_completed{ exportChanged }, reason::stale_completion);
		reject_unchanged(active, release_maintenance_failed{ pending, reason::none }, reason::invalid_completion);
	}

	void test_cancellation_and_durable_truth()
	{
		auto active       = current();
		auto pending      = begin(active, command::preview_diagnostics);
		auto cancellation = gesture(active, command::cancel);
		expect(cancellation.Request == release_maintenance_request{ pending, true } && active.Pending, "advisory cancellation retains the owed terminal");
		reject_unchanged(active, release_maintenance_intent{ authority(), command::cancel, { } }, reason::no_selection);
		expect(transition(active, release_maintenance_prepared{ pending, snapshot() }).Accepted && !active.Prepared && active.Phase == phase::cancelled, "late preparation after cancellation never publishes a preview");
		prepare(active, command::prepare_clear, inventory(command::prepare_clear));
		expect(gesture(active, command::cancel).SelectionChanged && !active.Prepared, "cancel dismisses a destructive preview without effects");
		prepare(active, command::prepare_clear, inventory(command::prepare_clear));
		pending = begin(active, command::clear_diagnostics);
		gesture(active, command::cancel);
		expect(transition(active, release_maintenance_cancelled{ pending }).Accepted && active.Phase == phase::cancelled, "pre-commit cancellation reports no deletion");
		prepare(active, command::prepare_clear, inventory(command::prepare_clear));
		pending = begin(active, command::clear_diagnostics);
		gesture(active, command::cancel);
		expect(transition(active, release_maintenance_commit_started{ pending }).Accepted, "irreversible work may have raced advisory cancellation");
		expect(transition(active, release_maintenance_completed{ pending }).Accepted && active.Phase == phase::completed, "durable success wins that cancellation race");
		prepare(active, command::prepare_purge, inventory());
		pending = begin(active, command::purge_profile);
		expect(transition(active, release_maintenance_commit_started{ pending }).Accepted, "purge commits under its exact context");
		expect(transition(active, release_maintenance_failed{ pending, reason::io_failure }).Diagnostic == reason::recovery_required && active.RecoveryRequired, "partial destructive failure needs host recovery, never success or cancellation");
		reject_unchanged(active, release_maintenance_intent{ authority(), command::prepare_purge, { } }, reason::recovery_required);
		expect(transition(active, release_maintenance_authority_changed{ authority(), Available }).Accepted && !active.RecoveryRequired && active.Reason == reason::none, "only host readmission after recovery reopens work");
		prepare(active, command::preview_diagnostics, snapshot());
		pending = begin(active, command::export_diagnostics);
		expect(transition(active, release_maintenance_commit_started{ pending }).Accepted, "export commits under its exact context");
		expect(transition(active, release_maintenance_failed{ pending, reason::io_failure }).Diagnostic == reason::io_failure && !active.Prepared, "export failure consumes the preview and remains visible");
		// [[BPR-1038]]. A refusal because storage is not private is an
		// operational failure under its own reason, and before any commit it
		// needs no recovery.
		pending = begin(active, command::prepare_purge);
		expect(transition(active, release_maintenance_failed{ pending, reason::storage_not_private }).Diagnostic == reason::storage_not_private
				   && active.Phase == phase::failed && !active.RecoveryRequired,
			   "a storage privacy refusal fails preparation under its own reason");
	}

	void test_inventory_confirmation_and_capability_revocation()
	{
		auto       active                                                       = current();
		const auto prepared                                                     = prepare(active, command::prepare_purge, inventory());
		auto       variants                                                     = std::vector<context>(6U, prepared);
		std::get<release_maintenance_inventory>(variants[0].Selection).Revision = release_maintenance_revision{ 99U };
		std::get<release_maintenance_inventory>(variants[1].Selection).Digest[1] ^= 1U;
		std::get<release_maintenance_inventory>(variants[2].Selection).Categories = 1U;
		++std::get<release_maintenance_inventory>(variants[3].Selection).Entries;
		++std::get<release_maintenance_inventory>(variants[4].Selection).UnknownEntries;
		std::get<release_maintenance_inventory>(variants[5].Selection).Complete = false;
		for (const auto& variant : variants) {
			reject_unchanged(active, release_maintenance_intent{ authority(), command::purge_profile, variant }, reason::selection_changed);
			reject_unchanged(active, release_maintenance_intent{ authority(), command::cancel, variant }, reason::selection_changed);
		}
		expect(transition(active, release_maintenance_authority_changed{ authority(), { } }).SelectionChanged && !active.Prepared, "capability revocation invalidates a ready destructive confirmation");
		reject_unchanged(active, release_maintenance_intent{ authority(), command::purge_profile, prepared }, reason::unavailable);
		active = current();
		prepare(active, command::prepare_purge, inventory());
		const auto pending = begin(active, command::purge_profile);
		expect(transition(active, release_maintenance_commit_started{ pending }).Accepted, "exact purge enters irreversible phase");
		const auto invalidated = transition(active, release_maintenance_invalidated{ authority() });
		expect(invalidated.Diagnostic == reason::recovery_required && !invalidated.Request && active.RecoveryRequired, "retiring irreversible purge preserves a recovery obligation");
		reject_unchanged(active, release_maintenance_completed{ pending }, reason::stale_completion);
		reject_unchanged(active, release_maintenance_intent{ authority(), command::prepare_clear, { } }, reason::recovery_required);
	}

	void test_replacement_invalidation_and_shutdown()
	{
		for (const auto operation : { command::preview_diagnostics, command::prepare_clear, command::prepare_purge }) {
			auto       active      = current();
			const auto pending     = begin(active, operation);
			auto       replacement = authority();
			replacement.Root       = release_maintenance_root_id{ 9U };
			const auto replaced    = transition(active, release_maintenance_authority_changed{ replacement, Available });
			expect(replaced.AuthorityChanged && replaced.Request == release_maintenance_request{ pending, true } && !active.Pending, "root replacement retires and cancels the exact old request");
			reject_unchanged(active, release_maintenance_prepared{ pending, snapshot() }, reason::stale_completion);
			reject_unchanged(active, release_maintenance_intent{ authority(), operation, { } }, reason::invalid_authority);
			reject_unchanged(active, release_maintenance_invalidated{ authority() }, reason::invalid_authority);
		}
		auto       active   = current();
		const auto prepared = prepare(active, command::preview_diagnostics, snapshot());
		expect(transition(active, release_maintenance_invalidated{ authority() }).SelectionChanged && !active.Prepared, "changed source invalidates its accepted preview");
		reject_unchanged(active, release_maintenance_intent{ authority(), command::export_diagnostics, prepared }, reason::no_selection);
		prepare(active, command::preview_diagnostics, snapshot());
		const auto pending     = begin(active, command::export_diagnostics);
		const auto invalidated = transition(active, release_maintenance_invalidated{ authority() });
		expect(invalidated.Request == release_maintenance_request{ pending, true }, "selection change cancels pending export publication");
		reject_unchanged(active, release_maintenance_completed{ pending }, reason::stale_completion);
		const auto fresh = begin(active, command::preview_diagnostics);
		expect(fresh.Generation != pending.Generation && fresh.Request != pending.Request, "same-profile retry cannot reuse stale identity");
		const auto stopped = transition(active, release_maintenance_shutdown{ });
		expect(stopped.Request == release_maintenance_request{ fresh, true } && active.Stopped, "shutdown cancels and closes admission");
		reject_unchanged(active, release_maintenance_prepared{ fresh, snapshot() }, reason::stale_completion);
		reject_unchanged(active, release_maintenance_authority_changed{ authority(), Available }, reason::shutdown);
		active               = current();
		const auto preparing = begin(active, command::prepare_purge);
		const auto revoked   = transition(active, release_maintenance_authority_changed{ { }, Available });
		expect(revoked.Request == release_maintenance_request{ preparing, true } && active.Authority == release_maintenance_authority{ } && active.Capabilities == release_maintenance_capabilities{ }, "invalid replacement fails closed and revokes prior authority");
	}

	void test_identity_exhaustion_and_replay()
	{
		constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
		auto           active  = current();
		active.NextRequest     = request_id{ maximum };
		reject_unchanged(active, release_maintenance_intent{ authority(), command::preview_diagnostics, { } }, reason::identity_exhausted);
		active             = current();
		active.Generation  = release_maintenance_generation{ maximum - 1U };
		const auto pending = begin(active, command::preview_diagnostics);
		expect(pending.Generation.value() == maximum, "last generation is exact and cannot wrap");
		expect(gesture(active, command::cancel).Request.has_value(), "exhaustion cannot suppress owed cancellation");
		expect(transition(active, release_maintenance_cancelled{ pending }).Accepted, "last request still receives its terminal");
		reject_unchanged(active, release_maintenance_intent{ authority(), command::preview_diagnostics, { } }, reason::identity_exhausted);
		expect(transition(active, release_maintenance_invalidated{ authority() }).Accepted, "exhausted state still invalidates");
		expect(active.Exhausted && active.Generation.value() == maximum, "invalidation saturates and permanently closes exhausted identity");

		auto left  = current();
		auto right = current();
		for (std::uint32_t iteration = 0U; iteration < 256U; ++iteration) {
			const release_maintenance_event start = release_maintenance_intent{ authority(), command::preview_diagnostics, { } };
			expect(transition(left, start) == transition(right, start) && left == right, "same input produces identical request and state");
			auto candidate                             = snapshot();
			candidate.Records                          = iteration * 8U;
			const release_maintenance_event completion = release_maintenance_prepared{ *left.Pending, candidate };
			expect(transition(left, completion) == transition(right, completion) && left == right, "bounded accept/refuse replay is deterministic");
			expect(left.Prepared.has_value() == (candidate.Records <= release_diagnostic_record_limit), "replay retains only permitted bounded selections");
			reject_unchanged(left, completion, reason::stale_completion);
			reject_unchanged(right, completion, reason::stale_completion);
		}
	}
}

namespace {
	/// [[BPR-1722]]. A saved export is named for its local date and
	/// time, to the minute, and the version: sortable, with no character a
	/// file system refuses and nothing that could name a host, account or
	/// repository.
	void test_export_file_name()
	{
		const auto at = diagnostic_export_local_time{ .Year = 2026, .Month = 10, .Day = 21, .Hour = 14, .Minute = 32 };
		expect(diagnostic_export_file_name(at, "0.3.1") == "BetterPullReview-diagnostics-2026-10-21-1432-0.3.1.zip", "the name holds the local date, time and version");
		expect(diagnostic_export_file_name({ .Year = 2027, .Month = 1, .Day = 2, .Hour = 3, .Minute = 4 }, "1.0") == "BetterPullReview-diagnostics-2027-01-02-0304-1.0.zip", "every field is zero padded, so names sort by time");
		expect(diagnostic_export_file_name({ .Year = 2026, .Month = 10, .Day = 21, .Hour = 9, .Minute = 5 }, "0.3.1") < diagnostic_export_file_name(at, "0.3.1"), "an earlier export sorts first");
		for (const std::string_view version : { "", "0.3.1-dirty", "ghe.example.invalid", "octo/repo", "0..3", ".3", "3.", "0.3.1 ", "0:3" }) {
			expect(diagnostic_export_file_name(at, version) == "BetterPullReview-diagnostics-2026-10-21-1432.zip", "a version that is not dotted numbers is left out");
		}
		for (const auto& impossible : { diagnostic_export_local_time{ }, diagnostic_export_local_time{ .Year = 2026, .Month = 2, .Day = 30, .Hour = 1, .Minute = 1 }, diagnostic_export_local_time{ .Year = 2026, .Month = 10, .Day = 21, .Hour = 24, .Minute = 0 },
										diagnostic_export_local_time{ .Year = 2026, .Month = 10, .Day = 21, .Hour = 23, .Minute = 60 }, diagnostic_export_local_time{ .Year = 10000, .Month = 1, .Day = 1, .Hour = 0, .Minute = 0 } }) {
			expect(diagnostic_export_file_name(impossible, "0.3.1") == "BetterPullReview-diagnostics-0.3.1.zip", "an impossible time is left out");
		}
		for (const auto& name : { diagnostic_export_file_name(at, "0.3.1"), diagnostic_export_file_name(at, "ghe.example.invalid") }) {
			expect(name.find_first_of(":/\\<>|?*\" ") == std::string::npos, "no colon, separator, space or other character a file system refuses");
		}
	}

	auto get16(const std::string_view aBytes, const std::size_t aAt) -> std::uint32_t
	{
		expect(aAt + 2U <= aBytes.size(), "a field lies inside the archive");
		return static_cast<std::uint32_t>(static_cast<unsigned char>(aBytes[aAt])) | (static_cast<std::uint32_t>(static_cast<unsigned char>(aBytes[aAt + 1U])) << 8U);
	}
	auto get32(const std::string_view aBytes, const std::size_t aAt) -> std::uint32_t { return get16(aBytes, aAt) | (get16(aBytes, aAt + 2U) << 16U); }

	/// \brief The one entry of a single-entry archive, read field by field
	/// from its local header, central directory entry and end record, which
	/// must agree with each other and end the archive exactly.
	struct archive_entry
	{
		std::uint32_t Flags      = 0U;
		std::uint32_t Method     = 0U;
		std::uint32_t Time       = 0U;
		std::uint32_t Date       = 0U;
		std::uint32_t Crc        = 0U;
		std::uint32_t Compressed = 0U;
		std::uint32_t Size       = 0U;
		std::uint32_t Attributes = 0U;
		std::string   Name;
	};
	auto single_entry(const std::string_view aArchive) -> archive_entry
	{
		expect(get32(aArchive, 0U) == 0x04034B50U, "the archive begins with a local file header");
		auto entry = archive_entry{ .Flags = get16(aArchive, 6U), .Method = get16(aArchive, 8U), .Time = get16(aArchive, 10U), .Date = get16(aArchive, 12U), .Crc = get32(aArchive, 14U), .Compressed = get32(aArchive, 18U), .Size = get32(aArchive, 22U) };
		expect(get16(aArchive, 4U) == 20U && get16(aArchive, 28U) == 0U, "the local header needs version 2.0 and has no extra field");
		const auto nameBytes = get16(aArchive, 26U);
		entry.Name           = std::string{ aArchive.substr(30U, nameBytes) };
		const auto central   = std::size_t{ 30U } + nameBytes + entry.Compressed;
		expect(get32(aArchive, central) == 0x02014B50U, "the central directory follows the entry's data");
		expect(get16(aArchive, central + 6U) == 20U && get16(aArchive, central + 8U) == entry.Flags && get16(aArchive, central + 10U) == entry.Method && get16(aArchive, central + 12U) == entry.Time
				   && get16(aArchive, central + 14U) == entry.Date && get32(aArchive, central + 16U) == entry.Crc && get32(aArchive, central + 20U) == entry.Compressed && get32(aArchive, central + 24U) == entry.Size
				   && get16(aArchive, central + 28U) == nameBytes && get16(aArchive, central + 30U) == 0U && get16(aArchive, central + 32U) == 0U && get32(aArchive, central + 42U) == 0U
				   && aArchive.substr(central + 46U, nameBytes) == entry.Name,
			   "the central directory entry repeats the local header and points at it");
		entry.Attributes = get32(aArchive, central + 38U);
		const auto end   = central + 46U + nameBytes;
		expect(get32(aArchive, end) == 0x06054B50U && get16(aArchive, end + 4U) == 0U && get16(aArchive, end + 6U) == 0U && get16(aArchive, end + 8U) == 1U && get16(aArchive, end + 10U) == 1U
				   && get32(aArchive, end + 12U) == 46U + nameBytes && get32(aArchive, end + 16U) == central && get16(aArchive, end + 20U) == 0U && end + 22U == aArchive.size(),
			   "one end record on disk 0 counts one entry, sizes and places the directory, and ends the archive");
		return entry;
	}

	/// [[BPR-PENDING-AG]]. A saved export is a standard single-entry ZIP: the
	/// text deflated under the `.txt` name, with its CRC-32, sizes, UTF-8 name
	/// and local time, which zlib's inflate reads back byte for byte.
	void test_export_archive()
	{
		std::string text = "{\"format\":\"bpr-diagnostics-v2\",\"exported_at\":\"2026-10-21T14:32:05-07:00\",\"app_version\":\"0.3.1\"}\n";
		for (unsigned sequence = 1U; sequence <= 300U; ++sequence) {
			text += "{\"v\":3,\"event\":\"github.pull_request.succeeded\",\"utc_ms\":" + std::to_string(1780000000000ULL + (sequence * 3617ULL)) + ",\"sequence\":" + std::to_string(sequence)
				+ ",\"op\":\"github.pull_request\",\"outcome\":\"succeeded\",\"elapsed_us\":" + std::to_string((sequence * 7919U) % 100000U) + ",\"request_label\":\"request-" + std::to_string(sequence % 17U) + "\"}\n";
		}
		const auto at      = diagnostic_export_local_time{ .Year = 2026, .Month = 10, .Day = 21, .Hour = 14, .Minute = 32, .Second = 5 };
		const auto file    = diagnostic_export_file_name(at, "0.3.1");
		const auto archive = diagnostic_export_archive(file, text, at);
		const auto entry   = single_entry(archive);
		expect(entry.Name == "BetterPullReview-diagnostics-2026-10-21-1432-0.3.1.txt", "the entry keeps the export's .txt name");
		expect(entry.Method == 8U && entry.Flags == 0x0800U, "the entry is deflated, unencrypted, with a UTF-8 name and its sizes in the header");
		expect(entry.Crc == cycle_crc32(text) && entry.Size == text.size() && entry.Compressed == archive.size() - 30U - 46U - 22U - (2U * entry.Name.size()), "the CRC-32 and both sizes are the text's");
		expect(entry.Date == ((46U << 9U) | (10U << 5U) | 21U) && entry.Time == ((14U << 11U) | (32U << 5U) | 2U), "the entry is dated 2026-10-21 14:32:04, the local time to two seconds");
		expect((entry.Attributes >> 16U) == 0100600U, "a regular file, owner-only once extracted, as the saved file is");
		const auto data = std::span{ reinterpret_cast<const std::uint8_t*>(archive.data()) + 30U + entry.Name.size(), entry.Compressed };
		expect(inflate_cycle_deflate(data, text.size()) == text, "zlib's raw inflate gives back the text byte for byte");
		const auto read = read_cycle_zip({ reinterpret_cast<const std::uint8_t*>(archive.data()), archive.size() });
		expect(read && read->size() == 1U && read->front().Name == entry.Name && read->front().Bytes == text, "the archive reader admits it and returns exactly the text");
		expect(diagnostic_export_archive(file, text, at) == archive, "equal inputs give equal bytes");
		expect(archive.size() * 5U < text.size(), "repetitive records compress at least five times");

		// A text deflate cannot shrink is stored, with the standard CRC-32.
		const auto check = diagnostic_export_archive("check.zip", "123456789", at);
		const auto small = single_entry(check);
		expect(small.Name == "check.txt" && small.Method == 0U && small.Crc == 0xCBF43926U && small.Size == 9U && small.Compressed == 9U && check.substr(30U + small.Name.size(), 9U) == "123456789",
			   "a text deflate cannot shrink is stored, with CRC-32 CBF43926 for 123456789");
		expect(single_entry(diagnostic_export_archive("\xC3\xA9.zip", "x", at)).Name == "\xC3\xA9.txt", "a non-ASCII name is kept as UTF-8");
		for (const auto& impossible : { diagnostic_export_local_time{ }, diagnostic_export_local_time{ .Year = 1979, .Month = 12, .Day = 31, .Hour = 23, .Minute = 59 }, diagnostic_export_local_time{ .Year = 2108, .Month = 1, .Day = 1 },
										diagnostic_export_local_time{ .Year = 2026, .Month = 2, .Day = 30 }, diagnostic_export_local_time{ .Year = 2026, .Month = 10, .Day = 21, .Second = 60 } }) {
			const auto dated = single_entry(diagnostic_export_archive(file, "x", impossible));
			expect(dated.Date == 0x21U && dated.Time == 0U, "a time the format cannot hold gives 1980-01-01 00:00");
		}
	}

	/// [[BPR-PENDING-AG]]. The entry is named for the file, with `.txt`.
	void test_export_entry_name()
	{
		expect(diagnostic_export_entry_name("BetterPullReview-diagnostics-2026-10-21-1432-0.3.1.zip") == "BetterPullReview-diagnostics-2026-10-21-1432-0.3.1.txt", "a final .zip becomes .txt");
		expect(diagnostic_export_entry_name("Bug 42.ZIP") == "Bug 42.txt", "in any case");
		expect(diagnostic_export_entry_name("notes") == "notes.txt" && diagnostic_export_entry_name("notes.txt") == "notes.txt" && diagnostic_export_entry_name("a.zip.zip") == "a.zip.txt", "a name gains .txt once");
		expect(diagnostic_export_entry_name("diagnostic-\xC3\xA9.zip") == "diagnostic-\xC3\xA9.txt", "a UTF-8 name is kept");
		using namespace std::string_view_literals;
		for (const auto refused : { ""sv, ".zip"sv, "a/b.zip"sv, "a\\b.zip"sv, "a\nb.zip"sv, "a\0b.zip"sv, "\xFF.zip"sv, ".."sv }) {
			expect(diagnostic_export_entry_name(refused) == "BetterPullReview-diagnostics.txt", "an empty, separated, controlled or non-UTF-8 name gives the default");
		}
		expect(diagnostic_export_entry_name(std::string(256U, 'a')) == "BetterPullReview-diagnostics.txt", "a name over 255 bytes gives the default");
	}
}

auto main() -> int
{
	test_export_file_name();
	test_export_entry_name();
	test_export_archive();
	test_safe_defaults_and_closed_fields();
	test_public_two_phase_courses();
	test_snapshot_privacy_and_bounds();
	test_export_selection_and_confirmation();
	test_purge_scope_and_bounds();
	test_exact_preview_and_completion_identity();
	test_cancellation_and_durable_truth();
	test_inventory_confirmation_and_capability_revocation();
	test_replacement_invalidation_and_shutdown();
	test_identity_exhaustion_and_replay();
	std::cout << "release-maintenance-tests:pass\n";
	return EXIT_SUCCESS;
}
