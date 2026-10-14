#include "bpr/core/diff_compute.hpp"
#include "bpr/core/workbench_interaction.hpp"
#include "product_diff.hpp"

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
#include <variant>
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

	auto expect(const bool             aCondition,
				const std::string_view aLabel,
				const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aLabel << ": " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] constexpr auto file(const std::uint8_t aSeed) -> workbench_file_id
	{
		std::array<std::uint8_t, 32U> bytes{ };
		bytes[0]  = aSeed;
		bytes[31] = static_cast<std::uint8_t>(aSeed ^ 0x5AU);
		return workbench_file_id{ bytes };
	}

	/// \brief Builds a cursor whose selection sits inside the actionable order.
	[[nodiscard]] auto resident(const std::uint32_t aOrdinal,
								const std::uint32_t aCount,
								const std::uint8_t  aSeed = 7U)
		-> workbench_navigation_cursor
	{
		return {
			.File              = file(aSeed),
			.Ordinal           = aOrdinal,
			.ActionableCount   = aCount,
			.PreviousAvailable = aOrdinal > 1U,
			.NextAvailable     = aOrdinal < aCount,
		};
	}

	/// \brief Builds a cursor whose selection left the actionable order.
	[[nodiscard]] auto nonresident(const std::uint32_t aCount,
								   const bool          aPrevious,
								   const bool          aNext,
								   const std::uint8_t  aSeed = 9U)
		-> workbench_navigation_cursor
	{
		return {
			.File              = file(aSeed),
			.ActionableCount   = aCount,
			.PreviousAvailable = aPrevious,
			.NextAvailable     = aNext,
		};
	}

	/// \brief Builds a cursor with no selected file at all.
	[[nodiscard]] auto unselected(const std::uint32_t aCount)
		-> workbench_navigation_cursor
	{
		return {
			.ActionableCount   = aCount,
			.PreviousAvailable = aCount != 0U,
			.NextAvailable     = aCount != 0U,
		};
	}

	[[nodiscard]] auto active(
		const workbench_navigation_cursor& aCursor,
		const change_navigation_policy     aPolicy = change_navigation_policy::stop)
		-> workbench_interaction_state
	{
		return {
			.Navigation       = aCursor,
			.NavigationPolicy = aPolicy,
		};
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		std::array<std::uint8_t, 32U> bytes{ };
		bytes[0]  = aSeed;
		bytes[31] = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return Identity{ bytes };
	}

	[[nodiscard]] constexpr auto blob(const std::uint8_t aSeed) -> git_blob_oid
	{
		std::array<std::uint8_t, 32U> bytes{ };
		for (std::size_t index = 0U; index < 20U; ++index) {
			bytes[index]
				= static_cast<std::uint8_t>(aSeed + static_cast<std::uint8_t>(index));
		}
		return git_blob_oid{ git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto comparison_key(const diff_options& aOptions)
		-> file_comparison_key
	{
		const auto digest = options_digest(aOptions);
		expect(digest.has_value(), "the fixture options are normalized");
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
			.LeftBlob  = blob(21U),
			.RightBlob = blob(51U),
			.File      = opaque<file_identity>(31U),
			.Options   = *digest,
			.Algorithm = supported_diff_algorithm_revision,
		};
	}

	/// \brief Computes one immutable comparison with a canonical change order.
	///
	/// \details The workbench core owns no diff, so this fixture exists only to
	/// obtain RFC-BPR-0004's own answers: the change commands are checked against
	/// what `navigate_change` decides over the exact same order.
	[[nodiscard]] auto change_fixture() -> computed_file_diff
	{
		const auto options    = diff_options{ };
		const auto left       = std::string{ "alpha\nbravo\ncharlie\ndelta\necho\n" };
		const auto right      = std::string{ "alpha\nBRAVO\ncharlie\nDELTA\necho\nfox\n" };
		auto       leftBytes  = std::vector<std::uint8_t>{ left.begin(), left.end() };
		auto       rightBytes = std::vector<std::uint8_t>{ right.begin(), right.end() };
		const auto outcome    = bpr::tests::product_file_diff({
			.Key        = comparison_key(options),
			.Options    = options,
			.LeftBytes  = std::span<const std::uint8_t>{ leftBytes },
			.RightBytes = std::span<const std::uint8_t>{ rightBytes },
		});
		expect(outcome.Reason == diff_reason_code::none && outcome.Result.has_value(),
			   "the change fixture computes one immutable comparison");
		return *outcome.Result;
	}

	/// \brief Assembles the bounded change-cursor facts the host would report.
	///
	/// \details This is the change-side twin of the file cursor the application
	/// host already assembles: it resolves the retained selection against the
	/// canonical change order and reports position and neighbour facts, never the
	/// order itself.
	[[nodiscard]] auto change_cursor(
		const computed_file_diff&             aDiff,
		const std::optional<change_group_id>& aSelected) -> workbench_change_cursor
	{
		const auto& groups = aDiff.Alignment.Groups;
		auto        cursor = workbench_change_cursor{
			.Group       = aSelected,
			.ChangeCount = static_cast<std::uint32_t>(groups.size()),
		};
		const auto found = aSelected.has_value()
			? std::ranges::find(groups, *aSelected, &change_group::Id)
			: groups.end();
		if (found == groups.end()) {
			// No selection enters the order from either end; a selection the
			// order cannot place reaches nothing at all.
			cursor.PreviousAvailable = !aSelected.has_value() && !groups.empty();
			cursor.NextAvailable     = cursor.PreviousAvailable;
			return cursor;
		}
		cursor.Ordinal = static_cast<std::uint32_t>(
			std::ranges::distance(groups.begin(), found) + 1);
		cursor.PreviousAvailable = cursor.Ordinal > 1U;
		cursor.NextAvailable     = cursor.Ordinal < cursor.ChangeCount;
		return cursor;
	}

	/// \brief Builds an active workbench with one open comparison.
	[[nodiscard]] auto comparing(
		const workbench_change_cursor& aCursor,
		const change_navigation_policy aPolicy = change_navigation_policy::stop)
		-> workbench_interaction_state
	{
		expect(valid(aCursor), "the fixture change cursor is well formed");
		auto state   = active(resident(2U, 3U), aPolicy);
		state.Change = aCursor;
		return state;
	}

	[[nodiscard]] auto navigate(const workbench_interaction_state& aState,
								const change_navigation_direction  aDirection)
		-> workbench_interaction_output
	{
		auto probe = aState;
		auto output
			= transition(probe, navigate_workbench_file_intent{ .Direction = aDirection });
		expect(probe == aState,
			   "a navigation decision leaves the retained cursor to the host");
		return output;
	}

	[[nodiscard]] auto decided(const workbench_interaction_output& aOutput,
							   const workbench_navigation_target   aTarget,
							   const std::optional<std::uint32_t>  aOrdinal) -> bool
	{
		return aOutput.Reason == workbench_reason_code::none
			&& aOutput.Decision.has_value()
			&& aOutput.Decision->Target == aTarget
			&& aOutput.Decision->Ordinal == aOrdinal
			&& !aOutput.Decision->File.has_value();
	}

	[[nodiscard]] auto rejected(const workbench_interaction_output& aOutput,
								const workbench_reason_code         aReason) -> bool
	{
		return aOutput.Reason == aReason && !aOutput.Decision.has_value()
			&& !aOutput.Selection.has_value() && !aOutput.Drawers.has_value()
			&& !aOutput.Commands.has_value();
	}

	auto test_cursor_contract() -> void
	{
		expect(valid(resident(1U, 1U)) && valid(resident(1U, 3U))
				   && valid(resident(2U, 3U)) && valid(resident(3U, 3U)),
			   "a resident selection agreeing with its ordinal is well formed");
		expect(valid(unselected(0U)) && valid(unselected(4U)),
			   "an unselected cursor is well formed in an empty or filled order");
		expect(valid(nonresident(4U, false, false))
				   && valid(nonresident(4U, true, false))
				   && valid(nonresident(4U, false, true))
				   && valid(nonresident(4U, true, true)),
			   "a nonresident selection may report either neighbour fact");
		expect(valid(workbench_navigation_cursor{ .File = file(3U) }),
			   "a selected file with an empty actionable order is well formed");

		auto beyond            = resident(2U, 3U);
		beyond.ActionableCount = 1U;
		expect(!valid(beyond), "an ordinal past the actionable count is rejected");

		auto unnamed = resident(2U, 3U);
		unnamed.File.reset();
		expect(!valid(unnamed), "an ordinal without a selected file is rejected");

		auto zeroed = resident(2U, 3U);
		zeroed.File = workbench_file_id{ };
		expect(!valid(zeroed), "a zero file identity is rejected");

		auto wrongPrevious              = resident(1U, 3U);
		wrongPrevious.PreviousAvailable = true;
		expect(!valid(wrongPrevious),
			   "the first resident entry cannot claim a previous entry");

		auto wrongNext          = resident(3U, 3U);
		wrongNext.NextAvailable = true;
		expect(!valid(wrongNext),
			   "the last resident entry cannot claim a next entry");

		auto missingNext          = resident(1U, 3U);
		missingNext.NextAvailable = false;
		expect(!valid(missingNext),
			   "a resident entry cannot deny a neighbour its ordinal implies");

		auto strandedEntry              = unselected(4U);
		strandedEntry.PreviousAvailable = false;
		expect(!valid(strandedEntry),
			   "an unselected cursor cannot deny entry into a filled order");

		auto emptyWithNeighbour          = unselected(0U);
		emptyWithNeighbour.NextAvailable = true;
		expect(!valid(emptyWithNeighbour),
			   "an empty actionable order reaches nothing in either direction");
	}

	auto test_change_cursor_contract() -> void
	{
		const auto group = opaque<change_group_id>(5U);
		expect(valid(workbench_change_cursor{ .Group             = group,
											  .Ordinal           = 2U,
											  .ChangeCount       = 3U,
											  .PreviousAvailable = true,
											  .NextAvailable     = true }),
			   "a placed change selection agreeing with its ordinal is well formed");
		expect(valid(workbench_change_cursor{ .ChangeCount       = 3U,
											  .PreviousAvailable = true,
											  .NextAvailable     = true }),
			   "an unselected change cursor enters a nonempty order either way");
		expect(valid(workbench_change_cursor{ }),
			   "a comparison with no changes is well formed");
		expect(valid(workbench_change_cursor{ .Group = group, .ChangeCount = 3U }),
			   "a selection the change order cannot place is well formed");

		expect(!valid(workbench_change_cursor{ .Group       = group,
											   .Ordinal     = 4U,
											   .ChangeCount = 3U }),
			   "an ordinal past the change count is rejected");
		expect(!valid(workbench_change_cursor{ .Ordinal = 1U, .ChangeCount = 3U }),
			   "an ordinal without a selected change is rejected");
		expect(!valid(workbench_change_cursor{ .Group         = change_group_id{ },
											   .Ordinal       = 1U,
											   .ChangeCount   = 3U,
											   .NextAvailable = true }),
			   "a zero change-group identity is rejected");
		expect(!valid(workbench_change_cursor{ .Group             = group,
											   .Ordinal           = 1U,
											   .ChangeCount       = 3U,
											   .PreviousAvailable = true,
											   .NextAvailable     = true }),
			   "the first placed change cannot claim a previous change");
		expect(!valid(workbench_change_cursor{ .Group       = group,
											   .Ordinal     = 1U,
											   .ChangeCount = 3U }),
			   "a placed change cannot deny a neighbour its ordinal implies");
		expect(!valid(workbench_change_cursor{ .ChangeCount = 3U }),
			   "an unselected change cursor cannot deny entry into a filled order");
		expect(!valid(workbench_change_cursor{ .NextAvailable = true }),
			   "a comparison with no changes reaches nothing in either direction");
		// The one deliberate difference from the file cursor: the change order is
		// resolved rather than walked, so an unplaceable selection has no
		// neighbour the host could name on its behalf.
		expect(!valid(workbench_change_cursor{ .Group         = group,
											   .ChangeCount   = 3U,
											   .NextAvailable = true }),
			   "a selection the change order cannot place reaches nothing");
	}

	auto test_navigation_follows_actionable_order() -> void
	{
		const auto middle = active(resident(2U, 3U));
		expect(decided(navigate(middle, change_navigation_direction::next),
					   workbench_navigation_target::next_actionable,
					   3U),
			   "next steps to the following ordinal of the actionable order");
		expect(decided(navigate(middle, change_navigation_direction::previous),
					   workbench_navigation_target::previous_actionable,
					   1U),
			   "previous steps to the preceding ordinal of the actionable order");

		const auto last = active(resident(3U, 3U));
		expect(rejected(navigate(last, change_navigation_direction::next),
						workbench_reason_code::at_last_file),
			   "the stop policy refuses to step past the last actionable entry");
		expect(decided(navigate(last, change_navigation_direction::previous),
					   workbench_navigation_target::previous_actionable,
					   2U),
			   "the last actionable entry still steps backward");

		const auto first = active(resident(1U, 3U));
		expect(rejected(navigate(first, change_navigation_direction::previous),
						workbench_reason_code::at_first_file),
			   "the stop policy refuses to step before the first actionable entry");

		const auto wrappedLast
			= active(resident(3U, 3U), change_navigation_policy::wrap);
		expect(decided(navigate(wrappedLast, change_navigation_direction::next),
					   workbench_navigation_target::first_actionable,
					   1U),
			   "the wrap policy returns to the first actionable entry");
		const auto wrappedFirst
			= active(resident(1U, 3U), change_navigation_policy::wrap);
		expect(decided(navigate(wrappedFirst, change_navigation_direction::previous),
					   workbench_navigation_target::last_actionable,
					   3U),
			   "the wrap policy returns to the last actionable entry");

		const auto only = active(resident(1U, 1U));
		expect(rejected(navigate(only, change_navigation_direction::next),
						workbench_reason_code::at_last_file)
				   && rejected(navigate(only, change_navigation_direction::previous),
							   workbench_reason_code::at_first_file),
			   "a single actionable entry stops in both directions");
		const auto wrappedOnly
			= active(resident(1U, 1U), change_navigation_policy::wrap);
		expect(decided(navigate(wrappedOnly, change_navigation_direction::next),
					   workbench_navigation_target::first_actionable,
					   1U)
				   && decided(
					   navigate(wrappedOnly, change_navigation_direction::previous),
					   workbench_navigation_target::last_actionable,
					   1U),
			   "wrapping a single actionable entry reselects it");

		const auto stranded = active(nonresident(4U, true, true));
		expect(decided(navigate(stranded, change_navigation_direction::next),
					   workbench_navigation_target::next_actionable,
					   std::nullopt)
				   && decided(navigate(stranded, change_navigation_direction::previous),
							  workbench_navigation_target::previous_actionable,
							  std::nullopt),
			   "only the host can name the neighbour of a nonresident selection");

		const auto strandedAtEnd = active(nonresident(4U, true, false));
		expect(rejected(navigate(strandedAtEnd, change_navigation_direction::next),
						workbench_reason_code::at_last_file),
			   "a nonresident selection with no following entry stops");
		const auto wrappedFromEnd
			= active(nonresident(4U, true, false), change_navigation_policy::wrap);
		expect(decided(navigate(wrappedFromEnd, change_navigation_direction::next),
					   workbench_navigation_target::first_actionable,
					   1U),
			   "a nonresident selection with no following entry wraps");

		const auto empty = active(unselected(3U));
		expect(decided(navigate(empty, change_navigation_direction::next),
					   workbench_navigation_target::first_actionable,
					   1U)
				   && decided(navigate(empty, change_navigation_direction::previous),
							  workbench_navigation_target::last_actionable,
							  3U),
			   "navigation with no selection enters the actionable order at an end");

		const auto nothing = active(unselected(0U));
		expect(rejected(navigate(nothing, change_navigation_direction::next),
						workbench_reason_code::no_actionable_files)
				   && rejected(navigate(nothing, change_navigation_direction::previous),
							   workbench_reason_code::no_actionable_files),
			   "an empty actionable order refuses navigation");

		const auto inactive = workbench_interaction_state{ };
		expect(rejected(navigate(inactive, change_navigation_direction::next),
						workbench_reason_code::no_active_workbench)
				   && rejected(navigate(inactive, change_navigation_direction::previous),
							   workbench_reason_code::no_active_workbench),
			   "an inactive workbench refuses navigation");
	}

	[[nodiscard]] constexpr auto change_entry(
		const change_navigation_direction aDirection) -> std::size_t
	{
		return static_cast<std::size_t>(
			aDirection == change_navigation_direction::previous
				? workbench_command_id::previous_change
				: workbench_command_id::next_change);
	}

	/// \brief [[BPR-1328]]. A navigation command a press moves.
	[[nodiscard]] constexpr auto moves(const workbench_command_projection& aCommand) -> bool
	{
		return aCommand.Enabled && aCommand.DisabledReason == workbench_reason_code::none
			&& aCommand.Bound == workbench_reason_code::none;
	}

	/// \brief [[BPR-1328]]. A navigation command that stays pressable and does
	/// nothing for `aBound`.
	[[nodiscard]] constexpr auto bounded(
		const workbench_command_projection& aCommand,
		const workbench_reason_code         aBound) -> bool
	{
		return aCommand.Enabled && aCommand.DisabledReason == workbench_reason_code::none
			&& aCommand.Bound == aBound;
	}

	auto test_the_ends_of_the_change_order_are_named_not_derived() -> void
	{
		const auto  diff   = change_fixture();
		const auto& groups = diff.Alignment.Groups;
		expect(groups.size() >= 2U, "the change fixture carries several changes");

		const auto atStart = navigate_change(diff,
											 groups.front().Id,
											 change_navigation_direction::previous,
											 change_navigation_policy::stop);
		const auto atEnd   = navigate_change(diff,
											 groups.back().Id,
											 change_navigation_direction::next,
											 change_navigation_policy::stop);
		expect(atStart.Outcome == change_navigation_outcome::at_start
				   && atStart.Group == groups.front().Id
				   && atEnd.Outcome == change_navigation_outcome::at_end
				   && atEnd.Group == groups.back().Id,
			   "both ends of the change order still report the current group, so "
			   "a group-carrying result cannot mean a step was taken");

		const auto first    = comparing(change_cursor(diff, groups.front().Id));
		const auto atFirst  = project_workbench_commands(first);
		const auto previous = change_entry(change_navigation_direction::previous);
		const auto next     = change_entry(change_navigation_direction::next);
		// [[BPR-1328]]. An end of the change order keeps the command
		// pressable and names the bound a press meets.
		expect(bounded(atFirst.Commands[previous], workbench_reason_code::at_first_change)
				   && first.Change->Group == atStart.Group,
			   "the first change bounds previous change, still enabled, while the "
			   "retained cursor still reports that exact current group");
		expect(moves(atFirst.Commands[next]),
			   "the first change still offers the next change");

		const auto last   = comparing(change_cursor(diff, groups.back().Id));
		const auto atLast = project_workbench_commands(last);
		expect(bounded(atLast.Commands[next], workbench_reason_code::at_last_change)
				   && last.Change->Group == atEnd.Group,
			   "the last change bounds next change, still enabled, while the "
			   "retained cursor still reports that exact current group");
		expect(moves(atLast.Commands[previous]),
			   "the last change still offers the previous change");
	}

	auto test_change_commands_agree_with_navigate_change() -> void
	{
		const auto  diff   = change_fixture();
		const auto& groups = diff.Alignment.Groups;

		auto selections = std::vector<std::optional<change_group_id>>{ std::nullopt };
		for (const auto& group : groups) {
			selections.emplace_back(group.Id);
		}
		// A retained selection the replaced comparison no longer contains.
		selections.emplace_back(opaque<change_group_id>(200U));

		for (const auto policy :
			 { change_navigation_policy::stop, change_navigation_policy::wrap }) {
			for (const auto& selected : selections) {
				const auto state    = comparing(change_cursor(diff, selected), policy);
				const auto commands = project_workbench_commands(state);
				for (const auto direction : { change_navigation_direction::previous,
											  change_navigation_direction::next }) {
					const auto result
						= navigate_change(diff, selected, direction, policy);
					const auto reached
						= result.Outcome == change_navigation_outcome::selected
						|| result.Outcome == change_navigation_outcome::wrapped;
					const auto& command = commands.Commands[change_entry(direction)];
					// [[BPR-1328]]. Always enabled; a press moves exactly
					// when the semantic index would reach another change,
					// and otherwise names its bound.
					expect(command.Enabled && moves(command) == reached,
						   "a change command is enabled, and moves exactly when "
						   "the semantic index would reach another change");
					expect(reached == (command.Bound == workbench_reason_code::none),
						   "a change command a press cannot move names its bound");
				}
				expect(commands.Policy == policy,
					   "the delta carries the policy the change entries were "
					   "decided under");
			}
		}

		const auto stale         = comparing(change_cursor(diff, opaque<change_group_id>(200U)));
		const auto staleCommands = project_workbench_commands(stale);
		expect(bounded(staleCommands.Commands[change_entry(change_navigation_direction::previous)],
					   workbench_reason_code::invalid_change_selection),
			   "a selection the change order cannot place explains itself exactly");

		auto closed      = comparing(change_cursor(diff, std::nullopt));
		closed.Change    = workbench_change_cursor{ };
		const auto empty = project_workbench_commands(closed);
		expect(bounded(empty.Commands[change_entry(change_navigation_direction::next)],
					   workbench_reason_code::no_changes),
			   "a comparison with no changes explains itself exactly");
	}

	/// \brief [[BPR-1328]]. The owner, 2026-10-08, first: "It doesn't feel
	/// good for next change arrows to get greyed out when there's only 1
	/// change". Then, rejecting arrows that crossed into the next file: "Just
	/// let it be a bounded prev and next that you can always use."
	auto test_navigation_commands_are_bounded_and_always_enabled() -> void
	{
		const auto previous = change_entry(change_navigation_direction::previous);
		const auto next     = change_entry(change_navigation_direction::next);
		const auto lonely   = workbench_change_cursor{
			.Group       = opaque<change_group_id>(1U),
			.Ordinal     = 1U,
			.ChangeCount = 1U,
		};

		// The owner's case, in every file position: one change is both ends of
		// its comparison, and neither arrow greys out or leaves the file.
		for (const auto& position : { resident(1U, 3U), resident(2U, 3U), resident(3U, 3U), resident(1U, 1U) }) {
			auto state      = active(position);
			state.Change    = lonely;
			const auto deck = project_workbench_commands(state);
			expect(bounded(deck.Commands[previous], workbench_reason_code::at_first_change)
					   && bounded(deck.Commands[next], workbench_reason_code::at_last_change),
				   "a one-change comparison keeps both change commands enabled, "
				   "bounded by the ends of this file, wherever the file sits");
		}

		// The file commands follow the same rule at the ends of the order.
		const auto first = project_workbench_commands(active(resident(1U, 3U)));
		const auto last  = project_workbench_commands(active(resident(3U, 3U)));
		expect(bounded(first.Commands[0], workbench_reason_code::at_first_file) && moves(first.Commands[1])
				   && moves(last.Commands[0]) && bounded(last.Commands[1], workbench_reason_code::at_last_file),
			   "the first and last file keep both file commands enabled, bounded "
			   "at the ends of the order");
		expect(rejected(navigate(active(resident(3U, 3U)), change_navigation_direction::next),
						workbench_reason_code::at_last_file)
				   && rejected(navigate(active(resident(1U, 3U)), change_navigation_direction::previous),
							   workbench_reason_code::at_first_file),
			   "a press at a file bound decides nothing: no wrap and no move");

		// Only an inactive workbench, where nothing can be navigated, disables
		// them, and a disabled command carries no bound.
		const auto inactive = project_workbench_commands(workbench_interaction_state{ });
		for (std::size_t index = 0U; index < 4U; ++index) {
			expect(!inactive.Commands[index].Enabled
					   && inactive.Commands[index].DisabledReason == workbench_reason_code::no_active_workbench
					   && inactive.Commands[index].Bound == workbench_reason_code::none,
				   "an inactive workbench disables the four navigation commands");
		}

		// The wrap policy still wraps, so it has no bound.
		auto wrapping      = active(resident(2U, 3U), change_navigation_policy::wrap);
		wrapping.Change    = lonely;
		const auto wrapped = project_workbench_commands(wrapping);
		expect(moves(wrapped.Commands[previous]) && moves(wrapped.Commands[next]),
			   "the wrap policy has no bound inside the comparison");
	}

	[[nodiscard]] auto intent_for(const workbench_command_id aCommand)
		-> std::optional<workbench_interaction_intent>
	{
		switch (aCommand) {
		case workbench_command_id::previous_file:
		case workbench_command_id::next_file:
			return navigate_workbench_file_intent{
				.Direction = aCommand == workbench_command_id::previous_file
					? change_navigation_direction::previous
					: change_navigation_direction::next,
			};
		case workbench_command_id::toggle_files_drawer:
			return toggle_workbench_drawer_intent{ .Drawer = workbench_drawer::files };
		case workbench_command_id::toggle_conversations_drawer:
			return toggle_workbench_drawer_intent{ .Drawer = workbench_drawer::conversations };
		case workbench_command_id::toggle_repositories_drawer:
			return toggle_workbench_drawer_intent{ .Drawer = workbench_drawer::repositories };
		case workbench_command_id::toggle_one_pane:
			return toggle_workbench_one_pane_intent{ };
		case workbench_command_id::toggle_updates_line:
			return toggle_workbench_updates_line_intent{ };
		case workbench_command_id::toggle_drawers:
			return toggle_workbench_drawers_intent{ };
		case workbench_command_id::toggle_line_numbers:
			return toggle_workbench_line_numbers_intent{ };
		case workbench_command_id::go_to_header_source:
		case workbench_command_id::previous_change:
		case workbench_command_id::next_change:
			// A change gesture leaves as an RFC-BPR-0004 diff intent rather than
			// a workbench intent, so the workbench core projects its legality
			// without owning its transition.
			break;
		}
		return std::nullopt;
	}

	/// \brief Describes one named workbench state used by the command table.
	struct labelled_state
	{
		std::string_view            Label;
		workbench_interaction_state State;
	};

	auto check_command_agreement(const labelled_state& aCase) -> void
	{
		const auto commands = project_workbench_commands(aCase.State);
		for (std::size_t index = 0U; index < workbench_command_count; ++index) {
			const auto command    = commands.Commands[index];
			const auto identifier = static_cast<workbench_command_id>(index);
			expect(command.Id == identifier,
				   aCase.Label,
				   "the command delta projects one entry per command in order");

			const auto intent = intent_for(identifier);
			if (!intent.has_value()) {
				continue;
			}
			auto       probe  = aCase.State;
			const auto output = transition(probe, *intent);
			// [[BPR-1328]]. A navigation command stays enabled at its
			// bound, where the gesture is refused and a press does nothing.
			const auto accepted = command.Enabled && command.Bound == workbench_reason_code::none;
			expect(accepted == (output.Reason == workbench_reason_code::none),
				   aCase.Label,
				   "an enabled, unbounded command is exactly an accepted gesture");
			expect((command.Enabled ? command.Bound : command.DisabledReason) == output.Reason,
				   aCase.Label,
				   "a refused gesture is explained by the command's bound or its disabled reason");
			expect(probe == aCase.State || accepted,
				   aCase.Label,
				   "a rejected gesture changes no retained fact");
		}
		expect(commands.Policy == aCase.State.NavigationPolicy,
			   aCase.Label,
			   "the delta carries the retained navigation policy unchanged");
	}

	auto test_command_projection() -> void
	{
		const auto diff          = change_fixture();
		auto       overviewState = active(resident(2U, 3U));
		overviewState.Modality   = workbench_modality::overview;
		auto discussState        = active(resident(2U, 3U));
		discussState.Modality    = workbench_modality::discuss;
		const auto cases         = std::array<labelled_state, 12U>{ {
			{ "inactive", workbench_interaction_state{ } },
			{ "empty order", active(unselected(0U)) },
			{ "no selection", active(unselected(3U)) },
			{ "first of three", active(resident(1U, 3U)) },
			{ "middle of three", active(resident(2U, 3U)) },
			{ "last of three", active(resident(3U, 3U)) },
			{ "last of three wrapping",
			  active(resident(3U, 3U), change_navigation_policy::wrap) },
			{ "nonresident at end", active(nonresident(4U, true, false)) },
			{ "comparing at first change",
			  comparing(change_cursor(diff, diff.Alignment.Groups.front().Id)) },
			{ "comparing with no change selected",
			  comparing(change_cursor(diff, std::nullopt),
						change_navigation_policy::wrap) },
			{ "in Overview", overviewState },
			{ "in Discuss", discussState },
		} };
		for (const auto& state : cases) {
			check_command_agreement(state);
		}

		// [[BPR-0675]]. Every command except the three that give a drawer's
		// room back: those are about the window, not about a comparison, and
		// gating them left a reviewer unable to collapse a full drawer.
		// [[BPR-1130]]. The Updates disclosure is the fourth, for the same
		// reason: it gives back the timeline's row. [[BPR-1295]]: hiding every
		// drawer is the fifth; a new window's core is in Review.
		const auto     inactive      = project_workbench_commands(workbench_interaction_state{ });
		constexpr auto drawerCommand = [](const workbench_command_id aId) {
			return aId == workbench_command_id::toggle_files_drawer
				|| aId == workbench_command_id::toggle_conversations_drawer
				|| aId == workbench_command_id::toggle_repositories_drawer
				|| aId == workbench_command_id::toggle_updates_line
				|| aId == workbench_command_id::toggle_drawers;
		};
		expect(std::ranges::none_of(inactive.Commands,
									[&drawerCommand](const workbench_command_projection& aCommand) {
										return aCommand.Enabled && !drawerCommand(aCommand.Id);
									}),
			   "an inactive workbench disables every workbench command but its drawers");
		expect(std::ranges::all_of(inactive.Commands,
								   [&drawerCommand](const workbench_command_projection& aCommand) {
									   return !drawerCommand(aCommand.Id) || aCommand.Enabled;
								   }),
			   "and offers all five of those");
		expect(std::ranges::all_of(
				   inactive.Commands,
				   [&drawerCommand](const workbench_command_projection& aCommand) {
					   return drawerCommand(aCommand.Id)
						   ? aCommand.DisabledReason == workbench_reason_code::none
						   : aCommand.DisabledReason == (aCommand.Id == workbench_command_id::go_to_header_source ? workbench_reason_code::header_source_no_selection : workbench_reason_code::no_active_workbench);
				   }),
			   "an inactive workbench explains every disabled command the same way, and "
			   "names no reason for the ones it offers");

		expect(inactive
					   .Commands[static_cast<std::size_t>(
						   workbench_command_id::toggle_line_numbers)]
					   .Id
				   == workbench_command_id::toggle_line_numbers,
			   "the seventh entry projects its own command rather than a "
			   "default-constructed one");

		const auto emptyOrder = project_workbench_commands(active(unselected(0U)));
		expect(emptyOrder
				   .Commands[static_cast<std::size_t>(
					   workbench_command_id::toggle_line_numbers)]
				   .Enabled,
			   "an active workbench offers the line-number command even with no "
			   "actionable file, because numbering does not depend on one");
		// [[BPR-1328]]. The navigation commands stay pressable and name the
		// bound a press would meet.
		expect(bounded(emptyOrder.Commands[0], workbench_reason_code::no_actionable_files)
				   && bounded(emptyOrder.Commands[1], workbench_reason_code::no_actionable_files),
			   "an empty actionable order bounds both file commands, still enabled");
		expect(bounded(emptyOrder.Commands[2], workbench_reason_code::no_open_comparison)
				   && bounded(emptyOrder.Commands[3], workbench_reason_code::no_open_comparison),
			   "an active workbench with no open comparison bounds both change "
			   "commands, still enabled");
		expect(emptyOrder.Commands[4].Enabled && emptyOrder.Commands[5].Enabled
				   && emptyOrder.Commands[4].DisabledReason
					   == workbench_reason_code::none,
			   "an active workbench keeps both drawer commands available");

		const auto stopped = project_workbench_commands(active(resident(3U, 3U)));
		expect(moves(stopped.Commands[0])
				   && bounded(stopped.Commands[1], workbench_reason_code::at_last_file),
			   "the last actionable entry bounds next file under the stop policy");
		const auto wrapped = project_workbench_commands(
			active(resident(3U, 3U), change_navigation_policy::wrap));
		expect(moves(wrapped.Commands[0]) && moves(wrapped.Commands[1]),
			   "the wrap policy keeps both navigation commands available");
		const auto atFirst = project_workbench_commands(active(resident(1U, 3U)));
		expect(bounded(atFirst.Commands[0], workbench_reason_code::at_first_file),
			   "the first actionable entry bounds previous file under stop");
	}

	auto test_selection_gestures() -> void
	{
		auto       state   = active(resident(2U, 3U, 7U));
		const auto initial = state;

		const auto invalid = transition(
			state, select_workbench_file_intent{ .File = workbench_file_id{ } });
		expect(rejected(invalid, workbench_reason_code::invalid_file_identity)
				   && state == initial,
			   "an invalid file identity is refused");

		auto       inactive = workbench_interaction_state{ };
		const auto refused
			= transition(inactive, select_workbench_file_intent{ .File = file(7U) });
		expect(rejected(refused, workbench_reason_code::no_active_workbench)
				   && inactive == workbench_interaction_state{ },
			   "an inactive workbench refuses a file selection");

		const auto reselected
			= transition(state, select_workbench_file_intent{ .File = file(7U) });
		expect(reselected == workbench_interaction_output{ } && state == initial,
			   "reselecting the selected file decides and publishes nothing");

		const auto selected
			= transition(state, select_workbench_file_intent{ .File = file(8U) });
		expect(selected.Reason == workbench_reason_code::none
				   && selected.Decision.has_value()
				   && selected.Decision->Target
					   == workbench_navigation_target::exact_file
				   && selected.Decision->File == file(8U)
				   && !selected.Decision->Ordinal.has_value()
				   && !selected.Selection.has_value() && state == initial,
			   "selecting another file decides an exact target for the host");

		const auto resolved
			= observe_workbench_navigation(state, resident(3U, 3U, 8U));
		expect(resolved.Reason == workbench_reason_code::none
				   && resolved.Selection.has_value()
				   && resolved.Selection->Cursor == resident(3U, 3U, 8U)
				   && state.Navigation == resident(3U, 3U, 8U),
			   "the host resolution publishes the accepted selection");
		expect(resolved.Commands.has_value()
				   && bounded(resolved.Commands->Commands[1], workbench_reason_code::at_last_file),
			   "an accepted selection republishes changed command state");
	}

	auto test_navigation_observations() -> void
	{
		auto       state = workbench_interaction_state{ };
		const auto activated
			= observe_workbench_navigation(state, resident(2U, 3U, 7U));
		expect(activated.Selection.has_value()
				   && activated.Selection->Cursor == resident(2U, 3U, 7U)
				   && activated.Commands.has_value()
				   && activated.Commands->Commands[0].Enabled
				   && activated.Commands->Commands[4].Enabled,
			   "the first observation activates the workbench and its commands");

		const auto repeated
			= observe_workbench_navigation(state, resident(2U, 3U, 7U));
		expect(repeated == workbench_interaction_output{ },
			   "an unchanged observation publishes nothing");

		const auto sibling
			= observe_workbench_navigation(state, resident(2U, 3U, 11U));
		expect(sibling.Selection.has_value() && !sibling.Commands.has_value(),
			   "a selection change that keeps command legality publishes no commands");

		auto malformed            = resident(2U, 3U, 11U);
		malformed.NextAvailable   = false;
		const auto before         = state;
		const auto rejectedCursor = observe_workbench_navigation(state, malformed);
		expect(rejected(rejectedCursor,
						workbench_reason_code::invalid_navigation_cursor)
				   && state == before,
			   "a malformed cursor is rejected without changing retained facts");

		const auto cleared = observe_workbench_navigation(state, std::nullopt);
		// Index 4 is the files drawer, which [[BPR-0675]] keeps offered
		// through a deactivation; index 6 is the line numbers, which the
		// deactivation still disables because there are no panes to number.
		expect(cleared.Selection.has_value()
				   && !cleared.Selection->Cursor.has_value()
				   && cleared.Commands.has_value()
				   && !cleared.Commands->Commands[0].Enabled
				   && !cleared.Commands->Commands[6].Enabled
				   && cleared.Commands->Commands[4].Enabled
				   && !state.Navigation.has_value(),
			   "an absent observation deactivates the workbench and leaves its drawers");
	}

	auto test_change_observations() -> void
	{
		const auto  diff     = change_fixture();
		const auto& groups   = diff.Alignment.Groups;
		const auto  previous = change_entry(change_navigation_direction::previous);
		const auto  next     = change_entry(change_navigation_direction::next);

		auto       inactive = workbench_interaction_state{ };
		const auto refused
			= observe_workbench_change_navigation(inactive,
												  change_cursor(diff, std::nullopt));
		expect(refused == workbench_interaction_output{ }
				   && inactive.Change.has_value(),
			   "an inactive workbench retains the observed comparison and offers "
			   "no command it could not honour");
		expect(!project_workbench_commands(inactive).Commands[next].Enabled,
			   "no active workbench offers no change gesture");

		auto       state  = active(resident(2U, 3U));
		const auto opened = observe_workbench_change_navigation(
			state, change_cursor(diff, groups.front().Id));
		expect(opened.Reason == workbench_reason_code::none
				   && opened.Commands.has_value()
				   && moves(opened.Commands->Commands[next])
				   && bounded(opened.Commands->Commands[previous], workbench_reason_code::at_first_change)
				   && state.Change->Group == groups.front().Id,
			   "an opened comparison republishes exactly the change legality it "
			   "created");
		expect(!opened.Selection.has_value() && !opened.Decision.has_value()
				   && !opened.Drawers.has_value(),
			   "a change observation publishes command legality alone");

		const auto repeated = observe_workbench_change_navigation(
			state, change_cursor(diff, groups.front().Id));
		expect(repeated == workbench_interaction_output{ },
			   "an unchanged change observation publishes nothing");

		auto malformed              = change_cursor(diff, groups.front().Id);
		malformed.PreviousAvailable = true;
		const auto before           = state;
		const auto rejectedCursor
			= observe_workbench_change_navigation(state, malformed);
		expect(rejected(rejectedCursor, workbench_reason_code::invalid_change_cursor)
				   && state == before,
			   "a malformed change cursor is rejected without changing retained "
			   "facts");

		const auto closed
			= observe_workbench_change_navigation(state, std::nullopt);
		expect(closed.Commands.has_value()
				   && bounded(closed.Commands->Commands[next], workbench_reason_code::no_open_comparison)
				   && !state.Change.has_value(),
			   "an absent change observation closes the comparison");
	}

	auto test_line_number_state() -> void
	{
		auto inactive = workbench_interaction_state{ };
		expect(inactive.LineNumbers == workbench_line_numbers::shown,
			   "line numbers start shown");

		// It belongs to the workbench surface exactly as a drawer does. There
		// are no source panes to number until the host publishes one, so this
		// is not the one command an inactive workbench leaves enabled.
		const auto refused
			= transition(inactive, toggle_workbench_line_numbers_intent{ });
		expect(rejected(refused, workbench_reason_code::no_active_workbench)
				   && inactive == workbench_interaction_state{ },
			   "an inactive workbench refuses a line-number gesture");

		auto       state = active(resident(2U, 3U));
		const auto hidden
			= transition(state, toggle_workbench_line_numbers_intent{ });
		expect(hidden.Reason == workbench_reason_code::none
				   && hidden.LineNumbers.has_value()
				   && hidden.LineNumbers->State == workbench_line_numbers::hidden
				   && state.LineNumbers == workbench_line_numbers::hidden,
			   "toggling hides them");
		expect(!hidden.Drawers.has_value() && !hidden.Decision.has_value()
				   && !hidden.Selection.has_value(),
			   "a line-number gesture publishes no drawer or selection change");
		expect(state.FilesDrawer == workbench_drawer_state::expanded
				   && state.ConversationsDrawer == workbench_drawer_state::expanded
				   && state.Navigation.has_value(),
			   "a line-number gesture disturbs no other retained fact");

		const auto shown
			= transition(state, toggle_workbench_line_numbers_intent{ });
		expect(shown.LineNumbers.has_value()
				   && shown.LineNumbers->State == workbench_line_numbers::shown
				   && state.LineNumbers == workbench_line_numbers::shown,
			   "toggling again shows them");

		const auto unchanged = transition(
			state,
			set_workbench_line_numbers_intent{
				.State = workbench_line_numbers::shown,
			});
		expect(unchanged == workbench_interaction_output{ },
			   "setting the state it already has publishes nothing");

		const auto set = transition(
			state,
			set_workbench_line_numbers_intent{
				.State = workbench_line_numbers::hidden,
			});
		expect(set.LineNumbers.has_value()
				   && set.LineNumbers->State == workbench_line_numbers::hidden
				   && state.LineNumbers == workbench_line_numbers::hidden,
			   "an exact set carries a persisted setting back into the core");
	}

	auto test_one_pane_state() -> void
	{
		auto inactive = workbench_interaction_state{ };
		expect(inactive.OnePane == workbench_one_pane::disabled,
			   "one pane starts disabled");

		// It belongs to the workbench surface exactly as a drawer does. There
		// are no source panes to number until the host publishes one, so this
		// is not the one command an inactive workbench leaves enabled.
		const auto refused
			= transition(inactive, toggle_workbench_one_pane_intent{ });
		expect(rejected(refused, workbench_reason_code::no_active_workbench)
				   && inactive == workbench_interaction_state{ },
			   "an inactive workbench refuses a one-pane gesture");

		auto       state = active(resident(2U, 3U));
		const auto hidden
			= transition(state, toggle_workbench_one_pane_intent{ });
		expect(hidden.Reason == workbench_reason_code::none
				   && hidden.OnePane.has_value()
				   && hidden.OnePane->State == workbench_one_pane::enabled
				   && state.OnePane == workbench_one_pane::enabled,
			   "toggling enables one pane");
		expect(!hidden.Drawers.has_value() && !hidden.Decision.has_value()
				   && !hidden.Selection.has_value(),
			   "a one-pane gesture publishes no drawer or selection change");
		expect(state.FilesDrawer == workbench_drawer_state::expanded
				   && state.ConversationsDrawer == workbench_drawer_state::expanded
				   && state.Navigation.has_value(),
			   "a one-pane gesture disturbs no other retained fact");

		const auto shown
			= transition(state, toggle_workbench_one_pane_intent{ });
		expect(shown.OnePane.has_value()
				   && shown.OnePane->State == workbench_one_pane::disabled
				   && state.OnePane == workbench_one_pane::disabled,
			   "toggling again restores two panes");

		const auto unchanged = transition(
			state,
			set_workbench_one_pane_intent{
				.State = workbench_one_pane::disabled,
			});
		expect(unchanged == workbench_interaction_output{ },
			   "setting the state it already has publishes nothing");

		const auto set = transition(
			state,
			set_workbench_one_pane_intent{
				.State = workbench_one_pane::enabled,
			});
		expect(set.OnePane.has_value()
				   && set.OnePane->State == workbench_one_pane::enabled
				   && state.OnePane == workbench_one_pane::enabled,
			   "an exact set carries a persisted setting back into the core");
		for (const auto modality : { workbench_modality::review, workbench_modality::overview, workbench_modality::discuss, workbench_modality::threads }) {
			(void)transition(state, set_workbench_modality_intent{ .Modality = modality });
			expect(state.OnePane == workbench_one_pane::enabled, "modality never reclaims the pane preference");
		}
		expect(rejected(transition(inactive, set_workbench_one_pane_intent{ .State = workbench_one_pane::enabled }), workbench_reason_code::no_active_workbench), "inactive exact set is refused too");
	}

	/// \brief [[BPR-1130]]. The Updates line folds from the review bar's
	/// disclosure and from nothing else.
	auto test_updates_line_state() -> void
	{
		auto state = workbench_interaction_state{ };
		expect(state.UpdatesLine == workbench_updates_line::folded,
			   "a new window's Updates line starts folded");

		// The line is the review's chrome whether or not a workbench is
		// active, so the disclosure works while a pull request is still
		// loading, or when its comparison failed.
		const auto opened = transition(state, toggle_workbench_updates_line_intent{ });
		expect(opened.Reason == workbench_reason_code::none && opened.UpdatesLine.has_value()
				   && opened.UpdatesLine->State == workbench_updates_line::open
				   && state.UpdatesLine == workbench_updates_line::open,
			   "the disclosure opens the line with no workbench active");
		expect(!opened.Drawers && !opened.LineNumbers && !opened.OnePane && !opened.Selection
				   && !opened.Decision && !opened.Commands,
			   "the fold publishes nothing but itself, and changes no command's legality");

		const auto folded = transition(state, toggle_workbench_updates_line_intent{ });
		expect(folded.UpdatesLine && folded.UpdatesLine->State == workbench_updates_line::folded
				   && state.UpdatesLine == workbench_updates_line::folded,
			   "the disclosure folds it again");

		expect(transition(state, set_workbench_updates_line_intent{ .State = workbench_updates_line::folded })
				   == workbench_interaction_output{ },
			   "setting the state it already has publishes nothing");
		const auto restored = transition(state, set_workbench_updates_line_intent{ .State = workbench_updates_line::open });
		expect(restored.UpdatesLine && restored.UpdatesLine->State == workbench_updates_line::open,
			   "an exact set carries a window's persisted choice back into the core");

		// Nothing folds or opens on its own: no other gesture or observation
		// of this window touches it.
		auto live = active(resident(2U, 3U));
		static_cast<void>(transition(live, toggle_workbench_updates_line_intent{ }));
		for (const auto modality : { workbench_modality::overview, workbench_modality::review, workbench_modality::discuss, workbench_modality::threads }) {
			static_cast<void>(transition(live, set_workbench_modality_intent{ .Modality = modality }));
			expect(live.UpdatesLine == workbench_updates_line::open, "no modality folds the line");
		}
		static_cast<void>(transition(live, toggle_workbench_drawer_intent{ .Drawer = workbench_drawer::files }));
		static_cast<void>(transition(live, toggle_workbench_line_numbers_intent{ }));
		static_cast<void>(transition(live, navigate_workbench_file_intent{ .Direction = change_navigation_direction::next }));
		static_cast<void>(observe_workbench_navigation(live, std::nullopt));
		static_cast<void>(observe_workbench_navigation(live, resident(1U, 3U)));
		static_cast<void>(observe_workbench_change_navigation(live, std::nullopt));
		expect(live.UpdatesLine == workbench_updates_line::open,
			   "drawers, numbering, navigation and a workbench coming and going leave the line as it was");
		const auto commands = project_workbench_commands(live);
		const auto toggle   = commands.Commands[static_cast<std::size_t>(workbench_command_id::toggle_updates_line)];
		expect(toggle.Id == workbench_command_id::toggle_updates_line && toggle.Enabled,
			   "the tenth entry is the disclosure's own command, offered");
	}

	auto test_drawer_state() -> void
	{
		auto inactive = workbench_interaction_state{ };
		expect(inactive.FilesDrawer == workbench_drawer_state::expanded
				   && inactive.ConversationsDrawer == workbench_drawer_state::expanded,
			   "both drawers start expanded");
		// [[BPR-0675]]. A drawer holds the pull request's files, its threads
		// and the reviewer's repositories, all of which exist before any
		// comparison does, and the room it takes is the reviewer's to give
		// back whenever they like. Refusing this left an owner with three
		// full drawers, no way to collapse one, and two strip toggles that
		// did nothing.
		const auto collapsedWithoutASurface = transition(
			inactive,
			toggle_workbench_drawer_intent{ .Drawer = workbench_drawer::files });
		expect(collapsedWithoutASurface.Reason == workbench_reason_code::none
				   && collapsedWithoutASurface.Drawers.has_value()
				   && collapsedWithoutASurface.Drawers->Files == workbench_drawer_state::collapsed
				   && inactive.FilesDrawer == workbench_drawer_state::collapsed,
			   "a drawer collapses with no comparison open");
		expect(std::ranges::any_of(
				   project_workbench_commands(inactive).Commands,
				   [](const workbench_command_projection& aCommand) {
					   return aCommand.Id == workbench_command_id::toggle_files_drawer
						   && aCommand.Enabled;
				   }),
			   "and the command that does it is offered, not disabled");
		inactive = workbench_interaction_state{ };

		auto       state          = active(resident(2U, 3U));
		const auto collapsedFiles = transition(
			state,
			toggle_workbench_drawer_intent{ .Drawer = workbench_drawer::files });
		expect(collapsedFiles.Reason == workbench_reason_code::none
				   && collapsedFiles.Drawers.has_value()
				   && collapsedFiles.Drawers->Files
					   == workbench_drawer_state::collapsed
				   && collapsedFiles.Drawers->Conversations
					   == workbench_drawer_state::expanded
				   && state.FilesDrawer == workbench_drawer_state::collapsed
				   && state.ConversationsDrawer == workbench_drawer_state::expanded,
			   "toggling one drawer collapses it and leaves the other alone");
		expect(!collapsedFiles.Commands.has_value()
				   && !collapsedFiles.Decision.has_value()
				   && !collapsedFiles.Selection.has_value(),
			   "a drawer gesture publishes no command or selection change");

		const auto restored = transition(
			state,
			toggle_workbench_drawer_intent{ .Drawer = workbench_drawer::files });
		expect(restored.Drawers.has_value()
				   && restored.Drawers->Files == workbench_drawer_state::expanded
				   && state.FilesDrawer == workbench_drawer_state::expanded,
			   "toggling the same drawer again expands it");

		const auto unchanged = transition(
			state,
			set_workbench_drawer_intent{
				.Drawer = workbench_drawer::conversations,
				.State  = workbench_drawer_state::expanded,
			});
		expect(unchanged == workbench_interaction_output{ },
			   "setting the drawer state it already has publishes nothing");

		const auto set = transition(
			state,
			set_workbench_drawer_intent{
				.Drawer = workbench_drawer::conversations,
				.State  = workbench_drawer_state::collapsed,
			});
		expect(set.Drawers.has_value()
				   && set.Drawers->Conversations == workbench_drawer_state::collapsed
				   && set.Drawers->Files == workbench_drawer_state::expanded
				   && state.ConversationsDrawer == workbench_drawer_state::collapsed,
			   "setting a drawer state publishes both semantic drawer states");
	}

	auto test_overview_refuses_a_drawer_opened_by_hand() -> void
	{
		// [[BPR-0727]]. The owner chose: Overview keeps every drawer
		// collapsed; a hand-opened drawer is refused. The other three
		// modalities keep the room a drawer takes as the reviewer's.
		auto state = active(resident(2U, 3U));
		expect(state.Modality == workbench_modality::review,
			   "a workbench starts in Review, where every drawer gesture is accepted");

		const auto overview = transition(
			state,
			set_workbench_modality_intent{ .Modality = workbench_modality::overview });
		expect(overview.Reason == workbench_reason_code::none
				   && overview.Drawers.has_value()
				   && overview.Drawers->Files == workbench_drawer_state::collapsed
				   && overview.Drawers->Conversations == workbench_drawer_state::collapsed
				   && overview.Drawers->Repositories == workbench_drawer_state::collapsed
				   && state.Modality == workbench_modality::overview,
			   "choosing Overview collapses the three drawers in the same decision");
		expect(transition(state,
						  set_workbench_modality_intent{ .Modality = workbench_modality::overview })
				   == workbench_interaction_output{ },
			   "choosing the modality already chosen publishes nothing");

		const auto before  = state;
		const auto toggled = transition(
			state, toggle_workbench_drawer_intent{ .Drawer = workbench_drawer::files });
		const auto expanded = transition(
			state,
			set_workbench_drawer_intent{
				.Drawer = workbench_drawer::conversations,
				.State  = workbench_drawer_state::expanded,
			});
		expect(toggled.Reason == workbench_reason_code::overview_keeps_drawers_collapsed
				   && expanded.Reason == workbench_reason_code::overview_keeps_drawers_collapsed
				   && !toggled.Drawers.has_value() && !expanded.Drawers.has_value()
				   && state == before,
			   "in Overview a toggle and an exact expand are refused with the named reason "
			   "and change nothing");
		expect(std::ranges::all_of(
				   project_workbench_commands(state).Commands,
				   [](const workbench_command_projection& aCommand) {
					   return (aCommand.Id != workbench_command_id::toggle_files_drawer
							   && aCommand.Id != workbench_command_id::toggle_conversations_drawer)
						   || aCommand.Enabled;
				   }),
			   "the drawer toggles stay offered in Overview; the refusal is the reason, "
			   "not a dead control");
		expect(transition(state,
						  set_workbench_drawer_intent{
							  .Drawer = workbench_drawer::files,
							  .State  = workbench_drawer_state::collapsed,
						  })
				   == workbench_interaction_output{ },
			   "asking for the collapsed state Overview already keeps publishes nothing");

		for (const auto modality : { workbench_modality::review,
									 workbench_modality::discuss,
									 workbench_modality::threads }) {
			const auto chosen = transition(
				state, set_workbench_modality_intent{ .Modality = modality });
			expect(chosen.Reason == workbench_reason_code::none && state.Modality == modality,
				   "choosing another modality retains it");
			// Repositories, which neither Review's preset nor Discuss's opens.
			const auto opened = transition(
				state, toggle_workbench_drawer_intent{ .Drawer = workbench_drawer::repositories });
			expect(opened.Reason == workbench_reason_code::none
					   && opened.Drawers.has_value()
					   && opened.Drawers->Repositories == workbench_drawer_state::expanded,
				   "outside Overview a drawer opens by hand");
			const auto closed = transition(
				state, toggle_workbench_drawer_intent{ .Drawer = workbench_drawer::repositories });
			expect(closed.Drawers.has_value()
					   && closed.Drawers->Repositories == workbench_drawer_state::collapsed,
				   "and closes again");
			static_cast<void>(transition(
				state,
				set_workbench_modality_intent{ .Modality = workbench_modality::overview }));
		}
	}
	/// \brief Reads the three drawer states as one value.
	[[nodiscard]] auto drawers_of(const workbench_interaction_state& aState)
		-> workbench_drawer_delta
	{
		return {
			.Files         = aState.FilesDrawer,
			.Conversations = aState.ConversationsDrawer,
			.Repositories  = aState.RepositoriesDrawer,
		};
	}

	[[nodiscard]] constexpr auto arrangement(
		const workbench_drawer_state aFiles,
		const workbench_drawer_state aConversations,
		const workbench_drawer_state aRepositories) -> workbench_drawer_delta
	{
		return { .Files = aFiles, .Conversations = aConversations, .Repositories = aRepositories };
	}

	constexpr auto open   = workbench_drawer_state::expanded;
	constexpr auto closed = workbench_drawer_state::collapsed;

	[[nodiscard]] auto enter(workbench_interaction_state&   aState,
							 const workbench_modality       aModality,
							 const workbench_modality_entry aEntry = workbench_modality_entry::enter)
		-> workbench_interaction_output
	{
		return transition(aState, set_workbench_modality_intent{ .Modality = aModality, .Entry = aEntry });
	}

	[[nodiscard]] auto toggle_drawers_command(const workbench_interaction_state& aState)
		-> workbench_command_projection
	{
		return project_workbench_commands(aState).Commands[static_cast<std::size_t>(workbench_command_id::toggle_drawers)];
	}

	/// \brief [[BPR-1295]]. Review keeps the arrangement it last had; its
	/// preset applies on the window's first entry and on choosing Review in
	/// Review.
	auto test_review_remembers_its_arrangement() -> void
	{
		// A new window: the core is in Review by default, but Review has
		// not been entered. A pull request opens on Overview.
		auto state = active(resident(2U, 3U));
		expect(!state.ReviewEntered && !state.ReviewArrangement && !state.HiddenDrawers,
			   "a new window has entered no Review and remembers nothing");
		static_cast<void>(enter(state, workbench_modality::overview));
		expect(!state.ReviewArrangement.has_value(),
			   "leaving the default state before any entry keeps no arrangement for Review");

		// The first entry applies the preset: Files and Comments open,
		// Repositories as they were (Overview collapsed them).
		const auto first = enter(state, workbench_modality::review);
		expect(first.Drawers.has_value() && *first.Drawers == arrangement(open, open, closed)
				   && drawers_of(state) == arrangement(open, open, closed) && state.ReviewEntered,
			   "the window's first entry to Review applies its preset");
		expect(first.Commands.has_value()
				   && first.Commands->Commands[static_cast<std::size_t>(workbench_command_id::toggle_drawers)].Enabled,
			   "and offers the hide-and-show command, which Overview disabled");

		// The reviewer arranges Review by hand: Files closed, Repositories open.
		static_cast<void>(transition(state, toggle_workbench_drawer_intent{ .Drawer = workbench_drawer::files }));
		static_cast<void>(transition(state, toggle_workbench_drawer_intent{ .Drawer = workbench_drawer::repositories }));
		const auto mine = arrangement(closed, open, open);
		expect(drawers_of(state) == mine, "the fixture arrangement is the reviewer's");

		// Every way out and back keeps it: Overview, Discuss (whose preset the
		// window sends as exact intents), and an entry while already in Review.
		for (const auto away : { workbench_modality::overview, workbench_modality::discuss, workbench_modality::threads }) {
			static_cast<void>(enter(state, away));
			if (away == workbench_modality::discuss) {
				static_cast<void>(transition(state, set_workbench_drawer_intent{ .Drawer = workbench_drawer::files, .State = closed }));
				static_cast<void>(transition(state, set_workbench_drawer_intent{ .Drawer = workbench_drawer::repositories, .State = closed }));
			}
			expect(state.ReviewArrangement.has_value() && state.ReviewArrangement->Drawers == mine,
				   "leaving Review keeps its arrangement");
			const auto back = enter(state, workbench_modality::review);
			expect(drawers_of(state) == mine && !state.ReviewArrangement.has_value(),
				   "entering Review again restores the arrangement it last had, not the preset");
			expect(away != workbench_modality::overview || (back.Drawers.has_value() && *back.Drawers == mine),
				   "and publishes the restored drawers");
		}
		expect(enter(state, workbench_modality::review) == workbench_interaction_output{ } && drawers_of(state) == mine,
			   "entering Review while it shows changes nothing");

		// Choosing Review while it shows is the way back to the preset.
		const auto chosen = enter(state, workbench_modality::review, workbench_modality_entry::choose);
		expect(chosen.Drawers.has_value() && *chosen.Drawers == arrangement(open, open, open)
				   && drawers_of(state) == arrangement(open, open, open),
			   "choosing Review in Review applies the preset, Repositories as they were");

		// Choosing Review from another modality restores, as entering does.
		static_cast<void>(transition(state, toggle_workbench_drawer_intent{ .Drawer = workbench_drawer::conversations }));
		static_cast<void>(enter(state, workbench_modality::overview));
		static_cast<void>(enter(state, workbench_modality::review, workbench_modality_entry::choose));
		expect(drawers_of(state) == arrangement(open, closed, open),
			   "choosing Review from Overview restores Review's arrangement");

		// Windows are independent: another window's first entry is its preset.
		auto other = active(resident(1U, 3U));
		static_cast<void>(enter(other, workbench_modality::discuss));
		static_cast<void>(transition(other, set_workbench_drawer_intent{ .Drawer = workbench_drawer::files, .State = closed }));
		static_cast<void>(enter(other, workbench_modality::review));
		expect(drawers_of(other) == arrangement(open, open, open),
			   "a second window's first entry from Discuss applies the preset, Repositories as they were");

		// Negative control: a core that ignored the remembered arrangement
		// would land on the preset, which differs from the reviewer's here.
		expect(arrangement(open, open, closed) != mine && arrangement(open, open, open) != mine,
			   "the remembered arrangement differs from every preset this test could land on");
	}

	/// \brief [[BPR-1295]]. One command hides every open drawer and shows
	/// exactly those again.
	auto test_drawers_hide_and_show_exactly() -> void
	{
		auto state = active(resident(2U, 3U));
		static_cast<void>(enter(state, workbench_modality::review));
		// Every subset of open drawers comes back exactly.
		for (std::uint8_t mask = 1U; mask < 8U; ++mask) {
			const auto subset = arrangement((mask & 1U) != 0U ? open : closed,
											(mask & 2U) != 0U ? open : closed,
											(mask & 4U) != 0U ? open : closed);
			static_cast<void>(transition(state, set_workbench_drawer_intent{ .Drawer = workbench_drawer::files, .State = subset.Files }));
			static_cast<void>(transition(state, set_workbench_drawer_intent{ .Drawer = workbench_drawer::conversations, .State = subset.Conversations }));
			static_cast<void>(transition(state, set_workbench_drawer_intent{ .Drawer = workbench_drawer::repositories, .State = subset.Repositories }));
			expect(drawers_of(state) == subset && project_workbench_drawer_restore(state).Drawers == arrangement(open, open, subset.Repositories),
				   "with drawers open, showing would open Review's preset; nothing is remembered");
			const auto hidden = transition(state, toggle_workbench_drawers_intent{ });
			expect(hidden.Reason == workbench_reason_code::none && hidden.Drawers.has_value()
					   && *hidden.Drawers == arrangement(closed, closed, closed)
					   && state.HiddenDrawers == std::optional{ subset },
				   "hiding closes every drawer and remembers which were open");
			expect(project_workbench_drawer_restore(state).Drawers == subset,
				   "showing would open exactly the remembered set");
			const auto shown = transition(state, toggle_workbench_drawers_intent{ });
			expect(shown.Drawers.has_value() && *shown.Drawers == subset && drawers_of(state) == subset
					   && !state.HiddenDrawers.has_value(),
				   "showing opens exactly the remembered set and forgets it");
		}

		// Nothing remembered and nothing open: Review's preset.
		static_cast<void>(transition(state, set_workbench_drawer_intent{ .Drawer = workbench_drawer::files, .State = closed }));
		static_cast<void>(transition(state, set_workbench_drawer_intent{ .Drawer = workbench_drawer::conversations, .State = closed }));
		static_cast<void>(transition(state, set_workbench_drawer_intent{ .Drawer = workbench_drawer::repositories, .State = closed }));
		const auto preset = transition(state, toggle_workbench_drawers_intent{ });
		expect(preset.Drawers.has_value() && *preset.Drawers == arrangement(open, open, closed),
			   "with every drawer closed by hand and nothing remembered, showing opens Files and Comments");

		// A drawer opened by hand while hidden drops the memory.
		static_cast<void>(transition(state, set_workbench_drawer_intent{ .Drawer = workbench_drawer::repositories, .State = open }));
		static_cast<void>(transition(state, toggle_workbench_drawers_intent{ }));
		expect(state.HiddenDrawers == std::optional{ arrangement(open, open, open) }, "all three hidden");
		static_cast<void>(transition(state, toggle_workbench_drawer_intent{ .Drawer = workbench_drawer::conversations }));
		expect(!state.HiddenDrawers.has_value() && drawers_of(state) == arrangement(closed, open, closed),
			   "opening Comments by hand while hidden opens it alone and drops the memory");
		const auto handHides = transition(state, toggle_workbench_drawers_intent{ });
		expect(handHides.Drawers.has_value() && *handHides.Drawers == arrangement(closed, closed, closed)
				   && state.HiddenDrawers == std::optional{ arrangement(closed, open, closed) },
			   "the next press hides what is open now, not the old set");
		static_cast<void>(transition(state, toggle_workbench_drawers_intent{ }));

		// Choosing Review in Review drops it too.
		static_cast<void>(transition(state, set_workbench_drawer_intent{ .Drawer = workbench_drawer::repositories, .State = open }));
		static_cast<void>(transition(state, toggle_workbench_drawers_intent{ }));
		static_cast<void>(enter(state, workbench_modality::review, workbench_modality_entry::choose));
		expect(!state.HiddenDrawers.has_value() && drawers_of(state) == arrangement(open, open, closed),
			   "choosing Review in Review applies the preset and forgets the hidden set");

		// Leaving Review and coming back keeps it, hidden drawers included.
		static_cast<void>(transition(state, set_workbench_drawer_intent{ .Drawer = workbench_drawer::repositories, .State = open }));
		static_cast<void>(transition(state, toggle_workbench_drawers_intent{ }));
		for (const auto away : { workbench_modality::overview, workbench_modality::discuss, workbench_modality::threads }) {
			static_cast<void>(enter(state, away));
			expect(!state.HiddenDrawers.has_value(), "no other modality holds the hidden set");
			static_cast<void>(enter(state, workbench_modality::review));
			expect(drawers_of(state) == arrangement(closed, closed, closed)
					   && state.HiddenDrawers == std::optional{ arrangement(open, open, open) },
				   "a window that left Review with its drawers hidden comes back hidden");
		}
		static_cast<void>(transition(state, toggle_workbench_drawers_intent{ }));
		expect(drawers_of(state) == arrangement(open, open, open), "and the command still restores the same set");

		// Overview refuses with its reason; Discuss disables the command.
		static_cast<void>(enter(state, workbench_modality::overview));
		const auto inOverview = state;
		expect(rejected(transition(state, toggle_workbench_drawers_intent{ }), workbench_reason_code::overview_keeps_drawers_collapsed)
				   && state == inOverview,
			   "Overview refuses the command with overview_keeps_drawers_collapsed");
		expect(!toggle_drawers_command(state).Enabled
				   && toggle_drawers_command(state).DisabledReason == workbench_reason_code::overview_keeps_drawers_collapsed,
			   "and projects it disabled with that reason");
		static_cast<void>(enter(state, workbench_modality::discuss));
		const auto inDiscuss = state;
		expect(rejected(transition(state, toggle_workbench_drawers_intent{ }), workbench_reason_code::discuss_shows_the_conversation)
				   && state == inDiscuss,
			   "Discuss refuses it, the conversation being the page");
		expect(!toggle_drawers_command(state).Enabled
				   && toggle_drawers_command(state).DisabledReason == workbench_reason_code::discuss_shows_the_conversation,
			   "and projects it disabled");
		// [[BPR-1293]]. Threads disables it too: its drawers are hidden.
		static_cast<void>(enter(state, workbench_modality::threads));
		const auto inThreads = state;
		expect(rejected(transition(state, toggle_workbench_drawers_intent{ }), workbench_reason_code::threads_finds_a_thread)
				   && state == inThreads,
			   "Threads refuses it, the finder being the page");
		expect(!toggle_drawers_command(state).Enabled
				   && toggle_drawers_command(state).DisabledReason == workbench_reason_code::threads_finds_a_thread,
			   "and projects it disabled with its own reason");
		static_cast<void>(enter(state, workbench_modality::review));
		expect(toggle_drawers_command(state).Enabled && toggle_drawers_command(state).DisabledReason == workbench_reason_code::none,
			   "Review offers it");

		// Like a drawer toggle, it does not wait for a comparison.
		auto       inactive  = workbench_interaction_state{ };
		const auto noSurface = transition(inactive, toggle_workbench_drawers_intent{ });
		expect(noSurface.Reason == workbench_reason_code::none && drawers_of(inactive) == arrangement(closed, closed, closed),
			   "with no workbench active the drawers still hide");
	}
	/// \brief [[BPR-1293]]. Threads is entered and left without touching
	/// a drawer, from every modality, and Review comes back as it was left.
	auto test_conversations_changes_no_drawer() -> void
	{
		for (const auto from : { workbench_modality::overview, workbench_modality::review, workbench_modality::discuss }) {
			auto state = active(resident(1U, 3U));
			static_cast<void>(enter(state, workbench_modality::review));
			// Review arranged by hand and its drawers hidden with the command.
			static_cast<void>(transition(state, toggle_workbench_drawer_intent{ .Drawer = workbench_drawer::files }));
			static_cast<void>(transition(state, toggle_workbench_drawers_intent{ }));
			const auto hidden = state.HiddenDrawers;
			expect(hidden.has_value() && drawers_of(state) == arrangement(closed, closed, closed),
				   "the fixture leaves Review with its drawers hidden");
			static_cast<void>(enter(state, from));
			const auto before  = drawers_of(state);
			const auto entered = enter(state, workbench_modality::threads);
			expect(entered.Reason == workbench_reason_code::none && state.Modality == workbench_modality::threads,
				   "Threads is entered from every modality");
			expect(!entered.Drawers.has_value() && drawers_of(state) == before,
				   "entering it publishes no drawer and changes none");
			// A drawer gesture there is the reviewer's, as in Discuss; the
			// window keeps the drawers hidden whatever their states.
			const auto left = enter(state, from);
			expect(from == workbench_modality::review || (!left.Drawers.has_value() && drawers_of(state) == before),
				   "leaving it for the modality it came from changes no drawer either");
			static_cast<void>(enter(state, workbench_modality::review));
			expect(drawers_of(state) == arrangement(closed, closed, closed) && state.HiddenDrawers == hidden,
				   "Review comes back with the arrangement it last had, hidden drawers included");
			static_cast<void>(transition(state, toggle_workbench_drawers_intent{ }));
			expect(hidden.has_value() && drawers_of(state) == *hidden && hidden->Files == closed && hidden->Conversations == open,
				   "and the command restores the set it hid before Threads");
		}
		// Negative control: Discuss's preset is the window's two exact
		// intents, never the core's, so a core that applied one on entry would
		// have opened Comments here.
		auto preset = active(resident(1U, 3U));
		static_cast<void>(enter(preset, workbench_modality::review));
		static_cast<void>(transition(preset, set_workbench_drawer_intent{ .Drawer = workbench_drawer::conversations, .State = closed }));
		static_cast<void>(enter(preset, workbench_modality::threads));
		expect(preset.ConversationsDrawer == closed, "Conversations does not open the Comments drawer");
	}

	/// [[BPR-1356]]. A thread found in Threads: Review with Comments open,
	/// or Discuss with exactly the filters that would hide it cleared; my
	/// draft, which Discuss does not list, opens in Review and says why.
	auto test_a_thread_routes_to_review_or_discuss() -> void
	{
		const auto review = route_workbench_thread(open_workbench_thread_intent{ });
		expect(review == workbench_thread_route{ .Modality = workbench_modality::review, .OpenComments = true },
			   "a click or Return opens the thread in Review with Comments open");
		const auto hidingAll = workbench_discuss_filters{ .OtherFile = true, .OnlyNew = true, .HideResolved = true, .Listing = true };
		expect(route_workbench_thread(open_workbench_thread_intent{ .Hiding = hidingAll }).Clear == workbench_discuss_filters{ },
			   "Review clears none of Discuss's filters");

		const auto discuss = route_workbench_thread(open_workbench_thread_intent{ .Destination = workbench_thread_destination::discuss });
		expect(discuss == workbench_thread_route{ .Modality = workbench_modality::discuss },
			   "Discuss with nothing hiding the thread changes no filter and opens no drawer");
		expect(!discuss.Clear.any(), "and clears nothing");
		for (const auto& hiding : { workbench_discuss_filters{ .OtherFile = true }, workbench_discuss_filters{ .OnlyNew = true },
									workbench_discuss_filters{ .HideResolved = true }, workbench_discuss_filters{ .Listing = true },
									workbench_discuss_filters{ .OnlyNew = true, .HideResolved = true }, hidingAll }) {
			const auto route = route_workbench_thread(open_workbench_thread_intent{ .Destination = workbench_thread_destination::discuss, .Hiding = hiding });
			expect(route.Modality == workbench_modality::discuss && route.Clear == hiding && route.Clear.any() && !route.OpenComments
					   && route.Reason == workbench_thread_route_reason::none,
				   "Discuss clears exactly the filters that would hide the thread");
		}

		const auto draft = route_workbench_thread(open_workbench_thread_intent{
			.Destination = workbench_thread_destination::discuss,
			.Draft       = true,
			.Hiding      = workbench_discuss_filters{ .OtherFile = true },
		});
		expect(draft.Modality == workbench_modality::review && draft.OpenComments && !draft.Clear.any()
				   && draft.Reason == workbench_thread_route_reason::draft_opens_in_review,
			   "my draft asked for in Discuss opens in Review, says why and clears nothing");
		expect(route_workbench_thread(open_workbench_thread_intent{ .Draft = true }).Reason == workbench_thread_route_reason::none,
			   "my draft asked for in Review opens there with no reason");
	}

	void test_header_source_command_legality()
	{
		auto       state   = workbench_interaction_state{ };
		const auto command = [&] { return project_workbench_commands(state).Commands[static_cast<std::size_t>(workbench_command_id::go_to_header_source)]; };
		expect(!command().Enabled && command().DisabledReason == workbench_reason_code::header_source_no_selection, "no selection disables counterpart command");
		state.HeaderSource = header_source_status::unavailable_catalogue;
		expect(!command().Enabled && command().DisabledReason == workbench_reason_code::header_source_unavailable_catalogue, "unavailable catalogue has its own reason");
		state.HeaderSource = header_source_status::unsupported_file;
		expect(!command().Enabled && command().DisabledReason == workbench_reason_code::header_source_unsupported_file, "unsupported suffix has its own reason");
		state.Navigation = resident(1U, 1U);
		for (const auto status : { header_source_status::matched, header_source_status::no_match, header_source_status::ambiguous }) {
			state.HeaderSource = status;
			expect(command().Enabled, "accepted supported selection offers counterpart command even for no-op");
		}
		for (const auto modality : { workbench_modality::overview, workbench_modality::discuss, workbench_modality::threads }) {
			state.Modality = modality;
			expect(!command().Enabled && command().DisabledReason == workbench_reason_code::header_source_requires_review, "inactive modality cannot navigate hidden review source");
		}
	}
} // namespace

int main()
{
	test_header_source_command_legality();
	test_cursor_contract();
	test_change_cursor_contract();
	test_navigation_follows_actionable_order();
	test_the_ends_of_the_change_order_are_named_not_derived();
	test_change_commands_agree_with_navigate_change();
	test_navigation_commands_are_bounded_and_always_enabled();
	test_command_projection();
	test_selection_gestures();
	test_navigation_observations();
	test_change_observations();
	test_drawer_state();
	test_overview_refuses_a_drawer_opened_by_hand();
	test_line_number_state();
	test_one_pane_state();
	test_updates_line_state();
	test_review_remembers_its_arrangement();
	test_drawers_hide_and_show_exactly();
	test_conversations_changes_no_drawer();
	test_a_thread_routes_to_review_or_discuss();
	return EXIT_SUCCESS;
}
