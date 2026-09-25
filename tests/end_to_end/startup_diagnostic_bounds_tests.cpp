#include "../../src/app/src/startup_diagnostic.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/app/github_application.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
	using bpr::tests::expect;

	void check_capture_bounds()
	{
		using bpr::app::detail::bounded_startup_diagnostic;
		expect(bounded_startup_diagnostic("").empty(), "empty exception text stays empty");
		expect(bounded_startup_diagnostic("unknown storage cause") == "unknown storage cause", "useful unknown cause survives");
		expect(bounded_startup_diagnostic("line\nnext\r\t\x1b[31m\x7f") == "line next   [31m ", "ASCII controls cannot add terminal lines or escapes");
		for (const auto length : { 2047U, 2048U, 2049U, 1024U * 1024U }) {
			const auto source = std::string(length, 'x');
			const auto result = bounded_startup_diagnostic(source.c_str());
			expect(result.size() == std::min(length, 2048U), "captured cause has the exact declared byte bound");
			if (length <= 2048U) {
				expect(result == source, "a cause at the limit is retained without a false truncation marker");
			} else {
				expect(result.ends_with(" [truncated]"), "an over-limit cause explicitly reports truncation");
			}
		}
		expect(bpr::app::detail::startup_failure_cause_text(std::make_exception_ptr(42)) == "an exception that is not a std::exception", "non-standard exceptions keep their fixed diagnosis");
	}

	void check_retained_first_failure()
	{
		const auto cause = std::make_exception_ptr(std::runtime_error{ std::string(1024U * 1024U, 'z') });
		bpr::app::detail::note_startup_failure("synthetic-startup-arm", cause);
		const auto retained = bpr::app::startup_failure_source();
		expect(retained.starts_with("synthetic-startup-arm: "), "the actual composition retains its startup arm");
		expect(retained.size() == std::string_view{ "synthetic-startup-arm: " }.size() + 2048U && retained.ends_with(" [truncated]"), "composition caps before retaining a large exception cause");
		bpr::app::detail::note_startup_failure("later-arm", std::make_exception_ptr(std::runtime_error{ "later cause" }));
		expect(bpr::app::startup_failure_source() == retained, "a later cause does not replace the first failure");
	}
} // namespace

auto main() -> int
try {
	check_capture_bounds();
	check_retained_first_failure();
	std::cout << "startup-diagnostic-bounds:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
