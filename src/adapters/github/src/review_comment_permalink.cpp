#include "review_comment_permalink.hpp"

#include "github_mutation_contract.hpp"

#include <QString>

#include <algorithm>
#include <charconv>
#include <string>
#include <system_error>

namespace bpr::adapters::github::detail {
	namespace {
		[[nodiscard]] auto valid_utf8(const std::string_view aText) -> bool
		{
			return QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size())).toUtf8().toStdString() == aText;
		}

		[[nodiscard]] auto has_code_permalink(const std::string_view aText) -> bool
		{
			auto start = aText.find("https://");
			while (start != std::string_view::npos) {
				const auto end   = aText.find_first_of(" \t\r\n<>\"", start);
				const auto token = aText.substr(start, end == std::string_view::npos ? end : end - start);
				if (token.find("/blob/") != std::string_view::npos && token.find("#L") != std::string_view::npos) {
					return true;
				}
				start = end == std::string_view::npos ? end : aText.find("https://", end);
			}
			return false;
		}

		[[nodiscard]] auto valid_prose(const std::string_view aProse) -> bool
		{
			return !aProse.empty() && aProse.size() <= maximum_review_mutation_body_bytes
				&& aProse.front() != '\n' && aProse.front() != '\r'
				&& valid_utf8(aProse) && !has_code_permalink(aProse)
				&& !QString::fromUtf8(aProse.data(), static_cast<qsizetype>(aProse.size())).trimmed().isEmpty()
				&& std::none_of(aProse.begin(), aProse.end(), [](const unsigned char aByte) {
					   return (aByte < 0x20U && aByte != '\n' && aByte != '\r' && aByte != '\t') || aByte == 0x7FU;
				   });
		}

		[[nodiscard]] auto encoded_path(const std::filesystem::path& aPath) -> std::optional<std::string>
		{
			auto utf8 = std::u8string{ };
			try {
				utf8 = aPath.generic_u8string();
			} catch (const std::filesystem::filesystem_error&) {
				return std::nullopt;
			}
			const auto raw = std::string{ utf8.begin(), utf8.end() };
			if (raw.empty() || raw.size() > 4U * 1024U || raw.front() == '/' || !valid_utf8(raw)) {
				return std::nullopt;
			}
			auto start = std::size_t{ 0U };
			while (start <= raw.size()) {
				const auto end     = raw.find('/', start);
				const auto segment = std::string_view{ raw }.substr(start, end == std::string::npos ? end : end - start);
				if (segment.empty() || segment == "." || segment == "..") {
					return std::nullopt;
				}
				if (end == std::string::npos) {
					break;
				}
				start = end + 1U;
			}
			auto result = std::string{ };
			result.reserve(raw.size());
			constexpr std::string_view hex = "0123456789ABCDEF";
			for (const char character : raw) {
				const auto byte = static_cast<unsigned char>(character);
				if (byte < 0x20U || byte == 0x7FU || byte == '\\') {
					return std::nullopt;
				}
				if ((byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z')
					|| (byte >= '0' && byte <= '9') || byte == '-' || byte == '_' || byte == '.' || byte == '~' || byte == '/') {
					result.push_back(static_cast<char>(byte));
				} else {
					result.push_back('%');
					result.push_back(hex[byte >> 4U]);
					result.push_back(hex[byte & 0x0FU]);
				}
			}
			return result;
		}

		[[nodiscard]] auto source_url_prefix(const review_permalink_source& aSource) -> std::optional<std::string>
		{
			if (!application::valid(aSource.Target) || !aSource.Endpoint || !core::valid(aSource.Comparison) || aSource.LineCount == 0U) {
				return std::nullopt;
			}
			const auto& endpoint  = *aSource.Endpoint;
			const auto& request   = endpoint.request();
			const auto& key       = aSource.Comparison;
			const auto& authority = request.WorkbenchAuthority;
			const auto& commit    = request.Diff.Comparison.Right.Commit;
			if (!application::valid(authority) || endpoint.side() != core::diff_side::right || !key.RightBlob || *key.RightBlob != endpoint.blob()
				|| key.File != request.File.File || key.RightAnchor != request.Diff.Comparison.Right.Identity
				|| key.LeftAnchor != request.Diff.Comparison.Left.Identity || key.Authority != request.Diff.Authority
				|| key.Options != request.Diff.OptionsDigest || key.Algorithm != request.Diff.Algorithm
				|| authority.Host != aSource.Target.Semantic.Host || authority.Repository != aSource.Target.Semantic.Repository
				|| authority.PullRequest != aSource.Target.Semantic.PullRequest || authority.Number != aSource.Target.Number
				|| key.Authority.Profile != authority.Profile || key.Authority.ProfileRevision != authority.ProfileRevision
				|| key.Authority.HostAuthority != authority.HostAuthority || key.Authority.Account != authority.Account
				|| key.Authority.Viewer != authority.Viewer || key.Authority.Number != authority.Number
				|| key.Authority.Host != authority.Host || key.Repository != authority.Repository || key.PullRequest != authority.PullRequest
				|| !commit.valid() || commit.algorithm() != aSource.Target.Semantic.ExpectedHead.algorithm()
				|| endpoint.blob().algorithm() != commit.algorithm()
				|| request.Diff.Comparison.Right.Availability != core::anchor_availability::available) {
				return std::nullopt;
			}
			const auto path = encoded_path(endpoint.path());
			if (!path) {
				return std::nullopt;
			}
			auto result = std::string{ "https://" } + std::string{ aSource.Target.Hostname.value() } + "/" + aSource.Target.Repository.Owner
				+ "/" + aSource.Target.Repository.Name + "/blob/" + std::string{ core::canonical_hex(commit).view() } + "/" + *path;
			const auto dot       = path->rfind('.');
			const auto extension = dot == std::string::npos ? QString{ } : QString::fromStdString(path->substr(dot)).toLower();
			// Closed Markdown renderer suffixes; see the codec contract documentation.
			if (extension == ".md" || extension == ".markdown" || extension == ".mdown" || extension == ".mkdn" || extension == ".mkd" || extension == ".mdwn" || extension == ".mdx" || extension == ".litcoffee") {
				result += "?plain=1";
			}
			result += "#L";
			return result;
		}
	} // namespace

	auto compile_review_permalink_body(const review_permalink_source& aSource, const core::source_line_ref& aAnchor, const std::string_view aProse) -> std::optional<std::string>
	{
		const auto prefix = source_url_prefix(aSource);
		if (!prefix || !valid_prose(aProse) || aAnchor.Side != core::diff_side::right
			|| aAnchor.Comparison != core::comparison_digest(aSource.Comparison) || aAnchor.Line >= aSource.LineCount
			|| aSource.Endpoint->request().Diff.Comparison.Right.Commit != aSource.Target.Semantic.ExpectedHead) {
			return std::nullopt;
		}
		auto result = *prefix + std::to_string(static_cast<std::uint64_t>(aAnchor.Line) + 1U) + "\n\n";
		if (result.size() + aProse.size() > maximum_review_mutation_body_bytes) {
			return std::nullopt;
		}
		result.append(aProse);
		return result;
	}

	auto parse_review_permalink_body(const review_permalink_source& aSource, const std::string_view aBody) -> std::optional<parsed_review_permalink>
	{
		if (aBody.size() > maximum_review_mutation_body_bytes) {
			return std::nullopt;
		}
		const auto prefix    = source_url_prefix(aSource);
		const auto separator = aBody.find("\n\n");
		if (!prefix || separator == std::string_view::npos) {
			return std::nullopt;
		}
		const auto link  = aBody.substr(0U, separator);
		const auto prose = aBody.substr(separator + 2U);
		if (!link.starts_with(*prefix) || !valid_prose(prose)) {
			return std::nullopt;
		}
		const auto digits = link.substr(prefix->size());
		if (digits.empty() || digits.front() == '0') {
			return std::nullopt;
		}
		auto       line   = std::uint32_t{ 0U };
		const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), line);
		if (parsed.ec != std::errc{ } || parsed.ptr != digits.data() + digits.size() || line == 0U || line > aSource.LineCount) {
			return std::nullopt;
		}
		return parsed_review_permalink{
			.Anchor = { .Comparison = core::comparison_digest(aSource.Comparison), .Side = core::diff_side::right, .Line = line - 1U },
			.Prose  = std::string{ prose },
		};
	}
} // namespace bpr::adapters::github::detail
