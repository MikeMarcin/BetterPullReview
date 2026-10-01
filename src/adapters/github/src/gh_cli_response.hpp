#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GH_CLI_RESPONSE_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GH_CLI_RESPONSE_HPP

// The reader of what `gh api --include` prints: a status line, headers and
// the body. It takes the captured bytes and nothing else. [[BPR-0970]] fuzzes
// it through this header; the transport includes it and calls the same code.

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

	[[nodiscard]] inline auto
	parse_link_completeness(const std::optional<std::string>& aLink) noexcept
		-> std::optional<page_completeness>
	{
		if (!aLink.has_value()) {
			return page_completeness::complete;
		}
		if (aLink->empty()) {
			return std::nullopt;
		}

		const auto text           = std::string_view{ *aLink };
		const auto tokenCharacter = [](const char aCharacter) noexcept {
			const auto value = static_cast<unsigned char>(aCharacter);
			return ((value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9')) || aCharacter == '!'
				|| aCharacter == '#' || aCharacter == '$' || aCharacter == '%'
				|| aCharacter == '&' || aCharacter == '\'' || aCharacter == '*'
				|| aCharacter == '+' || aCharacter == '-' || aCharacter == '.'
				|| aCharacter == '^' || aCharacter == '_' || aCharacter == '`'
				|| aCharacter == '|' || aCharacter == '~';
		};
		const auto skipWhitespace = [&text](std::size_t& aPosition) noexcept {
			while (aPosition < text.size()
				   && (text[aPosition] == ' ' || text[aPosition] == '\t')) {
				++aPosition;
			}
		};
		const auto validRelation = [](const std::string_view aValue,
									  bool&                  aHasNext) noexcept {
			if (aValue.empty()) {
				return false;
			}
			std::size_t position = 0U;
			while (position < aValue.size()) {
				const auto separator = aValue.find(' ', position);
				const auto relation  = aValue.substr(position, separator == std::string_view::npos ? aValue.size() - position : separator - position);
				if (relation != "next" && relation != "prev" && relation != "first"
					&& relation != "last") {
					return false;
				}
				aHasNext = aHasNext || relation == "next";
				if (separator == std::string_view::npos) {
					break;
				}
				position = separator + 1U;
				if (position == aValue.size() || aValue[position] == ' ') {
					return false;
				}
			}
			return true;
		};

		bool        hasNext  = false;
		std::size_t position = 0U;
		while (position < text.size()) {
			skipWhitespace(position);
			if (position == text.size() || text[position] != '<') {
				return std::nullopt;
			}
			const auto targetEnd = text.find('>', position + 1U);
			if (targetEnd == std::string_view::npos || targetEnd == position + 1U) {
				return std::nullopt;
			}
			for (auto target = position + 1U; target < targetEnd; ++target) {
				const auto value = static_cast<unsigned char>(text[target]);
				if (value <= 0x20U || value >= 0x7fU || text[target] == '<'
					|| text[target] == '"') {
					return std::nullopt;
				}
			}
			position = targetEnd + 1U;

			bool relationSeen = false;
			while (true) {
				skipWhitespace(position);
				if (position == text.size() || text[position] == ',') {
					break;
				}
				if (text[position] != ';') {
					return std::nullopt;
				}
				++position;
				skipWhitespace(position);
				const auto nameBegin = position;
				while (position < text.size() && tokenCharacter(text[position])) {
					++position;
				}
				if (nameBegin == position) {
					return std::nullopt;
				}
				const auto name = text.substr(nameBegin, position - nameBegin);
				skipWhitespace(position);
				if (position == text.size() || text[position] != '=') {
					return std::nullopt;
				}
				++position;
				skipWhitespace(position);
				const auto quoted = position < text.size() && text[position] == '"';
				if (quoted) {
					++position;
				}
				const auto valueBegin = position;
				if (quoted) {
					while (position < text.size() && text[position] != '"') {
						const auto value = static_cast<unsigned char>(text[position]);
						if (value < 0x20U || value == 0x7fU
							|| text[position] == '\\') {
							return std::nullopt;
						}
						++position;
					}
					if (position == text.size()) {
						return std::nullopt;
					}
				} else {
					while (position < text.size() && tokenCharacter(text[position])) {
						++position;
					}
				}
				if (valueBegin == position) {
					return std::nullopt;
				}
				const auto value = text.substr(valueBegin, position - valueBegin);
				if (quoted) {
					++position;
				}
				if (name == "rel") {
					if (relationSeen || !validRelation(value, hasNext)) {
						return std::nullopt;
					}
					relationSeen = true;
				}
			}
			if (!relationSeen) {
				return std::nullopt;
			}
			if (position == text.size()) {
				break;
			}
			++position;
			if (position == text.size()) {
				return std::nullopt;
			}
		}
		return hasNext ? std::optional{ page_completeness::has_next_page }
					   : std::optional{ page_completeness::complete };
	}

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
		const auto completeness = parse_link_completeness(link);
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
