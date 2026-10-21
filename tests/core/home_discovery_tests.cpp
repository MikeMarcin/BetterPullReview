// [[BPR-1680]]. Home discovery's core decisions: the diagnostic line a
// refresh and each of its reads record.
#include "bpr/core/discovery_diagnosis.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {
	using namespace bpr::core;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void test_diagnosis_line()
	{
		const auto read = discovery_diagnosis{
			.Scope            = discovery_diagnosis_scope::read,
			.Refresh          = 3U,
			.Read             = 7U,
			.Reason           = discovery_diagnosis_reason::authored,
			.Repositories     = 1U,
			.Requests         = 6U,
			.WaitMilliseconds = 120U,
			.RunMilliseconds  = 3900U,
		};
		const auto text = describe(read);
		expect(text == "discovery=read refresh=3 read=7 reason=authored state=none repositories=1 pull_requests=0 reads=0 failed=0 requests=6 wait_ms=120 run_ms=3900",
			   "a read's line names its keys in their fixed order");
		expect(parse_discovery_diagnosis(text) == read, "the line reads back as the same diagnosis");

		const auto refresh = discovery_diagnosis{
			.Scope            = discovery_diagnosis_scope::refresh,
			.Refresh          = 3U,
			.State            = discovery_diagnosis_state::stale,
			.Repositories     = 5U,
			.PullRequests     = 20U,
			.Reads            = 30U,
			.Failed           = 1U,
			.Requests         = 220U,
			.WaitMilliseconds = 4100U,
			.RunMilliseconds  = 143000U,
		};
		expect(parse_discovery_diagnosis(describe(refresh)) == refresh, "a refresh's end reads back");
		for (const auto state : { discovery_diagnosis_state::none, discovery_diagnosis_state::current, discovery_diagnosis_state::partial,
								  discovery_diagnosis_state::stopped, discovery_diagnosis_state::cancelled }) {
			auto other  = refresh;
			other.State = state;
			expect(parse_discovery_diagnosis(describe(other)) == other, "every refresh state reads back");
		}
		for (const auto reason : { discovery_diagnosis_reason::retained, discovery_diagnosis_reason::requested }) {
			auto other   = read;
			other.Reason = reason;
			expect(parse_discovery_diagnosis(describe(other)) == other, "every read reason reads back");
		}

		auto invalid    = read;
		invalid.Refresh = 0U;
		expect(describe(invalid).empty(), "a refresh is numbered from 1");
		invalid      = read;
		invalid.Read = 0U;
		expect(describe(invalid).empty(), "a read is numbered within its refresh");
		invalid        = read;
		invalid.Reason = discovery_diagnosis_reason::none;
		expect(describe(invalid).empty(), "a read has a reason");
		invalid       = read;
		invalid.State = discovery_diagnosis_state::current;
		expect(describe(invalid).empty(), "a read has no refresh state");
		invalid      = refresh;
		invalid.Read = 1U;
		expect(describe(invalid).empty(), "a refresh line has no read ordinal");
		invalid        = refresh;
		invalid.Failed = 31U;
		expect(describe(invalid).empty(), "no more failures than reads");
		invalid       = refresh;
		invalid.Scope = static_cast<discovery_diagnosis_scope>(9U);
		expect(describe(invalid).empty() && name(invalid.Scope).empty(), "an unknown scope has no name and no line");

		for (const std::string_view hostile : {
				 "",
				 "discovery=read",
				 "discovery=read refresh=3 read=7 reason=authored state=none repositories=1 pull_requests=0 reads=0 failed=0 requests=6 wait_ms=120 run_ms=3900 ",
				 "discovery=read refresh=3 read=7 reason=authored state=none repositories=1 pull_requests=0 reads=0 failed=0 requests=6 wait_ms=120 run_ms=3900 host=example.com",
				 "discovery=read refresh=03 read=7 reason=authored state=none repositories=1 pull_requests=0 reads=0 failed=0 requests=6 wait_ms=120 run_ms=3900",
				 "discovery=read refresh=3 read=7 reason=octo-org/private state=none repositories=1 pull_requests=0 reads=0 failed=0 requests=6 wait_ms=120 run_ms=3900",
				 "discovery=read refresh=3 read=7 reason=authored state=none repositories=1 pull_requests=0 reads=0 failed=0 requests=6 wait_ms=120 run_ms=4294967296",
				 "discovery=read  refresh=3 read=7 reason=authored state=none repositories=1 pull_requests=0 reads=0 failed=0 requests=6 wait_ms=120 run_ms=3900",
				 "discovery=read refresh=3 reason=authored read=7 state=none repositories=1 pull_requests=0 reads=0 failed=0 requests=6 wait_ms=120 run_ms=3900",
				 "review_submission=review_not_listed post=not_sent post_class=0 http_class=0 expected=0 found=0 matched=0 candidates=0 pages=0",
			 }) {
			expect(!parse_discovery_diagnosis(hostile), "anything but exactly a line describe writes reads as none");
		}
	}
} // namespace

auto main() -> int
{
	test_diagnosis_line();
	std::cout << "home discovery core tests passed\n";
	return EXIT_SUCCESS;
}
