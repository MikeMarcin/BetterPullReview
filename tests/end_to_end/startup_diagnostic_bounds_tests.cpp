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
		for (const auto* text : { "", "private source canary", "line\nnext\r\t\x1b[31m\x7f" }) {
			expect(bounded_startup_diagnostic(text) == "[omitted:unclassified-exception]", "unclassified exception bytes never enter diagnostics");
		}
		for (const auto length : { 2047U, 2048U, 2049U, 1024U * 1024U }) {
			const auto source = std::string(length, 'x');
			expect(bounded_startup_diagnostic(source.c_str()) == "[omitted:unclassified-exception]", "large exception is omitted before copying");
		}
		expect(bounded_startup_diagnostic("Application data or installed Git is unavailable") == "Application data or installed Git is unavailable", "known public composition cause remains useful");
		expect(bpr::app::detail::startup_failure_cause_text(std::make_exception_ptr(42)) == "an exception that is not a std::exception", "non-standard exceptions keep their fixed diagnosis");
	}

	void check_retained_first_failure()
	{
		const auto cause = std::make_exception_ptr(std::runtime_error{ std::string(1024U * 1024U, 'z') });
		bpr::app::detail::note_startup_failure("synthetic-startup-arm", cause);
		const auto retained = bpr::app::startup_failure_source();
		expect(retained.starts_with("synthetic-startup-arm: "), "the actual composition retains its startup arm");
		expect(retained == "synthetic-startup-arm: [omitted:unclassified-exception]", "composition caps before retaining a large exception cause");
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
