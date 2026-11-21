#include "bpr/core/auto_merge.hpp"

namespace bpr::core {
	auto name(const auto_merge_change aChange) noexcept -> std::string_view
	{
		switch (aChange) {
		case auto_merge_change::enable:  return "enable";
		case auto_merge_change::disable: return "disable";
		}
		return { };
	}

	auto name(const merge_method aMethod) noexcept -> std::string_view
	{
		switch (aMethod) {
		case merge_method::merge:  return "merge";
		case merge_method::squash: return "squash";
		case merge_method::rebase: return "rebase";
		}
		return { };
	}

	auto name(const auto_merge_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case auto_merge_refusal::none:               return "none";
		case auto_merge_refusal::not_allowed:        return "not_allowed";
		case auto_merge_refusal::not_open:           return "not_open";
		case auto_merge_refusal::already:            return "already";
		case auto_merge_refusal::not_permitted:      return "not_permitted";
		case auto_merge_refusal::method_not_allowed: return "method_not_allowed";
		}
		return { };
	}

	auto allowed_merge_methods::allows(const merge_method aMethod) const noexcept -> bool
	{
		switch (aMethod) {
		case merge_method::merge:  return Merge;
		case merge_method::squash: return Squash;
		case merge_method::rebase: return Rebase;
		}
		return false;
	}

	auto allowed_merge_methods::list() const -> std::vector<merge_method>
	{
		auto methods = std::vector<merge_method>{ };
		for (const auto method : { merge_method::merge, merge_method::squash, merge_method::rebase }) {
			if (allows(method)) {
				methods.push_back(method);
			}
		}
		return methods;
	}

	auto auto_merge_offered(const auto_merge_facts& aFacts) -> auto_merge_offer
	{
		if (!aFacts.Open || !aFacts.Allowed) {
			return { };
		}
		if (aFacts.Enabled) {
			return aFacts.ViewerCanDisable ? auto_merge_offer{ .Change = auto_merge_change::disable } : auto_merge_offer{ };
		}
		auto methods = aFacts.Methods.list();
		if (!aFacts.ViewerCanEnable || methods.empty()) {
			return { };
		}
		return { .Change = auto_merge_change::enable, .Methods = std::move(methods) };
	}

	auto decide_auto_merge_change(const auto_merge_facts& aFacts, const auto_merge_change aChange, const std::optional<merge_method> aMethod) noexcept -> auto_merge_refusal
	{
		const auto enable = aChange == auto_merge_change::enable;
		if (!aFacts.Open) {
			return auto_merge_refusal::not_open;
		}
		if (!aFacts.Allowed && enable) {
			return auto_merge_refusal::not_allowed;
		}
		if (aFacts.Enabled == enable) {
			return auto_merge_refusal::already;
		}
		if (!(enable ? aFacts.ViewerCanEnable : aFacts.ViewerCanDisable)) {
			return auto_merge_refusal::not_permitted;
		}
		if (enable && (!aMethod || !aFacts.Methods.allows(*aMethod))) {
			return auto_merge_refusal::method_not_allowed;
		}
		return auto_merge_refusal::none;
	}

	void begin_auto_merge_change(auto_merge_overlay& aOverlay, const std::uint64_t aRequest, const auto_merge_change aChange, const std::optional<merge_method> aMethod) noexcept
	{
		if (aRequest == 0U) {
			return;
		}
		aOverlay.Request        = aRequest;
		aOverlay.PendingEnabled = aChange == auto_merge_change::enable;
		aOverlay.Method         = aOverlay.PendingEnabled ? aMethod : std::nullopt;
	}

	void settle_auto_merge_change(auto_merge_overlay& aOverlay, const std::uint64_t aRequest, const bool aEnabled) noexcept
	{
		if (!aOverlay.pending() || aOverlay.Request != aRequest) {
			return;
		}
		aOverlay.Request       = 0U;
		aOverlay.Answered      = aEnabled;
		aOverlay.Disagreements = 0U;
		if (!aEnabled) {
			aOverlay.Method.reset();
		}
	}

	void abandon_auto_merge_change(auto_merge_overlay& aOverlay, const std::uint64_t aRequest) noexcept
	{
		if (aOverlay.pending() && aOverlay.Request == aRequest) {
			aOverlay.Request = 0U;
			if (!aOverlay.Answered.value_or(false)) {
				aOverlay.Method.reset();
			}
		}
	}

	void observe_auto_merge(auto_merge_overlay& aOverlay, const bool aEnabled) noexcept
	{
		if (!aOverlay.Answered.has_value()) {
			return;
		}
		if (aEnabled == *aOverlay.Answered || ++aOverlay.Disagreements >= 2U) {
			aOverlay.Answered.reset();
			aOverlay.Disagreements = 0U;
			if (!aOverlay.pending()) {
				aOverlay.Method.reset();
			}
		}
	}

	auto project_auto_merge(const bool aReadEnabled, const auto_merge_overlay& aOverlay) noexcept -> projected_auto_merge
	{
		if (aOverlay.pending()) {
			return { .Enabled = aOverlay.PendingEnabled, .Pending = true, .Method = aOverlay.Method };
		}
		const auto enabled = aOverlay.Answered.value_or(aReadEnabled);
		return { .Enabled = enabled, .Method = enabled && aOverlay.Answered ? aOverlay.Method : std::nullopt };
	}
} // namespace bpr::core
