#include "bpr/core/markdown_image.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <utility>

namespace bpr::core {
	namespace {
		constexpr std::size_t maximum_tag_bytes       = 8U * 1024U;
		constexpr std::size_t maximum_tag_attributes  = 32U;
		constexpr std::size_t maximum_picture_sources = 8U;
		constexpr std::size_t maximum_scanned_bytes   = 256U * 1024U;
		constexpr std::size_t maximum_admissions      = 4U * markdown_image_references_per_body;

		/// GitHub's own image hosts a GitHub.com body may be served from.
		constexpr std::array github_image_hosts{
			std::string_view{ "camo.githubusercontent.com" },
			std::string_view{ "private-user-images.githubusercontent.com" },
			std::string_view{ "user-images.githubusercontent.com" },
			std::string_view{ "raw.githubusercontent.com" },
			std::string_view{ "avatars.githubusercontent.com" },
			std::string_view{ "objects.githubusercontent.com" },
			std::string_view{ "media.githubusercontent.com" },
			std::string_view{ "github.com" },
		};

		/// Storage a GitHub.com image host may redirect to: `<prefix><name>.s3.amazonaws.com`.
		constexpr std::array github_storage_prefixes{
			std::string_view{ "github-production-user-asset-" },
			std::string_view{ "github-production-repository-" },
		};
		constexpr std::string_view github_storage_suffix = ".s3.amazonaws.com";

		auto ascii_lower(std::string aText) -> std::string
		{
			std::ranges::transform(aText, aText.begin(), [](const char aChar) {
				return aChar >= 'A' && aChar <= 'Z' ? static_cast<char>(aChar + ('a' - 'A')) : aChar;
			});
			return aText;
		}

		auto equal_ascii_insensitive(const std::string_view aLeft, const std::string_view aRight) -> bool
		{
			return aLeft.size() == aRight.size()
				&& std::ranges::equal(aLeft, aRight, [](const char aA, const char aB) {
					   const auto lower = [](const char aChar) { return aChar >= 'A' && aChar <= 'Z' ? static_cast<char>(aChar + ('a' - 'A')) : aChar; };
					   return lower(aA) == lower(aB);
				   });
		}

		auto hex_value(const char aChar) -> int
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

		auto ascii_space(const char aChar) -> bool
		{
			return aChar == ' ' || aChar == '\t' || aChar == '\n' || aChar == '\r' || aChar == '\f';
		}

		/// Whitespace length at `aIndex`: ASCII space, or U+2028/U+2029, which a
		/// reader's document uses for line breaks inside one paragraph.
		auto space_at(const std::string_view aText, const std::size_t aIndex) -> std::size_t
		{
			if (aIndex >= aText.size()) {
				return 0U;
			}
			if (ascii_space(aText[aIndex])) {
				return 1U;
			}
			if (aText.substr(aIndex, 3) == "\xE2\x80\xA8" || aText.substr(aIndex, 3) == "\xE2\x80\xA9") {
				return 3U;
			}
			return 0U;
		}

		auto skip_space(const std::string_view aText, std::size_t aIndex) -> std::size_t
		{
			for (auto width = space_at(aText, aIndex); width != 0U; width = space_at(aText, aIndex)) {
				aIndex += width;
			}
			return aIndex;
		}

		auto trim_ascii(std::string_view aText) -> std::string_view
		{
			while (!aText.empty() && ascii_space(aText.front())) {
				aText.remove_prefix(1);
			}
			while (!aText.empty() && ascii_space(aText.back())) {
				aText.remove_suffix(1);
			}
			return aText;
		}

		void append_utf8(std::string& aOut, const char32_t aCode)
		{
			if (aCode < 0x80U) {
				aOut += static_cast<char>(aCode);
			} else if (aCode < 0x800U) {
				aOut += static_cast<char>(0xC0U | (aCode >> 6U));
				aOut += static_cast<char>(0x80U | (aCode & 0x3FU));
			} else if (aCode < 0x10000U) {
				aOut += static_cast<char>(0xE0U | (aCode >> 12U));
				aOut += static_cast<char>(0x80U | ((aCode >> 6U) & 0x3FU));
				aOut += static_cast<char>(0x80U | (aCode & 0x3FU));
			} else {
				aOut += static_cast<char>(0xF0U | (aCode >> 18U));
				aOut += static_cast<char>(0x80U | ((aCode >> 12U) & 0x3FU));
				aOut += static_cast<char>(0x80U | ((aCode >> 6U) & 0x3FU));
				aOut += static_cast<char>(0x80U | (aCode & 0x3FU));
			}
		}

		/// Decodes the character references an attribute value can hold; an
		/// unknown or malformed reference stays as written.
		auto decode_references(const std::string_view aValue) -> std::string
		{
			std::string decoded;
			decoded.reserve(aValue.size());
			for (std::size_t index = 0; index < aValue.size(); ++index) {
				if (aValue[index] != '&') {
					decoded += aValue[index];
					continue;
				}
				const auto semicolon = aValue.find(';', index);
				if (semicolon == std::string_view::npos || semicolon - index > 10U) {
					decoded += '&';
					continue;
				}
				const auto name = aValue.substr(index + 1, semicolon - index - 1);
				if (name == "amp") {
					decoded += '&';
				} else if (name == "lt") {
					decoded += '<';
				} else if (name == "gt") {
					decoded += '>';
				} else if (name == "quot") {
					decoded += '"';
				} else if (name == "apos") {
					decoded += '\'';
				} else if (name.size() >= 2U && name.front() == '#') {
					const auto    hexadecimal = name[1] == 'x' || name[1] == 'X';
					const auto    digits      = name.substr(hexadecimal ? 2U : 1U);
					std::uint32_t code        = 0U;
					const auto    parsed      = std::from_chars(digits.data(), digits.data() + digits.size(), code, hexadecimal ? 16 : 10);
					if (digits.empty() || parsed.ec != std::errc{ } || parsed.ptr != digits.data() + digits.size() || code == 0U
						|| code > 0x10FFFFU || (code >= 0xD800U && code <= 0xDFFFU)) {
						decoded += '&';
						continue;
					}
					append_utf8(decoded, static_cast<char32_t>(code));
				} else {
					decoded += '&';
					continue;
				}
				index = semicolon;
			}
			return decoded;
		}

		struct html_attribute
		{
			std::string Name;
			std::string Value;
		};

		struct html_tag
		{
			std::string                 Name;
			bool                        Closing = false;
			std::vector<html_attribute> Attributes;
			std::size_t                 End = 0U;

			[[nodiscard]] auto attribute(const std::string_view aName) const -> const std::string*
			{
				const auto found = std::ranges::find(Attributes, aName, &html_attribute::Name);
				return found == Attributes.end() ? nullptr : &found->Value;
			}
		};

		auto name_character(const char aChar) -> bool
		{
			return (aChar >= 'a' && aChar <= 'z') || (aChar >= 'A' && aChar <= 'Z') || (aChar >= '0' && aChar <= '9');
		}

		auto attribute_name_character(const char aChar) -> bool
		{
			return !ascii_space(aChar) && aChar != '"' && aChar != '\'' && aChar != '>' && aChar != '/' && aChar != '=' && aChar != '<'
				&& static_cast<unsigned char>(aChar) > 0x20U;
		}

		/// One tag starting at `aBegin` ('<'): its lowercase name, whether it
		/// closes, and its attributes with references decoded. The first
		/// occurrence of an attribute wins, as in HTML.
		auto parse_tag(const std::string_view aText, const std::size_t aBegin) -> std::optional<html_tag>
		{
			const auto limit = std::min(aText.size(), aBegin + maximum_tag_bytes);
			if (aBegin >= limit || aText[aBegin] != '<') {
				return std::nullopt;
			}
			auto tag   = html_tag{ };
			auto index = aBegin + 1;
			if (index < limit && aText[index] == '/') {
				tag.Closing = true;
				++index;
			}
			const auto nameBegin = index;
			while (index < limit && name_character(aText[index])) {
				++index;
			}
			if (index == nameBegin || !((aText[nameBegin] >= 'a' && aText[nameBegin] <= 'z') || (aText[nameBegin] >= 'A' && aText[nameBegin] <= 'Z'))) {
				return std::nullopt;
			}
			tag.Name = ascii_lower(std::string{ aText.substr(nameBegin, index - nameBegin) });
			while (index < limit) {
				const auto spaced = skip_space(aText, index);
				const auto gap    = spaced != index;
				index             = spaced;
				if (index >= limit) {
					return std::nullopt;
				}
				if (aText[index] == '>') {
					tag.End = index + 1;
					return tag;
				}
				if (aText[index] == '/' && index + 1 < limit && aText[index + 1] == '>' && !tag.Closing) {
					tag.End = index + 2;
					return tag;
				}
				if (tag.Closing || !gap || tag.Attributes.size() >= maximum_tag_attributes) {
					return std::nullopt;
				}
				const auto attributeBegin = index;
				while (index < limit && attribute_name_character(aText[index])) {
					++index;
				}
				if (index == attributeBegin) {
					return std::nullopt;
				}
				auto       attribute = html_attribute{ .Name = ascii_lower(std::string{ aText.substr(attributeBegin, index - attributeBegin) }), .Value = { } };
				const auto afterName = skip_space(aText, index);
				if (afterName < limit && aText[afterName] == '=') {
					index = skip_space(aText, afterName + 1);
					if (index >= limit) {
						return std::nullopt;
					}
					if (aText[index] == '"' || aText[index] == '\'') {
						const auto quote = aText[index];
						const auto close = aText.find(quote, index + 1);
						if (close == std::string_view::npos || close >= limit) {
							return std::nullopt;
						}
						attribute.Value = decode_references(aText.substr(index + 1, close - index - 1));
						index           = close + 1;
					} else {
						const auto valueBegin = index;
						while (index < limit && space_at(aText, index) == 0U && aText[index] != '>' && aText[index] != '"' && aText[index] != '\''
							   && aText[index] != '<' && aText[index] != '=' && aText[index] != '`') {
							++index;
						}
						if (index == valueBegin) {
							return std::nullopt;
						}
						attribute.Value = decode_references(aText.substr(valueBegin, index - valueBegin));
					}
				}
				if (tag.attribute(attribute.Name) == nullptr) {
					tag.Attributes.push_back(std::move(attribute));
				}
			}
			return std::nullopt;
		}

		auto first_srcset_candidate(const std::string_view aSrcset) -> std::string
		{
			auto text = trim_ascii(aSrcset);
			auto end  = std::size_t{ 0 };
			while (end < text.size() && !ascii_space(text[end])) {
				++end;
			}
			auto candidate = text.substr(0, end);
			if (end == text.size() && candidate.ends_with(',')) {
				candidate.remove_suffix(1);
			}
			return std::string{ candidate };
		}

		enum class colour_scheme : std::uint8_t {
			dark,
			light,
		};

		auto scheme_of(const std::string_view aMedia) -> std::optional<colour_scheme>
		{
			std::string compact;
			for (const auto ch : aMedia) {
				if (!ascii_space(ch)) {
					compact += ch;
				}
			}
			compact = ascii_lower(std::move(compact));
			if (compact == "(prefers-color-scheme:dark)") {
				return colour_scheme::dark;
			}
			if (compact == "(prefers-color-scheme:light)") {
				return colour_scheme::light;
			}
			return std::nullopt;
		}

		void read_image_tag(const html_tag& aTag, html_image_span& aSpan)
		{
			if (const auto* source = aTag.attribute("src")) {
				aSpan.Source = std::string{ trim_ascii(*source) };
			}
			if (const auto* alt = aTag.attribute("alt")) {
				aSpan.Alt = *alt;
			}
			if (const auto* width = aTag.attribute("width")) {
				aSpan.Hints.Width = parse_markdown_image_length(*width);
			}
			if (const auto* height = aTag.attribute("height")) {
				aSpan.Hints.Height = parse_markdown_image_length(*height);
			}
		}

		/// An `<img>` or a `<picture>` of colour-scheme sources ending in one `<img>`.
		auto parse_image_core(const std::string_view aText, const std::size_t aBegin) -> std::optional<html_image_span>
		{
			const auto tag = parse_tag(aText, aBegin);
			if (!tag || tag->Closing) {
				return std::nullopt;
			}
			auto span  = html_image_span{ };
			span.Begin = aBegin;
			if (tag->Name == "img") {
				read_image_tag(*tag, span);
				span.End = tag->End;
				return span;
			}
			if (tag->Name != "picture") {
				return std::nullopt;
			}
			auto index = tag->End;
			for (std::size_t sources = 0; sources <= maximum_picture_sources; ++sources) {
				index            = skip_space(aText, index);
				const auto inner = parse_tag(aText, index);
				if (!inner || inner->Closing) {
					return std::nullopt;
				}
				if (inner->Name == "img") {
					read_image_tag(*inner, span);
					const auto close = parse_tag(aText, skip_space(aText, inner->End));
					if (!close || !close->Closing || close->Name != "picture") {
						return std::nullopt;
					}
					span.End = close->End;
					return span;
				}
				const auto* media  = inner->attribute("media");
				const auto* srcset = inner->attribute("srcset");
				const auto  scheme = media != nullptr ? scheme_of(*media) : std::nullopt;
				if (inner->Name != "source" || !scheme || srcset == nullptr) {
					return std::nullopt;
				}
				auto& slot = *scheme == colour_scheme::dark ? span.DarkSource : span.LightSource;
				if (!slot) {
					slot = first_srcset_candidate(*srcset);
				}
				index = inner->End;
			}
			return std::nullopt;
		}

		auto parse_image_markup(const std::string_view aText, const std::size_t aBegin) -> std::optional<html_image_span>
		{
			const auto tag = parse_tag(aText, aBegin);
			if (!tag || tag->Closing) {
				return std::nullopt;
			}
			if (tag->Name != "a") {
				return parse_image_core(aText, aBegin);
			}
			const auto* href = tag->attribute("href");
			if (href == nullptr) {
				return std::nullopt;
			}
			auto inner = parse_image_core(aText, skip_space(aText, tag->End));
			if (!inner) {
				return std::nullopt;
			}
			const auto close = parse_tag(aText, skip_space(aText, inner->End));
			if (!close || !close->Closing || close->Name != "a") {
				return std::nullopt;
			}
			inner->Begin = aBegin;
			inner->End   = close->End;
			inner->Link  = std::string{ trim_ascii(*href) };
			return inner;
		}

		struct web_address
		{
			std::string Scheme;
			std::string Host;
			std::string Port;
			std::string Rest;
		};

		auto safe_host(const std::string_view aHost) -> bool
		{
			return !aHost.empty() && aHost.front() != '.' && aHost.back() != '.' && aHost.find("..") == std::string_view::npos
				&& std::ranges::all_of(aHost, [](const char aChar) {
					   return (aChar >= 'a' && aChar <= 'z') || (aChar >= 'A' && aChar <= 'Z') || (aChar >= '0' && aChar <= '9') || aChar == '-' || aChar == '.';
				   });
		}

		auto printable_address(const std::string_view aAddress) -> bool
		{
			return !aAddress.empty() && aAddress.size() <= markdown_image_source_bytes
				&& std::ranges::all_of(aAddress, [](const char aChar) {
					   const auto ch = static_cast<unsigned char>(aChar);
					   return ch > 0x20U && ch < 0x7FU && ch != '\\';
				   });
		}

		/// An absolute `scheme://host[:port]rest` with a DNS host, no userinfo.
		auto parse_web_address(const std::string_view aAddress) -> std::optional<web_address>
		{
			const auto separator = aAddress.find("://");
			if (separator == std::string_view::npos || separator == 0U) {
				return std::nullopt;
			}
			auto address         = web_address{ };
			address.Scheme       = ascii_lower(std::string{ aAddress.substr(0, separator) });
			const auto rest      = aAddress.substr(separator + 3);
			const auto authority = rest.substr(0, rest.find_first_of("/?#"));
			if (authority.find('@') != std::string_view::npos || authority.find('[') != std::string_view::npos) {
				return std::nullopt;
			}
			const auto colon = authority.find(':');
			address.Host     = ascii_lower(std::string{ authority.substr(0, colon) });
			if (colon != std::string_view::npos) {
				address.Port      = std::string{ authority.substr(colon + 1) };
				unsigned   port   = 0U;
				const auto parsed = std::from_chars(address.Port.data(), address.Port.data() + address.Port.size(), port);
				if (parsed.ec != std::errc{ } || parsed.ptr != address.Port.data() + address.Port.size() || port == 0U || port > 65535U) {
					return std::nullopt;
				}
			}
			if (!safe_host(address.Host)) {
				return std::nullopt;
			}
			address.Rest = std::string{ rest.substr(authority.size()) };
			return address;
		}

		auto uuid_text(const std::string_view aText) -> bool
		{
			if (aText.size() != 36U) {
				return false;
			}
			for (std::size_t index = 0; index < aText.size(); ++index) {
				const auto dash = index == 8U || index == 13U || index == 18U || index == 23U;
				if (dash ? aText[index] != '-' : hex_value(aText[index]) < 0) {
					return false;
				}
			}
			return true;
		}

		auto path_of(const std::string_view aAddress) -> std::string_view
		{
			return aAddress.substr(0, aAddress.find_first_of("?#"));
		}

		/// A relative path with `.` and `..` resolved; refused if it climbs out.
		auto normalized_relative(const std::string_view aPath) -> std::optional<std::string>
		{
			std::vector<std::string_view> parts;
			auto                          path = aPath;
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
			std::string result;
			for (const auto part : parts) {
				result += '/';
				result += part;
			}
			return result;
		}

		auto github_storage_host(const std::string_view aHost) -> bool
		{
			if (!aHost.ends_with(github_storage_suffix)) {
				return false;
			}
			return std::ranges::any_of(github_storage_prefixes, [aHost](const std::string_view aPrefix) {
				if (!aHost.starts_with(aPrefix) || aHost.size() <= aPrefix.size() + github_storage_suffix.size()) {
					return false;
				}
				const auto middle = aHost.substr(aPrefix.size(), aHost.size() - aPrefix.size() - github_storage_suffix.size());
				return std::ranges::all_of(middle, [](const char aChar) { return (aChar >= 'a' && aChar <= 'z') || (aChar >= '0' && aChar <= '9') || aChar == '-'; });
			});
		}

		auto credential_origin(const web_address& aAddress, const markdown_image_fetch_policy& aPolicy) -> bool
		{
			const auto hostname = ascii_lower(aPolicy.Hostname);
			return aPolicy.Contract == markdown_image_host_contract::ghes && aPolicy.CredentialAvailable && aAddress.Port.empty()
				&& (aAddress.Host == hostname || aAddress.Host == "media." + hostname);
		}

		auto admit_hop(const std::string_view aAddress, const bool aCredentialSoFar, const bool aRedirect,
					   const markdown_image_fetch_policy& aPolicy, const std::uint32_t aHop)
			-> std::expected<markdown_image_hop, markdown_image_state>
		{
			const auto address = std::string{ aAddress.substr(0, aAddress.find('#')) };
			if (!printable_address(address)) {
				return std::unexpected{ markdown_image_state::blocked };
			}
			const auto parsed = parse_web_address(address);
			if (!parsed || parsed->Scheme != "https" || (!parsed->Port.empty() && parsed->Port != "443")) {
				return std::unexpected{ markdown_image_state::blocked };
			}
			if (aPolicy.Contract == markdown_image_host_contract::github_com) {
				const auto known = std::ranges::find(github_image_hosts, std::string_view{ parsed->Host }) != github_image_hosts.end();
				if (!known && !(aRedirect && github_storage_host(parsed->Host))) {
					return std::unexpected{ markdown_image_state::blocked };
				}
				return markdown_image_hop{ .Address = address, .Credential = false, .Hop = aHop };
			}
			if (aPolicy.Hostname.empty()) {
				return std::unexpected{ markdown_image_state::blocked };
			}
			return markdown_image_hop{ .Address = address, .Credential = aCredentialSoFar && credential_origin(*parsed, aPolicy), .Hop = aHop };
		}

		auto resolved_length(const markdown_image_length& aLength, const std::uint32_t aAvailable) -> std::uint32_t
		{
			if (aLength.Unit == markdown_image_length::unit::percent) {
				return static_cast<std::uint32_t>((static_cast<std::uint64_t>(aAvailable) * aLength.Value) / 100U);
			}
			return aLength.Value;
		}

		auto scaled(const std::uint64_t aValue, const std::uint64_t aNumerator, const std::uint64_t aDenominator) -> std::uint32_t
		{
			if (aDenominator == 0U) {
				return 1U;
			}
			const auto value = (aValue * aNumerator + aDenominator / 2U) / aDenominator;
			return static_cast<std::uint32_t>(std::clamp<std::uint64_t>(value, 1U, markdown_image_side_pixels));
		}

		auto clamped_to_width(markdown_image_size aSize, const std::uint32_t aAvailable) -> markdown_image_size
		{
			aSize.Width          = std::max<std::uint32_t>(aSize.Width, 1U);
			aSize.Height         = std::max<std::uint32_t>(aSize.Height, 1U);
			const auto available = std::max<std::uint32_t>(aAvailable, 1U);
			if (aSize.Width > available) {
				aSize.Height = scaled(aSize.Height, available, aSize.Width);
				aSize.Width  = available;
			}
			return aSize;
		}
	} // namespace

	auto parse_markdown_image_length(const std::string_view aText) -> std::optional<markdown_image_length>
	{
		auto text  = trim_ascii(aText);
		auto end   = std::size_t{ 0 };
		auto value = std::uint32_t{ 0 };
		while (end < text.size() && end < 6U && text[end] >= '0' && text[end] <= '9') {
			value = value * 10U + static_cast<std::uint32_t>(text[end] - '0');
			++end;
		}
		if (end == 0U || (end < text.size() && text[end] >= '0' && text[end] <= '9')) {
			return std::nullopt;
		}
		auto rest = text.substr(end);
		if (rest.starts_with('.')) {
			auto fraction = std::size_t{ 1 };
			while (fraction < rest.size() && rest[fraction] >= '0' && rest[fraction] <= '9') {
				++fraction;
			}
			if (fraction == 1U) {
				return std::nullopt;
			}
			rest.remove_prefix(fraction);
		}
		rest = trim_ascii(rest);
		if (value == 0U) {
			return std::nullopt;
		}
		if (rest.empty() || equal_ascii_insensitive(rest, "px")) {
			return markdown_image_length{ .Unit = markdown_image_length::unit::pixels, .Value = std::min(value, markdown_image_side_pixels) };
		}
		if (rest == "%") {
			return markdown_image_length{ .Unit = markdown_image_length::unit::percent, .Value = std::min<std::uint32_t>(value, 100U) };
		}
		return std::nullopt;
	}

	auto scan_html_images(const std::string_view aText) -> std::vector<html_image_span>
	{
		std::vector<html_image_span> spans;
		const auto                   text  = aText.substr(0, std::min(aText.size(), maximum_scanned_bytes));
		auto                         index = std::size_t{ 0 };
		while (spans.size() < markdown_image_references_per_body) {
			index = text.find('<', index);
			if (index == std::string_view::npos) {
				break;
			}
			if (auto span = parse_image_markup(text, index)) {
				index = span->End;
				spans.push_back(std::move(*span));
			} else {
				++index;
			}
		}
		return spans;
	}

	auto markdown_linked_images(const std::string_view aMarkdown) -> std::vector<markdown_linked_image>
	{
		std::vector<markdown_linked_image> found;
		auto                               fence      = std::string_view{ };
		auto                               offset     = std::size_t{ 0 };
		const auto                         text       = aMarkdown.substr(0, std::min(aMarkdown.size(), maximum_scanned_bytes));
		const auto                         maximum    = 4U * markdown_image_references_per_body;
		while (offset < text.size() && found.size() < maximum) {
			const auto end  = text.find('\n', offset);
			auto       line = text.substr(offset, end == std::string_view::npos ? text.size() - offset : end - offset);
			offset += line.size() + 1U;
			auto indent = std::size_t{ 0 };
			while (indent < line.size() && line[indent] == ' ') {
				++indent;
			}
			const auto marker = line.substr(indent);
			if (!fence.empty()) {
				if (indent <= 3U && marker.starts_with(fence)) {
					fence = { };
				}
				continue;
			}
			if (indent <= 3U && (marker.starts_with("```") || marker.starts_with("~~~"))) {
				fence = marker.substr(0, 3);
				continue;
			}
			if (indent >= 4U || line.starts_with('\t')) {
				continue;
			}
			// Inline code spans hide whatever they hold.
			auto masked = std::string{ line };
			for (auto open = masked.find('`'); open != std::string::npos;) {
				auto run = open;
				while (run < masked.size() && masked[run] == '`') {
					++run;
				}
				const auto ticks = masked.substr(open, run - open);
				const auto close = masked.find(ticks, run);
				if (close == std::string::npos) {
					break;
				}
				std::fill(masked.begin() + static_cast<std::ptrdiff_t>(open), masked.begin() + static_cast<std::ptrdiff_t>(close + ticks.size()), ' ');
				open = masked.find('`', close + ticks.size());
			}
			for (auto start = masked.find("[!["); start != std::string::npos && found.size() < maximum; start = masked.find("[![", start + 3U)) {
				const auto altEnd = masked.find("](", start + 3U);
				if (altEnd == std::string::npos) {
					break;
				}
				auto cursor = altEnd + 2U;
				auto source = std::string{ };
				if (cursor < masked.size() && masked[cursor] == '<') {
					const auto close = masked.find('>', cursor);
					if (close == std::string::npos) {
						continue;
					}
					source = masked.substr(cursor + 1U, close - cursor - 1U);
					cursor = close + 1U;
				} else {
					const auto stop = masked.find_first_of(" )", cursor);
					if (stop == std::string::npos) {
						continue;
					}
					source = masked.substr(cursor, stop - cursor);
					cursor = stop;
				}
				while (cursor < masked.size() && masked[cursor] == ' ') {
					++cursor;
				}
				if (cursor < masked.size() && (masked[cursor] == '"' || masked[cursor] == '\'')) {
					const auto close = masked.find(masked[cursor], cursor + 1U);
					if (close == std::string::npos) {
						continue;
					}
					cursor = close + 1U;
					while (cursor < masked.size() && masked[cursor] == ' ') {
						++cursor;
					}
				}
				if (masked.compare(cursor, 3U, ")](") != 0) {
					continue;
				}
				cursor += 3U;
				const auto stop = masked.find_first_of(" )", cursor);
				if (stop == std::string::npos || masked[stop] != ')' || source.empty() || stop == cursor) {
					continue;
				}
				found.push_back({ .Source = std::move(source), .Link = masked.substr(cursor, stop - cursor) });
			}
		}
		return found;
	}

	auto served_markdown_images(const std::string_view aHtml) -> std::optional<std::vector<markdown_served_image>>
	{
		if (aHtml.size() > markdown_image_body_html_bytes) {
			return std::nullopt;
		}
		std::vector<markdown_served_image> served;
		auto                               tags  = std::size_t{ 0 };
		auto                               index = std::size_t{ 0 };
		while ((index = aHtml.find('<', index)) != std::string_view::npos) {
			const auto tag = parse_tag(aHtml, index);
			if (!tag || tag->Closing || (tag->Name != "img" && tag->Name != "source")) {
				++index;
				continue;
			}
			if (++tags > markdown_image_served_tags_per_body) {
				return std::nullopt;
			}
			const auto* source    = tag->Name == "img" ? tag->attribute("src") : tag->attribute("srcset");
			const auto* canonical = tag->attribute("data-canonical-src");
			auto        address   = source == nullptr ? std::string{ } : tag->Name == "img" ? std::string{ trim_ascii(*source) }
																							: first_srcset_candidate(*source);
			if (!address.empty()) {
				served.push_back({ .Source = std::move(address), .Canonical = canonical == nullptr ? std::string{ } : std::string{ trim_ascii(*canonical) } });
			}
			index = tag->End;
		}
		return served;
	}

	auto admit_markdown_image_source(const std::string_view aSource, const std::string_view aHostname)
		-> std::expected<markdown_image_source, markdown_image_state>
	{
		const auto source = trim_ascii(aSource);
		if (!printable_address(source) || source.starts_with("//")) {
			return std::unexpected{ markdown_image_state::blocked };
		}
		const auto schemeEnd = source.find(':');
		const auto pathStart = source.find_first_of("/?#");
		if (schemeEnd != std::string_view::npos && (pathStart == std::string_view::npos || schemeEnd < pathStart)) {
			const auto address = parse_web_address(source);
			if (!address || (address->Scheme != "http" && address->Scheme != "https")) {
				return std::unexpected{ markdown_image_state::blocked };
			}
			const auto  path   = path_of(address->Rest);
			const auto  prefix = std::string_view{ "/user-attachments/assets/" };
			const auto& host   = address->Host;
			if (address->Scheme == "https" && address->Port.empty() && host == ascii_lower(std::string{ aHostname }) && path.starts_with(prefix)
				&& uuid_text(path.substr(prefix.size())) && path.size() == prefix.size() + 36U) {
				return markdown_image_source{ .Kind = markdown_image_source_kind::attachment, .Written = std::string{ source }, .Canonical = ascii_lower(std::string{ path.substr(prefix.size()) }) };
			}
			return markdown_image_source{ .Kind = markdown_image_source_kind::absolute_web, .Written = std::string{ source }, .Canonical = std::string{ source } };
		}
		const auto path = path_of(source);
		if (path.empty() || !normalized_relative(path)) {
			return std::unexpected{ markdown_image_state::blocked };
		}
		return markdown_image_source{ .Kind = markdown_image_source_kind::repository_relative, .Written = std::string{ source }, .Canonical = std::string{ source } };
	}

	auto match_served_markdown_image(const markdown_image_source& aSource, const std::span<const markdown_served_image> aServed)
		-> std::optional<std::string>
	{
		switch (aSource.Kind) {
		case markdown_image_source_kind::absolute_web:
			for (const auto& served : aServed) {
				if (!served.Canonical.empty() && served.Canonical == aSource.Written) {
					return served.Source;
				}
			}
			for (const auto& served : aServed) {
				if (served.Source == aSource.Written) {
					return served.Source;
				}
			}
			return std::nullopt;
		case markdown_image_source_kind::attachment:
			for (const auto& served : aServed) {
				const auto path    = path_of(served.Source);
				const auto segment = ascii_lower(std::string{ path.substr(path.rfind('/') == std::string_view::npos ? 0U : path.rfind('/') + 1U) });
				const auto stem    = segment.substr(0, segment.find('.'));
				if (stem == aSource.Canonical || stem.ends_with("-" + aSource.Canonical) || served.Source == aSource.Written) {
					return served.Source;
				}
			}
			return std::nullopt;
		case markdown_image_source_kind::repository_relative: {
			const auto wanted = normalized_relative(path_of(aSource.Written));
			for (const auto& served : aServed) {
				if (served.Source == aSource.Written) {
					return served.Source;
				}
				if (wanted && served.Source.find("://") != std::string::npos && path_of(served.Source).ends_with(*wanted)) {
					return served.Source;
				}
			}
			return std::nullopt;
		}
		}
		return std::nullopt;
	}

	auto resolved_markdown_image_source(markdown_image_source aSource, const std::string_view aServed) -> markdown_image_source
	{
		if (aSource.Kind == markdown_image_source_kind::repository_relative) {
			aSource.Canonical = std::string{ path_of(aServed) };
		}
		return aSource;
	}

	auto admit_markdown_image_fetch(const std::string_view aServed, const markdown_image_fetch_policy& aPolicy)
		-> std::expected<markdown_image_hop, markdown_image_state>
	{
		return admit_hop(aServed, true, false, aPolicy, 0U);
	}

	auto admit_markdown_image_redirect(const markdown_image_hop& aPrevious, const std::string_view aLocation,
									   const markdown_image_fetch_policy& aPolicy) -> std::expected<markdown_image_hop, markdown_image_state>
	{
		if (aPrevious.Hop >= markdown_image_redirects) {
			return std::unexpected{ markdown_image_state::failed };
		}
		auto location = std::string{ trim_ascii(aLocation) };
		if (location.starts_with('/') && !location.starts_with("//")) {
			const auto previous = parse_web_address(aPrevious.Address);
			if (!previous) {
				return std::unexpected{ markdown_image_state::blocked };
			}
			location = previous->Scheme + "://" + previous->Host + (previous->Port.empty() ? std::string{ } : ":" + previous->Port) + location;
		}
		return admit_hop(location, aPrevious.Credential, true, aPolicy, aPrevious.Hop + 1U);
	}

	auto markdown_image_dimensions_admitted(const markdown_image_size aSize) noexcept -> bool
	{
		return aSize.Width != 0U && aSize.Height != 0U && aSize.Width <= markdown_image_side_pixels && aSize.Height <= markdown_image_side_pixels
			&& static_cast<std::uint64_t>(aSize.Width) * aSize.Height <= markdown_image_frame_pixels;
	}

	auto markdown_image_animation_admitted(const std::size_t aFrames, const std::size_t aDecodedBytes) noexcept -> bool
	{
		return aFrames <= markdown_image_animation_frames && aDecodedBytes <= markdown_image_decoded_bytes;
	}

	auto markdown_image_display_size(const markdown_image_size aNatural, const markdown_image_hints& aHints, const std::uint32_t aAvailableWidth)
		-> markdown_image_size
	{
		const auto natural = markdown_image_size{ std::max<std::uint32_t>(aNatural.Width, 1U), std::max<std::uint32_t>(aNatural.Height, 1U) };
		const auto width   = aHints.Width ? std::optional{ resolved_length(*aHints.Width, aAvailableWidth) } : std::nullopt;
		// A percentage height is relative to an auto-height container: browsers ignore it.
		const auto height = aHints.Height && aHints.Height->Unit == markdown_image_length::unit::pixels ? std::optional{ aHints.Height->Value } : std::nullopt;
		auto       size   = natural;
		if (width && height) {
			size = { *width, *height };
		} else if (width) {
			size = { *width, scaled(*width, natural.Height, natural.Width) };
		} else if (height) {
			size = { scaled(*height, natural.Width, natural.Height), *height };
		}
		return clamped_to_width(size, aAvailableWidth);
	}

	auto markdown_image_reserved_size(const markdown_image_hints& aHints, const std::uint32_t aAvailableWidth) -> std::optional<markdown_image_size>
	{
		if (!aHints.Width || !aHints.Height || aHints.Height->Unit != markdown_image_length::unit::pixels) {
			return std::nullopt;
		}
		return clamped_to_width({ resolved_length(*aHints.Width, aAvailableWidth), aHints.Height->Value }, aAvailableWidth);
	}

	auto admit_markdown_images(const markdown_image_intent& aIntent, const markdown_image_observation& aObservation)
		-> std::expected<std::vector<markdown_image_admission>, markdown_image_refusal>
	{
		if (!aObservation.Context || *aObservation.Context != aIntent.Context || aIntent.Context.Revision == 0U || !aObservation.BodyAvailable) {
			return std::unexpected{ markdown_image_refusal::stale };
		}
		if (!aObservation.Available) {
			return std::unexpected{ markdown_image_refusal::unavailable };
		}
		std::vector<markdown_image_admission> admissions;
		std::vector<std::uint32_t>            seen;
		for (const auto& reference : aIntent.References) {
			if (admissions.size() >= maximum_admissions) {
				break;
			}
			if (std::ranges::find(seen, reference.Ordinal) != seen.end()) {
				continue;
			}
			seen.push_back(reference.Ordinal);
			if (reference.Ordinal >= markdown_image_references_per_body) {
				admissions.push_back({ .Ordinal = reference.Ordinal, .Source = std::unexpected{ markdown_image_state::over_limit } });
				continue;
			}
			admissions.push_back({ .Ordinal = reference.Ordinal, .Source = admit_markdown_image_source(reference.Source, aObservation.Hostname) });
		}
		return admissions;
	}

	auto markdown_image_cache_key(const std::string_view aHostname, const std::uint64_t aAccount, const markdown_image_source& aSource)
		-> std::string
	{
		auto key = ascii_lower(std::string{ aHostname });
		key += '\n';
		key += std::to_string(aAccount);
		key += '\n';
		key += static_cast<char>('0' + static_cast<int>(aSource.Kind));
		key += '\n';
		key += aSource.Canonical;
		return key;
	}
} // namespace bpr::core
