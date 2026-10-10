#include "bpr/application/setup_destination.hpp"

#include "bpr/application/repository_addition_input.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

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
		auto hex_digit(const char aValue) -> bool
		{
			return digit(aValue) || (aValue >= 'a' && aValue <= 'f') || (aValue >= 'A' && aValue <= 'F');
		}
		auto lowercase(std::string_view aText) -> std::string
		{
			auto result = std::string{ aText };
			for (auto& value : result) {
				if (value >= 'A' && value <= 'F') {
					value = static_cast<char>(value + ('a' - 'A'));
				}
			}
			return result;
		}
		/// A positive decimal number of at most the given width; GitHub writes
		/// every identity and line in a link this way.
		template <typename TValue>
		auto positive_number(const std::string_view aText) -> std::optional<TValue>
		{
			auto       value  = TValue{ };
			const auto parsed = std::from_chars(aText.data(), aText.data() + aText.size(), value);
			if (aText.empty() || aText.front() == '0' || parsed.ec != std::errc{ } || parsed.ptr != aText.data() + aText.size()) {
				return std::nullopt;
			}
			return value;
		}
		/// [[BPR-0987]]. Reads a fragment GitHub writes on a pull request's
		/// pages into a typed location. A fragment it does not know is kept as
		/// unrecognized: the pull request is still named, the location is not.
		auto fragment_location(std::string_view aFragment) -> core::pull_request_location
		{
			constexpr auto maximumLine = std::uint32_t{ 10'000'000U };
			auto           result      = core::pull_request_location{ .Kind = core::pull_request_location_kind::unrecognized };
			const auto     identity    = [&result](const core::pull_request_location_kind aKind, const std::string_view aDigits) {
				if (const auto value = positive_number<std::uint64_t>(aDigits)) {
					result = core::pull_request_location{ .Kind = aKind, .Identity = *value };
				}
			};
			if (aFragment.starts_with("diff-")) {
				aFragment.remove_prefix(5U);
				if (aFragment.size() < 64U || !std::ranges::all_of(aFragment.substr(0U, 64U), hex_digit)) {
					return result;
				}
				auto file = core::pull_request_location{ .Kind = core::pull_request_location_kind::file, .FileDigest = lowercase(aFragment.substr(0U, 64U)) };
				aFragment.remove_prefix(64U);
				if (!aFragment.empty()) {
					const auto side = aFragment.front();
					const auto line = positive_number<std::uint32_t>(aFragment.substr(1U));
					if ((side != 'L' && side != 'R') || !line || *line > maximumLine) {
						return result;
					}
					file.Side = side == 'L' ? core::diff_side::left : core::diff_side::right;
					file.Line = *line;
				}
				return file;
			}
			if (aFragment.starts_with("discussion_r")) {
				identity(core::pull_request_location_kind::review_comment, aFragment.substr(12U));
			} else if (aFragment.starts_with("pullrequestreview-")) {
				identity(core::pull_request_location_kind::review, aFragment.substr(18U));
			} else if (aFragment.starts_with("issuecomment-")) {
				identity(core::pull_request_location_kind::issue_comment, aFragment.substr(13U));
			} else if (aFragment.starts_with('r')) {
				// [[BPR-1222]]. Files changed names a review comment `#rID`; a tour links its stops that way.
				identity(core::pull_request_location_kind::review_comment, aFragment.substr(1U));
			}
			return result;
		}
	}

	auto parse_repository_addition(std::string_view aText, const std::string_view aDefaultHostname)
		-> std::expected<repository_addition_target, repository_addition_input_problem>
	{
		using problem = repository_addition_input_problem;
		if (aText.size() > 4096U) {
			return std::unexpected{ problem::too_long };
		}
		constexpr auto whitespace = std::string_view{ " \t\r\n" };
		const auto     first      = aText.find_first_not_of(whitespace);
		if (first == std::string_view::npos) {
			return std::unexpected{ problem::empty };
		}
		aText = aText.substr(first, aText.find_last_not_of(whitespace) - first + 1U);
		if (std::ranges::any_of(aText, [](const unsigned char aByte) { return aByte <= 32U || aByte >= 127U || aByte == '\\'; })) {
			return std::unexpected{ problem::invalid_character };
		}
		auto hostname = aDefaultHostname;
		if (aText.starts_with("https://")) {
			aText.remove_prefix(8U);
			const auto slash = aText.find('/');
			if (slash == std::string_view::npos) {
				return std::unexpected{ problem::repository_root_required };
			}
			hostname = aText.substr(0U, slash);
			aText.remove_prefix(slash + 1U);
			if (aText.ends_with('/')) {
				aText.remove_suffix(1U);
			}
		}
		if (!valid_host(hostname)) {
			return std::unexpected{ problem::invalid_host };
		}
		const auto slash = aText.find('/');
		if (slash == std::string_view::npos || aText.find('/', slash + 1U) != std::string_view::npos
			|| aText.find_first_of("?#:") != std::string_view::npos) {
			return std::unexpected{ problem::repository_root_required };
		}
		auto result     = repository_addition_target{ };
		result.Hostname = hostname;
		std::ranges::transform(result.Hostname, result.Hostname.begin(), [](const char aValue) {
			return aValue >= 'A' && aValue <= 'Z' ? static_cast<char>(aValue + ('a' - 'A')) : aValue;
		});
		result.Repository.Owner = aText.substr(0U, slash);
		result.Repository.Name  = aText.substr(slash + 1U);
		if (!core::valid(result.Repository)) {
			return std::unexpected{ problem::invalid_repository };
		}
		return result;
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
		const auto commit     = path.starts_with("/commits/");
		if (!path.empty() && path != "/" && path != "/files" && path != "/commits") {
			if (!commit || path.size() <= 9U || path.size() > 73U || !std::ranges::all_of(path.substr(9U), hex_digit)) {
				return std::nullopt;
			}
		}
		if (path != "/") {
			result.Location = path;
		}
		result.Location += fragment;
		// [[BPR-0987]]. The typed location: a commit page names its commit
		// whatever its fragment says; otherwise the fragment names the
		// location and the page (canonical, files, commits) is a view.
		if (commit) {
			result.Target = core::pull_request_location{ .Kind = core::pull_request_location_kind::commit, .Commit = lowercase(path.substr(9U)) };
		} else if (fragment.size() > 1U) {
			result.Target = fragment_location(fragment.substr(1U));
		}
		return result;
	}

	setup_destination_host::setup_destination_host(setup_destination_port& aPort)
		: Port{ aPort }
	{ }
	auto setup_destination_host::pending() const -> const std::optional<core::pending_setup_destination>& { return Pending; }
	void setup_destination_host::submit_repository(const std::string_view aText)
	{
		Pending.reset();
		const auto target = parse_repository_addition(aText, Port.shorthand_hostname());
		if (!target || NextRequest == std::numeric_limits<std::uint64_t>::max()) {
			present("Enter owner/name or a complete HTTPS repository URL.");
			return;
		}
		auto destination       = core::setup_destination{ };
		destination.Hostname   = target->Hostname;
		destination.Repository = target->Repository;
		destination.Kind       = core::setup_destination_kind::repository;
		Pending                = core::pending_setup_destination{
			.Request     = core::setup_request_id{ NextRequest++ },
			.Owner       = Port.owner(),
			.Destination = std::move(destination),
		};
		resolve();
	}
	void setup_destination_host::present(std::string aNotice, const bool aOpened, const bool aNavigated)
	{
		Port.present({ .Pending = Pending, .Notice = std::move(aNotice), .Opened = aOpened, .Navigated = aNavigated });
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
	auto setup_destination_host::begin_authentication(const core::setup_request_id aRequest, const core::host_profile_id aProfile) -> bool
	{
		if (!Pending || Pending->Request != aRequest || Pending->Owner != Port.owner()
			|| (Pending->Stage != core::setup_destination_stage::add_server && Pending->Stage != core::setup_destination_stage::sign_in && Pending->Stage != core::setup_destination_stage::authenticating)) {
			return false;
		}
		const auto profiles = Port.profiles();
		const auto chosen   = std::ranges::find(profiles, aProfile, &setup_profile_observation::Profile);
		if (chosen == profiles.end()) {
			return false;
		}
		auto selected = core::resolve_setup_destination(*Pending, Port.owner(), std::span{ &*chosen, 1U });
		if (!selected || selected->Profile != aProfile) {
			return false;
		}
		selected->Stage = core::setup_destination_stage::authenticating;
		Pending         = *selected;
		present("Connecting to " + Pending->Destination.Hostname);
		return Pending == selected && Port.owner() == selected->Owner;
	}

	void setup_destination_host::committed(const core::setup_request_id aRequest)
	{
		if (Pending && Pending->Request == aRequest && Pending->Owner == Port.owner()) {
			Pending->Stage = core::setup_destination_stage::opening;
			present(Pending->Destination.Kind == core::setup_destination_kind::repository
						? "Sign-in accepted. Return to the repository draft and choose Add repository."
						: "Sign-in accepted. Opening the requested pull request.");
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
		const auto opening = Port.open(aProfile, *accepted);
		// [[BPR-0987]]. Whether the location can be shown is decided once the
		// receiving window has accepted the pull request, and said there.
		if (!Pending) {
			if (opening == setup_destination_opening::repository_prepared) {
				Port.present({ .RepositoryPrepared = true });
			} else if (opening == setup_destination_opening::unavailable) {
				present("The sign-in destination changed. Prepare the repository again.");
			} else {
				present({ }, true, opening == setup_destination_opening::navigated);
			}
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
