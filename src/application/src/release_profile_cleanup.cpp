#include "bpr/application/release_profile_cleanup.hpp"
#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <condition_variable>
#include <limits>
#include <mutex>

namespace bpr::application {
	namespace {
		using reason = core::release_maintenance_reason;
		auto identities(const std::function<std::unique_ptr<device_binding_store_port>()>& aBindings, core::bpr_profile_id aProfile, std::stop_token aStop)
			-> std::expected<std::vector<github_credential_authority_key>, reason>
		{
			auto bindings = aBindings();
			if (!bindings) {
				return std::unexpected(reason::unavailable);
			}
			auto records = bindings->load(aStop);
			if (!records) {
				return std::unexpected(aStop.stop_requested() ? reason::cancelled : reason::io_failure);
			}
			if (records->size() > maximum_device_bindings) {
				return std::unexpected(reason::limit_exceeded);
			}
			std::vector<github_credential_authority_key> keys;
			for (const auto& record : *records) {
				if (!valid(record) || record.Authority.BprProfile != aProfile) {
					return std::unexpected(reason::invalid_authority);
				}
				// Tombstones also retain the exact key of a possibly failed prior deletion.
				keys.push_back(record.Authority);
			}
			std::ranges::sort(keys);
			if (std::ranges::adjacent_find(keys) != keys.end()) {
				return std::unexpected(reason::invalid_selection);
			}
			return keys;
		}
		auto inventory(const std::vector<github_credential_authority_key>& aKeys) -> release_profile_resource_inventory
		{
			std::string bytes{ "bpr-credential-cleanup-v1\n" };
			for (const auto& key : aKeys) {
				bytes.append(std::to_string(key.BprProfile.value())).push_back(':');
				bytes.append(std::to_string(key.Profile.value())).push_back(':');
				bytes.append(std::to_string(key.ProfileRevision.value())).push_back(':');
				bytes.append(std::to_string(key.Host.value())).push_back(':');
				bytes.append(key.Hostname.value()).push_back(':');
				bytes.append(std::to_string(key.Account.value())).push_back(':');
				bytes.append(std::to_string(static_cast<unsigned>(key.ServerContract))).push_back('\n');
			}
			return { core::sha256({ reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size() }), static_cast<std::uint32_t>(aKeys.size() * 2U) };
		}
		struct deletion_wait
		{
			std::mutex                                         Mutex;
			std::condition_variable                            Ready;
			std::optional<github_credential_delete_completion> Completion;
		};
		auto delete_slot(github_credential_vault_port& aVault, const github_credential_operation_context& aContext, bool aRefresh)
			-> release_maintenance_result
		{
			auto wait     = std::make_shared<deletion_wait>();
			auto callback = [wait](github_credential_delete_completion aCompletion) {
				const std::lock_guard lock{ wait->Mutex };
				if (!wait->Completion) {
					wait->Completion = std::move(aCompletion);
				}
				wait->Ready.notify_one();
			};
			if (aRefresh) {
				aVault.delete_refresh(aContext, callback);
			} else {
				aVault.delete_credential(aContext, callback);
			}
			std::unique_lock lock{ wait->Mutex };
			wait->Ready.wait(lock, [&wait] { return wait->Completion.has_value(); });
			const auto* success = std::get_if<github_credential_delete_success>(&*wait->Completion);
			if (!success || success->Context != aContext || !valid(*success)) {
				return std::unexpected(reason::io_failure);
			}
			return { };
		}
	}
	release_profile_credentials::release_profile_credentials(core::bpr_profile_id aProfile, core::application_session_id aSession,
															 std::function<std::unique_ptr<device_binding_store_port>()> aBindings, github_credential_vault_port& aVault)
		: Profile{ aProfile }
		, Session{ aSession }
		, Bindings{ std::move(aBindings) }
		, Vault{ aVault }
	{ }
	auto release_profile_credentials::inspect(std::stop_token aStop) -> std::expected<release_profile_resource_inventory, reason>
	{
		if (Profile.value() == 0U || Session.value() == 0U) {
			return std::unexpected(reason::invalid_authority);
		}
		auto keys = identities(Bindings, Profile, aStop);
		if (!keys) {
			return std::unexpected(keys.error());
		}
		return inventory(*keys);
	}
	auto release_profile_credentials::erase(const release_profile_resource_inventory& aInventory) -> release_maintenance_result
	{
		if (Profile.value() == 0U || Session.value() == 0U) {
			return std::unexpected(reason::invalid_authority);
		}
		auto keys = identities(Bindings, Profile, { });
		if (!keys) {
			return std::unexpected(keys.error());
		}
		if (inventory(*keys) != aInventory) {
			return std::unexpected(reason::selection_changed);
		}
		for (const auto& key : *keys) {
			if (NextRequest > std::numeric_limits<std::uint64_t>::max() - 2U) {
				return std::unexpected(reason::identity_exhausted);
			}
			for (const auto refresh : { false, true }) {
				const github_credential_operation_context context{ core::request_id{ NextRequest++ }, Session, key, core::github_identity_generation{ 1U } };
				auto                                      result = delete_slot(Vault, context, refresh);
				if (!result) {
					return result;
				}
			}
		}
		return { };
	}
}
