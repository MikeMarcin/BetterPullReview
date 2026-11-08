#include "github_application/profile_runtime.hpp"

#include <QByteArray>
#include <QByteArrayView>
#include <QCryptographicHash>

#include <algorithm>
#include <limits>
#include <variant>

namespace bpr::app {
	auto github_profile_runtime::credential_vault() noexcept
		-> application::github_credential_vault_port&
	{
#if defined(BPR_BUILD_TESTING)
		if (Fixture != nullptr) {
			return Fixture->credential_vault();
		}
#endif
		return *OwnedVault;
	}

	void github_profile_runtime::reconcile_release_access()
	{
		// Closing a review window is not signing out of the independently
		// selected app-wide release account. Explicit identity changes are.
		if (Closing || !Host) {
			return;
		}
		const auto authority  = Host->accepted_credential_authority();
		const auto generation = Host->accepted_identity_generation();
		std::erase_if(ReleaseLeases, [&](const auto& aLease) {
			const auto access = aLease.Access.lock();
			if (!access) {
				return true;
			}
			if (authority != aLease.Authority || generation != aLease.Generation) {
				access->revoke();
				return true;
			}
			return false;
		});
	}

	auto github_profile_runtime::release_account() -> std::optional<release_update_account>
	{
		if (Closing || !Host || !Host->accepted_session()) {
			return std::nullopt;
		}
		const auto authority  = Host->accepted_credential_authority();
		const auto generation = Host->accepted_identity_generation();
		if (!authority || authority->ServerContract != application::github_server_contract::github_com_2022_11_28
			|| authority->Hostname.value() != "github.com" || generation.value() == 0U) {
			return std::nullopt;
		}
		const auto found = std::ranges::find_if(Profiles, [&](const auto& aProfile) {
			return aProfile.profile() == authority->Profile && aProfile.revision() == authority->ProfileRevision
				&& aProfile.authentication_transport() == application::github_authentication_transport::native_https;
		});
		if (found == Profiles.end()) {
			return std::nullopt;
		}
		const auto key = std::to_string(authority->BprProfile.value()) + ":" + std::to_string(authority->Profile.value())
			+ ":" + std::to_string(authority->ProfileRevision.value()) + ":" + std::to_string(authority->Host.value())
			+ ":" + std::to_string(authority->Account.value()) + ":github.com";
		const auto binding = QCryptographicHash::hash(QByteArray::fromStdString(key), QCryptographicHash::Sha256).toHex().toStdString();
		auto       acquire = [weak = weak_from_this(), accepted = *authority, generation](core::release_update_access_id                                                aIdentity,
																						  std::shared_ptr<application::release_update_archive_store>                    aStore,
																						  std::function<void(std::shared_ptr<application::release_update_access_port>)> aDone) {
			const auto self = weak.lock();
			if (!self) {
				aDone({ });
				return;
			}
			// A separate session namespace isolates update vault requests.
			const auto context = application::github_credential_operation_context{
				core::request_id{ aIdentity.value() }, core::application_session_id{ std::numeric_limits<std::uint64_t>::max() }, accepted, generation
			};
			self->acquire_release_access(context, aIdentity, std::move(aStore), std::move(aDone));
		};
		return release_update_account{ binding, Host->accepted_session()->Viewer.Login + " · " + Admission.Label, std::move(acquire) };
	}

	void github_profile_runtime::acquire_release_access(application::github_credential_operation_context aContext, core::release_update_access_id aIdentity,
														std::shared_ptr<application::release_update_archive_store>                    aStore,
														std::function<void(std::shared_ptr<application::release_update_access_port>)> aDone)
	{
		if (Closing || Host->accepted_credential_authority() != aContext.Authority
			|| Host->accepted_identity_generation() != aContext.IdentityGeneration) {
			aDone({ });
			return;
		}
		++ReleaseAccessPending;
		try {
			credential_vault().retrieve(aContext, [self = shared_from_this(), aContext, aIdentity, aStore, aDone](application::github_credential_retrieve_completion aCompletion) {
				std::shared_ptr<adapters::github::detail::native_release_update_access> access;
				if (auto* success = std::get_if<application::github_credential_retrieve_success>(&aCompletion); success && success->Context == aContext) {
					access = adapters::github::detail::make_native_release_update_access(std::move(*success), aIdentity, aStore);
				}
				self->DecisionLane.post([self, aContext, access, aDone] {
					--self->ReleaseAccessPending;
					if (self->Closing || self->Host->accepted_credential_authority() != aContext.Authority
						|| self->Host->accepted_identity_generation() != aContext.IdentityGeneration) {
						if (access) {
							access->revoke();
						}
						aDone({ });
						return;
					}
					std::erase_if(self->ReleaseLeases, [](const auto& aLease) { return aLease.Access.expired(); });
					if (access) {
						self->ReleaseLeases.push_back({ aContext.Authority, aContext.IdentityGeneration, access });
					}
					aDone(access);
				});
			});
		} catch (...) {
			--ReleaseAccessPending;
			aDone({ });
		}
	}
}
