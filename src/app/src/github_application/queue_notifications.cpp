#include "github_application/queue_notifications.hpp"

#include "bpr/application/github_host_authority.hpp"
#include "bpr/core/github_model.hpp"

#include <algorithm>
#include <chrono>
#include <iterator>
#include <string>
#include <vector>

#if defined(BPR_BUILD_TESTING)
	#include "bpr/application/review_queue_notification.hpp"
#endif

namespace bpr::app {
	auto queue_observation_for(
		const application::accepted_pull_request_handoff& aHandoff,
		const application::github_pull_request_snapshot&  aSnapshot)
		-> core::pull_request_observation
	{
		auto paths = std::vector<std::string>{ };
		paths.reserve(aSnapshot.Files.size());
		std::ranges::transform(
			aSnapshot.Files,
			std::back_inserter(paths),
			[](const application::github_file_record& aFile) {
				return aFile.Path;
			});
		return {
			.Ref = {
				.Host       = std::string{ aHandoff.Hostname.value() },
				.Account    = aSnapshot.Viewer.Login,
				.Repository = aSnapshot.Repository.Owner + "/"
					+ aSnapshot.Repository.Name,
				.Number = aSnapshot.PullRequest.Number.value(),
			},
			.Author = aSnapshot.PullRequest.Author.has_value() ? aSnapshot.PullRequest.Author->Login : std::string{ },
			// Exact accepted provider identity establishes authorship without saving a repository.
			.Role         = core::github_role::none,
			.ChangedPaths = std::move(paths),
			.Title        = aSnapshot.PullRequest.Title,
			.Draft        = aSnapshot.PullRequest.Draft,
		};
	}

	system_discovery_clock::system_discovery_clock(std::shared_ptr<const std::atomic<std::int64_t>> aOffset)
		: Offset{ std::move(aOffset) }
	{
	}

	auto system_discovery_clock::now_seconds() const -> std::int64_t
	{
		const auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		return Offset != nullptr ? now + Offset->load() : now;
	}

	auto system_discovery_clock::elapsed_milliseconds() const -> std::int64_t
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}

	void composition_queue_refresh::retain(
		application::queue_session_id     aSession,
		core::pull_request_observation    aObservation,
		application::queue_refresh_status aStatus)
	{
		const auto lock = std::scoped_lock{ Mutex };
		Retained        = application::queue_refresh_completion{
			.Session     = std::move(aSession),
			.Ref         = aObservation.Ref,
			.Status      = aStatus,
			.Observation = std::move(aObservation),
		};
	}

	void composition_queue_refresh::clear()
	{
		const auto lock = std::scoped_lock{ Mutex };
		Retained.reset();
	}

	auto composition_queue_refresh::refresh(
		const application::queue_refresh_request& aRequest,
		std::stop_token                           aStop) -> application::queue_refresh_completion
	{
		if (aStop.stop_requested()) {
			return {
				.Session = aRequest.Session,
				.Ref     = aRequest.Ref,
				.Status  = application::queue_refresh_status::cancelled,
			};
		}
		const auto lock = std::scoped_lock{ Mutex };
		if (!Retained.has_value() || Retained->Session != aRequest.Session
			|| Retained->Ref != aRequest.Ref) {
			return {
				.Session = aRequest.Session,
				.Ref     = aRequest.Ref,
				.Status  = application::queue_refresh_status::cancelled,
			};
		}
		// This port replays an already accepted snapshot; it does not own a
		// provider ETag. A head OID is not a substitute because review and
		// comment activity can change while the head stays fixed.
		return *Retained;
	}
#if defined(BPR_BUILD_TESTING)
	auto fixture_notification_sink::permission()
		-> application::notification_permission
	{
		return application::notification_permission::granted;
	}

	auto fixture_notification_sink::request_permission()
		-> application::notification_permission
	{
		return application::notification_permission::granted;
	}

	auto fixture_notification_sink::deliver(
		const application::delivered_notification& aPayload)
		-> application::notification_submission_result
	{
		if (Payload.has_value()) {
			return application::notification_submission_result::
				suppressed_unattributable_activation;
		}
		Payload = aPayload;
		return application::notification_submission_result::submitted;
	}

	void fixture_notification_sink::set_activation_sink(
		application::notification_activation_sink aSink)
	{
		Activation = std::move(aSink);
	}

	auto fixture_notification_sink::activate() -> bool
	{
		if (!Payload.has_value() || ActivationConsumed || !Activation) {
			return false;
		}
		ActivationConsumed = true;
		Activation(Payload->Target);
		return true;
	}

	auto fixture_notification_sink::submitted_payload() const
		-> std::optional<application::delivered_notification>
	{
		return Payload;
	}
#endif
}
