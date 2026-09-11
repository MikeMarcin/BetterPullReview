#include "bpr/application/setup_destination.hpp"

#include <algorithm>
#include <charconv>
#include <limits>

namespace bpr::application {
	namespace {
		auto ascii_letter(const char aValue) -> bool
		{
			return (aValue >= 'a' && aValue <= 'z') || (aValue >= 'A' && aValue <= 'Z');
		}
		auto digit(const char aValue) -> bool { return aValue >= '0' && aValue <= '9'; }
		auto valid_host(const std::string_view aHost) -> bool
		{
			if (aHost.empty() || aHost.size() > 253U) {
				return false;
			}
			auto label = std::size_t{ 0 };
			for (const char value : aHost) {
				if (value == '.') {
					if (label == 0U || label > 63U) {
						return false;
					}
					label = 0U;
				} else {
					if (!ascii_letter(value) && !digit(value) && value != '-') {
						return false;
					}
					if (label == 0U && value == '-') {
						return false;
					}
					++label;
				}
			}
			return label > 0U && label <= 63U && aHost.back() != '-'
				&& aHost.find("-.") == std::string_view::npos;
		}
	}

	auto parse_setup_destination(std::string_view aText) -> std::optional<core::setup_destination>
	{
		if (aText.size() > 4096U) {
			return std::nullopt;
		}
		while (!aText.empty() && (aText.front() == ' ' || aText.front() == '\t' || aText.front() == '\n' || aText.front() == '\r')) {
			aText.remove_prefix(1U);
		}
		while (!aText.empty() && (aText.back() == ' ' || aText.back() == '\t' || aText.back() == '\n' || aText.back() == '\r')) {
			aText.remove_suffix(1U);
		}
		if (aText.empty() || std::ranges::any_of(aText, [](const unsigned char aValue) { return aValue <= 32U || aValue >= 127U || aValue == '\\'; })) {
			return std::nullopt;
		}
		auto       result = core::setup_destination{ };
		const auto url    = aText.starts_with("https://");
		if (url) {
			aText.remove_prefix(8U);
			const auto slash = aText.find('/');
			if (slash == std::string_view::npos || !valid_host(aText.substr(0U, slash))) {
				return std::nullopt;
			}
			result.Hostname = aText.substr(0U, slash);
			for (auto& value : result.Hostname) {
				if (value >= 'A' && value <= 'Z') {
					value = static_cast<char>(value + ('a' - 'A'));
				}
			}
			aText.remove_prefix(slash + 1U);
		} else if (aText.find(':') != std::string_view::npos) {
			return std::nullopt;
		}
		const auto slash = aText.find('/');
		if (slash == std::string_view::npos) {
			return std::nullopt;
		}
		result.Repository.Owner = aText.substr(0U, slash);
		aText.remove_prefix(slash + 1U);
		const auto separator = aText.find_first_of("/#");
		if (separator == std::string_view::npos) {
			return std::nullopt;
		}
		result.Repository.Name = aText.substr(0U, separator);
		const auto shorthand   = aText[separator] == '#';
		aText.remove_prefix(separator);
		if (shorthand) {
			if (url) {
				return std::nullopt;
			}
			aText.remove_prefix(1U);
		} else {
			if (!aText.starts_with("/pull/")) {
				return std::nullopt;
			}
			aText.remove_prefix(6U);
		}
		const auto endNumber = aText.find_first_of("/?#");
		const auto number    = aText.substr(0U, endNumber);
		auto       value     = std::uint32_t{ };
		const auto parsed    = std::from_chars(number.data(), number.data() + number.size(), value);
		if (number.empty() || parsed.ec != std::errc{ } || parsed.ptr != number.data() + number.size()) {
			return std::nullopt;
		}
		result.Number = core::pull_request_number{ value };
		if (!result.Number.valid() || !valid(result.Repository)) {
			return std::nullopt;
		}
		aText.remove_prefix(number.size());
		if (shorthand && !aText.empty()) {
			return std::nullopt;
		}
		const auto fragmentAt = aText.find('#');
		const auto fragment   = fragmentAt == std::string_view::npos ? std::string_view{ } : aText.substr(fragmentAt);
		const auto path       = aText.substr(0U, aText.find_first_of("?#"));
		if (!path.empty() && path != "/" && path != "/files" && path != "/commits") {
			if (!path.starts_with("/commits/") || path.size() <= 9U || path.size() > 73U
				|| !std::ranges::all_of(path.substr(9U), [](const char aValue) { return digit(aValue) || (aValue >= 'a' && aValue <= 'f') || (aValue >= 'A' && aValue <= 'F'); })) {
				return std::nullopt;
			}
		}
		if (path != "/") {
			result.Location = path;
		}
		result.Location += fragment;
		return result;
	}

	setup_destination_host::setup_destination_host(setup_destination_port& aPort)
		: Port{ aPort }
	{ }
	auto setup_destination_host::pending() const -> const std::optional<core::pending_setup_destination>& { return Pending; }
	void setup_destination_host::present(std::string aNotice, const bool aOpened)
	{
		Port.present({ .Pending = Pending, .Notice = std::move(aNotice), .Opened = aOpened });
	}
	void setup_destination_host::submit(const std::string_view aText)
	{
		Pending.reset();
		auto destination = parse_setup_destination(aText);
		if (!destination || NextRequest == std::numeric_limits<std::uint64_t>::max()) {
			present("That is not a supported pull request link or owner/name#number.");
			return;
		}
		if (destination->Hostname.empty()) {
			destination->Hostname = Port.shorthand_hostname();
		}
		if (destination->Hostname.empty()) {
			present("Choose a server or paste the complete pull request link.");
			return;
		}
		Pending = core::pending_setup_destination{
			.Request     = core::setup_request_id{ NextRequest++ },
			.Owner       = Port.owner(),
			.Destination = std::move(*destination),
			.Profile     = std::nullopt,
		};
		resolve();
	}
	void setup_destination_host::resolve()
	{
		if (!Pending) {
			return;
		}
		if (Pending->Owner != Port.owner()) {
			cancel();
			return;
		}
		Pending = core::resolve_setup_destination(*Pending, Port.owner(), Port.profiles());
		if (!Pending) {
			present();
			return;
		}
		if (Pending->Stage == core::setup_destination_stage::add_server) {
			present("Add server and continue: " + Pending->Destination.Hostname);
			return;
		}
		if (Pending->Stage == core::setup_destination_stage::ready) {
			authenticated(Pending->Request, *Pending->Profile, Pending->Destination.Hostname);
			return;
		}
		if (Pending->Stage == core::setup_destination_stage::public_preview) {
			// [[BPR-0732]]. One gesture: the host activates the signed-out
			// session and opens the destination when it exists, with no
			// continuation press between the paste and the pull request.
			const auto previewing = *Pending;
			present("Opening " + previewing.Destination.Hostname + " without signing in…");
			if (Pending == previewing && Port.owner() == previewing.Owner) {
				Port.preview(previewing);
			}
			return;
		}
		present("Sign in to continue to " + Pending->Destination.Hostname);
	}
	void setup_destination_host::continue_setup(const core::setup_request_id aRequest)
	{
		if (!Pending || Pending->Request != aRequest) {
			return;
		}
		if (Pending->Owner != Port.owner()) {
			cancel();
			return;
		}
		const auto pending = *Pending;
		if (pending.Stage == core::setup_destination_stage::add_server) {
			Port.configure(pending);
		} else if (pending.Stage == core::setup_destination_stage::sign_in) {
			Pending->Stage            = core::setup_destination_stage::authenticating;
			const auto authenticating = *Pending;
			present("Signing in to " + pending.Destination.Hostname);
			if (Pending == authenticating && Port.owner() == authenticating.Owner) {
				Port.authenticate(authenticating);
			}
		}
	}
	void setup_destination_host::committed(const core::setup_request_id aRequest)
	{
		if (Pending && Pending->Request == aRequest && Pending->Owner == Port.owner()) {
			Pending->Stage = core::setup_destination_stage::opening;
			present("Sign-in accepted. Opening the requested pull request.");
		}
	}

	void setup_destination_host::configured(const core::setup_request_id aRequest)
	{
		if (Pending && Pending->Request == aRequest && Pending->Stage == core::setup_destination_stage::add_server) {
			resolve();
		}
	}
	void setup_destination_host::authenticated(const core::setup_request_id aRequest,
											   const core::host_profile_id aProfile, const std::string_view aHostname)
	{
		const auto accepted = core::accept_setup_destination(Pending, aRequest, Port.owner(), aProfile, aHostname);
		if (!accepted) {
			return;
		}
		Pending.reset();
		Port.open(aProfile, *accepted);
		if (!Pending) {
			present(accepted->Location.empty() ? std::string{ } : "The requested location is unavailable; opened the pull request. Requested location: " + accepted->Location, true);
		}
	}
	void setup_destination_host::previewed(const core::setup_request_id aRequest,
										   const core::host_profile_id aProfile, const std::string_view aHostname)
	{
		if (Pending && Pending->Stage == core::setup_destination_stage::public_preview) {
			authenticated(aRequest, aProfile, aHostname);
		}
	}
	void setup_destination_host::cancel()
	{
		Pending.reset();
		present();
	}
}
