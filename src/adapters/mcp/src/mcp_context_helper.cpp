/// \file
/// \brief Read-only MCP context helper over one frozen review projection.

#include "bpr/adapters/mcp/mcp_context_helper.hpp"

#include "bpr/core/epoch_model.hpp"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <expected>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>

#if defined(_WIN32)
	#include <stdlib.h>

	#define BPR_MCP_ENVIRON _environ
#else
extern "C" char** environ;

	#define BPR_MCP_ENVIRON environ
#endif

namespace bpr::adapters::mcp {
	namespace {
		struct json_member;

		/// \brief One decoded JSON value, bounded in depth by the reader.
		struct json_node
		{
			enum class kind : std::uint8_t {
				null,
				boolean,
				number,
				string,
				array,
				object,
			};

			kind                     Kind    = kind::null;
			bool                     Boolean = false;
			double                   Number  = 0.0;
			std::string              Text;
			std::vector<json_node>   Array;
			std::vector<json_member> Object;

			[[nodiscard]] auto member(std::string_view aName) const
				-> const json_node*;
		};

		/// [[BPR-1260]]. A named member rather than `std::pair`: the pair's
		/// default constructor asks whether `json_node` is implicitly default
		/// constructible while `json_node` is still being defined, which Clang
		/// with libstdc++ 15 refuses as an incomplete type.
		struct json_member
		{
			std::string Name;
			json_node   Value;
		};

		auto json_node::member(const std::string_view aName) const
			-> const json_node*
		{
			for (const auto& entry : Object) {
				if (entry.Name == aName) {
					return &entry.Value;
				}
			}
			return nullptr;
		}

		/// \brief Decodes one bounded JSON document with no external library.
		class json_reader final
		{
		public:
			explicit json_reader(const std::string_view aText) noexcept
				: Text{ aText }
			{
			}

			[[nodiscard]] auto parse() -> std::optional<json_node>
			{
				skip_whitespace();
				auto value = parse_value(0U);
				if (!value) {
					return std::nullopt;
				}
				skip_whitespace();
				if (Index != Text.size()) {
					return std::nullopt;
				}
				return value;
			}

		private:
			void skip_whitespace() noexcept
			{
				while (Index < Text.size()) {
					const auto character = Text[Index];
					if (character != ' ' && character != '\t' && character != '\n'
						&& character != '\r') {
						return;
					}
					++Index;
				}
			}

			[[nodiscard]] auto peek() const noexcept -> char
			{
				return Index < Text.size() ? Text[Index] : '\0';
			}

			[[nodiscard]] auto consume(const std::string_view aLiteral) noexcept
				-> bool
			{
				if (Text.substr(Index).starts_with(aLiteral)) {
					Index += aLiteral.size();
					return true;
				}
				return false;
			}

			[[nodiscard]] auto parse_value(const std::size_t aDepth)
				-> std::optional<json_node>
			{
				if (aDepth > maximum_mcp_json_depth) {
					return std::nullopt;
				}
				skip_whitespace();
				switch (peek()) {
				case 'n':
					return consume("null") ? std::optional{ json_node{ } }
										   : std::nullopt;
				case 't':
					return consume("true")
						? std::optional{ json_node{
							  .Kind    = json_node::kind::boolean,
							  .Boolean = true } }
						: std::nullopt;
				case 'f':
					return consume("false")
						? std::optional{ json_node{
							  .Kind    = json_node::kind::boolean,
							  .Boolean = false } }
						: std::nullopt;
				case '"':
					return parse_string_value();
				case '[':
					return parse_array(aDepth);
				case '{':
					return parse_object(aDepth);
				default:
					return parse_number();
				}
			}

			[[nodiscard]] auto parse_string_value() -> std::optional<json_node>
			{
				auto text = parse_string();
				if (!text) {
					return std::nullopt;
				}
				return json_node{
					.Kind = json_node::kind::string,
					.Text = std::move(*text),
				};
			}

			[[nodiscard]] auto parse_hex_quad() -> std::optional<std::uint32_t>
			{
				if (Index + 4U > Text.size()) {
					return std::nullopt;
				}
				auto value = std::uint32_t{ 0U };
				for (auto position = std::size_t{ 0U }; position < 4U; ++position) {
					const auto character = Text[Index + position];
					auto       digit     = std::uint32_t{ 0U };
					if (character >= '0' && character <= '9') {
						digit = static_cast<std::uint32_t>(character - '0');
					} else if (character >= 'a' && character <= 'f') {
						digit = static_cast<std::uint32_t>(character - 'a') + 10U;
					} else if (character >= 'A' && character <= 'F') {
						digit = static_cast<std::uint32_t>(character - 'A') + 10U;
					} else {
						return std::nullopt;
					}
					value = (value << 4U) | digit;
				}
				Index += 4U;
				return value;
			}

			static void append_code_point(
				std::string& aOutput, const std::uint32_t aCodePoint)
			{
				if (aCodePoint < 0x80U) {
					aOutput.push_back(static_cast<char>(aCodePoint));
				} else if (aCodePoint < 0x800U) {
					aOutput.push_back(
						static_cast<char>(0xC0U | (aCodePoint >> 6U)));
					aOutput.push_back(
						static_cast<char>(0x80U | (aCodePoint & 0x3FU)));
				} else if (aCodePoint < 0x10000U) {
					aOutput.push_back(
						static_cast<char>(0xE0U | (aCodePoint >> 12U)));
					aOutput.push_back(
						static_cast<char>(0x80U | ((aCodePoint >> 6U) & 0x3FU)));
					aOutput.push_back(
						static_cast<char>(0x80U | (aCodePoint & 0x3FU)));
				} else {
					aOutput.push_back(
						static_cast<char>(0xF0U | (aCodePoint >> 18U)));
					aOutput.push_back(
						static_cast<char>(0x80U | ((aCodePoint >> 12U) & 0x3FU)));
					aOutput.push_back(
						static_cast<char>(0x80U | ((aCodePoint >> 6U) & 0x3FU)));
					aOutput.push_back(
						static_cast<char>(0x80U | (aCodePoint & 0x3FU)));
				}
			}

			[[nodiscard]] auto parse_string() -> std::optional<std::string>
			{
				if (peek() != '"') {
					return std::nullopt;
				}
				++Index;
				auto output = std::string{ };
				while (Index < Text.size()) {
					const auto character = Text[Index];
					++Index;
					if (character == '"') {
						return output;
					}
					if (character != '\\') {
						output.push_back(character);
						continue;
					}
					if (Index >= Text.size()) {
						return std::nullopt;
					}
					const auto escape = Text[Index];
					++Index;
					switch (escape) {
					case '"':
						output.push_back('"');
						break;
					case '\\':
						output.push_back('\\');
						break;
					case '/':
						output.push_back('/');
						break;
					case 'b':
						output.push_back('\b');
						break;
					case 'f':
						output.push_back('\f');
						break;
					case 'n':
						output.push_back('\n');
						break;
					case 'r':
						output.push_back('\r');
						break;
					case 't':
						output.push_back('\t');
						break;
					case 'u': {
						const auto high = parse_hex_quad();
						if (!high) {
							return std::nullopt;
						}
						auto codePoint = *high;
						if (codePoint >= 0xD800U && codePoint <= 0xDBFFU) {
							if (!consume("\\u")) {
								return std::nullopt;
							}
							const auto low = parse_hex_quad();
							if (!low || *low < 0xDC00U || *low > 0xDFFFU) {
								return std::nullopt;
							}
							codePoint = 0x10000U
								+ ((codePoint - 0xD800U) << 10U)
								+ (*low - 0xDC00U);
						}
						append_code_point(output, codePoint);
						break;
					}
					default:
						return std::nullopt;
					}
				}
				return std::nullopt;
			}

			[[nodiscard]] auto parse_number() -> std::optional<json_node>
			{
				const auto begin = Index;
				if (peek() == '-') {
					++Index;
				}
				while (Index < Text.size()
					   && ((Text[Index] >= '0' && Text[Index] <= '9')
						   || Text[Index] == '.' || Text[Index] == 'e'
						   || Text[Index] == 'E' || Text[Index] == '+'
						   || Text[Index] == '-')) {
					++Index;
				}
				if (Index == begin) {
					return std::nullopt;
				}
				const auto token = std::string{ Text.substr(begin, Index - begin) };
				auto       value = 0.0;
				try {
					std::size_t consumed = 0U;
					value                = std::stod(token, &consumed);
					if (consumed != token.size()) {
						return std::nullopt;
					}
				} catch (...) {
					return std::nullopt;
				}
				return json_node{
					.Kind   = json_node::kind::number,
					.Number = value,
				};
			}

			[[nodiscard]] auto parse_array(const std::size_t aDepth)
				-> std::optional<json_node>
			{
				++Index;
				auto node = json_node{ .Kind = json_node::kind::array };
				skip_whitespace();
				if (peek() == ']') {
					++Index;
					return node;
				}
				while (true) {
					auto element = parse_value(aDepth + 1U);
					if (!element) {
						return std::nullopt;
					}
					node.Array.push_back(std::move(*element));
					skip_whitespace();
					if (peek() == ',') {
						++Index;
						continue;
					}
					if (peek() == ']') {
						++Index;
						return node;
					}
					return std::nullopt;
				}
			}

			[[nodiscard]] auto parse_object(const std::size_t aDepth)
				-> std::optional<json_node>
			{
				++Index;
				auto node = json_node{ .Kind = json_node::kind::object };
				skip_whitespace();
				if (peek() == '}') {
					++Index;
					return node;
				}
				while (true) {
					skip_whitespace();
					auto key = parse_string();
					if (!key) {
						return std::nullopt;
					}
					skip_whitespace();
					if (peek() != ':') {
						return std::nullopt;
					}
					++Index;
					auto value = parse_value(aDepth + 1U);
					if (!value) {
						return std::nullopt;
					}
					node.Object.push_back(json_member{ .Name = std::move(*key), .Value = std::move(*value) });
					skip_whitespace();
					if (peek() == ',') {
						++Index;
						continue;
					}
					if (peek() == '}') {
						++Index;
						return node;
					}
					return std::nullopt;
				}
			}

			std::string_view Text;
			std::size_t      Index = 0U;
		};

		[[nodiscard]] auto json_escape(const std::string_view aValue) -> std::string
		{
			constexpr std::string_view hex    = "0123456789abcdef";
			auto                       output = std::string{ };
			output.reserve(aValue.size() + 2U);
			for (const auto character : aValue) {
				const auto byte = static_cast<unsigned char>(character);
				switch (character) {
				case '"':
					output += "\\\"";
					break;
				case '\\':
					output += "\\\\";
					break;
				case '\n':
					output += "\\n";
					break;
				case '\r':
					output += "\\r";
					break;
				case '\t':
					output += "\\t";
					break;
				default:
					if (byte < 0x20U) {
						output += "\\u00";
						output.push_back(hex[byte >> 4U]);
						output.push_back(hex[byte & 0x0FU]);
					} else {
						output.push_back(character);
					}
					break;
				}
			}
			return output;
		}

		[[nodiscard]] auto json_string(const std::string_view aValue) -> std::string
		{
			return std::string{ "\"" } + json_escape(aValue) + "\"";
		}

		/// \brief Echoes one JSON-RPC identity exactly as the caller sent it.
		[[nodiscard]] auto render_identity(const json_node& aId)
			-> std::optional<std::string>
		{
			switch (aId.Kind) {
			case json_node::kind::null:
				return std::string{ "null" };
			case json_node::kind::string:
				return json_string(aId.Text);
			case json_node::kind::number: {
				// [[BPR-0970]]. Converting a double outside the 64-bit range is
				// undefined, so such an identity is refused before the cast.
				// Both bounds are powers of two and exact as doubles.
				if (!(aId.Number >= -9'223'372'036'854'775'808.0 && aId.Number < 9'223'372'036'854'775'808.0)) {
					return std::nullopt;
				}
				const auto integral = static_cast<std::int64_t>(aId.Number);
				if (static_cast<double>(integral) != aId.Number) {
					return std::nullopt;
				}
				return std::to_string(integral);
			}
			case json_node::kind::boolean:
			case json_node::kind::array:
			case json_node::kind::object:
				return std::nullopt;
			}
			return std::nullopt;
		}

		[[nodiscard]] auto refusal_code(const mcp_refusal aRefusal) noexcept -> int
		{
			switch (aRefusal) {
			case mcp_refusal::invalid_request:
				return -32600;
			case mcp_refusal::unknown_method:
				return -32601;
			case mcp_refusal::unknown_tool:
			case mcp_refusal::invalid_arguments:
			case mcp_refusal::network_location_refused:
			case mcp_refusal::path_outside_snapshot:
			case mcp_refusal::snapshot_entry_missing:
			case mcp_refusal::snapshot_entry_too_large:
				return -32602;
			case mcp_refusal::snapshot_unavailable:
				return -32001;
			case mcp_refusal::revision_binding_stale:
				return -32002;
			}
			return -32603;
		}

		[[nodiscard]] auto result_response(
			const std::string_view aId, const std::string_view aResult)
			-> std::string
		{
			return std::string{ R"({"jsonrpc":"2.0","id":)" } + std::string{ aId }
			+ R"(,"result":)" + std::string{ aResult } + "}";
		}

		[[nodiscard]] auto error_response(
			const std::string_view aId, const mcp_refusal aRefusal) -> std::string
		{
			const auto refusalName = std::string{ name(aRefusal) };
			return std::string{ R"({"jsonrpc":"2.0","id":)" } + std::string{ aId }
			+ R"(,"error":{"code":)"
				+ std::to_string(refusal_code(aRefusal)) + R"(,"message":)"
				+ json_string(refusalName) + R"(,"data":{"refusal":)"
				+ json_string(refusalName) + "}}}";
		}

		[[nodiscard]] auto text_content(const std::string_view aText) -> std::string
		{
			return std::string{ R"({"content":[{"type":"text","text":)" }
			+ json_string(aText) + R"(}],"isError":false})";
		}

		[[nodiscard]] auto decode_hex_digit(const char aCharacter) noexcept
			-> std::optional<std::uint8_t>
		{
			if (aCharacter >= '0' && aCharacter <= '9') {
				return static_cast<std::uint8_t>(aCharacter - '0');
			}
			if (aCharacter >= 'a' && aCharacter <= 'f') {
				return static_cast<std::uint8_t>(aCharacter - 'a' + 10);
			}
			return std::nullopt;
		}

		[[nodiscard]] auto decode_identity(const std::string_view aText)
			-> std::optional<std::array<std::uint8_t, 32U>>
		{
			if (aText.size() != 64U) {
				return std::nullopt;
			}
			auto bytes = std::array<std::uint8_t, 32U>{ };
			for (auto index = std::size_t{ 0U }; index < 32U; ++index) {
				const auto high = decode_hex_digit(aText[index * 2U]);
				const auto low  = decode_hex_digit(aText[(index * 2U) + 1U]);
				if (!high || !low) {
					return std::nullopt;
				}
				bytes[index] = static_cast<std::uint8_t>(
					(static_cast<unsigned>(*high) << 4U)
					| static_cast<unsigned>(*low));
			}
			return bytes;
		}

		[[nodiscard]] auto encode_identity(
			const std::array<std::uint8_t, 32U>& aBytes) -> std::string
		{
			constexpr std::string_view hex    = "0123456789abcdef";
			auto                       output = std::string{ };
			output.reserve(64U);
			for (const auto byte : aBytes) {
				output.push_back(hex[byte >> 4U]);
				output.push_back(hex[byte & 0x0FU]);
			}
			return output;
		}

		[[nodiscard]] auto decode_unsigned(const std::string_view aText)
			-> std::optional<std::uint64_t>
		{
			if (aText.empty()) {
				return std::nullopt;
			}
			auto       value  = std::uint64_t{ 0U };
			const auto result = std::from_chars(
				aText.data(), aText.data() + aText.size(), value);
			if (result.ec != std::errc{ }
				|| result.ptr != aText.data() + aText.size()) {
				return std::nullopt;
			}
			return value;
		}

		[[nodiscard]] auto split(
			const std::string_view aText, const char aSeparator)
			-> std::vector<std::string_view>
		{
			auto parts    = std::vector<std::string_view>{ };
			auto position = std::size_t{ 0U };
			while (true) {
				const auto next = aText.find(aSeparator, position);
				if (next == std::string_view::npos) {
					parts.push_back(aText.substr(position));
					return parts;
				}
				parts.push_back(aText.substr(position, next - position));
				position = next + 1U;
			}
		}

		[[nodiscard]] auto read_text_file(
			const std::filesystem::path& aPath, const std::size_t aLimit)
			-> std::optional<std::string>
		{
			auto input = std::ifstream{ aPath, std::ios::binary };
			if (!input) {
				return std::nullopt;
			}
			auto output = std::string{ };
			auto buffer = std::array<char, 8192U>{ };
			while (input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()))
				   || input.gcount() > 0) {
				const auto read = static_cast<std::size_t>(input.gcount());
				if (output.size() + read > aLimit) {
					return std::nullopt;
				}
				output.append(buffer.data(), read);
			}
			return output;
		}

		/// \brief Whether `aCandidate` is `aRoot` or lies inside it.
		///
		/// \details Compared component by component. A textual prefix test would
		/// let `/snapshot-elsewhere` pass for `/snapshot`.
		[[nodiscard]] auto is_within(
			const std::filesystem::path& aRoot,
			const std::filesystem::path& aCandidate) -> bool
		{
			auto rootPart      = aRoot.begin();
			auto candidatePart = aCandidate.begin();
			for (; rootPart != aRoot.end(); ++rootPart, ++candidatePart) {
				if (candidatePart == aCandidate.end()
					|| *candidatePart != *rootPart) {
					return false;
				}
			}
			return true;
		}

		/// \brief Whether the observed binding is not the pinned one.
		[[nodiscard]] auto binding_moved(
			const core::agent_revision_binding& aPinned,
			const core::agent_revision_binding& aObserved) noexcept -> bool
		{
			return aPinned != aObserved;
		}

		[[nodiscard]] auto files_root(const std::filesystem::path& aSnapshotRoot)
			-> std::filesystem::path
		{
			return aSnapshotRoot / "files";
		}

		/// \brief Resolves one requested path, or refuses it by exact cause.
		[[nodiscard]] auto resolve_snapshot_path(
			const std::filesystem::path& aSnapshotRoot,
			const std::string_view       aPath)
			-> std::expected<std::filesystem::path, mcp_refusal>
		{
			if (aPath.empty() || aPath.size() > 4096U) {
				return std::unexpected{ mcp_refusal::invalid_arguments };
			}
			if (names_uri_scheme(aPath) || names_network_authority(aPath)) {
				return std::unexpected{ mcp_refusal::network_location_refused };
			}

			auto       filesystemError = std::error_code{ };
			const auto root            = std::filesystem::weakly_canonical(
				files_root(aSnapshotRoot), filesystemError);
			if (filesystemError) {
				return std::unexpected{ mcp_refusal::snapshot_unavailable };
			}
			auto resolved = std::filesystem::weakly_canonical(
				root / std::filesystem::path{ aPath }, filesystemError);
			if (filesystemError) {
				return std::unexpected{ mcp_refusal::path_outside_snapshot };
			}
			if (!is_within(root, resolved)) {
				return std::unexpected{ mcp_refusal::path_outside_snapshot };
			}
			return resolved;
		}

		[[nodiscard]] auto environment_names() -> std::vector<std::string>
		{
			auto names = std::vector<std::string>{ };
			for (auto** entry = BPR_MCP_ENVIRON;
				 entry != nullptr && *entry != nullptr;
				 ++entry) {
				const auto entryText = std::string_view{ *entry };
				names.emplace_back(entryText.substr(0U, entryText.find('=')));
			}
			return names;
		}

		[[nodiscard]] auto allowlisted(const std::string_view aName) noexcept -> bool
		{
			return std::ranges::find(mcp_environment_allowlist, aName)
				!= mcp_environment_allowlist.end();
		}

		void remove_environment_variable(const std::string& aName)
		{
#if defined(_WIN32)
			static_cast<void>(::_putenv_s(aName.c_str(), ""));
#else
			static_cast<void>(::unsetenv(aName.c_str()));
#endif
		}
	} // namespace

	auto name(const mcp_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case mcp_refusal::invalid_request:
			return "invalid_request";
		case mcp_refusal::unknown_method:
			return "unknown_method";
		case mcp_refusal::unknown_tool:
			return "unknown_tool";
		case mcp_refusal::invalid_arguments:
			return "invalid_arguments";
		case mcp_refusal::network_location_refused:
			return "network_location_refused";
		case mcp_refusal::path_outside_snapshot:
			return "path_outside_snapshot";
		case mcp_refusal::snapshot_entry_missing:
			return "snapshot_entry_missing";
		case mcp_refusal::snapshot_entry_too_large:
			return "snapshot_entry_too_large";
		case mcp_refusal::revision_binding_stale:
			return "revision_binding_stale";
		case mcp_refusal::snapshot_unavailable:
			return "snapshot_unavailable";
		}
		return "invalid_request";
	}

	auto names_uri_scheme(const std::string_view aPath) noexcept -> bool
	{
		return aPath.find("://") != std::string_view::npos;
	}

	auto names_network_authority(const std::string_view aPath) noexcept -> bool
	{
		return aPath.starts_with("//") || aPath.starts_with("\\\\");
	}

	auto encode_revision_binding(const core::agent_revision_binding& aBinding)
		-> std::string
	{
		return std::to_string(aBinding.Host.value()) + "/"
			+ std::to_string(aBinding.Repository.value()) + "/"
			+ std::to_string(aBinding.PullRequest.value()) + "/"
			+ std::string{ core::canonical_hex(aBinding.BaseOid).view() } + "/"
			+ std::string{ core::canonical_hex(aBinding.HeadOid).view() } + "/"
			+ std::to_string(aBinding.LeftEpoch.value()) + "/"
			+ std::to_string(aBinding.RightEpoch.value());
	}

	auto decode_revision_binding(const std::string_view aText)
		-> std::optional<core::agent_revision_binding>
	{
		const auto parts = split(aText, '/');
		if (parts.size() != 7U) {
			return std::nullopt;
		}
		const auto host        = decode_unsigned(parts[0]);
		const auto repository  = decode_unsigned(parts[1]);
		const auto pullRequest = decode_unsigned(parts[2]);
		const auto base        = core::parse_git_commit_oid(parts[3]);
		const auto head        = core::parse_git_commit_oid(parts[4]);
		const auto leftEpoch   = decode_unsigned(parts[5]);
		const auto rightEpoch  = decode_unsigned(parts[6]);
		if (!host || !repository || !pullRequest || !base || !head || !leftEpoch
			|| !rightEpoch) {
			return std::nullopt;
		}
		return core::agent_revision_binding{
			.Host        = core::git_host_id{ *host },
			.Repository  = core::repository_id{ *repository },
			.PullRequest = core::pull_request_id{ *pullRequest },
			.BaseOid     = *base,
			.HeadOid     = *head,
			.LeftEpoch
			= core::epoch_ordinal{ static_cast<std::int64_t>(*leftEpoch) },
			.RightEpoch
			= core::epoch_ordinal{ static_cast<std::int64_t>(*rightEpoch) },
		};
	}

	auto reduce_environment_to_allowlist() -> mcp_environment_report
	{
		auto       report    = mcp_environment_report{ };
		const auto received  = environment_names();
		report.ReceivedCount = received.size();
		for (const auto& variableName : received) {
			if (!allowlisted(variableName)) {
				remove_environment_variable(variableName);
			}
		}
		const auto retained            = environment_names();
		report.RetainedCount           = retained.size();
		report.UnexpectedRetainedCount = static_cast<std::size_t>(
			std::ranges::count_if(retained, [](const auto& aName) { return !allowlisted(aName); }));
		for (auto index = std::size_t{ 0U }; index < mcp_environment_allowlist.size(); ++index) {
			const auto name                   = mcp_environment_allowlist[index];
			report.ReceivedAllowlisted[index] = std::ranges::find(received, name) != received.end();
			report.RetainedAllowlisted[index] = std::ranges::find(retained, name) != retained.end();
		}
		return report;
	}

	auto read_snapshot_alignment(const std::filesystem::path& aSnapshotRoot)
		-> std::optional<std::vector<core::agent_finding_snapshot_file>>
	{
		const auto text = read_text_file(
			aSnapshotRoot / "alignment", maximum_mcp_snapshot_entry_bytes);
		if (!text) {
			return std::nullopt;
		}
		auto files = std::vector<core::agent_finding_snapshot_file>{ };
		for (const auto line : split(*text, '\n')) {
			if (line.empty()) {
				continue;
			}
			const auto separator = line.find('\t');
			if (separator == std::string_view::npos) {
				return std::nullopt;
			}
			const auto field = line.substr(0U, separator);
			const auto value = line.substr(separator + 1U);
			if (field == "file") {
				if (files.size() >= maximum_mcp_snapshot_files) {
					return std::nullopt;
				}
				files.push_back({ .Path = std::string{ value } });
				continue;
			}
			if (files.empty()) {
				return std::nullopt;
			}
			auto& current = files.back();
			if (field == "comparison") {
				const auto identity = decode_identity(value);
				if (!identity) {
					return std::nullopt;
				}
				current.Comparison = core::file_comparison_digest{ *identity };
			} else if (field == "left_line_count" || field == "right_line_count") {
				const auto count = decode_unsigned(value);
				if (!count || *count > 0xFFFFFFFFU) {
					return std::nullopt;
				}
				auto& target = field == "left_line_count" ? current.LeftLineCount
														  : current.RightLineCount;
				target       = static_cast<std::uint32_t>(*count);
			} else if (field == "segment") {
				const auto identity = decode_identity(value);
				if (!identity) {
					return std::nullopt;
				}
				current.Segments.emplace_back(*identity);
			} else if (field == "group") {
				const auto identity = decode_identity(value);
				if (!identity) {
					return std::nullopt;
				}
				current.Groups.emplace_back(*identity);
			} else if (field == "move") {
				const auto identity = decode_identity(value);
				if (!identity) {
					return std::nullopt;
				}
				current.Moves.emplace_back(*identity);
			} else {
				return std::nullopt;
			}
		}
		return files;
	}

	mcp_context_helper::mcp_context_helper(
		std::filesystem::path aSnapshotRoot, core::agent_revision_binding aPinnedBinding)
		: SnapshotRoot{ std::move(aSnapshotRoot) }
		, Pinned{ aPinnedBinding }
	{
	}

	auto mcp_context_helper::pinned_binding() const noexcept
		-> const core::agent_revision_binding&
	{
		return Pinned;
	}

	auto mcp_context_helper::handle_line(const std::string_view aLine)
		-> std::optional<std::string>
	{
		auto line = aLine;
		while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
			line.remove_suffix(1U);
		}
		if (line.empty()) {
			return std::nullopt;
		}
		if (line.size() >= maximum_mcp_request_bytes) {
			return error_response("null", mcp_refusal::invalid_request);
		}

		const auto document = json_reader{ line }.parse();
		if (!document || document->Kind != json_node::kind::object) {
			return error_response("null", mcp_refusal::invalid_request);
		}

		const auto* const version = document->member("jsonrpc");
		if (version == nullptr || version->Kind != json_node::kind::string
			|| version->Text != "2.0") {
			return error_response("null", mcp_refusal::invalid_request);
		}

		const auto* const identity   = document->member("id");
		auto              renderedId = std::optional<std::string>{ };
		if (identity != nullptr) {
			renderedId = render_identity(*identity);
			if (!renderedId) {
				return error_response("null", mcp_refusal::invalid_request);
			}
		}

		const auto* const method = document->member("method");
		if (method == nullptr || method->Kind != json_node::kind::string) {
			return renderedId
				? std::optional{ error_response(
					  *renderedId, mcp_refusal::invalid_request) }
				: std::nullopt;
		}

		// A notification carries no identity, so it has no answer to carry one.
		if (!renderedId) {
			return std::nullopt;
		}
		const auto& responseId = *renderedId;

		if (method->Text == "initialize") {
			return result_response(
				responseId,
				std::string{ R"({"protocolVersion":)" }
					+ json_string(mcp_protocol_version)
					+ R"(,"capabilities":{"tools":{"listChanged":false}},"serverInfo":{"name":)"
					+ json_string(mcp_server_name) + R"(,"version":)"
					+ json_string(mcp_server_version) + "}}");
		}
		if (method->Text == "tools/list") {
			auto tools = std::string{ R"({"tools":[)" };
			for (auto index = std::size_t{ 0U }; index < mcp_tools.size(); ++index) {
				const auto& descriptor = mcp_tools[index];
				if (index != 0U) {
					tools += ',';
				}
				tools += std::string{ R"({"name":)" }
					+ json_string(descriptor.Name) + R"(,"description":)"
					+ json_string(descriptor.Summary) + R"(,"inputSchema":)";
				tools += descriptor.TakesPath
					? R"({"type":"object","properties":{"path":{"type":"string","description":"A path relative to the frozen snapshot root."}},"required":["path"],"additionalProperties":false})"
					: R"({"type":"object","properties":{},"additionalProperties":false})";
				tools += '}';
			}
			tools += "]}";
			return result_response(responseId, tools);
		}
		if (method->Text != "tools/call") {
			return error_response(responseId, mcp_refusal::unknown_method);
		}

		const auto* const parameters = document->member("params");
		if (parameters == nullptr
			|| parameters->Kind != json_node::kind::object) {
			return error_response(responseId, mcp_refusal::invalid_arguments);
		}
		const auto* const toolName = parameters->member("name");
		if (toolName == nullptr || toolName->Kind != json_node::kind::string) {
			return error_response(responseId, mcp_refusal::invalid_arguments);
		}
		const auto selected = std::ranges::find(
			mcp_tools, toolName->Text, &mcp_tool_descriptor::Name);
		if (selected == mcp_tools.end()) {
			return error_response(responseId, mcp_refusal::unknown_tool);
		}

		// Staleness is decided before anything is read. A helper that read
		// first and compared afterwards would already have served the wrong
		// revision by the time it answered.
		const auto observedText = read_text_file(
			SnapshotRoot / "binding", maximum_mcp_request_bytes);
		if (!observedText) {
			return error_response(responseId, mcp_refusal::snapshot_unavailable);
		}
		const auto observed
			= decode_revision_binding(split(*observedText, '\n').front());
		if (!observed) {
			return error_response(responseId, mcp_refusal::snapshot_unavailable);
		}
		if (binding_moved(Pinned, *observed)) {
			return error_response(
				responseId, mcp_refusal::revision_binding_stale);
		}

		auto requestedPath = std::string_view{ };
		if (selected->TakesPath) {
			const auto* const arguments = parameters->member("arguments");
			if (arguments == nullptr
				|| arguments->Kind != json_node::kind::object) {
				return error_response(responseId, mcp_refusal::invalid_arguments);
			}
			const auto* const path = arguments->member("path");
			if (path == nullptr || path->Kind != json_node::kind::string) {
				return error_response(responseId, mcp_refusal::invalid_arguments);
			}
			requestedPath = path->Text;
		}

		switch (selected->Tool) {
		case mcp_tool::snapshot_binding:
			return result_response(
				responseId, text_content(encode_revision_binding(Pinned)));
		case mcp_tool::list_snapshot_files: {
			const auto files = read_snapshot_alignment(SnapshotRoot);
			if (!files) {
				return error_response(
					responseId, mcp_refusal::snapshot_unavailable);
			}
			auto listing = std::string{ };
			for (const auto& file : *files) {
				listing += file.Path;
				listing += '\n';
			}
			return result_response(responseId, text_content(listing));
		}
		case mcp_tool::read_snapshot_file: {
			const auto resolved
				= resolve_snapshot_path(SnapshotRoot, requestedPath);
			if (!resolved) {
				return error_response(responseId, resolved.error());
			}
			auto filesystemError = std::error_code{ };
			if (!std::filesystem::is_regular_file(*resolved, filesystemError)) {
				return error_response(
					responseId, mcp_refusal::snapshot_entry_missing);
			}
			const auto contents
				= read_text_file(*resolved, maximum_mcp_snapshot_entry_bytes);
			if (!contents) {
				return error_response(
					responseId, mcp_refusal::snapshot_entry_too_large);
			}
			return result_response(responseId, text_content(*contents));
		}
		case mcp_tool::snapshot_file_alignment: {
			const auto resolved
				= resolve_snapshot_path(SnapshotRoot, requestedPath);
			if (!resolved) {
				return error_response(responseId, resolved.error());
			}
			const auto files = read_snapshot_alignment(SnapshotRoot);
			if (!files) {
				return error_response(
					responseId, mcp_refusal::snapshot_unavailable);
			}
			const auto match = std::ranges::find(
				*files, requestedPath, &core::agent_finding_snapshot_file::Path);
			if (match == files->end()) {
				return error_response(
					responseId, mcp_refusal::snapshot_entry_missing);
			}
			auto facts = std::string{ "path\t" } + match->Path + "\ncomparison\t"
				+ encode_identity(match->Comparison.bytes())
				+ "\nleft_line_count\t" + std::to_string(match->LeftLineCount)
				+ "\nright_line_count\t" + std::to_string(match->RightLineCount)
				+ "\n";
			for (const auto& segment : match->Segments) {
				facts += "segment\t" + encode_identity(segment.bytes()) + "\n";
			}
			for (const auto& group : match->Groups) {
				facts += "group\t" + encode_identity(group.bytes()) + "\n";
			}
			for (const auto& move : match->Moves) {
				facts += "move\t" + encode_identity(move.bytes()) + "\n";
			}
			return result_response(responseId, text_content(facts));
		}
		}
		return error_response(responseId, mcp_refusal::unknown_tool);
	}
} // namespace bpr::adapters::mcp
