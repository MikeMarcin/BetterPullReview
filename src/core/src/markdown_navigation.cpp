#include "bpr/core/markdown_navigation.hpp"

#include <algorithm>
#include <charconv>
#include <vector>

namespace bpr::core {
	namespace {
		auto ascii_lower(std::string aText) -> std::string
		{
			std::ranges::transform(aText, aText.begin(), [](const char aChar) {
				return aChar >= 'A' && aChar <= 'Z' ? static_cast<char>(aChar + ('a' - 'A')) : aChar;
			});
			return aText;
		}

		auto hex(const char aChar) -> int
		{
			if (aChar >= '0' && aChar <= '9') {
				return aChar - '0';
			}
			if (aChar >= 'a' && aChar <= 'f') {
				return aChar - 'a' + 10;
			}
			if (aChar >= 'A' && aChar <= 'F') {
				return aChar - 'A' + 10;
			}
			return -1;
		}

		auto safe_text(const std::string_view aText) -> bool
		{
			for (std::size_t index = 0; index < aText.size(); ++index) {
				const auto ch = static_cast<unsigned char>(aText[index]);
				if (ch <= 32 || ch >= 127 || ch == '\\') {
					return false;
				}
				if (ch != '%') {
					continue;
				}
				if (index + 2 >= aText.size() || hex(aText[index + 1]) < 0 || hex(aText[index + 2]) < 0) {
					return false;
				}
				const auto decoded = hex(aText[index + 1]) * 16 + hex(aText[index + 2]);
				if (decoded <= 32 || decoded == 127 || decoded == '\\') {
					return false;
				}
				index += 2;
			}
			return true;
		}

		auto safe_name(const std::string_view aName) -> bool
		{
			return !aName.empty() && aName != "." && aName != ".."
				&& std::ranges::all_of(aName, [](const char aChar) {
					   return (aChar >= 'a' && aChar <= 'z') || (aChar >= 'A' && aChar <= 'Z')
						   || (aChar >= '0' && aChar <= '9') || aChar == '-' || aChar == '_' || aChar == '.';
				   });
		}

		auto safe_authority(std::string_view aAuthority) -> bool
		{
			// DNS/IPv4 hostnames and an optional decimal port; no userinfo, escapes or guessed hosts.
			const auto colon = aAuthority.find(':');
			if (colon != std::string_view::npos) {
				const auto portText = aAuthority.substr(colon + 1);
				unsigned   port     = 0;
				const auto parsed   = std::from_chars(portText.data(), portText.data() + portText.size(), port);
				if (parsed.ec != std::errc{ } || parsed.ptr != portText.data() + portText.size() || port == 0 || port > 65535) {
					return false;
				}
				aAuthority = aAuthority.substr(0, colon);
			}
			return safe_name(aAuthority) && aAuthority.front() != '.' && aAuthority.back() != '.'
				&& aAuthority.find("..") == std::string_view::npos && aAuthority.find('_') == std::string_view::npos;
		}

		auto carries_credentials(const std::string_view aAddress) -> bool
		{
			const auto query = aAddress.find('?');
			if (query == std::string_view::npos) {
				return false;
			}
			auto fields = aAddress.substr(query + 1);
			fields      = fields.substr(0, fields.find('#'));
			while (!fields.empty()) {
				const auto  end   = fields.find('&');
				const auto  field = fields.substr(0, end);
				const auto  key   = field.substr(0, field.find('='));
				std::string decoded;
				for (std::size_t index = 0; index < key.size(); ++index) {
					if (key[index] == '%') {
						decoded += static_cast<char>(hex(key[index + 1]) * 16 + hex(key[index + 2]));
						index += 2;
					} else {
						decoded += key[index];
					}
				}
				decoded = ascii_lower(std::move(decoded));
				if (decoded.find("token") != std::string::npos || decoded.find("password") != std::string::npos
					|| decoded.find("secret") != std::string::npos || decoded == "authorization" || decoded == "auth"
					|| decoded == "credential" || decoded == "signature" || decoded == "sig") {
					return true;
				}
				if (end == std::string_view::npos) {
					break;
				}
				fields.remove_prefix(end + 1);
			}
			return false;
		}

		auto absolute_web_address(const std::string_view aAddress) -> bool
		{
			const auto separator = aAddress.find("://");
			if (separator == std::string_view::npos) {
				return false;
			}
			const auto scheme = ascii_lower(std::string{ aAddress.substr(0, separator) });
			if (scheme != "http" && scheme != "https") {
				return false;
			}
			const auto rest = aAddress.substr(separator + 3);
			return safe_authority(rest.substr(0, rest.find_first_of("/?#")));
		}

		auto resolve_relative(const std::string_view aDestination, const markdown_link_observation& aObservation)
			-> std::optional<markdown_destination>
		{
			if (!safe_authority(aObservation.Hostname) || !safe_name(aObservation.Owner) || !safe_name(aObservation.Repository)) {
				return std::nullopt;
			}
			if (aDestination.starts_with('#') && aObservation.Number.value() != 0) {
				return markdown_destination{ markdown_destination_kind::pull_request_fragment, std::string{ aDestination } };
			}
			if (aDestination.starts_with('/') || aDestination.starts_with('?') || aDestination.find(':') != std::string_view::npos
				|| (aObservation.Revision.size() != 40 && aObservation.Revision.size() != 64)
				|| !std::ranges::all_of(aObservation.Revision, [](const char aChar) { return hex(aChar) >= 0; })) {
				return std::nullopt;
			}
			const auto suffix = aDestination.find_first_of("?#");
			auto       path   = aDestination.substr(0, suffix);
			// Encoded separators/dot segments have ambiguous browser normalization. Keep source readable.
			if (path.find('%') != std::string_view::npos) {
				return std::nullopt;
			}
			std::vector<std::string_view> parts;
			while (!path.empty()) {
				const auto end  = path.find('/');
				const auto part = path.substr(0, end);
				if (part == "..") {
					if (parts.empty()) {
						return std::nullopt;
					}
					parts.pop_back();
				} else if (!part.empty() && part != ".") {
					parts.push_back(part);
				}
				if (end == std::string_view::npos) {
					break;
				}
				path.remove_prefix(end + 1);
			}
			if (parts.empty()) {
				return std::nullopt;
			}
			auto result = std::string{ };
			for (const auto part : parts) {
				if (!result.empty()) {
					result += '/';
				}
				result += part;
			}
			if (suffix != std::string_view::npos) {
				result += aDestination.substr(suffix);
			}
			return markdown_destination{ markdown_destination_kind::repository_path, std::move(result) };
		}
	}

	auto admit_markdown_link(const markdown_link_intent& aIntent, const markdown_link_observation& aObservation)
		-> std::expected<markdown_destination, markdown_link_result>
	{
		if (!aObservation.Context || aIntent.Context != *aObservation.Context || aIntent.Context.Revision == 0
			|| !aObservation.BodyAvailable) {
			return std::unexpected(markdown_link_result::stale);
		}
		if (!aObservation.BrowserAvailable) {
			return std::unexpected(markdown_link_result::unavailable);
		}
		const auto& destination = aIntent.Destination;
		if (destination.empty() || destination.size() > 4096 || !safe_text(destination) || carries_credentials(destination)) {
			return std::unexpected(markdown_link_result::refused);
		}
		if (absolute_web_address(destination)) {
			return markdown_destination{ markdown_destination_kind::absolute_web, destination };
		}
		if (const auto relative = resolve_relative(destination, aObservation)) {
			return *relative;
		}
		return std::unexpected(markdown_link_result::refused);
	}
}
