#include "bpr/core/release_update.hpp"

#include <algorithm>
#include <charconv>
#include <limits>
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
			if (aState.Pending) {
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
			if (aCommand == release_update_command::install) {
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
			aState.Pending     = release_update_context{ aState.Session, aState.NextRequest, aState.Generation, aState.Access, aOperation, aState.Candidate };
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

		void complete(release_update_state& aState, release_update_output& aOutput, const release_update_checked& aEvent, const release_update_time aTime)
		{
			if (!matches(aState, aEvent.Context, release_update_operation::check)) {
				aOutput.Diagnostic = release_update_reason::stale_completion;
				return;
			}
			auto selected = select_release_update(*aState.Installed, aEvent.Release);
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

	auto select_release_update(const release_update_version& aInstalled, const release_update_release& aRelease) -> release_update_selection
	{
		if (aRelease.Source != release_update_source::better_pull_review_github_com || aRelease.Id.value() == 0U || aRelease.Assets.size() > release_update_maximum_assets) {
			return { { }, release_update_reason::malformed_release };
		}
		if (aRelease.Draft || aRelease.Prerelease) {
			return { { }, release_update_reason::unstable_release };
		}
		const auto version = parse_release_update_version(aRelease.Tag);
		if (!version) {
			return { { }, release_update_reason::invalid_version };
		}
		if (*version <= aInstalled) {
			return { { }, release_update_reason::current };
		}
		const auto packageVersion = std::string_view{ aRelease.Tag }.substr(aRelease.Tag.starts_with('v') ? 1U : 0U);
		const auto assetName      = std::string{ "BetterPullReview-" } + std::string{ packageVersion } + "-macos-universal.zip";
		const auto count          = std::ranges::count_if(aRelease.Assets, [&](const auto& aAsset) { return aAsset.Name == assetName; });
		if (count != 1) {
			return { { }, count == 0 ? release_update_reason::no_compatible_asset : release_update_reason::ambiguous_asset };
		}
		const auto& asset = *std::ranges::find_if(aRelease.Assets, [&](const auto& aAsset) { return aAsset.Name == assetName; });
		if (asset.Id.value() == 0U || !asset.Uploaded || std::ranges::count(aRelease.Assets, asset.Id, &release_update_asset::Id) != 1) {
			return { { }, release_update_reason::malformed_release };
		}
		if (asset.Size == 0U || asset.Size > release_update_maximum_archive_bytes) {
			return { { }, release_update_reason::size_limit };
		}
		const auto digest = asset.Digest ? digest_bytes(*asset.Digest) : std::nullopt;
		if (asset.Digest && !digest) {
			return { { }, release_update_reason::invalid_digest };
		}
		return { release_update_candidate{ aRelease.Id, asset.Id, *version, aRelease.Tag, asset.Name, asset.Size, digest }, release_update_reason::none };
	}

	auto make_release_update_state(const application_session_id aSession, const std::string_view aInstalled,
								   const release_update_capability aCapability, release_update_preferences aPreferences, const release_update_time aTime) -> release_update_state
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
		state.Session          = aSession;
		state.Installed        = parse_release_update_version(aInstalled);
		state.Capability       = aCapability;
		state.Preferences      = std::move(aPreferences);
		state.MonotonicSeconds = aTime.MonotonicSeconds;
		state.ScheduleAnchor   = aTime.MonotonicSeconds;
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
			} else if constexpr (std::is_same_v<event_type, release_update_shutdown>) {
				if (!aState.Stopped) {
					invalidate(aState, output, release_update_reason::shutdown);
					aState.Stopped = true;
				}
			} else {
				complete(aState, output, aValue, aTime);
			}
		},
				   aEvent);
		return output;
	}
}
