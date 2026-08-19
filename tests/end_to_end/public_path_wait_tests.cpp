/// \file
/// \brief A timed-out wait names the conjunct that failed, and nothing else.
///
/// \details BPR-0381's last requirement, and the reason it is a requirement: a
/// gate that only ran when something else was already broken would not have
/// caught the gap this exists to close. So the report is asserted directly, on a
/// deliberately-false conjunction, with no application and no event loop.
///
/// Qt-free on purpose. `public_path_wait.hpp` holds only standard C++ precisely so
/// this can compile it; the polling loop that needs Qt lives elsewhere.

#include "public_path_wait.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {
	using bpr::e2e::wait_terms;

	int Failures = 0;

	void expect(const bool aCondition, const std::string_view aWhat)
	{
		if (!aCondition) {
			std::cerr << "public-path-wait-tests:failed:" << aWhat << '\n';
			++Failures;
		}
	}

	void expect_contains(
		const std::string&     aReport,
		const std::string_view aNeedle,
		const std::string_view aWhat)
	{
		if (aReport.find(aNeedle) == std::string::npos) {
			std::cerr << "public-path-wait-tests:failed:" << aWhat << ": expected \""
					  << aNeedle << "\" in \"" << aReport << "\"\n";
			++Failures;
		}
	}

	void expect_absent(
		const std::string&     aReport,
		const std::string_view aNeedle,
		const std::string_view aWhat)
	{
		if (aReport.find(aNeedle) != std::string::npos) {
			std::cerr << "public-path-wait-tests:failed:" << aWhat << ": did not expect \""
					  << aNeedle << "\" in \"" << aReport << "\"\n";
			++Failures;
		}
	}

	/// The shape BPR-0344 could not read: a conjunction whose middle term is
	/// false. `&&` short-circuits, so the terms after it never evaluate.
	void the_first_false_term_is_named()
	{
		auto       terms   = wait_terms{ };
		auto       reached = false;
		const auto held    = terms.require("remove-enabled", true)
			&& terms.require("row-count-is-one", false)
			&& [&reached, &terms] {
				   reached = true;
				   return terms.require("bucket-is-quiet", true);
			   }();

		expect(!held, "a false conjunct must fail the predicate");
		expect(!reached, "short-circuit must stop at the first false term");
		expect(
			terms.failing_term() == "row-count-is-one",
			"the failing term must be the first false one");

		const auto report = terms.render();
		expect_contains(report, "failed=row-count-is-one", "report names the failure");
		expect_contains(report, "remove-enabled=true", "terms before it are recorded");
		expect_contains(report, "row-count-is-one=false", "the failing term's value");
		expect_absent(report, "bucket-is-quiet", "terms after it are not invented");
	}

	/// A predicate that holds carries no failure, so a caller cannot print a
	/// report for a wait that worked.
	void a_satisfied_predicate_names_nothing()
	{
		auto terms = wait_terms{ };
		expect(
			terms.require("a", true) && terms.require("b", true),
			"a true conjunction must hold");
		expect(terms.failing_term().empty(), "a satisfied wait has no failing term");
		expect_absent(terms.render(), "failed=", "a satisfied wait reports no failure");
	}

	/// The values the card asks for, alongside the terms.
	void observed_values_are_reported()
	{
		auto terms = wait_terms{ };
		terms.note("rows", std::uint64_t{ 0U });
		terms.note("remove-enabled", false);
		terms.note_name("bucket", "changed-since-my-review");
		terms.note_identifier("ref", "github.com/octo-reviewer/review-repository#17");

		const auto report = terms.render();
		expect_contains(report, "rows=0", "a count is reported");
		expect_contains(report, "remove-enabled=false", "a flag is reported");
		expect_contains(report, "bucket=changed-since-my-review", "an enumerator is reported");
		expect_contains(
			report,
			"ref=github.com/octo-reviewer/review-repository#17",
			"an identifier is reported");
	}

	/// Bounded, and visibly so. A 20-second poll must not become 4000 lines.
	void the_report_is_bounded_and_says_when_it_truncates()
	{
		auto terms = wait_terms{ };
		for (auto index = std::size_t{ 0U }; index < wait_terms::maximum_terms + 8U;
			 ++index) {
			terms.note("term", static_cast<std::uint64_t>(index));
		}
		const auto report = terms.render();
		expect(
			report.size() <= wait_terms::maximum_bytes + 64U,
			"the report must stay within its byte bound");
		expect_contains(report, "elided=8", "truncation must be visible, not silent");
	}

	/// Nothing secret or content-bearing, by construction rather than by the
	/// fixtures happening to be synthetic.
	void a_hostile_identifier_is_bounded_and_sanitized()
	{
		auto terms   = wait_terms{ };
		auto hostile = std::string{ "line\nbreak\ttab\x01" };
		hostile += "=equals";
		hostile += std::string(400U, 'x');
		hostile += "\xc3\xa9";
		terms.note_identifier("ref", hostile);

		const auto report = terms.render();
		expect_absent(report, "\n", "a newline must not reach the report");
		expect_absent(report, "\t", "a tab must not reach the report");
		expect_absent(report, "\x01", "a control byte must not reach the report");
		expect(
			report.find('=') != std::string::npos
				&& report.find("=equals") == std::string::npos,
			"an embedded '=' must not let a value look like another term");
		expect(
			report.size() <= wait_terms::maximum_identifier_bytes + 32U,
			"an identifier must be capped");
		expect_contains(report, "...", "capping must be visible");
	}

	/// A term's own name is caller-supplied too.
	void a_hostile_term_name_is_sanitized()
	{
		auto terms = wait_terms{ };
		static_cast<void>(terms.require("name\nwith=breaks", false));
		const auto report = terms.render();
		expect_absent(report, "\n", "a newline in a term name must not reach the report");
		expect_contains(report, "name?with?breaks", "the name is sanitized in place");
	}
} // namespace

auto main() -> int
{
	the_first_false_term_is_named();
	a_satisfied_predicate_names_nothing();
	observed_values_are_reported();
	the_report_is_bounded_and_says_when_it_truncates();
	a_hostile_identifier_is_bounded_and_sanitized();
	a_hostile_term_name_is_sanitized();

	if (Failures != 0) {
		std::cerr << "public-path-wait-tests:failures=" << Failures << '\n';
		return EXIT_FAILURE;
	}
	std::cout << "public-path-wait-tests:pass\n";
	return EXIT_SUCCESS;
}
