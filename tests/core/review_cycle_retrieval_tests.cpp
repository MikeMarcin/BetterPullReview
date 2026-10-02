#include "bpr/core/review_cycle_retrieval.hpp"

#include "bpr/core/review_cycle_command.hpp"
#include "bpr/core/review_cycle_policy.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

/// [[BPR-1169]]. Reading one frozen round from outside the review: its
/// measure, whole-item pages bound to the round's digest, a consumer's stated
/// budget, and the round's own attachments only.
namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const char* aWhat)
	{
		if (!aValue) {
			std::cerr << "review cycle retrieval failure: " << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto item(const std::size_t aIndex, const std::string& aBody) -> cycle_wire_item
	{
		return { .Id = "I" + std::to_string(aIndex + 1U), .Revision = 1, .Body = aBody, .Anchors = { { .Kind = cycle_anchor_kind::cycle, .Revision = "R1" } } };
	}

	auto round_of(const std::size_t aItems, const std::size_t aBodyBytes) -> cycle_decoded_round
	{
		auto round                    = cycle_decoded_round{ };
		round.Payload.CycleId         = "RC-1";
		round.Payload.RoundId         = "F2";
		round.Payload.SourceRevision  = "R1";
		round.Payload.Assignment.Goal = "Make it total.";
		for (std::size_t index = 0; index < aItems; ++index) {
			round.Payload.Items.push_back(item(index, std::string(aBodyBytes, static_cast<char>('a' + static_cast<char>(index % 26U)))));
		}
		round.Digest = cycle_round_digest{ std::array<std::uint8_t, 32>{ 7, 1, 2, 3 } };
		return round;
	}

	auto source(const std::size_t aIndex, const std::size_t aBytes) -> cycle_wire_source
	{
		return { .Id = "S" + std::to_string(aIndex + 1U), .Path = "src/f" + std::to_string(aIndex) + ".cpp", .Revision = "R1", .Kind = cycle_context_kind::full_file, .Start = 0, .End = aBytes, .Bytes = std::string(aBytes, 'x') };
	}

	void measures_what_must_be_held()
	{
		auto round = round_of(3, 100);
		round.Payload.Items[1].Suggestions.push_back({ .Id = "SG2.1", .Anchor = { .Kind = cycle_anchor_kind::file, .Path = "a", .Revision = "R1" }, .Original = "x", .Replacement = "y" });
		round.Payload.Sources.push_back(source(0, 10));
		const auto extent = measure_round(round.Payload);
		auto       bytes  = std::uint64_t{ round.Payload.Assignment.Goal.size() };
		for (const auto& entry : round.Payload.Items) {
			bytes += encode_round_item(entry)->size();
		}
		expect(extent && extent->Items == 3U && extent->Suggestions == 1U && extent->Sources == 1U && extent->SourceBytes == 10U, "counts");
		expect(extent->InstructionBytes == bytes, "every item's canonical bytes and the assignment's texts");
	}

	void cursors_bind_one_round()
	{
		const auto round  = round_of(1, 10);
		const auto cursor = cycle_round_cursor{ .Round = cycle_round_id{ 2 }, .Digest = round.Digest, .Next = 7, .PageBytes = 8192 };
		const auto text   = encode_round_cursor(cursor);
		expect(text.starts_with("F2.0701020300") && text.ends_with(".7.8192"), "a readable reference");
		expect(parse_round_cursor(text) == cursor, "round trip");
		for (const auto* broken : { "F2.07.7.8192", "F02.0701020300000000000000000000000000000000000000000000000000000000.7.8192",
									"F2.0701020300000000000000000000000000000000000000000000000000000000.07.8192", "F2.0701020300000000000000000000000000000000000000000000000000000000.7",
									"F2.0701020300000000000000000000000000000000000000000000000000000000.7.8192.1", "F2.0701020300000000000000000000000000000000000000000000000000000000.-7.8192",
									"F2.0701020300000000000000000000000000000000000000000000000000000000.7.8192 ", "" }) {
			expect(!parse_round_cursor(broken), "any other spelling is refused");
		}
		expect(parse_round_cursor("F2.0701020300000000000000000000000000000000000000000000000000000000.7.8192")
				   && !parse_round_cursor("F2.0A01020300000000000000000000000000000000000000000000000000000000.7.8192"),
			   "lowercase digests only");
	}

	void pages_cover_every_item_once()
	{
		const auto round  = round_of(40, 500);
		const auto extent = *measure_round(round.Payload);
		auto       seen   = std::vector<std::string>{ };
		auto       from   = std::uint64_t{ 0 };
		auto       pages  = 0;
		while (true) {
			const auto page = page_round_items(round, cycle_round_id{ 2 }, from, minimum_round_page_bytes);
			expect(page.has_value() && page->First == from && !page->Items.empty(), "a page starts where it says");
			expect(page->Bytes <= minimum_round_page_bytes, "within the page");
			for (const auto& entry : page->Items) {
				seen.push_back(entry);
			}
			++pages;
			if (!page->Next) {
				break;
			}
			expect(page->Next->Round == cycle_round_id{ 2 } && page->Next->Digest == round.Digest && page->Next->PageBytes == minimum_round_page_bytes, "the cursor binds the round, its digest and the page size");
			expect(!admit_round_cursor(*page->Next, cycle_round_id{ 2 }, round.Digest, extent), "and is admitted for it");
			from = page->Next->Next;
		}
		expect(pages > 3, "several pages");
		expect(seen.size() == round.Payload.Items.size(), "every item, once");
		for (std::size_t index = 0; index < seen.size(); ++index) {
			expect(seen[index] == *encode_round_item(round.Payload.Items[index]), "in order, exactly as the payload holds it");
		}
		const auto whole = page_round_items(round, cycle_round_id{ 2 }, 0, default_round_page_bytes);
		expect(whole && whole->Items.size() == 40U && !whole->Next, "one page when it fits");
	}

	void refuses_what_would_mislead()
	{
		const auto round  = round_of(5, 100);
		const auto extent = *measure_round(round.Payload);
		expect(page_round_items(round, cycle_round_id{ 2 }, 0, minimum_round_page_bytes - 1U).error() == cycle_retrieval_problem::page_size, "below the smallest page");
		expect(page_round_items(round, cycle_round_id{ 2 }, 0, maximum_round_page_bytes + 1U).error() == cycle_retrieval_problem::page_size, "over the largest page");
		expect(page_round_items(round, cycle_round_id{ 2 }, 5, default_round_page_bytes).error() == cycle_retrieval_problem::cursor, "past the last item");
		const auto good = cycle_round_cursor{ .Round = cycle_round_id{ 2 }, .Digest = round.Digest, .Next = 3, .PageBytes = minimum_round_page_bytes };
		expect(!admit_round_cursor(good, cycle_round_id{ 2 }, round.Digest, extent), "a cursor of this round");
		auto other  = good;
		other.Round = cycle_round_id{ 1 };
		expect(admit_round_cursor(other, cycle_round_id{ 2 }, round.Digest, extent) == cycle_retrieval_problem::cursor, "another round's cursor");
		other        = good;
		other.Digest = cycle_round_digest{ std::array<std::uint8_t, 32>{ 9 } };
		expect(admit_round_cursor(other, cycle_round_id{ 2 }, round.Digest, extent) == cycle_retrieval_problem::cursor, "another digest's cursor");
		other      = good;
		other.Next = 0;
		expect(admit_round_cursor(other, cycle_round_id{ 2 }, round.Digest, extent) == cycle_retrieval_problem::cursor, "the first page is a read, not a cursor");
		other.Next = 5;
		expect(admit_round_cursor(other, cycle_round_id{ 2 }, round.Digest, extent) == cycle_retrieval_problem::cursor, "past the end");
		other           = good;
		other.PageBytes = 1;
		expect(admit_round_cursor(other, cycle_round_id{ 2 }, round.Digest, extent) == cycle_retrieval_problem::page_size, "a page size no read could give");

		// An item larger than the page asked for comes whole, alone; one larger than any page is never split.
		auto large                  = round_of(3, 100);
		large.Payload.Items[1].Body = std::string(20000, 'q');
		const auto alone            = page_round_items(large, cycle_round_id{ 2 }, 1, minimum_round_page_bytes);
		expect(alone && alone->Items.size() == 1U && alone->Items[0] == *encode_round_item(large.Payload.Items[1]) && alone->Next && alone->Next->Next == 2U, "whole and alone");
		large.Payload.Items[1].Body = std::string(maximum_round_page_bytes + 1U, 'q');
		expect(page_round_items(large, cycle_round_id{ 2 }, 1, maximum_round_page_bytes).error() == cycle_retrieval_problem::item_over_page, "never split");
		const auto before = page_round_items(large, cycle_round_id{ 2 }, 0, maximum_round_page_bytes);
		expect(before && before->Items.size() == 1U && before->Next && before->Next->Next == 1U, "the page before it stops short of it");
	}

	void budgets_ask_for_a_partition()
	{
		const auto extent = *measure_round(round_of(10, 1000).Payload);
		expect(admit_round_budget(extent, extent.InstructionBytes) == std::nullopt, "exactly enough");
		expect(admit_round_budget(extent, extent.InstructionBytes - 1U) == cycle_retrieval_problem::over_budget, "one byte short is a partition, not a first page");
	}

	void attachments_are_the_rounds_own()
	{
		auto payload = round_of(1, 10).Payload;
		expect(list_round_sources(payload, 0) && list_round_sources(payload, 0)->Count == 0U && !list_round_sources(payload, 0)->Next, "a round with none lists none");
		for (std::size_t index = 0; index < 300U; ++index) {
			payload.Sources.push_back(source(index, 16));
		}
		const auto first = list_round_sources(payload, 0);
		expect(first && first->Count == round_source_list_entries && first->Next == round_source_list_entries, "a bounded listing");
		const auto rest = list_round_sources(payload, *first->Next);
		expect(rest && rest->Count == 300U - round_source_list_entries && !rest->Next, "and the rest");
		expect(list_round_sources(payload, 300).error() == cycle_retrieval_problem::source_cursor && list_round_sources(payload, 301).error() == cycle_retrieval_problem::source_cursor, "past the end");

		expect(slice_round_source(payload, "S301", std::nullopt).error() == cycle_retrieval_problem::unknown_source, "no attachment by that name");
		expect(slice_round_source(payload, "src/f0.cpp", std::nullopt).error() == cycle_retrieval_problem::unknown_source, "a path is not an attachment identity");
		const auto whole = slice_round_source(payload, "S2", std::nullopt);
		expect(whole && whole->Source == 1U && whole->Range == cycle_source_range{ 0, 16 } && !whole->Next, "a whole attachment");
		const auto part = slice_round_source(payload, "S2", cycle_source_range{ 4, 9 });
		expect(part && part->Range == cycle_source_range{ 4, 9 }, "a range of it");
		expect(slice_round_source(payload, "S2", cycle_source_range{ 4, 17 }).error() == cycle_retrieval_problem::source_range, "not beyond it");
		payload.Sources.push_back(source(300, maximum_round_source_chunk_bytes + 10U));
		const auto chunk = slice_round_source(payload, "S301", std::nullopt);
		expect(chunk && chunk->Range == cycle_source_range{ 0, maximum_round_source_chunk_bytes } && chunk->Next == cycle_source_range{ maximum_round_source_chunk_bytes, maximum_round_source_chunk_bytes + 10U }, "a read is bounded and says what is left");

		expect(parse_source_range("0:16") == cycle_source_range{ 0, 16 } && parse_source_range("4:9") == cycle_source_range{ 4, 9 }, "START:END");
		for (const auto* broken : { "9:4", "4:4", "04:9", ":9", "4:", "4-9", "4:9:1", "a:9", "" }) {
			expect(!parse_source_range(broken), "any other spelling is refused");
		}
	}

	void consumer_commands_are_grant_scoped()
	{
		for (const auto command : { cycle_command::round_read, cycle_command::round_page, cycle_command::round_source, cycle_command::response_append }) {
			expect(authority_of(command) == cycle_command_authority::granted_consumer, "a consumer command acts only on a grant");
		}
		for (const auto command : { cycle_command::round_grant, cycle_command::round_revoke, cycle_command::policy, cycle_command::export_round, cycle_command::freeze, cycle_command::review, cycle_command::show }) {
			expect(authority_of(command) == cycle_command_authority::reviewer, "granting, policy and authoring are the reviewer's");
		}
		expect(!may_write(cycle_command::round_read) && !may_write(cycle_command::round_page) && !may_write(cycle_command::round_source), "reading a round changes nothing");
		expect(may_write(cycle_command::response_append) && may_write(cycle_command::round_grant) && may_write(cycle_command::round_revoke) && may_write(cycle_command::policy), "and these write");
		expect(parse_cycle_command("round read") == cycle_command::round_read && parse_cycle_command("response append") == cycle_command::response_append, "typed by name");
		const auto options = cycle_command_options(cycle_command::response_append);
		expect(std::ranges::find(options, std::string_view{ "--round" }) != options.end() && options.size() == 2U, "a response names its round and its file, nothing more");
	}

	void stored_and_imposed_policies_combine()
	{
		auto stored     = with_permission({ }, cycle_data_classification::unclassified, cycle_disclosure_channel::external_retrieval, cycle_channel_permission::allowed);
		auto imposed    = with_permission({ }, cycle_data_classification::unclassified, cycle_disclosure_channel::clipboard, cycle_channel_permission::denied);
		imposed         = with_permission(imposed, cycle_data_classification::enterprise_restricted, cycle_disclosure_channel::external_retrieval, cycle_channel_permission::denied);
		stored          = with_permission(stored, cycle_data_classification::enterprise_restricted, cycle_disclosure_channel::external_retrieval, cycle_channel_permission::allowed);
		const auto both = combine_policies(stored, imposed);
		expect(configured_permission(both, cycle_data_classification::unclassified, cycle_disclosure_channel::external_retrieval) == cycle_channel_permission::allowed, "an allowance stands");
		expect(configured_permission(both, cycle_data_classification::unclassified, cycle_disclosure_channel::clipboard) == cycle_channel_permission::denied, "a denial stands");
		expect(configured_permission(both, cycle_data_classification::enterprise_restricted, cycle_disclosure_channel::external_retrieval) == cycle_channel_permission::denied, "a denial beats an allowance");
		expect(configured_permission(both, cycle_data_classification::public_source, cycle_disclosure_channel::agent_adapter) == cycle_channel_permission::unconfigured, "nothing configured stays unconfigured");
		expect(combine_policies(stored, { }) == stored && combine_policies({ }, imposed) == imposed, "nothing combined changes nothing");
		for (std::size_t channel = 0; channel < cycle_disclosure_channel_count; ++channel) {
			const auto value = static_cast<cycle_disclosure_channel>(channel);
			expect(parse_channel(stable_name(value)) == value, "channel names round trip");
		}
		expect(!parse_channel("Clipboard") && !parse_classification("enterprise") && !parse_permission("allow"), "exact names only");
	}
}

auto main() -> int
{
	measures_what_must_be_held();
	cursors_bind_one_round();
	pages_cover_every_item_once();
	refuses_what_would_mislead();
	budgets_ask_for_a_partition();
	attachments_are_the_rounds_own();
	consumer_commands_are_grant_scoped();
	stored_and_imposed_policies_combine();
	std::cout << "review cycle retrieval: pass\n";
	return EXIT_SUCCESS;
}
