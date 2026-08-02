#include "bpr/core/diff_semantics.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		std::array<std::uint8_t, 32> bytes{ };
		bytes[0]  = aSeed;
		bytes[31] = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return Identity{ bytes };
	}

	[[nodiscard]] constexpr auto blob(const std::uint8_t aSeed) -> git_blob_oid
	{
		std::array<std::uint8_t, 32> bytes{ };
		for (std::size_t index = 0U; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + static_cast<std::uint8_t>(index));
		}
		return git_blob_oid{ git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto comparison_key(const diff_options& aOptions,
									  const bool          aHasLeft,
									  const bool          aHasRight,
									  const std::uint8_t  aFileSeed)
		-> file_comparison_key
	{
		const auto digest = options_digest(aOptions);
		expect(digest.has_value(), "test options are normalized");
		const auto scope       = opaque<pull_request_authority_scope_id>(11U);
		const auto incarnation = opaque<epoch_catalog_incarnation_id>(12U);
		const auto authority   = pull_request_authority{
			.Profile         = host_profile_id{ 10U },
			.ProfileRevision = github_profile_revision{ 20U },
			.Host            = git_host_id{ 30U },
			.HostAuthority   = opaque<host_authority_fingerprint>(13U),
			.Account         = github_account_id{ 40U },
			.Viewer          = github_viewer_actor_id{ 50U },
			.Repository      = repository_id{ 101U },
			.Cache           = opaque<repository_cache_scope_id>(14U),
			.PullRequest     = pull_request_id{ 202U },
			.Number          = pull_request_number{ 17U },
			.Scope           = scope,
		};
		return {
			.Authority   = authority,
			.Repository  = authority.Repository,
			.PullRequest = authority.PullRequest,
			.LeftAnchor  = comparison_anchor_identity{
				.Authority   = scope,
				.Incarnation = incarnation,
				.Kind        = comparison_anchor_kind::epoch_base,
				.Epoch       = epoch_ordinal{ 1 },
			},
			.RightAnchor = comparison_anchor_identity{
				.Authority   = scope,
				.Incarnation = incarnation,
				.Kind        = comparison_anchor_kind::epoch_head,
				.Epoch       = epoch_ordinal{ 2 },
			},
			.LeftBlob  = aHasLeft ? std::optional{ blob(21U) } : std::nullopt,
			.RightBlob = aHasRight ? std::optional{ blob(51U) } : std::nullopt,
			.File      = opaque<file_identity>(aFileSeed),
			.Options   = *digest,
			.Algorithm = diff_algorithm_revision{ 1U },
		};
	}

	[[nodiscard]] auto bytes(const std::string_view aText)
		-> std::vector<std::uint8_t>
	{
		return { aText.begin(), aText.end() };
	}

	[[nodiscard]] auto run(const std::optional<std::string_view> aLeft,
						   const std::optional<std::string_view> aRight,
						   const diff_options&                   aOptions  = { },
						   const bool                            aHasLeft  = true,
						   const bool                            aHasRight = true,
						   const std::uint8_t                    aFileSeed = 31U) -> computed_file_diff
	{
		auto       leftBytes  = aLeft.has_value() ? std::optional{ bytes(*aLeft) } : std::nullopt;
		auto       rightBytes = aRight.has_value() ? std::optional{ bytes(*aRight) } : std::nullopt;
		const auto outcome    = compute_file_diff({
			.Key        = comparison_key(aOptions, aHasLeft, aHasRight, aFileSeed),
			.Options    = aOptions,
			.LeftBytes  = leftBytes.has_value()
				? std::optional{ std::span<const std::uint8_t>{ *leftBytes } }
				: std::nullopt,
			.RightBytes = rightBytes.has_value()
				? std::optional{ std::span<const std::uint8_t>{ *rightBytes } }
				: std::nullopt,
		});
		expect(outcome.Result.has_value(), "fixture publishes a result");
		return *outcome.Result;
	}

	[[nodiscard]] constexpr auto line(const computed_file_diff& aDiff,
									  const diff_side           aSide,
									  const std::uint32_t       aNumber) -> source_line_ref
	{
		return { .Comparison = aDiff.Alignment.Digest, .Side = aSide, .Line = aNumber };
	}

	[[nodiscard]] auto segment(const computed_file_diff&  aDiff,
							   const alignment_segment_id aId)
		-> const alignment_segment&
	{
		const auto found = std::ranges::find(aDiff.Alignment.Segments, aId, &alignment_segment::Id);
		expect(found != aDiff.Alignment.Segments.end(), "segment identity exists");
		return *found;
	}

	auto test_exact_and_one_sided_mapping() -> void
	{
		const auto exact = run("alpha\nbeta\n", "alpha\nbeta\n");
		const auto left  = map_line(exact, line(exact, diff_side::left, 1U));
		expect(left.Kind == line_mapping_kind::exact_line && left.Counterpart == line(exact, diff_side::right, 1U) && !left.Group.has_value() && !left.Pair.has_value(),
			   "equal line maps to its exact counterpart");
		const auto reverse = map_line(exact, *left.Counterpart);
		expect(reverse.Kind == line_mapping_kind::exact_line && reverse.Counterpart == left.Source && reverse.Segment == left.Segment,
			   "equal mapping is exactly reversible");

		const auto insertion = run("a\nc\n", "a\nb\nc\n");
		const auto inserted  = map_line(insertion, line(insertion, diff_side::right, 1U));
		expect(inserted.Kind == line_mapping_kind::opposing_boundary && inserted.OpposingBoundary == source_boundary_ref{ insertion.Alignment.Digest, diff_side::left, 1U },
			   "middle insertion maps to the exact empty left boundary");
		const auto insertionDetail = detail_for_mapping(insertion, inserted);
		expect(insertionDetail.Kind == line_detail_kind::opposing_boundary && insertionDetail.Selected == inserted.Source && !insertionDetail.LeftLine.has_value() && insertionDetail.RightLine == inserted.Source && insertionDetail.OpposingBoundary == inserted.OpposingBoundary && insertionDetail.Segment == inserted.Segment && insertionDetail.Group == inserted.Group,
			   "one-sided detail preserves selected line boundary and identities");

		const auto deletion = run("a\nb\nc\n", "a\nc\n");
		const auto deleted  = map_line(deletion, line(deletion, diff_side::left, 1U));
		expect(deleted.Kind == line_mapping_kind::opposing_boundary && deleted.OpposingBoundary == source_boundary_ref{ deletion.Alignment.Digest, diff_side::right, 1U },
			   "middle deletion maps to the exact empty right boundary");
		const auto deletionDetail = detail_for_mapping(deletion, deleted);
		expect(deletionDetail.LeftLine == deleted.Source && !deletionDetail.RightLine.has_value(),
			   "deletion detail populates only its real left source line");

		const auto added     = run(std::nullopt, "new\n", { }, false, true);
		const auto addedLine = map_line(added, line(added, diff_side::right, 0U));
		expect(addedLine.Kind == line_mapping_kind::opposing_boundary && addedLine.OpposingBoundary->Boundary == 0U && addedLine.OpposingBoundary->Side == diff_side::left,
			   "added-file content maps to boundary zero on the absent side");
	}

	auto test_pair_span_and_unequal_tail_mapping() -> void
	{
		const auto paired = run("prefix alpha suffix\n", "prefix beta suffix\n");
		const auto left   = map_line(paired, line(paired, diff_side::left, 0U));
		const auto right  = map_line(paired, line(paired, diff_side::right, 0U));
		expect(left.Kind == line_mapping_kind::similar_line && right.Kind == line_mapping_kind::similar_line && left.Counterpart == right.Source && right.Counterpart == left.Source && left.Pair == right.Pair && left.Segment == right.Segment && left.Group == right.Group,
			   "accepted similar pair is reversible with one stable identity set");
		const auto detail = detail_for_line(paired, left.Source);
		expect(detail.Kind == line_detail_kind::paired_lines && detail.LeftLine == left.Source && detail.RightLine == left.Counterpart && detail.Pair == left.Pair && detail.Segment == left.Segment && detail.Group == left.Group && !detail.Intraline.empty() && std::ranges::all_of(detail.Intraline, [&detail](const intraline_edit& aEdit) { return aEdit.Pair == *detail.Pair; }),
			   "paired detail contains only that pair's intraline edits");

		auto unpairedOptions                        = diff_options{ };
		unpairedOptions.Pairing                     = pairing_policy::disabled;
		unpairedOptions.SimilarityThresholdPermille = 0U;
		const auto unpaired                         = run("left one\nleft two\n",
														  "right one\nright two\nright three\n",
														  unpairedOptions);
		const auto fromLeft                         = map_line(unpaired, line(unpaired, diff_side::left, 1U));
		const auto fromRight                        = map_line(unpaired, line(unpaired, diff_side::right, 2U));
		expect(fromLeft.Kind == line_mapping_kind::opposing_span && fromLeft.OpposingSpan->Lines == line_span{ 0U, 3U } && fromRight.Kind == line_mapping_kind::opposing_span && fromRight.OpposingSpan->Lines == line_span{ 0U, 2U },
			   "weak unpaired lines expose the complete opposing replacement span");
		const auto unpairedDetail = detail_for_mapping(unpaired, fromRight);
		expect(unpairedDetail.Kind == line_detail_kind::opposing_span && unpairedDetail.Selected == fromRight.Source && !unpairedDetail.LeftLine.has_value() && unpairedDetail.RightLine == fromRight.Source && unpairedDetail.OpposingSpan == fromRight.OpposingSpan && !unpairedDetail.Pair.has_value() && unpairedDetail.Intraline.empty(),
			   "unpaired detail does not invent a line or intraline edits");

		const auto four  = std::string_view{ "item one\nitem two\nitem three\nitem four\n" };
		const auto seven = std::string_view{
			"item 1\nitem 2\nitem 3\nitem 4\nitem 5\nitem 6\nitem 7\n"
		};
		const auto unequal = run(four, seven);
		for (auto index = std::uint32_t{ 0U }; index < 4U; ++index) {
			expect(map_line(unequal, line(unequal, diff_side::left, index))
						   .Counterpart
					   == line(unequal, diff_side::right, index),
				   "4:7 accepted anchors map exactly by their pair");
		}
		for (auto index = std::uint32_t{ 4U }; index < 7U; ++index) {
			const auto tail = map_line(unequal, line(unequal, diff_side::right, index));
			expect(tail.Kind == line_mapping_kind::opposing_boundary && tail.OpposingBoundary->Boundary == 4U && tail.OpposingBoundary->Side == diff_side::left,
				   "4:7 unpaired tail maps to the exact left endpoint boundary");
		}
	}

	auto test_projection_navigation_and_identity() -> void
	{
		expect(navigate_change(computed_file_diff{ },
							   std::nullopt,
							   change_navigation_direction::next,
							   change_navigation_policy::stop)
					   .Outcome
				   == change_navigation_outcome::invalid_selection,
			   "default projection fails navigation closed");
		const auto diff       = run("same\nold one\nmiddle\nold two\nend\n",
									"same\nnew one\nmiddle\nnew two\nend\n");
		const auto projection = project_changes(diff);
		expect(projection.available() && projection.Changes.size() == 2U && projection.Changes[0].Group == diff.Alignment.Groups[0] && projection.Changes[1].Group == diff.Alignment.Groups[1],
			   "change projection retains canonical group vector order exactly");
		for (std::size_t groupIndex = 0U; groupIndex < projection.Changes.size();
			 ++groupIndex) {
			const auto& change = projection.Changes[groupIndex];
			const auto& group  = diff.Alignment.Groups[groupIndex];
			expect(change.Segments.size() == static_cast<std::size_t>(group.Members.End - group.Members.Begin),
				   "change projection uses the canonical member index span");
			for (std::size_t member = 0U; member < change.Segments.size(); ++member) {
				expect(change.Segments[member].Id == diff.Alignment.Segments[group.Members.Begin + member].Id,
					   "projected segment identity comes from the alignment");
			}
		}

		const auto first        = projection.Changes.front().Group.Id;
		const auto last         = projection.Changes.back().Group.Id;
		const auto noneNext     = navigate_change(diff,
												  std::nullopt,
												  change_navigation_direction::next,
												  change_navigation_policy::stop);
		const auto nonePrevious = navigate_change(diff,
												  std::nullopt,
												  change_navigation_direction::previous,
												  change_navigation_policy::wrap);
		expect(noneNext.Outcome == change_navigation_outcome::selected && noneNext.Group == first && noneNext.Index == 0U && nonePrevious.Outcome == change_navigation_outcome::selected && nonePrevious.Group == last && nonePrevious.Index == 1U,
			   "no-selection navigation chooses first next and last previous");
		expect(navigate_change(diff,
							   first,
							   change_navigation_direction::previous,
							   change_navigation_policy::stop)
						   .Outcome
					   == change_navigation_outcome::at_start
				   && navigate_change(diff,
									  last,
									  change_navigation_direction::next,
									  change_navigation_policy::stop)
						   .Outcome
					   == change_navigation_outcome::at_end,
			   "stop navigation reports both endpoints without changing identity");
		const auto previousWrap = navigate_change(diff,
												  first,
												  change_navigation_direction::previous,
												  change_navigation_policy::wrap);
		const auto nextWrap     = navigate_change(diff,
												  last,
												  change_navigation_direction::next,
												  change_navigation_policy::wrap);
		expect(previousWrap.Outcome == change_navigation_outcome::wrapped && previousWrap.Group == last && nextWrap.Outcome == change_navigation_outcome::wrapped && nextWrap.Group == first,
			   "wrap navigation reports wrapping and the canonical target");
		expect(navigate_change(diff,
							   first,
							   change_navigation_direction::next,
							   change_navigation_policy::stop)
						   .Group
					   == last
				   && navigate_change(diff,
									  last,
									  change_navigation_direction::previous,
									  change_navigation_policy::stop)
						   .Group
					   == first,
			   "interior previous and next follow vector order");
		expect(navigate_change(diff,
							   opaque<change_group_id>(99U),
							   change_navigation_direction::next,
							   change_navigation_policy::stop)
					   .Outcome
				   == change_navigation_outcome::invalid_selection,
			   "unknown current group is invalid rather than ordinally guessed");
		expect(navigate_change(diff,
							   first,
							   static_cast<change_navigation_direction>(99U),
							   change_navigation_policy::stop)
						   .Outcome
					   == change_navigation_outcome::invalid_selection
				   && navigate_change(diff,
									  first,
									  change_navigation_direction::next,
									  static_cast<change_navigation_policy>(99U))
						   .Outcome
					   == change_navigation_outcome::invalid_selection,
			   "invalid navigation policy values fail closed");

		const auto unchanged = run("same\n", "same\n");
		const auto noChanges = project_changes(unchanged);
		expect(navigate_change(unchanged,
							   std::nullopt,
							   change_navigation_direction::next,
							   change_navigation_policy::wrap)
					   .Outcome
				   == change_navigation_outcome::no_changes,
			   "valid empty group vector has an explicit no-changes outcome");
		expect(navigate_change(unchanged,
							   opaque<change_group_id>(90U),
							   change_navigation_direction::next,
							   change_navigation_policy::stop)
					   .Outcome
				   == change_navigation_outcome::invalid_selection,
			   "current identity in an empty group vector is invalid selection");

		const auto selectedLine  = line(diff, diff_side::left, 1U);
		const auto mapping       = map_line(diff, selectedLine);
		const auto detail        = detail_for_mapping(diff, mapping);
		const auto lineSelection = select_line(diff, selectedLine);
		const auto selection     = select_group(diff, *mapping.Group);
		expect(mapping.Segment.has_value() && mapping.Group == first && detail.Segment == mapping.Segment && detail.Group == mapping.Group && detail.Pair == mapping.Pair && projection.Changes.front().Group.Id == *mapping.Group && std::ranges::find(projection.Changes.front().Segments, *mapping.Segment, &change_segment_projection::Id) != projection.Changes.front().Segments.end() && noneNext.Group == mapping.Group && lineSelection.has_value() && lineSelection->Segment == mapping.Segment && lineSelection->Group == mapping.Group && detail_for_selection(diff, *lineSelection) == detail && selection.has_value() && selection->Segment == mapping.Segment && selection->Group == mapping.Group && detail_for_selection(diff, *selection).Group == mapping.Group,
			   "alignment mapping detail navigation selection and change "
			   "projection agree");
	}

	auto test_selection_reconciliation() -> void
	{
		const auto diff           = run("old\nkeep\n", "new\nkeep\n");
		const auto replay         = run("old\nkeep\n", "new\nkeep\n");
		const auto lineSelection  = select_line(diff, line(diff, diff_side::right, 0U));
		const auto groupSelection = select_group(diff, diff.Alignment.Groups.front().Id);
		expect(
			lineSelection.has_value() && groupSelection.has_value() && reconcile_selection(replay, lineSelection).Outcome == selection_reconciliation_outcome::preserved && reconcile_selection(replay, groupSelection).Outcome == selection_reconciliation_outcome::preserved,
			"same-digest replay preserves valid stable line and group selection");

		const auto insertion     = run("same\n", "added\nsame\n");
		const auto insertedGroup = insertion.Alignment.Groups.front().Id;
		const auto preferred     = preferred_group_line(insertion, insertedGroup);
		const auto selectedGroup = select_group(insertion, insertedGroup);
		expect(preferred == line(insertion, diff_side::right, 0U) && selectedGroup->PreferredLine == *preferred,
			   "group preference uses first member right begin when left is empty");
		expect(preferred_group_line(diff, diff.Alignment.Groups.front().Id) == line(diff, diff_side::left, 0U),
			   "group preference uses first member left begin when populated");

		const auto changedDigest = run("old\nkeep\n", "new\nkeep\n", { }, true, true, 32U);
		expect(reconcile_selection(changedDigest, lineSelection).Outcome == selection_reconciliation_outcome::cleared && reconcile_selection(changedDigest, groupSelection).Outcome == selection_reconciliation_outcome::cleared,
			   "changed comparison digest clears without path or ordinal fallback");

		auto badLine               = *lineSelection;
		badLine.PreferredLine.Line = 99U;
		auto badGroup              = *groupSelection;
		badGroup.Group             = opaque<change_group_id>(88U);
		expect(reconcile_selection(replay, badLine).Outcome == selection_reconciliation_outcome::cleared && reconcile_selection(replay, badGroup).Outcome == selection_reconciliation_outcome::cleared,
			   "missing line or stable group identity clears selection");
		expect(detail_for_selection(replay, badGroup).Reason == semantic_lookup_reason::invalid_result,
			   "detail rejects a fabricated semantic selection");
	}

	auto test_unavailable_and_fail_closed() -> void
	{
		const auto text  = run("a\n", "b\n");
		auto       stale = line(text, diff_side::left, 0U);
		stale.Comparison = opaque<file_comparison_digest>(77U);
		expect(map_line(text, stale).Reason == semantic_lookup_reason::comparison_mismatch,
			   "mismatched comparison digest is typed as stale identity");
		expect(detail_for_line(text, stale).Reason == semantic_lookup_reason::comparison_mismatch,
			   "stale detail preserves the primary mapping reason");
		expect(map_line(text, line(text, diff_side::left, 1U)).Reason == semantic_lookup_reason::line_out_of_range,
			   "out-of-range line fails explicitly");
		auto invalidSide = line(text, diff_side::left, 0U);
		invalidSide.Side = static_cast<diff_side>(99U);
		expect(map_line(text, invalidSide).Reason == semantic_lookup_reason::invalid_side,
			   "invalid side fails explicitly");

		const auto binary        = run(std::string_view{ "a\0b", 3U }, "text");
		const auto binaryMapping = map_line(binary, line(binary, diff_side::left, 0U));
		expect(binaryMapping.Reason == semantic_lookup_reason::non_text && binaryMapping.Diagnostic == diff_reason_code::binary_file && project_changes(binary).Reason == semantic_lookup_reason::non_text,
			   "binary result exposes its typed nontext diagnostic");
		const auto missing        = run(std::nullopt, "text");
		const auto missingMapping = map_line(missing, line(missing, diff_side::right, 0U));
		expect(missingMapping.Reason == semantic_lookup_reason::non_text && missingMapping.Diagnostic == diff_reason_code::missing_left_blob,
			   "unavailable result exposes its typed diagnostic");
		expect(!select_line(binary, line(binary, diff_side::left, 0U)).has_value(),
			   "nontext content cannot create semantic selection");

		auto mutated = text;
		++mutated.Alignment.Segments.front().Left.End;
		const auto invalidLine       = line(mutated, diff_side::left, 0U);
		const auto invalidMapping    = map_line(mutated, invalidLine);
		const auto invalidProjection = project_changes(mutated);
		expect(invalidMapping.Reason == semantic_lookup_reason::invalid_result && invalidProjection.Reason == semantic_lookup_reason::invalid_result && detail_for_mapping(mutated, invalidMapping).Kind == line_detail_kind::unavailable && !move_for_line(mutated, invalidLine).has_value() && !select_line(mutated, invalidLine).has_value() && navigate_change(mutated, std::nullopt, change_navigation_direction::next, change_navigation_policy::stop).Outcome == change_navigation_outcome::invalid_selection,
			   "mutated invalid result fails every semantic projection closed");
		auto fabricated             = map_line(text, line(text, diff_side::left, 0U));
		fabricated.Segment          = opaque<alignment_segment_id>(98U);
		fabricated.Diagnostic       = diff_reason_code::binary_file;
		const auto fabricatedDetail = detail_for_mapping(text, fabricated);
		expect(fabricatedDetail.Reason == semantic_lookup_reason::invalid_result && !fabricatedDetail.Segment.has_value() && !fabricatedDetail.Group.has_value() && !fabricatedDetail.Pair.has_value() && fabricatedDetail.Diagnostic == text.Alignment.Diagnostic.Code,
			   "detail rejects fabricated mapping identities and diagnostics");
		const auto validSelection = select_line(text, line(text, diff_side::left, 0U));
		expect(reconcile_selection(mutated, validSelection).Outcome == selection_reconciliation_outcome::cleared,
			   "invalid replacement clears prior selection");
	}

	auto test_move_relation_stays_secondary() -> void
	{
		auto options    = diff_options{ };
		options.Moves   = move_detection_policy::deterministic_exact;
		const auto diff = run("move one\nmove two\nstay one\nstay two\nstay three\n",
							  "stay one\nstay two\nstay three\nmove one\nmove two\n",
							  options);
		expect(diff.Alignment.Moves.size() == 1U, "move fixture has one relation");
		for (auto offset = std::uint32_t{ 0U }; offset < 2U; ++offset) {
			const auto deletionLine     = line(diff, diff_side::left, offset);
			const auto insertionLine    = line(diff, diff_side::right, offset + 3U);
			const auto deletionPrimary  = map_line(diff, deletionLine);
			const auto insertionPrimary = map_line(diff, insertionLine);
			const auto deletionMove     = move_for_line(diff, deletionLine);
			const auto insertionMove    = move_for_line(diff, insertionLine);
			expect(
				deletionPrimary.Kind == line_mapping_kind::opposing_boundary && insertionPrimary.Kind == line_mapping_kind::opposing_boundary && deletionMove.has_value() && insertionMove.has_value() && deletionMove->Counterpart == insertionLine && insertionMove->Counterpart == deletionLine && deletionMove->Relation == insertionMove->Relation && detail_for_line(diff, deletionLine).Move == deletionMove->Relation,
				"move is reversible metadata and never replaces primary boundary");
		}
		const auto projection = project_changes(diff);
		expect(projection.Changes.size() == 2U && projection.Changes[0].Moves.size() == 1U && projection.Changes[1].Moves.size() == 1U && projection.Changes[0].Moves.front() == projection.Changes[1].Moves.front(),
			   "both canonical change projections retain the same move marker");
	}

	auto verify_generated_mapping(const computed_file_diff& aDiff) -> void
	{
		const auto projection = project_changes(aDiff);
		expect(projection.available(), "generated result projects");
		const auto checkSide =
			[&aDiff](const diff_side aSide, const std::uint32_t aCount) {
				for (auto number = std::uint32_t{ 0U }; number < aCount; ++number) {
					const auto source  = line(aDiff, aSide, number);
					const auto mapping = map_line(aDiff, source);
					expect(mapping.Kind != line_mapping_kind::unavailable && mapping.Segment.has_value(),
						   "generated source line always has one primary mapping");
					const auto& owner = segment(aDiff, *mapping.Segment);
					if (mapping.Kind == line_mapping_kind::exact_line || mapping.Kind == line_mapping_kind::similar_line) {
						expect(mapping.Counterpart.has_value() && valid(*mapping.Counterpart, aDiff.Alignment.LeftLineCount, aDiff.Alignment.RightLineCount),
							   "generated paired counterpart stays in bounds");
						const auto reverse = map_line(aDiff, *mapping.Counterpart);
						expect(reverse.Counterpart == source && reverse.Segment == mapping.Segment && reverse.Pair == mapping.Pair,
							   "generated exact or similar pair is reversible");
					} else if (mapping.Kind == line_mapping_kind::opposing_boundary) {
						expect(mapping.OpposingBoundary.has_value() && valid(*mapping.OpposingBoundary, aDiff.Alignment.LeftLineCount, aDiff.Alignment.RightLineCount) && (mapping.OpposingBoundary->Side == diff_side::left ? owner.Left.empty() : owner.Right.empty()),
							   "generated one-sided line maps to its exact empty span");
					} else {
						const auto expected = aSide == diff_side::left ? owner.Right : owner.Left;
						expect(mapping.OpposingSpan.has_value() && mapping.OpposingSpan->Lines == expected && valid(expected, aSide == diff_side::left ? aDiff.Alignment.RightLineCount : aDiff.Alignment.LeftLineCount),
							   "generated unpaired replacement exposes full span");
					}
					if (move_for_line(aDiff, source).has_value()) {
						expect(mapping.Kind == line_mapping_kind::opposing_boundary,
							   "generated move never changes the primary mapping");
					}
				}
			};
		checkSide(diff_side::left, aDiff.Alignment.LeftLineCount);
		checkSide(diff_side::right, aDiff.Alignment.RightLineCount);

		auto visited    = std::vector<change_group_id>{ };
		auto navigation = navigate_change(aDiff,
										  std::nullopt,
										  change_navigation_direction::next,
										  change_navigation_policy::stop);
		if (projection.Changes.empty()) {
			expect(navigation.Outcome == change_navigation_outcome::no_changes,
				   "generated empty traversal reports no changes");
			return;
		}
		while (navigation.Outcome == change_navigation_outcome::selected) {
			visited.push_back(*navigation.Group);
			navigation = navigate_change(aDiff,
										 navigation.Group,
										 change_navigation_direction::next,
										 change_navigation_policy::stop);
		}
		expect(navigation.Outcome == change_navigation_outcome::at_end && visited.size() == projection.Changes.size(),
			   "generated navigation visits every group exactly once");
		for (std::size_t index = 0U; index < visited.size(); ++index) {
			expect(visited[index] == projection.Changes[index].Group.Id,
				   "generated traversal preserves exact group vector order");
		}
	}

	auto test_generated_properties() -> void
	{
		const auto texts = std::array<std::string_view, 7U>{
			"", "a\n", "b\n", "a\nb\n", "b\na\n", "a one\nb\n", "a two\nc\n"
		};
		for (const auto left : texts) {
			for (const auto right : texts) {
				verify_generated_mapping(run(left, right));
			}
		}
	}
} // namespace

int main()
{
	test_exact_and_one_sided_mapping();
	test_pair_span_and_unequal_tail_mapping();
	test_projection_navigation_and_identity();
	test_selection_reconciliation();
	test_unavailable_and_fail_closed();
	test_move_relation_stays_secondary();
	test_generated_properties();
	std::cout << "BPR-0074 diff semantic projection tests passed\n";
	return EXIT_SUCCESS;
}
