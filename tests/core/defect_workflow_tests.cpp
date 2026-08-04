#include "bpr/core/defect_workflow.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {
	using namespace bpr;

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
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes.fill(aSeed);
		return Identity{ bytes };
	}

	[[nodiscard]] auto defect(const core::defect_state aState)
		-> core::defect_record
	{
		return core::defect_record{
			.Id     = core::defect_id{ 1U },
			.Anchor = core::source_line_ref{
				.Comparison = opaque<core::file_comparison_digest>(2U),
				.Side       = core::diff_side::right,
				.Line       = 30U },
			.State = aState,
		};
	}

	[[nodiscard]] auto command(const core::defect_command_kind aKind)
		-> core::defect_command
	{
		return core::defect_command{ .Id = core::defect_id{ 1U }, .Kind = aKind };
	}

	using kind  = core::defect_command_kind;
	using state = core::defect_state;

	void test_the_ordinary_lifecycle_runs_end_to_end()
	{
		const auto claimed
			= core::apply(defect(state::open), command(kind::claim_fix));
		expect(
			claimed.Changed && claimed.Defect.State == state::fix_claimed,
			"an author's claim moves an open defect to fix-claimed");

		const auto verified
			= core::apply(claimed.Defect, command(kind::verify));
		expect(
			verified.Changed && verified.Defect.State == state::verified,
			"an explicit reviewer verification reaches verified");
		expect(
			!core::outstanding(verified.Defect.State),
			"a verified defect asks nothing further of the reviewer");

		const auto reopened
			= core::apply(verified.Defect, command(kind::reopen));
		expect(
			reopened.Changed && reopened.Defect.State == state::reopened
				&& core::outstanding(reopened.Defect.State),
			"reopening puts it back in front of the reviewer");
	}

	// The rule the whole RFC exists to protect. `verify` is the only command
	// that reaches verified, and it is an explicit reviewer intent — there is
	// deliberately no command by which a claim or a code change can verify.
	void test_only_an_explicit_verification_reaches_verified()
	{
		constexpr auto everythingElse = std::array{
			kind::claim_fix,
			kind::reopen,
			kind::withdraw,
			kind::accept_as_is,
			kind::reclassify,
		};

		for (const auto starting :
			 { state::open, state::fix_claimed, state::needs_verification,
			   state::reopened }) {
			for (const auto attempt : everythingElse) {
				auto issued = command(attempt);
				if (attempt == kind::reclassify) {
					issued.Severity = core::defect_severity::major;
				}
				const auto outcome = core::apply(defect(starting), issued);
				expect(
					outcome.Defect.State != state::verified,
					"no command other than verify ever reaches verified");
			}
		}
	}

	// Both mean "somebody says this is done" and neither is verified. They stay
	// distinct because the reviewer needs to know whether a person asserted the
	// fix or a heuristic noticed a change.
	void test_a_claim_and_a_suggestion_are_both_short_of_verified()
	{
		const auto claimed
			= core::apply(defect(state::open), command(kind::claim_fix));
		expect(
			claimed.Defect.State == state::fix_claimed
				&& claimed.Defect.State != state::verified,
			"a fix claim is not a verification");
		expect(
			core::outstanding(state::fix_claimed)
				&& core::outstanding(state::needs_verification),
			"both still ask something of the reviewer");
		expect(
			state::fix_claimed != state::needs_verification,
			"the two are distinguishable, so their source is not lost");
	}

	void test_verifying_nothing_is_refused()
	{
		for (const auto starting : { state::open, state::reopened }) {
			const auto refused
				= core::apply(defect(starting), command(kind::verify));
			expect(
				refused.Rejection == core::defect_rejection::nothing_to_verify
					&& !refused.Changed,
				"verifying a defect nobody has addressed is refused");
		}

		const auto verified = core::apply(
			defect(state::needs_verification),
			command(kind::verify));
		expect(
			verified.Changed && verified.Defect.State == state::verified,
			"verifying a defect with evidence is legal");
	}

	void test_reopening_requires_something_to_reopen()
	{
		for (const auto starting :
			 { state::open, state::fix_claimed, state::needs_verification }) {
			const auto refused
				= core::apply(defect(starting), command(kind::reopen));
			expect(
				refused.Rejection == core::defect_rejection::not_verified
					&& !refused.Changed,
				"reopening something never verified is refused");
		}
	}

	// Withdrawn and accepted-as-is are decisions already taken. Reopening one
	// is a new defect, not a transition, so the earlier decision is not
	// overwritten.
	void test_a_settled_defect_stays_settled()
	{
		for (const auto settled :
			 { state::withdrawn, state::accepted_as_is }) {
			for (const auto attempt :
				 { kind::claim_fix, kind::verify, kind::reopen,
				   kind::withdraw, kind::accept_as_is }) {
				const auto refused
					= core::apply(defect(settled), command(attempt));
				expect(
					refused.Rejection == core::defect_rejection::already_settled
						&& !refused.Changed,
					"a settled defect refuses every further command");
			}
		}
	}

	void test_reclassification_changes_enrichment_and_not_the_lifecycle()
	{
		auto reclassify     = command(kind::reclassify);
		reclassify.Severity = core::defect_severity::critical;
		reclassify.Category = core::defect_category::security;

		const auto outcome
			= core::apply(defect(state::fix_claimed), reclassify);
		expect(
			outcome.Changed
				&& outcome.Defect.Severity == core::defect_severity::critical
				&& outcome.Defect.Category == core::defect_category::security,
			"reclassifying changes severity and category");
		expect(
			outcome.Defect.State == state::fix_claimed,
			"reclassifying moves nothing in the lifecycle");

		const auto empty
			= core::apply(defect(state::open), command(kind::reclassify));
		expect(
			empty.Rejection == core::defect_rejection::nothing_to_reclassify
				&& !empty.Changed,
			"a reclassification that names neither field is refused");

		auto severityOnly     = command(kind::reclassify);
		severityOnly.Severity = core::defect_severity::major;
		const auto partial    = core::apply(defect(state::open), severityOnly);
		expect(
			partial.Defect.Severity == core::defect_severity::major
				&& partial.Defect.Category == defect(state::open).Category,
			"naming one field leaves the other alone");
	}

	void test_a_command_for_another_defect_is_refused()
	{
		auto elsewhere = command(kind::verify);
		elsewhere.Id   = core::defect_id{ 99U };
		const auto refused
			= core::apply(defect(state::fix_claimed), elsewhere);
		expect(
			refused.Rejection == core::defect_rejection::unknown_defect
				&& !refused.Changed,
			"a command naming another defect is refused as unknown");
	}

	void test_every_enumerator_names_itself()
	{
		expect(
			core::name(state::needs_verification) == "needs_verification"
				&& core::name(state::accepted_as_is) == "accepted_as_is",
			"every state names itself");
		expect(
			core::name(core::defect_severity::critical) == "critical"
				&& core::name(core::defect_category::security) == "security",
			"severity and category name themselves");
		expect(
			core::name(kind::accept_as_is) == "accept_as_is"
				&& core::name(core::defect_rejection::nothing_to_verify)
					== "nothing_to_verify",
			"every command and rejection names itself");
	}
} // namespace

auto main() -> int
{
	test_the_ordinary_lifecycle_runs_end_to_end();
	test_only_an_explicit_verification_reaches_verified();
	test_a_claim_and_a_suggestion_are_both_short_of_verified();
	test_verifying_nothing_is_refused();
	test_reopening_requires_something_to_reopen();
	test_a_settled_defect_stays_settled();
	test_reclassification_changes_enrichment_and_not_the_lifecycle();
	test_a_command_for_another_defect_is_refused();
	test_every_enumerator_names_itself();

	std::cout << "defect-workflow-core-tests:pass\n";
	return EXIT_SUCCESS;
}
