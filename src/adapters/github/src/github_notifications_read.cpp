#include "github_notifications_read.hpp"

#include "github_json_preflight.hpp"
#include "github_paged_read.hpp"

#include <glaze/glaze.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <optional>
#include <system_error>
#include <utility>

namespace bpr::adapters::github::detail {
	namespace {
		struct notification_subject_json
		{
			std::optional<std::string> Url;
			std::optional<std::string> LatestCommentUrl;
			std::string                Type;
			struct glaze
			{
				static constexpr auto value = glz::object("url", &notification_subject_json::Url, "latest_comment_url", &notification_subject_json::LatestCommentUrl, "type", &notification_subject_json::Type);
			};
		};

		struct notification_repository_json
		{
			std::string FullName;
			struct glaze
			{
				static constexpr auto value = glz::object("full_name", &notification_repository_json::FullName);
			};
		};

		struct notification_json
		{
			std::string                  Id;
			bool                         Unread = true;
			std::string                  Reason;
			std::string                  UpdatedAt;
			std::optional<std::string>   LastReadAt;
			notification_subject_json    Subject;
			notification_repository_json Repository;
			struct glaze
			{
				static constexpr auto value = glz::object("id", &notification_json::Id, "unread", &notification_json::Unread, "reason", &notification_json::Reason,
														  "updated_at", &notification_json::UpdatedAt, "last_read_at", &notification_json::LastReadAt,
														  "subject", &notification_json::Subject, "repository", &notification_json::Repository);
			};
		};

		struct notification_json_options : glz::opts
		{
			// Glaze discovers these overrides by their exact library-defined names.
			// NOLINTNEXTLINE(readability-identifier-naming)
			bool validate_skipped = true;
			// NOLINTNEXTLINE(readability-identifier-naming)
			bool validate_trailing_whitespace = true;

			constexpr notification_json_options()
			{
				error_on_unknown_keys = false;
				error_on_missing_keys = false;
				partial_read          = false;
			}
		};

		[[nodiscard]] auto lower(std::string aText) -> std::string
		{
			std::ranges::transform(aText, aText.begin(), [](const unsigned char aByte) {
				return static_cast<char>(aByte >= 'A' && aByte <= 'Z' ? aByte - 'A' + 'a' : aByte);
			});
			return aText;
		}

		[[nodiscard]] auto header(const wire_response& aResponse, const std::string_view aName) -> std::optional<std::string>
		{
			auto found = std::optional<std::string>{ };
			for (const auto& entry : aResponse.Headers) {
				if (lower(entry.Name) == aName) {
					if (found) {
						return std::nullopt;
					}
					found = entry.Value;
				}
			}
			return found;
		}

		template <class Number>
		[[nodiscard]] auto decimal(const std::optional<std::string>& aValue) -> std::optional<Number>
		{
			if (!aValue || aValue->empty() || aValue->size() > 12U) {
				return std::nullopt;
			}
			auto       value  = Number{ };
			const auto parsed = std::from_chars(aValue->data(), aValue->data() + aValue->size(), value);
			if (parsed.ec != std::errc{ } || parsed.ptr != aValue->data() + aValue->size()) {
				return std::nullopt;
			}
			return value;
		}

		/// The decimal number ending \p aUrl after \p aMarker, when the marker is there.
		[[nodiscard]] auto trailing_number(const std::string_view aUrl, const std::string_view aMarker) -> std::optional<std::uint64_t>
		{
			const auto at = aUrl.rfind(aMarker);
			if (at == std::string_view::npos) {
				return std::nullopt;
			}
			const auto digits = aUrl.substr(at + aMarker.size());
			auto       value  = std::uint64_t{ 0U };
			const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), value);
			if (digits.empty() || parsed.ec != std::errc{ } || parsed.ptr != digits.data() + digits.size() || value == 0U) {
				return std::nullopt;
			}
			return value;
		}

		[[nodiscard]] auto reason_of(const std::string_view aReason) noexcept -> core::activity_reason
		{
			if (aReason == "author") {
				return core::activity_reason::author;
			}
			if (aReason == "comment") {
				return core::activity_reason::comment;
			}
			if (aReason == "mention") {
				return core::activity_reason::mention;
			}
			if (aReason == "team_mention") {
				return core::activity_reason::team_mention;
			}
			if (aReason == "review_requested") {
				return core::activity_reason::review_requested;
			}
			return core::activity_reason::other;
		}

		[[nodiscard]] auto anchor_of(const std::optional<std::string>& aUrl, const std::string& aRepository) -> core::activity_anchor
		{
			if (!aUrl) {
				return { };
			}
			const auto scope = "/repos/" + aRepository + "/";
			if (aUrl->find(scope) == std::string::npos) {
				return { };
			}
			if (const auto id = trailing_number(*aUrl, scope + "issues/comments/")) {
				return { .Kind = core::activity_anchor_kind::issue_comment, .Id = *id };
			}
			if (const auto id = trailing_number(*aUrl, scope + "pulls/comments/")) {
				return { .Kind = core::activity_anchor_kind::review_comment, .Id = *id };
			}
			if (const auto id = trailing_number(*aUrl, "/reviews/")) {
				return { .Kind = core::activity_anchor_kind::review, .Id = *id };
			}
			return { };
		}

		[[nodiscard]] auto repository_name(const std::string_view aName) noexcept -> bool
		{
			const auto slash = aName.find('/');
			return slash != std::string_view::npos && slash != 0U && slash + 1U < aName.size() && aName.find('/', slash + 1U) == std::string_view::npos
				&& aName.size() <= 140U && std::ranges::all_of(aName, [](const unsigned char aByte) {
					   return (aByte >= 'a' && aByte <= 'z') || (aByte >= 'A' && aByte <= 'Z') || (aByte >= '0' && aByte <= '9') || aByte == '-' || aByte == '_' || aByte == '.' || aByte == '/';
				   });
		}

		[[nodiscard]] auto http_date(const std::string_view aValue) noexcept -> bool
		{
			constexpr auto shape = std::string_view{ "AAA, 00 AAA 0000 00:00:00 GMT" };
			return aValue.size() == shape.size() && std::ranges::equal(aValue, shape, [](const char aByte, const char aShape) {
					   switch (aShape) {
					   case 'A': return (aByte >= 'A' && aByte <= 'Z') || (aByte >= 'a' && aByte <= 'z');
					   case '0': return aByte >= '0' && aByte <= '9';
					   default:  return aByte == aShape;
					   }
				   });
		}
	} // namespace

	auto notification_timestamp(const std::string_view aValue) -> std::optional<std::int64_t>
	{
		// Exactly `YYYY-MM-DDTHH:MM:SSZ`, as GitHub writes it.
		if (aValue.size() != 20U || aValue[4] != '-' || aValue[7] != '-' || aValue[10] != 'T' || aValue[13] != ':' || aValue[16] != ':' || aValue[19] != 'Z') {
			return std::nullopt;
		}
		const auto field = [aValue](const std::size_t aAt, const std::size_t aLength) -> std::optional<int> {
			auto       value  = 0;
			const auto parsed = std::from_chars(aValue.data() + aAt, aValue.data() + aAt + aLength, value);
			if (parsed.ec != std::errc{ } || parsed.ptr != aValue.data() + aAt + aLength) {
				return std::nullopt;
			}
			return value;
		};
		const auto year = field(0U, 4U), month = field(5U, 2U), day = field(8U, 2U), hour = field(11U, 2U), minute = field(14U, 2U), second = field(17U, 2U);
		if (!year || !month || !day || !hour || !minute || !second || *hour > 23 || *minute > 59 || *second > 60) {
			return std::nullopt;
		}
		const auto date = std::chrono::year_month_day{ std::chrono::year{ *year }, std::chrono::month{ static_cast<unsigned>(*month) }, std::chrono::day{ static_cast<unsigned>(*day) } };
		if (!date.ok()) {
			return std::nullopt;
		}
		const auto days = std::chrono::sys_days{ date }.time_since_epoch().count();
		return static_cast<std::int64_t>(days) * 86400 + *hour * 3600 + *minute * 60 + *second;
	}

	auto read_notifications_response(const wire_response& aResponse) -> notifications_read
	{
		auto result               = notifications_read{ };
		result.Poll.PollInterval  = decimal<std::uint32_t>(header(aResponse, "x-poll-interval"));
		result.Poll.RetryAfter    = decimal<std::uint32_t>(header(aResponse, "retry-after"));
		result.Poll.RateRemaining = decimal<std::uint32_t>(header(aResponse, "x-ratelimit-remaining"));
		result.Poll.RateReset     = decimal<std::int64_t>(header(aResponse, "x-ratelimit-reset"));
		if (const auto modified = header(aResponse, "last-modified"); modified && http_date(*modified)) {
			result.LastModified = *modified;
		}
		const auto limited = (aResponse.Status == 403 || aResponse.Status == 429)
			&& (aResponse.Status == 429 || result.Poll.RetryAfter || result.Poll.RateRemaining == 0U
				|| github_error_body_is_secondary_rate_limit(aResponse.Body, 65536U));
		if (limited) {
			result.Poll.Outcome = core::activity_poll_outcome::rate_limited;
			return result;
		}
		switch (aResponse.Status) {
		case 304:
			result.Poll.Outcome = core::activity_poll_outcome::unchanged;
			return result;
		case 403:
		case 404:
			// The sign-in cannot read notifications: a token without the scope,
			// or a GitHub App's, which GitHub refuses this list.
			result.Poll.Outcome = core::activity_poll_outcome::unsupported;
			return result;
		case 200:
			break;
		default:
			result.Poll.Outcome = core::activity_poll_outcome::failed;
			return result;
		}

		auto entries = std::vector<notification_json>{ };
		if (aResponse.Body.size() > maximum_notifications_body_bytes
			|| !json_preflight{ aResponse.Body, { .Depth = 12U, .Elements = notifications_page_size + 1U, .Values = 64U * 1024U } }.run()
			|| glz::read<notification_json_options{ }>(entries, aResponse.Body) || entries.size() > notifications_page_size) {
			result.Poll.Outcome = core::activity_poll_outcome::failed;
			result.LastModified.clear();
			return result;
		}
		for (const auto& entry : entries) {
			if (entry.Subject.Type != "PullRequest" || !entry.Subject.Url || !repository_name(entry.Repository.FullName)) {
				continue;
			}
			const auto number  = trailing_number(*entry.Subject.Url, "/repos/" + entry.Repository.FullName + "/pulls/");
			const auto updated = notification_timestamp(entry.UpdatedAt);
			if (!number || *number > static_cast<std::uint64_t>(core::maximum_pull_request_number) || !updated || entry.Id.empty() || entry.Id.size() > 32U
				|| !std::ranges::all_of(entry.Id, [](const char aByte) { return aByte >= '0' && aByte <= '9'; })) {
				continue;
			}
			result.Threads.push_back(core::activity_thread{
				.Id         = entry.Id,
				.UpdatedAt  = *updated,
				.Unread     = entry.Unread,
				.LastReadAt = entry.LastReadAt ? notification_timestamp(*entry.LastReadAt) : std::nullopt,
				.Reason     = reason_of(entry.Reason),
				.Repository = entry.Repository.FullName,
				.Number     = static_cast<std::int64_t>(*number),
				.Anchor     = anchor_of(entry.Subject.LatestCommentUrl, entry.Repository.FullName),
			});
		}
		result.Poll.Outcome = core::activity_poll_outcome::changed;
		result.Complete     = entries.size() < notifications_page_size && aResponse.Completeness != page_completeness::has_next_page;
		return result;
	}
} // namespace bpr::adapters::github::detail
