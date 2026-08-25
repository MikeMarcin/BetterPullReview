#include "macos_keychain.hpp"
#include "macos_keychain_test_support.hpp"

#include "bpr/application/manual_executor.hpp"

#include <Security/SecBase.h>

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {
	using namespace bpr;
	using namespace bpr::adapters::github;
	using namespace bpr::tests::macos_keychain;

	using authority_key       = application::github_credential_authority_key;
	using operation_context   = application::github_credential_operation_context;
	using credential_material = application::github_credential_material;
	using vault_failure       = application::github_credential_vault_failure;

	static_assert(std::is_abstract_v<macos_keychain_backend>);
	static_assert(!std::is_copy_constructible_v<macos_keychain_backend>);
	static_assert(!std::is_move_constructible_v<macos_keychain_backend>);
	static_assert(
		!std::copy_constructible<macos_keychain_backend_retrieve_result>);
	static_assert(
		std::move_constructible<macos_keychain_backend_retrieve_result>);
	using native_factory_signature = auto (*)(
		application::effect_executor&)
		-> macos_keychain_vault_factory_result;
	using injected_factory_signature = auto (*)(
		application::effect_executor&,
		std::shared_ptr<macos_keychain_backend>,
		std::shared_ptr<macos_keychain_backend>) noexcept
		-> macos_keychain_vault_factory_result;
	static_assert(std::is_same_v<
				  decltype(&make_macos_keychain_vault),
				  native_factory_signature>);
	static_assert(std::is_same_v<
				  decltype(&make_macos_keychain_integration_test_vault),
				  native_factory_signature>);
	static_assert(std::is_same_v<
				  decltype(&make_macos_keychain_vault_with_backend),
				  injected_factory_signature>);

	[[nodiscard]] auto hex_encoded(const std::string_view aInput) -> std::string
	{
		constexpr std::string_view digits = "0123456789abcdef";
		std::string                result;
		result.reserve(aInput.size() * 2U);
		for (const auto character : aInput) {
			const auto value = static_cast<unsigned char>(character);
			result.push_back(digits[value >> 4U]);
			result.push_back(digits[value & 0x0fU]);
		}
		return result;
	}

	struct wipe_observation
	{
		std::size_t Buffers = 0U;
		std::size_t Bytes   = 0U;
	};

	class wiping_buffer final
	{
	public:
		wiping_buffer(
			const std::span<const std::byte>  aBytes,
			std::shared_ptr<wipe_observation> aObservation)
			: Bytes{ std::make_unique<std::byte[]>(aBytes.size()) }
			, Size{ aBytes.size() }
			, Observation{ std::move(aObservation) }
		{
			std::ranges::copy(aBytes, Bytes.get());
		}

		~wiping_buffer() noexcept
		{
			reset();
		}

		wiping_buffer(const wiping_buffer&)                    = delete;
		auto operator=(const wiping_buffer&) -> wiping_buffer& = delete;

		wiping_buffer(wiping_buffer&& aOther) noexcept
			: Bytes{ std::move(aOther.Bytes) }
			, Size{ std::exchange(aOther.Size, 0U) }
			, Observation{ std::move(aOther.Observation) }
		{
		}

		auto operator=(wiping_buffer&& aOther) noexcept -> wiping_buffer&
		{
			if (this == &aOther) {
				return *this;
			}
			reset();
			Bytes       = std::move(aOther.Bytes);
			Size        = std::exchange(aOther.Size, 0U);
			Observation = std::move(aOther.Observation);
			return *this;
		}

		[[nodiscard]] auto bytes() const noexcept
			-> std::span<const std::byte>
		{
			return { Bytes.get(), Size };
		}

	private:
		void reset() noexcept
		{
			if (Bytes == nullptr) {
				Size = 0U;
				return;
			}

			const auto wiped  = Size;
			auto*      cursor = reinterpret_cast<volatile unsigned char*>(Bytes.get());
			while (Size != 0U) {
				*cursor = 0U;
				++cursor;
				--Size;
			}
			Bytes.reset();
			if (Observation != nullptr) {
				++Observation->Buffers;
				Observation->Bytes += wiped;
			}
		}

		std::unique_ptr<std::byte[]>      Bytes;
		std::size_t                       Size = 0U;
		std::shared_ptr<wipe_observation> Observation;
	};

	enum class backend_operation : std::uint8_t {
		store,
		retrieve,
		delete_credential,
	};

	enum class injected_exception : std::uint8_t {
		none,
		allocation,
		other,
	};

	struct backend_call
	{
		backend_operation Operation = backend_operation::retrieve;
		authority_key     Authority;
	};

	class wiping_fake_backend final : public macos_keychain_backend
	{
	public:
		explicit wiping_fake_backend(
			std::shared_ptr<wipe_observation> aObservation)
			: Observation{ std::move(aObservation) }
		{
		}

		[[nodiscard]] auto store(
			const authority_key&       aAuthorityValue,
			const credential_material& aCredential)
			-> macos_keychain_backend_store_result override
		{
			Calls.push_back({
				.Operation = backend_operation::store,
				.Authority = aAuthorityValue,
			});
			run_hook(StoreHook);
			throw_if_requested(NextStoreException);

			if (NextStoreFailure.has_value()) {
				const auto failure = std::exchange(NextStoreFailure, std::nullopt);
				return std::unexpected{ *failure };
			}
			if (MalformedStoreResult) {
				MalformedStoreResult = false;
				return application::github_credential_store_disposition::
					uninitialized;
			}

			Stored.insert_or_assign(
				aAuthorityValue,
				wiping_buffer{ aCredential.bytes(), Observation });
			return application::github_credential_store_disposition::stored;
		}

		[[nodiscard]] auto retrieve(
			const authority_key& aAuthorityValue)
			-> macos_keychain_backend_retrieve_result override
		{
			Calls.push_back({
				.Operation = backend_operation::retrieve,
				.Authority = aAuthorityValue,
			});
			run_hook(RetrieveHook);
			throw_if_requested(NextRetrieveException);

			if (NextRetrieveFailure.has_value()) {
				const auto failure = std::exchange(NextRetrieveFailure, std::nullopt);
				return *failure;
			}
			if (MalformedRetrieveResult) {
				MalformedRetrieveResult = false;
				return vault_failure::none;
			}

			const auto found = Stored.find(aAuthorityValue);
			if (found == Stored.end()) {
				return macos_keychain_credential_absent{ };
			}
			auto material = credential_material::create(found->second.bytes());
			if (!material.has_value()) {
				return vault_failure::allocation_failure;
			}
			return std::move(*material);
		}

		[[nodiscard]] auto delete_credential(
			const authority_key& aAuthorityValue)
			-> macos_keychain_backend_delete_result override
		{
			Calls.push_back({
				.Operation = backend_operation::delete_credential,
				.Authority = aAuthorityValue,
			});
			run_hook(DeleteHook);
			throw_if_requested(NextDeleteException);

			if (NextDeleteFailure.has_value()) {
				const auto failure = std::exchange(NextDeleteFailure, std::nullopt);
				return std::unexpected{ *failure };
			}
			if (MalformedDeleteResult) {
				MalformedDeleteResult = false;
				return application::github_credential_delete_disposition::
					uninitialized;
			}

			const auto removed = Stored.erase(aAuthorityValue);
			return removed == 0U
				? application::github_credential_delete_disposition::absent
				: application::github_credential_delete_disposition::deleted;
		}

		[[nodiscard]] auto contains(
			const authority_key& aAuthorityValue) const -> bool
		{
			return Stored.contains(aAuthorityValue);
		}

		[[nodiscard]] auto matches(
			const authority_key&             aAuthorityValue,
			const std::span<const std::byte> aExpected) const -> bool
		{
			const auto found = Stored.find(aAuthorityValue);
			return found != Stored.end()
				&& same_bytes(found->second.bytes(), aExpected);
		}

		std::vector<backend_call>    Calls;
		std::optional<vault_failure> NextStoreFailure;
		std::optional<vault_failure> NextRetrieveFailure;
		std::optional<vault_failure> NextDeleteFailure;
		injected_exception           NextStoreException      = injected_exception::none;
		injected_exception           NextRetrieveException   = injected_exception::none;
		injected_exception           NextDeleteException     = injected_exception::none;
		bool                         MalformedStoreResult    = false;
		bool                         MalformedRetrieveResult = false;
		bool                         MalformedDeleteResult   = false;
		std::function<void()>        StoreHook;
		std::function<void()>        RetrieveHook;
		std::function<void()>        DeleteHook;

	private:
		static void run_hook(std::function<void()>& aHook)
		{
			auto oneShot = std::exchange(aHook, { });
			if (oneShot) {
				oneShot();
			}
		}

		static void throw_if_requested(injected_exception& aRequested)
		{
			const auto exception = std::exchange(aRequested, injected_exception::none);
			switch (exception) {
			case injected_exception::none:
				return;
			case injected_exception::allocation:
				throw std::bad_alloc{ };
			case injected_exception::other:
				throw std::runtime_error{ "categorical fake backend failure" };
			}
		}

		std::map<authority_key, wiping_buffer> Stored;
		std::shared_ptr<wipe_observation>      Observation;
	};

	class throwing_effect_executor final : public application::effect_executor
	{
	public:
		explicit throwing_effect_executor(
			const injected_exception aException) noexcept
			: Exception{ aException }
		{
		}

		void post(std::function<void()> aTask) override
		{
			switch (Exception) {
			case injected_exception::allocation:
				throw std::bad_alloc{ };
			case injected_exception::other:
				throw std::runtime_error{ "categorical executor failure" };
			case injected_exception::none:
				aTask();
				return;
			}
		}

	private:
		injected_exception Exception;
	};

	template <typename Completion>
	struct terminal_recorder
	{
		std::size_t               Calls = 0U;
		std::optional<Completion> Terminal;

		void record(Completion aCompletion)
		{
			++Calls;
			Terminal.emplace(std::move(aCompletion));
		}
	};

	using store_recorder    = terminal_recorder<application::github_credential_store_completion>;
	using retrieve_recorder = terminal_recorder<application::github_credential_retrieve_completion>;
	using delete_recorder   = terminal_recorder<application::github_credential_delete_completion>;

	[[nodiscard]] auto generated_bytes(
		const std::uint8_t aSeed,
		const std::size_t  aSize = 41U) -> std::vector<std::byte>
	{
		auto bytes = std::vector<std::byte>(aSize);
		auto value = aSeed;
		for (auto& byte : bytes) {
			value = static_cast<std::uint8_t>(
				static_cast<std::uint16_t>(value) * 37U + 19U);
			byte = static_cast<std::byte>(value);
		}
		return bytes;
	}

	[[nodiscard]] auto make_test_vault(
		application::manual_executor&               aLane,
		const std::shared_ptr<wiping_fake_backend>& aBackend)
		-> std::unique_ptr<application::github_credential_vault_port>
	{
		auto result = make_macos_keychain_vault_with_backend(aLane, aBackend);
		expect(result.has_value(), "injected Keychain vault is created");
		return std::move(*result);
	}

	[[nodiscard]] auto make_test_vault_with_refresh(
		application::manual_executor&               aLane,
		const std::shared_ptr<wiping_fake_backend>& aBackend,
		const std::shared_ptr<wiping_fake_backend>& aRefreshBackend)
		-> std::unique_ptr<application::github_credential_vault_port>
	{
		auto result = make_macos_keychain_vault_with_backend(
			aLane,
			aBackend,
			aRefreshBackend);
		expect(result.has_value(), "injected two-slot Keychain vault is created");
		return std::move(*result);
	}

	void submit_store(
		application::github_credential_vault_port& aVault,
		const operation_context&                   aOperation,
		const std::span<const std::byte>           aBytes,
		store_recorder&                            aRecorder)
	{
		aVault.store(
			application::github_credential_enrollment{
				.Context    = aOperation,
				.Credential = material_from(aBytes),
			},
			[&aRecorder](
				application::github_credential_store_completion aTerminal) {
				aRecorder.record(std::move(aTerminal));
			});
	}

	void submit_retrieve(
		application::github_credential_vault_port& aVault,
		const operation_context&                   aOperation,
		retrieve_recorder&                         aRecorder)
	{
		aVault.retrieve(
			aOperation,
			[&aRecorder](
				application::github_credential_retrieve_completion aTerminal) {
				aRecorder.record(std::move(aTerminal));
			});
	}

	void submit_delete(
		application::github_credential_vault_port& aVault,
		const operation_context&                   aOperation,
		delete_recorder&                           aRecorder)
	{
		aVault.delete_credential(
			aOperation,
			[&aRecorder](
				application::github_credential_delete_completion aTerminal) {
				aRecorder.record(std::move(aTerminal));
			});
	}

	template <typename Terminal, typename Completion>
	[[nodiscard]] auto terminal_as(const std::optional<Completion>& aCompletion)
		-> const Terminal*
	{
		return aCompletion.has_value()
			? std::get_if<Terminal>(&*aCompletion)
			: nullptr;
	}

	void expect_store_success(
		const store_recorder&    aRecorder,
		const operation_context& aOperation)
	{
		const auto* success = terminal_as<
			application::github_credential_store_success>(aRecorder.Terminal);
		expect(
			aRecorder.Calls == 1U && success != nullptr
				&& success->Context == aOperation
				&& success->Disposition
					== application::github_credential_store_disposition::stored,
			"store completes once with exact successful context");
	}

	void expect_store_error(
		const store_recorder&    aRecorder,
		const operation_context& aOperation,
		const vault_failure      aFailure)
	{
		const auto* error = terminal_as<
			application::github_credential_store_error>(aRecorder.Terminal);
		expect(
			aRecorder.Calls == 1U && error != nullptr
				&& error->Context == aOperation && error->Failure == aFailure,
			"store completes once with exact typed failure");
	}

	void expect_store_cancelled(
		const store_recorder&    aRecorder,
		const operation_context& aOperation)
	{
		const auto* cancelled = terminal_as<
			application::github_credential_store_cancelled>(
			aRecorder.Terminal);
		expect(
			aRecorder.Calls == 1U && cancelled != nullptr
				&& cancelled->Context == aOperation,
			"store completes once with exact cancellation");
	}

	void expect_retrieve_success(
		const retrieve_recorder&         aRecorder,
		const operation_context&         aOperation,
		const std::span<const std::byte> aBytes)
	{
		const auto* success = terminal_as<
			application::github_credential_retrieve_success>(
			aRecorder.Terminal);
		expect(
			aRecorder.Calls == 1U && success != nullptr
				&& success->Context == aOperation
				&& same_bytes(success->Credential.bytes(), aBytes),
			"retrieve completes once with exact credential and context");
	}

	void expect_retrieve_absent(
		const retrieve_recorder& aRecorder,
		const operation_context& aOperation)
	{
		const auto* absent = terminal_as<
			application::github_credential_retrieve_absent>(
			aRecorder.Terminal);
		expect(
			aRecorder.Calls == 1U && absent != nullptr
				&& absent->Context == aOperation,
			"retrieve completes once with exact absence");
	}

	void expect_retrieve_error(
		const retrieve_recorder& aRecorder,
		const operation_context& aOperation,
		const vault_failure      aFailure)
	{
		const auto* error = terminal_as<
			application::github_credential_retrieve_error>(
			aRecorder.Terminal);
		expect(
			aRecorder.Calls == 1U && error != nullptr
				&& error->Context == aOperation && error->Failure == aFailure,
			"retrieve completes once with exact typed failure");
	}

	void expect_retrieve_cancelled(
		const retrieve_recorder& aRecorder,
		const operation_context& aOperation)
	{
		const auto* cancelled = terminal_as<
			application::github_credential_retrieve_cancelled>(
			aRecorder.Terminal);
		expect(
			aRecorder.Calls == 1U && cancelled != nullptr
				&& cancelled->Context == aOperation,
			"retrieve completes once with exact cancellation");
	}

	void expect_delete_success(
		const delete_recorder&                                  aRecorder,
		const operation_context&                                aOperation,
		const application::github_credential_delete_disposition aDisposition)
	{
		const auto* success = terminal_as<
			application::github_credential_delete_success>(
			aRecorder.Terminal);
		expect(
			aRecorder.Calls == 1U && success != nullptr
				&& success->Context == aOperation
				&& success->Disposition == aDisposition,
			"delete completes once with exact disposition and context");
	}

	void expect_delete_error(
		const delete_recorder&   aRecorder,
		const operation_context& aOperation,
		const vault_failure      aFailure)
	{
		const auto* error = terminal_as<
			application::github_credential_delete_error>(
			aRecorder.Terminal);
		expect(
			aRecorder.Calls == 1U && error != nullptr
				&& error->Context == aOperation && error->Failure == aFailure,
			"delete completes once with exact typed failure");
	}

	void expect_delete_cancelled(
		const delete_recorder&   aRecorder,
		const operation_context& aOperation)
	{
		const auto* cancelled = terminal_as<
			application::github_credential_delete_cancelled>(
			aRecorder.Terminal);
		expect(
			aRecorder.Calls == 1U && cancelled != nullptr
				&& cancelled->Context == aOperation,
			"delete completes once with exact cancellation");
	}

	void expect_status(
		const OSStatus                   aStatus,
		const macos_keychain_status_kind aKind,
		const vault_failure              aFailure)
	{
		const auto mapped = classify_macos_keychain_status(static_cast<std::int32_t>(aStatus));
		expect(
			mapped.Kind == aKind && mapped.Failure == aFailure,
			"Security status has the exact bounded classification");
	}

	void test_service_and_authority_encoding()
	{
		expect(
			macos_keychain_service_identifier(
				macos_keychain_service_scope::production)
				== "org.betterpullreview.github-native-credential."
				   "file-keychain.v2",
			"production Keychain service is exact and stable");
		expect(
			macos_keychain_service_identifier(
				macos_keychain_service_scope::integration_test)
				== "org.betterpullreview.github-native-credential."
				   "file-keychain.integration-test.v2",
			"integration service is isolated from production");
		// The refresh slot is a distinct service. Sharing the credential service
		// and separating the two by account encoding would let a query for one
		// return the other, because the account encoding is the authority key
		// and both slots deliberately name the same authority.
		expect(
			macos_keychain_service_identifier(
				macos_keychain_service_scope::production_refresh)
				== "org.betterpullreview.github-native-refresh."
				   "file-keychain.v1",
			"production refresh service is exact and stable");
		expect(
			macos_keychain_service_identifier(
				macos_keychain_service_scope::integration_test_refresh)
				== "org.betterpullreview.github-native-refresh.file-keychain."
				   "integration-test.v1",
			"integration refresh service is isolated from production");

		const auto scopes = {
			macos_keychain_service_scope::production,
			macos_keychain_service_scope::integration_test,
			macos_keychain_service_scope::production_refresh,
			macos_keychain_service_scope::integration_test_refresh,
		};
		auto seen = std::vector<std::string_view>{ };
		for (const auto scope : scopes) {
			const auto identifier = macos_keychain_service_identifier(scope);
			expect(valid(scope), "every named scope is valid");
			expect(!identifier.empty(), "every named scope has an identifier");
			for (const auto& other : seen) {
				expect(
					other != identifier,
					"no two scopes share a Keychain service");
			}
			seen.push_back(identifier);
		}

		expect(
			macos_keychain_service_identifier(
				static_cast<macos_keychain_service_scope>(255U))
				.empty(),
			"malformed service scope has no identifier");
		expect(
			!valid(static_cast<macos_keychain_service_scope>(255U)),
			"and is not a valid scope");

		const auto base    = authority();
		const auto encoded = encode_macos_keychain_authority_account(base);
		expect(encoded.has_value(), "valid authority encodes");
		expect(
			std::ranges::all_of(
				*encoded,
				[](const char aCharacter) {
					return (aCharacter >= '0' && aCharacter <= '9')
						|| (aCharacter >= 'a' && aCharacter <= 'f');
				}),
			"account key is fixed-width lowercase hexadecimal");
		const auto encodedView = std::string_view{ encoded->data(), encoded->size() };
		expect(
			encodedView.substr(0U, 66U)
					== "000000000000000100000000000000020000000000000003"
					   "000000000000000401"
				&& encodedView.substr(66U, 2U) == "0a"
				&& encodedView.substr(68U, 20U)
					== "6769746875622e636f6d"
				&& std::ranges::all_of(
					encodedView.substr(88U),
					[](const char aCharacter) { return aCharacter == '0'; }),
			"GitHub.com authority has the exact stable v2 encoding");
		const auto enterpriseEncoding = encode_macos_keychain_authority_account(
			authority(
				1U,
				2U,
				3U,
				4U,
				application::github_server_contract::
					ghes_3_18_2022_11_28));
		const auto enterpriseView = enterpriseEncoding.has_value()
			? std::string_view{
				  enterpriseEncoding->data(), enterpriseEncoding->size()
			  }
			: std::string_view{ };
		expect(
			enterpriseView.substr(0U, 66U)
					== "000000000000000100000000000000020000000000000003"
					   "000000000000000402"
				&& enterpriseView.substr(66U, 2U) == "13"
				&& enterpriseView.substr(68U, 38U)
					== "6768652e6578616d706c652e696e76616c6964"
				&& std::ranges::all_of(
					enterpriseView.substr(106U),
					[](const char aCharacter) { return aCharacter == '0'; }),
			"GHES 3.18 authority has the exact stable v2 encoding");
		const auto otherEnterpriseEncoding = encode_macos_keychain_authority_account(authority(
			1U,
			2U,
			3U,
			4U,
			application::github_server_contract::
				ghes_3_18_2022_11_28,
			"ghe.other.invalid"));
		expect(
			otherEnterpriseEncoding.has_value()
				&& *otherEnterpriseEncoding != *enterpriseEncoding,
			"canonical hostname is an injective account-key dimension");
		const auto maximumHostname = std::string(63U, 'a') + "." + std::string(63U, 'b') + "."
			+ std::string(63U, 'c') + "." + std::string(61U, 'd');
		const auto maximumEncoding = encode_macos_keychain_authority_account(authority(
			1U,
			2U,
			3U,
			4U,
			application::github_server_contract::
				ghes_3_18_2022_11_28,
			maximumHostname));
		const auto maximumView     = maximumEncoding.has_value()
			? std::string_view{
				  maximumEncoding->data(), maximumEncoding->size()
			  }
			: std::string_view{ };
		expect(
			maximumHostname.size()
					== application::maximum_github_hostname_bytes
				&& maximumView.size()
					== macos_keychain_authority_account_bytes
				&& maximumView.substr(66U, 2U) == "fd"
				&& maximumView.substr(68U) == hex_encoded(maximumHostname),
			"maximum hostname is encoded losslessly without truncation");

		const std::array variants{
			authority(9U, 2U, 3U, 4U),
			authority(1U, 9U, 3U, 4U),
			authority(1U, 2U, 9U, 4U),
			authority(1U, 2U, 3U, 9U),
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
			authority(1U, 23U, 4U, 5U),
			authority(12U, 3U, 4U, 5U),
		};
		for (const auto& variant : variants) {
			const auto different = encode_macos_keychain_authority_account(variant);
			expect(
				different.has_value() && *different != *encoded,
				"every authority dimension is account-key injective");
		}
		const auto adversarialA = encode_macos_keychain_authority_account(
			authority(1U, 23U, 456U, 7U));
		const auto adversarialB = encode_macos_keychain_authority_account(
			authority(12U, 3U, 45U, 67U));
		expect(
			adversarialA.has_value() && adversarialB.has_value()
				&& *adversarialA != *adversarialB,
			"decimal concatenation cannot collide");

		auto malformed    = base;
		malformed.Profile = { };
		expect(
			encode_macos_keychain_authority_account(malformed)
				== std::unexpected{ vault_failure::invalid_request },
			"zero profile is rejected before Keychain");
		malformed                 = base;
		malformed.ProfileRevision = { };
		expect(
			encode_macos_keychain_authority_account(malformed)
				== std::unexpected{ vault_failure::invalid_request },
			"zero profile revision is rejected before Keychain");
		malformed      = base;
		malformed.Host = { };
		expect(
			encode_macos_keychain_authority_account(malformed)
				== std::unexpected{ vault_failure::invalid_request },
			"zero host is rejected before Keychain");
		malformed          = base;
		malformed.Hostname = { };
		expect(
			encode_macos_keychain_authority_account(malformed)
				== std::unexpected{ vault_failure::invalid_request },
			"empty hostname is rejected before Keychain");
		malformed.Account = { };
		expect(
			encode_macos_keychain_authority_account(malformed)
				== std::unexpected{ vault_failure::invalid_request },
			"zero account is rejected before Keychain");
		malformed                = base;
		malformed.ServerContract = static_cast<application::github_server_contract>(255U);
		expect(
			encode_macos_keychain_authority_account(malformed)
				== std::unexpected{ vault_failure::invalid_request },
			"malformed server contract is rejected before Keychain");
	}

	void test_complete_status_classification()
	{
		expect_status(
			errSecSuccess,
			macos_keychain_status_kind::success,
			vault_failure::none);
		expect_status(
			errSecItemNotFound,
			macos_keychain_status_kind::item_not_found,
			vault_failure::none);
		expect_status(
			errSecDuplicateItem,
			macos_keychain_status_kind::duplicate_item,
			vault_failure::none);

		for (const auto status : {
				 errSecInteractionNotAllowed,
				 errSecInteractionRequired,
				 errSecInDarkWake,
			 }) {
			expect_status(
				status,
				macos_keychain_status_kind::failure,
				vault_failure::locked);
		}
		for (const auto status : {
				 errSecAuthFailed,
				 errSecUserCanceled,
				 errSecWrPerm,
				 errSecReadOnly,
				 errSecReadOnlyAttr,
				 errSecMissingEntitlement,
				 errSecRestrictedAPI,
				 errSecNoAccessForItem,
				 errSecInvalidOwnerEdit,
				 errSecDataNotModifiable,
			 }) {
			expect_status(
				status,
				macos_keychain_status_kind::failure,
				vault_failure::access_denied);
		}
		for (const auto status : {
				 errSecDecode,
				 errSecInvalidKeychain,
				 errSecDataNotAvailable,
			 }) {
			expect_status(
				status,
				macos_keychain_status_kind::failure,
				vault_failure::corrupt);
		}
		expect_status(
			errSecAllocate,
			macos_keychain_status_kind::failure,
			vault_failure::allocation_failure);
		for (const auto status : { errSecParam, errSecBadReq }) {
			expect_status(
				status,
				macos_keychain_status_kind::failure,
				vault_failure::invalid_request);
		}
		for (const auto status : {
				 errSecNotAvailable,
				 errSecNoDefaultKeychain,
				 errSecNoSuchKeychain,
				 errSecNoStorageModule,
			 }) {
			expect_status(
				status,
				macos_keychain_status_kind::failure,
				vault_failure::unavailable);
		}
		expect_status(
			errSecInternalComponent,
			macos_keychain_status_kind::failure,
			vault_failure::operation_failed);
	}

	void test_invalid_and_duplicate_requests()
	{
		application::manual_executor lane;
		auto                         observation = std::make_shared<wipe_observation>();
		auto                         backend     = std::make_shared<wiping_fake_backend>(observation);
		auto                         vault       = make_test_vault(lane, backend);
		const auto                   bytes       = generated_bytes(3U);

		auto malformed    = context();
		malformed.Request = { };
		store_recorder invalidStore;
		submit_store(*vault, malformed, bytes, invalidStore);
		expect_store_error(
			invalidStore,
			malformed,
			vault_failure::invalid_request);

		malformed                    = context();
		malformed.ApplicationSession = { };
		retrieve_recorder invalidRetrieve;
		submit_retrieve(*vault, malformed, invalidRetrieve);
		expect_retrieve_error(
			invalidRetrieve,
			malformed,
			vault_failure::invalid_request);

		malformed                    = context();
		malformed.IdentityGeneration = { };
		delete_recorder invalidDelete;
		submit_delete(*vault, malformed, invalidDelete);
		expect_delete_error(
			invalidDelete,
			malformed,
			vault_failure::invalid_request);

		malformed                   = context();
		malformed.Authority.Account = { };
		retrieve_recorder invalidAuthority;
		submit_retrieve(*vault, malformed, invalidAuthority);
		expect_retrieve_error(
			invalidAuthority,
			malformed,
			vault_failure::invalid_request);
		expect(
			lane.pending() == 0U && backend->Calls.empty(),
			"invalid work never reaches the lane or backend");

		auto nullFactory = make_macos_keychain_vault_with_backend(lane, { });
		expect(
			!nullFactory.has_value()
				&& nullFactory.error() == vault_failure::invalid_request,
			"null backend fails closed");

		const auto     duplicateStoreContext = context(authority(), 100U);
		store_recorder firstStore;
		store_recorder duplicateStore;
		submit_store(
			*vault,
			duplicateStoreContext,
			bytes,
			firstStore);
		submit_store(
			*vault,
			duplicateStoreContext,
			bytes,
			duplicateStore);
		expect_store_error(
			duplicateStore,
			duplicateStoreContext,
			vault_failure::invalid_request);
		expect(lane.pending() == 1U, "duplicate store is not queued");
		lane.run_next();
		expect_store_success(firstStore, duplicateStoreContext);

		const auto        duplicateRetrieveContext = context(authority(), 101U);
		retrieve_recorder firstRetrieve;
		retrieve_recorder duplicateRetrieve;
		submit_retrieve(
			*vault,
			duplicateRetrieveContext,
			firstRetrieve);
		submit_retrieve(
			*vault,
			duplicateRetrieveContext,
			duplicateRetrieve);
		expect_retrieve_error(
			duplicateRetrieve,
			duplicateRetrieveContext,
			vault_failure::invalid_request);
		lane.run_next();
		expect_retrieve_success(
			firstRetrieve,
			duplicateRetrieveContext,
			bytes);

		const auto      duplicateDeleteContext = context(authority(), 102U);
		delete_recorder firstDelete;
		delete_recorder duplicateDelete;
		submit_delete(
			*vault,
			duplicateDeleteContext,
			firstDelete);
		submit_delete(
			*vault,
			duplicateDeleteContext,
			duplicateDelete);
		expect_delete_error(
			duplicateDelete,
			duplicateDeleteContext,
			vault_failure::invalid_request);
		lane.run_next();
		expect_delete_success(
			firstDelete,
			duplicateDeleteContext,
			application::github_credential_delete_disposition::deleted);

		const auto        crossKindContext = context(authority(), 103U);
		store_recorder    crossKindStore;
		retrieve_recorder crossKindRetrieve;
		delete_recorder   crossKindDelete;
		submit_store(
			*vault,
			crossKindContext,
			bytes,
			crossKindStore);
		submit_retrieve(
			*vault,
			crossKindContext,
			crossKindRetrieve);
		submit_delete(
			*vault,
			crossKindContext,
			crossKindDelete);
		expect_retrieve_error(
			crossKindRetrieve,
			crossKindContext,
			vault_failure::invalid_request);
		expect_delete_error(
			crossKindDelete,
			crossKindContext,
			vault_failure::invalid_request);
		expect(
			lane.pending() == 1U,
			"duplicate correlation is rejected across operation kinds");
		lane.run_next();
		expect_store_success(crossKindStore, crossKindContext);
	}

	void test_replacement_absence_delete_and_reordering()
	{
		application::manual_executor lane;
		auto                         observation = std::make_shared<wipe_observation>();
		auto                         backend     = std::make_shared<wiping_fake_backend>(observation);
		auto                         vault       = make_test_vault(lane, backend);
		const auto                   first       = generated_bytes(5U);
		const auto                   second      = generated_bytes(7U);
		const auto                   key         = authority();

		const auto     firstContext  = context(key, 200U);
		const auto     secondContext = context(key, 201U);
		store_recorder firstStore;
		store_recorder secondStore;
		submit_store(*vault, firstContext, first, firstStore);
		submit_store(*vault, secondContext, second, secondStore);
		expect(lane.pending() == 2U, "independent contexts can queue");
		lane.run(1U);
		expect_store_success(secondStore, secondContext);
		lane.run(0U);
		expect_store_success(firstStore, firstContext);
		expect(
			backend->matches(key, first),
			"executor reordering determines the atomic replacement order");
		expect(
			observation->Buffers == 1U
				&& observation->Bytes == second.size(),
			"replaced fake material is overwritten");

		const auto        retrieveContext = context(key, 202U);
		retrieve_recorder retrieve;
		submit_retrieve(*vault, retrieveContext, retrieve);
		lane.run_next();
		expect_retrieve_success(retrieve, retrieveContext, first);

		const std::array isolatedAuthorities{
			authority(9U, 2U, 3U, 4U),
			authority(1U, 9U, 3U, 4U),
			authority(1U, 2U, 9U, 4U),
			authority(1U, 2U, 3U, 9U),
			authority(
				1U,
				2U,
				3U,
				4U,
				application::github_server_contract::
					ghes_3_18_2022_11_28),
		};
		auto isolatedRequest = std::uint64_t{ 210U };
		for (const auto& isolated : isolatedAuthorities) {
			const auto        isolatedContext = context(isolated, isolatedRequest++);
			retrieve_recorder isolatedRetrieve;
			submit_retrieve(
				*vault,
				isolatedContext,
				isolatedRetrieve);
			lane.run_next();
			expect_retrieve_absent(
				isolatedRetrieve,
				isolatedContext);
		}

		const auto      deleteContext = context(key, 203U);
		delete_recorder deleted;
		submit_delete(*vault, deleteContext, deleted);
		lane.run_next();
		expect_delete_success(
			deleted,
			deleteContext,
			application::github_credential_delete_disposition::deleted);
		expect(!backend->contains(key), "delete removes exact authority");

		const auto        absentContext = context(key, 204U);
		retrieve_recorder absent;
		submit_retrieve(*vault, absentContext, absent);
		lane.run_next();
		expect_retrieve_absent(absent, absentContext);

		const auto      deleteAbsentContext = context(key, 205U);
		delete_recorder deleteAbsent;
		submit_delete(*vault, deleteAbsentContext, deleteAbsent);
		lane.run_next();
		expect_delete_success(
			deleteAbsent,
			deleteAbsentContext,
			application::github_credential_delete_disposition::absent);
	}

	void test_failures_preserve_existing_material()
	{
		application::manual_executor lane;
		auto                         observation = std::make_shared<wipe_observation>();
		auto                         backend     = std::make_shared<wiping_fake_backend>(observation);
		auto                         vault       = make_test_vault(lane, backend);
		const auto                   original    = generated_bytes(11U);
		const auto                   replacement = generated_bytes(13U);
		const auto                   key         = authority();

		store_recorder initial;
		submit_store(*vault, context(key, 300U), original, initial);
		lane.run_next();
		expect_store_success(initial, context(key, 300U));

		constexpr std::array failures{
			vault_failure::locked,
			vault_failure::unavailable,
			vault_failure::access_denied,
			vault_failure::corrupt,
			vault_failure::allocation_failure,
			vault_failure::operation_failed,
		};
		std::uint64_t request = 301U;
		for (const auto failure : failures) {
			const auto storeContext   = context(key, request++);
			backend->NextStoreFailure = failure;
			store_recorder store;
			submit_store(*vault, storeContext, replacement, store);
			lane.run_next();
			expect_store_error(store, storeContext, failure);
			expect(
				backend->matches(key, original),
				"failed replacement preserves existing material");

			const auto retrieveContext   = context(key, request++);
			backend->NextRetrieveFailure = failure;
			retrieve_recorder retrieve;
			submit_retrieve(*vault, retrieveContext, retrieve);
			lane.run_next();
			expect_retrieve_error(retrieve, retrieveContext, failure);

			const auto deleteContext   = context(key, request++);
			backend->NextDeleteFailure = failure;
			delete_recorder deleted;
			submit_delete(*vault, deleteContext, deleted);
			lane.run_next();
			expect_delete_error(deleted, deleteContext, failure);
			expect(
				backend->matches(key, original),
				"failed delete preserves existing material");
		}

		backend->MalformedStoreResult  = true;
		const auto     badStoreContext = context(key, request++);
		store_recorder badStore;
		submit_store(*vault, badStoreContext, replacement, badStore);
		lane.run_next();
		expect_store_error(
			badStore,
			badStoreContext,
			vault_failure::operation_failed);

		backend->MalformedRetrieveResult     = true;
		const auto        badRetrieveContext = context(key, request++);
		retrieve_recorder badRetrieve;
		submit_retrieve(*vault, badRetrieveContext, badRetrieve);
		lane.run_next();
		expect_retrieve_error(
			badRetrieve,
			badRetrieveContext,
			vault_failure::operation_failed);

		backend->MalformedDeleteResult   = true;
		const auto      badDeleteContext = context(key, request);
		delete_recorder badDelete;
		submit_delete(*vault, badDeleteContext, badDelete);
		lane.run_next();
		expect_delete_error(
			badDelete,
			badDeleteContext,
			vault_failure::operation_failed);
		expect(
			backend->matches(key, original),
			"malformed backend results fail closed without mutation");
	}

	void test_executor_and_backend_exceptions()
	{
		const auto original    = generated_bytes(31U);
		const auto replacement = generated_bytes(37U);
		const auto key         = authority();

		application::manual_executor lane;
		auto                         observation = std::make_shared<wipe_observation>();
		auto                         backend     = std::make_shared<wiping_fake_backend>(observation);
		auto                         vault       = make_test_vault(lane, backend);
		store_recorder               initial;
		submit_store(*vault, context(key, 350U), original, initial);
		lane.run_next();
		expect_store_success(initial, context(key, 350U));

		struct exception_expectation
		{
			injected_exception Exception = injected_exception::none;
			vault_failure      Failure   = vault_failure::operation_failed;
		};
		constexpr std::array exceptions{
			exception_expectation{
				.Exception = injected_exception::allocation,
				.Failure   = vault_failure::allocation_failure,
			},
			exception_expectation{
				.Exception = injected_exception::other,
				.Failure   = vault_failure::operation_failed,
			},
		};
		auto request = std::uint64_t{ 351U };
		for (const auto expectation : exceptions) {
			const auto storeContext     = context(key, request++);
			backend->NextStoreException = expectation.Exception;
			store_recorder store;
			submit_store(*vault, storeContext, replacement, store);
			lane.run_next();
			expect_store_error(store, storeContext, expectation.Failure);
			expect(
				backend->matches(key, original),
				"throwing replacement preserves prior material");

			const auto retrieveContext     = context(key, request++);
			backend->NextRetrieveException = expectation.Exception;
			retrieve_recorder retrieve;
			submit_retrieve(*vault, retrieveContext, retrieve);
			lane.run_next();
			expect_retrieve_error(
				retrieve,
				retrieveContext,
				expectation.Failure);

			const auto deleteContext     = context(key, request++);
			backend->NextDeleteException = expectation.Exception;
			delete_recorder deleted;
			submit_delete(*vault, deleteContext, deleted);
			lane.run_next();
			expect_delete_error(
				deleted,
				deleteContext,
				expectation.Failure);
			expect(
				backend->matches(key, original),
				"throwing delete preserves prior material");
		}

		request = 370U;
		for (const auto expectation : exceptions) {
			throwing_effect_executor executor{ expectation.Exception };
			auto                     postObservation = std::make_shared<wipe_observation>();
			auto                     postBackend     = std::make_shared<wiping_fake_backend>(postObservation);
			auto                     result          = make_macos_keychain_vault_with_backend(
				executor,
				postBackend);
			expect(result.has_value(), "throwing executor vault is created");
			auto postVault = std::move(*result);

			const auto     storeContext = context(key, request++);
			store_recorder store;
			submit_store(
				*postVault,
				storeContext,
				replacement,
				store);
			expect_store_error(store, storeContext, expectation.Failure);

			const auto        retrieveContext = context(key, request++);
			retrieve_recorder retrieve;
			submit_retrieve(
				*postVault,
				retrieveContext,
				retrieve);
			expect_retrieve_error(
				retrieve,
				retrieveContext,
				expectation.Failure);

			const auto      deleteContext = context(key, request++);
			delete_recorder deleted;
			submit_delete(
				*postVault,
				deleteContext,
				deleted);
			expect_delete_error(
				deleted,
				deleteContext,
				expectation.Failure);
			expect(
				postBackend->Calls.empty(),
				"executor post failure never reaches the backend");
		}
	}

	void test_cancellation_races_and_sibling_isolation()
	{
		application::manual_executor lane;
		auto                         observation = std::make_shared<wipe_observation>();
		auto                         backend     = std::make_shared<wiping_fake_backend>(observation);
		auto                         vault       = make_test_vault(lane, backend);
		const auto                   bytes       = generated_bytes(17U);
		const auto                   key         = authority();

		const auto     queuedStoreContext = context(key, 400U);
		store_recorder queuedStore;
		submit_store(*vault, queuedStoreContext, bytes, queuedStore);
		vault->cancel(queuedStoreContext);
		expect_store_cancelled(queuedStore, queuedStoreContext);
		expect(!backend->contains(key), "cancelled store has no side effect");
		lane.run_next();
		expect(
			queuedStore.Calls == 1U && backend->Calls.empty(),
			"late queued store task is suppressed");

		const auto        queuedRetrieveContext = context(key, 401U);
		retrieve_recorder queuedRetrieve;
		submit_retrieve(
			*vault,
			queuedRetrieveContext,
			queuedRetrieve);
		vault->cancel(queuedRetrieveContext);
		expect_retrieve_cancelled(
			queuedRetrieve,
			queuedRetrieveContext);
		lane.run_next();
		expect(
			queuedRetrieve.Calls == 1U && backend->Calls.empty(),
			"late queued retrieve task is suppressed");

		const auto      queuedDeleteContext = context(key, 402U);
		delete_recorder queuedDelete;
		submit_delete(*vault, queuedDeleteContext, queuedDelete);
		vault->cancel(queuedDeleteContext);
		expect_delete_cancelled(queuedDelete, queuedDeleteContext);
		lane.run_next();
		expect(
			queuedDelete.Calls == 1U && backend->Calls.empty(),
			"late queued delete task is suppressed");

		const auto     runningStoreContext = context(key, 403U);
		store_recorder runningStore;
		backend->StoreHook = [&vault, runningStoreContext] {
			vault->cancel(runningStoreContext);
		};
		submit_store(*vault, runningStoreContext, bytes, runningStore);
		lane.run_next();
		expect_store_success(runningStore, runningStoreContext);
		expect(
			backend->contains(key),
			"cancel after backend start loses to terminal result");

		const auto        runningRetrieveContext = context(key, 404U);
		retrieve_recorder runningRetrieve;
		backend->RetrieveHook = [&vault, runningRetrieveContext] {
			vault->cancel(runningRetrieveContext);
		};
		submit_retrieve(
			*vault,
			runningRetrieveContext,
			runningRetrieve);
		lane.run_next();
		expect_retrieve_success(
			runningRetrieve,
			runningRetrieveContext,
			bytes);

		const auto        siblingA = context(key, 405U);
		const auto        siblingB = context(key, 406U);
		retrieve_recorder retrieveA;
		retrieve_recorder retrieveB;
		submit_retrieve(*vault, siblingA, retrieveA);
		submit_retrieve(*vault, siblingB, retrieveB);
		vault->cancel(siblingA);
		expect_retrieve_cancelled(retrieveA, siblingA);
		lane.drain();
		expect_retrieve_success(retrieveB, siblingB, bytes);
		expect(
			retrieveA.Calls == 1U,
			"exact cancellation does not receive a late terminal");

		const auto      runningDeleteContext = context(key, 407U);
		delete_recorder runningDelete;
		backend->DeleteHook = [&vault, runningDeleteContext] {
			vault->cancel(runningDeleteContext);
		};
		submit_delete(
			*vault,
			runningDeleteContext,
			runningDelete);
		lane.run_next();
		expect_delete_success(
			runningDelete,
			runningDeleteContext,
			application::github_credential_delete_disposition::deleted);
	}

	void test_teardown_and_callback_reentrancy()
	{
		application::manual_executor lane;
		auto                         observation = std::make_shared<wipe_observation>();
		auto                         backend     = std::make_shared<wiping_fake_backend>(observation);
		auto                         vault       = make_test_vault(lane, backend);
		const auto                   bytes       = generated_bytes(19U);
		const auto                   key         = authority();

		const auto        storeContext    = context(key, 500U);
		const auto        retrieveContext = context(key, 501U);
		store_recorder    store;
		retrieve_recorder reentrantRetrieve;
		vault->store(
			application::github_credential_enrollment{
				.Context    = storeContext,
				.Credential = material_from(bytes),
			},
			[&store,
			 &reentrantRetrieve,
			 &vault,
			 retrieveContext](
				application::github_credential_store_completion aTerminal) {
				store.record(std::move(aTerminal));
				submit_retrieve(
					*vault,
					retrieveContext,
					reentrantRetrieve);
			});
		lane.run_next();
		expect_store_success(store, storeContext);
		expect(
			lane.pending() == 1U,
			"completion can safely enqueue a reentrant operation");
		lane.run_next();
		expect_retrieve_success(
			reentrantRetrieve,
			retrieveContext,
			bytes);

		const auto        teardownStoreContext    = context(key, 502U);
		const auto        teardownRetrieveContext = context(key, 503U);
		const auto        teardownDeleteContext   = context(key, 504U);
		store_recorder    teardownStore;
		retrieve_recorder teardownRetrieve;
		delete_recorder   teardownDelete;
		submit_store(
			*vault,
			teardownStoreContext,
			bytes,
			teardownStore);
		submit_retrieve(
			*vault,
			teardownRetrieveContext,
			teardownRetrieve);
		submit_delete(
			*vault,
			teardownDeleteContext,
			teardownDelete);
		const auto callsBeforeTeardown = backend->Calls.size();
		vault.reset();
		expect_store_cancelled(teardownStore, teardownStoreContext);
		expect_retrieve_cancelled(
			teardownRetrieve,
			teardownRetrieveContext);
		expect_delete_cancelled(
			teardownDelete,
			teardownDeleteContext);
		lane.drain();
		expect(
			backend->Calls.size() == callsBeforeTeardown
				&& teardownStore.Calls == 1U
				&& teardownRetrieve.Calls == 1U
				&& teardownDelete.Calls == 1U,
			"teardown terminals queued work once and posted tasks no-op");
	}

	void test_teardown_atomically_claims_all_queued_work()
	{
		application::manual_executor lane;
		auto                         observation   = std::make_shared<wipe_observation>();
		auto                         backend       = std::make_shared<wiping_fake_backend>(observation);
		auto                         vault         = make_test_vault(lane, backend);
		const auto                   bytes         = generated_bytes(21U);
		const auto                   firstContext  = context(authority(), 550U);
		const auto                   secondContext = context(authority(), 551U);
		store_recorder               first;
		store_recorder               second;

		std::mutex              gateMutex;
		std::condition_variable gate;
		auto                    firstCallbackEntered = false;
		auto                    releaseFirstCallback = false;

		vault->store(
			application::github_credential_enrollment{
				.Context    = firstContext,
				.Credential = material_from(bytes),
			},
			[&first,
			 &gateMutex,
			 &gate,
			 &firstCallbackEntered,
			 &releaseFirstCallback](
				application::github_credential_store_completion aTerminal) {
				first.record(std::move(aTerminal));
				auto lock            = std::unique_lock{ gateMutex };
				firstCallbackEntered = true;
				gate.notify_all();
				gate.wait(lock, [&releaseFirstCallback] {
					return releaseFirstCallback;
				});
			});
		submit_store(
			*vault,
			secondContext,
			bytes,
			second);
		expect(
			lane.pending() == 2U,
			"two teardown-race operations are queued");

		auto destroyer = std::thread{ [&vault] {
			vault.reset();
		} };

		auto entered = false;
		{
			auto lock = std::unique_lock{ gateMutex };
			entered   = gate.wait_for(
				lock,
				std::chrono::seconds{ 2 },
				[&firstCallbackEntered] {
					return firstCallbackEntered;
				});
		}
		if (!entered) {
			{
				const auto lock      = std::scoped_lock{ gateMutex };
				releaseFirstCallback = true;
			}
			gate.notify_all();
			destroyer.join();
			expect(false, "teardown reaches the first cancellation callback");
		}

		lane.run(1U);
		{
			const auto lock      = std::scoped_lock{ gateMutex };
			releaseFirstCallback = true;
		}
		gate.notify_all();
		destroyer.join();

		expect_store_cancelled(first, firstContext);
		expect_store_cancelled(second, secondContext);
		expect(
			backend->Calls.empty(),
			"teardown atomically claims every queued operation");

		const auto firstCalls  = first.Calls;
		const auto secondCalls = second.Calls;
		lane.drain();
		expect(
			backend->Calls.empty() && first.Calls == firstCalls
				&& second.Calls == secondCalls,
			"posted tasks no-op after concurrent teardown");
	}

	void test_fake_backend_wipes_on_replacement_delete_and_teardown()
	{
		application::manual_executor lane;
		auto                         observation = std::make_shared<wipe_observation>();
		auto                         backend     = std::make_shared<wiping_fake_backend>(observation);
		auto                         vault       = make_test_vault(lane, backend);
		const auto                   first       = generated_bytes(23U, 37U);
		const auto                   second      = generated_bytes(29U, 43U);

		store_recorder storeFirst;
		submit_store(
			*vault,
			context(authority(), 600U),
			first,
			storeFirst);
		lane.run_next();

		store_recorder storeSecond;
		submit_store(
			*vault,
			context(authority(), 601U),
			second,
			storeSecond);
		lane.run_next();
		expect(
			observation->Buffers == 1U
				&& observation->Bytes == first.size(),
			"replacement overwrites the prior fake buffer");

		delete_recorder deleted;
		submit_delete(
			*vault,
			context(authority(), 602U),
			deleted);
		lane.run_next();
		expect(
			observation->Buffers == 2U
				&& observation->Bytes == first.size() + second.size(),
			"delete overwrites the current fake buffer");

		store_recorder finalStore;
		submit_store(
			*vault,
			context(authority(), 603U),
			first,
			finalStore);
		lane.run_next();
		vault.reset();
		backend.reset();
		expect(
			observation->Buffers == 3U
				&& observation->Bytes
					== first.size() + second.size() + first.size(),
			"backend teardown overwrites every remaining fake byte");
	}

	// The slot an operation belongs to travels with it. These two prove the
	// consequence: refresh work reaches only the refresh service, credential
	// work reaches only the credential service, and a vault built without a
	// refresh service says so in the type system rather than falling back.
	void test_the_refresh_slot_reaches_only_its_own_service()
	{
		application::manual_executor lane;
		auto                         observation    = std::make_shared<wipe_observation>();
		auto                         backend        = std::make_shared<wiping_fake_backend>(observation);
		auto                         refreshBackend = std::make_shared<wiping_fake_backend>(observation);
		auto                         vault          = make_test_vault_with_refresh(lane, backend, refreshBackend);

		const auto operation = context(authority(), 700U);
		const auto refresh   = generated_bytes(23U);

		store_recorder stored;
		vault->store_refresh(
			application::github_credential_enrollment{
				.Context    = operation,
				.Credential = material_from(refresh),
			},
			[&stored](application::github_credential_store_completion aTerminal) {
				stored.record(std::move(aTerminal));
			});
		lane.run_next();
		expect(stored.Calls == 1U, "the refresh store terminates once");
		expect(
			terminal_as<application::github_credential_store_success>(
				stored.Terminal)
				!= nullptr,
			"the refresh store succeeds against its own backend");
		expect(
			refreshBackend->Calls.size() == 1U,
			"the refresh backend performed the work");
		expect(
			backend->Calls.empty(),
			"and the credential backend never saw the refresh material");
		expect(
			refreshBackend->matches(operation.Authority, refresh),
			"the refresh service holds the exact bytes");

		retrieve_recorder credential;
		submit_retrieve(*vault, context(authority(), 701U), credential);
		lane.run_next();
		expect(
			terminal_as<application::github_credential_retrieve_absent>(
				credential.Terminal)
				!= nullptr,
			"a credential query is not answered by the refresh service");

		retrieve_recorder read;
		vault->retrieve_refresh(
			context(authority(), 702U),
			[&read](
				application::github_credential_retrieve_completion aTerminal) {
				read.record(std::move(aTerminal));
			});
		lane.run_next();
		const auto* readBack = terminal_as<
			application::github_credential_retrieve_success>(read.Terminal);
		expect(readBack != nullptr, "the refresh slot answers its own query");
		expect(
			same_bytes(readBack->Credential.bytes(), refresh),
			"with the exact refresh bytes");

		delete_recorder removed;
		vault->delete_refresh(
			context(authority(), 703U),
			[&removed](
				application::github_credential_delete_completion aTerminal) {
				removed.record(std::move(aTerminal));
			});
		lane.run_next();
		const auto* deleted = terminal_as<
			application::github_credential_delete_success>(removed.Terminal);
		expect(deleted != nullptr, "the refresh delete succeeds");
		expect(
			deleted->Disposition
				== application::github_credential_delete_disposition::deleted,
			"and reports the entry as removed");
		expect(
			refreshBackend->Calls.size() == 3U,
			"the refresh service performed all three refresh operations");
		expect(
			backend->Calls.size() == 1U
				&& backend->Calls.front().Operation == backend_operation::retrieve,
			"the credential service performed only the one credential query");
	}

	void test_a_vault_without_a_refresh_service_reports_unavailable()
	{
		application::manual_executor lane;
		auto                         observation = std::make_shared<wipe_observation>();
		auto                         backend     = std::make_shared<wiping_fake_backend>(observation);
		auto                         vault       = make_test_vault(lane, backend);

		store_recorder stored;
		vault->store_refresh(
			application::github_credential_enrollment{
				.Context    = context(authority(), 710U),
				.Credential = material_from(generated_bytes(29U)),
			},
			[&stored](application::github_credential_store_completion aTerminal) {
				stored.record(std::move(aTerminal));
			});
		lane.run_next();
		const auto* failure = terminal_as<
			application::github_credential_store_error>(stored.Terminal);
		expect(failure != nullptr, "the refresh store fails");
		expect(
			failure->Failure == vault_failure::unavailable,
			"with the typed unavailable failure");
		expect(
			backend->Calls.empty(),
			"and the credential backend was never asked to hold it");
	}
}

auto main() -> int
{
	test_service_and_authority_encoding();
	test_complete_status_classification();
	test_invalid_and_duplicate_requests();
	test_replacement_absence_delete_and_reordering();
	test_failures_preserve_existing_material();
	test_executor_and_backend_exceptions();
	test_cancellation_races_and_sibling_isolation();
	test_teardown_and_callback_reentrancy();
	test_teardown_atomically_claims_all_queued_work();
	test_fake_backend_wipes_on_replacement_delete_and_teardown();
	test_the_refresh_slot_reaches_only_its_own_service();
	test_a_vault_without_a_refresh_service_reports_unavailable();
	return EXIT_SUCCESS;
}
