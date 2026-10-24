/// \file
/// \brief Entry point for the read-only MCP context helper.
///
/// \details The whole program is one loop over newline-delimited JSON-RPC 2.0
/// messages on stdin. There is no listener, no outbound socket, no credential,
/// and no filesystem root beyond the frozen snapshot: the executable links
/// `BetterPullReview::Core`, its JSON decoder and the standard library.
/// Executable composition also selects the process allocator (RFC-BPR-0093);
/// no allocator API enters the helper adapter contract.

#include "bpr/adapters/mcp/mcp_context_helper.hpp"

#include "mcp_terminal_diagnostic.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

namespace {
	/// \brief One environment variable's value, or nothing when it is unset.
	///
	/// \details The helper links no Qt, so the portable `qEnvironmentVariable`
	/// idiom the application uses is not available here. MSVC refuses
	/// `std::getenv` under its secure-CRT policy (C4996) and offers
	/// `_dupenv_s`, which allocates a copy the caller frees; every other
	/// toolchain's `std::getenv` returns a pointer into the process
	/// environment that is copied at once (BPR-0519).
	[[nodiscard]] auto environment_value(const char* const aName)
		-> std::optional<std::string>
	{
#if defined(_MSC_VER)
		char*       buffer = nullptr;
		std::size_t size   = 0;
		if (_dupenv_s(&buffer, &size, aName) != 0 || buffer == nullptr) {
			return std::nullopt;
		}
		auto value = std::string{ buffer };
		std::free(buffer);
		return value;
#else
		const char* const value = std::getenv(aName);
		if (value == nullptr) {
			return std::nullopt;
		}
		return std::string{ value };
#endif
	}

	using namespace bpr;

	/// \brief Reads one line, dropping anything past the declared bound.
	///
	/// \details The bound is enforced here so a hostile writer cannot make the
	/// helper allocate without limit before the request is even looked at. A
	/// line that reaches the bound is delivered at exactly the bound, which
	/// `handle_line` refuses as `invalid_request`.
	[[nodiscard]] auto read_bounded_line(std::istream& aInput, std::string& aLine)
		-> bool
	{
		aLine.clear();
		auto character = aInput.get();
		if (character == std::char_traits<char>::eof()) {
			return false;
		}
		while (character != std::char_traits<char>::eof() && character != '\n') {
			if (aLine.size() < adapters::mcp::maximum_mcp_request_bytes) {
				aLine.push_back(static_cast<char>(character));
			}
			character = aInput.get();
		}
		return true;
	}
} // namespace

auto main(const int aArgumentCount, char**) -> int
{
	// Before a single byte of request is read. The parent's environment is
	// whatever the parent had; this process keeps only the allowlist.
	const auto environment = adapters::mcp::reduce_environment_to_allowlist();
	auto       diagnostic  = adapters::mcp::terminal_environment_summary(environment);

	// Closed: this helper takes no option at all, so no host, port, endpoint,
	// or listener can be named to it. The count is reported; the text is not,
	// because a diagnostic that echoes argv is a diagnostic that can print a
	// secret somebody passed by mistake.
	if (aArgumentCount != 1) {
		diagnostic += "mcp-startup-refusal\tunexpected_argument\t" + std::to_string(aArgumentCount - 1) + "\n";
		adapters::mcp::write_terminal_diagnostic(diagnostic);
		return EXIT_FAILURE;
	}

	const auto snapshotRoot = environment_value("BPR_MCP_SNAPSHOT_ROOT");
	const auto pinnedText   = environment_value("BPR_MCP_PINNED_BINDING");
	if (!snapshotRoot || !pinnedText) {
		diagnostic += "mcp-startup-refusal\tmissing_environment\n";
		adapters::mcp::write_terminal_diagnostic(diagnostic);
		return EXIT_FAILURE;
	}
	const auto pinned = adapters::mcp::decode_revision_binding(*pinnedText);
	if (!pinned) {
		diagnostic += "mcp-startup-refusal\tinvalid_pinned_binding\n";
		adapters::mcp::write_terminal_diagnostic(diagnostic);
		return EXIT_FAILURE;
	}

	auto helper = adapters::mcp::mcp_context_helper{
		std::filesystem::path{ *snapshotRoot },
		*pinned,
	};
	diagnostic += "mcp-ready\n";
	adapters::mcp::write_terminal_diagnostic(diagnostic);

	auto line = std::string{ };
	while (read_bounded_line(std::cin, line)) {
		const auto response = helper.handle_line(line);
		if (response) {
			std::cout << *response << '\n';
			std::cout.flush();
		}
	}
	return EXIT_SUCCESS;
}
