#include "bpr/core/review_cycle_send.hpp"

#include <algorithm>
#include <array>
#include <ranges>
#include <utility>

namespace bpr::core {
	namespace {
		constexpr auto protocols = std::array{ agent_protocol::codex_app_server, agent_protocol::codex_jsonl_exec, agent_protocol::claude_stream_json, agent_protocol::external_terminal };

		auto ascii_lower(const std::string_view aText) -> std::string
		{
			auto lower = std::string{ aText };
			std::ranges::transform(lower, lower.begin(), [](const char aSymbol) { return aSymbol >= 'A' && aSymbol <= 'Z' ? static_cast<char>(aSymbol - 'A' + 'a') : aSymbol; });
			return lower;
		}

		auto find_profile(const std::span<const agent_profile> aProfiles, const agent_profile_id aProfile) -> const agent_profile*
		{
			const auto found = std::ranges::find(aProfiles, aProfile, &agent_profile::Id);
			return found == aProfiles.end() ? nullptr : &*found;
		}
	}

	auto send_session_name(const std::string_view aName) noexcept -> bool
	{
		const auto alphanumeric = [](const char aSymbol) { return (aSymbol >= 'a' && aSymbol <= 'z') || (aSymbol >= 'A' && aSymbol <= 'Z') || (aSymbol >= '0' && aSymbol <= '9'); };
		// It starts with a letter or digit, so a harness never reads it as an option.
		return !aName.empty() && aName.size() <= maximum_send_session_bytes && alphanumeric(aName.front())
			&& std::ranges::all_of(aName, [&](const char aSymbol) { return alphanumeric(aSymbol) || aSymbol == '.' || aSymbol == '_' || aSymbol == '-'; });
	}

	auto continues_sessions(const agent_protocol aProtocol) noexcept -> bool
	{
		return aProtocol == agent_protocol::claude_stream_json;
	}

	auto send_session_record(const agent_protocol aProtocol, const std::string_view aSession) -> std::string
	{
		if (!send_session_name(aSession) || stable_name(aProtocol).empty()) {
			return { };
		}
		return std::string{ stable_name(aProtocol) } + "/" + std::string{ aSession };
	}

	auto parse_send_session_record(const std::string_view aRecord) -> std::optional<std::pair<agent_protocol, std::string>>
	{
		const auto slash = aRecord.find('/');
		if (slash == std::string_view::npos) {
			return std::nullopt;
		}
		const auto name    = aRecord.substr(0U, slash);
		const auto session = aRecord.substr(slash + 1U);
		for (const auto protocol : protocols) {
			if (stable_name(protocol) == name && send_session_name(session)) {
				return std::pair{ protocol, std::string{ session } };
			}
		}
		return std::nullopt;
	}

	auto cycle_send_policy(const cycle_workspace_origin& aOrigin, const std::optional<std::string_view> aAccountHost) -> agent_host_policy
	{
		const auto enterprise = aOrigin.Classification == cycle_data_classification::enterprise_restricted || (aAccountHost && ascii_lower(*aAccountHost) != "github.com");
		return enterprise ? built_in_agent_policy(send_enterprise_host, built_in_agent_host_kind::explicit_ghes) : built_in_agent_policy(send_github_host, built_in_agent_host_kind::github_com);
	}

	auto send_sessions(const std::span<const cycle_delivery_record> aDeliveries, const agent_protocol aProtocol) -> std::vector<std::string>
	{
		auto sessions = std::vector<std::string>{ };
		for (const auto& record : aDeliveries | std::views::reverse) {
			const auto& seen = record.Observation;
			if (seen.State != cycle_delivery_state::recipient_acknowledged || seen.Channel != cycle_disclosure_channel::agent_adapter) {
				continue;
			}
			auto parsed = parse_send_session_record(seen.Destination);
			if (parsed && parsed->first == aProtocol && std::ranges::find(sessions, parsed->second) == sessions.end()) {
				sessions.push_back(std::move(parsed->second));
			}
		}
		return sessions;
	}

	auto send_offers(const std::span<const agent_profile> aProfiles, const agent_host_policy& aPolicy, const std::span<const cycle_delivery_record> aDeliveries)
		-> std::vector<cycle_send_offer>
	{
		auto offers = std::vector<cycle_send_offer>{ };
		offers.reserve(aProfiles.size());
		for (const auto& profile : aProfiles) {
			auto offer = cycle_send_offer{
				.Profile     = profile.Id,
				.DisplayName = profile.DisplayName,
				.Verdict     = decide_agent_invocation(profile, aPolicy),
				.Continues   = continues_sessions(profile.Protocol),
			};
			if (offer.Continues) {
				offer.Sessions = send_sessions(aDeliveries, profile.Protocol);
			}
			offers.push_back(std::move(offer));
		}
		return offers;
	}

	auto stable_name(const cycle_send_problem aValue) noexcept -> std::string_view
	{
		constexpr auto names = std::array<std::string_view, 5>{ "channel_refused", "profile_unknown", "profile_refused", "session_not_continuable", "session_unknown" };
		const auto     index = static_cast<std::size_t>(aValue);
		return index < names.size() ? names.at(index) : std::string_view{ };
	}

	auto decide_send(const cycle_authorization aChannel, const std::span<const agent_profile> aProfiles, const agent_host_policy& aPolicy,
					 const std::span<const cycle_delivery_record> aDeliveries, const cycle_send_request& aRequest) -> std::optional<cycle_send_refusal>
	{
		if (aChannel != cycle_authorization::allowed) {
			return cycle_send_refusal{ .Problem = cycle_send_problem::channel_refused, .Channel = aChannel };
		}
		const auto* profile = find_profile(aProfiles, aRequest.Profile);
		if (profile == nullptr) {
			return cycle_send_refusal{ .Problem = cycle_send_problem::profile_unknown };
		}
		const auto verdict = decide_agent_invocation(*profile, aPolicy);
		if (!verdict.Permitted) {
			return cycle_send_refusal{ .Problem = cycle_send_problem::profile_refused, .Verdict = verdict };
		}
		if (!aRequest.Session.Continue) {
			return std::nullopt;
		}
		if (!continues_sessions(profile->Protocol)) {
			return cycle_send_refusal{ .Problem = cycle_send_problem::session_not_continuable, .Verdict = verdict };
		}
		const auto known = send_sessions(aDeliveries, profile->Protocol);
		if (std::ranges::find(known, *aRequest.Session.Continue) == known.end()) {
			return cycle_send_refusal{ .Problem = cycle_send_problem::session_unknown, .Verdict = verdict };
		}
		return std::nullopt;
	}

	auto stable_name(const cycle_send_end aValue) noexcept -> std::string_view
	{
		constexpr auto names = std::array<std::string_view, 5>{ "completed", "not_started", "stopped", "timed_out", "cancelled" };
		const auto     index = static_cast<std::size_t>(aValue);
		return index < names.size() ? names.at(index) : std::string_view{ };
	}

	auto send_observations(const cycle_round& aRound, const cycle_delivery_output& aOutput, const agent_protocol aProtocol, const cycle_send_result& aResult)
		-> std::vector<cycle_delivery_observation>
	{
		const auto seen = [&](const cycle_delivery_state aState) {
			return cycle_delivery_observation{ .Round = aRound.Id, .Digest = aRound.Digest, .State = aState, .Channel = cycle_disclosure_channel::agent_adapter };
		};
		if (aResult.End == cycle_send_end::not_started) {
			return { seen(cycle_delivery_state::failed) };
		}
		auto observations          = std::vector<cycle_delivery_observation>{ seen(cycle_delivery_state::accepted_by_adapter) };
		observations.back().Output = aOutput;
		if (aResult.Session && send_session_name(*aResult.Session)) {
			observations.push_back(seen(cycle_delivery_state::recipient_acknowledged));
			observations.back().Destination = send_session_record(aProtocol, *aResult.Session);
		}
		if (aResult.End != cycle_send_end::completed) {
			observations.push_back(seen(cycle_delivery_state::uncertain));
		}
		return observations;
	}

	auto reply_response(const std::string_view aReply) -> std::optional<std::string_view>
	{
		constexpr auto blank = std::string_view{ " \t\r\n" };
		const auto     first = aReply.find_first_not_of(blank);
		if (first == std::string_view::npos) {
			return std::nullopt;
		}
		const auto whole = aReply.substr(first, aReply.find_last_not_of(blank) - first + 1U);
		if (whole.front() == '{' && whole.back() == '}') {
			return whole;
		}
		constexpr auto opening = std::string_view{ "```json\n" };
		constexpr auto closing = std::string_view{ "\n```" };
		const auto     start   = aReply.find(opening);
		if (start == std::string_view::npos || aReply.find(opening, start + opening.size()) != std::string_view::npos) {
			return std::nullopt;
		}
		const auto body = start + opening.size();
		const auto end  = aReply.find(closing, body);
		if (end == std::string_view::npos) {
			return std::nullopt;
		}
		return aReply.substr(body, end - body);
	}
}
