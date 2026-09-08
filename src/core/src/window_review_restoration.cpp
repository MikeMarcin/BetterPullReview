#include "bpr/core/window_review_restoration.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace bpr::core {
	auto valid(const window_review_authority& aAuthority) noexcept -> bool
	{
		if ((aAuthority.Mode != github_access_mode::authenticated && aAuthority.Mode != github_access_mode::public_preview)
			|| aAuthority.Hostname.empty() || aAuthority.Hostname.size() > 253U) {
			return false;
		}
		std::size_t label    = 0;
		char        previous = '.';
		for (const char value : aAuthority.Hostname) {
			if (value == '.') {
				if (label == 0 || previous == '-') {
					return false;
				}
				label = 0;
			} else {
				if (!((value >= 'a' && value <= 'z') || (value >= '0' && value <= '9') || (value == '-' && label != 0))) {
					return false;
				}
				++label;
				if (label > 63U) {
					return false;
				}
			}
			previous = value;
		}
		return label != 0 && previous != '-';
	}
	auto valid(const window_review_destination& aDestination) noexcept -> bool
	{
		return valid(window_review_authority{ aDestination.Mode, aDestination.Hostname }) && valid(aDestination.Repository) && aDestination.Number.value() != 0;
	}
	auto valid_window_review_records(const std::vector<window_review_record>& aRecords) noexcept -> bool
	{
		if (aRecords.size() > maximum_bpr_windows_per_profile) {
			return false;
		}
		std::uint64_t previous = 0;
		for (const auto& record : aRecords) {
			if (record.Window.value() <= previous || !valid(record.Destination)) {
				return false;
			}
			previous = record.Window.value();
		}
		return true;
	}
	window_review_restoration::window_review_restoration(bpr_profile_id aProfile, std::vector<bpr_window_context> aContexts, std::vector<std::string> aConfiguredHosts)
		: Profile{ aProfile }
		, ConfiguredHosts{ std::move(aConfiguredHosts) }
	{
		if (Profile.value() == 0 || aContexts.size() > maximum_bpr_windows_per_profile || ConfiguredHosts.size() > maximum_bpr_profiles) {
			throw std::invalid_argument("invalid window restoration scope");
		}
		for (const auto& host : ConfiguredHosts) {
			if (!valid(window_review_authority{ github_access_mode::authenticated, host })) {
				throw std::invalid_argument("invalid configured hostname");
			}
		}
		for (const auto context : aContexts) {
			if (!attach(context)) {
				throw std::invalid_argument("invalid window restoration context");
			}
		}
		Initializing = false;
	}
	auto window_review_restoration::attach(bpr_window_context aContext) -> bool
	{
		if ((!Initializing && (aContext.Window.value() <= LastWindowId || aContext.Generation.value() != 1))
			|| aContext.Profile != Profile || aContext.Window.value() == 0 || aContext.Generation.value() == 0 || Slots.size() >= maximum_bpr_windows_per_profile
			|| std::ranges::any_of(Slots, [aContext](const auto& aSlot) { return aSlot.Context.Window == aContext.Window; })) {
			return false;
		}
		Slots.push_back({ aContext, false, false, { }, { } });
		LastWindowId = std::max(LastWindowId, aContext.Window.value());
		return true;
	}
	auto window_review_restoration::current(bpr_window_context aContext) const noexcept -> bool
	{
		return std::ranges::any_of(Slots, [aContext](const auto& aSlot) { return aSlot.Context == aContext; });
	}
	auto window_review_restoration::load(const std::vector<window_review_record>& aRecords) -> window_review_actions
	{
		window_review_actions actions;
		if (!valid_window_review_records(aRecords)) {
			actions.Failures.push_back({ { }, window_review_failure::corrupt });
			return actions;
		}
		auto merged = aRecords;
		std::erase_if(merged, [this](const auto& aRecord) { return std::ranges::none_of(Slots, [&aRecord](const auto& aSlot) { return aSlot.Context.Window == aRecord.Window; }); });
		for (const auto& record : Records) {
			const auto found = std::ranges::find_if(merged, [&record](const auto& aRecord) { return aRecord.Window == record.Window; });
			if (found == merged.end()) {
				merged.push_back(record);
			} else {
				*found = record;
			}
		}
		std::ranges::sort(merged, { }, &window_review_record::Window);
		Records = std::move(merged);
		if (Loaded || StartupSuppressed) {
			Loaded = true;
			return actions;
		}
		Loaded              = true;
		bool hadDestination = false;
		for (auto slot = Slots.rbegin(); slot != Slots.rend(); ++slot) {
			if (slot->Context.Generation.value() == 1) {
				continue;
			}
			const auto record = std::ranges::find_if(Records, [&slot](const auto& aRecord) { return aRecord.Window == slot->Context.Window; });
			if (record == Records.end()) {
				continue;
			}
			hadDestination = true;
			if (slot->Cancelled) {
				continue;
			}
			if (std::ranges::find(ConfiguredHosts, record->Destination.Hostname) == ConfiguredHosts.end()) {
				actions.Failures.push_back({ slot->Context, window_review_failure::missing_server });
				continue;
			}
			const auto authority = window_review_authority{ record->Destination.Mode, record->Destination.Hostname };
			if (!Chosen) {
				Chosen = authority;
			}
			if (*Chosen != authority) {
				actions.Failures.push_back({ slot->Context, window_review_failure::conflicting_authority });
				continue;
			}
			slot->Pending = record->Destination;
		}
		if (Chosen || !hadDestination) {
			Startup         = request_id{ NextRequest++ };
			actions.Startup = window_review_startup_request{ *Startup, Chosen };
		}
		return actions;
	}
	auto window_review_restoration::open_ready() -> window_review_actions
	{
		window_review_actions actions;
		for (auto& slot : Slots) {
			if (!slot.Cancelled && slot.Ready && slot.Pending && !Startup) {
				if (NextRequest == std::numeric_limits<std::uint64_t>::max()) {
					actions.Failures.push_back({ slot.Context, window_review_failure::identity_exhausted });
					slot.Pending.reset();
					continue;
				}
				slot.Request = request_id{ NextRequest++ };
				actions.Opens.push_back({ slot.Context, *slot.Request, *slot.Pending });
				slot.Pending.reset();
			}
		}
		return actions;
	}
	auto window_review_restoration::mark_window_ready(bpr_window_context aContext) -> window_review_actions
	{
		for (auto& slot : Slots) {
			if (slot.Context == aContext) {
				slot.Ready = true;
			}
		}
		return open_ready();
	}
	void window_review_restoration::cancel_restore(bpr_window_context aContext)
	{
		for (auto& slot : Slots) {
			if (slot.Context == aContext) {
				slot.Cancelled = true;
				slot.Pending.reset();
				slot.Request.reset();
			}
		}
		if (Loaded && Chosen && std::ranges::none_of(Slots, [](const auto& aSlot) { return !aSlot.Cancelled && aSlot.Pending.has_value(); })) {
			Startup.reset();
		}
	}
	void window_review_restoration::cancel_startup()
	{
		StartupSuppressed = true;
		Startup.reset();
		for (auto& slot : Slots) {
			slot.Pending.reset();
			slot.Request.reset();
			slot.Cancelled = true;
		}
	}
	auto window_review_restoration::current_startup(request_id aRequest) const noexcept -> bool { return Startup && *Startup == aRequest && !StartupSuppressed; }
	auto window_review_restoration::complete_startup(request_id aRequest, const std::expected<window_review_authority, window_review_failure>& aResult) -> window_review_actions
	{
		window_review_actions actions;
		if (!current_startup(aRequest)) {
			return actions;
		}
		Startup.reset();
		if (!aResult || !valid(*aResult) || (Chosen && *Chosen != *aResult)) {
			for (auto& slot : Slots) {
				if (slot.Pending) {
					actions.Failures.push_back({ slot.Context, !aResult ? aResult.error() : window_review_failure::conflicting_authority });
					slot.Pending.reset();
				}
			}
			return actions;
		}
		return open_ready();
	}
	auto window_review_restoration::current_restore(bpr_window_context aContext, request_id aRequest) const noexcept -> bool
	{
		return std::ranges::any_of(Slots, [aContext, aRequest](const auto& aSlot) { return aSlot.Context == aContext && aSlot.Request == aRequest && !aSlot.Cancelled; });
	}
	auto window_review_restoration::complete_restore(bpr_window_context aContext, request_id aRequest, const std::expected<void, window_review_failure>& aResult) -> window_review_actions
	{
		window_review_actions actions;
		if (!current_restore(aContext, aRequest)) {
			return actions;
		}
		for (auto& slot : Slots) {
			if (slot.Context == aContext) {
				slot.Request.reset();
			}
		}
		if (!aResult) {
			actions.Failures.push_back({ aContext, aResult.error() });
		}
		return actions;
	}
	auto window_review_restoration::accepted_read(bpr_window_context aContext, const window_review_destination& aDestination) -> bool
	{
		if (!current(aContext) || !valid(aDestination) || std::ranges::find(ConfiguredHosts, aDestination.Hostname) == ConfiguredHosts.end()) {
			return false;
		}
		const auto found = std::ranges::find_if(Records, [aContext](const auto& aRecord) { return aRecord.Window == aContext.Window; });
		if (found != Records.end()) {
			if (found->Destination == aDestination) {
				return false;
			}
			found->Destination = aDestination;
		} else {
			Records.push_back({ aContext.Window, aDestination });
			std::ranges::sort(Records, { }, &window_review_record::Window);
		}
		return true;
	}
	auto window_review_restoration::close(bpr_window_context aContext) -> bool
	{
		if (!current(aContext)) {
			return false;
		}
		cancel_restore(aContext);
		std::erase_if(Slots, [aContext](const auto& aSlot) { return aSlot.Context == aContext; });
		std::erase_if(Records, [aContext](const auto& aRecord) { return aRecord.Window == aContext.Window; });
		return true;
	}
	auto window_review_restoration::records() const noexcept -> const std::vector<window_review_record>& { return Records; }
}
