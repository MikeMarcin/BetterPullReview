#include "qualification_build.hpp"
#include "test_expectation.hpp"

#include <cstdlib>
#include <string_view>

// [[BPR-1674]] Negative controls must fail in both ordinary configurations.
auto main(int argc, char** argv) -> int
try {
	bpr::tests::report_qualification_build();
	if (argc != 2) {
		return EXIT_FAILURE;
	}
	const auto mode = std::string_view{ argv[1] };
	if (mode == "caught") {
		try {
			bpr::tests::expect(false, "qualification caught-failure control");
		} catch (...) {
		}
		return EXIT_SUCCESS;
	}
	if (mode == "fail") {
		bpr::tests::expect(false, "qualification expectation control");
	}
	return mode == "pass" ? EXIT_SUCCESS : EXIT_FAILURE;
} catch (...) {
	return bpr::tests::failing_exit();
}
