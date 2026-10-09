// [[BPR-1088]]. The Overview's label columns as a stored preference
// (RFC-BPR-0049 R20, R28; RFC-BPR-0051 R41): every one of the twelve labels
// can be chosen, in any order, and kept as stable IDs; a repeated or unknown
// ID is refused when built and, when read back from storage, left out and
// counted while every valid ID keeps its place.

#include "bpr/core/feedback_label_columns.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(bool aValue, std::string_view aWhy)
	{
		if (!aValue) {
			std::cerr << "feedback label columns failure: " << aWhy << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto label_at(std::size_t aIndex) -> feedback_label
	{
		return static_cast<feedback_label>(aIndex);
	}

	auto columns(std::initializer_list<feedback_label> aLabels) -> feedback_label_columns
	{
		const auto labels = std::vector<feedback_label>(aLabels);
		auto       made   = make_feedback_label_columns(labels);
		expect(made.has_value(), "the fixture's own selection is valid");
		return *made;
	}

	void every_label_and_order_round_trips()
	{
		const auto none = feedback_label_columns{ };
		expect(labels_of(none).empty() && feedback_label_columns_text({ }).empty(), "the default chooses no column and stores nothing");
		expect(read_feedback_label_columns("") == feedback_label_columns_reading{ }, "empty text reads as no column, with nothing dropped");
		for (std::size_t index = 0U; index < feedback_label_count; ++index) {
			const auto one  = columns({ label_at(index) });
			const auto text = feedback_label_columns_text(one);
			expect(text == stable_name(label_at(index)), "one column stores its stable ID");
			expect(read_feedback_label_columns(text) == feedback_label_columns_reading{ .Columns = one }, "each of the twelve labels reads back alone");
		}
		auto all = std::array<feedback_label, feedback_label_count>{ };
		for (std::size_t index = 0U; index < all.size(); ++index) {
			all[index] = label_at(feedback_label_count - 1U - index);
		}
		const auto reversed = make_feedback_label_columns(all);
		expect(reversed.has_value() && reversed->Count == feedback_label_count, "all twelve, in reverse catalog order, are one selection");
		const auto text = feedback_label_columns_text(*reversed);
		expect(text == "quibble,polish,typo,note,chore,thought,question,todo,issue,suggestion,nitpick,praise", "all twelve store in the chosen order");
		expect(text.size() == 84U && text.size() <= maximum_feedback_label_columns_text, "the longest selection fits the stored bound");
		const auto back = read_feedback_label_columns(text);
		expect(back.Unknown == 0U && back.Repeated == 0U && back.Columns == *reversed, "all twelve read back in the same order with nothing dropped");
		// Every rotation of the catalog, so each label is first and last once.
		for (std::size_t shift = 0U; shift < feedback_label_count; ++shift) {
			auto rotated = std::array<feedback_label, feedback_label_count>{ };
			for (std::size_t index = 0U; index < rotated.size(); ++index) {
				rotated[index] = label_at((index + shift) % feedback_label_count);
			}
			const auto made = make_feedback_label_columns(std::span{ rotated }.first(1U + shift));
			expect(made.has_value() && read_feedback_label_columns(feedback_label_columns_text(*made)).Columns == *made, "every rotation and length round-trips");
			expect(std::ranges::equal(labels_of(*made), std::span{ rotated }.first(1U + shift)), "labels come back in the order chosen");
		}
	}

	void building_refuses_rather_than_drops()
	{
		const auto twice = std::array{ feedback_label::issue, feedback_label::question, feedback_label::issue };
		expect(make_feedback_label_columns(twice).error() == feedback_label_columns_refusal::duplicate_label, "a label chosen twice is refused, not deduplicated");
		const auto unknown = std::array{ feedback_label::issue, static_cast<feedback_label>(12) };
		expect(make_feedback_label_columns(unknown).error() == feedback_label_columns_refusal::unknown_label, "a value past the catalog is refused");
		auto thirteen = std::array<feedback_label, feedback_label_count + 1U>{ };
		expect(make_feedback_label_columns(thirteen).error() == feedback_label_columns_refusal::too_many, "more entries than labels are refused");

		auto built = columns({ feedback_label::note, feedback_label::issue });
		expect(check_feedback_label_columns(built).has_value(), "a built selection checks clean");
		auto repeated      = built;
		repeated.Labels[1] = feedback_label::note;
		expect(check_feedback_label_columns(repeated).error() == feedback_label_columns_refusal::duplicate_label, "a hand-built repeat fails the check");
		auto outside      = built;
		outside.Labels[0] = static_cast<feedback_label>(200);
		expect(check_feedback_label_columns(outside).error() == feedback_label_columns_refusal::unknown_label, "a hand-built unknown label fails the check");
		auto padded       = built;
		padded.Labels[11] = feedback_label::quibble;
		expect(check_feedback_label_columns(padded).error() == feedback_label_columns_refusal::noncanonical && padded != built, "storage past Count must stay canonical, so equality means the same selection");
		auto overfull  = built;
		overfull.Count = 13U;
		expect(check_feedback_label_columns(overfull).error() == feedback_label_columns_refusal::too_many && labels_of(overfull).size() == feedback_label_count, "an overfull count fails the check and reads no further than the array");
		expect(stable_name(feedback_label_columns_refusal::duplicate_label) == "duplicate_label" && stable_name(feedback_label_columns_refusal::unknown_label) == "unknown_label", "refusals have stable names");
	}

	void reading_keeps_every_valid_id_and_counts_the_rest()
	{
		const auto repeated = read_feedback_label_columns("issue,question,issue");
		expect(repeated.Columns == columns({ feedback_label::issue, feedback_label::question }) && repeated.Repeated == 1U && repeated.Unknown == 0U,
			   "a repeated ID is left out once, the first place kept, and counted");
		const auto unknown = read_feedback_label_columns("praise,blocker,todo");
		expect(unknown.Columns == columns({ feedback_label::praise, feedback_label::todo }) && unknown.Unknown == 1U && unknown.Repeated == 0U,
			   "an unknown ID is left out and counted; the IDs around it keep their order");
		const auto cased = read_feedback_label_columns("Issue,ISSUE,issue");
		expect(cased.Columns == columns({ feedback_label::issue }) && cased.Unknown == 2U, "only exact stable IDs are labels: no case folding");
		const auto empty = read_feedback_label_columns("issue,,note,");
		expect(empty.Columns == columns({ feedback_label::issue, feedback_label::note }) && empty.Unknown == 2U, "empty entries count as unknown");
		const auto spaced = read_feedback_label_columns(" issue");
		expect(spaced.Columns.Count == 0U && spaced.Unknown == 1U, "surrounding spaces are not trimmed into an ID");
		const auto nothing = read_feedback_label_columns(",");
		expect(nothing.Columns.Count == 0U && nothing.Unknown == 2U, "a lone comma is two empty entries");

		auto many = std::string{ };
		for (int round = 0; round < 3; ++round) {
			for (std::size_t index = 0U; index < feedback_label_count; ++index) {
				many += std::string{ stable_name(label_at(index)) } + ",";
			}
		}
		many.pop_back();
		expect(many.size() == 254U, "three catalogs fill the bound");
		const auto thrice = read_feedback_label_columns(many);
		expect(thrice.Columns.Count == feedback_label_count && thrice.Repeated == 24U && thrice.Unknown == 0U, "every label once from three copies; 24 repeats");

		const auto longText = many + ",chore,note";
		const auto cut      = read_feedback_label_columns(longText);
		expect(cut.Columns.Count == feedback_label_count && cut.Unknown == 1U, "text past the bound is not read and counts as one unknown entry");
		const auto noComma = read_feedback_label_columns(std::string(maximum_feedback_label_columns_text + 10U, 'x'));
		expect(noComma.Columns.Count == 0U && noComma.Unknown == 1U, "an over-long entry with no comma reads as nothing, with one unknown");
		const auto saturated = read_feedback_label_columns(std::string(maximum_feedback_label_columns_text, ','));
		expect(saturated.Unknown == 255U, "counts saturate rather than wrap");
		expect(check_feedback_label_columns(saturated.Columns).has_value() && check_feedback_label_columns(thrice.Columns).has_value(), "whatever is read is a valid selection");
	}
}

int main()
{
	every_label_and_order_round_trips();
	building_refuses_rather_than_drops();
	reading_keeps_every_valid_id_and_counts_the_rest();
	std::cout << "FEEDBACK_LABEL_COLUMNS_OK\n";
	return EXIT_SUCCESS;
}
