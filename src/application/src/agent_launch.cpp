#include "bpr/application/agent_launch.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <ranges>
#include <utility>

namespace bpr::application {
	namespace {
		void append_field(
			std::string&           aOutput,
			const std::string_view aName,
			const std::string_view aValue)
		{
			aOutput += aName;
			aOutput += '\t';
			for (const auto character : aValue) {
				switch (character) {
				case '\n':
					aOutput += "\\n";
					break;
				case '\r':
					aOutput += "\\r";
					break;
				case '\t':
					aOutput += "\\t";
					break;
				case '\\':
					aOutput += "\\\\";
					break;
				default:
					aOutput += character;
					break;
				}
			}
			aOutput += '\n';
		}

		template <typename Value>
		void append_number(
			std::string& aOutput, const std::string_view aName, const Value aValue)
		{
			append_field(aOutput, aName, std::to_string(aValue));
		}

		template <typename Identity>
		[[nodiscard]] auto hex_identity(const Identity& aIdentity) -> std::string
		{
			constexpr std::string_view digits = "0123456789abcdef";
			auto                       output = std::string{ };
			output.reserve(aIdentity.bytes().size() * 2U);
			for (const auto byte : aIdentity.bytes()) {
				output.push_back(digits[byte >> 4U]);
				output.push_back(digits[byte & 0x0FU]);
			}
			return output;
		}

		void append_source_line(
			std::string&                 aOutput,
			const std::string&           aPrefix,
			const core::source_line_ref& aLine)
		{
			append_field(
				aOutput, aPrefix + ".comparison", hex_identity(aLine.Comparison));
			append_number(
				aOutput,
				aPrefix + ".side",
				static_cast<std::uint8_t>(aLine.Side));
			append_number(aOutput, aPrefix + ".line", aLine.Line);
		}

		template <typename Identity>
		void append_optional_identity(
			std::string&                   aOutput,
			const std::string&             aPrefix,
			const std::optional<Identity>& aIdentity)
		{
			append_number(
				aOutput, aPrefix + ".present", aIdentity.has_value() ? 1 : 0);
			if (aIdentity) {
				append_field(aOutput, aPrefix + ".value", hex_identity(*aIdentity));
			}
		}

		[[nodiscard]] auto json_escape(const std::string_view aValue) -> std::string
		{
			auto output = std::string{ };
			output.reserve(aValue.size());
			constexpr std::string_view hex = "0123456789abcdef";
			for (const auto character : aValue) {
				switch (character) {
				case '"':
					output += "\\\"";
					break;
				case '\\':
					output += "\\\\";
					break;
				case '\b':
					output += "\\b";
					break;
				case '\f':
					output += "\\f";
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
				default: {
					const auto byte = static_cast<unsigned char>(character);
					if (byte < 0x20U) {
						output += "\\u00";
						output.push_back(hex[byte >> 4U]);
						output.push_back(hex[byte & 0x0FU]);
					} else {
						output += character;
					}
					break;
				}
				}
			}
			return output;
		}

		/// \brief Wraps one encoded supplement in the protocol's own first frame.
		///
		/// \details Where the reviewer's typed message ends up is a property of
		/// this function, and it is one position: `chat_message`, inside the
		/// payload, which every protocol carries exactly once. For
		/// `codex_jsonl_exec` and `external_terminal` the payload is the frame
		/// body after the `BPR-CONTEXT/2` header; for `claude_stream_json` it is
		/// the JSON-escaped text of the one user message; and for
		/// `codex_app_server` this frame is the handshake, so the payload — and
		/// with it the message — rides the deferred context
		/// `build_agent_protocol_followup` sends once the thread is identified.
		///
		/// Nothing is appended to a frame here. A message added beside the
		/// payload would be the same question asked twice on three protocols
		/// and lost on the fourth, and the decode side would have to learn a
		/// second place to look for it.
		[[nodiscard]] auto frame_agent_input(
			const core::agent_protocol aProtocol, const std::string_view aPayload)
			-> std::string
		{
			const auto escaped = json_escape(aPayload);
			switch (aProtocol) {
			case core::agent_protocol::codex_app_server:
				return "{\"method\":\"initialize\",\"id\":0,\"params\":{"
					   "\"clientInfo\":{\"name\":\"betterpullreview\","
					   "\"title\":\"BetterPullReview\",\"version\":\"1\"}}}\n";
			case core::agent_protocol::codex_jsonl_exec:
				return "BPR-CONTEXT/2 CODEX-EXEC\n" + std::string{ aPayload };
			case core::agent_protocol::claude_stream_json:
				return "{\"type\":\"user\",\"message\":{\"role\":\"user\","
					   "\"content\":[{\"type\":\"text\",\"text\":\""
					+ escaped + "\"}]}}\n";
			case core::agent_protocol::external_terminal:
				return "BPR-CONTEXT/2 EXTERNAL-TERMINAL\n"
					+ std::string{ aPayload };
			}
			return { };
		}

		enum class json_kind : std::uint8_t {
			object_value,
			array_value,
			string_value,
			number_value,
			boolean_value,
			null_value,
		};

		struct json_value
		{
			json_kind                                       Kind = json_kind::null_value;
			std::string                                     Text;
			std::vector<std::pair<std::string, json_value>> Members;
			std::vector<json_value>                         Elements;
			bool                                            Boolean = false;
			/// The half-open byte range this value occupied in its own input.
			/// Recorded so a payload can be lifted out of a frame exactly as it
			/// was written, rather than re-serialized into something the agent
			/// never sent.
			std::size_t Begin = 0U;
			std::size_t End   = 0U;
		};

		class bounded_json_parser final
		{
		public:
			explicit bounded_json_parser(const std::string_view aInput)
				: Input{ aInput }
			{
			}

			[[nodiscard]] auto parse()
				-> std::expected<json_value, agent_protocol_failure>
			{
				auto value = parse_value(0U);
				skip_space();
				if (!value || Position != Input.size()) {
					return malformed();
				}
				return value;
			}

		private:
			static constexpr std::size_t maximum_json_depth = 32U;

			[[nodiscard]] static auto malformed()
				-> std::unexpected<agent_protocol_failure>
			{
				return std::unexpected(agent_protocol_failure::malformed_frame);
			}

			void skip_space() noexcept
			{
				while (Position < Input.size()
					   && (Input[Position] == ' ' || Input[Position] == '\t'
						   || Input[Position] == '\n' || Input[Position] == '\r')) {
					++Position;
				}
			}

			[[nodiscard]] auto parse_value(const std::size_t aDepth)
				-> std::expected<json_value, agent_protocol_failure>
			{
				skip_space();
				const auto begin = Position;
				auto       value = parse_value_at(aDepth);
				if (value) {
					value->Begin = begin;
					value->End   = Position;
				}
				return value;
			}

			[[nodiscard]] auto parse_value_at(const std::size_t aDepth)
				-> std::expected<json_value, agent_protocol_failure>
			{
				if (aDepth >= maximum_json_depth) {
					return malformed();
				}
				skip_space();
				if (Position == Input.size()) {
					return malformed();
				}
				const auto next = Input[Position];
				if (next == '{') {
					return parse_object(aDepth);
				}
				if (next == '[') {
					return parse_array(aDepth);
				}
				if (next == '"') {
					auto text = parse_string();
					if (!text) {
						return malformed();
					}
					return json_value{
						.Kind = json_kind::string_value,
						.Text = std::move(*text),
					};
				}
				if (next == 't') {
					return parse_literal("true", json_kind::boolean_value, true);
				}
				if (next == 'f') {
					return parse_literal("false", json_kind::boolean_value, false);
				}
				if (next == 'n') {
					return parse_literal("null", json_kind::null_value, false);
				}
				return parse_number();
			}

			[[nodiscard]] auto parse_object(const std::size_t aDepth)
				-> std::expected<json_value, agent_protocol_failure>
			{
				++Position;
				auto result = json_value{ .Kind = json_kind::object_value };
				skip_space();
				if (Position < Input.size() && Input[Position] == '}') {
					++Position;
					return result;
				}
				while (Position < Input.size()) {
					auto name = parse_string();
					if (
						!name
						|| std::ranges::find_if(
							   result.Members,
							   [&name](const auto& aMember) {
								   return aMember.first == *name;
							   })
							!= result.Members.end()) {
						return malformed();
					}
					skip_space();
					if (Position == Input.size() || Input[Position] != ':') {
						return malformed();
					}
					++Position;
					auto value = parse_value(aDepth + 1U);
					if (!value) {
						return malformed();
					}
					result.Members.emplace_back(std::move(*name), std::move(*value));
					skip_space();
					if (Position == Input.size()) {
						return malformed();
					}
					const auto delimiter = Input[Position++];
					if (delimiter == '}') {
						return result;
					}
					if (delimiter != ',') {
						return malformed();
					}
					skip_space();
				}
				return malformed();
			}

			[[nodiscard]] auto parse_array(const std::size_t aDepth)
				-> std::expected<json_value, agent_protocol_failure>
			{
				++Position;
				auto result = json_value{ .Kind = json_kind::array_value };
				skip_space();
				if (Position < Input.size() && Input[Position] == ']') {
					++Position;
					return result;
				}
				while (Position < Input.size()) {
					auto value = parse_value(aDepth + 1U);
					if (!value) {
						return malformed();
					}
					result.Elements.push_back(std::move(*value));
					skip_space();
					if (Position == Input.size()) {
						return malformed();
					}
					const auto delimiter = Input[Position++];
					if (delimiter == ']') {
						return result;
					}
					if (delimiter != ',') {
						return malformed();
					}
				}
				return malformed();
			}

			[[nodiscard]] auto parse_string()
				-> std::expected<std::string, agent_protocol_failure>
			{
				skip_space();
				if (Position == Input.size() || Input[Position] != '"') {
					return malformed();
				}
				++Position;
				auto result = std::string{ };
				while (Position < Input.size()) {
					const auto byte = static_cast<unsigned char>(Input[Position++]);
					if (byte == '"') {
						return result;
					}
					if (byte < 0x20U) {
						return malformed();
					}
					if (byte != '\\') {
						if (byte < 0x80U) {
							result.push_back(static_cast<char>(byte));
						} else if (!append_raw_utf8(result, byte)) {
							return malformed();
						}
						continue;
					}
					if (Position == Input.size()) {
						return malformed();
					}
					switch (Input[Position++]) {
					case '"':
						result += '"';
						break;
					case '\\':
						result += '\\';
						break;
					case '/':
						result += '/';
						break;
					case 'b':
						result += '\b';
						break;
					case 'f':
						result += '\f';
						break;
					case 'n':
						result += '\n';
						break;
					case 'r':
						result += '\r';
						break;
					case 't':
						result += '\t';
						break;
					case 'u': {
						auto codePoint = parse_hex_quad();
						if (!codePoint) {
							return malformed();
						}
						if (*codePoint >= 0xD800U && *codePoint <= 0xDBFFU) {
							if (
								Position + 2U > Input.size()
								|| Input[Position] != '\\'
								|| Input[Position + 1U] != 'u') {
								return malformed();
							}
							Position += 2U;
							auto low = parse_hex_quad();
							if (!low || *low < 0xDC00U || *low > 0xDFFFU) {
								return malformed();
							}
							*codePoint = 0x10000U + ((*codePoint - 0xD800U) << 10U)
								+ (*low - 0xDC00U);
						} else if (*codePoint >= 0xDC00U && *codePoint <= 0xDFFFU) {
							return malformed();
						}
						append_utf8(result, *codePoint);
						break;
					}
					default:
						return malformed();
					}
				}
				return malformed();
			}

			[[nodiscard]] auto append_raw_utf8(
				std::string& aOutput, const unsigned char aLeadingByte) -> bool
			{
				const auto begin             = Position - 1U;
				auto       codePoint         = std::uint32_t{ 0U };
				auto       continuationCount = std::size_t{ 0U };
				auto       minimum           = std::uint32_t{ 0U };
				if (aLeadingByte >= 0xC2U && aLeadingByte <= 0xDFU) {
					codePoint         = aLeadingByte & 0x1FU;
					continuationCount = 1U;
					minimum           = 0x80U;
				} else if (aLeadingByte >= 0xE0U && aLeadingByte <= 0xEFU) {
					codePoint         = aLeadingByte & 0x0FU;
					continuationCount = 2U;
					minimum           = 0x800U;
				} else if (aLeadingByte >= 0xF0U && aLeadingByte <= 0xF4U) {
					codePoint         = aLeadingByte & 0x07U;
					continuationCount = 3U;
					minimum           = 0x10000U;
				} else {
					return false;
				}
				if (continuationCount > Input.size() - Position) {
					return false;
				}
				for (auto count = std::size_t{ 0U }; count < continuationCount; ++count) {
					const auto continuation
						= static_cast<unsigned char>(Input[Position++]);
					if ((continuation & 0xC0U) != 0x80U) {
						return false;
					}
					codePoint = (codePoint << 6U) | (continuation & 0x3FU);
				}
				if (
					codePoint < minimum || codePoint > 0x10FFFFU
					|| (codePoint >= 0xD800U && codePoint <= 0xDFFFU)) {
					return false;
				}
				aOutput.append(Input.substr(begin, continuationCount + 1U));
				return true;
			}

			[[nodiscard]] auto parse_hex_quad()
				-> std::expected<std::uint32_t, agent_protocol_failure>
			{
				if (Position + 4U > Input.size()) {
					return malformed();
				}
				auto value = std::uint32_t{ 0U };
				for (auto count = 0U; count < 4U; ++count) {
					const auto byte = Input[Position++];
					value <<= 4U;
					if (byte >= '0' && byte <= '9') {
						value += static_cast<std::uint32_t>(byte - '0');
					} else if (byte >= 'a' && byte <= 'f') {
						value += static_cast<std::uint32_t>(byte - 'a' + 10);
					} else if (byte >= 'A' && byte <= 'F') {
						value += static_cast<std::uint32_t>(byte - 'A' + 10);
					} else {
						return malformed();
					}
				}
				return value;
			}

			static void append_utf8(std::string& aOutput, const std::uint32_t aCodePoint)
			{
				if (aCodePoint <= 0x7FU) {
					aOutput.push_back(static_cast<char>(aCodePoint));
				} else if (aCodePoint <= 0x7FFU) {
					aOutput.push_back(static_cast<char>(0xC0U | (aCodePoint >> 6U)));
					aOutput.push_back(static_cast<char>(0x80U | (aCodePoint & 0x3FU)));
				} else if (aCodePoint <= 0xFFFFU) {
					aOutput.push_back(static_cast<char>(0xE0U | (aCodePoint >> 12U)));
					aOutput.push_back(
						static_cast<char>(0x80U | ((aCodePoint >> 6U) & 0x3FU)));
					aOutput.push_back(static_cast<char>(0x80U | (aCodePoint & 0x3FU)));
				} else {
					aOutput.push_back(static_cast<char>(0xF0U | (aCodePoint >> 18U)));
					aOutput.push_back(
						static_cast<char>(0x80U | ((aCodePoint >> 12U) & 0x3FU)));
					aOutput.push_back(
						static_cast<char>(0x80U | ((aCodePoint >> 6U) & 0x3FU)));
					aOutput.push_back(static_cast<char>(0x80U | (aCodePoint & 0x3FU)));
				}
			}

			[[nodiscard]] auto parse_literal(
				const std::string_view aLiteral,
				const json_kind        aKind,
				const bool             aBoolean)
				-> std::expected<json_value, agent_protocol_failure>
			{
				if (!Input.substr(Position).starts_with(aLiteral)) {
					return malformed();
				}
				Position += aLiteral.size();
				return json_value{ .Kind = aKind, .Boolean = aBoolean };
			}

			[[nodiscard]] auto parse_number()
				-> std::expected<json_value, agent_protocol_failure>
			{
				const auto begin = Position;
				if (Position < Input.size() && Input[Position] == '-') {
					++Position;
				}
				if (Position == Input.size()) {
					return malformed();
				}
				if (Input[Position] == '0') {
					++Position;
				} else if (Input[Position] >= '1' && Input[Position] <= '9') {
					while (
						Position < Input.size() && Input[Position] >= '0'
						&& Input[Position] <= '9') {
						++Position;
					}
				} else {
					return malformed();
				}
				if (Position < Input.size() && Input[Position] == '.') {
					++Position;
					if (Position == Input.size() || Input[Position] < '0'
						|| Input[Position] > '9') {
						return malformed();
					}
					while (
						Position < Input.size() && Input[Position] >= '0'
						&& Input[Position] <= '9') {
						++Position;
					}
				}
				if (
					Position < Input.size()
					&& (Input[Position] == 'e' || Input[Position] == 'E')) {
					++Position;
					if (
						Position < Input.size()
						&& (Input[Position] == '+' || Input[Position] == '-')) {
						++Position;
					}
					if (Position == Input.size() || Input[Position] < '0'
						|| Input[Position] > '9') {
						return malformed();
					}
					while (
						Position < Input.size() && Input[Position] >= '0'
						&& Input[Position] <= '9') {
						++Position;
					}
				}
				return json_value{
					.Kind = json_kind::number_value,
					.Text = std::string{ Input.substr(begin, Position - begin) },
				};
			}

			std::string_view Input;
			std::size_t      Position = 0U;
		};

		[[nodiscard]] auto member(
			const json_value& aObject, const std::string_view aName) noexcept
			-> const json_value*
		{
			if (aObject.Kind != json_kind::object_value) {
				return nullptr;
			}
			const auto found = std::ranges::find_if(
				aObject.Members,
				[aName](const auto& aMember) { return aMember.first == aName; });
			return found == aObject.Members.end() ? nullptr : &found->second;
		}

		[[nodiscard]] auto string_member(
			const json_value& aObject, const std::string_view aName) noexcept
			-> const std::string*
		{
			const auto* value = member(aObject, aName);
			return value != nullptr && value->Kind == json_kind::string_value
				? &value->Text
				: nullptr;
		}

		[[nodiscard]] auto object_member(
			const json_value& aObject, const std::string_view aName) noexcept
			-> const json_value*
		{
			const auto* value = member(aObject, aName);
			return value != nullptr && value->Kind == json_kind::object_value ? value
																			  : nullptr;
		}

		[[nodiscard]] auto turn_identity_and_status(const json_value& aTurn)
			-> std::optional<std::pair<std::string, std::string>>
		{
			const auto* id     = string_member(aTurn, "id");
			const auto* items  = member(aTurn, "items");
			const auto* status = string_member(aTurn, "status");
			if (
				id == nullptr || id->empty() || items == nullptr
				|| items->Kind != json_kind::array_value || status == nullptr) {
				return std::nullopt;
			}
			return std::pair{ *id, *status };
		}

		[[nodiscard]] auto no_protocol_events()
			-> std::vector<agent_protocol_event>
		{
			return { };
		}

		/// \brief Lifts one result frame's finding payload out, undecoded.
		///
		/// \details The payload is carried as the exact bytes the child wrote,
		/// taken from the recorded span, so the shared loop decodes what
		/// arrived rather than a re-rendering of it. Deciding what those bytes
		/// mean is deliberately not done here: the frame readers know the
		/// protocol, and the loop is the one place both transports reach.
		[[nodiscard]] auto payload_events(
			const json_value&      aCarrier,
			const std::string_view aFrame) -> std::vector<agent_protocol_event>
		{
			const auto* payload = member(aCarrier, agent_finding_payload_member);
			if (payload == nullptr || payload->End <= payload->Begin
				|| payload->End > aFrame.size()) {
				return no_protocol_events();
			}
			return std::vector<agent_protocol_event>{ {
				.Kind = agent_protocol_event_kind::finding_payload,
				.Text = std::string{ aFrame.substr(
					payload->Begin, payload->End - payload->Begin) },
			} };
		}

		[[nodiscard]] auto decode_app_server_frame(
			const json_value&      aRoot,
			const std::string_view aFrame,
			std::uint8_t&          aPhase,
			std::string&           aThread,
			std::string&           aTurn)
			-> std::expected<std::vector<agent_protocol_event>, agent_protocol_failure>
		{
			if (const auto* id = member(aRoot, "id"); id != nullptr) {
				const auto expectedId = aPhase == 0U ? std::string_view{ "0" }
					: aPhase == 2U                   ? std::string_view{ "1" }
					: aPhase == 4U                   ? std::string_view{ "2" }
													 : std::string_view{ };
				if (
					id->Kind != json_kind::number_value || expectedId.empty()
					|| id->Text != expectedId) {
					return std::unexpected(agent_protocol_failure::malformed_frame);
				}
				if (member(aRoot, "error") != nullptr) {
					return std::unexpected(agent_protocol_failure::remote_failed);
				}
				const auto* result = object_member(aRoot, "result");
				if (result == nullptr) {
					return std::unexpected(agent_protocol_failure::malformed_frame);
				}
				if (aPhase == 0U) {
					aPhase = 1U;
					return std::vector<agent_protocol_event>{ {
						.Kind = agent_protocol_event_kind::initialization_complete,
					} };
				}
				if (aPhase == 2U) {
					const auto* thread = object_member(*result, "thread");
					const auto* threadId
						= thread == nullptr ? nullptr : string_member(*thread, "id");
					if (threadId == nullptr || threadId->empty()) {
						return std::unexpected(
							agent_protocol_failure::malformed_frame);
					}
					aThread = *threadId;
					aPhase  = 3U;
					return std::vector<agent_protocol_event>{ {
						.Kind = agent_protocol_event_kind::context_channel_ready,
						.Text = aThread,
					} };
				}
				if (aPhase == 4U) {
					const auto* turn   = object_member(*result, "turn");
					const auto  parsed = turn == nullptr
						? std::optional<std::pair<std::string, std::string>>{ }
						: turn_identity_and_status(*turn);
					if (!parsed || parsed->second != "inProgress") {
						return std::unexpected(
							agent_protocol_failure::malformed_frame);
					}
					aTurn  = parsed->first;
					aPhase = 5U;
					return no_protocol_events();
				}
				return std::unexpected(agent_protocol_failure::malformed_frame);
			}

			const auto* method = string_member(aRoot, "method");
			const auto* params = object_member(aRoot, "params");
			if (method == nullptr || params == nullptr) {
				return std::unexpected(agent_protocol_failure::malformed_frame);
			}
			if (*method == "item/agentMessage/delta") {
				const auto* thread = string_member(*params, "threadId");
				const auto* turn   = string_member(*params, "turnId");
				const auto* item   = string_member(*params, "itemId");
				const auto* delta  = string_member(*params, "delta");
				if (
					aPhase != 5U || thread == nullptr || *thread != aThread
					|| turn == nullptr || *turn != aTurn || item == nullptr
					|| item->empty() || delta == nullptr) {
					return std::unexpected(agent_protocol_failure::malformed_frame);
				}
				return std::vector<agent_protocol_event>{ {
					.Kind = agent_protocol_event_kind::output,
					.Text = *delta,
				} };
			}
			if (*method == "turn/completed") {
				const auto* thread = string_member(*params, "threadId");
				const auto* turn   = object_member(*params, "turn");
				const auto  parsed = turn == nullptr
					? std::optional<std::pair<std::string, std::string>>{ }
					: turn_identity_and_status(*turn);
				if (
					aPhase != 5U || thread == nullptr || *thread != aThread
					|| !parsed || parsed->first != aTurn) {
					return std::unexpected(agent_protocol_failure::malformed_frame);
				}
				if (parsed->second == "failed") {
					return std::unexpected(agent_protocol_failure::remote_failed);
				}
				if (parsed->second == "interrupted") {
					return std::unexpected(agent_protocol_failure::remote_interrupted);
				}
				if (parsed->second != "completed") {
					return std::unexpected(agent_protocol_failure::malformed_frame);
				}
				aPhase = 6U;
				return payload_events(*turn, aFrame);
			}
			if (*method == "error") {
				return std::unexpected(agent_protocol_failure::remote_failed);
			}
			constexpr auto ignored = std::array{
				std::string_view{ "thread/started" },
				std::string_view{ "turn/started" },
				std::string_view{ "item/started" },
				std::string_view{ "item/completed" },
			};
			if (std::ranges::find(ignored, *method) != ignored.end()) {
				return no_protocol_events();
			}
			return std::unexpected(agent_protocol_failure::malformed_frame);
		}

		[[nodiscard]] auto decode_codex_jsonl_frame(
			const json_value&      aRoot,
			const std::string_view aFrame,
			std::uint8_t&          aPhase,
			std::string&           aThread)
			-> std::expected<std::vector<agent_protocol_event>, agent_protocol_failure>
		{
			const auto* type = string_member(aRoot, "type");
			if (type == nullptr) {
				return std::unexpected(agent_protocol_failure::malformed_frame);
			}
			if (*type == "thread.started" && aPhase == 0U) {
				const auto* thread = string_member(aRoot, "thread_id");
				if (thread == nullptr || thread->empty()) {
					return std::unexpected(agent_protocol_failure::malformed_frame);
				}
				aThread = *thread;
				aPhase  = 1U;
				return no_protocol_events();
			}
			if (*type == "turn.started" && aPhase == 1U) {
				aPhase = 2U;
				return no_protocol_events();
			}
			if ((*type == "turn.failed" || *type == "error") && aPhase == 2U) {
				return std::unexpected(agent_protocol_failure::remote_failed);
			}
			if (*type == "turn.completed" && aPhase == 2U) {
				const auto* usage = object_member(aRoot, "usage");
				if (usage == nullptr) {
					return std::unexpected(agent_protocol_failure::malformed_frame);
				}
				aPhase = 3U;
				return payload_events(aRoot, aFrame);
			}
			if ((*type == "item.started" || *type == "item.completed")
				&& aPhase == 2U) {
				const auto* item     = object_member(aRoot, "item");
				const auto* itemId   = item == nullptr ? nullptr : string_member(*item, "id");
				const auto* itemType = item == nullptr ? nullptr : string_member(*item, "type");
				if (itemId == nullptr || itemId->empty() || itemType == nullptr) {
					return std::unexpected(agent_protocol_failure::malformed_frame);
				}
				if (*type == "item.completed" && *itemType == "agent_message") {
					const auto* text = string_member(*item, "text");
					if (text == nullptr) {
						return std::unexpected(
							agent_protocol_failure::malformed_frame);
					}
					return std::vector<agent_protocol_event>{ {
						.Kind = agent_protocol_event_kind::output,
						.Text = *text,
					} };
				}
				return no_protocol_events();
			}
			return std::unexpected(agent_protocol_failure::malformed_frame);
		}

		[[nodiscard]] auto decode_claude_frame(
			const json_value&      aRoot,
			const std::string_view aFrame,
			std::uint8_t&          aPhase,
			bool&                  aSawAssistant,
			std::string&           aSession)
			-> std::expected<std::vector<agent_protocol_event>, agent_protocol_failure>
		{
			const auto* type    = string_member(aRoot, "type");
			const auto* session = string_member(aRoot, "session_id");
			if (type == nullptr || session == nullptr || session->empty()) {
				return std::unexpected(agent_protocol_failure::malformed_frame);
			}
			if (*type == "system" && aPhase == 0U) {
				const auto* subtype = string_member(aRoot, "subtype");
				if (subtype == nullptr || *subtype != "init") {
					return std::unexpected(agent_protocol_failure::malformed_frame);
				}
				aSession = *session;
				aPhase   = 1U;
				return no_protocol_events();
			}
			if (aPhase != 1U || *session != aSession) {
				return std::unexpected(agent_protocol_failure::malformed_frame);
			}
			if (*type == "assistant" || *type == "user") {
				const auto* message = object_member(aRoot, "message");
				const auto* role    = message == nullptr ? nullptr : string_member(*message, "role");
				const auto* content = message == nullptr ? nullptr : member(*message, "content");
				if (
					role == nullptr || *role != *type || content == nullptr
					|| content->Kind != json_kind::array_value) {
					return std::unexpected(agent_protocol_failure::malformed_frame);
				}
				auto events = std::vector<agent_protocol_event>{ };
				for (const auto& block : content->Elements) {
					const auto* blockType = string_member(block, "type");
					if (blockType == nullptr) {
						return std::unexpected(
							agent_protocol_failure::malformed_frame);
					}
					if (*type == "assistant" && *blockType == "text") {
						const auto* text = string_member(block, "text");
						if (text == nullptr) {
							return std::unexpected(
								agent_protocol_failure::malformed_frame);
						}
						events.push_back({
							.Kind = agent_protocol_event_kind::output,
							.Text = *text,
						});
					}
				}
				if (*type == "assistant") {
					aSawAssistant = true;
				}
				return events;
			}
			if (*type == "result") {
				const auto* subtype = string_member(aRoot, "subtype");
				const auto* isError = member(aRoot, "is_error");
				if (
					subtype == nullptr || isError == nullptr
					|| isError->Kind != json_kind::boolean_value) {
					return std::unexpected(agent_protocol_failure::malformed_frame);
				}
				if (isError->Boolean || *subtype != "success") {
					return std::unexpected(agent_protocol_failure::remote_failed);
				}
				if (!aSawAssistant || string_member(aRoot, "result") == nullptr) {
					return std::unexpected(agent_protocol_failure::malformed_frame);
				}
				aPhase = 2U;
				return payload_events(aRoot, aFrame);
			}
			return std::unexpected(agent_protocol_failure::malformed_frame);
		}

		[[nodiscard]] auto malformed_payload()
			-> std::unexpected<agent_protocol_failure>
		{
			return std::unexpected(
				agent_protocol_failure::malformed_finding_payload);
		}

		/// \brief Reads one JSON number as a bounded non-negative integer.
		///
		/// \details Signs, fractions, and exponents are refused rather than
		/// rounded: an agent that wrote `1e3` for a line number did not write a
		/// line number, and accepting it would put a value nobody typed into an
		/// identity.
		[[nodiscard]] auto decode_unsigned(
			const json_value* aValue, const std::uint64_t aMaximum) noexcept
			-> std::optional<std::uint64_t>
		{
			if (aValue == nullptr || aValue->Kind != json_kind::number_value
				|| aValue->Text.empty()) {
				return std::nullopt;
			}
			auto value = std::uint64_t{ 0U };
			for (const auto character : aValue->Text) {
				if (character < '0' || character > '9') {
					return std::nullopt;
				}
				if (value > (aMaximum - static_cast<std::uint64_t>(character - '0')) / 10U) {
					return std::nullopt;
				}
				value = value * 10U + static_cast<std::uint64_t>(character - '0');
			}
			return value;
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

		/// \brief Reads exactly `aCharacters` lowercase hex digits into bytes.
		///
		/// \details Lowercase only, and exact width only. Accepting a short
		/// identity would let two different identities share a spelling, and
		/// accepting uppercase would give one identity two.
		[[nodiscard]] auto decode_hex_bytes(
			const std::string& aText, const std::size_t aCharacters) noexcept
			-> std::optional<std::array<std::uint8_t, 32U>>
		{
			if (aText.size() != aCharacters) {
				return std::nullopt;
			}
			auto bytes = std::array<std::uint8_t, 32U>{ };
			for (auto index = std::size_t{ 0U }; index < aCharacters; index += 2U) {
				const auto high = decode_hex_digit(aText[index]);
				const auto low  = decode_hex_digit(aText[index + 1U]);
				if (!high || !low) {
					return std::nullopt;
				}
				bytes[index / 2U] = static_cast<std::uint8_t>(
					(*high << 4U) | *low);
			}
			return bytes;
		}

		template <typename Identity>
		[[nodiscard]] auto decode_identity(
			const json_value& aObject, const std::string_view aName) noexcept
			-> std::optional<Identity>
		{
			const auto* text = string_member(aObject, aName);
			if (text == nullptr) {
				return std::nullopt;
			}
			const auto bytes = decode_hex_bytes(*text, 64U);
			if (!bytes) {
				return std::nullopt;
			}
			return Identity{ *bytes };
		}

		/// \brief Reads one commit identifier in its canonical hex spelling.
		///
		/// \details The width selects the algorithm, exactly as Git's own
		/// spelling does, so there is no second algorithm field that could
		/// disagree with the digits beside it.
		[[nodiscard]] auto decode_commit_oid(const std::string& aText) noexcept
			-> std::optional<core::git_commit_oid>
		{
			const auto algorithm = aText.size() == 40U
				? core::git_hash_algorithm::sha1
				: aText.size() == 64U ? core::git_hash_algorithm::sha256
									  : core::git_hash_algorithm::invalid;
			if (algorithm == core::git_hash_algorithm::invalid) {
				return std::nullopt;
			}
			const auto bytes = decode_hex_bytes(aText, aText.size());
			if (!bytes) {
				return std::nullopt;
			}
			return core::git_commit_oid{ algorithm, *bytes };
		}

		[[nodiscard]] auto decode_side(const std::string& aText) noexcept
			-> std::optional<core::diff_side>
		{
			if (aText == "left") {
				return core::diff_side::left;
			}
			if (aText == "right") {
				return core::diff_side::right;
			}
			return std::nullopt;
		}

		/// \brief Reads one enumerator by the stable name the core already emits.
		template <typename Enumerator, std::size_t Count>
		[[nodiscard]] auto decode_named_enumerator(
			const std::string&                   aText,
			const std::array<Enumerator, Count>& aDeclared) noexcept
			-> std::optional<Enumerator>
		{
			for (const auto declared : aDeclared) {
				if (name(declared) == aText) {
					return declared;
				}
			}
			return std::nullopt;
		}

		/// \brief Reads how sure the agent said it was.
		///
		/// \details The four spellings are declared here and nowhere else,
		/// because `agent_finding.hpp` deliberately never bounds confidence:
		/// refusing an undeclared enumerator there would be a branch in which
		/// confidence changed an anchoring outcome. This is that refusal's home.
		[[nodiscard]] auto decode_confidence(const std::string& aText) noexcept
			-> std::optional<core::agent_finding_confidence>
		{
			if (aText == "unstated") {
				return core::agent_finding_confidence::unstated;
			}
			if (aText == "low") {
				return core::agent_finding_confidence::low;
			}
			if (aText == "medium") {
				return core::agent_finding_confidence::medium;
			}
			if (aText == "high") {
				return core::agent_finding_confidence::high;
			}
			return std::nullopt;
		}

		[[nodiscard]] auto decode_position(
			const json_value&      aAnchor,
			const std::string_view aName,
			const std::string_view aOrdinate) noexcept
			-> std::optional<std::pair<core::file_comparison_digest,
									   std::pair<core::diff_side, std::uint32_t>>>
		{
			const auto* position = object_member(aAnchor, aName);
			if (position == nullptr) {
				return std::nullopt;
			}
			const auto comparison
				= decode_identity<core::file_comparison_digest>(
					*position, "comparison");
			const auto* side = string_member(*position, "side");
			const auto  ordinate
				= decode_unsigned(member(*position, aOrdinate), 0xFFFF'FFFFU);
			if (!comparison || side == nullptr || !ordinate) {
				return std::nullopt;
			}
			const auto decodedSide = decode_side(*side);
			if (!decodedSide) {
				return std::nullopt;
			}
			return std::pair{
				*comparison,
				std::pair{ *decodedSide, static_cast<std::uint32_t>(*ordinate) },
			};
		}

		/// \brief Reads the position and refinements the agent named.
		///
		/// \details Absent members stay absent. Whether the result names a
		/// place the frozen snapshot contains — or names two places, or none —
		/// is `core::validate_agent_finding`'s verdict and not decided here.
		[[nodiscard]] auto decode_anchor(const json_value& aFinding) noexcept
			-> std::optional<core::agent_finding_anchor>
		{
			const auto* anchor = object_member(aFinding, "anchor");
			if (anchor == nullptr) {
				return std::nullopt;
			}
			auto result = core::agent_finding_anchor{ };
			if (member(*anchor, "line") != nullptr) {
				const auto line = decode_position(*anchor, "line", "line");
				if (!line) {
					return std::nullopt;
				}
				result.Line = core::source_line_ref{
					.Comparison = line->first,
					.Side       = line->second.first,
					.Line       = line->second.second,
				};
			}
			if (member(*anchor, "boundary") != nullptr) {
				const auto boundary
					= decode_position(*anchor, "boundary", "boundary");
				if (!boundary) {
					return std::nullopt;
				}
				result.Boundary = core::source_boundary_ref{
					.Comparison = boundary->first,
					.Side       = boundary->second.first,
					.Boundary   = boundary->second.second,
				};
			}
			if (member(*anchor, "segment") != nullptr) {
				result.Segment = decode_identity<core::alignment_segment_id>(
					*anchor, "segment");
				if (!result.Segment) {
					return std::nullopt;
				}
			}
			if (member(*anchor, "group") != nullptr) {
				result.Group = decode_identity<core::change_group_id>(
					*anchor, "group");
				if (!result.Group) {
					return std::nullopt;
				}
			}
			if (member(*anchor, "move") != nullptr) {
				result.Move = decode_identity<core::move_relation_id>(
					*anchor, "move");
				if (!result.Move) {
					return std::nullopt;
				}
			}
			return result;
		}

		[[nodiscard]] auto decode_bounded_text(
			const json_value&      aFinding,
			const std::string_view aName,
			const std::size_t      aMaximum) noexcept -> std::optional<std::string>
		{
			const auto* text = string_member(aFinding, aName);
			if (text == nullptr || text->size() > aMaximum) {
				return std::nullopt;
			}
			return *text;
		}

		[[nodiscard]] auto decode_analyzed_commits(const json_value& aFinding)
			-> std::optional<std::vector<core::git_commit_oid>>
		{
			const auto* commits = member(aFinding, "analyzedCommits");
			if (commits == nullptr || commits->Kind != json_kind::array_value
				|| commits->Elements.size() > core::maximum_finding_analyzed_oids) {
				return std::nullopt;
			}
			auto result = std::vector<core::git_commit_oid>{ };
			result.reserve(commits->Elements.size());
			for (const auto& element : commits->Elements) {
				if (element.Kind != json_kind::string_value) {
					return std::nullopt;
				}
				const auto oid = decode_commit_oid(element.Text);
				if (!oid) {
					return std::nullopt;
				}
				result.push_back(*oid);
			}
			return result;
		}

		[[nodiscard]] auto decode_finding(const json_value& aFinding)
			-> std::optional<core::agent_finding>
		{
			if (aFinding.Kind != json_kind::object_value) {
				return std::nullopt;
			}
			const auto provider
				= decode_unsigned(member(aFinding, "provider"), 0xFFFF'FFFF'FFFF'FFFFU);
			const auto* severity   = string_member(aFinding, "severity");
			const auto* confidence = string_member(aFinding, "confidence");
			const auto* category   = string_member(aFinding, "category");
			if (
				!provider || severity == nullptr || confidence == nullptr
				|| category == nullptr) {
				return std::nullopt;
			}
			const auto decodedSeverity
				= decode_named_enumerator(*severity, std::array{
														 core::defect_severity::trivial,
														 core::defect_severity::minor,
														 core::defect_severity::major,
														 core::defect_severity::critical,
													 });
			const auto decodedCategory
				= decode_named_enumerator(*category, std::array{
														 core::defect_category::correctness,
														 core::defect_category::security,
														 core::defect_category::performance,
														 core::defect_category::maintainability,
														 core::defect_category::style,
														 core::defect_category::documentation,
														 core::defect_category::other,
													 });
			const auto decodedConfidence = decode_confidence(*confidence);
			if (!decodedSeverity || !decodedCategory || !decodedConfidence) {
				return std::nullopt;
			}
			const auto* lines  = object_member(aFinding, "lines");
			const auto  begin  = lines == nullptr
				? std::nullopt
				: decode_unsigned(member(*lines, "begin"), 0xFFFF'FFFFU);
			const auto  end    = lines == nullptr
				? std::nullopt
				: decode_unsigned(member(*lines, "end"), 0xFFFF'FFFFU);
			const auto  anchor = decode_anchor(aFinding);
			if (!begin || !end || !anchor) {
				return std::nullopt;
			}
			const auto path = decode_bounded_text(
				aFinding, "path", core::maximum_identified_path_bytes);
			const auto claim = decode_bounded_text(
				aFinding, "claim", core::maximum_finding_claim_bytes);
			const auto evidence = decode_bounded_text(
				aFinding, "evidence", core::maximum_finding_evidence_bytes);
			const auto consequence = decode_bounded_text(
				aFinding, "consequence", core::maximum_finding_consequence_bytes);
			const auto requiredContext = decode_bounded_text(
				aFinding,
				"requiredContext",
				core::maximum_finding_required_context_bytes);
			if (!path || !claim || !evidence || !consequence || !requiredContext) {
				return std::nullopt;
			}
			auto suggested = std::optional<std::string>{ };
			if (member(aFinding, "suggestedComment") != nullptr) {
				suggested = decode_bounded_text(
					aFinding,
					"suggestedComment",
					core::maximum_finding_comment_bytes);
				if (!suggested) {
					return std::nullopt;
				}
			}
			const auto* base = string_member(aFinding, "analyzedBase");
			const auto* head = string_member(aFinding, "analyzedHead");
			if (base == nullptr || head == nullptr) {
				return std::nullopt;
			}
			const auto decodedBase = decode_commit_oid(*base);
			const auto decodedHead = decode_commit_oid(*head);
			const auto commits     = decode_analyzed_commits(aFinding);
			if (!decodedBase || !decodedHead || !commits) {
				return std::nullopt;
			}
			return core::agent_finding{
				.Provider         = core::agent_profile_id{ *provider },
				.Severity         = *decodedSeverity,
				.Confidence       = *decodedConfidence,
				.Category         = *decodedCategory,
				.Path             = *path,
				.Lines            = { .Begin = static_cast<std::uint32_t>(*begin),
									  .End   = static_cast<std::uint32_t>(*end) },
				.Anchor           = *anchor,
				.Claim            = *claim,
				.Evidence         = *evidence,
				.Consequence      = *consequence,
				.SuggestedComment = suggested,
				.RequiredContext  = *requiredContext,
				.AnalyzedBaseOid  = *decodedBase,
				.AnalyzedHeadOid  = *decodedHead,
				.AnalyzedCommits  = *commits,
			};
		}

		[[nodiscard]] auto decode_structured_frame(
			const core::agent_protocol aProtocol,
			const std::string_view     aFrame,
			std::uint8_t&              aPhase,
			bool&                      aSawAssistant,
			std::string&               aSession,
			std::string&               aThread,
			std::string&               aTurn)
			-> std::expected<std::vector<agent_protocol_event>, agent_protocol_failure>
		{
			if (aProtocol == core::agent_protocol::external_terminal) {
				// External terminal has no result frame, so a payload has no
				// carrier here. The sentinel is still recognized rather than
				// ignored: an attempt to return findings over this protocol
				// must be refused by name, and reading it as ordinary output
				// would hide it from the reviewer and the decoder alike.
				if (aFrame.starts_with(agent_finding_payload_sentinel)) {
					return std::vector<agent_protocol_event>{ {
						.Kind = agent_protocol_event_kind::finding_payload,
						.Text = std::string{ aFrame },
					} };
				}
				return std::vector<agent_protocol_event>{ {
					.Kind = agent_protocol_event_kind::output,
					.Text = std::string{ aFrame },
				} };
			}
			auto parsed = bounded_json_parser{ aFrame }.parse();
			if (!parsed || parsed->Kind != json_kind::object_value) {
				return std::unexpected(agent_protocol_failure::malformed_frame);
			}
			switch (aProtocol) {
			case core::agent_protocol::codex_app_server:
				return decode_app_server_frame(
					*parsed, aFrame, aPhase, aThread, aTurn);
			case core::agent_protocol::codex_jsonl_exec:
				return decode_codex_jsonl_frame(*parsed, aFrame, aPhase, aThread);
			case core::agent_protocol::claude_stream_json:
				return decode_claude_frame(
					*parsed, aFrame, aPhase, aSawAssistant, aSession);
			case core::agent_protocol::external_terminal:
				break;
			}
			return std::unexpected(agent_protocol_failure::malformed_frame);
		}
	} // namespace

	auto build_agent_context_supplement(
		const agent_context_observation& aObservation,
		const core::agent_session&       aSession)
		-> std::expected<core::agent_context_supplement, agent_context_failure>
	{
		if (
			!valid(aSession) || aSession.Binding != aObservation.Binding
			|| aObservation.Files.size() > core::maximum_supplement_paths
			|| aObservation.Private.Notes.size() > maximum_private_notes
			|| aObservation.Private.Defects.size() > maximum_private_defects
			|| aObservation.IncludedNotes.size() > core::maximum_selected_notes
			|| aObservation.ChatMessage.size() > core::maximum_chat_message_bytes) {
			return std::unexpected(agent_context_failure::invalid_observation);
		}
		auto request = core::agent_supplement_request{
			.ApplicationSession = aObservation.ApplicationSession,
			.Session            = aSession,
			.PullRequestUrl     = aObservation.PullRequestUrl,
			.PreviousUpload     = aObservation.PreviousUpload,
			.LastReviewedHead   = aObservation.Private.LastReviewedHead,
			.Selection          = aObservation.Selection,
		};
		if (!aObservation.ChatMessage.empty()) {
			// Bound here, to the session this build is for, because that is
			// the session the turn runs under: the launch path allocates the
			// identity, and a message keyed to the one the chat was holding
			// would be a message for the previous conversation.
			request.Message = core::agent_chat_message{
				.Session = aSession.Id,
				.Text    = aObservation.ChatMessage,
			};
		}
		request.Files.reserve(aObservation.Files.size());
		for (const auto& input : aObservation.Files) {
			request.Files.push_back({
				.File               = input.Receipt.File,
				.Material           = input.Receipt.Material,
				.Path               = input.Path,
				.Read               = input.Receipt.Read,
				.ChangedSinceReview = input.ChangedSinceReview,
				.Accepted           = input.Receipt.Accepted,
				.Excluded           = input.Receipt.Excluded,
			});
		}
		request.Defects.reserve(aObservation.Private.Defects.size());
		for (const auto& defect : aObservation.Private.Defects) {
			if (!valid(defect)) {
				return std::unexpected(agent_context_failure::invalid_observation);
			}
			request.Defects.push_back({
				.File         = defect.File,
				.Conversation = defect.Conversation,
				.History      = defect.History,
			});
		}

		// The opt-in is built here, beside the selection, and starts off. It
		// is keyed to this session, so it admits nothing into the next one, and
		// it is turned on only by the reviewer having named prose for *this*
		// invocation. Deriving it from anything else — a profile, a setting, or
		// the mere existence of notes in the ledger — is the "switched on in
		// March, leaking in September" shape `agent_private_context_optin`
		// exists to prevent.
		auto optIn = core::agent_private_context_optin{ .Session = aSession.Id };

		auto selected = std::vector<local_note_id>{ };
		selected.reserve(aObservation.IncludedNotes.size());
		for (const auto id : aObservation.IncludedNotes) {
			if (std::ranges::find(selected, id) != selected.end()) {
				return std::unexpected(agent_context_failure::duplicate_selected_note);
			}
			selected.push_back(id);
			const auto* found = static_cast<const local_note_record*>(nullptr);
			for (const auto& note : aObservation.Private.Notes) {
				if (note.Id != id) {
					continue;
				}
				if (found != nullptr) {
					return std::unexpected(
						agent_context_failure::invalid_observation);
				}
				found = &note;
			}
			if (found == nullptr) {
				return std::unexpected(agent_context_failure::selected_note_missing);
			}
			if (!valid(*found)) {
				return std::unexpected(agent_context_failure::invalid_observation);
			}
			request.SelectedNotes.push_back({
				.File       = found->Attachment
					? std::optional{ found->Attachment->File }
					: std::nullopt,
				.Comparison = found->Attachment
					? std::optional{ found->Attachment->Comparison }
					: std::nullopt,
				.Text       = found->Note.Text,
			});
			optIn.Notes = true;
		}

		if (aObservation.IncludedDraft.has_value()) {
			if (!aObservation.CurrentDraft.has_value()) {
				return std::unexpected(agent_context_failure::selected_draft_missing);
			}
			const auto& current = *aObservation.CurrentDraft;
			if (current.Choice != *aObservation.IncludedDraft) {
				return std::unexpected(agent_context_failure::selected_draft_stale);
			}
			if (
				current.Choice.Revision == 0U
				|| current.Choice.Id != current.Draft.Id
				|| current.Draft.State != core::review_draft_state::drafting
				|| current.Draft.Body.empty()
				|| current.Draft.Body.size() > core::maximum_draft_bytes) {
				return std::unexpected(agent_context_failure::selected_draft_invalid);
			}
			request.SelectedDrafts.push_back({
				.Anchor = current.Draft.Anchor,
				.Text   = current.Draft.Body,
			});
			optIn.Drafts = true;
		}

		// The gated overload, always. The ungated one-argument build is no
		// longer reachable from the host: it would carry whatever the request
		// happened to hold, and the request is assembled a few lines above from
		// a ledger observation the reviewer did not necessarily select from.
		const auto built = core::build_agent_supplement(request, optIn);
		if (!built) {
			return std::unexpected(agent_context_failure::invalid_observation);
		}
		return *built;
	}

	auto materialize_agent_environment(
		const core::agent_profile& aProfile,
		const environment_reader&  aParent)
		-> std::vector<std::pair<std::string, std::string>>
	{
		auto result = std::vector<std::pair<std::string, std::string>>{ };
		if (!aParent || !core::valid(aProfile)) {
			return result;
		}
		result.reserve(aProfile.AllowedEnvironment.size());
		for (const auto& name : aProfile.AllowedEnvironment) {
			if (auto value = aParent(name); value.has_value()) {
				result.emplace_back(name, std::move(*value));
			}
		}
		return result;
	}

	auto encode_agent_supplement(
		const core::agent_context_supplement& aSupplement) -> std::string
	{
		auto result = std::string{ };
		append_number(result, "schema", aSupplement.SchemaVersion);
		append_number(
			result,
			"application_session",
			aSupplement.ApplicationSession.value());
		append_number(result, "session", aSupplement.Session.Id.value());
		append_number(result, "profile", aSupplement.Session.Profile.value());
		append_number(
			result, "host", aSupplement.Session.Binding.Host.value());
		append_number(
			result,
			"repository",
			aSupplement.Session.Binding.Repository.value());
		append_number(
			result,
			"pull_request",
			aSupplement.Session.Binding.PullRequest.value());
		append_field(result, "pull_request_url", aSupplement.PullRequestUrl);
		// The one position the reviewer's typed question occupies. Written
		// unconditionally, and empty when the launch is not a chat turn, so a
		// reader never has to decide whether a missing line means "no question"
		// or "a question this build forgot". Written once, here, because this
		// is the supplement's whole wire form: a question placed in the framing
		// as well would reach the agent twice and it would have been asked once.
		append_field(result, "chat_message", aSupplement.Message);
		append_field(
			result,
			"base",
			core::canonical_hex(aSupplement.Session.Binding.BaseOid).view());
		append_field(
			result,
			"head",
			core::canonical_hex(aSupplement.Session.Binding.HeadOid).view());
		append_number(
			result,
			"left_epoch",
			aSupplement.Session.Binding.LeftEpoch.value());
		append_number(
			result,
			"right_epoch",
			aSupplement.Session.Binding.RightEpoch.value());

		append_number(
			result,
			"previous_upload.present",
			aSupplement.PreviousUpload.has_value() ? 1 : 0);
		if (aSupplement.PreviousUpload) {
			append_field(
				result,
				"previous_upload.value",
				core::canonical_hex(*aSupplement.PreviousUpload).view());
		}
		append_number(
			result,
			"last_reviewed_head.present",
			aSupplement.LastReviewedHead.has_value() ? 1 : 0);
		if (aSupplement.LastReviewedHead) {
			append_field(
				result,
				"last_reviewed_head.value",
				core::canonical_hex(*aSupplement.LastReviewedHead).view());
		}

		append_number(result, "file.count", aSupplement.Files.size());
		for (auto index = std::size_t{ 0U }; index < aSupplement.Files.size(); ++index) {
			const auto  prefix = "file." + std::to_string(index);
			const auto& file   = aSupplement.Files[index];
			append_field(result, prefix + ".identity", hex_identity(file.File));
			append_field(result, prefix + ".material", hex_identity(file.Material));
			append_field(result, prefix + ".path", file.Path);
			append_number(
				result, prefix + ".read", static_cast<std::uint8_t>(file.Read));
			append_number(
				result,
				prefix + ".changed_since_review",
				file.ChangedSinceReview ? 1 : 0);
			append_number(result, prefix + ".accepted", file.Accepted ? 1 : 0);
			append_number(result, prefix + ".excluded", file.Excluded ? 1 : 0);
		}

		append_number(result, "defect.count", aSupplement.Defects.size());
		for (auto index = std::size_t{ 0U }; index < aSupplement.Defects.size(); ++index) {
			const auto  prefix = "defect." + std::to_string(index);
			const auto& defect = aSupplement.Defects[index];
			append_field(result, prefix + ".file", hex_identity(defect.File));
			append_number(
				result, prefix + ".conversation", defect.Conversation.value());
			append_number(
				result, prefix + ".id", defect.History.Defect.Id.value());
			append_source_line(
				result, prefix + ".anchor", defect.History.Defect.Anchor);
			append_source_line(
				result, prefix + ".original_anchor", defect.History.OriginalAnchor);
			append_number(
				result,
				prefix + ".severity",
				static_cast<std::uint8_t>(defect.History.Defect.Severity));
			append_number(
				result,
				prefix + ".category",
				static_cast<std::uint8_t>(defect.History.Defect.Category));
			append_number(
				result,
				prefix + ".lifecycle",
				static_cast<std::uint8_t>(defect.History.Defect.State));
			append_number(
				result,
				prefix + ".availability",
				static_cast<std::uint8_t>(defect.History.Availability));
		}

		append_number(
			result, "selection.present", aSupplement.Selection.has_value() ? 1 : 0);
		if (aSupplement.Selection) {
			const auto& context = *aSupplement.Selection;
			append_field(result, "selection.file", hex_identity(context.File));
			append_field(
				result,
				"selection.comparison",
				hex_identity(context.Selection.Comparison));
			append_number(
				result,
				"selection.kind",
				static_cast<std::uint8_t>(context.Selection.Kind));
			append_source_line(
				result, "selection.preferred", context.Selection.PreferredLine);
			append_optional_identity(
				result, "selection.segment", context.Selection.Segment);
			append_optional_identity(
				result, "selection.group", context.Selection.Group);
			append_number(
				result,
				"selection.mapping_confidence",
				static_cast<std::uint8_t>(context.Confidence));
			append_number(
				result,
				"selection.source_text.present",
				context.SelectedSourceText.has_value() ? 1 : 0);
			if (context.SelectedSourceText) {
				append_field(
					result,
					"selection.source_text.value",
					*context.SelectedSourceText);
			}
			append_number(result, "selection.segment.count", context.Segments.size());
			for (auto index = std::size_t{ 0U }; index < context.Segments.size(); ++index) {
				const auto  prefix  = "selection.segment." + std::to_string(index);
				const auto& segment = context.Segments[index];
				append_field(result, prefix + ".id", hex_identity(segment.Id));
				append_number(
					result, prefix + ".kind", static_cast<std::uint8_t>(segment.Kind));
				append_number(result, prefix + ".left.begin", segment.Left.Begin);
				append_number(result, prefix + ".left.end", segment.Left.End);
				append_number(result, prefix + ".right.begin", segment.Right.Begin);
				append_number(result, prefix + ".right.end", segment.Right.End);
				append_optional_identity(result, prefix + ".group", segment.Group);
			}
			append_number(result, "selection.group.count", context.Groups.size());
			for (auto index = std::size_t{ 0U }; index < context.Groups.size(); ++index) {
				const auto  prefix = "selection.group." + std::to_string(index);
				const auto& group  = context.Groups[index];
				append_field(result, prefix + ".id", hex_identity(group.Id));
				append_number(result, prefix + ".members.begin", group.Members.Begin);
				append_number(result, prefix + ".members.end", group.Members.End);
				append_number(result, prefix + ".left.begin", group.Left.Begin);
				append_number(result, prefix + ".left.end", group.Left.End);
				append_number(result, prefix + ".right.begin", group.Right.Begin);
				append_number(result, prefix + ".right.end", group.Right.End);
				append_number(result, prefix + ".first.left", group.First.Left);
				append_number(result, prefix + ".first.right", group.First.Right);
				append_number(result, prefix + ".last.left", group.Last.Left);
				append_number(result, prefix + ".last.right", group.Last.Right);
				append_number(
					result, prefix + ".kind", static_cast<std::uint8_t>(group.Kind));
				append_number(result, prefix + ".added", group.AddedLines);
				append_number(result, prefix + ".deleted", group.DeletedLines);
				append_number(
					result, prefix + ".replaced_left", group.ReplacedLeftLines);
				append_number(
					result, prefix + ".replaced_right", group.ReplacedRightLines);
			}
		}

		append_number(result, "note.count", aSupplement.Notes.size());
		for (auto index = std::size_t{ 0U }; index < aSupplement.Notes.size(); ++index) {
			const auto  prefix = "note." + std::to_string(index);
			const auto& note   = aSupplement.Notes[index];
			append_optional_identity(result, prefix + ".file", note.File);
			append_number(
				result,
				prefix + ".comparison.present",
				note.Comparison.has_value() ? 1 : 0);
			if (note.Comparison) {
				append_field(
					result,
					prefix + ".comparison.value",
					hex_identity(*note.Comparison));
			}
			append_field(result, prefix + ".text", note.Text);
		}

		append_number(result, "draft.count", aSupplement.Drafts.size());
		for (auto index = std::size_t{ 0U }; index < aSupplement.Drafts.size(); ++index) {
			const auto  prefix = "draft." + std::to_string(index);
			const auto& draft  = aSupplement.Drafts[index];
			append_number(
				result,
				prefix + ".anchor.present",
				draft.Anchor.has_value() ? 1 : 0);
			if (draft.Anchor) {
				append_source_line(result, prefix + ".anchor", *draft.Anchor);
			}
			append_field(result, prefix + ".text", draft.Text);
		}
		return result;
	}

	auto build_agent_launch(
		const core::agent_profile&            aProfile,
		const core::agent_context_supplement& aSupplement,
		const environment_reader&             aParent)
		-> std::expected<agent_launch_request, agent_launch_failure>
	{
		if (!core::valid(aProfile) || core::stable_name(aProfile.Protocol).empty()) {
			return std::unexpected(agent_launch_failure::invalid_profile);
		}
		if (aProfile.Id != aSupplement.Session.Profile) {
			return std::unexpected(agent_launch_failure::profile_session_mismatch);
		}
		if (aSupplement.SchemaVersion != core::agent_supplement_schema_version) {
			return std::unexpected(
				agent_launch_failure::unsupported_supplement_schema);
		}
		auto arguments = std::vector<std::string>{ };
		switch (aProfile.Protocol) {
		case core::agent_protocol::codex_app_server:
			arguments.emplace_back("app-server");
			break;
		case core::agent_protocol::codex_jsonl_exec:
			arguments.emplace_back("exec");
			arguments.emplace_back("--json");
			arguments.emplace_back("-");
			break;
		case core::agent_protocol::claude_stream_json:
			arguments.emplace_back("-p");
			arguments.emplace_back("--input-format");
			arguments.emplace_back("stream-json");
			arguments.emplace_back("--output-format");
			arguments.emplace_back("stream-json");
			arguments.emplace_back("--verbose");
			break;
		case core::agent_protocol::external_terminal:
			break;
		}
		const auto payload = encode_agent_supplement(aSupplement);
		auto       request = agent_launch_request{
			.Protocol    = aProfile.Protocol,
			.Session     = aSupplement.Session.Id,
			.Executable  = aProfile.Executable,
			.Arguments   = std::move(arguments),
			.Environment = materialize_agent_environment(aProfile, aParent),
			.Input       = frame_agent_input(aProfile.Protocol, payload),
		};
		if (aProfile.Protocol == core::agent_protocol::codex_app_server) {
			request.DeferredContext = payload;
		}
		return request;
	}

	auto decode_agent_finding_payload(
		const core::agent_protocol   aProtocol,
		const std::string_view       aPayload,
		const core::agent_session_id aSession)
		-> std::expected<agent_finding_payload, agent_protocol_failure>
	{
		if (aProtocol == core::agent_protocol::external_terminal) {
			return std::unexpected(
				agent_protocol_failure::unsupported_finding_transport);
		}
		if (aPayload.size() > maximum_agent_finding_payload_bytes) {
			return std::unexpected(
				agent_protocol_failure::finding_payload_too_large);
		}
		const auto parsed = bounded_json_parser{ aPayload }.parse();
		if (!parsed || parsed->Kind != json_kind::object_value) {
			return malformed_payload();
		}
		const auto session = decode_unsigned(
			member(*parsed, "session"), 0xFFFF'FFFF'FFFF'FFFFU);
		if (!session) {
			return malformed_payload();
		}
		// Before a single finding is read. A payload naming another session is
		// not this run's answer, and decoding it first would mean deciding what
		// to do with findings that were never about this conversation.
		if (core::agent_session_id{ *session } != aSession) {
			return std::unexpected(
				agent_protocol_failure::unknown_finding_session);
		}
		const auto* findings = member(*parsed, "findings");
		if (findings == nullptr || findings->Kind != json_kind::array_value) {
			return malformed_payload();
		}
		if (findings->Elements.size() > maximum_agent_findings_per_turn) {
			return std::unexpected(agent_protocol_failure::too_many_findings);
		}
		auto payload = agent_finding_payload{ .Session = aSession };
		payload.Findings.reserve(findings->Elements.size());
		for (const auto& element : findings->Elements) {
			auto finding = decode_finding(element);
			if (!finding) {
				return malformed_payload();
			}
			payload.Findings.push_back(std::move(*finding));
		}
		return payload;
	}

	auto build_agent_protocol_followup(
		const agent_launch_request& aRequest,
		const agent_protocol_event& aControl)
		-> std::expected<std::string, agent_protocol_failure>
	{
		if (aRequest.Protocol != core::agent_protocol::codex_app_server) {
			return std::unexpected(agent_protocol_failure::malformed_frame);
		}
		switch (aControl.Kind) {
		case agent_protocol_event_kind::initialization_complete:
			return "{\"method\":\"initialized\"}\n"
				   "{\"method\":\"thread/start\",\"id\":1,\"params\":{}}\n";
		case agent_protocol_event_kind::context_channel_ready:
			if (aControl.Text.empty() || aRequest.DeferredContext.empty()) {
				return std::unexpected(agent_protocol_failure::malformed_frame);
			}
			return "{\"method\":\"turn/start\",\"id\":2,\"params\":{"
				   "\"threadId\":\""
				+ json_escape(aControl.Text)
				+ "\",\"input\":[{\"type\":\"text\",\"text\":\""
				+ json_escape(aRequest.DeferredContext) + "\"}]}}\n";
		case agent_protocol_event_kind::output:
		case agent_protocol_event_kind::finding_payload:
			break;
		}
		return std::unexpected(agent_protocol_failure::malformed_frame);
	}

	agent_protocol_decoder::agent_protocol_decoder(
		const core::agent_protocol aProtocol)
		: Protocol{ aProtocol }
	{
	}

	auto agent_protocol_decoder::push(const std::string_view aBytes)
		-> std::expected<std::vector<agent_protocol_event>, agent_protocol_failure>
	{
		if (Failed) {
			return std::unexpected(agent_protocol_failure::malformed_frame);
		}
		if (
			Protocol == core::agent_protocol::codex_app_server
			&& (Phase == 1U || Phase == 3U) && !aBytes.empty()) {
			Failed = true;
			return std::unexpected(agent_protocol_failure::causal_violation);
		}
		if (aBytes.size() > maximum_agent_stdout_bytes - TotalBytes) {
			Failed = true;
			return std::unexpected(agent_protocol_failure::output_too_large);
		}
		TotalBytes += aBytes.size();
		Pending.append(aBytes);
		auto messages = std::vector<agent_protocol_event>{ };
		while (true) {
			const auto newline = Pending.find('\n');
			if (newline == std::string::npos) {
				break;
			}
			if (newline > maximum_agent_frame_bytes) {
				Failed = true;
				return std::unexpected(agent_protocol_failure::frame_too_large);
			}
			auto frame = Pending.substr(0U, newline);
			Pending.erase(0U, newline + 1U);
			const auto decoded = decode_structured_frame(
				Protocol,
				frame,
				Phase,
				SawAssistant,
				RemoteSession,
				RemoteThread,
				RemoteTurn);
			if (!decoded) {
				Failed = true;
				return std::unexpected(decoded.error());
			}
			// A finding payload is deliberately not a control event. It asks
			// the host for no follow-up write, so pausing the decode on it
			// would change the causal contract App Server's two follow-ups own.
			const auto hasControl = std::ranges::any_of(
				*decoded,
				[](const agent_protocol_event& aEvent) {
					return aEvent.Kind
						== agent_protocol_event_kind::initialization_complete
						|| aEvent.Kind
						== agent_protocol_event_kind::context_channel_ready;
				});
			if (hasControl && !Pending.empty()) {
				Failed = true;
				return std::unexpected(agent_protocol_failure::causal_violation);
			}
			messages.insert(
				messages.end(),
				std::make_move_iterator(decoded->begin()),
				std::make_move_iterator(decoded->end()));
			if (hasControl) {
				break;
			}
		}
		if (Pending.size() > maximum_agent_frame_bytes) {
			Failed = true;
			return std::unexpected(agent_protocol_failure::frame_too_large);
		}
		return messages;
	}

	auto agent_protocol_decoder::finish()
		-> std::expected<std::vector<agent_protocol_event>, agent_protocol_failure>
	{
		if (Failed) {
			return std::unexpected(agent_protocol_failure::malformed_frame);
		}
		if (!Pending.empty() && Protocol != core::agent_protocol::external_terminal) {
			Failed = true;
			return std::unexpected(agent_protocol_failure::incomplete_frame);
		}
		if (Protocol == core::agent_protocol::external_terminal) {
			if (Pending.empty()) {
				return std::vector<agent_protocol_event>{ };
			}
			auto message = std::move(Pending);
			Pending.clear();
			return std::vector<agent_protocol_event>{ {
				.Kind = agent_protocol_event_kind::output,
				.Text = std::move(message),
			} };
		}
		const auto complete
			= (Protocol == core::agent_protocol::codex_app_server && Phase == 6U)
			|| (Protocol == core::agent_protocol::codex_jsonl_exec && Phase == 3U)
			|| (Protocol == core::agent_protocol::claude_stream_json && Phase == 2U);
		if (!complete) {
			Failed = true;
			return std::unexpected(agent_protocol_failure::incomplete_frame);
		}
		return std::vector<agent_protocol_event>{ };
	}

	auto agent_protocol_decoder::acknowledge_followup(
		const agent_protocol_event_kind aControl)
		-> std::expected<void, agent_protocol_failure>
	{
		const auto followsInitialization
			= aControl == agent_protocol_event_kind::initialization_complete
			&& Phase == 1U;
		const auto followsThreadStart
			= aControl == agent_protocol_event_kind::context_channel_ready
			&& Phase == 3U;
		if (
			Failed || Protocol != core::agent_protocol::codex_app_server
			|| (!followsInitialization && !followsThreadStart)) {
			Failed = true;
			return std::unexpected(agent_protocol_failure::causal_violation);
		}
		Phase = followsInitialization ? 2U : 4U;
		return { };
	}

	auto name(const agent_run_outcome aOutcome) noexcept -> std::string_view
	{
		switch (aOutcome) {
		case agent_run_outcome::success:
			return "success";
		case agent_run_outcome::failed_to_start:
			return "failed_to_start";
		case agent_run_outcome::workspace_unavailable:
			return "workspace_unavailable";
		case agent_run_outcome::nonzero_exit:
			return "nonzero_exit";
		case agent_run_outcome::protocol_failure:
			return "protocol_failure";
		case agent_run_outcome::cancelled:
			return "cancelled";
		case agent_run_outcome::timeout:
			return "timeout";
		case agent_run_outcome::stale_result:
			return "stale_result";
		}
		return "unknown";
	}

	auto finalize_agent_run(
		agent_run_result                    aResult,
		const core::agent_session&          aSession,
		const core::agent_revision_binding& aObserved) -> agent_run_result
	{
		if (
			aResult.Outcome == agent_run_outcome::success
			&& (aResult.Session != aSession.Id
				|| !core::may_treat_as_current(aSession, aObserved))) {
			aResult.Outcome = agent_run_outcome::stale_result;
			aResult.Events.clear();
		}
		return aResult;
	}
} // namespace bpr::application
