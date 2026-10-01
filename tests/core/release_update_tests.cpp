#include "bpr/core/release_update.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
	using namespace bpr::core;
	using reason  = release_update_reason;
	using phase   = release_update_phase;
	using command = release_update_command;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto release(const std::string& aTag = "v2.3.4") -> release_update_release
	{
		const auto version = aTag.starts_with('v') ? aTag.substr(1U) : aTag;
		return { release_update_source::better_pull_review_github_com, release_update_release_id{ 10U }, aTag, false, false, { { release_update_asset_id{ 20U }, "BetterPullReview-" + version + "-macos-universal.zip", 100U, std::nullopt, true } } };
	}

	auto state(const release_update_preferences aPreferences = { }, const release_update_time aTime = { 100'000, 100U }) -> release_update_state
	{
		auto result = make_release_update_state(application_session_id{ 1U }, "1.2.3", release_update_capability::eligible, aPreferences, aTime);
		static_cast<void>(transition(result, release_update_access_changed{ release_update_access_id{ 7U }, release_update_access_kind::native_github_com }, aTime));
		return result;
	}

	auto begin(release_update_state& aState, const release_update_time aTime = { 100'000, 100U }) -> release_update_context
	{
		const auto output = transition(aState, release_update_intent{ command::check, { } }, aTime);
		expect(output.Requests.size() == 1U && aState.Phase == phase::checking, "manual check emits a request");
		return *aState.Pending;
	}

	void offer(release_update_state& aState)
	{
		const auto context = begin(aState);
		static_cast<void>(transition(aState, release_update_checked{ context, release() }, { 100'001, 101U }));
		expect(aState.Phase == phase::available && aState.Candidate, "newer exact ZIP is offered");
	}

	void test_version_grammar_and_precedence()
	{
		for (const auto tag : { "1.2.3", "v1.2.3", "1.2.3+0", "v1.2.3+build.001-a" }) {
			expect(parse_release_update_version(tag) == release_update_version{ 1U, 2U, 3U }, "metadata/v prefix do not affect precedence");
		}
		for (const auto tag : { "", "1", "1.2", "1.2.3.4", "v", "V1.2.3", "1.2.3-rc.1", "1.2.3+", "1.2.3+a..b", "1.2.3+.a", "1.2.3+a.", "1.2.3+a_b", "1.2.3+a+b", "01.2.3", "1.02.3", "1.2.03", "+1.2.3", " 1.2.3", "1.2.3\n", "1.2.18446744073709551616", "1.-2.3" }) {
			expect(!parse_release_update_version(tag), "malformed/prerelease/overflow version refuses");
		}
		expect(!parse_release_update_version("1.2.3+" + std::string(128U, 'a')), "tag has a fixed bound");
		expect(select_release_update({ 2U, 3U, 4U }, release("2.3.4+new-build")).Reason == reason::current, "equal build variants never reinstall");
		expect(select_release_update({ 10U, 0U, 0U }, release("9.99.99")).Reason == reason::current, "comparison is numeric, not lexical");
		expect(select_release_update({ 1U, 9U, 9U }, release("1.10.0")).Candidate.has_value(), "numeric minor increase offers");
	}

	void test_release_and_asset_selection()
	{
		const release_update_version installed{ 1U, 0U, 0U };
		auto                         candidate = release();
		candidate.Assets[0].Digest             = "sha256:" + std::string(64U, 'a');
		const auto selected                    = select_release_update(installed, candidate);
		expect(selected.Candidate && selected.Candidate->Release == candidate.Id
				   && selected.Candidate->Asset == candidate.Assets[0].Id && selected.Candidate->Tag == candidate.Tag
				   && selected.Candidate->Size == 100U && selected.Candidate->Sha256->front() == 0xaaU,
			   "complete candidate identity and digest bind together");
		candidate.Draft = true;
		expect(select_release_update(installed, candidate).Reason == reason::unstable_release, "draft refused");
		candidate.Draft      = false;
		candidate.Prerelease = true;
		expect(select_release_update(installed, candidate).Reason == reason::unstable_release, "prerelease flag refused");
		candidate        = release();
		candidate.Source = release_update_source::unrecognized;
		expect(select_release_update(installed, candidate).Reason == reason::malformed_release, "source fixed to distribution repo");
		candidate = release();
		candidate.Assets.push_back(candidate.Assets[0]);
		expect(select_release_update(installed, candidate).Reason == reason::ambiguous_asset, "duplicate matching assets refuse");
		candidate.Assets[1].Name = "another.zip";
		expect(select_release_update(installed, candidate).Reason == reason::malformed_release, "duplicate asset identity refuses even under another name");
		for (const auto name : { "source.zip", "BetterPullReview-2.3.4-macos-universal.dmg", "OtherProduct-2.3.4-macos-universal.zip", "BetterPullReview-2.3.4-macos-arm64.zip", "BetterPullReview-2.3.5-macos-universal.zip" }) {
			candidate                = release();
			candidate.Assets[0].Name = name;
			expect(select_release_update(installed, candidate).Reason == reason::no_compatible_asset, "exact universal ZIP only");
		}
		for (const auto digest : { "sha256:abc", "md5:00000000000000000000000000000000", "", "sha256:" }) {
			candidate                  = release();
			candidate.Assets[0].Digest = digest;
			expect(select_release_update(installed, candidate).Reason == reason::invalid_digest, "present malformed digest refuses");
		}
		candidate                  = release();
		candidate.Assets[0].Digest = "sha256:" + std::string(64U, 'z');
		expect(select_release_update(installed, candidate).Reason == reason::invalid_digest, "nonhex digest refuses");
		for (const auto bytes : { std::uint64_t{ 0U }, release_update_maximum_archive_bytes + 1U }) {
			candidate                = release();
			candidate.Assets[0].Size = bytes;
			expect(select_release_update(installed, candidate).Reason == reason::size_limit, "archive advertised bounds refuse");
		}
		candidate = release();
		candidate.Assets.resize(129U);
		expect(select_release_update(installed, candidate).Reason == reason::malformed_release, "asset list bound enforced");
		candidate                    = release();
		candidate.Assets[0].Uploaded = false;
		expect(select_release_update(installed, candidate).Reason == reason::malformed_release, "incomplete upload refuses");
	}

	void test_scheduling_preferences_and_failures()
	{
		auto current = state();
		auto output  = transition(current, release_update_tick{ }, { 100'000, 100U });
		expect(output.Requests.size() == 1U && current.Preferences.LastAttempt == 100'000, "first due automatic check records attempt");
		const auto pending = *current.Pending;
		static_cast<void>(transition(current, release_update_failed{ pending, reason::offline }, { 100'010, 110U }));
		expect(!current.Preferences.LastSuccess && current.Reason == reason::offline, "offline is failure and does not record success");
		output = transition(current, release_update_tick{ }, { 9'999'999, 111U });
		expect(output.Requests.empty() && output.Diagnostic == reason::not_due, "wall-clock jump cannot accelerate retry");
		output = transition(current, release_update_tick{ }, { 1, 86'500U });
		expect(output.Requests.size() == 1U, "monotonic full day permits retry despite backward wall clock");
		static_cast<void>(transition(current, release_update_failed{ *current.Pending, reason::rate_limited, 120U }, { 2, 86'501U }));
		output = transition(current, release_update_intent{ command::check, { } }, { 3, 86'502U });
		expect(output.Requests.empty() && output.Diagnostic == reason::rate_limited, "manual respects retry-after");
		output = transition(current, release_update_intent{ command::check, { } }, { 4, 86'621U });
		expect(output.Requests.size() == 1U, "manual bypasses daily delay after rate expiry");
		static_cast<void>(transition(current, release_update_checked{ *current.Pending, release() }, { 5, 86'622U }));
		const auto offered = current.Candidate;
		static_cast<void>(transition(current, release_update_intent{ command::later, offered }, { 6, 86'623U }));
		expect(!current.OfferVisible && !current.Preferences.SkippedVersion, "Later never skips");
		static_cast<void>(transition(current, release_update_intent{ command::skip, offered }, { 6, 86'623U }));
		expect(current.Preferences.SkippedVersion == offered->Version && !current.Candidate, "Skip stores the exact semantic version");
		static_cast<void>(transition(current, release_update_tick{ }, { 7, 173'021U }));
		static_cast<void>(transition(current, release_update_checked{ *current.Pending, release() }, { 8, 173'022U }));
		expect(!current.Candidate && current.Reason == reason::skipped, "automatic offer honors skip");
		const auto manual = begin(current, { 9, 173'023U });
		static_cast<void>(transition(current, release_update_checked{ manual, release() }, { 10, 173'024U }));
		expect(current.Candidate && current.Preferences.LastSuccess == 10, "manual ignores skip and records successful time separately");
		static_cast<void>(transition(current, release_update_tick{ }, { 11, 259'423U }));
		static_cast<void>(transition(current, release_update_checked{ *current.Pending, release("v3.0.0") }, { 12, 259'424U }));
		expect(current.Candidate && current.Candidate->Version == release_update_version{ 3U, 0U, 0U }, "Skip never suppresses a newer version");
		auto restored = state(current.Preferences, { 13, 5U });
		expect(restored.Preferences == current.Preferences
				   && transition(restored, release_update_tick{ }, { 13, 5U }).Requests.empty(),
			   "a restored preference snapshot preserves skip and remaining daily interval");

		auto future = state({ true, { }, 200'000, 200'000 }, { 100'000, 5U });
		expect(future.Preferences.LastAttempt == 100'000 && !future.Preferences.LastSuccess, "future wall facts normalized");
		expect(transition(future, release_update_tick{ }, { 100'000, 5U }).Requests.empty(), "future attempt waits a bounded day");
		expect(transition(future, release_update_tick{ }, { 100'000, 86'405U }).Requests.size() == 1U, "future attempt cannot suppress forever");
		auto negative = state({ true, { }, -1, -3 });
		expect(transition(negative, release_update_tick{ }, { 100'000, 100U }).Requests.size() == 1U, "negative persisted facts cannot suppress");
		auto       overflow = state({ }, { 100'000, std::numeric_limits<std::uint64_t>::max() });
		const auto initial  = transition(overflow, release_update_tick{ }, { 100'000, std::numeric_limits<std::uint64_t>::max() });
		expect(initial.Requests.size() == 1U, "maximum monotonic initial check allowed");
		static_cast<void>(transition(overflow, release_update_failed{ *overflow.Pending, reason::offline }, { 100'000, std::numeric_limits<std::uint64_t>::max() }));
		expect(transition(overflow, release_update_tick{ }, { 100'000, std::numeric_limits<std::uint64_t>::max() }).Requests.empty(), "monotonic arithmetic cannot wrap into a loop");
	}

	void test_capability_gates_and_identity()
	{
		for (const auto capability : { release_update_capability::managed, release_update_capability::unsupported_platform,
									   release_update_capability::untrusted_installation, release_update_capability::read_only,
									   release_update_capability::translocated, release_update_capability::mounted_image }) {
			auto current       = state();
			current.Capability = capability;
			expect(transition(current, release_update_tick{ }, { 100'000, 100U }).Requests.empty(), "ineligible installation issues no automatic request");
			expect(!release_update_commands(current)[0].enabled(), "manual fallback has typed unavailable command");
		}
		auto disabled = state({ false, { }, { }, { } });
		expect(transition(disabled, release_update_tick{ }, { 100'000, 100U }).Requests.empty(), "disabled automatic check sends nothing");
		expect(begin(disabled).Access.value() == 7U, "manual remains available when automatic checks disabled");
		for (const auto& event : { release_update_event{ release_update_access_changed{ } },
								   release_update_event{ release_update_set_automatic{ false } }, release_update_event{ release_update_shutdown{ } },
								   release_update_event{ release_update_intent{ command::cancel, { } } } }) {
			auto       current = state();
			const auto pending = begin(current);
			const auto output  = transition(current, event, { 100'000, 100U });
			expect(output.Requests.size() == 1U && output.Requests[0].Cancel, "invalidation cancels exact operation");
			expect(transition(current, release_update_checked{ pending, release() }, { 100'000, 100U }).Diagnostic == reason::stale_completion && !current.Candidate, "late completion cannot restore authority");
		}
		auto       current = state();
		const auto pending = begin(current);
		for (std::size_t field = 0; field < 6U; ++field) {
			auto changed = pending;
			switch (field) {
			case 0:  changed.Session = application_session_id{ 3U }; break;
			case 1:  changed.Request = request_id{ 3U }; break;
			case 2:  changed.Generation = release_update_generation{ 3U }; break;
			case 3:  changed.Access = release_update_access_id{ 3U }; break;
			case 4:  changed.Operation = release_update_operation::download; break;
			default: changed.Candidate = select_release_update({ 1U, 0U, 0U }, release()).Candidate; break;
			}
			expect(transition(current, release_update_checked{ changed, release() }, { 100'000, 100U }).Diagnostic == reason::stale_completion, "all context fields reject mismatch");
		}
		static_cast<void>(transition(current, release_update_checked{ pending, release() }, { 100'000, 100U }));
		expect(transition(current, release_update_checked{ pending, release() }, { 100'000, 100U }).Diagnostic == reason::stale_completion, "duplicate terminal rejected");
	}

	void test_exact_download_verification_course()
	{
		auto current = state();
		offer(current);
		// [[BPR-1262]]. The candidate is copied out of its optional before it
		// changes: GCC 15 at -O3 read the size inside a copied optional as
		// possibly uninitialized.
		auto wrong = *current.Candidate;
		++wrong.Size;
		expect(transition(current, release_update_intent{ command::download, wrong }, { 100'001, 101U }).Diagnostic == reason::changed_candidate, "gesture sealed to candidate");
		static_cast<void>(transition(current, release_update_intent{ command::download, current.Candidate }, { 100'001, 101U }));
		const auto download = *current.Pending;
		for (std::size_t field = 0U; field < 7U; ++field) {
			auto changed = download;
			switch (field) {
			case 0U: changed.Candidate->Release = release_update_release_id{ 99U }; break;
			case 1U: changed.Candidate->Asset = release_update_asset_id{ 99U }; break;
			case 2U: changed.Candidate->Tag = "v2.3.4+changed"; break;
			case 3U: changed.Candidate->AssetName += ".other"; break;
			case 4U: ++changed.Candidate->Size; break;
			case 5U: ++changed.Candidate->Version.Patch; break;
			default: changed.Candidate->Sha256 = std::array<std::uint8_t, 32>{ }; break;
			}
			expect(transition(current, release_update_downloaded{ changed, 100U }, { 100'001, 101U }).Diagnostic == reason::stale_completion,
				   "every selected release/asset/digest field is part of completion identity");
		}
		static_cast<void>(transition(current, release_update_progress{ download, 50U }, { 100'001, 101U }));
		expect(current.Bytes == 50U, "accepted progress projected");
		expect(transition(current, release_update_progress{ download, 101U }, { 100'001, 101U }).Diagnostic == reason::invalid_progress, "oversized progress refused");
		expect(transition(current, release_update_progress{ download, 49U }, { 100'001, 101U }).Diagnostic == reason::invalid_progress, "backward progress refused");
		static_cast<void>(transition(current, release_update_downloaded{ download, 100U }, { 100'002, 102U }));
		expect(current.Phase == phase::verifying && current.Pending->Candidate == download.Candidate, "download requests exact candidate verification");
		const auto verify = *current.Pending;
		expect(transition(current, release_update_verified{ download }, { 100'002, 102U }).Diagnostic == reason::stale_completion, "download context cannot certify verification");
		static_cast<void>(transition(current, release_update_verified{ verify }, { 100'003, 103U }));
		expect(current.Phase == phase::ready && !current.Pending, "native-port fact reaches ready");
		expect(transition(current, release_update_intent{ command::install, current.Candidate }, { 100'003, 103U }).Diagnostic == reason::installer_unavailable, "uncomposed installer cannot be enabled by verification");
		static_cast<void>(transition(current, release_update_access_changed{ release_update_access_id{ 8U }, release_update_access_kind::native_github_com }, { 100'004, 104U }));
		expect(!current.Candidate, "downloaded candidate does not survive account replacement as installable");
		for (const auto failure : { reason::unauthenticated, reason::forbidden, reason::inaccessible, reason::offline, reason::malformed_release, reason::certificate, reason::redirect, reason::timed_out }) {
			auto       failed  = state();
			const auto request = begin(failed);
			static_cast<void>(transition(failed, release_update_failed{ request, failure }, { 100'001, 101U }));
			expect(failed.Phase == phase::failed && failed.Reason == failure && !failed.Preferences.LastSuccess, "typed failures never report current");
		}
	}
}

auto main() -> int
{
	for (const bool changed : { false, true }) {
		auto current               = state();
		current.InstallerAvailable = true;
		offer(current);
		static_cast<void>(transition(current, release_update_intent{ command::download, current.Candidate }, { 100002, 102 }));
		static_cast<void>(transition(current, release_update_downloaded{ *current.Pending, 100U }, { 100002, 102 }));
		static_cast<void>(transition(current, release_update_verified{ *current.Pending }, { 100002, 102 }));
		const auto selected = current.Candidate;
		static_cast<void>(transition(current, release_update_intent{ command::install, selected }, { 100003, 103 }));
		expect(current.Pending && current.Pending->Operation == release_update_operation::refresh_install, "install first refreshes current authority and candidate");
		auto metadata = release();
		if (changed) {
			metadata.Assets.front().Digest = "sha256:" + std::string(64U, 'a');
		}
		const auto refreshed = transition(current, release_update_checked{ *current.Pending, metadata }, { 100003, 103 });
		if (changed) {
			expect(current.Phase == phase::failed && current.Reason == reason::changed_candidate && refreshed.Requests.empty(), "changed digest cannot transfer installation");
		} else {
			expect(current.Pending && current.Pending->Operation == release_update_operation::install && current.Candidate == selected, "exact refreshed candidate transfers install capability");
			const auto install = *current.Pending;
			static_cast<void>(transition(current, release_update_handed_off{ install }, { 100004, 104 }));
			expect(current.Phase == phase::handed_off && !current.Pending, "helper acceptance is not installation success");
			expect(transition(current, release_update_handed_off{ install }, { 100004, 104 }).Diagnostic == reason::stale_completion, "duplicate handoff is stale");
		}
	}
	test_version_grammar_and_precedence();
	test_release_and_asset_selection();
	test_scheduling_preferences_and_failures();
	test_capability_gates_and_identity();
	test_exact_download_verification_course();
	std::cout << "release-update-core-tests:pass\n";
}
