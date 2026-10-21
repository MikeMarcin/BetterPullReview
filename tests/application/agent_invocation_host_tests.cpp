/// \file
/// \brief Focused legality, effect-ordering, stale-result, and cleanup proofs.

#include "bpr/application/agent_invocation_host.hpp"

#include <algorithm>
#include <array>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		bytes.fill(aSeed);
		return Identity{ bytes };
	}

	[[nodiscard]] auto commit(const std::uint8_t aSeed) -> core::git_commit_oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		for (auto index = std::size_t{ 0U }; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + index);
		}
		return core::git_commit_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto authority(const core::git_host_id aHost)
		-> core::pull_request_authority
	{
		return {
			.Profile         = core::host_profile_id{ 10U },
			.ProfileRevision = core::github_profile_revision{ 20U },
			.Host            = aHost,
			.HostAuthority   = opaque<core::host_authority_fingerprint>(1U),
			.Account         = core::github_account_id{ 40U },
			.Viewer          = core::github_viewer_actor_id{ 50U },
			.Repository      = core::repository_id{ 101U },
			.Cache           = opaque<core::repository_cache_scope_id>(2U),
			.PullRequest     = core::pull_request_id{ 202U },
			.Number          = core::pull_request_number{ 17U },
			.Scope           = opaque<core::pull_request_authority_scope_id>(3U),
		};
	}

	[[nodiscard]] auto binding(const core::git_host_id aHost)
		-> core::agent_revision_binding
	{
		return {
			.Host        = aHost,
			.Repository  = core::repository_id{ 101U },
			.PullRequest = core::pull_request_id{ 202U },
			.BaseOid     = commit(1U),
			.HeadOid     = commit(2U),
			.LeftEpoch   = core::epoch_ordinal{ 1U },
			.RightEpoch  = core::epoch_ordinal{ 2U },
		};
	}

	[[nodiscard]] auto request(
		const core::git_host_id      aHost,
		const core::agent_profile_id aProfile) -> application::agent_invocation_request
	{
		return {
			.ApplicationSession = core::application_session_id{ 1U },
			.Authority          = authority(aHost),
			.Profile            = aProfile,
			.Context            = {
				.ApplicationSession = core::application_session_id{ 1U },
				.Binding            = binding(aHost),
				.PullRequestUrl     = "https://github.example/octo/review/pull/17",
			},
		};
	}

	class session_catalog_spy final
		: public application::agent_session_catalog_port
	{
	public:
		/// [[BPR-1211]]. Answers on `aLane` when one is given, as the durable
		/// catalog does; inline otherwise.
		explicit session_catalog_spy(application::decision_executor* aLane = nullptr)
			: Lane{ aLane }
		{
		}

		void admit(
			const application::agent_session_admission&                                                aAdmission,
			std::function<void(std::expected<core::agent_session, application::ledger_store_failure>)> aDone) override
		{
			++AdmissionAttempts;
			auto answer = std::expected<core::agent_session, application::ledger_store_failure>{ };
			if (AdmissionFailure.has_value()) {
				answer = std::unexpected(*AdmissionFailure);
			} else {
				Latest = core::agent_session{
					.Id      = core::agent_session_id{ Next++ },
					.Profile = aAdmission.Profile,
					.Binding = aAdmission.Binding,
				};
				answer = *Latest;
			}
			deliver([done = std::move(aDone), answer] { done(answer); });
		}

		void session_catalog(
			std::function<void(std::expected<application::agent_session_catalog_state, application::ledger_store_failure>)> aDone) override
		{
			++Loads;
			const auto answer = application::agent_session_catalog_state{
				.Latest = Latest, .Next = core::agent_session_id{ Next }
			};
			deliver([done = std::move(aDone), answer] { done(answer); });
		}

		std::uint64_t                                    Next              = 41U;
		std::size_t                                      AdmissionAttempts = 0U;
		std::size_t                                      Loads             = 0U;
		std::optional<application::ledger_store_failure> AdmissionFailure;
		std::optional<core::agent_session>               Latest;

	private:
		void deliver(std::function<void()> aAnswer)
		{
			if (Lane != nullptr) {
				Lane->post(std::move(aAnswer));
			} else {
				aAnswer();
			}
		}

		application::decision_executor* Lane = nullptr;
	};

	class queued_executor final
		: public application::effect_executor,
		  public application::decision_executor
	{
	public:
		void post(std::function<void()> aTask) override
		{
			Tasks.push_back(std::move(aTask));
		}

		void drain()
		{
			while (!Tasks.empty()) {
				auto task = std::move(Tasks.front());
				Tasks.pop_front();
				task();
			}
		}

		[[nodiscard]] auto empty() const noexcept -> bool
		{
			return Tasks.empty();
		}

	private:
		std::deque<std::function<void()>> Tasks;
	};

	class threaded_effect_executor final : public application::effect_executor
	{
	public:
		void post(std::function<void()> aTask) override
		{
			Threads.emplace_back(std::move(aTask));
		}

		void join()
		{
			Threads.clear();
		}

	private:
		std::vector<std::jthread> Threads;
	};

	class workspace_spy final : public application::agent_workspace_port
	{
	public:
		[[nodiscard]] auto materialize_agent_workspace(
			const application::agent_workspace_materialization_request& aRequest,
			const std::stop_token                                       aStop)
			-> std::expected<application::owned_agent_workspace,
							 application::agent_workspace_error> override
		{
			++Starts;
			LastRequest = aRequest;
			if (MaterializationFailure.has_value()) {
				return std::unexpected{ *MaterializationFailure };
			}
			if (aStop.stop_requested()) {
				return std::unexpected{ application::agent_workspace_failure::cancelled };
			}
			// [[BPR-1262]]. The nested workspace names its type: braced without
			// it, an aggregate holding a std::string is the shape GCC 15
			// destroys twice when a later member throws ([[BPR-0816]]).
			auto result = application::owned_agent_workspace{
				.Workspace = application::prepared_workspace{
					.Policy     = core::agent_workspace_policy::disposable_review_workspace,
					.PinnedHead = aRequest.Session.Binding.HeadOid,
					.Path       = "/tmp/bpr-agent-owned/exact-head",
				},
				.ApplicationSession = aRequest.ApplicationSession,
				.Authority          = aRequest.Authority,
				.Session            = aRequest.Session.Id,
				.ParentDirectory    = aRequest.ParentDirectory,
				.OwnershipMarker    = "owned",
				.DirectoryIdentity  = { .Opaque = { 2U, 1U, 0U, 1U } },
				.LifetimeGuard      = Guard,
			};
			if (AfterMaterialization) {
				AfterMaterialization();
			}
			return result;
		}

		[[nodiscard]] auto cleanup_agent_workspace(
			const application::owned_agent_workspace& aWorkspace,
			const std::stop_token)
			-> std::expected<void, application::agent_workspace_error> override
		{
			++Cleanups;
			LastCleanup = aWorkspace;
			CleanupAttempts.push_back(aWorkspace);
			if (FailNextCleanup) {
				FailNextCleanup = false;
				return std::unexpected{
					application::agent_workspace_failure::cleanup_failed
				};
			}
			return { };
		}

		std::size_t                                                         Starts   = 0U;
		std::size_t                                                         Cleanups = 0U;
		std::optional<application::agent_workspace_lifetime_guard>          Guard;
		std::optional<application::agent_workspace_materialization_request> LastRequest;
		std::optional<application::owned_agent_workspace>                   LastCleanup;
		std::vector<application::owned_agent_workspace>                     CleanupAttempts;
		std::function<void()>                                               AfterMaterialization;
		std::optional<application::agent_workspace_error>                   MaterializationFailure;
		bool                                                                FailNextCleanup = false;
	};

	class process_spy final : public application::agent_process_port
	{
	public:
		[[nodiscard]] auto run(
			const application::agent_launch_request& aRequest,
			const int,
			const application::agent_output_sink& aSink,
			const std::stop_token                 aStop)
			-> application::agent_run_result override
		{
			++Starts;
			LastRequest       = aRequest;
			LastStopRequested = aStop.stop_requested();
			if (AfterStart) {
				AfterStart();
				LastStopRequested = aStop.stop_requested();
			}
			if (LastStopRequested) {
				return {
					.Session = aRequest.Session,
					.Outcome = application::agent_run_outcome::cancelled,
				};
			}
			auto event = application::agent_stream_event{
				.Session = aRequest.Session,
				.Text    = "bounded result",
			};
			if (aSink) {
				aSink(event);
			}
			return {
				.Session  = aRequest.Session,
				.Outcome  = application::agent_run_outcome::success,
				.Events   = { event },
				.ExitCode = 0,
				.Findings = Payload,
			};
		}

		void cancel() noexcept override
		{
			++Cancels;
		}

		std::size_t                                      Starts            = 0U;
		std::size_t                                      Cancels           = 0U;
		bool                                             LastStopRequested = false;
		std::optional<application::agent_launch_request> LastRequest;
		std::function<void()>                            AfterStart;
		/// What a completed turn returned, as the shared loop decoded it.
		std::optional<application::agent_finding_payload> Payload;
	};

	/// \brief Records every durable authority agent output could try to reach.
	class ledger_store_spy final : public application::review_ledger_store_port
	{
	public:
		[[nodiscard]] auto commit(const application::receipt_persistence_request&)
			-> std::expected<void, application::ledger_store_failure> override
		{
			Touched.emplace_back("commit");
			return { };
		}

		[[nodiscard]] auto load(const core::file_identity&) -> std::expected<
			std::optional<core::file_receipt>,
			application::ledger_store_failure> override
		{
			Touched.emplace_back("load");
			return std::optional<core::file_receipt>{ };
		}

		[[nodiscard]] auto store_conversation_receipt(const core::conversation_receipt&)
			-> std::expected<void, application::ledger_store_failure> override
		{
			Touched.emplace_back("store_conversation_receipt");
			return { };
		}

		[[nodiscard]] auto load_conversation_receipts() -> std::expected<
			std::vector<core::conversation_receipt>,
			application::ledger_store_failure> override
		{
			Touched.emplace_back("load_conversation_receipts");
			return std::vector<core::conversation_receipt>{ };
		}

		[[nodiscard]] auto store_note(const application::local_note_record&           aNote,
									  std::optional<application::local_note_revision> aExpectedRevision = std::nullopt)
			-> std::expected<application::local_note_record, application::local_note_failure> override
		{
			Touched.emplace_back("store_note");
			if (Refusal.has_value()) {
				return std::unexpected(application::local_note_failure{ .Reason = *Refusal });
			}
			const auto found = std::ranges::find(Notes, aNote.Id, &application::local_note_record::Id);
			if ((found == Notes.end() && aExpectedRevision) || (found != Notes.end() && (!aExpectedRevision || found->Revision != *aExpectedRevision))) {
				return std::unexpected(application::local_note_failure{ .Reason  = application::ledger_store_failure::conflict,
																		.Current = found == Notes.end() ? std::nullopt : std::optional{ *found } });
			}
			auto accepted     = aNote;
			accepted.Revision = application::local_note_revision{ NextNoteRevision++ };
			if (found == Notes.end()) {
				Notes.push_back(accepted);
			} else {
				*found = accepted;
			}
			return accepted;
		}

		[[nodiscard]] auto erase_note(application::local_note_id aNote, application::local_note_revision aExpectedRevision)
			-> std::expected<void, application::local_note_failure> override
		{
			Touched.emplace_back("erase_note");
			const auto found = std::ranges::find(Notes, aNote, &application::local_note_record::Id);
			if (found == Notes.end() || found->Revision != aExpectedRevision) {
				return std::unexpected(application::local_note_failure{ .Reason  = application::ledger_store_failure::conflict,
																		.Current = found == Notes.end() ? std::nullopt : std::optional{ *found } });
			}
			Notes.erase(found);
			return { };
		}

		[[nodiscard]] auto store_last_reviewed_head(const core::git_commit_oid&)
			-> std::expected<void, application::ledger_store_failure> override
		{
			Touched.emplace_back("store_last_reviewed_head");
			return { };
		}

		[[nodiscard]] auto erase_last_reviewed_head()
			-> std::expected<void, application::ledger_store_failure> override
		{
			Touched.emplace_back("erase_last_reviewed_head");
			return { };
		}

		[[nodiscard]] auto store_defect(const application::durable_defect_record&)
			-> std::expected<void, application::ledger_store_failure> override
		{
			Touched.emplace_back("store_defect");
			return { };
		}

		[[nodiscard]] auto erase_defect(core::defect_id)
			-> std::expected<void, application::ledger_store_failure> override
		{
			Touched.emplace_back("erase_defect");
			return { };
		}

		[[nodiscard]] auto store_generated_mark(const core::generated_file_mark&)
			-> std::expected<void, application::ledger_store_failure> override
		{
			Touched.emplace_back("store_generated_mark");
			return { };
		}

		[[nodiscard]] auto erase_generated_mark(const core::file_identity&)
			-> std::expected<void, application::ledger_store_failure> override
		{
			Touched.emplace_back("erase_generated_mark");
			return { };
		}

		[[nodiscard]] auto load_private() -> std::expected<
			application::private_ledger_observation,
			application::ledger_store_failure> override
		{
			Touched.emplace_back("load_private");
			return application::private_ledger_observation{ };
		}

		[[nodiscard]] auto admit_agent_session(
			const application::agent_session_admission&)
			-> std::expected<core::agent_session, application::ledger_store_failure> override
		{
			Touched.emplace_back("admit_agent_session");
			return std::unexpected(
				application::ledger_store_failure::unavailable);
		}

		[[nodiscard]] auto load_agent_session_catalog() -> std::expected<
			application::agent_session_catalog_state,
			application::ledger_store_failure> override
		{
			Touched.emplace_back("load_agent_session_catalog");
			return application::agent_session_catalog_state{ };
		}

		std::vector<std::string>                         Touched;
		std::vector<application::local_note_record>      Notes;
		std::uint64_t                                    NextNoteRevision = 1U;
		std::optional<application::ledger_store_failure> Refusal;
	};

	class blocking_process_spy final : public application::agent_process_port
	{
	public:
		[[nodiscard]] auto run(
			const application::agent_launch_request& aRequest,
			const int,
			const application::agent_output_sink&,
			const std::stop_token aStop)
			-> application::agent_run_result override
		{
			auto lock = std::unique_lock{ Mutex };
			Started   = true;
			Changed.notify_all();
			auto wake = std::stop_callback{ aStop, [this] {
											   Changed.notify_all();
										   } };
			Changed.wait(lock, [aStop] {
				return aStop.stop_requested();
			});
			return {
				.Session = aRequest.Session,
				.Outcome = application::agent_run_outcome::cancelled,
			};
		}

		void cancel() noexcept override
		{
			Changed.notify_all();
		}

		void wait_started()
		{
			auto lock = std::unique_lock{ Mutex };
			Changed.wait(lock, [this] {
				return Started;
			});
		}

	private:
		std::mutex              Mutex;
		std::condition_variable Changed;
		bool                    Started = false;
	};

	class projection_spy final
		: public application::agent_invocation_projection_port
	{
	public:
		void offer(const std::span<const core::agent_profile> aProfiles) override
		{
			if (ThrowOffer) {
				throw std::runtime_error{ "offer" };
			}
			Offered.assign(aProfiles.begin(), aProfiles.end());
		}

		void refuse(const std::string_view aReason) override
		{
			if (ThrowRefuse) {
				throw std::runtime_error{ "refuse" };
			}
			Refusals.emplace_back(aReason);
		}

		void admit(
			const application::agent_invocation_projection_identity& aIdentity) override
		{
			if (ThrowAdmit) {
				throw std::runtime_error{ "admit" };
			}
			AdmittedIdentities.push_back(aIdentity);
		}

		void start(
			const application::agent_invocation_projection_identity& aIdentity,
			const application::prepared_workspace&                   aWorkspace) override
		{
			if (ThrowStart) {
				throw std::runtime_error{ "start" };
			}
			StartedIdentity = aIdentity;
			Started         = aWorkspace;
		}

		void stream(
			const application::agent_invocation_projection_identity& aIdentity,
			const application::agent_stream_event&                   aEvent) override
		{
			if (ThrowStream) {
				throw std::runtime_error{ "stream" };
			}
			StreamIdentities.push_back(aIdentity);
			Streamed.push_back(aEvent);
		}

		void finish(
			const application::agent_invocation_projection_identity& aIdentity,
			const application::agent_run_result&                     aResult) override
		{
			if (ThrowFinish) {
				throw std::runtime_error{ "finish" };
			}
			FinishIdentities.push_back(aIdentity);
			Finished.push_back(aResult);
			if (AfterFinish) {
				AfterFinish();
			}
		}

		void deliver_findings(
			const application::agent_invocation_projection_identity& aIdentity,
			const application::agent_finding_delivery&               aDelivery) override
		{
			DeliveryIdentities.push_back(aIdentity);
			Deliveries.push_back(aDelivery);
		}

		void present_session_catalog(
			const application::agent_session_catalog_projection& aProjection)
			override
		{
			SessionCatalogs.push_back(aProjection);
		}

		std::vector<core::agent_profile> Offered;
		std::vector<std::string>         Refusals;
		std::vector<application::agent_invocation_projection_identity>
													   AdmittedIdentities;
		std::optional<application::prepared_workspace> Started;
		std::optional<application::agent_invocation_projection_identity>
			StartedIdentity;
		std::vector<application::agent_invocation_projection_identity>
			StreamIdentities;
		std::vector<application::agent_invocation_projection_identity>
													 FinishIdentities;
		std::vector<application::agent_stream_event> Streamed;
		std::vector<application::agent_run_result>   Finished;
		std::vector<application::agent_invocation_projection_identity>
														 DeliveryIdentities;
		std::vector<application::agent_finding_delivery> Deliveries;
		std::vector<application::agent_session_catalog_projection>
							  SessionCatalogs;
		std::function<void()> AfterFinish;
		bool                  ThrowOffer  = false;
		bool                  ThrowRefuse = false;
		bool                  ThrowAdmit  = false;
		bool                  ThrowStart  = false;
		bool                  ThrowStream = false;
		bool                  ThrowFinish = false;
	};

	struct harness
	{
		queued_executor                    Effects;
		queued_executor                    Decisions;
		workspace_spy                      Workspaces;
		process_spy                        Processes;
		projection_spy                     Projection;
		session_catalog_spy                Sessions{ &Decisions };
		std::size_t                        EnvironmentReads = 0U;
		application::agent_invocation_host Host{
			core::built_in_agent_profiles(),
			{
				core::built_in_agent_policy(
					core::git_host_id{ 1U },
					core::built_in_agent_host_kind::github_com),
				core::built_in_agent_policy(
					core::git_host_id{ 2U },
					core::built_in_agent_host_kind::explicit_ghes),
			},
			"/tmp/bpr-agent-owned",
			Sessions,
			Workspaces,
			Processes,
			[this](const std::string& aName) -> std::optional<std::string> {
				++EnvironmentReads;
				if (aName == "PATH") {
					return "/usr/bin:/bin";
				}
				if (aName == "GITHUB_TOKEN") {
					return "must-not-cross";
				}
				return std::nullopt;
			},
			Effects,
			Decisions,
			Projection,
		};

		void settle()
		{
			for (auto turn = 0; turn < 4; ++turn) {
				Decisions.drain();
				Effects.drain();
			}
			expect(Effects.empty() && Decisions.empty(), "the test host settles");
		}
	};

	void test_explicit_settings_cross_the_existing_invocation_host()
	{
		const auto hostId = core::git_host_id{ 1 };
		for (const auto profileId : { core::codex_app_server_profile_id, core::codex_jsonl_profile_id }) {
			auto test = harness{ };
			test.Host.observe(core::application_session_id{ 1 }, authority(hostId), binding(hostId));
			auto input              = request(hostId, profileId);
			input.RequestedSettings = { "requested-model", "high" };
			test.Host.dispatch(input);
			test.settle();
			if (profileId == core::codex_app_server_profile_id) {
				expect(test.Processes.Starts == 1 && test.Processes.LastRequest->Metadata.Requested == input.RequestedSettings,
					   "production invocation host passes explicitly requested settings to its existing launcher");
			} else {
				expect(test.Processes.Starts == 0 && !test.Projection.Finished.empty(), "unsupported setting reaches a refusal with no process start");
			}
			expect(test.Host.is_quiescent(), "settings refusal/success finishes owned work");
		}
	}

	void test_ghes_policy_refuses_codex_before_effects()
	{
		auto test = harness{ };
		test.Host.observe(
			core::application_session_id{ 1U },
			authority(core::git_host_id{ 2U }),
			binding(core::git_host_id{ 2U }));
		test.Host.dispatch(request(
			core::git_host_id{ 2U }, core::codex_jsonl_profile_id));
		expect(
			!test.Projection.Refusals.empty()
				&& test.Workspaces.Starts == 0U && test.Processes.Starts == 0U
				&& test.Sessions.AdmissionAttempts == 0U
				&& test.Host.statistics().CoreDecisions == 1U,
			"explicit GHES Codex policy is decided by core before any workspace or process effect");
	}

	void test_context_refusal_precedes_durable_admission()
	{
		auto test = harness{ };
		test.Host.observe(
			core::application_session_id{ 1U },
			authority(core::git_host_id{ 1U }),
			binding(core::git_host_id{ 1U }));
		auto invalid = request(
			core::git_host_id{ 1U }, core::external_terminal_profile_id);
		invalid.Context.IncludedNotes = { application::local_note_id{ 99U } };
		test.Host.dispatch(std::move(invalid));
		expect(
			test.Sessions.AdmissionAttempts == 0U
				&& test.Workspaces.Starts == 0U && test.Processes.Starts == 0U
				&& test.EnvironmentReads == 0U
				&& test.Projection.Refusals.back() == "invalid_agent_context",
			"exact context refusal performs zero durable writes and zero downstream effects");
	}

	void test_durable_admission_refusal_precedes_every_downstream_effect()
	{
		auto test = harness{ };
		test.Sessions.AdmissionFailure
			= application::ledger_store_failure::quota_exceeded;
		test.Host.observe(
			core::application_session_id{ 1U },
			authority(core::git_host_id{ 1U }),
			binding(core::git_host_id{ 1U }));
		test.Host.dispatch(request(
			core::git_host_id{ 1U }, core::external_terminal_profile_id));
		expect(test.Projection.Refusals.empty() && test.Sessions.AdmissionAttempts == 1U,
			   "[[BPR-1211]] durable admission answers on the decision lane, never inside the dispatch");
		test.Decisions.drain();
		expect(
			test.Sessions.AdmissionAttempts == 1U
				&& test.Workspaces.Starts == 0U && test.Processes.Starts == 0U
				&& test.EnvironmentReads == 0U
				&& test.Projection.Refusals.back()
					== "agent_session_admission_quota_exceeded"
				&& test.Host.statistics().SessionAdmissionFailures == 1U,
			"typed durable admission refusal is visible before workspace, environment, executable, or process work");
	}

	void test_latest_retained_session_uses_every_core_classification_without_retargeting()
	{
		auto       test            = harness{ };
		const auto retainedBinding = binding(core::git_host_id{ 1U });
		test.Sessions.Latest       = core::agent_session{
			.Id      = core::agent_session_id{ 40U },
			.Profile = core::external_terminal_profile_id,
			.Binding = retainedBinding,
		};
		auto observations = std::array{
			retainedBinding,
			retainedBinding,
			retainedBinding,
			retainedBinding,
		};
		observations[1].HeadOid    = commit(9U);
		observations[2].RightEpoch = core::epoch_ordinal{ 9U };
		observations[3].Repository = core::repository_id{ 999U };
		const auto expected        = std::array{
			core::agent_session_staleness::current,
			core::agent_session_staleness::head_moved,
			core::agent_session_staleness::epoch_pair_changed,
			core::agent_session_staleness::different_review,
		};
		for (auto index = std::size_t{ 0U }; index < expected.size(); ++index) {
			test.Host.observe(
				core::application_session_id{ 1U },
				authority(core::git_host_id{ 1U }),
				observations[index]);
			test.Decisions.drain();
			const auto& projected = test.Projection.SessionCatalogs.back();
			expect(
				projected.LatestStatus.has_value()
					&& projected.LatestStatus->Staleness == expected[index]
					&& projected.Latest->Binding == retainedBinding,
				"catalog presentation classifies current state without changing stored binding");
		}
	}

	void test_valid_dispatch_materializes_before_process_and_cleans_after_terminal()
	{
		auto test             = harness{ };
		test.Workspaces.Guard = application::agent_workspace_lifetime_guard{ .Path = "/tmp/fixture-ownership.jsonl", .FileIdentity = { 1U, 2U, 0U, 3U } };
		test.Host.observe(
			core::application_session_id{ 1U },
			authority(core::git_host_id{ 1U }),
			binding(core::git_host_id{ 1U }));
		test.Host.dispatch(request(
			core::git_host_id{ 1U }, core::external_terminal_profile_id));
		test.settle();
		expect(
			test.Workspaces.Starts == 1U && test.Processes.Starts == 1U
				&& test.Workspaces.Cleanups == 1U
				&& test.Projection.Finished.size() == 1U
				&& test.Projection.Finished.front().Outcome
					== application::agent_run_outcome::success,
			"one permitted gesture has one materialization, launch, terminal, and cleanup");
		expect(
			test.Sessions.AdmissionAttempts == 1U
				&& test.Workspaces.LastRequest->Session.Id
					== core::agent_session_id{ 41U }
				&& test.Processes.LastRequest->Session
					== core::agent_session_id{ 41U },
			"the durable allocator identity is reused by every later effect");
		const auto& launch = *test.Processes.LastRequest;
		expect(launch.WorkspaceLifetimeGuard == test.Workspaces.Guard
				   && test.Workspaces.LastCleanup->LifetimeGuard == test.Workspaces.Guard,
			   "exact lifetime guard survives materialization, launch and cleanup unchanged");
		expect(
			launch.WorkingDirectory == "/tmp/bpr-agent-owned/exact-head"
				&& launch.Input.find("github.example/octo/review/pull/17")
					!= std::string::npos
				&& std::ranges::none_of(launch.Arguments, [](const std::string& aArgument) {
					   return aArgument.find("github.example") != std::string::npos;
				   })
				&& std::ranges::none_of(launch.Environment, [](const auto& aEntry) {
					   return aEntry.first == "GITHUB_TOKEN";
				   }),
			"working directory is assigned after materialization while context stays on stdin and secrets stay out");
	}

	void test_revision_movement_discards_result_and_still_cleans()
	{
		auto       test            = harness{ };
		const auto activeAuthority = authority(core::git_host_id{ 1U });
		test.Host.observe(
			core::application_session_id{ 1U },
			activeAuthority,
			binding(core::git_host_id{ 1U }));
		test.Host.dispatch(request(
			core::git_host_id{ 1U }, core::external_terminal_profile_id));
		test.Decisions.drain();
		test.Effects.drain();
		auto moved    = binding(core::git_host_id{ 1U });
		moved.HeadOid = commit(9U);
		test.Host.observe(core::application_session_id{ 1U }, activeAuthority, moved);
		test.settle();
		expect(
			test.Projection.Finished.size() == 1U
				&& test.Projection.Finished.front().Outcome
					== application::agent_run_outcome::stale_result
				&& test.Projection.Finished.front().Events.empty()
				&& test.Workspaces.Cleanups == 1U && test.Host.is_quiescent(),
			"revision movement clears the old answer but does not leak its owned workspace");
		expect(
			!test.Projection.SessionCatalogs.empty()
				&& test.Projection.SessionCatalogs.back().Latest->Id
					== core::agent_session_id{ 41U }
				&& test.Projection.SessionCatalogs.back().LatestStatus->Staleness
					== core::agent_session_staleness::head_moved,
			"latest retained metadata is presented as stale without retargeting its binding");
	}

	void test_superseded_dominates_mixed_cancellation_and_head_movement()
	{
		auto       test            = harness{ };
		const auto activeAuthority = authority(core::git_host_id{ 1U });
		test.Host.observe(
			core::application_session_id{ 1U },
			activeAuthority,
			binding(core::git_host_id{ 1U }));
		test.Workspaces.AfterMaterialization = [&test, activeAuthority] {
			test.Host.cancel();
			auto moved    = binding(core::git_host_id{ 1U });
			moved.HeadOid = commit(9U);
			test.Host.observe(
				core::application_session_id{ 1U },
				activeAuthority,
				moved);
		};
		test.Host.dispatch(request(
			core::git_host_id{ 1U }, core::external_terminal_profile_id));
		test.settle();
		expect(
			test.Projection.Finished.size() == 1U
				&& test.Projection.Finished.front().Outcome
					== application::agent_run_outcome::stale_result
				&& test.Workspaces.Cleanups == 1U && test.Host.is_quiescent(),
			"head movement dominates mixed cancellation so an old answer is always stale");
	}

	void test_prestart_workspace_failure_is_an_admitted_typed_terminal()
	{
		auto       test       = harness{ };
		const auto diagnostic = std::string{
			"fatal: could not materialize /tmp/bpr-agent-workspace from "
			"<redacted-url>\n"
		};
		test.Workspaces.MaterializationFailure = application::agent_workspace_error{
			application::agent_workspace_failure::unavailable,
			diagnostic,
		};
		test.Host.observe(
			core::application_session_id{ 1U },
			authority(core::git_host_id{ 1U }),
			binding(core::git_host_id{ 1U }));
		test.Host.dispatch(request(
			core::git_host_id{ 1U }, core::external_terminal_profile_id));
		test.settle();
		expect(
			test.Projection.AdmittedIdentities.size() == 1U
				&& test.Projection.StartedIdentity == std::nullopt
				&& test.Projection.Finished.size() == 1U
				&& test.Projection.Finished.front().Outcome
					== application::agent_run_outcome::workspace_unavailable
				&& test.Projection.Finished.front().StandardError == diagnostic
				&& test.Processes.Starts == 0U && test.Host.is_quiescent(),
			"workspace failure retains identity and diagnostic through its typed terminal");
		// BPR-0348. `Starts == 0U` above already says no executable was reached,
		// and the terminal must say the same thing. It used to answer
		// `failed_to_start`, which sends whoever reads it to look at the agent
		// for a failure that happened in the cache several layers earlier.
		expect(
			test.Projection.Finished.front().Outcome
				!= application::agent_run_outcome::failed_to_start,
			"a workspace that could not be materialized is not a launch failure");
	}

	void test_projection_exceptions_never_escape_or_skip_cleanup()
	{
		{
			auto test                  = harness{ };
			test.Projection.ThrowOffer = true;
			test.Host.observe(
				core::application_session_id{ 1U },
				authority(core::git_host_id{ 1U }),
				binding(core::git_host_id{ 1U }));
		}
		{
			auto test                   = harness{ };
			test.Projection.ThrowRefuse = true;
			test.Host.observe(
				core::application_session_id{ 1U },
				authority(core::git_host_id{ 2U }),
				binding(core::git_host_id{ 2U }));
			test.Host.dispatch(request(
				core::git_host_id{ 2U }, core::codex_jsonl_profile_id));
			expect(
				test.Workspaces.Starts == 0U && test.Processes.Starts == 0U
					&& test.Host.is_quiescent(),
				"throwing refusal is contained before effects");
		}

		for (const auto phase : { 0, 1, 2, 3 }) {
			auto test                   = harness{ };
			test.Projection.ThrowAdmit  = phase == 0;
			test.Projection.ThrowStart  = phase == 1;
			test.Projection.ThrowStream = phase == 2;
			test.Projection.ThrowFinish = phase == 3;
			test.Host.observe(
				core::application_session_id{ 1U },
				authority(core::git_host_id{ 1U }),
				binding(core::git_host_id{ 1U }));
			test.Host.dispatch(request(
				core::git_host_id{ 1U }, core::external_terminal_profile_id));
			test.settle();
			expect(
				test.Workspaces.Cleanups == 1U && test.Host.is_quiescent(),
				"throwing admitted/start/stream/finish projection cannot skip exact cleanup");
		}
	}

	void test_cancel_and_shutdown_before_effect_create_no_workspace_or_process()
	{
		for (const auto shutdown : { false, true }) {
			auto test = harness{ };
			test.Host.observe(
				core::application_session_id{ 1U },
				authority(core::git_host_id{ 1U }),
				binding(core::git_host_id{ 1U }));
			test.Host.dispatch(request(
				core::git_host_id{ 1U }, core::external_terminal_profile_id));
			shutdown ? test.Host.shutdown() : test.Host.cancel();
			test.settle();
			expect(
				test.Processes.Starts == 0U && test.Workspaces.Cleanups == 0U
					&& test.Projection.Finished.size() == 1U
					&& test.Projection.Finished.front().Outcome
						== application::agent_run_outcome::cancelled
					&& test.Host.is_quiescent(),
				"pre-effect cancellation and shutdown create no child or workspace to leak");
		}
	}

	void test_cancellation_is_unlosable_at_each_prelaunch_boundary_and_recovers()
	{
		auto test = harness{ };
		test.Host.observe(
			core::application_session_id{ 1U },
			authority(core::git_host_id{ 1U }),
			binding(core::git_host_id{ 1U }));

		auto existing = request(
			core::git_host_id{ 1U }, core::external_terminal_profile_id);
		existing.ExistingCheckoutChosen = true;
		existing.ExistingCheckout       = application::checkout_observation{
			.Path        = "/tmp/reviewer-explicit-checkout",
			.Cleanliness = application::checkout_cleanliness::unknown,
		};
		test.Host.dispatch(existing);
		test.Host.cancel();
		test.settle();
		expect(
			test.Processes.Starts == 0U
				&& test.Projection.Finished.back().Outcome
					== application::agent_run_outcome::cancelled,
			"existing-checkout cancellation before its queued launch is retained");

		test.Workspaces.AfterMaterialization = [&test] {
			test.Host.cancel();
		};
		test.Host.dispatch(request(
			core::git_host_id{ 1U }, core::external_terminal_profile_id));
		test.settle();
		expect(
			test.Processes.Starts == 0U && test.Workspaces.Cleanups == 1U
				&& test.Projection.Finished.back().Outcome
					== application::agent_run_outcome::cancelled,
			"post-materialization cancellation cleans before process launch");

		test.Workspaces.AfterMaterialization = { };
		test.Host.dispatch(request(
			core::git_host_id{ 1U }, core::external_terminal_profile_id));
		test.settle();
		expect(
			test.Processes.Starts == 1U
				&& test.Projection.Finished.back().Outcome
					== application::agent_run_outcome::success,
			"a later invocation is not poisoned by an earlier cancellation");
	}

	void test_process_cancellation_uses_the_exact_invocation_token_and_recovers()
	{
		auto test = harness{ };
		test.Host.observe(
			core::application_session_id{ 1U },
			authority(core::git_host_id{ 1U }),
			binding(core::git_host_id{ 1U }));
		test.Processes.AfterStart = [&test] {
			test.Host.cancel();
		};
		test.Host.dispatch(request(
			core::git_host_id{ 1U }, core::external_terminal_profile_id));
		test.settle();
		expect(
			test.Processes.LastStopRequested
				&& test.Projection.Finished.back().Outcome
					== application::agent_run_outcome::cancelled
				&& test.Workspaces.Cleanups == 1U,
			"running process observes the stop token for its own invocation");

		test.Processes.AfterStart = { };
		test.Host.dispatch(request(
			core::git_host_id{ 1U }, core::external_terminal_profile_id));
		test.settle();
		expect(
			!test.Processes.LastStopRequested
				&& test.Projection.Finished.back().Outcome
					== application::agent_run_outcome::success,
			"process cancellation generation permits a later recovery run");
	}

	void test_projection_identity_and_queued_destruction_are_safe()
	{
		auto effects    = queued_executor{ };
		auto decisions  = queued_executor{ };
		auto workspaces = workspace_spy{ };
		auto processes  = process_spy{ };
		auto projection = projection_spy{ };
		auto sessions   = session_catalog_spy{ };
		{
			auto host = std::make_unique<application::agent_invocation_host>(
				core::built_in_agent_profiles(),
				std::vector{
					core::built_in_agent_policy(
						core::git_host_id{ 1U },
						core::built_in_agent_host_kind::github_com),
				},
				"/tmp/bpr-agent-owned",
				sessions,
				workspaces,
				processes,
				[](const std::string&) -> std::optional<std::string> {
					return std::nullopt;
				},
				effects,
				decisions,
				projection);
			host->observe(
				core::application_session_id{ 1U },
				authority(core::git_host_id{ 1U }),
				binding(core::git_host_id{ 1U }));
			host->dispatch(request(
				core::git_host_id{ 1U }, core::external_terminal_profile_id));
		}
		effects.drain();
		decisions.drain();
		expect(
			workspaces.Starts == 0U && processes.Starts == 0U
				&& projection.Finished.empty(),
			"queued callbacks weak-no-op after host destruction without touching dead ports");

		auto live = harness{ };
		live.Host.observe(
			core::application_session_id{ 1U },
			authority(core::git_host_id{ 1U }),
			binding(core::git_host_id{ 1U }));
		live.Host.dispatch(request(
			core::git_host_id{ 1U }, core::external_terminal_profile_id));
		live.settle();
		expect(
			live.Projection.StartedIdentity.has_value()
				&& live.Projection.StreamIdentities
					== std::vector{ *live.Projection.StartedIdentity }
				&& live.Projection.FinishIdentities
					== std::vector{ *live.Projection.StartedIdentity },
			"start, stream, and finish share one exact serialized invocation identity");
	}

	void test_destruction_cancels_running_work_before_ports_can_die()
	{
		auto effects    = threaded_effect_executor{ };
		auto decisions  = queued_executor{ };
		auto workspaces = workspace_spy{ };
		auto processes  = blocking_process_spy{ };
		auto projection = projection_spy{ };
		auto sessions   = session_catalog_spy{ };
		auto host       = std::make_unique<application::agent_invocation_host>(
			core::built_in_agent_profiles(),
			std::vector{
				core::built_in_agent_policy(
					core::git_host_id{ 1U },
					core::built_in_agent_host_kind::github_com),
			},
			"/tmp/bpr-agent-owned",
			sessions,
			workspaces,
			processes,
			[](const std::string&) -> std::optional<std::string> {
				return std::nullopt;
			},
			effects,
			decisions,
			projection);
		host->observe(
			core::application_session_id{ 1U },
			authority(core::git_host_id{ 1U }),
			binding(core::git_host_id{ 1U }));
		host->dispatch(request(
			core::git_host_id{ 1U }, core::external_terminal_profile_id));
		decisions.drain();
		processes.wait_started();
		host.reset();
		effects.join();
		decisions.drain();
		expect(
			workspaces.Starts == 1U && workspaces.Cleanups == 1U
				&& projection.Finished.empty(),
			"destruction waits for running callback exit and exact-cleans before ports die");
	}

	void test_reentrant_projection_destruction_never_waits_on_itself()
	{
		auto effects    = queued_executor{ };
		auto decisions  = queued_executor{ };
		auto workspaces = workspace_spy{ };
		auto processes  = process_spy{ };
		auto projection = projection_spy{ };
		auto sessions   = session_catalog_spy{ };
		auto host       = std::make_unique<application::agent_invocation_host>(
			core::built_in_agent_profiles(),
			std::vector{
				core::built_in_agent_policy(
					core::git_host_id{ 1U },
					core::built_in_agent_host_kind::github_com),
			},
			"/tmp/bpr-agent-owned",
			sessions,
			workspaces,
			processes,
			[](const std::string&) -> std::optional<std::string> {
				return std::nullopt;
			},
			effects,
			decisions,
			projection);
		projection.AfterFinish = [&host] {
			host.reset();
		};
		host->observe(
			core::application_session_id{ 1U },
			authority(core::git_host_id{ 1U }),
			binding(core::git_host_id{ 1U }));
		host->dispatch(request(
			core::git_host_id{ 1U }, core::external_terminal_profile_id));
		decisions.drain();
		effects.drain();
		decisions.drain();
		effects.drain();
		expect(
			!host && workspaces.Cleanups == 1U && effects.empty()
				&& projection.Finished.size() == 1U,
			"finish projection may destroy its host without self-wait or post-destroy effects");
	}

	void test_cleanup_refusal_retains_exact_authority_and_nonquiescence()
	{
		auto effects    = queued_executor{ };
		auto decisions  = queued_executor{ };
		auto workspaces = workspace_spy{ };
		auto processes  = process_spy{ };
		auto projection = projection_spy{ };
		auto sessions   = session_catalog_spy{ };
		auto host       = std::make_unique<application::agent_invocation_host>(
			core::built_in_agent_profiles(),
			std::vector{
				core::built_in_agent_policy(
					core::git_host_id{ 1U },
					core::built_in_agent_host_kind::github_com),
			},
			"/tmp/bpr-agent-owned",
			sessions,
			workspaces,
			processes,
			[](const std::string&) -> std::optional<std::string> {
				return std::nullopt;
			},
			effects,
			decisions,
			projection);
		workspaces.FailNextCleanup = true;
		host->observe(
			core::application_session_id{ 1U },
			authority(core::git_host_id{ 1U }),
			binding(core::git_host_id{ 1U }));
		host->dispatch(request(
			core::git_host_id{ 1U }, core::external_terminal_profile_id));
		decisions.drain();
		host->retry_cleanup();
		host->retry_cleanup();
		effects.drain();
		decisions.drain();
		effects.drain();
		expect(
			!host->is_quiescent()
				&& host->statistics().WorkspaceCleanupFailures == 1U
				&& workspaces.CleanupAttempts.size() == 1U,
			"pre-terminal retry requests cannot clean while the process phase is pending");
		const auto authorityCapability = workspaces.CleanupAttempts.front();
		host->retry_cleanup();
		host->retry_cleanup();
		expect(
			workspaces.CleanupAttempts.size() == 1U,
			"multiple retry requests enqueue no concurrent cleanup effect");
		effects.drain();
		expect(
			host->is_quiescent() && workspaces.CleanupAttempts.size() == 2U
				&& workspaces.CleanupAttempts.back() == authorityCapability
				&& host->statistics().WorkspaceCleanups == 1U,
			"one actor-confined retry exact-cleans with the retained capability");
	}
	[[nodiscard]] auto snapshot_files()
		-> std::vector<core::agent_finding_snapshot_file>
	{
		return { {
			.Path           = "src/canary.cpp",
			.Comparison     = opaque<core::file_comparison_digest>(5U),
			.LeftLineCount  = 40U,
			.RightLineCount = 40U,
		} };
	}

	[[nodiscard]] auto returned_finding(std::string aPath) -> core::agent_finding
	{
		return {
			.Provider        = core::agent_profile_id{ 1U },
			.Severity        = core::defect_severity::major,
			.Confidence      = core::agent_finding_confidence::high,
			.Category        = core::defect_category::correctness,
			.Path            = std::move(aPath),
			.Lines           = { .Begin = 10U, .End = 12U },
			.Anchor          = { .Line = core::source_line_ref{
									 .Comparison
									 = opaque<core::file_comparison_digest>(5U),
									 .Side = core::diff_side::right,
									 .Line = 11U,
								 } },
			.Claim           = "the retry loop never terminates",
			.Evidence        = "while (true) with no bound",
			.Consequence     = "the worker hangs",
			.RequiredContext = "the caller's timeout",
			.AnalyzedBaseOid = commit(1U),
			.AnalyzedHeadOid = commit(2U),
			.AnalyzedCommits = { commit(2U) },
		};
	}

	[[nodiscard]] auto finding_request(
		const core::git_host_id                 aHost,
		const std::vector<core::agent_finding>& aFindings)
		-> application::agent_invocation_request
	{
		auto built        = request(aHost, core::external_terminal_profile_id);
		built.FrozenFiles = snapshot_files();
		static_cast<void>(aFindings);
		return built;
	}

	[[nodiscard]] auto payload_of(const std::vector<core::agent_finding>& aFindings)
		-> application::agent_finding_payload
	{
		return {
			.Session  = core::agent_session_id{ 41U },
			.Findings = aFindings,
		};
	}

	void expect_binding_field_for_field(
		const core::agent_revision_binding& aDelivered,
		const core::agent_revision_binding& aExpected)
	{
		expect(
			aDelivered.Host == aExpected.Host
				&& aDelivered.Repository == aExpected.Repository
				&& aDelivered.PullRequest == aExpected.PullRequest
				&& aDelivered.BaseOid == aExpected.BaseOid
				&& aDelivered.HeadOid == aExpected.HeadOid
				&& aDelivered.LeftEpoch == aExpected.LeftEpoch
				&& aDelivered.RightEpoch == aExpected.RightEpoch,
			"the delivered binding is the session's own, field for field");
	}

	void test_findings_reenter_once_with_the_sessions_own_binding()
	{
		auto test              = harness{ };
		test.Processes.Payload = payload_of({ returned_finding("src/canary.cpp") });
		test.Host.observe(
			core::application_session_id{ 1U },
			authority(core::git_host_id{ 1U }),
			binding(core::git_host_id{ 1U }));
		test.Host.dispatch(
			finding_request(core::git_host_id{ 1U }, { }));
		test.settle();
		expect(
			test.Projection.Deliveries.size() == 1U
				&& test.Projection.DeliveryIdentities.size() == 1U
				&& test.Projection.DeliveryIdentities.front().Session.Id
					== core::agent_session_id{ 41U },
			"one run's payload re-enters exactly once, on the completion lane");
		const auto& delivered = test.Projection.Deliveries.front();
		expect(
			delivered.Session == core::agent_session_id{ 41U }
				&& delivered.Staleness == core::agent_session_staleness::current,
			"the delivery names its own session and reports it current");
		expect_binding_field_for_field(
			delivered.Binding, binding(core::git_host_id{ 1U }));
		expect(
			delivered.Findings.size() == 1U
				&& delivered.Findings.front().Verdict.State
					== core::agent_finding_anchor_state::anchored
				&& delivered.Findings.front().Verdict.Refusal
					== core::agent_finding_refusal::none
				&& delivered.Findings.front().Finding
					== returned_finding("src/canary.cpp"),
			"a finding the frozen snapshot contains arrives anchored and intact");
		expect(
			test.Projection.Finished.size() == 1U
				&& !test.Projection.Finished.front().Findings.has_value(),
			"no undecoded payload reaches a presentation beside the verdicts");
	}

	void test_an_invalid_anchor_reenters_unanchored_rather_than_dropped()
	{
		auto test              = harness{ };
		test.Processes.Payload = payload_of({
			returned_finding("src/canary.cpp"),
			returned_finding("src/nobody-named-this.cpp"),
		});
		test.Host.observe(
			core::application_session_id{ 1U },
			authority(core::git_host_id{ 1U }),
			binding(core::git_host_id{ 1U }));
		test.Host.dispatch(finding_request(core::git_host_id{ 1U }, { }));
		test.settle();
		expect(
			test.Projection.Deliveries.size() == 1U
				&& test.Projection.Deliveries.front().Findings.size() == 2U,
			"a finding the snapshot cannot place is delivered, not discarded");
		const auto& refused = test.Projection.Deliveries.front().Findings.back();
		expect(
			refused.Verdict.State == core::agent_finding_anchor_state::unanchored
				&& refused.Verdict.Refusal
					== core::agent_finding_refusal::unknown_path
				&& refused.Finding.Path == "src/nobody-named-this.cpp"
				&& refused.Verdict.AnalyzedBaseOid == commit(1U)
				&& refused.Verdict.AnalyzedHeadOid == commit(2U),
			"the verdict names the refusal and keeps the finding's own revisions");
		expect(
			test.Projection.Deliveries.front().Findings.front().Verdict.State
				== core::agent_finding_anchor_state::anchored,
			"one refused anchor does not refuse the finding beside it");
	}

	/// \brief The continued session one turn is dispatched under.
	///
	/// \details Bound to the revision the reviewer kept, which is the same
	/// revision `request` names, because the host refuses a turn whose two
	/// answers about one revision differ.
	[[nodiscard]] auto continued_request(const core::git_host_id aHost)
		-> application::agent_invocation_request
	{
		auto built             = finding_request(aHost, { });
		built.ContinuedSession = core::agent_session{
			.Id      = core::agent_session_id{ 41U },
			.Profile = core::external_terminal_profile_id,
			.Binding = binding(aHost),
		};
		return built;
	}

	void test_a_continued_turn_survives_the_binding_moving_under_it()
	{
		auto       test            = harness{ };
		const auto activeAuthority = authority(core::git_host_id{ 1U });
		test.Processes.Payload     = payload_of({ returned_finding("src/canary.cpp") });
		test.Host.observe(
			core::application_session_id{ 1U },
			activeAuthority,
			binding(core::git_host_id{ 1U }));
		// The revision under review moves while the turn is running, which is
		// what an epoch acceptance arriving behind a refresh does. The process
		// spy re-reads its stop token after this, so a superseded turn comes
		// back cancelled and a surviving one comes back with its answer.
		test.Processes.AfterStart = [&test, activeAuthority] {
			auto moved    = binding(core::git_host_id{ 1U });
			moved.HeadOid = commit(9U);
			test.Host.observe(
				core::application_session_id{ 1U }, activeAuthority, moved);
		};
		test.Host.dispatch(continued_request(core::git_host_id{ 1U }));
		test.settle();
		expect(
			test.Processes.Starts == 1U && !test.Processes.LastStopRequested
				&& test.Processes.Cancels == 0U,
			"a binding change does not stop a turn asked about the older revision");
		expect(
			test.Projection.Finished.size() == 1U
				&& test.Projection.Finished.front().Outcome
					== application::agent_run_outcome::success
				&& !test.Projection.Finished.front().Events.empty(),
			"the continued turn completes and keeps the answer it produced");
		expect(
			test.Sessions.AdmissionAttempts == 0U
				&& test.Workspaces.Cleanups == 1U && test.Host.is_quiescent(),
			"continuing admits no session and still cleans the workspace it owned");
		expect(
			test.Projection.Deliveries.size() == 1U
				&& test.Projection.Deliveries.front().Staleness
					== core::agent_session_staleness::head_moved
				&& test.Projection.Deliveries.front().Findings.size() == 1U
				&& test.Projection.Deliveries.front().Findings.front().Verdict.State
					== core::agent_finding_anchor_state::anchored,
			"the delivery still states how far the review moved and anchors against the frozen snapshot");
		expect_binding_field_for_field(
			test.Projection.Deliveries.front().Binding,
			binding(core::git_host_id{ 1U }));
	}

	void test_a_fresh_turn_under_the_same_binding_change_is_superseded()
	{
		auto       test            = harness{ };
		const auto activeAuthority = authority(core::git_host_id{ 1U });
		test.Processes.Payload     = payload_of({ returned_finding("src/canary.cpp") });
		test.Host.observe(
			core::application_session_id{ 1U },
			activeAuthority,
			binding(core::git_host_id{ 1U }));
		test.Processes.AfterStart = [&test, activeAuthority] {
			auto moved    = binding(core::git_host_id{ 1U });
			moved.HeadOid = commit(9U);
			test.Host.observe(
				core::application_session_id{ 1U }, activeAuthority, moved);
		};
		// The same movement, under a turn asked about the revision that moved.
		// Nothing here is continued, so the guard does not apply and this is
		// the behaviour every fresh turn has always had.
		test.Host.dispatch(finding_request(core::git_host_id{ 1U }, { }));
		test.settle();
		expect(
			test.Processes.LastStopRequested && test.Processes.Cancels == 1U,
			"a fresh turn is stopped when the revision it was asked about moves");
		expect(
			test.Projection.Finished.size() == 1U
				&& test.Projection.Finished.front().Outcome
					== application::agent_run_outcome::stale_result
				&& test.Projection.Finished.front().Events.empty()
				&& test.Workspaces.Cleanups == 1U && test.Host.is_quiescent(),
			"the superseded fresh turn reports stale and leaks no workspace");
	}

	void test_a_payload_after_an_accepted_head_move_arrives_stale()
	{
		auto       test            = harness{ };
		const auto activeAuthority = authority(core::git_host_id{ 1U });
		test.Processes.Payload     = payload_of({ returned_finding("src/canary.cpp") });
		test.Host.observe(
			core::application_session_id{ 1U },
			activeAuthority,
			binding(core::git_host_id{ 1U }));
		test.Host.dispatch(finding_request(core::git_host_id{ 1U }, { }));
		test.Decisions.drain();
		test.Effects.drain();
		auto moved    = binding(core::git_host_id{ 1U });
		moved.HeadOid = commit(9U);
		test.Host.observe(core::application_session_id{ 1U }, activeAuthority, moved);
		test.settle();
		expect(
			test.Projection.Finished.size() == 1U
				&& test.Projection.Finished.front().Outcome
					== application::agent_run_outcome::stale_result,
			"the terminal still reports the accepted head move as stale");
		expect(
			test.Projection.Deliveries.size() == 1U
				&& test.Projection.Deliveries.front().Staleness
					== core::agent_session_staleness::head_moved,
			"the payload survives supersession and arrives stale by name");
		const auto& delivered = test.Projection.Deliveries.front();
		expect_binding_field_for_field(
			delivered.Binding, binding(core::git_host_id{ 1U }));
		expect(
			delivered.Binding.HeadOid == commit(2U)
				&& delivered.Binding.HeadOid != moved.HeadOid,
			"the delivery keeps the head the run started with, not the new one");
		expect(
			delivered.Findings.size() == 1U
				&& delivered.Findings.front().Finding.AnalyzedBaseOid == commit(1U)
				&& delivered.Findings.front().Finding.AnalyzedHeadOid == commit(2U)
				&& delivered.Findings.front().Verdict.AnalyzedHeadOid == commit(2U),
			"the analyzed revisions survive the move intact");
	}

	/// [[BPR-1211]]. The session identity is allocated off the decision lane.
	/// While it is, no second turn starts; an answer for a review this host
	/// has left starts nothing.
	void test_session_admission_answers_later_and_admits_one_turn()
	{
		auto test = harness{ };
		test.Host.observe(core::application_session_id{ 1U }, authority(core::git_host_id{ 1U }), binding(core::git_host_id{ 1U }));
		test.Decisions.drain();
		test.Host.dispatch(request(core::git_host_id{ 1U }, core::external_terminal_profile_id));
		test.Host.dispatch(request(core::git_host_id{ 1U }, core::external_terminal_profile_id));
		expect(test.Sessions.AdmissionAttempts == 1U && test.Projection.Refusals == std::vector<std::string>{ "agent_invocation_already_running" }
				   && !test.Host.is_quiescent() && test.Workspaces.Starts == 0U,
			   "a second turn is refused while the first one's identity is allocated");
		test.settle();
		expect(test.Processes.Starts == 1U && test.Host.is_quiescent(), "the admitted turn then runs once");

		auto stale = harness{ };
		stale.Host.observe(core::application_session_id{ 1U }, authority(core::git_host_id{ 1U }), binding(core::git_host_id{ 1U }));
		stale.Decisions.drain();
		stale.Host.dispatch(request(core::git_host_id{ 1U }, core::external_terminal_profile_id));
		auto moved    = binding(core::git_host_id{ 1U });
		moved.HeadOid = commit(9U);
		stale.Host.observe(core::application_session_id{ 1U }, authority(core::git_host_id{ 1U }), moved);
		stale.settle();
		expect(stale.Sessions.AdmissionAttempts == 1U && stale.Workspaces.Starts == 0U && stale.Processes.Starts == 0U
				   && !stale.Projection.Refusals.empty() && stale.Projection.Refusals.back() == "stale_or_mismatched_agent_context"
				   && stale.Host.is_quiescent(),
			   "an identity answered after the binding moved starts nothing and is refused by name");
	}

	void test_a_promoted_finding_reaches_only_the_note_authority()
	{
		auto       store    = ledger_store_spy{ };
		const auto finding  = returned_finding("src/canary.cpp");
		const auto snapshot = core::agent_finding_snapshot{
			.Binding = binding(core::git_host_id{ 1U }),
			.Files   = snapshot_files(),
		};
		const auto verdict = core::validate_agent_finding(finding, snapshot);
		expect(
			verdict.State == core::agent_finding_anchor_state::anchored,
			"the promoted finding is one the frozen snapshot anchors");
		const auto stored = application::store_promoted_finding(
			store,
			application::local_note_id{ 7U },
			application::local_note_attachment{
				.File       = opaque<core::file_identity>(4U),
				.Comparison = opaque<core::file_comparison_digest>(5U),
			},
			core::agent_finding_promotion_request{
				.Finding = finding,
				.Verdict = verdict,
				.Choice  = core::agent_finding_promotion_choice::private_note,
			});
		expect(
			stored.has_value() && stored->Id == application::local_note_id{ 7U }
				&& stored->Revision == application::local_note_revision{ 1U }
				&& stored->Note.Text == finding.Claim,
			"a promoted note carries the claim verbatim into the durable record");
		expect(
			store.Touched.size() == 1U && store.Touched.front() == "store_note"
				&& store.Notes.size() == 1U,
			"agent output reaches store_note and no other durable authority");

		auto       elsewhere = ledger_store_spy{ };
		const auto refused   = application::store_promoted_finding(
			elsewhere,
			application::local_note_id{ 8U },
			std::nullopt,
			core::agent_finding_promotion_request{
				.Finding = finding,
				.Verdict = verdict,
				.Choice  = core::agent_finding_promotion_choice::defect,
			});
		expect(
			!refused.has_value() && refused.error().WrongArtifact
				&& elsewhere.Touched.empty(),
			"a promotion into another artifact writes nothing through this path");

		auto       unanchored = ledger_store_spy{ };
		const auto missing    = returned_finding("src/nobody-named-this.cpp");
		const auto refusal    = application::store_promoted_finding(
			unanchored,
			application::local_note_id{ 9U },
			std::nullopt,
			core::agent_finding_promotion_request{
				.Finding = missing,
				.Verdict = core::validate_agent_finding(missing, snapshot),
				.Choice  = core::agent_finding_promotion_choice::private_note,
			});
		expect(
			!refusal.has_value()
				&& refusal.error().Refusal
					== core::agent_finding_promotion_refusal::unanchored
				&& unanchored.Touched.empty(),
			"an unanchored finding is refused before the store is touched");
	}

} // namespace

auto main() -> int
{
	test_explicit_settings_cross_the_existing_invocation_host();
	test_session_admission_answers_later_and_admits_one_turn();
	test_ghes_policy_refuses_codex_before_effects();
	test_context_refusal_precedes_durable_admission();
	test_durable_admission_refusal_precedes_every_downstream_effect();
	test_latest_retained_session_uses_every_core_classification_without_retargeting();
	test_valid_dispatch_materializes_before_process_and_cleans_after_terminal();
	test_revision_movement_discards_result_and_still_cleans();
	test_superseded_dominates_mixed_cancellation_and_head_movement();
	test_prestart_workspace_failure_is_an_admitted_typed_terminal();
	test_projection_exceptions_never_escape_or_skip_cleanup();
	test_cancel_and_shutdown_before_effect_create_no_workspace_or_process();
	test_cancellation_is_unlosable_at_each_prelaunch_boundary_and_recovers();
	test_process_cancellation_uses_the_exact_invocation_token_and_recovers();
	test_projection_identity_and_queued_destruction_are_safe();
	test_destruction_cancels_running_work_before_ports_can_die();
	test_reentrant_projection_destruction_never_waits_on_itself();
	test_cleanup_refusal_retains_exact_authority_and_nonquiescence();
	test_findings_reenter_once_with_the_sessions_own_binding();
	test_an_invalid_anchor_reenters_unanchored_rather_than_dropped();
	test_a_payload_after_an_accepted_head_move_arrives_stale();
	test_a_continued_turn_survives_the_binding_moving_under_it();
	test_a_fresh_turn_under_the_same_binding_change_is_superseded();
	test_a_promoted_finding_reaches_only_the_note_authority();
	std::cout << "agent-invocation-host-tests:pass\n";
	return EXIT_SUCCESS;
}
