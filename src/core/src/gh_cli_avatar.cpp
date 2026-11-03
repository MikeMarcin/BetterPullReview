#include "bpr/core/gh_cli_avatar.hpp"

#include "bpr/core/identity_avatar_picture.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <limits>
#include <memory>
#include <utility>

namespace bpr::core {
	namespace {
		constexpr std::string_view size_query  = "?s=";
		constexpr std::string_view bot_segment = "%5Bbot%5D";

		constexpr std::array<std::string_view, 4> batch_names{ "answered", "cli_unavailable", "cli_signed_out", "refused" };

		[[nodiscard]] constexpr auto printable(const char aChar) noexcept -> bool
		{
			return aChar > ' ' && aChar < '\x7f';
		}

		[[nodiscard]] auto printable_token(const std::string_view aText, const std::size_t aLimit) noexcept -> bool
		{
			return !aText.empty() && aText.size() <= aLimit && std::ranges::all_of(aText, printable);
		}

		[[nodiscard]] auto state_name(const markdown_image_state aState) noexcept -> std::string_view
		{
			switch (aState) {
			case markdown_image_state::loaded:      return "loaded";
			case markdown_image_state::failed:      return "failed";
			case markdown_image_state::too_large:   return "too_large";
			case markdown_image_state::blocked:     return "blocked";
			case markdown_image_state::unsupported: return "unsupported";
			case markdown_image_state::loading:
			case markdown_image_state::over_limit:  break;
			}
			return { };
		}

		[[nodiscard]] auto state_of(const std::string_view aName) noexcept -> std::optional<markdown_image_state>
		{
			for (const auto state : { markdown_image_state::loaded, markdown_image_state::failed, markdown_image_state::too_large, markdown_image_state::blocked,
									  markdown_image_state::unsupported }) {
				if (state_name(state) == aName) {
					return state;
				}
			}
			return std::nullopt;
		}

		/// A decimal with no sign and no leading zero, at most \p aLimit.
		[[nodiscard]] auto decimal(const std::string_view aDigits, const std::uint64_t aLimit) noexcept -> std::optional<std::uint64_t>
		{
			if (aDigits.empty() || aDigits.size() > 20U || (aDigits.size() > 1U && aDigits.front() == '0')) {
				return std::nullopt;
			}
			auto        value  = std::uint64_t{ 0 };
			const auto* first  = std::to_address(aDigits.begin());
			const auto* end    = std::to_address(aDigits.end());
			const auto  result = std::from_chars(first, end, value);
			if (result.ec != std::errc{ } || result.ptr != end || value > aLimit) {
				return std::nullopt;
			}
			return value;
		}

		/// The words of one line, which ends at the next newline.
		[[nodiscard]] auto take_line(std::string_view& aText, const std::size_t aLimit) -> std::optional<std::string_view>
		{
			const auto end = aText.find('\n');
			if (end == std::string_view::npos || end > aLimit) {
				return std::nullopt;
			}
			const auto line = aText.substr(0U, end);
			aText.remove_prefix(end + 1U);
			return line;
		}

		[[nodiscard]] auto split_words(std::string_view aLine) -> std::vector<std::string_view>
		{
			std::vector<std::string_view> words;
			while (!aLine.empty()) {
				const auto end = aLine.find(' ');
				words.push_back(aLine.substr(0U, end));
				if (end == std::string_view::npos) {
					break;
				}
				aLine.remove_prefix(end + 1U);
				if (aLine.empty()) {
					// A trailing space makes an empty last word, which no field admits.
					words.emplace_back();
				}
			}
			return words;
		}

		/// The login as `admit_identity_avatar` writes it into a path segment.
		[[nodiscard]] auto avatar_segment(const std::string_view aSegment) -> bool
		{
			auto login = std::string{ aSegment };
			if (aSegment.ends_with(bot_segment)) {
				login = std::string{ aSegment.substr(0U, aSegment.size() - bot_segment.size()) } + "[bot]";
			}
			return identity_avatar_login(login) && identity_avatar_folded_login(login) == login;
		}
	} // namespace

	auto gh_cli_avatar_hostname(const std::string_view aHostname) noexcept -> bool
	{
		if (aHostname.empty() || aHostname.size() > 253U || aHostname.front() == '.' || aHostname.back() == '.' || aHostname.find("..") != std::string_view::npos) {
			return false;
		}
		return std::ranges::all_of(aHostname, [](const char aChar) { return (aChar >= 'a' && aChar <= 'z') || (aChar >= '0' && aChar <= '9') || aChar == '-' || aChar == '.'; });
	}

	auto gh_cli_avatar_policy(const std::string_view aHostname) -> markdown_image_fetch_policy
	{
		return { .Contract = markdown_image_host_contract::ghes, .Hostname = std::string{ aHostname }, .CredentialAvailable = true, .AvatarOrigins = true, .CliCredential = false };
	}

	auto admit_gh_cli_avatar_address(const std::string_view aAddress, const std::string_view aHostname)
		-> std::expected<markdown_image_hop, markdown_image_state>
	{
		if (!gh_cli_avatar_hostname(aHostname) || aAddress.size() > gh_cli_avatar_address_bytes) {
			return std::unexpected{ markdown_image_state::blocked };
		}
		const auto prefix = identity_avatar_api_prefix(aHostname);
		if (!aAddress.starts_with(prefix)) {
			return std::unexpected{ markdown_image_state::blocked };
		}
		const auto rest  = aAddress.substr(prefix.size());
		const auto query = rest.find(size_query);
		if (query == std::string_view::npos || !avatar_segment(rest.substr(0U, query))) {
			return std::unexpected{ markdown_image_state::blocked };
		}
		const auto size = decimal(rest.substr(query + size_query.size()), std::numeric_limits<std::uint32_t>::max());
		if (!size || std::ranges::find(identity_avatar_picture_sizes, static_cast<std::uint32_t>(*size)) == identity_avatar_picture_sizes.end()) {
			return std::unexpected{ markdown_image_state::blocked };
		}
		auto hop = admit_markdown_image_fetch(aAddress, gh_cli_avatar_policy(aHostname));
		if (!hop || !hop->Credential || hop->Address != aAddress) {
			return std::unexpected{ markdown_image_state::blocked };
		}
		return hop;
	}

	auto encode_gh_cli_avatar_requests(const std::span<const std::string> aAddresses) -> std::optional<std::string>
	{
		if (aAddresses.empty() || aAddresses.size() > gh_cli_avatar_batch_limit) {
			return std::nullopt;
		}
		auto text = std::string{ gh_cli_avatar_protocol } + '\n';
		for (const auto& address : aAddresses) {
			if (!printable_token(address, gh_cli_avatar_address_bytes)) {
				return std::nullopt;
			}
			text += address;
			text += '\n';
		}
		return text;
	}

	auto decode_gh_cli_avatar_requests(std::string_view aText) -> std::optional<std::vector<std::string>>
	{
		const auto header = take_line(aText, gh_cli_avatar_protocol.size());
		if (!header || *header != gh_cli_avatar_protocol) {
			return std::nullopt;
		}
		std::vector<std::string> addresses;
		while (!aText.empty()) {
			const auto line = take_line(aText, gh_cli_avatar_address_bytes);
			if (!line || !printable_token(*line, gh_cli_avatar_address_bytes) || addresses.size() == gh_cli_avatar_batch_limit) {
				return std::nullopt;
			}
			addresses.emplace_back(*line);
		}
		if (addresses.empty()) {
			return std::nullopt;
		}
		return addresses;
	}

	auto encode_gh_cli_avatar_batch(const gh_cli_avatar_batch& aBatch) -> std::optional<std::string>
	{
		const auto index = static_cast<std::size_t>(aBatch.State);
		if (index >= batch_names.size() || aBatch.Answers.size() > gh_cli_avatar_batch_limit
			|| (aBatch.State != gh_cli_avatar_batch_state::answered && !aBatch.Answers.empty())) {
			return std::nullopt;
		}
		auto text = std::string{ gh_cli_avatar_protocol } + ' ' + std::string{ batch_names[index] } + ' ' + std::to_string(aBatch.Answers.size()) + '\n';
		for (const auto& answer : aBatch.Answers) {
			const auto name    = state_name(answer.State);
			const auto loaded  = answer.State == markdown_image_state::loaded;
			const auto typed   = answer.ContentType.empty() || printable_token(answer.ContentType, gh_cli_avatar_content_type_bytes);
			const auto bounded = answer.Bytes.size() <= identity_avatar_encoded_bytes && loaded == !answer.Bytes.empty();
			if (name.empty() || answer.HttpStatus > 999U || !typed || !bounded) {
				return std::nullopt;
			}
			text += name;
			text += ' ';
			text += std::to_string(answer.HttpStatus);
			text += ' ';
			text += answer.ContentType.empty() ? std::string{ "-" } : answer.ContentType;
			text += ' ';
			text += std::to_string(answer.Bytes.size());
			text += '\n';
			text += answer.Bytes;
		}
		return text;
	}

	auto decode_gh_cli_avatar_batch(std::string_view aText, const std::size_t aExpected) -> std::optional<gh_cli_avatar_batch>
	{
		const auto header = take_line(aText, gh_cli_avatar_protocol.size() + 32U);
		if (!header) {
			return std::nullopt;
		}
		const auto words = split_words(*header);
		if (words.size() != 3U || words[0] != gh_cli_avatar_protocol) {
			return std::nullopt;
		}
		const auto named = std::ranges::find(batch_names, words[1]);
		const auto count = decimal(words[2], gh_cli_avatar_batch_limit);
		if (named == batch_names.end() || !count) {
			return std::nullopt;
		}
		auto batch  = gh_cli_avatar_batch{ };
		batch.State = static_cast<gh_cli_avatar_batch_state>(std::distance(batch_names.begin(), named));
		if (batch.State == gh_cli_avatar_batch_state::answered ? *count != aExpected : *count != 0U) {
			return std::nullopt;
		}
		for (std::size_t index = 0U; index < *count; ++index) {
			const auto line = take_line(aText, 64U + gh_cli_avatar_content_type_bytes);
			if (!line) {
				return std::nullopt;
			}
			const auto fields = split_words(*line);
			if (fields.size() != 4U) {
				return std::nullopt;
			}
			const auto state  = state_of(fields[0]);
			const auto status = decimal(fields[1], 999U);
			const auto length = decimal(fields[3], identity_avatar_encoded_bytes);
			const auto typed  = fields[2] == "-" || printable_token(fields[2], gh_cli_avatar_content_type_bytes);
			if (!state || !status || !length || !typed || *length > aText.size() || (*state == markdown_image_state::loaded) == (*length == 0U)) {
				return std::nullopt;
			}
			auto answer        = gh_cli_avatar_answer{ };
			answer.State       = *state;
			answer.HttpStatus  = static_cast<std::uint16_t>(*status);
			answer.ContentType = fields[2] == "-" ? std::string{ } : std::string{ fields[2] };
			answer.Bytes       = std::string{ aText.substr(0U, *length) };
			aText.remove_prefix(*length);
			batch.Answers.push_back(std::move(answer));
		}
		if (!aText.empty()) {
			return std::nullopt;
		}
		return batch;
	}
} // namespace bpr::core
