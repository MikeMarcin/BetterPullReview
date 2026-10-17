#include "github_provider.hpp"
#include "github_anonymous_read_admission.hpp"
#include "github_paged_read.hpp"
#include "github_publication_contract.hpp"
#include "github_viewer_named_blob_read.hpp"
#include "native_https_transport.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <limits>
#include <map>
#include <mutex>
#include <new>
#include <optional>
#include <ranges>
#include <stop_token>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::adapters::github::detail {
	namespace {
		[[nodiscard]] auto diagnostic_operation_for(const read_operation aOperation) noexcept
			-> application::diagnostic_paged_operation
		{
			switch (aOperation) {
			case read_operation::get_viewer_identity:
				return application::diagnostic_paged_operation::get_viewer_identity;
			case read_operation::get_repository:
				return application::diagnostic_paged_operation::get_repository;
			case read_operation::get_pull_request:
				return application::diagnostic_paged_operation::get_pull_request;
			case read_operation::list_pull_request_files:
				return application::diagnostic_paged_operation::list_pull_request_files;
			case read_operation::list_pull_request_commits:
				return application::diagnostic_paged_operation::list_pull_request_commits;
			case read_operation::list_reviews:
				return application::diagnostic_paged_operation::list_reviews;
			case read_operation::list_review_threads:
				return application::diagnostic_paged_operation::list_review_threads;
			case read_operation::list_issue_comments:
				return application::diagnostic_paged_operation::list_issue_comments;
			case read_operation::list_commit_statuses:
			case read_operation::list_checks:
				return application::diagnostic_paged_operation::list_checks;
			case read_operation::search_pull_requests:
				return application::diagnostic_paged_operation::search_pull_requests;
			case read_operation::list_review_comments:
				return application::diagnostic_paged_operation::list_review_comments;
			case read_operation::list_pull_request_timeline:
				return application::diagnostic_paged_operation::list_pull_request_timeline;
			case read_operation::get_merge_base:
				return application::diagnostic_paged_operation::get_merge_base;
			case read_operation::get_merge_assessment:
			case read_operation::resolve_body_images:
			case read_operation::list_review_requests:
			case read_operation::list_branch_rules:
			case read_operation::get_branch_protection:
			case read_operation::search_personal_pull_requests:
			case read_operation::list_requested_reviewers:
				// [[BPR-0988]]. Discovery is not part of a pull request read.
				return application::diagnostic_paged_operation::none;
			}
			return application::diagnostic_paged_operation::none;
		}

		[[nodiscard]] auto actor_conflict_summary(const application::github_actor_login_conflict& aConflict)
			-> application::diagnostic_summary
		{
			constexpr std::array<std::string_view, 8U> origins{ "viewer", "pull_request", "commit", "review", "thread_comment", "review_comment_root", "review_comment_reply", "issue_comment" };
			const auto                                 first       = static_cast<std::size_t>(aConflict.First);
			const auto                                 conflicting = static_cast<std::size_t>(aConflict.Conflicting);
			application::diagnostic_summary            result;
			if (first >= origins.size() || conflicting >= origins.size()) {
				return result;
			}
			auto       output = result.Text.begin();
			const auto append = [&](const std::string_view aText) { output = std::ranges::copy(aText, output).out; };
			append("actor_conflict=");
			append(origins[first]);
			append(",");
			append(origins[conflicting]);
			append(aConflict.AsciiCaseOnly ? ",ascii_case_only" : ",different");
			result.Size  = static_cast<std::uint16_t>(output - result.Text.begin());
			result.State = application::diagnostic_summary_state::sanitized;
			return result;
		}

		class pull_request_response_observer final : public github_paged_response_observer
		{
		public:
			pull_request_response_observer(application::diagnostic_log_port* aLog, core::pull_request_request_context aContext,
										   application::github_pull_request_progress_handler aProgress,
										   application::github_pull_request_overview_handler aOverview) noexcept
				: Log{ aLog }
				, Context{ std::move(aContext) }
				, Progress{ std::move(aProgress) }
				, Overview{ std::move(aOverview) }
				, DiagnosticGeneration{ aLog ? aLog->context_generation() : 0U }
			{ }
			void overview_observed(application::github_pull_request_overview_observation aObservation) noexcept override
			{
				try {
					if (Overview) {
						Overview(std::move(aObservation));
					}
				} catch (...) { }
			}
			void request_started(const read_operation aOperation) noexcept override
			{
				LastOperation = diagnostic_operation_for(aOperation);
				report_progress(aOperation);
				if (Log != nullptr) {
					static_cast<void>(Log->record({ .Operation         = application::diagnostic_operation::pull_request,
													.Outcome           = application::diagnostic_outcome::started,
													.Request           = Context.Request.value(),
													.WindowSession     = Context.ApplicationSession.value(),
													.PagedOperation    = *LastOperation,
													.Detail            = application::diagnostic_detail::verbose,
													.ContextGeneration = DiagnosticGeneration }));
				}
			}
			void validation_failed(const application::github_read_validation aValidation) noexcept override
			{
				ValidationError    = aValidation.Error;
				RelationshipDetail = aValidation.Detail;
				if (Log != nullptr && aValidation.ActorConflict) {
					constexpr std::array<application::diagnostic_paged_operation, 8U> operations{
						application::diagnostic_paged_operation::get_viewer_identity,
						application::diagnostic_paged_operation::get_pull_request,
						application::diagnostic_paged_operation::list_pull_request_commits,
						application::diagnostic_paged_operation::list_reviews,
						application::diagnostic_paged_operation::list_review_threads,
						application::diagnostic_paged_operation::list_review_comments,
						application::diagnostic_paged_operation::list_review_comments,
						application::diagnostic_paged_operation::list_issue_comments,
					};
					const auto origin = static_cast<std::size_t>(aValidation.ActorConflict->Conflicting);
					static_cast<void>(Log->record({
						.Operation          = application::diagnostic_operation::pull_request,
						.Outcome            = application::diagnostic_outcome::failed,
						.Request            = Context.Request.value(),
						.WindowSession      = Context.ApplicationSession.value(),
						.PagedOperation     = origin < operations.size() ? operations[origin] : application::diagnostic_paged_operation::none,
						.Summary            = actor_conflict_summary(*aValidation.ActorConflict),
						.GithubFailure      = aValidation.Error,
						.RelationshipDetail = aValidation.Detail,
						.ContextGeneration  = DiagnosticGeneration,
					}));
				}
			}
			void observe(const read_operation aOperation, const int aHttpStatus) noexcept override
			{
				LastOperation = diagnostic_operation_for(aOperation);
				if (aHttpStatus >= 200 && aHttpStatus < 300) {
					++PagesReceived;
					report_progress(aOperation);
				}
				if (Log != nullptr) {
					static_cast<void>(Log->record({
						.Operation      = application::diagnostic_operation::pull_request,
						.Outcome        = application::diagnostic_outcome::response_received,
						.Request        = Context.Request.value(),
						.WindowSession  = Context.ApplicationSession.value(),
						.HttpStatus     = aHttpStatus >= 0 && aHttpStatus <= 65535 ? static_cast<std::uint16_t>(aHttpStatus) : std::uint16_t{ 0 },
						.PagedOperation = *LastOperation,
					}));
				}
			}
			std::optional<application::diagnostic_paged_operation>   LastOperation;
			std::optional<application::github_read_validation_error> ValidationError;
			std::optional<application::github_read_validation_error> RelationshipDetail;

		private:
			void report_progress(const read_operation aOperation) noexcept
			{
				if (!Progress) {
					return;
				}
				using phase  = application::github_read_phase;
				auto current = phase::metadata;
				switch (aOperation) {
				case read_operation::list_pull_request_files:    current = phase::files; break;
				case read_operation::list_pull_request_commits:  current = phase::commits; break;
				case read_operation::list_reviews:               current = phase::reviews; break;
				case read_operation::list_review_threads:
				case read_operation::list_review_comments:       current = phase::review_threads; break;
				case read_operation::list_issue_comments:        current = phase::comments; break;
				case read_operation::list_commit_statuses:
				case read_operation::list_checks:                current = phase::checks; break;
				case read_operation::search_pull_requests:       current = phase::related_pull_requests; break;
				case read_operation::list_pull_request_timeline: current = phase::update_history; break;
				default:                                         break;
				}
				try {
					Progress(Context, { .Phase = current, .PagesReceived = PagesReceived, .Sequence = ++Sequence });
				} catch (...) {
					// Progress is observational and cannot alter the read terminal.
				}
			}
			application::diagnostic_log_port*                 Log;
			core::pull_request_request_context                Context;
			application::github_pull_request_progress_handler Progress;
			application::github_pull_request_overview_handler Overview;
			std::uint64_t                                     DiagnosticGeneration = 0;
			std::uint32_t                                     PagesReceived        = 0;
			std::uint32_t                                     Sequence             = 0;
		};

		enum class provider_phase : std::uint8_t {
			profile,
			viewer,
			capability,
			pull_request,
			/// [[BPR-0988]].
			discovery,
			/// RFC-BPR-0084.
			image_resolution,
		};

		using provider_parent_context = std::variant<core::github_session_request_context,
													 core::pull_request_request_context,
													 core::repository_request_context>;

		struct provider_job
		{
			provider_phase                       Phase = provider_phase::profile;
			provider_parent_context              Context;
			application::github_provider_attempt Attempt;
			std::stop_source                     Stop;
		};

		struct provider_state
		{
			application::effect_executor*                       Effects = nullptr;
			std::shared_ptr<application::diagnostic_log_port>   Log;
			std::shared_ptr<github_wire_transport_factory_port> TransportFactory;
			github_provider_read_authority                      ReadAuthority
				= github_provider_read_authority::authenticated;
			std::shared_ptr<github_profile_catalog>    Profiles;
			std::mutex                                 Mutex;
			std::vector<std::shared_ptr<provider_job>> Jobs;
			/// [[BPR-1373]]. The account each pinned CLI profile first signed in
			/// as in this run, by profile and login, under `Mutex`.
			std::map<std::pair<std::uint64_t, std::string>, core::github_account_id> CliPinnedAccounts;
			/// [[BPR-1560]]. Merge bases GitHub named for an exact recorded base
			/// and head, by host, profile and repository, under `Mutex`. An exact
			/// pair's merge base never changes, so a poll that finds the same pair
			/// asks nothing; only answers are kept, never failures. An open pull
			/// request's base branch moves, so its merge base is asked every read.
			/// [[BPR-1572]]: with the commit count GitHub's answer gave for the
			/// pair, which is as fixed as its merge base.
			struct remembered_merge_base
			{
				std::string                  MergeBaseOid;
				std::optional<std::uint64_t> AheadBy;
			};
			std::map<std::tuple<std::uint64_t, std::uint64_t, std::uint64_t, std::string, std::string>, remembered_merge_base> MergeBases;
			std::atomic<std::uint64_t>                                                                                         NextSnapshot{ 1U };
			bool                                                                                                               Closing = false;
			/// RFC-BPR-0084. Serial image lanes, each with its own media
			/// transport per profile, touched only on that lane.
			struct media_lane
			{
				application::effect_executor*                             Executor = nullptr;
				std::map<std::uint64_t, github_provider::media_performer> Transports;
				std::atomic<std::size_t>                                  Queued{ 0U };
			};
			std::vector<std::unique_ptr<media_lane>> MediaLanes;
			github_provider::media_transport_factory MediaFactory;
		};

		enum class make_job_failure : std::uint8_t {
			closing,
			exhausted,
		};

		using make_job_result = std::expected<std::shared_ptr<provider_job>, make_job_failure>;

		template <typename Handler>
		class provider_completion_delivery final
		{
		public:
			explicit provider_completion_delivery(Handler aHandler) noexcept
				: HandlerValue{ std::move(aHandler) }
			{
			}

			template <typename Completion>
			void deliver(Completion aCompletion) noexcept
			{
				if (Delivered.exchange(true, std::memory_order_acq_rel)) {
					return;
				}
				try {
					HandlerValue(std::move(aCompletion));
				} catch (...) {
				}
			}

		private:
			Handler          HandlerValue;
			std::atomic_bool Delivered = false;
		};

		template <typename Handler>
		[[nodiscard]] auto make_delivery(Handler&& aHandler) -> std::shared_ptr<
			provider_completion_delivery<std::remove_cvref_t<Handler>>>
		{
			return std::make_shared<
				provider_completion_delivery<std::remove_cvref_t<Handler>>>(
				std::forward<Handler>(aHandler));
		}

		class github_review_mutation_transport final
			: public github_review_mutation_transport_lease
		{
		public:
			github_review_mutation_transport(
				application::github_profile_authority            aProfile,
				application::github_credential_operation_context aCredentialContext,
				std::shared_ptr<github_wire_transport_port>      aTransport) noexcept
				: Profile{ std::move(aProfile) }
				, CredentialContext{ std::move(aCredentialContext) }
				, Transport{ std::move(aTransport) }
			{
			}

			[[nodiscard]] auto profile() const noexcept
				-> const application::github_profile_authority& override
			{
				return Profile;
			}

			[[nodiscard]] auto credential_context() const noexcept
				-> const application::github_credential_operation_context& override
			{
				return CredentialContext;
			}

			[[nodiscard]] auto mutation_transport() noexcept
				-> github_mutation_wire_transport* override
			{
				return active()
					? dynamic_cast<github_mutation_wire_transport*>(Transport.get())
					: nullptr;
			}

			[[nodiscard]] auto strict_cli() const noexcept -> bool override
			{
				return active() && Transport->strict_cli();
			}

		private:
			application::github_profile_authority            Profile;
			application::github_credential_operation_context CredentialContext;
			std::shared_ptr<github_wire_transport_port>      Transport;
		};

		/// [[BPR-1177]]. The publication catalog over one account's wire,
		/// keeping that wire alive for as long as the catalog is held.
		struct github_owned_publication_session
		{
			github_owned_publication_session(std::shared_ptr<github_wire_transport_port> aOwner, github_publication_wire_transport& aWire,
											 const application::github_credential_operation_context& aContext) noexcept
				: Owner{ std::move(aOwner) }
				, Session{ aWire, aContext }
			{
			}

			std::shared_ptr<github_wire_transport_port> Owner;
			github_publication_session                  Session;
		};

		class github_wire_account_transport final
			: public application::github_account_transport_lease,
			  public application::git_smart_transport_factory_port,
			  public application::review_mutation_transport_factory_port,
			  public application::github_publication_session_factory_port
		{
		public:
			github_wire_account_transport(
				application::github_profile_authority            aProfile,
				application::github_credential_operation_context aCredentialContext,
				std::shared_ptr<github_wire_transport_port>      aTransport) noexcept
				: Profile{ aProfile }
				, CredentialContext{ aCredentialContext }
				, Transport{ std::move(aTransport) }
			{
			}

			[[nodiscard]] auto supports_sparse_commit_trees() const noexcept -> bool override
			{
				const auto* factory = dynamic_cast<const application::git_smart_transport_factory_port*>(Transport.get());
				return factory != nullptr && factory->supports_sparse_commit_trees();
			}

			[[nodiscard]] auto acquire(
				const application::git_smart_request_context& aContext)
				-> application::git_smart_transport_result<
					application::git_smart_transport> override
			{
				if (auto* const factory = dynamic_cast<application::git_smart_transport_factory_port*>(
						Transport.get())) {
					return factory->acquire(aContext);
				}
				const auto native = std::dynamic_pointer_cast<native_https_transport>(Transport);
				if (native == nullptr) {
					return std::unexpected{
						application::git_smart_transport_error{
							.Failure = application::git_smart_transport_failure::
								unavailable }
					};
				}
				return make_native_git_smart_transport(native, aContext, CredentialContext.ApplicationSession);
			}

			[[nodiscard]] auto acquire_review_mutation_transport() noexcept
				-> application::review_mutation_transport override
			{
				if (dynamic_cast<github_mutation_wire_transport*>(Transport.get())
					== nullptr) {
					return { };
				}
				try {
					return std::make_shared<github_review_mutation_transport>(
						Profile, CredentialContext, Transport);
				} catch (...) {
					return { };
				}
			}

			/// The catalog over this account's own authority, when its wire is
			/// the native HTTPS transport; the session refuses any other host,
			/// account or server contract before a request exists.
			[[nodiscard]] auto acquire_publication_session() noexcept -> std::shared_ptr<application::github_publication_port> override
			{
				auto* const wire = dynamic_cast<github_publication_wire_transport*>(Transport.get());
				if (wire == nullptr) {
					return { };
				}
				try {
					auto owned = std::make_shared<github_owned_publication_session>(Transport, *wire, CredentialContext);
					return std::shared_ptr<application::github_publication_port>{ owned, &owned->Session };
				} catch (...) {
					return { };
				}
			}

			application::github_profile_authority            Profile;
			application::github_credential_operation_context CredentialContext;
			std::shared_ptr<github_wire_transport_port>      Transport;
		};

		[[nodiscard]] auto
		provider_error(const application::github_provider_failure aFailure) noexcept
			-> application::github_provider_error
		{
			return {
				.Failure           = aFailure,
				.RetryAfterSeconds = std::nullopt,
				.RateLimits        = { },
				.RateLimitCause    = std::nullopt,
			};
		}

		[[nodiscard]] constexpr auto rate_limits_are_empty_or_exact(
			const application::github_rate_limit_state&   aState,
			const application::github_rate_limit_resource aExpected) noexcept -> bool
		{
			if (!application::valid(aState) || !application::has_observations(aState)) {
				return application::valid(aState);
			}
			const auto count = static_cast<std::size_t>(aState.Core.has_value())
				+ static_cast<std::size_t>(aState.Search.has_value())
				+ static_cast<std::size_t>(aState.Graphql.has_value());
			return count == 1U && application::find(aState, aExpected) != nullptr;
		}

		[[nodiscard]] auto
		configuration_error(const native_https_configuration_failure aFailure) noexcept
			-> application::github_provider_error
		{
			switch (aFailure) {
			case native_https_configuration_failure::invalid_credential_material:
			case native_https_configuration_failure::invalid_account_credential:
				return provider_error(
					application::github_provider_failure::authentication);
			case native_https_configuration_failure::allocation_failure:
				return provider_error(
					application::github_provider_failure::response_limit);
			case native_https_configuration_failure::profile_transport_mismatch:
			case native_https_configuration_failure::profile_policy_mismatch:
			case native_https_configuration_failure::invalid_bootstrap_context:
			case native_https_configuration_failure::credential_authority_mismatch:
			case native_https_configuration_failure::invalid_test_configuration:
				return provider_error(
					application::github_provider_failure::unavailable);
			case native_https_configuration_failure::
				anonymous_read_contract_unsupported:
				return provider_error(provider_failure_for(
					anonymous_read_refusal::server_contract_unsupported));
			}
			return provider_error(application::github_provider_failure::unavailable);
		}

		[[nodiscard]] auto
		profile_authority(const validated_github_host_profile& aProfile) noexcept
			-> application::github_profile_authority
		{
			return {
				.Profile               = aProfile.profile(),
				.ProfileRevision       = aProfile.revision(),
				.Host                  = aProfile.host(),
				.Hostname              = application::github_canonical_hostname{ aProfile.hostname() },
				.ServerContract        = aProfile.server_contract(),
				.Authentication        = aProfile.authentication_transport(),
				.ProbeContractRevision = application::github_probe_contract_revision{ 1U },
			};
		}

		[[nodiscard]] auto
		exact_profile(const application::github_profile_authority& aAuthority,
					  const validated_github_host_profile&         aProfile) noexcept -> bool
		{
			return application::valid(aAuthority)
				&& aAuthority == profile_authority(aProfile);
		}

		[[nodiscard]] auto
		find_profile(const provider_state&       aState,
					 const core::host_profile_id aIdentity) noexcept
			-> std::shared_ptr<const validated_github_host_profile>
		{
			return aState.Profiles->find_profile(aIdentity);
		}

		[[nodiscard]] auto
		find_exact_profile(const provider_state&                        aState,
						   const application::github_profile_authority& aAuthority) noexcept -> std::shared_ptr<const validated_github_host_profile>
		{
			auto profile = find_profile(aState, aAuthority.Profile);
			if (profile == nullptr || !exact_profile(aAuthority, *profile)) {
				return { };
			}
			return profile;
		}

		[[nodiscard]] auto structurally_valid(
			const application::github_capability_cache_entry& aEvidence) noexcept -> bool
		{
			const application::github_capability_probe_observation observation{
				.Authority   = aEvidence.Authority,
				.ObservedAt  = aEvidence.ObservedAt,
				.Facts       = aEvidence.Facts,
				.SchemaFacts = aEvidence.SchemaFacts,
			};
			return application::valid(observation) && aEvidence.Revision.value() != 0U
				&& aEvidence.ExpiresBefore > aEvidence.ObservedAt
				&& (aEvidence.ExpiresBefore.value() - aEvidence.ObservedAt.value()
					<= application::maximum_github_capability_freshness_window)
				&& aEvidence.Capabilities
				== application::projected_capabilities(observation)
				&& aEvidence.ReadShape
				== application::projected_read_shape(observation);
		}

		[[nodiscard]] auto
		valid_request(const application::github_profile_request& aRequest) noexcept
			-> bool
		{
			return application::detail::valid_context(aRequest.Context)
				&& aRequest.Attempt.valid();
		}

		[[nodiscard]] auto
		valid_request(const application::github_viewer_request& aRequest) noexcept
			-> bool
		{
			return application::detail::valid_context(aRequest.Context)
				&& application::valid(aRequest.Authority)
				&& aRequest.Context.Profile == aRequest.Authority.Profile
				&& aRequest.Attempt.valid();
		}

		[[nodiscard]] auto
		valid_request(const application::github_capability_request& aRequest) noexcept
			-> bool
		{
			return application::detail::valid_context(aRequest.Context)
				&& application::valid(aRequest.ProfileAuthority)
				&& aRequest.Context.Profile == aRequest.ProfileAuthority.Profile
				&& aRequest.Viewer.Account.value() != 0U
				&& aRequest.AccountTransport != nullptr && aRequest.Attempt.valid();
		}

		/// \brief Checks everything about a read request but its transport.
		///
		/// \details Split out because an anonymous read carries no account
		/// transport lease to check: there is no account, so there is no lease.
		/// Every correlation, authority, and capability-evidence rule below
		/// still applies to both authorities, and the authenticated entry keeps
		/// the lease requirement it has always had.
		[[nodiscard]] auto valid_read_correlation(
			const application::github_pull_request_provider_request& aRequest) noexcept
			-> bool
		{
			return (!aRequest.AuthenticationSession || aRequest.AuthenticationSession->value() != 0U)
				&& application::detail::valid_context(aRequest.Context)
				&& application::valid(aRequest.ProfileAuthority)
				&& aRequest.Context.Profile == aRequest.ProfileAuthority.Profile
				&& aRequest.Context.Host == aRequest.ProfileAuthority.Host
				&& aRequest.Viewer.Account == aRequest.Context.Account
				&& aRequest.CapabilityEvidence.Authority.Profile
				== aRequest.Context.Profile
				&& aRequest.CapabilityEvidence.Authority.Host == aRequest.Context.Host
				&& aRequest.CapabilityEvidence.Authority.Account
				== aRequest.Context.Account
				&& aRequest.CapabilityEvidence.Authority.ProfileRevision
				== aRequest.ProfileAuthority.ProfileRevision
				&& aRequest.CapabilityEvidence.Authority.ServerContract
				== aRequest.ProfileAuthority.ServerContract
				&& aRequest.CapabilityEvidence.Authority.ProbeContractRevision
				== aRequest.ProfileAuthority.ProbeContractRevision
				&& aRequest.CapabilityEvidence.Revision
				== aRequest.Context.Capabilities
				&& structurally_valid(aRequest.CapabilityEvidence)
				&& aRequest.Attempt.valid();
		}

		/// \brief Reports whether a signed-out read names itself correctly.
		///
		/// \details The same correlation the authenticated read must satisfy,
		/// with one term replaced rather than dropped: a signed-out session
		/// never probed, so `structurally_valid` -- which asks whether the
		/// evidence is what an observation would have projected -- has nothing
		/// to check, and the synthesised shape is required in its place. The
		/// account-shaped terms in `valid_read_correlation` still hold here,
		/// because a signed-out read names the same absent account in all
		/// three places rather than naming none.
		[[nodiscard]] auto valid_anonymous_read_correlation(
			const application::github_pull_request_provider_request& aRequest) noexcept
			-> bool
		{
			return !aRequest.AuthenticationSession
				&& application::detail::valid_context(aRequest.Context)
				&& application::valid(aRequest.ProfileAuthority)
				&& aRequest.Context.Profile == aRequest.ProfileAuthority.Profile
				&& aRequest.Context.Host == aRequest.ProfileAuthority.Host
				&& aRequest.Viewer.Account == aRequest.Context.Account
				&& aRequest.CapabilityEvidence.Authority.Profile
				== aRequest.Context.Profile
				&& aRequest.CapabilityEvidence.Authority.Host == aRequest.Context.Host
				&& aRequest.CapabilityEvidence.Authority.Account
				== aRequest.Context.Account
				&& aRequest.CapabilityEvidence.Authority.ProfileRevision
				== aRequest.ProfileAuthority.ProfileRevision
				&& aRequest.CapabilityEvidence.Authority.ServerContract
				== aRequest.ProfileAuthority.ServerContract
				&& aRequest.CapabilityEvidence.Authority.ProbeContractRevision
				== aRequest.ProfileAuthority.ProbeContractRevision
				&& aRequest.CapabilityEvidence.Revision
				== aRequest.Context.Capabilities
				&& (signed_out_read_identity(aRequest)
					|| structurally_valid(aRequest.CapabilityEvidence))
				&& aRequest.Attempt.valid();
		}

		[[nodiscard]] auto valid_request(
			const application::github_pull_request_provider_request& aRequest) noexcept
			-> bool
		{
			return valid_read_correlation(aRequest)
				&& aRequest.AccountTransport != nullptr;
		}

		/// \brief Reports the refusal an anonymous authority owes an operation.
		///
		/// \details Callers ask only about operations an anonymous authority
		/// never admits, so the admitted branch is unreachable rather than
		/// merely unlikely; it stays because a silent fall-through would be
		/// worse than a conservative one.
		[[nodiscard]] auto anonymous_read_error(
			const read_operation                      aOperation,
			const application::github_server_contract aContract) noexcept
			-> application::github_provider_error
		{
			const auto admitted = admit_anonymous_read(aOperation, aContract);
			return provider_error(
				admitted.has_value()
					? application::github_provider_failure::unavailable
					: provider_failure_for(admitted.error()));
		}

		/// \brief Derives the session correlation an anonymous read runs under.
		///
		/// \details An anonymous transport binds to a session and identity
		/// generation, never to an account, so this carries across exactly the
		/// fields the binding compares and drops the one it must not.
		[[nodiscard]] auto anonymous_session_context(
			const core::pull_request_request_context& aContext) noexcept
			-> core::github_session_request_context
		{
			return {
				.Request            = aContext.Request,
				.ApplicationSession = aContext.ApplicationSession,
				.Profile            = aContext.Profile,
				.IdentityGeneration = aContext.IdentityGeneration,
			};
		}

		[[nodiscard]] auto make_job(const std::shared_ptr<provider_state>&     aState,
									const provider_phase                       aPhase,
									provider_parent_context                    aContext,
									const application::github_provider_attempt aAttempt)
			-> make_job_result
		{
			try {
				auto                   job = std::make_shared<provider_job>(provider_job{
					.Phase   = aPhase,
					.Context = std::move(aContext),
					.Attempt = aAttempt,
					.Stop    = { },
				});
				const std::scoped_lock lock{ aState->Mutex };
				if (aState->Closing) {
					return std::unexpected{ make_job_failure::closing };
				}
				aState->Jobs.push_back(job);
				return job;
			} catch (...) {
				return std::unexpected{ make_job_failure::exhausted };
			}
		}

		auto finish_job(const std::shared_ptr<provider_state>& aState,
						const std::shared_ptr<provider_job>&   aJob) noexcept -> bool
		{
			const std::scoped_lock lock{ aState->Mutex };
			const auto             cancelled = aJob->Stop.stop_requested();
			const auto             match     = std::ranges::find(aState->Jobs, aJob);
			if (match != aState->Jobs.end()) {
				aState->Jobs.erase(match);
			}
			return cancelled;
		}

		[[nodiscard]] auto
		allocate_snapshot(const std::shared_ptr<provider_state>& aState) noexcept
			-> std::optional<core::provider_snapshot_id>
		{
			auto current = aState->NextSnapshot.load(std::memory_order_relaxed);
			while (current != 0U && current != std::numeric_limits<std::uint64_t>::max()) {
				if (aState->NextSnapshot.compare_exchange_weak(current,
															   current + 1U,
															   std::memory_order_relaxed,
															   std::memory_order_relaxed)) {
					return core::provider_snapshot_id{ current };
				}
			}
			return std::nullopt;
		}

		[[nodiscard]] auto
		stop_after_failed_post(const std::shared_ptr<provider_state>& aState,
							   const std::shared_ptr<provider_job>&   aJob) noexcept
			-> bool
		{
			const auto wasCancelled = aJob->Stop.stop_requested();
			aJob->Stop.request_stop();
			static_cast<void>(finish_job(aState, aJob));
			return wasCancelled;
		}

		template <typename Request>
		void cancel_jobs(const std::shared_ptr<provider_state>& aState,
						 const provider_phase                   aPhase,
						 const Request&                         aRequest) noexcept
		{
			const provider_parent_context context{ aRequest.Context };
			const std::scoped_lock        lock{ aState->Mutex };
			for (const auto& job : aState->Jobs) {
				if (job->Phase == aPhase && job->Context == context
					&& job->Attempt == aRequest.Attempt) {
					job->Stop.request_stop();
				}
			}
		}

		[[nodiscard]] auto
		expected_server(const validated_github_host_profile& aProfile) noexcept
			-> std::optional<server_contract>
		{
			switch (aProfile.server_contract()) {
			case application::github_server_contract::github_com_2022_11_28:
				return server_contract::github_com;
			case application::github_server_contract::ghes_3_18_2022_11_28:
				return server_contract::ghes_3_18;
			}
			return std::nullopt;
		}

		[[nodiscard]] auto
		viewer_wire_request(const validated_github_host_profile&      aProfile,
							const application::github_viewer_request& aRequest)
			-> std::expected<github_wire_request, application::github_provider_error>
		{
			const auto server = expected_server(aProfile);
			if (!server.has_value()) {
				return std::unexpected{
					provider_error(application::github_provider_failure::api_version)
				};
			}
			semantic_read_request semantic{
				.Server            = *server,
				.Operation         = read_operation::get_viewer_identity,
				.Owner             = { },
				.Repository        = { },
				.PullRequestNumber = 0U,
				.HeadOid           = { },
				.SearchTerms       = { },
				.Page              = 1U,
				.GraphqlPage       = std::nullopt,
			};
			const auto wire = compile_read_request(semantic);
			if (!wire.has_value()) {
				return std::unexpected{ provider_error(
					application::github_provider_failure::malformed_response) };
			}
			return github_wire_request{
				.Correlation = github_wire_correlation{
					.Parent          = aRequest.Context,
					.ProfileRevision = aRequest.Authority.ProfileRevision,
					.Host            = aRequest.Authority.Host,
					.Operation       = read_operation::get_viewer_identity,
					.Page            = 1U,
				},
				.Wire = *wire,
			};
		}

		// Observations detect account changes; they cannot contain a concurrent
		// gh auth switch. A mismatch permanently refuses this observed lease.
		class cli_observed_account_transport final : public github_wire_transport_port,
													 public github_mutation_wire_transport,
													 public application::git_smart_transport_factory_port,
													 public std::enable_shared_from_this<cli_observed_account_transport>
		{
			class observed_git_transport final : public application::git_smart_transport_port
			{
			public:
				observed_git_transport(std::shared_ptr<cli_observed_account_transport> aOwner,
									   application::git_smart_transport                aInner)
					: Owner{ std::move(aOwner) }
					, Inner{ std::move(aInner) }
				{ }
				void revoke() noexcept override { Inner->revoke(); }
				auto revoked() const noexcept -> bool override { return Inner->revoked() || Owner->Mismatch.load(); }
				// [[BPR-1370]]. A text blob is read first through GraphQL,
				// whose answer names the account that read it. A blob that
				// way cannot carry (binary, truncated, larger than a batch
				// blob, or not verified) takes the REST read between a pair
				// of account observations, as before.
				auto read_exact_blob(const application::git_smart_exact_blob_request& aRequest, std::stop_token aStop)
					-> application::git_smart_transport_result<std::vector<std::byte>> override
				{
					using failure = application::git_smart_transport_failure;
					if (Owner->strict_cli()) {
						if (aStop.stop_requested() || revoked()) {
							return std::unexpected{ application::git_smart_transport_error{ application::git_smart_transport_failure::cancelled } };
						}
						auto result = Inner->read_exact_blob(aRequest, aStop);
						if (!result && result.error().Failure == failure::authentication && Inner->revoked()) {
							Owner->Mismatch.store(true);
						}
						return result;
					}
					if (auto* reader = dynamic_cast<viewer_named_blob_reader*>(Inner.get())) {
						const application::git_smart_exact_blobs_request text{
							.Context       = aRequest.Context,
							.ExpectedBlobs = { aRequest.ExpectedBlob },
							.MaximumBytes  = std::min(aRequest.MaximumBytes, application::maximum_git_exact_blob_batch_bytes),
						};
						if (application::valid(text)) {
							auto answer = read_naming_viewer(*reader, text, aStop);
							if (answer && answer->size() == 1U && answer->front()) {
								return std::move(*answer->front());
							}
							if (!answer && (answer.error().Failure == failure::cancelled || answer.error().Failure == failure::authentication)) {
								return std::unexpected{ answer.error() };
							}
						}
					}
					return perform_guarded<std::vector<std::byte>>(aStop, [&] { return Inner->read_exact_blob(aRequest, aStop); });
				}
				// [[BPR-1327]]. One GraphQL read for the whole batch. Its
				// answer names the account that read it ([[BPR-1370]]); a
				// transport that cannot say takes one pair of account
				// observations around the batch instead.
				auto read_exact_blobs(const application::git_smart_exact_blobs_request& aRequest, std::stop_token aStop)
					-> application::git_smart_transport_result<application::git_smart_exact_blobs> override
				{
					if (auto* reader = dynamic_cast<viewer_named_blob_reader*>(Inner.get())) {
						return read_naming_viewer(*reader, aRequest, aStop);
					}
					return perform_guarded<application::git_smart_exact_blobs>(aStop, [&] { return Inner->read_exact_blobs(aRequest, aStop); });
				}
				auto discover(const application::git_smart_discovery_request& aRequest, std::stop_token aStop)
					-> application::git_smart_transport_result<application::git_smart_discovery_response> override
				{
					return perform_guarded<application::git_smart_discovery_response>(aStop, [&] { return Inner->discover(aRequest, aStop); });
				}
				auto ls_refs(const application::git_smart_ls_refs_request& aRequest, std::stop_token aStop)
					-> application::git_smart_transport_result<application::git_smart_ls_refs_response> override
				{
					return perform_guarded<application::git_smart_ls_refs_response>(aStop, [&] { return Inner->ls_refs(aRequest, aStop); });
				}
				auto fetch(const application::git_smart_fetch_request& aRequest, application::git_smart_wire_consumer& aConsumer, std::stop_token aStop)
					-> application::git_smart_transport_result<void> override
				{
					return perform_guarded<void>(aStop, [&] { return Inner->fetch(aRequest, aConsumer, aStop); });
				}

			private:
				/// The answer's account is held to the lease's bound account
				/// and login exactly as an observation is: a mismatch
				/// refuses this lease for good.
				auto read_naming_viewer(viewer_named_blob_reader& aReader, const application::git_smart_exact_blobs_request& aRequest, std::stop_token aStop)
					-> application::git_smart_transport_result<application::git_smart_exact_blobs>
				{
					using failure = application::git_smart_transport_failure;
					if (aStop.stop_requested() || revoked()) {
						return std::unexpected{ application::git_smart_transport_error{ failure::cancelled } };
					}
					auto answer = aReader.read_exact_blobs_naming_viewer(aRequest, aStop);
					if (!answer) {
						return std::unexpected{ answer.error() };
					}
					if (!Owner->admit(answer->Viewer)) {
						revoke();
						return std::unexpected{ application::git_smart_transport_error{ failure::authentication } };
					}
					if (aStop.stop_requested() || revoked()) {
						return std::unexpected{ application::git_smart_transport_error{ failure::cancelled } };
					}
					return std::move(answer->Blobs);
				}
				template <typename TValue, typename TCall>
				auto perform_guarded(std::stop_token aStop, TCall aCall) -> application::git_smart_transport_result<TValue>
				{
					if (auto error = check(aStop)) {
						return std::unexpected{ *error };
					}
					auto result = aCall();
					if (auto error = check(aStop)) {
						return std::unexpected{ *error };
					}
					return result;
				}
				auto check(std::stop_token aStop) -> std::optional<application::git_smart_transport_error>
				{
					using failure = application::git_smart_transport_failure;
					if (aStop.stop_requested() || revoked()) {
						return application::git_smart_transport_error{ failure::cancelled };
					}
					if (!Owner->strict_cli()) {
						if (const auto error = Owner->check(Owner->Viewer, aStop)) {
							revoke();
							return application::git_smart_transport_error{ aStop.stop_requested()                                                       ? failure::cancelled
																			   : error->Failure == application::github_provider_failure::authentication ? failure::authentication
																																						: failure::unavailable };
						}
					}
					return std::nullopt;
				}
				std::shared_ptr<cli_observed_account_transport> Owner;
				application::git_smart_transport                Inner;
			};

		public:
			cli_observed_account_transport(std::shared_ptr<github_wire_transport_port> aTransport,
										   github_wire_request aViewer, server_contract aServer, core::github_account_id aAccount, std::string aLogin)
				: Transport{ std::move(aTransport) }
				, Viewer{ std::move(aViewer) }
				, Server{ aServer }
				, Account{ aAccount }
				, Login{ std::move(aLogin) }
			{ }

			[[nodiscard]] auto strict_cli() const noexcept -> bool override { return Transport->strict_cli(); }
			[[nodiscard]] auto begin_operation(std::stop_token aStop) -> github_wire_operation override
			{
				auto inner = Transport->begin_operation(aStop);
				if (!inner || !*inner) {
					return inner;
				}
				auto scope = std::make_shared<cli_observed_account_transport>(*inner, Viewer, Server, Account, Login);
				if (auto error = scope->check(Viewer, aStop)) {
					return std::unexpected{ *error };
				}
				return std::shared_ptr<github_wire_transport_port>{ std::move(scope) };
			}
			[[nodiscard]] auto begin_mutation_operation(std::stop_token aStop) -> github_mutation_operation override
			{
				auto* mutation = dynamic_cast<github_mutation_wire_transport*>(Transport.get());
				if (!mutation || !Transport->strict_cli()) {
					return std::unexpected(application::review_mutation_failure::unsupported);
				}
				auto acquired = mutation->begin_mutation_operation(aStop);
				if (!acquired) {
					return std::unexpected(acquired.error());
				}
				auto wire = std::dynamic_pointer_cast<github_wire_transport_port>(*acquired);
				if (!wire) {
					return std::unexpected(application::review_mutation_failure::unsupported);
				}
				auto scope = std::make_shared<cli_observed_account_transport>(std::move(wire), Viewer, Server, Account, Login);
				if (auto error = scope->check(Viewer, aStop)) {
					return std::unexpected(aStop.stop_requested()                                                       ? application::review_mutation_failure::cancelled
											   : error->Failure == application::github_provider_failure::authentication ? application::review_mutation_failure::undesignated_target
																														: application::review_mutation_failure::unavailable);
				}
				return std::shared_ptr<github_mutation_wire_transport>{ std::move(scope) };
			}

			[[nodiscard]] auto supports_sparse_commit_trees() const noexcept -> bool override
			{
				const auto* factory = dynamic_cast<const application::git_smart_transport_factory_port*>(Transport.get());
				return !Mismatch.load() && factory != nullptr && factory->supports_sparse_commit_trees();
			}

			auto acquire(const application::git_smart_request_context& aContext)
				-> application::git_smart_transport_result<application::git_smart_transport> override
			{
				auto* factory = dynamic_cast<application::git_smart_transport_factory_port*>(Transport.get());
				if (!factory || Mismatch.load() || aContext.Authority.Account != Account) {
					return std::unexpected{ application::git_smart_transport_error{ application::git_smart_transport_failure::unavailable } };
				}
				auto inner = factory->acquire(aContext);
				if (!inner) {
					return std::unexpected{ inner.error() };
				}
				return std::make_shared<observed_git_transport>(shared_from_this(), *inner);
			}

			auto perform(const github_wire_request& aRequest, std::stop_token aStop) -> github_wire_completion override
			{
				const auto& viewer = Viewer;
				if (auto error = check(viewer, aStop)) {
					if (aStop.stop_requested()) {
						return github_wire_cancelled{ .Correlation = aRequest.Correlation };
					}
					return github_wire_error{ .Correlation = aRequest.Correlation, .Error = *error };
				}
				if (aStop.stop_requested()) {
					return github_wire_cancelled{ .Correlation = aRequest.Correlation };
				}
				auto result = Transport->perform(aRequest, aStop);
				if (std::holds_alternative<github_wire_success>(result)) {
					if (auto error = check(viewer, aStop)) {
						return github_wire_error{ .Correlation = aRequest.Correlation, .Error = *error };
					}
				}
				return result;
			}

			auto perform_review_mutation(const github_mutation_wire_request& aRequest, std::stop_token aStop,
										 github_mutation_transmission_witness aWitness) -> github_mutation_wire_completion override
			{
				auto*       inner     = dynamic_cast<github_mutation_wire_transport*>(Transport.get());
				const auto& session   = std::get<core::github_session_request_context>(Viewer.Correlation.Parent);
				const auto& authority = aRequest.Context.Authority;
				if (!inner || !application::valid(aRequest.Context) || authority.Account != Account
					|| authority.Profile != session.Profile || authority.Host != Viewer.Correlation.Host
					|| authority.ProfileRevision != Viewer.Correlation.ProfileRevision
					|| aRequest.Context.ApplicationSession != session.ApplicationSession
					|| aRequest.Context.IdentityGeneration != session.IdentityGeneration) {
					return github_mutation_wire_error{ };
				}
				if (check(Viewer, aStop)) {
					return aStop.stop_requested() ? github_mutation_wire_completion{ github_mutation_wire_cancelled{ } }
												  : github_mutation_wire_completion{ github_mutation_wire_error{ } };
				}
				// [[BPR-1572]]. The account check is a read, after the session's
				// last observation of the pull request. A write bound to a pull
				// request reads it once more here, after the check, so the pull
				// request is still the last read before the write: a head or a
				// recorded base GitHub moved during the account reads sends
				// nothing. This compares the head and recorded base only; it does
				// not ask the merge base again, so a base branch rewritten after
				// the session's compares, before GitHub replaces the recorded
				// base, is not seen here (the inherent limit the architecture
				// documents).
				if (const auto target = written_target(aRequest.Wire)) {
					if (auto refused = confirm_written_target(*inner, aRequest, *target, aStop)) {
						return *refused;
					}
				}
				bool attempted = false;
				auto result    = inner->perform_review_mutation(aRequest, aStop, [&] {
					if (!aWitness || !aWitness()) {
						return false;
					}
					attempted = true;
					return true;
				});
				if (check(Viewer, aStop)) {
					const auto sent = attempted ? github_mutation_transmission::indeterminate : github_mutation_transmission::definitely_not_sent;
					return aStop.stop_requested() ? github_mutation_wire_completion{ github_mutation_wire_cancelled{ .Transmission = sent } }
												  : github_mutation_wire_completion{ github_mutation_wire_error{ .Transmission = sent } };
				}
				return result;
			}

			auto perform_capability_probe(const github_capability_probe_wire_request& aRequest, std::stop_token aStop) -> github_capability_probe_wire_completion override
			{
				const auto& viewer = Viewer;
				if (auto error = check(viewer, aStop)) {
					if (aStop.stop_requested()) {
						return github_capability_probe_wire_cancelled{ .Correlation = aRequest.Correlation };
					}
					return github_capability_probe_wire_error{ .Correlation = aRequest.Correlation, .Error = *error };
				}
				if (aStop.stop_requested()) {
					return github_capability_probe_wire_cancelled{ .Correlation = aRequest.Correlation };
				}
				auto result = Transport->perform_capability_probe(aRequest, aStop);
				if (std::holds_alternative<github_capability_probe_wire_success>(result)) {
					if (auto error = check(viewer, aStop)) {
						return github_capability_probe_wire_error{ .Correlation = aRequest.Correlation, .Error = *error };
					}
				}
				return result;
			}

		private:
			/// [[BPR-1572]]. The pull request `aTarget` names, read through the
			/// inner transport and held to the target: no refusal when its
			/// head and recorded base are the target's, else the refusal the
			/// write is answered with, not sent.
			static auto confirm_written_target(github_mutation_wire_transport& aInner, const github_mutation_wire_request& aWrite,
											   const application::review_mutation_target_binding& aTarget, std::stop_token aStop)
				-> std::optional<github_mutation_wire_completion>
			{
				using transmission = github_mutation_transmission;
				const auto refuse  = [](const application::review_mutation_failure aFailure) {
					return std::optional<github_mutation_wire_completion>{ github_mutation_wire_error{ .Failure = aFailure, .Transmission = transmission::definitely_not_sent } };
				};
				const auto request = compile_target_observation_request(aTarget, aTarget.ServerContract);
				if (!request) {
					return refuse(application::review_mutation_failure::unavailable);
				}
				const auto answer = aInner.perform_review_mutation(github_mutation_wire_request{ .Context = aWrite.Context, .Wire = *request }, aStop, { });
				if (std::holds_alternative<github_mutation_wire_cancelled>(answer)) {
					return std::optional<github_mutation_wire_completion>{ github_mutation_wire_cancelled{ .Transmission = transmission::definitely_not_sent } };
				}
				const auto* success = std::get_if<github_mutation_wire_success>(&answer);
				if (success == nullptr) {
					return refuse(application::review_mutation_failure::unavailable);
				}
				const auto observed = parse_target_observation(success->Response, aTarget);
				if (!observed) {
					return refuse(target_recorded_base_moved(success->Response, aTarget) ? application::review_mutation_failure::target_moved
																						 : application::review_mutation_failure::unavailable);
				}
				if (observed->Target.Semantic.ExpectedHead != aTarget.Semantic.ExpectedHead) {
					return refuse(application::review_mutation_failure::stale_head);
				}
				return std::nullopt;
			}

			auto check(const github_wire_request& aViewer, std::stop_token aStop) -> std::optional<application::github_provider_error>
			{
				if (Mismatch.load()) {
					return provider_error(application::github_provider_failure::authentication);
				}
				if (aStop.stop_requested()) {
					return provider_error(application::github_provider_failure::unavailable);
				}
				const auto result = Transport->perform(aViewer, aStop);
				if (const auto* error = std::get_if<github_wire_error>(&result);
					error != nullptr && error->Correlation == aViewer.Correlation && application::valid(error->Error)) {
					return error->Error;
				}
				const auto* success = std::get_if<github_wire_success>(&result);
				if (success == nullptr || success->Correlation != aViewer.Correlation) {
					return provider_error(application::github_provider_failure::malformed_response);
				}
				const auto observed = normalize_observed_github_viewer(success->Response, Server);
				if (!observed) {
					return observed.error();
				}
				if (!admit(github_answering_account{ .Account = observed->Viewer.Account, .Login = observed->Viewer.Login })) {
					return provider_error(application::github_provider_failure::authentication);
				}
				return std::nullopt;
			}
			/// The one comparison every observation, and every answer that
			/// names its own account, passes ([[BPR-1370]]).
			auto admit(const github_answering_account& aObserved) -> bool
			{
				if (aObserved.Account != Account || aObserved.Login != Login) {
					Mismatch.store(true);
					return false;
				}
				return !Mismatch.load();
			}
			std::shared_ptr<github_wire_transport_port> Transport;
			github_wire_request                         Viewer;
			server_contract                             Server;
			core::github_account_id                     Account;
			std::string                                 Login;
			std::atomic_bool                            Mismatch = false;
		};

		[[nodiscard]] auto
		provider_failure(const capability_probe_unavailability aFailure) noexcept
			-> application::github_provider_failure
		{
			switch (aFailure) {
			case capability_probe_unavailability::authentication:
				return application::github_provider_failure::authentication;
			case capability_probe_unavailability::permission:
				return application::github_provider_failure::permission;
			case capability_probe_unavailability::network:
				return application::github_provider_failure::network;
			case capability_probe_unavailability::api_version:
				return application::github_provider_failure::api_version;
			case capability_probe_unavailability::rate_limit:
				return application::github_provider_failure::rate_limit;
			case capability_probe_unavailability::certificate:
				return application::github_provider_failure::certificate;
			case capability_probe_unavailability::redirect:
				return application::github_provider_failure::redirect;
			case capability_probe_unavailability::timeout:
				return application::github_provider_failure::timeout;
			case capability_probe_unavailability::response_limit:
			case capability_probe_unavailability::allocation_failure:
				return application::github_provider_failure::response_limit;
			case capability_probe_unavailability::invalid_contract:
			case capability_probe_unavailability::invalid_limits:
			case capability_probe_unavailability::invalid_status:
			case capability_probe_unavailability::invalid_content_type:
			case capability_probe_unavailability::partial_response:
			case capability_probe_unavailability::malformed_response:
				return application::github_provider_failure::malformed_response;
			case capability_probe_unavailability::cancelled:
			case capability_probe_unavailability::none:
				return application::github_provider_failure::unavailable;
			}
			return application::github_provider_failure::unavailable;
		}

		[[nodiscard]] auto
		schema_state(const capability_schema_fact_state aState) noexcept
			-> core::github_capability_state
		{
			switch (aState) {
			case capability_schema_fact_state::matched:
				return core::github_capability_state::supported;
			case capability_schema_fact_state::missing:
			case capability_schema_fact_state::incompatible:
				return core::github_capability_state::unsupported;
			case capability_schema_fact_state::unknown:
				return core::github_capability_state::unknown;
			}
			return core::github_capability_state::unknown;
		}

		[[nodiscard]] auto
		capability_facts(const core::github_capabilities& aCapabilities)
			-> std::array<application::github_capability_fact,
						  application::github_capability_fact_count>
		{
			std::array<application::github_capability_fact,
					   application::github_capability_fact_count>
				facts{ };
			for (std::size_t index = 0U; index < facts.size(); ++index) {
				const auto capability = application::canonical_github_capabilities[index];
				facts[index]          = {
					.Capability = capability,
					.State      = core::capability_state(aCapabilities, capability),
					.Source     = application::github_capability_evidence_source::
						observed_contract,
				};
			}
			return facts;
		}

		[[nodiscard]] auto schema_facts(const capability_probe_observation& aObservation)
			-> std::array<application::github_schema_capability_fact,
						  application::github_schema_capability_fact_count>
		{
			std::array<application::github_schema_capability_fact,
					   application::github_schema_capability_fact_count>
				facts{ };
			for (std::size_t index = 0U; index < facts.size(); ++index) {
				facts[index] = {
					.Capability = application::canonical_github_schema_capabilities[index],
					.State      = schema_state(aObservation.Facts[index]),
					.Source     = application::github_capability_evidence_source::
						observed_contract,
				};
			}
			return facts;
		}

		[[nodiscard]] auto exact_account_transport(
			const application::github_account_transport& aOpaque,
			const application::github_profile_authority& aProfile,
			const core::application_session_id           aSession,
			const core::github_identity_generation       aGeneration,
			const core::github_account_id                aAccount,
			const std::optional<core::request_id>        aCredentialRequest = std::nullopt)
			-> std::shared_ptr<github_wire_account_transport>
		{
			auto transport = std::dynamic_pointer_cast<github_wire_account_transport>(aOpaque);
			if (transport == nullptr || transport->Transport == nullptr
				|| transport->Profile != aProfile
				|| transport->CredentialContext.ApplicationSession != aSession
				|| transport->CredentialContext.IdentityGeneration != aGeneration
				|| transport->CredentialContext.Authority.Profile != aProfile.Profile
				|| transport->CredentialContext.Authority.ProfileRevision
					!= aProfile.ProfileRevision
				|| transport->CredentialContext.Authority.Host != aProfile.Host
				|| transport->CredentialContext.Authority.Hostname != aProfile.Hostname
				|| transport->CredentialContext.Authority.Account != aAccount
				|| transport->CredentialContext.Authority.ServerContract
					!= aProfile.ServerContract) {
				return { };
			}
			if (aCredentialRequest.has_value()
				&& transport->CredentialContext.Request != *aCredentialRequest) {
				return { };
			}
			return transport;
		}
	}

	class github_provider::implementation final
	{
	public:
		explicit implementation(std::shared_ptr<provider_state> aState) noexcept
			: State{ std::move(aState) }
		{
		}

		std::shared_ptr<provider_state> State;
	};

	auto native_github_wire_transport_factory::make_bootstrap(
		const validated_github_host_profile&       aProfile,
		const core::github_session_request_context aContext,
		application::github_credential_material    aCredential) noexcept
		-> github_wire_transport_factory_result
	{
		auto transport = make_native_https_bootstrap_transport(
			aProfile, aContext, std::move(aCredential));
		if (!transport.has_value()) {
			return std::unexpected{ configuration_error(transport.error()) };
		}
		return std::shared_ptr<github_wire_transport_port>{ std::move(*transport) };
	}

	auto native_github_wire_transport_factory::make_account(
		const validated_github_host_profile&            aProfile,
		application::github_credential_retrieve_success aCredential) noexcept
		-> github_wire_transport_factory_result
	{
		auto transport = make_native_https_account_transport(aProfile, std::move(aCredential));
		if (!transport.has_value()) {
			return std::unexpected{ configuration_error(transport.error()) };
		}
		return std::shared_ptr<github_wire_transport_port>{ std::move(*transport) };
	}

	auto native_github_wire_transport_factory::make_anonymous(
		const validated_github_host_profile&       aProfile,
		const core::github_session_request_context aContext) noexcept
		-> github_wire_transport_factory_result
	{
		auto transport = make_native_https_anonymous_read_transport(
			aProfile, aContext);
		if (!transport.has_value()) {
			return std::unexpected{ configuration_error(transport.error()) };
		}
		return std::shared_ptr<github_wire_transport_port>{ std::move(*transport) };
	}

	github_provider::github_provider(
		std::unique_ptr<implementation> aImplementation) noexcept
		: Implementation{ std::move(aImplementation) }
	{
	}

	github_provider::~github_provider()
	{
		shutdown();
	}

	void github_provider::resolve_profile(
		application::github_profile_request            aRequest,
		application::github_profile_completion_handler aCompletion)
	{
		if (Implementation == nullptr || !valid_request(aRequest)) {
			aCompletion(application::github_profile_error{
				.Request = aRequest,
				.Error   = provider_error(
					application::github_provider_failure::unavailable),
			});
			return;
		}
		const auto state   = Implementation->State;
		const auto profile = find_profile(*state, aRequest.Context.Profile);
		std::shared_ptr<provider_completion_delivery<
			application::github_profile_completion_handler>>
			delivery;
		try {
			delivery = make_delivery(std::move(aCompletion));
		} catch (...) {
			aCompletion(application::github_profile_error{
				.Request = aRequest,
				.Error   = provider_error(
					application::github_provider_failure::response_limit),
			});
			return;
		}
		const auto jobResult = make_job(
			state, provider_phase::profile, aRequest.Context, aRequest.Attempt);
		if (!jobResult.has_value()) {
			if (jobResult.error() == make_job_failure::closing) {
				delivery->deliver(
					application::github_profile_cancelled{ .Request = aRequest });
			} else {
				delivery->deliver(application::github_profile_error{
					.Request = aRequest,
					.Error   = provider_error(
						application::github_provider_failure::response_limit),
				});
			}
			return;
		}
		const auto& job = *jobResult;
		try {
			state->Effects->post(
				[state, job, profile, aRequest, delivery] {
					try {
						if (finish_job(state, job)) {
							delivery->deliver(application::github_profile_cancelled{
								.Request = aRequest,
							});
						} else if (profile == nullptr) {
							delivery->deliver(application::github_profile_error{
								.Request = aRequest,
								.Error   = provider_error(
									application::github_provider_failure::
										unavailable),
							});
						} else {
							delivery->deliver(application::github_profile_success{
								.Request   = aRequest,
								.Authority = profile_authority(*profile),
							});
						}
					} catch (...) {
						const auto cancelled = finish_job(state, job);
						if (cancelled) {
							delivery->deliver(application::github_profile_cancelled{
								.Request = aRequest,
							});
						} else {
							delivery->deliver(application::github_profile_error{
								.Request = aRequest,
								.Error   = provider_error(
									application::github_provider_failure::
										response_limit),
							});
						}
					}
				});
		} catch (...) {
			const auto cancelled = stop_after_failed_post(state, job);
			if (cancelled) {
				delivery->deliver(application::github_profile_cancelled{
					.Request = aRequest,
				});
			} else {
				delivery->deliver(application::github_profile_error{
					.Request = aRequest,
					.Error   = provider_error(
						application::github_provider_failure::response_limit),
				});
			}
		}
	}

	void github_provider::cancel_profile(
		const application::github_profile_request aRequest)
	{
		if (Implementation != nullptr) {
			cancel_jobs(Implementation->State, provider_phase::profile, aRequest);
		}
	}

	void github_provider::observe_viewer(
		application::github_viewer_submission         aSubmission,
		application::github_viewer_completion_handler aCompletion)
	{

		const auto log = Implementation != nullptr ? Implementation->State->Log : nullptr;
		if (log) {
			const auto requestId = aSubmission.Request.Context.Request.value();
			static_cast<void>(log->record({ .Operation = application::diagnostic_operation::viewer, .Outcome = application::diagnostic_outcome::started, .Request = requestId }));
			try {
				auto candidate = [log, requestId, completion = aCompletion](application::github_viewer_completion aTerminal) {
					auto outcome = application::diagnostic_outcome::succeeded;
					if (std::holds_alternative<application::github_viewer_error>(aTerminal)) {
						outcome = application::diagnostic_outcome::failed;
					}
					if (std::holds_alternative<application::github_viewer_cancelled>(aTerminal)) {
						outcome = application::diagnostic_outcome::cancelled;
					}
					static_cast<void>(log->record({ .Operation = application::diagnostic_operation::viewer, .Outcome = outcome, .Request = requestId }));
					completion(std::move(aTerminal));
				};
				aCompletion = std::move(candidate);
			} catch (...) { /* Logging cannot consume or replace the original completion on allocation failure. */
			}
		}
		auto request = aSubmission.Request;
		if (Implementation == nullptr || !valid_request(request)) {
			aCompletion(application::github_viewer_error{
				.Request = request,
				.Error   = provider_error(
					application::github_provider_failure::unavailable),
			});
			return;
		}
		const auto state   = Implementation->State;
		const auto profile = find_exact_profile(*state, request.Authority);
		if (profile == nullptr
			|| (request.Authority.Authentication
					== application::github_authentication_transport::github_cli
				&& !std::holds_alternative<std::monostate>(
					aSubmission.Authentication))) {
			aCompletion(application::github_viewer_error{
				.Request = request,
				.Error   = provider_error(
					application::github_provider_failure::unavailable),
			});
			return;
		}
		if (request.Authority.Authentication
				== application::github_authentication_transport::github_cli
			&& !state->TransportFactory->supports_cli()) {
			aCompletion(application::github_viewer_error{
				.Request = request,
				.Error   = provider_error(application::github_provider_failure::cli_unavailable),
			});
			return;
		}

		if (state->ReadAuthority
			== github_provider_read_authority::anonymous_public) {
			// Refused here, before a job, a transport, or a wire request
			// exists. The viewer operation names an account and an anonymous
			// authority has none; a GHES profile is refused first, and for
			// every operation. Either way the provider observes zero requests.
			aCompletion(application::github_viewer_error{
				.Request = request,
				.Error   = anonymous_read_error(
					read_operation::get_viewer_identity,
					profile->server_contract()),
			});
			return;
		}

		std::shared_ptr<provider_completion_delivery<
			application::github_viewer_completion_handler>>
			delivery;
		try {
			delivery = make_delivery(std::move(aCompletion));
		} catch (...) {
			aCompletion(application::github_viewer_error{
				.Request = request,
				.Error   = provider_error(
					application::github_provider_failure::response_limit),
			});
			return;
		}
		const auto jobResult = make_job(
			state, provider_phase::viewer, request.Context, request.Attempt);
		if (!jobResult.has_value()) {
			if (jobResult.error() == make_job_failure::closing) {
				delivery->deliver(
					application::github_viewer_cancelled{ .Request = request });
			} else {
				delivery->deliver(application::github_viewer_error{
					.Request = request,
					.Error   = provider_error(
						application::github_provider_failure::response_limit),
				});
			}
			return;
		}
		const auto&                                                job = *jobResult;
		std::shared_ptr<application::github_viewer_authentication> authentication;
		try {
			authentication = std::make_shared<application::github_viewer_authentication>(
				std::move(aSubmission.Authentication));
			state->Effects->post(
				[state,
				 job,
				 profile,
				 request,
				 viewerAuthentication = std::move(authentication),
				 delivery]() mutable {
					const auto cancelled = [&] {
						finish_job(state, job);
						delivery->deliver(application::github_viewer_cancelled{
							.Request = request,
						});
					};
					const auto failed =
						[&](application::github_provider_error aError) {
							if (finish_job(state, job)) {
								delivery->deliver(application::github_viewer_cancelled{
									.Request = request,
								});
								return;
							}
							delivery->deliver(application::github_viewer_error{
								.Request = request,
								.Error   = std::move(aError),
							});
						};
					try {
						if (job->Stop.stop_requested()) {
							cancelled();
							return;
						}
						if (viewerAuthentication->valueless_by_exception()) {
							failed(provider_error(
								application::github_provider_failure::
									malformed_response));
							return;
						}

						std::optional<
							application::github_credential_operation_context>
															 accountContext;
						github_wire_transport_factory_result transport = std::unexpected{ provider_error(
							application::github_provider_failure::unavailable) };
						if (auto* bootstrap = std::get_if<
								application::github_bootstrap_credential>(
								viewerAuthentication.get());
							bootstrap != nullptr) {
							if (!application::valid(bootstrap->Credential)) {
								failed(provider_error(
									application::github_provider_failure::
										authentication));
								return;
							}
							transport = state->TransportFactory->make_bootstrap(
								*profile,
								request.Context,
								std::move(bootstrap->Credential));
						} else if (auto* account = std::get_if<
									   application::
										   github_credential_retrieve_success>(
									   viewerAuthentication.get());
								   account != nullptr) {
							if (!application::valid(*account)
								|| account->Context.Request
									!= request.Context.Request
								|| account->Context.ApplicationSession
									!= request.Context.ApplicationSession
								|| account->Context.Authority.Profile
									!= request.Context.Profile
								|| account->Context.Authority.ProfileRevision
									!= request.Authority.ProfileRevision
								|| account->Context.Authority.Host
									!= request.Authority.Host
								|| account->Context.Authority.Hostname
									!= request.Authority.Hostname
								|| account->Context.Authority.ServerContract
									!= request.Authority.ServerContract
								|| account->Context.IdentityGeneration
									!= request.Context.IdentityGeneration) {
								failed(provider_error(
									application::github_provider_failure::
										authentication));
								return;
							}
							accountContext = account->Context;
							transport      = state->TransportFactory->make_account(
								*profile, std::move(*account));
						} else if (request.Authority.Authentication
									   == application::github_authentication_transport::github_cli
								   && std::holds_alternative<std::monostate>(*viewerAuthentication)) {
							transport = state->TransportFactory->make_cli(
								*profile, request.Context);
						}
						if (!transport.has_value()) {
							failed(transport.error());
							return;
						}
						const auto wire = viewer_wire_request(*profile, request);
						if (!wire.has_value()) {
							failed(wire.error());
							return;
						}
						const auto terminal = (*transport)->perform(*wire, job->Stop.get_token());
						if (terminal.valueless_by_exception()) {
							failed(provider_error(
								application::github_provider_failure::
									malformed_response));
							return;
						}
						if (const auto* wireCancelled = std::get_if<github_wire_cancelled>(&terminal);
							wireCancelled != nullptr) {
							if (wireCancelled->Correlation != wire->Correlation) {
								failed(provider_error(
									application::github_provider_failure::
										malformed_response));
							} else {
								cancelled();
							}
							return;
						}
						if (const auto* error = std::get_if<github_wire_error>(&terminal);
							error != nullptr) {
							if (error->Correlation != wire->Correlation
								|| !application::valid(error->Error)
								|| !rate_limits_are_empty_or_exact(
									error->Error.RateLimits,
									application::github_rate_limit_resource::core)) {
								failed(provider_error(
									application::github_provider_failure::
										malformed_response));
							} else {
								failed(error->Error);
							}
							return;
						}
						const auto* success = std::get_if<github_wire_success>(&terminal);
						if (success == nullptr
							|| success->Correlation != wire->Correlation) {
							failed(provider_error(
								application::github_provider_failure::
									malformed_response));
							return;
						}
						const auto server = expected_server(*profile);
						if (!server.has_value()) {
							failed(provider_error(
								application::github_provider_failure::api_version));
							return;
						}
						const auto observed = normalize_observed_github_viewer(
							success->Response, *server);
						if (!observed.has_value()) {
							failed(observed.error());
							return;
						}
						if (accountContext.has_value()
							&& observed->Viewer.Account
								!= accountContext->Authority.Account) {
							// The stored credential signs in as someone its
							// binding never named. Refused as before, and said
							// as such ([[BPR-0644]]): the host keeps a
							// remembered grant through it and shows why.
							auto mismatch            = provider_error(application::github_provider_failure::authentication);
							mismatch.AccountMismatch = true;
							failed(std::move(mismatch));
							return;
						}
						// [[BPR-1373]]. A pinned profile's gh falls back silently to
						// the active account's token when its login has no keychain
						// entry, so the answer must name the pinned login, and the
						// account that login first signed in as in this run.
						if (request.Authority.Authentication == application::github_authentication_transport::github_cli
							&& !profile->cli_account_login().empty()) {
							auto bound = false;
							{
								const std::lock_guard lock{ state->Mutex };
								const auto            key = std::pair{ profile->profile().value(), std::string{ profile->cli_account_login() } };
								const auto [entry, fresh] = state->CliPinnedAccounts.try_emplace(key, observed->Viewer.Account);
								bound                     = (profile->cli_account().value() == 0U || profile->cli_account() == observed->Viewer.Account)
									&& observed->Viewer.Login == profile->cli_account_login()
									&& (fresh || entry->second == observed->Viewer.Account);
								if (fresh && !bound) {
									state->CliPinnedAccounts.erase(entry);
								}
							}
							if (!bound) {
								auto mismatch            = provider_error(application::github_provider_failure::authentication);
								mismatch.AccountMismatch = true;
								failed(std::move(mismatch));
								return;
							}
						}
						if (request.Authority.Authentication
							== application::github_authentication_transport::github_cli) {
							// This records the observed account for the scoped lease. It
							// contains no CLI credential and does not claim that gh's
							// mutable active account is pinned for later requests.
							transport      = std::make_shared<cli_observed_account_transport>(*transport, *wire, *server, observed->Viewer.Account, observed->Viewer.Login);
							accountContext = application::github_credential_operation_context{
								.Request            = request.Context.Request,
								.ApplicationSession = request.Context.ApplicationSession,
								.Authority          = {
									.Profile         = request.Authority.Profile,
									.ProfileRevision = request.Authority.ProfileRevision,
									.Host            = request.Authority.Host,
									.Hostname        = request.Authority.Hostname,
									.Account         = observed->Viewer.Account,
									.ServerContract  = request.Authority.ServerContract,
									.BprProfile      = core::default_bpr_profile_id,
								},
								.IdentityGeneration = request.Context.IdentityGeneration,
							};
						}

						application::github_account_transport accountTransport;
						if (accountContext.has_value()) {
							accountTransport = std::make_shared<github_wire_account_transport>(
								request.Authority,
								*accountContext,
								*transport);
						}
						if (finish_job(state, job)) {
							delivery->deliver(application::github_viewer_cancelled{
								.Request = request,
							});
							return;
						}
						delivery->deliver(application::github_viewer_success{
							.Request          = request,
							.Viewer           = observed->Viewer,
							.RateLimits       = observed->RateLimits,
							.AccountTransport = std::move(accountTransport),
						});
					} catch (...) {
						failed(provider_error(
							application::github_provider_failure::response_limit));
					}
				});
		} catch (...) {
			const auto wasCancelled = stop_after_failed_post(state, job);
			if (wasCancelled) {
				delivery->deliver(application::github_viewer_cancelled{
					.Request = request,
				});
			} else {
				delivery->deliver(application::github_viewer_error{
					.Request = request,
					.Error   = provider_error(
						application::github_provider_failure::response_limit),
				});
			}
		}
	}

	void github_provider::cancel_viewer(
		const application::github_viewer_request aRequest)
	{
		if (Implementation != nullptr) {
			cancel_jobs(Implementation->State, provider_phase::viewer, aRequest);
		}
	}

	void github_provider::probe_capabilities(
		application::github_capability_request            aRequest,
		application::github_capability_completion_handler aCompletion)
	{

		const auto log = Implementation != nullptr ? Implementation->State->Log : nullptr;
		if (log) {
			const auto requestId = aRequest.Context.Request.value();
			static_cast<void>(log->record({ .Operation = application::diagnostic_operation::capability, .Outcome = application::diagnostic_outcome::started, .Request = requestId }));
			try {
				auto candidate = [log, requestId, completion = aCompletion](application::github_capability_completion aTerminal) {
					auto outcome = application::diagnostic_outcome::succeeded;
					if (std::holds_alternative<application::github_capability_error>(aTerminal)) {
						outcome = application::diagnostic_outcome::failed;
					}
					if (std::holds_alternative<application::github_capability_cancelled>(aTerminal)) {
						outcome = application::diagnostic_outcome::cancelled;
					}
					static_cast<void>(log->record({ .Operation = application::diagnostic_operation::capability, .Outcome = outcome, .Request = requestId }));
					completion(std::move(aTerminal));
				};
				aCompletion = std::move(candidate);
			} catch (...) { /* Logging cannot consume or replace the original completion on allocation failure. */
			}
		}
		if (Implementation != nullptr
			&& Implementation->State->ReadAuthority
				== github_provider_read_authority::anonymous_public) {
			// The probe is the GraphQL surface, and an anonymous read shape is
			// all-false by construction. Refusing the whole surface is what
			// keeps the shape a fact rather than something normalized out of a
			// refused probe.
			aCompletion(application::github_capability_error{
				.Request = std::move(aRequest),
				.Error   = provider_error(provider_failure_for(
					anonymous_read_refusal::graphql_surface_unavailable)),
			});
			return;
		}
		if (Implementation == nullptr || !valid_request(aRequest)) {
			aCompletion(application::github_capability_error{
				.Request = std::move(aRequest),
				.Error   = provider_error(
					application::github_provider_failure::unavailable),
			});
			return;
		}
		const auto state            = Implementation->State;
		const auto profile          = find_exact_profile(*state, aRequest.ProfileAuthority);
		const auto accountTransport = exact_account_transport(aRequest.AccountTransport,
															  aRequest.ProfileAuthority,
															  aRequest.Context.ApplicationSession,
															  aRequest.Context.IdentityGeneration,
															  aRequest.Viewer.Account,
															  aRequest.Context.Request);
		if (profile == nullptr || accountTransport == nullptr) {
			aCompletion(application::github_capability_error{
				.Request = std::move(aRequest),
				.Error   = provider_error(
					application::github_provider_failure::unavailable),
			});
			return;
		}
		std::shared_ptr<provider_completion_delivery<
			application::github_capability_completion_handler>>
			delivery;
		try {
			delivery = make_delivery(std::move(aCompletion));
		} catch (...) {
			aCompletion(application::github_capability_error{
				.Request = std::move(aRequest),
				.Error   = provider_error(
					application::github_provider_failure::response_limit),
			});
			return;
		}
		const auto jobResult = make_job(
			state, provider_phase::capability, aRequest.Context, aRequest.Attempt);
		if (!jobResult.has_value()) {
			if (jobResult.error() == make_job_failure::closing) {
				delivery->deliver(application::github_capability_cancelled{
					.Request = std::move(aRequest),
				});
			} else {
				delivery->deliver(application::github_capability_error{
					.Request = std::move(aRequest),
					.Error   = provider_error(
						application::github_provider_failure::response_limit),
				});
			}
			return;
		}
		const auto& job = *jobResult;
		try {
			state->Effects->post(
				[state,
				 job,
				 profile,
				 accountTransport,
				 aRequest,
				 delivery]() mutable {
					application::github_rate_limit_state rateLimits;
					capability_probe_batch               batch;
					const auto                           cancelled = [&] {
						finish_job(state, job);
						delivery->deliver(application::github_capability_cancelled{
							.Request = aRequest,
						});
					};
					const auto failed =
						[&](application::github_provider_error              aError,
							const application::github_capability_diagnostic aDetail = application::github_capability_diagnostic::unavailable_detail) {
							if (!aError.RateLimits.Graphql.has_value() && rateLimits.Graphql.has_value()) {
								aError.RateLimits = rateLimits;
							}
							if (finish_job(state, job)) {
								delivery->deliver(
									application::github_capability_cancelled{
										.Request = aRequest,
									});
								return;
							}
							delivery->deliver(application::github_capability_error{
								.Request          = aRequest,
								.Error            = std::move(aError),
								.CapabilityDetail = application::bounded_capability_diagnostic(aDetail),
							});
						};
					try {
						auto operation = accountTransport->Transport->begin_operation(job->Stop.get_token());
						if (!operation) {
							failed(operation.error());
							return;
						}
						const auto transport = *operation ? *operation : accountTransport->Transport;
						for (std::uint8_t part = 0U; part < capability_probe_part_count; ++part) {
							if (job->Stop.stop_requested()) {
								cancelled();
								return;
							}
							const auto compiled = compile_capability_probe_request(*profile, { }, part);
							if (!compiled.has_value()) {
								failed(
									provider_error(provider_failure(compiled.error())), application::github_capability_diagnostic::provider_request_compilation);
								return;
							}
							const github_capability_probe_wire_request wire{
								.Correlation = github_capability_probe_wire_correlation{
									.Part            = part,
									.Session         = aRequest.Context,
									.ProfileRevision = aRequest.ProfileAuthority.ProfileRevision,
									.Host            = aRequest.ProfileAuthority.Host,
									.ObservedAccount = aRequest.Viewer.Account,
								},
								.Wire = *compiled,
							};
							const auto terminal = transport->perform_capability_probe(
								wire, job->Stop.get_token());
							if (terminal.valueless_by_exception()) {
								failed(provider_error(
										   application::github_provider_failure::
											   malformed_response),
									   application::github_capability_diagnostic::provider_correlation);
								return;
							}
							if (const auto* wireCancelled = std::get_if<github_capability_probe_wire_cancelled>(
									&terminal);
								wireCancelled != nullptr) {
								if (wireCancelled->Correlation != wire.Correlation) {
									failed(provider_error(
											   application::github_provider_failure::
												   malformed_response),
										   application::github_capability_diagnostic::provider_correlation);
								} else {
									cancelled();
								}
								return;
							}
							if (const auto* error = std::get_if<github_capability_probe_wire_error>(
									&terminal);
								error != nullptr) {
								if (error->Correlation != wire.Correlation
									|| !application::valid(error->Error)
									|| !rate_limits_are_empty_or_exact(
										error->Error.RateLimits,
										application::github_rate_limit_resource::
											graphql)) {
									failed(provider_error(
											   application::github_provider_failure::
												   malformed_response),
										   error->Correlation != wire.Correlation ? application::github_capability_diagnostic::provider_correlation : application::github_capability_diagnostic::provider_metadata);
								} else {
									failed(error->Error, error->CapabilityDetail);
								}
								return;
							}
							const auto* success = std::get_if<github_capability_probe_wire_success>(
								&terminal);
							if (success == nullptr
								|| success->Correlation != wire.Correlation
								|| success->Response.RateLimitHeaderCount
									> success->Response.RateLimitHeaders.size()) {
								failed(provider_error(
										   application::github_provider_failure::
											   malformed_response),
									   success != nullptr && success->Correlation == wire.Correlation
										   ? application::github_capability_diagnostic::provider_metadata
										   : application::github_capability_diagnostic::provider_correlation);
								return;
							}

							if (state->Log) {
								static_cast<void>(state->Log->record({
									.Operation  = application::diagnostic_operation::capability,
									.Outcome    = application::diagnostic_outcome::response_received,
									.Request    = aRequest.Context.Request.value(),
									.HttpStatus = static_cast<std::uint16_t>(success->Response.Status),
									.Summary    = success->Summary,
								}));
							}

							wire_response metadataResponse{
								.Status  = success->Response.Status,
								.Headers = {
									http_header{
										.Name  = "Content-Type",
										.Value = success->Response.ContentType,
									},
								},
								.Body         = { },
								.Completeness = page_completeness::complete,
							};
							metadataResponse.Headers.reserve(
								success->Response.RateLimitHeaderCount + 1U);
							for (std::size_t index = 0U;
								 index < success->Response.RateLimitHeaderCount;
								 ++index) {
								const auto& header = success->Response.RateLimitHeaders[index];
								metadataResponse.Headers.push_back(http_header{
									.Name  = header.Name,
									.Value = header.Value,
								});
							}
							const auto metadata = parse_github_response_metadata(metadataResponse);
							const auto server   = expected_server(*profile);
							if (!metadata.has_value()) {
								failed(metadata.error(), application::github_capability_diagnostic::provider_metadata);
								return;
							}
							if (!server.has_value()
								|| (!metadata->RateLimit.has_value()
									&& *server == server_contract::github_com)
								|| (metadata->RateLimit.has_value()
									&& metadata->RateLimit->Resource
										!= application::github_rate_limit_resource::
											graphql)) {
								failed(provider_error(
										   application::github_provider_failure::
											   malformed_response),
									   application::github_capability_diagnostic::provider_rate_tuple);
								return;
							}
							if (metadata->RateLimit.has_value()) {
								static_cast<void>(application::observe(rateLimits, *metadata->RateLimit));
							}
							const auto appended = batch.append(success->Response, part);
							if (!appended) {
								const auto& observation = appended.error();
								auto        failure     = provider_error(
									valid(observation)
										? provider_failure(observation.Unavailability)
										: application::github_provider_failure::
											  malformed_response);
								failure.RateLimits = rateLimits;
								if (failure.Failure
									== application::github_provider_failure::rate_limit) {
									if (!metadata->RateLimit.has_value()) {
										failed(provider_error(
												   application::github_provider_failure::
													   malformed_response),
											   application::github_capability_diagnostic::provider_rate_tuple);
										return;
									}
									auto cause             = *metadata->RateLimit;
									cause.SecondaryLimited = cause.Remaining != 0U;
									static_cast<void>(application::observe(
										failure.RateLimits, cause));
									failure.RateLimitCause    = cause;
									failure.RetryAfterSeconds = metadata->RetryAfterSeconds.has_value()
										? metadata->RetryAfterSeconds
										: (cause.SecondaryLimited
											   ? std::optional<std::uint32_t>{ 60U }
											   : std::nullopt);
								} else if (metadata->RetryAfterSeconds.has_value()) {
									failed(provider_error(
											   application::github_provider_failure::
												   malformed_response),
										   application::github_capability_diagnostic::provider_retry_metadata);
									return;
								}
								failed(application::valid(failure)
										   ? failure
										   : provider_error(
												 application::github_provider_failure::
													 malformed_response),
									   valid(observation) ? observation.CapabilityDetail : application::github_capability_diagnostic::provider_observation);
								return;
							}
							if (metadata->RetryAfterSeconds.has_value()) {
								failed(provider_error(
										   application::github_provider_failure::
											   malformed_response),
									   application::github_capability_diagnostic::provider_retry_metadata);
								return;
							}
							if (part + 1U < capability_probe_part_count && rateLimits.Graphql.has_value() && rateLimits.Graphql->Remaining == 0U) {
								auto failure           = provider_error(application::github_provider_failure::rate_limit);
								failure.RateLimits     = rateLimits;
								failure.RateLimitCause = rateLimits.Graphql;
								failed(failure);
								return;
							}
						}
						const auto observation = batch.finish();
						if (!observation.Complete || !valid(observation)) {
							failed(provider_error(provider_failure(observation.Unavailability)), observation.CapabilityDetail);
							return;
						}
						auto facts    = capability_facts(observation.Capabilities);
						auto detailed = schema_facts(observation);
						if (finish_job(state, job)) {
							delivery->deliver(
								application::github_capability_cancelled{
									.Request = aRequest,
								});
							return;
						}
						delivery->deliver(application::github_capability_success{
							.Request     = aRequest,
							.Facts       = std::move(facts),
							.SchemaFacts = std::move(detailed),
							.RateLimits  = std::move(rateLimits),
						});
					} catch (...) {
						failed(provider_error(
								   application::github_provider_failure::response_limit),
							   application::github_capability_diagnostic::unavailable_detail);
					}
				});
		} catch (...) {
			const auto wasCancelled = stop_after_failed_post(state, job);
			if (wasCancelled) {
				delivery->deliver(application::github_capability_cancelled{
					.Request = std::move(aRequest),
				});
			} else {
				delivery->deliver(application::github_capability_error{
					.Request = std::move(aRequest),
					.Error   = provider_error(
						application::github_provider_failure::response_limit),
				});
			}
		}
	}

	void github_provider::cancel_capabilities(
		const application::github_capability_request aRequest)
	{
		if (Implementation != nullptr) {
			cancel_jobs(Implementation->State, provider_phase::capability, aRequest);
		}
	}

	void github_provider::fetch_pull_request(
		application::github_pull_request_provider_request   aRequest,
		application::github_pull_request_completion_handler aCompletion)
	{
		fetch_pull_request_with_progress(std::move(aRequest), std::move(aCompletion), { });
	}

	void github_provider::fetch_pull_request_with_progress(
		application::github_pull_request_provider_request   aRequest,
		application::github_pull_request_completion_handler aCompletion,
		application::github_pull_request_progress_handler   aProgress)
	{
		fetch_pull_request_with_observations(std::move(aRequest), std::move(aCompletion), std::move(aProgress), { });
	}

	void github_provider::fetch_pull_request_with_observations(
		application::github_pull_request_provider_request   aRequest,
		application::github_pull_request_completion_handler aCompletion,
		application::github_pull_request_progress_handler   aProgress,
		application::github_pull_request_overview_handler   aOverview)
	{
		const auto anonymous = Implementation != nullptr
			&& Implementation->State->ReadAuthority
				== github_provider_read_authority::anonymous_public;
		if (
			Implementation == nullptr
			|| !(anonymous ? valid_anonymous_read_correlation(aRequest)
						   : valid_request(aRequest))) {
			aCompletion(application::github_pull_request_error{
				.Request = std::move(aRequest),
				.Error   = provider_error(
					application::github_provider_failure::unavailable),
			});
			return;
		}
		const auto state   = Implementation->State;
		const auto profile = find_exact_profile(*state, aRequest.ProfileAuthority);
		if (profile == nullptr) {
			aCompletion(application::github_pull_request_error{
				.Request = std::move(aRequest),
				.Error   = provider_error(
					application::github_provider_failure::unavailable),
			});
			return;
		}
		std::shared_ptr<github_wire_account_transport> accountTransport;
		std::shared_ptr<github_wire_transport_port>    readTransport;
		if (anonymous) {
			// Every refusal below happens before a transport is constructed, so
			// a refused request costs zero requests rather than one that is
			// discarded. The contract rule is asked first: a GHES profile is
			// refused for every operation.
			const auto admitted = admit_anonymous_read(
				read_operation::get_pull_request, profile->server_contract());
			if (!admitted.has_value()) {
				aCompletion(application::github_pull_request_error{
					.Request = std::move(aRequest),
					.Error   = provider_error(
						provider_failure_for(admitted.error())),
				});
				return;
			}
			// An anonymous read shape is all-false. Requiring that here is what
			// keeps the shared read path from routing its GraphQL operation on
			// this authority: the operation is refused by the shape it would
			// need, before a request naming it can be compiled.
			if (aRequest.CapabilityEvidence.ReadShape
				!= application::github_read_shape{ }) {
				aCompletion(application::github_pull_request_error{
					.Request = std::move(aRequest),
					.Error   = provider_error(provider_failure_for(
						anonymous_read_refusal::graphql_surface_unavailable)),
				});
				return;
			}
			auto transport = state->TransportFactory->make_anonymous(
				*profile, anonymous_session_context(aRequest.Context));
			if (!transport.has_value()) {
				aCompletion(application::github_pull_request_error{
					.Request = std::move(aRequest),
					.Error   = transport.error(),
				});
				return;
			}
			readTransport = std::move(*transport);
		} else {
			accountTransport = exact_account_transport(aRequest.AccountTransport,
													   aRequest.ProfileAuthority,
													   aRequest.AuthenticationSession.value_or(aRequest.Context.ApplicationSession),
													   aRequest.Context.IdentityGeneration,
													   aRequest.Context.Account);
			if (accountTransport != nullptr) {
				readTransport = accountTransport->Transport;
			}
		}
		if (readTransport == nullptr) {
			aCompletion(application::github_pull_request_error{
				.Request = std::move(aRequest),
				.Error   = provider_error(
					application::github_provider_failure::unavailable),
			});
			return;
		}
		std::shared_ptr<provider_completion_delivery<
			application::github_pull_request_completion_handler>>
			delivery;
		try {
			delivery = make_delivery(std::move(aCompletion));
		} catch (...) {
			aCompletion(application::github_pull_request_error{
				.Request = std::move(aRequest),
				.Error   = provider_error(
					application::github_provider_failure::response_limit),
			});
			return;
		}
		const auto jobResult = make_job(
			state, provider_phase::pull_request, aRequest.Context, aRequest.Attempt);
		if (!jobResult.has_value()) {
			if (jobResult.error() == make_job_failure::closing) {
				delivery->deliver(application::github_pull_request_cancelled{
					.Request = std::move(aRequest),
				});
			} else {
				delivery->deliver(application::github_pull_request_error{
					.Request = std::move(aRequest),
					.Error   = provider_error(
						application::github_provider_failure::response_limit),
				});
			}
			return;
		}
		const auto& job = *jobResult;
		try {
			state->Effects->post(
				[state,
				 job,
				 profile,
				 accountTransport,
				 readTransport,
				 aRequest,
				 progress = std::move(aProgress),
				 overview = std::move(aOverview),
				 delivery]() mutable {
					pull_request_response_observer observer{ state->Log.get(), aRequest.Context, std::move(progress), std::move(overview) };
					const auto                     cancelled = [&] {
						finish_job(state, job);
						delivery->deliver(
							application::github_pull_request_cancelled{
								.Request           = aRequest,
								.LastReadOperation = observer.LastOperation,
							});
					};
					const auto failed =
						[&](application::github_provider_error aError) {
							if (finish_job(state, job)) {
								delivery->deliver(
									application::github_pull_request_cancelled{
										.Request           = aRequest,
										.LastReadOperation = observer.LastOperation,
									});
								return;
							}
							delivery->deliver(application::github_pull_request_error{
								.Request            = aRequest,
								.Error              = std::move(aError),
								.LastReadOperation  = observer.LastOperation,
								.ValidationError    = observer.ValidationError,
								.RelationshipDetail = observer.RelationshipDetail,
							});
						};
					try {
						if (job->Stop.stop_requested()) {
							cancelled();
							return;
						}
						const auto snapshot = allocate_snapshot(state);
						if (!snapshot.has_value()) {
							failed(provider_error(
								application::github_provider_failure::
									response_limit));
							return;
						}
						auto operation = readTransport->begin_operation(job->Stop.get_token());
						if (!operation) {
							failed(operation.error());
							return;
						}
						if (*operation) {
							readTransport = std::move(*operation);
						}
						auto result = perform_paged_github_read(*readTransport,
																*profile,
																aRequest,
																*snapshot,
																job->Stop.get_token(),
																{ },
																state->ReadAuthority, &observer);
						if (!result.has_value()) {
							auto& error = result.error();
							if (error.valueless_by_exception()) {
								failed(provider_error(
									application::github_provider_failure::
										malformed_response));
							} else if (std::holds_alternative<
										   github_paged_read_cancelled>(error)) {
								cancelled();
							} else {
								const auto& provider = std::get<application::github_provider_error>(
									error);
								failed(
									application::valid(provider)
										? provider
										: provider_error(
											  application::github_provider_failure::
												  malformed_response));
							}
							return;
						}
						// Timeline failure does not erase the independently accepted live read.
						auto timeline = perform_paged_github_timeline_read(*readTransport, *profile,
																		   aRequest, result->Repository.Repository, job->Stop.get_token(), { }, state->ReadAuthority, &observer);
						if (timeline) {
							result->ForcePushEvents      = std::move(timeline->ForcePushEvents);
							result->TimelineAvailability = application::github_timeline_availability::available;
							if (timeline->RateLimits.Core) {
								result->RateLimits.Core = timeline->RateLimits.Core;
							}
						} else if (std::holds_alternative<github_paged_read_cancelled>(timeline.error())) {
							cancelled();
							return;
						} else {
							const auto& error = std::get<application::github_provider_error>(timeline.error());
							if (error.RateLimits.Core) {
								result->RateLimits.Core = error.RateLimits.Core;
							}
							switch (error.Failure) {
							case application::github_provider_failure::response_limit:
								result->TimelineAvailability = application::github_timeline_availability::capped;
								break;
							case application::github_provider_failure::authentication:
							case application::github_provider_failure::permission:
							case application::github_provider_failure::api_version:
							case application::github_provider_failure::unsupported_capability:
							case application::github_provider_failure::rate_limit:
								result->TimelineAvailability = application::github_timeline_availability::refused;
								break;
							default:
								result->TimelineAvailability = application::github_timeline_availability::failed;
								break;
							}
						}
						// [[BPR-1560]], [[BPR-1572]]. GitHub's Files changed, its
						// commit list and the lines a LEFT comment is numbered in
						// compare from the merge base of the base commit GitHub
						// records for the pull request (`base.sha`) and the head,
						// not from `base.sha` itself, which may lag or lead. GitHub
						// keeps `base.sha` while the base branch only moves forward,
						// even when it takes commits the head has (a stacked pull
						// request's base branch fast-forwarded to, or merging, part
						// of it), and replaces it with the branch's new tip a few
						// seconds after the branch is force-pushed (measured on
						// github.com, 2026-10-17). The reads that bind it to this
						// snapshot: the merge base against the recorded base (an
						// exact pair, remembered); for an open pull request the
						// merge base against the base branch, which either names
						// the same commit or, when it names another, is accepted
						// only while GitHub's own commit count for the pull request
						// is the recorded base's and the recorded base is an
						// ancestor of the branch tip GitHub resolved, so the base
						// branch only moved forward and GitHub keeps comparing from
						// the recorded base; otherwise GitHub is replacing the
						// recorded base, or its answers disagree, and the merge
						// base is refused. Then the pull request's metadata again,
						// which must still be the snapshot's, so the file list and
						// the merge base describe one state. A closed pull request,
						// whose branch may have moved on or taken the head, is
						// compared with its recorded base only. Lost
						// authentication fails the whole read, as on every optional
						// read; any other failure leaves the merge base unavailable
						// and never stands in another base.
						{
							enum class merge_base_step : std::uint8_t {
								answered,
								unavailable,
								refused,
								stop,
							};
							const auto open      = result->PullRequest.State == application::github_pull_request_state::open;
							const auto head      = std::string{ result->PullRequest.HeadOid.value() };
							const auto recorded  = std::string{ result->PullRequest.BaseOid.value() };
							auto       stopError = std::optional<github_paged_read_failure>{ };
							auto       observe   = [&](const application::github_rate_limit_state& aRates) {
								if (aRates.Core) {
									result->RateLimits.Core = aRates.Core;
								}
							};
							const auto settle = [&](const github_paged_read_failure& aFailure) {
								if (std::holds_alternative<github_paged_read_cancelled>(aFailure)) {
									stopError = aFailure;
									return merge_base_step::stop;
								}
								const auto& error = std::get<application::github_provider_error>(aFailure);
								observe(error.RateLimits);
								if (error.Failure == application::github_provider_failure::authentication) {
									stopError = aFailure;
									return merge_base_step::stop;
								}
								return merge_base_step::unavailable;
							};
							auto ask = [&](const github_merge_base_basis& aBasis, const std::string& aHead, std::optional<github_merge_base_read>& aAnswer) {
								auto read = perform_github_merge_base_read(*readTransport, *profile, aRequest, result->Repository.Repository, aBasis, aHead,
																		   job->Stop.get_token(), { }, state->ReadAuthority, &observer);
								if (!read) {
									return settle(read.error());
								}
								observe(read->RateLimits);
								aAnswer = std::move(*read);
								return merge_base_step::answered;
							};
							const auto key   = std::tuple{ aRequest.Context.Host.value(), aRequest.Context.Profile.value(), result->Repository.Repository.value(), recorded, head };
							auto       exact = std::optional<github_merge_base_read>{ };
							{
								std::scoped_lock lock{ state->Mutex };
								if (const auto found = state->MergeBases.find(key); found != state->MergeBases.end()) {
									exact = github_merge_base_read{ .MergeBaseOid = found->second.MergeBaseOid, .BaseCommitOid = recorded, .AheadBy = found->second.AheadBy };
								}
							}
							auto step = exact ? merge_base_step::answered : ask(github_merge_base_basis{ .BaseOid = recorded }, head, exact);
							if (step == merge_base_step::answered && exact) {
								constexpr auto   maximumRemembered = std::size_t{ 256U };
								std::scoped_lock lock{ state->Mutex };
								if (state->MergeBases.size() >= maximumRemembered) {
									state->MergeBases.clear();
								}
								state->MergeBases.emplace(key, provider_state::remembered_merge_base{ .MergeBaseOid = exact->MergeBaseOid, .AheadBy = exact->AheadBy });
							}
							auto chosen = exact;
							if (step == merge_base_step::answered && open) {
								auto tip = std::optional<github_merge_base_read>{ };
								step     = ask(github_merge_base_basis{ .BaseRef = result->PullRequest.BaseRef, .BaseOid = recorded }, head, tip);
								if (step == merge_base_step::answered && tip->MergeBaseOid == exact->MergeBaseOid) {
									chosen = std::move(tip);
								} else if (step == merge_base_step::answered) {
									// [[BPR-1572]]. The base branch names another merge
									// base. GitHub still compares from the recorded
									// base when its commit count for the pull request
									// is that base's and the base branch only moved
									// forward from it.
									const auto& reported = result->PullRequest.ReportedCommitCount;
									if (!exact->AheadBy || !reported || *exact->AheadBy != *reported) {
										step = merge_base_step::refused;
									} else {
										auto ancestry = std::optional<github_merge_base_read>{ };
										step          = ask(github_merge_base_basis{ .BaseOid = recorded }, tip->BaseCommitOid, ancestry);
										if (step == merge_base_step::answered && ancestry->MergeBaseOid != recorded) {
											step = merge_base_step::refused;
										}
									}
								}
							}
							if (step == merge_base_step::answered) {
								const auto confirmed = perform_github_pull_request_confirmation(*readTransport, *profile, aRequest, *result,
																								job->Stop.get_token(), { }, state->ReadAuthority, &observer);
								if (!confirmed) {
									step = settle(confirmed.error());
								} else {
									observe(confirmed->RateLimits);
									step = confirmed->Unchanged ? merge_base_step::answered : merge_base_step::refused;
								}
							}
							if (step == merge_base_step::stop) {
								if (std::holds_alternative<github_paged_read_cancelled>(*stopError)) {
									cancelled();
								} else {
									failed(std::get<application::github_provider_error>(*stopError));
								}
								return;
							}
							if (step == merge_base_step::answered) {
								result->MergeBaseOid          = application::commit_oid{ chosen->MergeBaseOid };
								result->MergeBaseTipOid       = application::commit_oid{ chosen->BaseCommitOid };
								result->MergeBaseAvailability = application::github_merge_base_availability::available;
							} else {
								result->MergeBaseAvailability = step == merge_base_step::refused ? application::github_merge_base_availability::refused
																								 : application::github_merge_base_availability::failed;
							}
						}
						if (finish_job(state, job)) {
							delivery->deliver(
								application::github_pull_request_cancelled{
									.Request           = aRequest,
									.LastReadOperation = observer.LastOperation,
								});
							return;
						}
						delivery->deliver(application::github_pull_request_success{
							.Request           = aRequest,
							.Snapshot          = std::move(*result),
							.LastReadOperation = observer.LastOperation,
						});
					} catch (...) {
						failed(provider_error(
							application::github_provider_failure::response_limit));
					}
				});
		} catch (...) {
			const auto wasCancelled = stop_after_failed_post(state, job);
			if (wasCancelled) {
				delivery->deliver(application::github_pull_request_cancelled{
					.Request = std::move(aRequest),
				});
			} else {
				delivery->deliver(application::github_pull_request_error{
					.Request = std::move(aRequest),
					.Error   = provider_error(
						application::github_provider_failure::response_limit),
				});
			}
		}
	}

	void github_provider::discover(application::personal_discovery_request            aRequest,
								   application::personal_discovery_completion_handler aCompletion)
	{
		const auto unavailable = [&aRequest, &aCompletion](const application::github_provider_failure aFailure) {
			aCompletion(application::personal_discovery_error{ .Request = std::move(aRequest), .Error = provider_error(aFailure) });
		};
		// A discovery read is authenticated only: a signed-out authority has
		// no viewer to search for, so the anonymous provider refuses it
		// before any transport is chosen.
		if (Implementation == nullptr || Implementation->State->ReadAuthority != github_provider_read_authority::authenticated) {
			unavailable(application::github_provider_failure::unavailable);
			return;
		}
		const auto state   = Implementation->State;
		const auto profile = find_exact_profile(*state, aRequest.ProfileAuthority);
		if (profile == nullptr) {
			unavailable(application::github_provider_failure::unavailable);
			return;
		}
		const auto accountTransport = exact_account_transport(aRequest.AccountTransport, aRequest.ProfileAuthority,
															  aRequest.AuthenticationSession.value_or(aRequest.Context.ApplicationSession),
															  aRequest.Context.IdentityGeneration, aRequest.Context.Account);
		if (accountTransport == nullptr) {
			unavailable(application::github_provider_failure::unavailable);
			return;
		}
		std::shared_ptr<provider_completion_delivery<application::personal_discovery_completion_handler>> delivery;
		try {
			delivery = make_delivery(std::move(aCompletion));
		} catch (...) {
			aCompletion(application::personal_discovery_error{ .Request = std::move(aRequest), .Error = provider_error(application::github_provider_failure::response_limit) });
			return;
		}
		const auto jobResult = make_job(state, provider_phase::discovery, aRequest.Context, application::github_provider_attempt{ 1U });
		if (!jobResult.has_value()) {
			if (jobResult.error() == make_job_failure::closing) {
				delivery->deliver(application::personal_discovery_cancelled{ .Request = std::move(aRequest) });
			} else {
				delivery->deliver(application::personal_discovery_error{ .Request = std::move(aRequest), .Error = provider_error(application::github_provider_failure::response_limit) });
			}
			return;
		}
		const auto& job = *jobResult;
		try {
			state->Effects->post([state, job, profile, accountTransport, aRequest, delivery]() mutable {
				try {
					auto operation = accountTransport->Transport->begin_operation(job->Stop.get_token());
					if (!operation) {
						finish_job(state, job);
						delivery->deliver(application::personal_discovery_error{ .Request = aRequest, .Error = operation.error() });
						return;
					}
					const auto transport = *operation ? *operation : accountTransport->Transport;
					auto       result    = job->Stop.stop_requested()
						? github_personal_discovery_read_result{ std::unexpected{ github_paged_read_failure{ github_paged_read_cancelled{ } } } }
						: perform_personal_discovery_read(*transport, *profile, aRequest, job->Stop.get_token());
					const auto was       = finish_job(state, job);
					if (was || (!result.has_value() && std::holds_alternative<github_paged_read_cancelled>(result.error()))) {
						delivery->deliver(application::personal_discovery_cancelled{ .Request = aRequest });
						return;
					}
					if (!result.has_value()) {
						const auto* error = std::get_if<application::github_provider_error>(&result.error());
						delivery->deliver(application::personal_discovery_error{
							.Request = aRequest,
							.Error   = error != nullptr && application::valid(*error) ? *error : provider_error(application::github_provider_failure::malformed_response),
						});
						return;
					}
					delivery->deliver(application::personal_discovery_success{
						.Request      = aRequest,
						.Rows         = std::move(result->Rows),
						.Completeness = result->Completeness,
						.RateLimits   = std::move(result->RateLimits),
					});
				} catch (...) {
					static_cast<void>(finish_job(state, job));
					delivery->deliver(application::personal_discovery_error{ .Request = aRequest, .Error = provider_error(application::github_provider_failure::response_limit) });
				}
			});
		} catch (...) {
			if (stop_after_failed_post(state, job)) {
				delivery->deliver(application::personal_discovery_cancelled{ .Request = std::move(aRequest) });
			} else {
				delivery->deliver(application::personal_discovery_error{ .Request = std::move(aRequest), .Error = provider_error(application::github_provider_failure::response_limit) });
			}
		}
	}

	void github_provider::resolve(application::markdown_image_resolution_request                         aRequest,
								  std::function<void(application::markdown_image_resolution_completion)> aCompletion)
	{
		const auto unavailable = [&aRequest, &aCompletion](const application::github_provider_failure aFailure) {
			aCompletion(application::markdown_image_resolution_error{ .Request = std::move(aRequest), .Error = provider_error(aFailure) });
		};
		// Only an authenticated reader has GitHub's signed addresses for it.
		if (Implementation == nullptr || Implementation->State->ReadAuthority != github_provider_read_authority::authenticated) {
			unavailable(application::github_provider_failure::unavailable);
			return;
		}
		const auto state   = Implementation->State;
		const auto profile = find_exact_profile(*state, aRequest.ProfileAuthority);
		if (profile == nullptr) {
			unavailable(application::github_provider_failure::unavailable);
			return;
		}
		const auto accountTransport = exact_account_transport(aRequest.AccountTransport, aRequest.ProfileAuthority,
															  aRequest.AuthenticationSession.value_or(aRequest.Context.ApplicationSession),
															  aRequest.Context.IdentityGeneration, aRequest.Context.Account);
		if (accountTransport == nullptr) {
			unavailable(application::github_provider_failure::unavailable);
			return;
		}
		std::shared_ptr<provider_completion_delivery<std::function<void(application::markdown_image_resolution_completion)>>> delivery;
		try {
			delivery = make_delivery(std::move(aCompletion));
		} catch (...) {
			aCompletion(application::markdown_image_resolution_error{ .Request = std::move(aRequest), .Error = provider_error(application::github_provider_failure::response_limit) });
			return;
		}
		const auto jobResult = make_job(state, provider_phase::image_resolution, aRequest.Context, application::github_provider_attempt{ 1U });
		if (!jobResult.has_value()) {
			if (jobResult.error() == make_job_failure::closing) {
				delivery->deliver(application::markdown_image_resolution_cancelled{ .Request = std::move(aRequest) });
			} else {
				delivery->deliver(application::markdown_image_resolution_error{ .Request = std::move(aRequest), .Error = provider_error(application::github_provider_failure::response_limit) });
			}
			return;
		}
		const auto& job = *jobResult;
		try {
			state->Effects->post([state, job, profile, accountTransport, aRequest, delivery]() mutable {
				try {
					auto operation = accountTransport->Transport->begin_operation(job->Stop.get_token());
					if (!operation) {
						finish_job(state, job);
						delivery->deliver(application::markdown_image_resolution_error{ .Request = aRequest, .Error = operation.error() });
						return;
					}
					const auto transport = *operation ? *operation : accountTransport->Transport;
					auto       result    = job->Stop.stop_requested()
						? github_markdown_image_resolution_result{ std::unexpected{ github_paged_read_failure{ github_paged_read_cancelled{ } } } }
						: perform_markdown_image_resolution(*transport, *profile, aRequest, job->Stop.get_token());
					const auto was       = finish_job(state, job);
					if (was || (!result.has_value() && std::holds_alternative<github_paged_read_cancelled>(result.error()))) {
						delivery->deliver(application::markdown_image_resolution_cancelled{ .Request = aRequest });
						return;
					}
					if (!result.has_value()) {
						const auto* error = std::get_if<application::github_provider_error>(&result.error());
						delivery->deliver(application::markdown_image_resolution_error{
							.Request = aRequest,
							.Error   = error != nullptr && application::valid(*error) ? *error : provider_error(application::github_provider_failure::malformed_response),
						});
						return;
					}
					delivery->deliver(application::markdown_image_resolution_success{ .Request = aRequest, .Bodies = std::move(result->Bodies), .RateLimits = std::move(result->RateLimits) });
				} catch (...) {
					static_cast<void>(finish_job(state, job));
					delivery->deliver(application::markdown_image_resolution_error{ .Request = aRequest, .Error = provider_error(application::github_provider_failure::response_limit) });
				}
			});
		} catch (...) {
			if (stop_after_failed_post(state, job)) {
				delivery->deliver(application::markdown_image_resolution_cancelled{ .Request = std::move(aRequest) });
			} else {
				delivery->deliver(application::markdown_image_resolution_error{ .Request = std::move(aRequest), .Error = provider_error(application::github_provider_failure::response_limit) });
			}
		}
	}

	void github_provider::cancel_resolution(const application::markdown_image_resolution_request& aRequest)
	{
		if (Implementation == nullptr) {
			return;
		}
		const auto             state   = Implementation->State;
		const auto             context = provider_parent_context{ aRequest.Context };
		const std::scoped_lock lock{ state->Mutex };
		for (const auto& job : state->Jobs) {
			if (job->Phase == provider_phase::image_resolution && job->Context == context) {
				job->Stop.request_stop();
			}
		}
	}

	auto make_native_markdown_image_media_factory() -> github_provider::media_transport_factory
	{
		return [](const validated_github_host_profile& aProfile) -> github_provider::media_performer {
			auto transport = make_native_https_media_transport(aProfile);
			if (!transport) {
				return { };
			}
			auto shared = std::shared_ptr<native_https_transport>{ std::move(*transport) };
			return [shared](const application::markdown_image_fetch_request& aRequest, std::stop_token aStop) {
				return shared->perform_markdown_image(aRequest, std::move(aStop));
			};
		};
	}

	void github_provider::configure_markdown_images(std::vector<application::effect_executor*> aLanes, media_transport_factory aFactory)
	{
		if (Implementation == nullptr) {
			return;
		}
		const auto             state = Implementation->State;
		const std::scoped_lock lock{ state->Mutex };
		state->MediaLanes.clear();
		for (auto* const lane : aLanes) {
			if (lane != nullptr) {
				auto entry      = std::make_unique<provider_state::media_lane>();
				entry->Executor = lane;
				state->MediaLanes.push_back(std::move(entry));
			}
		}
		state->MediaFactory = std::move(aFactory);
	}

	void github_provider::fetch(application::markdown_image_fetch_request                     aRequest,
								std::stop_token                                               aStop,
								std::function<void(application::markdown_image_fetch_result)> aCompletion)
	{
		auto delivery = std::shared_ptr<provider_completion_delivery<std::function<void(application::markdown_image_fetch_result)>>>{ };
		try {
			delivery = make_delivery(std::move(aCompletion));
		} catch (...) {
			return;
		}
		const auto failed = [&delivery] { delivery->deliver(application::markdown_image_fetch_result{ std::unexpected{ core::markdown_image_state::failed } }); };
		if (Implementation == nullptr || Implementation->State->ReadAuthority != github_provider_read_authority::authenticated) {
			failed();
			return;
		}
		const auto state   = Implementation->State;
		const auto profile = find_exact_profile(*state, aRequest.ProfileAuthority);
		if (profile == nullptr || aRequest.Policy.Hostname != std::string{ aRequest.ProfileAuthority.Hostname.value() }) {
			failed();
			return;
		}
		try {
			if (aRequest.First.Credential) {
				// Only the exact accepted account's own native transport holds
				// the credential; a CLI profile's does not, so its hop goes out
				// credential-free below.
				const auto account = exact_account_transport(aRequest.AccountTransport, aRequest.ProfileAuthority,
															 aRequest.AuthenticationSession.value_or(core::application_session_id{ }),
															 aRequest.IdentityGeneration, aRequest.Account);
				const auto native  = account ? std::dynamic_pointer_cast<native_https_transport>(account->Transport) : nullptr;
				if (native != nullptr) {
					state->Effects->post([native, aRequest, aStop, delivery] {
						delivery->deliver(native->perform_markdown_image(aRequest, aStop));
					});
					return;
				}
				aRequest.First.Credential           = false;
				aRequest.Policy.CredentialAvailable = false;
			}
			provider_state::media_lane* lane = nullptr;
			{
				const std::scoped_lock lock{ state->Mutex };
				for (const auto& candidate : state->MediaLanes) {
					if (lane == nullptr || candidate->Queued.load() < lane->Queued.load()) {
						lane = candidate.get();
					}
				}
			}
			if (lane == nullptr || !state->MediaFactory) {
				failed();
				return;
			}
			++lane->Queued;
			lane->Executor->post([state, lane, profile, aRequest, aStop, delivery] {
				auto result = application::markdown_image_fetch_result{ std::unexpected{ core::markdown_image_state::failed } };
				try {
					const auto key  = profile->profile().value();
					auto&      slot = lane->Transports[key];
					if (!slot) {
						slot = state->MediaFactory(*profile);
					}
					if (slot) {
						result = slot(aRequest, aStop);
					}
				} catch (...) {
				}
				--lane->Queued;
				delivery->deliver(std::move(result));
			});
		} catch (...) {
			failed();
		}
	}

	void github_provider::cancel_discovery(const application::personal_discovery_request& aRequest)
	{
		if (Implementation == nullptr) {
			return;
		}
		const auto             state   = Implementation->State;
		const auto             context = provider_parent_context{ aRequest.Context };
		const std::scoped_lock lock{ state->Mutex };
		for (const auto& job : state->Jobs) {
			if (job->Phase == provider_phase::discovery && job->Context == context) {
				job->Stop.request_stop();
			}
		}
	}

	void github_provider::cancel_pull_request(
		const application::github_pull_request_provider_request aRequest)
	{
		if (Implementation != nullptr) {
			cancel_jobs(
				Implementation->State, provider_phase::pull_request, aRequest);
		}
	}

	void github_provider::shutdown() noexcept
	{
		if (Implementation == nullptr) {
			return;
		}
		const auto             state = Implementation->State;
		const std::scoped_lock lock{ state->Mutex };
		if (state->Closing) {
			return;
		}
		state->Closing = true;
		for (const auto& job : state->Jobs) {
			job->Stop.request_stop();
		}
	}

	auto github_profile_catalog::register_profile(const validated_github_host_profile& aProfile) noexcept
		-> std::expected<void, github_provider_configuration_failure>
	{
		try {
			if (!application::valid(profile_authority(aProfile))) {
				return std::unexpected{ github_provider_configuration_failure::invalid_profile };
			}
			auto       entry = std::make_shared<const validated_github_host_profile>(aProfile);
			const auto lock  = std::lock_guard{ Mutex };
			if (Closing) {
				return std::unexpected{ github_provider_configuration_failure::closing };
			}
			for (const auto& profile : Profiles) {
				if (profile->profile() != aProfile.profile()) {
					continue;
				}
				if (profile_authority(*profile) == profile_authority(aProfile)
					&& profile->rest_root() == aProfile.rest_root()
					&& profile->graphql_root() == aProfile.graphql_root()
					&& profile->proxy_mode() == aProfile.proxy_mode()
					&& profile->trust_mode() == aProfile.trust_mode()
					&& profile->provenance() == aProfile.provenance()) {
					return { };
				}
				return std::unexpected{ github_provider_configuration_failure::duplicate_profile };
			}
			if (Profiles.size() >= maximum_github_profile_catalog_entries) {
				return std::unexpected{ github_provider_configuration_failure::collection_limit };
			}
			Profiles.push_back(std::move(entry));
			return { };
		} catch (...) {
			return std::unexpected{ github_provider_configuration_failure::allocation_failure };
		}
	}

	auto github_profile_catalog::find_profile(const core::host_profile_id aIdentity) const noexcept
		-> std::shared_ptr<const validated_github_host_profile>
	{
		const auto lock  = std::lock_guard{ Mutex };
		const auto match = std::ranges::find_if(Profiles,
												[aIdentity](const auto& aProfile) { return aProfile->profile() == aIdentity; });
		return match == Profiles.end() ? nullptr : *match;
	}

	void github_profile_catalog::shutdown() noexcept
	{
		const auto lock = std::lock_guard{ Mutex };
		Closing         = true;
	}

	auto make_github_provider(std::shared_ptr<github_profile_catalog>             aProfiles,
							  application::effect_executor&                       aEffects,
							  std::shared_ptr<github_wire_transport_factory_port> aTransportFactory,
							  const github_provider_read_authority                aReadAuthority,
							  std::shared_ptr<application::diagnostic_log_port>   aLog) noexcept
		-> github_provider_factory_result
	{
		if (!aProfiles || !aTransportFactory) {
			return std::unexpected{ github_provider_configuration_failure::invalid_profile };
		}
		try {
			auto state              = std::make_shared<provider_state>();
			state->Effects          = &aEffects;
			state->TransportFactory = std::move(aTransportFactory);
			state->ReadAuthority    = aReadAuthority;
			state->Log              = std::move(aLog);
			state->Profiles         = std::move(aProfiles);
			auto implementation     = std::make_unique<github_provider::implementation>(std::move(state));
			return std::make_unique<github_provider>(std::move(implementation));
		} catch (...) {
			return std::unexpected{ github_provider_configuration_failure::allocation_failure };
		}
	}

	namespace {
		auto make_github_provider_from_pointers(
			const std::span<const validated_github_host_profile* const> aProfiles,
			application::effect_executor&                               aEffects,
			std::shared_ptr<github_wire_transport_factory_port>         aTransportFactory,
			const github_provider_read_authority                        aReadAuthority,
			std::shared_ptr<application::diagnostic_log_port>           aLog) noexcept
			-> github_provider_factory_result
		{
			if (aProfiles.empty()) {
				return std::unexpected{
					github_provider_configuration_failure::no_profiles
				};
			}
			if (aTransportFactory == nullptr) {
				return std::unexpected{
					github_provider_configuration_failure::invalid_profile
				};
			}
			for (std::size_t index = 0U; index < aProfiles.size(); ++index) {
				if (aProfiles[index] == nullptr
					|| !application::valid(profile_authority(*aProfiles[index]))) {
					return std::unexpected{
						github_provider_configuration_failure::invalid_profile
					};
				}
				const auto previousProfiles = aProfiles.first(index);
				const auto duplicate        = std::ranges::find_if(
					previousProfiles,
					[&](const auto& aProfile) {
						return aProfile != nullptr
							&& aProfile->profile() == aProfiles[index]->profile();
					});
				if (duplicate != previousProfiles.end()) {
					return std::unexpected{
						github_provider_configuration_failure::duplicate_profile
					};
				}
			}
			try {
				auto catalog = std::make_shared<github_profile_catalog>();
				for (const auto* const profile : aProfiles) {
					const auto registered = catalog->register_profile(*profile);
					if (!registered) {
						return std::unexpected{ registered.error() };
					}
				}
				return make_github_provider(std::move(catalog), aEffects,
											std::move(aTransportFactory), aReadAuthority, std::move(aLog));
			} catch (...) {
				return std::unexpected{
					github_provider_configuration_failure::allocation_failure
				};
			}
		}
	}

	auto make_github_provider(
		const std::span<const validated_github_host_profile> aProfiles,
		application::effect_executor&                        aEffects,
		std::shared_ptr<github_wire_transport_factory_port>  aTransportFactory,
		const github_provider_read_authority                 aReadAuthority,
		std::shared_ptr<application::diagnostic_log_port>    aLog) noexcept
		-> github_provider_factory_result
	{
		try {
			auto profilePointers = std::vector<const validated_github_host_profile*>{ };
			profilePointers.reserve(aProfiles.size());
			for (const auto& profile : aProfiles) {
				profilePointers.push_back(&profile);
			}
			return make_github_provider_from_pointers(
				profilePointers, aEffects, std::move(aTransportFactory),
				aReadAuthority, std::move(aLog));
		} catch (...) {
			return std::unexpected{
				github_provider_configuration_failure::allocation_failure
			};
		}
	}

	auto make_github_provider(
		const std::span<const validated_github_host_profile* const> aProfiles,
		application::effect_executor&                               aEffects,
		std::shared_ptr<github_wire_transport_factory_port>         aTransportFactory,
		const github_provider_read_authority                        aReadAuthority,
		std::shared_ptr<application::diagnostic_log_port>           aLog) noexcept
		-> github_provider_factory_result
	{
		return make_github_provider_from_pointers(
			aProfiles, aEffects, std::move(aTransportFactory), aReadAuthority, std::move(aLog));
	}

	auto make_cli_observed_account_transport_for_testing(
		std::shared_ptr<github_wire_transport_port>             aInner,
		const application::github_credential_operation_context& aContext,
		std::string                                             aLogin) -> std::shared_ptr<github_mutation_wire_transport>
	{
		const auto server = aContext.Authority.ServerContract == application::github_server_contract::ghes_3_18_2022_11_28 ? server_contract::ghes_3_18 : server_contract::github_com;
		const auto wire   = compile_read_request(semantic_read_request{ .Server = server, .Operation = read_operation::get_viewer_identity, .Page = 1U });
		if (!wire) {
			return nullptr;
		}
		auto viewer = github_wire_request{
			.Correlation = github_wire_correlation{
				.Parent          = core::github_session_request_context{ .Request = aContext.Request, .ApplicationSession = aContext.ApplicationSession, .Profile = aContext.Authority.Profile, .IdentityGeneration = aContext.IdentityGeneration },
				.ProfileRevision = aContext.Authority.ProfileRevision,
				.Host            = aContext.Authority.Host,
				.Operation       = read_operation::get_viewer_identity,
				.Page            = 1U,
			},
			.Wire = *wire,
		};
		return std::make_shared<cli_observed_account_transport>(std::move(aInner), std::move(viewer), server, aContext.Authority.Account, std::move(aLogin));
	}
}
