#include "bpr/core/review_cycle_share.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-1161]]. Share routes decided by the one channel decision,
/// Compose feedback's edits, and which delivery observations may be recorded.

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const std::string_view aWhat)
	{
		if (!aValue) {
			std::cerr << "review cycle share failure: " << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename TIdentity>
	constexpr auto identity(const std::uint8_t aSeed) -> TIdentity
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[0]   = aSeed;
		return TIdentity{ bytes };
	}

	constexpr auto subject = local_cycle_subject{
		.Authority  = { .Profile = bpr_profile_id{ 1 }, .Incarnation = identity<cycle_catalog_incarnation>(1) },
		.Cycle      = review_cycle_id{ 3 },
		.Generation = 1,
	};
	constexpr auto round = cycle_round{
		.Id            = { .Cycle = review_cycle_id{ 3 }, .Id = cycle_round_id{ 2 } },
		.Draft         = { .Cycle = review_cycle_id{ 3 }, .Id = cycle_round_draft_id{ 1 } },
		.DraftRevision = 4,
		.Digest        = identity<cycle_round_digest>(9),
	};

	constexpr auto all_routes          = std::array{ cycle_share_route::copy_document, cycle_share_route::save_document, cycle_share_route::save_bundle, cycle_share_route::external_agent,
													 cycle_share_route::send_to_agent };
	constexpr auto all_classifications = std::array{ cycle_data_classification::unclassified, cycle_data_classification::public_source, cycle_data_classification::enterprise_restricted };
	constexpr auto all_permissions     = std::array{ cycle_channel_permission::unconfigured, cycle_channel_permission::allowed, cycle_channel_permission::denied };

	auto offer_of(const std::vector<cycle_share_route_offer>& aOffers, const cycle_share_route aRoute) -> std::optional<cycle_share_route_offer>
	{
		for (const auto& offer : aOffers) {
			if (offer.Route == aRoute) {
				return offer;
			}
		}
		return std::nullopt;
	}

	auto policy_setting(const cycle_data_classification aClassification, const cycle_disclosure_channel aChannel, const cycle_channel_permission aPermission) -> cycle_disclosure_policy
	{
		auto  policy                                            = cycle_disclosure_policy{ };
		auto& setting                                           = aClassification == cycle_data_classification::unclassified ? policy.Unclassified
			: aClassification == cycle_data_classification::public_source                                                    ? policy.PublicSource
																															 : policy.EnterpriseRestricted;
		setting.Channels.at(static_cast<std::size_t>(aChannel)) = aPermission;
		return policy;
	}

	void lists_copy_and_save_first_and_optional_routes_only_when_reachable()
	{
		const auto folder  = cycle_workspace_origin{ };
		const auto plain   = share_routes({ }, folder, { });
		const auto ordered = std::vector<cycle_share_route>{ cycle_share_route::copy_document, cycle_share_route::save_document, cycle_share_route::save_bundle };
		expect(plain.size() == 3U && plain[0].Route == ordered[0] && plain[1].Route == ordered[1] && plain[2].Route == ordered[2], "Copy, Save and Save as Bundle, in that order");
		expect(plain[0].offered() && plain[1].offered() && plain[2].offered(), "all three offered for a plain folder of unknown origin by default");
		expect(!offer_of(plain, cycle_share_route::external_agent) && !offer_of(plain, cycle_share_route::send_to_agent), "no agent route, not even a disabled one, without the capability");
		const auto reachable = share_routes({ }, folder, { .ExternalInterface = true, .Adapter = true });
		expect(reachable.size() == 5U && reachable[3].Route == cycle_share_route::external_agent && reachable[4].Route == cycle_share_route::send_to_agent, "the optional routes follow");
		expect(reachable[3].Decision == cycle_authorization::channel_unconfigured && reachable[4].Decision == cycle_authorization::channel_unconfigured,
			   "reaching another program from source of unknown origin needs a configured allowance");
		const auto known = share_routes({ }, { .Origin = cycle_source_origin::local_git_checkout, .Classification = cycle_data_classification::public_source }, { .ExternalInterface = true, .Adapter = true });
		expect(std::ranges::all_of(known, [](const cycle_share_route_offer& aOffer) { return aOffer.offered(); }), "known public source: every reachable route offered");
	}

	/// RC56-10: every route's decision is `authorize`'s, for every
	/// classification, channel and setting; a refused route stays listed
	/// and no other route changes because of it.
	void decides_every_route_as_authorize_does()
	{
		auto rows = 0;
		for (const auto classification : all_classifications) {
			for (const auto configured : all_routes) {
				for (const auto permission : all_permissions) {
					const auto policy = policy_setting(classification, share_channel(configured), permission);
					const auto origin = cycle_workspace_origin{ .Origin = cycle_source_origin::local_folder, .Classification = classification };
					const auto offers = share_routes(policy, origin, { .ExternalInterface = true, .Adapter = true });
					expect(offers.size() == cycle_share_route_count, "every route listed, refused or not");
					for (const auto route : all_routes) {
						const auto offer    = offer_of(offers, route);
						const auto exported = authorize(policy, { .Actor = cycle_actor::reviewer, .Capability = cycle_capability::export_round, .Subject = subject, .Origin = origin, .Round = round.Id, .Digest = round.Digest, .Channel = share_channel(route) });
						expect(offer && offer->Decision == exported, "the route's decision is the reviewer export's");
						expect(offer->Decision == channel_permission(policy, classification, share_channel(route)), "and the channel decision on its own");
						if (route == cycle_share_route::external_agent) {
							// The same answer a granted harness reading through the local interface gets.
							const auto read = authorize(policy, { .Actor = cycle_actor::granted_consumer, .Capability = cycle_capability::read_round, .Subject = subject, .Origin = origin, .Round = round.Id, .Digest = round.Digest, .Channel = cycle_disclosure_channel::external_retrieval, .Grant = response_only_grant(subject, round) });
							expect(read == exported, "a harness's retrieval is decided as the desktop's route is");
						}
						if (share_channel(route) != share_channel(configured)) {
							const auto unaffected = channel_permission({ }, classification, share_channel(route));
							expect(offer->Decision == unaffected, "a setting for one channel changes no other route");
						}
						++rows;
					}
				}
			}
		}
		expect(rows == 3 * 5 * 3 * 5, "the whole table");
		// Denying the clipboard for unknown source: Copy refused and still listed; Save keeps its own answer.
		const auto denied = share_routes(policy_setting(cycle_data_classification::unclassified, cycle_disclosure_channel::clipboard, cycle_channel_permission::denied), { }, { });
		expect(denied.size() == 3U && denied[0].Decision == cycle_authorization::channel_denied && denied[1].offered(), "a refused Copy is shown refused; Save is decided on its own");
		auto none = cycle_disclosure_policy{ };
		none.Unclassified.Channels.fill(cycle_channel_permission::denied);
		const auto shut = share_routes(none, { }, { .ExternalInterface = true, .Adapter = true });
		expect(shut.size() == 5U && std::ranges::none_of(shut, [](const cycle_share_route_offer& aOffer) { return aOffer.offered(); }), "with every channel denied nothing is offered, and nothing is hidden");
	}

	void reads_origin_from_the_bound_workspace()
	{
		auto cycle = review_cycle_state{ .Subject = subject };
		expect(cycle_origin(cycle) == cycle_workspace_origin{ }, "unbound: a plain folder, unclassified");
		cycle.Workspace = cycle_workspace_binding{ .Root = { .Kind = workspace_root_kind::folder, .Location = "/work/project" } };
		expect(cycle_origin(cycle).Origin == cycle_source_origin::local_folder && cycle_origin(cycle).Classification == cycle_data_classification::unclassified, "a plain folder of unknown origin");
		cycle.Workspace->Root.Kind = workspace_root_kind::git_linked_worktree;
		expect(cycle_origin(cycle).Origin == cycle_source_origin::local_git_checkout && cycle_origin(cycle).Classification == cycle_data_classification::unclassified, "a checkout, still unclassified");
	}

	auto note(const std::uint64_t aItem, const std::uint64_t aWording, std::vector<cycle_round_suggestion_pin> aSuggestions = { }, const private_thread_lifecycle aLifecycle = private_thread_lifecycle::open)
		-> cycle_next_round_item
	{
		return { .Item = cycle_feedback_item_id{ aItem }, .Wording = private_review_turn_revision_id{ aWording }, .Lifecycle = aLifecycle, .Suggestions = std::move(aSuggestions) };
	}

	auto items_of(const cycle_round_selection& aSelection) -> std::vector<std::uint64_t>
	{
		auto items = std::vector<std::uint64_t>{ };
		for (const auto& item : aSelection.Items) {
			items.push_back(item.Item.value());
		}
		return items;
	}

	void composes_one_deliberate_edit_at_a_time()
	{
		const auto pin   = cycle_round_suggestion_pin{ .Id = cycle_suggestion_id{ 1 }, .Revision = 1 };
		auto       notes = std::vector{ note(1, 1), note(2, 3, { pin }), note(3, 1, { }, private_thread_lifecycle::archived), note(4, 2) };
		const auto first = first_composition(notes, cycle_source_revision_id{ 2 });
		expect(items_of(first) == std::vector<std::uint64_t>{ 1, 2, 4 } && first.Source == cycle_source_revision_id{ 2 } && first.Action == cycle_round_action::implement, "every open note, in item order");
		expect(first.Items[1].Wording == private_review_turn_revision_id{ 3 } && first.Items[1].Suggestions == std::vector{ pin }, "its current wording and the suggestions it offers");

		const auto left = edit_composition(first, notes, include_round_item{ .Item = cycle_feedback_item_id{ 2 }, .Include = false });
		expect(left && items_of(*left) == std::vector<std::uint64_t>{ 1, 4 }, "leave a note out");
		auto related = first;
		related.Items[0].Related.emplace_back(2);
		const auto unrelated = edit_composition(related, notes, include_round_item{ .Item = cycle_feedback_item_id{ 2 }, .Include = false });
		expect(unrelated && unrelated->Items[0].Related.empty(), "and its relations go with it");
		const auto back = edit_composition(*left, notes, include_round_item{ .Item = cycle_feedback_item_id{ 2 } });
		expect(back && items_of(*back) == std::vector<std::uint64_t>{ 1, 4, 2 } && back->Items[2].Suggestions == std::vector{ pin }, "included again at the end, with its suggestion");
		expect(!edit_composition(first, notes, include_round_item{ .Item = cycle_feedback_item_id{ 1 } }), "including an included note changes nothing");
		expect(!edit_composition(first, notes, include_round_item{ .Item = cycle_feedback_item_id{ 3 } }), "an archived note is not offered");
		expect(!edit_composition(first, notes, include_round_item{ .Item = cycle_feedback_item_id{ 9 } }), "nor an unknown one");

		const auto earlier = edit_composition(first, notes, move_round_item{ .Item = cycle_feedback_item_id{ 4 }, .Earlier = true });
		expect(earlier && items_of(*earlier) == std::vector<std::uint64_t>{ 1, 4, 2 }, "move one place earlier");
		const auto later = edit_composition(first, notes, move_round_item{ .Item = cycle_feedback_item_id{ 1 }, .Earlier = false });
		expect(later && items_of(*later) == std::vector<std::uint64_t>{ 2, 1, 4 }, "move one place later");
		expect(!edit_composition(first, notes, move_round_item{ .Item = cycle_feedback_item_id{ 1 }, .Earlier = true }), "the first cannot move earlier");
		expect(!edit_composition(first, notes, move_round_item{ .Item = cycle_feedback_item_id{ 4 }, .Earlier = false }), "the last cannot move later");

		const auto plan = edit_composition(first, notes, set_round_action{ cycle_round_action::plan });
		expect(plan && plan->Action == cycle_round_action::plan && items_of(*plan) == items_of(first), "the requested action");
		expect(!edit_composition(first, notes, set_round_action{ cycle_round_action::implement }), "the same action changes nothing");
		const auto goal = edit_composition(first, notes, set_round_goal{ "Tighten parsing." });
		expect(goal && goal->Goal == "Tighten parsing.", "the optional goal, as written");

		// The reviewer reworded note 2 and revised its suggestion after choosing it.
		notes[1]             = note(2, 5, { { .Id = cycle_suggestion_id{ 1 }, .Revision = 2 } });
		const auto refreshed = refresh_composition(first, notes);
		expect(refreshed && refreshed->Items[1].Wording == private_review_turn_revision_id{ 5 } && refreshed->Items[1].Suggestions[0].Revision == 2U, "an included note's current wording and suggestion");
		expect(!refresh_composition(*refreshed, notes), "nothing to refresh when current");
		notes[0]        = note(1, 4, { }, private_thread_lifecycle::archived);
		const auto kept = refresh_composition(*refreshed, notes);
		expect(!kept && items_of(*refreshed) == std::vector<std::uint64_t>{ 1, 2, 4 }, "an archived note stays, so freezing names it");
	}

	auto observation(const cycle_delivery_state aState, const cycle_disclosure_channel aChannel) -> cycle_delivery_observation
	{
		return {
			.Round       = round.Id,
			.Digest      = round.Digest,
			.State       = aState,
			.Channel     = aChannel,
			.Output      = cycle_delivery_output{ .Format = cycle_round_file_kind::markdown, .Digest = identity<cycle_material_digest>(4), .Bytes = 120 },
			.Destination = aState == cycle_delivery_state::saved ? "RC-3-F2.md" : "",
		};
	}

	/// RC56-07: each state needs its own evidence; none stands for another.
	void admits_only_observations_their_channel_can_show()
	{
		using enum cycle_delivery_state;
		using enum cycle_disclosure_channel;
		const auto states   = std::array{ copied, saved, retrieved, accepted_by_adapter, recipient_acknowledged, failed, uncertain };
		const auto channels = std::array{ clipboard, local_file, terminal_output, external_retrieval, agent_adapter };
		const auto fits     = [](const cycle_delivery_state aState, const cycle_disclosure_channel aChannel) {
			switch (aState) {
			case copied:                 return aChannel == clipboard;
			case saved:                  return aChannel == local_file;
			case retrieved:              return aChannel == external_retrieval || aChannel == terminal_output;
			case accepted_by_adapter:    return aChannel == agent_adapter;
			case recipient_acknowledged: return aChannel == agent_adapter || aChannel == external_retrieval;
			case failed:
			case uncertain:              return true;
			}
			return false;
		};
		for (const auto state : states) {
			for (const auto channel : channels) {
				const auto problem = admit_delivery(subject, round, observation(state, channel));
				expect(fits(state, channel) ? !problem : problem == cycle_delivery_problem::wrong_channel, "state and channel");
			}
		}
		auto copiedElsewhere   = observation(copied, clipboard);
		copiedElsewhere.Digest = identity<cycle_round_digest>(8);
		expect(admit_delivery(subject, round, copiedElsewhere) == cycle_delivery_problem::wrong_round, "another digest is another round");
		auto foreign     = observation(copied, clipboard);
		foreign.Round.Id = cycle_round_id{ 3 };
		expect(admit_delivery(subject, round, foreign) == cycle_delivery_problem::wrong_round, "another round");
		auto bare   = observation(copied, clipboard);
		bare.Output = std::nullopt;
		expect(admit_delivery(subject, round, bare) == cycle_delivery_problem::output_required, "copied names the bytes that left");
		auto unconfirmed   = observation(uncertain, clipboard);
		unconfirmed.Output = std::nullopt;
		expect(!admit_delivery(subject, round, unconfirmed), "an unconfirmed copy needs no bytes");
		auto named        = observation(copied, clipboard);
		named.Destination = "RC-3-F2.md";
		expect(admit_delivery(subject, round, named) == cycle_delivery_problem::unexpected_destination, "only a saved file has a destination");
		for (const auto* path : { "", ".", "..", "/Users/someone/RC-3-F2.md", "notes/RC-3-F2.md", "C:RC.md", "folder\\RC.md", "RC\n.md" }) {
			auto saved        = observation(cycle_delivery_state::saved, local_file);
			saved.Destination = path;
			expect(admit_delivery(subject, round, saved) == cycle_delivery_problem::destination_not_a_name, "a file's name, never its folder");
		}
		auto longest        = observation(cycle_delivery_state::saved, local_file);
		longest.Destination = std::string(maximum_delivery_name_bytes, 'a');
		expect(!admit_delivery(subject, round, longest), "a name at its bound");
		longest.Destination.push_back('a');
		expect(admit_delivery(subject, round, longest) == cycle_delivery_problem::destination_not_a_name, "over it");
		expect(delivery_destination_name("RC-3-F2.bpr-review.zip") && delivery_destination_name("Review – F2.md"), "ordinary and non-ASCII names");
	}
	auto bytes_of(const std::string_view aText) -> std::span<const std::uint8_t>
	{
		return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
	}

	auto range_of(const std::string_view aFile, const std::string_view aText, const std::size_t aFrom = 0) -> cycle_byte_range
	{
		const auto at = aFile.find(aText, aFrom);
		expect(at != std::string_view::npos, "the test's text is in the file");
		return { .Start = at, .End = at + aText.size() };
	}

	/// Kind and line of each row, as "c12 n14 -14 +14 ~3": what the reviewer reads.
	auto shape(const cycle_note_excerpt& aExcerpt) -> std::string
	{
		auto text = std::string{ };
		for (const auto& row : aExcerpt.Rows) {
			if (!text.empty()) {
				text += ' ';
			}
			switch (row.Kind) {
			case cycle_excerpt_row_kind::context: text += 'c'; break;
			case cycle_excerpt_row_kind::noted:   text += 'n'; break;
			case cycle_excerpt_row_kind::removed: text += '-'; break;
			case cycle_excerpt_row_kind::added:   text += '+'; break;
			case cycle_excerpt_row_kind::omitted: text += '~'; break;
			}
			text += std::to_string(row.Kind == cycle_excerpt_row_kind::omitted ? row.Omitted : row.Line.value_or(0U));
		}
		return text;
	}

	void excerpts_a_note_with_its_changes()
	{
		// Thirty-one lines, as the design round's parser.cpp; line n reads "line n" except two.
		auto file = std::string{ };
		for (int line = 1; line <= 31; ++line) {
			file += line == 14 ? std::string{ "\treturn check(aText);" } : line == 17 ? std::string{ "int check(std::string_view aText)" }
																					  : "line " + std::to_string(line);
			file += '\n';
		}
		const auto call = range_of(file, "check(aText)");

		const auto plain = excerpt_note(bytes_of(file), call, { }, cycle_excerpt_context_lines, cycle_excerpt_rows);
		expect(plain.has_value() && plain->FileLines == 31U, "the final line ending adds no line");
		expect(plain->Lines == cycle_line_span{ .First = 14, .Last = 14 }, "the note's own line");
		expect(shape(*plain) == "c12 c13 n14 c15 c16", "two unchanged lines either side of the noted line");
		expect(plain->Rows[2].Text == "\treturn check(aText);" && plain->Rows[0].Text == "line 12", "rows hold each line's exact text, without its ending");
		expect(!plain->Cut && plain->Changes == 0U && plain->Unplaced == 0U, "nothing cut, no change");

		// The suggested rename, at both places: the parts meet, so no line is left out.
		const auto rename = std::array{
			cycle_excerpt_change{ .Start = range_of(file, "check", call.Start).Start, .End = range_of(file, "check", call.Start).End, .Original = "check", .Replacement = "count_tokens" },
			cycle_excerpt_change{ .Start = range_of(file, "check(std").Start, .End = range_of(file, "check(std").Start + 5U, .Original = "check", .Replacement = "count_tokens" },
		};
		const auto renamed = excerpt_note(bytes_of(file), call, rename, cycle_excerpt_context_lines, cycle_excerpt_rows);
		expect(renamed && shape(*renamed) == "c12 c13 -14 +14 c15 c16 -17 +17 c18 c19", "each change replaces its line in place, in file order");
		expect(renamed->Rows[3].Text == "\treturn count_tokens(aText);" && renamed->Rows[7].Text == "int count_tokens(std::string_view aText)", "an added line is its whole edited line");
		expect(renamed->Changes == 2U && !renamed->Cut, "both changes shown");

		// A change far away keeps its own context; the lines between are counted, not shown.
		const auto distant = std::array{ cycle_excerpt_change{ .Start = range_of(file, "line 28").Start, .End = range_of(file, "line 28").End, .Original = "line 28", .Replacement = "line twenty-eight" } };
		const auto apart   = excerpt_note(bytes_of(file), call, distant, cycle_excerpt_context_lines, cycle_excerpt_rows);
		expect(apart && shape(*apart) == "c12 c13 n14 c15 c16 ~9 c26 c27 -28 +28 c29 c30", "an omitted row stands for the nine lines between");

		// A change whose bytes are not at its place is left out, never shown elsewhere.
		const auto stale  = std::array{ cycle_excerpt_change{ .Start = call.Start, .End = call.End, .Original = "parse(aText)", .Replacement = "x" } };
		const auto missed = excerpt_note(bytes_of(file), call, stale, cycle_excerpt_context_lines, cycle_excerpt_rows);
		expect(missed && missed->Unplaced == 1U && missed->Changes == 0U && shape(*missed) == "c12 c13 n14 c15 c16", "a change the file does not hold is not placed");
		const auto overlapping = std::array{ rename[0], cycle_excerpt_change{ .Start = call.Start, .End = call.End, .Original = "check(aText)", .Replacement = "x" } };
		const auto overlap     = excerpt_note(bytes_of(file), call, overlapping, cycle_excerpt_context_lines, cycle_excerpt_rows);
		expect(overlap && overlap->Changes == 1U && overlap->Unplaced == 1U, "a change over lines already changed is left out");

		// The sheet's limit cuts the rows and says so; the larger view has room.
		const auto cut = excerpt_note(bytes_of(file), call, rename, cycle_excerpt_context_lines, 4U);
		expect(cut && cut->Cut && shape(*cut) == "c12 c13 -14 +14", "rows stop at the limit");
		const auto whole = excerpt_note(bytes_of(file), call, rename, cycle_excerpt_larger_rows, cycle_excerpt_larger_rows);
		expect(whole && !whole->Cut && whole->Rows.size() == 33U && whole->Rows.front().Line == 1U && whole->Rows.back().Line == 31U, "the larger view shows the whole file, each change in place");

		// Whole lines inserted at a line start replace nothing; whole lines deleted add nothing.
		const auto line20  = range_of(file, "line 20\n");
		const auto inserts = std::array{ cycle_excerpt_change{ .Start = line20.Start, .End = line20.Start, .Original = "", .Replacement = "// one\n// two\n" } };
		const auto added   = excerpt_note(bytes_of(file), std::nullopt, inserts, cycle_excerpt_context_lines, cycle_excerpt_rows);
		expect(added && !added->Lines && shape(*added) == "c18 c19 +20 +21 c20 c21", "an insertion shows only what it adds");
		const auto deletes = std::array{ cycle_excerpt_change{ .Start = line20.Start, .End = line20.End, .Original = "line 20\n", .Replacement = "" } };
		const auto deleted = excerpt_note(bytes_of(file), std::nullopt, deletes, cycle_excerpt_context_lines, cycle_excerpt_rows);
		expect(deleted && shape(*deleted) == "c18 c19 -20 c21 c22", "a deletion shows only what it removes");

		// CRLF and a lone CR end lines as project_lines counts them; no ending shows.
		const auto mixed = std::string{ "one\r\ntwo\rthree\nfour" };
		const auto four  = excerpt_note(bytes_of(mixed), range_of(mixed, "four"), { }, 1U, cycle_excerpt_rows);
		expect(four && four->FileLines == 4U && four->Lines == project_lines(bytes_of(mixed), range_of(mixed, "four").Start, range_of(mixed, "four").End), "lines agree with project_lines");
		expect(shape(*four) == "c3 n4" && four->Rows[0].Text == "three", "the line before, without its ending");
		const auto two = excerpt_note(bytes_of(mixed), range_of(mixed, "two"), { }, 1U, cycle_excerpt_rows);
		expect(two && two->Rows[0].Text == "one" && two->Rows[1].Text == "two", "a CRLF line keeps no CR");

		expect(!excerpt_note(bytes_of(file), cycle_byte_range{ .Start = 0, .End = file.size() + 1U }, { }, 2U, 14U), "a range outside the file has no excerpt");
		const auto opening = excerpt_note(bytes_of(file), std::nullopt, { }, cycle_excerpt_context_lines, cycle_excerpt_rows);
		expect(opening && shape(*opening) == "c1 c2 c3 c4 c5", "a whole-file note shows the file's opening");
	}
}

int main()
{
	lists_copy_and_save_first_and_optional_routes_only_when_reachable();
	decides_every_route_as_authorize_does();
	reads_origin_from_the_bound_workspace();
	composes_one_deliberate_edit_at_a_time();
	admits_only_observations_their_channel_can_show();
	excerpts_a_note_with_its_changes();
	std::cout << "review-cycle-share:pass\n";
	return EXIT_SUCCESS;
}
