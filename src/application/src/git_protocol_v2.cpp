#include "bpr/application/git_protocol_v2.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <limits>
#include <new>
#include <string_view>

namespace bpr::application {
	namespace {
		enum class packet_kind : std::uint8_t {
			data,
			flush,
			delimiter,
			response_end,
		};

		struct packet_view
		{
			packet_kind      Kind = packet_kind::data;
			std::string_view Payload;
		};

		struct decoded_packets
		{
			std::array<packet_view, maximum_git_protocol_v2_control_packets> Packets{ };
			std::size_t                                                      Count = 0U;
		};

		[[nodiscard]] constexpr auto protocol_error(
			const git_protocol_v2_failure aFailure) noexcept -> git_protocol_v2_error
		{
			return { .Failure = aFailure };
		}

		[[nodiscard]] constexpr auto hexadecimal_nibble(
			const char aCharacter) noexcept -> std::optional<std::uint8_t>
		{
			if (aCharacter >= '0' && aCharacter <= '9') {
				return static_cast<std::uint8_t>(aCharacter - '0');
			}
			if (aCharacter >= 'a' && aCharacter <= 'f') {
				return static_cast<std::uint8_t>(aCharacter - 'a' + 10);
			}
			if (aCharacter >= 'A' && aCharacter <= 'F') {
				return static_cast<std::uint8_t>(aCharacter - 'A' + 10);
			}
			return std::nullopt;
		}

		[[nodiscard]] constexpr auto decode_packet_length(
			const std::span<const std::byte, 4U> aHeader) noexcept
			-> std::optional<std::uint16_t>
		{
			std::uint16_t length = 0U;
			for (const auto byte : aHeader) {
				const auto nibble = hexadecimal_nibble(static_cast<char>(std::to_integer<unsigned char>(
					byte)));
				if (!nibble.has_value()) {
					return std::nullopt;
				}
				length = static_cast<std::uint16_t>(
					static_cast<std::uint16_t>(length << 4U) | *nibble);
			}
			return length;
		}

		[[nodiscard]] auto decode_control_packets(
			const std::span<const std::byte> aResponse)
			-> git_protocol_v2_result<decoded_packets>
		{
			if (
				aResponse.empty()
				|| aResponse.size() > maximum_git_smart_control_response_bytes) {
				return std::unexpected(
					protocol_error(git_protocol_v2_failure::response_limit));
			}

			decoded_packets decoded;
			std::size_t     cursor = 0U;
			while (cursor < aResponse.size()) {
				if (
					decoded.Count >= decoded.Packets.size()
					|| aResponse.size() - cursor < 4U) {
					return std::unexpected(
						protocol_error(git_protocol_v2_failure::malformed_packet));
				}

				const auto header        = std::span<const std::byte, 4U>{ aResponse.data() + cursor, 4U };
				const auto encodedLength = decode_packet_length(header);
				if (!encodedLength.has_value()) {
					return std::unexpected(
						protocol_error(git_protocol_v2_failure::malformed_packet));
				}
				cursor += 4U;

				packet_view packet;
				switch (*encodedLength) {
				case 0U:
					packet.Kind = packet_kind::flush;
					break;
				case 1U:
					packet.Kind = packet_kind::delimiter;
					break;
				case 2U:
					packet.Kind = packet_kind::response_end;
					break;
				case 3U:
					return std::unexpected(
						protocol_error(git_protocol_v2_failure::malformed_packet));
				default: {
					if (
						*encodedLength > maximum_git_protocol_v2_packet_bytes
						|| *encodedLength < 4U) {
						return std::unexpected(
							protocol_error(git_protocol_v2_failure::malformed_packet));
					}
					const auto payloadSize = static_cast<std::size_t>(*encodedLength) - 4U;
					if (payloadSize > aResponse.size() - cursor) {
						return std::unexpected(
							protocol_error(git_protocol_v2_failure::malformed_packet));
					}
					packet.Kind    = packet_kind::data;
					packet.Payload = {
						reinterpret_cast<const char*>(aResponse.data() + cursor),
						payloadSize,
					};
					cursor += payloadSize;
					break;
				}
				}
				decoded.Packets[decoded.Count++] = packet;
			}
			return decoded;
		}

		[[nodiscard]] constexpr auto text_line_content(
			std::string_view aLine) noexcept -> std::optional<std::string_view>
		{
			if (
				aLine.empty() || aLine.find('\0') != std::string_view::npos
				|| aLine.find('\r') != std::string_view::npos) {
				return std::nullopt;
			}
			if (aLine.ends_with('\n')) {
				aLine.remove_suffix(1U);
			}
			if (aLine.empty() || aLine.find('\n') != std::string_view::npos) {
				return std::nullopt;
			}
			return aLine;
		}

		[[nodiscard]] constexpr auto valid_capability_key_character(
			const char aCharacter) noexcept -> bool
		{
			return (aCharacter >= 'a' && aCharacter <= 'z')
				|| (aCharacter >= 'A' && aCharacter <= 'Z')
				|| (aCharacter >= '0' && aCharacter <= '9') || aCharacter == '-'
				|| aCharacter == '_';
		}

		[[nodiscard]] constexpr auto valid_capability_value_character(
			const char aCharacter) noexcept -> bool
		{
			if (
				(aCharacter >= 'a' && aCharacter <= 'z')
				|| (aCharacter >= 'A' && aCharacter <= 'Z')
				|| (aCharacter >= '0' && aCharacter <= '9') || aCharacter == ' ') {
				return true;
			}
			constexpr std::string_view punctuation = "-_.,?/{}[]()<>!@#$%^&*+=:;";
			return punctuation.find(aCharacter) != std::string_view::npos;
		}

		struct parsed_capability
		{
			std::string_view                Key;
			std::optional<std::string_view> Value;
		};

		[[nodiscard]] auto parse_capability_line(const std::string_view aLine) noexcept
			-> std::optional<parsed_capability>
		{
			const auto content = text_line_content(aLine);
			if (!content.has_value()) {
				return std::nullopt;
			}

			const auto separator = content->find('=');
			const auto key       = content->substr(0U, separator);
			if (
				key.empty()
				|| !std::all_of(
					key.begin(),
					key.end(),
					valid_capability_key_character)) {
				return std::nullopt;
			}
			if (separator == std::string_view::npos) {
				return parsed_capability{ .Key = key };
			}

			const auto value = content->substr(separator + 1U);
			if (
				value.empty()
				|| !std::all_of(
					value.begin(),
					value.end(),
					valid_capability_value_character)) {
				return std::nullopt;
			}
			return parsed_capability{ .Key = key, .Value = value };
		}

		[[nodiscard]] constexpr auto object_format_name(
			const core::git_hash_algorithm aFormat) noexcept -> std::string_view
		{
			switch (aFormat) {
			case core::git_hash_algorithm::sha1:
				return "sha1";
			case core::git_hash_algorithm::sha256:
				return "sha256";
			case core::git_hash_algorithm::invalid:
				break;
			}
			return { };
		}

		[[nodiscard]] auto append_bytes(
			std::vector<std::byte>& aDestination,
			const std::string_view  aValue) -> bool
		{
			if (
				aDestination.size() > maximum_git_smart_request_bytes
				|| aValue.size()
					> maximum_git_smart_request_bytes - aDestination.size()) {
				return false;
			}
			aDestination.insert(
				aDestination.end(),
				reinterpret_cast<const std::byte*>(aValue.data()),
				reinterpret_cast<const std::byte*>(aValue.data() + aValue.size()));
			return true;
		}

		[[nodiscard]] auto append_packet(
			std::vector<std::byte>& aDestination,
			const std::string_view  aPayload) -> bool
		{
			const auto totalSize = aPayload.size() + 4U;
			if (
				aPayload.empty()
				|| totalSize > maximum_git_protocol_v2_packet_bytes
				|| totalSize > std::numeric_limits<std::uint16_t>::max()) {
				return false;
			}

			constexpr std::string_view digits = "0123456789abcdef";
			std::array<char, 4U>       header{ };
			auto                       remaining = totalSize;
			for (auto index = header.size(); index-- > 0U;) {
				header[index] = digits[remaining & 0x0fU];
				remaining >>= 4U;
			}
			return append_bytes(aDestination, { header.data(), header.size() })
				&& append_bytes(aDestination, aPayload);
		}

		[[nodiscard]] auto append_marker(
			std::vector<std::byte>& aDestination,
			const std::string_view  aMarker) -> bool
		{
			return aMarker.size() == 4U && append_bytes(aDestination, aMarker);
		}

		[[nodiscard]] auto append_request_prelude(
			std::vector<std::byte>&        aRequest,
			const std::string_view         aCommand,
			const core::git_hash_algorithm aFormat) -> bool
		{
			std::array<char, 32U>      objectFormatLine{ };
			constexpr std::string_view prefix     = "object-format=";
			auto                       output     = std::copy(prefix.begin(), prefix.end(), objectFormatLine.begin());
			const auto                 formatName = object_format_name(aFormat);
			output                                = std::copy(formatName.begin(), formatName.end(), output);
			*output++                             = '\n';

			return append_packet(aRequest, aCommand)
				&& append_packet(
					   aRequest,
					   { objectFormatLine.data(),
						 static_cast<std::size_t>(
							 output - objectFormatLine.begin()) })
				&& append_marker(aRequest, "0001");
		}

		[[nodiscard]] auto pull_request_ref(
			const core::pull_request_number aNumber) noexcept
			-> std::optional<std::array<char, 64U>>
		{
			if (!aNumber.valid()) {
				return std::nullopt;
			}

			std::array<char, 64U>      result{ };
			constexpr std::string_view prefix = "refs/pull/";
			constexpr std::string_view suffix = "/head";
			std::copy(prefix.begin(), prefix.end(), result.begin());
			auto*      begin    = result.data() + prefix.size();
			auto*      end      = result.data() + result.size() - suffix.size();
			const auto rendered = std::to_chars(begin, end, aNumber.value());
			if (rendered.ec != std::errc{ }) {
				return std::nullopt;
			}
			std::copy(suffix.begin(), suffix.end(), rendered.ptr);
			return result;
		}

		[[nodiscard]] auto pull_request_ref_view(
			const std::array<char, 64U>& aValue) noexcept -> std::string_view
		{
			const auto end = std::find(aValue.begin(), aValue.end(), '\0');
			return { aValue.data(), static_cast<std::size_t>(end - aValue.begin()) };
		}

		[[nodiscard]] constexpr auto expected_object_format(
			const git_smart_request_context& aContext) noexcept
			-> core::git_hash_algorithm
		{
			return aContext.Revision.Base.algorithm();
		}

		[[nodiscard]] auto valid_haves(
			const git_smart_request_context&            aContext,
			const std::span<const git_protocol_v2_have> aHaves,
			const core::git_hash_algorithm              aFormat) noexcept -> bool
		{
			return aHaves.size() <= maximum_git_protocol_v2_haves
				&& std::all_of(
					   aHaves.begin(),
					   aHaves.end(),
					   [&](const git_protocol_v2_have& aHave) {
						   return aHave.Cache == aContext.Authority.Cache
							   && aHave.Commit.valid()
							   && aHave.Commit.algorithm() == aFormat
							   && aHave.CatalogVisible && aHave.ObjectVerified;
					   });
		}

		[[nodiscard]] auto exact_lowercase_oid(
			const std::string_view         aText,
			const core::git_hash_algorithm aFormat) noexcept
			-> std::optional<core::git_commit_oid>
		{
			const auto expectedLength = aFormat == core::git_hash_algorithm::sha1
				? 40U
				: aFormat == core::git_hash_algorithm::sha256 ? 64U
															  : 0U;
			if (aText.size() != expectedLength) {
				return std::nullopt;
			}
			if (!std::all_of(
					aText.begin(),
					aText.end(),
					[](const char aCharacter) {
						return (aCharacter >= '0' && aCharacter <= '9')
							|| (aCharacter >= 'a' && aCharacter <= 'f');
					})) {
				return std::nullopt;
			}
			return core::parse_git_commit_oid(aText);
		}

		[[nodiscard]] auto render_oid_line(
			const std::string_view      aPrefix,
			const core::git_commit_oid& aOid,
			std::vector<std::byte>&     aRequest) -> bool
		{
			const auto            canonicalValue = core::canonical_hex(aOid);
			const auto            canonical      = canonicalValue.view();
			std::array<char, 80U> line{ };
			if (aPrefix.size() + canonical.size() + 1U > line.size()) {
				return false;
			}
			auto output = std::copy(aPrefix.begin(), aPrefix.end(), line.begin());
			output      = std::copy(canonical.begin(), canonical.end(), output);
			*output++   = '\n';
			return append_packet(
				aRequest,
				{ line.data(), static_cast<std::size_t>(output - line.begin()) });
		}

		[[nodiscard]] auto make_error(
			const git_pack_stream_failure aFailure) noexcept -> git_protocol_v2_error
		{
			return {
				.Failure     = git_protocol_v2_failure::pack_sink,
				.PackFailure = aFailure,
			};
		}

		class abort_once_pack_sink final : public git_pack_stream_sink
		{
		public:
			explicit abort_once_pack_sink(git_pack_stream_sink& aTarget) noexcept
				: Target{ aTarget }
			{
			}

			~abort_once_pack_sink() override
			{
				if (!Released) {
					abort();
				}
			}

			[[nodiscard]] auto append(
				const std::span<const std::byte> aBytes,
				const std::stop_token            aStop) -> git_pack_stream_result<void> override
			{
				return Target.append(aBytes, aStop);
			}

			[[nodiscard]] auto finish(const std::stop_token aStop)
				-> git_pack_stream_result<completed_git_pack_receipt> override
			{
				return Target.finish(aStop);
			}

			auto abort() noexcept -> void override
			{
				if (!Aborted) {
					Target.abort();
					Aborted = true;
				}
			}

			auto release() noexcept -> void
			{
				Released = true;
			}

		private:
			git_pack_stream_sink& Target;
			bool                  Aborted  = false;
			bool                  Released = false;
		};
	} // namespace

	auto valid(const git_protocol_v2_fetch_limits& aLimits) noexcept -> bool
	{
		return aLimits.MaximumWireBytes != 0U
			&& aLimits.MaximumWireBytes
			<= maximum_git_smart_fetch_response_bytes
			&& aLimits.MaximumPackBytes != 0U
			&& aLimits.MaximumPackBytes <= maximum_git_protocol_v2_pack_bytes
			&& aLimits.MaximumProgressBytes
			<= maximum_git_protocol_v2_progress_bytes
			&& aLimits.MaximumPackets != 0U
			&& aLimits.MaximumPackets <= maximum_git_protocol_v2_fetch_packets;
	}

	namespace detail {
		auto valid(const git_protocol_v2_capabilities& aCapabilities) noexcept -> bool
		{
			return aCapabilities.LsRefs && aCapabilities.Fetch
				&& !object_format_name(aCapabilities.ObjectFormat).empty();
		}

		auto parse_git_protocol_v2_advertisement(
			const std::span<const std::byte> aResponse,
			const core::git_hash_algorithm   aExpectedFormat)
			-> git_protocol_v2_result<git_protocol_v2_capabilities>
		{
			if (object_format_name(aExpectedFormat).empty()) {
				return std::unexpected(
					protocol_error(git_protocol_v2_failure::invalid_request));
			}

			const auto decoded = decode_control_packets(aResponse);
			if (!decoded.has_value()) {
				return std::unexpected(decoded.error());
			}
			// Smart HTTP may prepend its service announcement to the v2 stream.
			// Consume exactly that preamble; the version and capabilities still
			// pass through the same strict parser and aggregate packet bounds.
			std::size_t versionIndex = 0U;
			if (decoded->Count >= 2U
				&& decoded->Packets[0U].Kind == packet_kind::data
				&& text_line_content(decoded->Packets[0U].Payload)
					== std::optional<std::string_view>{ "# service=git-upload-pack" }) {
				if (decoded->Packets[1U].Kind != packet_kind::flush) {
					return std::unexpected(
						protocol_error(git_protocol_v2_failure::unexpected_section));
				}
				versionIndex = 2U;
			}
			if (
				decoded->Count < versionIndex + 2U
				|| decoded->Packets[versionIndex].Kind != packet_kind::data
				|| text_line_content(decoded->Packets[versionIndex].Payload)
					!= std::optional<std::string_view>{ "version 2" }) {
				return std::unexpected(
					protocol_error(git_protocol_v2_failure::unsupported_version));
			}

			git_protocol_v2_capabilities capabilities;
			std::array<std::string_view, maximum_git_protocol_v2_control_packets>
						seenKeys{ };
			std::size_t seenCount = 0U;
			bool        terminal  = false;
			for (std::size_t index = versionIndex + 1U; index < decoded->Count; ++index) {
				const auto& packet = decoded->Packets[index];
				if (packet.Kind == packet_kind::flush) {
					if (index + 1U != decoded->Count) {
						return std::unexpected(
							protocol_error(
								git_protocol_v2_failure::unexpected_section));
					}
					terminal = true;
					break;
				}
				if (packet.Kind != packet_kind::data) {
					return std::unexpected(
						protocol_error(git_protocol_v2_failure::unexpected_section));
				}

				const auto parsed = parse_capability_line(packet.Payload);
				if (!parsed.has_value()) {
					return std::unexpected(
						protocol_error(git_protocol_v2_failure::malformed_packet));
				}
				if (
					std::find(
						seenKeys.begin(),
						seenKeys.begin() + static_cast<std::ptrdiff_t>(seenCount),
						parsed->Key)
					!= seenKeys.begin()
						+ static_cast<std::ptrdiff_t>(seenCount)) {
					return std::unexpected(
						protocol_error(git_protocol_v2_failure::malformed_packet));
				}
				seenKeys[seenCount++] = parsed->Key;

				if (parsed->Key == "ls-refs") {
					capabilities.LsRefs = true;
				} else if (parsed->Key == "fetch") {
					capabilities.Fetch = true;
				} else if (parsed->Key == "object-format") {
					if (
						!parsed->Value.has_value()
						|| (*parsed->Value != "sha1" && *parsed->Value != "sha256")) {
						return std::unexpected(
							protocol_error(
								git_protocol_v2_failure::object_format_mismatch));
					}
					capabilities.ObjectFormat = *parsed->Value == "sha1"
						? core::git_hash_algorithm::sha1
						: core::git_hash_algorithm::sha256;
				}
			}

			if (!terminal) {
				return std::unexpected(
					protocol_error(git_protocol_v2_failure::incomplete_response));
			}
			if (
				!capabilities.LsRefs || !capabilities.Fetch
				|| capabilities.ObjectFormat == core::git_hash_algorithm::invalid) {
				return std::unexpected(
					protocol_error(git_protocol_v2_failure::missing_capability));
			}
			if (capabilities.ObjectFormat != aExpectedFormat) {
				return std::unexpected(
					protocol_error(git_protocol_v2_failure::object_format_mismatch));
			}
			return capabilities;
		}

		auto make_git_protocol_v2_ls_refs_request(
			const git_smart_request_context&    aContext,
			const git_protocol_v2_capabilities& aCapabilities)
			-> git_protocol_v2_result<std::vector<std::byte>>
		{
			if (
				!valid(aContext) || !valid(aCapabilities)
				|| aCapabilities.ObjectFormat != expected_object_format(aContext)) {
				return std::unexpected(
					protocol_error(git_protocol_v2_failure::invalid_request));
			}

			const auto prRefBuffer = pull_request_ref(aContext.Authority.Number);
			if (!prRefBuffer.has_value()) {
				return std::unexpected(
					protocol_error(git_protocol_v2_failure::invalid_request));
			}

			try {
				std::vector<std::byte> request;
				request.reserve(256U);

				std::array<char, maximum_git_base_branch_ref_bytes + 16U> baseLine{ };
				constexpr std::string_view                                refPrefix = "ref-prefix ";
				auto                                                      baseEnd   = std::copy(refPrefix.begin(), refPrefix.end(), baseLine.begin());
				baseEnd                                                             = std::copy(
					aContext.BaseRef.view().begin(),
					aContext.BaseRef.view().end(),
					baseEnd);
				*baseEnd++ = '\n';

				std::array<char, 80U> prLine{ };
				auto                  prEnd = std::copy(refPrefix.begin(), refPrefix.end(), prLine.begin());
				const auto            prRef = pull_request_ref_view(*prRefBuffer);
				prEnd                       = std::copy(prRef.begin(), prRef.end(), prEnd);
				*prEnd++                    = '\n';

				if (
					!append_request_prelude(
						request,
						"command=ls-refs\n",
						aCapabilities.ObjectFormat)
					|| !append_packet(
						request,
						{ baseLine.data(),
						  static_cast<std::size_t>(baseEnd - baseLine.begin()) })
					|| !append_packet(
						request,
						{ prLine.data(),
						  static_cast<std::size_t>(prEnd - prLine.begin()) })
					|| !append_marker(request, "0000")) {
					return std::unexpected(
						protocol_error(git_protocol_v2_failure::request_limit));
				}
				return request;
			} catch (const std::bad_alloc&) {
				return std::unexpected(
					protocol_error(git_protocol_v2_failure::allocation));
			}
		}

		auto parse_git_protocol_v2_ls_refs(
			const std::span<const std::byte> aResponse,
			const git_smart_request_context& aContext)
			-> git_protocol_v2_result<git_protocol_v2_refs>
		{
			if (!valid(aContext)) {
				return std::unexpected(
					protocol_error(git_protocol_v2_failure::invalid_request));
			}
			const auto prRefBuffer = pull_request_ref(aContext.Authority.Number);
			if (!prRefBuffer.has_value()) {
				return std::unexpected(
					protocol_error(git_protocol_v2_failure::invalid_request));
			}
			const auto prRef   = pull_request_ref_view(*prRefBuffer);
			const auto decoded = decode_control_packets(aResponse);
			if (!decoded.has_value()) {
				return std::unexpected(decoded.error());
			}

			git_protocol_v2_refs refs;
			bool                 baseSeen = false;
			bool                 headSeen = false;
			bool                 terminal = false;
			for (std::size_t index = 0U; index < decoded->Count; ++index) {
				const auto& packet = decoded->Packets[index];
				if (packet.Kind == packet_kind::flush) {
					if (index + 1U != decoded->Count) {
						return std::unexpected(
							protocol_error(
								git_protocol_v2_failure::unexpected_section));
					}
					terminal = true;
					break;
				}
				if (packet.Kind != packet_kind::data) {
					return std::unexpected(
						protocol_error(git_protocol_v2_failure::malformed_packet));
				}

				const auto line = text_line_content(packet.Payload);
				if (!line.has_value()) {
					return std::unexpected(
						protocol_error(git_protocol_v2_failure::malformed_packet));
				}
				const auto separator = line->find(' ');
				if (
					separator == std::string_view::npos
					|| line->find(' ', separator + 1U) != std::string_view::npos) {
					return std::unexpected(
						protocol_error(git_protocol_v2_failure::malformed_packet));
				}
				const auto oidText = line->substr(0U, separator);
				const auto refName = line->substr(separator + 1U);
				const auto oid     = exact_lowercase_oid(oidText, expected_object_format(aContext));
				if (!oid.has_value()) {
					return std::unexpected(
						protocol_error(git_protocol_v2_failure::oid_mismatch));
				}

				if (refName == aContext.BaseRef.view()) {
					if (baseSeen) {
						return std::unexpected(
							protocol_error(git_protocol_v2_failure::duplicate_ref));
					}
					// [[BPR-0667]]. Where the base branch points now, which is
					// not what the pull request recorded and must not be
					// required to be: a base branch moves whenever anything
					// else merges, and demanding equality here made the review
					// history unavailable for nearly every pull request on an
					// active repository. The base object's identity is
					// guaranteed where it can be: the fetch wants that exact
					// oid and `git_prepare_revision_request` prepares against
					// it, so a substituted object is still refused.
					refs.Base = *oid;
					baseSeen  = true;
				} else if (refName == prRef) {
					if (headSeen) {
						return std::unexpected(
							protocol_error(git_protocol_v2_failure::duplicate_ref));
					}
					if (*oid != aContext.Revision.Head) {
						return std::unexpected(
							protocol_error(git_protocol_v2_failure::oid_mismatch));
					}
					refs.Head = *oid;
					headSeen  = true;
				} else {
					return std::unexpected(
						protocol_error(git_protocol_v2_failure::unexpected_ref));
				}
			}
			if (!terminal || !baseSeen || !headSeen) {
				return std::unexpected(
					protocol_error(git_protocol_v2_failure::incomplete_response));
			}
			return refs;
		}

		auto make_git_protocol_v2_fetch_request(
			const git_smart_request_context&            aContext,
			const git_protocol_v2_capabilities&         aCapabilities,
			const std::span<const git_protocol_v2_have> aHaves,
			const std::optional<core::git_commit_oid>   aHistoricalHead)
			-> git_protocol_v2_result<std::vector<std::byte>>
		{
			if (
				!valid(aContext) || !valid(aCapabilities)
				|| (aHistoricalHead && (!aHistoricalHead->valid() || aHistoricalHead->algorithm() != expected_object_format(aContext)))
				|| aCapabilities.ObjectFormat != expected_object_format(aContext)
				|| !valid_haves(aContext, aHaves, aCapabilities.ObjectFormat)) {
				return std::unexpected(
					protocol_error(git_protocol_v2_failure::invalid_request));
			}

			std::array<core::git_commit_oid, maximum_git_protocol_v2_haves>
						deduplicatedHaves{ };
			std::size_t haveCount = 0U;
			for (const auto& have : aHaves) {
				const auto end = deduplicatedHaves.begin()
					+ static_cast<std::ptrdiff_t>(haveCount);
				if (
					std::find(
						deduplicatedHaves.begin(),
						end,
						have.Commit)
					== end) {
					deduplicatedHaves[haveCount++] = have.Commit;
				}
			}

			try {
				std::vector<std::byte> request;
				request.reserve(512U + haveCount * 72U);

				if (
					!append_request_prelude(
						request,
						"command=fetch\n",
						aCapabilities.ObjectFormat)
					|| !render_oid_line("want ", aHistoricalHead.value_or(aContext.Revision.Base), request)
					|| (!aHistoricalHead && aContext.Revision.Head != aContext.Revision.Base
						&& !render_oid_line(
							"want ",
							aContext.Revision.Head,
							request))) {
					return std::unexpected(
						protocol_error(git_protocol_v2_failure::request_limit));
				}
				for (std::size_t index = 0U; index < haveCount; ++index) {
					if (!render_oid_line("have ", deduplicatedHaves[index], request)) {
						return std::unexpected(
							protocol_error(git_protocol_v2_failure::request_limit));
					}
				}
				if (
					!append_packet(request, "done\n")
					|| !append_marker(request, "0000")) {
					return std::unexpected(
						protocol_error(git_protocol_v2_failure::request_limit));
				}
				return request;
			} catch (const std::bad_alloc&) {
				return std::unexpected(
					protocol_error(git_protocol_v2_failure::allocation));
			}
		}

		git_protocol_v2_fetch_parser::git_protocol_v2_fetch_parser(
			git_pack_stream_sink&              aSink,
			const git_protocol_v2_fetch_limits aLimits,
			const core::git_hash_algorithm     aExpectedFormat,
			const std::stop_token              aStop) noexcept
			: Sink{ &aSink }
			, Limits{ aLimits }
			, ExpectedFormat{ aExpectedFormat }
			, Stop{ aStop }
		{
			if (
				!valid(aLimits)
				|| object_format_name(aExpectedFormat).empty()) {
				static_cast<void>(
					fail(protocol_error(git_protocol_v2_failure::invalid_request)));
			}
		}

		git_protocol_v2_fetch_parser::~git_protocol_v2_fetch_parser()
		{
			if (State != response_state::finished) {
				abort_sink();
			}
		}

		auto git_protocol_v2_fetch_parser::fail(
			const git_protocol_v2_error aError) noexcept -> bool
		{
			if (!Error.has_value()) {
				Error = aError;
			}
			State = response_state::failed;
			abort_sink();
			return false;
		}

		auto git_protocol_v2_fetch_parser::begin_packet() noexcept -> bool
		{
			std::array<std::byte, 4U> header{ };
			std::transform(
				Header.begin(),
				Header.end(),
				header.begin(),
				[](const char aCharacter) {
					return static_cast<std::byte>(
						static_cast<unsigned char>(aCharacter));
				});
			const auto length = decode_packet_length(header);
			if (!length.has_value()) {
				return fail(protocol_error(git_protocol_v2_failure::malformed_packet));
			}
			if (Packets == Limits.MaximumPackets) {
				return fail(protocol_error(git_protocol_v2_failure::response_limit));
			}
			++Packets;

			if (*length <= 3U) {
				if (*length == 3U) {
					return fail(
						protocol_error(git_protocol_v2_failure::malformed_packet));
				}
				if (*length != 0U) {
					return fail(
						protocol_error(git_protocol_v2_failure::unexpected_section));
				}
				if (
					State != response_state::sideband || !SawPackData
					|| HeaderBytes != Header.size()) {
					return fail(
						protocol_error(git_protocol_v2_failure::incomplete_response));
				}
				State = response_state::terminal;
				reset_packet();
				return true;
			}
			if (
				*length > maximum_git_protocol_v2_packet_bytes
				|| *length < 4U) {
				return fail(protocol_error(git_protocol_v2_failure::malformed_packet));
			}
			if (State == response_state::terminal) {
				return fail(
					protocol_error(git_protocol_v2_failure::unexpected_section));
			}

			PayloadBytes    = static_cast<std::uint32_t>(*length - 4U);
			PayloadConsumed = 0U;
			if (State == response_state::packfile_header) {
				constexpr auto sectionNameBytes = std::string_view{ "packfile" }.size();
				if (
					PayloadBytes < sectionNameBytes
					|| PayloadBytes > SectionHeader.size()) {
					return fail(
						protocol_error(git_protocol_v2_failure::unexpected_section));
				}
			} else if (State == response_state::sideband && PayloadBytes < 1U) {
				return fail(protocol_error(git_protocol_v2_failure::malformed_packet));
			}
			return true;
		}

		auto git_protocol_v2_fetch_parser::consume_payload(
			const std::span<const std::byte> aBytes) noexcept -> bool
		{
			if (State == response_state::packfile_header) {
				if (
					SectionHeaderBytes > SectionHeader.size()
					|| aBytes.size() > SectionHeader.size() - SectionHeaderBytes) {
					return fail(
						protocol_error(git_protocol_v2_failure::malformed_packet));
				}
				for (const auto byte : aBytes) {
					if (SectionHeaderBytes == SectionHeader.size()) {
						return fail(
							protocol_error(
								git_protocol_v2_failure::malformed_packet));
					}
					SectionHeader[SectionHeaderBytes] = static_cast<char>(
						std::to_integer<unsigned char>(byte));
					++SectionHeaderBytes;
				}
				PayloadConsumed += static_cast<std::uint32_t>(aBytes.size());
				if (PayloadConsumed == PayloadBytes) {
					const auto section = text_line_content(
						{ SectionHeader.data(), SectionHeaderBytes });
					if (!section.has_value() || *section != "packfile") {
						return fail(
							protocol_error(
								git_protocol_v2_failure::unexpected_section));
					}
					State = response_state::sideband;
					reset_packet();
				}
				return true;
			}

			auto remaining = aBytes;
			if (!ChannelRead) {
				Channel     = std::to_integer<std::uint8_t>(remaining.front());
				ChannelRead = true;
				remaining   = remaining.subspan(1U);
				++PayloadConsumed;
				if (Channel == 3U) {
					return fail(
						protocol_error(git_protocol_v2_failure::sideband_error));
				}
				if (Channel != 1U && Channel != 2U) {
					return fail(
						protocol_error(git_protocol_v2_failure::malformed_packet));
				}
			}

			if (!remaining.empty()) {
				if (Channel == 1U) {
					if (
						remaining.size() > Limits.MaximumPackBytes - PackBytes) {
						return fail(
							protocol_error(git_protocol_v2_failure::pack_limit));
					}
					try {
						const auto appended = Sink->append(remaining, Stop);
						if (!appended.has_value()) {
							return fail(make_error(appended.error()));
						}
					} catch (...) {
						return fail(make_error(git_pack_stream_failure::storage));
					}
					PackBytes += remaining.size();
					SawPackData = true;
				} else {
					if (
						remaining.size()
						> Limits.MaximumProgressBytes - ProgressBytes) {
						return fail(
							protocol_error(git_protocol_v2_failure::progress_limit));
					}
					ProgressBytes += remaining.size();
				}
				PayloadConsumed += static_cast<std::uint32_t>(remaining.size());
			}

			if (PayloadConsumed == PayloadBytes) {
				reset_packet();
			}
			return true;
		}

		auto git_protocol_v2_fetch_parser::consume(
			const std::span<const std::byte> aBytes) noexcept -> bool
		{
			if (
				State == response_state::failed
				|| State == response_state::finished) {
				return false;
			}
			if (Stop.stop_requested()) {
				return fail(make_error(git_pack_stream_failure::cancelled));
			}
			if (aBytes.size() > Limits.MaximumWireBytes - WireBytes) {
				return fail(protocol_error(git_protocol_v2_failure::response_limit));
			}
			WireBytes += aBytes.size();

			std::size_t cursor = 0U;
			while (cursor < aBytes.size()) {
				if (State == response_state::terminal) {
					return fail(
						protocol_error(git_protocol_v2_failure::unexpected_section));
				}
				if (HeaderBytes < Header.size()) {
					const auto count = std::min(Header.size() - HeaderBytes, aBytes.size() - cursor);
					std::transform(
						aBytes.begin() + static_cast<std::ptrdiff_t>(cursor),
						aBytes.begin()
							+ static_cast<std::ptrdiff_t>(cursor + count),
						Header.begin() + static_cast<std::ptrdiff_t>(HeaderBytes),
						[](const std::byte aByte) {
							return static_cast<char>(
								std::to_integer<unsigned char>(aByte));
						});
					HeaderBytes += count;
					cursor += count;
					if (HeaderBytes != Header.size()) {
						continue;
					}
					if (!begin_packet()) {
						return false;
					}
					if (State == response_state::terminal) {
						continue;
					}
					if (cursor == aBytes.size()) {
						continue;
					}
				}

				const auto payloadRemaining = static_cast<std::size_t>(PayloadBytes - PayloadConsumed);
				const auto count            = std::min(payloadRemaining, aBytes.size() - cursor);
				if (count == 0U) {
					return fail(
						protocol_error(git_protocol_v2_failure::malformed_packet));
				}
				if (!consume_payload(aBytes.subspan(cursor, count))) {
					return false;
				}
				cursor += count;
			}
			return true;
		}

		auto git_protocol_v2_fetch_parser::finish()
			-> git_protocol_v2_result<git_protocol_v2_fetch_result>
		{
			if (Error.has_value()) {
				return std::unexpected(*Error);
			}
			if (
				State != response_state::terminal || HeaderBytes != 0U
				|| PayloadBytes != 0U || !SawPackData) {
				const auto error = protocol_error(git_protocol_v2_failure::incomplete_response);
				static_cast<void>(fail(error));
				return std::unexpected(error);
			}

			git_pack_stream_result<completed_git_pack_receipt> receipt = std::unexpected(git_pack_stream_failure::storage);
			try {
				receipt = Sink->finish(Stop);
			} catch (...) {
				const auto error = make_error(git_pack_stream_failure::storage);
				static_cast<void>(fail(error));
				return std::unexpected(error);
			}
			if (!receipt.has_value()) {
				const auto error = make_error(receipt.error());
				static_cast<void>(fail(error));
				return std::unexpected(error);
			}
			if (
				!agrees(*receipt)
				|| receipt->Spool.Bytes != PackBytes
				|| receipt->ExpectedPack.Algorithm != ExpectedFormat) {
				const auto error = make_error(git_pack_stream_failure::invalid_state);
				static_cast<void>(fail(error));
				return std::unexpected(error);
			}

			State = response_state::finished;
			return git_protocol_v2_fetch_result{
				.Pack          = *receipt,
				.WireBytes     = WireBytes,
				.PackBytes     = PackBytes,
				.ProgressBytes = ProgressBytes,
				.Packets       = Packets,
			};
		}

		auto git_protocol_v2_fetch_parser::abort() noexcept -> void
		{
			if (State != response_state::finished) {
				static_cast<void>(
					fail(protocol_error(git_protocol_v2_failure::incomplete_response)));
			}
		}

		auto git_protocol_v2_fetch_parser::error() const noexcept
			-> std::optional<git_protocol_v2_error>
		{
			return Error;
		}

		auto git_protocol_v2_fetch_parser::wire_bytes() const noexcept
			-> std::uint64_t
		{
			return WireBytes;
		}

		auto git_protocol_v2_fetch_parser::pack_bytes() const noexcept
			-> std::uint64_t
		{
			return PackBytes;
		}

		auto git_protocol_v2_fetch_parser::progress_bytes() const noexcept
			-> std::uint64_t
		{
			return ProgressBytes;
		}

		auto git_protocol_v2_fetch_parser::packets() const noexcept -> std::uint64_t
		{
			return Packets;
		}

		auto git_protocol_v2_fetch_parser::reset_packet() noexcept -> void
		{
			Header.fill('\0');
			HeaderBytes     = 0U;
			PayloadBytes    = 0U;
			PayloadConsumed = 0U;
			Channel         = 0U;
			ChannelRead     = false;
		}

		auto git_protocol_v2_fetch_parser::abort_sink() noexcept -> void
		{
			if (!SinkAborted) {
				Sink->abort();
				SinkAborted = true;
			}
		}

		static auto discover_git_protocol_v2(
			git_smart_transport_port&        aTransport,
			const git_smart_request_context& aContext,
			const std::stop_token            aStop)
			-> git_protocol_v2_client_result<git_protocol_v2_capabilities>
		{
			if (!valid(aContext)) {
				return std::unexpected(
					git_protocol_v2_client_error{
						protocol_error(git_protocol_v2_failure::invalid_request) });
			}
			const auto response = aTransport.discover(git_smart_discovery_request{ aContext }, aStop);
			if (!response.has_value()) {
				return std::unexpected(
					git_protocol_v2_client_error{ response.error() });
			}
			auto parsed = parse_git_protocol_v2_advertisement(
				response->Body,
				expected_object_format(aContext));
			if (!parsed.has_value()) {
				return std::unexpected(
					git_protocol_v2_client_error{ parsed.error() });
			}
			git_protocol_v2_client_result<git_protocol_v2_capabilities> result;
			*result = std::move(*parsed);
			return result;
		}

		static auto list_git_protocol_v2_refs(
			git_smart_transport_port&           aTransport,
			const git_smart_request_context&    aContext,
			const git_protocol_v2_capabilities& aCapabilities,
			const std::stop_token               aStop)
			-> git_protocol_v2_client_result<git_protocol_v2_refs>
		{
			const auto body = make_git_protocol_v2_ls_refs_request(aContext, aCapabilities);
			if (!body.has_value()) {
				return std::unexpected(
					git_protocol_v2_client_error{ body.error() });
			}
			const auto response = aTransport.ls_refs(
				git_smart_ls_refs_request{
					.Context = aContext,
					.Body    = *body,
				},
				aStop);
			if (!response.has_value()) {
				return std::unexpected(
					git_protocol_v2_client_error{ response.error() });
			}
			auto parsed = parse_git_protocol_v2_ls_refs(
				response->Body,
				aContext);
			if (!parsed.has_value()) {
				return std::unexpected(
					git_protocol_v2_client_error{ parsed.error() });
			}
			git_protocol_v2_client_result<git_protocol_v2_refs> result;
			*result = std::move(*parsed);
			return result;
		}

		static auto fetch_git_protocol_v2(
			git_smart_transport_port&                   aTransport,
			const git_smart_request_context&            aContext,
			const git_protocol_v2_capabilities&         aCapabilities,
			const std::span<const git_protocol_v2_have> aHaves,
			git_pack_stream_sink&                       aSink,
			const git_protocol_v2_fetch_limits          aLimits,
			const std::stop_token                       aStop,
			const std::optional<core::git_commit_oid>   aHistoricalHead)
			-> git_protocol_v2_client_result<git_protocol_v2_fetch_result>
		{
			const auto body = make_git_protocol_v2_fetch_request(aContext, aCapabilities, aHaves, aHistoricalHead);
			if (!body.has_value()) {
				aSink.abort();
				return std::unexpected(
					git_protocol_v2_client_error{ body.error() });
			}
			if (!valid(aLimits)) {
				aSink.abort();
				return std::unexpected(git_protocol_v2_client_error{
					protocol_error(git_protocol_v2_failure::invalid_request) });
			}

			git_protocol_v2_fetch_parser parser{
				aSink,
				aLimits,
				expected_object_format(aContext),
				aStop,
			};
			const auto transported = aTransport.fetch(
				git_smart_fetch_request{
					.Context              = aContext,
					.Body                 = *body,
					.MaximumResponseBytes = aLimits.MaximumWireBytes,
				},
				parser,
				aStop);
			if (!transported.has_value()) {
				if (
					transported.error().Failure
						== git_smart_transport_failure::consumer_rejected
					&& parser.error().has_value()) {
					return std::unexpected(
						git_protocol_v2_client_error{ *parser.error() });
				}
				parser.abort();
				return std::unexpected(
					git_protocol_v2_client_error{ transported.error() });
			}

			auto parsed = parser.finish();
			if (!parsed.has_value()) {
				return std::unexpected(
					git_protocol_v2_client_error{ parsed.error() });
			}
			git_protocol_v2_client_result<git_protocol_v2_fetch_result> result;
			*result = std::move(*parsed);
			return result;
		}

	} // namespace detail

	static auto acquire_git_protocol_v2_pack_impl(
		git_smart_transport_port&                   aTransport,
		const git_smart_request_context&            aContext,
		const std::span<const git_protocol_v2_have> aHaves,
		git_pack_stream_sink&                       aSink,
		const git_protocol_v2_fetch_limits          aLimits,
		const std::stop_token                       aStop,
		const std::optional<core::git_commit_oid>   aHistoricalHead)
		-> git_protocol_v2_client_result<git_protocol_v2_acquisition_result>
	{
		abort_once_pack_sink guardedSink{ aSink };
		const auto           cancelled = [] {
			return std::unexpected(git_protocol_v2_client_error{
				git_smart_transport_error{
					.Failure = git_smart_transport_failure::cancelled,
				} });
		};

		if (
			!valid(aContext) || !valid(aLimits)
			|| (aHistoricalHead && (!aHistoricalHead->valid() || aHistoricalHead->algorithm() != expected_object_format(aContext)))
			|| !valid_haves(
				aContext,
				aHaves,
				expected_object_format(aContext))) {
			return std::unexpected(git_protocol_v2_client_error{
				protocol_error(git_protocol_v2_failure::invalid_request) });
		}
		if (aStop.stop_requested()) {
			return cancelled();
		}
		const auto capabilities = detail::discover_git_protocol_v2(aTransport, aContext, aStop);
		if (!capabilities.has_value()) {
			return std::unexpected(capabilities.error());
		}

		if (aStop.stop_requested()) {
			return cancelled();
		}
		const auto refs = detail::list_git_protocol_v2_refs(
			aTransport,
			aContext,
			*capabilities,
			aStop);
		if (!refs.has_value()) {
			return std::unexpected(refs.error());
		}

		if (aStop.stop_requested()) {
			return cancelled();
		}
		const auto fetched = detail::fetch_git_protocol_v2(
			aTransport,
			aContext,
			*capabilities,
			aHaves,
			guardedSink,
			aLimits,
			aStop, aHistoricalHead);
		if (!fetched.has_value()) {
			return std::unexpected(fetched.error());
		}

		guardedSink.release();
		return git_protocol_v2_acquisition_result{
			.AdvertisedRefs = *refs,
			.Fetch          = *fetched,
		};
	}
	auto acquire_git_protocol_v2_pack(
		git_smart_transport_port&             aTransport,
		const git_smart_request_context&      aContext,
		std::span<const git_protocol_v2_have> aHaves,
		git_pack_stream_sink&                 aSink,
		git_protocol_v2_fetch_limits          aLimits,
		std::stop_token                       aStop)
		-> git_protocol_v2_client_result<git_protocol_v2_acquisition_result>
	{
		return acquire_git_protocol_v2_pack_impl(aTransport, aContext, aHaves, aSink, aLimits, aStop, std::nullopt);
	}

	auto acquire_git_protocol_v2_historical_pack(
		git_smart_transport_port&             aTransport,
		const git_smart_request_context&      aContext,
		core::git_commit_oid                  aHistoricalHead,
		std::span<const git_protocol_v2_have> aHaves,
		git_pack_stream_sink&                 aSink,
		git_protocol_v2_fetch_limits          aLimits,
		std::stop_token                       aStop)
		-> git_protocol_v2_client_result<git_protocol_v2_acquisition_result>
	{
		return acquire_git_protocol_v2_pack_impl(aTransport, aContext, aHaves, aSink, aLimits, aStop, aHistoricalHead);
	}

} // namespace bpr::application
