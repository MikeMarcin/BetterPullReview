#include "bpr/core/git_objects.hpp"
#include "bpr/core/review_cycle_promotion.hpp"
#include "bpr/core/sha256.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-1273]]. Promoting local notes into the linked pull request's
/// Review Tour: exact lines only when the published commit is proved to hold
/// the note's bytes, no source for every other note, the note's words
/// unchanged, a request identity that makes asking again change nothing, and
/// the tour's own placement policy for what each stop becomes on GitHub.

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const std::string_view aWhat)
	{
		if (!aValue) {
			std::cerr << "review cycle promotion failure: " << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto bytes(const std::string_view aText) -> std::span<const std::uint8_t>
	{
		return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
	}

	auto commit(const std::uint8_t aSeed) -> git_commit_oid
	{
		auto value = std::array<std::uint8_t, 32>{ };
		value[0]   = aSeed;
		value[19]  = 0xC1;
		return git_commit_oid{ git_hash_algorithm::sha1, value };
	}

	constexpr auto published = std::string_view{ "#include \"validate.hpp\"\nint parse(const char* aText)\n{\n\treturn aText && validate(aText) ? 0 : 1;\n}\n" };
	constexpr auto earlier   = std::string_view{ "int parse(const char* aText) { return check(aText); }\n" };

	auto subject() -> local_cycle_subject
	{
		auto incarnation = std::array<std::uint8_t, 32>{ };
		incarnation[3]   = 7;
		return { .Authority = { .Profile = bpr_profile_id{ 6 }, .Incarnation = cycle_catalog_incarnation{ incarnation } }, .Cycle = review_cycle_id{ 4 }, .Generation = 1 };
	}

	auto blob_of(const std::string_view aPath, const std::string_view aText, const bool aLineFeed = false) -> candidate_blob
	{
		return { .Path     = std::string{ aPath },
				 .Mode     = 0100644U,
				 .Object   = retag<git_entry_oid_kind>(git_blob_identity(bytes(aText), git_hash_algorithm::sha1)),
				 .Content  = cycle_material_digest{ sha256(bytes(aText)) },
				 .Size     = aText.size(),
				 .LineFeed = aLineFeed };
	}

	auto plan() -> publication_plan
	{
		auto value    = publication_plan{ };
		value.Start   = cycle_source_revision_id{ 1 };
		value.Final   = cycle_source_revision_id{ 2 };
		value.Format  = git_hash_algorithm::sha1;
		value.Parent  = commit(1);
		value.Commit  = commit(9);
		value.Blobs   = { blob_of("src/parse.cpp", published), blob_of("src/crlf.cpp", "a\r\nb\r\n", true) };
		value.Deleted = { "src/old.cpp" };
		return value;
	}

	auto link(const cycle_pull_origin aOrigin = cycle_pull_origin::published) -> cycle_pull_association
	{
		return { .Origin      = aOrigin,
				 .Publication = aOrigin == cycle_pull_origin::published ? std::optional{ cycle_publication_id{ 1 } } : std::nullopt,
				 .Address     = { .Host = "github.com", .Owner = "octo-org", .Repository = "review-repository", .Number = 17 },
				 .Head        = commit(9),
				 .Account     = github_account_id{ 1001 },
				 .Login       = "octo-reviewer" };
	}

	auto note(const std::uint64_t aItem, cycle_feedback_target aTarget, std::string aBody = "Check for an empty string too.") -> cycle_promotion_note
	{
		return { .Item = { .Cycle = review_cycle_id{ 4 }, .Id = cycle_feedback_item_id{ aItem } }, .Primary = std::move(aTarget), .Body = std::move(aBody) };
	}

	/// Lines 4 and 5 (one-based) of the published file: `return …` and `}`.
	auto anchor_on_published() -> cycle_source_anchor
	{
		const auto start = published.find("\treturn");
		const auto end   = published.rfind('}') + 1U;
		auto       made  = make_anchor(cycle_source_revision_id{ 2 }, "src/parse.cpp", bytes(published), start, end);
		expect(made.has_value(), "an anchor on the published revision");
		return *made;
	}

	void exact_lines()
	{
		const auto value  = plan();
		const auto anchor = anchor_on_published();
		const auto chosen = note(1, anchor);
		expect(promotion_needs_file(link(), &value, chosen), "a note on the published bytes asks for its file");
		const auto promoted = promote_note(subject(), link(), &value, chosen, bytes(published));
		expect(promoted.has_value() && promoted->Source == cycle_promotion_source::exact_lines, "a note on the published lines keeps its exact lines");
		const auto& source = promoted->Stop.Destination;
		expect(source && source->Commit == commit(9) && source->Path == "src/parse.cpp" && source->Side == diff_side::right, "at the published commit, on its head side");
		expect(source->FirstLine == 3U && source->LastLine == 4U, "lines 4 to 5, zero-based as a tour source counts them");
		expect(source->Blob == git_blob_identity(bytes(published), git_hash_algorithm::sha1), "the object the plan wrote, proved by the bytes");
		expect(promoted->Stop.Body == "Check for an empty string too." && promoted->Stop.Title == "src/parse.cpp", "the note's words, unchanged; the file as its title");
		expect(!promoted->Stop.Original && !promoted->Stop.Mapped && promoted->Stop.Preparation == tour_preparation::editing && !ready(promoted->Stop),
			   "no comparison is fabricated: the stop waits for the reviewer to complete it");

		// An off-hunk note: a line the change did not touch, in the same published file.
		const auto offStart = published.find("int parse");
		const auto off      = make_anchor(cycle_source_revision_id{ 2 }, "src/parse.cpp", bytes(published), offStart, offStart + 3U);
		const auto offHunk  = promote_note(subject(), link(), &value, note(2, *off), bytes(published));
		expect(offHunk && offHunk->Source == cycle_promotion_source::exact_lines && offHunk->Stop.Destination->FirstLine == 1U && offHunk->Stop.Destination->LastLine == 1U,
			   "an off-hunk note keeps its exact line too");

		// The proof is the bytes, never the path.
		const auto forged = std::string{ published } + "// another line\n";
		expect(promote_note(subject(), link(), &value, chosen, bytes(forged))->Source == cycle_promotion_source::unread, "bytes that do not hash to the published object prove nothing");
		expect(promote_note(subject(), link(), &value, chosen, std::nullopt)->Source == cycle_promotion_source::unread, "unread bytes prove nothing");
	}

	void no_current_source()
	{
		const auto value = plan();
		const auto check = [&](const cycle_promotion_note& aNote, const cycle_promotion_source aExpected, const std::string_view aWhat, const cycle_pull_association& aLink = link()) {
			const auto promoted = promote_note(subject(), aLink, aLink.Origin == cycle_pull_origin::published ? &value : nullptr, aNote, std::nullopt);
			expect(promoted.has_value() && promoted->Source == aExpected, aWhat);
			expect(!promoted->Stop.Destination && !promoted->Stop.Original && !promoted->Stop.Mapped, std::string{ aWhat } + ": no line is chosen for it");
			expect(promoted->Stop.Body == aNote.Body, std::string{ aWhat } + ": its words unchanged");
			expect(!promotion_needs_file(aLink, aLink.Origin == cycle_pull_origin::published ? &value : nullptr, aNote), std::string{ aWhat } + ": nothing to read");
			const auto planned = promoted_destination(promoted->Stop, commit(9), commit(1));
			expect(planned.Representation == tour_representation::unresolved && planned.NeedsQualification, std::string{ aWhat } + ": the policy decides once the reviewer chooses its lines");
		};
		const auto historical = make_anchor(cycle_source_revision_id{ 1 }, "src/parse.cpp", bytes(earlier), 4, 9);
		check(note(3, *historical), cycle_promotion_source::earlier_revision, "a historical note");
		check(note(4, cycle_whole_target{ }), cycle_promotion_source::whole_change, "a whole-cycle note");
		check(note(5, cycle_module_target{ .Path = "src" }), cycle_promotion_source::folder, "a folder note");
		check(note(6, cycle_file_target{ .Revision = cycle_source_revision_id{ 2 }, .Path = "src/parse.cpp", .Blob = cycle_material_digest{ sha256(bytes(published)) } }), cycle_promotion_source::whole_file,
			  "a whole-file note");
		const auto unchanged = make_anchor(cycle_source_revision_id{ 2 }, "README.md", bytes("# Parser\n"), 2, 8);
		check(note(7, *unchanged), cycle_promotion_source::outside_changes, "a file the commit did not write");
		const auto gone = make_anchor(cycle_source_revision_id{ 2 }, "src/old.cpp", bytes("int old() { return 0; }\n"), 4, 7);
		check(note(8, *gone), cycle_promotion_source::other_bytes, "a file the commit deletes");
		const auto crlf = make_anchor(cycle_source_revision_id{ 2 }, "src/crlf.cpp", bytes("a\r\nb\r\n"), 0, 1);
		check(note(9, *crlf), cycle_promotion_source::other_bytes, "a file whose line endings the commit normalises");
		const auto other = make_anchor(cycle_source_revision_id{ 2 }, "src/parse.cpp", bytes(earlier), 4, 9);
		check(note(10, *other), cycle_promotion_source::other_bytes, "a file the commit holds with other bytes");
		check(note(11, anchor_on_published()), cycle_promotion_source::linked_by_hand, "a pull request linked by hand proves no bytes", link(cycle_pull_origin::external));
	}

	void identity()
	{
		const auto value  = plan();
		const auto first  = promote_note(subject(), link(), &value, note(4, cycle_whole_target{ }), std::nullopt);
		const auto again  = promote_note(subject(), link(), &value, note(4, cycle_whole_target{ }), std::nullopt);
		const auto edited = promote_note(subject(), link(), &value, note(4, cycle_whole_target{ }, "Reworded."), std::nullopt);
		expect(first->RequestId == again->RequestId && first->Digest == again->Digest, "the same note to the same pull request is the same request");
		expect(first->RequestId.size() <= 64U && first->RequestId.starts_with("cycle-note-"), "a request identity a receipt holds");
		expect(edited->RequestId == first->RequestId && edited->Digest != first->Digest, "a reworded note is the same request with other content: refused, never a second stop");
		const auto other = promote_note(subject(), link(), &value, note(5, cycle_whole_target{ }), std::nullopt);
		expect(other->RequestId != first->RequestId, "another note is another request");
		auto elsewhere           = link();
		elsewhere.Address.Number = 18;
		expect(promote_note(subject(), elsewhere, &value, note(4, cycle_whole_target{ }), std::nullopt)->RequestId != first->RequestId, "another pull request is another request");
		auto cased          = link();
		cased.Address.Owner = "Octo-Org";
		expect(promote_note(subject(), cased, &value, note(4, cycle_whole_target{ }), std::nullopt)->RequestId == first->RequestId, "the owner's case names the same pull request");
		auto another                  = subject();
		auto incarnation              = std::array<std::uint8_t, 32>{ };
		incarnation[3]                = 8;
		another.Authority.Incarnation = cycle_catalog_incarnation{ incarnation };
		expect(promote_note(another, link(), &value, note(4, cycle_whole_target{ }), std::nullopt)->RequestId != first->RequestId, "another catalog's cycle is another request");
	}

	void refusals()
	{
		const auto value = plan();
		expect(promote_note(subject(), link(), &value, note(1, cycle_whole_target{ }, " \n\t"), std::nullopt).error() == cycle_promotion_refusal::empty, "a blank note is not promoted");
		expect(promote_note(subject(), link(), &value, note(1, cycle_whole_target{ }, std::string(maximum_tour_field_bytes + 1U, 'x')), std::nullopt).error() == cycle_promotion_refusal::bounds,
			   "a note longer than a stop holds");
		auto foreign       = note(1, cycle_whole_target{ });
		foreign.Item.Cycle = review_cycle_id{ 5 };
		expect(promote_note(subject(), link(), &value, foreign, std::nullopt).error() == cycle_promotion_refusal::mismatch, "another cycle's note");
		expect(promote_note(subject(), link(), nullptr, note(1, cycle_whole_target{ }), std::nullopt).error() == cycle_promotion_refusal::mismatch, "a native link without its plan");
	}

	void destinations()
	{
		const auto value    = plan();
		const auto promoted = promote_note(subject(), link(), &value, note(1, anchor_on_published()), bytes(published));
		const auto current  = promoted_destination(promoted->Stop, commit(9), commit(1));
		expect(current.Representation == tour_representation::native_line && current.JoinsPendingReview && !current.RequiresCompatibility,
			   "at the pull request's head the policy gives a line comment in the tour's review");
		const auto moved = promoted_destination(promoted->Stop, commit(10), commit(1));
		expect(moved.Representation == tour_representation::conversation_permalink && !moved.JoinsPendingReview,
			   "once the head moved on, the same lines are historical: a conversation permalink, never another line");
		// The same policy publication applies to the same stop.
		const auto observed = publication_observation(*promoted->Stop.Destination, commit(9), commit(1), promoted->Stop.Body, false, true);
		expect(plan_tour_destination(observed) == current, "the promoted destination is the publication policy's own");
		expect(publication_source(promoted->Stop) == promoted->Stop.Destination, "a promoted stop publishes from its remote address until the reviewer replaces it");
	}

	void tour_accepts()
	{
		const auto value    = plan();
		const auto promoted = promote_note(subject(), link(), &value, note(1, anchor_on_published()), bytes(published));
		auto       tour     = review_tour{ .Id = tour_draft_id{ 1 }, .Owner = { bpr_profile_id{ 6 }, git_host_id{ 1 }, host_authority_fingerprint{ std::array<std::uint8_t, 32>{ 1 } }, github_account_id{ 1001 }, repository_id{ 2 }, pull_request_id{ 3 } }, .Base = commit(1), .Head = commit(9) };
		const auto added    = apply(tour, { .Draft = tour.Id, .Owner = tour.Owner, .ExpectedRevision = tour.Revision, .Kind = tour_command_kind::insert, .Value = promoted->Stop, .Position = 0 });
		expect(added.Refusal == tour_refusal::none && added.Accepted.Stops.size() == 1U, "the tour's own insert takes the stop");
		const auto& stop = added.Accepted.Stops.front();
		expect(stop.Body == promoted->Stop.Body && stop.Destination == promoted->Stop.Destination && stop.Preparation == tour_preparation::editing, "as promoted, waiting for the reviewer");
		const auto destinations = std::vector{ promoted_destination(stop, commit(9), commit(1)) };
		expect(assess_tour_publication(added.Accepted, destinations, false) == tour_publication_readiness::incomplete, "nothing is publishable until the reviewer completes it");
	}
}

auto main() -> int
{
	exact_lines();
	no_current_source();
	identity();
	refusals();
	destinations();
	tour_accepts();
	std::cout << "review cycle promotion: pass\n";
	return EXIT_SUCCESS;
}
