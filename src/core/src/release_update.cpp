#include "bpr/core/release_update.hpp"

#include <algorithm>
#include <charconv>
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto ascii_digit(const char aValue) noexcept -> bool
		{
			return aValue >= '0' && aValue <= '9';
		}

		[[nodiscard]] auto valid_build_metadata(const std::string_view aText) noexcept -> bool
		{
			bool identifier = false;
			for (const auto value : aText) {
				if (value == '.') {
					if (!identifier) {
						return false;
					}
					identifier = false;
				} else {
					if (!ascii_digit(value) && !(value >= 'A' && value <= 'Z')
						&& !(value >= 'a' && value <= 'z') && value != '-') {
						return false;
					}
					identifier = true;
				}
			}
			return identifier;
		}

		[[nodiscard]] auto parse_component(std::string_view& aText, const bool aLast)
			-> std::optional<std::uint64_t>
		{
			const auto end = aText.find('.');
			if (aLast != (end == std::string_view::npos)) {
				return std::nullopt;
			}
			const auto component = aText.substr(0U, end);
			if (component.empty() || (component.size() > 1U && component.front() == '0')
				|| !std::ranges::all_of(component, ascii_digit)) {
				return std::nullopt;
			}
			std::uint64_t value  = 0U;
			const auto    result = std::from_chars(component.data(), component.data() + component.size(), value);
			if (result.ec != std::errc{ }) {
				return std::nullopt;
			}
			aText = aLast ? std::string_view{ } : aText.substr(end + 1U);
			return value;
		}

		[[nodiscard]] auto package_version(const std::string_view aTag) -> std::string_view
		{
			return aTag.starts_with('v') ? aTag.substr(1U) : aTag;
		}

		constexpr std::string_view build_marker = "-build.";

		[[nodiscard]] auto build_number(std::string_view aText) noexcept -> std::optional<std::uint32_t>
		{
			const auto number = aText.size() <= 4U ? parse_component(aText, true) : std::nullopt;
			if (!number || *number == 0U || *number > release_update_maximum_build_number) {
				return std::nullopt;
			}
			return static_cast<std::uint32_t>(*number);
		}

		[[nodiscard]] auto package_build_number(const std::string_view aVersion, std::string_view aName)
			-> std::optional<std::uint32_t>
		{
			constexpr std::string_view prefix = "BetterPullReview-";
			constexpr std::string_view suffix = "-macos-universal.zip";
			if (aName.size() < prefix.size() + suffix.size() || !aName.starts_with(prefix) || !aName.ends_with(suffix)) {
				return std::nullopt;
			}
			aName.remove_prefix(prefix.size());
			aName.remove_suffix(suffix.size());
			if (aName == aVersion) {
				return 0U;
			}
			if (aVersion.contains('+') || !aName.starts_with(aVersion)) {
				return std::nullopt;
			}
			aName.remove_prefix(aVersion.size());
			return aName.starts_with(build_marker) ? build_number(aName.substr(build_marker.size())) : std::nullopt;
		}

		[[nodiscard]] auto matching_build(std::string_view aTag, const std::optional<release_update_prerelease_tag>& aPrerelease,
										  const release_update_asset& aAsset) -> std::optional<std::uint32_t>
		{
			const auto version = package_version(aPrerelease ? aPrerelease->MarketingTag : aTag);
			const auto build   = package_build_number(version, aAsset.Name);
			return aPrerelease && build != aPrerelease->BuildNumber ? std::nullopt : build;
		}

		[[nodiscard]] auto digest_bytes(const std::string_view aText)
			-> std::optional<std::array<std::uint8_t, 32>>
		{
			if (aText.size() != 71U || !aText.starts_with("sha256:")) {
				return std::nullopt;
			}
			std::array<std::uint8_t, 32> bytes{ };
			for (std::size_t index = 0U; index < bytes.size(); ++index) {
				const auto* begin  = aText.data() + 7U + 2U * index;
				const auto  result = std::from_chars(begin, begin + 2U, bytes[index], 16);
				if (result.ec != std::errc{ } || result.ptr != begin + 2U) {
					return std::nullopt;
				}
			}
			return bytes;
		}

		[[nodiscard]] auto capability_reason(const release_update_capability aCapability) -> release_update_reason
		{
			switch (aCapability) {
			case release_update_capability::eligible:               return release_update_reason::none;
			case release_update_capability::managed:                return release_update_reason::managed;
			case release_update_capability::unsupported_platform:   return release_update_reason::unsupported_platform;
			case release_update_capability::untrusted_installation: return release_update_reason::untrusted_installation;
			case release_update_capability::read_only:              return release_update_reason::read_only;
			case release_update_capability::translocated:           return release_update_reason::translocated;
			case release_update_capability::mounted_image:          return release_update_reason::mounted_image;
			}
			return release_update_reason::unavailable;
		}

		[[nodiscard]] auto elapsed(const std::uint64_t aNow, const std::uint64_t aAnchor, const std::uint64_t aDelay) -> bool
		{
			return aNow >= aAnchor && aNow - aAnchor >= aDelay;
		}

		[[nodiscard]] auto admission_reason(const release_update_state& aState) -> release_update_reason
		{
			if (aState.Stopped) {
				return release_update_reason::shutdown;
			}
			if (aState.Exhausted) {
				return release_update_reason::identity_exhausted;
			}
			if (aState.Pending || aState.Phase == release_update_phase::handed_off) {
				return release_update_reason::busy;
			}
			if (const auto reason = capability_reason(aState.Capability); reason != release_update_reason::none) {
				return reason;
			}
			if (!aState.Installed) {
				return release_update_reason::invalid_version;
			}
			if (aState.Access.value() == 0U) {
				return release_update_reason::access_required;
			}
			if (!elapsed(aState.MonotonicSeconds, aState.RateAnchor, aState.RateDelay)) {
				return release_update_reason::rate_limited;
			}
			return release_update_reason::none;
		}

		[[nodiscard]] auto command_reason(const release_update_state& aState, const release_update_command aCommand) -> release_update_reason
		{
			if (aCommand > release_update_command::install) {
				return release_update_reason::unavailable;
			}
			if (aState.Stopped) {
				return release_update_reason::shutdown;
			}
			if (aCommand == release_update_command::install && !aState.InstallerAvailable) {
				return release_update_reason::installer_unavailable;
			}
			if (aCommand == release_update_command::cancel) {
				return aState.Pending ? release_update_reason::none : release_update_reason::no_candidate;
			}
			if (const auto reason = admission_reason(aState); reason != release_update_reason::none) {
				return reason;
			}
			if (aCommand == release_update_command::check) {
				return release_update_reason::none;
			}
			if (!aState.Candidate) {
				return release_update_reason::no_candidate;
			}
			if (aCommand == release_update_command::download && aState.Phase != release_update_phase::available) {
				return release_update_reason::no_candidate;
			}
			if (aCommand == release_update_command::install && aState.Phase != release_update_phase::ready) {
				return release_update_reason::no_candidate;
			}
			if (aCommand == release_update_command::later && !aState.OfferVisible) {
				return release_update_reason::no_candidate;
			}
			return release_update_reason::none;
		}

		void invalidate(release_update_state& aState, release_update_output& aOutput, const release_update_reason aReason)
		{
			if (aState.Pending) {
				aOutput.Requests.push_back({ *aState.Pending, true });
			}
			aState.Pending.reset();
			aState.Candidate.reset();
			aState.Bytes        = 0U;
			aState.OfferVisible = false;
			aState.Phase        = release_update_phase::idle;
			aState.Reason       = aReason;
			if (aState.Generation.value() == std::numeric_limits<std::uint64_t>::max()) {
				aState.Exhausted = true;
			} else {
				aState.Generation = release_update_generation{ aState.Generation.value() + 1U };
			}
			aOutput.Changed  = true;
			aOutput.Accepted = true;
		}

		void request(release_update_state& aState, release_update_output& aOutput, const release_update_operation aOperation)
		{
			if (aState.NextRequest.value() == 0U || aState.NextRequest.value() == std::numeric_limits<std::uint64_t>::max() || aState.Session.value() == 0U) {
				aState.Exhausted = true;
				aState.Phase     = release_update_phase::failed;
				aState.Reason    = release_update_reason::identity_exhausted;
				aOutput.Changed  = true;
				return;
			}
			aState.Pending     = release_update_context{ aState.Session, aState.NextRequest, aState.Generation, aState.Access, aOperation, aState.Candidate, aState.Preferences.Channel };
			aState.NextRequest = request_id{ aState.NextRequest.value() + 1U };
			aOutput.Requests.push_back({ *aState.Pending, false });
			aOutput.Accepted = true;
			aOutput.Changed  = true;
		}

		void check(release_update_state& aState, release_update_output& aOutput, const release_update_check_mode aMode, const release_update_time aTime)
		{
			if (const auto reason = admission_reason(aState); reason != release_update_reason::none) {
				aOutput.Diagnostic = reason;
				if (aMode == release_update_check_mode::manual && !aState.Pending && !aState.Stopped) {
					aState.Phase    = release_update_phase::failed;
					aState.Reason   = reason;
					aOutput.Changed = true;
				}
				return;
			}
			if (aMode == release_update_check_mode::automatic) {
				if (!aState.Preferences.AutomaticChecks) {
					aOutput.Diagnostic = release_update_reason::disabled;
					return;
				}
				if (!elapsed(aState.MonotonicSeconds, aState.ScheduleAnchor, aState.ScheduleDelay)) {
					aOutput.Diagnostic = release_update_reason::not_due;
					return;
				}
			}
			aState.Candidate.reset();
			aState.OfferVisible            = false;
			aState.Bytes                   = 0U;
			aState.CheckMode               = aMode;
			aState.Phase                   = release_update_phase::checking;
			aState.Reason                  = release_update_reason::none;
			aState.ScheduleAnchor          = aState.MonotonicSeconds;
			aState.ScheduleDelay           = release_update_day_seconds;
			aState.Preferences.LastAttempt = aTime.WallSeconds > 0 ? std::optional{ aTime.WallSeconds } : std::nullopt;
			aOutput.PreferencesChanged     = true;
			request(aState, aOutput, release_update_operation::check);
		}

		void intent(release_update_state& aState, release_update_output& aOutput, const release_update_intent& aIntent, const release_update_time aTime)
		{
			if (aIntent.Command == release_update_command::check) {
				check(aState, aOutput, release_update_check_mode::manual, aTime);
				return;
			}
			if (const auto reason = command_reason(aState, aIntent.Command); reason != release_update_reason::none) {
				aOutput.Diagnostic = reason;
				return;
			}
			if (aIntent.Command == release_update_command::cancel) {
				invalidate(aState, aOutput, release_update_reason::cancelled);
				return;
			}
			if (aIntent.Candidate != aState.Candidate) {
				aOutput.Diagnostic = release_update_reason::changed_candidate;
				return;
			}
			aOutput.Accepted = true;
			aOutput.Changed  = true;
			switch (aIntent.Command) {
			case release_update_command::install:
				aState.Phase = release_update_phase::quiescing;
				request(aState, aOutput, release_update_operation::refresh_install);
				break;
			case release_update_command::download:
				aState.Phase = release_update_phase::downloading;
				aState.Bytes = 0U;
				request(aState, aOutput, release_update_operation::download);
				break;
			case release_update_command::later: aState.OfferVisible = false; break;
			case release_update_command::skip:
				aState.Preferences.SkippedVersion = aState.Candidate->Version;
				aOutput.PreferencesChanged        = true;
				invalidate(aState, aOutput, release_update_reason::skipped);
				break;
			default: break;
			}
		}

		[[nodiscard]] auto matches(const release_update_state& aState, const release_update_context& aContext, const release_update_operation aOperation) -> bool
		{
			return !aState.Stopped && aState.Pending && aContext.Operation == aOperation && *aState.Pending == aContext;
		}

		void fail(release_update_state& aState, release_update_output& aOutput, const release_update_reason aReason)
		{
			aState.Pending.reset();
			aState.Candidate.reset();
			aState.OfferVisible = false;
			aState.Phase        = release_update_phase::failed;
			aState.Reason       = aReason;
			aOutput.Accepted    = true;
			aOutput.Changed     = true;
		}

		auto select_from_listing(const release_update_version& aInstalled, std::uint32_t aInstalledBuildNumber,
								 const std::vector<release_update_release>& aReleases, release_update_channel aChannel)
			-> release_update_selection
		{
			if (aChannel == release_update_channel::stable) {
				return aReleases.size() == 1U ? select_release_update(aInstalled, aReleases.front(), aInstalledBuildNumber)
											  : release_update_selection{ { }, release_update_reason::malformed_release };
			}
			if (aReleases.size() > release_update_maximum_releases) {
				return { { }, release_update_reason::size_limit };
			}
			using rank = std::tuple<release_update_version, std::uint32_t, bool>;
			std::optional<rank>      bestRank;
			release_update_selection best{ { }, release_update_reason::current };
			bool                     ambiguous = false;
			for (const auto& release : aReleases) {
				auto selected = select_release_update(aInstalled, release, aInstalledBuildNumber, aChannel);
				if (!selected.Candidate) {
					if (!bestRank && selected.Reason != release_update_reason::unstable_release
						&& selected.Reason != release_update_reason::current
						&& (best.Reason == release_update_reason::current || selected.Reason < best.Reason)) {
						best.Reason = selected.Reason;
					}
					continue;
				}
				const rank current{ selected.Candidate->Version, selected.Candidate->BuildNumber, !release.Prerelease };
				if (!bestRank || current > *bestRank) {
					bestRank  = current;
					ambiguous = std::ranges::count(aReleases, release.Id, &release_update_release::Id) != 1;
					best      = std::move(selected);
				} else if (current == *bestRank) {
					ambiguous = true;
				}
			}
			return ambiguous ? release_update_selection{ { }, release_update_reason::ambiguous_asset } : best;
		}

		void complete(release_update_state& aState, release_update_output& aOutput, const release_update_checked& aEvent, const release_update_time aTime)
		{
			const auto refreshing = aEvent.Context.Operation == release_update_operation::refresh_install;
			if (!matches(aState, aEvent.Context, refreshing ? release_update_operation::refresh_install : release_update_operation::check)) {
				aOutput.Diagnostic = release_update_reason::stale_completion;
				return;
			}
			auto selected = select_from_listing(*aState.Installed, aState.InstalledBuildNumber, aEvent.Releases, aEvent.Context.Channel);
			if (refreshing) {
				if (!selected.Candidate || selected.Candidate != aState.Candidate) {
					fail(aState, aOutput, release_update_reason::changed_candidate);
					return;
				}
				aState.Pending.reset();
				request(aState, aOutput, release_update_operation::install);
				return;
			}
			if (selected.Reason != release_update_reason::none && selected.Reason != release_update_reason::current) {
				fail(aState, aOutput, selected.Reason);
				return;
			}
			aState.Pending.reset();
			aState.Preferences.LastSuccess = aTime.WallSeconds > 0 ? std::optional{ aTime.WallSeconds } : std::nullopt;
			aOutput.PreferencesChanged     = true;
			aOutput.Accepted               = true;
			aOutput.Changed                = true;
			aState.Reason                  = selected.Reason;
			aState.Phase                   = release_update_phase::idle;
			if (!selected.Candidate) {
				return;
			}
			if (aState.CheckMode == release_update_check_mode::automatic && aState.Preferences.SkippedVersion == selected.Candidate->Version) {
				aState.Reason = release_update_reason::skipped;
				return;
			}
			aState.Candidate    = std::move(selected.Candidate);
			aState.Phase        = release_update_phase::available;
			aState.OfferVisible = true;
		}

		void complete(release_update_state& aState, release_update_output& aOutput, const release_update_downloaded& aEvent, release_update_time)
		{
			if (!matches(aState, aEvent.Context, release_update_operation::download)) {
				aOutput.Diagnostic = release_update_reason::stale_completion;
				return;
			}
			if (aEvent.Bytes != aState.Candidate->Size) {
				fail(aState, aOutput, release_update_reason::size_limit);
				return;
			}
			aState.Pending.reset();
			aState.Bytes = aEvent.Bytes;
			aState.Phase = release_update_phase::verifying;
			request(aState, aOutput, release_update_operation::verify);
		}

		void complete(release_update_state& aState, release_update_output& aOutput, const release_update_verified& aEvent, release_update_time)
		{
			if (!matches(aState, aEvent.Context, release_update_operation::verify)) {
				aOutput.Diagnostic = release_update_reason::stale_completion;
				return;
			}
			aState.Pending.reset();
			aState.Phase        = release_update_phase::ready;
			aState.OfferVisible = true;
			aOutput.Accepted    = true;
			aOutput.Changed     = true;
		}

		void complete(release_update_state& aState, release_update_output& aOutput, const release_update_failed& aEvent, release_update_time)
		{
			if (!matches(aState, aEvent.Context, aEvent.Context.Operation)) {
				aOutput.Diagnostic = release_update_reason::stale_completion;
				return;
			}
			if (aEvent.Reason == release_update_reason::rate_limited) {
				aState.RateAnchor = aState.MonotonicSeconds;
				aState.RateDelay  = std::max(std::uint64_t{ 60U }, aEvent.RetryAfterSeconds);
			}
			fail(aState, aOutput, aEvent.Reason == release_update_reason::none ? release_update_reason::unavailable : aEvent.Reason);
		}

		void complete(release_update_state& aState, release_update_output& aOutput, const release_update_handed_off& aEvent, release_update_time)
		{
			if (!matches(aState, aEvent.Context, release_update_operation::install)) {
				aOutput.Diagnostic = release_update_reason::stale_completion;
				return;
			}
			aState.Pending.reset();
			aState.Phase     = release_update_phase::handed_off;
			aState.Reason    = release_update_reason::none;
			aOutput.Accepted = true;
			aOutput.Changed  = true;
		}

		void complete(release_update_state& aState, release_update_output& aOutput, const release_update_progress& aEvent, release_update_time)
		{
			if (!matches(aState, aEvent.Context, release_update_operation::download)) {
				aOutput.Diagnostic = release_update_reason::stale_completion;
				return;
			}
			if (aEvent.Bytes > aState.Candidate->Size || aEvent.Bytes < aState.Bytes) {
				aOutput.Diagnostic = release_update_reason::invalid_progress;
				return;
			}
			aOutput.Accepted = true;
			aOutput.Changed  = aState.Bytes != aEvent.Bytes;
			aState.Bytes     = aEvent.Bytes;
		}
	}

	auto parse_release_update_version(std::string_view aTag) noexcept -> std::optional<release_update_version>
	{
		if (aTag.empty() || aTag.size() > release_update_maximum_tag_bytes) {
			return std::nullopt;
		}
		if (aTag.front() == 'v') {
			aTag.remove_prefix(1U);
		}
		if (const auto build = aTag.find('+'); build != std::string_view::npos) {
			if (!valid_build_metadata(aTag.substr(build + 1U))) {
				return std::nullopt;
			}
			aTag = aTag.substr(0U, build);
		}
		const auto major = parse_component(aTag, false);
		const auto minor = parse_component(aTag, false);
		const auto patch = parse_component(aTag, true);
		if (!major || !minor || !patch) {
			return std::nullopt;
		}
		return release_update_version{ *major, *minor, *patch };
	}

	auto parse_release_update_prerelease_tag(const std::string_view aTag) noexcept -> std::optional<release_update_prerelease_tag>
	{
		if (!aTag.starts_with('v') || aTag.size() > release_update_maximum_tag_bytes) {
			return std::nullopt;
		}
		const auto separator = aTag.find(build_marker);
		if (separator == std::string_view::npos) {
			return std::nullopt;
		}
		const auto marketing = aTag.substr(0U, separator);
		const auto build     = build_number(aTag.substr(separator + build_marker.size()));
		if (!build || marketing.contains('+') || !parse_release_update_version(marketing)) {
			return std::nullopt;
		}
		return release_update_prerelease_tag{ marketing, *build };
	}

	auto release_update_asset_candidate(const release_update_release_id aRelease, const std::string_view aTag,
										const release_update_asset& aAsset) -> release_update_selection
	{
		const auto prerelease = parse_release_update_prerelease_tag(aTag);
		const auto version    = parse_release_update_version(prerelease ? prerelease->MarketingTag : aTag);
		if (!version) {
			return { { }, release_update_reason::invalid_version };
		}
		const auto build = matching_build(aTag, prerelease, aAsset);
		if (!build) {
			return { { }, release_update_reason::no_compatible_asset };
		}
		if (aRelease.value() == 0U || aAsset.Id.value() == 0U || !aAsset.Uploaded) {
			return { { }, release_update_reason::malformed_release };
		}
		if (aAsset.Size == 0U || aAsset.Size > release_update_maximum_archive_bytes) {
			return { { }, release_update_reason::size_limit };
		}
		const auto digest = aAsset.Digest ? digest_bytes(*aAsset.Digest) : std::nullopt;
		if (aAsset.Digest && !digest) {
			return { { }, release_update_reason::invalid_digest };
		}
		return { release_update_candidate{ aRelease, aAsset.Id, *version, std::string{ aTag }, aAsset.Name, aAsset.Size, digest, *build }, release_update_reason::none };
	}

	auto select_release_update(const release_update_version& aInstalled, const release_update_release& aRelease,
							   const std::uint32_t aInstalledBuildNumber, const release_update_channel aChannel) -> release_update_selection
	{
		if (aRelease.Source != release_update_source::better_pull_review_github_com || aRelease.Id.value() == 0U || aRelease.Assets.size() > release_update_maximum_assets) {
			return { { }, release_update_reason::malformed_release };
		}
		if (aRelease.Draft || (aRelease.Prerelease && aChannel != release_update_channel::include_prereleases)) {
			return { { }, release_update_reason::unstable_release };
		}
		const auto prerelease = aRelease.Prerelease ? parse_release_update_prerelease_tag(aRelease.Tag) : std::nullopt;
		const auto version    = parse_release_update_version(prerelease ? prerelease->MarketingTag : aRelease.Tag);
		if (!version || (aRelease.Prerelease && !prerelease)) {
			return { { }, release_update_reason::invalid_version };
		}
		const bool sameVersion         = *version == aInstalled;
		const bool knownInstalledBuild = aInstalledBuildNumber > 0U && aInstalledBuildNumber <= release_update_maximum_build_number;
		if (*version < aInstalled || (sameVersion && (!knownInstalledBuild || *version == release_update_version{ }))) {
			return { { }, release_update_reason::current };
		}
		const bool numbered = sameVersion || prerelease.has_value()
			|| (aChannel == release_update_channel::include_prereleases && std::ranges::any_of(aRelease.Assets, [&](const auto& aAsset) {
								  return matching_build(aRelease.Tag, prerelease, aAsset).value_or(0U) != 0U;
							  }));
		const auto matches = [&](const release_update_asset& aAsset) {
			const auto build = matching_build(aRelease.Tag, prerelease, aAsset);
			return build && (*build != 0U) == numbered;
		};
		const auto count = std::ranges::count_if(aRelease.Assets, matches);
		if (count == 0) {
			return { { }, sameVersion && !prerelease ? release_update_reason::current : release_update_reason::no_compatible_asset };
		}
		if (count > 1) {
			return { { }, release_update_reason::ambiguous_asset };
		}
		const auto& asset = *std::ranges::find_if(aRelease.Assets, matches);
		if (std::ranges::count(aRelease.Assets, asset.Id, &release_update_asset::Id) != 1) {
			return { { }, release_update_reason::malformed_release };
		}
		auto selected = release_update_asset_candidate(aRelease.Id, aRelease.Tag, asset);
		if (sameVersion && selected.Candidate && selected.Candidate->BuildNumber <= aInstalledBuildNumber) {
			return { { }, release_update_reason::current };
		}
		return selected;
	}

	auto make_release_update_state(const application_session_id aSession, const std::string_view aInstalled,
								   const release_update_capability aCapability, release_update_preferences aPreferences,
								   const release_update_time aTime, const std::uint32_t aInstalledBuildNumber) -> release_update_state
	{
		const auto now = std::max(std::int64_t{ 0 }, aTime.WallSeconds);
		if (aPreferences.LastAttempt && *aPreferences.LastAttempt <= 0) {
			aPreferences.LastAttempt.reset();
		}
		if (aPreferences.LastAttempt && *aPreferences.LastAttempt > now) {
			aPreferences.LastAttempt = now;
		}
		if (aPreferences.LastSuccess && (*aPreferences.LastSuccess <= 0 || *aPreferences.LastSuccess > now)) {
			aPreferences.LastSuccess.reset();
		}
		release_update_state state;
		state.Session              = aSession;
		state.Installed            = parse_release_update_version(aInstalled);
		state.InstalledBuildNumber = aInstalledBuildNumber;
		state.Capability           = aCapability;
		state.Preferences          = std::move(aPreferences);
		state.MonotonicSeconds     = aTime.MonotonicSeconds;
		state.ScheduleAnchor       = aTime.MonotonicSeconds;
		if (state.Preferences.LastAttempt) {
			const auto age      = static_cast<std::uint64_t>(now - *state.Preferences.LastAttempt);
			state.ScheduleDelay = release_update_day_seconds - std::min(age, release_update_day_seconds);
		}
		return state;
	}

	auto release_update_commands(const release_update_state& aState) -> std::array<release_update_command_projection, 6>
	{
		std::array<release_update_command_projection, 6> commands{ };
		for (std::size_t index = 0U; index < commands.size(); ++index) {
			const auto command = static_cast<release_update_command>(index);
			commands[index]    = { command, command_reason(aState, command) };
		}
		return commands;
	}

	auto release_update_can_change_channel(const release_update_state& aState) noexcept -> bool
	{
		return !aState.Stopped && aState.Capability == release_update_capability::eligible
			&& aState.Phase != release_update_phase::handed_off
			&& (!aState.Pending || aState.Pending->Operation != release_update_operation::install);
	}

	auto transition(release_update_state& aState, const release_update_event& aEvent, const release_update_time aTime) -> release_update_output
	{
		release_update_output output;
		const bool            rateBlocked = !elapsed(aState.MonotonicSeconds, aState.RateAnchor, aState.RateDelay);
		aState.MonotonicSeconds           = std::max(aState.MonotonicSeconds, aTime.MonotonicSeconds);
		output.Changed                    = rateBlocked && elapsed(aState.MonotonicSeconds, aState.RateAnchor, aState.RateDelay);
		std::visit([&](const auto& aValue) {
			using event_type = std::decay_t<decltype(aValue)>;
			if constexpr (std::is_same_v<event_type, release_update_intent>) {
				intent(aState, output, aValue, aTime);
			} else if constexpr (std::is_same_v<event_type, release_update_tick>) {
				check(aState, output, release_update_check_mode::automatic, aTime);
			} else if constexpr (std::is_same_v<event_type, release_update_access_changed>) {
				if (!aState.Stopped) {
					invalidate(aState, output, release_update_reason::access_required);
					aState.Access = aValue.Kind == release_update_access_kind::native_github_com ? aValue.Access : release_update_access_id{ };
					if (aState.Access.value() != 0U) {
						aState.Reason = release_update_reason::none;
					}
				}
			} else if constexpr (std::is_same_v<event_type, release_update_set_automatic>) {
				if (!aState.Stopped && aState.Preferences.AutomaticChecks != aValue.Enabled) {
					aState.Preferences.AutomaticChecks = aValue.Enabled;
					output.PreferencesChanged          = true;
					invalidate(aState, output, aValue.Enabled ? release_update_reason::none : release_update_reason::disabled);
				}
			} else if constexpr (std::is_same_v<event_type, release_update_set_channel>) {
				if (release_update_can_change_channel(aState) && aState.Preferences.Channel != aValue.Channel
					&& (aValue.Channel == release_update_channel::stable || aValue.Channel == release_update_channel::include_prereleases)) {
					aState.Preferences.Channel = aValue.Channel;
					aState.Preferences.LastSuccess.reset();
					output.PreferencesChanged = true;
					invalidate(aState, output, release_update_reason::none);
				}
			} else if constexpr (std::is_same_v<event_type, release_update_shutdown>) {
				if (!aState.Stopped) {
					invalidate(aState, output, release_update_reason::shutdown);
					aState.Stopped = true;
				}
			} else if constexpr (std::is_same_v<event_type, release_update_installation_result>) {
				if (!aState.Stopped) {
					aState.InstallationOutcome = aValue.Installed;
					if (aState.Phase == release_update_phase::handed_off) {
						invalidate(aState, output, aValue.Installed ? release_update_reason::none : release_update_reason::recovery_required);
						aState.Phase = aValue.Installed ? release_update_phase::idle : release_update_phase::recovered;
					}
					output.Changed  = true;
					output.Accepted = true;
				}
			} else {
				complete(aState, output, aValue, aTime);
			}
		},
				   aEvent);
		return output;
	}
}
