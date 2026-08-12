#include "live_review_mutation_provider.hpp"
#include "review_mutation_provider_admission.hpp"

#include "bpr/adapters/sqlite/review_mutation_recovery_store.hpp"
#include "bpr/application/review_mutation_coordinator.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;
	using namespace bpr::adapters::github;
	using namespace bpr::adapters::github::detail;

	class fake_review_mutation_lease final
		: public application::review_mutation_transport_lease
	{
	};

	[[nodiscard]] auto mutation_lease()
		-> std::shared_ptr<fake_review_mutation_lease>
	{
		return std::make_shared<fake_review_mutation_lease>();
	}

	static_assert(std::is_base_of_v<
				  application::review_mutation_port,
				  live_review_mutation_provider>);

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
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes.fill(aSeed);
		return Identity{ bytes };
	}

	[[nodiscard]] auto commit(const std::uint8_t aSeed) -> core::git_commit_oid
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		std::fill_n(bytes.begin(), 20U, aSeed);
		return core::git_commit_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto target() -> application::review_mutation_target_binding
	{
		return {
			.Semantic = {
				.Host         = core::git_host_id{ 11U },
				.Repository   = core::repository_id{ 12U },
				.PullRequest  = core::pull_request_id{ 13U },
				.ExpectedHead = commit(0x11U),
			},
			.Hostname                 = application::github_canonical_hostname{ "github.com" },
			.ServerContract           = application::github_server_contract::github_com_2022_11_28,
			.Repository               = { .Owner = "MikeMarcin", .Name = "BPR-Test" },
			.Number                   = core::pull_request_number{ 1U },
			.RepositoryDatabaseId     = 1'328'277'631U,
			.RepositoryNodeId         = "R_kgDOTyvofw",
			.PullRequestDatabaseId    = 4'246'446'933U,
			.PullRequestNodeId        = "PR_kwDOTyvof879G6NV",
			.BaseRepositoryDatabaseId = 1'328'277'631U,
			.BaseOid                  = commit(0x10U),
			.BaseRef                  = "main",
			.HeadRef                  = "codex/disposable-live-review-20260810",
			.Open                     = true,
		};
	}

	[[nodiscard]] auto identity() -> application::review_mutation_acting_identity
	{
		return {
			.Account    = core::github_account_id{ 21U },
			.DatabaseId = 22U,
			.NodeId     = "MDQ6VXNlcjIy",
			.Login      = "fixture-reviewer",
		};
	}

	[[nodiscard]] auto credential_context()
		-> application::github_credential_operation_context
	{
		return {
			.Request            = core::request_id{ 31U },
			.ApplicationSession = core::application_session_id{ 32U },
			.Authority          = {
				.Profile         = core::host_profile_id{ 33U },
				.ProfileRevision = application::github_profile_revision{ 34U },
				.Host            = target().Semantic.Host,
				.Hostname        = application::github_canonical_hostname{ "github.com" },
				.Account         = identity().Account,
				.ServerContract  = application::github_server_contract::github_com_2022_11_28,
			},
			.IdentityGeneration = core::github_identity_generation{ 35U },
		};
	}

	[[nodiscard]] auto configuration() -> live_review_mutation_configuration
	{
		return {
			.CredentialContext = credential_context(),
			.Target            = target(),
			.ActingIdentity    = identity(),
		};
	}

	[[nodiscard]] auto request() -> application::review_mutation_request
	{
		const auto proposal = core::review_publication_proposal{
			.Draft  = core::review_draft_id{ 41U },
			.Anchor = {
				.Comparison = opaque<core::file_comparison_digest>(0x22U),
				.Side       = core::diff_side::right,
				.Line       = 12U,
			},
			.Body        = "Please rename this.",
			.Disposition = core::review_disposition::comment,
		};
		return {
			.Proposal       = proposal,
			.Target         = target().Semantic,
			.ActingIdentity = identity().Account,
			.IdempotencyKey = application::idempotency_key_for(proposal, target().Semantic),
			.LiveBinding    = application::review_mutation_live_binding{
				.Target         = target(),
				.ActingIdentity = identity(),
				.Anchor         = {
					.Semantic = proposal.Anchor,
					.Path     = "fixture/review.txt",
					.Commit   = target().Semantic.ExpectedHead,
				},
			},
		};
	}

	[[nodiscard]] auto request_at_path(std::string aPath)
		-> application::review_mutation_request
	{
		auto value                     = request();
		value.LiveBinding->Anchor.Path = std::move(aPath);
		return value;
	}

	[[nodiscard]] auto target_response(
		const std::uint64_t         aRepository = 1'328'277'631U,
		const core::git_commit_oid& aHead       = target().Semantic.ExpectedHead)
		-> github_mutation_wire_completion
	{
		return github_mutation_wire_success{
			.Response = {
				.Status = 200,
				.Body   = std::string{ "{\"id\":4246446933,"
									   "\"node_id\":\"PR_kwDOTyvof879G6NV\",\"number\":1,"
									   "\"state\":\"open\",\"head\":{\"sha\":\"" }
					+ std::string{ core::canonical_hex(aHead).view() }
					+ "\",\"ref\":\"codex/disposable-live-review-20260810\"},"
					  "\"base\":{\"sha\":\""
					+ std::string{ core::canonical_hex(target().BaseOid).view() }
					+ "\",\"ref\":\"main\",\"repo\":{\"id\":"
					+ std::to_string(aRepository)
					+ ",\"node_id\":\"R_kgDOTyvofw\","
					  "\"full_name\":\"MikeMarcin/BPR-Test\"}}}",
			},
		};
	}

	[[nodiscard]] auto viewer_response() -> github_mutation_wire_completion
	{
		return github_mutation_wire_success{
			.Response = {
				.Status = 200,
				.Body   = "{\"login\":\"fixture-reviewer\",\"id\":22,"
						  "\"node_id\":\"MDQ6VXNlcjIy\"}",
			},
		};
	}

	[[nodiscard]] auto comment_response(const std::size_t aId = 44U)
		-> github_mutation_wire_completion
	{
		return github_mutation_wire_success{
			.Response = {
				.Status = 201,
				.Body   = std::string{ "{\"id\":" } + std::to_string(aId)
					+ ",\"node_id\":\"PRRC_fixture_" + std::to_string(aId)
					+ "\","
					  "\"path\":\"fixture/review.txt\",\"commit_id\":\""
					+ std::string{ core::canonical_hex(target().Semantic.ExpectedHead).view() }
					+ "\",\"side\":\"RIGHT\",\"line\":13,"
					  "\"body\":\"Please rename this.\",\"user\":{"
					  "\"login\":\"fixture-reviewer\",\"id\":22,"
					  "\"node_id\":\"MDQ6VXNlcjIy\"}}",
			},
		};
	}

	[[nodiscard]] auto comment_list_response(
		const std::size_t aExactCount = 1U,
		const std::size_t aFirstId    = 44U)
		-> github_mutation_wire_completion
	{
		auto body = std::string{ "[" };
		for (auto index = std::size_t{ }; index < aExactCount; ++index) {
			if (index != 0U) {
				body.push_back(',');
			}
			body += std::string{ "{\"id\":" } + std::to_string(aFirstId + index)
				+ ",\"node_id\":\"PRRC_fixture_" + std::to_string(aFirstId + index)
				+ "\",\"path\":\"fixture/review.txt\",\"commit_id\":\""
				+ std::string{ core::canonical_hex(target().Semantic.ExpectedHead).view() }
				+ "\",\"side\":\"RIGHT\",\"line\":13,"
				  "\"body\":\"Please rename this.\",\"user\":{"
				  "\"login\":\"fixture-reviewer\",\"id\":22,"
				  "\"node_id\":\"MDQ6VXNlcjIy\"}}";
		}
		body.push_back(']');
		return github_mutation_wire_success{
			.Response = {
				.Status = 200,
				.Body   = std::move(body),
			},
		};
	}

	struct fake_recovery_state
	{
		std::map<std::string, application::review_mutation_recovery_record> Records;
		bool                                                                RejectTransmissionMark = false;
	};

	class fake_recovery final : public application::review_mutation_recovery_port
	{
	public:
		explicit fake_recovery(std::shared_ptr<fake_recovery_state> aState)
			: State{ std::move(aState) }
		{
		}

		auto load(const std::string& aKey)
			-> std::expected<
				std::optional<application::review_mutation_recovery_record>,
				application::review_mutation_recovery_failure> override
		{
			if (const auto found = State->Records.find(aKey);
				found != State->Records.end()) {
				return found->second;
			}
			return std::optional<application::review_mutation_recovery_record>{ };
		}

		auto admit(const application::review_mutation_recovery_record& aRecord)
			-> std::expected<
				application::review_mutation_recovery_admission,
				application::review_mutation_recovery_failure> override
		{
			if (const auto found = State->Records.find(aRecord.IdempotencyKey);
				found != State->Records.end()) {
				return application::review_mutation_recovery_admission{
					.Record = found->second,
				};
			}
			State->Records.emplace(aRecord.IdempotencyKey, aRecord);
			return application::review_mutation_recovery_admission{
				.Record   = aRecord,
				.Inserted = true,
			};
		}

		auto mark_possibly_transmitted(
			const std::string&                  aKey,
			const std::array<std::uint8_t, 32>& aFingerprint)
			-> std::expected<
				application::review_mutation_recovery_record,
				application::review_mutation_recovery_failure> override
		{
			if (State->RejectTransmissionMark) {
				return std::unexpected(
					application::review_mutation_recovery_failure::unavailable);
			}
			const auto found = State->Records.find(aKey);
			if (found == State->Records.end()
				|| found->second.RequestFingerprint != aFingerprint
				|| !found->second.BaselineExactCommentIds.has_value()
				|| found->second.State
					!= application::review_mutation_recovery_state::unresolved) {
				return std::unexpected(
					application::review_mutation_recovery_failure::conflict);
			}
			found->second.Transmission
				= application::review_mutation_recovery_transmission::possibly_transmitted;
			return found->second;
		}

		auto resolve(
			const std::string&                          aKey,
			const std::array<std::uint8_t, 32>&         aFingerprint,
			const application::review_mutation_receipt& aReceipt)
			-> std::expected<
				application::review_mutation_recovery_record,
				application::review_mutation_recovery_failure> override
		{
			const auto found = State->Records.find(aKey);
			if (found == State->Records.end()
				|| found->second.RequestFingerprint != aFingerprint
				|| !found->second.BaselineExactCommentIds.has_value()
				|| found->second.Transmission
					!= application::review_mutation_recovery_transmission::possibly_transmitted) {
				return std::unexpected(
					application::review_mutation_recovery_failure::conflict);
			}
			if (std::ranges::binary_search(
					*found->second.BaselineExactCommentIds,
					aReceipt.ServerCommentId)) {
				return std::unexpected(
					application::review_mutation_recovery_failure::conflict);
			}
			found->second.State   = application::review_mutation_recovery_state::succeeded;
			found->second.Receipt = aReceipt;
			return found->second;
		}

	private:
		std::shared_ptr<fake_recovery_state> State;
	};

	[[nodiscard]] auto make_recovery(
		std::shared_ptr<fake_recovery_state> aState
		= std::make_shared<fake_recovery_state>())
		-> std::unique_ptr<application::review_mutation_recovery_port>
	{
		return std::make_unique<fake_recovery>(std::move(aState));
	}

	[[nodiscard]] auto open_recovery(const std::filesystem::path& aRoot)
		-> std::unique_ptr<application::review_mutation_recovery_port>
	{
		auto opened = adapters::sqlite::make_review_mutation_recovery_store({
			.StorageRoot = aRoot,
			.Host        = "github.com",
			.Account     = std::to_string(identity().Account.value()),
			.Repository  = "MikeMarcin/BPR-Test",
			.PullRequest = 1,
		});
		expect(opened.has_value(), "persistent recovery store opens for exact scope");
		return std::move(*opened);
	}

	class fake_transport final : public github_mutation_wire_transport
	{
	public:
		[[nodiscard]] auto perform_review_mutation(
			const github_mutation_wire_request&  aRequest,
			const std::stop_token                aStop,
			github_mutation_transmission_witness aWitness)
			-> github_mutation_wire_completion override
		{
			Requests.push_back(aRequest);
			StopObserved.push_back(aStop.stop_requested());
			if (OnPerform) {
				OnPerform(Requests.size());
			}
			if (Completions.empty()) {
				return github_mutation_wire_error{
					.Failure = application::review_mutation_failure::unavailable,
				};
			}
			auto next = std::move(Completions.front());
			Completions.pop_front();
			if (aRequest.Wire.Operation
				== review_mutation_operation::create_review_comment) {
				if (ThrowBeforePostWitness) {
					throw std::runtime_error{ "simulated admission-to-post crash" };
				}
				const auto possiblyTransmitted
					= std::holds_alternative<github_mutation_wire_success>(next)
					|| (std::holds_alternative<github_mutation_wire_error>(next)
						&& std::get<github_mutation_wire_error>(next).Transmission
							== github_mutation_transmission::indeterminate)
					|| (std::holds_alternative<github_mutation_wire_cancelled>(next)
						&& std::get<github_mutation_wire_cancelled>(next).Transmission
							== github_mutation_transmission::indeterminate);
				if (possiblyTransmitted && (!aWitness || !aWitness())) {
					return github_mutation_wire_error{
						.Failure      = application::review_mutation_failure::unavailable,
						.Transmission = github_mutation_transmission::indeterminate,
					};
				}
				if (possiblyTransmitted && RepeatWitness && aWitness()) {
					return github_mutation_wire_error{
						.Failure      = application::review_mutation_failure::unavailable,
						.Transmission = github_mutation_transmission::indeterminate,
					};
				}
			}
			return next;
		}

		void exact_submit_course()
		{
			Completions.push_back(comment_list_response(0U));
			Completions.push_back(viewer_response());
			Completions.push_back(target_response());
			Completions.push_back(comment_response());
			Completions.push_back(comment_list_response());
		}

		std::deque<github_mutation_wire_completion> Completions;
		std::vector<github_mutation_wire_request>   Requests;
		std::vector<bool>                           StopObserved;
		std::function<void(std::size_t)>            OnPerform;
		bool                                        ThrowBeforePostWitness = false;
		bool                                        RepeatWitness          = false;
	};

	class fake_github_review_mutation_lease final
		: public github_review_mutation_transport_lease
	{
	public:
		explicit fake_github_review_mutation_lease(
			github_mutation_wire_transport& aTransport)
			: Profile{
				.Profile               = configuration().CredentialContext.Authority.Profile,
				.ProfileRevision       = configuration().CredentialContext.Authority.ProfileRevision,
				.Host                  = configuration().CredentialContext.Authority.Host,
				.Hostname              = configuration().CredentialContext.Authority.Hostname,
				.ServerContract        = configuration().CredentialContext.Authority.ServerContract,
				.Authentication        = application::github_authentication_transport::native_https,
				.ProbeContractRevision = application::github_probe_contract_revision{ 1U },
			}
			, Context{ configuration().CredentialContext }
			, Transport{ &aTransport }
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
			return Context;
		}

		[[nodiscard]] auto mutation_transport() noexcept
			-> github_mutation_wire_transport* override
		{
			return active() ? Transport : nullptr;
		}

	private:
		application::github_profile_authority            Profile;
		application::github_credential_operation_context Context;
		github_mutation_wire_transport*                  Transport = nullptr;
	};

	void test_observe_then_immediate_revalidate_then_submit()
	{
		auto transport = fake_transport{ };
		transport.exact_submit_course();
		auto session = review_mutation_session{
			transport, configuration(), mutation_lease(), make_recovery()
		};
		const auto receipt = session.submit(request());
		expect(receipt.has_value() && receipt->ServerCommentId == "PRRC_fixture_44",
			   "exact publication succeeds");
		expect(transport.Requests.size() == 5U
				   && transport.Requests[0].Wire.Operation
					   == review_mutation_operation::observe_review_comments
				   && transport.Requests[1].Wire.Operation
					   == review_mutation_operation::observe_viewer
				   && transport.Requests[2].Wire.Operation
					   == review_mutation_operation::observe_pull_request
				   && transport.Requests[3].Wire.Operation
					   == review_mutation_operation::create_review_comment
				   && transport.Requests[4].Wire.Operation
					   == review_mutation_operation::observe_review_comments
				   && transport.Requests[4].Wire.Path.ends_with("/comments?per_page=100"),
			   "the baseline precedes admission while exact authority remains final before post");
		expect(std::ranges::all_of(transport.Requests, [](const auto& aWire) {
				   return aWire.Context == credential_context();
			   }),
			   "every operation carries the exact credential authority without material");
	}

	void test_invalid_bindings_submit_nothing()
	{
		for (auto variant = 0; variant < 29; ++variant) {
			auto transport = fake_transport{ };
			auto session   = review_mutation_session{
				transport, configuration(), mutation_lease(), make_recovery()
			};
			auto changed = request();
			switch (variant) {
			case 0:
				changed.LiveBinding.reset();
				break;
			case 1:
				changed.LiveBinding->Target.Semantic.Host = core::git_host_id{ 99U };
				break;
			case 2:
				changed.LiveBinding->Target.Semantic.Repository = core::repository_id{ 99U };
				break;
			case 3:
				changed.LiveBinding->Target.Semantic.PullRequest = core::pull_request_id{ 99U };
				break;
			case 4:
				changed.LiveBinding->Target.Semantic.ExpectedHead = commit(0x44U);
				break;
			case 5:
				changed.LiveBinding->Target.Repository.Owner = "Other";
				break;
			case 6:
				changed.LiveBinding->Target.Repository.Name = "Other";
				break;
			case 7:
				changed.LiveBinding->Target.Number = core::pull_request_number{ 2U };
				break;
			case 8:
				changed.LiveBinding->Target.RepositoryDatabaseId += 1U;
				break;
			case 9:
				changed.LiveBinding->Target.RepositoryNodeId = "R_other";
				break;
			case 10:
				changed.LiveBinding->Target.PullRequestDatabaseId += 1U;
				break;
			case 11:
				changed.LiveBinding->Target.PullRequestNodeId = "PR_other";
				break;
			case 12:
				changed.LiveBinding->Target.BaseRepositoryDatabaseId += 1U;
				break;
			case 13:
				changed.LiveBinding->Target.Open = false;
				break;
			case 14:
				changed.LiveBinding->ActingIdentity.Account = core::github_account_id{ 99U };
				break;
			case 15:
				changed.LiveBinding->ActingIdentity.DatabaseId += 1U;
				break;
			case 16:
				changed.LiveBinding->ActingIdentity.NodeId = "MDQ6Other";
				break;
			case 17:
				changed.LiveBinding->ActingIdentity.Login = "other";
				break;
			case 18:
				changed.LiveBinding->Anchor.Semantic.Comparison
					= opaque<core::file_comparison_digest>(0x55U);
				break;
			case 19:
				changed.LiveBinding->Anchor.Semantic.Side = core::diff_side::left;
				break;
			case 20:
				changed.LiveBinding->Anchor.Semantic.Line += 1U;
				break;
			case 21:
				changed.LiveBinding->Anchor.Path = "/absolute";
				break;
			case 22:
				changed.LiveBinding->Anchor.Commit = commit(0x66U);
				break;
			case 23:
				changed.Proposal.Anchor.Line += 1U;
				break;
			case 24:
				changed.Target.Repository = core::repository_id{ 99U };
				break;
			case 25:
				changed.ActingIdentity = core::github_account_id{ 99U };
				break;
			case 26:
				changed.IdempotencyKey.assign(64U, '0');
				break;
			case 27:
				changed.Proposal.Disposition = core::review_disposition::approve;
				break;
			case 28:
				changed.Proposal.Body.clear();
				break;
			default:
				std::abort();
			}
			const auto refused = session.submit(changed);
			expect(!refused.has_value() && transport.Requests.empty(),
				   "invalid request produces zero transport submissions");
		}
	}

	void test_revalidation_drift_and_identity_failure_stop_before_write()
	{
		auto drift = fake_transport{ };
		drift.Completions.push_back(comment_list_response(0U));
		drift.Completions.push_back(viewer_response());
		drift.Completions.push_back(target_response(1'328'277'632U));
		auto driftSession = review_mutation_session{
			drift, configuration(), mutation_lease(), make_recovery()
		};
		const auto refused = driftSession.submit(request());
		expect(!refused.has_value() && drift.Requests.size() == 3U,
			   "repository drift stops before write");

		auto movedHead = fake_transport{ };
		movedHead.Completions.push_back(comment_list_response(0U));
		movedHead.Completions.push_back(viewer_response());
		movedHead.Completions.push_back(target_response(
			1'328'277'631U, commit(0x77U)));
		auto movedSession = review_mutation_session{
			movedHead, configuration(), mutation_lease(), make_recovery()
		};
		const auto stale = movedSession.submit(request());
		expect(!stale.has_value()
				   && stale.error() == application::review_mutation_failure::stale_head
				   && movedHead.Requests.size() == 3U,
			   "a fresh moved-head observation stops before write");

		auto identityFailure = fake_transport{ };
		identityFailure.Completions.push_back(comment_list_response(0U));
		identityFailure.Completions.emplace_back(github_mutation_wire_success{
			.Response = {
				.Status = 200,
				.Body   = "{\"login\":\"other\",\"id\":22,"
						  "\"node_id\":\"MDQ6VXNlcjIy\"}",
			},
		});
		auto identitySession = review_mutation_session{
			identityFailure, configuration(), mutation_lease(), make_recovery()
		};
		const auto wrongActor = identitySession.submit(request());
		expect(!wrongActor.has_value() && identityFailure.Requests.size() == 2U,
			   "acting identity drift stops before write");
	}

	void test_idempotency_and_indeterminate_attempts_never_duplicate()
	{
		auto transport = fake_transport{ };
		transport.exact_submit_course();
		auto session = review_mutation_session{
			transport, configuration(), mutation_lease(), make_recovery()
		};
		const auto first              = session.submit(request());
		const auto requestsAfterFirst = transport.Requests.size();
		const auto duplicate          = session.submit(request());
		expect(first.has_value() && duplicate.has_value() && duplicate->Duplicate
				   && duplicate->ServerCommentId == first->ServerCommentId
				   && transport.Requests.size() == requestsAfterFirst,
			   "known success returns the same identity without a second post");
		const auto pathB = session.submit(request_at_path("fixture/other.txt"));
		expect(!pathB.has_value()
				   && transport.Requests.size() == requestsAfterFirst,
			   "a reused key with a different resolved path cannot reuse a receipt or submit");

		auto recovery  = std::make_shared<fake_recovery_state>();
		auto uncertain = fake_transport{ };
		uncertain.Completions.push_back(comment_list_response());
		uncertain.Completions.push_back(viewer_response());
		uncertain.Completions.push_back(target_response());
		uncertain.Completions.emplace_back(github_mutation_wire_error{
			.Failure      = application::review_mutation_failure::unavailable,
			.Transmission = github_mutation_transmission::indeterminate,
		});
		uncertain.Completions.push_back(comment_list_response());
		auto uncertainSession = review_mutation_session{
			uncertain, configuration(), mutation_lease(), make_recovery(recovery)
		};
		const auto lost = uncertainSession.submit(request());
		expect(!lost.has_value() && uncertain.Requests.size() == 5U,
			   "a witnessed indeterminate POST with only C0 remains unresolved");

		auto restarted = fake_transport{ };
		restarted.Completions.push_back(viewer_response());
		restarted.Completions.push_back(target_response());
		restarted.Completions.push_back(comment_list_response(2U));
		auto restartedSession = review_mutation_session{
			restarted, configuration(), mutation_lease(), make_recovery(recovery)
		};
		const auto recovered = restartedSession.submit(request());
		const auto count     = restarted.Requests.size();
		const auto otherPath = restartedSession.submit(
			request_at_path("fixture/other.txt"));
		expect(recovered.has_value() && recovered->Duplicate
				   && recovered->ServerCommentId == "PRRC_fixture_45"
				   && !otherPath.has_value() && restarted.Requests.size() == count
				   && count == 3U
				   && std::ranges::none_of(restarted.Requests, [](const auto& aWire) {
						  return aWire.Wire.Operation
							  == review_mutation_operation::create_review_comment;
					  }),
			   "restart recovery selects only new C1 and conflicting reuse refuses before transport");

		auto ambiguousRecovery = std::make_shared<fake_recovery_state>();
		auto ambiguous         = fake_transport{ };
		ambiguous.Completions.push_back(comment_list_response());
		ambiguous.Completions.push_back(viewer_response());
		ambiguous.Completions.push_back(target_response());
		ambiguous.Completions.emplace_back(github_mutation_wire_error{
			.Failure      = application::review_mutation_failure::unavailable,
			.Transmission = github_mutation_transmission::indeterminate,
		});
		ambiguous.Completions.push_back(comment_list_response(3U));
		auto ambiguousSession = review_mutation_session{
			ambiguous,
			configuration(),
			mutation_lease(),
			make_recovery(ambiguousRecovery),
		};
		const auto unresolved = ambiguousSession.submit_and_reconcile(request());
		expect(!unresolved.Result.has_value() && unresolved.MutationAttempted
				   && ambiguousRecovery->Records.at(request().IdempotencyKey).State
					   == application::review_mutation_recovery_state::unresolved,
			   "two new exact identities remain ambiguous and durably unresolved");
	}

	void test_prepared_attempts_never_reconcile_external_comments()
	{
		for (const auto crashBeforeWitness : { false, true }) {
			auto state     = std::make_shared<fake_recovery_state>();
			auto transport = fake_transport{ };
			transport.Completions.push_back(comment_list_response(0U));
			transport.Completions.push_back(viewer_response());
			transport.Completions.push_back(target_response());
			transport.Completions.emplace_back(github_mutation_wire_error{
				.Failure      = application::review_mutation_failure::unavailable,
				.Transmission = github_mutation_transmission::definitely_not_sent,
			});
			transport.ThrowBeforePostWitness = crashBeforeWitness;
			auto session                     = review_mutation_session{
				transport, configuration(), mutation_lease(), make_recovery(state)
			};
			const auto first = session.submit_and_reconcile(request());
			expect(!first.Result.has_value()
					   && state->Records.at(request().IdempotencyKey).Transmission
						   == application::review_mutation_recovery_transmission::prepared,
				   "admission-to-POST crash and definite no-send both remain prepared");

			auto restarted = fake_transport{ };
			restarted.Completions.push_back(viewer_response());
			restarted.Completions.push_back(target_response());
			restarted.Completions.push_back(comment_list_response());
			auto restartedSession = review_mutation_session{
				restarted, configuration(), mutation_lease(), make_recovery(state)
			};
			const auto recovery = restartedSession.submit_and_reconcile(request());
			expect(!recovery.Result.has_value() && restarted.Requests.empty(),
				   "a later external C1 cannot resolve a prepared attempt or trigger recovery transport");
		}
	}

	void test_transmission_witness_is_one_shot()
	{
		auto transport          = fake_transport{ };
		transport.RepeatWitness = true;
		transport.exact_submit_course();
		auto session = review_mutation_session{
			transport, configuration(), mutation_lease(), make_recovery()
		};
		const auto receipt = session.submit(request());
		expect(receipt.has_value() && receipt->ServerCommentId == "PRRC_fixture_44",
			   "a repeated transport witness is rejected after the first durable transition");
	}

	void test_failed_transmission_witness_stays_prepared()
	{
		auto state                    = std::make_shared<fake_recovery_state>();
		state->RejectTransmissionMark = true;
		auto transport                = fake_transport{ };
		transport.exact_submit_course();
		auto session = review_mutation_session{
			transport, configuration(), mutation_lease(), make_recovery(state)
		};
		const auto first = session.submit_and_reconcile(request());
		expect(!first.Result.has_value()
				   && state->Records.at(request().IdempotencyKey).Transmission
					   == application::review_mutation_recovery_transmission::prepared,
			   "a failed durable witness aborts the POST course and stays prepared");

		state->RejectTransmissionMark = false;
		auto restarted                = fake_transport{ };
		restarted.Completions.push_back(viewer_response());
		restarted.Completions.push_back(target_response());
		restarted.Completions.push_back(comment_list_response());
		auto restartedSession = review_mutation_session{
			restarted, configuration(), mutation_lease(), make_recovery(state)
		};
		const auto recovery = restartedSession.submit_and_reconcile(request());
		expect(!recovery.Result.has_value() && restarted.Requests.empty(),
			   "a failed witness cannot adopt a later external comment after restart");
	}

	void test_direct_post_receipt_must_be_new_to_the_baseline()
	{
		auto preexisting = fake_transport{ };
		preexisting.Completions.push_back(comment_list_response());
		preexisting.Completions.push_back(viewer_response());
		preexisting.Completions.push_back(target_response());
		preexisting.Completions.push_back(comment_response());
		preexisting.Completions.push_back(comment_list_response());
		auto preexistingSession = review_mutation_session{
			preexisting, configuration(), mutation_lease(), make_recovery()
		};
		const auto refused = preexistingSession.submit_and_reconcile(request());
		expect(!refused.Result.has_value() && refused.MutationAttempted,
			   "a direct 201 identity already present in C0 cannot resolve the attempt");

		auto direct = fake_transport{ };
		direct.Completions.push_back(comment_list_response());
		direct.Completions.push_back(viewer_response());
		direct.Completions.push_back(target_response());
		direct.Completions.push_back(comment_response(45U));
		direct.Completions.emplace_back(github_mutation_wire_error{
			.Failure      = application::review_mutation_failure::unavailable,
			.Transmission = github_mutation_transmission::definitely_not_sent,
		});
		auto directSession = review_mutation_session{
			direct, configuration(), mutation_lease(), make_recovery()
		};
		const auto accepted = directSession.submit_and_reconcile(request());
		expect(accepted.Result.has_value() && !accepted.Result->Duplicate
				   && accepted.Result->ServerCommentId == "PRRC_fixture_45",
			   "a new exact direct 201 remains authoritative when reconciliation is unavailable");
	}

	void test_baseline_incomplete_attempt_never_observes_or_resolves()
	{
		auto state = std::make_shared<fake_recovery_state>();
		auto seed  = fake_transport{ };
		seed.Completions.push_back(comment_list_response(0U));
		seed.Completions.push_back(viewer_response());
		seed.Completions.push_back(target_response());
		seed.Completions.emplace_back(github_mutation_wire_error{
			.Failure      = application::review_mutation_failure::unavailable,
			.Transmission = github_mutation_transmission::indeterminate,
		});
		seed.Completions.push_back(comment_list_response(0U));
		auto seedSession = review_mutation_session{
			seed, configuration(), mutation_lease(), make_recovery(state)
		};
		static_cast<void>(seedSession.submit_and_reconcile(request()));
		state->Records.at(request().IdempotencyKey).BaselineExactCommentIds.reset();
		auto transport = fake_transport{ };
		auto session   = review_mutation_session{
			transport, configuration(), mutation_lease(), make_recovery(state)
		};
		const auto submission = session.submit_and_reconcile(request());
		expect(!submission.Result.has_value() && !submission.MutationAttempted
				   && transport.Requests.empty(),
			   "a migrated baseline-incomplete unresolved row remains guarded without observation or POST");
	}

	void test_reconciliation_rejects_omitted_or_ambiguous_exact_comment()
	{
		auto paginated = comment_list_response();
		std::get<github_mutation_wire_success>(paginated).Response.Headers.push_back({
			.Name  = "Link",
			.Value = "<https://api.github.com/page=2>; rel=\"next\"",
		});
		const auto incomplete = parse_review_comment_observations(
			std::get<github_mutation_wire_success>(paginated).Response, request());
		expect(!incomplete.has_value(),
			   "a paginated comment snapshot cannot omit the idempotency target");

		const auto multiple = parse_review_comment_observations(
			std::get<github_mutation_wire_success>(comment_list_response(2U)).Response,
			request());
		expect(multiple.has_value() && multiple->ExactReceipts.size() == 2U
				   && multiple->ExactReceipts[0].ServerCommentId == "PRRC_fixture_44"
				   && multiple->ExactReceipts[1].ServerCommentId == "PRRC_fixture_45",
			   "all exact matches are exposed in deterministic server-identity order");
	}

	void test_typed_transport_outcomes_propagate_without_diagnostics()
	{
		auto refusedTransport = fake_transport{ };
		refusedTransport.Completions.emplace_back(github_mutation_wire_error{
			.Failure = application::review_mutation_failure::refused_by_server,
		});
		auto refusedSession = review_mutation_session{
			refusedTransport, configuration(), mutation_lease(), make_recovery()
		};
		const auto refused = refusedSession.observe_target();
		expect(!refused.has_value()
				   && refused.error() == application::review_mutation_failure::refused_by_server,
			   "server refusal remains typed");

		auto cancelledTransport = fake_transport{ };
		cancelledTransport.Completions.emplace_back(github_mutation_wire_cancelled{ });
		auto cancelledSession = review_mutation_session{
			cancelledTransport, configuration(), mutation_lease(), make_recovery()
		};
		const auto cancelled = cancelledSession.observe_target();
		expect(!cancelled.has_value()
				   && cancelled.error() == application::review_mutation_failure::cancelled,
			   "cancellation remains typed");
	}

	void test_stop_token_cancels_every_session_phase_before_the_next_wire_call()
	{
		auto stopped = std::stop_source{ };
		static_cast<void>(stopped.request_stop());
		auto beforeTransport = fake_transport{ };
		auto beforeSession   = review_mutation_session{
			beforeTransport, configuration(), mutation_lease(), make_recovery()
		};
		const auto before = beforeSession.submit(request(), stopped.get_token());
		expect(!before.has_value()
				   && before.error() == application::review_mutation_failure::cancelled
				   && beforeTransport.Requests.empty(),
			   "a pre-stopped request performs no wire operation");

		auto afterBaselineStop = std::stop_source{ };
		auto afterBaseline     = fake_transport{ };
		afterBaseline.Completions.push_back(comment_list_response(0U));
		afterBaseline.OnPerform = [&afterBaselineStop](const std::size_t aCount) {
			if (aCount == 1U) {
				static_cast<void>(afterBaselineStop.request_stop());
			}
		};
		auto afterBaselineSession = review_mutation_session{
			afterBaseline, configuration(), mutation_lease(), make_recovery()
		};
		const auto baselineCancelled = afterBaselineSession.submit(
			request(), afterBaselineStop.get_token());
		expect(!baselineCancelled.has_value()
				   && baselineCancelled.error()
					   == application::review_mutation_failure::cancelled
				   && afterBaseline.Requests.size() == 1U,
			   "stopping after baseline observation prevents durable admission and authority reads");

		auto afterViewerStop = std::stop_source{ };
		auto afterViewer     = fake_transport{ };
		afterViewer.Completions.push_back(comment_list_response(0U));
		afterViewer.Completions.push_back(viewer_response());
		afterViewer.OnPerform = [&afterViewerStop](const std::size_t aCount) {
			if (aCount == 2U) {
				static_cast<void>(afterViewerStop.request_stop());
			}
		};
		auto afterViewerSession = review_mutation_session{
			afterViewer, configuration(), mutation_lease(), make_recovery()
		};
		const auto viewerCancelled = afterViewerSession.submit(
			request(), afterViewerStop.get_token());
		expect(!viewerCancelled.has_value()
				   && viewerCancelled.error()
					   == application::review_mutation_failure::cancelled
				   && afterViewer.Requests.size() == 2U,
			   "stopping after viewer observation prevents target observation");

		auto beforeWriteStop = std::stop_source{ };
		auto beforeWrite     = fake_transport{ };
		beforeWrite.Completions.push_back(comment_list_response(0U));
		beforeWrite.Completions.push_back(viewer_response());
		beforeWrite.Completions.push_back(target_response());
		beforeWrite.OnPerform = [&beforeWriteStop](const std::size_t aCount) {
			if (aCount == 3U) {
				static_cast<void>(beforeWriteStop.request_stop());
			}
		};
		auto beforeWriteSession = review_mutation_session{
			beforeWrite, configuration(), mutation_lease(), make_recovery()
		};
		const auto writeCancelled = beforeWriteSession.submit(
			request(), beforeWriteStop.get_token());
		expect(!writeCancelled.has_value()
				   && writeCancelled.error()
					   == application::review_mutation_failure::cancelled
				   && beforeWrite.Requests.size() == 3U
				   && std::ranges::none_of(
					   beforeWrite.Requests,
					   [](const auto& aWire) {
						   return aWire.Wire.Operation
							   == review_mutation_operation::create_review_comment;
					   }),
			   "stopping after exact target observation prevents the POST");
	}

	void test_retained_revoked_lease_refuses_every_provider_use()
	{
		auto transport = fake_transport{ };
		auto lease     = mutation_lease();
		auto session   = review_mutation_session{
			transport,
			configuration(),
			lease,
			make_recovery(),
		};
		lease->revoke();

		const auto observed  = session.observe_target();
		const auto submitted = session.submit(request());
		expect(!observed.has_value()
				   && observed.error()
					   == application::review_mutation_failure::unavailable
				   && !submitted.has_value()
				   && submitted.error()
					   == application::review_mutation_failure::unavailable
				   && transport.Requests.empty(),
			   "a retained revoked lease refuses observation and submission before transport use");
	}

	void test_retained_revoked_lease_refuses_provider_admission()
	{
		auto transport = fake_transport{ };
		auto lease     = std::make_shared<fake_github_review_mutation_lease>(transport);
		auto retained  = application::review_mutation_transport{ lease };
		expect(admit_review_mutation_provider(retained, target(), identity()).has_value(),
			   "the exact active lease is admitted by the factory gate");
		lease->revoke();

		auto admission = admit_review_mutation_provider(
			retained,
			target(),
			identity());
		expect(!admission.has_value()
				   && admission.error()
					   == application::review_mutation_failure::unavailable,
			   "a retained revoked lease refuses the provider factory gate");
	}

	void write_unresolved_restart_fixture(const std::filesystem::path& aRoot)
	{
		auto transport = fake_transport{ };
		transport.Completions.push_back(comment_list_response());
		transport.Completions.push_back(viewer_response());
		transport.Completions.push_back(target_response());
		transport.Completions.emplace_back(github_mutation_wire_error{
			.Failure      = application::review_mutation_failure::unavailable,
			.Transmission = github_mutation_transmission::indeterminate,
		});
		transport.Completions.push_back(comment_list_response());
		auto session = review_mutation_session{
			transport, configuration(), mutation_lease(), open_recovery(aRoot)
		};
		const auto submission = session.submit_and_reconcile(request());
		expect(submission.MutationAttempted && !submission.Result.has_value()
				   && submission.Reconciliation.has_value()
				   && submission.Reconciliation->size() == 1U
				   && transport.Requests.size() == 5U,
			   "first process persists C0 and leaves a visible unresolved attempt");
		std::cout << "restart-recovery-phase-one:unresolved\n";
	}

	void recover_unresolved_restart_fixture(const std::filesystem::path& aRoot)
	{
		auto transport = fake_transport{ };
		transport.Completions.push_back(viewer_response());
		transport.Completions.push_back(target_response());
		transport.Completions.push_back(comment_list_response(2U));
		auto session = review_mutation_session{
			transport, configuration(), mutation_lease(), open_recovery(aRoot)
		};
		const auto submission = session.submit_and_reconcile(request());
		expect(submission.Result.has_value() && submission.Result->Duplicate
				   && submission.Result->ServerCommentId == "PRRC_fixture_45"
				   && submission.Reconciliation.has_value()
				   && submission.Reconciliation->size() == 2U
				   && !submission.MutationAttempted
				   && std::ranges::none_of(transport.Requests, [](const auto& aWire) {
						  return aWire.Wire.Operation
							  == review_mutation_operation::create_review_comment;
					  }),
			   "second process resolves only from an exact server snapshot without POST");
		std::cout << "restart-recovery-phase-two:resolved-get-only\n";
	}

	[[noreturn]] void write_prepared_crash_fixture(
		const std::filesystem::path& aRoot)
	{
		auto transport = fake_transport{ };
		transport.Completions.push_back(comment_list_response(0U));
		transport.Completions.push_back(viewer_response());
		transport.Completions.push_back(target_response());
		transport.Completions.emplace_back(github_mutation_wire_error{
			.Failure      = application::review_mutation_failure::unavailable,
			.Transmission = github_mutation_transmission::definitely_not_sent,
		});
		transport.ThrowBeforePostWitness = true;
		auto session                     = review_mutation_session{
			transport, configuration(), mutation_lease(), open_recovery(aRoot)
		};
		const auto submission = session.submit_and_reconcile(request());
		expect(!submission.Result.has_value() && transport.Requests.size() == 4U,
			   "first process crashes after durable preparation and before POST witness");
		std::cout << "restart-prepared-phase-one:crash-before-post\n"
				  << std::flush;
		std::_Exit(EXIT_SUCCESS);
	}

	void recover_prepared_restart_fixture(const std::filesystem::path& aRoot)
	{
		auto transport = fake_transport{ };
		transport.Completions.push_back(viewer_response());
		transport.Completions.push_back(target_response());
		transport.Completions.push_back(comment_list_response());
		auto session = review_mutation_session{
			transport, configuration(), mutation_lease(), open_recovery(aRoot)
		};
		const auto submission = session.submit_and_reconcile(request());
		expect(!submission.Result.has_value() && !submission.MutationAttempted
				   && transport.Requests.empty(),
			   "second process cannot reconcile external C1 against a prepared attempt");
		std::cout << "restart-prepared-phase-two:zero-transport\n";
	}
} // namespace

auto main(const int aCount, char** aValues) -> int
{
	if (aCount == 3 && std::string_view{ aValues[1] } == "--write-unresolved") {
		write_unresolved_restart_fixture(aValues[2]);
		return EXIT_SUCCESS;
	}
	if (aCount == 3 && std::string_view{ aValues[1] } == "--recover-unresolved") {
		recover_unresolved_restart_fixture(aValues[2]);
		return EXIT_SUCCESS;
	}
	if (aCount == 3 && std::string_view{ aValues[1] } == "--write-prepared-crash") {
		write_prepared_crash_fixture(aValues[2]);
	}
	if (aCount == 3 && std::string_view{ aValues[1] } == "--recover-prepared") {
		recover_prepared_restart_fixture(aValues[2]);
		return EXIT_SUCCESS;
	}
	if (aCount != 1) {
		return EXIT_FAILURE;
	}
	test_observe_then_immediate_revalidate_then_submit();
	test_invalid_bindings_submit_nothing();
	test_revalidation_drift_and_identity_failure_stop_before_write();
	test_idempotency_and_indeterminate_attempts_never_duplicate();
	test_prepared_attempts_never_reconcile_external_comments();
	test_transmission_witness_is_one_shot();
	test_failed_transmission_witness_stays_prepared();
	test_direct_post_receipt_must_be_new_to_the_baseline();
	test_baseline_incomplete_attempt_never_observes_or_resolves();
	test_reconciliation_rejects_omitted_or_ambiguous_exact_comment();
	test_typed_transport_outcomes_propagate_without_diagnostics();
	test_stop_token_cancels_every_session_phase_before_the_next_wire_call();
	test_retained_revoked_lease_refuses_every_provider_use();
	test_retained_revoked_lease_refuses_provider_admission();
	std::cout << "live-review-mutation-provider-tests:pass\n";
	return EXIT_SUCCESS;
}
