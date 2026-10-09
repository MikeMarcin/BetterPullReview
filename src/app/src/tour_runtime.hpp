#ifndef BPR_APP_TOUR_RUNTIME_HPP
#define BPR_APP_TOUR_RUNTIME_HPP
#include "bpr/application/github_read_model.hpp"
#include "bpr/application/profile_runtime_lease.hpp"
#include "bpr/application/review_submission_application_host.hpp"
#include "bpr/application/review_tour_host.hpp"
#include "bpr/application/review_tour_source.hpp"
#include "tour_assist.hpp"
#include "tour_cli_types.hpp"
#include "tour_feedback.hpp"
#include "tour_publication_plan.hpp"
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
namespace bpr::app {
	struct tour_remote_read
	{
		application::tour_saved_target            Target;
		application::github_pull_request_snapshot Snapshot;
	};
	using tour_source_material = application::review_tour_source_material;

	/// \brief Exact observed provider composition, retained only during this command.
	struct tour_publication_context
	{
		application::review_submission_live_binding                                                      Binding;
		application::review_submission_provider_factory                                                  Factory;
		core::application_session_id                                                                     Session;
		bool                                                                                             Qualified = false;
		std::function<tour_cli_result<std::optional<application::staged_review_plan>>(std::string_view)> Attempt;
	};
	/// \brief Headless provider composition; publication has a separate exact-plan authority.
	class tour_remote_reader
	{
	public:
		virtual auto source(const core::tour_code_destination&) -> tour_cli_result<tour_source_material> { return std::unexpected(tour_cli_error{ "source_unavailable", "This read provider cannot load pinned source.", 5 }); }
		virtual auto publication() -> tour_cli_result<tour_publication_context> { return std::unexpected(tour_cli_error{ "publication_unavailable", "This provider has no qualified tour publication capability.", 5 }); }
		virtual ~tour_remote_reader()                                                                                                                                    = default;
		virtual auto connect(std::string aHostname, bool aGh, bool aInteractive) -> tour_cli_result<std::string>                                                         = 0;
		virtual auto open(std::string aHostname, core::repository_locator aRepository, core::pull_request_number aNumber, bool aGh) -> tour_cli_result<tour_remote_read> = 0;
	};
	using tour_reader_factory = std::function<std::unique_ptr<tour_remote_reader>(std::filesystem::path, core::bpr_profile_id)>;
	struct tour_local_snapshot
	{
		application::tour_saved_target                   Target;
		core::review_tour                                Draft;
		std::string                                      UndoLabel, RedoLabel;
		std::string                                      PublishedObservation = "not_refreshed";
		std::size_t                                      PendingServerReviews = 0;
		application::tour_resume_hint                    Resume{ };
		std::optional<application::tour_request_receipt> Receipt{ };
		/// Assist context, an assist or outline outcome, or hooks: one bounded JSON document, or empty.
		std::string AssistJson{ };
	};
	/// \brief [[BPR-1092]]. The draft whose pull request was read, and its label counts.
	struct tour_counted
	{
		tour_local_snapshot  Snapshot;
		tour_feedback_counts Counts;
	};
	/// \brief Read-only preview or journal recovery; the plan itself grants no writes.
	struct tour_publication_preview
	{
		tour_local_snapshot                            Snapshot;
		tour_publication_options                       Options;
		std::optional<application::staged_review_plan> Plan;
		application::staged_publication_result         Result;
		std::string                                    Status;
		std::size_t                                    PendingServerReviews = 0;
		bool                                           Resume               = false;
	};
	/// \brief One short profile-owned transaction. Destroy before waiting for human input.
	class tour_runtime final
	{
	public:
		tour_runtime(std::filesystem::path aBase, std::string aProfile, tour_reader_factory aReaders = { });
		~tour_runtime();
		[[nodiscard]] auto admission() const -> tour_cli_result<core::bpr_profile_id>;
		/// \brief The admitted exact root and lease, so an owner that holds this runtime can serve its tour channel.
		struct owner_binding
		{
			std::filesystem::path                               Root;
			core::bpr_profile_id                                Profile;
			std::shared_ptr<application::profile_runtime_lease> Lease;
		};
		[[nodiscard]] auto owner() const -> tour_cli_result<owner_binding>;
		/// rief Whether another process owns the profile, so every command goes through its tour channel.
		[[nodiscard]] auto forwarding() const -> bool;
		[[nodiscard]] auto connect(std::string aHostname, bool aGh, bool aInteractive) -> tour_cli_result<std::string>;
		[[nodiscard]] auto open(std::string aHostname, core::repository_locator aRepository, core::pull_request_number aNumber, bool aGh, std::string aRoot, bool aOffline) -> tour_cli_result<tour_local_snapshot>;
		[[nodiscard]] auto load(std::string_view aDraft, bool aMachine) -> tour_cli_result<tour_local_snapshot>;
		[[nodiscard]] auto source(std::string_view aDraft, const core::tour_code_destination& aDestination, bool aGh) -> tour_cli_result<tour_source_material>;
		[[nodiscard]] auto mutate(std::string_view aDraft, application::tour_service_request aRequest) -> tour_cli_result<tour_local_snapshot>;
		/// \brief Issues the draft's one live assist ticket. `aExtra` is source evidence this process read itself;
		/// a running owner cannot yet read a provider for it, so a forwarded request must carry none.
		[[nodiscard]] auto assist_issue(std::string_view aDraft, const tour_assist_query& aQuery, std::vector<application::writing_source> aExtra) -> tour_cli_result<tour_local_snapshot>;
		[[nodiscard]] auto assist_cancel(std::string_view aDraft) -> tour_cli_result<tour_local_snapshot>;
		/// \brief Effective hooks as one JSON document; a running owner answers from its own settings.
		[[nodiscard]] auto hooks_show() -> tour_cli_result<std::string>;
		/// \brief Replaces or clears one hook override. Only the profile owner may; a running desktop's settings are its own.
		[[nodiscard]] auto hooks_store(core::writing_hook aHook, std::string aInstructions) -> tour_cli_result<std::string>;
		/// \brief Reads the durable outcome of one exact earlier local request; never replays it.
		[[nodiscard]] auto request_status(std::string_view aDraft, std::string_view aRequest) -> tour_cli_result<tour_local_snapshot>;
		/// \brief [[BPR-1092]]. Reads the draft's exact pull request once through the existing reader, as
		/// `preview --refresh` does, and counts its conversations as the desktop's Overview does, with the
		/// profile's Settings chips and count mode unless \p aOptions names them. A running owner holds the
		/// provider, so this is refused there rather than forwarded.
		[[nodiscard]] auto feedback_counts(std::string_view aDraft, bool aGh, const tour_counts_options& aOptions) -> tour_cli_result<tour_counted>;
		[[nodiscard]] auto prepare_publication(std::string_view, bool aGh, tour_publication_options, std::string_view aAttempt = { }, bool aRecover = false) -> tour_cli_result<tour_publication_preview>;
		/// \brief Reobserves an exact preview after a live terminal confirmation; never persists a grant.
		[[nodiscard]] auto        publish(const tour_publication_preview&, bool aGh) -> tour_cli_result<tour_publication_preview>;
		[[nodiscard]] static auto draft_token(const application::tour_saved_target& aTarget) -> std::string;

	private:
		struct implementation;
		std::unique_ptr<implementation> Implementation;
	};
	[[nodiscard]] auto make_tour_remote_reader(std::filesystem::path, core::bpr_profile_id) -> std::unique_ptr<tour_remote_reader>;
}
#endif
