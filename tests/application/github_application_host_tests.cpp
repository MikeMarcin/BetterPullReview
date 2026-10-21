#include "../support/qualification_build.hpp"
#include "bpr/application/github_application_host.hpp"
#include "bpr/application/manual_executor.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <new>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

// [[BPR-1112]]. Allocation failure on demand, for every allocation this test
// binary makes through `operator new`. `arm(n)` lets n more allocations
// succeed and fails the next one with std::bad_alloc. Disarmed, every
// allocation is an ordinary one.
namespace allocation_fault {
	inline std::atomic<long long> Remaining{ -1 };
	inline std::atomic<bool>      Fired{ false };
	inline void                   arm(const long long aSucceeding) noexcept
	{
		Fired     = false;
		Remaining = aSucceeding;
	}
	// Disarms, and reports whether the armed failure happened.
	inline auto disarm() noexcept -> bool
	{
		Remaining = -1;
		return Fired.exchange(false);
	}
}
// The standard library declares these with reserved parameter names.
// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
auto operator new(const std::size_t aSize) -> void*
{
	if (allocation_fault::Remaining.load() >= 0 && allocation_fault::Remaining.fetch_sub(1) == 0) {
		allocation_fault::Remaining = -1;
		allocation_fault::Fired     = true;
		throw std::bad_alloc{ };
	}
	if (auto* const block = std::malloc(aSize == 0U ? 1U : aSize); block != nullptr) {
		return block;
	}
	throw std::bad_alloc{ };
}
// [[BPR-1262]]. GCC 15 at -O3 inlined these into delete-expressions and then
// reported `free` on memory from `operator new`, not seeing that this file
// replaces both. Out of line, the pair is a pair again.
#if defined(__GNUC__)
[[gnu::noinline]]
#endif
// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
void operator delete(void* aBlock) noexcept
{
	std::free(aBlock);
}
#if defined(__GNUC__)
[[gnu::noinline]]
#endif
// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
void operator delete(void* aBlock, std::size_t) noexcept
{
	std::free(aBlock);
}

// [[BPR-1124]]. Whether the two allocation sweeps may fail every allocation in
// turn. MSVC's checked standard library (`_ITERATOR_DEBUG_LEVEL` 1 or 2) gives
// each container a proxy, and allocates it inside noexcept moves, a
// std::vector's move constructor among them. A std::bad_alloc injected there
// can only reach std::terminate, and it says nothing about the product: a
// Release build (level 0) makes no such allocation, and neither libc++ nor
// libstdc++ allocates in those moves. So the sweeps run everywhere except in a
// checked MSVC build, which prints that they did not.
#if defined(_ITERATOR_DEBUG_LEVEL) && _ITERATOR_DEBUG_LEVEL != 0
	#define BPR_TEST_ALLOCATION_SWEEPS 0
#else
	#define BPR_TEST_ALLOCATION_SWEEPS 1
#endif

namespace {
	using namespace bpr;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename Type>
	concept exposes_requests = requires(const Type& aValue) {
		aValue.Requests;
	};

	template <typename Type>
	concept exposes_accepted_session = requires(const Type& aValue) {
		aValue.AcceptedSession;
	};

	template <typename Type>
	concept exposes_accepted_snapshot = requires(const Type& aValue) {
		aValue.AcceptedSnapshot;
	};

	template <typename Type>
	concept exposes_viewer = requires(const Type& aValue) {
		aValue.Viewer;
	};

	template <typename Type>
	concept exposes_capability_evidence = requires(const Type& aValue) {
		aValue.CapabilityEvidence;
	};

	template <typename Type>
	concept exposes_account_transport = requires(const Type& aValue) {
		aValue.AccountTransport;
	};

	template <typename Type>
	concept exposes_credential = requires(const Type& aValue) {
		aValue.Credential;
	};

	template <typename Type>
	concept exposes_detail = requires(const Type& aValue) {
		aValue.Detail;
	};

	class fake_smart_transport final
		: public application::git_smart_transport_port
	{
	public:
		void revoke() noexcept override { Revoked = true; }

		[[nodiscard]] auto revoked() const noexcept -> bool override
		{
			return Revoked;
		}

		[[nodiscard]] auto discover(
			const application::git_smart_discovery_request&,
			std::stop_token)
			-> application::git_smart_transport_result<
				application::git_smart_discovery_response> override
		{
			return unavailable<application::git_smart_discovery_response>();
		}

		[[nodiscard]] auto ls_refs(
			const application::git_smart_ls_refs_request&,
			std::stop_token)
			-> application::git_smart_transport_result<
				application::git_smart_ls_refs_response> override
		{
			return unavailable<application::git_smart_ls_refs_response>();
		}

		[[nodiscard]] auto fetch(
			const application::git_smart_fetch_request&,
			application::git_smart_wire_consumer&,
			std::stop_token)
			-> application::git_smart_transport_result<void> override
		{
			return unavailable<void>();
		}

	private:
		template <typename Value>
		[[nodiscard]] auto unavailable() const
			-> application::git_smart_transport_result<Value>
		{
			return std::unexpected{ application::git_smart_transport_error{
				.Failure = Revoked
					? application::git_smart_transport_failure::cancelled
					: application::git_smart_transport_failure::unavailable,
			} };
		}

		bool Revoked = false;
	};

	struct fake_review_mutation_lease final
		: application::review_mutation_transport_lease
	{
	};

	/// [[BPR-1177]]. A publication catalog that answers nothing: the host only
	/// hands it on, and a test only compares which one it handed on.
	struct stub_publication_port final : application::github_publication_port
	{
		template <typename T>
		static auto none() -> application::github_publication_result<T>
		{
			return std::unexpected(application::github_publication_error{ });
		}
		auto observe_repository(const application::github_publication_target&, std::stop_token) -> application::github_publication_result<application::github_publication_capability> override
		{
			return none<application::github_publication_capability>();
		}
		auto observe_branch(const application::github_publication_target&, const std::string&, std::stop_token) -> application::github_publication_result<std::optional<core::git_commit_oid>> override
		{
			return none<std::optional<core::git_commit_oid>>();
		}
		auto observe_commit(const application::github_publication_target&, const core::git_commit_oid&, std::stop_token)
			-> application::github_publication_result<std::optional<application::github_publication_commit>> override
		{
			return none<std::optional<application::github_publication_commit>>();
		}
		auto observe_tree(const application::github_publication_target&, const core::git_tree_oid&, std::stop_token) -> application::github_publication_result<bool> override
		{
			return none<bool>();
		}
		auto observe_blob(const application::github_publication_target&, const core::git_entry_oid&, std::stop_token) -> application::github_publication_result<bool> override
		{
			return none<bool>();
		}
		auto create_blob(const application::github_publication_target&, std::span<const std::uint8_t>, const core::git_entry_oid&, std::stop_token) -> application::github_publication_result<void> override
		{
			return none<void>();
		}
		auto create_tree(const application::github_publication_target&, const core::git_tree_oid&, const std::vector<core::git_tree_entry>&, const std::vector<std::string>&, const core::git_tree_oid&,
						 std::stop_token) -> application::github_publication_result<void> override
		{
			return none<void>();
		}
		auto create_commit(const application::github_publication_target&, const core::git_commit_content&, const core::git_commit_oid&, std::stop_token) -> application::github_publication_result<void> override
		{
			return none<void>();
		}
		auto create_branch(const application::github_publication_target&, const std::string&, const core::git_commit_oid&, std::stop_token)
			-> application::github_publication_result<application::github_branch_creation> override
		{
			return none<application::github_branch_creation>();
		}
		auto create_pull_request(const application::github_publication_target&, const application::github_pull_request_draft&, std::stop_token)
			-> application::github_publication_result<application::github_pull_request_observation> override
		{
			return none<application::github_pull_request_observation>();
		}
		auto find_pull_requests(const application::github_publication_target&, const std::string&, const std::string&, std::stop_token)
			-> application::github_publication_result<std::vector<application::github_pull_request_observation>> override
		{
			return none<std::vector<application::github_pull_request_observation>>();
		}
		auto observe_pull_request(const application::github_publication_target&, std::uint64_t, std::stop_token)
			-> application::github_publication_result<std::optional<application::github_pull_request_observation>> override
		{
			return none<std::optional<application::github_pull_request_observation>>();
		}
	};

	struct fake_lease final
		: application::github_account_transport_lease,
		  application::git_smart_transport_factory_port,
		  application::review_mutation_transport_factory_port,
		  application::github_publication_session_factory_port
	{
		[[nodiscard]] auto acquire_publication_session() noexcept -> std::shared_ptr<application::github_publication_port> override
		{
			return Publication;
		}
		/// The catalog this account's wire mints; null for a wire that cannot carry publication.
		std::shared_ptr<application::github_publication_port> Publication = std::make_shared<stub_publication_port>();

		[[nodiscard]] auto acquire(
			const application::git_smart_request_context& aContext)
			-> application::git_smart_transport_result<
				application::git_smart_transport> override
		{
			AcquiredContexts.push_back(aContext);
			LastTransport = std::make_shared<fake_smart_transport>();
			return LastTransport;
		}

		[[nodiscard]] auto acquire_review_mutation_transport() noexcept
			-> application::review_mutation_transport override
		{
			try {
				auto lease = std::make_shared<fake_review_mutation_lease>();
				MutationLeases.push_back(lease);
				return lease;
			} catch (...) {
				return { };
			}
		}

		std::vector<application::git_smart_request_context>      AcquiredContexts;
		std::shared_ptr<fake_smart_transport>                    LastTransport;
		std::vector<std::shared_ptr<fake_review_mutation_lease>> MutationLeases;
	};

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		auto bytes    = std::array<std::uint8_t, 32U>{ };
		bytes.front() = aSeed;
		bytes.back()  = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return Identity{ bytes };
	}

	class fake_provider final : public application::github_provider_port
	{
	public:
		struct profile_call
		{
			application::github_profile_request            Request;
			application::github_profile_completion_handler Completion;
		};

		enum class viewer_authentication {
			cli,
			bootstrap,
			account,
		};

		struct viewer_call
		{
			application::github_viewer_request            Request;
			viewer_authentication                         Authentication = viewer_authentication::cli;
			application::github_viewer_completion_handler Completion;
		};

		struct capability_call
		{
			application::github_capability_request            Request;
			application::github_capability_completion_handler Completion;
		};

		struct pull_request_call
		{
			application::github_pull_request_provider_request   Request;
			application::github_pull_request_completion_handler Completion;
		};

		void resolve_profile(
			application::github_profile_request            aRequest,
			application::github_profile_completion_handler aCompletion) override
		{
			if (ThrowOnProfileSubmit) {
				throw std::runtime_error{ "profile submit" };
			}
			Profiles.push_back({
				.Request    = aRequest,
				.Completion = std::move(aCompletion),
			});
		}

		void cancel_profile(application::github_profile_request aRequest) override
		{
			CancelledProfiles.push_back(aRequest);
			if (CompleteProfileFromWorkerOnCancel) {
				const auto found = std::ranges::find(Profiles, aRequest, &profile_call::Request);
				expect(found != Profiles.end(), "cancelled profile call exists");
				auto        completion = found->Completion;
				std::thread worker{
					[workerCompletion = std::move(completion), aRequest]() mutable {
						workerCompletion(application::github_profile_cancelled{
							.Request = aRequest,
						});
					}
				};
				worker.join();
			}
		}

		void observe_viewer(
			application::github_viewer_submission         aSubmission,
			application::github_viewer_completion_handler aCompletion) override
		{
			auto authentication = viewer_authentication::cli;
			if (std::holds_alternative<application::github_bootstrap_credential>(
					aSubmission.Authentication)) {
				authentication = viewer_authentication::bootstrap;
			} else if (std::holds_alternative<
						   application::github_credential_retrieve_success>(
						   aSubmission.Authentication)) {
				authentication = viewer_authentication::account;
			}
			Viewers.push_back({
				.Request        = aSubmission.Request,
				.Authentication = authentication,
				.Completion     = std::move(aCompletion),
			});
		}

		void cancel_viewer(application::github_viewer_request aRequest) override
		{
			CancelledViewers.push_back(aRequest);
		}

		void probe_capabilities(
			application::github_capability_request            aRequest,
			application::github_capability_completion_handler aCompletion) override
		{
			Capabilities.push_back({
				.Request    = aRequest,
				.Completion = std::move(aCompletion),
			});
		}

		void
		cancel_capabilities(application::github_capability_request aRequest) override
		{
			CancelledCapabilities.push_back(aRequest);
		}

		void fetch_pull_request(
			application::github_pull_request_provider_request   aRequest,
			application::github_pull_request_completion_handler aCompletion) override
		{
			PullRequests.push_back({
				.Request    = aRequest,
				.Completion = std::move(aCompletion),
			});
		}

		void cancel_pull_request(
			application::github_pull_request_provider_request aRequest) override
		{
			CancelledPullRequests.push_back(std::move(aRequest));
		}

		std::vector<profile_call>                           Profiles;
		std::vector<viewer_call>                            Viewers;
		std::vector<capability_call>                        Capabilities;
		std::vector<pull_request_call>                      PullRequests;
		std::vector<application::github_profile_request>    CancelledProfiles;
		std::vector<application::github_viewer_request>     CancelledViewers;
		std::vector<application::github_capability_request> CancelledCapabilities;
		std::vector<application::github_pull_request_provider_request>
			 CancelledPullRequests;
		bool ThrowOnProfileSubmit              = false;
		bool CompleteProfileFromWorkerOnCancel = false;
	};

	class fake_vault final : public application::github_credential_vault_port
	{
	public:
		struct store_call
		{
			application::github_credential_operation_context        Context;
			application::github_credential_store_completion_handler Completion;
		};

		struct retrieve_call
		{
			application::github_credential_operation_context           Context;
			application::github_credential_retrieve_completion_handler Completion;
		};

		struct delete_call
		{
			application::github_credential_operation_context         Context;
			application::github_credential_delete_completion_handler Completion;
		};

		// [[BPR-0548]]. The two rules every shipped platform vault enforces on
		// all six port methods, so a host test cannot pass against a
		// permissiveness no real vault has: `valid()` on the context or
		// enrollment plus a callable completion, refused `invalid_request`
		// before anything is recorded; and one pending operation per
		// `github_credential_operation_context`, the duplicate refused
		// `invalid_request` before it is recorded. A recorded call stays
		// pending until the test runs its completion, which is this double's
		// stand-in for the platform vault's effect lane.
		[[nodiscard]] auto claim(
			const application::github_credential_operation_context& aContext)
			-> bool
		{
			return Pending->insert(aContext).second;
		}

		void release(
			const application::github_credential_operation_context& aContext)
		{
			Pending->erase(aContext);
		}

		template <typename Handler>
		[[nodiscard]] auto releasing(
			const application::github_credential_operation_context& aContext,
			Handler                                                 aCompletion) -> Handler
		{
			return [pending = Pending, aContext, completion = std::move(aCompletion)](
					   auto aTerminal) mutable {
				pending->erase(aContext);
				completion(std::move(aTerminal));
			};
		}

		void store(application::github_credential_enrollment aEnrollment,
				   application::github_credential_store_completion_handler
					   aCompletion) override
		{
			if (ThrowOnStore) {
				throw std::runtime_error{ "vault store" };
			}
			const auto context  = aEnrollment.Context;
			const auto admitted = application::valid(aEnrollment) && aCompletion
				&& claim(context);
			if (!admitted) {
				aEnrollment.Credential.reset();
				if (aCompletion) {
					aCompletion(application::github_credential_store_error{
						.Context = context,
						.Failure = application::github_credential_vault_failure::
							invalid_request,
					});
				}
				return;
			}
			StoredCredentialBytes.push_back(aEnrollment.Credential.size());
			Stores.push_back({
				.Context    = context,
				.Completion = releasing(context, std::move(aCompletion)),
			});
		}

		void retrieve(application::github_credential_operation_context aContext,
					  application::github_credential_retrieve_completion_handler
						  aCompletion) override
		{
			if (!application::valid(aContext) || !aCompletion
				|| !claim(aContext)) {
				if (aCompletion) {
					aCompletion(application::github_credential_retrieve_error{
						.Context = aContext,
						.Failure = application::github_credential_vault_failure::
							invalid_request,
					});
				}
				return;
			}
			Retrieves.push_back({
				.Context    = aContext,
				.Completion = releasing(aContext, std::move(aCompletion)),
			});
		}

		void
		delete_credential(application::github_credential_operation_context aContext,
						  application::github_credential_delete_completion_handler
							  aCompletion) override
		{
			if (!application::valid(aContext) || !aCompletion
				|| !claim(aContext)) {
				if (aCompletion) {
					aCompletion(application::github_credential_delete_error{
						.Context = aContext,
						.Failure = application::github_credential_vault_failure::
							invalid_request,
					});
				}
				return;
			}
			Deletes.push_back({
				.Context    = aContext,
				.Completion = releasing(aContext, std::move(aCompletion)),
			});
		}

		// What the refresh slot answers a presence probe with. The default is
		// the platform that has no second slot at all, which is what every
		// test below that is about the credential path wants; the two named
		// answers are the two kinds of material a restart can find.
		enum class refresh_answer : std::uint8_t {
			unavailable,
			absent,
			present,
		};

		// [[BPR-0540]]. What the refresh slot's erasure answers with. The
		// default is a real second slot that deletes what it held, which is
		// what every test about the credential path wants: the erasure
		// succeeds beside the credential's and reports nothing. `unavailable`
		// is the store that could not be reached, and since every shipped
		// platform has a real slot (BPR-0542, BPR-0543) it is a failure like
		// any other.
		enum class refresh_delete_answer : std::uint8_t {
			deleted,
			unavailable,
			failed,
		};

		// The GitHub host owns the credential slot only. Nothing it does writes
		// the refresh slot, so storing into it still answers the way a platform
		// without one does; only the presence probe reads it.
		void store_refresh(
			application::github_credential_enrollment               aEnrollment,
			application::github_credential_store_completion_handler aCompletion)
			override
		{
			const auto context  = aEnrollment.Context;
			const auto admitted = application::valid(aEnrollment) && aCompletion;
			aEnrollment.Credential.reset();
			if (!aCompletion) {
				return;
			}
			if (!admitted || !claim(context)) {
				aCompletion(application::github_credential_store_error{
					.Context = context,
					.Failure = application::github_credential_vault_failure::
						invalid_request,
				});
				return;
			}
			release(context);
			// [[BPR-0644]]. Opt-in, for the courses that stand in for the device
			// host's rotated refresh store and then watch the slot leave.
			if (AcceptRefreshStores) {
				RefreshSlots.insert(context.Authority);
				aCompletion(application::github_credential_store_success{ .Context = context, .Disposition = application::github_credential_store_disposition::stored });
				return;
			}
			aCompletion(application::github_credential_store_error{
				.Context = context,
				.Failure = application::github_credential_vault_failure::unavailable,
			});
		}

		void retrieve_refresh(
			application::github_credential_operation_context aContext,
			application::github_credential_retrieve_completion_handler
				aCompletion) override
		{
			if (!application::valid(aContext) || !aCompletion
				|| !claim(aContext)) {
				if (aCompletion) {
					aCompletion(application::github_credential_retrieve_error{
						.Context = aContext,
						.Failure = application::github_credential_vault_failure::
							invalid_request,
					});
				}
				return;
			}
			release(aContext);
			RefreshRetrieves.push_back(aContext);
			switch (RefreshAnswer) {
			case refresh_answer::present: {
				const auto bytes    = std::array{ std::byte{ 'r' }, std::byte{ 't' } };
				auto       material = application::github_credential_material::create(
					std::span<const std::byte>{ bytes });
				if (material.has_value()) {
					aCompletion(application::github_credential_retrieve_success{
						.Context    = aContext,
						.Credential = std::move(*material),
					});
					return;
				}
				break;
			}
			case refresh_answer::absent:
				aCompletion(application::github_credential_retrieve_absent{
					.Context = aContext,
				});
				return;
			case refresh_answer::unavailable:
				break;
			}
			aCompletion(application::github_credential_retrieve_error{
				.Context = aContext,
				.Failure = application::github_credential_vault_failure::unavailable,
			});
		}

		// [[BPR-0540]]. Recorded, then answered the way a platform with no
		// second slot answers. The recording is what lets a test say the
		// refresh slot left under the same authority as the credential; the
		// answer is what keeps this fake the no-second-slot platform every
		// credential-path test above it assumes.
		void delete_refresh(
			application::github_credential_operation_context         aContext,
			application::github_credential_delete_completion_handler aCompletion)
			override
		{
			if (!application::valid(aContext) || !aCompletion
				|| !claim(aContext)) {
				if (aCompletion) {
					aCompletion(application::github_credential_delete_error{
						.Context = aContext,
						.Failure = application::github_credential_vault_failure::
							invalid_request,
					});
				}
				return;
			}
			RefreshDeletes.push_back(aContext);
			if (HoldRefreshDelete) {
				// [[BPR-0545]]. The platform vault's queue: the operation is
				// accepted and stays pending under its context until the
				// worker thread dequeues it, and that window is exactly when a
				// second submission under the same context is refused. The
				// main thread ordinarily wins the race into it.
				HeldRefreshDelete        = releasing(aContext, std::move(aCompletion));
				HeldRefreshDeleteContext = aContext;
				return;
			}
			release(aContext);
			if (RefreshDeleteAnswer == refresh_delete_answer::deleted) {
				RefreshSlots.erase(aContext.Authority);
				aCompletion(application::github_credential_delete_success{
					.Context     = aContext,
					.Disposition = application::
						github_credential_delete_disposition::deleted,
				});
				return;
			}
			aCompletion(application::github_credential_delete_error{
				.Context = aContext,
				.Failure = RefreshDeleteAnswer
						== refresh_delete_answer::unavailable
					? application::github_credential_vault_failure::unavailable
					: application::github_credential_vault_failure::operation_failed,
			});
		}

		void
		cancel(application::github_credential_operation_context aContext) override
		{
			Cancellations.push_back(aContext);
		}

		// Answers a held refresh erasure, releasing its context the way the
		// `releasing` wrapper does for every other recorded operation.
		void answer_held_refresh_delete()
		{
			auto held = std::exchange(
				HeldRefreshDelete,
				application::github_credential_delete_completion_handler{ });
			expect(held != nullptr, "a refresh erasure was held");
			held(application::github_credential_delete_success{
				.Context     = HeldRefreshDeleteContext,
				.Disposition = application::
					github_credential_delete_disposition::deleted,
			});
		}

		std::vector<store_call>                                       Stores;
		std::vector<retrieve_call>                                    Retrieves;
		std::vector<delete_call>                                      Deletes;
		std::vector<std::size_t>                                      StoredCredentialBytes;
		std::vector<application::github_credential_operation_context> Cancellations;
		std::vector<application::github_credential_operation_context> RefreshRetrieves;
		std::vector<application::github_credential_operation_context> RefreshDeletes;
		std::set<application::github_credential_authority_key>        RefreshSlots;
		bool                                                          AcceptRefreshStores = false;
		application::github_credential_delete_completion_handler      HeldRefreshDelete;
		application::github_credential_operation_context              HeldRefreshDeleteContext;
		bool                                                          HoldRefreshDelete = false;
		// The contexts whose operation this double has accepted and not yet
		// answered. Held indirectly so a harness that moves the double keeps
		// one queue rather than two.
		std::shared_ptr<
			std::set<application::github_credential_operation_context>>
							  Pending = std::make_shared<
								  std::set<application::github_credential_operation_context>>();
		refresh_answer        RefreshAnswer       = refresh_answer::unavailable;
		refresh_delete_answer RefreshDeleteAnswer = refresh_delete_answer::deleted;
		bool                  ThrowOnStore        = false;
	};

	class throwing_once_lane final : public application::decision_executor
	{
	public:
		void post(std::function<void()> aTask) override
		{
			if (ThrowNext) {
				ThrowNext = false;
				throw std::runtime_error{ "decision post" };
			}
			Inner.post(std::move(aTask));
		}

		[[nodiscard]] auto pending() const noexcept -> std::size_t
		{
			return Inner.pending();
		}

		void run_next()
		{
			Inner.run_next();
		}

	private:
		application::manual_executor Inner;
		bool                         ThrowNext = true;
	};

	class recording_projections final : public application::github_projection_port
	{
	public:
		void publish(
			const application::github_presentation_update& aUpdate) override
		{
			if (aUpdate.Transition.has_value() && ThrowNextOutput) {
				ThrowNextOutput = false;
				throw std::runtime_error{ "transition projection" };
			}
			if (!aUpdate.HostDiagnostics.empty() && ThrowNextDiagnostic) {
				ThrowNextDiagnostic = false;
				throw std::runtime_error{ "diagnostic projection" };
			}
			if (aUpdate.Transition.has_value() && OnNextOutput) {
				auto action = std::exchange(OnNextOutput, std::function<void()>{ });
				action();
			}
			if (aUpdate.Transition.has_value() && OnEveryOutput) {
				OnEveryOutput();
			}
			Updates.push_back(aUpdate);
		}

		[[nodiscard]] auto transition_count() const noexcept -> std::size_t
		{
			return static_cast<std::size_t>(std::ranges::count_if(
				Updates,
				[](const auto& aUpdate) { return aUpdate.Transition.has_value(); }));
		}

		[[nodiscard]] auto session_count() const noexcept -> std::size_t
		{
			return static_cast<std::size_t>(std::ranges::count_if(
				Updates,
				[](const auto& aUpdate) { return aUpdate.Session.has_value(); }));
		}

		[[nodiscard]] auto pull_request_count() const noexcept -> std::size_t
		{
			return static_cast<std::size_t>(std::ranges::count_if(
				Updates,
				[](const auto& aUpdate) { return aUpdate.PullRequest.has_value(); }));
		}

		/// \brief [[BPR-0644]]. The reason of the last failure a remembered grant
		/// was kept through, if one was published.
		[[nodiscard]] auto kept_grant_reason() const -> std::optional<core::github_reason_code>
		{
			auto reason = std::optional<core::github_reason_code>{ };
			for (const auto& update : Updates) {
				for (const auto& diagnostic : update.HostDiagnostics) {
					if (diagnostic.GrantKept) {
						reason = diagnostic.Reason;
					}
				}
			}
			return reason;
		}

		/// \brief [[BPR-0644]]. Whether the last failure a remembered grant was
		/// kept through was the grant signing in as another account.
		[[nodiscard]] auto kept_grant_names_another_account() const -> bool
		{
			auto another = false;
			for (const auto& update : Updates) {
				for (const auto& diagnostic : update.HostDiagnostics) {
					if (diagnostic.GrantKept) {
						another = diagnostic.AccountMismatch;
					}
				}
			}
			return another;
		}

		[[nodiscard]] auto host_diagnostic_count() const noexcept -> std::size_t
		{
			std::size_t count = 0U;
			for (const auto& update : Updates) {
				count += update.HostDiagnostics.size();
			}
			return count;
		}

		[[nodiscard]] auto last_session_update() const
			-> const application::github_presentation_update&
		{
			const auto found = std::ranges::find_if(
				Updates.rbegin(),
				Updates.rend(),
				[](const auto& aUpdate) { return aUpdate.Session.has_value(); });
			expect(found != Updates.rend(), "a session projection update exists");
			return *found;
		}

		[[nodiscard]] auto last_transition_update() const
			-> const application::github_presentation_update&
		{
			const auto found = std::ranges::find_if(
				Updates.rbegin(),
				Updates.rend(),
				[](const auto& aUpdate) { return aUpdate.Transition.has_value(); });
			expect(
				found != Updates.rend(),
				"a transition projection update exists");
			return *found;
		}

		[[nodiscard]] auto last_pull_request_update() const
			-> const application::github_presentation_update&
		{
			const auto found = std::ranges::find_if(
				Updates.rbegin(),
				Updates.rend(),
				[](const auto& aUpdate) { return aUpdate.PullRequest.has_value(); });
			expect(
				found != Updates.rend(),
				"a pull-request projection update exists");
			return *found;
		}

		[[nodiscard]] auto last_host_diagnostic() const
			-> const application::github_host_diagnostic_projection&
		{
			for (auto update = Updates.rbegin(); update != Updates.rend(); ++update) {
				if (!update->HostDiagnostics.empty()) {
					return update->HostDiagnostics.back();
				}
			}
			expect(false, "a host diagnostic projection exists");
			std::abort();
		}

		std::vector<application::github_presentation_update> Updates;
		bool                                                 ThrowNextOutput     = false;
		bool                                                 ThrowNextDiagnostic = false;
		std::function<void()>                                OnNextOutput;
		std::function<void()>                                OnEveryOutput;
	};

	class recording_epoch_handoffs final
		: public application::github_epoch_handoff_port
	{
	public:
		void accept(application::accepted_pull_request_handoff aHandoff) override
		{
			Accepted.push_back(std::move(aHandoff));
		}

		void invalidate(
			application::pull_request_authority_invalidation aInvalidation) override
		{
			Invalidated.push_back(aInvalidation);
			if (ThrowInvalidation) {
				throw std::runtime_error{ "injected epoch invalidation failure" };
			}
		}

		std::vector<application::accepted_pull_request_handoff>       Accepted;
		std::vector<application::pull_request_authority_invalidation> Invalidated;
		bool                                                          ThrowInvalidation = false;
	};

	[[nodiscard]] auto initial_state() -> core::github_state
	{
		return core::github_state{
			.ApplicationSession    = core::application_session_id{ 41U },
			.NextRequest           = core::request_id{ 1U },
			.ActiveProfile         = std::nullopt,
			.IdentityGeneration    = { },
			.PendingSession        = std::nullopt,
			.Host                  = std::nullopt,
			.Account               = std::nullopt,
			.Capabilities          = { },
			.CapabilityRevision    = { },
			.PullRequestGeneration = { },
			.PendingPullRequest    = std::nullopt,
			.ActiveLocator         = std::nullopt,
			.ActiveNumber          = std::nullopt,
			.Repository            = std::nullopt,
			.PullRequest           = std::nullopt,
			.AcceptedSnapshot      = std::nullopt,
		};
	}

	[[nodiscard]] auto native_profile(const core::host_profile_id aProfile)
		-> application::github_profile_authority
	{
		return {
			.Profile               = aProfile,
			.ProfileRevision       = application::github_profile_revision{ 31U },
			.Host                  = core::git_host_id{ 21U },
			.Hostname              = application::github_canonical_hostname{ "github.com" },
			.ServerContract        = application::github_server_contract::github_com_2022_11_28,
			.Authentication        = application::github_authentication_transport::native_https,
			.ProbeContractRevision = application::github_probe_contract_revision{ 1U },
		};
	}

	[[nodiscard]] auto cli_profile(const core::host_profile_id aProfile)
		-> application::github_profile_authority
	{
		auto result           = native_profile(aProfile);
		result.Authentication = application::github_authentication_transport::github_cli;
		return result;
	}

	[[nodiscard]] auto ghes_cli_profile(const core::host_profile_id aProfile)
		-> application::github_profile_authority
	{
		auto result           = cli_profile(aProfile);
		result.Hostname       = application::github_canonical_hostname{ "ghe.example.invalid" };
		result.ServerContract = application::github_server_contract::ghes_3_18_2022_11_28;
		return result;
	}

	[[nodiscard]] auto ghes_native_profile(
		const core::host_profile_id aProfile,
		const std::string_view      aHostname = "ghe.example.invalid")
		-> application::github_profile_authority
	{
		auto result           = native_profile(aProfile);
		result.Hostname       = application::github_canonical_hostname{ aHostname };
		result.ServerContract = application::github_server_contract::ghes_3_18_2022_11_28;
		return result;
	}

	[[nodiscard]] auto
	viewer(const core::github_account_id aAccount = core::github_account_id{ 51U })
		-> application::github_viewer_record
	{
		return {
			.Account     = aAccount,
			.Node        = application::github_actor_id{ "viewer-node" },
			.Login       = "reviewer",
			.DisplayName = "Review Engineer",
		};
	}

	[[nodiscard]] auto
	rate_limit(const application::github_rate_limit_resource aResource = application::github_rate_limit_resource::core)
		-> application::github_rate_limit_record
	{
		return {
			.Resource         = aResource,
			.Limit            = 5'000U,
			.Remaining        = 4'999U,
			.ResetUnixSeconds = 2'000'000'000U,
		};
	}

	[[nodiscard]] auto rate_limits() -> application::github_rate_limit_state
	{
		auto search       = rate_limit(application::github_rate_limit_resource::search);
		search.Limit      = 30U;
		search.Remaining  = 29U;
		auto graphql      = rate_limit(application::github_rate_limit_resource::graphql);
		graphql.Remaining = 4'998U;
		return {
			.Core    = rate_limit(application::github_rate_limit_resource::core),
			.Search  = search,
			.Graphql = graphql,
		};
	}

	[[nodiscard]] auto projected_rate_limit(
		const application::github_rate_limit_record& aRecord)
		-> application::github_rate_limit_projection
	{
		application::github_rate_limit_resource_projection resource = application::github_rate_limit_resource_projection::core;
		if (aRecord.Resource == application::github_rate_limit_resource::search) {
			resource = application::github_rate_limit_resource_projection::search;
		} else if (aRecord.Resource == application::github_rate_limit_resource::graphql) {
			resource = application::github_rate_limit_resource_projection::graphql;
		}

		return {
			.Resource         = resource,
			.Limit            = aRecord.Limit,
			.Remaining        = aRecord.Remaining,
			.ResetUnixSeconds = aRecord.ResetUnixSeconds,
			.SecondaryLimited = aRecord.SecondaryLimited,
		};
	}

	[[nodiscard]] auto projected_rate_limits(
		const application::github_rate_limit_state& aState)
		-> application::github_rate_limits_projection
	{
		const auto project = [](const auto& aRecord)
			-> std::optional<application::github_rate_limit_projection> {
			return aRecord.has_value()
				? std::optional{ projected_rate_limit(*aRecord) }
				: std::nullopt;
		};
		return {
			.Core    = project(aState.Core),
			.Search  = project(aState.Search),
			.Graphql = project(aState.Graphql),
		};
	}

	[[nodiscard]] auto
	rate_limits_for(const application::github_rate_limit_record& aRecord)
		-> application::github_rate_limit_state
	{
		application::github_rate_limit_state result;
		expect(application::observe(result, aRecord),
			   "single-resource rate fixture is valid");
		return result;
	}

	[[nodiscard]] auto
	rate_limits_for(const application::github_rate_limit_resource aResource)
		-> application::github_rate_limit_state
	{
		return rate_limits_for(rate_limit(aResource));
	}

	[[nodiscard]] auto capability_facts()
		-> std::array<application::github_capability_fact,
					  application::github_capability_fact_count>
	{
		using capability = core::github_capability;
		using fact       = application::github_capability_fact;
		using source     = application::github_capability_evidence_source;
		using state      = core::github_capability_state;
		return {
			fact{
				.Capability = capability::review_threads,
				.State      = state::supported,
				.Source     = source::observed_contract,
			},
			fact{
				.Capability = capability::viewed_file_mutations,
				.State      = state::supported,
				.Source     = source::observed_contract,
			},
			fact{
				.Capability = capability::resolve_thread,
				.State      = state::supported,
				.Source     = source::observed_contract,
			},
			fact{
				.Capability = capability::unresolve_thread,
				.State      = state::supported,
				.Source     = source::observed_contract,
			},
		};
	}

	[[nodiscard]] auto schema_facts()
		-> std::array<application::github_schema_capability_fact,
					  application::github_schema_capability_fact_count>
	{
		using capability = application::github_schema_capability;
		using fact       = application::github_schema_capability_fact;
		using source     = application::github_capability_evidence_source;
		using state      = core::github_capability_state;
		return {
			fact{ capability::review_threads_read,
				  state::supported,
				  source::observed_contract },
			fact{ capability::viewed_file_state_read,
				  state::supported,
				  source::observed_contract },
			fact{ capability::mark_file_as_viewed,
				  state::supported,
				  source::observed_contract },
			fact{ capability::unmark_file_as_viewed,
				  state::supported,
				  source::observed_contract },
			fact{ capability::resolve_permission,
				  state::supported,
				  source::observed_contract },
			fact{ capability::resolve_review_thread,
				  state::supported,
				  source::observed_contract },
			fact{ capability::unresolve_permission,
				  state::supported,
				  source::observed_contract },
			fact{ capability::unresolve_review_thread,
				  state::supported,
				  source::observed_contract },
		};
	}

	[[nodiscard]] auto unsupported_capability_facts()
		-> std::array<application::github_capability_fact,
					  application::github_capability_fact_count>
	{
		auto result = capability_facts();
		for (auto& fact : result) {
			fact.State = core::github_capability_state::unsupported;
		}
		return result;
	}

	[[nodiscard]] auto unsupported_schema_facts()
		-> std::array<application::github_schema_capability_fact,
					  application::github_schema_capability_fact_count>
	{
		auto result = schema_facts();
		for (auto& fact : result) {
			fact.State = core::github_capability_state::unsupported;
		}
		return result;
	}

	[[nodiscard]] auto credential() -> application::github_credential_material
	{
		const std::array bytes{
			std::byte{ 0x01 },
			std::byte{ 0x02 },
			std::byte{ 0x03 },
		};
		auto result = application::github_credential_material::create(bytes);
		expect(result.has_value(), "credential fixture allocates");
		return std::move(*result);
	}

	[[nodiscard]] auto
	snapshot(const application::github_pull_request_provider_request& aRequest)
		-> application::github_pull_request_snapshot
	{
		return {
			.Profile = aRequest.Context.Profile,
			.Host    = aRequest.Context.Host,
			.Account = aRequest.Context.Account,
			.Viewer  = {
				.Actor = aRequest.Viewer.Node,
				.Login = aRequest.Viewer.Login,
			},
			.Snapshot   = core::provider_snapshot_id{ 91U },
			.Repository = {
				.Repository    = core::repository_id{ 71U },
				.Node          = application::github_node_id{ "repository-node" },
				.Owner         = aRequest.Context.Repository.Owner,
				.Name          = aRequest.Context.Repository.Name,
				.DefaultBranch = "main",
			},
			.PullRequest = {
				.PullRequest = core::pull_request_id{ 81U },
				.Repository  = core::repository_id{ 71U },
				.Number      = aRequest.Context.Number,
				.Node        = application::github_node_id{ "pull-request-node" },
				.Title       = "Host orchestration",
				.Body        = "Bounded.",
				.State       = application::github_pull_request_state::open,
				.Draft       = false,
				.Author      = std::nullopt,
				.BaseOid     = application::commit_oid{ "1111111111111111111111111111111111111111" },
				.HeadOid     = application::commit_oid{ "2222222222222222222222222222222222222222" },
				.BaseRef     = "main",
				.HeadRef     = "host",
			},
			.Files                     = { },
			.Commits                   = { },
			.Reviews                   = { },
			.ReviewThreadsAvailability = application::github_collection_availability::available,
			.ReviewThreads             = { },
			.IssueComments             = { },
			.Checks                    = { },
			.SearchResults             = { },
			.CapabilityEvidence        = {
				.Capabilities    = aRequest.CapabilityEvidence.Capabilities,
				.Revision        = aRequest.CapabilityEvidence.Revision,
				.ReadShape       = aRequest.CapabilityEvidence.ReadShape,
				.ContractVersion = 1U,
			},
			.RateLimits = rate_limits(),
		};
	}

	[[nodiscard]] auto rich_snapshot(
		const application::github_pull_request_provider_request& aRequest)
		-> application::github_pull_request_snapshot
	{
		auto                                   result = snapshot(aRequest);
		const application::github_actor_record author{
			.Actor = application::github_actor_id{ "author-node" },
			.Login = "octo-author",
		};
		const application::github_actor_record reviewer{
			.Actor = aRequest.Viewer.Node,
			.Login = aRequest.Viewer.Login,
		};
		constexpr std::string_view path = "src/application/host.cpp";

		result.PullRequest.Author = author;
		result.Files.push_back(application::github_file_record{
			.Path         = std::string{ path },
			.PreviousPath = std::nullopt,
			.Change       = application::github_file_change::modified,
			.Additions    = 4U,
			.Deletions    = 2U,
			.ContentKind  = application::github_file_content_kind::text,
			.ViewedState  = application::github_file_viewed_state::viewed,
		});
		result.Reviews.push_back(application::github_review_record{
			.DatabaseId = 4'001U,
			.Review     = application::github_review_id{ "review-1" },
			.Author     = reviewer,
			.State      = application::github_review_state::changes_requested,
			.Commit     = std::nullopt,
			.Body       = "Please preserve identity.",
		});
		result.ReviewThreads.push_back(application::github_review_thread_record{
			.Thread                    = application::github_thread_id{ "thread-1" },
			.Subject                   = application::github_review_thread_subject::line,
			.Path                      = std::string{ path },
			.Line                      = 42U,
			.OriginalLine              = 37U,
			.DiffSide                  = application::github_diff_side::right,
			.Resolved                  = true,
			.Outdated                  = false,
			.ViewerResolvePermission   = application::github_permission_state::denied,
			.ViewerUnresolvePermission = application::github_permission_state::allowed,
			.Comments                  = {
				application::github_review_comment_record{
					.Comment      = application::github_comment_id{ "review-comment-1" },
					.ReplyTo      = std::nullopt,
					.Author       = reviewer,
					.Body         = "Thread root",
					.Path         = std::string{ path },
					.Line         = 42U,
					.OriginalLine = 37U,
					.Outdated     = false,
					.Deleted      = false,
					// [[BPR-1564]]. A comment on lines 40 to 42.
					.Observation = { .StartLine = 40U },
				},
				application::github_review_comment_record{
					.Comment      = application::github_comment_id{ "review-comment-2" },
					.ReplyTo      = application::github_comment_id{ "review-comment-1" },
					.Author       = author,
					.Body         = "Follow-up",
					.Path         = std::string{ path },
					.Line         = 42U,
					.OriginalLine = 37U,
					.Outdated     = true,
					.Deleted      = true,
				},
				application::github_review_comment_record{
					.Comment      = application::github_comment_id{ "review-comment-3" },
					.ReplyTo      = application::github_comment_id{ "review-comment-2" },
					.Author       = reviewer,
					.Body         = "Third level",
					.Path         = std::string{ path },
					.Line         = 42U,
					.OriginalLine = 37U,
					.Outdated     = false,
					.Deleted      = false,
				},
			},
		});
		result.IssueComments.push_back(application::github_issue_comment_record{
			.Comment = application::github_comment_id{ "issue-comment-1" },
			.Author  = author,
			.Body    = "Issue follow-up",
			.Deleted = false,
		});
		return result;
	}

	struct host_fixture
	{
		fake_provider                                   Provider;
		fake_vault                                      Vault;
		application::manual_executor                    Lane;
		recording_projections                           Projections;
		application::github_credential_binding_catalog  OwnedBindings;
		application::github_credential_binding_catalog& Bindings;
		application::github_application_host            Host;

		host_fixture()
			: Bindings{ OwnedBindings }
			, Host{ initial_state(),
					Provider,
					Provider,
					Provider,
					Provider,
					Vault,
					Bindings,
					Lane,
					Projections }
		{
		}

		explicit host_fixture(
			const application::github_credential_shutdown_policy aShutdownPolicy, const bool aEnablePreview = false)
			: Bindings{ OwnedBindings }
			, Host{ initial_state(),
					Provider,
					Provider,
					Provider,
					Provider,
					Vault,
					Bindings,
					Lane,
					Projections,
					aShutdownPolicy, nullptr, aEnablePreview ? &Provider : nullptr }
		{
		}

		explicit host_fixture(
			application::github_credential_binding_catalog&      aBindings,
			const application::github_credential_shutdown_policy aShutdownPolicy = application::github_credential_shutdown_policy::retain,
			core::bpr_profile_id                                 aBprProfile     = core::default_bpr_profile_id)
			: Bindings{ aBindings }
			, Host{ initial_state(),
					Provider,
					Provider,
					Provider,
					Provider,
					Vault,
					Bindings,
					Lane,
					Projections,
					aShutdownPolicy, nullptr, nullptr, nullptr, nullptr, aBprProfile }
		{
		}

		explicit host_fixture(application::github_epoch_handoff_port& aEpochs)
			: Bindings{ OwnedBindings }
			, Host{ initial_state(),
					Provider,
					Provider,
					Provider,
					Provider,
					Vault,
					Bindings,
					Lane,
					Projections,
					application::github_credential_shutdown_policy::retain,
					&aEpochs }
		{
		}
	};

	auto advance_native_to_store(host_fixture& aFixture) -> void
	{
		aFixture.Host.enroll_profile(core::host_profile_id{ 11U }, credential());
		const auto profile = aFixture.Provider.Profiles.front().Request;
		aFixture.Provider.Profiles.front().Completion(
			application::github_profile_success{
				.Request   = profile,
				.Authority = native_profile(profile.Context.Profile),
			});
		aFixture.Lane.run_next();
		const auto bootstrap = aFixture.Provider.Viewers.front().Request;
		aFixture.Provider.Viewers.front().Completion(
			application::github_viewer_success{
				.Request          = bootstrap,
				.Viewer           = viewer(),
				.RateLimits       = rate_limits_for(application::github_rate_limit_resource::core),
				.AccountTransport = { },
			});
		aFixture.Lane.run_next();
	}

	auto advance_native_to_retrieve(host_fixture& aFixture) -> void
	{
		advance_native_to_store(aFixture);
		const auto store = aFixture.Vault.Stores.front().Context;
		aFixture.Vault.Stores.front().Completion(
			application::github_credential_store_success{
				.Context     = store,
				.Disposition = application::github_credential_store_disposition::stored,
			});
		aFixture.Lane.run_next();
	}

	auto establish_native_session(
		host_fixture&                 aFixture,
		const core::host_profile_id   aProfile = core::host_profile_id{ 11U },
		const core::github_account_id aAccount = core::github_account_id{ 51U },
		const std::optional<application::github_profile_authority>&
														aResolvedProfile = std::nullopt,
		const application::github_credential_provenance aProvenance
		= application::github_credential_provenance::manual,
		std::optional<application::device_binding_lifetime> aLifetime        = std::nullopt,
		const bool                                          aPrimaryAttached = true)
		-> std::shared_ptr<fake_lease>
	{
		aFixture.Host.enroll_profile(aProfile, credential(), aProvenance, aLifetime);
		expect(aFixture.Provider.Profiles.size() == 1U,
			   "activation synchronously submits only profile resolution");
		const auto profileRequest = aFixture.Provider.Profiles.front().Request;
		aFixture.Provider.Profiles.front().Completion(
			application::github_profile_success{
				.Request   = profileRequest,
				.Authority = aResolvedProfile.value_or(
					native_profile(profileRequest.Context.Profile)),
			});
		expect(aFixture.Provider.Viewers.empty(),
			   "worker completion cannot advance the host before decision lane");
		aFixture.Lane.run_next();
		expect(aFixture.Provider.Viewers.size() == 1U
				   && aFixture.Provider.Viewers.front().Authentication
					   == fake_provider::viewer_authentication::bootstrap,
			   "native enrollment performs a typed bootstrap viewer observation");

		const auto bootstrapRequest = aFixture.Provider.Viewers.front().Request;
		aFixture.Provider.Viewers.front().Completion(
			application::github_viewer_success{
				.Request          = bootstrapRequest,
				.Viewer           = viewer(aAccount),
				.RateLimits       = rate_limits_for(application::github_rate_limit_resource::core),
				.AccountTransport = { },
			});
		expect(aFixture.Vault.Stores.empty(),
			   "bootstrap result is serialized before vault mutation");
		aFixture.Lane.run_next();
		expect(aFixture.Vault.Stores.size() == 1U
				   && aFixture.Vault.Stores.front().Context.Authority.Account
					   == aAccount,
			   "vault key uses only the account observed from bootstrap /user");

		const auto storeContext = aFixture.Vault.Stores.front().Context;
		aFixture.Vault.Stores.front().Completion(
			application::github_credential_store_success{
				.Context     = storeContext,
				.Disposition = application::github_credential_store_disposition::stored,
			});
		aFixture.Lane.run_next();
		expect(aFixture.Vault.Retrieves.size() == 1U
				   && aFixture.Vault.Retrieves.front().Context.Authority
					   == storeContext.Authority,
			   "stored bootstrap credential is retrieved under the same authority");

		const auto retrieveContext = aFixture.Vault.Retrieves.front().Context;
		aFixture.Vault.Retrieves.front().Completion(
			application::github_credential_retrieve_success{
				.Context    = retrieveContext,
				.Credential = credential(),
			});
		aFixture.Lane.run_next();
		expect(aFixture.Provider.Viewers.size() == 2U
				   && aFixture.Provider.Viewers.back().Authentication
					   == fake_provider::viewer_authentication::account,
			   "exact retrieval drives a second account-authenticated /user");

		const auto accountRequest = aFixture.Provider.Viewers.back().Request;
		const auto lease          = std::make_shared<fake_lease>();
		aFixture.Provider.Viewers.back().Completion(
			application::github_viewer_success{
				.Request          = accountRequest,
				.Viewer           = viewer(aAccount),
				.RateLimits       = rate_limits_for(application::github_rate_limit_resource::core),
				.AccountTransport = lease,
			});
		aFixture.Lane.run_next();
		expect(aFixture.Provider.Capabilities.size() == 1U
				   && aFixture.Provider.Capabilities.front().Request.AccountTransport
					   == lease,
			   "only the exact account-authenticated lease reaches probing");

		const auto capabilityRequest = aFixture.Provider.Capabilities.front().Request;
		aFixture.Provider.Capabilities.front().Completion(
			application::github_capability_success{
				.Request     = capabilityRequest,
				.Facts       = capability_facts(),
				.SchemaFacts = schema_facts(),
				.RateLimits  = rate_limits_for(
					application::github_rate_limit_resource::graphql),
			});
		expect(!aFixture.Host.accepted_session().has_value()
				   && aFixture.Host.capability_cache_size() == 0U,
			   "capability success cannot mutate core or cache off lane");
		aFixture.Lane.run_next();
		expect(aFixture.Host.accepted_session().has_value()
				   && aFixture.Host.capability_cache_size() == 1U
				   && aFixture.Projections.session_count() == (aPrimaryAttached ? 1U : 0U),
			   "core acceptance precedes one exact capability-cache commit");
		return lease;
	}

	[[nodiscard]] auto smart_context(
		const application::accepted_pull_request_handoff& aHandoff)
		-> application::git_smart_request_context
	{
		return {
			.ApplicationSession = aHandoff.ApplicationSession,
			.Authority          = {
				.Profile         = aHandoff.Profile,
				.ProfileRevision = aHandoff.ProfileRevision,
				.Host            = aHandoff.Host,
				.HostAuthority   = aHandoff.HostAuthority,
				.Account         = aHandoff.Account,
				.Viewer          = aHandoff.Viewer,
				.Repository      = aHandoff.Repository,
				.Cache           = opaque<core::repository_cache_scope_id>(1U),
				.PullRequest     = aHandoff.PullRequest,
				.Number          = aHandoff.Number,
				.Scope           = opaque<core::pull_request_authority_scope_id>(2U),
			},
			.Revision = {
				.Source = {
					.Observation           = opaque<core::epoch_source_observation_id>(3U),
					.Snapshot              = aHandoff.Snapshot,
					.IdentityGeneration    = aHandoff.IdentityGeneration,
					.PullRequestGeneration = aHandoff.PullRequestGeneration,
				},
				.Origin              = aHandoff.Origin,
				.BaseBranch          = aHandoff.BaseBranch,
				.Base                = aHandoff.Base,
				.Head                = aHandoff.Head,
				.CurrentViewerReview = aHandoff.CurrentViewerReview,
			},
			.Repository = aHandoff.Locator,
			.BaseRef    = aHandoff.BaseRef,
		};
	}

	auto advance_resume_to_capability(
		host_fixture&                                       aFixture,
		const application::github_credential_authority_key& aExpectedAuthority)
		-> application::github_capability_request
	{
		aFixture.Host.resume_profile(core::host_profile_id{ 11U });
		expect(aFixture.Provider.Profiles.size() == 1U,
			   "resume resolves only the selected profile");
		const auto profile = aFixture.Provider.Profiles.front().Request;
		aFixture.Provider.Profiles.front().Completion(
			application::github_profile_success{
				.Request   = profile,
				.Authority = native_profile(profile.Context.Profile),
			});
		aFixture.Lane.run_next();
		expect(aFixture.Vault.Retrieves.size() == 1U
				   && aFixture.Vault.Retrieves.front().Context.Authority
					   == aExpectedAuthority
				   && aFixture.Vault.Stores.empty()
				   && aFixture.Provider.Viewers.empty(),
			   "trusted resume retrieves the exact authority without bootstrap or "
			   "store");

		const auto retrieve = aFixture.Vault.Retrieves.front().Context;
		aFixture.Vault.Retrieves.front().Completion(
			application::github_credential_retrieve_success{
				.Context    = retrieve,
				.Credential = credential(),
			});
		aFixture.Lane.run_next();
		expect(aFixture.Provider.Viewers.size() == 1U
				   && aFixture.Provider.Viewers.front().Authentication
					   == fake_provider::viewer_authentication::account,
			   "resumed material can perform only the account-authenticated /user");

		const auto account = aFixture.Provider.Viewers.front().Request;
		aFixture.Provider.Viewers.front().Completion(
			application::github_viewer_success{
				.Request          = account,
				.Viewer           = viewer(),
				.RateLimits       = rate_limits_for(application::github_rate_limit_resource::core),
				.AccountTransport = std::make_shared<fake_lease>(),
			});
		aFixture.Lane.run_next();
		expect(aFixture.Provider.Capabilities.size() == 1U,
			   "the resumed credential reaches probing only after exact /user");
		return aFixture.Provider.Capabilities.front().Request;
	}

	auto seed_persistent_binding(
		application::github_credential_binding_catalog& aBindings,
		const core::host_profile_id                     aProfile = core::host_profile_id{ 11U },
		const core::github_account_id                   aAccount = core::github_account_id{ 51U },
		const std::optional<application::github_profile_authority>&
			aResolvedProfile = std::nullopt)
		-> application::github_credential_authority_key
	{
		const auto   priorBindings = aBindings.size();
		host_fixture source{ aBindings };
		establish_native_session(source, aProfile, aAccount, aResolvedProfile);
		const auto authority = source.Vault.Stores.front().Context.Authority;
		source.Host.shutdown();
		expect(aBindings.size() == priorBindings + 1U
				   && source.Vault.Deletes.empty(),
			   "real enrollment seeds one retained host-observed binding");
		return authority;
	}

	void test_bpr_profile_credential_admission()
	{
		application::github_credential_binding_catalog firstBindings;
		application::github_credential_binding_catalog secondBindings;
		host_fixture                                   first{ firstBindings, application::github_credential_shutdown_policy::retain, core::bpr_profile_id{ 2U } };
		host_fixture                                   second{ secondBindings, application::github_credential_shutdown_policy::retain, core::bpr_profile_id{ 3U } };
		establish_native_session(first);
		establish_native_session(second);
		auto       firstKey  = first.Vault.Stores.front().Context.Authority;
		const auto secondKey = second.Vault.Stores.front().Context.Authority;
		expect(firstKey.BprProfile == core::bpr_profile_id{ 2U } && secondKey.BprProfile == core::bpr_profile_id{ 3U } && firstKey != secondKey, "same host/account enrollment mints disjoint admitted BPR authorities");
		firstKey.BprProfile = secondKey.BprProfile;
		expect(firstKey == secondKey, "only admitted BPR scope differs for identical accepted viewers");
		host_fixture foreign{ firstBindings };
		foreign.Host.resume_profile(core::host_profile_id{ 11U });
		expect(foreign.Provider.Profiles.empty() && foreign.Vault.Retrieves.empty() && foreign.Vault.Deletes.empty(), "foreign remembered authority refuses before provider or vault effects");
		bool accepted = true;
		first.Host.prepare_device_rotation(secondKey, first.Host.state().IdentityGeneration, [&accepted](bool aResult) { accepted = aResult; });
		expect(!accepted, "foreign refresh authority cannot prepare rotation");
		first.Host.shutdown();
		second.Host.shutdown();
		foreign.Host.shutdown();
		bool rejected = false;
		try {
			host_fixture invalid{ firstBindings, application::github_credential_shutdown_policy::retain, core::bpr_profile_id{ 0U } };
		} catch (const std::invalid_argument&) {
			rejected = true;
		}
		expect(rejected, "zero BPR profile admission is invalid");
	}

	auto test_bootstrap_failure_stages_are_exact_and_replacement_safe() -> void
	{
		for (const auto detail : { application::github_capability_diagnostic::graphql_errors, static_cast<application::github_capability_diagnostic>(255U) }) {
			for (const auto stage : { application::github_bootstrap_failure_stage::account_identity,
									  application::github_bootstrap_failure_stage::capability_discovery }) {
				for (const bool replace : { false, true }) {
					host_fixture fixture;
					fixture.Host.enroll_profile(core::host_profile_id{ 11U }, credential());
					const auto profile = fixture.Provider.Profiles.front().Request;
					fixture.Provider.Profiles.front().Completion(application::github_profile_success{
						.Request = profile, .Authority = native_profile(profile.Context.Profile) });
					fixture.Lane.drain();
					if (stage == application::github_bootstrap_failure_stage::capability_discovery) {
						const auto bootstrap = fixture.Provider.Viewers.front().Request;
						fixture.Provider.Viewers.front().Completion(application::github_viewer_success{
							.Request = bootstrap, .Viewer = viewer(), .RateLimits = rate_limits_for(application::github_rate_limit_resource::core), .AccountTransport = { } });
						fixture.Lane.drain();
						const auto store = fixture.Vault.Stores.front().Context;
						fixture.Vault.Stores.front().Completion(application::github_credential_store_success{
							.Context = store, .Disposition = application::github_credential_store_disposition::stored });
						fixture.Lane.drain();
						const auto retrieve = fixture.Vault.Retrieves.front().Context;
						fixture.Vault.Retrieves.front().Completion(application::github_credential_retrieve_success{
							.Context = retrieve, .Credential = credential() });
						fixture.Lane.drain();
						const auto account = fixture.Provider.Viewers.back().Request;
						fixture.Provider.Viewers.back().Completion(application::github_viewer_success{
							.Request = account, .Viewer = viewer(), .RateLimits = rate_limits_for(application::github_rate_limit_resource::core), .AccountTransport = std::make_shared<fake_lease>() });
						fixture.Lane.drain();
					}
					if (replace) {
						fixture.Host.dispatch(core::activate_host_profile_intent{ .Profile = core::host_profile_id{ 12U } });
					}
					if (stage == application::github_bootstrap_failure_stage::account_identity) {
						const auto call = fixture.Provider.Viewers.front();
						call.Completion(application::github_viewer_error{
							.Request = call.Request, .Error = { .Failure = application::github_provider_failure::malformed_response } });
					} else {
						const auto call = fixture.Provider.Capabilities.front();
						call.Completion(application::github_capability_error{
							.Request = call.Request, .Error = { .Failure = application::github_provider_failure::malformed_response }, .CapabilityDetail = detail });
					}
					fixture.Lane.drain();
					if (!fixture.Vault.Deletes.empty()) {
						const auto call = fixture.Vault.Deletes.front();
						call.Completion(application::github_credential_delete_success{
							.Context = call.Context, .Disposition = application::github_credential_delete_disposition::deleted });
						fixture.Lane.drain();
					}
					if (replace) {
						expect(fixture.Projections.host_diagnostic_count() == 0U
								   && fixture.Host.state().ActiveProfile == core::host_profile_id{ 12U },
							   "retired bootstrap failure cannot label or fail the replacement identity");
					} else {
						const auto& update = fixture.Projections.last_transition_update();
						expect(update.Transition->Status->Status == core::github_status::session_failed
								   && update.Transition->Diagnostics.front().Code == core::github_reason_code::malformed_response
								   && update.HostDiagnostics.size() == 1U
								   && update.HostDiagnostics.front().BootstrapStage == stage
								   && update.HostDiagnostics.front().CapabilityDetail == (stage == application::github_bootstrap_failure_stage::capability_discovery ? application::bounded_capability_diagnostic(detail) : application::github_capability_diagnostic::none),
							   "exact bootstrap stage accompanies its unchanged core failure after cleanup");
					}
				}
			}
		}
	}

	auto test_local_capability_refusal_details() -> void
	{
		using diagnostic = application::github_capability_diagnostic;
		for (const auto detail : { diagnostic::host_correlation, diagnostic::host_rate_state, diagnostic::host_cache_stage, diagnostic::host_provider_error }) {
			application::github_credential_binding_catalog bindings;
			const auto                                     authority = seed_persistent_binding(bindings);
			host_fixture                                   fixture{ bindings };
			auto                                           request = advance_resume_to_capability(fixture, authority);
			auto                                           facts   = capability_facts();
			auto                                           rates   = rate_limits_for(application::github_rate_limit_resource::graphql);
			if (detail == diagnostic::host_correlation) {
				request.Context.Request = core::request_id{ 999U };
			}
			if (detail == diagnostic::host_rate_state) {
				rates = rate_limits_for(application::github_rate_limit_resource::search);
			}
			if (detail == diagnostic::host_cache_stage) {
				facts.front().State = static_cast<core::github_capability_state>(255U);
			}
			if (detail == diagnostic::host_provider_error) {
				fixture.Provider.Capabilities.front().Completion(application::github_capability_error{
					.Request          = request,
					.Error            = { .Failure = static_cast<application::github_provider_failure>(255U) },
					.CapabilityDetail = diagnostic::host_cache_stage,
				});
			} else {
				fixture.Provider.Capabilities.front().Completion(application::github_capability_success{
					.Request = request, .Facts = std::move(facts), .SchemaFacts = schema_facts(), .RateLimits = rates });
			}
			fixture.Lane.drain();
			expect(fixture.Projections.last_host_diagnostic().CapabilityDetail == detail
					   && fixture.Projections.last_host_diagnostic().Reason == core::github_reason_code::malformed_response,
				   "local capability refusal exposes its exact branch without changing core reason");
		}
	}

	auto test_setup_authentication_preserves_review_until_exact_acceptance() -> void
	{
		host_fixture fixture;
		establish_native_session(fixture);
		fixture.Host.dispatch(core::open_pull_request_intent{ .Repository = { "openai", "better-pull-review" }, .Number = core::pull_request_number{ 27U } });
		const auto reviewRequest = fixture.Provider.PullRequests.back().Request;
		fixture.Provider.PullRequests.back().Completion(application::github_pull_request_success{ .Request = reviewRequest, .Snapshot = snapshot(reviewRequest) });
		fixture.Lane.drain();
		const auto state         = fixture.Host.state();
		const auto review        = fixture.Host.accepted_snapshot();
		const auto stores        = fixture.Vault.Stores.size();
		const auto pending       = core::pending_setup_destination{ .Request = core::setup_request_id{ 1U }, .Owner = core::setup_owner_id{ 1U }, .Destination = { .Hostname = "github.com", .Repository = { "other", "repo" }, .Number = core::pull_request_number{ 42U }, .Location = { } }, .Profile = core::host_profile_id{ 12U }, .Stage = core::setup_destination_stage::authenticating };
		auto       acceptedCalls = std::size_t{ 0U };
		fixture.Host.stage_setup_enrollment(pending, credential(), [&](const auto&, bool) { ++acceptedCalls; return true; });
		const auto profileRequest = fixture.Provider.Profiles.back().Request;
		fixture.Provider.Profiles.back().Completion(application::github_profile_success{ .Request = profileRequest, .Authority = native_profile(core::host_profile_id{ 12U }) });
		fixture.Lane.drain();
		const auto viewerRequest = fixture.Provider.Viewers.back().Request;
		fixture.Provider.Viewers.back().Completion(application::github_viewer_success{ .Request = viewerRequest, .Viewer = viewer(core::github_account_id{ 52U }), .RateLimits = rate_limits_for(application::github_rate_limit_resource::core), .AccountTransport = { } });
		fixture.Host.cancel_setup_enrollment();
		fixture.Lane.drain();
		expect(acceptedCalls == 0U && fixture.Host.state() == state && fixture.Host.accepted_snapshot() == review
				   && fixture.Vault.Stores.size() == stores,
			   "cancelled staged /user leaves active review and vault untouched");
		fixture.Host.stage_setup_enrollment(pending, credential(), [&](const auto&, bool) { ++acceptedCalls; return false; });
		const auto secondProfile = fixture.Provider.Profiles.back().Request;
		fixture.Provider.Profiles.back().Completion(application::github_profile_success{ .Request = secondProfile, .Authority = native_profile(core::host_profile_id{ 12U }) });
		fixture.Lane.drain();
		const auto secondViewer = fixture.Provider.Viewers.back().Request;
		fixture.Provider.Viewers.back().Completion(application::github_viewer_success{ .Request = secondViewer, .Viewer = viewer(core::github_account_id{ 52U }), .RateLimits = rate_limits_for(application::github_rate_limit_resource::core), .AccountTransport = { } });
		fixture.Lane.drain();
		expect(acceptedCalls == 1U && fixture.Host.state() == state && fixture.Host.accepted_snapshot() == review
				   && fixture.Vault.Stores.size() == stores,
			   "obsolete owner refuses commit without replacing review");
	}

	auto test_setup_restage_same_destination_has_fresh_attempt() -> void
	{
		for (const auto mode : { 0, 1, 2 }) {
			host_fixture fixture;
			const auto   pending = core::pending_setup_destination{ .Request = core::setup_request_id{ 1U }, .Owner = core::setup_owner_id{ 1U }, .Destination = { .Hostname = "github.com", .Repository = { "other", "repo" }, .Number = core::pull_request_number{ 42U }, .Location = { } }, .Profile = core::host_profile_id{ 11U }, .Stage = core::setup_destination_stage::authenticating };
			const auto   restage = [&] {
				fixture.Host.cancel_setup_enrollment();
				auto secondCredential = application::github_credential_material::create(std::as_bytes(std::span{ "second-setup-token", std::size_t{ 18U } }));
				expect(secondCredential.has_value(), "synthetic second token");
				fixture.Host.stage_setup_enrollment(pending, std::move(*secondCredential), [](const auto&, bool) { return false; });
			};
			fixture.Host.stage_setup_enrollment(pending, credential(), [&](const auto&, bool) { if (mode == 1) { restage(); } else if (mode == 2) { fixture.Host.dispatch(core::deactivate_github_identity_intent{}); } return true; });
			const auto firstProfile = fixture.Provider.Profiles.back().Request;
			fixture.Provider.Profiles.back().Completion(application::github_profile_success{ .Request = firstProfile, .Authority = native_profile(core::host_profile_id{ 11U }) });
			fixture.Lane.drain();
			const auto firstViewer = fixture.Provider.Viewers.back();
			if (mode == 0) {
				restage();
			}
			firstViewer.Completion(application::github_viewer_success{ .Request = firstViewer.Request, .Viewer = viewer(), .RateLimits = rate_limits_for(application::github_rate_limit_resource::core), .AccountTransport = { } });
			fixture.Lane.drain();
			if (mode == 2) {
				expect(!fixture.Host.state().ActiveProfile && fixture.Vault.Stores.empty() && fixture.Provider.Profiles.size() == 1U, "reentrant authority replacement retires stage before deferred dispatch");
				continue;
			}
			expect(fixture.Provider.Profiles.size() == 2U && fixture.Provider.Profiles.back().Request.Context != firstProfile.Context,
				   "each stage attempt has distinct provider request identity");
			expect(!fixture.Host.state().ActiveProfile && fixture.Vault.Stores.empty(), "old bootstrap cannot commit replacement material, including reentrant restage");
			const auto secondProfile = fixture.Provider.Profiles.back().Request;
			fixture.Provider.Profiles.back().Completion(application::github_profile_success{ .Request = secondProfile, .Authority = native_profile(core::host_profile_id{ 11U }) });
			fixture.Lane.drain();
			expect(fixture.Provider.Viewers.size() == 2U && fixture.Provider.Viewers.back().Request.Context != firstViewer.Request.Context, "replacement continues under its own attempt");
			fixture.Host.cancel_setup_enrollment();
		}
	}

	auto test_setup_acceptance_reuses_validated_bootstrap() -> void
	{
		host_fixture fixture;
		const auto   pending = core::pending_setup_destination{ .Request = core::setup_request_id{ 1U }, .Owner = core::setup_owner_id{ 1U }, .Destination = { .Hostname = "github.com", .Repository = { "other", "repo" }, .Number = core::pull_request_number{ 42U }, .Location = { } }, .Profile = core::host_profile_id{ 11U }, .Stage = core::setup_destination_stage::authenticating };
		fixture.Host.stage_setup_enrollment(pending, credential(), [&](const auto& aPending, const bool aSuccess) { return aSuccess && aPending == pending; });
		const auto profile = fixture.Provider.Profiles.back().Request;
		fixture.Provider.Profiles.back().Completion(application::github_profile_success{ .Request = profile, .Authority = native_profile(core::host_profile_id{ 11U }) });
		fixture.Lane.drain();
		const auto request = fixture.Provider.Viewers.back().Request;
		expect(!fixture.Host.state().ActiveProfile && fixture.Vault.Stores.empty(), "staged bootstrap changes no authority");
		fixture.Provider.Viewers.back().Completion(application::github_viewer_success{ .Request = request, .Viewer = viewer(), .RateLimits = rate_limits_for(application::github_rate_limit_resource::core), .AccountTransport = { } });
		fixture.Lane.drain();
		expect(fixture.Host.state().ActiveProfile == core::host_profile_id{ 11U } && fixture.Provider.Profiles.size() == 2U, "accepted continuation activates exact authority");
		const auto committedProfile = fixture.Provider.Profiles.back().Request;
		fixture.Provider.Profiles.back().Completion(application::github_profile_success{ .Request = committedProfile, .Authority = native_profile(core::host_profile_id{ 11U }) });
		fixture.Lane.drain();
		expect(fixture.Provider.Viewers.size() == 1U && fixture.Vault.Stores.size() == 1U
				   && fixture.Vault.Stores.back().Context.Authority.Account == viewer().Account,
			   "commit uses accepted bootstrap once and stores under its exact viewer");
	}

	auto test_enrollment_acceptance_snapshot_and_exact_delete() -> void
	{
		host_fixture fixture;
		const auto   lease = establish_native_session(fixture);
		expect(fixture.Host.accepted_review_mutation_transport() == nullptr,
			   "an accepted account alone cannot expose mutation authority");

		fixture.Host.dispatch(core::open_pull_request_intent{
			.Repository = { .Owner = "openai", .Name = "better-pull-review" },
			.Number     = core::pull_request_number{ 27U },
		});
		expect(fixture.Provider.PullRequests.size() == 1U,
			   "accepted session submits one typed pull-request read");
		const auto request = fixture.Provider.PullRequests.front().Request;
		fixture.Provider.PullRequests.front().Completion(
			application::github_pull_request_success{
				.Request  = request,
				.Snapshot = snapshot(request),
			});
		expect(!fixture.Host.accepted_snapshot().has_value(),
			   "snapshot remains staged until the decision lane accepts it");
		fixture.Lane.run_next();
		expect(fixture.Host.accepted_snapshot().has_value()
				   && fixture.Projections.pull_request_count() == 1U
				   && fixture.Host.rate_limits() == rate_limits(),
			   "exact core snapshot delta commits with distinct resource limits");
		const auto retainedMutationLease
			= fixture.Host.accepted_review_mutation_transport();
		expect(retainedMutationLease != nullptr && retainedMutationLease->active()
				   && lease->MutationLeases.size() == 1U
				   && retainedMutationLease == lease->MutationLeases.front(),
			   "the exact accepted snapshot mints one opaque mutation lease");

		fixture.Host.dispatch(core::activate_host_profile_intent{
			.Profile = core::host_profile_id{ 12U },
		});
		expect(fixture.Host.accepted_review_mutation_transport() == nullptr,
			   "identity replacement revokes the accepted mutation lease immediately");
		expect(!retainedMutationLease->active(),
			   "a retained lease cannot survive profile and account invalidation");
		expect(fixture.Vault.Deletes.size() == 1U
				   && fixture.Vault.Deletes.front().Context.Authority.Account
					   == viewer().Account
				   && fixture.Provider.Profiles.size() == 1U,
			   "identity invalidation exact-deletes before resolving the next "
			   "profile");

		const auto deleteContext = fixture.Vault.Deletes.front().Context;
		fixture.Vault.Deletes.front().Completion(
			application::github_credential_delete_error{
				.Context = deleteContext,
				.Failure = application::github_credential_vault_failure::operation_failed,
			});
		fixture.Lane.run_next();
		expect(fixture.Provider.Profiles.size() == 1U
				   && fixture.Projections.host_diagnostic_count() != 0U
				   && fixture.Projections.last_host_diagnostic().Reason
					   == core::github_reason_code::vault_unavailable,
			   "delete failure is visible and blocks new session establishment");
	}

	auto test_retained_mutation_lease_is_revoked_on_shutdown() -> void
	{
		host_fixture fixture;
		establish_native_session(fixture);
		fixture.Host.dispatch(core::open_pull_request_intent{
			.Repository = { .Owner = "openai", .Name = "better-pull-review" },
			.Number     = core::pull_request_number{ 27U },
		});
		const auto request = fixture.Provider.PullRequests.front().Request;
		fixture.Provider.PullRequests.front().Completion(
			application::github_pull_request_success{
				.Request  = request,
				.Snapshot = snapshot(request),
			});
		fixture.Lane.run_next();
		const auto retained = fixture.Host.accepted_review_mutation_transport();
		expect(retained != nullptr && retained->active(),
			   "an accepted snapshot exposes an active mutation lease before shutdown");

		fixture.Host.shutdown();
		expect(!retained->active()
				   && fixture.Host.accepted_review_mutation_transport() == nullptr,
			   "shutdown revokes every retained mutation lease synchronously");
	}

	/// [[BPR-1177]]. The accepted account's publication catalog: present with
	/// no pull request snapshot, under exactly that account's identity, absent
	/// for a wire that cannot carry publication, and withdrawn with the identity.
	auto test_accepted_publication_account() -> void
	{
		host_fixture fixture;
		expect(!fixture.Host.accepted_publication_account(), "signed out there is no publication account");
		const auto lease   = establish_native_session(fixture);
		const auto account = fixture.Host.accepted_publication_account();
		expect(account && account->Provider == lease->Publication && account->Account == core::github_account_id{ 51U } && account->Login == "reviewer" && account->Host == "github.com"
				   && account->Server == application::github_server_contract::github_com_2022_11_28,
			   "the accepted account mints its own publication catalog, with no pull request snapshot");
		expect(fixture.Host.accepted_review_mutation_transport() == nullptr, "and no review-mutation authority comes with it");
		const auto kept    = lease->Publication;
		lease->Publication = nullptr;
		expect(!fixture.Host.accepted_publication_account(), "a wire that cannot carry publication gives no account");
		lease->Publication = kept;
		fixture.Host.dispatch(core::activate_host_profile_intent{ .Profile = core::host_profile_id{ 12U } });
		expect(!fixture.Host.accepted_publication_account(), "identity replacement withdraws it at once");
	}

	auto test_accepted_snapshot_handoff_and_exact_invalidation() -> void
	{
		recording_epoch_handoffs epochs;
		host_fixture             fixture{ epochs };
		const auto               accountLease = establish_native_session(fixture);

		fixture.Host.dispatch(core::open_pull_request_intent{
			.Repository = { .Owner = "openai", .Name = "better-pull-review" },
			.Number     = core::pull_request_number{ 27U },
		});
		const auto firstRequest  = fixture.Provider.PullRequests.front().Request;
		auto       firstSnapshot = snapshot(firstRequest);
		firstSnapshot.Reviews.push_back(application::github_review_record{
			.DatabaseId = 4'001U,
			.Review     = application::github_review_id{ "review-1" },
			.Author     = application::github_actor_record{
				.Actor = firstSnapshot.Viewer.Actor,
				.Login = firstSnapshot.Viewer.Login,
			},
			.State  = application::github_review_state::approved,
			.Commit = firstSnapshot.PullRequest.HeadOid,
			.Body   = "Reviewed",
		});
		fixture.Provider.PullRequests.front().Completion(
			application::github_pull_request_success{
				.Request  = firstRequest,
				.Snapshot = firstSnapshot,
			});
		expect(epochs.Accepted.empty(),
			   "provider completion cannot reach epoch ingestion off lane");
		fixture.Lane.run_next();
		expect(epochs.Accepted.size() == 1U && epochs.Invalidated.empty(),
			   "only the core-accepted snapshot reaches epoch ingestion");

		const auto& first = epochs.Accepted.front();
		expect(application::valid(first)
				   && first.ApplicationSession
					   == core::application_session_id{ 41U }
				   && first.Profile == core::host_profile_id{ 11U }
				   && first.ProfileRevision == core::github_profile_revision{ 31U }
				   && first.Host == core::git_host_id{ 21U }
				   && first.Account == core::github_account_id{ 51U }
				   && first.Viewer == core::github_viewer_actor_id{ 51U }
				   && first.Repository == core::repository_id{ 71U }
				   && first.PullRequest == core::pull_request_id{ 81U }
				   && first.Number == core::pull_request_number{ 27U }
				   && first.Locator
					   == core::repository_locator{
						   .Owner = "openai",
						   .Name  = "better-pull-review",
					   }
				   && first.BaseRef.view() == "refs/heads/main" && first.ObjectFormat == core::git_hash_algorithm::sha1 && core::canonical_hex(first.Base).view() == "1111111111111111111111111111111111111111" && core::canonical_hex(first.Head).view() == "2222222222222222222222222222222222222222" && first.HostAuthority.valid() && first.BaseBranch.valid() && first.CurrentViewerReview.has_value() && first.CurrentViewerReview->Review == core::github_review_id{ 4'001U } && first.CurrentViewerReview->Actor == first.Viewer && first.CurrentViewerReview->Commit == first.Head,
			   "handoff contains one exact provider-free authority, revision, "
			   "base ref, and current-viewer review identity");
		const auto firstHostAuthority         = first.HostAuthority;
		const auto firstBaseBranch            = first.BaseBranch;
		const auto firstPullRequestGeneration = first.PullRequestGeneration;

		fixture.Host.dispatch(core::refresh_pull_request_intent{ });
		const auto secondRequest           = fixture.Provider.PullRequests.back().Request;
		auto       secondSnapshot          = snapshot(secondRequest);
		secondSnapshot.Snapshot            = core::provider_snapshot_id{ 92U };
		secondSnapshot.PullRequest.HeadOid = application::commit_oid{
			"3333333333333333333333333333333333333333"
		};
		fixture.Provider.PullRequests.back().Completion(
			application::github_pull_request_success{
				.Request  = secondRequest,
				.Snapshot = secondSnapshot,
			});
		fixture.Lane.run_next();
		expect(epochs.Accepted.size() == 2U && epochs.Invalidated.empty(),
			   "same-authority refresh hands off the new revision without an "
			   "authority invalidation");
		const auto& second = epochs.Accepted.back();
		expect(second.HostAuthority == firstHostAuthority
				   && second.BaseBranch == firstBaseBranch
				   && second.PullRequestGeneration.value()
					   > firstPullRequestGeneration.value()
				   && second.Snapshot == core::provider_snapshot_id{ 92U }
				   && core::canonical_hex(second.Head).view()
					   == "3333333333333333333333333333333333333333",
			   "refresh preserves stable authority identities while advancing "
			   "the exact source generation and head");
		expect(second.SmartGit != nullptr,
			   "accepted native-HTTPS handoff carries one smart authority lease");
		const auto context  = smart_context(second);
		auto       acquired = second.SmartGit->acquire(context);
		expect(acquired.has_value()
				   && *acquired == accountLease->LastTransport
				   && accountLease->AcquiredContexts
					   == std::vector{ context }
				   && !(*acquired)->revoked(),
			   "the exact handoff authority acquires one bound transport");

		const auto expectedInvalidation = application::pull_request_authority_invalidation{
			.ApplicationSession = second.ApplicationSession,
			.Profile            = second.Profile,
			.ProfileRevision    = second.ProfileRevision,
			.Host               = second.Host,
			.HostAuthority      = second.HostAuthority,
			.Account            = second.Account,
			.Viewer             = second.Viewer,
			.Repository         = second.Repository,
			.PullRequest        = second.PullRequest,
			.IdentityGeneration = second.IdentityGeneration,
		};
		epochs.ThrowInvalidation = true;
		fixture.Host.dispatch(core::open_pull_request_intent{
			.Repository = { .Owner = "openai", .Name = "another-repository" },
			.Number     = core::pull_request_number{ 28U },
		});
		expect(epochs.Invalidated
					   == std::vector{ expectedInvalidation }
				   && fixture.Host.epoch_handoff_delivery_failed()
				   && second.SmartGit->revoked()
				   && (*acquired)->revoked(),
			   "opening an unrelated pull request invalidates only the exact "
			   "previous epoch authority, revokes its issued transport before "
			   "new provider work, and contains a failing observer");
	}

	auto test_success_projection_is_safe_atomic_and_conversation_complete() -> void
	{
		static_assert(
			!exposes_requests<application::github_transition_projection>);
		static_assert(
			!exposes_accepted_session<application::github_transition_projection>);
		static_assert(
			!exposes_accepted_snapshot<application::github_transition_projection>);
		static_assert(!exposes_viewer<application::github_session_projection>);
		static_assert(
			!exposes_capability_evidence<
				application::github_session_projection>);
		static_assert(
			!exposes_account_transport<application::github_session_projection>);
		static_assert(!exposes_credential<application::github_session_projection>);
		static_assert(
			std::is_same_v<
				decltype(application::github_session_projection::RateLimits),
				application::github_rate_limits_projection>);

		host_fixture fixture;
		establish_native_session(fixture);

		const auto& sessionUpdate   = fixture.Projections.last_session_update();
		const auto& acceptedSession = *fixture.Host.accepted_session();
		expect(
			sessionUpdate.Transition.has_value()
				&& sessionUpdate.Session.has_value()
				&& !sessionUpdate.PullRequest.has_value()
				&& sessionUpdate.HostDiagnostics.empty(),
			"accepted session publishes one atomic transition and safe session");
		expect(
			sessionUpdate.Transition->Status.has_value()
				&& sessionUpdate.Transition->Status->Status
					== core::github_status::session_ready,
			"accepted session transition reports ready status");
		expect(
			sessionUpdate.Session->Profile == core::host_profile_id{ 11U }
				&& sessionUpdate.Session->Host == core::git_host_id{ 21U }
				&& sessionUpdate.Session->Account
					== core::github_account_id{ 51U }
				&& sessionUpdate.Session->Login == "reviewer"
				&& sessionUpdate.Session->DisplayName == "Review Engineer"
				&& sessionUpdate.Session->Capabilities
					== acceptedSession.CapabilityEvidence.Capabilities
				&& sessionUpdate.Session->CapabilityRevision
					== acceptedSession.CapabilityEvidence.Revision
				&& sessionUpdate.Session->RateLimits
					== projected_rate_limits(acceptedSession.RateLimits),
			"session projection contains only accepted identity, login, "
			"capability, and rate facts");

		fixture.Host.dispatch(core::open_pull_request_intent{
			.Repository = { .Owner = "openai", .Name = "better-pull-review" },
			.Number     = core::pull_request_number{ 27U },
		});
		const auto request          = fixture.Provider.PullRequests.front().Request;
		const auto acceptedSnapshot = rich_snapshot(request);
		fixture.Provider.PullRequests.front().Completion(
			application::github_pull_request_success{
				.Request  = request,
				.Snapshot = acceptedSnapshot,
			});
		fixture.Lane.run_next();

		const auto& pullRequestUpdate = fixture.Projections.last_pull_request_update();
		expect(
			pullRequestUpdate.Transition.has_value()
				&& !pullRequestUpdate.Session.has_value()
				&& pullRequestUpdate.PullRequest.has_value()
				&& pullRequestUpdate.HostDiagnostics.empty(),
			"accepted pull request publishes one atomic transition and projection");
		expect(
			pullRequestUpdate.Transition->Status.has_value()
				&& pullRequestUpdate.Transition->Status->Status
					== core::github_status::pull_request_ready,
			"accepted pull-request transition reports ready status");

		const auto& pullRequest = *pullRequestUpdate.PullRequest;
		expect(
			pullRequest.Profile == acceptedSnapshot.Profile
				&& pullRequest.Host == acceptedSnapshot.Host
				&& pullRequest.Account == acceptedSnapshot.Account
				&& pullRequest.Snapshot == acceptedSnapshot.Snapshot
				&& pullRequest.Repository
					== acceptedSnapshot.Repository.Repository
				&& pullRequest.PullRequest
					== acceptedSnapshot.PullRequest.PullRequest
				&& pullRequest.AuthorLogin == "octo-author"
				&& pullRequest.FileCount == 1U
				&& pullRequest.ReviewCount == 1U
				&& pullRequest.ReviewThreadCount == 1U
				&& pullRequest.IssueCommentCount == 1U
				&& pullRequest.ReviewThreadsAvailable
				&& pullRequest.Capabilities
					== acceptedSnapshot.CapabilityEvidence.Capabilities
				&& pullRequest.CapabilityRevision
					== acceptedSnapshot.CapabilityEvidence.Revision
				&& pullRequest.RateLimits
					== projected_rate_limits(acceptedSnapshot.RateLimits),
			"pull-request projection retains presentation-safe snapshot facts");
		expect(
			pullRequest.Conversations.size() == 6U
				&& pullRequest.Conversations[0U].Id == "review:review-1"
				&& pullRequest.Conversations[1U].Id
					== "review-thread:thread-1"
				&& pullRequest.Conversations[2U].Id
					== "review-comment:review-comment-1"
				&& pullRequest.Conversations[3U].Id
					== "review-comment:review-comment-2"
				&& pullRequest.Conversations[4U].Id
					== "review-comment:review-comment-3"
				&& pullRequest.Conversations[5U].Id
					== "issue-comment:issue-comment-1",
			"review conversation identities are stable, prefixed, and ordered");

		const auto& review       = pullRequest.Conversations[0U];
		const auto& thread       = pullRequest.Conversations[1U];
		const auto& threadRoot   = pullRequest.Conversations[2U];
		const auto& threadReply  = pullRequest.Conversations[3U];
		const auto& deepReply    = pullRequest.Conversations[4U];
		const auto& issueComment = pullRequest.Conversations[5U];
		expect(
			review.Kind == application::github_conversation_kind::review
				&& !review.ParentId.has_value()
				&& review.AuthorLogin == "reviewer"
				&& review.Body == "Please preserve identity."
				&& review.ReviewState
					== application::github_review_state_projection::
						changes_requested,
			"review projects its author, body, and review state");
		expect(
			thread.Kind == application::github_conversation_kind::review_thread
				&& !thread.ParentId.has_value()
				&& thread.Path == "src/application/host.cpp"
				&& thread.Line == 42U && thread.OriginalLine == 37U
				&& thread.DiffSide
					== application::github_conversation_diff_side::right
				&& thread.AnchorAvailability
					== application::github_conversation_anchor_availability::current
				&& thread.Resolved && !thread.Outdated,
			"review thread projects its location and state");
		expect(thread.StartLine == 40U && threadRoot.StartLine == 40U && !issueComment.StartLine.has_value() && !review.StartLine.has_value(),
			   "a thread on several lines projects its first line on the thread and its comments");
		expect(
			threadRoot.Kind
					== application::github_conversation_kind::review_comment
				&& threadRoot.ParentId == thread.Id
				&& threadRoot.AuthorLogin == "reviewer"
				&& threadRoot.Body == "Thread root"
				&& threadRoot.Path == thread.Path && threadRoot.Line == 42U
				&& threadRoot.OriginalLine == thread.OriginalLine
				&& threadRoot.DiffSide == thread.DiffSide
				&& threadRoot.AnchorAvailability == thread.AnchorAvailability
				&& threadRoot.Resolved && !threadRoot.Outdated
				&& !threadRoot.Deleted,
			"root review comment projects under its stable thread identity");
		expect(
			threadReply.Kind
					== application::github_conversation_kind::review_comment
				&& threadReply.ParentId == threadRoot.Id
				&& threadReply.AuthorLogin == "octo-author"
				&& threadReply.Body == "Follow-up"
				&& threadReply.Resolved && threadReply.Outdated
				&& threadReply.Deleted,
			"reply review comment nests under the comment it replied to and "
			"retains thread state and comment flags");
		expect(
			deepReply.ParentId == threadReply.Id,
			"a reply to a reply nests three levels deep rather than flattening "
			"against the thread");
		expect(
			issueComment.Kind
					== application::github_conversation_kind::issue_comment
				&& !issueComment.ParentId.has_value()
				&& issueComment.AuthorLogin == "octo-author"
				&& issueComment.Body == "Issue follow-up"
				&& !issueComment.Deleted,
			"issue comment projects as a distinct top-level conversation");
	}

	auto test_unanchored_line_thread_projects_without_a_guessed_line() -> void
	{
		for (const bool outdated : { false, true }) {
			host_fixture fixture;
			establish_native_session(fixture);
			fixture.Host.dispatch(core::open_pull_request_intent{
				.Repository = { .Owner = "openai", .Name = "better-pull-review" },
				.Number     = core::pull_request_number{ 27U },
			});
			const auto request  = fixture.Provider.PullRequests.front().Request;
			auto       snapshot = rich_snapshot(request);
			auto&      thread   = snapshot.ReviewThreads.front();
			thread.Line.reset();
			thread.OriginalLine.reset();
			thread.Outdated = outdated;
			fixture.Provider.PullRequests.front().Completion(
				application::github_pull_request_success{
					.Request  = request,
					.Snapshot = snapshot,
				});
			fixture.Lane.run_next();
			const auto& update = fixture.Projections.last_pull_request_update();
			expect(update.PullRequest.has_value()
					   && update.PullRequest->Conversations.size() >= 3U,
				   "an unanchored LINE thread reaches the composed host projection");
			const auto& projectedThread = update.PullRequest->Conversations[1U];
			const auto& projectedRoot   = update.PullRequest->Conversations[2U];
			expect(projectedThread.Id == "review-thread:thread-1"
					   && projectedThread.Path == thread.Path
					   && !projectedThread.Line && !projectedThread.OriginalLine
					   && projectedThread.DiffSide
						   == application::github_conversation_diff_side::right
					   && projectedThread.AnchorAvailability
						   == (outdated
								   ? application::github_conversation_anchor_availability::outdated_revision
								   : application::github_conversation_anchor_availability::missing_current_line)
					   && projectedRoot.ParentId == projectedThread.Id
					   && projectedRoot.Id == "review-comment:review-comment-1"
					   && !projectedRoot.Line && !projectedRoot.OriginalLine,
				   "the thread and comments retain identity but do not borrow a comment coordinate as a source-line anchor");
		}
	}

	auto test_every_provider_failure_maps_to_safe_pull_request_failure() -> void
	{
		using provider_failure = application::github_provider_failure;
		using reason           = core::github_reason_code;

		struct failure_mapping
		{
			provider_failure Provider;
			reason           CoreReason;
			bool             Retryable;
		};

		constexpr auto mappings = std::array{
			failure_mapping{
				provider_failure::authentication,
				reason::authentication_failed,
				false,
			},
			failure_mapping{
				provider_failure::permission,
				reason::permission_denied,
				false,
			},
			failure_mapping{
				provider_failure::network,
				reason::network_unavailable,
				true,
			},
			failure_mapping{
				provider_failure::api_version,
				reason::api_version_unsupported,
				false,
			},
			failure_mapping{
				provider_failure::unsupported_capability,
				reason::capability_unavailable,
				false,
			},
			failure_mapping{
				provider_failure::rate_limit,
				reason::rate_limited,
				false,
			},
			failure_mapping{
				provider_failure::certificate,
				reason::certificate_rejected,
				false,
			},
			failure_mapping{
				provider_failure::redirect,
				reason::redirect_rejected,
				false,
			},
			failure_mapping{
				provider_failure::timeout,
				reason::timed_out,
				true,
			},
			failure_mapping{
				provider_failure::process_startup,
				reason::network_unavailable,
				false,
			},
			failure_mapping{
				provider_failure::cli_unavailable,
				reason::cli_unavailable,
				false,
			},
			failure_mapping{
				provider_failure::vault,
				reason::vault_unavailable,
				false,
			},
			failure_mapping{
				provider_failure::malformed_response,
				reason::malformed_response,
				false,
			},
			failure_mapping{
				provider_failure::response_limit,
				reason::response_too_large,
				false,
			},
			failure_mapping{
				provider_failure::unavailable,
				reason::network_unavailable,
				true,
			},
		};
		static_assert(
			mappings.size()
			== static_cast<std::size_t>(provider_failure::unavailable) + 1U);
		static_assert(!exposes_detail<application::github_provider_error>);
		static_assert(!exposes_detail<application::github_pull_request_error>);
		static_assert(
			!exposes_detail<application::github_host_diagnostic_projection>);
		static_assert(!exposes_detail<core::github_diagnostic>);

		for (const auto& mapping : mappings) {
			host_fixture fixture;
			establish_native_session(fixture);
			fixture.Host.dispatch(core::open_pull_request_intent{
				.Repository = { .Owner = "openai", .Name = "better-pull-review" },
				.Number     = core::pull_request_number{ 27U },
			});

			const auto attempts = mapping.Retryable
				? application::maximum_github_provider_attempts
				: std::uint8_t{ 1U };
			for (std::uint8_t attempt = 0U; attempt < attempts; ++attempt) {
				const auto                         request = fixture.Provider.PullRequests.at(attempt).Request;
				application::github_provider_error error{
					.Failure           = mapping.Provider,
					.RetryAfterSeconds = std::nullopt,
					.RateLimits        = { },
					.RateLimitCause    = std::nullopt,
				};
				if (mapping.Provider == provider_failure::rate_limit) {
					auto exhausted       = rate_limit();
					exhausted.Remaining  = 0U;
					error.RateLimits     = rate_limits_for(exhausted);
					error.RateLimitCause = exhausted;
				}
				expect(application::valid(error),
					   "provider failure fixture is structurally valid");
				fixture.Provider.PullRequests.at(attempt).Completion(
					application::github_pull_request_error{
						.Request = request,
						.Error   = error,
					});
				fixture.Lane.run_next();
			}

			const auto& update              = fixture.Projections.last_transition_update();
			const auto  expectedDiagnostics = std::vector{ core::github_diagnostic{ .Code = mapping.CoreReason } };
			expect(
				fixture.Provider.PullRequests.size() == attempts
					&& !fixture.Host.accepted_snapshot().has_value()
					&& !fixture.Host.state().AcceptedSnapshot.has_value()
					&& update.Transition->Status
						== core::github_status_delta{
							.Status = core::github_status::pull_request_failed }
					&& update.Transition->Diagnostics == expectedDiagnostics && !update.Session.has_value() && !update.PullRequest.has_value() && update.HostDiagnostics.empty(),
				"every provider failure maps to one safe typed pull-request "
				"failure without an accepted snapshot");
		}
	}

	auto test_retries_are_bounded_and_stale_work_is_cancelled() -> void
	{
		host_fixture fixture;
		fixture.Host.dispatch(core::activate_host_profile_intent{
			.Profile = core::host_profile_id{ 11U },
		});
		const auto oldRequest = fixture.Provider.Profiles.front().Request;
		const auto replay     = fixture.Host.pending_session_presentation();
		expect(replay && replay->Transition && replay->Transition->Status == core::github_status_delta{ .Status = core::github_status::establishing_session }
				   && replay->Transition->Identity->Profile == core::host_profile_id{ 11U } && !replay->Session && !replay->PullRequest,
			   "late-attached windows can replay validation without an accepted account or private content");
		const auto beforeReentry = fixture.Host.state();
		fixture.Host.dispatch(core::activate_host_profile_intent{ .Profile = core::host_profile_id{ 11U } });
		expect(fixture.Host.state() == beforeReentry && fixture.Provider.Profiles.size() == 1U && fixture.Provider.CancelledProfiles.empty(),
			   "duplicate connection ingress keeps the pending authority and starts no effect");
		fixture.Host.dispatch(core::activate_host_profile_intent{
			.Profile = core::host_profile_id{ 12U },
		});
		expect(fixture.Provider.CancelledProfiles == std::vector{ oldRequest }
				   && fixture.Provider.Profiles.size() == 2U,
			   "profile switch cancels exact in-flight work");

		fixture.Provider.Profiles.front().Completion(
			application::github_profile_success{
				.Request   = oldRequest,
				.Authority = cli_profile(oldRequest.Context.Profile),
			});
		fixture.Lane.run_next();
		expect(fixture.Provider.Viewers.empty(),
			   "reordered stale profile success cannot start viewer work");

		const auto currentRequest = fixture.Provider.Profiles.back().Request;
		fixture.Provider.Profiles.back().Completion(
			application::github_profile_success{
				.Request   = currentRequest,
				.Authority = cli_profile(currentRequest.Context.Profile),
			});
		fixture.Lane.run_next();
		expect(fixture.Provider.Viewers.size() == 1U,
			   "current profile success starts the viewer phase");

		const auto viewerRequest = fixture.Provider.Viewers.front().Request;
		fixture.Provider.Viewers.front().Completion(
			application::github_viewer_success{
				.Request          = viewerRequest,
				.Viewer           = viewer(),
				.RateLimits       = rate_limits_for(application::github_rate_limit_resource::core),
				.AccountTransport = std::make_shared<fake_lease>(),
			});
		fixture.Lane.run_next();
		expect(fixture.Provider.Capabilities.size() == 1U,
			   "CLI viewer starts capability probing");

		for (std::uint8_t attempt = 1U; attempt <= 3U; ++attempt) {
			auto& call = fixture.Provider
							 .Capabilities[static_cast<std::size_t>(attempt - 1U)];
			expect(call.Request.Attempt
					   == application::github_provider_attempt{ attempt },
				   "retry attempt identity is monotone and exact");
			call.Completion(application::github_capability_error{
				.Request = call.Request,
				.Error   = {
					.Failure           = attempt == 1U
						? application::github_provider_failure::unavailable
						: attempt == 2U
						? application::github_provider_failure::timeout
						: application::github_provider_failure::network,
					.RetryAfterSeconds = std::nullopt,
					.RateLimits        = { },
					.RateLimitCause    = std::nullopt,
				},
			});
			expect(fixture.Provider.Capabilities.size()
					   == static_cast<std::size_t>(attempt),
				   "retry is never submitted from the worker callback");
			fixture.Lane.run_next();
		}
		expect(
			fixture.Provider.Capabilities.size() == 3U
				&& !fixture.Host.accepted_session().has_value()
				&& fixture.Host.state().PendingSession == std::nullopt,
			"three total immediate serialized attempts exhaust to a core failure");
	}

	auto test_retry_after_is_terminal_and_preserved() -> void
	{
		{
			host_fixture fixture;
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			const auto request = fixture.Provider.Profiles.front().Request;
			fixture.Provider.Profiles.front().Completion(
				application::github_profile_error{
					.Request = request,
					.Error   = {
						.Failure           = application::github_provider_failure::unavailable,
						.RetryAfterSeconds = 30U,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					},
				});
			fixture.Lane.run_next();
			expect(fixture.Provider.Profiles.size() == 1U
					   && fixture.Projections.host_diagnostic_count() == 1U
					   && fixture.Projections.last_host_diagnostic().Reason
						   == core::github_reason_code::network_unavailable
					   && fixture.Projections.last_host_diagnostic().RetryAfterSeconds
						   == 30U,
				   "positive Retry-After prevents an immediate retry and is "
				   "preserved");
		}

		{
			host_fixture fixture;
			establish_native_session(fixture);
			fixture.Host.dispatch(core::open_pull_request_intent{
				.Repository = { .Owner = "openai", .Name = "better-pull-review" },
				.Number     = core::pull_request_number{ 27U },
			});
			const auto                                  request = fixture.Provider.PullRequests.front().Request;
			const application::github_rate_limit_record exhausted{
				.Resource         = application::github_rate_limit_resource::core,
				.Limit            = 5'000U,
				.Remaining        = 0U,
				.ResetUnixSeconds = 2'000'000'100U,
				.SecondaryLimited = false,
			};
			fixture.Provider.PullRequests.front().Completion(
				application::github_pull_request_error{
					.Request = request,
					.Error   = {
						.Failure           = application::github_provider_failure::rate_limit,
						.RetryAfterSeconds = application::maximum_github_retry_after_seconds,
						.RateLimits        = rate_limits_for(exhausted),
						.RateLimitCause    = exhausted,
					},
				});
			fixture.Lane.run_next();
			expect(
				fixture.Provider.PullRequests.size() == 1U
					&& fixture.Projections.host_diagnostic_count() == 1U
					&& fixture.Projections.last_host_diagnostic().Reason
						== core::github_reason_code::rate_limited
					&& fixture.Projections.last_host_diagnostic().RetryAfterSeconds
						== application::maximum_github_retry_after_seconds
					&& fixture.Projections.last_host_diagnostic().RateLimits.Core
						== projected_rate_limits(fixture.Host.rate_limits()).Core
					&& fixture.Host.rate_limits().Core == exhausted,
				"bounded rate-limit Retry-After reaches the projection advisory");
		}

		{
			host_fixture fixture;
			establish_native_session(fixture);
			fixture.Host.dispatch(core::open_pull_request_intent{
				.Repository = { .Owner = "openai", .Name = "better-pull-review" },
				.Number     = core::pull_request_number{ 27U },
			});
			const auto request   = fixture.Provider.PullRequests.front().Request;
			auto       exhausted = rate_limit();
			exhausted.Remaining  = 0U;
			fixture.Provider.PullRequests.front().Completion(
				application::github_pull_request_error{
					.Request = request,
					.Error   = {
						.Failure           = application::github_provider_failure::unavailable,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = rate_limits_for(exhausted),
						.RateLimitCause    = std::nullopt,
					},
				});
			fixture.Lane.run_next();
			expect(fixture.Provider.PullRequests.size() == 1U
					   && fixture.Host.rate_limits().Core == exhausted,
				   "primary exhaustion suppresses immediate unavailable retries");
		}

		{
			host_fixture fixture;
			establish_native_session(fixture);
			fixture.Host.dispatch(core::open_pull_request_intent{
				.Repository = { .Owner = "openai", .Name = "better-pull-review" },
				.Number     = core::pull_request_number{ 27U },
			});
			const auto                                  request = fixture.Provider.PullRequests.front().Request;
			const application::github_rate_limit_record secondary{
				.Resource         = application::github_rate_limit_resource::core,
				.Limit            = 5'000U,
				.Remaining        = 4'999U,
				.ResetUnixSeconds = 2'000'000'200U,
				.SecondaryLimited = true,
			};
			fixture.Provider.PullRequests.front().Completion(
				application::github_pull_request_error{
					.Request = request,
					.Error   = {
						.Failure           = application::github_provider_failure::rate_limit,
						.RetryAfterSeconds = 60U,
						.RateLimits        = rate_limits_for(secondary),
						.RateLimitCause    = secondary,
					},
				});
			fixture.Lane.run_next();
			expect(fixture.Host.rate_limits().Core == secondary
					   && fixture.Projections.last_host_diagnostic().RateLimits.Core
						   == projected_rate_limit(secondary)
					   && fixture.Projections.last_host_diagnostic().RetryAfterSeconds
						   == 60U,
				   "secondary-limit state and fallback delay remain distinct");
		}

		{
			host_fixture fixture;
			establish_native_session(fixture);
			fixture.Host.dispatch(core::open_pull_request_intent{
				.Repository = { .Owner = "openai", .Name = "better-pull-review" },
				.Number     = core::pull_request_number{ 27U },
			});
			auto search      = rate_limit(application::github_rate_limit_resource::search);
			search.Remaining = 17U;
			const auto first = fixture.Provider.PullRequests.front().Request;
			fixture.Provider.PullRequests.front().Completion(
				application::github_pull_request_error{
					.Request = first,
					.Error   = {
						.Failure           = application::github_provider_failure::unavailable,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = rate_limits_for(search),
						.RateLimitCause    = std::nullopt,
					},
				});
			fixture.Lane.run_next();

			auto core         = rate_limit(application::github_rate_limit_resource::core);
			core.Remaining    = 4'123U;
			const auto second = fixture.Provider.PullRequests.back().Request;
			fixture.Provider.PullRequests.back().Completion(
				application::github_pull_request_error{
					.Request = second,
					.Error   = {
						.Failure           = application::github_provider_failure::unavailable,
						.RetryAfterSeconds = 30U,
						.RateLimits        = rate_limits_for(core),
						.RateLimitCause    = std::nullopt,
					},
				});
			fixture.Lane.run_next();
			expect(
				fixture.Provider.PullRequests.size() == 2U
					&& fixture.Projections.host_diagnostic_count() == 1U
					&& fixture.Projections.last_host_diagnostic().RateLimits
						== projected_rate_limits(fixture.Host.rate_limits())
					&& fixture.Host.rate_limits().Core == core
					&& fixture.Host.rate_limits().Search == search
					&& fixture.Host.rate_limits().Graphql.has_value(),
				"terminal advisory projects all resource observations accumulated "
				"across retries");
		}
	}

	auto test_wrong_correlations_and_submit_exceptions_fail_closed() -> void
	{
		{
			host_fixture fixture;
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			const auto current    = fixture.Provider.Profiles.front().Request;
			auto       wrong      = current;
			wrong.Context.Request = core::request_id{ 999U };
			fixture.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = wrong,
					.Authority = cli_profile(current.Context.Profile),
				});
			fixture.Lane.run_next();
			expect(!fixture.Host.state().PendingSession.has_value()
					   && fixture.Provider.Viewers.empty(),
				   "wrong-correlated current provider terminal fails closed");
		}

		{
			host_fixture fixture;
			fixture.Provider.ThrowOnProfileSubmit = true;
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			expect(fixture.Lane.pending() == 1U,
				   "synchronous provider throw is converted to a lane completion");
			fixture.Lane.run_next();
			expect(!fixture.Host.state().PendingSession.has_value(),
				   "provider submit throw terminates the current session");
		}

		{
			host_fixture fixture;
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			const auto request = fixture.Provider.Profiles.front().Request;
			fixture.Provider.Profiles.front().Completion(
				application::github_profile_error{
					.Request = request,
					.Error   = {
						.Failure           = application::github_provider_failure::network,
						.RetryAfterSeconds = 1U,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					},
				});
			fixture.Lane.run_next();
			expect(
				fixture.Provider.Profiles.size() == 1U
					&& !fixture.Host.state().PendingSession.has_value()
					&& fixture.Projections.host_diagnostic_count() == 0U,
				"invalid retry metadata is terminal without an advisory or retry");
		}

		{
			host_fixture fixture;
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			const auto request = fixture.Provider.Profiles.front().Request;
			fixture.Provider.Profiles.front().Completion(
				application::github_profile_error{
					.Request = request,
					.Error   = {
						.Failure           = application::github_provider_failure::permission,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = rate_limits_for(
							application::github_rate_limit_record{
								.Resource = application::
									github_rate_limit_resource::core,
								.Limit            = 5'000U,
								.Remaining        = 4'999U,
								.ResetUnixSeconds = 2'000'000'000U,
								.SecondaryLimited = false,
							}),
						.RateLimitCause = std::nullopt,
					},
				});
			fixture.Lane.run_next();
			expect(!fixture.Host.rate_limits().Core.has_value()
					   && !fixture.Host.rate_limits().Search.has_value()
					   && !fixture.Host.rate_limits().Graphql.has_value()
					   && fixture.Projections.host_diagnostic_count() == 0U
					   && !fixture.Host.state().PendingSession.has_value(),
				   "local profile resolution cannot fabricate remote rate state");
		}

		{
			fake_provider                                  provider;
			fake_vault                                     vault;
			throwing_once_lane                             lane;
			recording_projections                          projections;
			application::github_credential_binding_catalog bindings;
			application::github_application_host           host{
				initial_state(),
				provider,
				provider,
				provider,
				provider,
				vault,
				bindings,
				lane,
				projections,
			};
			host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			const auto request = provider.Profiles.front().Request;
			provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = request,
					.Authority = cli_profile(request.Context.Profile),
				});
			expect(lane.pending() == 1U,
				   "a throwing lane post falls back to one typed completion");
			lane.run_next();
			expect(!host.state().PendingSession.has_value()
					   && provider.Viewers.empty(),
				   "lane post failure cannot strand provider work");
		}

		{
			host_fixture fixture;
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			const auto profile = fixture.Provider.Profiles.front().Request;
			fixture.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = profile,
					.Authority = cli_profile(profile.Context.Profile),
				});
			fixture.Lane.run_next();
			const auto request = fixture.Provider.Viewers.front().Request;
			fixture.Provider.Viewers.front().Completion(
				application::github_viewer_error{
					.Request = request,
					.Error   = {
						.Failure           = application::github_provider_failure::network,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = rate_limits_for(
							application::github_rate_limit_resource::search),
						.RateLimitCause = std::nullopt,
					},
				});
			fixture.Lane.run_next();
			expect(fixture.Provider.Viewers.size() == 1U
					   && !fixture.Host.rate_limits().Search.has_value()
					   && !fixture.Host.state().PendingSession.has_value(),
				   "viewer errors cannot inject a search rate observation");
		}

		{
			host_fixture fixture;
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			const auto profile = fixture.Provider.Profiles.front().Request;
			fixture.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = profile,
					.Authority = cli_profile(profile.Context.Profile),
				});
			fixture.Lane.run_next();
			const auto viewerRequest = fixture.Provider.Viewers.front().Request;
			fixture.Provider.Viewers.front().Completion(
				application::github_viewer_success{
					.Request    = viewerRequest,
					.Viewer     = viewer(),
					.RateLimits = rate_limits_for(
						application::github_rate_limit_resource::core),
					.AccountTransport = std::make_shared<fake_lease>(),
				});
			fixture.Lane.run_next();
			const auto capability = fixture.Provider.Capabilities.front().Request;
			fixture.Provider.Capabilities.front().Completion(
				application::github_capability_error{
					.Request = capability,
					.Error   = {
						.Failure           = application::github_provider_failure::permission,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = rate_limits_for(
							application::github_rate_limit_resource::search),
						.RateLimitCause = std::nullopt,
					},
				});
			fixture.Lane.run_next();
			expect(fixture.Provider.Capabilities.size() == 1U
					   && !fixture.Host.rate_limits().Search.has_value()
					   && !fixture.Host.state().PendingSession.has_value(),
				   "capability errors cannot inject a search rate observation");
		}
	}

	auto test_rate_observation_presence_follows_server_contract() -> void
	{
		{
			host_fixture fixture;
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			const auto profile = fixture.Provider.Profiles.front().Request;
			fixture.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = profile,
					.Authority = cli_profile(profile.Context.Profile),
				});
			fixture.Lane.run_next();
			const auto viewerRequest = fixture.Provider.Viewers.front().Request;
			fixture.Provider.Viewers.front().Completion(
				application::github_viewer_success{
					.Request          = viewerRequest,
					.Viewer           = viewer(),
					.RateLimits       = { },
					.AccountTransport = std::make_shared<fake_lease>(),
				});
			fixture.Lane.run_next();
			expect(fixture.Provider.Capabilities.empty()
					   && !fixture.Host.state().PendingSession.has_value(),
				   "GitHub.com viewer success requires a core rate observation");
		}

		{
			host_fixture fixture;
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			const auto profile = fixture.Provider.Profiles.front().Request;
			fixture.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = profile,
					.Authority = cli_profile(profile.Context.Profile),
				});
			fixture.Lane.run_next();
			const auto viewerRequest = fixture.Provider.Viewers.front().Request;
			fixture.Provider.Viewers.front().Completion(
				application::github_viewer_success{
					.Request    = viewerRequest,
					.Viewer     = viewer(),
					.RateLimits = rate_limits_for(
						application::github_rate_limit_resource::core),
					.AccountTransport = std::make_shared<fake_lease>(),
				});
			fixture.Lane.run_next();
			const auto capability = fixture.Provider.Capabilities.front().Request;
			fixture.Provider.Capabilities.front().Completion(
				application::github_capability_success{
					.Request     = capability,
					.Facts       = capability_facts(),
					.SchemaFacts = schema_facts(),
					.RateLimits  = { },
				});
			fixture.Lane.run_next();
			expect(!fixture.Host.accepted_session().has_value(),
				   "GitHub.com capability success requires a GraphQL rate "
				   "observation");
		}

		{
			host_fixture fixture;
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			const auto profile = fixture.Provider.Profiles.front().Request;
			fixture.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = profile,
					.Authority = ghes_cli_profile(profile.Context.Profile),
				});
			fixture.Lane.run_next();
			const auto viewerRequest = fixture.Provider.Viewers.front().Request;
			fixture.Provider.Viewers.front().Completion(
				application::github_viewer_success{
					.Request          = viewerRequest,
					.Viewer           = viewer(),
					.RateLimits       = { },
					.AccountTransport = std::make_shared<fake_lease>(),
				});
			fixture.Lane.run_next();
			const auto capability = fixture.Provider.Capabilities.front().Request;
			fixture.Provider.Capabilities.front().Completion(
				application::github_capability_success{
					.Request     = capability,
					.Facts       = capability_facts(),
					.SchemaFacts = schema_facts(),
					.RateLimits  = { },
				});
			fixture.Lane.run_next();
			expect(
				fixture.Host.accepted_session().has_value()
					&& !application::has_observations(fixture.Host.rate_limits()),
				"GHES with disabled rate limits preserves explicit no-observation "
				"state");

			fixture.Host.dispatch(core::open_pull_request_intent{
				.Repository = { .Owner = "openai", .Name = "better-pull-review" },
				.Number     = core::pull_request_number{ 27U },
			});
			const auto request = fixture.Provider.PullRequests.front().Request;
			auto       result  = snapshot(request);
			result.RateLimits  = { };
			fixture.Provider.PullRequests.front().Completion(
				application::github_pull_request_success{
					.Request  = request,
					.Snapshot = std::move(result),
				});
			fixture.Lane.run_next();
			expect(fixture.Host.accepted_snapshot().has_value()
					   && !application::has_observations(
						   fixture.Host.accepted_snapshot()->RateLimits),
				   "GHES disabled-limit snapshot remains explicitly unobserved");

			fixture.Host.dispatch(core::open_pull_request_intent{
				.Repository = { .Owner = "openai", .Name = "better-pull-review" },
				.Number     = core::pull_request_number{ 28U },
			});
			const auto partialRequest = fixture.Provider.PullRequests.back().Request;
			auto       partial        = snapshot(partialRequest);
			partial.RateLimits        = rate_limits_for(application::github_rate_limit_resource::search);
			fixture.Provider.PullRequests.back().Completion(
				application::github_pull_request_success{
					.Request  = partialRequest,
					.Snapshot = std::move(partial),
				});
			fixture.Lane.run_next();
			expect(fixture.Host.accepted_snapshot().has_value()
					   && fixture.Host.accepted_snapshot()
							  ->RateLimits.Search.has_value()
					   && !fixture.Host.accepted_snapshot()
							   ->RateLimits.Core.has_value()
					   && !fixture.Host.accepted_snapshot()
							   ->RateLimits.Graphql.has_value(),
				   "GHES accepts the valid subset observed when rate limiting is "
				   "mixed");
		}

		{
			host_fixture fixture;
			establish_native_session(fixture);
			fixture.Host.dispatch(core::open_pull_request_intent{
				.Repository = { .Owner = "openai", .Name = "better-pull-review" },
				.Number     = core::pull_request_number{ 27U },
			});
			const auto request = fixture.Provider.PullRequests.front().Request;
			auto       result  = snapshot(request);
			result.RateLimits.Search.reset();
			fixture.Provider.PullRequests.front().Completion(
				application::github_pull_request_success{
					.Request  = request,
					.Snapshot = std::move(result),
				});
			fixture.Lane.run_next();
			expect(!fixture.Host.accepted_snapshot().has_value(),
				   "GitHub.com snapshots require every exercised resource bucket");
		}
	}

	auto test_no_graphql_read_rejects_graphql_error_observations() -> void
	{
		host_fixture fixture;
		fixture.Host.dispatch(core::activate_host_profile_intent{
			.Profile = core::host_profile_id{ 11U },
		});
		const auto profile = fixture.Provider.Profiles.front().Request;
		fixture.Provider.Profiles.front().Completion(
			application::github_profile_success{
				.Request   = profile,
				.Authority = cli_profile(profile.Context.Profile),
			});
		fixture.Lane.run_next();
		const auto viewerRequest = fixture.Provider.Viewers.front().Request;
		fixture.Provider.Viewers.front().Completion(
			application::github_viewer_success{
				.Request          = viewerRequest,
				.Viewer           = viewer(),
				.RateLimits       = rate_limits_for(application::github_rate_limit_resource::core),
				.AccountTransport = std::make_shared<fake_lease>(),
			});
		fixture.Lane.run_next();
		const auto capability = fixture.Provider.Capabilities.front().Request;
		fixture.Provider.Capabilities.front().Completion(
			application::github_capability_success{
				.Request     = capability,
				.Facts       = unsupported_capability_facts(),
				.SchemaFacts = unsupported_schema_facts(),
				.RateLimits  = rate_limits_for(
					application::github_rate_limit_resource::graphql),
			});
		fixture.Lane.run_next();
		expect(fixture.Host.accepted_session().has_value()
				   && !fixture.Host.accepted_session()
						   ->CapabilityEvidence.ReadShape.ReviewThreads
				   && !fixture.Host.accepted_session()
						   ->CapabilityEvidence.ReadShape.ViewedFileState,
			   "unsupported schema establishes a REST-only read shape");

		fixture.Host.dispatch(core::open_pull_request_intent{
			.Repository = { .Owner = "openai", .Name = "better-pull-review" },
			.Number     = core::pull_request_number{ 27U },
		});
		const auto request      = fixture.Provider.PullRequests.front().Request;
		const auto priorGraphql = fixture.Host.rate_limits().Graphql;
		auto       injected     = rate_limit(application::github_rate_limit_resource::graphql);
		injected.Remaining      = 123U;
		fixture.Provider.PullRequests.front().Completion(
			application::github_pull_request_error{
				.Request = request,
				.Error   = {
					.Failure           = application::github_provider_failure::permission,
					.RetryAfterSeconds = std::nullopt,
					.RateLimits        = rate_limits_for(injected),
					.RateLimitCause    = std::nullopt,
				},
			});
		fixture.Lane.run_next();
		expect(fixture.Host.rate_limits().Graphql == priorGraphql
				   && fixture.Projections.host_diagnostic_count() == 0U
				   && !fixture.Host.state().PendingPullRequest.has_value(),
			   "REST-only reads cannot inject a GraphQL error observation");
	}

	auto test_projection_failures_cannot_interrupt_effects_or_cleanup() -> void
	{
		{
			host_fixture fixture;
			fixture.Projections.ThrowNextOutput = true;
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			expect(fixture.Provider.Profiles.size() == 1U
					   && fixture.Host.projection_delivery_failed(),
				   "a failed transition projection cannot veto decided effects");
		}

		{
			host_fixture fixture;
			advance_native_to_retrieve(fixture);
			const auto retrieve = fixture.Vault.Retrieves.front().Context;
			fixture.Vault.Retrieves.front().Completion(
				application::github_credential_retrieve_success{
					.Context    = retrieve,
					.Credential = credential(),
				});
			fixture.Lane.run_next();

			const auto account = fixture.Provider.Viewers.back().Request;
			fixture.Provider.Viewers.back().Completion(
				application::github_viewer_success{
					.Request    = account,
					.Viewer     = viewer(),
					.RateLimits = rate_limits_for(
						application::github_rate_limit_resource::core),
					.AccountTransport = std::make_shared<fake_lease>(),
				});
			fixture.Lane.run_next();
			const auto capability = fixture.Provider.Capabilities.front().Request;

			fixture.Projections.ThrowNextDiagnostic = true;
			fixture.Provider.Capabilities.front().Completion(
				application::github_capability_error{
					.Request = capability,
					.Error   = {
						.Failure           = application::github_provider_failure::unavailable,
						.RetryAfterSeconds = 30U,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					},
				});
			fixture.Lane.run_next();
			expect(
				fixture.Host.projection_delivery_failed()
					&& fixture.Vault.Deletes.size() == 1U,
				"a failed error advisory cannot strand a provisional credential");

			const auto cleanup                      = fixture.Vault.Deletes.front().Context;
			fixture.Projections.ThrowNextDiagnostic = true;
			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_error{
					.Context = cleanup,
					.Failure = application::github_credential_vault_failure::
						operation_failed,
				});
			fixture.Lane.run_next();
			expect(!fixture.Host.state().PendingSession.has_value()
					   && fixture.Projections.host_diagnostic_count() == 0U,
				   "a failed cleanup diagnostic cannot interrupt the deferred "
				   "terminal");
		}
	}

	auto test_projection_reentry_is_serialized_after_the_current_turn() -> void
	{
		{
			host_fixture fixture;
			fixture.Projections.OnNextOutput = [&fixture] { fixture.Host.dispatch(core::deactivate_github_identity_intent{ }); };
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			expect(
				fixture.Provider.Profiles.size() == 1U
					&& fixture.Provider.CancelledProfiles
						== std::vector{ fixture.Provider.Profiles.front().Request }
					&& !fixture.Host.state().ActiveProfile.has_value()
					&& !fixture.Host.state().PendingSession.has_value(),
				"projection reentry is ordered after and cancels the current "
				"effect");
		}

		{
			host_fixture fixture;
			advance_native_to_retrieve(fixture);
			const auto retrieve = fixture.Vault.Retrieves.front().Context;
			fixture.Vault.Retrieves.front().Completion(
				application::github_credential_retrieve_success{
					.Context    = retrieve,
					.Credential = credential(),
				});
			fixture.Lane.run_next();
			const auto account = fixture.Provider.Viewers.back().Request;
			fixture.Provider.Viewers.back().Completion(
				application::github_viewer_success{
					.Request    = account,
					.Viewer     = viewer(),
					.RateLimits = rate_limits_for(
						application::github_rate_limit_resource::core),
					.AccountTransport = std::make_shared<fake_lease>(),
				});
			fixture.Lane.run_next();

			fixture.Projections.OnNextOutput = [&fixture] { fixture.Host.dispatch(core::deactivate_github_identity_intent{ }); };
			const auto capability            = fixture.Provider.Capabilities.front().Request;
			fixture.Provider.Capabilities.front().Completion(
				application::github_capability_success{
					.Request     = capability,
					.Facts       = capability_facts(),
					.SchemaFacts = schema_facts(),
					.RateLimits  = rate_limits_for(
						application::github_rate_limit_resource::graphql),
				});
			fixture.Lane.run_next();
			expect(fixture.Projections.session_count() == 1U
					   && !fixture.Host.accepted_session().has_value()
					   && fixture.Vault.Deletes.size() == 1U,
				   "session publication finishes before deferred reentrant "
				   "invalidation");
		}

		{
			host_fixture fixture;
			fixture.Projections.OnEveryOutput = [&fixture] { fixture.Host.dispatch(core::deactivate_github_identity_intent{ }); };
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			expect(fixture.Lane.pending() == 1U
					   && !fixture.Host.state().ActiveProfile.has_value(),
				   "repeated projection feedback yields after one deferred "
				   "generation");

			fixture.Projections.OnEveryOutput = { };
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 12U },
			});
			expect(fixture.Provider.Profiles.size() == 1U
					   && !fixture.Host.state().ActiveProfile.has_value(),
				   "new ingress cannot overtake an older deferred generation");
			fixture.Lane.run_next();
			expect(fixture.Lane.pending() == 0U
					   && fixture.Provider.Profiles.size() == 2U
					   && fixture.Host.state().ActiveProfile
						   == core::host_profile_id{ 12U },
				   "deferred feedback stays FIFO and yields fairly on the decision "
				   "lane");
		}

		{
			host_fixture fixture;
			fixture.Projections.OnEveryOutput = [&fixture] { fixture.Host.dispatch(core::deactivate_github_identity_intent{ }); };
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			expect(fixture.Lane.pending() == 1U,
				   "feedback continuation is pending before terminal shutdown");
			fixture.Host.shutdown();
			expect(fixture.Host.is_closing()
					   && fixture.Provider.CancelledProfiles.size() == 1U,
				   "shutdown closes immediately despite an older deferred "
				   "continuation");
		}
	}

	auto test_invalid_enrollment_cannot_leak_into_a_later_activation() -> void
	{
		host_fixture fixture;
		fixture.Host.enroll_profile({ }, credential());
		expect(fixture.Provider.Profiles.empty(),
			   "invalid profile enrollment submits no provider work");

		fixture.Host.dispatch(core::activate_host_profile_intent{
			.Profile = core::host_profile_id{ 11U },
		});
		const auto request = fixture.Provider.Profiles.front().Request;
		fixture.Provider.Profiles.front().Completion(
			application::github_profile_success{
				.Request   = request,
				.Authority = native_profile(request.Context.Profile),
			});
		fixture.Lane.run_next();
		expect(fixture.Provider.Viewers.empty()
				   && !fixture.Host.state().PendingSession.has_value(),
			   "a later activation cannot consume a rejected enrollment secret");
	}

	auto test_snapshot_authority_and_pr_cancellation_fail_closed() -> void
	{
		{
			host_fixture fixture;
			establish_native_session(fixture);
			fixture.Host.dispatch(core::open_pull_request_intent{
				.Repository = { .Owner = "openai", .Name = "better-pull-review" },
				.Number     = core::pull_request_number{ 27U },
			});
			const auto request                         = fixture.Provider.PullRequests.front().Request;
			auto       fabricated                      = snapshot(request);
			fabricated.CapabilityEvidence.Capabilities = { };
			fabricated.CapabilityEvidence.ReadShape    = { };
			fabricated.ReviewThreadsAvailability       = application::github_collection_availability::unavailable;
			fixture.Provider.PullRequests.front().Completion(
				application::github_pull_request_success{
					.Request  = request,
					.Snapshot = std::move(fabricated),
				});
			fixture.Lane.run_next();
			expect(!fixture.Host.accepted_snapshot().has_value()
					   && fixture.Projections.pull_request_count() == 0U,
				   "same-revision fabricated capability evidence cannot be staged");
		}

		{
			host_fixture fixture;
			establish_native_session(fixture);
			fixture.Host.dispatch(core::open_pull_request_intent{
				.Repository = { .Owner = "openai", .Name = "better-pull-review" },
				.Number     = core::pull_request_number{ 27U },
			});
			const auto request     = fixture.Provider.PullRequests.front().Request;
			auto       wrongViewer = snapshot(request);
			wrongViewer.Viewer     = {
				.Actor = application::github_actor_id{ "other-viewer" },
				.Login = "other",
			};
			fixture.Provider.PullRequests.front().Completion(
				application::github_pull_request_success{
					.Request  = request,
					.Snapshot = std::move(wrongViewer),
				});
			fixture.Lane.run_next();
			expect(!fixture.Host.accepted_snapshot().has_value()
					   && fixture.Projections.pull_request_count() == 0U,
				   "provider snapshot cannot replace the authenticated viewer");
		}

		{
			host_fixture fixture;
			establish_native_session(fixture);
			fixture.Host.dispatch(core::open_pull_request_intent{
				.Repository = { .Owner = "openai", .Name = "better-pull-review" },
				.Number     = core::pull_request_number{ 27U },
			});
			const auto request = fixture.Provider.PullRequests.front().Request;
			fixture.Provider.PullRequests.front().Completion(
				application::github_pull_request_success{
					.Request  = request,
					.Snapshot = snapshot(request),
				});
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 12U },
			});
			expect(fixture.Provider.CancelledPullRequests == std::vector{ request },
				   "identity switch cancels the exact in-flight PR request");
			fixture.Lane.run_next();
			expect(!fixture.Host.accepted_snapshot().has_value()
					   && fixture.Projections.pull_request_count() == 0U,
				   "queued old-identity PR success is inert after cancellation");
		}

		{
			host_fixture fixture;
			establish_native_session(fixture);
			fixture.Host.dispatch(core::open_pull_request_intent{
				.Repository = { .Owner = "openai", .Name = "better-pull-review" },
				.Number     = core::pull_request_number{ 27U },
			});
			const auto request = fixture.Provider.PullRequests.front().Request;
			fixture.Provider.PullRequests.front().Completion(
				application::github_pull_request_error{
					.Request = request,
					.Error   = {
						.Failure           = application::github_provider_failure::rate_limit,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = rate_limits_for(
							application::github_rate_limit_record{
								.Resource = application::
									github_rate_limit_resource::core,
								.Limit            = 5'000U,
								.Remaining        = 0U,
								.ResetUnixSeconds = 2'000'000'300U,
								.SecondaryLimited = false,
							}),
						.RateLimitCause = application::github_rate_limit_record{
							.Resource = application::
								github_rate_limit_resource::core,
							.Limit            = 5'000U,
							.Remaining        = 0U,
							.ResetUnixSeconds = 2'000'000'300U,
							.SecondaryLimited = false,
						},
					},
				});
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 12U },
			});
			fixture.Lane.run_next();
			expect(!fixture.Host.rate_limits().Core.has_value()
					   && !fixture.Host.rate_limits().Search.has_value()
					   && !fixture.Host.rate_limits().Graphql.has_value()
					   && fixture.Projections.host_diagnostic_count() == 0U,
				   "stale old-identity rate observations cannot repopulate host "
				   "state");
		}

		{
			host_fixture fixture;
			establish_native_session(fixture);
			fixture.Host.dispatch(core::open_pull_request_intent{
				.Repository = { .Owner = "openai", .Name = "better-pull-review" },
				.Number     = core::pull_request_number{ 27U },
			});
			const auto request = fixture.Provider.PullRequests.front().Request;
			fixture.Provider.PullRequests.front().Completion(
				application::github_pull_request_cancelled{
					.Request = request,
				});
			fixture.Lane.run_next();
			expect(
				!fixture.Host.state().PendingPullRequest.has_value()
					&& !fixture.Host.accepted_snapshot().has_value(),
				"current PR cancellation clears pending state without a snapshot");
		}
	}

	auto test_new_open_clears_host_snapshot_but_refresh_retains_it() -> void
	{
		host_fixture fixture;
		establish_native_session(fixture);
		fixture.Host.dispatch(core::open_pull_request_intent{
			.Repository = { .Owner = "openai", .Name = "better-pull-review" },
			.Number     = core::pull_request_number{ 27U },
		});
		const auto first = fixture.Provider.PullRequests.front().Request;
		fixture.Provider.PullRequests.front().Completion(
			application::github_pull_request_success{
				.Request  = first,
				.Snapshot = snapshot(first),
			});
		fixture.Lane.run_next();
		expect(fixture.Host.accepted_snapshot().has_value(),
			   "first PR snapshot is accepted");

		fixture.Host.dispatch(core::refresh_pull_request_intent{ });
		expect(fixture.Host.accepted_snapshot().has_value(),
			   "refresh retains the last accepted snapshot while loading");
		const auto refresh = fixture.Provider.PullRequests.back().Request;
		fixture.Provider.PullRequests.back().Completion(
			application::github_pull_request_cancelled{
				.Request = refresh,
			});
		fixture.Lane.run_next();
		expect(fixture.Host.accepted_snapshot().has_value(),
			   "cancelled refresh preserves the last accepted snapshot");

		fixture.Host.dispatch(core::open_pull_request_intent{
			.Repository = { .Owner = "openai", .Name = "another-repository" },
			.Number     = core::pull_request_number{ 28U },
		});
		expect(!fixture.Host.accepted_snapshot().has_value(),
			   "accepted new open clears the old host-owned snapshot immediately");
		const auto second = fixture.Provider.PullRequests.back().Request;
		fixture.Provider.PullRequests.back().Completion(
			application::github_pull_request_error{
				.Request = second,
				.Error   = {
					.Failure           = application::github_provider_failure::permission,
					.RetryAfterSeconds = std::nullopt,
					.RateLimits        = { },
					.RateLimitCause    = std::nullopt,
				},
			});
		fixture.Lane.run_next();
		expect(!fixture.Host.accepted_snapshot().has_value(),
			   "failed new open cannot revive the previous PR snapshot");
	}

	auto test_vault_wrong_context_and_submit_throw_fail_closed() -> void
	{
		{
			host_fixture fixture;
			advance_native_to_store(fixture);
			const auto expected = fixture.Vault.Stores.front().Context;
			auto       wrong    = expected;
			wrong.Request       = core::request_id{ 999U };
			fixture.Vault.Stores.front().Completion(
				application::github_credential_store_success{
					.Context     = wrong,
					.Disposition = application::github_credential_store_disposition::stored,
				});
			fixture.Lane.run_next();
			expect(
				fixture.Vault.Deletes.size() == 1U
					&& fixture.Vault.Deletes.front().Context.Authority
						== expected.Authority
					&& fixture.Vault.Deletes.front().Context != wrong,
				"wrong-context store success cleans only host-captured authority");
			const auto cleanup = fixture.Vault.Deletes.front().Context;
			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			fixture.Lane.run_next();
			const auto diagnostics = fixture.Projections.host_diagnostic_count();
			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			fixture.Lane.run_next();
			expect(
				fixture.Projections.host_diagnostic_count() == diagnostics
					&& !fixture.Host.state().PendingSession.has_value(),
				"duplicate delete terminal is stale and cannot rewrite lifecycle");
		}

		{
			host_fixture fixture;
			fixture.Vault.ThrowOnStore = true;
			advance_native_to_store(fixture);
			expect(fixture.Lane.pending() == 1U,
				   "synchronous vault throw is converted to a lane completion");
			fixture.Lane.run_next();
			expect(
				!fixture.Host.accepted_session().has_value()
					&& !fixture.Vault.Deletes.empty(),
				"vault submit throw terminates and cleans provisional authority");
		}

		{
			host_fixture fixture;
			advance_native_to_retrieve(fixture);
			const auto retrieve = fixture.Vault.Retrieves.front().Context;
			fixture.Vault.Retrieves.front().Completion(
				application::github_credential_retrieve_success{
					.Context    = retrieve,
					.Credential = credential(),
				});
			fixture.Lane.run_next();
			const auto viewerRequest = fixture.Provider.Viewers.back().Request;
			fixture.Vault.Retrieves.front().Completion(
				application::github_credential_retrieve_success{
					.Context    = retrieve,
					.Credential = credential(),
				});
			fixture.Lane.run_next();
			expect(
				fixture.Provider.Viewers.size() == 2U
					&& fixture.Provider.CancelledViewers.empty()
					&& fixture.Vault.Deletes.empty()
					&& fixture.Provider.Viewers.back().Request == viewerRequest,
				"duplicate retrieve terminal is stale and cannot tear down viewer");
		}
	}

	auto test_reenrollment_waits_for_running_vault_cleanup() -> void
	{
		{
			host_fixture fixture;
			advance_native_to_store(fixture);
			const auto running = fixture.Vault.Stores.front().Context;

			fixture.Host.enroll_profile(core::host_profile_id{ 11U }, credential());
			expect(fixture.Vault.Cancellations == std::vector{ running }
					   && fixture.Vault.Deletes.empty()
					   && fixture.Provider.Profiles.size() == 1U,
				   "same-authority re-enrollment waits for running store terminal");

			fixture.Vault.Stores.front().Completion(
				application::github_credential_store_success{
					.Context     = running,
					.Disposition = application::github_credential_store_disposition::stored,
				});
			fixture.Lane.run_next();
			expect(fixture.Vault.Deletes.size() == 1U
					   && fixture.Vault.Deletes.front().Context.Authority
						   == running.Authority
					   && fixture.Provider.Profiles.size() == 1U,
				   "store terminal starts exact cleanup before replacement work");

			const auto cleanup = fixture.Vault.Deletes.front().Context;
			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			fixture.Lane.run_next();
			expect(fixture.Provider.Profiles.size() == 2U,
				   "replacement profile starts only after store cleanup succeeds");
		}

		{
			host_fixture fixture;
			advance_native_to_retrieve(fixture);
			const auto running = fixture.Vault.Retrieves.front().Context;

			fixture.Host.enroll_profile(core::host_profile_id{ 11U }, credential());
			expect(
				fixture.Vault.Cancellations == std::vector{ running }
					&& fixture.Vault.Deletes.empty()
					&& fixture.Provider.Profiles.size() == 1U,
				"same-authority re-enrollment waits for running retrieve terminal");

			fixture.Vault.Retrieves.front().Completion(
				application::github_credential_retrieve_success{
					.Context    = running,
					.Credential = credential(),
				});
			fixture.Lane.run_next();
			expect(
				fixture.Vault.Deletes.size() == 1U
					&& fixture.Vault.Deletes.front().Context.Authority
						== running.Authority
					&& fixture.Provider.Profiles.size() == 1U,
				"retrieve terminal starts exact cleanup before replacement work");

			const auto cleanup = fixture.Vault.Deletes.front().Context;
			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			fixture.Lane.run_next();
			expect(
				fixture.Provider.Profiles.size() == 2U,
				"replacement profile starts only after retrieve cleanup succeeds");
		}
	}

	auto test_bootstrap_lease_and_changed_account_fail_closed() -> void
	{
		{
			host_fixture fixture;
			fixture.Host.enroll_profile(core::host_profile_id{ 11U }, credential());
			const auto profile = fixture.Provider.Profiles.front().Request;
			fixture.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = profile,
					.Authority = native_profile(profile.Context.Profile),
				});
			fixture.Lane.run_next();
			const auto bootstrap = fixture.Provider.Viewers.front().Request;
			fixture.Provider.Viewers.front().Completion(
				application::github_viewer_success{
					.Request    = bootstrap,
					.Viewer     = viewer(),
					.RateLimits = rate_limits_for(
						application::github_rate_limit_resource::core),
					.AccountTransport = std::make_shared<fake_lease>(),
				});
			fixture.Lane.run_next();
			expect(fixture.Vault.Stores.empty()
					   && !fixture.Host.accepted_session().has_value()
					   && !fixture.Host.state().PendingSession.has_value(),
				   "a bootstrap provider cannot smuggle a reusable account lease");
		}

		{
			host_fixture fixture;
			fixture.Host.enroll_profile(core::host_profile_id{ 11U }, credential());
			const auto profile = fixture.Provider.Profiles.front().Request;
			fixture.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = profile,
					.Authority = native_profile(profile.Context.Profile),
				});
			fixture.Lane.run_next();
			const auto bootstrap = fixture.Provider.Viewers.front().Request;
			fixture.Provider.Viewers.front().Completion(
				application::github_viewer_success{
					.Request    = bootstrap,
					.Viewer     = viewer(),
					.RateLimits = rate_limits_for(
						application::github_rate_limit_resource::core),
					.AccountTransport = { },
				});
			fixture.Lane.run_next();
			const auto store = fixture.Vault.Stores.front().Context;
			fixture.Vault.Stores.front().Completion(
				application::github_credential_store_success{
					.Context     = store,
					.Disposition = application::github_credential_store_disposition::stored,
				});
			fixture.Lane.run_next();
			const auto retrieve = fixture.Vault.Retrieves.front().Context;
			fixture.Vault.Retrieves.front().Completion(
				application::github_credential_retrieve_success{
					.Context    = retrieve,
					.Credential = credential(),
				});
			fixture.Lane.run_next();
			const auto account = fixture.Provider.Viewers.back().Request;
			fixture.Provider.Viewers.back().Completion(
				application::github_viewer_success{
					.Request    = account,
					.Viewer     = viewer(core::github_account_id{ 52U }),
					.RateLimits = rate_limits_for(
						application::github_rate_limit_resource::core),
					.AccountTransport = std::make_shared<fake_lease>(),
				});
			fixture.Lane.run_next();
			expect(
				fixture.Provider.Capabilities.empty()
					&& fixture.Vault.Deletes.size() == 1U
					&& fixture.Vault.Deletes.front().Context.Authority.Account
						== core::github_account_id{ 51U },
				"changed second /user identity triggers exact provisional cleanup");
			const auto cleanup = fixture.Vault.Deletes.front().Context;
			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			fixture.Lane.run_next();
			expect(!fixture.Host.accepted_session().has_value()
					   && !fixture.Host.state().PendingSession.has_value(),
				   "changed account never becomes an accepted core session");
		}
	}

	// [[BPR-1302]]. A caller driving the decision lane waits for a running
	// host's work with is_running_idle. is_quiescent is the shutdown
	// predicate: the tour CLI waited on it and never returned.
	auto test_running_idleness_is_distinct_from_shutdown_quiescence() -> void
	{
		host_fixture fixture;
		expect(fixture.Host.is_running_idle() && !fixture.Host.is_quiescent(),
			   "a running host that owes nothing is idle and not quiescent");
		static_cast<void>(establish_native_session(fixture));
		fixture.Lane.drain();
		expect(fixture.Host.accepted_session().has_value() && fixture.Host.is_running_idle()
				   && !fixture.Host.is_quiescent(),
			   "an accepted session leaves the running host idle, never quiescent");

		fixture.Host.dispatch(core::open_pull_request_intent{
			.Repository = { .Owner = "openai", .Name = "better-pull-review" },
			.Number     = core::pull_request_number{ 27U },
		});
		expect(!fixture.Host.is_running_idle() && !fixture.Host.is_quiescent(),
			   "an outstanding pull request read is work in flight");
		const auto request = fixture.Provider.PullRequests.front().Request;
		fixture.Provider.PullRequests.front().Completion(
			application::github_pull_request_success{
				.Request  = request,
				.Snapshot = snapshot(request),
			});
		expect(!fixture.Host.is_running_idle(),
			   "a completion queued on the lane is not yet answered");
		fixture.Lane.drain();
		expect(fixture.Host.accepted_snapshot().has_value() && fixture.Host.is_running_idle()
				   && !fixture.Host.is_quiescent(),
			   "the drained lane leaves the accepted read and an idle running host");

		fixture.Host.shutdown();
		fixture.Lane.drain();
		expect(!fixture.Host.is_running_idle() && fixture.Host.is_quiescent(),
			   "after shutdown the host is quiescent and no longer running-idle");
	}

	auto test_shutdown_cancels_and_suppresses_late_completion() -> void
	{
		host_fixture fixture;
		fixture.Host.dispatch(core::activate_host_profile_intent{
			.Profile = core::host_profile_id{ 11U },
		});
		const auto request         = fixture.Provider.Profiles.front().Request;
		const auto projectionCount = fixture.Projections.transition_count();
		fixture.Host.shutdown();
		// [[BPR-0545]]. The occasion's refresh erasure answers on this
		// lane too, and quiescence waits for its terminal.
		fixture.Lane.drain();
		expect(fixture.Host.is_closing() && fixture.Host.is_quiescent()
				   && fixture.Provider.CancelledProfiles == std::vector{ request }
				   && !fixture.Host.state().ActiveProfile.has_value(),
			   "shutdown closes ingress, invalidates core, and cancels exact work");
		fixture.Provider.Profiles.front().Completion(
			application::github_profile_success{
				.Request   = request,
				.Authority = cli_profile(request.Context.Profile),
			});
		fixture.Lane.run_next();
		expect(fixture.Provider.Viewers.empty()
				   && fixture.Projections.transition_count() == projectionCount,
			   "late completion after shutdown cannot publish or start new work");
	}

	auto test_shutdown_preserves_persistent_credentials() -> void
	{
		static_assert(!std::is_trivially_copyable_v<
					  application::github_credential_binding_catalog>);
		static_assert(!std::is_copy_constructible_v<
					  application::github_credential_binding_catalog>);
		static_assert(!std::is_move_constructible_v<
					  application::github_credential_binding_catalog>);

		application::github_credential_binding_catalog bindings;
		host_fixture                                   fixture{ bindings };
		establish_native_session(fixture);
		expect(bindings.size() == 1U,
			   "exact /user acceptance records one host-owned binding");
		fixture.Host.shutdown();
		// [[BPR-0545]]. The occasion's refresh erasure answers on this
		// lane too, and quiescence waits for its terminal.
		fixture.Lane.drain();
		expect(fixture.Vault.Deletes.empty() && fixture.Host.is_quiescent()
				   && bindings.size() == 1U,
			   "ordinary shutdown preserves accepted persistent credentials");
	}

	auto test_shutdown_deletes_persistent_credentials_by_policy() -> void
	{
		constexpr auto policy = application::github_credential_shutdown_policy::
			delete_retained_on_shutdown;

		{
			host_fixture fixture{ policy };
			establish_native_session(fixture);
			const auto authority       = fixture.Vault.Stores.front().Context.Authority;
			const auto transitionCount = fixture.Projections.transition_count();

			fixture.Host.shutdown();
			expect(
				fixture.Vault.Deletes.size() == 1U
					&& fixture.Vault.Deletes.front().Context.Authority == authority
					&& fixture.Bindings.size() == 1U
					&& !fixture.Host.is_quiescent(),
				"delete-on-shutdown waits for exact accepted-credential cleanup");

			fixture.Host.shutdown();
			expect(fixture.Vault.Deletes.size() == 1U
					   && fixture.Vault.Cancellations.empty(),
				   "duplicate shutdown preserves the one required delete");

			const auto cleanup = fixture.Vault.Deletes.front().Context;
			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::absent,
				});
			expect(!fixture.Host.is_quiescent() && fixture.Bindings.size() == 1U,
				   "delete completion cannot clear authority off the decision "
				   "lane");
			fixture.Lane.run_next();
			// [[BPR-0545]]. The occasion's refresh erasure is issued from this
			// terminal, so the cleanup is not finished until its own answer has
			// been applied, and that is what holds quiescence here.
			expect(!fixture.Host.is_quiescent()
					   && !fixture.Host.shutdown_cleanup_failed()
					   && fixture.Bindings.size() == 0U,
				   "the credential's terminal clears its binding and hands the "
				   "occasion to the slot's erasure");
			fixture.Lane.run_next();
			// [[BPR-0545]]. The occasion's refresh erasure answers on this
			// lane too, and quiescence waits for its terminal.
			fixture.Lane.drain();
			expect(fixture.Host.is_quiescent()
					   && !fixture.Host.shutdown_cleanup_failed()
					   && fixture.Bindings.size() == 0U
					   && fixture.Projections.transition_count()
						   == transitionCount,
				   "authoritative deletion or absence completes shutdown cleanup");

			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_error{
					.Context = cleanup,
					.Failure = application::github_credential_vault_failure::
						operation_failed,
				});
			fixture.Lane.run_next();
			// [[BPR-0545]]. The occasion's refresh erasure answers on this
			// lane too, and quiescence waits for its terminal.
			fixture.Lane.drain();
			expect(fixture.Host.is_quiescent()
					   && !fixture.Host.shutdown_cleanup_failed()
					   && fixture.Bindings.size() == 0U,
				   "late duplicate delete cannot rewrite successful cleanup");
		}

		{
			host_fixture fixture{ policy };
			establish_native_session(fixture);
			fixture.Host.shutdown();
			const auto cleanup       = fixture.Vault.Deletes.front().Context;
			auto       wrong         = cleanup;
			wrong.IdentityGeneration = core::github_identity_generation{
				cleanup.IdentityGeneration.value() + 1U
			};
			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = wrong,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			fixture.Lane.run_next();
			// The second turn is the refresh erasure's own terminal, which the
			// first one issued ([[BPR-0545]]).
			fixture.Lane.run_next();
			// [[BPR-0545]]. The occasion's refresh erasure answers on this
			// lane too, and quiescence waits for its terminal.
			fixture.Lane.drain();
			expect(
				fixture.Host.is_quiescent()
					&& fixture.Host.shutdown_cleanup_failed()
					&& fixture.Bindings.size() == 1U
					&& fixture.Projections.last_host_diagnostic().Reason
						== core::github_reason_code::vault_unavailable,
				"wrong cleanup terminal is visible and retains the exact binding "
				"tombstone");

			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			fixture.Lane.run_next();
			expect(fixture.Host.shutdown_cleanup_failed()
					   && fixture.Bindings.size() == 1U,
				   "late exact delete cannot conceal an already failed cleanup");
		}
	}

	auto test_caller_cache_cannot_bypass_capability_probe() -> void
	{
		static_assert(!std::is_constructible_v<
					  application::github_application_host,
					  core::github_state,
					  application::github_profile_port&,
					  application::github_viewer_port&,
					  application::github_capability_port&,
					  application::github_pull_request_port&,
					  application::github_credential_vault_port&,
					  application::github_credential_binding_catalog&,
					  application::github_capability_cache,
					  application::decision_executor&,
					  application::github_projection_port&>);

		auto untrusted = application::github_capability_cache::create({ });
		expect(untrusted.has_value(), "hostile cache fixture creates");
		const auto profile = native_profile(core::host_profile_id{ 11U });
		const auto forged  = untrusted->store(application::github_capability_probe_observation{
			.Authority = {
				.Profile               = profile.Profile,
				.ProfileRevision       = profile.ProfileRevision,
				.Host                  = profile.Host,
				.Account               = core::github_account_id{ 51U },
				.ServerContract        = profile.ServerContract,
				.ProbeContractRevision = profile.ProbeContractRevision,
			},
			.ObservedAt  = application::github_freshness_epoch{ 1U },
			.Facts       = capability_facts(),
			.SchemaFacts = schema_facts(),
		});
		expect(forged.stored() && untrusted->size() == 1U,
			   "a standalone caller cache can contain syntactically valid facts");

		host_fixture fixture;
		advance_native_to_retrieve(fixture);
		const auto retrieve = fixture.Vault.Retrieves.front().Context;
		fixture.Vault.Retrieves.front().Completion(
			application::github_credential_retrieve_success{
				.Context    = retrieve,
				.Credential = credential(),
			});
		fixture.Lane.run_next();
		const auto account = fixture.Provider.Viewers.back().Request;
		fixture.Provider.Viewers.back().Completion(
			application::github_viewer_success{
				.Request          = account,
				.Viewer           = viewer(),
				.RateLimits       = rate_limits_for(application::github_rate_limit_resource::core),
				.AccountTransport = std::make_shared<fake_lease>(),
			});
		fixture.Lane.run_next();
		expect(fixture.Provider.Capabilities.size() == 1U
				   && fixture.Host.capability_cache_size() == 0U
				   && !fixture.Host.accepted_session().has_value(),
			   "production host owns an empty cache and still performs the probe");
	}

	auto test_catalog_resume_and_replacement_revalidate_persistent_credentials()
		-> void
	{
		{
			application::github_credential_binding_catalog bindings;
			const auto                                     authority = seed_persistent_binding(bindings);
			host_fixture                                   resumed{ bindings };
			const auto                                     capability = advance_resume_to_capability(resumed, authority);
			resumed.Provider.Capabilities.front().Completion(
				application::github_capability_success{
					.Request     = capability,
					.Facts       = capability_facts(),
					.SchemaFacts = schema_facts(),
					.RateLimits  = rate_limits_for(
						application::github_rate_limit_resource::graphql),
				});
			resumed.Lane.run_next();
			expect(resumed.Host.accepted_session().has_value()
					   && resumed.Vault.Stores.empty() && bindings.size() == 1U,
				   "fresh host resumes only after exact account and capability "
				   "validation");
		}

		{
			application::github_credential_binding_catalog bindings;
			static_cast<void>(seed_persistent_binding(bindings));
			host_fixture mismatchedProfile{ bindings };
			mismatchedProfile.Host.resume_profile(core::host_profile_id{ 11U });
			const auto request      = mismatchedProfile.Provider.Profiles.front().Request;
			auto       changed      = native_profile(request.Context.Profile);
			changed.ProfileRevision = application::github_profile_revision{ 32U };
			mismatchedProfile.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = request,
					.Authority = changed,
				});
			mismatchedProfile.Lane.run_next();
			expect(mismatchedProfile.Vault.Retrieves.empty()
					   && mismatchedProfile.Vault.Deletes.size() == 1U
					   && bindings.size() == 1U,
				   "profile revision replacement exact-deletes the retained "
				   "authority");
			const auto cleanup = mismatchedProfile.Vault.Deletes.front().Context;
			mismatchedProfile.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			mismatchedProfile.Lane.run_next();
			expect(
				bindings.size() == 0U
					&& !mismatchedProfile.Host.state().PendingSession.has_value(),
				"successful replacement cleanup forgets the trusted binding");
		}

		{
			application::github_credential_binding_catalog bindings;
			const auto                                     profile = core::host_profile_id{ 11U };
			static_cast<void>(seed_persistent_binding(
				bindings,
				profile,
				core::github_account_id{ 51U },
				ghes_native_profile(profile)));
			host_fixture mismatchedHostname{ bindings };
			mismatchedHostname.Host.resume_profile(profile);
			const auto request = mismatchedHostname.Provider.Profiles.front().Request;
			const auto changed = ghes_native_profile(
				request.Context.Profile, "ghe.other.invalid");
			mismatchedHostname.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = request,
					.Authority = changed,
				});
			mismatchedHostname.Lane.run_next();
			expect(
				mismatchedHostname.Vault.Retrieves.empty()
					&& mismatchedHostname.Vault.Deletes.size() == 1U
					&& bindings.size() == 1U,
				"hostname replacement exact-deletes the retained authority");
			const auto cleanup = mismatchedHostname.Vault.Deletes.front().Context;
			mismatchedHostname.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			mismatchedHostname.Lane.run_next();
			expect(
				bindings.size() == 0U
					&& !mismatchedHostname.Host.state()
							.PendingSession.has_value(),
				"hostname replacement cleanup forgets the trusted binding");
		}

		{
			application::github_credential_binding_catalog bindings;
			static_cast<void>(seed_persistent_binding(bindings));
			host_fixture changedAccount{ bindings };
			changedAccount.Host.resume_profile(core::host_profile_id{ 11U });
			const auto profile = changedAccount.Provider.Profiles.front().Request;
			changedAccount.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = profile,
					.Authority = native_profile(profile.Context.Profile),
				});
			changedAccount.Lane.run_next();
			const auto retrieve = changedAccount.Vault.Retrieves.front().Context;
			changedAccount.Vault.Retrieves.front().Completion(
				application::github_credential_retrieve_success{
					.Context    = retrieve,
					.Credential = credential(),
				});
			changedAccount.Lane.run_next();
			const auto account = changedAccount.Provider.Viewers.front().Request;
			changedAccount.Provider.Viewers.front().Completion(
				application::github_viewer_success{
					.Request    = account,
					.Viewer     = viewer(core::github_account_id{ 52U }),
					.RateLimits = rate_limits_for(
						application::github_rate_limit_resource::core),
					.AccountTransport = std::make_shared<fake_lease>(),
				});
			changedAccount.Lane.run_next();
			expect(changedAccount.Provider.Capabilities.empty()
					   && changedAccount.Vault.Deletes.size() == 1U
					   && changedAccount.Vault.Deletes.front()
							   .Context.Authority.Account
						   == core::github_account_id{ 51U },
				   "resumed /user account mismatch exact-deletes the selected "
				   "authority");
		}

		{
			application::github_credential_binding_catalog bindings;
			const auto                                     authority = seed_persistent_binding(bindings);
			host_fixture                                   failedProbe{ bindings };
			const auto                                     capability = advance_resume_to_capability(failedProbe, authority);
			failedProbe.Provider.Capabilities.front().Completion(
				application::github_capability_error{
					.Request = capability,
					.Error   = {
						.Failure           = application::github_provider_failure::permission,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					},
				});
			failedProbe.Lane.run_next();
			expect(
				failedProbe.Vault.Deletes.empty()
					&& !failedProbe.Host.accepted_session().has_value()
					&& !failedProbe.Host.state().PendingSession.has_value()
					&& bindings.size() == 1U,
				"ordinary resume failure preserves the trusted persistent binding");

			failedProbe.Host.resume_profile(core::host_profile_id{ 11U });
			expect(failedProbe.Vault.Deletes.empty()
					   && failedProbe.Provider.Profiles.size() == 2U,
				   "same-profile resume retry preserves rather than deletes its "
				   "binding");
			const auto retryProfile = failedProbe.Provider.Profiles.back().Request;
			failedProbe.Provider.Profiles.back().Completion(
				application::github_profile_success{
					.Request   = retryProfile,
					.Authority = native_profile(retryProfile.Context.Profile),
				});
			failedProbe.Lane.run_next();
			expect(failedProbe.Vault.Retrieves.size() == 2U
					   && failedProbe.Vault.Deletes.empty(),
				   "same-profile retry reaches the exact retained vault authority");
			const auto retryRetrieve = failedProbe.Vault.Retrieves.back().Context;
			failedProbe.Vault.Retrieves.back().Completion(
				application::github_credential_retrieve_error{
					.Context = retryRetrieve,
					.Failure = application::github_credential_vault_failure::
						operation_failed,
				});
			failedProbe.Lane.run_next();

			failedProbe.Host.dispatch(core::deactivate_github_identity_intent{ });
			expect(failedProbe.Vault.Deletes.size() == 1U,
				   "explicit invalidation after failure exact-deletes retained "
				   "material");
			const auto cleanup = failedProbe.Vault.Deletes.front().Context;
			failedProbe.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			failedProbe.Lane.run_next();
			expect(bindings.size() == 0U,
				   "explicit invalidation forgets the retained catalog authority");
		}

		{
			application::github_credential_binding_catalog bindings;
			const auto                                     authorityA = seed_persistent_binding(
				bindings, core::host_profile_id{ 11U }, core::github_account_id{ 51U });
			const auto authorityB = seed_persistent_binding(
				bindings, core::host_profile_id{ 12U }, core::github_account_id{ 52U });
			host_fixture switchProfile{ bindings };
			const auto   capability = advance_resume_to_capability(switchProfile, authorityA);
			switchProfile.Provider.Capabilities.front().Completion(
				application::github_capability_success{
					.Request     = capability,
					.Facts       = capability_facts(),
					.SchemaFacts = schema_facts(),
					.RateLimits  = rate_limits_for(
						application::github_rate_limit_resource::graphql),
				});
			switchProfile.Lane.run_next();

			switchProfile.Host.resume_profile(core::host_profile_id{ 12U });
			expect(
				switchProfile.Vault.Deletes.size() == 1U
					&& switchProfile.Vault.Deletes.front().Context.Authority
						== authorityA
					&& switchProfile.Provider.Profiles.size() == 1U,
				"switching retained profiles deletes the active authority first");
			const auto cleanup = switchProfile.Vault.Deletes.front().Context;
			switchProfile.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			switchProfile.Lane.run_next();
			expect(bindings.size() == 1U
					   && switchProfile.Provider.Profiles.size() == 2U,
				   "successful predecessor cleanup starts the staged retained "
				   "profile");
			// [[BPR-0545]]. The predecessor's refresh erasure is issued from
			// that delete's terminal and answers on the lane; its turn is
			// taken here so the staged profile's own terminal is next.
			switchProfile.Lane.run_next();

			const auto failedB = switchProfile.Provider.Profiles.back().Request;
			switchProfile.Provider.Profiles.back().Completion(
				application::github_profile_error{
					.Request = failedB,
					.Error   = {
						.Failure           = application::github_provider_failure::permission,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					},
				});
			switchProfile.Lane.run_next();
			expect(!switchProfile.Host.state().PendingSession.has_value()
					   && switchProfile.Vault.Deletes.size() == 1U,
				   "ordinary staged-profile failure preserves its trusted binding");

			switchProfile.Host.dispatch(core::deactivate_github_identity_intent{ });
			expect(
				switchProfile.Vault.Deletes.size() == 2U
					&& switchProfile.Vault.Deletes.back().Context.Authority
						== authorityB,
				"later deactivation exact-deletes the staged retained authority");
		}

		{
			application::github_credential_binding_catalog bindings;
			const auto                                     authorityA = seed_persistent_binding(
				bindings, core::host_profile_id{ 11U }, core::github_account_id{ 51U });
			const auto authorityB = seed_persistent_binding(
				bindings, core::host_profile_id{ 12U }, core::github_account_id{ 52U });
			host_fixture inflightSwitch{ bindings };
			inflightSwitch.Host.resume_profile(core::host_profile_id{ 11U });
			const auto profileA = inflightSwitch.Provider.Profiles.front().Request;
			inflightSwitch.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = profileA,
					.Authority = native_profile(profileA.Context.Profile),
				});
			inflightSwitch.Lane.run_next();
			expect(inflightSwitch.Vault.Retrieves.size() == 1U
					   && inflightSwitch.Vault.Retrieves.front().Context.Authority
						   == authorityA,
				   "profile A resume starts its exact retained retrieval");

			inflightSwitch.Host.resume_profile(core::host_profile_id{ 12U });
			expect(inflightSwitch.Vault.Deletes.empty()
					   && inflightSwitch.Provider.Profiles.size() == 1U,
				   "profile B waits for the running profile A vault terminal");
			const auto retrieveA = inflightSwitch.Vault.Retrieves.front().Context;
			inflightSwitch.Vault.Retrieves.front().Completion(
				application::github_credential_retrieve_cancelled{
					.Context = retrieveA,
				});
			inflightSwitch.Lane.run_next();
			expect(inflightSwitch.Vault.Deletes.size() == 1U
					   && inflightSwitch.Vault.Deletes.front().Context.Authority
						   == authorityA
					   && bindings.size() == 2U,
				   "deferred invalidation exact-deletes A rather than staged B");

			const auto cleanupA = inflightSwitch.Vault.Deletes.front().Context;
			inflightSwitch.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanupA,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			inflightSwitch.Lane.run_next();
			expect(bindings.size() == 1U
					   && inflightSwitch.Provider.Profiles.size() == 2U,
				   "profile B starts only after A cleanup forgets A");

			const auto profileB = inflightSwitch.Provider.Profiles.back().Request;
			inflightSwitch.Provider.Profiles.back().Completion(
				application::github_profile_error{
					.Request = profileB,
					.Error   = {
						.Failure           = application::github_provider_failure::permission,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					},
				});
			inflightSwitch.Lane.run_next();
			inflightSwitch.Host.dispatch(
				core::deactivate_github_identity_intent{ });
			expect(inflightSwitch.Vault.Deletes.size() == 2U
					   && inflightSwitch.Vault.Deletes.back().Context.Authority
						   == authorityB,
				   "failed B remains selected for later exact invalidation");
		}

		{
			application::github_credential_binding_catalog bindings;
			static_cast<void>(seed_persistent_binding(bindings));
			host_fixture replacement{ bindings };
			replacement.Host.enroll_profile(core::host_profile_id{ 11U },
											credential());
			const auto profile = replacement.Provider.Profiles.front().Request;
			replacement.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = profile,
					.Authority = native_profile(profile.Context.Profile),
				});
			replacement.Lane.run_next();
			expect(replacement.Vault.Deletes.size() == 1U
					   && replacement.Provider.Viewers.empty(),
				   "fresh enrollment deletes an existing trusted binding first");
			const auto cleanup = replacement.Vault.Deletes.front().Context;
			replacement.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			replacement.Lane.run_next();
			expect(bindings.size() == 0U
					   && replacement.Provider.Profiles.size() == 2U,
				   "replacement cleanup resumes enrollment with no stale selector");
		}

		{
			application::github_credential_binding_catalog bindings;
			const auto                                     authority = seed_persistent_binding(bindings);
			host_fixture                                   failedProfile{ bindings };
			failedProfile.Host.resume_profile(core::host_profile_id{ 11U });
			const auto resume = failedProfile.Provider.Profiles.front().Request;
			failedProfile.Provider.Profiles.front().Completion(
				application::github_profile_error{
					.Request = resume,
					.Error   = {
						.Failure           = application::github_provider_failure::permission,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					},
				});
			failedProfile.Lane.run_next();
			expect(bindings.size() == 1U && failedProfile.Vault.Deletes.empty(),
				   "profile failure preserves the retained binding for recovery");

			failedProfile.Host.enroll_profile(core::host_profile_id{ 12U },
											  credential());
			expect(
				failedProfile.Vault.Deletes.size() == 1U
					&& failedProfile.Vault.Deletes.front().Context.Authority
						== authority,
				"later profile enrollment exact-deletes a failed resume binding");
			const auto cleanup = failedProfile.Vault.Deletes.front().Context;
			failedProfile.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			failedProfile.Lane.run_next();
			expect(bindings.size() == 0U
					   && failedProfile.Provider.Profiles.size() == 2U
					   && failedProfile.Provider.Profiles.back()
							   .Request.Context.Profile
						   == core::host_profile_id{ 12U },
				   "profile switch continues only after exact retained cleanup");
		}

		{
			application::github_credential_binding_catalog bindings;
			const auto                                     authority = seed_persistent_binding(bindings);
			host_fixture                                   changedTransport{ bindings };
			changedTransport.Host.resume_profile(core::host_profile_id{ 11U });
			const auto request = changedTransport.Provider.Profiles.front().Request;
			changedTransport.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = request,
					.Authority = cli_profile(request.Context.Profile),
				});
			changedTransport.Lane.run_next();
			expect(
				changedTransport.Vault.Deletes.size() == 1U
					&& changedTransport.Vault.Deletes.front().Context.Authority
						== authority
					&& changedTransport.Provider.Viewers.empty(),
				"native-to-CLI replacement exact-deletes the old native binding");
		}

		{
			application::github_credential_binding_catalog bindings;
			static_cast<void>(seed_persistent_binding(bindings));
			host_fixture absent{ bindings };
			absent.Host.resume_profile(core::host_profile_id{ 11U });
			const auto profile = absent.Provider.Profiles.front().Request;
			absent.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = profile,
					.Authority = native_profile(profile.Context.Profile),
				});
			absent.Lane.run_next();
			const auto retrieve = absent.Vault.Retrieves.front().Context;
			absent.Vault.Retrieves.front().Completion(
				application::github_credential_retrieve_absent{
					.Context = retrieve,
				});
			absent.Lane.run_next();
			expect(bindings.size() == 0U && absent.Vault.Deletes.empty()
					   && !absent.Host.state().PendingSession.has_value(),
				   "authoritative vault absence forgets a stale binding without "
				   "delete");
		}
	}

	auto test_shutdown_preserves_inflight_persistent_resume() -> void
	{
		enum class retrieve_terminal {
			success,
			cancelled,
			error,
		};

		application::github_credential_binding_catalog bindings;
		static_cast<void>(seed_persistent_binding(bindings));

		for (const auto terminal : { retrieve_terminal::success,
									 retrieve_terminal::cancelled,
									 retrieve_terminal::error }) {
			host_fixture fixture{ bindings };
			fixture.Host.resume_profile(core::host_profile_id{ 11U });
			const auto profile = fixture.Provider.Profiles.front().Request;
			fixture.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = profile,
					.Authority = native_profile(profile.Context.Profile),
				});
			fixture.Lane.run_next();
			const auto running = fixture.Vault.Retrieves.front().Context;

			fixture.Host.shutdown();
			expect(fixture.Vault.Cancellations == std::vector{ running }
					   && fixture.Vault.Deletes.empty()
					   && !fixture.Host.is_quiescent(),
				   "shutdown cancels persistent retrieval without deleting its "
				   "binding");

			switch (terminal) {
			case retrieve_terminal::success:
				fixture.Vault.Retrieves.front().Completion(
					application::github_credential_retrieve_success{
						.Context    = running,
						.Credential = credential(),
					});
				break;
			case retrieve_terminal::cancelled:
				fixture.Vault.Retrieves.front().Completion(
					application::github_credential_retrieve_cancelled{
						.Context = running,
					});
				break;
			case retrieve_terminal::error:
				fixture.Vault.Retrieves.front().Completion(
					application::github_credential_retrieve_error{
						.Context = running,
						.Failure = application::github_credential_vault_failure::
							operation_failed,
					});
				break;
			}
			fixture.Lane.run_next();
			// [[BPR-0545]]. The occasion's refresh erasure answers on this
			// lane too, and quiescence waits for its terminal.
			fixture.Lane.drain();
			expect(fixture.Vault.Deletes.empty() && fixture.Host.is_quiescent()
					   && bindings.size() == 1U,
				   "every persistent retrieval terminal preserves the trusted "
				   "binding");
		}
	}

	auto test_shutdown_deletes_inflight_persistent_resume_by_policy() -> void
	{
		enum class retrieve_terminal {
			success,
			cancelled,
			error,
		};

		constexpr auto policy = application::github_credential_shutdown_policy::
			delete_retained_on_shutdown;
		for (const auto terminal : { retrieve_terminal::success,
									 retrieve_terminal::cancelled,
									 retrieve_terminal::error }) {
			application::github_credential_binding_catalog bindings;
			const auto                                     authority = seed_persistent_binding(bindings);
			host_fixture                                   fixture{ bindings, policy };
			fixture.Host.resume_profile(core::host_profile_id{ 11U });
			const auto profile = fixture.Provider.Profiles.front().Request;
			fixture.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = profile,
					.Authority = native_profile(profile.Context.Profile),
				});
			fixture.Lane.run_next();
			const auto running = fixture.Vault.Retrieves.front().Context;

			fixture.Host.shutdown();
			expect(fixture.Vault.Cancellations == std::vector{ running }
					   && fixture.Vault.Deletes.empty()
					   && !fixture.Host.is_quiescent(),
				   "delete policy waits for the in-flight retained retrieval "
				   "terminal");

			switch (terminal) {
			case retrieve_terminal::success:
				fixture.Vault.Retrieves.front().Completion(
					application::github_credential_retrieve_success{
						.Context    = running,
						.Credential = credential(),
					});
				break;
			case retrieve_terminal::cancelled:
				fixture.Vault.Retrieves.front().Completion(
					application::github_credential_retrieve_cancelled{
						.Context = running,
					});
				break;
			case retrieve_terminal::error:
				fixture.Vault.Retrieves.front().Completion(
					application::github_credential_retrieve_error{
						.Context = running,
						.Failure = application::github_credential_vault_failure::
							operation_failed,
					});
				break;
			}
			fixture.Lane.run_next();
			expect(
				fixture.Vault.Deletes.size() == 1U
					&& fixture.Vault.Deletes.front().Context.Authority == authority
					&& !fixture.Host.is_quiescent(),
				"every retained retrieval terminal chains into one exact delete");

			fixture.Vault.Retrieves.front().Completion(
				application::github_credential_retrieve_cancelled{
					.Context = running,
				});
			fixture.Lane.run_next();
			expect(fixture.Vault.Deletes.size() == 1U
					   && !fixture.Host.is_quiescent()
					   && !fixture.Host.shutdown_cleanup_failed(),
				   "duplicate retrieval terminal cannot overtake required delete");

			const auto cleanup = fixture.Vault.Deletes.front().Context;
			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			fixture.Lane.run_next();
			// [[BPR-0545]]. That terminal issues the occasion's refresh
			// erasure, and quiescence waits for its answer too.
			fixture.Lane.run_next();
			// [[BPR-0545]]. The occasion's refresh erasure answers on this
			// lane too, and quiescence waits for its terminal.
			fixture.Lane.drain();
			expect(fixture.Host.is_quiescent()
					   && !fixture.Host.shutdown_cleanup_failed()
					   && bindings.size() == 0U,
				   "retained retrieval cleanup reaches exact deletion before "
				   "quiescence");
		}
	}

	auto test_shutdown_cancels_running_vault_work() -> void
	{
		enum class store_terminal {
			success,
			cancelled,
			error,
		};

		for (const auto terminal : {
				 store_terminal::success,
				 store_terminal::cancelled,
				 store_terminal::error,
			 }) {
			host_fixture fixture;
			advance_native_to_store(fixture);
			const auto running         = fixture.Vault.Stores.front().Context;
			const auto projectionCount = fixture.Projections.transition_count();

			fixture.Host.shutdown();
			expect(fixture.Vault.Cancellations == std::vector{ running }
					   && fixture.Vault.Deletes.empty()
					   && !fixture.Host.is_quiescent(),
				   "shutdown cancels a running store without early delete");

			switch (terminal) {
			case store_terminal::success:
				fixture.Vault.Stores.front().Completion(
					application::github_credential_store_success{
						.Context     = running,
						.Disposition = application::
							github_credential_store_disposition::stored,
					});
				break;
			case store_terminal::cancelled:
				fixture.Vault.Stores.front().Completion(
					application::github_credential_store_cancelled{
						.Context = running,
					});
				break;
			case store_terminal::error:
				fixture.Vault.Stores.front().Completion(
					application::github_credential_store_error{
						.Context = running,
						.Failure = application::github_credential_vault_failure::
							operation_failed,
					});
				break;
			}
			expect(fixture.Vault.Deletes.empty(),
				   "store terminal cannot clean up off the decision lane");
			fixture.Lane.run_next();
			expect(fixture.Vault.Deletes.size() == 1U
					   && fixture.Vault.Deletes.front().Context.Authority
						   == running.Authority
					   && fixture.Provider.Profiles.size() == 1U
					   && fixture.Projections.transition_count() == projectionCount
					   && !fixture.Host.is_quiescent(),
				   "every store terminal exact-cleans before shutdown quiesces");

			const auto cleanup = fixture.Vault.Deletes.front().Context;
			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			fixture.Lane.run_next();
			// [[BPR-0545]]. That terminal issues the occasion's refresh
			// erasure, and quiescence waits for its answer too.
			fixture.Lane.run_next();
			// [[BPR-0545]]. The occasion's refresh erasure answers on this
			// lane too, and quiescence waits for its terminal.
			fixture.Lane.drain();
			expect(fixture.Host.is_quiescent()
					   && fixture.Provider.Profiles.size() == 1U
					   && fixture.Projections.transition_count() == projectionCount,
				   "shutdown quiesces after provisional store cleanup terminates");
		}

		for (const auto terminal : {
				 store_terminal::success,
				 store_terminal::cancelled,
				 store_terminal::error,
			 }) {
			host_fixture fixture;
			advance_native_to_retrieve(fixture);
			const auto running         = fixture.Vault.Retrieves.front().Context;
			const auto projectionCount = fixture.Projections.transition_count();

			fixture.Host.shutdown();
			expect(fixture.Vault.Cancellations == std::vector{ running }
					   && fixture.Vault.Deletes.empty()
					   && !fixture.Host.is_quiescent(),
				   "shutdown cancels a running retrieve without early delete");

			switch (terminal) {
			case store_terminal::success:
				fixture.Vault.Retrieves.front().Completion(
					application::github_credential_retrieve_success{
						.Context    = running,
						.Credential = credential(),
					});
				break;
			case store_terminal::cancelled:
				fixture.Vault.Retrieves.front().Completion(
					application::github_credential_retrieve_cancelled{
						.Context = running,
					});
				break;
			case store_terminal::error:
				fixture.Vault.Retrieves.front().Completion(
					application::github_credential_retrieve_error{
						.Context = running,
						.Failure = application::github_credential_vault_failure::
							operation_failed,
					});
				break;
			}
			fixture.Lane.run_next();
			expect(
				fixture.Vault.Deletes.size() == 1U
					&& fixture.Vault.Deletes.front().Context.Authority
						== running.Authority
					&& fixture.Provider.Viewers.size() == 1U
					&& fixture.Projections.transition_count() == projectionCount
					&& !fixture.Host.is_quiescent(),
				"every retrieve terminal exact-cleans the provisional credential");

			const auto cleanup = fixture.Vault.Deletes.front().Context;
			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			fixture.Lane.run_next();
			// [[BPR-0545]]. The occasion's refresh erasure answers on this
			// lane too, and quiescence waits for its terminal.
			fixture.Lane.drain();
			expect(fixture.Host.is_quiescent()
					   && !fixture.Host.shutdown_cleanup_failed(),
				   "retrieve shutdown cleanup succeeds before quiescence");
		}
	}

	auto test_shutdown_exact_cleans_every_provisional_phase() -> void
	{
		{
			host_fixture fixture;
			advance_native_to_retrieve(fixture);
			const auto retrieve = fixture.Vault.Retrieves.front().Context;
			fixture.Vault.Retrieves.front().Completion(
				application::github_credential_retrieve_success{
					.Context    = retrieve,
					.Credential = credential(),
				});
			fixture.Lane.run_next();
			const auto viewerRequest = fixture.Provider.Viewers.back().Request;

			fixture.Host.shutdown();
			expect(
				fixture.Provider.CancelledViewers == std::vector{ viewerRequest }
					&& fixture.Vault.Deletes.size() == 1U
					&& !fixture.Host.is_quiescent(),
				"shutdown during account viewer starts exact provisional cleanup");
		}

		{
			host_fixture fixture;
			advance_native_to_retrieve(fixture);
			const auto retrieve = fixture.Vault.Retrieves.front().Context;
			fixture.Vault.Retrieves.front().Completion(
				application::github_credential_retrieve_success{
					.Context    = retrieve,
					.Credential = credential(),
				});
			fixture.Lane.run_next();
			const auto viewerRequest = fixture.Provider.Viewers.back().Request;
			fixture.Provider.Viewers.back().Completion(
				application::github_viewer_success{
					.Request    = viewerRequest,
					.Viewer     = viewer(),
					.RateLimits = rate_limits_for(
						application::github_rate_limit_resource::core),
					.AccountTransport = std::make_shared<fake_lease>(),
				});
			fixture.Lane.run_next();
			const auto capabilityRequest = fixture.Provider.Capabilities.front().Request;

			fixture.Host.shutdown();
			expect(fixture.Provider.CancelledCapabilities
						   == std::vector{ capabilityRequest }
					   && fixture.Vault.Deletes.size() == 1U
					   && !fixture.Host.is_quiescent(),
				   "shutdown during capability probe exact-cleans persisted "
				   "enrollment");

			const auto cleanup = fixture.Vault.Deletes.front().Context;
			auto       wrong   = cleanup;
			wrong.Request      = core::request_id{ 999U };
			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = wrong,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			fixture.Lane.run_next();
			// [[BPR-0545]]. The occasion's refresh erasure answers on this
			// lane too, and quiescence waits for its terminal.
			fixture.Lane.drain();
			expect(fixture.Host.is_quiescent()
					   && fixture.Host.shutdown_cleanup_failed()
					   && fixture.Bindings.size() == 1U,
				   "wrong cleanup terminal leaves a recoverable catalog tombstone");

			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			fixture.Lane.run_next();
			// [[BPR-0545]]. The occasion's refresh erasure answers on this
			// lane too, and quiescence waits for its terminal.
			fixture.Lane.drain();
			expect(fixture.Host.is_quiescent()
					   && fixture.Host.shutdown_cleanup_failed(),
				   "a second terminal cannot rewrite failed shutdown cleanup");
		}

		{
			host_fixture fixture;
			establish_native_session(fixture);
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 12U },
			});
			const auto cleanup = fixture.Vault.Deletes.front().Context;
			fixture.Host.shutdown();
			expect(fixture.Vault.Cancellations.empty()
					   && fixture.Vault.Deletes.size() == 1U
					   && !fixture.Host.is_quiescent(),
				   "shutdown preserves an exact delete already in flight");
			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_error{
					.Context = cleanup,
					.Failure = application::github_credential_vault_failure::
						operation_failed,
				});
			fixture.Lane.run_next();
			// [[BPR-0545]]. The occasion's refresh erasure answers on this
			// lane too, and quiescence waits for its terminal.
			fixture.Lane.drain();
			expect(
				fixture.Host.is_quiescent()
					&& fixture.Host.shutdown_cleanup_failed()
					&& fixture.Bindings.size() == 1U
					&& fixture.Projections.host_diagnostic_count() != 0U
					&& fixture.Projections.last_host_diagnostic().Reason
						== core::github_reason_code::vault_unavailable,
				"terminal cleanup failure is distinct from successful quiescence");
		}
	}

	auto test_shutdown_cancel_has_no_completion_gate_lock_inversion() -> void
	{
		host_fixture fixture;
		fixture.Host.dispatch(core::activate_host_profile_intent{
			.Profile = core::host_profile_id{ 11U },
		});
		fixture.Provider.CompleteProfileFromWorkerOnCancel = true;
		fixture.Host.shutdown();
		expect(fixture.Host.is_quiescent() && fixture.Lane.pending() == 1U,
			   "worker callback can hand off while cancel waits during shutdown");
		fixture.Lane.drain();
	}

	auto test_destroyed_host_suppresses_late_callbacks_before_posting() -> void
	{
		{
			fake_provider                                  provider;
			fake_vault                                     vault;
			application::manual_executor                   lane;
			recording_projections                          projections;
			application::github_credential_binding_catalog bindings;
			auto                                           host = std::make_unique<application::github_application_host>(
				initial_state(),
				provider,
				provider,
				provider,
				provider,
				vault,
				bindings,
				lane,
				projections);
			host->dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			const auto request = provider.Profiles.front().Request;
			host.reset();

			provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = request,
					.Authority = cli_profile(request.Context.Profile),
				});
			expect(lane.pending() == 0U && provider.Viewers.empty(),
				   "late provider terminal cannot post after host destruction");
		}

		{
			fake_provider                                  provider;
			fake_vault                                     vault;
			application::manual_executor                   lane;
			recording_projections                          projections;
			application::github_credential_binding_catalog bindings;
			auto                                           host = std::make_unique<application::github_application_host>(
				initial_state(),
				provider,
				provider,
				provider,
				provider,
				vault,
				bindings,
				lane,
				projections);
			host->enroll_profile(core::host_profile_id{ 11U }, credential());
			const auto profile = provider.Profiles.front().Request;
			provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = profile,
					.Authority = native_profile(profile.Context.Profile),
				});
			lane.run_next();
			const auto bootstrap = provider.Viewers.front().Request;
			provider.Viewers.front().Completion(application::github_viewer_success{
				.Request          = bootstrap,
				.Viewer           = viewer(),
				.RateLimits       = rate_limits_for(application::github_rate_limit_resource::core),
				.AccountTransport = { },
			});
			lane.run_next();
			const auto store = vault.Stores.front().Context;
			host.reset();

			vault.Stores.front().Completion(
				application::github_credential_store_success{
					.Context     = store,
					.Disposition = application::github_credential_store_disposition::stored,
				});
			expect(lane.pending() == 0U && vault.Deletes.empty()
					   && bindings.size() == 1U,
				   "late stored secret retains an exact catalog tombstone");
		}
	}

	// The restart signal, at the boundary that owns the vault. The stage a
	// device flow left behind does not survive a process, so the only thing
	// that can still say "this credential renews itself" is the entry the
	// vault kept beside it -- and only a device grant ever writes one. The
	// probe is issued on acceptance and its answer is published, because a
	// reader that already saw the acceptance would otherwise never be told.
	void test_accepted_credential_renewability_is_answered_by_the_vault()
	{
		{
			host_fixture pasted;
			pasted.Vault.RefreshAnswer = fake_vault::refresh_answer::absent;
			static_cast<void>(establish_native_session(pasted));
			expect(pasted.Host.accepted_credential_authority().has_value(),
				   "an accepted native session names a credential slot");
			expect(pasted.Vault.RefreshRetrieves.size() == 1U
					   && pasted.Vault.RefreshRetrieves.front().Authority
						   == *pasted.Host.accepted_credential_authority(),
				   "acceptance probes the refresh slot under the exact "
				   "accepted authority");
			const auto published = pasted.Projections.session_count();
			expect(pasted.Lane.pending() == 0U,
				   "an absent answer costs the decision lane no turn");
			expect(!pasted.Host.accepted_credential_renewable(),
				   "material with no refresh entry beside it is not renewable");
			expect(pasted.Projections.session_count() == published,
				   "an absent answer republishes nothing");
		}

		{
			host_fixture granted;
			granted.Vault.RefreshAnswer = fake_vault::refresh_answer::present;
			static_cast<void>(establish_native_session(
				granted,
				core::host_profile_id{ 11U },
				core::github_account_id{ 51U },
				std::nullopt,
				application::github_credential_provenance::device_grant));
			expect(!granted.Host.accepted_credential_renewable(),
				   "the answer is not read off lane");
			const auto published = granted.Projections.session_count();
			granted.Lane.run_next();
			expect(granted.Host.accepted_credential_renewable(),
				   "a refresh entry under the accepted authority is renewable");
			expect(granted.Projections.session_count() == published + 1U,
				   "the settled answer republishes the accepted session once");
			granted.Host.shutdown();
			expect(!granted.Host.accepted_credential_renewable(),
				   "a closed host renews nothing");
		}

		// [[BPR-0540]]. The defect this rule exists for. Both slots are keyed
		// by the same authority, so a token the reviewer pasted for an account
		// a past grant enrolled finds that grant's material under its own key.
		// Presence answers `yes` and provenance answers `no`, and the second
		// one decides: spending it would replace the session the reviewer
		// asked for with a device one they did not.
		{
			host_fixture orphan;
			orphan.Vault.RefreshAnswer = fake_vault::refresh_answer::present;
			static_cast<void>(establish_native_session(orphan));
			const auto published = orphan.Projections.session_count();
			expect(orphan.Vault.RefreshRetrieves.size() == 1U,
				   "a pasted acceptance still asks the vault the question");
			expect(orphan.Lane.pending() == 1U,
				   "the answer is carried onto the lane, where provenance is");
			orphan.Lane.run_next();
			expect(!orphan.Host.accepted_credential_renewable(),
				   "a pasted token beside a past grant's material never renews");
			expect(orphan.Projections.session_count() == published,
				   "nothing is republished, so nothing can arm a renewal");
		}
	}

	// [[BPR-0540]]. The refresh slot leaves with the credential it renews.
	// Every occasion that exact-deletes the base credential under an authority
	// is one occasion in the host, so the three the product has -- an orderly
	// shutdown under the delete policy, an invalidation, and an enrollment
	// superseding the session that stored the material -- are covered by the
	// one submission this checks for, under the same authority and through the
	// same port.
	// [[BPR-0545]]. The regression this card exists for. Every shipped vault
	// keys its pending operations by the exact
	// `github_credential_operation_context` and answers a duplicate
	// `invalid_request` before queuing, so an occasion that submitted the
	// slot's erasure and the credential's delete under one context lost the
	// second -- ordinarily the credential's own delete, because the main
	// thread wins the race to the second call. Holding the erasure pending is
	// what a platform vault does with a queued operation, and it is the window
	// the refusal lives in; with the two halves sequential there is no window.
	// [[BPR-0547]]. The one supersession that keeps the slot. A renewal ends
	// by storing the rotated refresh material under the authority the grant
	// established and then enrolling the rotated access token through the
	// device-grant enrollment boundary; the enrollment activates the profile,
	// and the activation's invalidate deletes the credential the rotation
	// replaced. Carrying the slot's erasure along with that delete destroys
	// the material stored a moment earlier and on this same lane, and the next
	// renewal retrieves an absent slot: renewal works exactly once. The
	// contrast is the block above -- the same session, superseded by a pasted
	// credential, still erases -- so what is checked here is the provenance of
	// the incoming enrollment deciding it, not the occasion being skipped.
	void test_a_rotation_keeps_the_material_it_just_stored()
	{
		host_fixture fixture;
		fixture.Vault.RefreshDeleteAnswer
			= fake_vault::refresh_delete_answer::deleted;
		static_cast<void>(establish_native_session(
			fixture,
			core::host_profile_id{ 11U },
			core::github_account_id{ 51U },
			std::nullopt,
			application::github_credential_provenance::device_grant));
		const auto authority = fixture.Vault.Stores.front().Context.Authority;
		expect(fixture.Vault.RefreshDeletes.empty(),
			   "an accepted grant erases nothing");

		fixture.Host.enroll_profile(
			core::host_profile_id{ 11U },
			credential(),
			application::github_credential_provenance::device_grant);
		expect(fixture.Vault.Deletes.size() == 1U
				   && fixture.Vault.Deletes.front().Context.Authority
					   == authority
				   && fixture.Vault.RefreshDeletes.empty(),
			   "the rotation's enrollment still deletes the access credential "
			   "it replaces");
		const auto superseded = fixture.Vault.Deletes.front().Context;
		fixture.Vault.Deletes.front().Completion(
			application::github_credential_delete_success{
				.Context     = superseded,
				.Disposition = application::
					github_credential_delete_disposition::deleted,
			});
		fixture.Lane.drain();
		expect(fixture.Vault.RefreshDeletes.empty(),
			   "a device grant superseding a device-grant session keeps the "
			   "refresh slot the rotation just filled");
		expect(fixture.Vault.Deletes.size() == 1U,
			   "and the occasion is the one delete, not a retry of it");
	}

	// [[BPR-0549]]. Drives a device grant superseding a device-grant session on
	// the same profile up to the point the retention window is open: the
	// outgoing credential has been deleted, its refresh slot deliberately has
	// not, and the incoming session has begun without yet naming an account.
	// Returns the authority the held slot is keyed under.
	[[nodiscard]] auto open_retention_window(host_fixture& aFixture)
		-> application::github_credential_authority_key
	{
		aFixture.Vault.RefreshDeleteAnswer
			= fake_vault::refresh_delete_answer::deleted;
		static_cast<void>(establish_native_session(
			aFixture,
			core::host_profile_id{ 11U },
			core::github_account_id{ 51U },
			std::nullopt,
			application::github_credential_provenance::device_grant));
		const auto authority = aFixture.Vault.Stores.front().Context.Authority;
		aFixture.Host.enroll_profile(
			core::host_profile_id{ 11U },
			credential(),
			application::github_credential_provenance::device_grant);
		expect(aFixture.Vault.Deletes.size() == 1U,
			   "the supersession deletes the access credential it replaces");
		const auto superseded = aFixture.Vault.Deletes.front().Context;
		aFixture.Vault.Deletes.front().Completion(
			application::github_credential_delete_success{
				.Context     = superseded,
				.Disposition = application::
					github_credential_delete_disposition::deleted,
			});
		aFixture.Lane.drain();
		expect(aFixture.Vault.RefreshDeletes.empty()
				   && aFixture.Provider.Profiles.size() == 2U,
			   "the slot is held while the incoming session can still turn out "
			   "to name the authority it is keyed under");
		return authority;
	}

	// Answers the incoming session's profile resolution, which is the last
	// step before the bootstrap `/user` that names its account.
	void answer_superseding_profile(host_fixture& aFixture)
	{
		const auto request = aFixture.Provider.Profiles.back().Request;
		aFixture.Provider.Profiles.back().Completion(
			application::github_profile_success{
				.Request   = request,
				.Authority = native_profile(request.Context.Profile),
			});
		aFixture.Lane.drain();
		expect(aFixture.Provider.Viewers.size() == 3U
				   && aFixture.Vault.RefreshDeletes.empty(),
			   "a resolved profile does not yet say which account is arriving");
	}

	void answer_superseding_bootstrap(host_fixture&                 aFixture,
									  const core::github_account_id aAccount)
	{
		const auto request = aFixture.Provider.Viewers.back().Request;
		aFixture.Provider.Viewers.back().Completion(
			application::github_viewer_success{
				.Request    = request,
				.Viewer     = viewer(aAccount),
				.RateLimits = rate_limits_for(
					application::github_rate_limit_resource::core),
				.AccountTransport = { },
			});
		aFixture.Lane.drain();
	}

	// [[BPR-0549]]. The window closes on a different account, and the slot the
	// outgoing session filled leaves under the outgoing authority. The
	// retention [[BPR-0547]] introduced is decided at the invalidate, where the
	// incoming enrollment's provenance and profile are known and its account is
	// not; a device grant for another account on the same profile satisfies
	// every one of those conditions, so before this the slot stayed behind
	// keyed to an account the new session never names -- the orphan
	// [[BPR-0540]] closed, reached through a narrower door, and six months of
	// refresh material left in the platform vault after the reviewer signed in
	// as somebody else.
	void test_a_retained_slot_leaves_when_another_account_arrives()
	{
		host_fixture fixture;
		const auto   retained = open_retention_window(fixture);
		answer_superseding_profile(fixture);
		answer_superseding_bootstrap(fixture, core::github_account_id{ 77U });
		expect(fixture.Vault.RefreshDeletes.size() == 1U
				   && fixture.Vault.RefreshDeletes.front().Authority == retained,
			   "a grant naming another account erases the outgoing slot under "
			   "the outgoing authority");
	}

	// [[BPR-0549]]. The window closes on the same account without erasing, and
	// the ordinary occasion afterwards is still the only erasure. This is the
	// retention itself, so it holds before this change as well: it is here to
	// keep the narrowing from becoming "erase whenever the window closes",
	// which would put the renewal chain back to working exactly once.
	void test_a_retained_slot_stays_when_the_same_account_arrives()
	{
		host_fixture fixture;
		const auto   retained = open_retention_window(fixture);
		answer_superseding_profile(fixture);
		answer_superseding_bootstrap(fixture, core::github_account_id{ 51U });
		expect(fixture.Vault.RefreshDeletes.empty()
				   && fixture.Vault.Stores.size() == 2U
				   && fixture.Vault.Stores.back().Context.Authority == retained,
			   "the rotation's own account keeps the slot it just filled");

		const auto storeContext = fixture.Vault.Stores.back().Context;
		fixture.Vault.Stores.back().Completion(
			application::github_credential_store_success{
				.Context     = storeContext,
				.Disposition = application::
					github_credential_store_disposition::stored,
			});
		fixture.Lane.drain();
		const auto retrieveContext = fixture.Vault.Retrieves.back().Context;
		fixture.Vault.Retrieves.back().Completion(
			application::github_credential_retrieve_success{
				.Context    = retrieveContext,
				.Credential = credential(),
			});
		fixture.Lane.drain();
		const auto accountRequest = fixture.Provider.Viewers.back().Request;
		const auto lease          = std::make_shared<fake_lease>();
		fixture.Provider.Viewers.back().Completion(
			application::github_viewer_success{
				.Request    = accountRequest,
				.Viewer     = viewer(core::github_account_id{ 51U }),
				.RateLimits = rate_limits_for(
					application::github_rate_limit_resource::core),
				.AccountTransport = lease,
			});
		fixture.Lane.drain();
		const auto capabilityRequest
			= fixture.Provider.Capabilities.back().Request;
		fixture.Provider.Capabilities.back().Completion(
			application::github_capability_success{
				.Request     = capabilityRequest,
				.Facts       = capability_facts(),
				.SchemaFacts = schema_facts(),
				.RateLimits  = rate_limits_for(
					application::github_rate_limit_resource::graphql),
			});
		fixture.Lane.drain();
		expect(fixture.Host.accepted_session().has_value()
				   && fixture.Vault.RefreshDeletes.empty(),
			   "the renewed session is established over the slot it kept");

		fixture.Host.enroll_profile(
			core::host_profile_id{ 11U },
			credential(),
			application::github_credential_provenance::manual);
		const auto signedOut = fixture.Vault.Deletes.back().Context;
		fixture.Vault.Deletes.back().Completion(
			application::github_credential_delete_success{
				.Context     = signedOut,
				.Disposition = application::
					github_credential_delete_disposition::deleted,
			});
		fixture.Lane.drain();
		expect(fixture.Vault.RefreshDeletes.size() == 1U
				   && fixture.Vault.RefreshDeletes.front().Authority == retained,
			   "and the ordinary occasion afterwards is the one erasure");
	}

	// [[BPR-0549]]. The window is not a way to leak material by failing at the
	// right moment. A session that never reaches an authority and a shutdown
	// that arrives first both mean nobody is coming to spend the held slot, so
	// each erases it -- the shutdown inside its own quiescence accounting,
	// which the outstanding-erasure count already holds composition open for.
	void test_a_retained_slot_does_not_survive_a_failure_or_a_shutdown()
	{
		{
			host_fixture fixture;
			const auto   retained = open_retention_window(fixture);
			const auto   request  = fixture.Provider.Profiles.back().Request;
			fixture.Provider.Profiles.back().Completion(
				application::github_profile_error{
					.Request = request,
					.Error   = {
						.Failure           = application::github_provider_failure::permission,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					},
				});
			fixture.Lane.drain();
			expect(fixture.Vault.RefreshDeletes.size() == 1U
					   && fixture.Vault.RefreshDeletes.front().Authority
						   == retained,
				   "a session that fails inside the window erases the slot it "
				   "was being held for");
		}

		{
			host_fixture fixture;
			const auto   retained = open_retention_window(fixture);
			fixture.Host.shutdown();
			fixture.Lane.drain();
			expect(fixture.Vault.RefreshDeletes.size() == 1U
					   && fixture.Vault.RefreshDeletes.front().Authority
						   == retained,
				   "and a shutdown inside the window erases it rather than "
				   "leaving it in the vault");
		}
	}

	void test_an_occasion_never_has_two_operations_pending_under_one_context()
	{
		constexpr auto policy = application::github_credential_shutdown_policy::
			delete_retained_on_shutdown;
		host_fixture fixture{ policy };
		fixture.Vault.HoldRefreshDelete = true;
		fixture.Vault.RefreshDeleteAnswer
			= fake_vault::refresh_delete_answer::deleted;
		static_cast<void>(establish_native_session(
			fixture,
			core::host_profile_id{ 11U },
			core::github_account_id{ 51U },
			std::nullopt,
			application::github_credential_provenance::device_grant));

		fixture.Host.shutdown();
		expect(fixture.Vault.Deletes.size() == 1U
				   && fixture.Vault.RefreshDeletes.empty(),
			   "the credential's own delete is the occasion's first operation, "
			   "and no duplicate context refuses it");
		const auto occasion = fixture.Vault.Deletes.front().Context;
		fixture.Vault.Deletes.front().Completion(
			application::github_credential_delete_success{
				.Context     = occasion,
				.Disposition = application::
					github_credential_delete_disposition::deleted,
			});
		fixture.Lane.run_next();
		expect(fixture.Vault.RefreshDeletes.size() == 1U
				   && fixture.Vault.RefreshDeletes.front() == occasion
				   && !fixture.Host.is_quiescent(),
			   "the slot's erasure follows under the same context, which the "
			   "vault released when it began the first");
		fixture.Vault.answer_held_refresh_delete();
		expect(!fixture.Host.is_quiescent(),
			   "the count it holds is released on the lane, not beside the "
			   "vault's answer");
		fixture.Lane.run_next();
		// [[BPR-0545]]. The occasion's refresh erasure answers on this
		// lane too, and quiescence waits for its terminal.
		fixture.Lane.drain();
		expect(fixture.Host.is_quiescent()
				   && !fixture.Host.shutdown_cleanup_failed()
				   && fixture.Bindings.size() == 0U,
			   "an orderly shutdown deletes both slots and reports no cleanup "
			   "failure");
	}

	void test_refresh_material_shares_the_credential_erasure_path()
	{
		constexpr auto policy = application::github_credential_shutdown_policy::
			delete_retained_on_shutdown;
		{
			host_fixture fixture{ policy };
			fixture.Vault.RefreshDeleteAnswer
				= fake_vault::refresh_delete_answer::deleted;
			static_cast<void>(establish_native_session(
				fixture,
				core::host_profile_id{ 11U },
				core::github_account_id{ 51U },
				std::nullopt,
				application::github_credential_provenance::device_grant));
			const auto authority = fixture.Vault.Stores.front().Context.Authority;
			fixture.Host.shutdown();
			expect(fixture.Vault.Deletes.size() == 1U
					   && fixture.Vault.RefreshDeletes.empty(),
				   "the occasion opens with the credential's own delete, alone "
				   "under its context ([[BPR-0545]])");
			const auto cleanup = fixture.Vault.Deletes.front().Context;
			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::
						github_credential_delete_disposition::deleted,
				});
			fixture.Lane.run_next();
			expect(fixture.Vault.RefreshDeletes.size() == 1U
					   && fixture.Vault.RefreshDeletes.front().Authority
						   == authority
					   && fixture.Vault.RefreshDeletes.front() == cleanup,
				   "shutdown after a grant erases both slots under one "
				   "authority");
			fixture.Lane.run_next();
			expect(!fixture.Host.shutdown_cleanup_failed()
					   && fixture.Host.is_quiescent()
					   && fixture.Lane.pending() == 0U,
				   "an answered erasure reports nothing and leaves the cleanup "
				   "exact");
		}

		// The superseding enrollment. A second sign-in for a profile the host
		// already holds a binding for deletes what it holds before it
		// establishes anything, and the material that renewed the old session
		// goes with it.
		{
			host_fixture fixture;
			fixture.Vault.RefreshDeleteAnswer
				= fake_vault::refresh_delete_answer::deleted;
			static_cast<void>(establish_native_session(
				fixture,
				core::host_profile_id{ 11U },
				core::github_account_id{ 51U },
				std::nullopt,
				application::github_credential_provenance::device_grant));
			const auto authority = fixture.Vault.Stores.front().Context.Authority;
			expect(fixture.Vault.RefreshDeletes.empty(),
				   "an accepted session erases nothing");
			fixture.Host.enroll_profile(core::host_profile_id{ 11U },
										credential());
			expect(fixture.Vault.Deletes.size() == 1U
					   && fixture.Vault.RefreshDeletes.empty(),
				   "the superseding enrollment deletes the credential first");
			const auto superseded = fixture.Vault.Deletes.front().Context;
			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = superseded,
					.Disposition = application::
						github_credential_delete_disposition::deleted,
				});
			fixture.Lane.drain();
			expect(fixture.Vault.RefreshDeletes.size() == 1U
					   && fixture.Vault.RefreshDeletes.front().Authority
						   == authority
					   && fixture.Vault.RefreshDeletes.front() == superseded,
				   "a superseding enrollment erases the refresh slot it "
				   "supersedes");
		}

		// A failure is reported, never silent: material this host meant to
		// destroy is still in the vault under an authority the reviewer has
		// finished with, and an orderly shutdown that ends that way did not
		// clean up.
		{
			host_fixture fixture{ policy };
			fixture.Vault.RefreshDeleteAnswer
				= fake_vault::refresh_delete_answer::failed;
			static_cast<void>(establish_native_session(
				fixture,
				core::host_profile_id{ 11U },
				core::github_account_id{ 51U },
				std::nullopt,
				application::github_credential_provenance::device_grant));
			const auto diagnostics = fixture.Projections.host_diagnostic_count();
			fixture.Host.shutdown();
			const auto ending = fixture.Vault.Deletes.front().Context;
			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = ending,
					.Disposition = application::
						github_credential_delete_disposition::deleted,
				});
			fixture.Lane.run_next();
			expect(fixture.Vault.RefreshDeletes.size() == 1U
					   && fixture.Lane.pending() == 1U
					   && !fixture.Host.is_quiescent()
					   && !fixture.Host.shutdown_cleanup_failed(),
				   "a failed erasure carries its one answer onto the lane, and "
				   "the count it holds keeps quiescence from being observed "
				   "before the report ([[BPR-0545]] R1)");
			fixture.Lane.run_next();
			// [[BPR-0545]]. The occasion's refresh erasure answers on this
			// lane too, and quiescence waits for its terminal.
			fixture.Lane.drain();
			expect(fixture.Host.shutdown_cleanup_failed()
					   && fixture.Projections.host_diagnostic_count() > diagnostics
					   && fixture.Host.is_quiescent(),
				   "refresh material left in the vault fails shutdown cleanup, "
				   "and quiescence follows the report rather than racing it");
		}

		// The store that could not be reached. Every shipped platform has a
		// real second slot, so `unavailable` is not "nothing was ever written
		// there": it is a delete the vault did not perform, and it fails the
		// cleanup exactly as it would for the base slot.
		{
			host_fixture fixture{ policy };
			fixture.Vault.RefreshDeleteAnswer
				= fake_vault::refresh_delete_answer::unavailable;
			static_cast<void>(establish_native_session(fixture));
			fixture.Host.shutdown();
			const auto unreachable = fixture.Vault.Deletes.front().Context;
			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = unreachable,
					.Disposition = application::
						github_credential_delete_disposition::deleted,
				});
			fixture.Lane.run_next();
			expect(fixture.Vault.RefreshDeletes.size() == 1U
					   && fixture.Lane.pending() == 1U,
				   "an unreachable refresh store carries its answer onto the lane");
			fixture.Lane.run_next();
			// [[BPR-0545]]. The occasion's refresh erasure answers on this
			// lane too, and quiescence waits for its terminal.
			fixture.Lane.drain();
			expect(fixture.Host.shutdown_cleanup_failed()
					   && fixture.Host.is_quiescent(),
				   "an unreachable refresh store fails shutdown cleanup");
		}
	}

	class binding_store final : public application::device_binding_store_port
	{
	public:
		std::vector<application::device_binding_record> Rows;
		std::vector<std::string>                        Operations;
		bool                                            RefuseStore = false;
		bool                                            RefuseErase = false;
		bool                                            RefuseLoad  = false;
		auto                                            load(std::stop_token aStop) -> application::device_binding_load_result override
		{
			Operations.emplace_back("load");
			if (aStop.stop_requested()) {
				return std::unexpected(application::device_binding_failure::cancelled);
			}
			if (RefuseLoad) {
				return std::unexpected(application::device_binding_failure::unavailable);
			}
			return Rows;
		}
		auto store(const application::device_binding_record& aRecord, std::stop_token aStop) -> application::device_binding_result override
		{
			Operations.emplace_back("store");
			if (RefuseStore || aStop.stop_requested()) {
				return std::unexpected(application::device_binding_failure::unavailable);
			}
			Rows = { aRecord };
			return { };
		}
		auto erase(const application::github_credential_authority_key& aAuthority, std::stop_token) -> application::device_binding_result override
		{
			Operations.emplace_back("erase");
			if (RefuseErase) {
				return std::unexpected(application::device_binding_failure::unavailable);
			}
			for (auto& row : Rows) {
				if (row.Authority == aAuthority) {
					row.Tombstone = true;
				}
			}
			return { };
		}
	};
	auto binding_lifetime() -> application::device_binding_lifetime { return { .IssuedAt = 1000, .AccessExpiresAt = 4600, .RefreshExpiresAt = 10000 }; }
	void settle_grant(host_fixture& aFixture)
	{
		establish_native_session(aFixture, core::host_profile_id{ 11U }, core::github_account_id{ 51U }, std::nullopt, application::github_credential_provenance::device_grant, binding_lifetime());
		aFixture.Host.settle_device_binding(*aFixture.Host.accepted_credential_authority(), aFixture.Host.accepted_identity_generation(), binding_lifetime());
	}
	void test_device_binding_persistence_barriers()
	{
		{
			host_fixture                            fixture{ application::github_credential_shutdown_policy::retain_accepted_device_grants };
			binding_store                           store;
			application::manual_executor            effects;
			application::device_binding_persistence persistence{ store, effects, fixture.Lane };
			fixture.Host.configure_device_persistence(persistence, [] { return std::int64_t{ 1000 }; });
			settle_grant(fixture);
			expect(store.Operations.empty() && !fixture.Host.accepted_device_binding_durable(), "metadata waits for effect lane and cannot advertise uncommitted grant");
			effects.drain();
			expect(!fixture.Host.accepted_device_binding_durable(), "effect terminal cannot mutate decision state");
			fixture.Lane.drain();
			expect(fixture.Host.accepted_device_binding_durable(), "settled exact pair becomes durable after decision terminal");
			// [[BPR-0660]]. The grant facts reach the session projection with no
			// credential: the kind, the issue instant, renewal and durability.
			{
				const auto& updates = fixture.Projections.Updates;
				expect(!updates.empty() && updates.back().Session.has_value() && updates.back().Session->Grant.has_value()
						   && updates.back().Session->Grant->Kind == application::github_grant_kind_projection::device_grant
						   && updates.back().Session->Grant->SignedInAt == std::optional<std::int64_t>{ 1000 }
						   && updates.back().Session->Grant->Renews && updates.back().Session->Grant->Remembered,
					   "a durable device grant projects its kind, issue instant, renewal and durability");
			}
			fixture.Host.shutdown();
			fixture.Lane.drain();
			expect(fixture.Vault.Deletes.empty() && fixture.Host.is_quiescent(), "new shutdown policy retains accepted durable device grant");
		}
		{
			host_fixture                            fixture;
			binding_store                           store;
			application::manual_executor            effects;
			application::device_binding_persistence persistence{ store, effects, fixture.Lane };
			fixture.Host.configure_device_persistence(persistence, [] { return std::int64_t{ 1000 }; });
			settle_grant(fixture);
			effects.drain();
			fixture.Lane.drain();
			fixture.Host.enroll_profile(core::host_profile_id{ 11U }, credential());
			expect(fixture.Vault.Deletes.empty(), "manual replacement waits for durable tombstone before erasing access");
			effects.drain();
			expect(store.Rows.front().Tombstone && fixture.Vault.Deletes.empty(), "tombstone commits before vault work and marshals terminal");
			fixture.Lane.drain();
			expect(fixture.Vault.Deletes.size() == 1U, "successful tombstone releases ordered access erasure");
			fixture.Host.shutdown();
			const auto deletion = fixture.Vault.Deletes.front();
			deletion.Completion(application::github_credential_delete_success{ .Context = deletion.Context, .Disposition = application::github_credential_delete_disposition::deleted });
			fixture.Lane.drain();
			expect(fixture.Host.is_quiescent(), "shutdown drains outstanding replacement cleanup");
		}
		{
			host_fixture                            fixture;
			binding_store                           store;
			application::manual_executor            effects;
			application::device_binding_persistence persistence{ store, effects, fixture.Lane };
			fixture.Host.configure_device_persistence(persistence, [] { return std::int64_t{ 1000 }; });
			settle_grant(fixture);
			effects.drain();
			fixture.Lane.drain();
			store.RefuseErase = true;
			fixture.Host.enroll_profile(core::host_profile_id{ 11U }, credential());
			effects.drain();
			fixture.Lane.drain();
			expect(fixture.Vault.Deletes.empty() && fixture.Provider.Profiles.size() == 1U, "failed tombstone prevents replacement of either slot");
			fixture.Host.shutdown();
			effects.drain();
			fixture.Lane.drain();
		}
		{
			host_fixture                            fixture{ application::github_credential_shutdown_policy::retain_accepted_device_grants };
			binding_store                           store;
			application::manual_executor            effects;
			application::device_binding_persistence persistence{ store, effects, fixture.Lane };
			fixture.Host.configure_device_persistence(persistence, [] { return std::int64_t{ 1000 }; });
			settle_grant(fixture);
			fixture.Host.shutdown();
			expect(!fixture.Host.is_quiescent(), "pending metadata prevents shutdown quiescence");
			effects.drain();
			fixture.Lane.drain();
			effects.drain();
			fixture.Lane.drain();
			expect(!fixture.Host.accepted_device_binding_durable() && fixture.Vault.Deletes.size() == 1U, "cancelled save cannot retain or advertise a device grant");
			const auto deletion = fixture.Vault.Deletes.front();
			deletion.Completion(application::github_credential_delete_success{ .Context = deletion.Context, .Disposition = application::github_credential_delete_disposition::deleted });
			fixture.Lane.drain();
			expect(fixture.Host.is_quiescent(), "cancelled save and protected tombstone fully drain");
		}
	}

	// [[BPR-0884]]. An executor that can refuse a post. A refusal retains
	// nothing, as both executor contracts require of a post that throws.
	class refusable_executor final : public application::decision_executor, public application::effect_executor
	{
	public:
		void post(std::function<void()> aTask) override
		{
			if (Refuse) {
				auto refusal = std::runtime_error{ "refused post" };
				// [[BPR-1112]]. The allocation fault starts counting once this
				// refusal is certain, so every failure it injects comes after it.
				if (ArmAfterRefusal >= 0) {
					allocation_fault::arm(std::exchange(ArmAfterRefusal, -1));
				}
				throw refusal;
			}
			Inner.post(std::move(aTask));
		}
		application::manual_executor Inner;
		bool                         Refuse          = false;
		long long                    ArmAfterRefusal = -1;
	};
	// A refused effect post leaves the job to report `unavailable` through the
	// decision lane. When that post is refused too, the lane must stay drainable.
	// [[BPR-1111]]. Every request's terminals are counted: an admitted request
	// gets exactly one, and a withdrawn one none.
	void test_device_binding_persistence_stays_drainable_when_its_posts_are_refused()
	{
		using result = application::device_binding_load_result;
		struct terminals
		{
			std::size_t           Count = 0U;
			std::optional<result> Last;
			[[nodiscard]] auto    once() const -> bool { return Count == 1U && Last.has_value(); }
			[[nodiscard]] auto    once_with(const application::device_binding_failure aFailure) const -> bool { return once() && !Last->has_value() && Last->error() == aFailure; }
		};
		binding_store                           store;
		refusable_executor                      effects;
		refusable_executor                      lane;
		application::device_binding_persistence persistence{ store, effects, lane };
		const auto                              refuse = [&](const bool aRefuse) {
			effects.Refuse = aRefuse;
			lane.Refuse    = aRefuse;
		};
		const auto refused = [&persistence](terminals& aTerminals) {
			try {
				persistence.load([&aTerminals](result aLoaded) {
					++aTerminals.Count;
					aTerminals.Last = std::move(aLoaded);
				});
			} catch (const std::runtime_error&) {
				return true;
			}
			return false;
		};

		// Admission: neither the effect nor its failure terminal can be posted.
		terminals withdrawn;
		refuse(true);
		expect(refused(withdrawn) && persistence.is_quiescent(), "a request whose effect and terminal are both refused is withdrawn, and the caller sees the refusal");
		refuse(false);
		terminals next;
		expect(!refused(next), "the next request is admitted");
		effects.Inner.drain();
		lane.Inner.drain();
		expect(withdrawn.Count == 0U && next.once() && next.Last->has_value() && persistence.is_quiescent(), "a withdrawn request has no terminal, and the next one has exactly one");

		// A queued successor: the lane turn that finishes its predecessor can
		// post neither the successor's effect nor its terminal.
		terminals first;
		terminals second;
		expect(!refused(first) && !refused(second), "two requests are queued");
		effects.Inner.drain();
		refuse(true);
		auto escaped = false;
		try {
			lane.Inner.drain();
		} catch (...) {
			escaped = true;
		}
		expect(!escaped && first.once() && first.Last->has_value() && second.once_with(application::device_binding_failure::unavailable),
			   "the successor reports unavailable exactly once in the same lane turn, after its predecessor's one terminal");
		expect(persistence.is_quiescent() && std::ranges::count(store.Operations, "load") == 2, "the successor never reached the store, and the lane is quiescent");
		refuse(false);
		terminals after;
		expect(!refused(after), "a request after the stranded successor is admitted");
		effects.Inner.drain();
		lane.Inner.drain();
		expect(after.once() && after.Last->has_value() && persistence.is_quiescent(), "the lane serves the request after the stranded successor, once");
		expect(first.Count == 1U && second.Count == 1U && next.Count == 1U && withdrawn.Count == 0U, "no earlier request gets a second terminal later");

		// A closing refusal whose own terminal cannot be posted is not left pending.
		persistence.cancel_pending();
		lane.Refuse = true;
		terminals closing;
		expect(refused(closing) && closing.Count == 0U && persistence.is_quiescent(), "a refusal whose terminal is refused leaves nothing pending and no terminal");
	}

	// [[BPR-1048]]. A host whose persistence lane can be made to withdraw a
	// request. With both executors refusing, the lane posts neither the
	// request's effect nor its terminal, so it withdraws the request and
	// throws, and no terminal follows ([[BPR-0884]]). Every host call site
	// counts the request in `OutstandingBindings`, and some set a pending
	// flag, before it calls the lane.
	class withdrawing_binding_lane
	{
	public:
		explicit withdrawing_binding_lane(const std::int64_t aNow = 1000)
			: Fixture{ application::github_credential_shutdown_policy::retain_accepted_device_grants }
		{
			Fixture.Host.configure_device_persistence(Persistence, [aNow] { return aNow; });
		}
		// Runs `aCall` while the lane withdraws every request, and reports
		// whether a throw reached the caller.
		template <typename TCall>
		[[nodiscard]] auto escapes_while_withdrawing(TCall&& aCall) -> bool
		{
			Effects.Refuse = true;
			Lane.Refuse    = true;
			auto escaped   = false;
			try {
				std::forward<TCall>(aCall)();
			} catch (...) {
				escaped = true;
			}
			Effects.Refuse = false;
			Lane.Refuse    = false;
			return escaped;
		}
		// Runs every queue, and answers every vault deletion, until nothing is left.
		void settle()
		{
			while (Effects.Inner.pending() != 0U || Lane.Inner.pending() != 0U || Fixture.Lane.pending() != 0U || Answered < Fixture.Vault.Deletes.size()) {
				Effects.Inner.drain();
				Lane.Inner.drain();
				Fixture.Lane.drain();
				answer_deletes();
			}
		}
		// The failure a refused binding request publishes.
		[[nodiscard]] auto failed_visibly(const std::size_t aDiagnosticsBefore) const -> bool
		{
			return Fixture.Projections.host_diagnostic_count() > aDiagnosticsBefore
				&& Fixture.Projections.last_host_diagnostic().Reason == core::github_reason_code::vault_unavailable;
		}
		[[nodiscard]] auto tombstoned() const -> bool { return !Store.Rows.empty() && Store.Rows.front().Tombstone; }
		// Signs out, then asks for a restore. True when the restore reads the
		// store, rather than being answered `unavailable` at admission because
		// a binding request is still counted.
		[[nodiscard]] auto later_restore_reaches_the_store() -> bool
		{
			Fixture.Host.dispatch(core::deactivate_github_identity_intent{ });
			settle();
			const auto loads    = std::ranges::count(Store.Operations, "load");
			const auto results  = std::make_shared<std::vector<application::device_profile_restore_status>>();
			const auto admitted = Fixture.Host.restore_device_profile(native_profile(core::host_profile_id{ 11U }), [results](const application::device_profile_restore_status aStatus) { results->push_back(aStatus); });
			settle();
			return admitted && std::ranges::count(Store.Operations, "load") == loads + 1 && results->size() == 1U;
		}
		[[nodiscard]] auto quiescent_once_closed() -> bool
		{
			Fixture.Host.shutdown();
			settle();
			return Fixture.Host.is_quiescent();
		}

		host_fixture                            Fixture;
		binding_store                           Store;
		refusable_executor                      Effects;
		refusable_executor                      Lane;
		application::device_binding_persistence Persistence{ Store, Effects, Lane };

	private:
		void answer_deletes()
		{
			for (; Answered < Fixture.Vault.Deletes.size(); ++Answered) {
				const auto deletion = Fixture.Vault.Deletes.at(Answered);
				deletion.Completion(application::github_credential_delete_success{ .Context = deletion.Context, .Disposition = application::github_credential_delete_disposition::deleted });
			}
		}
		std::size_t Answered = 0U;
	};

	// `persist_settled_device_binding`: a withdrawn store is a failed store.
	void test_a_withdrawn_binding_store_fails_like_a_refused_one()
	{
		withdrawing_binding_lane course;
		auto&                    host = course.Fixture.Host;
		establish_native_session(course.Fixture, core::host_profile_id{ 11U }, core::github_account_id{ 51U }, std::nullopt, application::github_credential_provenance::device_grant, binding_lifetime());
		const auto authority   = *host.accepted_credential_authority();
		const auto generation  = host.accepted_identity_generation();
		const auto diagnostics = course.Fixture.Projections.host_diagnostic_count();
		const auto escaped     = course.escapes_while_withdrawing([&] { host.settle_device_binding(authority, generation, binding_lifetime()); });
		course.settle();
		expect(!escaped, "store: the host handles a withdrawn store, so its caller sees no throw");
		expect(course.failed_visibly(diagnostics) && course.Store.Operations.empty() && !host.accepted_device_binding_durable(),
			   "store: a withdrawn store fails visibly, and nothing is written or remembered");
		host.settle_device_binding(authority, generation, binding_lifetime());
		course.settle();
		expect(course.Store.Operations == std::vector<std::string>{ "store" } && host.accepted_device_binding_durable(), "store: the pending flag does not block the next store");
		expect(course.later_restore_reaches_the_store(), "store: a later restore is not refused by a leftover count");
		expect(course.quiescent_once_closed(), "store: the host is quiescent once closed");
	}

	// `start_delete`: a withdrawn tombstone takes the refused tombstone's path.
	void test_a_withdrawn_binding_tombstone_fails_like_a_refused_one()
	{
		// A replacement is waiting on the tombstone. As with a refused one, the
		// replacement fails and neither vault slot is touched.
		{
			withdrawing_binding_lane course;
			auto&                    host = course.Fixture.Host;
			settle_grant(course.Fixture);
			course.settle();
			const auto authority   = *host.accepted_credential_authority();
			const auto diagnostics = course.Fixture.Projections.host_diagnostic_count();
			const auto escaped     = course.escapes_while_withdrawing([&] { host.enroll_profile(core::host_profile_id{ 11U }, credential()); });
			course.settle();
			expect(!escaped, "replacement: the host handles a withdrawn tombstone, so its caller sees no throw");
			expect(course.failed_visibly(diagnostics) && !course.tombstoned() && course.Fixture.Vault.Deletes.empty() && course.Fixture.Provider.Profiles.size() == 1U && !host.accepted_session(),
				   "replacement: a withdrawn tombstone fails the replacement, and neither slot is touched");
			host.dispatch(core::deactivate_github_identity_intent{ });
			course.settle();
			expect(std::ranges::count(course.Store.Operations, "erase") == 1 && course.tombstoned() && course.Fixture.Vault.Deletes.size() == 1U && course.Fixture.Vault.Deletes.front().Context.Authority == authority,
				   "replacement: the pending flag does not block the next tombstone, and Sign out then erases the credential");
			expect(course.later_restore_reaches_the_store(), "replacement: a later restore is not refused by a leftover count");
			expect(course.quiescent_once_closed(), "replacement: the host is quiescent once closed");
		}
		// Sign out, with nothing waiting. As with a refused tombstone, the
		// credential that was signed out is still erased.
		{
			withdrawing_binding_lane course;
			auto&                    host = course.Fixture.Host;
			settle_grant(course.Fixture);
			course.settle();
			const auto authority   = *host.accepted_credential_authority();
			const auto diagnostics = course.Fixture.Projections.host_diagnostic_count();
			const auto escaped     = course.escapes_while_withdrawing([&] { host.dispatch(core::deactivate_github_identity_intent{ }); });
			course.settle();
			expect(!escaped, "sign out: the host handles a withdrawn tombstone, so its caller sees no throw");
			expect(course.failed_visibly(diagnostics) && !course.tombstoned() && course.Fixture.Vault.Deletes.size() == 1U && course.Fixture.Vault.Deletes.front().Context.Authority == authority,
				   "sign out: a withdrawn tombstone fails visibly, and the signed-out credential is still erased");
			expect(course.later_restore_reaches_the_store(), "sign out: a later restore is not refused by a leftover count");
			expect(course.quiescent_once_closed(), "sign out: the host is quiescent once closed");
		}
	}

	// [[BPR-1116]]. `prepare_device_rotation` for an accepted grant writes no
	// tombstone. It answers once, changes nothing and calls no lane, so even a
	// lane that withdraws every request cannot make it fail or leave anything
	// counted ([[BPR-1112]]).
	void test_the_rotation_barrier_writes_nothing()
	{
		withdrawing_binding_lane course;
		auto&                    host = course.Fixture.Host;
		settle_grant(course.Fixture);
		course.settle();
		const auto        authority   = *host.accepted_credential_authority();
		const auto        generation  = host.accepted_identity_generation();
		const auto        diagnostics = course.Fixture.Projections.host_diagnostic_count();
		const auto        operations  = course.Store.Operations;
		std::vector<bool> answers;
		const auto        rotate  = [&](const application::github_credential_authority_key& aAuthority, const core::github_identity_generation aGeneration) { host.prepare_device_rotation(aAuthority, aGeneration, [&answers](const bool aReady) { answers.push_back(aReady); }); };
		const auto        escaped = course.escapes_while_withdrawing([&] { rotate(authority, generation); });
		expect(!escaped && answers == std::vector{ true }, "rotation: the barrier answers true exactly once, at once, while the lane withdraws everything");
		course.settle();
		expect(course.Store.Operations == operations && !course.tombstoned() && host.accepted_device_binding_durable() && course.Fixture.Projections.host_diagnostic_count() == diagnostics,
			   "rotation: the barrier writes no tombstone, keeps the grant durable, and publishes nothing");
		rotate(authority, generation);
		auto foreign    = authority;
		foreign.Account = core::github_account_id{ authority.Account.value() + 1U };
		rotate(foreign, generation);
		rotate(authority, core::github_identity_generation{ generation.value() + 1U });
		course.settle();
		expect(answers == std::vector{ true, true, false, false } && course.Store.Operations == operations && !course.tombstoned(),
			   "rotation: the next barrier writes nothing either, and a foreign account or a stale generation is refused");
		expect(course.later_restore_reaches_the_store(), "rotation: a later restore is not refused by a leftover count");
		expect(course.quiescent_once_closed(), "rotation: the host is quiescent once closed");
	}

	// [[BPR-1112]]. A withdrawal while the host shuts down. Its failure runs in
	// the shutdown turn, its count is released, and the host is quiescent with
	// no later call: no second shutdown and no other ingress.
	void test_a_withdrawal_at_shutdown_runs_its_failure()
	{
		withdrawing_binding_lane course;
		auto&                    host = course.Fixture.Host;
		// Accepted but never stored, so the shutdown policy removes it, and its
		// tombstone is the request the lane withdraws.
		establish_native_session(course.Fixture, core::host_profile_id{ 11U }, core::github_account_id{ 51U }, std::nullopt, application::github_credential_provenance::device_grant, binding_lifetime());
		course.settle();
		const auto authority   = *host.accepted_credential_authority();
		const auto diagnostics = course.Fixture.Projections.host_diagnostic_count();
		const auto escaped     = course.escapes_while_withdrawing([&] { host.shutdown(); });
		expect(!escaped, "shutdown: the host handles a withdrawn tombstone, so its caller sees no throw");
		expect(course.failed_visibly(diagnostics) && host.shutdown_cleanup_failed() && course.Store.Operations.empty() && course.Fixture.Vault.Deletes.size() == 1U
				   && course.Fixture.Vault.Deletes.front().Context.Authority == authority,
			   "shutdown: the withdrawn tombstone's failure ran in the shutdown turn, and its path still erases the credential");
		course.settle();
		expect(host.is_quiescent(), "shutdown: the count is released, and the host is quiescent with no later call");
	}

	// [[BPR-1112]]. Out of memory in a shutdown after the lane withdrew its
	// tombstone: each allocation after the withdrawal is failed in turn. The
	// withdrawn request's failure still runs, whether the turn ends normally
	// or in an exception, so draining the lanes is enough and no second
	// shutdown is needed. Measured on macOS, none of these failures ends the
	// turn in an exception; `run_turn` would fail the queue before rethrowing
	// if one did.
	void test_a_shutdown_that_fails_after_a_withdrawal_still_runs_its_failure()
	{
#if !BPR_TEST_ALLOCATION_SWEEPS
		std::cout << "BPR-1112 throwing shutdown: not run, because _ITERATOR_DEBUG_LEVEL " << _ITERATOR_DEBUG_LEVEL << " allocates inside noexcept moves ([[BPR-1124]])\n";
#else
		auto thrown = std::size_t{ 0U };
		auto steps  = 0LL;
		for (;; ++steps) {
			withdrawing_binding_lane course;
			auto&                    host = course.Fixture.Host;
			establish_native_session(course.Fixture, core::host_profile_id{ 11U }, core::github_account_id{ 51U }, std::nullopt, application::github_credential_provenance::device_grant, binding_lifetime());
			course.settle();
			// The decision lane's refusal is the one that withdraws the tombstone.
			course.Lane.ArmAfterRefusal = steps;
			const auto escaped          = course.escapes_while_withdrawing([&] { host.shutdown(); });
			const auto injected         = allocation_fault::disarm();
			course.settle();
			expect(host.is_quiescent(), "a shutdown whose allocation " + std::to_string(steps) + " fails still reaches quiescence with no later call");
			if (!injected) {
				break;
			}
			thrown += escaped ? 1U : 0U;
		}
		std::cout << "BPR-1112 throwing shutdown: " << steps << " allocations after the withdrawal failed in turn, " << thrown << " ended the turn in an exception\n";
		expect(steps > 0, "the shutdown allocates after the withdrawal, so the sweep failed something");
#endif
	}

	// [[BPR-1112]]. `start_delete` builds its terminal handler before it
	// changes any state. Each allocation in a shutdown that must erase a
	// durable grant is failed in turn. If the cleanup's request was neither
	// admitted by the lane nor failed, the grant's durability is as it was.
	// And the host always reaches quiescence.
	void test_a_failed_allocation_in_start_delete_changes_nothing()
	{
#if !BPR_TEST_ALLOCATION_SWEEPS
		std::cout << "BPR-1112 start_delete: not run, because _ITERATOR_DEBUG_LEVEL " << _ITERATOR_DEBUG_LEVEL << " allocates inside noexcept moves ([[BPR-1124]])\n";
#else
		auto changed = std::size_t{ 0U };
		auto steps   = 0LL;
		for (;; ++steps) {
			host_fixture                            fixture{ application::github_credential_shutdown_policy::delete_retained_on_shutdown };
			binding_store                           store;
			application::manual_executor            effects;
			application::device_binding_persistence persistence{ store, effects, fixture.Lane };
			fixture.Host.configure_device_persistence(persistence, [] { return std::int64_t{ 1000 }; });
			settle_grant(fixture);
			effects.drain();
			fixture.Lane.drain();
			expect(fixture.Host.accepted_device_binding_durable(), "start_delete: the grant is durable before the shutdown");
			const auto diagnostics = fixture.Projections.host_diagnostic_count();
			allocation_fault::arm(steps);
			try {
				fixture.Host.shutdown();
			} catch (...) {
			}
			const auto injected = allocation_fault::disarm();
			const auto admitted = !persistence.is_quiescent();
			const auto failed   = !fixture.Vault.Deletes.empty() || fixture.Projections.host_diagnostic_count() > diagnostics;
			if (!admitted && !failed && fixture.Host.accepted_session().has_value() && !fixture.Host.accepted_device_binding_durable()) {
				++changed;
				std::cerr << "start_delete: allocation " << steps << " left durability changed with no request and no terminal\n";
			}
			auto       answered = std::size_t{ 0U };
			const auto drain    = [&] {
				while (effects.pending() != 0U || fixture.Lane.pending() != 0U || answered < fixture.Vault.Deletes.size()) {
					effects.drain();
					fixture.Lane.drain();
					for (; answered < fixture.Vault.Deletes.size(); ++answered) {
						const auto deletion = fixture.Vault.Deletes.at(answered);
						deletion.Completion(application::github_credential_delete_success{ .Context = deletion.Context, .Disposition = application::github_credential_delete_disposition::deleted });
					}
				}
			};
			drain();
			// A failure before the shutdown closed the host leaves it open; the
			// caller's second shutdown then closes it. What is asserted is that
			// nothing stays counted.
			if (!fixture.Host.is_quiescent()) {
				fixture.Host.shutdown();
				drain();
			}
			expect(fixture.Host.is_quiescent(), "start_delete: a shutdown whose allocation " + std::to_string(steps) + " fails still reaches quiescence");
			if (!injected) {
				break;
			}
		}
		std::cout << "BPR-1112 start_delete: " << steps << " allocations failed in turn\n";
		expect(steps > 0 && changed == 0U, "start_delete: no failed allocation changes durability without a request or a terminal");
#endif
	}

	// The launch restore's erase of a binding whose refresh lifetime elapsed.
	void test_a_withdrawn_launch_restore_erase_is_not_left_counted()
	{
		using status                                     = application::device_profile_restore_status;
		const auto                               profile = native_profile(core::host_profile_id{ 11U });
		const application::device_binding_record record{ .Authority = { .Profile = profile.Profile, .ProfileRevision = profile.ProfileRevision, .Host = profile.Host, .Hostname = profile.Hostname, .Account = core::github_account_id{ 51U }, .ServerContract = profile.ServerContract }, .Lifetime = binding_lifetime() };
		withdrawing_binding_lane                 course{ binding_lifetime().RefreshExpiresAt };
		course.Store.Rows = { record };
		std::vector<status> results;
		course.Fixture.Host.restore_device_profile(profile, [&results](const status aStatus) { results.push_back(aStatus); });
		// The load reads the store, and its terminal then asks for the tombstone.
		course.Effects.Inner.drain();
		const auto escaped = course.escapes_while_withdrawing([&] { course.Lane.Inner.drain(); });
		course.settle();
		const auto withdrawn = !escaped && results == std::vector{ status::unavailable } && !course.tombstoned();
		expect(withdrawn && course.later_restore_reaches_the_store() && course.tombstoned(),
			   "launch erase: the restore ends unavailable with no tombstone written, and the next restore reaches the store and writes it");
		expect(course.quiescent_once_closed(), "launch erase: the host is quiescent once closed");
	}

	void test_restore_device_profile_exact_predicate_cancels_before_authentication()
	{
		using status                                     = application::device_profile_restore_status;
		const auto                               profile = native_profile(core::host_profile_id{ 11U });
		const application::device_binding_record record{ .Authority = { .Profile = profile.Profile, .ProfileRevision = profile.ProfileRevision, .Host = profile.Host, .Hostname = profile.Hostname, .Account = core::github_account_id{ 51U }, .ServerContract = profile.ServerContract }, .Lifetime = binding_lifetime() };
		for (int scenario = 0; scenario < 3; ++scenario) {
			host_fixture  fixture;
			binding_store store;
			store.Rows = { record };
			application::manual_executor            effects;
			application::device_binding_persistence persistence{ store, effects, fixture.Lane };
			fixture.Host.configure_device_persistence(persistence, [] { return std::int64_t{ 1000 }; });
			std::vector<status> results;
			std::uint64_t       currentRequest = 1U;
			const auto          request        = currentRequest;
			expect(fixture.Host.restore_device_profile(profile, [&](status aStatus) { results.push_back(aStatus); }, [&] {
				if (scenario == 1) { throw std::runtime_error{ "predicate refusal" }; }
				if (scenario == 2) { fixture.Host.shutdown(); return true; }
				return currentRequest == request; }), "exact startup request reserves a restore slot");
			currentRequest = 2U;
			effects.drain();
			fixture.Lane.drain();
			expect(results == std::vector{ status::cancelled }
					   && fixture.Provider.Profiles.empty() && fixture.Vault.Retrieves.empty()
					   && fixture.Bindings.size() == 0U
					   && store.Operations == std::vector<std::string>{ "load" },
				   "stale, throwing and reentrant-closing predicate cancels before binding or auth mutation");
			if (scenario == 0) {
				const auto nextRequest = currentRequest;
				expect(fixture.Host.restore_device_profile(profile, [&](status aStatus) { results.push_back(aStatus); }, [&, nextRequest] { return currentRequest == nextRequest; }), "new exact request can restore after older request retires");
				effects.drain();
				fixture.Lane.drain();
				expect(results == std::vector{ status::cancelled, status::ready_for_session }
						   && fixture.Provider.Profiles.size() == 1U,
					   "retired predicate cannot cancel a newer attempt");
			}
			fixture.Host.shutdown();
			fixture.Lane.drain();
		}
	}

	void test_restore_device_profile_bounds_pending_and_delivering_terminals()
	{
		using status = application::device_profile_restore_status;
		host_fixture                            fixture;
		binding_store                           store;
		application::manual_executor            effects;
		application::device_binding_persistence persistence{ store, effects, fixture.Lane };
		fixture.Host.configure_device_persistence(persistence, [] { return std::int64_t{ 1000 }; });
		const auto  profile   = native_profile(core::host_profile_id{ 11U });
		std::size_t callbacks = 0U;
		expect(fixture.Host.restore_device_profile(profile, [&](status) { ++callbacks; }), "one load reserves one restore slot");
		for (std::size_t index = 1U; index < application::maximum_pending_device_profile_restores; ++index) {
			expect(fixture.Host.restore_device_profile(profile, [&](status) {
				if (callbacks == 0U) {
					expect(!fixture.Host.restore_device_profile(profile, [&](status) { ++callbacks; }),
						   "active delivery still owns its slot during reentrant admission");
				}
				++callbacks;
			}),
				   "bounded refusal terminal reserves a slot");
		}
		const auto queued = fixture.Lane.pending();
		for (std::size_t index = 0U; index < 100U; ++index) {
			expect(!fixture.Host.restore_device_profile(profile, [&](status) { ++callbacks; }), "restore overflow refuses admission without callback");
		}
		expect(effects.pending() == 1U && queued == 31U && fixture.Lane.pending() == queued,
			   "overflow grows neither load effects nor terminal queue");
		fixture.Lane.drain();
		expect(callbacks == 31U, "only admitted refusal observers ran");
		fixture.Host.shutdown();
		const auto pendingAfterClose = fixture.Lane.pending();
		expect(!fixture.Host.restore_device_profile(profile, [&](status) { ++callbacks; })
				   && fixture.Lane.pending() == pendingAfterClose,
			   "closing refuses new restore work without a terminal");
		effects.drain();
		fixture.Lane.drain();
		expect(callbacks == 32U && fixture.Host.is_quiescent(), "accepted load supplies final cancelled terminal and releases every slot");
		expect(!fixture.Host.restore_device_profile(profile) && fixture.Host.is_quiescent(), "legacy call cannot reopen quiescent shutdown ingress");
		{
			fake_provider                                  provider;
			fake_vault                                     vault;
			throwing_once_lane                             lane;
			recording_projections                          projections;
			application::github_credential_binding_catalog bindings;
			application::github_application_host           host{ initial_state(), provider, provider, provider, provider, vault, bindings, lane, projections };
			expect(host.restore_device_profile(profile, [](status) { }), "delivery failure follows admitted call");
			host.shutdown();
			expect(host.projection_delivery_failed() && host.is_quiescent(), "rejected decision post reports delivery failure and releases slot");
		}
	}

	void test_restore_device_profile_has_one_terminal_for_missing_invalid_and_ready()
	{
		using status                                     = application::device_profile_restore_status;
		const auto                               profile = native_profile(core::host_profile_id{ 11U });
		const application::device_binding_record record{ .Authority = { .Profile = profile.Profile, .ProfileRevision = profile.ProfileRevision, .Host = profile.Host, .Hostname = profile.Hostname, .Account = core::github_account_id{ 51U }, .ServerContract = profile.ServerContract }, .Lifetime = binding_lifetime() };
		for (int scenario = 0; scenario < 7; ++scenario) {
			host_fixture                            fixture;
			binding_store                           store;
			application::manual_executor            effects;
			application::device_binding_persistence persistence{ store, effects, fixture.Lane };
			fixture.Host.configure_device_persistence(persistence, [scenario] { return std::int64_t{ scenario == 4 ? 4600 : 1000 }; });
			if (scenario != 0) {
				store.Rows = { record };
			}
			if (scenario == 1) {
				store.Rows.front().Authority.Account = { };
			}
			if (scenario == 2) {
				store.Rows.push_back(record);
			}
			if (scenario == 3) {
				store.Rows.front().Tombstone = true;
			}
			if (scenario == 5) {
				store.RefuseLoad = true;
			}
			std::vector<status> results;
			fixture.Host.restore_device_profile(profile, [&](const status aStatus) { results.push_back(aStatus); });
			expect(results.empty(), "restore callback never runs during ingress");
			effects.drain();
			expect(results.empty(), "restore worker does not deliver decision observation");
			fixture.Lane.run_next();
			expect(results.empty(), "load turn queues its terminal instead of invoking it recursively");
			fixture.Lane.drain();
			effects.drain();
			fixture.Lane.drain();
			expect(results == std::vector{ scenario == 6 ? status::ready_for_session : status::unavailable },
				   "missing, malformed, duplicate, tombstoned, expired and refused loads each terminate once");
			expect(fixture.Vault.Retrieves.empty() && fixture.Vault.Stores.empty(), "binding terminal carries no credential effect or enrollment");
			if (scenario == 6) {
				expect(fixture.Provider.Profiles.size() == 1U && !fixture.Host.accepted_session(),
					   "ready terminal reports queued resume, never accepted authentication");
			}
			fixture.Host.shutdown();
			fixture.Lane.drain();
			expect(results.size() == 1U, "shutdown cannot redeliver a retired terminal");
		}
	}

	// [[BPR-0644]]. A binding whose access lifetime elapsed while the
	// application was closed holds its session for a renewal instead of being
	// tombstoned. The renewal asks for the rotation barrier, which writes no
	// tombstone for it, and ends exactly once: by its rotated grant enrolling
	// under the binding's exact account, or without enrollment. Under the
	// owner's rule of 2026-09-24 a failure -- the network, a refusal, a
	// refresh lifetime already gone, the rotated grant's own `/user` -- keeps
	// the grant: no slot is erased, the row stays live, the held session fails
	// saying why, and Sign out afterwards erases both slots. Only the person's
	// own endings erase it -- Sign out, a replacing token. Since the owner's
	// decision of 2026-09-24 that includes a rotated grant that names another
	// account: it is kept, and its failure says the account differs.
	void test_restore_device_profile_renews_an_access_expired_grant_before_its_session()
	{
		using status                                     = application::device_profile_restore_status;
		const auto                               profile = native_profile(core::host_profile_id{ 11U });
		const application::device_binding_record record{ .Authority = { .Profile = profile.Profile, .ProfileRevision = profile.ProfileRevision, .Host = profile.Host, .Hostname = profile.Hostname, .Account = core::github_account_id{ 51U }, .ServerContract = profile.ServerContract }, .Lifetime = binding_lifetime() };
		const auto                               rotated = application::device_binding_lifetime{ .IssuedAt = 5000, .AccessExpiresAt = 33800, .RefreshExpiresAt = 20000 };
		enum class ending { enrollment,
							other_account,
							network_failure,
							refused,
							rotated_user_offline,
							sign_out_after_refresh_stored,
							replaced_by_token,
							refresh_expired };
		const auto namesOnlyTheRenewal = [&record](const host_fixture& aFixture) {
			return std::ranges::all_of(aFixture.Vault.Deletes, [&record](const auto& aCall) { return aCall.Context.Authority == record.Authority; })
				&& std::ranges::all_of(aFixture.Vault.RefreshDeletes, [&record](const auto& aContext) { return aContext.Authority == record.Authority; });
		};
		const auto completeDeletes = [](host_fixture& aFixture) {
			for (std::size_t index = 0U; index < aFixture.Vault.Deletes.size(); ++index) {
				const auto deletion = aFixture.Vault.Deletes.at(index);
				deletion.Completion(application::github_credential_delete_success{ .Context = deletion.Context, .Disposition = application::github_credential_delete_disposition::deleted });
				aFixture.Lane.drain();
			}
		};
		// The kept grant: nothing erased, the row live, the session failed with
		// the reason the host published beside it, and an orderly quit keeping
		// it too. Then, in a second fixture over the same store, Sign out
		// removes it. That is the person's own gesture, and the only one.
		const auto expectKept = [&](host_fixture& aFixture, binding_store& aStore, application::manual_executor& aEffects, const core::github_reason_code aReason, const std::string_view aEnding) {
			aFixture.Lane.drain();
			aEffects.drain();
			aFixture.Lane.drain();
			expect(aFixture.Vault.Deletes.empty() && aFixture.Vault.RefreshDeletes.empty() && !aStore.Rows.front().Tombstone,
				   std::string{ aEnding } + " keeps the grant: no slot is erased and the row stays live");
			expect(!aFixture.Host.accepted_session() && !aFixture.Host.state().PendingSession,
				   std::string{ aEnding } + " fails the held session visibly");
			expect(aFixture.Projections.kept_grant_reason() == std::optional{ aReason },
				   std::string{ aEnding } + " says the grant was kept, beside the failure it was kept through");
			aFixture.Host.dispatch(core::deactivate_github_identity_intent{ });
			aFixture.Lane.drain();
			aEffects.drain();
			aFixture.Lane.drain();
			expect(aFixture.Vault.Deletes.size() == 1U && namesOnlyTheRenewal(aFixture) && aStore.Rows.front().Tombstone,
				   std::string{ aEnding } + ": Sign out on the failed session erases the kept grant's exact slot behind a tombstone");
			completeDeletes(aFixture);
			expect(aFixture.Vault.RefreshDeletes.size() == 1U && namesOnlyTheRenewal(aFixture),
				   std::string{ aEnding } + ": and its refresh slot with it");
		};
		for (const auto end : { ending::enrollment, ending::other_account, ending::network_failure, ending::refused, ending::rotated_user_offline, ending::sign_out_after_refresh_stored, ending::replaced_by_token, ending::refresh_expired }) {
			host_fixture                                                   fixture;
			binding_store                                                  store;
			application::manual_executor                                   effects;
			application::device_binding_persistence                        persistence{ store, effects, fixture.Lane };
			std::vector<std::optional<application::device_launch_renewal>> renewals;
			const auto                                                     now = end == ending::refresh_expired ? std::int64_t{ 10000 } : std::int64_t{ 5000 };
			fixture.Host.configure_device_persistence(persistence, [now] { return now; }, [&renewals](const std::optional<application::device_launch_renewal>& aRenewal) { renewals.push_back(aRenewal); });
			store.Rows = { record };
			std::vector<status> results;
			fixture.Host.restore_device_profile(profile, [&](const status aStatus) { results.push_back(aStatus); });
			effects.drain();
			fixture.Lane.drain();
			effects.drain();
			fixture.Lane.drain();
			if (end == ending::refresh_expired) {
				expect(renewals.empty() && results == std::vector{ status::unavailable }, "a refresh-expired binding asks for no renewal and cannot resume");
				expect(fixture.Provider.Profiles.empty() && fixture.Vault.Retrieves.empty() && fixture.Vault.RefreshRetrieves.empty(), "and reads or spends nothing");
				expectKept(fixture, store, effects, core::github_reason_code::authentication_failed, "a refresh-expired binding");
				fixture.Host.shutdown();
				fixture.Lane.drain();
				continue;
			}
			expect(results == std::vector{ status::ready_for_session } && renewals.size() == 1U && renewals.front().has_value(), "an access-expired binding asks for one launch renewal");
			const auto renewal = *renewals.front();
			expect(renewal.Authority == record.Authority && renewal.Lifetime == record.Lifetime && renewal.IdentityGeneration.value() != 0U, "the renewal names the exact slot, its lifetime and the held session's generation");
			expect(fixture.Provider.Profiles.empty() && fixture.Vault.Retrieves.empty(), "the held session presents nothing to the provider or the vault");
			std::optional<bool> barrier;
			fixture.Host.prepare_device_rotation(renewal.Authority, renewal.IdentityGeneration, [&barrier](const bool aReady) { barrier = aReady; });
			effects.drain();
			fixture.Lane.drain();
			expect(barrier == std::optional{ true } && !store.Rows.front().Tombstone && store.Operations == std::vector<std::string>{ "load" },
				   "the launch renewal's barrier answers ready and writes no tombstone, so a renewal that cannot finish leaves the row live");

			if (end == ending::enrollment || end == ending::other_account || end == ending::rotated_user_offline) {
				fixture.Host.enroll_profile(profile.Profile, credential(), application::github_credential_provenance::device_grant, rotated, true);
				fixture.Lane.drain();
				expect(fixture.Provider.Profiles.size() == 1U && renewals.size() == 1U, "the rotated grant's own enrollment carries the renewal on without abandoning it");
				const auto profileRequest = fixture.Provider.Profiles.front().Request;
				fixture.Provider.Profiles.front().Completion(application::github_profile_success{ .Request = profileRequest, .Authority = profile });
				fixture.Lane.drain();
				expect(fixture.Provider.Viewers.size() == 1U, "the rotated grant meets its bootstrap /user");
				if (end == ending::rotated_user_offline) {
					// The token endpoint answered and the rotated refresh token
					// is in its slot; the network then fails the rotated grant's
					// own /user on every attempt.
					for (std::size_t attempt = 0U; attempt < fixture.Provider.Viewers.size() && attempt < 8U; ++attempt) {
						const auto call = fixture.Provider.Viewers.at(attempt);
						call.Completion(application::github_viewer_error{ .Request = call.Request, .Error = { .Failure = application::github_provider_failure::network } });
						fixture.Lane.drain();
					}
					expect(renewals.size() == 2U && !renewals.back().has_value(), "the device host's renewal is released");
					expect(fixture.Vault.Stores.empty(), "no rotated access token reached the vault");
					expectKept(fixture, store, effects, core::github_reason_code::network_unavailable, "a rotated grant whose /user could not be reached");
				} else {
					const auto bootstrap = fixture.Provider.Viewers.front().Request;
					// The token endpoint's answer, read back through /user: the
					// binding's own account, or one it never named.
					const auto account = end == ending::enrollment ? record.Authority.Account : core::github_account_id{ 52U };
					fixture.Provider.Viewers.front().Completion(application::github_viewer_success{ .Request = bootstrap, .Viewer = viewer(account), .RateLimits = rate_limits_for(application::github_rate_limit_resource::core), .AccountTransport = { } });
					fixture.Lane.drain();
					effects.drain();
					fixture.Lane.drain();
					if (end == ending::enrollment) {
						expect(fixture.Vault.Stores.size() == 1U && fixture.Vault.Stores.front().Context.Authority == record.Authority && fixture.Vault.Deletes.empty(), "the binding's own account stores the rotated access token under the exact slot");
					} else {
						expect(fixture.Vault.Stores.empty(), "another account's token is refused before it reaches the vault");
						expect(renewals.size() == 2U && !renewals.back().has_value(), "and the device host's renewal is released");
						// [[BPR-0644]]. The owner's decision of 2026-09-24: kept
						// like every other failure, not erased on its own, and
						// said as the account differing.
						expect(fixture.Projections.kept_grant_names_another_account(), "the failure says the rotated grant signs in as an account the binding never named");
						expectKept(fixture, store, effects, core::github_reason_code::malformed_response, "a rotated grant that signs in as another account");
					}
				}
			} else if (end == ending::network_failure || end == ending::refused) {
				const auto failure = end == ending::network_failure ? core::github_failure::network : core::github_failure::authentication;
				fixture.Host.invalidate_device_grant(renewal.Authority, renewal.IdentityGeneration, failure);
				fixture.Lane.drain();
				expect(fixture.Provider.Profiles.empty() && renewals.size() == 2U && !renewals.back().has_value(), "a renewal that cannot finish presents nothing and releases the device host");
				expectKept(fixture, store, effects, core::reason_for(failure), end == ending::network_failure ? "an offline launch renewal" : "a refused launch renewal");
			} else if (end == ending::sign_out_after_refresh_stored) {
				// Stands in for the device host's store of the rotated refresh
				// token, which lands before the rotated access token enrolls.
				fixture.Vault.AcceptRefreshStores = true;
				fixture.Vault.store_refresh(application::github_credential_enrollment{ .Context = { .Request = core::request_id{ 900U }, .ApplicationSession = core::application_session_id{ 1U }, .Authority = record.Authority, .IdentityGeneration = renewal.IdentityGeneration }, .Credential = credential() }, [](application::github_credential_store_completion) { });
				expect(fixture.Vault.RefreshSlots.contains(record.Authority), "the rotated refresh token is in the slot");
				fixture.Host.dispatch(core::deactivate_github_identity_intent{ });
				fixture.Lane.drain();
				effects.drain();
				fixture.Lane.drain();
				expect(renewals.size() == 2U && !renewals.back().has_value(), "Sign out abandons the renewal so no rotated token is enrolled over it");
				expect(fixture.Vault.Deletes.size() == 1U && namesOnlyTheRenewal(fixture) && store.Rows.front().Tombstone, "Sign out erases the renewal's exact slot behind a tombstone");
				completeDeletes(fixture);
				expect(fixture.Vault.RefreshSlots.empty() && fixture.Vault.RefreshDeletes.size() == 1U && namesOnlyTheRenewal(fixture), "no rotated refresh token stays behind the tombstoned row");
				std::optional<bool> late;
				fixture.Host.prepare_device_rotation(renewal.Authority, renewal.IdentityGeneration, [&late](const bool aReady) { late = aReady; });
				expect(late == std::optional{ false }, "an abandoned renewal is refused its barrier");
			} else {
				fixture.Host.enroll_profile(profile.Profile, credential());
				fixture.Lane.drain();
				effects.drain();
				fixture.Lane.drain();
				expect(renewals.size() == 2U && !renewals.back().has_value(), "a pasted token abandons the renewal");
				expect(fixture.Vault.Deletes.size() == 1U && namesOnlyTheRenewal(fixture) && fixture.Provider.Profiles.empty(), "the renewal's slot is erased before the replacing session begins");
				completeDeletes(fixture);
				expect(fixture.Provider.Profiles.size() == 1U, "the replacing session begins once the erasure answers");
				const auto profileRequest = fixture.Provider.Profiles.front().Request;
				fixture.Provider.Profiles.front().Completion(application::github_profile_success{ .Request = profileRequest, .Authority = profile });
				fixture.Lane.drain();
				const auto bootstrap = fixture.Provider.Viewers.front().Request;
				fixture.Provider.Viewers.front().Completion(application::github_viewer_success{ .Request = bootstrap, .Viewer = viewer(core::github_account_id{ 52U }), .RateLimits = rate_limits_for(application::github_rate_limit_resource::core), .AccountTransport = { } });
				fixture.Lane.drain();
				expect(fixture.Vault.Stores.size() == 1U && fixture.Vault.Stores.front().Context.Authority.Account == core::github_account_id{ 52U }, "the replacing token is stored under its own account");
				expect(namesOnlyTheRenewal(fixture), "and nothing the replacing grant owns is erased");
			}
			fixture.Host.shutdown();
			effects.drain();
			fixture.Lane.drain();
			completeDeletes(fixture);
		}
	}

	void test_restore_device_profile_refusal_reentrancy_and_shutdown_terminals()
	{
		using status       = application::device_profile_restore_status;
		const auto profile = native_profile(core::host_profile_id{ 11U });
		{
			host_fixture        fixture;
			std::vector<status> results;
			fixture.Host.restore_device_profile(profile, [&](const status aStatus) {
				results.push_back(aStatus);
				fixture.Host.shutdown();
				expect(!fixture.Host.is_quiescent(), "terminal delivery stays accounted through reentrant shutdown");
			});
			expect(results.empty(), "unconfigured restore refusal is also deferred");
			fixture.Lane.drain();
			expect(results == std::vector{ status::unavailable } && fixture.Host.is_quiescent(), "refusal terminal drains after callback returns");
		}
		{
			host_fixture                            fixture;
			binding_store                           store;
			application::manual_executor            effects;
			application::device_binding_persistence persistence{ store, effects, fixture.Lane };
			fixture.Host.configure_device_persistence(persistence, [] { return std::int64_t{ 1000 }; });
			std::vector<status> results;
			fixture.Host.restore_device_profile(profile, [&](const status aStatus) {
				results.push_back(aStatus);
				fixture.Host.restore_device_profile(profile, [&](const status aNext) { results.push_back(aNext); });
			});
			effects.drain();
			fixture.Lane.drain();
			expect(results.size() == 1U && effects.pending() == 1U, "reentrant restore is a distinct bounded load");
			fixture.Host.shutdown();
			expect(!fixture.Host.is_quiescent(), "pending load and terminal prevent early shutdown quiescence");
			effects.drain();
			fixture.Lane.run_next();
			expect(!fixture.Host.is_quiescent() && results.size() == 1U, "load cancellation still owes one terminal");
			fixture.Lane.drain();
			expect(results == std::vector{ status::unavailable, status::cancelled } && fixture.Host.is_quiescent(), "reentrant pending restore terminates cancelled on shutdown");
		}
		{
			host_fixture fixture;
			fixture.Host.restore_device_profile(profile, [&](status) { fixture.Host.shutdown(); throw std::runtime_error{ "observer refusal" }; });
			fixture.Lane.drain();
			fixture.Host.shutdown();
			expect(fixture.Host.projection_delivery_failed() && fixture.Host.is_quiescent(), "throwing observer is reported without stranding quiescence");
		}
	}

	void test_restore_device_profile_superseded_load_terminates_without_resuming()
	{
		using status = application::device_profile_restore_status;
		host_fixture                            fixture;
		const auto                              profile = native_profile(core::host_profile_id{ 11U });
		binding_store                           store;
		application::manual_executor            effects;
		application::device_binding_persistence persistence{ store, effects, fixture.Lane };
		fixture.Host.configure_device_persistence(persistence, [] { return std::int64_t{ 1000 }; });
		std::vector<status> results;
		fixture.Host.restore_device_profile(profile, [&](const status aStatus) { results.push_back(aStatus); });
		fixture.Host.restore_device_profile(profile, [&](const status aStatus) { results.push_back(aStatus); });
		fixture.Host.dispatch(core::deactivate_github_identity_intent{ });
		effects.drain();
		fixture.Lane.drain();
		expect(results == std::vector{ status::unavailable, status::cancelled }, "overlapping call refuses and superseded admitted load cancels exactly once");
		expect(fixture.Provider.Profiles.empty() && fixture.Vault.Retrieves.empty(), "stale load cannot resume or read credentials");
		fixture.Host.shutdown();
		expect(fixture.Host.is_quiescent(), "stale terminal fully drains");
	}

	void test_restored_binding_is_validated_before_resume()
	{
		const auto                               profile = native_profile(core::host_profile_id{ 11U });
		const application::device_binding_record record{ .Authority = { .Profile = profile.Profile, .ProfileRevision = profile.ProfileRevision, .Host = profile.Host, .Hostname = profile.Hostname, .Account = core::github_account_id{ 51U }, .ServerContract = profile.ServerContract }, .Lifetime = binding_lifetime() };

		for (const auto scope : { core::bpr_profile_id{ 0U }, core::bpr_profile_id{ 2U } }) {
			host_fixture  fixture;
			binding_store store;
			auto          foreign        = record;
			foreign.Authority.BprProfile = scope;
			store.Rows                   = { foreign };
			application::manual_executor            effects;
			application::device_binding_persistence persistence{ store, effects, fixture.Lane };
			fixture.Host.configure_device_persistence(persistence, [] { return std::int64_t{ 1000 }; });
			fixture.Host.restore_device_profile(profile);
			effects.drain();
			fixture.Lane.drain();
			expect(fixture.Provider.Profiles.empty() && fixture.Vault.Retrieves.empty() && fixture.Vault.Deletes.empty() && store.Operations == std::vector<std::string>{ "load" }, "foreign or invalid restored scope refuses before provider, vault or tombstone effects");
			fixture.Host.shutdown();
		}
		for (const auto invalidTime : { std::int64_t{ 999 }, std::int64_t{ 4600 } }) {
			host_fixture  fixture;
			binding_store store;
			store.Rows = { record };
			application::manual_executor            effects;
			application::device_binding_persistence persistence{ store, effects, fixture.Lane };
			fixture.Host.configure_device_persistence(persistence, [invalidTime] { return invalidTime; });
			fixture.Host.restore_device_profile(profile);
			effects.drain();
			fixture.Lane.drain();
			effects.drain();
			fixture.Lane.drain();
			// [[BPR-0644]]. No sink is composed here, so neither can renew. The
			// owner's rule keeps the grant: nothing is read or spent, the row
			// stays live, and the held session fails asking for sign-in.
			expect(fixture.Provider.Profiles.empty() && fixture.Vault.Retrieves.empty() && !store.Rows.front().Tombstone && fixture.Vault.Deletes.empty(),
				   "a backward or expired stored grant reads nothing and is kept, not tombstoned");
			expect(!fixture.Host.state().PendingSession && fixture.Projections.kept_grant_reason() == std::optional{ core::github_reason_code::authentication_failed },
				   "and its held session fails saying it needs signing in again");
			fixture.Host.shutdown();
		}
		{
			host_fixture  fixture;
			binding_store store;
			store.Rows = { record, record };
			application::manual_executor            effects;
			application::device_binding_persistence persistence{ store, effects, fixture.Lane };
			fixture.Host.configure_device_persistence(persistence, [] { return std::int64_t{ 1000 }; });
			fixture.Host.restore_device_profile(profile);
			effects.drain();
			fixture.Lane.drain();
			expect(fixture.Provider.Profiles.empty(), "duplicate stored selector is refused whole");
			fixture.Host.shutdown();
		}
		{
			host_fixture  fixture;
			binding_store store;
			store.Rows = { record };
			application::manual_executor            effects;
			application::device_binding_persistence persistence{ store, effects, fixture.Lane };
			fixture.Host.configure_device_persistence(persistence, [] { return std::int64_t{ 1000 }; });
			fixture.Host.restore_device_profile(profile);
			effects.drain();
			fixture.Lane.drain();
			expect(fixture.Provider.Profiles.size() == 1U && fixture.Vault.Retrieves.empty(), "restored selector first resolves exact compiled profile");
			const auto request = fixture.Provider.Profiles.front().Request;
			fixture.Provider.Profiles.front().Completion(application::github_profile_success{ .Request = request, .Authority = profile });
			fixture.Lane.drain();
			expect(fixture.Vault.Retrieves.size() == 1U && fixture.Vault.Retrieves.front().Context.Authority == record.Authority, "only the exact stored account selects access material");
			const auto retrieve = fixture.Vault.Retrieves.front();
			retrieve.Completion(application::github_credential_retrieve_absent{ .Context = retrieve.Context });
			fixture.Lane.drain();
			expect(fixture.Vault.RefreshRetrieves.empty(), "missing access cannot spend orphan refresh");
			effects.drain();
			fixture.Lane.drain();
			// [[BPR-0644]]. Kept under the owner's rule: the launch says the
			// credential store failed and nothing resumes from it.
			expect(!store.Rows.front().Tombstone && fixture.Vault.Deletes.empty() && !fixture.Host.state().PendingSession,
				   "missing access fails the restore visibly and keeps the binding");
			fixture.Host.shutdown();
			fixture.Lane.drain();
			expect(fixture.Host.is_quiescent() && fixture.Vault.Deletes.empty(), "an orderly quit after missing access erases nothing");
		}
	}

	// [[BPR-0644]]. The owner's rule of 2026-09-24 replaces BPR-0541's
	// fail-closed erasure for these faults: a restored grant whose resume
	// fails -- a locked or denied vault, the network, an authentication
	// refusal -- keeps its material and its live row. The launch fails
	// visibly with the reason, an orderly quit keeps the grant, the next
	// launch tries again, and only Sign out removes it.
	void test_restored_binding_launch_failures_keep_the_grant()
	{
		const auto                               profile = native_profile(core::host_profile_id{ 11U });
		const application::device_binding_record record{ .Authority = { .Profile = profile.Profile, .ProfileRevision = profile.ProfileRevision, .Host = profile.Host, .Hostname = profile.Hostname, .Account = core::github_account_id{ 51U }, .ServerContract = profile.ServerContract }, .Lifetime = binding_lifetime() };
		enum class answer { locked_vault,
							denied_vault,
							network,
							authentication,
							other_account,
							refused_other_account };
		const auto reasonOf = [](const answer aAnswer) {
			switch (aAnswer) {
			case answer::locked_vault:
			case answer::denied_vault:
				return core::github_reason_code::vault_unavailable;
			case answer::network:
				return core::github_reason_code::network_unavailable;
			case answer::authentication:
				return core::github_reason_code::authentication_failed;
			case answer::other_account:
				return core::github_reason_code::malformed_response;
			case answer::refused_other_account:
				return core::github_reason_code::authentication_failed;
			}
			return core::github_reason_code::none;
		};
		// One launch: restore, resolve the profile, then the given answer on
		// every attempt until the session gives up.
		const auto launch = [&profile](host_fixture& aFixture, application::device_binding_persistence& aPersistence, application::manual_executor& aEffects, const std::optional<answer> aGiven) {
			aFixture.Host.configure_device_persistence(aPersistence, [] { return std::int64_t{ 1000 }; });
			aFixture.Host.restore_device_profile(profile);
			aEffects.drain();
			aFixture.Lane.drain();
			const auto request = aFixture.Provider.Profiles.front().Request;
			aFixture.Provider.Profiles.front().Completion(application::github_profile_success{ .Request = request, .Authority = profile });
			aFixture.Lane.drain();
			const auto retrieve = aFixture.Vault.Retrieves.front();
			if (aGiven == answer::locked_vault || aGiven == answer::denied_vault) {
				retrieve.Completion(application::github_credential_retrieve_error{ .Context = retrieve.Context, .Failure = aGiven == answer::locked_vault ? application::github_credential_vault_failure::locked : application::github_credential_vault_failure::access_denied });
			} else {
				// The host retries a transient read itself; every attempt meets
				// the same answer until the session gives up.
				auto answered = std::size_t{ 0U };
				while (aFixture.Host.state().PendingSession && answered < aFixture.Vault.Retrieves.size() && answered < 8U) {
					const auto attempt = aFixture.Vault.Retrieves.at(answered);
					attempt.Completion(application::github_credential_retrieve_success{ .Context = attempt.Context, .Credential = credential() });
					aFixture.Lane.drain();
					const auto call = aFixture.Provider.Viewers.at(answered);
					if (!aGiven.has_value() || aGiven == answer::other_account) {
						// The binding's own account, or ([[BPR-0644]]) one it
						// never named: the restored access slot's /user answers
						// account 52.
						const auto account = aGiven.has_value() ? core::github_account_id{ 52U } : core::github_account_id{ 51U };
						call.Completion(application::github_viewer_success{ .Request = call.Request, .Viewer = viewer(account), .RateLimits = rate_limits_for(application::github_rate_limit_resource::core), .AccountTransport = std::make_shared<fake_lease>() });
						aFixture.Lane.drain();
						break;
					}
					// The provider's own account check refuses another
					// account's /user as an authentication failure that says
					// so ([[BPR-0644]]), which is the production path.
					call.Completion(application::github_viewer_error{
						.Request = call.Request, .Error = { .Failure = aGiven == answer::network ? application::github_provider_failure::network : application::github_provider_failure::authentication, .AccountMismatch = aGiven == answer::refused_other_account } });
					aFixture.Lane.drain();
					++answered;
				}
			}
			aFixture.Lane.drain();
			aEffects.drain();
			aFixture.Lane.drain();
		};
		for (const auto given : { answer::locked_vault, answer::denied_vault, answer::network, answer::authentication, answer::other_account, answer::refused_other_account }) {
			binding_store store;
			store.Rows = { record };
			{
				host_fixture                            fixture{ application::github_credential_shutdown_policy::retain_accepted_device_grants };
				application::manual_executor            effects;
				application::device_binding_persistence persistence{ store, effects, fixture.Lane };
				launch(fixture, persistence, effects, given);
				expect(!store.Rows.front().Tombstone && fixture.Vault.Deletes.empty() && fixture.Vault.RefreshDeletes.empty(), "a failed restore keeps the grant: no tombstone, no erasure");
				expect(!fixture.Host.accepted_session() && !fixture.Host.state().PendingSession, "every failed restore fails the session visibly");
				expect(fixture.Projections.kept_grant_reason() == std::optional{ reasonOf(given) }, "and says the grant was kept, beside the failure's own reason");
				// [[BPR-0644]]. The owner's decision of 2026-09-24: material that
				// signs in as another account is kept like every other failure,
				// and the failure says so.
				expect(fixture.Projections.kept_grant_names_another_account() == (given == answer::other_account || given == answer::refused_other_account),
					   "only the other account's answer says the kept grant signs in as an account its binding never named");
				expect(fixture.Vault.Stores.empty(), "and nothing is stored under the binding's key for the account it never named");
				fixture.Host.shutdown();
				effects.drain();
				fixture.Lane.drain();
				expect(fixture.Vault.Deletes.empty() && !store.Rows.front().Tombstone && fixture.Host.is_quiescent(), "an orderly quit keeps the grant too");
			}
			if (given == answer::authentication || given == answer::other_account || given == answer::refused_other_account) {
				// A refused grant needs signing in again, and one that answers as
				// another account is met again at the next launch; Sign out
				// removes either.
				host_fixture                            fixture{ application::github_credential_shutdown_policy::retain_accepted_device_grants };
				application::manual_executor            effects;
				application::device_binding_persistence persistence{ store, effects, fixture.Lane };
				launch(fixture, persistence, effects, given);
				expect(fixture.Vault.Deletes.empty() && !store.Rows.front().Tombstone && fixture.Projections.kept_grant_reason() == std::optional{ reasonOf(given) }
						   && fixture.Projections.kept_grant_names_another_account() == (given == answer::other_account || given == answer::refused_other_account),
					   "the next launch meets the kept grant's failure again, keeps it, and says so again");
				fixture.Host.dispatch(core::deactivate_github_identity_intent{ });
				fixture.Lane.drain();
				effects.drain();
				fixture.Lane.drain();
				expect(fixture.Vault.Deletes.size() == 1U && fixture.Vault.Deletes.front().Context.Authority == record.Authority && store.Rows.front().Tombstone,
					   "Sign out on the failed session erases the kept grant behind a tombstone");
				const auto deletion = fixture.Vault.Deletes.front();
				deletion.Completion(application::github_credential_delete_success{ .Context = deletion.Context, .Disposition = application::github_credential_delete_disposition::deleted });
				fixture.Lane.drain();
				expect(fixture.Vault.RefreshDeletes.size() == 1U && fixture.Vault.RefreshDeletes.front().Authority == record.Authority, "with its refresh slot");
				fixture.Host.shutdown();
				fixture.Lane.drain();
				continue;
			}
			// The next launch, with the fault gone, restores the same grant.
			host_fixture                            next{ application::github_credential_shutdown_policy::retain_accepted_device_grants };
			application::manual_executor            effects;
			application::device_binding_persistence persistence{ store, effects, next.Lane };
			launch(next, persistence, effects, std::nullopt);
			expect(next.Vault.Retrieves.front().Context.Authority == record.Authority && next.Provider.Viewers.size() == 1U && next.Vault.Deletes.empty(),
				   "the next launch retries the kept grant and presents it to /user");
			next.Host.shutdown();
			effects.drain();
			next.Lane.drain();
		}
	}

	void test_sign_out_during_binding_load_erases_the_loaded_authority()
	{
		const auto                               profile = native_profile(core::host_profile_id{ 11U });
		const application::device_binding_record record{ .Authority = { .Profile = profile.Profile, .ProfileRevision = profile.ProfileRevision, .Host = profile.Host, .Hostname = profile.Hostname, .Account = core::github_account_id{ 51U }, .ServerContract = profile.ServerContract }, .Lifetime = binding_lifetime() };
		for (const auto action : { 0U, 1U, 2U, 3U }) {
			const auto    signOut = action != 0U;
			host_fixture  fixture{ application::github_credential_shutdown_policy::retain_accepted_device_grants };
			binding_store store;
			store.Rows = { record };
			application::manual_executor            effects;
			application::device_binding_persistence persistence{ store, effects, fixture.Lane };
			fixture.Host.configure_device_persistence(persistence, [] { return std::int64_t{ 1000 }; });
			std::vector<application::device_profile_restore_status> results;
			if (action == 3U) {
				fixture.Host.restore_device_profile(profile, [&](auto aStatus) { results.push_back(aStatus); }, [] { return false; });
			} else {
				fixture.Host.restore_device_profile(profile);
			}
			if (signOut) {
				fixture.Host.dispatch(core::deactivate_github_identity_intent{ });
				if (action == 2U) {
					fixture.Host.shutdown();
				}
			} else {
				fixture.Host.shutdown();
			}
			effects.drain();
			fixture.Lane.drain();
			effects.drain();
			fixture.Lane.drain();
			expect(fixture.Provider.Profiles.empty(), "retired startup load cannot activate a profile");
			if (action == 3U) {
				expect(results == std::vector{ application::device_profile_restore_status::cancelled }, "predicate cancellation preserves the core-owned Sign out cleanup");
			}
			if (!signOut) {
				expect(!store.Rows.front().Tombstone && fixture.Vault.Deletes.empty() && fixture.Host.is_quiescent(), "ordinary shutdown during startup load preserves remembered grant");
				continue;
			}
			expect(store.Rows.front().Tombstone && store.Rows.front().Authority == record.Authority,
				   "Sign out during startup load must tombstone the exact remembered binding");
			expect(fixture.Vault.Deletes.size() == 1U && fixture.Vault.Deletes.front().Context.Authority == record.Authority,
				   "Sign out during startup load starts exact access cleanup");
			const auto deletion = fixture.Vault.Deletes.front();
			deletion.Completion(application::github_credential_delete_success{ .Context = deletion.Context, .Disposition = application::github_credential_delete_disposition::deleted });
			fixture.Lane.drain();
			expect(fixture.Vault.RefreshDeletes.size() == 1U && fixture.Vault.RefreshDeletes.front().Authority == record.Authority,
				   "Sign out during startup load also erases exact refresh slot");
			fixture.Host.shutdown();
			fixture.Lane.drain();
			expect(fixture.Host.is_quiescent(), "startup Sign out cleanup quiesces");
		}
	}

	void test_startup_cleanup_refuses_a_mismatched_compiled_profile()
	{
		host_fixture  fixture;
		const auto    profile = native_profile(core::host_profile_id{ 11U });
		binding_store store;
		store.Rows = { application::device_binding_record{ .Authority = { .Profile = profile.Profile, .ProfileRevision = application::github_profile_revision{ profile.ProfileRevision.value() + 1U }, .Host = profile.Host, .Hostname = profile.Hostname, .Account = core::github_account_id{ 51U }, .ServerContract = profile.ServerContract }, .Lifetime = binding_lifetime() } };
		application::manual_executor            effects;
		application::device_binding_persistence persistence{ store, effects, fixture.Lane };
		fixture.Host.configure_device_persistence(persistence, [] { return std::int64_t{ 1000 }; });
		fixture.Host.restore_device_profile(profile);
		fixture.Host.dispatch(core::deactivate_github_identity_intent{ });
		effects.drain();
		fixture.Lane.drain();
		expect(!store.Rows.front().Tombstone && fixture.Vault.Deletes.empty() && fixture.Vault.RefreshDeletes.empty() && fixture.Provider.Viewers.empty(),
			   "startup cleanup cannot target vault authority from a different compiled profile revision");
		fixture.Host.shutdown();
		expect(fixture.Host.is_quiescent(), "mismatched startup cleanup fails closed and quiesces");
	}

	void test_failed_startup_load_keeps_repeated_replacement_blocked()
	{
		host_fixture                             fixture;
		const auto                               profile = native_profile(core::host_profile_id{ 11U });
		const application::device_binding_record record{ .Authority = { .Profile = profile.Profile, .ProfileRevision = profile.ProfileRevision, .Host = profile.Host, .Hostname = profile.Hostname, .Account = core::github_account_id{ 51U }, .ServerContract = profile.ServerContract }, .Lifetime = binding_lifetime() };
		binding_store                            store;
		store.Rows       = { record };
		store.RefuseLoad = true;
		application::manual_executor            effects;
		application::device_binding_persistence persistence{ store, effects, fixture.Lane };
		fixture.Host.configure_device_persistence(persistence, [] { return std::int64_t{ 1000 }; });
		fixture.Host.restore_device_profile(profile);
		fixture.Host.enroll_profile(profile.Profile, credential());
		effects.drain();
		fixture.Lane.drain();
		expect(!store.Rows.front().Tombstone && fixture.Vault.Deletes.empty() && fixture.Vault.RefreshDeletes.empty(),
			   "unavailable startup load leaves the previous durable binding and pair untouched");
		for (std::size_t attempt = 0; attempt < 2U; ++attempt) {
			const auto profilesBefore    = fixture.Provider.Profiles.size();
			const auto viewersBefore     = fixture.Provider.Viewers.size();
			const auto diagnosticsBefore = fixture.Projections.host_diagnostic_count();
			fixture.Host.enroll_profile(profile.Profile, credential());
			effects.drain();
			fixture.Lane.drain();
			// If replacement reaches local profile or bootstrap /user, answer it
			// normally. The security assertion concerns the eventual vault write.
			if (fixture.Provider.Profiles.size() > profilesBefore) {
				const auto request = fixture.Provider.Profiles.back().Request;
				fixture.Provider.Profiles.back().Completion(application::github_profile_success{ .Request = request, .Authority = profile });
				fixture.Lane.drain();
			}
			if (fixture.Provider.Viewers.size() > viewersBefore) {
				const auto request = fixture.Provider.Viewers.back().Request;
				fixture.Provider.Viewers.back().Completion(application::github_viewer_success{ .Request = request, .Viewer = viewer(record.Authority.Account), .RateLimits = rate_limits_for(application::github_rate_limit_resource::core), .AccountTransport = { } });
				fixture.Lane.drain();
			}
			expect(fixture.Projections.host_diagnostic_count() > diagnosticsBefore, "each unresolved replacement refusal is visible");
			expect(fixture.Vault.Stores.empty() && !store.Rows.front().Tombstone && fixture.Vault.RefreshDeletes.empty(),
				   "repeated manual enrollment after uncertain startup load must not overwrite access beneath live device metadata");
		}
		fixture.Host.shutdown();
		effects.drain();
		fixture.Lane.drain();
		expect(fixture.Host.is_quiescent(), "uncertain startup load blocks replacement without stranding shutdown");
	}

	void test_replacement_during_binding_load_waits_for_exact_cleanup()
	{
		const auto                               profile = native_profile(core::host_profile_id{ 11U });
		const application::device_binding_record record{ .Authority = { .Profile = profile.Profile, .ProfileRevision = profile.ProfileRevision, .Host = profile.Host, .Hostname = profile.Hostname, .Account = core::github_account_id{ 51U }, .ServerContract = profile.ServerContract }, .Lifetime = binding_lifetime() };
		for (const bool preview : { false, true }) {
			host_fixture  fixture{ application::github_credential_shutdown_policy::retain, true };
			binding_store store;
			store.Rows = { record };
			application::manual_executor            effects;
			application::device_binding_persistence persistence{ store, effects, fixture.Lane };
			fixture.Host.configure_device_persistence(persistence, [] { return std::int64_t{ 1000 }; });
			fixture.Vault.HoldRefreshDelete = true;
			fixture.Host.restore_device_profile(profile);
			if (preview) {
				fixture.Host.activate_public_preview(profile.Profile);
			} else {
				fixture.Host.enroll_profile(profile.Profile, credential());
			}
			expect(fixture.Provider.Profiles.empty() && fixture.Vault.Stores.empty() && fixture.Vault.Deletes.empty(),
				   "startup replacement cannot touch either slot before the remembered binding loads");
			effects.drain();
			fixture.Lane.drain();
			expect(!store.Rows.front().Tombstone && fixture.Vault.Deletes.empty() && fixture.Provider.Profiles.empty(),
				   "startup replacement waits for durable tombstone before session effects");
			effects.drain();
			fixture.Lane.drain();
			expect(store.Rows.front().Tombstone && fixture.Vault.Deletes.size() == 1U
					   && fixture.Vault.Deletes.front().Context.Authority == record.Authority && fixture.Provider.Profiles.empty(),
				   "startup replacement tombstones the exact old binding before access cleanup");
			const auto deletion = fixture.Vault.Deletes.front();
			deletion.Completion(application::github_credential_delete_success{ .Context = deletion.Context, .Disposition = application::github_credential_delete_disposition::deleted });
			fixture.Lane.drain();
			expect(fixture.Vault.RefreshDeletes.size() == 1U && fixture.Provider.Profiles.empty() && fixture.Vault.Stores.empty(),
				   "replacement effects wait for the old refresh delete to release the shared core request context");
			fixture.Vault.answer_held_refresh_delete();
			fixture.Lane.drain();
			expect(fixture.Vault.RefreshDeletes.size() == 1U && fixture.Vault.RefreshDeletes.front().Authority == record.Authority
					   && fixture.Provider.Profiles.size() == 1U && fixture.Provider.Viewers.empty() && fixture.Vault.Stores.empty(),
				   "only after ordered pair cleanup may the replacement resolve its own profile");
			expect(!fixture.Host.accepted_session(), "startup cleanup never accepts the retired device session");
			fixture.Host.shutdown();
			fixture.Lane.drain();
			expect(fixture.Host.is_quiescent(), "startup replacement cleanup drains");
		}
	}

	// [[BPR-1116]]. An accepted session's renewal that gives up keeps its
	// grant, as a launch renewal does: the session fails visibly with the
	// renewal's failure, the row stays live, both slots stay, an orderly quit
	// keeps it, and only Sign out or a new sign-in removes it.
	void test_an_accepted_renewal_that_gives_up_keeps_the_grant()
	{
		const auto reasonOf = [](const core::github_failure aFailure) {
			switch (aFailure) {
			case core::github_failure::network:
				return core::github_reason_code::network_unavailable;
			case core::github_failure::vault:
				return core::github_reason_code::vault_unavailable;
			default:
				return core::github_reason_code::authentication_failed;
			}
		};
		const auto profile = native_profile(core::host_profile_id{ 11U });
		for (const auto failure : { core::github_failure::network, core::github_failure::vault, core::github_failure::authentication }) {
			const auto    name = std::string{ failure == core::github_failure::network ? "network" : failure == core::github_failure::vault ? "vault"
																																			: "refused" };
			binding_store store;
			{
				host_fixture                            fixture{ application::github_credential_shutdown_policy::retain_accepted_device_grants };
				application::manual_executor            effects;
				application::device_binding_persistence persistence{ store, effects, fixture.Lane };
				fixture.Host.configure_device_persistence(persistence, [] { return std::int64_t{ 1000 }; });
				settle_grant(fixture);
				effects.drain();
				fixture.Lane.drain();
				expect(fixture.Host.accepted_device_binding_durable() && store.Rows.size() == 1U && !store.Rows.front().Tombstone, name + ": the accepted grant is remembered");
				const auto authority = *fixture.Host.accepted_credential_authority();
				fixture.Host.invalidate_device_grant(authority, fixture.Host.accepted_identity_generation(), failure);
				effects.drain();
				fixture.Lane.drain();
				expect(!fixture.Host.accepted_session() && !fixture.Host.state().PendingSession
						   && fixture.Projections.last_transition_update().Transition->Status.has_value()
						   && fixture.Projections.last_transition_update().Transition->Status->Status == core::github_status::session_failed,
					   name + ": a renewal that gives up ends its session visibly");
				expect(fixture.Vault.Deletes.empty() && fixture.Vault.RefreshDeletes.empty() && !store.Rows.front().Tombstone
						   && std::ranges::count(store.Operations, "erase") == 0,
					   name + ": and keeps the grant: no tombstone, and neither slot is erased");
				expect(fixture.Projections.kept_grant_reason() == std::optional{ reasonOf(failure) }, name + ": the failure says the grant was kept, with the renewal's own reason");
				if (failure == core::github_failure::authentication) {
					// A refused grant needs signing in again; Sign out removes it.
					fixture.Host.dispatch(core::deactivate_github_identity_intent{ });
					effects.drain();
					fixture.Lane.drain();
					expect(fixture.Vault.Deletes.size() == 1U && fixture.Vault.Deletes.front().Context.Authority == authority && store.Rows.front().Tombstone,
						   name + ": Sign out on the failed session erases the kept grant behind a tombstone");
					const auto deletion = fixture.Vault.Deletes.front();
					deletion.Completion(application::github_credential_delete_success{ .Context = deletion.Context, .Disposition = application::github_credential_delete_disposition::deleted });
					fixture.Lane.drain();
					expect(fixture.Vault.RefreshDeletes.size() == 1U && fixture.Vault.RefreshDeletes.front().Authority == authority, name + ": with its refresh slot");
					fixture.Host.shutdown();
					fixture.Lane.drain();
					expect(fixture.Host.is_quiescent(), name + ": Sign out drains");
					continue;
				}
				fixture.Host.shutdown();
				effects.drain();
				fixture.Lane.drain();
				expect(fixture.Vault.Deletes.empty() && fixture.Vault.RefreshDeletes.empty() && !store.Rows.front().Tombstone && fixture.Host.is_quiescent(),
					   name + ": an orderly quit keeps the grant too");
			}
			// The next launch reads the live row and presents the kept grant to
			// `/user` again.
			host_fixture                            next{ application::github_credential_shutdown_policy::retain_accepted_device_grants };
			application::manual_executor            effects;
			application::device_binding_persistence persistence{ store, effects, next.Lane };
			next.Host.configure_device_persistence(persistence, [] { return std::int64_t{ 1000 }; });
			next.Host.restore_device_profile(profile);
			effects.drain();
			next.Lane.drain();
			expect(next.Provider.Profiles.size() == 1U, name + ": the next launch restores the kept grant");
			const auto request = next.Provider.Profiles.front().Request;
			next.Provider.Profiles.front().Completion(application::github_profile_success{ .Request = request, .Authority = profile });
			next.Lane.drain();
			expect(next.Vault.Retrieves.size() == 1U && next.Vault.Retrieves.front().Context.Authority == store.Rows.front().Authority && next.Vault.Deletes.empty(),
				   name + ": and reads the kept access slot");
			next.Host.shutdown();
			effects.drain();
			next.Lane.drain();
		}
		// A grant that was never remembered -- its binding not stored yet --
		// ends the same way, and is not claimed as kept on this machine. It is
		// kept for this session only: the orderly quit removes it, as it
		// always has.
		{
			host_fixture                            fixture{ application::github_credential_shutdown_policy::retain_accepted_device_grants };
			binding_store                           store;
			application::manual_executor            effects;
			application::device_binding_persistence persistence{ store, effects, fixture.Lane };
			fixture.Host.configure_device_persistence(persistence, [] { return std::int64_t{ 1000 }; });
			establish_native_session(fixture, core::host_profile_id{ 11U }, core::github_account_id{ 51U }, std::nullopt, application::github_credential_provenance::device_grant, binding_lifetime());
			const auto authority = *fixture.Host.accepted_credential_authority();
			fixture.Host.invalidate_device_grant(authority, fixture.Host.accepted_identity_generation(), core::github_failure::network);
			fixture.Lane.drain();
			expect(!fixture.Host.accepted_session() && fixture.Vault.Deletes.empty() && !fixture.Projections.kept_grant_reason().has_value(),
				   "unremembered: the session ends, nothing is erased at once, and nothing is claimed as kept");
			fixture.Host.shutdown();
			effects.drain();
			fixture.Lane.drain();
			expect(fixture.Vault.Deletes.size() == 1U && fixture.Vault.Deletes.front().Context.Authority == authority, "unremembered: the orderly quit removes a grant kept for this session only");
			const auto deletion = fixture.Vault.Deletes.front();
			deletion.Completion(application::github_credential_delete_success{ .Context = deletion.Context, .Disposition = application::github_credential_delete_disposition::deleted });
			fixture.Lane.drain();
			expect(fixture.Host.is_quiescent(), "unremembered: the quit drains");
		}
		host_fixture fixture{ application::github_credential_shutdown_policy::retain, true };
		settle_grant(fixture);
		const auto authority  = *fixture.Host.accepted_credential_authority();
		const auto generation = fixture.Host.accepted_identity_generation();
		auto       foreign    = authority;
		foreign.Account       = core::github_account_id{ authority.Account.value() + 1U };
		fixture.Host.invalidate_device_grant(foreign, generation, core::github_failure::authentication);
		fixture.Host.invalidate_device_grant(authority, core::github_identity_generation{ generation.value() + 1U }, core::github_failure::authentication);
		expect(fixture.Host.accepted_session().has_value() && fixture.Vault.Deletes.empty(),
			   "foreign account or stale generation renewal failure cannot end the accepted grant");
		fixture.Projections.OnNextOutput = [&fixture, authority, generation] {
			fixture.Host.invalidate_device_grant(authority, generation, core::github_failure::authentication);
		};
		fixture.Host.activate_public_preview(authority.Profile);
		fixture.Lane.drain();
		expect(fixture.Host.state().ActiveProfile == authority.Profile && fixture.Host.state().AccessMode == core::github_access_mode::public_preview,
			   "a renewal failure deferred during replacement cannot end the replacement identity");
		fixture.Host.shutdown();
		const auto deletion = fixture.Vault.Deletes.front();
		deletion.Completion(application::github_credential_delete_success{ .Context = deletion.Context, .Disposition = application::github_credential_delete_disposition::deleted });
		fixture.Lane.drain();
		expect(fixture.Host.is_quiescent(), "retired renewal cleanup drains");
	}

	// [[BPR-1116]]. A restored grant whose refresh slot is missing stays
	// signed in on its access token, says it does not renew itself, and is
	// not erased; Sign out removes it.
	void test_a_restored_grant_without_a_refresh_slot_stays_signed_in()
	{
		const auto                               profile = native_profile(core::host_profile_id{ 11U });
		const application::device_binding_record record{ .Authority = { .Profile = profile.Profile, .ProfileRevision = profile.ProfileRevision, .Host = profile.Host, .Hostname = profile.Hostname, .Account = core::github_account_id{ 51U }, .ServerContract = profile.ServerContract }, .Lifetime = binding_lifetime() };
		host_fixture                             fixture{ application::github_credential_shutdown_policy::retain_accepted_device_grants };
		binding_store                            store;
		store.Rows = { record };
		application::manual_executor            effects;
		application::device_binding_persistence persistence{ store, effects, fixture.Lane };
		fixture.Host.configure_device_persistence(persistence, [] { return std::int64_t{ 1000 }; });
		fixture.Vault.RefreshAnswer = fake_vault::refresh_answer::absent;
		fixture.Host.restore_device_profile(profile);
		effects.drain();
		fixture.Lane.drain();
		const auto request = fixture.Provider.Profiles.front().Request;
		fixture.Provider.Profiles.front().Completion(application::github_profile_success{ .Request = request, .Authority = profile });
		fixture.Lane.drain();
		const auto retrieve = fixture.Vault.Retrieves.front();
		retrieve.Completion(application::github_credential_retrieve_success{ .Context = retrieve.Context, .Credential = credential() });
		fixture.Lane.drain();
		const auto account = fixture.Provider.Viewers.front().Request;
		fixture.Provider.Viewers.front().Completion(application::github_viewer_success{ .Request = account, .Viewer = viewer(record.Authority.Account), .RateLimits = rate_limits_for(application::github_rate_limit_resource::core), .AccountTransport = std::make_shared<fake_lease>() });
		fixture.Lane.drain();
		const auto capabilities = fixture.Provider.Capabilities.front().Request;
		fixture.Provider.Capabilities.front().Completion(application::github_capability_success{ .Request = capabilities, .Facts = capability_facts(), .SchemaFacts = schema_facts(), .RateLimits = rate_limits_for(application::github_rate_limit_resource::graphql) });
		fixture.Lane.drain();
		effects.drain();
		fixture.Lane.drain();
		expect(fixture.Vault.RefreshRetrieves.size() == 1U && fixture.Vault.RefreshRetrieves.front().Authority == record.Authority, "the accepted restored grant asks whether its refresh slot is there");
		expect(fixture.Host.accepted_session().has_value() && fixture.Host.accepted_device_binding_durable() && !fixture.Host.accepted_credential_renewable(),
			   "a missing refresh slot does not sign the restored session out");
		expect(fixture.Vault.Deletes.empty() && fixture.Vault.RefreshDeletes.empty() && !store.Rows.front().Tombstone && std::ranges::count(store.Operations, "erase") == 0,
			   "and does not erase the grant");
		const auto& grant = fixture.Projections.last_session_update().Session->Grant;
		expect(grant.has_value() && grant->Kind == application::github_grant_kind_projection::device_grant && !grant->Renews && grant->Remembered,
			   "the card says the grant does not renew itself and is remembered");
		fixture.Host.dispatch(core::deactivate_github_identity_intent{ });
		effects.drain();
		fixture.Lane.drain();
		expect(fixture.Vault.Deletes.size() == 1U && fixture.Vault.Deletes.front().Context.Authority == record.Authority && store.Rows.front().Tombstone,
			   "Sign out removes it behind a tombstone");
		const auto deletion = fixture.Vault.Deletes.front();
		deletion.Completion(application::github_credential_delete_success{ .Context = deletion.Context, .Disposition = application::github_credential_delete_disposition::deleted });
		fixture.Lane.drain();
		fixture.Host.shutdown();
		fixture.Lane.drain();
		expect(fixture.Host.is_quiescent(), "the restored session without a refresh slot drains");
	}

	void test_binding_load_cancellation_and_stale_terminal()
	{
		binding_store                           store;
		application::manual_executor            effects;
		application::manual_executor            lane;
		application::device_binding_persistence persistence{ store, effects, lane };
		std::size_t                             callbacks = 0;
		for (std::size_t index = 0; index < 65U; ++index) {
			persistence.load([&callbacks](application::device_binding_load_result) { ++callbacks; });
		}
		expect(!persistence.is_quiescent(), "saturation refusal remains counted until callback");
		persistence.cancel_pending();
		while (!persistence.is_quiescent()) {
			effects.drain();
			lane.drain();
		}
		expect(callbacks == 65U, "every accepted and refused load has exactly one terminal");
		{
			auto                                    fixture = std::make_unique<host_fixture>();
			application::device_binding_persistence hostPersistence{ store, effects, fixture->Lane };
			fixture->Host.configure_device_persistence(hostPersistence, [] { return std::int64_t{ 1000 }; });
			fixture->Host.restore_device_profile(native_profile(core::host_profile_id{ 11U }));
			fixture->Host.shutdown();
			expect(!fixture->Host.is_quiescent(), "load is part of host shutdown quiescence");
			effects.drain();
			fixture->Lane.drain();
			expect(fixture->Host.is_quiescent() && fixture->Provider.Profiles.empty(), "cancelled startup load cannot activate profile");
		}
	}

	void test_accepted_server_snapshots_fan_out_without_window_effects()
	{
		host_fixture fixture;
		establish_native_session(fixture);
		std::vector<std::shared_ptr<application::github_window_read_host>> windows;
		std::array<recording_epoch_handoffs, 3>                            epochs;
		std::array<std::size_t, 3>                                         publications{ };
		bool                                                               detachDuringPublication = false;
		std::function<void()>                                              supersedeDuringPublication;
		for (std::size_t index = 0; index < 3; ++index) {
			windows.push_back(std::make_shared<application::github_window_read_host>(
				core::bpr_window_context{ core::default_bpr_profile_id, core::bpr_window_id{ index + 1 }, core::bpr_window_generation{ 1 } },
				core::application_session_id{ 200 + index }, fixture.Provider, fixture.Lane,
				[&, index](const application::github_window_read_update& aUpdate) {
					if (aUpdate.Presentation.PullRequest) {
						++publications[index];
					}
					fixture.Host.observe_attached_read(windows[index], aUpdate.Transition.AcceptedSnapshot.has_value());
					if (index == 1 && supersedeDuringPublication && aUpdate.Presentation.PullRequest) {
						auto supersede = std::exchange(supersedeDuringPublication, { });
						supersede();
					}
					if (index == 1 && detachDuringPublication && aUpdate.Presentation.PullRequest) {
						detachDuringPublication = false;
						fixture.Host.detach_window(windows[2]->window());
					}
				},
				&epochs[index]));
			expect(fixture.Host.attach_window(windows.back()), "profile admits explicit snapshot subscribers");
		}
		const auto complete = [&](const std::size_t aIndex, const std::uint32_t aNumber, const std::uint64_t aRevision) {
			windows[aIndex]->dispatch(core::open_pull_request_intent{ { "openai", "better-pull-review" }, core::pull_request_number{ aNumber } });
			const auto read  = fixture.Provider.PullRequests.back();
			auto       value = rich_snapshot(read.Request);
			value.Snapshot   = core::provider_snapshot_id{ aRevision };
			read.Completion(application::github_pull_request_success{ read.Request, value });
			fixture.Lane.drain();
		};
		complete(0, 27, 100);
		complete(1, 27, 101);
		complete(2, 28, 102);
		const auto unrelated            = windows[2]->accepted_snapshot();
		const auto siblingState         = windows[1]->state();
		const auto siblingEpochs        = epochs[1].Accepted.size();
		const auto siblingInvalidations = epochs[1].Invalidated.size();
		for (std::uint64_t revision = 103; revision < 106; ++revision) {
			const auto beforeReads = fixture.Provider.PullRequests.size();
			windows[0]->dispatch(core::refresh_pull_request_intent{ });
			const auto read                                       = fixture.Provider.PullRequests.back();
			auto       value                                      = rich_snapshot(read.Request);
			value.Snapshot                                        = core::provider_snapshot_id{ revision };
			value.IssueComments.front().Body                      = "Edited server post " + std::to_string(revision);
			value.ReviewThreads.front().Resolved                  = revision % 2U != 0U;
			value.ReviewThreads.front().ViewerResolvePermission   = value.ReviewThreads.front().Resolved
				? application::github_permission_state::denied
				: application::github_permission_state::allowed;
			value.ReviewThreads.front().ViewerUnresolvePermission = value.ReviewThreads.front().Resolved
				? application::github_permission_state::allowed
				: application::github_permission_state::denied;
			if (revision == 103) {
				auto added    = value.IssueComments.front();
				added.Comment = application::github_comment_id{ "new-server-post" };
				value.IssueComments.push_back(added);
			} else if (revision == 105) {
				value.IssueComments.clear();
			}
			read.Completion(application::github_pull_request_success{ read.Request, value });
			fixture.Lane.drain();
			expect(windows[1]->accepted_snapshot() == windows[0]->accepted_snapshot()
					   && windows[0]->accepted_snapshot() == value,
				   "added, edited, deleted and resolve/reopen server observations reach sibling");
			auto expected             = siblingState;
			expected.AcceptedSnapshot = value.Snapshot;
			expect(windows[1]->state() == expected && epochs[1].Accepted.size() == siblingEpochs
					   && epochs[1].Invalidated.size() == siblingInvalidations
					   && fixture.Provider.PullRequests.size() == beforeReads + 1,
				   "fanout preserves sibling requests, navigation and epoch authority with no network loop");
			expect(windows[2]->accepted_snapshot() == unrelated, "different PR cannot receive the shared observation");
		}
		windows[1]->dispatch(core::open_pull_request_intent{ { "openai", "better-pull-review" }, core::pull_request_number{ 29 } });
		const auto pending = fixture.Provider.PullRequests.back();
		complete(0, 27, 106);
		expect(!windows[1]->accepted_snapshot(), "pending navigation cannot be overwritten by a sibling");
		pending.Completion(application::github_pull_request_cancelled{ pending.Request });
		fixture.Lane.drain();
		complete(1, 27, 107);
		complete(2, 27, 108);
		supersedeDuringPublication = [&] { complete(2, 27, 110); };
		complete(0, 27, 109);
		for (const auto& window : windows) {
			expect(window->accepted_snapshot()->Snapshot == core::provider_snapshot_id{ 110 },
				   "a reentrant newer server observation supersedes the outer fanout in every window");
		}
		const auto thirdPublications = publications[2];
		detachDuringPublication      = true;
		complete(0, 27, 111);
		expect(!windows[2]->accepted_snapshot() && publications[2] == thirdPublications,
			   "reentrant detach invalidates a captured recipient before delivery");
		fixture.Host.shutdown();
		fixture.Lane.drain();
		expect(fixture.Host.is_quiescent(), "server snapshot sharing creates no additional lifetime work");
	}

	void test_profile_authentication_shared_by_independent_windows()
	{
		host_fixture fixture;
		const auto   transport    = establish_native_session(fixture);
		const auto   profileReads = fixture.Provider.Profiles.size();
		auto         sibling      = std::make_shared<application::github_window_read_host>(
			core::bpr_window_context{ core::default_bpr_profile_id, core::bpr_window_id{ 2 }, core::bpr_window_generation{ 1 } },
			core::application_session_id{ 200 }, fixture.Provider, fixture.Lane,
			[](const application::github_window_read_update&) { });
		expect(fixture.Host.attach_window(sibling), "authenticated profile admits second window without authentication");
		expect(!fixture.Host.attach_window(sibling), "duplicate window owner is refused");
		fixture.Host.dispatch(core::open_pull_request_intent{ { "openai", "better-pull-review" }, core::pull_request_number{ 27 } });
		sibling->dispatch(core::open_pull_request_intent{ { "openai", "better-pull-review" }, core::pull_request_number{ 28 } });
		expect(fixture.Provider.PullRequests.size() == 2 && fixture.Provider.Profiles.size() == profileReads,
			   "two windows issue independent reads through one authenticated profile");
		const auto first  = fixture.Provider.PullRequests[0];
		const auto second = fixture.Provider.PullRequests[1];
		expect(first.Request.AccountTransport == second.Request.AccountTransport
				   && first.Request.Context.ApplicationSession != second.Request.Context.ApplicationSession,
			   "account transport is shared but read correlation is window-local");
		second.Completion(application::github_pull_request_success{ second.Request, snapshot(second.Request) });
		fixture.Lane.drain();
		expect(sibling->accepted_snapshot().has_value() && !fixture.Host.accepted_snapshot(),
			   "second window completion cannot install first window snapshot");
		first.Completion(application::github_pull_request_success{ first.Request, snapshot(first.Request) });
		fixture.Lane.drain();
		expect(fixture.Host.accepted_snapshot() && sibling->accepted_snapshot()
				   && fixture.Host.accepted_snapshot()->PullRequest.Number != sibling->accepted_snapshot()->PullRequest.Number,
			   "interleaved snapshots retain independent selected pull requests");
		const auto primaryLease = fixture.Host.accepted_review_mutation_transport();
		const auto siblingLease = sibling->mutation_transport();
		fixture.Host.detach_window({ core::default_bpr_profile_id, core::bpr_window_id{ 1 }, core::bpr_window_generation{ 1 } });
		expect(!primaryLease->active() && siblingLease->active() && fixture.Host.accepted_session(),
			   "closing initial read revokes only its lease and leaves shared authentication alive");
		sibling->dispatch(core::refresh_pull_request_intent{ });
		const auto late = fixture.Provider.PullRequests.back();
		fixture.Host.dispatch(core::deactivate_github_identity_intent{ });
		expect(!siblingLease->active() && !sibling->accepted_snapshot(),
			   "profile signout synchronously retires every attached read and lease");
		late.Completion(application::github_pull_request_success{ late.Request, snapshot(late.Request) });
		fixture.Lane.drain();
		expect(!sibling->accepted_snapshot() && sibling->is_quiescent(),
			   "late shared-session read cannot revive retired window authority");
		expect(transport->MutationLeases.size() == 2, "windows acquired distinct mutation leases from shared transport");
		const auto deletion = fixture.Vault.Deletes.back();
		deletion.Completion(application::github_credential_delete_success{ deletion.Context,
																		   application::github_credential_delete_disposition::deleted });
		fixture.Lane.drain();
		fixture.Provider.Profiles.clear();
		fixture.Provider.Viewers.clear();
		fixture.Provider.Capabilities.clear();
		fixture.Vault.Stores.clear();
		fixture.Vault.Retrieves.clear();
		fixture.Projections.Updates.clear();
		establish_native_session(fixture, core::host_profile_id{ 11 }, core::github_account_id{ 51 }, std::nullopt, application::github_credential_provenance::manual, std::nullopt, false);
		expect(sibling->state().Account == viewer().Account && sibling->state().IdentityGeneration == fixture.Host.accepted_identity_generation(),
			   "same authority reauthentication broadcasts a fresh identity to retained sibling");
		const auto readsBefore = fixture.Provider.PullRequests.size();
		sibling->dispatch(core::open_pull_request_intent{ { "openai", "better-pull-review" }, core::pull_request_number{ 29 } });
		expect(fixture.Provider.PullRequests.size() == readsBefore + 1, "reauthenticated sibling can issue a new independent read");
		const auto reopened = fixture.Provider.PullRequests.back();
		fixture.Host.dispatch(core::deactivate_github_identity_intent{ });
		const auto replacementDelete = fixture.Vault.Deletes.back();
		replacementDelete.Completion(application::github_credential_delete_success{ replacementDelete.Context,
																					application::github_credential_delete_disposition::deleted });
		fixture.Lane.drain();
		fixture.Provider.Profiles.clear();
		fixture.Provider.Viewers.clear();
		fixture.Provider.Capabilities.clear();
		fixture.Vault.Stores.clear();
		fixture.Vault.Retrieves.clear();
		fixture.Projections.Updates.clear();
		establish_native_session(fixture, core::host_profile_id{ 12 }, core::github_account_id{ 52 }, ghes_native_profile(core::host_profile_id{ 12 }),
								 application::github_credential_provenance::manual, std::nullopt, false);
		expect(sibling->state().Account == core::github_account_id{ 52 }
				   && sibling->state().ActiveProfile == core::host_profile_id{ 12 },
			   "exact retirement authorizes retained window to follow accepted account and server-profile replacement");
		reopened.Completion(application::github_pull_request_success{ reopened.Request, snapshot(reopened.Request) });
		fixture.Lane.drain();
		expect(!sibling->accepted_snapshot(), "old account completion cannot publish into rebound attachment");
		sibling->dispatch(core::open_pull_request_intent{ { "openai", "better-pull-review" }, core::pull_request_number{ 30 } });
		const auto replacementRead = fixture.Provider.PullRequests.back();
		expect(replacementRead.Request.Context.Account == core::github_account_id{ 52 },
			   "rebound attachment issues reads only with replacement account authority");
		fixture.Host.detach_window(sibling->window());
		sibling.reset();
		fixture.Host.shutdown();
		fixture.Lane.drain();
		expect(!fixture.Host.is_quiescent(), "detached pending read remains counted after external owner releases it");
		replacementRead.Completion(application::github_pull_request_cancelled{ replacementRead.Request });
		fixture.Lane.drain();
		expect(fixture.Host.is_quiescent(), "profile shutdown drains detached terminal before releasing shared ports");
	}

	void test_signed_out_explicit_windows_adopt_authentication_at_capacity()
	{
		host_fixture                                                       fixture;
		std::vector<std::shared_ptr<application::github_window_read_host>> windows;
		for (std::uint64_t index = 1; index <= 33; ++index) {
			auto window = std::make_shared<application::github_window_read_host>(
				core::bpr_window_context{ core::default_bpr_profile_id, core::bpr_window_id{ index }, core::bpr_window_generation{ 1 } },
				core::application_session_id{ 1000 + index }, fixture.Provider, fixture.Lane,
				[](const application::github_window_read_update&) { });
			expect(fixture.Host.attach_window(window) == (index <= 32), "signed-out profile admits exactly 32 explicit windows");
			windows.push_back(std::move(window));
		}
		expect(!fixture.Host.attach_window(windows.front()), "duplicate signed-out attachment is refused");
		establish_native_session(fixture);
		for (std::size_t index = 0; index < 32; ++index) {
			expect(windows[index]->state().Account == viewer().Account, "first accepted authentication reaches every signed-out attachment");
		}
		fixture.Host.dispatch(core::open_pull_request_intent{ { "openai", "better-pull-review" }, core::pull_request_number{ 26 } });
		expect(fixture.Provider.PullRequests.empty(), "legacy read cannot create a thirty-third consumer or alias explicit first window");
		windows[0]->dispatch(core::open_pull_request_intent{ { "openai", "better-pull-review" }, core::pull_request_number{ 27 } });
		windows[1]->dispatch(core::open_pull_request_intent{ { "openai", "better-pull-review" }, core::pull_request_number{ 28 } });
		expect(fixture.Provider.PullRequests.size() == 2, "authenticated windows have independent read effects");
		const auto first  = fixture.Provider.PullRequests[0];
		const auto second = fixture.Provider.PullRequests[1];
		expect(first.Request.AccountTransport == second.Request.AccountTransport
				   && first.Request.Context.ApplicationSession != second.Request.Context.ApplicationSession,
			   "explicit windows share authentication with distinct request identities");
		fixture.Host.detach_window(windows.front()->window());
		fixture.Host.detach_window(windows.front()->window());
		fixture.Host.detach_window({ core::bpr_profile_id{ 2 }, core::bpr_window_id{ 2 }, core::bpr_window_generation{ 1 } });
		fixture.Host.detach_window({ core::default_bpr_profile_id, core::bpr_window_id{ 2 }, core::bpr_window_generation{ 2 } });
		first.Completion(application::github_pull_request_success{ first.Request, snapshot(first.Request) });
		second.Completion(application::github_pull_request_success{ second.Request, snapshot(second.Request) });
		fixture.Lane.drain();
		expect(!windows[0]->accepted_snapshot() && windows[1]->accepted_snapshot(), "exact first detach and stale foreign detaches preserve sibling authority");
		fixture.Host.dispatch(core::open_pull_request_intent{ { "openai", "better-pull-review" }, core::pull_request_number{ 29 } });
		expect(fixture.Provider.PullRequests.size() == 2, "retired explicit first identity cannot reenter through compatibility reader");
		fixture.Host.shutdown();
		expect(!fixture.Host.attach_window(windows.back()), "shutdown refuses a fresh attachment");
		fixture.Lane.drain();
		expect(fixture.Host.is_quiescent(), "explicit windows drain after profile shutdown");
	}

	void test_explicit_first_detach_preserves_existing_legacy_reader()
	{
		host_fixture fixture;
		establish_native_session(fixture);
		fixture.Host.dispatch(core::open_pull_request_intent{ { "openai", "better-pull-review" }, core::pull_request_number{ 27 } });
		const auto legacy = fixture.Provider.PullRequests.back();
		legacy.Completion(application::github_pull_request_success{ legacy.Request, snapshot(legacy.Request) });
		fixture.Lane.drain();
		const auto lease  = fixture.Host.accepted_review_mutation_transport();
		auto       window = std::make_shared<application::github_window_read_host>(
			core::bpr_window_context{ core::default_bpr_profile_id, core::bpr_window_id{ 1 }, core::bpr_window_generation{ 1 } },
			core::application_session_id{ 200 }, fixture.Provider, fixture.Lane,
			[](const application::github_window_read_update&) { });
		expect(fixture.Host.attach_window(window), "explicit first window can coexist with established compatibility reader");
		fixture.Host.detach_window(window->window());
		fixture.Host.detach_window(window->window());
		expect(fixture.Host.accepted_snapshot() && lease->active(), "explicit and repeated stale detach never closes legacy reader");
		fixture.Host.dispatch(core::refresh_pull_request_intent{ });
		expect(fixture.Provider.PullRequests.size() == 2, "legacy reader remains admitted after explicit detach");
		const auto refresh = fixture.Provider.PullRequests.back();
		fixture.Host.shutdown();
		refresh.Completion(application::github_pull_request_cancelled{ refresh.Request });
		fixture.Lane.drain();
		expect(fixture.Host.is_quiescent(), "compatibility and explicit readers drain together");
	}

	void test_pending_primary_read_blocks_profile_quiescence()
	{
		host_fixture fixture;
		establish_native_session(fixture);
		fixture.Host.dispatch(core::open_pull_request_intent{ { "openai", "better-pull-review" }, core::pull_request_number{ 27 } });
		const auto pending = fixture.Provider.PullRequests.back();
		fixture.Host.shutdown();
		fixture.Lane.drain();
		expect(!fixture.Host.is_quiescent(), "pending primary read remains part of profile lifetime");
		pending.Completion(application::github_pull_request_cancelled{ pending.Request });
		fixture.Lane.drain();
		expect(fixture.Host.is_quiescent(), "primary cancellation terminal completes profile drain");
	}

	void test_restored_windows_admit_descending_ids_once_before_fresh_windows()
	{
		host_fixture fixture;
		const auto   window = [&](std::uint64_t aId, std::uint64_t aGeneration) {
			return std::make_shared<application::github_window_read_host>(
				core::bpr_window_context{ core::default_bpr_profile_id, core::bpr_window_id{ aId }, core::bpr_window_generation{ aGeneration } },
				core::application_session_id{ 1000 + aId }, fixture.Provider, fixture.Lane,
				[](const application::github_window_read_update&) { });
		};
		auto second = window(2, 2);
		auto first  = window(1, 2);
		expect(fixture.Host.attach_window(second) && fixture.Host.attach_window(first), "restored MRU order2,1 retains exact IDs");
		establish_native_session(fixture);
		fixture.Host.detach_window(first->window());
		fixture.Host.dispatch(core::open_pull_request_intent{ { "openai", "better-pull-review" }, core::pull_request_number{ 30 } });
		expect(fixture.Provider.PullRequests.empty(), "retired restored ID1 cannot reenter as an implicit legacy reader");
		fixture.Host.detach_window(second->window());
		expect(!fixture.Host.attach_window(window(2, 2)), "retired restored context cannot reattach through a new reader");
		expect(!fixture.Host.attach_window(window(2, 3)), "retired restored stable ID cannot change generation to reattach");
		expect(!fixture.Host.attach_window(window(2, 1)), "restored highwater rejects fresh reuse");
		auto fresh = window(3, 1);
		expect(fixture.Host.attach_window(fresh), "fresh window follows maximum restored ID");
		fixture.Host.detach_window(fresh->window());
		expect(!fixture.Host.attach_window(window(3, 2)), "retired fresh ID cannot masquerade as restored");
		expect(!fixture.Host.attach_window(window(4, 2)), "fresh admission permanently seals restoration phase");
		expect(fixture.Host.attach_window(window(4, 1)), "fresh monotonic admission remains available");
		fixture.Host.shutdown();
		fixture.Lane.drain();
		expect(fixture.Host.is_quiescent(), "restored and fresh windows drain together");
	}

	void test_restored_window_history_is_bounded_independently_of_live_capacity()
	{
		host_fixture fixture;
		const auto   window = [&](std::uint64_t aId, std::uint64_t aGeneration) {
			return std::make_shared<application::github_window_read_host>(
				core::bpr_window_context{ core::default_bpr_profile_id, core::bpr_window_id{ aId }, core::bpr_window_generation{ aGeneration } },
				core::application_session_id{ 1000 + aId }, fixture.Provider, fixture.Lane,
				[](const application::github_window_read_update&) { });
		};
		for (std::uint64_t id = 32; id > 0; --id) {
			expect(fixture.Host.attach_window(window(id, 2)), "all32 restored windows admit in descending order");
		}
		expect(!fixture.Host.attach_window(window(33, 2)), "restoration respects live capacity");
		fixture.Host.detach_window({ core::default_bpr_profile_id, core::bpr_window_id{ 1 }, core::bpr_window_generation{ 2 } });
		expect(!fixture.Host.attach_window(window(33, 2)), "retirement does not grow the bounded restored history");
		expect(fixture.Host.attach_window(window(33, 1)), "fresh window uses released live capacity after32 restores");
		fixture.Host.shutdown();
		fixture.Lane.drain();
		expect(fixture.Host.is_quiescent(), "full restored history does not prevent shutdown");
	}

	void test_profile_window_attachment_churn_is_bounded_by_live_consumers()
	{
		host_fixture fixture;
		establish_native_session(fixture);
		for (std::uint64_t index = 2; index < 70; ++index) {
			auto window = std::make_shared<application::github_window_read_host>(
				core::bpr_window_context{ core::default_bpr_profile_id, core::bpr_window_id{ index }, core::bpr_window_generation{ 1 } },
				core::application_session_id{ 1000 + index }, fixture.Provider, fixture.Lane,
				[](const application::github_window_read_update&) { });
			expect(fixture.Host.attach_window(window), "sequential closed windows do not exhaust live attachment capacity");
			fixture.Host.detach_window(window->window());
			expect(!fixture.Host.attach_window(window), "retired stable window identity cannot be attached again");
		}
	}

}

auto main() -> int
{
	bpr::tests::report_qualification_build();
	bpr::tests::run_qualification_case("test_accepted_server_snapshots_fan_out_without_window_effects", &test_accepted_server_snapshots_fan_out_without_window_effects);
	bpr::tests::run_qualification_case("test_signed_out_explicit_windows_adopt_authentication_at_capacity", &test_signed_out_explicit_windows_adopt_authentication_at_capacity);
	bpr::tests::run_qualification_case("test_explicit_first_detach_preserves_existing_legacy_reader", &test_explicit_first_detach_preserves_existing_legacy_reader);
	bpr::tests::run_qualification_case("test_profile_authentication_shared_by_independent_windows", &test_profile_authentication_shared_by_independent_windows);
	bpr::tests::run_qualification_case("test_profile_window_attachment_churn_is_bounded_by_live_consumers", &test_profile_window_attachment_churn_is_bounded_by_live_consumers);
	bpr::tests::run_qualification_case("test_restored_windows_admit_descending_ids_once_before_fresh_windows", &test_restored_windows_admit_descending_ids_once_before_fresh_windows);
	bpr::tests::run_qualification_case("test_restored_window_history_is_bounded_independently_of_live_capacity", &test_restored_window_history_is_bounded_independently_of_live_capacity);
	bpr::tests::run_qualification_case("test_pending_primary_read_blocks_profile_quiescence", &test_pending_primary_read_blocks_profile_quiescence);
	bpr::tests::run_qualification_case("test_bpr_profile_credential_admission", &test_bpr_profile_credential_admission);
	bpr::tests::run_qualification_case("test_bootstrap_failure_stages_are_exact_and_replacement_safe", &test_bootstrap_failure_stages_are_exact_and_replacement_safe);
	bpr::tests::run_qualification_case("test_local_capability_refusal_details", &test_local_capability_refusal_details);
	bpr::tests::run_qualification_case("test_failed_startup_load_keeps_repeated_replacement_blocked", &test_failed_startup_load_keeps_repeated_replacement_blocked);
	bpr::tests::run_qualification_case("test_sign_out_during_binding_load_erases_the_loaded_authority", &test_sign_out_during_binding_load_erases_the_loaded_authority);
	bpr::tests::run_qualification_case("test_replacement_during_binding_load_waits_for_exact_cleanup", &test_replacement_during_binding_load_waits_for_exact_cleanup);
	bpr::tests::run_qualification_case("test_startup_cleanup_refuses_a_mismatched_compiled_profile", &test_startup_cleanup_refuses_a_mismatched_compiled_profile);
	bpr::tests::run_qualification_case("test_an_accepted_renewal_that_gives_up_keeps_the_grant", &test_an_accepted_renewal_that_gives_up_keeps_the_grant);
	bpr::tests::run_qualification_case("test_a_restored_grant_without_a_refresh_slot_stays_signed_in", &test_a_restored_grant_without_a_refresh_slot_stays_signed_in);
	bpr::tests::run_qualification_case("test_restore_device_profile_exact_predicate_cancels_before_authentication", &test_restore_device_profile_exact_predicate_cancels_before_authentication);
	bpr::tests::run_qualification_case("test_restore_device_profile_bounds_pending_and_delivering_terminals", &test_restore_device_profile_bounds_pending_and_delivering_terminals);
	bpr::tests::run_qualification_case("test_restore_device_profile_has_one_terminal_for_missing_invalid_and_ready", &test_restore_device_profile_has_one_terminal_for_missing_invalid_and_ready);
	bpr::tests::run_qualification_case("test_restore_device_profile_renews_an_access_expired_grant_before_its_session", &test_restore_device_profile_renews_an_access_expired_grant_before_its_session);
	bpr::tests::run_qualification_case("test_restored_binding_launch_failures_keep_the_grant", &test_restored_binding_launch_failures_keep_the_grant);
	bpr::tests::run_qualification_case("test_restore_device_profile_refusal_reentrancy_and_shutdown_terminals", &test_restore_device_profile_refusal_reentrancy_and_shutdown_terminals);
	bpr::tests::run_qualification_case("test_restore_device_profile_superseded_load_terminates_without_resuming", &test_restore_device_profile_superseded_load_terminates_without_resuming);
	bpr::tests::run_qualification_case("test_restored_binding_is_validated_before_resume", &test_restored_binding_is_validated_before_resume);
	bpr::tests::run_qualification_case("test_device_binding_persistence_barriers", &test_device_binding_persistence_barriers);
	bpr::tests::run_qualification_case("test_device_binding_persistence_stays_drainable_when_its_posts_are_refused", &test_device_binding_persistence_stays_drainable_when_its_posts_are_refused);
	bpr::tests::run_qualification_case("test_a_withdrawn_binding_store_fails_like_a_refused_one", &test_a_withdrawn_binding_store_fails_like_a_refused_one);
	bpr::tests::run_qualification_case("test_a_withdrawn_binding_tombstone_fails_like_a_refused_one", &test_a_withdrawn_binding_tombstone_fails_like_a_refused_one);
	bpr::tests::run_qualification_case("test_the_rotation_barrier_writes_nothing", &test_the_rotation_barrier_writes_nothing);
	bpr::tests::run_qualification_case("test_a_withdrawal_at_shutdown_runs_its_failure", &test_a_withdrawal_at_shutdown_runs_its_failure);
	bpr::tests::run_qualification_case("test_a_shutdown_that_fails_after_a_withdrawal_still_runs_its_failure", &test_a_shutdown_that_fails_after_a_withdrawal_still_runs_its_failure);
	bpr::tests::run_qualification_case("test_a_failed_allocation_in_start_delete_changes_nothing", &test_a_failed_allocation_in_start_delete_changes_nothing);
	bpr::tests::run_qualification_case("test_a_withdrawn_launch_restore_erase_is_not_left_counted", &test_a_withdrawn_launch_restore_erase_is_not_left_counted);
	bpr::tests::run_qualification_case("test_binding_load_cancellation_and_stale_terminal", &test_binding_load_cancellation_and_stale_terminal);
	bpr::tests::run_qualification_case("test_setup_authentication_preserves_review_until_exact_acceptance", &test_setup_authentication_preserves_review_until_exact_acceptance);
	bpr::tests::run_qualification_case("test_setup_acceptance_reuses_validated_bootstrap", &test_setup_acceptance_reuses_validated_bootstrap);
	bpr::tests::run_qualification_case("test_setup_restage_same_destination_has_fresh_attempt", &test_setup_restage_same_destination_has_fresh_attempt);
	bpr::tests::run_qualification_case("test_enrollment_acceptance_snapshot_and_exact_delete", &test_enrollment_acceptance_snapshot_and_exact_delete);
	bpr::tests::run_qualification_case("test_accepted_publication_account", &test_accepted_publication_account);
	bpr::tests::run_qualification_case("test_retained_mutation_lease_is_revoked_on_shutdown", &test_retained_mutation_lease_is_revoked_on_shutdown);
	bpr::tests::run_qualification_case("test_accepted_snapshot_handoff_and_exact_invalidation", &test_accepted_snapshot_handoff_and_exact_invalidation);
	bpr::tests::run_qualification_case("test_running_idleness_is_distinct_from_shutdown_quiescence", &test_running_idleness_is_distinct_from_shutdown_quiescence);
	bpr::tests::run_qualification_case("test_success_projection_is_safe_atomic_and_conversation_complete", &test_success_projection_is_safe_atomic_and_conversation_complete);
	bpr::tests::run_qualification_case("test_unanchored_line_thread_projects_without_a_guessed_line", &test_unanchored_line_thread_projects_without_a_guessed_line);
	bpr::tests::run_qualification_case("test_every_provider_failure_maps_to_safe_pull_request_failure", &test_every_provider_failure_maps_to_safe_pull_request_failure);
	bpr::tests::run_qualification_case("test_retries_are_bounded_and_stale_work_is_cancelled", &test_retries_are_bounded_and_stale_work_is_cancelled);
	bpr::tests::run_qualification_case("test_retry_after_is_terminal_and_preserved", &test_retry_after_is_terminal_and_preserved);
	bpr::tests::run_qualification_case("test_wrong_correlations_and_submit_exceptions_fail_closed", &test_wrong_correlations_and_submit_exceptions_fail_closed);
	bpr::tests::run_qualification_case("test_rate_observation_presence_follows_server_contract", &test_rate_observation_presence_follows_server_contract);
	bpr::tests::run_qualification_case("test_no_graphql_read_rejects_graphql_error_observations", &test_no_graphql_read_rejects_graphql_error_observations);
	bpr::tests::run_qualification_case("test_projection_failures_cannot_interrupt_effects_or_cleanup", &test_projection_failures_cannot_interrupt_effects_or_cleanup);
	bpr::tests::run_qualification_case("test_projection_reentry_is_serialized_after_the_current_turn", &test_projection_reentry_is_serialized_after_the_current_turn);
	bpr::tests::run_qualification_case("test_invalid_enrollment_cannot_leak_into_a_later_activation", &test_invalid_enrollment_cannot_leak_into_a_later_activation);
	bpr::tests::run_qualification_case("test_snapshot_authority_and_pr_cancellation_fail_closed", &test_snapshot_authority_and_pr_cancellation_fail_closed);
	bpr::tests::run_qualification_case("test_new_open_clears_host_snapshot_but_refresh_retains_it", &test_new_open_clears_host_snapshot_but_refresh_retains_it);
	bpr::tests::run_qualification_case("test_vault_wrong_context_and_submit_throw_fail_closed", &test_vault_wrong_context_and_submit_throw_fail_closed);
	bpr::tests::run_qualification_case("test_reenrollment_waits_for_running_vault_cleanup", &test_reenrollment_waits_for_running_vault_cleanup);
	bpr::tests::run_qualification_case("test_bootstrap_lease_and_changed_account_fail_closed", &test_bootstrap_lease_and_changed_account_fail_closed);
	bpr::tests::run_qualification_case("test_shutdown_cancels_and_suppresses_late_completion", &test_shutdown_cancels_and_suppresses_late_completion);
	bpr::tests::run_qualification_case("test_shutdown_preserves_persistent_credentials", &test_shutdown_preserves_persistent_credentials);
	bpr::tests::run_qualification_case("test_shutdown_deletes_persistent_credentials_by_policy", &test_shutdown_deletes_persistent_credentials_by_policy);
	bpr::tests::run_qualification_case("test_caller_cache_cannot_bypass_capability_probe", &test_caller_cache_cannot_bypass_capability_probe);
	bpr::tests::run_qualification_case("test_catalog_resume_and_replacement_revalidate_persistent_credentials", &test_catalog_resume_and_replacement_revalidate_persistent_credentials);
	bpr::tests::run_qualification_case("test_shutdown_preserves_inflight_persistent_resume", &test_shutdown_preserves_inflight_persistent_resume);
	bpr::tests::run_qualification_case("test_shutdown_deletes_inflight_persistent_resume_by_policy", &test_shutdown_deletes_inflight_persistent_resume_by_policy);
	bpr::tests::run_qualification_case("test_shutdown_cancels_running_vault_work", &test_shutdown_cancels_running_vault_work);
	bpr::tests::run_qualification_case("test_shutdown_exact_cleans_every_provisional_phase", &test_shutdown_exact_cleans_every_provisional_phase);
	bpr::tests::run_qualification_case("test_shutdown_cancel_has_no_completion_gate_lock_inversion", &test_shutdown_cancel_has_no_completion_gate_lock_inversion);
	bpr::tests::run_qualification_case("test_destroyed_host_suppresses_late_callbacks_before_posting", &test_destroyed_host_suppresses_late_callbacks_before_posting);
	bpr::tests::run_qualification_case("test_accepted_credential_renewability_is_answered_by_the_vault", &test_accepted_credential_renewability_is_answered_by_the_vault);
	bpr::tests::run_qualification_case("test_refresh_material_shares_the_credential_erasure_path", &test_refresh_material_shares_the_credential_erasure_path);
	bpr::tests::run_qualification_case("test_an_occasion_never_has_two_operations_pending_under_one_context", &test_an_occasion_never_has_two_operations_pending_under_one_context);
	bpr::tests::run_qualification_case("test_a_rotation_keeps_the_material_it_just_stored", &test_a_rotation_keeps_the_material_it_just_stored);
	bpr::tests::run_qualification_case("test_a_retained_slot_leaves_when_another_account_arrives", &test_a_retained_slot_leaves_when_another_account_arrives);
	bpr::tests::run_qualification_case("test_a_retained_slot_stays_when_the_same_account_arrives", &test_a_retained_slot_stays_when_the_same_account_arrives);
	bpr::tests::run_qualification_case("test_a_retained_slot_does_not_survive_a_failure_or_a_shutdown", &test_a_retained_slot_does_not_survive_a_failure_or_a_shutdown);
	std::cout << "application github host tests passed\n";
	return EXIT_SUCCESS;
}
