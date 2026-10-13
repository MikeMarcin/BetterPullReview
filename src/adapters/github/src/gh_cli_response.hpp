#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GH_CLI_RESPONSE_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GH_CLI_RESPONSE_HPP

// The reader of what `gh api --include` prints: a status line, headers and
// the body. It takes the captured bytes and nothing else. [[BPR-0970]] fuzzes
// it through this header; the transport includes it and calls the same code.

#include "github_http_helpers.hpp"
#include "github_read_contract.hpp"

#include "bpr/application/github_provider_port.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace bpr::adapters::github::detail {
	/// \brief The most standard output one `gh` invocation may print.
	inline constexpr std::size_t maximum_gh_cli_standard_output_bytes = 4U * 1024U * 1024U;

	[[nodiscard]] inline auto response_header_name(const std::string_view aName) -> bool
	{
		return !aName.empty() && std::ranges::all_of(aName, [](const unsigned char aByte) {
			return (aByte >= 'a' && aByte <= 'z') || (aByte >= 'A' && aByte <= 'Z')
				|| (aByte >= '0' && aByte <= '9') || std::string_view{ "!#$%&'*+-.^_`|~" }.find(static_cast<char>(aByte)) != std::string_view::npos;
		});
	}

	[[nodiscard]] inline auto lowercase_header(std::string aName) -> std::string
	{
		std::ranges::transform(aName, aName.begin(), [](const unsigned char aByte) {
			return static_cast<char>(aByte >= 'A' && aByte <= 'Z' ? aByte - 'A' + 'a' : aByte);
		});
		return aName;
	}

	[[nodiscard]] inline auto selected_response_header(const std::string_view aName) -> bool
	{
		return aName == "content-type" || aName == "link" || aName == "x-ratelimit-limit"
			|| aName == "x-ratelimit-remaining" || aName == "x-ratelimit-reset"
			|| aName == "x-ratelimit-resource" || aName == "retry-after";
	}

	// gh 2.96.0 processResponse prints an LF status line, CRLF headers and
	// a CRLF separator, then copies the uncolored body without HTTP framing.
	[[nodiscard]] inline auto parse_gh_cli_response(const std::string_view aOutput)
		-> std::expected<wire_response, application::github_provider_failure>
	{
		using failure = application::github_provider_failure;
		if (aOutput.size() > maximum_gh_cli_standard_output_bytes) {
			return std::unexpected{ failure::response_limit };
		}
		const auto statusEnd = aOutput.find('\n');
		if (statusEnd == std::string_view::npos || statusEnd > 256U) {
			return std::unexpected{ failure::malformed_response };
		}
		auto status = aOutput.substr(0U, statusEnd);
		if (status.ends_with('\r')) {
			status.remove_suffix(1U);
		}
		if (!(status.starts_with("HTTP/1.0 ") || status.starts_with("HTTP/1.1 ") || status.starts_with("HTTP/2.0 "))
			|| status.size() < 12U || (status.size() > 12U && status[12U] != ' ')
			|| !std::ranges::all_of(status.substr(9U, 3U), [](const char aByte) { return aByte >= '0' && aByte <= '9'; })
			|| !std::ranges::all_of(status, [](const unsigned char aByte) { return aByte >= 0x20U && aByte != 0x7fU; })) {
			return std::unexpected{ failure::malformed_response };
		}
		wire_response response;
		response.Status = (status[9U] - '0') * 100 + (status[10U] - '0') * 10 + status[11U] - '0';
		if (response.Status < 200 || response.Status > 599) {
			return std::unexpected{ failure::malformed_response };
		}
		std::optional<std::string> link;
		std::size_t                position  = statusEnd + 1U;
		bool                       separated = false;
		while (position < aOutput.size()) {
			const auto end = aOutput.find("\r\n", position);
			if (end == std::string_view::npos || end - position > 8U * 1024U || end > 64U * 1024U) {
				return std::unexpected{ failure::malformed_response };
			}
			const auto line = aOutput.substr(position, end - position);
			position        = end + 2U;
			if (line.empty()) {
				separated = true;
				break;
			}
			if (response.Headers.size() == 128U) {
				return std::unexpected{ failure::response_limit };
			}
			const auto colon = line.find(':');
			if (colon == std::string_view::npos || !response_header_name(line.substr(0U, colon))) {
				return std::unexpected{ failure::malformed_response };
			}
			auto value = line.substr(colon + 1U);
			while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
				value.remove_prefix(1U);
			}
			while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
				value.remove_suffix(1U);
			}
			if (!std::ranges::all_of(value, [](const unsigned char aByte) { return (aByte >= 0x20U && aByte != 0x7fU) || aByte == '\t'; })) {
				return std::unexpected{ failure::malformed_response };
			}
			const auto name = lowercase_header(std::string{ line.substr(0U, colon) });
			if ((selected_response_header(name) || name == "content-length") && std::ranges::any_of(response.Headers, [&name](const auto& aHeader) { return lowercase_header(aHeader.Name) == name; })) {
				return std::unexpected{ failure::malformed_response };
			}
			if (name == "link") {
				link = value;
			}
			response.Headers.push_back({ .Name = std::string{ line.substr(0U, colon) }, .Value = std::string{ value } });
		}
		const auto completeness = link_completeness(link);
		if (!separated || !completeness.has_value()) {
			return std::unexpected{ failure::malformed_response };
		}
		response.Completeness = *completeness;
		response.Body         = aOutput.substr(position);
		for (const auto& header : response.Headers) {
			if (lowercase_header(header.Name) != "content-length") {
				continue;
			}
			std::size_t length = 0U;
			const auto  parsed = std::from_chars(header.Value.data(), header.Value.data() + header.Value.size(), length);
			if (parsed.ec != std::errc{ } || parsed.ptr != header.Value.data() + header.Value.size() || length != response.Body.size()) {
				return std::unexpected{ failure::malformed_response };
			}
		}
		// Provider metadata accepts only this projection; other validated wire
		// headers (including Content-Length) remain transport-local.
		std::erase_if(response.Headers, [](const auto& aHeader) {
			return !selected_response_header(lowercase_header(aHeader.Name));
		});
		return response;
	}
}

#endif
