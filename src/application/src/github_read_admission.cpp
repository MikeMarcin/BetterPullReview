#include "bpr/application/github_read_admission.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

namespace bpr::application {
	namespace {
		constexpr std::size_t maximum_entries = 256U;
		auto                  system_time() -> core::github_read_time
		{
			return {
				static_cast<std::uint64_t>(std::max<std::int64_t>(0, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count())),
				static_cast<std::uint64_t>(std::max<std::int64_t>(0, std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()))
			};
		}
		auto used(github_rate_limit_resource aResource, bool aGraphql) -> bool
		{
			return aResource == github_rate_limit_resource::core || aResource == github_rate_limit_resource::search
				|| (aGraphql && aResource == github_rate_limit_resource::graphql);
		}
	}

	github_read_admission::github_read_admission(clock aClock)
		: Clock{ aClock ? std::move(aClock) : clock{ system_time } }
	{ }

	auto github_read_admission::refusal(const github_profile_authority& aProfile, const github_viewer_record& aViewer,
										bool aUsesGraphql) const -> std::optional<github_provider_error>
	{
		const auto now = Clock();
		if (Overflow.remaining_seconds(now) != 0U) {
			return github_provider_error{ .Failure = github_provider_failure::response_limit };
		}
		const entry* selected  = nullptr;
		auto         remaining = std::uint64_t{ 0 };
		for (const auto& item : Entries) {
			const auto seconds = item.Policy.remaining_seconds(now);
			if (item.Host == aProfile.Hostname && item.Account == aViewer.Node
				&& used(item.Error.RateLimitCause->Resource, aUsesGraphql) && seconds > remaining) {
				selected  = &item;
				remaining = seconds;
			}
		}
		if (selected) {
			auto error              = selected->Error;
			error.RetryAfterSeconds = static_cast<std::uint32_t>(std::min<std::uint64_t>(remaining, maximum_github_retry_after_seconds));
			return error;
		}
		// No eviction of a live deadline, even after repeated account switches.
		if (Entries.size() == maximum_entries && std::ranges::all_of(Entries, [&](const auto& aEntry) { return aEntry.Policy.remaining_seconds(now) != 0U; })) {
			return github_provider_error{ .Failure = github_provider_failure::response_limit };
		}
		return std::nullopt;
	}

	void github_read_admission::observe(const github_profile_authority& aProfile, const github_viewer_record& aViewer,
										const github_provider_error& aError)
	{
		if (!valid(aError) || aError.Failure != github_provider_failure::rate_limit) {
			return;
		}
		const auto now   = Clock();
		auto       found = std::ranges::find_if(Entries, [&](const auto& aEntry) {
			return aEntry.Host == aProfile.Hostname && aEntry.Account == aViewer.Node
				&& aEntry.Error.RateLimitCause->Resource == aError.RateLimitCause->Resource;
		});
		if (found == Entries.end()) {
			if (Entries.size() == maximum_entries) {
				found = std::ranges::find_if(Entries, [&](const auto& aEntry) { return aEntry.Policy.remaining_seconds(now) == 0U; });
				if (found == Entries.end()) {
					const auto& cause = *aError.RateLimitCause;
					Overflow.observe(now, aError.RetryAfterSeconds, cause.SecondaryLimited ? std::nullopt : std::optional{ cause.ResetUnixSeconds });
					return;
				}
				*found = entry{ aProfile.Hostname, aViewer.Node, aError, { } };
			} else {
				Entries.push_back({ aProfile.Hostname, aViewer.Node, aError, { } });
				found = std::prev(Entries.end());
			}
		}
		found->Error      = aError;
		const auto& cause = *aError.RateLimitCause;
		found->Policy.observe(now, aError.RetryAfterSeconds, cause.SecondaryLimited ? std::nullopt : std::optional{ cause.ResetUnixSeconds });
	}

	void github_read_admission::succeeded(const github_profile_authority& aProfile, const github_viewer_record& aViewer, bool aUsesGraphql)
	{
		const auto now = Clock();
		for (auto& item : Entries) {
			if (item.Host == aProfile.Hostname && item.Account == aViewer.Node && used(item.Error.RateLimitCause->Resource, aUsesGraphql)) {
				item.Policy.succeeded(now);
			}
		}
	}
}
