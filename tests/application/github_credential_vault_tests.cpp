#include "bpr/application/github_credential_vault.hpp"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {
	using namespace bpr;

	using authority_key         = application::github_credential_authority_key;
	using operation_context     = application::github_credential_operation_context;
	using credential_material   = application::github_credential_material;
	using credential_vault_port = application::github_credential_vault_port;

	static_assert(!std::default_initializable<credential_material>);
	static_assert(!std::copy_constructible<credential_material>);
	static_assert(!std::is_copy_assignable_v<credential_material>);
	static_assert(std::is_nothrow_destructible_v<credential_material>);
	static_assert(std::is_nothrow_move_constructible_v<credential_material>);
	static_assert(std::is_nothrow_move_assignable_v<credential_material>);
	static_assert(
		std::same_as<
			decltype(std::declval<const credential_material&>().bytes()),
			std::span<const std::byte>>);
	static_assert(
		!std::convertible_to<const credential_material&, std::string>);
	static_assert(
		!std::constructible_from<credential_material, std::string>);
	static_assert(std::is_abstract_v<credential_vault_port>);
	static_assert(!std::copy_constructible<credential_vault_port>);
	static_assert(!std::is_copy_assignable_v<credential_vault_port>);
	static_assert(!std::is_move_constructible_v<credential_vault_port>);
	static_assert(!std::is_move_assignable_v<credential_vault_port>);

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto runtime_bytes(
		const std::size_t  aSize,
		const std::uint8_t aSeed) -> std::vector<std::byte>
	{
		auto bytes = std::vector<std::byte>(aSize);
		auto value = aSeed;
		for (auto& byte : bytes) {
			value = static_cast<std::uint8_t>(
				static_cast<std::uint16_t>(value) * 29U + 17U);
			byte = static_cast<std::byte>(value);
		}
		return bytes;
	}

	auto same_bytes(
		const std::span<const std::byte> aActual,
		const std::span<const std::byte> aExpected) -> bool
	{
		return std::ranges::equal(aActual, aExpected);
	}

	auto material_from(const std::span<const std::byte> aBytes)
		-> credential_material
	{
		auto material = credential_material::create(aBytes);
		expect(material.has_value(), "valid credential material is created");
		return std::move(*material);
	}

	auto authority(
		const std::uint64_t                       aProfile         = 1U,
		const std::uint64_t                       aProfileRevision = 2U,
		const std::uint64_t                       aHost            = 3U,
		const std::uint64_t                       aAccount         = 4U,
		const application::github_server_contract aServer          = application::github_server_contract::
			github_com_2022_11_28,
		const std::string_view aHostname = { }) -> authority_key
	{
		return {
			.Profile         = core::host_profile_id{ aProfile },
			.ProfileRevision = application::github_profile_revision{ aProfileRevision },
			.Host            = core::git_host_id{ aHost },
			.Hostname        = application::github_canonical_hostname{
				aHostname.empty()
					? (aServer
							   == application::github_server_contract::
								   github_com_2022_11_28
						   ? "github.com"
						   : "ghe.example.invalid")
					: aHostname },
			.Account        = core::github_account_id{ aAccount },
			.ServerContract = aServer,
		};
	}

	auto context(
		const authority_key& aKey        = authority(),
		const std::uint64_t  aRequest    = 10U,
		const std::uint64_t  aSession    = 11U,
		const std::uint64_t  aGeneration = 12U) -> operation_context
	{
		return {
			.Request            = core::request_id{ aRequest },
			.ApplicationSession = core::application_session_id{ aSession },
			.Authority          = aKey,
			.IdentityGeneration = core::github_identity_generation{ aGeneration },
		};
	}

	class queued_fake_vault final
		: public application::github_credential_vault_port
	{
	public:
		void store(
			application::github_credential_enrollment               aEnrollment,
			application::github_credential_store_completion_handler aCompletion)
			override
		{
			if (!valid(aEnrollment) || pending(aEnrollment.Context)) {
				publish_store_without_credential(
					aEnrollment.Credential,
					aCompletion,
					application::github_credential_store_error{
						.Context = aEnrollment.Context,
						.Failure = application::github_credential_vault_failure::
							invalid_request,
					});
				return;
			}

			PendingStores.push_back(pending_store{
				.Enrollment = std::move(aEnrollment),
				.Completion = std::move(aCompletion),
			});
		}

		void retrieve(
			operation_context aOperation,
			application::github_credential_retrieve_completion_handler
				aCompletion) override
		{
			if (!valid(aOperation) || pending(aOperation)) {
				aCompletion(application::github_credential_retrieve_error{
					.Context = aOperation,
					.Failure = application::github_credential_vault_failure::
						invalid_request,
				});
				return;
			}

			PendingRetrieves.push_back(pending_retrieve{
				.Context    = aOperation,
				.Completion = std::move(aCompletion),
			});
		}

		void delete_credential(
			operation_context                                        aOperation,
			application::github_credential_delete_completion_handler aCompletion)
			override
		{
			if (!valid(aOperation) || pending(aOperation)) {
				aCompletion(application::github_credential_delete_error{
					.Context = aOperation,
					.Failure = application::github_credential_vault_failure::
						invalid_request,
				});
				return;
			}

			PendingDeletes.push_back(pending_delete{
				.Context    = aOperation,
				.Completion = std::move(aCompletion),
			});
		}

		void cancel(const operation_context aOperation) override
		{
			const auto store = find_store(aOperation);
			if (store != PendingStores.end()) {
				auto pendingOperation = std::move(*store);
				PendingStores.erase(store);
				publish_store_without_credential(
					pendingOperation.Enrollment.Credential,
					pendingOperation.Completion,
					application::github_credential_store_cancelled{
						.Context = pendingOperation.Enrollment.Context,
					});
				return;
			}

			const auto retrieve = find_retrieve(aOperation);
			if (retrieve != PendingRetrieves.end()) {
				auto pendingOperation = std::move(*retrieve);
				PendingRetrieves.erase(retrieve);
				pendingOperation.Completion(
					application::github_credential_retrieve_cancelled{
						.Context = pendingOperation.Context,
					});
				return;
			}

			const auto deletion = find_delete(aOperation);
			if (deletion != PendingDeletes.end()) {
				auto pendingOperation = std::move(*deletion);
				PendingDeletes.erase(deletion);
				pendingOperation.Completion(
					application::github_credential_delete_cancelled{
						.Context = pendingOperation.Context,
					});
			}
		}

		[[nodiscard]] auto complete_store(
			const operation_context                            aOperation,
			const application::github_credential_vault_failure aFailure = application::github_credential_vault_failure::none) -> bool
		{
			const auto found = find_store(aOperation);
			if (found == PendingStores.end()) {
				return false;
			}

			auto pendingOperation = std::move(*found);
			PendingStores.erase(found);
			if (aFailure != application::github_credential_vault_failure::none) {
				publish_store_without_credential(
					pendingOperation.Enrollment.Credential,
					pendingOperation.Completion,
					application::github_credential_store_error{
						.Context = pendingOperation.Enrollment.Context,
						.Failure = aFailure,
					});
				return true;
			}

			Stored.insert_or_assign(
				pendingOperation.Enrollment.Context.Authority,
				std::move(pendingOperation.Enrollment.Credential));
			pendingOperation.Completion(
				application::github_credential_store_success{
					.Context     = pendingOperation.Enrollment.Context,
					.Disposition = application::github_credential_store_disposition::
						stored,
				});
			return true;
		}

		[[nodiscard]] auto complete_retrieve(
			const operation_context                            aOperation,
			const application::github_credential_vault_failure aFailure = application::github_credential_vault_failure::none) -> bool
		{
			const auto found = find_retrieve(aOperation);
			if (found == PendingRetrieves.end()) {
				return false;
			}

			auto pendingOperation = std::move(*found);
			PendingRetrieves.erase(found);
			if (aFailure != application::github_credential_vault_failure::none) {
				pendingOperation.Completion(
					application::github_credential_retrieve_error{
						.Context = pendingOperation.Context,
						.Failure = aFailure,
					});
				return true;
			}

			const auto stored = Stored.find(aOperation.Authority);
			if (stored == Stored.end()) {
				pendingOperation.Completion(
					application::github_credential_retrieve_absent{
						.Context = pendingOperation.Context,
					});
				return true;
			}

			auto credential = credential_material::create(stored->second.bytes());
			if (!credential.has_value()) {
				pendingOperation.Completion(
					application::github_credential_retrieve_error{
						.Context = pendingOperation.Context,
						.Failure = application::github_credential_vault_failure::
							allocation_failure,
					});
				return true;
			}

			pendingOperation.Completion(
				application::github_credential_retrieve_success{
					.Context    = pendingOperation.Context,
					.Credential = std::move(*credential),
				});
			return true;
		}

		[[nodiscard]] auto complete_delete(
			const operation_context                            aOperation,
			const application::github_credential_vault_failure aFailure = application::github_credential_vault_failure::none) -> bool
		{
			const auto found = find_delete(aOperation);
			if (found == PendingDeletes.end()) {
				return false;
			}

			auto pendingOperation = std::move(*found);
			PendingDeletes.erase(found);
			if (aFailure != application::github_credential_vault_failure::none) {
				pendingOperation.Completion(
					application::github_credential_delete_error{
						.Context = pendingOperation.Context,
						.Failure = aFailure,
					});
				return true;
			}

			const auto erased = Stored.erase(aOperation.Authority);
			pendingOperation.Completion(
				application::github_credential_delete_success{
					.Context     = pendingOperation.Context,
					.Disposition = erased == 0U
						? application::github_credential_delete_disposition::
							  absent
						: application::github_credential_delete_disposition::
							  deleted,
				});
			return true;
		}

		[[nodiscard]] auto stored_count() const noexcept -> std::size_t
		{
			return Stored.size();
		}

		[[nodiscard]] auto pending_count() const noexcept -> std::size_t
		{
			return PendingStores.size()
				+ PendingRetrieves.size()
				+ PendingDeletes.size();
		}

		[[nodiscard]] auto stored_matches(
			const authority_key&             aKey,
			const std::span<const std::byte> aExpected) const -> bool
		{
			const auto found = Stored.find(aKey);
			return found != Stored.end()
				&& same_bytes(found->second.bytes(), aExpected);
		}

	private:
		static void publish_store_without_credential(
			credential_material&                                     aCredential,
			application::github_credential_store_completion_handler& aCompletion,
			application::github_credential_store_completion          aTerminal)
		{
			aCredential.reset();
			aCompletion(std::move(aTerminal));
		}

		struct pending_store
		{
			application::github_credential_enrollment               Enrollment;
			application::github_credential_store_completion_handler Completion;
		};

		struct pending_retrieve
		{
			operation_context Context;
			application::github_credential_retrieve_completion_handler
				Completion;
		};

		struct pending_delete
		{
			operation_context                                        Context;
			application::github_credential_delete_completion_handler Completion;
		};

		using store_iterator    = std::vector<pending_store>::iterator;
		using retrieve_iterator = std::vector<pending_retrieve>::iterator;
		using delete_iterator   = std::vector<pending_delete>::iterator;

		[[nodiscard]] auto find_store(
			const operation_context& aOperation) -> store_iterator
		{
			return std::ranges::find_if(
				PendingStores,
				[&aOperation](const pending_store& aPendingOperation) {
					return aPendingOperation.Enrollment.Context == aOperation;
				});
		}

		[[nodiscard]] auto find_retrieve(
			const operation_context& aOperation) -> retrieve_iterator
		{
			return std::ranges::find_if(
				PendingRetrieves,
				[&aOperation](const pending_retrieve& aPendingOperation) {
					return aPendingOperation.Context == aOperation;
				});
		}

		[[nodiscard]] auto find_delete(
			const operation_context& aOperation) -> delete_iterator
		{
			return std::ranges::find_if(
				PendingDeletes,
				[&aOperation](const pending_delete& aPendingOperation) {
					return aPendingOperation.Context == aOperation;
				});
		}

		[[nodiscard]] auto pending(
			const operation_context& aOperation) -> bool
		{
			return find_store(aOperation) != PendingStores.end()
				|| find_retrieve(aOperation) != PendingRetrieves.end()
				|| find_delete(aOperation) != PendingDeletes.end();
		}

		std::map<authority_key, credential_material> Stored;
		std::vector<pending_store>                   PendingStores;
		std::vector<pending_retrieve>                PendingRetrieves;
		std::vector<pending_delete>                  PendingDeletes;
	};

	template <typename Completion>
	struct completion_observation
	{
		std::size_t               Calls = 0U;
		std::optional<Completion> Terminal;
	};

	using store_observation = completion_observation<
		application::github_credential_store_completion>;
	using retrieve_observation = completion_observation<
		application::github_credential_retrieve_completion>;
	using delete_observation = completion_observation<
		application::github_credential_delete_completion>;

	template <typename Completion>
	auto observe_completion(completion_observation<Completion>& aObservation)
		-> std::function<void(Completion)>
	{
		return [&aObservation](Completion aTerminal) {
			++aObservation.Calls;
			aObservation.Terminal.emplace(std::move(aTerminal));
		};
	}

	auto observe_successful_retrieve(
		retrieve_observation&           aObservation,
		std::vector<operation_context>& aCompletionOrder,
		const std::string_view          aFailureMessage)
		-> application::github_credential_retrieve_completion_handler
	{
		return [&aObservation, &aCompletionOrder, aFailureMessage](
				   application::github_credential_retrieve_completion aTerminal) {
			const auto* success = std::get_if<application::github_credential_retrieve_success>(
				&aTerminal);
			expect(success != nullptr, aFailureMessage);
			aCompletionOrder.push_back(success->Context);
			++aObservation.Calls;
			aObservation.Terminal.emplace(std::move(aTerminal));
		};
	}

	void queue_store(
		queued_fake_vault&               aVault,
		const operation_context          aOperation,
		const std::span<const std::byte> aBytes,
		store_observation&               aObservation)
	{
		aVault.store(
			application::github_credential_enrollment{
				.Context    = aOperation,
				.Credential = material_from(aBytes),
			},
			observe_completion(aObservation));
	}

	template <typename Alternative, typename Completion>
	auto terminal_as(
		const completion_observation<Completion>& aObservation)
		-> const Alternative*
	{
		if (!aObservation.Terminal.has_value()) {
			return nullptr;
		}
		return std::get_if<Alternative>(&*aObservation.Terminal);
	}

	auto store_success(
		const store_observation& aObservation)
		-> const application::github_credential_store_success*
	{
		return terminal_as<application::github_credential_store_success>(
			aObservation);
	}

	auto retrieve_success(
		const retrieve_observation& aObservation)
		-> const application::github_credential_retrieve_success*
	{
		return terminal_as<application::github_credential_retrieve_success>(
			aObservation);
	}

	auto delete_success(
		const delete_observation& aObservation)
		-> const application::github_credential_delete_success*
	{
		return terminal_as<application::github_credential_delete_success>(
			aObservation);
	}

	void test_material_contract()
	{
		const auto empty = credential_material::create({ });
		expect(!empty.has_value(), "empty credential material is rejected");
		expect(
			empty.error()
				== application::github_credential_material_error::empty,
			"empty material reports its bounded error");

		const auto maximum = runtime_bytes(
			application::maximum_github_credential_bytes,
			1U);
		auto atLimit = credential_material::create(maximum);
		expect(atLimit.has_value(), "credential material accepts the limit");
		expect(valid(*atLimit), "at-limit credential material is valid");
		expect(
			atLimit->size() == maximum.size(),
			"credential material reports its exact size");
		expect(
			same_bytes(atLimit->bytes(), maximum),
			"credential material exposes exact read-only bytes");

		const auto oversized = runtime_bytes(
			application::maximum_github_credential_bytes + 1U,
			2U);
		const auto tooLarge = credential_material::create(oversized);
		expect(!tooLarge.has_value(), "oversized credential is rejected");
		expect(
			tooLarge.error()
				== application::github_credential_material_error::too_large,
			"oversized material reports its bounded error");

		const auto firstBytes = runtime_bytes(31U, 3U);
		auto       first      = material_from(firstBytes);
		auto       moved      = std::move(first);
		expect(first.empty(), "move construction empties the source"); // NOLINT(bugprone-use-after-move): the moved-from contract is under test.
		expect(!valid(first), "moved-from material is invalid");
		expect(
			same_bytes(moved.bytes(), firstBytes),
			"move construction preserves exact bytes");

		const auto secondBytes = runtime_bytes(29U, 4U);
		auto       second      = material_from(secondBytes);
		second                 = std::move(moved);
		expect(moved.empty(), "move assignment empties the source"); // NOLINT(bugprone-use-after-move): the moved-from contract is under test.
		expect(
			same_bytes(second.bytes(), firstBytes),
			"move assignment replaces prior material");

		second.reset();
		expect(second.empty(), "explicit reset empties material");
		expect(second.bytes().empty(), "reset removes readable bytes");
		expect(!valid(second), "reset material is invalid");

		expect(
			!valid(application::github_credential_material_error::none),
			"material success sentinel is not an error");
		expect(
			!valid(static_cast<application::github_credential_material_error>(
				0xffU)),
			"malformed material error is rejected");
	}

	void test_authority_and_context_validation()
	{
		const auto maximumHostname = std::string(63U, 'a') + "." + std::string(63U, 'b') + "."
			+ std::string(63U, 'c') + "." + std::string(61U, 'd');
		const application::github_canonical_hostname ownedMaximum{
			maximumHostname
		};
		expect(
			maximumHostname.size()
					== application::maximum_github_hostname_bytes
				&& ownedMaximum.valid()
				&& ownedMaximum.value() == maximumHostname,
			"maximum canonical hostname is retained byte-exactly");
		const auto                                   oversizedHostname = maximumHostname + "e";
		const application::github_canonical_hostname rejectedOversized{
			oversizedHostname
		};
		const application::github_canonical_hostname rejectedNoncanonical{
			"GitHub.com"
		};
		expect(
			oversizedHostname.size()
					== application::maximum_github_hostname_bytes + 1U
				&& !rejectedOversized.valid()
				&& rejectedOversized.value().empty()
				&& !rejectedNoncanonical.valid()
				&& rejectedNoncanonical.value().empty(),
			"oversized and non-canonical hostnames have no owned authority");

		const auto baseAuthority = authority();
		const auto baseContext   = context(baseAuthority);
		expect(valid(baseAuthority), "complete credential authority is valid");
		expect(valid(baseContext), "complete operation context is valid");

		auto invalidAuthority    = baseAuthority;
		invalidAuthority.Profile = core::host_profile_id{ };
		expect(!valid(invalidAuthority), "zero profile is rejected");

		invalidAuthority                 = baseAuthority;
		invalidAuthority.ProfileRevision = application::github_profile_revision{ };
		expect(!valid(invalidAuthority), "zero profile revision is rejected");

		invalidAuthority      = baseAuthority;
		invalidAuthority.Host = core::git_host_id{ };
		expect(!valid(invalidAuthority), "zero host is rejected");

		invalidAuthority          = baseAuthority;
		invalidAuthority.Hostname = { };
		expect(!valid(invalidAuthority), "empty hostname is rejected");

		invalidAuthority          = baseAuthority;
		invalidAuthority.Hostname = application::github_canonical_hostname{ "GitHub.com" };
		expect(!valid(invalidAuthority), "non-canonical hostname is rejected");

		const auto enterpriseAuthority = authority(
			1U,
			2U,
			3U,
			4U,
			application::github_server_contract::
				ghes_3_18_2022_11_28);
		auto differentHostname     = enterpriseAuthority;
		differentHostname.Hostname = application::github_canonical_hostname{ "ghe.other.invalid" };
		expect(
			valid(enterpriseAuthority) && valid(differentHostname)
				&& differentHostname != enterpriseAuthority,
			"canonical hostname is an exact credential authority dimension");
		invalidAuthority          = baseAuthority;
		invalidAuthority.Hostname = application::github_canonical_hostname{ "ghe.example.invalid" };
		expect(
			!valid(invalidAuthority),
			"GitHub.com contract rejects an enterprise hostname");
		invalidAuthority          = enterpriseAuthority;
		invalidAuthority.Hostname = application::github_canonical_hostname{ "api.github.com" };
		expect(
			!valid(invalidAuthority),
			"GHES contract rejects a GitHub.com subdomain");

		invalidAuthority         = baseAuthority;
		invalidAuthority.Account = core::github_account_id{ };
		expect(!valid(invalidAuthority), "zero account is rejected");

		invalidAuthority                = baseAuthority;
		invalidAuthority.ServerContract = static_cast<application::github_server_contract>(0xffU);
		expect(!valid(invalidAuthority), "malformed server contract is rejected");

		auto invalidContext    = baseContext;
		invalidContext.Request = core::request_id{ };
		expect(!valid(invalidContext), "zero request is rejected");

		invalidContext                    = baseContext;
		invalidContext.ApplicationSession = core::application_session_id{ };
		expect(!valid(invalidContext), "zero session is rejected");

		invalidContext           = baseContext;
		invalidContext.Authority = invalidAuthority;
		expect(!valid(invalidContext), "invalid authority is rejected in context");

		invalidContext                    = baseContext;
		invalidContext.IdentityGeneration = core::github_identity_generation{ };
		expect(!valid(invalidContext), "zero identity generation is rejected");

		const auto bytes      = runtime_bytes(8U, 5U);
		auto       enrollment = application::github_credential_enrollment{
			.Context    = baseContext,
			.Credential = material_from(bytes),
		};
		expect(valid(enrollment), "complete credential enrollment is valid");
		auto moved = std::move(enrollment.Credential);
		expect(
			!valid(enrollment),
			"enrollment with moved-from credential is rejected");
		expect(valid(moved), "moved enrollment material remains valid");
	}

	void test_invalid_operations_are_rejected()
	{
		using delete_error   = application::github_credential_delete_error;
		using retrieve_error = application::github_credential_retrieve_error;
		using store_error    = application::github_credential_store_error;

		queued_fake_vault vault;
		const auto        key              = authority();
		const auto        storedBytes      = runtime_bytes(18U, 6U);
		const auto        replacementBytes = runtime_bytes(19U, 7U);
		const auto        initial          = context(key, 20U, 21U, 22U);
		store_observation initialStore;
		queue_store(vault, initial, storedBytes, initialStore);
		expect(vault.complete_store(initial), "invalid-operation fixture is stored");

		const auto        invalidStoreContext = context(key, 0U, 21U, 22U);
		store_observation invalidStore;
		queue_store(
			vault,
			invalidStoreContext,
			replacementBytes,
			invalidStore);
		const auto* rejectedStore = terminal_as<store_error>(invalidStore);
		expect(
			invalidStore.Calls == 1U && rejectedStore != nullptr,
			"store rejects an invalid context once");
		expect(
			rejectedStore->Context == invalidStoreContext
				&& rejectedStore->Failure
					== application::github_credential_vault_failure::
						invalid_request,
			"store rejection preserves the supplied invalid context");
		expect(
			!valid(*invalidStore.Terminal),
			"a terminal echoing invalid store context remains invalid");

		auto invalidMaterial = material_from(replacementBytes);
		invalidMaterial.reset();
		const auto        invalidMaterialContext = context(key, 23U, 21U, 22U);
		store_observation invalidMaterialStore;
		vault.store(
			application::github_credential_enrollment{
				.Context    = invalidMaterialContext,
				.Credential = std::move(invalidMaterial),
			},
			observe_completion(invalidMaterialStore));
		expect(
			invalidMaterialStore.Calls == 1U
				&& terminal_as<store_error>(invalidMaterialStore) != nullptr,
			"store rejects invalid credential material once");
		expect(
			valid(*invalidMaterialStore.Terminal),
			"material rejection publishes a valid bounded terminal");

		const auto           invalidRetrieveContext = context(key, 24U, 0U, 22U);
		retrieve_observation invalidRetrieve;
		vault.retrieve(
			invalidRetrieveContext,
			observe_completion(invalidRetrieve));
		const auto* rejectedRetrieve = terminal_as<retrieve_error>(invalidRetrieve);
		expect(
			invalidRetrieve.Calls == 1U && rejectedRetrieve != nullptr,
			"retrieve rejects an invalid context once");
		expect(
			rejectedRetrieve->Context == invalidRetrieveContext
				&& rejectedRetrieve->Failure
					== application::github_credential_vault_failure::
						invalid_request,
			"retrieve rejection preserves the supplied invalid context");
		expect(
			!valid(*invalidRetrieve.Terminal),
			"a terminal echoing invalid retrieve context remains invalid");

		const auto         invalidDeleteContext = context(key, 25U, 21U, 0U);
		delete_observation invalidDelete;
		vault.delete_credential(
			invalidDeleteContext,
			observe_completion(invalidDelete));
		const auto* rejectedDelete = terminal_as<delete_error>(invalidDelete);
		expect(
			invalidDelete.Calls == 1U && rejectedDelete != nullptr,
			"delete rejects an invalid context once");
		expect(
			rejectedDelete->Context == invalidDeleteContext
				&& rejectedDelete->Failure
					== application::github_credential_vault_failure::
						invalid_request,
			"delete rejection preserves the supplied invalid context");
		expect(
			!valid(*invalidDelete.Terminal),
			"a terminal echoing invalid delete context remains invalid");

		expect(vault.pending_count() == 0U, "invalid operations queue no work");
		expect(
			vault.stored_count() == 1U
				&& vault.stored_matches(key, storedBytes),
			"invalid operations do not mutate stored credential material");
	}

	void test_store_retrieve_replace_and_delete()
	{
		queued_fake_vault vault;
		const auto        operation        = context();
		const auto        firstBytes       = runtime_bytes(23U, 6U);
		const auto        replacementBytes = runtime_bytes(27U, 7U);

		store_observation firstStore;
		queue_store(vault, operation, firstBytes, firstStore);
		expect(firstStore.Calls == 0U, "store completion is queued");
		expect(vault.complete_store(operation), "queued store completes");
		expect(firstStore.Calls == 1U, "store has one terminal completion");
		const auto* stored = store_success(firstStore);
		expect(stored != nullptr, "store succeeds");
		expect(stored->Context == operation, "store preserves exact context");
		expect(valid(*firstStore.Terminal), "store terminal is valid");
		expect(
			vault.stored_matches(operation.Authority, firstBytes),
			"store persists exact bytes");

		retrieve_observation firstRetrieve;
		vault.retrieve(operation, observe_completion(firstRetrieve));
		expect(vault.complete_retrieve(operation), "queued retrieval completes");
		const auto* retrieved = retrieve_success(firstRetrieve);
		expect(retrieved != nullptr, "stored credential is retrieved");
		expect(retrieved->Context == operation, "retrieve preserves context");
		expect(
			same_bytes(retrieved->Credential.bytes(), firstBytes),
			"retrieve returns exact stored bytes");
		expect(valid(*firstRetrieve.Terminal), "retrieve terminal is valid");

		const auto        replacementOperation = context(operation.Authority, 13U, 11U, 12U);
		store_observation replacementStore;
		queue_store(
			vault,
			replacementOperation,
			replacementBytes,
			replacementStore);
		expect(
			vault.complete_store(replacementOperation),
			"replacement store completes");
		expect(
			vault.stored_count() == 1U,
			"replacement preserves one authority record");
		expect(
			vault.stored_matches(operation.Authority, replacementBytes),
			"second store atomically replaces authority material");

		const auto           absentOperation = context(authority(91U), 14U, 11U, 12U);
		retrieve_observation absentRetrieve;
		vault.retrieve(absentOperation, observe_completion(absentRetrieve));
		expect(
			vault.complete_retrieve(absentOperation),
			"absent retrieval completes");
		const auto* absent = std::get_if<application::github_credential_retrieve_absent>(
			&*absentRetrieve.Terminal);
		expect(absent != nullptr, "missing authority reports explicit absence");
		expect(absent->Context == absentOperation, "absence preserves context");

		const auto         deleteOperation = context(operation.Authority, 15U, 11U, 12U);
		delete_observation deletion;
		vault.delete_credential(deleteOperation, observe_completion(deletion));
		expect(vault.complete_delete(deleteOperation), "delete completes");
		const auto* deleted = delete_success(deletion);
		expect(deleted != nullptr, "existing credential delete succeeds");
		expect(
			deleted->Disposition
				== application::github_credential_delete_disposition::deleted,
			"existing credential reports deleted");
		expect(vault.stored_count() == 0U, "delete removes authority material");

		const auto         absentDeleteOperation = context(operation.Authority, 16U, 11U, 12U);
		delete_observation absentDeletion;
		vault.delete_credential(
			absentDeleteOperation,
			observe_completion(absentDeletion));
		expect(
			vault.complete_delete(absentDeleteOperation),
			"absent delete completes");
		const auto* absentDelete = delete_success(absentDeletion);
		expect(absentDelete != nullptr, "absent delete is successful");
		expect(
			absentDelete->Disposition
				== application::github_credential_delete_disposition::absent,
			"absent delete reports absence");
	}

	void test_authority_isolation()
	{
		queued_fake_vault vault;
		const auto        authorities = std::vector<authority_key>{
			authority(),
			authority(21U, 2U, 3U, 4U),
			authority(1U, 22U, 3U, 4U),
			authority(1U, 2U, 23U, 4U),
			authority(1U, 2U, 3U, 24U),
			authority(
				1U,
				2U,
				3U,
				4U,
				application::github_server_contract::
					ghes_3_18_2022_11_28),
			authority(
				1U,
				2U,
				3U,
				4U,
				application::github_server_contract::
					ghes_3_18_2022_11_28,
				"ghe.other.invalid"),
		};

		auto credentials = std::vector<std::vector<std::byte>>{ };
		credentials.reserve(authorities.size());
		for (std::size_t index = 0U; index < authorities.size(); ++index) {
			credentials.push_back(runtime_bytes(
				17U + index,
				static_cast<std::uint8_t>(31U + index)));
			const auto operation = context(
				authorities[index],
				100U + index,
				200U,
				300U);
			store_observation observation;
			queue_store(vault, operation, credentials.back(), observation);
			expect(vault.complete_store(operation), "isolated store completes");
			expect(store_success(observation) != nullptr, "isolated store succeeds");
		}

		expect(
			vault.stored_count() == authorities.size(),
			"every authority dimension has an isolated vault entry");

		for (std::size_t index = 0U; index < authorities.size(); ++index) {
			const auto operation = context(
				authorities[index],
				400U + index,
				200U,
				300U);
			retrieve_observation observation;
			vault.retrieve(operation, observe_completion(observation));
			expect(
				vault.complete_retrieve(operation),
				"isolated retrieval completes");
			const auto* retrieved = retrieve_success(observation);
			expect(retrieved != nullptr, "isolated credential is present");
			expect(
				retrieved->Context == operation,
				"isolated retrieval preserves exact context");
			expect(
				same_bytes(retrieved->Credential.bytes(), credentials[index]),
				"isolated retrieval returns only its authority bytes");
		}
	}

	void test_operation_context_correlation_and_reordering()
	{
		queued_fake_vault vault;
		const auto        key     = authority();
		const auto        bytes   = runtime_bytes(19U, 41U);
		const auto        initial = context(key, 500U, 501U, 502U);
		store_observation stored;
		queue_store(vault, initial, bytes, stored);
		expect(vault.complete_store(initial), "correlation fixture is stored");

		const auto first             = context(key, 510U, 501U, 502U);
		const auto requestSibling    = context(key, 511U, 501U, 502U);
		const auto sessionSibling    = context(key, 510U, 512U, 502U);
		const auto generationSibling = context(key, 510U, 501U, 513U);

		retrieve_observation firstObservation;
		retrieve_observation requestObservation;
		retrieve_observation sessionObservation;
		retrieve_observation generationObservation;
		auto                 completionOrder = std::vector<operation_context>{ };

		vault.retrieve(
			first,
			observe_successful_retrieve(
				firstObservation,
				completionOrder,
				"first correlated retrieval succeeds"));
		vault.retrieve(
			requestSibling,
			observe_successful_retrieve(
				requestObservation,
				completionOrder,
				"request sibling retrieval succeeds"));
		vault.retrieve(
			sessionSibling,
			observe_successful_retrieve(
				sessionObservation,
				completionOrder,
				"session sibling retrieval succeeds"));
		vault.retrieve(
			generationSibling,
			observe_successful_retrieve(
				generationObservation,
				completionOrder,
				"generation sibling retrieval succeeds"));

		expect(
			!vault.complete_retrieve(context(key, 999U, 501U, 502U)),
			"unknown correlation does not complete a sibling");
		expect(
			vault.complete_retrieve(generationSibling),
			"generation sibling completes out of order");
		expect(
			vault.complete_retrieve(requestSibling),
			"request sibling completes out of order");
		expect(
			vault.complete_retrieve(first),
			"first request completes out of order");
		expect(
			vault.complete_retrieve(sessionSibling),
			"session sibling completes out of order");

		expect(completionOrder.size() == 4U, "all reordered operations complete");
		expect(
			completionOrder[0] == generationSibling
				&& completionOrder[1] == requestSibling
				&& completionOrder[2] == first
				&& completionOrder[3] == sessionSibling,
			"terminal order follows selected exact contexts");
		expect(
			retrieve_success(firstObservation)->Context == first,
			"request context is exactly correlated");
		expect(
			retrieve_success(requestObservation)->Context == requestSibling,
			"request identity is exactly correlated");
		expect(
			retrieve_success(sessionObservation)->Context == sessionSibling,
			"session identity is exactly correlated");
		expect(
			retrieve_success(generationObservation)->Context
				== generationSibling,
			"identity generation is exactly correlated");
	}

	void test_failures_preserve_state()
	{
		queued_fake_vault vault;
		const auto        key      = authority();
		const auto        oldBytes = runtime_bytes(21U, 51U);
		const auto        newBytes = runtime_bytes(22U, 52U);
		const auto        initial  = context(key, 600U, 601U, 602U);
		store_observation initialStore;
		queue_store(vault, initial, oldBytes, initialStore);
		expect(vault.complete_store(initial), "failure fixture is stored");

		const auto        failedStoreOperation = context(key, 610U, 601U, 602U);
		store_observation failedStore;
		queue_store(
			vault,
			failedStoreOperation,
			newBytes,
			failedStore);
		expect(
			vault.complete_store(
				failedStoreOperation,
				application::github_credential_vault_failure::
					operation_failed),
			"injected store failure completes");
		const auto* storeError = std::get_if<application::github_credential_store_error>(
			&*failedStore.Terminal);
		expect(storeError != nullptr, "store failure is explicit");
		expect(
			storeError->Failure
				== application::github_credential_vault_failure::
					operation_failed,
			"store failure preserves bounded reason");
		expect(
			vault.stored_matches(key, oldBytes),
			"failed replacement preserves prior material");

		const auto           failedRetrieveOperation = context(key, 611U, 601U, 602U);
		retrieve_observation failedRetrieve;
		vault.retrieve(
			failedRetrieveOperation,
			observe_completion(failedRetrieve));
		expect(
			vault.complete_retrieve(
				failedRetrieveOperation,
				application::github_credential_vault_failure::locked),
			"injected retrieve failure completes");
		const auto* retrieveError = std::get_if<application::github_credential_retrieve_error>(
			&*failedRetrieve.Terminal);
		expect(retrieveError != nullptr, "retrieve failure is explicit");
		expect(
			!std::holds_alternative<
				application::github_credential_retrieve_success>(
				*failedRetrieve.Terminal),
			"retrieve failure exposes no credential material");

		const auto         failedDeleteOperation = context(key, 612U, 601U, 602U);
		delete_observation failedDelete;
		vault.delete_credential(
			failedDeleteOperation,
			observe_completion(failedDelete));
		expect(
			vault.complete_delete(
				failedDeleteOperation,
				application::github_credential_vault_failure::access_denied),
			"injected delete failure completes");
		const auto* deleteError = std::get_if<application::github_credential_delete_error>(
			&*failedDelete.Terminal);
		expect(deleteError != nullptr, "delete failure is explicit");
		expect(
			vault.stored_matches(key, oldBytes),
			"failed delete preserves prior material");
	}

	void test_cancellation_and_late_completion()
	{
		queued_fake_vault vault;
		const auto        key              = authority();
		const auto        oldBytes         = runtime_bytes(25U, 61U);
		const auto        replacementBytes = runtime_bytes(26U, 62U);
		const auto        initial          = context(key, 700U, 701U, 702U);
		store_observation initialStore;
		queue_store(vault, initial, oldBytes, initialStore);
		expect(vault.complete_store(initial), "cancellation fixture is stored");

		const auto        cancelledStoreOperation = context(key, 710U, 701U, 702U);
		store_observation cancelledStore;
		queue_store(
			vault,
			cancelledStoreOperation,
			replacementBytes,
			cancelledStore);
		vault.cancel(cancelledStoreOperation);
		expect(
			cancelledStore.Calls == 1U,
			"store cancellation has one terminal");
		const auto* storeCancelled = std::get_if<application::github_credential_store_cancelled>(
			&*cancelledStore.Terminal);
		expect(storeCancelled != nullptr, "store reports cancellation");
		expect(
			storeCancelled->Context == cancelledStoreOperation,
			"store cancellation preserves exact context");
		expect(
			!vault.complete_store(cancelledStoreOperation),
			"late store completion is suppressed");
		expect(
			cancelledStore.Calls == 1U,
			"late store completion emits no second terminal");
		expect(
			vault.stored_matches(key, oldBytes),
			"cancelled store does not mutate material");

		const auto           cancelledRetrieveOperation = context(key, 711U, 701U, 702U);
		const auto           siblingRetrieveOperation   = context(key, 712U, 701U, 702U);
		retrieve_observation cancelledRetrieve;
		retrieve_observation siblingRetrieve;
		vault.retrieve(
			cancelledRetrieveOperation,
			observe_completion(cancelledRetrieve));
		vault.retrieve(
			siblingRetrieveOperation,
			observe_completion(siblingRetrieve));
		vault.cancel(cancelledRetrieveOperation);
		expect(
			cancelledRetrieve.Calls == 1U,
			"retrieve cancellation has one terminal");
		const auto* retrieveCancelled = std::get_if<application::github_credential_retrieve_cancelled>(
			&*cancelledRetrieve.Terminal);
		expect(retrieveCancelled != nullptr, "retrieve reports cancellation");
		expect(
			!std::holds_alternative<
				application::github_credential_retrieve_success>(
				*cancelledRetrieve.Terminal),
			"cancelled retrieval exposes no credential");
		expect(
			siblingRetrieve.Calls == 0U,
			"exact cancellation leaves sibling pending");
		expect(
			!vault.complete_retrieve(cancelledRetrieveOperation),
			"late retrieve completion is suppressed");
		expect(
			vault.complete_retrieve(siblingRetrieveOperation),
			"unaffected retrieve sibling completes");
		expect(
			retrieve_success(siblingRetrieve) != nullptr,
			"unaffected retrieve sibling succeeds");
		expect(
			cancelledRetrieve.Calls == 1U,
			"late retrieve emits no second terminal");

		const auto         cancelledDeleteOperation = context(key, 713U, 701U, 702U);
		delete_observation cancelledDelete;
		vault.delete_credential(
			cancelledDeleteOperation,
			observe_completion(cancelledDelete));
		vault.cancel(cancelledDeleteOperation);
		expect(
			cancelledDelete.Calls == 1U,
			"delete cancellation has one terminal");
		const auto* deleteCancelled = std::get_if<application::github_credential_delete_cancelled>(
			&*cancelledDelete.Terminal);
		expect(deleteCancelled != nullptr, "delete reports cancellation");
		expect(
			!vault.complete_delete(cancelledDeleteOperation),
			"late delete completion is suppressed");
		expect(
			cancelledDelete.Calls == 1U,
			"late delete emits no second terminal");
		expect(
			vault.stored_matches(key, oldBytes),
			"cancelled delete preserves material");
		expect(vault.pending_count() == 0U, "cancellation leaves no pending work");
	}

	void test_duplicate_pending_context()
	{
		queued_fake_vault    vault;
		const auto           operation = context(authority(), 800U, 801U, 802U);
		retrieve_observation original;
		retrieve_observation duplicate;

		vault.retrieve(operation, observe_completion(original));
		vault.retrieve(operation, observe_completion(duplicate));

		expect(original.Calls == 0U, "original duplicate context remains pending");
		expect(duplicate.Calls == 1U, "duplicate context is rejected once");
		const auto* error = std::get_if<application::github_credential_retrieve_error>(
			&*duplicate.Terminal);
		expect(error != nullptr, "duplicate context reports an error");
		expect(
			error->Failure
				== application::github_credential_vault_failure::
					invalid_request,
			"duplicate context reports invalid request");
		expect(vault.pending_count() == 1U, "duplicate does not disturb original");
		expect(
			vault.complete_retrieve(operation),
			"original duplicate context completes");
		expect(original.Calls == 1U, "original receives one terminal");
		expect(
			std::holds_alternative<
				application::github_credential_retrieve_absent>(
				*original.Terminal),
			"original pending operation retains its behavior");
		expect(
			duplicate.Calls == 1U,
			"original completion does not repeat duplicate terminal");
	}

	void test_terminal_validation()
	{
		using failure            = application::github_credential_vault_failure;
		using store_disposition  = application::github_credential_store_disposition;
		using delete_disposition = application::github_credential_delete_disposition;

		const auto operation = context();
		expect(!valid(failure::none), "failure sentinel is invalid");
		expect(
			valid(failure::unavailable),
			"bounded vault failure is valid");
		expect(
			!valid(static_cast<failure>(0xffU)),
			"malformed vault failure is invalid");
		expect(
			!valid(store_disposition::uninitialized),
			"store disposition sentinel is invalid");
		expect(
			!valid(static_cast<store_disposition>(0xffU)),
			"malformed store disposition is invalid");
		expect(
			!valid(delete_disposition::uninitialized),
			"delete disposition sentinel is invalid");
		expect(
			valid(delete_disposition::deleted)
				&& valid(delete_disposition::absent),
			"bounded delete dispositions are valid");
		expect(
			!valid(static_cast<delete_disposition>(0xffU)),
			"malformed delete disposition is invalid");

		const auto malformedStoreSuccess = application::github_credential_store_success{
			.Context     = operation,
			.Disposition = static_cast<store_disposition>(0xffU),
		};
		expect(
			!valid(malformedStoreSuccess),
			"store success rejects malformed result enum");

		const auto malformedStoreError = application::github_credential_store_error{
			.Context = operation,
			.Failure = static_cast<failure>(0xffU),
		};
		expect(
			!valid(malformedStoreError),
			"store error rejects malformed failure");

		const auto malformedRetrieveError = application::github_credential_retrieve_error{
			.Context = operation,
			.Failure = failure::none,
		};
		expect(
			!valid(malformedRetrieveError),
			"retrieve error rejects failure sentinel");

		const auto malformedDeleteSuccess = application::github_credential_delete_success{
			.Context     = operation,
			.Disposition = static_cast<delete_disposition>(0xffU),
		};
		expect(
			!valid(malformedDeleteSuccess),
			"delete success rejects malformed result enum");

		const auto malformedDeleteError = application::github_credential_delete_error{
			.Context = operation,
			.Failure = static_cast<failure>(0xffU),
		};
		expect(
			!valid(malformedDeleteError),
			"delete error rejects malformed failure");

		const auto invalidOperation = context(authority(), 0U, 11U, 12U);
		expect(
			!valid(application::github_credential_store_cancelled{
				.Context = invalidOperation,
			}),
			"store cancellation rejects invalid context");
		expect(
			!valid(application::github_credential_retrieve_absent{
				.Context = invalidOperation,
			}),
			"retrieve absence rejects invalid context");
		expect(
			!valid(application::github_credential_delete_cancelled{
				.Context = invalidOperation,
			}),
			"delete cancellation rejects invalid context");

		const auto validStore = application::github_credential_store_completion{
			application::github_credential_store_success{
				.Context     = operation,
				.Disposition = store_disposition::stored,
			}
		};
		expect(valid(validStore), "valid store variant is accepted");

		const auto validDelete = application::github_credential_delete_completion{
			application::github_credential_delete_success{
				.Context     = operation,
				.Disposition = delete_disposition::absent,
			}
		};
		expect(valid(validDelete), "valid delete variant is accepted");

		const auto invalidStore = application::github_credential_store_completion{
			malformedStoreSuccess
		};
		expect(!valid(invalidStore), "invalid store variant is rejected");

		const auto bytes         = runtime_bytes(7U, 71U);
		const auto validRetrieve = application::github_credential_retrieve_completion{
			application::github_credential_retrieve_success{
				.Context    = operation,
				.Credential = material_from(bytes),
			}
		};
		expect(valid(validRetrieve), "valid retrieve variant is accepted");
	}
}

auto main() -> int
{
	test_material_contract();
	test_authority_and_context_validation();
	test_invalid_operations_are_rejected();
	test_store_retrieve_replace_and_delete();
	test_authority_isolation();
	test_operation_context_correlation_and_reordering();
	test_failures_preserve_state();
	test_cancellation_and_late_completion();
	test_duplicate_pending_context();
	test_terminal_validation();
	return EXIT_SUCCESS;
}
