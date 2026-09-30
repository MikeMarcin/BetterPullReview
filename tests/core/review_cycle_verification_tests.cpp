#include "bpr/core/review_cycle_verification.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

/// \file
/// \brief Reconciliation, explicit reviewer verification bound to checked
/// material, and the next round's carried-forward selection (BPR-1165).

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const char* aWhat)
	{
		if (!aValue) {
			std::cerr << "review cycle verification failure: " << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto digest(const std::uint8_t aSeed) -> cycle_material_digest
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[0]   = aSeed;
		return cycle_material_digest{ bytes };
	}

	auto entry(std::string aPath, const std::uint8_t aContent) -> manifest_entry
	{
		return { .Path = std::move(aPath), .Mode = git_mode_regular, .Size = 1, .Content = digest(aContent) };
	}

	auto manifest(std::vector<manifest_entry> aEntries) -> cycle_manifest
	{
		return { .ScopeVersion = 1, .Coverage = cycle_capture_coverage::complete, .Entries = std::move(aEntries) };
	}

	auto thread(const std::uint64_t aItem, cycle_feedback_target aPrimary, std::vector<cycle_related_reference> aRelated = { }) -> cycle_feedback_thread
	{
		return { .Id = { .Cycle = review_cycle_id{ 1 }, .Id = cycle_feedback_item_id{ aItem } }, .Primary = std::move(aPrimary), .Related = std::move(aRelated) };
	}

	auto anchor(const std::string& aPath) -> cycle_source_anchor
	{
		return { .Revision = cycle_source_revision_id{ 1 }, .Path = aPath, .Blob = digest(1), .Start = 0, .End = 1, .Original = "x" };
	}

	auto decision(const std::uint64_t aId, const std::uint64_t aWording, const cycle_material_digest& aMaterial, const std::optional<cycle_reviewer_decision> aDecision)
		-> cycle_verification_record
	{
		return { .Id = { .Cycle = review_cycle_id{ 1 }, .Id = cycle_verification_id{ aId } }, .Item = cycle_feedback_item_id{ 1 }, .Wording = private_review_turn_revision_id{ aWording }, .Checked = cycle_source_revision_id{ 2 }, .Material = aMaterial, .Decision = aDecision };
	}

	void binds_checked_material()
	{
		const auto note  = thread(1, anchor("src/a.cpp"), { { .Target = cycle_file_target{ .Revision = cycle_source_revision_id{ 1 }, .Path = "src/b.cpp", .Blob = digest(2) } } });
		const auto r2    = manifest({ entry("src/a.cpp", 11), entry("src/b.cpp", 12), entry("src/c.cpp", 13) });
		const auto base  = verification_material(note, r2, digest(90));
		auto       other = r2;
		other.Entries[2] = entry("src/c.cpp", 99);
		expect(verification_material(note, other, digest(91)) == base, "an unrelated file changing leaves the checked material alone");
		auto changed       = r2;
		changed.Entries[0] = entry("src/a.cpp", 77);
		expect(verification_material(note, changed, digest(92)) != base, "the note's own file changing is relevant");
		auto related       = r2;
		related.Entries[1] = entry("src/b.cpp", 78);
		expect(verification_material(note, related, digest(93)) != base, "a related location's file is relevant too");
		const auto deleted = manifest({ entry("src/b.cpp", 12), entry("src/c.cpp", 13) });
		expect(verification_material(note, deleted, digest(94)) != base, "a missing file is relevant, never another file's bytes");
		expect(verification_material(thread(2, cycle_module_target{ .Path = "src" }), r2, digest(95)) == digest(95), "a module note is about everything captured");
		expect(verification_material(thread(3, cycle_whole_target{ }), r2, digest(96)) == digest(96), "so is a whole-cycle note");

		const auto record = decision(1, 1, base, cycle_reviewer_decision::verified);
		expect(verification_currency(record, private_review_turn_revision_id{ 1 }, base) == cycle_verification_currency::current, "current");
		expect(verification_currency(record, private_review_turn_revision_id{ 1 }, verification_material(note, changed, digest(92))) == cycle_verification_currency::material_changed, "the material changed");
		expect(verification_currency(record, private_review_turn_revision_id{ 2 }, base) == cycle_verification_currency::item_amended, "the note was amended");
	}

	void checks_decisions_before_recording()
	{
		const auto authority = local_cycle_authority{ .Profile = bpr_profile_id{ 1 }, .Incarnation = cycle_catalog_incarnation{ std::array<std::uint8_t, 32>{ 1 } } };
		auto       cycle     = create({ .Authority = authority }, { .Authority = authority, .Title = "Verify" })->Cycle;
		const auto note      = thread(1, cycle_whole_target{ });
		expect(check_verification(cycle, note, private_review_turn_revision_id{ 1 }, decision(1, 1, digest(1), cycle_reviewer_decision::verified)) == cycle_refusal::invalid_target, "nothing captured to check");
		for (std::uint8_t material = 1; material <= 2; ++material) {
			cycle = apply(cycle, { .Subject = cycle.Subject, .ExpectedRevision = cycle.Revision, .Change = record_cycle_capture{ .Capture = { .ScopeVersion = 1, .Material = digest(material), .Coverage = cycle_capture_coverage::complete } } }).Accepted;
		}
		expect(!check_verification(cycle, note, private_review_turn_revision_id{ 1 }, decision(1, 1, digest(1), cycle_reviewer_decision::verified)), "the current wording against a captured revision");
		expect(check_verification(cycle, note, private_review_turn_revision_id{ 2 }, decision(1, 1, digest(1), cycle_reviewer_decision::verified)) == cycle_refusal::conflict, "an older wording is not what is in front of the reviewer");
		auto wrong    = decision(1, 1, digest(1), cycle_reviewer_decision::verified);
		wrong.Checked = cycle_source_revision_id{ 5 };
		expect(check_verification(cycle, note, private_review_turn_revision_id{ 1 }, wrong) == cycle_refusal::invalid_target, "an uncaptured revision");
		wrong      = decision(1, 1, digest(1), cycle_reviewer_decision::verified);
		wrong.Item = cycle_feedback_item_id{ 2 };
		expect(check_verification(cycle, note, private_review_turn_revision_id{ 1 }, wrong) == cycle_refusal::invalid_identity, "another note");
	}

	void never_resolves_by_itself()
	{
		const auto note      = thread(1, anchor("src/a.cpp"));
		const auto material  = digest(40);
		const auto addressed = std::vector<cycle_item_statement>{ { .Response = cycle_response_id{ 1 }, .Round = cycle_round_id{ 1 }, .From = "Harness", .Disposition = cycle_response_disposition::addressed, .Body = "Fixed; tests pass." } };
		const auto gone      = cycle_target_correspondence{ .Kind = cycle_correspondence::unmapped, .Against = cycle_source_revision_id{ 2 } };
		const auto applied   = std::vector<cycle_suggestion_reconciliation>{ { .Suggestion = cycle_suggestion_id{ 1 }, .Observation = cycle_application_observation::already_applied } };
		auto       inputs    = cycle_reconciliation_inputs{ .Thread = &note, .Wording = private_review_turn_revision_id{ 1 }, .Against = cycle_source_revision_id{ 2 }, .Material = material, .Primary = gone, .Suggestions = applied, .Statements = addressed };
		auto       item      = reconcile_item(inputs);
		expect(item.Status == cycle_item_status::needs_verification && !resolved(item), "addressed, code gone and the suggestion present: still only needs verification");
		expect(item.Primary->Kind == cycle_correspondence::unmapped && item.Suggestions[0].Observation == cycle_application_observation::already_applied && item.Claim == cycle_response_disposition::addressed,
			   "each fact kept apart");

		const auto verified = std::vector{ decision(1, 1, material, cycle_reviewer_decision::verified) };
		inputs.Decisions    = verified;
		item                = reconcile_item(inputs);
		expect(item.Status == cycle_item_status::verified && resolved(item) && item.Currency == cycle_verification_currency::current, "the reviewer's decision verifies");
		inputs.Material = digest(41);
		item            = reconcile_item(inputs);
		expect(item.Status == cycle_item_status::recheck_needed && !resolved(item) && item.Decision == verified[0], "changed material: recheck needed, the decision kept as history");
		inputs.Material = material;
		inputs.Wording  = private_review_turn_revision_id{ 2 };
		item            = reconcile_item(inputs);
		expect(item.Status == cycle_item_status::recheck_needed && item.Currency == cycle_verification_currency::item_amended, "an amended note needs checking again");
		inputs.Wording     = private_review_turn_revision_id{ 1 };
		const auto cleared = std::vector{ verified[0], decision(2, 1, material, std::nullopt) };
		inputs.Decisions   = cleared;
		expect(reconcile_item(inputs).Status == cycle_item_status::needs_verification, "an explicitly cleared decision leaves the claim");
		const auto withdrawn = std::vector{ decision(3, 1, digest(7), cycle_reviewer_decision::withdrawn) };
		inputs.Decisions     = withdrawn;
		expect(resolved(reconcile_item(inputs)), "withdrawn closes the note");
		const auto deferred = std::vector{ decision(4, 1, material, cycle_reviewer_decision::deferred) };
		inputs.Decisions    = deferred;
		expect(reconcile_item(inputs).Status == cycle_item_status::deferred && !resolved(reconcile_item(inputs)), "deferred stays open");

		const auto prose = std::vector<cycle_item_statement>{ { .Response = cycle_response_id{ 2 }, .Round = cycle_round_id{ 1 }, .From = "Dana", .Body = "Not sure." } };
		expect(reconcile_item({ .Thread = &note, .Wording = private_review_turn_revision_id{ 1 }, .Statements = prose }).Status == cycle_item_status::reported, "prose only: reported, not verified");
		expect(reconcile_item({ .Thread = &note, .Wording = private_review_turn_revision_id{ 1 }, .Primary = gone }).Status == cycle_item_status::open, "code disappearing alone changes nothing");
	}

	void plans_the_next_round()
	{
		const auto id       = [](const std::uint64_t aValue) { return cycle_feedback_item_id{ aValue }; };
		const auto wording  = [](const std::uint64_t aValue) { return private_review_turn_revision_id{ aValue }; };
		const auto previous = cycle_round_selection{
			.Source      = cycle_source_revision_id{ 1 },
			.Action      = cycle_round_action::implement,
			.Goal        = "Harden parse.",
			.Constraints = { "Keep the API." },
			.WriteScope  = { "src" },
			.Items       = {
				{ .Item = id(1), .Wording = wording(1) },
				{ .Item = id(2), .Wording = wording(1), .Action = cycle_round_action::discuss, .Related = { id(1), id(4) } },
				{ .Item = id(3), .Wording = wording(1) },
				{ .Item = id(4), .Wording = wording(1), .Suggestions = { { .Id = cycle_suggestion_id{ 1 }, .Revision = 1 } } },
			},
			.Context   = { { .Revision = cycle_source_revision_id{ 1 }, .Path = "src/a.cpp" } },
			.Omissions = { "Left out the style notes." },
		};
		const auto items = std::vector<cycle_next_round_item>{
			{ .Item = id(1), .Wording = wording(1), .InRound = true, .Resolved = true },
			{ .Item = id(2), .Wording = wording(2), .InRound = true },
			{ .Item = id(3), .Wording = wording(1), .Lifecycle = private_thread_lifecycle::archived, .InRound = true },
			{ .Item = id(4), .Wording = wording(1), .Suggestions = { { .Id = cycle_suggestion_id{ 1 }, .Revision = 2 } }, .InRound = true },
			{ .Item = id(5), .Wording = wording(1) },
			{ .Item = id(6), .Wording = wording(1), .Lifecycle = private_thread_lifecycle::resolved_locally },
			{ .Item = id(7), .Wording = wording(1), .InRound = true },
		};
		const auto  plan = plan_next_round(previous, items, cycle_source_revision_id{ 3 });
		const auto& next = plan.Selection;
		expect(next.Source == cycle_source_revision_id{ 3 } && next.Action == previous.Action && next.Goal == previous.Goal && next.Constraints == previous.Constraints && next.WriteScope == previous.WriteScope,
			   "the previous assignment carried, against the new revision");
		expect(plan.Carried == std::vector{ id(2), id(4) } && plan.Added == std::vector{ id(5) } && plan.Resolved == std::vector{ id(1) } && plan.Archived == std::vector{ id(3) }, "open items carried, new notes added, resolved and archived left out");
		expect(next.Items.size() == 3U && next.Items[0].Item == id(2) && next.Items[0].Wording == wording(2) && next.Items[0].Action == cycle_round_action::discuss, "an amended item carries its new wording and its own action");
		expect(next.Items[0].Related == std::vector{ id(4) }, "relations only to items still in the round");
		expect(next.Items[1].Suggestions == std::vector<cycle_round_suggestion_pin>{ { .Id = cycle_suggestion_id{ 1 }, .Revision = 2 } }, "suggestions at their current revision");
		expect(next.Context.empty() && next.Omissions.empty(), "context and omissions are the reviewer's to choose again, not dumped");
		expect(encode_round_selection(next).has_value(), "the plan is an ordinary draft selection");
	}
}

namespace {
	void reads_the_previous_assignment()
	{
		auto payload = cycle_round_payload{
			.CycleId        = "RC-1",
			.RoundId        = "F1",
			.SourceRevision = "R2",
			.Assignment     = { .Action = cycle_round_action::plan, .Goal = "Plan it.", .Constraints = { "No new files." }, .WriteScope = { "src" } },
			.Items          = {
				{ .Id = "I2", .Revision = 3, .ActionOverride = cycle_round_action::discuss, .Related = { "I4" }, .Suggestions = { { .Id = "SG2.1", .Revision = 2 } } },
				{ .Id = "I4", .Revision = 1 },
			},
		};
		const auto selection = round_assignment(payload);
		expect(selection && selection->Source == cycle_source_revision_id{ 2 } && selection->Action == cycle_round_action::plan && selection->Goal == "Plan it." && selection->Constraints.size() == 1U
				   && selection->WriteScope == std::vector<std::string>{ "src" },
			   "the assignment read back");
		expect(selection->Items.size() == 2U && selection->Items[0].Item == cycle_feedback_item_id{ 2 } && selection->Items[0].Wording == private_review_turn_revision_id{ 3 }
				   && selection->Items[0].Action == cycle_round_action::discuss && selection->Items[0].Related == std::vector{ cycle_feedback_item_id{ 4 } }
				   && selection->Items[0].Suggestions == std::vector<cycle_round_suggestion_pin>{ { .Id = cycle_suggestion_id{ 1 }, .Revision = 2 } },
			   "each item's wording, action, relations and suggestion revisions");
		payload.Items[0].Suggestions[0].Id = "SG3.1";
		expect(!round_assignment(payload), "a suggestion of another note is not this note's");
		payload.Items[0].Suggestions.clear();
		payload.SourceRevision = "sender-R2";
		expect(!round_assignment(payload), "a sender's revision name is not a local revision");
	}
}

auto main() -> int
{
	reads_the_previous_assignment();
	binds_checked_material();
	checks_decisions_before_recording();
	never_resolves_by_itself();
	plans_the_next_round();
	std::cout << "review-cycle-verification-tests:pass\n";
	return EXIT_SUCCESS;
}
