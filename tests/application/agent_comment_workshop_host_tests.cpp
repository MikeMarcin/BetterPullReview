/// \file
/// \brief Proves each of the six workshop operations reaches one proposal.

#include "bpr/application/agent_comment_workshop_host.hpp"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	/// Records what the workshop showed. The host's only outbound path.
	struct recording_projection final
		: application::agent_comment_workshop_projection_port
	{
		std::vector<application::agent_comment_workshop_proposal> Proposals;
		std::vector<application::agent_comment_workshop_refusal>  Refusals;

		void present(
			const application::agent_comment_workshop_proposal& aProposal)
			override
		{
			Proposals.push_back(aProposal);
		}

		void refuse(
			const application::agent_comment_workshop_refusal aRefusal) override
		{
			Refusals.push_back(aRefusal);
		}
	};

	[[nodiscard]] auto a_draft() -> core::review_comment_draft
	{
		return core::review_comment_draft{
			.Id   = core::review_draft_id{ 7U },
			.Body = "This is confusing.",
		};
	}

	[[nodiscard]] auto an_edit(std::string aBody) -> core::review_draft_command
	{
		return core::review_draft_command{
			.Id   = core::review_draft_id{ 7U },
			.Kind = core::review_draft_command_kind::edit,
			.Body = std::move(aBody),
		};
	}

	/// \brief Every operation reaches the proposal, named, and none is lost.
	void test_each_operation_reaches_one_proposal_that_names_it()
	{
		auto projection = recording_projection{ };
		auto host       = application::agent_comment_workshop_host{ projection };

		for (auto index = std::size_t{ 0U };
			 index < core::workshop_operation_count;
			 ++index) {
			const auto asked = static_cast<core::workshop_operation>(index);
			const auto refusal
				= host.propose(a_draft(), an_edit("Name the failing case."), asked);
			expect(
				refusal == application::agent_comment_workshop_refusal::none,
				"an operation does not change whether a revision is offered");
			expect(
				projection.Proposals.size() == index + 1U
					&& projection.Proposals.back().Operation
						== std::optional{ asked },
				"the proposal names the operation that asked for it, so the "
				"before/after says why the after differs");
			expect(
				projection.Proposals.back().Original == a_draft().Body
					&& projection.Proposals.back().Proposed
						== "Name the failing case."
					&& projection.Proposals.back().Draft == a_draft().Id,
				"and carries the same two texts and the same draft it always "
				"did");
		}

		expect(
			projection.Refusals.empty()
				&& host.statistics().Proposals == core::workshop_operation_count
				&& host.statistics().Refusals == 0U,
			"six operations produced six proposals through the one path");
	}

	/// \brief A proposal nobody workshopped names no operation.
	void test_a_promoted_finding_still_names_no_operation()
	{
		auto projection = recording_projection{ };
		auto host       = application::agent_comment_workshop_host{ projection };

		expect(
			host.propose(a_draft(), an_edit("Name the failing case."))
				== application::agent_comment_workshop_refusal::none,
			"the shipped promotion path calls the same function it always "
			"called");
		expect(
			projection.Proposals.size() == 1U
				&& !projection.Proposals.front().Operation.has_value(),
			"and its proposal names no operation, because none was asked for "
			"- an operation here would claim one ran");
	}

	/// \brief Absent the draft it names, every operation is refused by name.
	void test_every_operation_is_refused_when_the_draft_is_absent()
	{
		auto projection = recording_projection{ };
		auto host       = application::agent_comment_workshop_host{ projection };

		// The draft the reviewer has is not the draft the command names -
		// which is what "the draft is absent" is at this layer, the widget
		// having no draft to name being the same refusal one layer up.
		auto elsewhere = an_edit("Name the failing case.");
		elsewhere.Id   = core::review_draft_id{ 8U };

		for (auto index = std::size_t{ 0U };
			 index < core::workshop_operation_count;
			 ++index) {
			const auto asked = static_cast<core::workshop_operation>(index);
			expect(
				host.propose(a_draft(), elsewhere, asked)
					== application::agent_comment_workshop_refusal::
						different_draft,
				"no operation makes a revision meant for another draft fit "
				"this one");
		}
		expect(
			projection.Proposals.empty()
				&& projection.Refusals.size() == core::workshop_operation_count,
			"and every refusal is said rather than nothing appearing");

		auto empty = an_edit(std::string{ });
		expect(
			host.propose(
				a_draft(), empty, core::workshop_operation::shorten)
				== application::agent_comment_workshop_refusal::empty_revision,
			"shortening a comment to nothing is a deletion nobody asked for");
		expect(
			host.propose(
				a_draft(),
				an_edit(a_draft().Body),
				core::workshop_operation::clarify)
				== application::agent_comment_workshop_refusal::unchanged,
			"and an operation that changed nothing is not offered as a "
			"change");
	}

	/// \brief An undeclared operation is dropped, not shown.
	void test_an_undeclared_operation_is_not_named_on_a_proposal()
	{
		auto projection = recording_projection{ };
		auto host       = application::agent_comment_workshop_host{ projection };

		expect(
			host.propose(
				a_draft(),
				an_edit("Name the failing case."),
				static_cast<core::workshop_operation>(
					core::workshop_operation_count))
				== application::agent_comment_workshop_refusal::none,
			"a cast enumerator does not make a good revision refusable");
		expect(
			projection.Proposals.size() == 1U
				&& !projection.Proposals.front().Operation.has_value(),
			"but it is not named on the proposal either, because there is no "
			"name for it to be read under");
	}
} // namespace

auto main() -> int
{
	test_each_operation_reaches_one_proposal_that_names_it();
	test_a_promoted_finding_still_names_no_operation();
	test_every_operation_is_refused_when_the_draft_is_absent();
	test_an_undeclared_operation_is_not_named_on_a_proposal();
	std::cout << "agent-comment-workshop-host-tests:pass\n";
	return EXIT_SUCCESS;
}
