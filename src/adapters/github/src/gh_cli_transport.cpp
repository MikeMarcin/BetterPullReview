#include "gh_cli_transport.hpp"
#include "github_paged_read.hpp"
#include "github_wire_request_validation.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <filesystem>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

#if !defined(_WIN32)
	#include <unistd.h>
#endif

namespace bpr::adapters::github::detail {
	namespace {
		constexpr std::size_t maximum_environment_value_bytes = 4U * 1024U;
		constexpr std::size_t maximum_proxy_bytes             = 2U * 1024U;
		constexpr std::size_t maximum_endpoint_bytes          = 4U * 1024U;
		constexpr std::size_t maximum_standard_input_bytes    = 256U * 1024U;
		constexpr std::size_t maximum_standard_output_bytes   = 4U * 1024U * 1024U;
		constexpr std::size_t maximum_standard_error_bytes    = 64U * 1024U;
		constexpr auto        process_timeout                 = std::chrono::seconds{ 30 };
		constexpr auto        process_reap_timeout            = std::chrono::seconds{ 2 };

		[[nodiscard]] auto constrain_gh_writes(github_cli_process_request aRequest,
											   const std::string&         aWritableRoot = { })
			-> github_cli_process_request
		{
#if defined(__APPLE__)
			// gh migrates legacy config even for --version and api. Deny writes
			// before exec, including symlink targets and descendants; observing a
			// change afterwards cannot undo a credential/configuration migration.
			std::vector<std::string> arguments{
				"-p",
				"(version 1)(allow default)(deny file-write*)(allow file-write* (literal \"/dev/null\"))",
				std::move(aRequest.Executable),
			};
			if (!aWritableRoot.empty()) {
				arguments[1] += "(allow file-write* (subpath \"" + aWritableRoot + "\"))";
			}
			arguments.insert(arguments.end(),
							 std::make_move_iterator(aRequest.Arguments.begin()),
							 std::make_move_iterator(aRequest.Arguments.end()));
			aRequest.Executable = "/usr/bin/sandbox-exec";
			aRequest.Arguments  = std::move(arguments);
#endif
			static_cast<void>(aWritableRoot);
			return aRequest;
		}

		[[nodiscard]] auto
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

		[[nodiscard]] auto response_header_name(const std::string_view aName) -> bool
		{
			return !aName.empty() && std::ranges::all_of(aName, [](const unsigned char aByte) {
				return (aByte >= 'a' && aByte <= 'z') || (aByte >= 'A' && aByte <= 'Z')
					|| (aByte >= '0' && aByte <= '9') || std::string_view{ "!#$%&'*+-.^_`|~" }.find(static_cast<char>(aByte)) != std::string_view::npos;
			});
		}

		[[nodiscard]] auto lowercase_header(std::string aName) -> std::string
		{
			std::ranges::transform(aName, aName.begin(), [](const unsigned char aByte) {
				return static_cast<char>(aByte >= 'A' && aByte <= 'Z' ? aByte - 'A' + 'a' : aByte);
			});
			return aName;
		}

		[[nodiscard]] auto selected_response_header(const std::string_view aName) -> bool
		{
			return aName == "content-type" || aName == "link" || aName == "x-ratelimit-limit"
				|| aName == "x-ratelimit-remaining" || aName == "x-ratelimit-reset"
				|| aName == "x-ratelimit-resource" || aName == "retry-after";
		}

		// gh 2.96.0 processResponse prints an LF status line, CRLF headers and
		// a CRLF separator, then copies the uncolored body without HTTP framing.
		[[nodiscard]] auto parse_cli_response(const std::string_view aOutput)
			-> std::expected<wire_response, application::github_provider_failure>
		{
			using failure = application::github_provider_failure;
			if (aOutput.size() > maximum_standard_output_bytes) {
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

		[[nodiscard]] auto completed_graphql_failure(const wire_response& aResponse)
			-> application::github_provider_error
		{
			using failure = application::github_provider_failure;
			application::github_provider_error error{
				.Failure                = failure::malformed_response,
				.RetryAfterSeconds      = std::nullopt,
				.RateLimits             = { },
				.RateLimitCause         = std::nullopt,
				.HttpStatus             = static_cast<std::uint16_t>(aResponse.Status),
				.TransportFailureSource = application::diagnostic_transport_failure_source::http_response,
			};
			const auto metadata = parse_github_response_metadata(aResponse);
			if (!metadata || aResponse.Status != 200
				|| aResponse.Completeness != page_completeness::complete
				|| (metadata->RateLimit && metadata->RateLimit->Resource != application::github_rate_limit_resource::graphql)) {
				return error;
			}
			// Reuse the provider's bounded GraphQL error parser. No error prose or
			// partial data enters the failure completion, even for valid-looking data.
			capability_probe_response response{
				.Status        = aResponse.Status,
				.Authenticated = true,
				.ContentType   = "application/json",
				.Body          = aResponse.Body,
			};
			for (const auto& header : aResponse.Headers) {
				const auto name = lowercase_header(header.Name);
				if (name.starts_with("x-ratelimit-") || name == "retry-after") {
					response.RateLimitHeaders[response.RateLimitHeaderCount++] = { .Name = header.Name, .Value = header.Value };
				}
			}
			const auto observation = normalize_capability_probe(response);
			if (metadata->RateLimit) {
				error.RateLimits.Graphql = metadata->RateLimit;
			}
			if (observation.Unavailability == capability_probe_unavailability::response_limit) {
				error.Failure = failure::response_limit;
			} else if (observation.Unavailability == capability_probe_unavailability::rate_limit && metadata->RateLimit) {
				error.Failure           = failure::rate_limit;
				error.RetryAfterSeconds = metadata->RetryAfterSeconds;
				auto cause              = *metadata->RateLimit;
				cause.SecondaryLimited  = cause.Remaining != 0U;
				if (cause.SecondaryLimited && !error.RetryAfterSeconds) {
					error.RetryAfterSeconds = 60U;
				}
				error.RateLimits.Graphql = cause;
				error.RateLimitCause     = cause;
			}
			return error;
		}

		[[nodiscard]] auto process_failure(const github_cli_process_failure aFailure)
			-> application::github_provider_failure
		{
			using failure = application::github_provider_failure;
			switch (aFailure) {
			case github_cli_process_failure::start_failure:         return failure::process_startup;
			case github_cli_process_failure::timeout:               return failure::timeout;
			case github_cli_process_failure::standard_output_limit:
			case github_cli_process_failure::standard_error_limit:  return failure::response_limit;
			default:                                                return failure::unavailable;
			}
		}

		[[nodiscard]] auto path_bytes(const std::filesystem::path& aPath)
			-> std::string
		{
			const auto bytes = aPath.generic_u8string();
			return {
				reinterpret_cast<const char*>(bytes.data()),
				bytes.size(),
			};
		}

		[[nodiscard]] auto safe_environment_value(
			const std::string_view aValue) noexcept -> bool
		{
			return !aValue.empty()
				&& aValue.size() <= maximum_environment_value_bytes
				&& std::ranges::all_of(
					aValue,
					[](const unsigned char aCharacter) {
						return aCharacter >= 0x20U && aCharacter != 0x7fU;
					});
		}

		[[nodiscard]] auto canonical_directory(
			const std::filesystem::path& aCandidate,
			std::filesystem::path&       aResult) -> bool
		{
			if (!aCandidate.is_absolute()) {
				return false;
			}
			std::error_code error;
			if (!std::filesystem::is_directory(aCandidate, error) || error) {
				return false;
			}
			aResult = std::filesystem::canonical(aCandidate, error);
			return !error && aResult.is_absolute();
		}

		[[maybe_unused, nodiscard]] auto canonical_file(
			const std::filesystem::path& aCandidate,
			std::filesystem::path&       aResult) -> bool
		{
			if (!aCandidate.is_absolute()) {
				return false;
			}
			std::error_code error;
			if (!std::filesystem::is_regular_file(aCandidate, error) || error) {
				return false;
			}
			aResult = std::filesystem::canonical(aCandidate, error);
			return !error && aResult.is_absolute();
		}

		[[nodiscard]] auto executable_name(
			const std::filesystem::path& aPath) -> bool
		{
#if defined(_WIN32)
			auto name = aPath.filename().string();
			std::ranges::transform(
				name,
				name.begin(),
				[](const unsigned char character) {
					if (character >= 'A' && character <= 'Z') {
						return static_cast<char>(character - 'A' + 'a');
					}
					return static_cast<char>(character);
				});
			return name == "gh.exe";
#else
			return aPath.filename() == "gh";
#endif
		}

		[[nodiscard]] auto executable_permissions(
			const std::filesystem::path& aPath) -> bool
		{
#if defined(_WIN32)
			return executable_name(aPath);
#else
			return ::access(aPath.c_str(), X_OK) == 0;
#endif
		}

		[[nodiscard]] auto repository_ancestor(
			std::filesystem::path aDirectory) -> bool
		{
			for (;;) {
				std::error_code error;
				const auto      markerExists = std::filesystem::exists(aDirectory / ".git", error);
				if (error || markerExists) {
					return true;
				}
				const auto headPath   = aDirectory / "HEAD";
				const auto headExists = std::filesystem::exists(headPath, error);
				if (error) {
					return true;
				}
				const auto headIsFile = headExists && std::filesystem::is_regular_file(headPath, error);
				if (error) {
					return true;
				}
				const auto objectsPath  = aDirectory / "objects";
				const auto objectsExist = std::filesystem::exists(objectsPath, error);
				if (error) {
					return true;
				}
				const auto objectsAreDirectory = objectsExist
					&& std::filesystem::is_directory(objectsPath, error);
				if (error) {
					return true;
				}
				const auto refsPath  = aDirectory / "refs";
				const auto refsExist = std::filesystem::exists(refsPath, error);
				if (error) {
					return true;
				}
				const auto refsAreDirectory = refsExist && std::filesystem::is_directory(refsPath, error);
				if (
					error
					|| (headIsFile && objectsAreDirectory
						&& refsAreDirectory)) {
					return true;
				}
				const auto parent = aDirectory.parent_path();
				if (parent.empty() || parent == aDirectory) {
					return false;
				}
				aDirectory = parent;
			}
		}

		[[nodiscard]] auto canonical_environment_directory(
			const std::filesystem::path& aCandidate,
			std::string&                 aValue) -> bool
		{
			std::filesystem::path canonical;
			if (!canonical_directory(aCandidate, canonical)) {
				return false;
			}
			aValue = path_bytes(canonical);
			return safe_environment_value(aValue);
		}

		[[maybe_unused, nodiscard]] auto valid_session_bus_address(
			const std::string_view aValue) noexcept -> bool
		{
			return safe_environment_value(aValue)
				&& (aValue.starts_with("unix:path=/")
					|| aValue.starts_with("unix:abstract="))
				&& aValue.find(';') == std::string_view::npos
				&& aValue.find('\\') == std::string_view::npos;
		}

		enum class proxy_validation : std::uint8_t {
			valid,
			invalid,
			credential_bearing,
		};

		[[nodiscard]] auto validate_proxy(
			const std::string_view aValue) noexcept -> proxy_validation
		{
			if (
				aValue.empty() || aValue.size() > maximum_proxy_bytes
				|| !safe_environment_value(aValue)) {
				return proxy_validation::invalid;
			}
			const auto prefix = aValue.starts_with("https://")
				? std::string_view{ "https://" }
				: aValue.starts_with("http://")
				? std::string_view{ "http://" }
				: std::string_view{ };
			if (prefix.empty()) {
				return proxy_validation::invalid;
			}
			const auto authorityAndPath = aValue.substr(prefix.size());
			const auto slash            = authorityAndPath.find('/');
			const auto authority        = authorityAndPath.substr(0U, slash);
			const auto path             = slash == std::string_view::npos
				? std::string_view{ }
				: authorityAndPath.substr(slash);
			if (authority.find('@') != std::string_view::npos) {
				return proxy_validation::credential_bearing;
			}
			if (
				authority.empty() || (!path.empty() && path != "/")
				|| aValue.find('?') != std::string_view::npos
				|| aValue.find('#') != std::string_view::npos
				|| aValue.find('%') != std::string_view::npos
				|| !std::ranges::any_of(
					authority,
					[](const unsigned char aCharacter) {
						return (
								   aCharacter >= 'a' && aCharacter <= 'z')
							|| (aCharacter >= 'A' && aCharacter <= 'Z')
							|| (aCharacter >= '0' && aCharacter <= '9');
					})
				|| !std::ranges::all_of(
					authority,
					[](const unsigned char aCharacter) {
						return (
								   aCharacter >= 'a' && aCharacter <= 'z')
							|| (aCharacter >= 'A' && aCharacter <= 'Z')
							|| (aCharacter >= '0' && aCharacter <= '9')
							|| aCharacter == '.' || aCharacter == '-'
							|| aCharacter == ':' || aCharacter == '['
							|| aCharacter == ']';
					})) {
				return proxy_validation::invalid;
			}
			return proxy_validation::valid;
		}

		[[nodiscard]] auto valid_no_proxy(
			const std::string_view aValue) noexcept -> bool
		{
			if (
				aValue.empty() || aValue.size() > maximum_proxy_bytes
				|| !safe_environment_value(aValue)
				|| !std::ranges::all_of(
					aValue,
					[](const unsigned char aCharacter) {
						return (
								   aCharacter >= 'a' && aCharacter <= 'z')
							|| (aCharacter >= 'A' && aCharacter <= 'Z')
							|| (aCharacter >= '0' && aCharacter <= '9')
							|| aCharacter == '.' || aCharacter == '-'
							|| aCharacter == '_' || aCharacter == '*'
							|| aCharacter == ',' || aCharacter == ':'
							|| aCharacter == '[' || aCharacter == ']';
					})) {
				return false;
			}
			std::size_t begin = 0U;
			while (begin < aValue.size()) {
				const auto end   = aValue.find(',', begin);
				const auto token = aValue.substr(
					begin,
					end == std::string_view::npos
						? aValue.size() - begin
						: end - begin);
				if (token.empty()) {
					return false;
				}
				if (end == std::string_view::npos) {
					return true;
				}
				begin = end + 1U;
			}
			return false;
		}

		auto append_environment(
			std::vector<github_cli_process_environment_entry>& aEnvironment,
			const std::string_view                             aName,
			const std::string_view                             aValue) -> void
		{
			aEnvironment.emplace_back(
				github_cli_process_environment_entry{
					.Name  = std::string{ aName },
					.Value = std::string{ aValue },
				});
		}

		auto append_fixed_environment(
			std::vector<github_cli_process_environment_entry>& aEnvironment,
			const std::string_view                             aSetting) -> void
		{
			const auto separator = aSetting.find('=');
			append_environment(
				aEnvironment,
				aSetting.substr(0U, separator),
				aSetting.substr(separator + 1U));
		}

		[[nodiscard]] auto bounded_environment(
			const std::vector<github_cli_process_environment_entry>& aEnvironment) noexcept -> bool
		{
			if (aEnvironment.size() > github_cli_process_max_environment_count) {
				return false;
			}
			std::size_t total = 0U;
			for (const auto& entry : aEnvironment) {
				const auto windowsEmptyPath = entry.Name == "PATH" && entry.Value.empty();
				if (
					entry.Name.empty()
					|| entry.Name.size()
						> github_cli_process_max_environment_name_bytes
					|| entry.Value.size() > maximum_environment_value_bytes
					|| (!windowsEmptyPath && !safe_environment_value(entry.Value))
					|| entry.Name.size()
						> github_cli_process_max_environment_bytes - total) {
					return false;
				}
				total += entry.Name.size();
				if (
					entry.Value.size()
					> github_cli_process_max_environment_bytes - total) {
					return false;
				}
				total += entry.Value.size();
			}
			return true;
		}

		[[nodiscard]] auto append_platform_environment(
			const gh_cli_platform_environment_facts&           aFacts,
			std::vector<github_cli_process_environment_entry>& aEnvironment)
			-> std::optional<gh_cli_configuration_failure>
		{
#if defined(__APPLE__)
			const auto* platform = std::get_if<gh_cli_macos_environment_facts>(&aFacts);
			if (platform == nullptr) {
				return gh_cli_configuration_failure::irrelevant_platform_facts;
			}
			std::string home;
			if (!canonical_environment_directory(platform->HomeDirectory, home)) {
				return gh_cli_configuration_failure::invalid_platform_environment;
			}
			append_environment(aEnvironment, "HOME", home);
#elif defined(_WIN32)
			const auto* platform = std::get_if<gh_cli_windows_environment_facts>(&aFacts);
			if (platform == nullptr) {
				return gh_cli_configuration_failure::irrelevant_platform_facts;
			}
			std::string profile;
			std::string roaming;
			std::string local;
			std::string system_root;
			if (
				!canonical_environment_directory(
					platform->UserProfileDirectory,
					profile)
				|| !canonical_environment_directory(
					platform->RoamingApplicationDataDirectory,
					roaming)
				|| !canonical_environment_directory(
					platform->LocalApplicationDataDirectory,
					local)
				|| !canonical_environment_directory(
					platform->SystemRootDirectory,
					system_root)) {
				return gh_cli_configuration_failure::invalid_platform_environment;
			}
			append_environment(aEnvironment, "USERPROFILE", profile);
			append_environment(aEnvironment, "APPDATA", roaming);
			append_environment(aEnvironment, "LOCALAPPDATA", local);
			append_environment(aEnvironment, "SystemRoot", system_root);
			append_environment(aEnvironment, "PATH", "");
#elif defined(__linux__)
			const auto* platform = std::get_if<gh_cli_linux_environment_facts>(&aFacts);
			if (platform == nullptr) {
				return gh_cli_configuration_failure::irrelevant_platform_facts;
			}
			std::string home;
			std::string runtime;
			if (
				!canonical_environment_directory(platform->HomeDirectory, home)
				|| !canonical_environment_directory(
					platform->RuntimeDirectory,
					runtime)
				|| !valid_session_bus_address(platform->SessionBusAddress)) {
				return gh_cli_configuration_failure::invalid_platform_environment;
			}
			append_environment(aEnvironment, "HOME", home);
			append_environment(aEnvironment, "XDG_RUNTIME_DIR", runtime);
			append_environment(
				aEnvironment,
				"DBUS_SESSION_BUS_ADDRESS",
				platform->SessionBusAddress);
#else
			static_cast<void>(aFacts);
			static_cast<void>(aEnvironment);
			return gh_cli_configuration_failure::irrelevant_platform_facts;
#endif
			return std::nullopt;
		}

		[[nodiscard]] auto validate_correlation(
			const github_wire_request&           aRequest,
			const validated_github_host_profile& aProfile)
			-> std::optional<gh_cli_invocation_failure>
		{
			const auto validation = validate_github_wire_request(aProfile, aRequest);
			if (validation.has_value()) {
				return std::nullopt;
			}
			switch (validation.error()) {
			case github_wire_request_validation_failure::
				profile_correlation_mismatch:
				return gh_cli_invocation_failure::
					profile_correlation_mismatch;
			case github_wire_request_validation_failure::
				server_contract_mismatch:
				return gh_cli_invocation_failure::server_contract_mismatch;
			case github_wire_request_validation_failure::
				invalid_wire_request:
				return gh_cli_invocation_failure::invalid_wire_request;
			}
			return gh_cli_invocation_failure::profile_correlation_mismatch;
		}

		[[nodiscard]] auto safe_relative_endpoint(
			const std::string_view aEndpoint) noexcept -> bool
		{
			return !aEndpoint.empty() && aEndpoint.size() <= maximum_endpoint_bytes
				&& aEndpoint.front() != '-' && aEndpoint.front() != '@'
				&& aEndpoint.find("://") == std::string_view::npos
				&& aEndpoint.find('{') == std::string_view::npos
				&& aEndpoint.find('}') == std::string_view::npos
				&& aEndpoint.find('\\') == std::string_view::npos
				&& std::ranges::none_of(
					aEndpoint,
					[](const unsigned char aCharacter) {
						return aCharacter <= 0x20U || aCharacter == 0x7fU;
					});
		}

		[[nodiscard]] auto translated_endpoint(
			const validated_github_host_profile& aProfile,
			const wire_request&                  aWire) -> std::optional<std::string>
		{
			std::string endpoint;
			if (aWire.Operation == read_operation::list_review_threads) {
				endpoint = "graphql";
			} else if (
				aProfile.server_contract()
				== application::github_server_contract::github_com_2022_11_28) {
				if (
					aWire.Path.size() < 2U || aWire.Path.front() != '/'
					|| aWire.Path[1U] == '/') {
					return std::nullopt;
				}
				endpoint = aWire.Path.substr(1U);
			} else {
				constexpr auto prefix = std::string_view{ "/api/v3/" };
				if (!aWire.Path.starts_with(prefix)) {
					return std::nullopt;
				}
				endpoint = aWire.Path.substr(prefix.size());
			}
			if (!aWire.Query.empty()) {
				endpoint += '?';
				endpoint += aWire.Query;
			}
			if (!safe_relative_endpoint(endpoint)) {
				return std::nullopt;
			}
			return endpoint;
		}

		[[nodiscard]] auto wire_error(
			const github_wire_request&                 aRequest,
			const application::github_provider_failure aFailure)
			-> github_wire_completion
		{
			return github_wire_error{
				.Correlation = aRequest.Correlation,
				.Error       = application::github_provider_error{
					.Failure           = aFailure,
					.RetryAfterSeconds = std::nullopt,
					.RateLimits        = { },
					.RateLimitCause    = std::nullopt,
				},
			};
		}
	}

	validated_gh_cli_configuration::validated_gh_cli_configuration(
		std::filesystem::path                             aExecutable,
		std::filesystem::path                             aNeutralWorkingDirectory,
		std::vector<github_cli_process_environment_entry> aEnvironment)
		: Executable{ std::move(aExecutable) }
		, NeutralWorkingDirectory{ std::move(aNeutralWorkingDirectory) }
		, Environment{ std::move(aEnvironment) }
	{
	}

	auto validated_gh_cli_configuration::executable() const noexcept
		-> const std::filesystem::path&
	{
		return Executable;
	}

	auto validated_gh_cli_configuration::neutral_working_directory()
		const noexcept -> const std::filesystem::path&
	{
		return NeutralWorkingDirectory;
	}

	auto validate_gh_cli_configuration(
		const gh_cli_configuration_candidate& aCandidate)
		-> gh_cli_configuration_result
	{
		if (!aCandidate.Executable.is_absolute()) {
			return std::unexpected{
				gh_cli_configuration_failure::executable_not_absolute
			};
		}
		std::error_code error;
		if (!std::filesystem::exists(aCandidate.Executable, error) || error) {
			return std::unexpected{
				gh_cli_configuration_failure::executable_not_found
			};
		}
		if (!std::filesystem::is_regular_file(aCandidate.Executable, error) || error) {
			return std::unexpected{
				gh_cli_configuration_failure::executable_not_regular
			};
		}
		auto executable = std::filesystem::canonical(aCandidate.Executable, error);
		if (error) {
			return std::unexpected{
				gh_cli_configuration_failure::executable_not_found
			};
		}
		if (!executable_name(executable)) {
			return std::unexpected{
				gh_cli_configuration_failure::executable_name_mismatch
			};
		}
		if (!executable_permissions(executable)) {
			return std::unexpected{
				gh_cli_configuration_failure::executable_not_executable
			};
		}

		if (!aCandidate.NeutralWorkingDirectory.is_absolute()) {
			return std::unexpected{
				gh_cli_configuration_failure::working_directory_not_absolute
			};
		}
		if (
			!std::filesystem::exists(
				aCandidate.NeutralWorkingDirectory,
				error)
			|| error) {
			return std::unexpected{
				gh_cli_configuration_failure::working_directory_not_found
			};
		}
		if (
			!std::filesystem::is_directory(
				aCandidate.NeutralWorkingDirectory,
				error)
			|| error) {
			return std::unexpected{
				gh_cli_configuration_failure::working_directory_not_directory
			};
		}
		auto workingDirectory = std::filesystem::canonical(
			aCandidate.NeutralWorkingDirectory,
			error);
		if (error) {
			return std::unexpected{
				gh_cli_configuration_failure::working_directory_not_found
			};
		}
		if (repository_ancestor(workingDirectory)) {
			return std::unexpected{
				gh_cli_configuration_failure::working_directory_is_repository
			};
		}

		constexpr std::array hardening{
			std::string_view{ "GH_PROMPT_DISABLED=1" },
			std::string_view{ "GH_NO_UPDATE_NOTIFIER=1" },
			std::string_view{ "GH_NO_EXTENSION_UPDATE_NOTIFIER=1" },
			std::string_view{ "GH_SPINNER_DISABLED=1" },
			std::string_view{ "GH_TELEMETRY=0" },
			std::string_view{ "NO_COLOR=1" },
			std::string_view{ "CLICOLOR=0" },
		};
		std::vector<github_cli_process_environment_entry> environment;
		environment.reserve(16U);
		for (const auto setting : hardening) {
			append_fixed_environment(environment, setting);
		}
		if (const auto platform = append_platform_environment(
				aCandidate.Platform,
				environment);
			platform.has_value()) {
			return std::unexpected{ *platform };
		}

		if (aCandidate.Https.Proxy.has_value()) {
			switch (validate_proxy(*aCandidate.Https.Proxy)) {
			case proxy_validation::valid:
				append_environment(
					environment,
					"HTTPS_PROXY",
					*aCandidate.Https.Proxy);
				break;
			case proxy_validation::credential_bearing:
				return std::unexpected{
					gh_cli_configuration_failure::
						credential_bearing_https_proxy
				};
			case proxy_validation::invalid:
				return std::unexpected{
					gh_cli_configuration_failure::invalid_https_proxy
				};
			}
		}
		if (aCandidate.Https.NoProxy.has_value()) {
			if (!valid_no_proxy(*aCandidate.Https.NoProxy)) {
				return std::unexpected{
					gh_cli_configuration_failure::invalid_no_proxy
				};
			}
			append_environment(
				environment,
				"NO_PROXY",
				*aCandidate.Https.NoProxy);
		}

#if !defined(__linux__)
		if (
			aCandidate.Https.CertificateFile.has_value()
			|| aCandidate.Https.CertificateDirectory.has_value()) {
			return std::unexpected{
				gh_cli_configuration_failure::irrelevant_platform_facts
			};
		}
#else
		if (aCandidate.Https.CertificateFile.has_value()) {
			std::filesystem::path certificate;
			if (!canonical_file(*aCandidate.Https.CertificateFile, certificate)) {
				return std::unexpected{
					gh_cli_configuration_failure::invalid_certificate_file
				};
			}
			append_environment(
				environment,
				"SSL_CERT_FILE",
				path_bytes(certificate));
		}
		if (aCandidate.Https.CertificateDirectory.has_value()) {
			std::filesystem::path certificate_directory;
			if (!canonical_directory(
					*aCandidate.Https.CertificateDirectory,
					certificate_directory)) {
				return std::unexpected{
					gh_cli_configuration_failure::invalid_certificate_directory
				};
			}
			append_environment(
				environment,
				"SSL_CERT_DIR",
				path_bytes(certificate_directory));
		}
#endif

		if (!bounded_environment(environment)) {
			return std::unexpected{
				gh_cli_configuration_failure::environment_limit
			};
		}

		const validated_gh_cli_configuration configuration{
			std::move(executable),
			std::move(workingDirectory),
			std::move(environment),
		};
		return gh_cli_configuration_result{ configuration };
	}

	auto compile_gh_cli_process_request(
		const validated_gh_cli_configuration& aConfiguration,
		const validated_github_host_profile&  aProfile,
		const github_wire_request&            aRequest) -> gh_cli_invocation_result
	{
		if (
			aProfile.authentication_transport()
			!= application::github_authentication_transport::github_cli) {
			return std::unexpected{
				gh_cli_invocation_failure::profile_transport_mismatch
			};
		}
		if (const auto correlation = validate_correlation(aRequest, aProfile);
			correlation.has_value()) {
			return std::unexpected{ *correlation };
		}
		const auto endpoint = translated_endpoint(aProfile, aRequest.Wire);
		if (!endpoint.has_value()) {
			return std::unexpected{ gh_cli_invocation_failure::invalid_route };
		}

		std::vector<std::string> arguments{
			"api",
			*endpoint,
			"--hostname",
			std::string{ aProfile.hostname() },
			"--include",
			"--method",
			aRequest.Wire.Method == http_method::get ? "GET" : "POST",
		};
		for (const auto& header : aRequest.Wire.Headers) {
			arguments.emplace_back("--header");
			arguments.emplace_back(header.Name + ": " + header.Value);
		}
		if (aRequest.Wire.Method == http_method::post) {
			arguments.emplace_back("--input");
			arguments.emplace_back("-");
		}
		if (
			aRequest.Wire.Body.size() > maximum_standard_input_bytes
			|| arguments.size() > 16U) {
			return std::unexpected{ gh_cli_invocation_failure::request_limit };
		}

		return constrain_gh_writes(github_cli_process_request{
			.Executable              = path_bytes(aConfiguration.Executable),
			.Arguments               = std::move(arguments),
			.Environment             = aConfiguration.Environment,
			.WorkingDirectory        = path_bytes(aConfiguration.NeutralWorkingDirectory),
			.StandardInput           = aRequest.Wire.Body,
			.StandardInputLimit      = maximum_standard_input_bytes,
			.StandardOutputLimit     = maximum_standard_output_bytes,
			.StandardErrorLimit      = maximum_standard_error_bytes,
			.Timeout                 = process_timeout,
			.ReapTimeout             = process_reap_timeout,
			.PreserveCompletedOutput = true,
		});
	}

	auto compile_gh_cli_version_request(
		const validated_gh_cli_configuration& aConfiguration)
		-> github_cli_process_request
	{
		return constrain_gh_writes(github_cli_process_request{
			.Executable          = path_bytes(aConfiguration.Executable),
			.Arguments           = { "--version" },
			.Environment         = aConfiguration.Environment,
			.WorkingDirectory    = path_bytes(aConfiguration.NeutralWorkingDirectory),
			.StandardInput       = { },
			.StandardInputLimit  = 1U,
			.StandardOutputLimit = 2048U,
			.StandardErrorLimit  = 2048U,
			.Timeout             = std::chrono::seconds{ 5 },
			.ReapTimeout         = process_reap_timeout,
		});
	}

	auto compile_gh_cli_git_clone_request(
		const validated_gh_cli_configuration&         aConfiguration,
		const validated_github_host_profile&          aProfile,
		const application::git_smart_request_context& aContext, core::git_commit_oid aCommit,
		const std::filesystem::path& aGit, const std::filesystem::path& aRoot,
		std::size_t aIndex) -> std::optional<github_cli_process_request>
	{
#if !defined(__APPLE__)
		static_cast<void>(aConfiguration);
		static_cast<void>(aProfile);
		static_cast<void>(aContext);
		static_cast<void>(aCommit);
		static_cast<void>(aGit);
		static_cast<void>(aRoot);
		static_cast<void>(aIndex);
		return std::nullopt;
#else
		std::error_code error;
		const auto      root = std::filesystem::canonical(aRoot, error);
		const auto      name = aRoot.filename().string();
		if (error || root != aRoot || root.parent_path() != "/private/tmp"
			|| !name.starts_with("bpr-cli-git-") || name.size() > 64U
			|| !std::ranges::all_of(name, [](unsigned char aByte) {
				   return (aByte >= 'a' && aByte <= 'z') || (aByte >= 'A' && aByte <= 'Z')
					   || (aByte >= '0' && aByte <= '9') || aByte == '-';
			   })
			|| aIndex > 1U || !application::valid(aContext) || !aCommit.valid() || aCommit.algorithm() != core::git_hash_algorithm::sha1 || aProfile.authentication_transport() != application::github_authentication_transport::github_cli || aContext.Authority.Profile != aProfile.profile() || aContext.Authority.Host != aProfile.host() || aContext.Authority.ProfileRevision.value() != aProfile.revision().value()) {
			return std::nullopt;
		}
		if (!aGit.is_absolute() || std::filesystem::canonical(aGit, error) != aGit || error
			|| !std::filesystem::is_regular_file(aGit, error) || error || !executable_permissions(aGit)) {
			return std::nullopt;
		}
		auto environment = aConfiguration.Environment;
		append_environment(environment, "PATH", path_bytes(aGit.parent_path()));
		append_fixed_environment(environment, "GIT_CONFIG_NOSYSTEM=1");
		append_fixed_environment(environment, "GIT_CONFIG_GLOBAL=/dev/null");
		append_fixed_environment(environment, "GIT_ATTR_NOSYSTEM=1");
		append_fixed_environment(environment, "GIT_TERMINAL_PROMPT=0");
		append_fixed_environment(environment, "GIT_ALLOW_PROTOCOL=https");
		return constrain_gh_writes(github_cli_process_request{
									   .Executable              = path_bytes(aConfiguration.Executable),
									   .Arguments               = { "repo", "clone", "https://" + std::string{ aProfile.hostname() } + "/" + aContext.Repository.Owner + "/" + aContext.Repository.Name + ".git",
																	path_bytes(root / ("clone-" + std::to_string(aIndex) + ".git")), "--no-upstream", "--",
																	"--bare", "--quiet", "--revision=" + std::string{ core::canonical_hex(aCommit).view() },
																	"--template=" + path_bytes(root / "empty"), "--config", "transfer.unpackLimit=0",
																	"--config", "fetch.unpackLimit=0", "--config", "core.hooksPath=/dev/null",
																	"--config", "core.fsmonitor=false" },
									   .Environment             = std::move(environment),
									   .WorkingDirectory        = path_bytes(aConfiguration.NeutralWorkingDirectory),
									   .StandardInput           = { },
									   .StandardInputLimit      = 1U,
									   .StandardOutputLimit     = 4096U,
									   .StandardErrorLimit      = maximum_standard_error_bytes,
									   .Timeout                 = std::chrono::minutes{ 2 },
									   .ReapTimeout             = process_reap_timeout,
									   .PreserveCompletedOutput = true,
								   },
								   path_bytes(root));
#endif
	}

	gh_cli_transport::gh_cli_transport(
		const validated_github_host_profile&  aProfile,
		const validated_gh_cli_configuration& aConfiguration,
		github_cli_process_port&              aProcess)
		: Profile{ aProfile }
		, Configuration{ aConfiguration }
		, Process{ aProcess }
	{
		for (const auto& entry : Configuration.Environment) {
#if defined(_WIN32)
			if (entry.Name == "APPDATA") {
				ConfigurationDirectory = std::filesystem::path{ entry.Value } / "GitHub CLI";
			}
#else
			if (entry.Name == "HOME") {
				ConfigurationDirectory = std::filesystem::path{ entry.Value } / ".config" / "gh";
			}
#endif
		}
		ConfigurationFiles = observe_configuration_files();
		std::error_code error;
		ExecutableSize = std::filesystem::file_size(Configuration.executable(), error);
		if (error) {
			return;
		}
		ExecutableWriteTime   = std::filesystem::last_write_time(Configuration.executable(), error);
		ConfigurationObserved = !error;
	}

	auto gh_cli_transport::observe_configuration_files() const
		-> std::optional<std::array<configuration_metadata, 3>>
	{
		if (ConfigurationDirectory.empty()) {
			return std::nullopt;
		}
		// Exactly three metadata observations; no directory enumeration or contents.
		// Same-metadata replacement, concurrent changes and keychain changes remain
		// uncontained. This detects ordinary stable changes, not account binding.
		const std::array                      paths{ ConfigurationDirectory, ConfigurationDirectory / "config.yml", ConfigurationDirectory / "hosts.yml" };
		std::array<configuration_metadata, 3> observations{ };
		for (std::size_t index = 0U; index < paths.size(); ++index) {
			std::error_code error;
			const auto      status = std::filesystem::status(paths[index], error);
			if (status.type() == std::filesystem::file_type::not_found
				&& (!error || error == std::errc::no_such_file_or_directory)) {
				continue;
			}
			if (error || (index == 0U ? !std::filesystem::is_directory(status) : !std::filesystem::is_regular_file(status))) {
				return std::nullopt;
			}
			auto& observation     = observations[index];
			observation.Type      = status.type();
			observation.Canonical = std::filesystem::canonical(paths[index], error);
			if (error) {
				return std::nullopt;
			}
			observation.Modified = std::filesystem::last_write_time(paths[index], error);
			if (error) {
				return std::nullopt;
			}
			if (index != 0U) {
				observation.Size = std::filesystem::file_size(paths[index], error);
				if (error) {
					return std::nullopt;
				}
			}
		}
		return observations;
	}

	auto gh_cli_transport::configuration_unchanged() const -> bool
	{
		if (!ConfigurationObserved || !ConfigurationFiles || observe_configuration_files() != ConfigurationFiles) {
			return false;
		}
		std::error_code error;
		const auto      executable = std::filesystem::canonical(Configuration.executable(), error);
		if (error || executable != Configuration.executable() || !executable_permissions(executable)) {
			return false;
		}
		const auto size = std::filesystem::file_size(executable, error);
		if (error || size != ExecutableSize) {
			return false;
		}
		const auto modified = std::filesystem::last_write_time(executable, error);
		if (error || modified != ExecutableWriteTime) {
			return false;
		}
		std::filesystem::path directory;
		return canonical_directory(Configuration.neutral_working_directory(), directory)
			&& directory == Configuration.neutral_working_directory() && !repository_ancestor(directory);
	}

	auto gh_cli_transport::availability_reasons() const noexcept
		-> std::array<gh_cli_availability_reason, 8>
	{
		return {
			gh_cli_availability_reason::
				opt_in_product_scope_in_progress,
			gh_cli_availability_reason::
				executable_identity_unverifiable,
			gh_cli_availability_reason::
				cross_authority_redirect_unverifiable,
			gh_cli_availability_reason::
				ambient_configuration_unverifiable,
			gh_cli_availability_reason::
				credential_identity_binding_unverifiable,
			gh_cli_availability_reason::
				working_directory_identity_unverifiable,
			gh_cli_availability_reason::
				descendant_process_containment_unverifiable,
			gh_cli_availability_reason::
				numeric_process_identity_reuse_unverifiable,
		};
	}

	auto gh_cli_transport::perform(
		const github_wire_request& aRequest,
		const std::stop_token      aStop) -> github_wire_completion
	{
		if (aStop.stop_requested()) {
			return github_wire_cancelled{
				.Correlation = aRequest.Correlation,
			};
		}
		if (!configuration_unchanged()) {
			return wire_error(aRequest, application::github_provider_failure::cli_unavailable);
		}
		const auto invocation = compile_gh_cli_process_request(Configuration, Profile, aRequest);
		if (!invocation.has_value()) {
			return wire_error(
				aRequest,
				application::github_provider_failure::unavailable);
		}
		if (aStop.stop_requested()) {
			return github_wire_cancelled{
				.Correlation = aRequest.Correlation,
			};
		}

		const auto result = Process.run(*invocation, aStop);
		if (aStop.stop_requested() || (!result && result.error() == github_cli_process_failure::cancelled)) {
			return github_wire_cancelled{ .Correlation = aRequest.Correlation };
		}
		if (!configuration_unchanged()) {
			return wire_error(aRequest, application::github_provider_failure::cli_unavailable);
		}
		if (!result) {
			return wire_error(aRequest, process_failure(result.error()));
		}
		auto response = parse_cli_response(result->StandardOutput);
		if (!response) {
			return wire_error(aRequest, response.error());
		}
		// A nonzero 2xx may carry incomplete data; never infer success from JSON.
		if (result->ExitCode != 0 && response->Status < 300) {
			if (aRequest.Correlation.Operation == read_operation::list_review_threads) {
				return github_wire_error{ .Correlation = aRequest.Correlation, .Error = completed_graphql_failure(*response) };
			}
			return wire_error(aRequest, application::github_provider_failure::unavailable);
		}
		return github_wire_success{ .Correlation = aRequest.Correlation, .Response = std::move(*response) };
	}
	auto gh_cli_transport::perform_capability_probe(
		const github_capability_probe_wire_request& aRequest,
		const std::stop_token                       aStop) -> github_capability_probe_wire_completion
	{
		const auto error = [&aRequest](const application::github_provider_failure aFailure) {
			return github_capability_probe_wire_error{
				.Correlation = aRequest.Correlation,
				.Error       = { .Failure = aFailure, .RetryAfterSeconds = std::nullopt, .RateLimits = { }, .RateLimitCause = std::nullopt },
			};
		};
		if (aStop.stop_requested()) {
			return github_capability_probe_wire_cancelled{ .Correlation = aRequest.Correlation };
		}
		if (Profile.authentication_transport() != application::github_authentication_transport::github_cli
			|| !validate_github_capability_probe_wire_request(Profile, aRequest)) {
			return error(application::github_provider_failure::unavailable);
		}
		if (!configuration_unchanged()) {
			return error(application::github_provider_failure::cli_unavailable);
		}
		github_cli_process_request invocation{
			.Executable              = path_bytes(Configuration.Executable),
			.Arguments               = { "api", "graphql", "--hostname", std::string{ Profile.hostname() }, "--include", "--method", "POST" },
			.Environment             = Configuration.Environment,
			.WorkingDirectory        = path_bytes(Configuration.NeutralWorkingDirectory),
			.StandardInput           = aRequest.Wire.Body,
			.StandardInputLimit      = maximum_standard_input_bytes,
			.StandardOutputLimit     = maximum_standard_output_bytes,
			.StandardErrorLimit      = maximum_standard_error_bytes,
			.Timeout                 = process_timeout,
			.ReapTimeout             = process_reap_timeout,
			.PreserveCompletedOutput = true,
		};
		for (const auto& header : aRequest.Wire.Headers) {
			invocation.Arguments.emplace_back("--header");
			invocation.Arguments.emplace_back(header.Name + ": " + header.Value);
		}
		invocation.Arguments.emplace_back("--input");
		invocation.Arguments.emplace_back("-");
		const auto result = Process.run(constrain_gh_writes(std::move(invocation)), aStop);
		if (aStop.stop_requested() || (!result && result.error() == github_cli_process_failure::cancelled)) {
			return github_capability_probe_wire_cancelled{ .Correlation = aRequest.Correlation };
		}
		if (!configuration_unchanged()) {
			return error(application::github_provider_failure::cli_unavailable);
		}
		if (!result) {
			return error(process_failure(result.error()));
		}
		auto parsed = parse_cli_response(result->StandardOutput);
		if (!parsed) {
			return error(parsed.error());
		}
		if (result->ExitCode != 0 && parsed->Status < 300) {
			return github_capability_probe_wire_error{ .Correlation = aRequest.Correlation, .Error = completed_graphql_failure(*parsed) };
		}
		if (parsed->Completeness != page_completeness::complete) {
			return error(application::github_provider_failure::malformed_response);
		}
		capability_probe_response response{ .Status = parsed->Status, .Authenticated = true, .ContentType = { }, .Body = std::move(parsed->Body) };
		for (const auto& header : parsed->Headers) {
			const auto name = lowercase_header(header.Name);
			if (name == "content-type") {
				response.ContentType = header.Value;
			}
			if (name == "x-ratelimit-limit" || name == "x-ratelimit-remaining" || name == "x-ratelimit-reset" || name == "x-ratelimit-resource" || name == "retry-after") {
				response.RateLimitHeaders[response.RateLimitHeaderCount++] = { .Name = header.Name, .Value = header.Value };
			}
		}
		return github_capability_probe_wire_success{ .Correlation = aRequest.Correlation, .Response = std::move(response) };
	}

}
