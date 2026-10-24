#include "review_comment_permalink.hpp"

#include "github_http_helpers.hpp"
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
				// A backslash is a name byte, refused below; on Windows the generic
				// form would turn it into a separator first.
				if (aPath.u8string().find(u8'\\') != std::u8string::npos) {
					return std::nullopt;
				}
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
			if (std::ranges::any_of(raw, [](const unsigned char aByte) { return aByte < 0x20U || aByte == 0x7FU || aByte == '\\'; })) {
				return std::nullopt;
			}
			return percent_encode(raw, "/");
		}

		auto code_url_prefix(const tour_permalink_scope& aScope, const core::tour_code_destination& aSource) -> std::optional<std::string>
		{
			if (!application::valid_github_canonical_hostname(aScope.Hostname) || !core::valid(aScope.Repository) || !core::valid(aSource)) {
				return { };
			}

			// The destination path is UTF-8; a `std::string` path is the ANSI code page on Windows.
			const auto path = encoded_path(std::filesystem::path{ std::u8string{ aSource.Path.begin(), aSource.Path.end() } });
			if (!path) {
				return std::nullopt;
			}
			auto result = std::string{ "https://" } + aScope.Hostname + "/" + aScope.Repository.Owner
				+ "/" + aScope.Repository.Name + "/blob/" + std::string{ core::canonical_hex(aSource.Commit).view() } + "/" + *path;
			const auto dot       = path->rfind('.');
			const auto extension = dot == std::string::npos ? QString{ } : QString::fromStdString(path->substr(dot)).toLower();
			// Closed Markdown renderer suffixes; see the codec contract documentation.
			if (extension == ".md" || extension == ".markdown" || extension == ".mdown" || extension == ".mkdn" || extension == ".mkd" || extension == ".mdwn" || extension == ".mdx" || extension == ".litcoffee") {
				result += "?plain=1";
			}
			result += "#L";
			return result;
		}

		[[nodiscard]] auto source_url_prefix(const review_permalink_source& aSource) -> std::optional<std::string>
		{
			// Reading a closed review grants no publication authority.
			auto readableTarget = aSource.Target;
			readableTarget.Open = true;
			if (!application::valid(readableTarget) || !aSource.Endpoint || !core::valid(aSource.Comparison) || aSource.LineCount == 0U) {
				return std::nullopt;
			}
			const auto& endpoint  = *aSource.Endpoint;
			const auto& request   = endpoint.request();
			const auto& key       = aSource.Comparison;
			const auto& authority = request.WorkbenchAuthority;
			// RFC-BPR-0089, [[BPR-1706]]. A commit-filtered diff's side is the
			// file at the filter's commit on that side, not at the comparison's
			// right anchor (the head): its permalink names that commit. Only a
			// filtered diff's left side has a permalink.
			// [[BPR-1735]]. On the Whole PR diff a left line GitHub resolves
			// nowhere names its line at the comparison's left commit, whose
			// file the host found to be the merge base's.
			const auto  right = endpoint.side() == core::diff_side::right;
			const auto& blob  = right ? key.RightBlob : key.LeftBlob;
			if (!right && !key.Filter && request.Diff.Comparison.Left.Availability != core::anchor_availability::available) {
				return std::nullopt;
			}
			const auto& commit = key.Filter ? (right ? key.Filter->Right : key.Filter->Left)
				: right                     ? request.Diff.Comparison.Right.Commit
											: request.Diff.Comparison.Left.Commit;
			if (!application::valid(authority) || !blob || *blob != endpoint.blob()
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
			// The repository path as given: its separators are already `/`, and a
			// backslash in it is a name byte that `encoded_path` refuses.
			const auto bytes = endpoint.path().u8string();
			return code_url_prefix({ std::string{ aSource.Target.Hostname.value() }, aSource.Target.Repository },
								   { .Commit = commit, .Blob = endpoint.blob(), .Path = std::string{ bytes.begin(), bytes.end() } });
		}
	} // namespace

	auto compile_tour_permalink(const tour_permalink_scope& aScope, const core::tour_code_destination& aSource) -> std::optional<std::string>
	{
		const auto prefix = code_url_prefix(aScope, aSource);
		if (!prefix) {
			return { };
		}
		auto result = *prefix + std::to_string(std::uint64_t{ aSource.FirstLine } + 1U);
		if (aSource.LastLine != aSource.FirstLine) {
			result += "-L" + std::to_string(std::uint64_t{ aSource.LastLine } + 1U);
		}
		return result;
	}
	auto parse_tour_permalink(const tour_permalink_scope& aScope, std::string_view aBody) -> std::optional<core::tour_code_destination>
	{
		if (aBody.size() > core::maximum_tour_field_bytes || !application::valid_github_canonical_hostname(aScope.Hostname) || !core::valid(aScope.Repository)) {
			return { };
		}
		const auto newline = aBody.find('\n');
		if (newline == std::string_view::npos) {
			return { };
		}
		auto link = aBody.substr(0, newline);
		if (link.ends_with('\r')) {
			link.remove_suffix(1);
		}
		const auto after = aBody.substr(newline + 1);
		if (!after.starts_with('\n') && !after.starts_with("\r\n")) {
			return { };
		}
		const auto prefix = "https://" + aScope.Hostname + "/" + aScope.Repository.Owner + "/" + aScope.Repository.Name + "/blob/";
		if (!link.starts_with(prefix)) {
			return { };
		}
		const auto address = link.substr(prefix.size());
		const auto slash   = address.find('/');
		if (slash == std::string_view::npos) {
			return { };
		}
		const auto commit   = core::parse_git_commit_oid(address.substr(0, slash));
		const auto fragment = address.rfind("#L");
		if (!commit || fragment == std::string_view::npos || fragment <= slash) {
			return { };
		}
		auto path = address.substr(slash + 1, fragment - slash - 1);
		if (path.ends_with("?plain=1")) {
			path.remove_suffix(8);
		}
		std::string decoded;
		for (std::size_t i = 0; i < path.size(); ++i) {
			if (path[i] != '%') {
				decoded += path[i];
				continue;
			}
			if (i + 2 >= path.size()) {
				return { };
			}
			unsigned   value  = 0;
			const auto result = std::from_chars(path.data() + i + 1, path.data() + i + 3, value, 16);
			if (result.ec != std::errc{ } || result.ptr != path.data() + i + 3) {
				return { };
			}
			decoded += static_cast<char>(value);
			i += 2;
		}
		const auto lines  = address.substr(fragment + 2);
		const auto range  = lines.find("-L");
		const auto number = [](std::string_view aText) -> std::optional<std::uint32_t> {
			std::uint32_t value  = 0;
			const auto    result = std::from_chars(aText.data(), aText.data() + aText.size(), value);
			if (aText.empty() || aText.front() == '0' || result.ec != std::errc{ } || result.ptr != aText.data() + aText.size() || !value) {
				return { };
			}
			return value - 1U;
		};
		const auto first = number(lines.substr(0, range));
		const auto last  = range == std::string_view::npos ? first : number(lines.substr(range + 2));
		if (!first || !last) {
			return { };
		}
		auto       source    = core::tour_code_destination{ .Commit = *commit, .Path = std::move(decoded), .FirstLine = *first, .LastLine = *last };
		const auto canonical = compile_tour_permalink(aScope, source);
		return canonical && *canonical == link ? std::optional{ source } : std::nullopt;
	}

	auto compile_review_permalink_body(const review_permalink_source& aSource, const core::source_line_ref& aAnchor, const std::string_view aProse) -> std::optional<std::string>
	{
		const auto prefix = source_url_prefix(aSource);
		if (!application::valid(aSource.Target) || !prefix || !valid_prose(aProse) || aAnchor.Side != aSource.Endpoint->side()
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
			.Anchor = { .Comparison = core::comparison_digest(aSource.Comparison), .Side = aSource.Endpoint->side(), .Line = line - 1U },
			.Prose  = std::string{ prose },
		};
	}
} // namespace bpr::adapters::github::detail
