#include "linux_secret_service.hpp"
#include "linux_secret_service_test_support.hpp"

#include "bpr/application/manual_executor.hpp"

#include <libsecret/secret.h>

#include <array>
#include <chrono>
#include <concepts>
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
	using namespace bpr::tests::linux_secret_service;

	using authority_key       = application::github_credential_authority_key;
	using operation_context   = application::github_credential_operation_context;
	using credential_material = application::github_credential_material;
	using vault_failure       = application::github_credential_vault_failure;

	static_assert(std::is_abstract_v<linux_secret_service_backend>);
	static_assert(
		!std::is_copy_constructible_v<linux_secret_service_backend>);
	static_assert(
		!std::is_move_constructible_v<linux_secret_service_backend>);
	static_assert(
		!std::copy_constructible<
			linux_secret_service_backend_retrieve_result>);
	static_assert(
		std::move_constructible<
			linux_secret_service_backend_retrieve_result>);
	using native_factory_signature = auto (*)(
		application::effect_executor&)
		-> linux_secret_service_vault_factory_result;
	// [[BPR-0542]] The injected factory takes both stores. The second is
	// defaulted so every existing single-slot caller still reads the same, and
	// pinning the signature here is what stops the refresh backend from being
	// quietly dropped from it again.
	using injected_factory_signature = auto (*)(
		application::effect_executor&,
		std::shared_ptr<linux_secret_service_backend>,
		std::shared_ptr<linux_secret_service_backend>) noexcept
		-> linux_secret_service_vault_factory_result;
	static_assert(std::is_same_v<
				  decltype(&make_linux_secret_service_vault),
				  native_factory_signature>);
	static_assert(std::is_same_v<
				  decltype(&make_linux_secret_service_integration_test_vault),
				  native_factory_signature>);
	static_assert(std::is_same_v<
				  decltype(&make_linux_secret_service_vault_with_backend),
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
			if (this != &aOther) {
				reset();
				Bytes       = std::move(aOther.Bytes);
				Size        = std::exchange(aOther.Size, 0U);
				Observation = std::move(aOther.Observation);
			}
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

	class wiping_fake_backend final : public linux_secret_service_backend
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
			-> linux_secret_service_backend_store_result override
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
			-> linux_secret_service_backend_retrieve_result override
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
				return linux_secret_service_credential_absent{ };
			}
			auto material = credential_material::create(found->second.bytes());
			if (!material.has_value()) {
				return vault_failure::allocation_failure;
			}
			return std::move(*material);
		}

		[[nodiscard]] auto delete_credential(
			const authority_key& aAuthorityValue)
			-> linux_secret_service_backend_delete_result override
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
		auto result = make_linux_secret_service_vault_with_backend(aLane, aBackend);
		expect(result.has_value(), "injected Secret Service vault is created");
		return std::move(*result);
	}

	[[nodiscard]] auto make_test_vault_with_refresh(
		application::manual_executor&               aLane,
		const std::shared_ptr<wiping_fake_backend>& aBackend,
		const std::shared_ptr<wiping_fake_backend>& aRefreshBackend)
		-> std::unique_ptr<application::github_credential_vault_port>
	{
		auto result = make_linux_secret_service_vault_with_backend(
			aLane,
			aBackend,
			aRefreshBackend);
		expect(
			result.has_value(),
			"injected two-slot Secret Service vault is created");
		return std::move(*result);
	}

	void submit_store_refresh(
		application::github_credential_vault_port& aVault,
		const operation_context&                   aOperation,
		const std::span<const std::byte>           aBytes,
		store_recorder&                            aRecorder)
	{
		aVault.store_refresh(
			application::github_credential_enrollment{
				.Context    = aOperation,
				.Credential = material_from(aBytes),
			},
			[&aRecorder](
				application::github_credential_store_completion aTerminal) {
				aRecorder.record(std::move(aTerminal));
			});
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

	template <typename StatusCode>
	void expect_status(
		const linux_secret_service_status_domain aDomain,
		const StatusCode                         aCode,
		const vault_failure                      aFailure)
	{
		expect(
			classify_linux_secret_service_status({
				.Domain = aDomain,
				.Code   = static_cast<std::int32_t>(aCode),
			}) == aFailure,
			"Secret Service status has the exact bounded classification");
	}

	void test_schema_and_authority_encoding()
	{
		constexpr std::string_view productionSchema  = "org.betterpullreview.github-native-credential."
													   "secret-service.v2";
		constexpr std::string_view integrationSchema = "org.betterpullreview.github-native-credential."
													   "secret-service.integration-test.v2";
		expect(
			linux_secret_service_schema_identifier(
				linux_secret_service_schema_scope::production)
				== productionSchema,
			"production Secret Service schema is exact and stable");
		expect(
			linux_secret_service_schema_identifier(
				linux_secret_service_schema_scope::integration_test)
				== integrationSchema,
			"integration schema is isolated from production");
		expect(
			linux_secret_service_schema_identifier(
				static_cast<linux_secret_service_schema_scope>(255U))
				.empty(),
			"malformed schema scope has no identifier");

		// [[BPR-0542]] The refresh slot's schemas, pinned beside the
		// credential's. The property that matters is not the spelling but that
		// no refresh identifier equals a credential identifier: two slots that
		// shared one schema would let either lookup answer for the other.
		constexpr std::string_view productionRefreshSchema  = "org.betterpullreview.github-native-refresh."
															  "secret-service.v1";
		constexpr std::string_view integrationRefreshSchema = "org.betterpullreview.github-native-refresh."
															  "secret-service.integration-test.v1";
		expect(
			linux_secret_service_refresh_schema_identifier(
				linux_secret_service_schema_scope::production)
				== productionRefreshSchema,
			"production refresh schema is exact and stable");
		expect(
			linux_secret_service_refresh_schema_identifier(
				linux_secret_service_schema_scope::integration_test)
				== integrationRefreshSchema,
			"integration refresh schema is isolated from production");
		expect(
			linux_secret_service_refresh_schema_identifier(
				static_cast<linux_secret_service_schema_scope>(255U))
				.empty(),
			"malformed refresh schema scope has no identifier");
		expect(
			productionRefreshSchema != productionSchema
				&& integrationRefreshSchema != integrationSchema
				&& productionRefreshSchema != integrationSchema
				&& integrationRefreshSchema != productionSchema,
			"no refresh schema collides with a credential schema");

		const auto base    = authority();
		const auto encoded = encode_linux_secret_service_authority_account(base);
		expect(encoded.has_value(), "valid authority encodes");
		expect(
			std::ranges::all_of(*encoded, [](const char aCharacter) {
				return (aCharacter >= '0' && aCharacter <= '9')
					|| (aCharacter >= 'a' && aCharacter <= 'f');
			}),
			"authority attribute is fixed-width lowercase hexadecimal");
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

		const auto enterprise     = encode_linux_secret_service_authority_account(authority(
			1U,
			2U,
			3U,
			4U,
			application::github_server_contract::
				ghes_3_18_2022_11_28));
		const auto enterpriseView = enterprise.has_value()
			? std::string_view{ enterprise->data(), enterprise->size() }
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
			"GHES authority has the exact stable v2 encoding");
		const auto otherEnterprise = encode_linux_secret_service_authority_account(authority(
			1U,
			2U,
			3U,
			4U,
			application::github_server_contract::
				ghes_3_18_2022_11_28,
			"ghe.other.invalid"));
		expect(
			otherEnterprise.has_value()
				&& *otherEnterprise != *enterprise,
			"canonical hostname is an injective attribute-key dimension");
		const auto maximumHostname = std::string(63U, 'a') + "." + std::string(63U, 'b') + "."
			+ std::string(63U, 'c') + "." + std::string(61U, 'd');
		const auto maximumEncoding = encode_linux_secret_service_authority_account(authority(
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
					== linux_secret_service_authority_account_bytes
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
			const auto different = encode_linux_secret_service_authority_account(variant);
			expect(
				different.has_value() && *different != *encoded,
				"every authority dimension is attribute-key injective");
		}

		auto malformed    = base;
		malformed.Profile = { };
		expect(
			encode_linux_secret_service_authority_account(malformed)
				== std::unexpected{ vault_failure::invalid_request },
			"zero profile is rejected before Secret Service");
		malformed                 = base;
		malformed.ProfileRevision = { };
		expect(
			encode_linux_secret_service_authority_account(malformed)
				== std::unexpected{ vault_failure::invalid_request },
			"zero profile revision is rejected before Secret Service");
		malformed      = base;
		malformed.Host = { };
		expect(
			encode_linux_secret_service_authority_account(malformed)
				== std::unexpected{ vault_failure::invalid_request },
			"zero host is rejected before Secret Service");
		malformed          = base;
		malformed.Hostname = { };
		expect(
			encode_linux_secret_service_authority_account(malformed)
				== std::unexpected{ vault_failure::invalid_request },
			"empty hostname is rejected before Secret Service");
		malformed.Account = { };
		expect(
			encode_linux_secret_service_authority_account(malformed)
				== std::unexpected{ vault_failure::invalid_request },
			"zero account is rejected before Secret Service");
		malformed                = base;
		malformed.ServerContract = static_cast<application::github_server_contract>(255U);
		expect(
			encode_linux_secret_service_authority_account(malformed)
				== std::unexpected{ vault_failure::invalid_request },
			"malformed contract is rejected before Secret Service");
	}

	void test_status_classification()
	{
		using domain = linux_secret_service_status_domain;

		expect_status(
			domain::secret_service,
			SECRET_ERROR_IS_LOCKED,
			vault_failure::locked);
		for (const auto code : {
				 SECRET_ERROR_PROTOCOL,
				 SECRET_ERROR_INVALID_FILE_FORMAT,
				 SECRET_ERROR_MISMATCHED_SCHEMA,
				 SECRET_ERROR_NO_MATCHING_ATTRIBUTE,
				 SECRET_ERROR_WRONG_TYPE,
				 SECRET_ERROR_EMPTY_TABLE,
			 }) {
			expect_status(
				domain::secret_service,
				code,
				vault_failure::corrupt);
		}
		expect_status(
			domain::secret_service,
			SECRET_ERROR_NO_SUCH_OBJECT,
			vault_failure::unavailable);
		expect_status(
			domain::secret_service,
			SECRET_ERROR_ALREADY_EXISTS,
			vault_failure::operation_failed);

		for (const auto code : {
				 G_DBUS_ERROR_ACCESS_DENIED,
				 G_DBUS_ERROR_AUTH_FAILED,
				 G_DBUS_ERROR_SPAWN_PERMISSIONS_INVALID,
			 }) {
			expect_status(domain::dbus, code, vault_failure::access_denied);
		}
		for (const auto code : {
				 G_DBUS_ERROR_SERVICE_UNKNOWN,
				 G_DBUS_ERROR_NAME_HAS_NO_OWNER,
				 G_DBUS_ERROR_NO_REPLY,
				 G_DBUS_ERROR_BAD_ADDRESS,
				 G_DBUS_ERROR_NO_SERVER,
				 G_DBUS_ERROR_TIMEOUT,
				 G_DBUS_ERROR_DISCONNECTED,
				 G_DBUS_ERROR_UNKNOWN_OBJECT,
				 G_DBUS_ERROR_SPAWN_SERVICE_NOT_FOUND,
			 }) {
			expect_status(domain::dbus, code, vault_failure::unavailable);
		}
		expect_status(
			domain::dbus,
			G_DBUS_ERROR_NO_MEMORY,
			vault_failure::allocation_failure);
		expect_status(
			domain::dbus,
			G_DBUS_ERROR_INVALID_FILE_CONTENT,
			vault_failure::corrupt);

		for (const auto code : {
				 G_IO_ERROR_PERMISSION_DENIED,
				 G_IO_ERROR_PROXY_AUTH_FAILED,
				 G_IO_ERROR_PROXY_NEED_AUTH,
				 G_IO_ERROR_PROXY_NOT_ALLOWED,
			 }) {
			expect_status(domain::io, code, vault_failure::access_denied);
		}
		expect_status(
			domain::io,
			G_IO_ERROR_INVALID_ARGUMENT,
			vault_failure::invalid_request);
		for (const auto code : {
				 G_IO_ERROR_PARTIAL_INPUT,
				 G_IO_ERROR_INVALID_DATA,
				 G_IO_ERROR_MESSAGE_TOO_LARGE,
			 }) {
			expect_status(domain::io, code, vault_failure::corrupt);
		}
		for (const auto code : {
				 G_IO_ERROR_NOT_FOUND,
				 G_IO_ERROR_NOT_INITIALIZED,
				 G_IO_ERROR_CLOSED,
				 G_IO_ERROR_TIMED_OUT,
				 G_IO_ERROR_DBUS_ERROR,
				 G_IO_ERROR_CONNECTION_REFUSED,
				 G_IO_ERROR_BROKEN_PIPE,
				 G_IO_ERROR_NOT_CONNECTED,
			 }) {
			expect_status(domain::io, code, vault_failure::unavailable);
		}
		expect_status(domain::none, 0, vault_failure::operation_failed);
		expect_status(domain::other, 71, vault_failure::operation_failed);
	}

	void test_invalid_duplicate_and_exact_authority()
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
			"invalid work never reaches the effect lane or backend");

		auto nullFactory = make_linux_secret_service_vault_with_backend(lane, { });
		expect(
			!nullFactory.has_value()
				&& nullFactory.error() == vault_failure::invalid_request,
			"null backend fails closed");

		const auto     duplicateContext = context(authority(), 100U);
		store_recorder first;
		store_recorder duplicate;
		submit_store(*vault, duplicateContext, bytes, first);
		submit_store(*vault, duplicateContext, bytes, duplicate);
		expect_store_error(
			duplicate,
			duplicateContext,
			vault_failure::invalid_request);

		retrieve_recorder crossKind;
		submit_retrieve(*vault, duplicateContext, crossKind);
		expect_retrieve_error(
			crossKind,
			duplicateContext,
			vault_failure::invalid_request);
		expect(lane.pending() == 1U, "duplicate correlation queues once");
		lane.run_next();
		expect_store_success(first, duplicateContext);
	}

	void test_replacement_absence_delete_and_reordering()
	{
		application::manual_executor lane;
		auto                         observation = std::make_shared<wipe_observation>();
		auto                         backend     = std::make_shared<wiping_fake_backend>(observation);
		auto                         vault       = make_test_vault(lane, backend);
		const auto                   first       = generated_bytes(5U, 73U);
		const auto                   maximum     = generated_bytes(
			7U,
			application::maximum_github_credential_bytes);
		const auto key = authority();

		store_recorder firstStore;
		store_recorder maximumStore;
		submit_store(*vault, context(key, 200U), first, firstStore);
		submit_store(
			*vault,
			context(key, 201U),
			maximum,
			maximumStore);
		expect(lane.pending() == 2U, "independent contexts can queue");
		lane.run(1U);
		expect_store_success(maximumStore, context(key, 201U));
		expect(
			backend->matches(key, maximum),
			"reordered maximum-size store reaches the backend first");

		retrieve_recorder maximumRetrieve;
		submit_retrieve(
			*vault,
			context(key, 202U),
			maximumRetrieve);
		lane.run(1U);
		expect_retrieve_success(
			maximumRetrieve,
			context(key, 202U),
			maximum);

		lane.run(0U);
		expect_store_success(firstStore, context(key, 200U));
		expect(
			backend->matches(key, first),
			"executor order determines the atomic replacement result");
		expect(
			observation->Buffers == 1U
				&& observation->Bytes == maximum.size(),
			"replacement overwrites all maximum-size fake material");

		retrieve_recorder retrieved;
		submit_retrieve(*vault, context(key, 203U), retrieved);
		lane.run_next();
		expect_retrieve_success(retrieved, context(key, 203U), first);

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
		auto request = std::uint64_t{ 210U };
		for (const auto& isolated : isolatedAuthorities) {
			const auto        isolatedContext = context(isolated, request++);
			retrieve_recorder absent;
			submit_retrieve(*vault, isolatedContext, absent);
			lane.run_next();
			expect_retrieve_absent(absent, isolatedContext);
		}

		delete_recorder deleted;
		submit_delete(*vault, context(key, 204U), deleted);
		lane.run_next();
		expect_delete_success(
			deleted,
			context(key, 204U),
			application::github_credential_delete_disposition::deleted);
		expect(!backend->contains(key), "delete removes exact authority");

		retrieve_recorder absent;
		submit_retrieve(*vault, context(key, 205U), absent);
		lane.run_next();
		expect_retrieve_absent(absent, context(key, 205U));

		delete_recorder deleteAbsent;
		submit_delete(*vault, context(key, 206U), deleteAbsent);
		lane.run_next();
		expect_delete_success(
			deleteAbsent,
			context(key, 206U),
			application::github_credential_delete_disposition::absent);
	}

	void test_failures_and_malformed_results_preserve_material()
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
		auto request = std::uint64_t{ 301U };
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

		backend->MalformedStoreResult = true;
		store_recorder badStore;
		submit_store(*vault, context(key, request++), replacement, badStore);
		lane.run_next();
		expect_store_error(
			badStore,
			context(key, request - 1U),
			vault_failure::operation_failed);

		backend->MalformedRetrieveResult = true;
		retrieve_recorder badRetrieve;
		submit_retrieve(
			*vault,
			context(key, request++),
			badRetrieve);
		lane.run_next();
		expect_retrieve_error(
			badRetrieve,
			context(key, request - 1U),
			vault_failure::operation_failed);

		backend->MalformedDeleteResult = true;
		delete_recorder badDelete;
		submit_delete(*vault, context(key, request), badDelete);
		lane.run_next();
		expect_delete_error(
			badDelete,
			context(key, request),
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

		struct expectation
		{
			injected_exception Exception = injected_exception::none;
			vault_failure      Failure   = vault_failure::operation_failed;
		};
		constexpr std::array cases{
			expectation{
				.Exception = injected_exception::allocation,
				.Failure   = vault_failure::allocation_failure,
			},
			expectation{
				.Exception = injected_exception::other,
				.Failure   = vault_failure::operation_failed,
			},
		};
		auto request = std::uint64_t{ 351U };
		for (const auto expected : cases) {
			const auto storeContext     = context(key, request++);
			backend->NextStoreException = expected.Exception;
			store_recorder store;
			submit_store(*vault, storeContext, replacement, store);
			lane.run_next();
			expect_store_error(store, storeContext, expected.Failure);

			const auto retrieveContext     = context(key, request++);
			backend->NextRetrieveException = expected.Exception;
			retrieve_recorder retrieve;
			submit_retrieve(*vault, retrieveContext, retrieve);
			lane.run_next();
			expect_retrieve_error(
				retrieve,
				retrieveContext,
				expected.Failure);

			const auto deleteContext     = context(key, request++);
			backend->NextDeleteException = expected.Exception;
			delete_recorder deleted;
			submit_delete(*vault, deleteContext, deleted);
			lane.run_next();
			expect_delete_error(
				deleted,
				deleteContext,
				expected.Failure);
			expect(
				backend->matches(key, original),
				"backend exceptions preserve prior material");
		}

		request = 380U;
		for (const auto expected : cases) {
			throwing_effect_executor executor{ expected.Exception };
			auto                     postObservation = std::make_shared<wipe_observation>();
			auto                     postBackend     = std::make_shared<wiping_fake_backend>(postObservation);
			auto                     result          = make_linux_secret_service_vault_with_backend(
				executor,
				postBackend);
			expect(result.has_value(), "throwing executor vault is created");
			auto postVault = std::move(*result);

			const auto     storeContext = context(key, request++);
			store_recorder store;
			submit_store(*postVault, storeContext, replacement, store);
			expect_store_error(store, storeContext, expected.Failure);

			const auto        retrieveContext = context(key, request++);
			retrieve_recorder retrieve;
			submit_retrieve(*postVault, retrieveContext, retrieve);
			expect_retrieve_error(
				retrieve,
				retrieveContext,
				expected.Failure);

			const auto      deleteContext = context(key, request++);
			delete_recorder deleted;
			submit_delete(*postVault, deleteContext, deleted);
			expect_delete_error(
				deleted,
				deleteContext,
				expected.Failure);
			expect(
				postBackend->Calls.empty(),
				"executor post failure never reaches the backend");
		}
	}

	void test_cancellation_reentrancy_and_teardown()
	{
		application::manual_executor lane;
		auto                         observation = std::make_shared<wipe_observation>();
		auto                         backend     = std::make_shared<wiping_fake_backend>(observation);
		auto                         vault       = make_test_vault(lane, backend);
		const auto                   bytes       = generated_bytes(17U);
		const auto                   key         = authority();

		const auto     queuedContext = context(key, 400U);
		store_recorder queued;
		submit_store(*vault, queuedContext, bytes, queued);
		vault->cancel(queuedContext);
		expect_store_cancelled(queued, queuedContext);
		lane.run_next();
		expect(
			queued.Calls == 1U && backend->Calls.empty(),
			"cancelled queued work has no effect or late completion");

		const auto     runningContext = context(key, 401U);
		store_recorder running;
		backend->StoreHook = [&vault, runningContext] {
			vault->cancel(runningContext);
		};
		submit_store(*vault, runningContext, bytes, running);
		lane.run_next();
		expect_store_success(running, runningContext);
		expect(
			backend->contains(key),
			"cancel after backend start loses to terminal result");

		const auto        siblingA = context(key, 402U);
		const auto        siblingB = context(key, 403U);
		retrieve_recorder retrieveA;
		retrieve_recorder retrieveB;
		submit_retrieve(*vault, siblingA, retrieveA);
		submit_retrieve(*vault, siblingB, retrieveB);
		vault->cancel(siblingA);
		expect_retrieve_cancelled(retrieveA, siblingA);
		lane.drain();
		expect_retrieve_success(retrieveB, siblingB, bytes);

		const auto        storeContext     = context(key, 404U);
		const auto        reentrantContext = context(key, 405U);
		store_recorder    store;
		retrieve_recorder reentrant;
		vault->store(
			application::github_credential_enrollment{
				.Context    = storeContext,
				.Credential = material_from(bytes),
			},
			[&store, &reentrant, &vault, reentrantContext](
				application::github_credential_store_completion aTerminal) {
				store.record(std::move(aTerminal));
				submit_retrieve(*vault, reentrantContext, reentrant);
			});
		lane.run_next();
		expect_store_success(store, storeContext);
		expect(lane.pending() == 1U, "callback can enqueue new work");
		lane.run_next();
		expect_retrieve_success(reentrant, reentrantContext, bytes);

		const auto        teardownStoreContext    = context(key, 406U);
		const auto        teardownRetrieveContext = context(key, 407U);
		const auto        teardownDeleteContext   = context(key, 408U);
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
		const auto callsBefore = backend->Calls.size();
		vault.reset();
		expect_store_cancelled(teardownStore, teardownStoreContext);
		expect_retrieve_cancelled(
			teardownRetrieve,
			teardownRetrieveContext);
		expect_delete_cancelled(teardownDelete, teardownDeleteContext);
		lane.drain();
		expect(
			backend->Calls.size() == callsBefore
				&& teardownStore.Calls == 1U
				&& teardownRetrieve.Calls == 1U
				&& teardownDelete.Calls == 1U,
			"teardown terminals queued work once and tasks no-op");
	}

	void test_teardown_atomically_claims_queued_work()
	{
		application::manual_executor lane;
		auto                         observation   = std::make_shared<wipe_observation>();
		auto                         backend       = std::make_shared<wiping_fake_backend>(observation);
		auto                         vault         = make_test_vault(lane, backend);
		const auto                   bytes         = generated_bytes(21U);
		const auto                   firstContext  = context(authority(), 500U);
		const auto                   secondContext = context(authority(), 501U);
		store_recorder               first;
		store_recorder               second;

		std::mutex              gateMutex;
		std::condition_variable gate;
		auto                    callbackEntered = false;
		auto                    releaseCallback = false;

		vault->store(
			application::github_credential_enrollment{
				.Context    = firstContext,
				.Credential = material_from(bytes),
			},
			[&first,
			 &gateMutex,
			 &gate,
			 &callbackEntered,
			 &releaseCallback](
				application::github_credential_store_completion aTerminal) {
				first.record(std::move(aTerminal));
				auto lock       = std::unique_lock{ gateMutex };
				callbackEntered = true;
				gate.notify_all();
				gate.wait(lock, [&releaseCallback] {
					return releaseCallback;
				});
			});
		submit_store(*vault, secondContext, bytes, second);

		auto destroyer = std::thread{ [&vault] {
			vault.reset();
		} };
		auto entered = false;
		{
			auto lock = std::unique_lock{ gateMutex };
			entered   = gate.wait_for(
				lock,
				std::chrono::seconds{ 2 },
				[&callbackEntered] {
					return callbackEntered;
				});
		}
		if (!entered) {
			{
				const auto lock = std::scoped_lock{ gateMutex };
				releaseCallback = true;
			}
			gate.notify_all();
			destroyer.join();
			expect(false, "teardown reaches first cancellation callback");
		}

		lane.run(1U);
		{
			const auto lock = std::scoped_lock{ gateMutex };
			releaseCallback = true;
		}
		gate.notify_all();
		destroyer.join();

		expect_store_cancelled(first, firstContext);
		expect_store_cancelled(second, secondContext);
		expect(
			backend->Calls.empty(),
			"teardown atomically claims all queued work");
		lane.drain();
		expect(
			first.Calls == 1U && second.Calls == 1U
				&& backend->Calls.empty(),
			"late tasks cannot publish or reach backend");
	}

	void test_fake_backend_wipes()
	{
		application::manual_executor lane;
		auto                         observation = std::make_shared<wipe_observation>();
		auto                         backend     = std::make_shared<wiping_fake_backend>(observation);
		auto                         vault       = make_test_vault(lane, backend);
		const auto                   first       = generated_bytes(23U, 37U);
		const auto                   second      = generated_bytes(29U, 43U);

		store_recorder firstStore;
		submit_store(
			*vault,
			context(authority(), 600U),
			first,
			firstStore);
		lane.run_next();
		store_recorder secondStore;
		submit_store(
			*vault,
			context(authority(), 601U),
			second,
			secondStore);
		lane.run_next();
		expect(
			observation->Buffers == 1U
				&& observation->Bytes == first.size(),
			"replacement overwrites prior fake buffer");

		delete_recorder deleted;
		submit_delete(*vault, context(authority(), 602U), deleted);
		lane.run_next();
		expect(
			observation->Buffers == 2U
				&& observation->Bytes == first.size() + second.size(),
			"delete overwrites current fake buffer");

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
			"backend teardown overwrites remaining fake material");
	}
	/// [[BPR-0542]] The refresh slot is its own store, and the credential slot
	/// cannot see it. This is the cut that matters: the old adapter answered
	/// `unavailable` from a stub, and the tempting cheap fix -- one schema, two
	/// attribute values -- would let a mistyped attribute hand a refresh token
	/// to a caller asking for an access token. Two backends behind two schemas
	/// make the confusion unrepresentable, and this proves the wiring.
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
		submit_store_refresh(*vault, operation, refresh, stored);
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
			"the refresh store holds the exact bytes");
		expect(
			!backend->contains(operation.Authority),
			"the credential slot holds nothing under that authority");

		retrieve_recorder credential;
		submit_retrieve(*vault, context(authority(), 701U), credential);
		lane.run_next();
		expect(
			terminal_as<application::github_credential_retrieve_absent>(
				credential.Terminal)
				!= nullptr,
			"a credential query is not answered by the refresh store");

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
			readBack != nullptr
				&& same_bytes(readBack->Credential.bytes(), refresh),
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
			deleted != nullptr
				&& deleted->Disposition
					== application::github_credential_delete_disposition::deleted,
			"and reports the entry as removed");
		expect(
			refreshBackend->Calls.size() == 3U,
			"the refresh store performed all three refresh operations");
		expect(
			backend->Calls.size() == 1U
				&& backend->Calls.front().Operation == backend_operation::retrieve,
			"the credential store performed only the one credential query");
	}

	/// A composition that built no second store still says `unavailable` --
	/// and that is now the *only* thing `unavailable` means on this slot.
	void test_a_vault_without_a_refresh_service_reports_unavailable()
	{
		application::manual_executor lane;
		auto                         observation = std::make_shared<wipe_observation>();
		auto                         backend     = std::make_shared<wiping_fake_backend>(observation);
		auto                         vault       = make_test_vault(lane, backend);

		store_recorder stored;
		submit_store_refresh(
			*vault,
			context(authority(), 710U),
			generated_bytes(29U),
			stored);
		lane.run_next();
		const auto* storeFailure = terminal_as<
			application::github_credential_store_error>(stored.Terminal);
		expect(storeFailure != nullptr, "the refresh store fails");
		expect(
			storeFailure != nullptr
				&& storeFailure->Failure == vault_failure::unavailable,
			"with unavailable, because there is no second store to reach");

		retrieve_recorder read;
		vault->retrieve_refresh(
			context(authority(), 711U),
			[&read](
				application::github_credential_retrieve_completion aTerminal) {
				read.record(std::move(aTerminal));
			});
		lane.run_next();
		const auto* readFailure = terminal_as<
			application::github_credential_retrieve_error>(read.Terminal);
		expect(
			readFailure != nullptr
				&& readFailure->Failure == vault_failure::unavailable,
			"the refresh read is unavailable too");

		delete_recorder removed;
		vault->delete_refresh(
			context(authority(), 712U),
			[&removed](
				application::github_credential_delete_completion aTerminal) {
				removed.record(std::move(aTerminal));
			});
		lane.run_next();
		const auto* deleteFailure = terminal_as<
			application::github_credential_delete_error>(removed.Terminal);
		expect(
			deleteFailure != nullptr
				&& deleteFailure->Failure == vault_failure::unavailable,
			"and so is the refresh delete");
		expect(
			backend->Calls.empty(),
			"none of the three reached the credential store");
	}
}

auto main() -> int
{
	test_schema_and_authority_encoding();
	test_status_classification();
	test_invalid_duplicate_and_exact_authority();
	test_replacement_absence_delete_and_reordering();
	test_failures_and_malformed_results_preserve_material();
	test_executor_and_backend_exceptions();
	test_cancellation_reentrancy_and_teardown();
	test_teardown_atomically_claims_queued_work();
	test_fake_backend_wipes();
	test_the_refresh_slot_reaches_only_its_own_service();
	test_a_vault_without_a_refresh_service_reports_unavailable();
	return EXIT_SUCCESS;
}
