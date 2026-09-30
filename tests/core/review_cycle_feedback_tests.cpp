#include "bpr/core/private_review.hpp"
#include "bpr/core/review_cycle_feedback.hpp"
#include "bpr/core/sha256.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const char* aWhat)
	{
		if (!aValue) {
			std::cerr << "review cycle feedback failure: " << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto bytes(const std::string_view aText) -> std::span<const std::uint8_t>
	{
		return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
	}

	auto digest(const std::string_view aText) -> cycle_material_digest
	{
		return cycle_material_digest{ sha256(bytes(aText)) };
	}

	template <typename TIdentity>
	auto identity(const std::uint8_t aSeed) -> TIdentity
	{
		auto value = std::array<std::uint8_t, 32>{ };
		value[0]   = aSeed;
		return TIdentity{ value };
	}

	/// A cycle with two captured revisions, R1 and R2.
	auto captured_cycle() -> review_cycle_state
	{
		const auto authority = local_cycle_authority{ .Profile = bpr_profile_id{ 1 }, .Incarnation = identity<cycle_catalog_incarnation>(1) };
		auto       created   = create({ .Authority = authority }, { .Authority = authority, .Title = "Parser", .Scope = { } });
		expect(created.has_value(), "create");
		auto cycle = created->Cycle;
		for (const auto material : std::array<std::uint8_t, 2>{ 1, 2 }) {
			const auto outcome = apply(cycle, { .Subject = cycle.Subject, .ExpectedRevision = cycle.Revision, .Change = record_cycle_capture{ .Capture = { .ScopeVersion = 1, .Material = identity<cycle_material_digest>(material), .Entries = 3, .Coverage = cycle_capture_coverage::complete } } });
			expect(outcome.Refusal == cycle_refusal::none && outcome.Captured, "capture");
			cycle = outcome.Accepted;
		}
		return cycle;
	}

	auto anchor(const std::uint64_t aRevision, const std::string_view aPath, const std::string_view aFile, const std::string_view aText, const std::size_t aOccurrence = 0)
		-> cycle_source_anchor
	{
		auto start = aFile.find(aText);
		for (std::size_t skip = 0; skip < aOccurrence; ++skip) {
			start = aFile.find(aText, start + 1U);
		}
		expect(start != std::string_view::npos, "anchor text present");
		auto made = make_anchor(cycle_source_revision_id{ aRevision }, std::string{ aPath }, bytes(aFile), start, start + aText.size());
		expect(made.has_value(), "make anchor");
		return *made;
	}

	constexpr std::string_view parser_r1 = "int parse(const char* aText)\n{\n\treturn check(aText);\n}\n";
	constexpr std::string_view lexer_r1  = "void lex()\n{\n\tadvance();\n}\n";

	/// One note covers several files and an older revision; nothing but a body is required.
	void authors_range_file_module_cycle_and_multi_location_notes()
	{
		const auto cycle = captured_cycle();

		// A range in R2, a related range in another file and a related range in R1.
		auto discussion = begin_cycle_feedback{
			.Primary = anchor(2, "src/parser.cpp", parser_r1, "check(aText)"),
			.Related = {
				{ .Target = anchor(2, "src/lexer.cpp", lexer_r1, "advance()") },
				{ .Target = anchor(1, "src/parser.cpp", parser_r1, "int parse"), .Disclosure = cycle_reference_disclosure::private_only },
			},
			.Body = "These two calls validate the same input twice.",
		};
		const auto begun = begin_feedback(cycle, cycle.Revision, discussion);
		expect(begun.has_value(), "multi-location note");
		expect(begun->Thread.Id.Id == cycle_feedback_item_id{ 1 } && begun->Cycle.Next.FeedbackItem == 2 && begun->Cycle.Revision == cycle.Revision + 1, "item allocated");
		expect(begun->Thread.Related.size() == 2 && begun->Thread.Related[1].Disclosure == cycle_reference_disclosure::private_only, "related locations kept");
		expect(begun->Revision.Author.Provenance == authored_provenance::human && !begun->Revision.Author.Model, "reviewer authored, no model");
		expect(valid(begun->Thread) && valid(begun->Revision), "valid records");

		// File, module and cycle notes need no range and invent no line.
		const auto file   = begin_feedback(begun->Cycle, begun->Cycle.Revision, { .Primary = cycle_file_target{ cycle_source_revision_id{ 2 }, "src/lexer.cpp", digest(lexer_r1) }, .Body = "Split this file." });
		const auto module = begin_feedback(file->Cycle, file->Cycle.Revision, { .Primary = cycle_module_target{ .Path = "src", .Revision = std::nullopt }, .Body = "Module layering." });
		const auto whole  = begin_feedback(module->Cycle, module->Cycle.Revision, { .Primary = cycle_whole_target{ }, .Body = "" });
		expect(file && module && whole, "file, module and cycle notes");
		expect(whole->Thread.Id.Id == cycle_feedback_item_id{ 4 }, "sequential items");
		expect(!observe_correspondence(module->Thread.Primary, { }) && !observe_correspondence(whole->Thread.Primary, { }), "module and cycle notes name no bytes");

		// Stale revisions, unallocated revisions and malformed targets are refused.
		expect(begin_feedback(cycle, cycle.Revision - 1, discussion).error() == cycle_refusal::conflict, "stale cycle revision");
		auto future                                            = discussion;
		std::get<cycle_source_anchor>(future.Primary).Revision = cycle_source_revision_id{ 3 };
		expect(begin_feedback(cycle, cycle.Revision, future).error() == cycle_refusal::invalid_target, "unallocated revision");
		auto escaping                                        = discussion;
		std::get<cycle_source_anchor>(escaping.Primary).Path = "../outside.cpp";
		expect(begin_feedback(cycle, cycle.Revision, escaping).error() == cycle_refusal::invalid_target, "unsafe path");
		auto torn = discussion;
		std::get<cycle_source_anchor>(torn.Primary).Original.pop_back();
		expect(begin_feedback(cycle, cycle.Revision, torn).error() == cycle_refusal::invalid_target, "range and bytes disagree");
		auto forward               = discussion;
		forward.Related[0].Rebinds = 2;
		expect(begin_feedback(cycle, cycle.Revision, forward).error() == cycle_refusal::invalid_target, "rebinds a later reference");
		auto crowded = discussion;
		crowded.Related.assign(maximum_cycle_related_references + 1U, discussion.Related[0]);
		expect(begin_feedback(cycle, cycle.Revision, crowded).error() == cycle_refusal::bounds, "related bound");
		auto verbose = discussion;
		verbose.Body.assign(maximum_private_reviewer_body_bytes + 1U, 'x');
		expect(begin_feedback(cycle, cycle.Revision, verbose).error() == cycle_refusal::bounds, "reviewer body bound");

		auto archived      = cycle;
		archived.Lifecycle = cycle_lifecycle::archived;
		expect(begin_feedback(archived, archived.Revision, discussion).error() == cycle_refusal::archived, "archived cycle");
	}

	/// Anchors are exact bytes; line numbers are only their presentation.
	void anchors_are_immutable_byte_ranges()
	{
		const auto target = cycle_feedback_target{ anchor(1, "src/parser.cpp", parser_r1, "check(aText)") };
		expect(verify_target(target, bytes(parser_r1)), "anchor verifies against its bytes");
		const auto& original = std::get<cycle_source_anchor>(target);
		expect(original.Original == "check(aText)" && original.Before == "int parse(const char* aText)\n{\n\treturn " && original.After == ";\n}\n", "context kept");
		expect(project_lines(bytes(parser_r1), original.Start, original.End) == cycle_line_span{ 3, 3 }, "line three");

		// Any change to the file, range or context no longer verifies.
		expect(!verify_target(target, bytes("int parse(const char* aText)\n{\n\treturn check(aText) ;\n}\n")), "different file");
		auto moved = original;
		++moved.Start;
		++moved.End;
		expect(!verify_target(moved, bytes(parser_r1)), "shifted range");
		auto context   = original;
		context.Before = "x";
		expect(!verify_target(context, bytes(parser_r1)), "wrong context");
		expect(verify_target(cycle_file_target{ cycle_source_revision_id{ 1 }, "src/parser.cpp", digest(parser_r1) }, bytes(parser_r1)), "file target");
		expect(!verify_target(cycle_file_target{ cycle_source_revision_id{ 1 }, "src/parser.cpp", digest(lexer_r1) }, bytes(parser_r1)), "file target digest");

		// Ranges end on scalar boundaries and need text.
		constexpr auto unicode = std::string_view{ "caf\xC3\xA9 = 1;\n" };
		expect(!make_anchor(cycle_source_revision_id{ 1 }, "u.txt", bytes(unicode), 0, 4), "split scalar");
		expect(make_anchor(cycle_source_revision_id{ 1 }, "u.txt", bytes(unicode), 0, 5).has_value(), "whole scalar");
		expect(!make_anchor(cycle_source_revision_id{ 1 }, "b.bin", bytes(std::string_view{ "a\0b", 3 }), 0, 1), "binary refuses a range");
		expect(!make_anchor(cycle_source_revision_id{ 1 }, "a.txt", bytes("abc"), 2, 4), "past the end");

		// Context is two lines each side, and at most 512 bytes.
		constexpr auto lines = std::string_view{ "1\n2\n3\n4\nTARGET\n6\n7\n8\n" };
		const auto     many  = anchor(1, "l.txt", lines, "TARGET");
		expect(many.Before == "3\n4\n" && many.After == "\n6\n7\n", "two context lines");
		const auto longLine = std::string(2000U, 'a') + "TARGET" + std::string(2000U, 'b');
		const auto bounded  = anchor(1, "l.txt", longLine, "TARGET");
		expect(bounded.Before.size() == maximum_cycle_anchor_context_bytes && bounded.After.size() == maximum_cycle_anchor_context_bytes, "context bound");

		// LF, CRLF and a lone CR each end a line; an insertion shows as its line.
		constexpr auto mixed = std::string_view{ "a\r\nb\rc\nd" };
		expect(project_lines(bytes(mixed), 3, 4) == cycle_line_span{ 2, 2 }, "after CRLF");
		expect(project_lines(bytes(mixed), 5, 6) == cycle_line_span{ 3, 3 }, "after CR");
		expect(project_lines(bytes(mixed), 0, 3) == cycle_line_span{ 1, 1 }, "a range ending at a line break stays on its line");
		expect(project_lines(bytes(mixed), 8, 8) == cycle_line_span{ 4, 4 }, "insertion point at the end");
		expect(project_lines(bytes(mixed), 1, 2) == cycle_line_span{ 1, 1 }, "between CR and LF is still line one");
		expect(!project_lines(bytes(mixed), 3, 99), "outside the file");
	}

	/// Where an original attachment sits in newer material is observed, never chosen.
	void derives_current_correspondence_separately()
	{
		const auto original = cycle_feedback_target{ anchor(1, "src/parser.cpp", parser_r1, "check(aText)") };
		const auto against  = [](const std::string_view aFile, const cycle_path_state aState = cycle_path_state::present) {
			return cycle_current_source{ .Revision = cycle_source_revision_id{ 2 }, .State = aState, .Blob = digest(aFile), .Bytes = bytes(aFile) };
		};

		const auto same = observe_correspondence(original, against(parser_r1));
		expect(same && same->Kind == cycle_correspondence::exact_material && same->Range == std::pair<std::uint64_t, std::uint64_t>{ 39, 51 }, "unchanged file");

		constexpr auto shifted = std::string_view{ "// header\nint parse(const char* aText)\n{\n\treturn check(aText);\n}\n" };
		const auto     mapped  = observe_correspondence(original, against(shifted));
		expect(mapped && mapped->Kind == cycle_correspondence::exact_mapped && mapped->Range == std::pair<std::uint64_t, std::uint64_t>{ 49, 61 }, "moved down");

		// A copied block: the bytes repeat, and the original context singles one out.
		constexpr auto copied  = std::string_view{ "int parse(const char* aText)\n{\n\treturn check(aText);\n}\nint again(const char* aText)\n{\n\treturn check(aText);\n}\n" };
		const auto     context = observe_correspondence(original, against(copied));
		expect(context && context->Kind == cycle_correspondence::exact_mapped && context->Range->first == 39 && context->Candidates == 2, "context disambiguates");
		// ...and when the context changed too, nothing is chosen.
		constexpr auto twice = std::string_view{ "a(){ return check(aText); }\nb(){ return check(aText); }\n" };
		const auto     both  = observe_correspondence(original, against(twice));
		expect(both && both->Kind == cycle_correspondence::ambiguous && !both->Range && both->Candidates == 2, "repeated text is ambiguous");

		const auto gone = observe_correspondence(original, against("int parse(const char* aText) { return verify(aText); }\n"));
		expect(gone && gone->Kind == cycle_correspondence::unmapped && gone->Candidates == 0, "text no longer present");
		const auto deleted = observe_correspondence(original, { .Revision = cycle_source_revision_id{ 2 }, .State = cycle_path_state::deleted });
		expect(deleted && deleted->Kind == cycle_correspondence::deleted, "deleted file");
		const auto absent = observe_correspondence(original, { .Revision = cycle_source_revision_id{ 2 }, .State = cycle_path_state::absent });
		expect(absent && absent->Kind == cycle_correspondence::deleted, "absent from a complete capture");
		for (const auto state : { cycle_path_state::unknown, cycle_path_state::outside_scope, cycle_path_state::unavailable }) {
			const auto unknown = observe_correspondence(original, { .Revision = cycle_source_revision_id{ 2 }, .State = state });
			expect(unknown && unknown->Kind == cycle_correspondence::unavailable, "unknown paths are unavailable, not deleted");
		}
		const auto unread = observe_correspondence(original, { .Revision = cycle_source_revision_id{ 2 }, .State = cycle_path_state::present, .Blob = digest("x") });
		expect(unread && unread->Kind == cycle_correspondence::unavailable, "bytes not read");

		// The original is never rewritten by an observation.
		expect(std::get<cycle_source_anchor>(original).Start == 39 && std::get<cycle_source_anchor>(original).Revision == cycle_source_revision_id{ 1 }, "original kept");

		// A file note maps by path.
		const auto file = cycle_feedback_target{ cycle_file_target{ cycle_source_revision_id{ 1 }, "src/parser.cpp", digest(parser_r1) } };
		expect(observe_correspondence(file, against(parser_r1))->Kind == cycle_correspondence::exact_material, "file unchanged");
		expect(observe_correspondence(file, against(shifted))->Kind == cycle_correspondence::exact_mapped, "file changed");

		// An insertion point is found only through its surroundings.
		const auto point  = cycle_feedback_target{ *make_anchor(cycle_source_revision_id{ 1 }, "src/parser.cpp", bytes(parser_r1), 30, 30) };
		const auto placed = observe_correspondence(point, against(shifted));
		expect(placed && placed->Kind == cycle_correspondence::exact_mapped && placed->Range == std::pair<std::uint64_t, std::uint64_t>{ 40, 40 }, "insertion mapped");
	}

	auto author(const authored_provenance aProvenance) -> cycle_feedback_author
	{
		return { .Provenance = aProvenance };
	}

	/// Threads are versioned with the same lineage rules as pull-request private threads.
	void keeps_versioned_private_lineage()
	{
		// Shared records: the header, completion, derivation and digest types are the private-thread ones.
		static_assert(std::is_same_v<decltype(cycle_feedback_thread::Lineage), private_thread_lineage>);
		static_assert(std::is_same_v<decltype(cycle_feedback_revision::Completion), private_revision_completion>);
		static_assert(std::is_same_v<decltype(cycle_feedback_revision::EditDerivation), std::optional<private_derivation_kind>>);
		static_assert(std::is_same_v<decltype(cycle_feedback_revision_ref::Digest), decltype(private_revision_reference::Digest)>);
		static_assert(std::is_same_v<decltype(private_review_thread::Lifecycle), decltype(private_thread_lineage::Lifecycle)>);

		const auto cycle  = captured_cycle();
		const auto begun  = begin_feedback(cycle, cycle.Revision, { .Primary = anchor(2, "src/parser.cpp", parser_r1, "check(aText)"), .Body = "Why check twice?" });
		auto       thread = begun->Thread;
		const auto first  = begun->Revision;

		// A reply from an adapter it observed, streamed incomplete.
		auto observed    = author(authored_provenance::adapter_observed);
		observed.Adapter = "codex-cli";
		observed.Model   = "gpt-6";
		const auto reply = apply_feedback(cycle, thread, 1, append_feedback_turn{ .Author = observed, .Body = "Because the", .Completion = private_revision_completion::incomplete, .ReplyTo = first.Identity.Position.Revision }, &first);
		expect(reply && reply->Changed && reply->Thread.Lineage.Version == 2 && reply->Thread.NextTurn == 3, "adapter reply");
		expect(reply->Revision->Identity.Position.Turn == private_review_turn_id{ 2 } && reply->Revision->Identity.Position.Order == private_vertex_order{ 2 }, "next turn and order");
		expect(reply->Revision->ReplyTo == first.Identity && valid(*reply->Revision), "reply lineage");
		thread = reply->Thread;

		// Editing incomplete text needs explicit adoption; the adoption is recorded.
		const auto rushed = apply_feedback(cycle, thread, 2, edit_feedback_turn{ .Latest = reply->Revision->Identity.Position.Revision, .Body = "Because the input is untrusted." }, &*reply->Revision);
		expect(!rushed && rushed.error() == cycle_refusal::incomplete_source, "incomplete needs adoption");
		const auto adopted = apply_feedback(cycle, thread, 2, edit_feedback_turn{ .Latest = reply->Revision->Identity.Position.Revision, .Body = "Because the input is untrusted.", .AdoptIncomplete = true }, &*reply->Revision);
		expect(adopted && adopted->Revision->EditDerivation == private_derivation_kind::adopted_from_incomplete_run, "adopted");
		expect(adopted->Revision->Predecessor == reply->Revision->Identity && adopted->Revision->Identity.Position.Turn == private_review_turn_id{ 2 }, "same turn, predecessor kept");
		expect(adopted->Revision->Author.Provenance == authored_provenance::human && adopted->Revision->ReplyTo == first.Identity && valid(*adopted->Revision), "reviewer successor");
		thread = adopted->Thread;

		// Editing a revision that is no longer the latest of its turn is stale.
		expect(apply_feedback(cycle, thread, 3, edit_feedback_turn{ .Latest = reply->Revision->Identity.Position.Revision, .Body = "x" }, &*adopted->Revision).error() == cycle_refusal::conflict, "stale edit");
		expect(apply_feedback(cycle, thread, 2, append_feedback_turn{ .Author = author(authored_provenance::human), .Body = "x" }, nullptr).error() == cycle_refusal::conflict, "stale version");
		expect(apply_feedback(cycle, thread, 3, edit_feedback_turn{ .Latest = private_review_turn_revision_id{ 9 }, .Body = "x" }, nullptr).error() == cycle_refusal::missing_source, "edit of nothing");
		expect(apply_feedback(cycle, thread, 3, append_feedback_turn{ .Author = author(authored_provenance::human), .Body = "x", .ReplyTo = private_review_turn_revision_id{ 9 } }, nullptr).error() == cycle_refusal::missing_source, "reply to nothing");

		// Provenance classes stay distinct and their fields consistent.
		auto pasted  = author(authored_provenance::self_reported);
		pasted.Model = "claims to be gpt-6";
		expect(apply_feedback(cycle, thread, 3, append_feedback_turn{ .Author = pasted, .Body = "Pasted answer." }, nullptr).has_value(), "self-reported claim");
		expect(apply_feedback(cycle, thread, 3, append_feedback_turn{ .Author = author(authored_provenance::unknown), .Body = "Imported." }, nullptr).has_value(), "unknown");
		auto modelHuman  = author(authored_provenance::human);
		modelHuman.Model = "gpt-6";
		expect(apply_feedback(cycle, thread, 3, append_feedback_turn{ .Author = modelHuman, .Body = "x" }, nullptr).error() == cycle_refusal::invalid_author, "human with a model");
		expect(apply_feedback(cycle, thread, 3, append_feedback_turn{ .Author = author(authored_provenance::adapter_observed), .Body = "x" }, nullptr).error() == cycle_refusal::invalid_author, "observed without an adapter");
		auto claimedAdapter    = author(authored_provenance::self_reported);
		claimedAdapter.Adapter = "codex-cli";
		expect(apply_feedback(cycle, thread, 3, append_feedback_turn{ .Author = claimedAdapter, .Body = "x" }, nullptr).error() == cycle_refusal::invalid_author, "a self-report is not observed");
		expect(apply_feedback(cycle, thread, 3, append_feedback_turn{ .Author = author(authored_provenance::human), .Body = "x", .Completion = private_revision_completion::incomplete }, nullptr).error() == cycle_refusal::invalid_completion, "human text is complete");

		// Related locations are appended; a rebinding keeps the original.
		const auto newer   = anchor(2, "src/parser.cpp", "// header\nint parse(const char* aText)\n{\n\treturn check(aText);\n}\n", "check(aText)");
		const auto rebound = apply_feedback(cycle, thread, 3, add_feedback_reference{ { .Target = newer, .Rebinds = 0 } }, nullptr);
		expect(rebound && rebound->Thread.Related.size() == 1 && rebound->Thread.Primary == thread.Primary && !rebound->Revision, "rebinding appended");
		expect(apply_feedback(cycle, thread, 3, add_feedback_reference{ { .Target = newer, .Rebinds = 1 } }, nullptr).error() == cycle_refusal::invalid_target, "rebinding nothing");

		// Archiving closes the thread to content until it is reopened.
		const auto archived = apply_feedback(cycle, thread, 3, set_feedback_lifecycle{ private_thread_lifecycle::archived }, nullptr);
		expect(archived && archived->Changed, "archive");
		expect(apply_feedback(cycle, archived->Thread, 4, append_feedback_turn{ .Author = author(authored_provenance::human), .Body = "x" }, nullptr).error() == cycle_refusal::closed, "archived thread");
		const auto same = apply_feedback(cycle, archived->Thread, 4, set_feedback_lifecycle{ private_thread_lifecycle::archived }, nullptr);
		expect(same && !same->Changed && same->Thread == archived->Thread, "same lifecycle is a no-op");
		const auto reopened = apply_feedback(cycle, archived->Thread, 4, set_feedback_lifecycle{ private_thread_lifecycle::open }, nullptr);
		expect(reopened && reopened->Thread.Lineage.Version == 5, "reopened");

		// The pull-request thread rules are the same functions.
		auto header = private_thread_lineage{ .Version = 3, .LastOrder = private_vertex_order{ 3 }, .Lifecycle = private_thread_lifecycle::archived };
		expect(private_write_refusal(header, 3) == private_review_refusal::archived && private_write_refusal(header, 2) == private_review_refusal::conflict, "shared write rule");
		expect(provenance_of(private_review_actor{ private_reviewer_actor{ } }) == authored_provenance::human, "reviewer is human");
		expect(provenance_of(private_review_actor{ private_agent_actor{ agent_run_id{ 1 } } }) == authored_provenance::adapter_observed, "a run is observed");
		expect(provenance_of(private_review_actor{ private_legacy_unknown_actor{ } }) == authored_provenance::unknown, "legacy is unknown");
	}
}

auto main() -> int
{
	authors_range_file_module_cycle_and_multi_location_notes();
	anchors_are_immutable_byte_ranges();
	derives_current_correspondence_separately();
	keeps_versioned_private_lineage();
	return EXIT_SUCCESS;
}
