#include "bpr/application/manual_executor.hpp"
#include "bpr/application/review_mutation_application_host.hpp"

#include "../../src/application/src/review_mutation_execution_authority.hpp"
#include "bpr/core/feedback_draft_edit.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
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
		bytes[0]   = aSeed;
		bytes[31]  = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return Identity{ bytes };
	}

	template <typename Oid>
	[[nodiscard]] auto oid(const std::uint8_t aSeed) -> Oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		for (auto index = std::size_t{ 0U }; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + index);
		}
		return Oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto workbench_authority()
		-> application::workbench_pull_request_authority
	{
		return {
			.ApplicationSession    = core::application_session_id{ 3U },
			.Profile               = core::host_profile_id{ 10U },
			.ProfileRevision       = core::github_profile_revision{ 20U },
			.Host                  = core::git_host_id{ 30U },
			.HostAuthority         = opaque<core::host_authority_fingerprint>(13U),
			.Account               = core::github_account_id{ 40U },
			.Viewer                = core::github_viewer_actor_id{ 50U },
			.Repository            = core::repository_id{ 101U },
			.PullRequest           = core::pull_request_id{ 202U },
			.Number                = core::pull_request_number{ 17U },
			.Snapshot              = core::provider_snapshot_id{ 60U },
			.IdentityGeneration    = core::github_identity_generation{ 70U },
			.PullRequestGeneration = core::request_generation{ 80U },
		};
	}

	[[nodiscard]] auto selected_diff_context()
		-> application::workbench_selected_diff_context
	{
		const auto workbench = workbench_authority();
		const auto authority = core::pull_request_authority{
			.Profile         = workbench.Profile,
			.ProfileRevision = workbench.ProfileRevision,
			.Host            = workbench.Host,
			.HostAuthority   = workbench.HostAuthority,
			.Account         = workbench.Account,
			.Viewer          = workbench.Viewer,
			.Repository      = workbench.Repository,
			.Cache           = opaque<core::repository_cache_scope_id>(14U),
			.PullRequest     = workbench.PullRequest,
			.Number          = workbench.Number,
			.Scope           = opaque<core::pull_request_authority_scope_id>(15U),
		};
		const auto options = core::normalize_diff_options(core::diff_options{ });
		expect(options.has_value(), "default options normalize");
		const auto digest = core::options_digest(*options);
		expect(digest.has_value(), "default options have a digest");
		return {
			.ApplicationSession = workbench.ApplicationSession,
			.Authority          = authority,
			.Comparison         = {
				.Left = {
					.Identity = {
						.Authority   = authority.Scope,
						.Incarnation = opaque<core::epoch_catalog_incarnation_id>(16U),
						.Kind        = core::comparison_anchor_kind::epoch_base,
						.Epoch       = core::epoch_ordinal{ 1 },
					},
					.Commit       = oid<core::git_commit_oid>(20U),
					.Availability = core::anchor_availability::available,
				},
				.Right = {
					.Identity = {
						.Authority   = authority.Scope,
						.Incarnation = opaque<core::epoch_catalog_incarnation_id>(16U),
						.Kind        = core::comparison_anchor_kind::epoch_head,
						.Epoch       = core::epoch_ordinal{ 1 },
					},
					.Commit       = oid<core::git_commit_oid>(30U),
					.Availability = core::anchor_availability::available,
				},
			},
			.ComparisonRevision = core::comparison_revision{ 1U },
			.Options            = *options,
			.OptionsDigest      = *digest,
			.Algorithm          = core::supported_diff_algorithm_revision,
		};
	}

	[[nodiscard]] auto target() -> core::review_mutation_target
	{
		return {
			.Host         = core::git_host_id{ 30U },
			.Repository   = core::repository_id{ 101U },
			.PullRequest  = core::pull_request_id{ 202U },
			.ExpectedHead = oid<core::git_commit_oid>(30U),
		};
	}

	[[nodiscard]] auto live_target()
		-> application::review_mutation_target_binding
	{
		return {
			.Semantic                 = target(),
			.Hostname                 = application::github_canonical_hostname{ "github.com" },
			.ServerContract           = application::github_server_contract::github_com_2022_11_28,
			.Repository               = { .Owner = "MikeMarcin", .Name = "BPR-Test" },
			.Number                   = core::pull_request_number{ 17U },
			.RepositoryDatabaseId     = 101U,
			.RepositoryNodeId         = "R_fixture",
			.PullRequestDatabaseId    = 202U,
			.PullRequestNodeId        = "PR_fixture",
			.BaseRepositoryDatabaseId = 101U,
			.BaseOid                  = oid<core::git_commit_oid>(20U),
			.BaseRef                  = "main",
			.HeadRef                  = "fixture-head",
			.Open                     = true,
		};
	}

	[[nodiscard]] auto acting_identity()
		-> application::review_mutation_acting_identity
	{
		return {
			.Account    = core::github_account_id{ 40U },
			.DatabaseId = 41U,
			.NodeId     = "U_fixture",
			.Login      = "fixture-reviewer",
		};
	}

	[[nodiscard]] auto publication_request()
		-> application::review_mutation_publication_request
	{
		const auto context = selected_diff_context();
		const auto file    = opaque<core::file_identity>(44U);
		auto       key     = core::file_comparison_key{
			.Authority   = context.Authority,
			.Repository  = context.Authority.Repository,
			.PullRequest = context.Authority.PullRequest,
			.LeftAnchor  = context.Comparison.Left.Identity,
			.RightAnchor = context.Comparison.Right.Identity,
			.LeftBlob    = oid<core::git_blob_oid>(40U),
			.RightBlob   = oid<core::git_blob_oid>(50U),
			.File        = file,
			.Options     = context.OptionsDigest,
			.Algorithm   = context.Algorithm,
		};
		const auto comparison = core::comparison_digest(key);
		const auto anchor     = core::source_line_ref{
			.Comparison = comparison,
			.Side       = core::diff_side::right,
			.Line       = 12U,
		};
		auto result = application::review_mutation_publication_request{
			.Proposal = {
				.Draft       = core::review_draft_id{ 7U },
				.Anchor      = anchor,
				.Body        = "Please rename this.",
				.Disposition = core::review_disposition::comment,
			},
			.Selection = {
				.Comparison    = comparison,
				.Kind          = core::semantic_selection_kind::line,
				.PreferredLine = anchor,
			},
			.SelectedDiff = {
				.Request = {
					.Context = {
						.Request    = core::request_id{ 5U },
						.Session    = core::application_session_id{ 3U },
						.Generation = core::request_generation{ 8U },
					},
					.WorkbenchAuthority = workbench_authority(),
					.Diff               = context,
					.File               = {
						.File             = file,
						.PresentationPath = "display.cpp",
						.CurrentPath      = "renamed/current.cpp",
						.PreviousPath     = "previous/original.cpp",
						.Change           = application::workbench_file_change_kind::renamed,
						.ContentKind      = application::workbench_file_content_kind::text,
					},
				},
				.Key       = std::move(key),
				.LeftPath  = "previous/original.cpp",
				.RightPath = "renamed/current.cpp",
			},
		};
		result.SelectedDiff.LeftEndpoint
			= application::make_workbench_selected_diff_endpoint_for_testing(
				result.SelectedDiff.Request,
				core::diff_side::left,
				*result.SelectedDiff.Key.LeftBlob,
				*result.SelectedDiff.LeftPath);
		result.SelectedDiff.RightEndpoint
			= application::make_workbench_selected_diff_endpoint_for_testing(
				result.SelectedDiff.Request,
				core::diff_side::right,
				*result.SelectedDiff.Key.RightBlob,
				*result.SelectedDiff.RightPath);
		return result;
	}

	void bind_resolved_endpoints(
		application::review_mutation_publication_request& aRequest)
	{
		aRequest.SelectedDiff.LeftEndpoint.reset();
		if (aRequest.SelectedDiff.Key.LeftBlob.has_value()
			&& aRequest.SelectedDiff.LeftPath.has_value()) {
			aRequest.SelectedDiff.LeftEndpoint
				= application::make_workbench_selected_diff_endpoint_for_testing(
					aRequest.SelectedDiff.Request,
					core::diff_side::left,
					*aRequest.SelectedDiff.Key.LeftBlob,
					*aRequest.SelectedDiff.LeftPath);
		}
		aRequest.SelectedDiff.RightEndpoint.reset();
		if (aRequest.SelectedDiff.Key.RightBlob.has_value()
			&& aRequest.SelectedDiff.RightPath.has_value()) {
			aRequest.SelectedDiff.RightEndpoint
				= application::make_workbench_selected_diff_endpoint_for_testing(
					aRequest.SelectedDiff.Request,
					core::diff_side::right,
					*aRequest.SelectedDiff.Key.RightBlob,
					*aRequest.SelectedDiff.RightPath);
		}
	}

	class fake_provider final : public application::review_mutation_port
	{
	public:
		auto observe_target() -> std::expected<
			core::review_mutation_target,
			application::review_mutation_failure> override
		{
			return observe_target(std::stop_token{ });
		}

		auto observe_target(const std::stop_token aStop) -> std::expected<
			core::review_mutation_target,
			application::review_mutation_failure> override
		{
			++Observations;
			StopObserved.push_back(aStop.stop_requested());
			if (OnObserve) {
				OnObserve();
			}
			return aStop.stop_requested()
				? std::unexpected(application::review_mutation_failure::cancelled)
				: std::expected<core::review_mutation_target, application::review_mutation_failure>{ Reported };
		}

		auto submit(const application::review_mutation_request& aRequest)
			-> std::expected<
				application::review_mutation_receipt,
				application::review_mutation_failure> override
		{
			return submit(aRequest, std::stop_token{ });
		}

		auto submit(
			const application::review_mutation_request& aRequest,
			const std::stop_token                       aStop)
			-> std::expected<
				application::review_mutation_receipt,
				application::review_mutation_failure> override
		{
			StopObserved.push_back(aStop.stop_requested());
			if (aStop.stop_requested()) {
				return std::unexpected(application::review_mutation_failure::cancelled);
			}
			Submitted.push_back(aRequest);
			return application::review_mutation_receipt{
				.ServerCommentId = "fixture-comment",
				.Duplicate       = false,
			};
		}

		auto submit_and_reconcile(
			const application::review_mutation_request& aRequest,
			const std::stop_token                       aStop)
			-> application::review_mutation_submission override
		{
			if (NextSubmission.has_value()) {
				Submitted.push_back(aRequest);
				auto result = std::move(*NextSubmission);
				NextSubmission.reset();
				return result;
			}
			return {
				.Result            = submit(aRequest, aStop),
				.MutationAttempted = true,
			};
		}

		[[nodiscard]] auto capabilities() const
			-> application::review_provider_capabilities override
		{
			return { .ResolvesThreads = Resolves, .RecordsViewedState = false };
		}
		auto resolve_thread(
			const application::review_thread_resolution_request& aRequest,
			const std::stop_token                                aStop)
			-> std::expected<application::review_thread_resolution_receipt, application::review_mutation_failure> override
		{
			if (aStop.stop_requested()) {
				return std::unexpected(application::review_mutation_failure::cancelled);
			}
			Resolved.push_back(aRequest);
			return application::review_thread_resolution_receipt{ .Thread = aRequest.Thread, .Resolved = aRequest.Resolve };
		}

		bool                                                       Resolves = false;
		std::vector<application::review_thread_resolution_request> Resolved;
		core::review_mutation_target                               Reported     = target();
		std::size_t                                                Observations = 0U;
		std::vector<bool>                                          StopObserved;
		std::vector<application::review_mutation_request>          Submitted;
		std::function<void()>                                      OnObserve;
		std::optional<application::review_mutation_submission>     NextSubmission;
	};

	class recording_projection final
		: public application::review_mutation_projection_port,
		  public application::review_mutation_source_port
	{
	public:
		void publish(const application::review_mutation_terminal& aTerminal) override
		{
			Published.push_back(aTerminal);
		}
		void publish_resolution(const application::review_thread_resolution_terminal& aTerminal) override
		{
			Resolutions.push_back(aTerminal);
		}

		std::optional<application::review_mutation_source_observation> Source = [] {
			const auto request = publication_request();
			return application::review_mutation_source_observation{
				.Authority      = request.SelectedDiff.Request.WorkbenchAuthority,
				.File           = request.SelectedDiff.Request.File,
				.Target         = live_target(),
				.ActingIdentity = acting_identity(),
				.Material       = { .Key = request.SelectedDiff.Key, .LeftLines = 100U, .RightLines = 100U },
			};
		}();
		auto                                                        observe_review_source(core::workbench_file_id) const -> std::optional<application::review_mutation_source_observation> override { return Source; }
		std::vector<application::review_mutation_terminal>          Published;
		std::vector<application::review_thread_resolution_terminal> Resolutions;
	};

	class parallel_executor final : public application::effect_executor
	{
	public:
		void post(std::function<void()> aTask) override
		{
			std::scoped_lock lock{ Mutex };
			Workers.emplace_back(std::move(aTask));
			++Posts;
		}

		void join()
		{
			auto workers = std::vector<std::jthread>{ };
			{
				std::scoped_lock lock{ Mutex };
				workers.swap(Workers);
			}
			workers.clear();
		}

		[[nodiscard]] auto posts() const -> std::size_t
		{
			std::scoped_lock lock{ Mutex };
			return Posts;
		}

	private:
		mutable std::mutex        Mutex;
		std::vector<std::jthread> Workers;
		std::size_t               Posts = 0U;
	};

	class barrier_provider final : public application::review_mutation_port
	{
	public:
		auto observe_target() -> std::expected<
			core::review_mutation_target,
			application::review_mutation_failure> override
		{
			return observe_target(std::stop_token{ });
		}

		auto observe_target(const std::stop_token aStop) -> std::expected<
			core::review_mutation_target,
			application::review_mutation_failure> override
		{
			{
				auto lock = std::unique_lock{ Mutex };
				++Active;
				MaximumActive = std::max(MaximumActive, Active);
				Entered       = true;
				Condition.notify_all();
				Condition.wait(lock, [this] { return Released; });
				--Active;
			}
			return aStop.stop_requested()
				? std::unexpected(application::review_mutation_failure::cancelled)
				: std::expected<
					  core::review_mutation_target,
					  application::review_mutation_failure>{ target() };
		}

		auto submit(const application::review_mutation_request& aRequest)
			-> std::expected<
				application::review_mutation_receipt,
				application::review_mutation_failure> override
		{
			return submit(aRequest, std::stop_token{ });
		}

		auto submit(
			const application::review_mutation_request&,
			const std::stop_token)
			-> std::expected<
				application::review_mutation_receipt,
				application::review_mutation_failure> override
		{
			++Submissions;
			return application::review_mutation_receipt{
				.ServerCommentId = "unexpected",
			};
		}

		[[nodiscard]] auto capabilities() const
			-> application::review_provider_capabilities override
		{
			return { };
		}

		void wait_until_entered()
		{
			auto lock = std::unique_lock{ Mutex };
			Condition.wait(lock, [this] { return Entered; });
		}

		void release()
		{
			{
				std::scoped_lock lock{ Mutex };
				Released = true;
			}
			Condition.notify_all();
		}

		[[nodiscard]] auto maximum_active() const -> std::size_t
		{
			std::scoped_lock lock{ Mutex };
			return MaximumActive;
		}

		std::size_t Submissions = 0U;

	private:
		mutable std::mutex      Mutex;
		std::condition_variable Condition;
		std::size_t             Active        = 0U;
		std::size_t             MaximumActive = 0U;
		bool                    Entered       = false;
		bool                    Released      = false;
	};

	[[nodiscard]] auto testing_grant()
		-> application::review_mutation_execution_grant
	{
		return application::make_review_mutation_testing_grant(
			core::application_session_id{ 3U },
			core::request_generation{ 8U },
			live_target(),
			acting_identity());
	}

	// [[BPR-0657]]. A thread resolution: refused before any effect without
	// the provider's capability, a nameless thread, or a grant that does not
	// permit it (another session; a one-shot issued for a publication); with
	// the capability and a permitting grant it reaches the provider bound to
	// the target and actor and its terminal names the thread's new state.
	// [[BPR-0657]]. A root comment drafted from a left-side selection is
	// unsupported, since it is anchored where it is published; a reply drafted
	// from the same selection is admitted, since its thread anchors it.
	void test_a_reply_may_be_drafted_from_a_left_side_selection()
	{
		auto  provider   = std::make_unique<fake_provider>();
		auto* observed   = provider.get();
		auto  effects    = application::manual_executor{ };
		auto  decisions  = application::manual_executor{ };
		auto  projection = recording_projection{ };
		auto  host       = application::review_mutation_application_host{
			std::move(provider),
			live_target(),
			acting_identity(),
			core::application_session_id{ 3U },
			core::request_generation{ 8U },
			testing_grant(),
			effects,
			decisions,
			projection,
			&projection,
		};
		auto left                         = publication_request();
		left.Selection.PreferredLine.Side = core::diff_side::left;
		left.Proposal.Anchor              = left.Selection.PreferredLine;
		static_cast<void>(host.publish(left));
		expect(effects.pending() == 0U && decisions.pending() == 1U, "a left-side root comment is refused before any effect");
		decisions.run_next();
		expect(!projection.Published.empty() && !projection.Published.back().Result.has_value()
				   && projection.Published.back().Result.error() == application::review_mutation_failure::unsupported,
			   "the left-side root comment is unsupported");
		auto reply                   = left;
		reply.Proposal.ReplyToThread = "PRRT_fixture_1";
		static_cast<void>(host.publish(reply));
		expect(effects.pending() == 1U, "a reply from the same selection is admitted");
		effects.run_next();
		decisions.run_next();
		expect(observed->Submitted.size() == 1U
				   && observed->Submitted.front().Proposal.ReplyToThread == std::optional<std::string>{ "PRRT_fixture_1" }
				   && observed->Submitted.front().LiveBinding.has_value()
				   && observed->Submitted.front().LiveBinding->Anchor.Semantic.Side == core::diff_side::left
				   && projection.Published.back().Result.has_value(),
			   "the reply reaches the provider naming its thread, bound to the left-side line it was drafted from");
		host.shutdown();
	}

	// [[BPR-1212]]. A root comment on a base line is GitHub's `side: LEFT`.
	// It is admitted only with the source's evidence that the left document
	// is numbered as GitHub's base, and it is bound to that side with the base
	// document's line count; a range on the base side keeps its first line.
	void test_a_base_line_root_needs_githubs_base_numbering()
	{
		auto  provider                        = std::make_unique<fake_provider>();
		auto* observed                        = provider.get();
		auto  effects                         = application::manual_executor{ };
		auto  decisions                       = application::manual_executor{ };
		auto  projection                      = recording_projection{ };
		projection.Source->Material.LeftLines = 60U;
		auto host                             = application::review_mutation_application_host{
			std::move(provider),
			live_target(),
			acting_identity(),
			core::application_session_id{ 3U },
			core::request_generation{ 8U },
			testing_grant(),
			effects,
			decisions,
			projection,
			&projection,
		};
		auto base                         = publication_request();
		base.Selection.PreferredLine.Side = core::diff_side::left;
		base.Proposal.Anchor              = base.Selection.PreferredLine;
		static_cast<void>(host.publish(base));
		decisions.drain();
		expect(effects.pending() == 0U && !projection.Published.empty() && !projection.Published.back().Result.has_value()
				   && projection.Published.back().Result.error() == application::review_mutation_failure::unsupported,
			   "without the evidence a base-line root is unsupported before any effect");

		projection.Source->Material.BaseNumberedAsGitHub = true;
		auto ranged                                      = base;
		ranged.Proposal.Draft                            = core::review_draft_id{ 71U };
		ranged.Proposal.StartLine                        = 10U;
		static_cast<void>(host.publish(ranged));
		expect(effects.pending() == 1U, "with the evidence a base-line range is admitted");
		effects.run_next();
		decisions.run_next();
		expect(observed->Submitted.size() == 1U && observed->Submitted.front().LiveBinding.has_value()
				   && observed->Submitted.front().LiveBinding->Anchor.Semantic.Side == core::diff_side::left
				   && observed->Submitted.front().LiveBinding->Anchor.Semantic.Line == 12U
				   && observed->Submitted.front().LiveBinding->Anchor.Path == "renamed/current.cpp"
				   && observed->Submitted.front().Proposal.StartLine == std::optional<std::uint32_t>{ 10U }
				   && observed->Submitted.front().Source.has_value() && observed->Submitted.front().Source->LineCount == 60U,
			   "it reaches the provider on the left, on the head's path, with its first line and the base document's line count");

		auto past                         = base;
		past.Proposal.Draft               = core::review_draft_id{ 72U };
		past.Selection.PreferredLine.Line = 60U;
		past.Proposal.Anchor              = past.Selection.PreferredLine;
		static_cast<void>(host.publish(past));
		decisions.drain();
		expect(effects.pending() == 0U && !projection.Published.back().Result.has_value(), "a base line past the base document is refused");
		host.shutdown();
	}

	void test_thread_resolution_needs_the_capability_and_a_permitting_grant()
	{
		auto  provider   = std::make_unique<fake_provider>();
		auto* observed   = provider.get();
		auto  effects    = application::manual_executor{ };
		auto  decisions  = application::manual_executor{ };
		auto  projection = recording_projection{ };
		auto  host       = application::review_mutation_application_host{
			std::move(provider),
			live_target(),
			acting_identity(),
			core::application_session_id{ 3U },
			core::request_generation{ 8U },
			testing_grant(),
			effects,
			decisions,
			projection,
			&projection,
		};
		const auto request = application::review_thread_resolution_publication_request{
			.Thread = "PRRT_fixture_1", .Resolve = true
		};
		const auto refused = [&](const application::review_mutation_failure aFailure, const char* const aWhy) {
			expect(effects.pending() == 0U && decisions.pending() == 1U, aWhy);
			decisions.run_next();
			expect(!projection.Resolutions.empty() && !projection.Resolutions.back().Result.has_value()
					   && projection.Resolutions.back().Result.error() == aFailure
					   && projection.Resolutions.back().Request == request,
				   aWhy);
		};
		static_cast<void>(host.resolve(request));
		refused(application::review_mutation_failure::unsupported,
				"without the provider's capability the resolution is refused before any effect");
		observed->Resolves = true;
		static_cast<void>(host.resolve({ .Thread = "", .Resolve = true }));
		expect(decisions.pending() == 1U, "a nameless thread is refused before any effect");
		decisions.run_next();
		expect(projection.Resolutions.back().Result.error() == application::review_mutation_failure::undesignated_target,
			   "a nameless thread is an undesignated target");
		static_cast<void>(host.resolve(request, application::make_review_mutation_testing_grant(core::application_session_id{ 4U }, core::request_generation{ 8U }, live_target(), acting_identity())));
		refused(application::review_mutation_failure::unavailable, "another session's grant permits nothing");
		static_cast<void>(host.resolve(request, application::make_review_mutation_testing_grant(core::application_session_id{ 3U }, core::request_generation{ 8U }, live_target(), acting_identity(), publication_request())));
		refused(application::review_mutation_failure::unavailable, "a one-shot grant issued for a publication permits no resolution");
		expect(observed->Resolved.empty() && host.statistics().ThreadResolutions == 0U,
			   "no refusal reached the provider");

		const auto context = host.resolve(request);
		expect(context.Request.value() != 0U && effects.pending() == 1U && observed->Resolved.empty(),
			   "an admitted resolution is queued and returns immediately");
		effects.run_next();
		expect(observed->Resolved.size() == 1U && observed->Resolved.front().Thread == "PRRT_fixture_1"
				   && observed->Resolved.front().Resolve && observed->Resolved.front().Target == target()
				   && observed->Resolved.front().LiveBinding.has_value()
				   && observed->Resolved.front().LiveBinding->Target == live_target()
				   && observed->Resolved.front().LiveBinding->ActingIdentity == acting_identity(),
			   "the effect asks the provider for the exact thread, bound to the target and the actor");
		expect(decisions.pending() == 1U, "the effect posts its terminal to the decision lane");
		decisions.run_next();
		expect(projection.Resolutions.back().Context == context && projection.Resolutions.back().Result.has_value()
				   && projection.Resolutions.back().Result->Thread == "PRRT_fixture_1"
				   && projection.Resolutions.back().Result->Resolved
				   && host.statistics().ThreadResolutions == 1U,
			   "the terminal names the thread in its new state");
		auto unresolve    = request;
		unresolve.Resolve = false;
		static_cast<void>(host.resolve(unresolve));
		effects.run_next();
		decisions.run_next();
		expect(observed->Resolved.size() == 2U && !observed->Resolved.back().Resolve
				   && projection.Resolutions.back().Result.has_value() && !projection.Resolutions.back().Result->Resolved,
			   "unresolve reaches the provider and answers the open thread");
		host.shutdown();
	}

	void test_live_binding_uses_exact_right_resolved_path_asynchronously()
	{
		auto  provider   = std::make_unique<fake_provider>();
		auto* observed   = provider.get();
		auto  effects    = application::manual_executor{ };
		auto  decisions  = application::manual_executor{ };
		auto  projection = recording_projection{ };
		auto  host       = application::review_mutation_application_host{
			std::move(provider),
			live_target(),
			acting_identity(),
			core::application_session_id{ 3U },
			core::request_generation{ 8U },
			testing_grant(),
			effects,
			decisions,
			projection,
			&projection,
		};

		const auto context = host.publish(publication_request());
		expect(context.Request.value() != 0U && effects.pending() == 1U,
			   "an admitted request is queued and returns immediately");
		expect(observed->Observations == 0U && projection.Published.empty(),
			   "no provider or projection work runs inline");
		effects.run_next();
		expect(observed->Observations == 2U && observed->Submitted.size() == 1U,
			   "the effect performs the two target checks and one submission");
		expect(decisions.pending() == 1U && projection.Published.empty(),
			   "the effect posts its terminal to the serialized decision lane");
		decisions.run_next();
		expect(projection.Published.size() == 1U
				   && projection.Published.front().Result.has_value(),
			   "one successful terminal is published");
		expect(observed->Submitted.front().LiveBinding.has_value()
				   && observed->Submitted.front().LiveBinding->Anchor.Path
					   == "renamed/current.cpp"
				   && observed->Submitted.front().LiveBinding->Anchor.Semantic.Side
					   == core::diff_side::right
				   && observed->Submitted.front().LiveBinding->Anchor.Commit
					   == target().ExpectedHead,
			   "the production-bound path uses the exact resolved right path and head");

		expect(observed->Submitted.front().Source
				   && observed->Submitted.front().Source->Endpoint == publication_request().SelectedDiff.RightEndpoint
				   && observed->Submitted.front().Source->Comparison == projection.Source->Material.Key
				   && observed->Submitted.front().Source->LineCount == *projection.Source->Material.RightLines
				   && observed->Submitted.front().Source->Placement == projection.Source->Placement,
			   "the provider receives the exact retained source and placement accepted by the host");

		const auto retry = host.publish(publication_request());
		effects.run_next();
		decisions.run_next();
		expect(retry != context && observed->Submitted.size() == 2U
				   && observed->Submitted[0].IdempotencyKey
					   == observed->Submitted[1].IdempotencyKey,
			   "a retry gets new host identity while preserving mutation idempotency identity");

		auto copied = publication_request();
		copied.SelectedDiff.Request.File.Change
			= application::workbench_file_change_kind::copied;
		copied.SelectedDiff.Request.File.CurrentPath  = "copied/current.cpp";
		copied.SelectedDiff.Request.File.PreviousPath = "source/original.cpp";
		copied.SelectedDiff.LeftPath                  = "copied/current.cpp";
		copied.SelectedDiff.RightPath                 = "copied/current.cpp";
		bind_resolved_endpoints(copied);
		projection.Source->File = copied.SelectedDiff.Request.File;
		static_cast<void>(host.publish(std::move(copied)));
		effects.run_next();
		decisions.run_next();
		expect(observed->Submitted.size() == 3U
				   && observed->Submitted.back().LiveBinding->Anchor.Path
					   == "copied/current.cpp",
			   "a copied file publishes only its exact epoch-resolved right endpoint");
	}

	void test_source_observations_fail_closed_before_provider_work()
	{
		auto  provider   = std::make_unique<fake_provider>();
		auto* observed   = provider.get();
		auto  effects    = application::manual_executor{ };
		auto  decisions  = application::manual_executor{ };
		auto  projection = recording_projection{ };
		auto  host       = application::review_mutation_application_host{
			std::move(provider), live_target(), acting_identity(),
			core::application_session_id{ 3U }, core::request_generation{ 8U },
			testing_grant(), effects, decisions, projection, &projection
		};
		const auto exact       = *projection.Source;
		const auto corruptions = std::vector<std::function<void(application::review_mutation_source_observation&)>>{
			[](auto& aSource) { aSource.Authority.Snapshot           = core::provider_snapshot_id{ 61U }; },
			[](auto& aSource) { aSource.Authority.IdentityGeneration = core::github_identity_generation{ 71U }; },
			[](auto& aSource) { aSource.File.File                    = opaque<core::file_identity>(45U); },
			[](auto& aSource) { aSource.File.CurrentPath             = "other.cpp"; },
			[](auto& aSource) { aSource.Material.Key.RightBlob       = oid<core::git_blob_oid>(51U); },
			[](auto& aSource) { aSource.Target.Semantic.ExpectedHead = oid<core::git_commit_oid>(31U); },
			[](auto& aSource) { aSource.ActingIdentity.NodeId        = "U_other"; },
			[](auto& aSource) { aSource.Material.RightLines.reset(); },
			[](auto& aSource) { aSource.Material.RightLines = 12U; },
			[](auto& aSource) { aSource.Material.RightLines = 0U; },
			[](auto& aSource) { aSource.Placement           = static_cast<core::review_placement>(99); },
		};
		const auto refused = [&] {
			static_cast<void>(host.publish(publication_request()));
			expect(effects.pending() == 0U && decisions.pending() == 1U,
				   "ineligible source is refused before provider work");
			decisions.run_next();
			expect(!projection.Published.back().Result, "ineligible source cannot publish");
		};
		projection.Source.reset();
		refused();
		for (const auto& corrupt : corruptions) {
			projection.Source = exact;
			corrupt(*projection.Source);
			refused();
		}
		expect(observed->Observations == 0U && observed->Submitted.empty(),
			   "absent catalogue and stale or unavailable material perform zero provider calls");
		projection.Source                      = exact;
		projection.Source->Material.RightLines = 13U;
		static_cast<void>(host.publish(publication_request()));
		expect(effects.pending() == 1U, "last valid line with unknown placement may negotiate natively");
		effects.run_next();
		decisions.run_next();
		expect(observed->Submitted.size() == 1U && projection.Published.back().Result,
			   "unknown placement is not fabricated rejection evidence");
		host.shutdown();
	}

	void test_attempted_failure_publishes_authoritative_reconciliation()
	{
		auto provider            = std::make_unique<fake_provider>();
		provider->NextSubmission = application::review_mutation_submission{
			.Result = std::unexpected(
				application::review_mutation_failure::unavailable),
			.Reconciliation = std::vector<application::review_comment_projection>{
				{ .ServerId = "PRRC_existing", .Body = "Server authority." },
			},
			.MutationAttempted = true,
		};
		auto effects    = application::manual_executor{ };
		auto decisions  = application::manual_executor{ };
		auto projection = recording_projection{ };
		auto host       = application::review_mutation_application_host{
			std::move(provider),
			live_target(),
			acting_identity(),
			core::application_session_id{ 3U },
			core::request_generation{ 8U },
			testing_grant(),
			effects,
			decisions,
			projection,
			&projection,
		};

		static_cast<void>(host.publish(publication_request()));
		effects.drain();
		decisions.drain();
		expect(projection.Published.size() == 1U
				   && projection.Published.front().MutationAttempted
				   && !projection.Published.front().Result.has_value()
				   && projection.Published.front().Reconciliation.has_value()
				   && projection.Published.front().Reconciliation->size() == 1U
				   && projection.Published.front().Reconciliation->front().ServerId
					   == "PRRC_existing",
			   "an attempted indeterminate terminal retains its real server snapshot");
	}

	void test_missing_grant_and_invalid_bindings_queue_no_effect()
	{
		auto provider   = fake_provider{ };
		auto effects    = application::manual_executor{ };
		auto decisions  = application::manual_executor{ };
		auto projection = recording_projection{ };
		auto host       = application::review_mutation_application_host{
			provider,
			live_target(),
			acting_identity(),
			core::application_session_id{ 3U },
			core::request_generation{ 8U },
			application::review_mutation_execution_grant{ },
			effects,
			decisions,
			projection,
			&projection,
		};
		static_cast<void>(host.publish(publication_request()));
		expect(effects.pending() == 0U && provider.Observations == 0U,
			   "a missing grant performs no provider work");
		decisions.run_next();
		expect(!projection.Published.front().Result.has_value()
				   && projection.Published.front().Result.error()
					   == application::review_mutation_failure::unavailable,
			   "a missing grant produces a typed unavailable terminal");

		auto grantedProvider   = fake_provider{ };
		auto grantedEffects    = application::manual_executor{ };
		auto grantedDecisions  = application::manual_executor{ };
		auto grantedProjection = recording_projection{ };
		auto granted           = application::review_mutation_application_host{
			grantedProvider,
			live_target(),
			acting_identity(),
			core::application_session_id{ 3U },
			core::request_generation{ 8U },
			testing_grant(),
			grantedEffects,
			grantedDecisions,
			grantedProjection,
			&grantedProjection,
		};
		auto left                         = publication_request();
		left.Selection.PreferredLine.Side = core::diff_side::left;
		left.Proposal.Anchor              = left.Selection.PreferredLine;
		static_cast<void>(granted.publish(left));
		auto absent = publication_request();
		absent.SelectedDiff.RightPath.reset();
		static_cast<void>(granted.publish(absent));
		auto arbitraryPath                   = publication_request();
		arbitraryPath.SelectedDiff.RightPath = "invented/third.cpp";
		static_cast<void>(granted.publish(arbitraryPath));
		auto stale = publication_request();
		stale.SelectedDiff.Request.Diff.Comparison.Right.Commit
			= oid<core::git_commit_oid>(31U);
		static_cast<void>(granted.publish(stale));
		expect(grantedEffects.pending() == 0U && grantedProvider.Observations == 0U,
			   "left, absent, arbitrary-path, and stale bindings perform no provider work");
		grantedDecisions.drain();
		expect(grantedProjection.Published.size() == 4U
				   && grantedProjection.Published[0].Result.error()
					   == application::review_mutation_failure::unsupported
				   && grantedProjection.Published[1].Result.error()
					   == application::review_mutation_failure::unsupported
				   && grantedProjection.Published[2].Result.error()
					   == application::review_mutation_failure::undesignated_target
				   && grantedProjection.Published[3].Result.error()
					   == application::review_mutation_failure::stale_head,
			   "each invalid request publishes its own typed refusal without supersession");
	}

	void test_cancellation_supersession_and_shutdown_are_quiescent()
	{
		auto provider   = fake_provider{ };
		auto effects    = application::manual_executor{ };
		auto decisions  = application::manual_executor{ };
		auto projection = recording_projection{ };
		auto host       = application::review_mutation_application_host{
			provider,
			live_target(),
			acting_identity(),
			core::application_session_id{ 3U },
			core::request_generation{ 8U },
			testing_grant(),
			effects,
			decisions,
			projection,
			&projection,
		};

		const auto first             = host.publish(publication_request());
		auto       secondRequest     = publication_request();
		secondRequest.Proposal.Draft = core::review_draft_id{ 8U };
		secondRequest.Proposal.Body  = "Second publication.";
		const auto second            = host.publish(secondRequest);
		expect(first != second && effects.pending() == 1U,
			   "accepted requests share one host-owned serialized drain");
		effects.drain();
		decisions.drain();
		expect(provider.Submitted.size() == 1U
				   && provider.Submitted.front().Proposal.Draft
					   == core::review_draft_id{ 8U }
				   && projection.Published.size() == 1U
				   && projection.Published.front().Context == second,
			   "only the current request can submit and publish a terminal");

		auto cancellation  = host.publish(publication_request());
		provider.OnObserve = [&host, cancellation] { host.cancel(cancellation); };
		effects.run_next();
		provider.OnObserve = { };
		decisions.run_next();
		expect(projection.Published.size() == 2U
				   && !projection.Published.back().Result.has_value()
				   && projection.Published.back().Result.error()
					   == application::review_mutation_failure::cancelled,
			   "cancellation between observations reaches a typed terminal");

		static_cast<void>(host.publish(publication_request()));
		host.shutdown();
		effects.drain();
		decisions.drain();
		expect(host.is_quiescent() && projection.Published.size() == 2U,
			   "shutdown stops and drains owned work without publishing stale terminals");
	}

	void test_parallel_executor_cannot_overlap_effects_and_shutdown_drains_queue()
	{
		auto provider   = barrier_provider{ };
		auto effects    = parallel_executor{ };
		auto decisions  = application::manual_executor{ };
		auto projection = recording_projection{ };
		auto host       = application::review_mutation_application_host{
			provider,
			live_target(),
			acting_identity(),
			core::application_session_id{ 3U },
			core::request_generation{ 8U },
			testing_grant(),
			effects,
			decisions,
			projection,
			&projection,
		};

		static_cast<void>(host.publish(publication_request()));
		provider.wait_until_entered();
		auto replacement           = publication_request();
		replacement.Proposal.Draft = core::review_draft_id{ 9U };
		replacement.Proposal.Body  = "Queued while the first effect is active.";
		static_cast<void>(host.publish(std::move(replacement)));
		expect(effects.posts() == 1U && host.outstanding_requests() == 2U,
			   "a parallel executor receives only one drain while replacement work queues");

		host.shutdown();
		provider.release();
		effects.join();
		decisions.drain();
		expect(provider.maximum_active() == 1U && provider.Submissions == 0U,
			   "host-owned draining keeps provider concurrency at one and stops queued work");
		expect(host.is_quiescent() && projection.Published.empty(),
			   "shutdown retires the active and queued requests without stale terminals");
	}

	void test_user_grants_bind_exact_requests_and_allow_successive_gestures()
	{
		auto  provider   = std::make_unique<fake_provider>();
		auto* observed   = provider.get();
		auto  effects    = application::manual_executor{ };
		auto  decisions  = application::manual_executor{ };
		auto  projection = recording_projection{ };
		auto  host       = application::review_mutation_application_host{
			std::move(provider), live_target(), acting_identity(), core::application_session_id{ 3U }, core::request_generation{ 8U }, { }, effects, decisions, projection, &projection
		};
		auto request = publication_request();
		auto issue   = [&](const application::review_mutation_publication_request& aRequest) {
			return application::make_review_mutation_testing_grant(
				core::application_session_id{ 3U }, core::request_generation{ 8U },
				live_target(), acting_identity(), aRequest);
		};
		expect(!application::detail::review_mutation_user_authority_issuer::issue(
				   core::application_session_id{ 3U }, core::request_generation{ 8U },
				   live_target(), acting_identity(), request),
			   "automated builds cannot issue normal production authority");
		auto grant   = issue(request);
		auto changed = request;
		changed.Proposal.Body += " substituted";
		static_cast<void>(host.publish(changed, grant));
		changed                = request;
		changed.Proposal.Draft = core::review_draft_id{ 900U };
		static_cast<void>(host.publish(changed, grant));
		changed              = request;
		changed.Acknowledged = !request.Acknowledged;
		static_cast<void>(host.publish(changed, grant));
		changed                        = request;
		changed.SelectedDiff.RightPath = "other.cpp";
		static_cast<void>(host.publish(changed, grant));
		expect(effects.pending() == 0U, "altered publication fields cannot consume the exact grant");
		decisions.drain();
		auto wrongActor = acting_identity();
		wrongActor.Login += "-other";
		auto wrongTarget = live_target();
		wrongTarget.Repository.Name += "-other";
		static_cast<void>(host.publish(request, application::make_review_mutation_testing_grant(core::application_session_id{ 3U }, core::request_generation{ 8U }, live_target(), wrongActor, request)));
		static_cast<void>(host.publish(request, application::make_review_mutation_testing_grant(core::application_session_id{ 3U }, core::request_generation{ 8U }, wrongTarget, acting_identity(), request)));
		static_cast<void>(host.publish(request, application::make_review_mutation_testing_grant(core::application_session_id{ 4U }, core::request_generation{ 8U }, live_target(), acting_identity(), request)));
		static_cast<void>(host.publish(request, application::make_review_mutation_testing_grant(core::application_session_id{ 3U }, core::request_generation{ 9U }, live_target(), acting_identity(), request)));
		expect(effects.pending() == 0U, "actor, target, session and generation cannot be substituted");
		decisions.drain();
		static_cast<void>(host.publish(request, grant));
		static_cast<void>(host.publish(request, grant));
		expect(effects.pending() == 1U, "one user grant admits exactly one request");
		effects.drain();
		decisions.drain();
		expect(observed->Submitted.size() == 1U, "replaying a user grant does not duplicate publication");
		request.Proposal.Body  = "A distinct second user comment.";
		request.Proposal.Draft = core::review_draft_id{ 8U };
		static_cast<void>(host.publish(request, issue(request)));
		effects.drain();
		decisions.drain();
		expect(observed->Submitted.size() == 2U, "another deliberate user gesture can publish another comment");
		host.shutdown();
		static_cast<void>(host.publish(request, issue(request)));
		expect(effects.pending() == 0U, "a fresh grant cannot revive a retired host");
	}

	void test_one_shot_authority_consumes_only_the_first_exact_admission()
	{
		auto  provider   = std::make_unique<fake_provider>();
		auto* observed   = provider.get();
		auto  effects    = application::manual_executor{ };
		auto  decisions  = application::manual_executor{ };
		auto  projection = recording_projection{ };
		auto  grant      = application::make_review_mutation_one_shot_testing_grant(
			live_target(),
			std::chrono::steady_clock::now() + std::chrono::minutes{ 5 });
		auto host = application::review_mutation_application_host{
			std::move(provider),
			live_target(),
			acting_identity(),
			core::application_session_id{ 3U },
			core::request_generation{ 8U },
			grant,
			effects,
			decisions,
			projection,
			&projection,
		};

		auto invalid                         = publication_request();
		invalid.Selection.PreferredLine.Side = core::diff_side::left;
		invalid.Proposal.Anchor              = invalid.Selection.PreferredLine;
		static_cast<void>(host.publish(std::move(invalid)));
		expect(effects.pending() == 0U && observed->Observations == 0U,
			   "an invalid exact binding neither consumes nor queues the one-shot authority");
		decisions.drain();

		const auto first = host.publish(publication_request());
		expect(first.Request.value() != 0U && effects.pending() == 1U,
			   "the first exact request consumes immediately before queue insertion");
		auto second           = publication_request();
		second.Proposal.Draft = core::review_draft_id{ 81U };
		second.Proposal.Body  = "A later request must not consume again.";
		static_cast<void>(host.publish(std::move(second)));
		expect(effects.pending() == 1U,
			   "a consumed authority refuses a second request without superseding the admitted effect");
		effects.drain();
		decisions.drain();
		expect(observed->Submitted.size() == 1U
				   && observed->Submitted.front().Proposal.Draft
					   == core::review_draft_id{ 7U },
			   "only the first exact request reaches the provider");
		expect(projection.Published.size() == 3U,
			   "invalid, consumed, and successful attempts each receive a typed terminal");
	}

	void test_one_shot_authority_expiry_invalidation_and_shutdown_revoke_grants()
	{
		auto expired = application::make_review_mutation_one_shot_testing_grant(
			live_target(),
			std::chrono::steady_clock::now() - std::chrono::seconds{ 1 });
		auto expiredProvider   = std::make_unique<fake_provider>();
		auto expiredEffects    = application::manual_executor{ };
		auto expiredDecisions  = application::manual_executor{ };
		auto expiredProjection = recording_projection{ };
		auto expiredHost       = application::review_mutation_application_host{
			std::move(expiredProvider),
			live_target(),
			acting_identity(),
			core::application_session_id{ 3U },
			core::request_generation{ 8U },
			expired,
			expiredEffects,
			expiredDecisions,
			expiredProjection,
			&expiredProjection,
		};
		static_cast<void>(expiredHost.publish(publication_request()));
		expect(expiredEffects.pending() == 0U,
			   "an expired one-shot authority queues no effect");

		{
			auto grant = application::make_review_mutation_one_shot_testing_grant(
				live_target(),
				std::chrono::steady_clock::now() + std::chrono::minutes{ 5 });
			auto provider   = std::make_unique<fake_provider>();
			auto effects    = application::manual_executor{ };
			auto decisions  = application::manual_executor{ };
			auto projection = recording_projection{ };
			auto host       = application::review_mutation_application_host{
				std::move(provider),
				live_target(),
				acting_identity(),
				core::application_session_id{ 3U },
				core::request_generation{ 8U },
				grant,
				effects,
				decisions,
				projection,
				&projection,
			};
			host.shutdown();

			auto replacementProvider   = std::make_unique<fake_provider>();
			auto replacementEffects    = application::manual_executor{ };
			auto replacementDecisions  = application::manual_executor{ };
			auto replacementProjection = recording_projection{ };
			auto replacementHost       = application::review_mutation_application_host{
				std::move(replacementProvider),
				live_target(),
				acting_identity(),
				core::application_session_id{ 3U },
				core::request_generation{ 8U },
				grant,
				replacementEffects,
				replacementDecisions,
				replacementProjection,
				&replacementProjection,
			};
			static_cast<void>(replacementHost.publish(publication_request()));
			expect(replacementEffects.pending() == 0U,
				   "shutdown revokes an unused one-shot authority");
		}

		{
			auto grant = application::make_review_mutation_one_shot_testing_grant(
				live_target(),
				std::chrono::steady_clock::now() + std::chrono::minutes{ 5 });
			auto provider   = std::make_unique<fake_provider>();
			auto effects    = application::manual_executor{ };
			auto decisions  = application::manual_executor{ };
			auto projection = recording_projection{ };
			auto host       = application::review_mutation_application_host{
				std::move(provider),
				live_target(),
				acting_identity(),
				core::application_session_id{ 3U },
				core::request_generation{ 8U },
				grant,
				effects,
				decisions,
				projection,
				&projection,
			};
			auto invalid                         = publication_request();
			invalid.Selection.PreferredLine.Side = core::diff_side::left;
			invalid.Proposal.Anchor              = invalid.Selection.PreferredLine;
			const auto context                   = host.publish(std::move(invalid));
			host.cancel(context);
			static_cast<void>(host.publish(publication_request()));
			expect(effects.pending() == 1U,
				   "cancelling an invalid refusal does not revoke an unused one-shot authority");
		}

		auto grant = application::make_review_mutation_one_shot_testing_grant(
			live_target(),
			std::chrono::steady_clock::now() + std::chrono::minutes{ 5 });
		auto providerOne   = std::make_unique<fake_provider>();
		auto effectsOne    = application::manual_executor{ };
		auto decisionsOne  = application::manual_executor{ };
		auto projectionOne = recording_projection{ };
		auto hostOne       = application::review_mutation_application_host{
			std::move(providerOne),
			live_target(),
			acting_identity(),
			core::application_session_id{ 3U },
			core::request_generation{ 8U },
			grant,
			effectsOne,
			decisionsOne,
			projectionOne,
			&projectionOne,
		};
		auto providerTwo   = std::make_unique<fake_provider>();
		auto effectsTwo    = application::manual_executor{ };
		auto decisionsTwo  = application::manual_executor{ };
		auto projectionTwo = recording_projection{ };
		auto hostTwo       = application::review_mutation_application_host{
			std::move(providerTwo),
			live_target(),
			acting_identity(),
			core::application_session_id{ 4U },
			core::request_generation{ 8U },
			grant,
			effectsTwo,
			decisionsTwo,
			projectionTwo,
			&projectionTwo,
		};
		static_cast<void>(hostOne.publish(publication_request()));
		expect(effectsOne.pending() == 1U && effectsTwo.pending() == 0U,
			   "a mismatched second host cannot revoke the first exact session binding");
	}

	void test_one_shot_authority_rejects_same_core_different_full_identity()
	{
		auto grant = application::make_review_mutation_one_shot_testing_grant(
			live_target(),
			std::chrono::steady_clock::now() + std::chrono::minutes{ 5 });
		auto wrongTarget = live_target();
		wrongTarget.RepositoryNodeId += "-substituted";
		auto wrongProvider   = std::make_unique<fake_provider>();
		auto wrongEffects    = application::manual_executor{ };
		auto wrongDecisions  = application::manual_executor{ };
		auto wrongProjection = recording_projection{ };
		auto wrongHost       = application::review_mutation_application_host{
			std::move(wrongProvider),
			wrongTarget,
			acting_identity(),
			core::application_session_id{ 3U },
			core::request_generation{ 8U },
			grant,
			wrongEffects,
			wrongDecisions,
			wrongProjection,
			&wrongProjection,
		};
		static_cast<void>(wrongHost.publish(publication_request()));
		expect(wrongEffects.pending() == 0U,
			   "same core identity with different provider target identity is refused");

		auto exactProvider   = std::make_unique<fake_provider>();
		auto exactEffects    = application::manual_executor{ };
		auto exactDecisions  = application::manual_executor{ };
		auto exactProjection = recording_projection{ };
		auto exactHost       = application::review_mutation_application_host{
			std::move(exactProvider),
			live_target(),
			acting_identity(),
			core::application_session_id{ 3U },
			core::request_generation{ 8U },
			grant,
			exactEffects,
			exactDecisions,
			exactProjection,
			&exactProjection,
		};

		auto wrongActor = acting_identity();
		wrongActor.NodeId += "-substituted";
		auto actorProvider   = std::make_unique<fake_provider>();
		auto actorEffects    = application::manual_executor{ };
		auto actorDecisions  = application::manual_executor{ };
		auto actorProjection = recording_projection{ };
		auto actorHost       = application::review_mutation_application_host{
			std::move(actorProvider),
			live_target(),
			wrongActor,
			core::application_session_id{ 3U },
			core::request_generation{ 8U },
			grant,
			actorEffects,
			actorDecisions,
			actorProjection,
			&actorProjection,
		};
		static_cast<void>(actorHost.publish(publication_request()));
		expect(actorEffects.pending() == 0U,
			   "the carrier seals to the first fresh authenticated actor and rejects a different full identity");
		static_cast<void>(exactHost.publish(publication_request()));
		expect(exactEffects.pending() == 1U,
			   "invalid target and later actor bind attempts do not revoke the exact sealed authority");
	}
	/// [[BPR-1082]]. RFC-BPR-0048 R42. A label chosen through the draft owner is
	/// part of the exact prepared request. While the draft is publishing the
	/// label cannot change, so a retry after a lost answer repeats the same
	/// body and key; after a definite failure a new label is a new request
	/// that the earlier grant cannot carry.
	void test_a_label_changed_after_preparation_needs_a_new_gesture()
	{
		auto  provider   = std::make_unique<fake_provider>();
		auto* observed   = provider.get();
		auto  effects    = application::manual_executor{ };
		auto  decisions  = application::manual_executor{ };
		auto  projection = recording_projection{ };
		auto  host       = application::review_mutation_application_host{
			std::move(provider), live_target(), acting_identity(), core::application_session_id{ 3U }, core::request_generation{ 8U }, { }, effects, decisions, projection, &projection
		};
		const auto issue = [](const application::review_mutation_publication_request& aRequest) {
			return application::make_review_mutation_testing_grant(core::application_session_id{ 3U }, core::request_generation{ 8U }, live_target(), acting_identity(), aRequest);
		};
		const auto step = [](const core::revisioned_review_draft& aDraft, core::review_draft_command_kind aKind) {
			const auto outcome = core::apply(aDraft, { .Command = { .Id = aDraft.Draft.Id, .Kind = aKind }, .ExpectedRevision = aDraft.Revision, .Scope = aDraft.Scope });
			expect(outcome.Changed, "the draft owner accepts the step");
			return outcome.Accepted;
		};
		const auto label = [](const core::revisioned_review_draft& aDraft, core::feedback_label aLabel) {
			const auto plan = core::plan_feedback_draft_edit(aDraft, core::feedback_draft_basis_of(aDraft), { .Change = core::feedback_label_change::set, .Label = aLabel });
			expect(plan.Command.has_value(), "the label is planned");
			const auto outcome = core::apply(aDraft, *plan.Command);
			expect(outcome.Changed, "the draft owner accepts the label");
			return outcome.Accepted;
		};
		const auto prepare = [](const core::revisioned_review_draft& aDraft) {
			auto request      = publication_request();
			auto proposal     = core::propose_publication(aDraft.Draft, core::review_disposition::comment);
			proposal->Durable = core::durable_review_publication_identity{ .Incarnation = opaque<core::private_ledger_incarnation>(9U).bytes(), .DraftRevision = aDraft.Revision };
			request.Proposal  = *proposal;
			return request;
		};

		auto draft       = core::revisioned_review_draft{ .Draft = { .Id = core::review_draft_id{ 7U }, .Anchor = publication_request().Proposal.Anchor, .Body = "Please rename this." }, .Revision = 1U, .Scope = core::review_draft_scope_id{ 1U } };
		draft            = step(label(draft, core::feedback_label::issue), core::review_draft_command_kind::begin_publish);
		const auto first = prepare(draft);
		expect(first.Proposal.Body == "issue: Please rename this.", "the prepared request carries the labelled body");

		observed->NextSubmission = application::review_mutation_submission{ .Result = std::unexpected(application::review_mutation_failure::unavailable), .MutationAttempted = true };
		static_cast<void>(host.publish(first, issue(first)));
		effects.drain();
		decisions.drain();
		expect(observed->Submitted.size() == 1U && !projection.Published.back().Result.has_value() && projection.Published.back().MutationAttempted, "the answer is lost after transmission");
		const auto held = core::plan_feedback_draft_edit(draft, core::feedback_draft_basis_of(draft), { .Change = core::feedback_label_change::set, .Label = core::feedback_label::question });
		expect(held.Refusal == core::feedback_draft_edit_refusal::not_editable && !held.Command, "the label cannot change while the outcome is unknown");
		const auto retry = prepare(draft);
		expect(retry == first, "recovery prepares the identical request");
		static_cast<void>(host.publish(retry, issue(retry)));
		effects.drain();
		decisions.drain();
		expect(observed->Submitted.size() == 2U && observed->Submitted[1].IdempotencyKey == observed->Submitted[0].IdempotencyKey && observed->Submitted[1].Proposal.Body == observed->Submitted[0].Proposal.Body, "the retry carries the same body and idempotency key");

		// A definite failure returns the draft; a new label is a new action.
		draft             = step(draft, core::review_draft_command_kind::fail_publish);
		draft             = step(label(draft, core::feedback_label::question), core::review_draft_command_kind::begin_publish);
		const auto second = prepare(draft);
		expect(second.Proposal.Body == "question: Please rename this." && second.Proposal.Durable != first.Proposal.Durable, "the relabelled request is bound to a newer draft revision");
		static_cast<void>(host.publish(second, issue(first)));
		expect(effects.pending() == 0U, "the grant for the earlier request cannot carry the relabelled body");
		decisions.drain();
		static_cast<void>(host.publish(second, issue(second)));
		effects.drain();
		decisions.drain();
		expect(observed->Submitted.size() == 3U && observed->Submitted[2].Proposal.Body == "question: Please rename this." && observed->Submitted[2].IdempotencyKey != observed->Submitted[0].IdempotencyKey, "a new gesture publishes the new body under a new key");
	}
} // namespace

auto main() -> int
{
	test_user_grants_bind_exact_requests_and_allow_successive_gestures();
	test_source_observations_fail_closed_before_provider_work();
	test_live_binding_uses_exact_right_resolved_path_asynchronously();
	test_attempted_failure_publishes_authoritative_reconciliation();
	test_missing_grant_and_invalid_bindings_queue_no_effect();
	test_cancellation_supersession_and_shutdown_are_quiescent();
	test_parallel_executor_cannot_overlap_effects_and_shutdown_drains_queue();
	test_one_shot_authority_consumes_only_the_first_exact_admission();
	test_one_shot_authority_expiry_invalidation_and_shutdown_revoke_grants();
	test_one_shot_authority_rejects_same_core_different_full_identity();
	test_thread_resolution_needs_the_capability_and_a_permitting_grant();
	test_a_reply_may_be_drafted_from_a_left_side_selection();
	test_a_base_line_root_needs_githubs_base_numbering();
	test_a_label_changed_after_preparation_needs_a_new_gesture();
	std::cout << "review-mutation-application-host-tests:pass\n";
	return EXIT_SUCCESS;
}
