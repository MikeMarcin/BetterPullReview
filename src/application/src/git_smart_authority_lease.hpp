#ifndef BPR_SRC_APPLICATION_SRC_GIT_SMART_AUTHORITY_LEASE_HPP
#define BPR_SRC_APPLICATION_SRC_GIT_SMART_AUTHORITY_LEASE_HPP

#include "bpr/application/git_smart_transport_port.hpp"
#include "bpr/application/github_epoch_handoff.hpp"
#include "bpr/application/github_provider_port.hpp"

#include <atomic>
#include <expected>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

/// \file
/// The smart-Git authority lease a read host hands the epoch host with an
/// accepted pull request, and the revocable state behind it. Internal to the
/// application library: the window read host mints one for its own window,
/// and `github_application_host` mints one it lends another holder
/// ([[BPR-2181]]). Both revoke through the shared state.

namespace bpr::application {
	struct smart_authority_state final
	{
		void retain(const git_smart_transport& aTransport)
		{
			const std::scoped_lock lock{ Mutex };
			std::erase_if(Transports,
						  [](const auto& aCandidate) { return aCandidate.expired(); });
			Transports.emplace_back(aTransport);
		}

		void revoke() noexcept
		{
			if (Revoked.exchange(true, std::memory_order_acq_rel)) {
				return;
			}
			auto transports = std::vector<
				std::weak_ptr<git_smart_transport_port>>{ };
			{
				const std::scoped_lock lock{ Mutex };
				transports.swap(Transports);
			}
			for (const auto& candidate : transports) {
				if (const auto transport = candidate.lock()) {
					transport->revoke();
				}
			}
		}

		std::atomic_bool                                     Revoked = false;
		std::mutex                                           Mutex;
		std::vector<std::weak_ptr<git_smart_transport_port>> Transports;
	};

	class bound_git_smart_authority final : public git_smart_authority_lease
	{
	public:
		bound_git_smart_authority(git_smart_authority_binding            aBinding,
								  github_account_transport               aAccount,
								  std::shared_ptr<smart_authority_state> aState)
			: Binding{ std::move(aBinding) }
			, Account{ std::move(aAccount) }
			, State{ std::move(aState) }
		{
		}

		// The credential-free binding. Composition owns the anonymous
		// factory for the host's lifetime, so the lease borrows it rather
		// than sharing an account transport it must never hold.
		bound_git_smart_authority(git_smart_authority_binding            aBinding,
								  git_smart_transport_factory_port*      aAnonymous,
								  std::shared_ptr<smart_authority_state> aState)
			: Binding{ std::move(aBinding) }
			, Anonymous{ aAnonymous }
			, State{ std::move(aState) }
		{
		}

		~bound_git_smart_authority() override { revoke(); }

		[[nodiscard]] auto binding() const noexcept
			-> const git_smart_authority_binding& override
		{
			return Binding;
		}

		[[nodiscard]] auto supports_sparse_commit_trees() const noexcept -> bool override
		{
			if (State->Revoked.load(std::memory_order_acquire)) {
				return false;
			}
			auto* const factory = Account == nullptr
				? Anonymous
				: dynamic_cast<git_smart_transport_factory_port*>(Account.get());
			return factory != nullptr && factory->supports_sparse_commit_trees();
		}

		[[nodiscard]] auto acquire(const git_smart_request_context& aContext)
			-> git_smart_transport_result<git_smart_transport> override
		{
			if (State->Revoked.load(std::memory_order_acquire)
				|| !application::valid(aContext)
				|| aContext.ApplicationSession != Binding.ApplicationSession
				|| aContext.Authority.Profile != Binding.Profile
				|| aContext.Authority.ProfileRevision != Binding.ProfileRevision
				|| aContext.Authority.Host != Binding.Host
				|| aContext.Authority.HostAuthority != Binding.HostAuthority
				|| aContext.Authority.Account != Binding.Account
				|| aContext.Authority.Repository != Binding.Repository
				|| aContext.Repository != Binding.Locator
				|| aContext.Revision.Base.algorithm() != Binding.ObjectFormat
				|| aContext.Revision.Head.algorithm() != Binding.ObjectFormat) {
				return std::unexpected{ git_smart_transport_error{
					.Failure = git_smart_transport_failure::invalid_request } };
			}
			auto* const factory = Account == nullptr
				? Anonymous
				: dynamic_cast<git_smart_transport_factory_port*>(Account.get());
			if (factory == nullptr) {
				return std::unexpected{ git_smart_transport_error{
					.Failure = git_smart_transport_failure::unavailable } };
			}
			auto acquired = factory->acquire(aContext);
			if (!acquired) {
				return std::unexpected{ acquired.error() };
			}
			State->retain(*acquired);
			if (State->Revoked.load(std::memory_order_acquire)) {
				(*acquired)->revoke();
				return std::unexpected{ git_smart_transport_error{
					.Failure = git_smart_transport_failure::cancelled } };
			}
			return std::move(*acquired);
		}

		void revoke() noexcept override { State->revoke(); }

		[[nodiscard]] auto revoked() const noexcept -> bool override
		{
			return State->Revoked.load(std::memory_order_acquire);
		}

		[[nodiscard]] auto state() const noexcept
			-> std::shared_ptr<smart_authority_state>
		{
			return State;
		}

	private:
		git_smart_authority_binding            Binding;
		github_account_transport               Account;
		git_smart_transport_factory_port*      Anonymous = nullptr;
		std::shared_ptr<smart_authority_state> State;
	};
}

#endif // BPR_SRC_APPLICATION_SRC_GIT_SMART_AUTHORITY_LEASE_HPP
