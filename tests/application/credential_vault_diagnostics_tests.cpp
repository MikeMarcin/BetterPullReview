/// \file
/// \brief [[BPR-1829]]. Credential vault calls leave `credential.vault`
/// records: each store, load and delete of the account slot and its refresh
/// slot records its start and its one terminal with the vault's typed
/// failure, its answer passes through unchanged, and no credential byte,
/// hostname or account reaches a record. Also: every sign-in, vault and
/// updater failure the host can name forms a valid line of its own family.

#include "bpr/application/credential_vault_diagnostics.hpp"
#include "bpr/application/operation_diagnostics.hpp"

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;
	using application::diagnostic_operation;
	using application::diagnostic_outcome;
	using application::github_credential_vault_failure;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	constexpr std::string_view canary   = "PRIVATE_CANARY_TOKEN";
	constexpr std::string_view hostname = "private-canary.example.invalid";

	class recording_log final : public application::diagnostic_log_port
	{
	public:
		[[nodiscard]] auto record(const application::diagnostic_event& aEvent) noexcept -> application::diagnostic_log_status override
		{
			Events.push_back(aEvent);
			return application::diagnostic_log_status::available;
		}
		[[nodiscard]] auto status() const noexcept -> application::diagnostic_log_status override { return application::diagnostic_log_status::available; }
		[[nodiscard]] auto line(const std::size_t aIndex) const -> std::string { return std::string{ Events.at(aIndex).Summary.text() }; }
		[[nodiscard]] auto is(const std::size_t aIndex, const diagnostic_outcome aOutcome, const std::string_view aPrefix) const -> bool
		{
			return aIndex < Events.size() && Events[aIndex].Operation == diagnostic_operation::credential_vault && Events[aIndex].Outcome == aOutcome && line(aIndex).starts_with(aPrefix)
				&& core::parse_operation_diagnosis(line(aIndex)).has_value();
		}

		std::vector<application::diagnostic_event> Events;
	};

	[[nodiscard]] auto material(const std::string_view aText) -> application::github_credential_material
	{
		auto created = application::github_credential_material::create(std::as_bytes(std::span{ aText.data(), aText.size() }));
		expect(created.has_value(), "test material is valid");
		return std::move(*created);
	}

	[[nodiscard]] auto text_of(const application::github_credential_material& aMaterial) -> std::string
	{
		const auto bytes = aMaterial.bytes();
		return { reinterpret_cast<const char*>(bytes.data()), bytes.size() };
	}

	[[nodiscard]] auto context() -> application::github_credential_operation_context
	{
		auto result               = application::github_credential_operation_context{ };
		result.Request            = core::request_id{ 9U };
		result.ApplicationSession = core::application_session_id{ 1U };
		result.Authority.Profile  = core::host_profile_id{ 11U };
		result.Authority.Host     = core::git_host_id{ 3U };
		result.Authority.Account  = core::github_account_id{ 424242U };
		result.Authority.Hostname = application::github_canonical_hostname{ hostname };
		result.IdentityGeneration = core::github_identity_generation{ 2U };
		return result;
	}

	/// Holds each completion until the test answers it; counts what reached it.
	class scripted_vault final : public application::github_credential_vault_port
	{
	public:
		void store(application::github_credential_enrollment aEnrollment, application::github_credential_store_completion_handler aCompletion) override
		{
			stored(std::move(aEnrollment), std::move(aCompletion));
		}
		void retrieve(application::github_credential_operation_context, application::github_credential_retrieve_completion_handler aCompletion) override
		{
			fail_if_asked();
			Retrieve = std::move(aCompletion);
		}
		void delete_credential(application::github_credential_operation_context, application::github_credential_delete_completion_handler aCompletion) override
		{
			fail_if_asked();
			Delete = std::move(aCompletion);
		}
		void store_refresh(application::github_credential_enrollment aEnrollment, application::github_credential_store_completion_handler aCompletion) override
		{
			stored(std::move(aEnrollment), std::move(aCompletion));
		}
		void retrieve_refresh(application::github_credential_operation_context, application::github_credential_retrieve_completion_handler aCompletion) override
		{
			fail_if_asked();
			Retrieve = std::move(aCompletion);
		}
		void delete_refresh(application::github_credential_operation_context, application::github_credential_delete_completion_handler aCompletion) override
		{
			fail_if_asked();
			Delete = std::move(aCompletion);
		}
		void cancel(application::github_credential_operation_context) override { ++Cancellations; }

		application::github_credential_store_completion_handler    Store;
		application::github_credential_retrieve_completion_handler Retrieve;
		application::github_credential_delete_completion_handler   Delete;
		std::string                                                StoredText;
		int                                                        Cancellations = 0;
		bool                                                       Throw         = false;

	private:
		void fail_if_asked() const
		{
			if (Throw) {
				throw std::runtime_error{ "submission refused" };
			}
		}
		void stored(application::github_credential_enrollment aEnrollment, application::github_credential_store_completion_handler aCompletion)
		{
			fail_if_asked();
			StoredText = text_of(aEnrollment.Credential);
			Store      = std::move(aCompletion);
		}
	};

	struct course
	{
		course()
		{
			auto owned = std::make_unique<scripted_vault>();
			Inner      = owned.get();
			Vault      = application::with_credential_vault_diagnostics(std::move(owned), Log);
		}

		std::shared_ptr<recording_log>                             Log   = std::make_shared<recording_log>();
		scripted_vault*                                            Inner = nullptr;
		std::unique_ptr<application::github_credential_vault_port> Vault;
	};

	void expect_private(const recording_log& aLog)
	{
		for (const auto& event : aLog.Events) {
			const auto text = std::string{ event.Summary.text() };
			expect(text.find(canary) == std::string::npos && text.find("private-canary") == std::string::npos && text.find("424242") == std::string::npos,
				   "no credential byte, hostname or account reaches a vault record");
			expect(event.Summary.State == application::diagnostic_summary_state::sanitized, "every vault record is a sanitized line");
		}
	}

	void test_store()
	{
		course world;
		world.Vault->store({ .Context = context(), .Credential = material(canary) }, [&](application::github_credential_store_completion aCompletion) {
			expect(std::holds_alternative<application::github_credential_store_success>(aCompletion), "the store answer passes through unchanged");
		});
		expect(world.Inner->StoredText == canary, "the credential reaches the vault unchanged");
		expect(world.Log->Events.size() == 1U && world.Log->is(0U, diagnostic_outcome::started, "action=vault_store failure=none process=none exit=none duration_ms=0 count=0 "), "a store records its start");
		world.Inner->Store(application::github_credential_store_success{ .Context = context(), .Disposition = application::github_credential_store_disposition::stored });
		expect(world.Log->Events.size() == 2U && world.Log->is(1U, diagnostic_outcome::succeeded, "action=vault_store failure=none "), "a stored credential records succeeded");

		world.Vault->store_refresh({ .Context = context(), .Credential = material(canary) }, { });
		world.Inner->Store(application::github_credential_store_error{ .Context = context(), .Failure = github_credential_vault_failure::locked });
		expect(world.Log->is(2U, diagnostic_outcome::started, "action=refresh_store ") && world.Log->is(3U, diagnostic_outcome::failed, "action=refresh_store failure=locked "), "a locked keychain records failed locked");

		world.Vault->store({ .Context = context(), .Credential = material(canary) }, { });
		world.Inner->Store(application::github_credential_store_cancelled{ .Context = context() });
		expect(world.Log->is(5U, diagnostic_outcome::cancelled, "action=vault_store failure=cancelled "), "a cancelled store records cancelled");

		world.Vault->store({ .Context = context(), .Credential = material(canary) }, { });
		world.Inner->Store(application::github_credential_store_error{ .Context = context(), .Failure = github_credential_vault_failure::none });
		expect(world.Log->is(7U, diagnostic_outcome::failed, "action=vault_store failure=operation_failed "), "an error naming no failure is still a failure");
		expect_private(*world.Log);
	}

	void test_load()
	{
		course      world;
		std::string seen;
		world.Vault->retrieve(context(), [&](application::github_credential_retrieve_completion aCompletion) {
			if (auto* success = std::get_if<application::github_credential_retrieve_success>(&aCompletion)) {
				seen = text_of(success->Credential);
			}
		});
		world.Inner->Retrieve(application::github_credential_retrieve_success{ .Context = context(), .Credential = material(canary) });
		expect(seen == canary, "the loaded credential passes through unchanged");
		expect(world.Log->is(0U, diagnostic_outcome::started, "action=vault_load ") && world.Log->is(1U, diagnostic_outcome::succeeded, "action=vault_load failure=none ")
				   && world.Log->line(1U).find(" count=1 ") != std::string::npos,
			   "a found credential records succeeded counting one");

		world.Vault->retrieve_refresh(context(), { });
		world.Inner->Retrieve(application::github_credential_retrieve_absent{ .Context = context() });
		expect(world.Log->is(3U, diagnostic_outcome::succeeded, "action=refresh_load failure=none ") && world.Log->line(3U).find(" count=0 ") != std::string::npos, "an absent slot records succeeded counting zero");

		world.Vault->retrieve(context(), { });
		world.Inner->Retrieve(application::github_credential_retrieve_error{ .Context = context(), .Failure = github_credential_vault_failure::access_denied });
		expect(world.Log->is(5U, diagnostic_outcome::failed, "action=vault_load failure=access_denied "), "a denied keychain read records failed access_denied");
		expect_private(*world.Log);
	}

	void test_delete_cancel_and_throw()
	{
		course world;
		world.Vault->delete_credential(context(), { });
		world.Inner->Delete(application::github_credential_delete_success{ .Context = context(), .Disposition = application::github_credential_delete_disposition::deleted });
		expect(world.Log->is(1U, diagnostic_outcome::succeeded, "action=vault_delete failure=none ") && world.Log->line(1U).find(" count=1 ") != std::string::npos, "a removed credential counts one");
		world.Vault->delete_refresh(context(), { });
		world.Inner->Delete(application::github_credential_delete_success{ .Context = context(), .Disposition = application::github_credential_delete_disposition::absent });
		expect(world.Log->is(3U, diagnostic_outcome::succeeded, "action=refresh_delete ") && world.Log->line(3U).find(" count=0 ") != std::string::npos, "an absent entry counts zero");
		world.Vault->delete_credential(context(), { });
		world.Inner->Delete(application::github_credential_delete_error{ .Context = context(), .Failure = github_credential_vault_failure::corrupt });
		expect(world.Log->is(5U, diagnostic_outcome::failed, "action=vault_delete failure=corrupt "), "a corrupt entry records failed corrupt");

		world.Vault->cancel(context());
		expect(world.Inner->Cancellations == 1U && world.Log->Events.size() == 6U, "cancellation passes through and records nothing itself");

		world.Inner->Throw = true;
		auto threw         = false;
		try {
			world.Vault->retrieve(context(), { });
		} catch (const std::runtime_error&) {
			threw = true;
		}
		expect(threw && !world.Inner->Retrieve, "a refused submission still throws and the vault retained nothing");
		expect(world.Log->Events.size() == 8U && world.Log->is(6U, diagnostic_outcome::started, "action=vault_load ") && world.Log->is(7U, diagnostic_outcome::failed, "action=vault_load failure=unavailable "),
			   "a submission that throws records failed unavailable");
		expect_private(*world.Log);
	}

	void test_null_composition()
	{
		auto  owned = std::make_unique<scripted_vault>();
		auto* inner = owned.get();
		auto  plain = application::with_credential_vault_diagnostics(std::move(owned), nullptr);
		expect(plain.get() == inner, "negative control: with no log the vault is returned unwrapped");
		expect(application::with_credential_vault_diagnostics(nullptr, std::make_shared<recording_log>()) == nullptr, "a null vault stays null");
	}

	/// Every typed failure each family can name forms a valid line of its own family.
	void test_every_failure_forms_a_line()
	{
		using failure = core::operation_failure;
		for (auto value = 0U; value <= static_cast<unsigned>(github_credential_vault_failure::allocation_failure); ++value) {
			const auto named = application::operation_failure_of(static_cast<github_credential_vault_failure>(value));
			expect(!core::describe({ .Action = core::operation_action::vault_load, .Failure = named }).empty(), "every vault failure is a vault line");
		}
		for (auto value = 0U; value <= static_cast<unsigned>(core::release_update_reason::shutdown); ++value) {
			const auto named = application::operation_failure_of(static_cast<core::release_update_reason>(value));
			expect(!core::describe({ .Action = core::operation_action::update_download, .Failure = named }).empty(), "every updater reason is an updater line");
		}
		for (auto value = 0U; value <= static_cast<unsigned>(application::device_authorization_refresh_failure::storage_failed); ++value) {
			const auto named = application::operation_failure_of(static_cast<application::device_authorization_refresh_failure>(value));
			expect(!core::describe({ .Action = core::operation_action::token_renewal, .Failure = named }).empty(), "every renewal failure is a sign-in line");
		}
		for (auto stage = 0U; stage <= static_cast<unsigned>(core::device_authorization_stage::failed); ++stage) {
			for (auto reason = 0U; reason <= static_cast<unsigned>(core::device_authorization_reason_code::vault_unavailable); ++reason) {
				const auto named = application::operation_failure_of(static_cast<core::device_authorization_stage>(stage), static_cast<core::device_authorization_reason_code>(reason));
				expect(!core::describe({ .Action = core::operation_action::device_sign_in, .Failure = named }).empty(), "every device terminal is a sign-in line");
			}
		}
		for (auto operation = 0U; operation <= static_cast<unsigned>(core::release_update_operation::install); ++operation) {
			const auto action = application::operation_action_of(static_cast<core::release_update_operation>(operation));
			expect(application::diagnostic_operation_of(action) == diagnostic_operation::updater, "every updater job is logged as updater.release");
		}
		expect(application::diagnostic_operation_of(core::operation_action::sign_out) == diagnostic_operation::sign_in
				   && application::diagnostic_operation_of(core::operation_action::refresh_delete) == diagnostic_operation::credential_vault,
			   "sign-in and vault actions are logged under their own operations");
		expect(core::describe({ .Action = core::operation_action::vault_load, .Failure = failure::offline }).empty()
				   && core::describe({ .Action = core::operation_action::update_check, .Failure = failure::locked }).empty()
				   && core::describe({ .Action = core::operation_action::sign_out, .Failure = failure::corrupt }).empty(),
			   "negative control: another family's failure is refused");
	}
} // namespace

auto main() -> int
{
	test_store();
	test_load();
	test_delete_cancel_and_throw();
	test_null_composition();
	test_every_failure_forms_a_line();
	std::cout << "credential-vault-diagnostics-tests:pass\n";
	return EXIT_SUCCESS;
}
