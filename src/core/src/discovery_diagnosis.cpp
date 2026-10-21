#include "bpr/core/discovery_diagnosis.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <utility>

namespace bpr::core {
	namespace {
		constexpr std::array<std::string_view, 2> scope_names{ "refresh", "read" };
		constexpr std::array<std::string_view, 4> reason_names{ "none", "retained", "authored", "requested" };
		constexpr std::array<std::string_view, 6> state_names{ "none", "current", "partial", "stale", "stopped", "cancelled" };

		template <typename TEnum, std::size_t N>
		[[nodiscard]] auto named(const std::array<std::string_view, N>& aNames, const TEnum aValue) noexcept -> std::string_view
		{
			const auto index = static_cast<std::size_t>(aValue);
			return index < aNames.size() ? aNames[index] : std::string_view{ };
		}

		/// One `key=value` word, ending the text or followed by one space and more.
		[[nodiscard]] auto word(std::string_view& aText, const std::string_view aKey) -> std::optional<std::string_view>
		{
			if (!aText.starts_with(aKey) || aText.size() == aKey.size() || aText[aKey.size()] != '=') {
				return std::nullopt;
			}
			aText.remove_prefix(aKey.size() + 1U);
			const auto end   = aText.find(' ');
			const auto value = aText.substr(0U, end);
			aText.remove_prefix(end == std::string_view::npos ? aText.size() : end + 1U);
			if (value.empty() || (end != std::string_view::npos && aText.empty())) {
				return std::nullopt;
			}
			return value;
		}

		/// A decimal with no leading zero that fits 32 bits.
		[[nodiscard]] auto number(std::string_view& aText, const std::string_view aKey) -> std::optional<std::uint32_t>
		{
			const auto digits = word(aText, aKey);
			auto       value  = std::uint32_t{ 0U };
			if (!digits || (digits->size() > 1U && digits->front() == '0')) {
				return std::nullopt;
			}
			const auto parsed = std::from_chars(digits->data(), digits->data() + digits->size(), value);
			if (parsed.ec != std::errc{ } || parsed.ptr != digits->data() + digits->size()) {
				return std::nullopt;
			}
			return value;
		}

		template <typename TEnum, std::size_t N>
		[[nodiscard]] auto enumerated(std::string_view& aText, const std::string_view aKey, const std::array<std::string_view, N>& aNames) -> std::optional<TEnum>
		{
			const auto value = word(aText, aKey);
			if (!value) {
				return std::nullopt;
			}
			const auto found = std::ranges::find(aNames, *value);
			return found == aNames.end() ? std::nullopt : std::optional{ static_cast<TEnum>(found - aNames.begin()) };
		}
	} // namespace

	auto name(const discovery_diagnosis_scope aScope) noexcept -> std::string_view
	{
		return named(scope_names, aScope);
	}

	auto name(const discovery_diagnosis_reason aReason) noexcept -> std::string_view
	{
		return named(reason_names, aReason);
	}

	auto name(const discovery_diagnosis_state aState) noexcept -> std::string_view
	{
		return named(state_names, aState);
	}

	auto valid(const discovery_diagnosis& aDiagnosis) noexcept -> bool
	{
		if (name(aDiagnosis.Scope).empty() || name(aDiagnosis.Reason).empty() || name(aDiagnosis.State).empty() || aDiagnosis.Refresh == 0U
			|| aDiagnosis.Failed > aDiagnosis.Reads) {
			return false;
		}
		if (aDiagnosis.Scope == discovery_diagnosis_scope::refresh) {
			return aDiagnosis.Read == 0U && aDiagnosis.Reason == discovery_diagnosis_reason::none;
		}
		return aDiagnosis.Read != 0U && aDiagnosis.Reason != discovery_diagnosis_reason::none && aDiagnosis.State == discovery_diagnosis_state::none
			&& aDiagnosis.Reads == 0U && aDiagnosis.Failed == 0U;
	}

	auto describe(const discovery_diagnosis& aDiagnosis) -> std::string
	{
		if (!valid(aDiagnosis)) {
			return { };
		}
		auto text = std::string{ "discovery=" };
		text += name(aDiagnosis.Scope);
		text += " refresh=" + std::to_string(aDiagnosis.Refresh);
		text += " read=" + std::to_string(aDiagnosis.Read);
		text += " reason=";
		text += name(aDiagnosis.Reason);
		text += " state=";
		text += name(aDiagnosis.State);
		for (const auto& [key, value] : std::array<std::pair<std::string_view, std::uint32_t>, 7>{ { { " repositories=", aDiagnosis.Repositories },
																									 { " pull_requests=", aDiagnosis.PullRequests },
																									 { " reads=", aDiagnosis.Reads },
																									 { " failed=", aDiagnosis.Failed },
																									 { " requests=", aDiagnosis.Requests },
																									 { " wait_ms=", aDiagnosis.WaitMilliseconds },
																									 { " run_ms=", aDiagnosis.RunMilliseconds } } }) {
			text += key;
			text += std::to_string(value);
		}
		return text;
	}

	auto parse_discovery_diagnosis(std::string_view aText) -> std::optional<discovery_diagnosis>
	{
		const auto scope   = enumerated<discovery_diagnosis_scope>(aText, "discovery", scope_names);
		const auto refresh = scope ? number(aText, "refresh") : std::nullopt;
		const auto read    = refresh ? number(aText, "read") : std::nullopt;
		const auto reason  = read ? enumerated<discovery_diagnosis_reason>(aText, "reason", reason_names) : std::nullopt;
		const auto state   = reason ? enumerated<discovery_diagnosis_state>(aText, "state", state_names) : std::nullopt;
		if (!state) {
			return std::nullopt;
		}
		auto result = discovery_diagnosis{ .Scope = *scope, .Refresh = *refresh, .Read = *read, .Reason = *reason, .State = *state };
		for (const auto& [key, field] : std::array<std::pair<std::string_view, std::uint32_t*>, 7>{ { { "repositories", &result.Repositories },
																									  { "pull_requests", &result.PullRequests },
																									  { "reads", &result.Reads },
																									  { "failed", &result.Failed },
																									  { "requests", &result.Requests },
																									  { "wait_ms", &result.WaitMilliseconds },
																									  { "run_ms", &result.RunMilliseconds } } }) {
			const auto value = number(aText, key);
			if (!value) {
				return std::nullopt;
			}
			*field = *value;
		}
		if (!aText.empty() || !valid(result)) {
			return std::nullopt;
		}
		return result;
	}
} // namespace bpr::core
