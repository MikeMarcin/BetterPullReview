#include "../support/test_expectation.hpp"
#include "bpr/core/github_read_backoff.hpp"

#include <cstdlib>
#include <iostream>

int main()
try {
	using bpr::core::github_read_backoff;
	using bpr::core::github_read_time;
	using bpr::tests::expect;
	github_read_backoff policy;
	github_read_time    now{ 1000, 100 };
	policy.observe(now, 2, 105);
	expect(policy.remaining_seconds(now) == 5, "primary reset wins over shorter Retry-After");
	policy.observe(now, 8, 105);
	expect(policy.remaining_seconds(now) == 8, "Retry-After wins over earlier reset");
	policy.observe(now, 1, 101);
	expect(policy.remaining_seconds(now) == 8, "out-of-order observations never shorten a deadline");
	policy.succeeded(now);
	expect(policy.remaining_seconds(now) == 8, "concurrent success cannot release cooldown");
	now.UnixSeconds           = 999999;
	now.MonotonicMilliseconds = 8999;
	expect(policy.remaining_seconds(now) == 1, "wall-clock movement cannot release monotonic deadline early");
	now.MonotonicMilliseconds = 9000;
	expect(policy.remaining_seconds(now) == 0, "exact monotonic deadline admits explicit retry");
	policy.succeeded(now);
	for (auto seconds : { 60U, 120U, 240U, 480U, 900U, 900U }) {
		policy.observe(now, std::nullopt, std::nullopt);
		expect(policy.remaining_seconds(now) == seconds, "missing server delay doubles only up to cap");
		now.MonotonicMilliseconds += seconds * 1000;
	}
	policy.succeeded(now);
	policy.observe(now, 0, 1);
	expect(policy.remaining_seconds(now) == 60, "expired/zero server minimum uses reset fallback");
	policy.observe(now, 86400, std::nullopt);
	expect(policy.remaining_seconds(now) == 86400, "fallback cap never truncates a server minimum");
	policy.observe(now, std::nullopt, std::numeric_limits<std::uint64_t>::max());
	expect(policy.remaining_seconds(now) > 86400, "extreme reset saturates without wrapping into admission");
	std::cout << "github-read-backoff:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
