#include "windows_credential_manager.hpp"
#include "windows_credential_manager_test_support.hpp"

#include "bpr/application/manual_executor.hpp"

#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

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

namespace
{
using namespace bpr;
using namespace bpr::adapters::github;
using namespace bpr::tests::windows_credential_manager;

using authority_key =
    application::github_credential_authority_key;
using operation_context =
    application::github_credential_operation_context;
using credential_material =
    application::github_credential_material;
using vault_failure =
    application::github_credential_vault_failure;

static_assert(std::is_abstract_v<windows_credential_manager_backend>);
static_assert(
    !std::is_copy_constructible_v<windows_credential_manager_backend>);
static_assert(
    !std::is_move_constructible_v<windows_credential_manager_backend>);
static_assert(
    !std::copy_constructible<
        windows_credential_manager_backend_retrieve_result>);
static_assert(
    std::move_constructible<
        windows_credential_manager_backend_retrieve_result>);
using native_factory_signature = auto (*)(
    application::effect_executor&)
    -> windows_credential_manager_vault_factory_result;
using injected_factory_signature = auto (*)(
    application::effect_executor&,
    std::shared_ptr<windows_credential_manager_backend>) noexcept
    -> windows_credential_manager_vault_factory_result;
static_assert(std::is_same_v<
    decltype(&make_windows_credential_manager_vault),
    native_factory_signature>);
static_assert(std::is_same_v<
    decltype(&make_windows_credential_manager_integration_test_vault),
    native_factory_signature>);
static_assert(std::is_same_v<
    decltype(&make_windows_credential_manager_vault_with_backend),
    injected_factory_signature>);

[[nodiscard]] auto hex_encoded(const std::string_view input) -> std::wstring
{
    constexpr std::wstring_view digits = L"0123456789abcdef";
    std::wstring result;
    result.reserve(input.size() * 2U);
    for(const auto character : input)
    {
        const auto value =
            static_cast<unsigned char>(character);
        result.push_back(digits[value >> 4U]);
        result.push_back(digits[value & 0x0fU]);
    }
    return result;
}

struct wipe_observation
{
    std::size_t Buffers = 0U;
    std::size_t Bytes = 0U;
};

class wiping_buffer final
{
public:
    wiping_buffer(
        const std::span<const std::byte> bytes,
        std::shared_ptr<wipe_observation> observation)
        : Bytes{std::make_unique<std::byte[]>(bytes.size())}
        , Size{bytes.size()}
        , Observation{std::move(observation)}
    {
        std::ranges::copy(bytes, Bytes.get());
    }

    ~wiping_buffer() noexcept
    {
        reset();
    }

    wiping_buffer(const wiping_buffer&) = delete;
    auto operator=(const wiping_buffer&) -> wiping_buffer& = delete;

    wiping_buffer(wiping_buffer&& other) noexcept
        : Bytes{std::move(other.Bytes)}
        , Size{std::exchange(other.Size, 0U)}
        , Observation{std::move(other.Observation)}
    {
    }

    auto operator=(wiping_buffer&& other) noexcept -> wiping_buffer&
    {
        if(this == &other)
        {
            return *this;
        }
        reset();
        Bytes = std::move(other.Bytes);
        Size = std::exchange(other.Size, 0U);
        Observation = std::move(other.Observation);
        return *this;
    }

    [[nodiscard]] auto bytes() const noexcept
        -> std::span<const std::byte>
    {
        return {Bytes.get(), Size};
    }

private:
    void reset() noexcept
    {
        if(Bytes == nullptr)
        {
            Size = 0U;
            return;
        }

        const auto wiped = Size;
        auto* cursor =
            reinterpret_cast<volatile unsigned char*>(Bytes.get());
        while(Size != 0U)
        {
            *cursor = 0U;
            ++cursor;
            --Size;
        }
        Bytes.reset();
        if(Observation != nullptr)
        {
            ++Observation->Buffers;
            Observation->Bytes += wiped;
        }
    }

    std::unique_ptr<std::byte[]> Bytes;
    std::size_t Size = 0U;
    std::shared_ptr<wipe_observation> Observation;
};

enum class backend_operation : std::uint8_t
{
    store,
    retrieve,
    delete_credential,
};

enum class injected_exception : std::uint8_t
{
    none,
    allocation,
    other,
};

struct backend_call
{
    backend_operation Operation = backend_operation::retrieve;
    authority_key Authority;
};

class wiping_fake_backend final : public windows_credential_manager_backend
{
public:
    explicit wiping_fake_backend(
        std::shared_ptr<wipe_observation> observation)
        : Observation{std::move(observation)}
    {
    }

    [[nodiscard]] auto store(
        const authority_key& authority_value,
        const credential_material& credential)
        -> windows_credential_manager_backend_store_result override
    {
        Calls.push_back({
            .Operation = backend_operation::store,
            .Authority = authority_value,
        });
        run_hook(StoreHook);
        throw_if_requested(NextStoreException);

        if(NextStoreFailure.has_value())
        {
            const auto failure =
                std::exchange(NextStoreFailure, std::nullopt);
            return std::unexpected{*failure};
        }
        if(MalformedStoreResult)
        {
            MalformedStoreResult = false;
            return application::github_credential_store_disposition::
                uninitialized;
        }

        Stored.insert_or_assign(
            authority_value,
            wiping_buffer{credential.bytes(), Observation});
        return application::github_credential_store_disposition::stored;
    }

    [[nodiscard]] auto retrieve(
        const authority_key& authority_value)
        -> windows_credential_manager_backend_retrieve_result override
    {
        Calls.push_back({
            .Operation = backend_operation::retrieve,
            .Authority = authority_value,
        });
        run_hook(RetrieveHook);
        throw_if_requested(NextRetrieveException);

        if(NextRetrieveFailure.has_value())
        {
            const auto failure =
                std::exchange(NextRetrieveFailure, std::nullopt);
            return *failure;
        }
        if(MalformedRetrieveResult)
        {
            MalformedRetrieveResult = false;
            return vault_failure::none;
        }

        const auto found = Stored.find(authority_value);
        if(found == Stored.end())
        {
            return windows_credential_manager_credential_absent{};
        }
        auto material =
            credential_material::create(found->second.bytes());
        if(!material.has_value())
        {
            return vault_failure::allocation_failure;
        }
        return std::move(*material);
    }

    [[nodiscard]] auto delete_credential(
        const authority_key& authority_value)
        -> windows_credential_manager_backend_delete_result override
    {
        Calls.push_back({
            .Operation = backend_operation::delete_credential,
            .Authority = authority_value,
        });
        run_hook(DeleteHook);
        throw_if_requested(NextDeleteException);

        if(NextDeleteFailure.has_value())
        {
            const auto failure =
                std::exchange(NextDeleteFailure, std::nullopt);
            return std::unexpected{*failure};
        }
        if(MalformedDeleteResult)
        {
            MalformedDeleteResult = false;
            return application::github_credential_delete_disposition::
                uninitialized;
        }

        const auto removed = Stored.erase(authority_value);
        return removed == 0U
            ? application::github_credential_delete_disposition::absent
            : application::github_credential_delete_disposition::deleted;
    }

    [[nodiscard]] auto contains(
        const authority_key& authority_value) const -> bool
    {
        return Stored.contains(authority_value);
    }

    [[nodiscard]] auto matches(
        const authority_key& authority_value,
        const std::span<const std::byte> expected) const -> bool
    {
        const auto found = Stored.find(authority_value);
        return found != Stored.end()
            && same_bytes(found->second.bytes(), expected);
    }

    std::vector<backend_call> Calls;
    std::optional<vault_failure> NextStoreFailure;
    std::optional<vault_failure> NextRetrieveFailure;
    std::optional<vault_failure> NextDeleteFailure;
    injected_exception NextStoreException = injected_exception::none;
    injected_exception NextRetrieveException = injected_exception::none;
    injected_exception NextDeleteException = injected_exception::none;
    bool MalformedStoreResult = false;
    bool MalformedRetrieveResult = false;
    bool MalformedDeleteResult = false;
    std::function<void()> StoreHook;
    std::function<void()> RetrieveHook;
    std::function<void()> DeleteHook;

private:
    static void run_hook(std::function<void()>& hook)
    {
        auto one_shot = std::exchange(hook, {});
        if(one_shot)
        {
            one_shot();
        }
    }

    static void throw_if_requested(injected_exception& requested)
    {
        const auto exception =
            std::exchange(requested, injected_exception::none);
        switch(exception)
        {
        case injected_exception::none:
            return;
        case injected_exception::allocation:
            throw std::bad_alloc{};
        case injected_exception::other:
            throw std::runtime_error{"categorical fake backend failure"};
        }
    }

    std::map<authority_key, wiping_buffer> Stored;
    std::shared_ptr<wipe_observation> Observation;
};

class throwing_effect_executor final : public application::effect_executor
{
public:
    explicit throwing_effect_executor(
        const injected_exception exception) noexcept
        : Exception{exception}
    {
    }

    void post(std::function<void()> task) override
    {
        switch(Exception)
        {
        case injected_exception::allocation:
            throw std::bad_alloc{};
        case injected_exception::other:
            throw std::runtime_error{"categorical executor failure"};
        case injected_exception::none:
            task();
            return;
        }
    }

private:
    injected_exception Exception;
};

template<typename Completion>
struct terminal_recorder
{
    std::size_t Calls = 0U;
    std::optional<Completion> Terminal;

    void record(Completion completion)
    {
        ++Calls;
        Terminal.emplace(std::move(completion));
    }
};

using store_recorder =
    terminal_recorder<application::github_credential_store_completion>;
using retrieve_recorder =
    terminal_recorder<application::github_credential_retrieve_completion>;
using delete_recorder =
    terminal_recorder<application::github_credential_delete_completion>;

[[nodiscard]] auto generated_bytes(
    const std::uint8_t seed,
    const std::size_t size = 41U) -> std::vector<std::byte>
{
    auto bytes = std::vector<std::byte>(size);
    auto value = seed;
    for(auto& byte : bytes)
    {
        value = static_cast<std::uint8_t>(
            static_cast<std::uint16_t>(value) * 37U + 19U);
        byte = static_cast<std::byte>(value);
    }
    return bytes;
}

[[nodiscard]] auto make_test_vault(
    application::manual_executor& lane,
    const std::shared_ptr<wiping_fake_backend>& backend)
    -> std::unique_ptr<application::github_credential_vault_port>
{
    auto result =
        make_windows_credential_manager_vault_with_backend(lane, backend);
    expect(result.has_value(), "injected Credential Manager vault is created");
    return std::move(*result);
}

void submit_store(
    application::github_credential_vault_port& vault,
    const operation_context& operation,
    const std::span<const std::byte> bytes,
    store_recorder& recorder)
{
    vault.store(
        application::github_credential_enrollment{
            .Context = operation,
            .Credential = material_from(bytes),
        },
        [&recorder](
            application::github_credential_store_completion terminal) {
            recorder.record(std::move(terminal));
        });
}

void submit_retrieve(
    application::github_credential_vault_port& vault,
    const operation_context& operation,
    retrieve_recorder& recorder)
{
    vault.retrieve(
        operation,
        [&recorder](
            application::github_credential_retrieve_completion terminal) {
            recorder.record(std::move(terminal));
        });
}

void submit_delete(
    application::github_credential_vault_port& vault,
    const operation_context& operation,
    delete_recorder& recorder)
{
    vault.delete_credential(
        operation,
        [&recorder](
            application::github_credential_delete_completion terminal) {
            recorder.record(std::move(terminal));
        });
}

template<typename Terminal, typename Completion>
[[nodiscard]] auto terminal_as(const std::optional<Completion>& completion)
    -> const Terminal*
{
    return completion.has_value()
        ? std::get_if<Terminal>(&*completion)
        : nullptr;
}

void expect_store_success(
    const store_recorder& recorder,
    const operation_context& operation)
{
    const auto* success = terminal_as<
        application::github_credential_store_success>(recorder.Terminal);
    expect(
        recorder.Calls == 1U && success != nullptr
            && success->Context == operation
            && success->Disposition
                == application::github_credential_store_disposition::stored,
        "store completes once with exact successful context");
}

void expect_store_error(
    const store_recorder& recorder,
    const operation_context& operation,
    const vault_failure failure)
{
    const auto* error = terminal_as<
        application::github_credential_store_error>(recorder.Terminal);
    expect(
        recorder.Calls == 1U && error != nullptr
            && error->Context == operation && error->Failure == failure,
        "store completes once with exact typed failure");
}

void expect_store_cancelled(
    const store_recorder& recorder,
    const operation_context& operation)
{
    const auto* cancelled = terminal_as<
        application::github_credential_store_cancelled>(
        recorder.Terminal);
    expect(
        recorder.Calls == 1U && cancelled != nullptr
            && cancelled->Context == operation,
        "store completes once with exact cancellation");
}

void expect_retrieve_success(
    const retrieve_recorder& recorder,
    const operation_context& operation,
    const std::span<const std::byte> bytes)
{
    const auto* success = terminal_as<
        application::github_credential_retrieve_success>(
        recorder.Terminal);
    expect(
        recorder.Calls == 1U && success != nullptr
            && success->Context == operation
            && same_bytes(success->Credential.bytes(), bytes),
        "retrieve completes once with exact credential and context");
}

void expect_retrieve_absent(
    const retrieve_recorder& recorder,
    const operation_context& operation)
{
    const auto* absent = terminal_as<
        application::github_credential_retrieve_absent>(
        recorder.Terminal);
    expect(
        recorder.Calls == 1U && absent != nullptr
            && absent->Context == operation,
        "retrieve completes once with exact absence");
}

void expect_retrieve_error(
    const retrieve_recorder& recorder,
    const operation_context& operation,
    const vault_failure failure)
{
    const auto* error = terminal_as<
        application::github_credential_retrieve_error>(
        recorder.Terminal);
    expect(
        recorder.Calls == 1U && error != nullptr
            && error->Context == operation && error->Failure == failure,
        "retrieve completes once with exact typed failure");
}

void expect_retrieve_cancelled(
    const retrieve_recorder& recorder,
    const operation_context& operation)
{
    const auto* cancelled = terminal_as<
        application::github_credential_retrieve_cancelled>(
        recorder.Terminal);
    expect(
        recorder.Calls == 1U && cancelled != nullptr
            && cancelled->Context == operation,
        "retrieve completes once with exact cancellation");
}

void expect_delete_success(
    const delete_recorder& recorder,
    const operation_context& operation,
    const application::github_credential_delete_disposition disposition)
{
    const auto* success = terminal_as<
        application::github_credential_delete_success>(
        recorder.Terminal);
    expect(
        recorder.Calls == 1U && success != nullptr
            && success->Context == operation
            && success->Disposition == disposition,
        "delete completes once with exact disposition and context");
}

void expect_delete_error(
    const delete_recorder& recorder,
    const operation_context& operation,
    const vault_failure failure)
{
    const auto* error = terminal_as<
        application::github_credential_delete_error>(
        recorder.Terminal);
    expect(
        recorder.Calls == 1U && error != nullptr
            && error->Context == operation && error->Failure == failure,
        "delete completes once with exact typed failure");
}

void expect_delete_cancelled(
    const delete_recorder& recorder,
    const operation_context& operation)
{
    const auto* cancelled = terminal_as<
        application::github_credential_delete_cancelled>(
        recorder.Terminal);
    expect(
        recorder.Calls == 1U && cancelled != nullptr
            && cancelled->Context == operation,
        "delete completes once with exact cancellation");
}

template<typename Status>
void expect_status(
    const Status status,
    const windows_credential_manager_status_kind kind,
    const vault_failure failure)
{
    const auto mapped =
        classify_windows_credential_manager_status(
            static_cast<std::uint32_t>(status));
    expect(
        mapped.Kind == kind && mapped.Failure == failure,
        "Win32 status has the exact bounded classification");
}

void test_target_and_authority_encoding()
{
    constexpr std::wstring_view production_prefix =
        L"org.betterpullreview.github-native-credential."
        L"windows-credential-manager.v2/";
    constexpr std::wstring_view integration_prefix =
        L"org.betterpullreview.github-native-credential."
        L"windows-credential-manager.integration-test.v2/";
    expect(
        windows_credential_manager_target_prefix(
            windows_credential_manager_target_scope::production)
            == production_prefix,
        "production Credential Manager target prefix is exact and stable");
    expect(
        windows_credential_manager_target_prefix(
            windows_credential_manager_target_scope::integration_test)
            == integration_prefix,
        "integration target prefix is isolated from production");
    expect(
        windows_credential_manager_target_prefix(
            static_cast<windows_credential_manager_target_scope>(255U))
            .empty(),
        "malformed target scope has no prefix");
    expect(
        production_prefix.size()
                + windows_credential_manager_authority_account_chars
            == 650U,
        "production target is exactly 650 UTF-16 code units");
    expect(
        integration_prefix.size()
                + windows_credential_manager_authority_account_chars
            == 667U,
        "integration target is exactly 667 UTF-16 code units");

    const auto base = authority();
    const auto encoded =
        encode_windows_credential_manager_authority_account(base);
    expect(encoded.has_value(), "valid authority encodes");
    expect(
        std::ranges::all_of(
            *encoded,
            [](const wchar_t character) {
                return (character >= L'0' && character <= L'9')
                    || (character >= L'a' && character <= L'f');
            }),
        "account key is fixed-width lowercase hexadecimal");
    const auto encoded_view =
        std::wstring_view{encoded->data(), encoded->size()};
    expect(
        encoded_view.substr(0U, 66U)
                == L"000000000000000100000000000000020000000000000003"
                   L"000000000000000401"
            && encoded_view.substr(66U, 2U) == L"0a"
            && encoded_view.substr(68U, 20U)
                   == L"6769746875622e636f6d"
            && std::ranges::all_of(
                encoded_view.substr(88U),
                [](const wchar_t character) { return character == L'0'; }),
        "GitHub.com authority has the exact stable v2 encoding");
    const auto production_target =
        std::wstring{production_prefix}
        + std::wstring{encoded->data(), encoded->size()};
    const auto integration_target =
        std::wstring{integration_prefix}
        + std::wstring{encoded->data(), encoded->size()};
    expect(
        production_target.starts_with(production_prefix)
            && production_target.substr(production_prefix.size())
                   == encoded_view
            && production_target.size() == 650U,
        "production target name is exact and complete");
    expect(
        integration_target.starts_with(integration_prefix)
            && integration_target.substr(integration_prefix.size())
                   == encoded_view
            && integration_target.size() == 667U,
        "integration target name is exact and complete");
    const auto enterprise_encoding =
        encode_windows_credential_manager_authority_account(
            authority(
                1U,
                2U,
                3U,
                4U,
                application::github_server_contract::
                    ghes_3_18_2022_11_28));
    const auto enterprise_view =
        enterprise_encoding.has_value()
        ? std::wstring_view{
              enterprise_encoding->data(), enterprise_encoding->size()}
        : std::wstring_view{};
    expect(
        enterprise_view.substr(0U, 66U)
                == L"000000000000000100000000000000020000000000000003"
                   L"000000000000000402"
            && enterprise_view.substr(66U, 2U) == L"13"
            && enterprise_view.substr(68U, 38U)
                   == L"6768652e6578616d706c652e696e76616c6964"
            && std::ranges::all_of(
                enterprise_view.substr(106U),
                [](const wchar_t character) { return character == L'0'; }),
        "GHES 3.18 authority has the exact stable v2 encoding");
    const auto other_enterprise_encoding =
        encode_windows_credential_manager_authority_account(authority(
            1U,
            2U,
            3U,
            4U,
            application::github_server_contract::
                ghes_3_18_2022_11_28,
            "ghe.other.invalid"));
    expect(
        other_enterprise_encoding.has_value()
            && *other_enterprise_encoding != *enterprise_encoding,
        "canonical hostname is an injective target-key dimension");
    const auto maximum_hostname =
        std::string(63U, 'a') + "." + std::string(63U, 'b') + "."
        + std::string(63U, 'c') + "." + std::string(61U, 'd');
    const auto maximum_encoding =
        encode_windows_credential_manager_authority_account(authority(
            1U,
            2U,
            3U,
            4U,
            application::github_server_contract::
                ghes_3_18_2022_11_28,
            maximum_hostname));
    const auto maximum_view =
        maximum_encoding.has_value()
        ? std::wstring_view{
              maximum_encoding->data(), maximum_encoding->size()}
        : std::wstring_view{};
    expect(
        maximum_hostname.size()
                == application::maximum_github_hostname_bytes
            && maximum_view.size()
                   == windows_credential_manager_authority_account_chars
            && maximum_view.substr(66U, 2U) == L"fd"
            && maximum_view.substr(68U) == hex_encoded(maximum_hostname),
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
    for(const auto& variant : variants)
    {
        const auto different =
            encode_windows_credential_manager_authority_account(variant);
        expect(
            different.has_value() && *different != *encoded,
            "every authority dimension is account-key injective");
    }
    const auto adversarial_a =
        encode_windows_credential_manager_authority_account(
            authority(1U, 23U, 456U, 7U));
    const auto adversarial_b =
        encode_windows_credential_manager_authority_account(
            authority(12U, 3U, 45U, 67U));
    expect(
        adversarial_a.has_value() && adversarial_b.has_value()
            && *adversarial_a != *adversarial_b,
        "decimal concatenation cannot collide");

    auto malformed = base;
    malformed.Profile = {};
    expect(
        encode_windows_credential_manager_authority_account(malformed)
            == std::unexpected{vault_failure::invalid_request},
        "zero profile is rejected before Credential Manager");
    malformed = base;
    malformed.ProfileRevision = {};
    expect(
        encode_windows_credential_manager_authority_account(malformed)
            == std::unexpected{vault_failure::invalid_request},
        "zero profile revision is rejected before Credential Manager");
    malformed = base;
    malformed.Host = {};
    expect(
        encode_windows_credential_manager_authority_account(malformed)
            == std::unexpected{vault_failure::invalid_request},
        "zero host is rejected before Credential Manager");
    malformed = base;
    malformed.Hostname = {};
    expect(
        encode_windows_credential_manager_authority_account(malformed)
            == std::unexpected{vault_failure::invalid_request},
        "empty hostname is rejected before Credential Manager");
    malformed.Account = {};
    expect(
        encode_windows_credential_manager_authority_account(malformed)
            == std::unexpected{vault_failure::invalid_request},
        "zero account is rejected before Credential Manager");
    malformed = base;
    malformed.ServerContract =
        static_cast<application::github_server_contract>(255U);
    expect(
        encode_windows_credential_manager_authority_account(malformed)
            == std::unexpected{vault_failure::invalid_request},
        "malformed server contract is rejected before Credential Manager");
}

void test_complete_status_classification()
{
    expect_status(
        ERROR_SUCCESS,
        windows_credential_manager_status_kind::success,
        vault_failure::none);
    expect_status(
        ERROR_NOT_FOUND,
        windows_credential_manager_status_kind::item_not_found,
        vault_failure::none);

    for(const auto status :
        {ERROR_ACCESS_DENIED, ERROR_PRIVILEGE_NOT_HELD})
    {
        expect_status(
            status,
            windows_credential_manager_status_kind::failure,
            vault_failure::access_denied);
    }
    for(const auto status :
        {ERROR_INVALID_DATA, ERROR_BAD_FORMAT, ERROR_FILE_CORRUPT})
    {
        expect_status(
            status,
            windows_credential_manager_status_kind::failure,
            vault_failure::corrupt);
    }
    for(const auto status :
        {ERROR_NOT_ENOUGH_MEMORY, ERROR_OUTOFMEMORY})
    {
        expect_status(
            status,
            windows_credential_manager_status_kind::failure,
            vault_failure::allocation_failure);
    }
    for(const auto status :
        {
            ERROR_INVALID_PARAMETER,
            ERROR_INVALID_FLAGS,
            ERROR_BAD_USERNAME,
            ERROR_BAD_LENGTH,
        })
    {
        expect_status(
            status,
            windows_credential_manager_status_kind::failure,
            vault_failure::invalid_request);
    }
    expect_status(
        ERROR_NO_SUCH_LOGON_SESSION,
        windows_credential_manager_status_kind::failure,
        vault_failure::unavailable);
    expect_status(
        ERROR_BUSY,
        windows_credential_manager_status_kind::failure,
        vault_failure::operation_failed);
    expect_status(
        ERROR_LOCK_VIOLATION,
        windows_credential_manager_status_kind::failure,
        vault_failure::operation_failed);

    for(const auto status : {
            ERROR_SUCCESS,
            ERROR_NOT_FOUND,
            ERROR_NO_SUCH_LOGON_SESSION,
            ERROR_ACCESS_DENIED,
            ERROR_PRIVILEGE_NOT_HELD,
            ERROR_INVALID_DATA,
            ERROR_BAD_FORMAT,
            ERROR_FILE_CORRUPT,
            ERROR_NOT_ENOUGH_MEMORY,
            ERROR_OUTOFMEMORY,
            ERROR_INVALID_PARAMETER,
            ERROR_INVALID_FLAGS,
            ERROR_BAD_USERNAME,
            ERROR_BAD_LENGTH,
            ERROR_BUSY,
            ERROR_LOCK_VIOLATION,
        })
    {
        expect(
            classify_windows_credential_manager_status(
                static_cast<std::uint32_t>(status))
                    .Failure
                != vault_failure::locked,
            "Win32 Credential Manager has no invented locked status");
    }
}

void test_invalid_and_duplicate_requests()
{
    application::manual_executor lane;
    auto observation = std::make_shared<wipe_observation>();
    auto backend =
        std::make_shared<wiping_fake_backend>(observation);
    auto vault = make_test_vault(lane, backend);
    const auto bytes = generated_bytes(3U);

    auto malformed = context();
    malformed.Request = {};
    store_recorder invalid_store;
    submit_store(*vault, malformed, bytes, invalid_store);
    expect_store_error(
        invalid_store,
        malformed,
        vault_failure::invalid_request);

    malformed = context();
    malformed.ApplicationSession = {};
    retrieve_recorder invalid_retrieve;
    submit_retrieve(*vault, malformed, invalid_retrieve);
    expect_retrieve_error(
        invalid_retrieve,
        malformed,
        vault_failure::invalid_request);

    malformed = context();
    malformed.IdentityGeneration = {};
    delete_recorder invalid_delete;
    submit_delete(*vault, malformed, invalid_delete);
    expect_delete_error(
        invalid_delete,
        malformed,
        vault_failure::invalid_request);

    malformed = context();
    malformed.Authority.Account = {};
    retrieve_recorder invalid_authority;
    submit_retrieve(*vault, malformed, invalid_authority);
    expect_retrieve_error(
        invalid_authority,
        malformed,
        vault_failure::invalid_request);
    expect(
        lane.pending() == 0U && backend->Calls.empty(),
        "invalid work never reaches the lane or backend");

    auto null_factory =
        make_windows_credential_manager_vault_with_backend(lane, {});
    expect(
        !null_factory.has_value()
            && null_factory.error() == vault_failure::invalid_request,
        "null backend fails closed");

    const auto duplicate_store_context =
        context(authority(), 100U);
    store_recorder first_store;
    store_recorder duplicate_store;
    submit_store(
        *vault,
        duplicate_store_context,
        bytes,
        first_store);
    submit_store(
        *vault,
        duplicate_store_context,
        bytes,
        duplicate_store);
    expect_store_error(
        duplicate_store,
        duplicate_store_context,
        vault_failure::invalid_request);
    expect(lane.pending() == 1U, "duplicate store is not queued");
    lane.run_next();
    expect_store_success(first_store, duplicate_store_context);

    const auto duplicate_retrieve_context =
        context(authority(), 101U);
    retrieve_recorder first_retrieve;
    retrieve_recorder duplicate_retrieve;
    submit_retrieve(
        *vault,
        duplicate_retrieve_context,
        first_retrieve);
    submit_retrieve(
        *vault,
        duplicate_retrieve_context,
        duplicate_retrieve);
    expect_retrieve_error(
        duplicate_retrieve,
        duplicate_retrieve_context,
        vault_failure::invalid_request);
    lane.run_next();
    expect_retrieve_success(
        first_retrieve,
        duplicate_retrieve_context,
        bytes);

    const auto duplicate_delete_context =
        context(authority(), 102U);
    delete_recorder first_delete;
    delete_recorder duplicate_delete;
    submit_delete(
        *vault,
        duplicate_delete_context,
        first_delete);
    submit_delete(
        *vault,
        duplicate_delete_context,
        duplicate_delete);
    expect_delete_error(
        duplicate_delete,
        duplicate_delete_context,
        vault_failure::invalid_request);
    lane.run_next();
    expect_delete_success(
        first_delete,
        duplicate_delete_context,
        application::github_credential_delete_disposition::deleted);

    const auto cross_kind_context =
        context(authority(), 103U);
    store_recorder cross_kind_store;
    retrieve_recorder cross_kind_retrieve;
    delete_recorder cross_kind_delete;
    submit_store(
        *vault,
        cross_kind_context,
        bytes,
        cross_kind_store);
    submit_retrieve(
        *vault,
        cross_kind_context,
        cross_kind_retrieve);
    submit_delete(
        *vault,
        cross_kind_context,
        cross_kind_delete);
    expect_retrieve_error(
        cross_kind_retrieve,
        cross_kind_context,
        vault_failure::invalid_request);
    expect_delete_error(
        cross_kind_delete,
        cross_kind_context,
        vault_failure::invalid_request);
    expect(
        lane.pending() == 1U,
        "duplicate correlation is rejected across operation kinds");
    lane.run_next();
    expect_store_success(cross_kind_store, cross_kind_context);
}

void test_replacement_absence_delete_and_reordering()
{
    application::manual_executor lane;
    auto observation = std::make_shared<wipe_observation>();
    auto backend =
        std::make_shared<wiping_fake_backend>(observation);
    auto vault = make_test_vault(lane, backend);
    const auto first = generated_bytes(5U);
    const auto second = generated_bytes(7U);
    const auto key = authority();

    const auto first_context = context(key, 200U);
    const auto second_context = context(key, 201U);
    store_recorder first_store;
    store_recorder second_store;
    submit_store(*vault, first_context, first, first_store);
    submit_store(*vault, second_context, second, second_store);
    expect(lane.pending() == 2U, "independent contexts can queue");
    lane.run(1U);
    expect_store_success(second_store, second_context);
    lane.run(0U);
    expect_store_success(first_store, first_context);
    expect(
        backend->matches(key, first),
        "executor reordering determines the atomic replacement order");
    expect(
        observation->Buffers == 1U
            && observation->Bytes == second.size(),
        "replaced fake material is overwritten");

    const auto retrieve_context = context(key, 202U);
    retrieve_recorder retrieve;
    submit_retrieve(*vault, retrieve_context, retrieve);
    lane.run_next();
    expect_retrieve_success(retrieve, retrieve_context, first);

    const std::array isolated_authorities{
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
    auto isolated_request = std::uint64_t{210U};
    for(const auto& isolated : isolated_authorities)
    {
        const auto isolated_context =
            context(isolated, isolated_request++);
        retrieve_recorder isolated_retrieve;
        submit_retrieve(
            *vault,
            isolated_context,
            isolated_retrieve);
        lane.run_next();
        expect_retrieve_absent(
            isolated_retrieve,
            isolated_context);
    }

    const auto delete_context = context(key, 203U);
    delete_recorder deleted;
    submit_delete(*vault, delete_context, deleted);
    lane.run_next();
    expect_delete_success(
        deleted,
        delete_context,
        application::github_credential_delete_disposition::deleted);
    expect(!backend->contains(key), "delete removes exact authority");

    const auto absent_context = context(key, 204U);
    retrieve_recorder absent;
    submit_retrieve(*vault, absent_context, absent);
    lane.run_next();
    expect_retrieve_absent(absent, absent_context);

    const auto delete_absent_context = context(key, 205U);
    delete_recorder delete_absent;
    submit_delete(*vault, delete_absent_context, delete_absent);
    lane.run_next();
    expect_delete_success(
        delete_absent,
        delete_absent_context,
        application::github_credential_delete_disposition::absent);
}

void test_failures_preserve_existing_material()
{
    application::manual_executor lane;
    auto observation = std::make_shared<wipe_observation>();
    auto backend =
        std::make_shared<wiping_fake_backend>(observation);
    auto vault = make_test_vault(lane, backend);
    const auto original = generated_bytes(11U);
    const auto replacement = generated_bytes(13U);
    const auto key = authority();

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
    for(const auto failure : failures)
    {
        const auto store_context = context(key, request++);
        backend->NextStoreFailure = failure;
        store_recorder store;
        submit_store(*vault, store_context, replacement, store);
        lane.run_next();
        expect_store_error(store, store_context, failure);
        expect(
            backend->matches(key, original),
            "failed replacement preserves existing material");

        const auto retrieve_context = context(key, request++);
        backend->NextRetrieveFailure = failure;
        retrieve_recorder retrieve;
        submit_retrieve(*vault, retrieve_context, retrieve);
        lane.run_next();
        expect_retrieve_error(retrieve, retrieve_context, failure);

        const auto delete_context = context(key, request++);
        backend->NextDeleteFailure = failure;
        delete_recorder deleted;
        submit_delete(*vault, delete_context, deleted);
        lane.run_next();
        expect_delete_error(deleted, delete_context, failure);
        expect(
            backend->matches(key, original),
            "failed delete preserves existing material");
    }

    backend->MalformedStoreResult = true;
    const auto bad_store_context = context(key, request++);
    store_recorder bad_store;
    submit_store(*vault, bad_store_context, replacement, bad_store);
    lane.run_next();
    expect_store_error(
        bad_store,
        bad_store_context,
        vault_failure::operation_failed);

    backend->MalformedRetrieveResult = true;
    const auto bad_retrieve_context = context(key, request++);
    retrieve_recorder bad_retrieve;
    submit_retrieve(*vault, bad_retrieve_context, bad_retrieve);
    lane.run_next();
    expect_retrieve_error(
        bad_retrieve,
        bad_retrieve_context,
        vault_failure::operation_failed);

    backend->MalformedDeleteResult = true;
    const auto bad_delete_context = context(key, request);
    delete_recorder bad_delete;
    submit_delete(*vault, bad_delete_context, bad_delete);
    lane.run_next();
    expect_delete_error(
        bad_delete,
        bad_delete_context,
        vault_failure::operation_failed);
    expect(
        backend->matches(key, original),
        "malformed backend results fail closed without mutation");
}

void test_executor_and_backend_exceptions()
{
    const auto original = generated_bytes(31U);
    const auto replacement = generated_bytes(37U);
    const auto key = authority();

    application::manual_executor lane;
    auto observation = std::make_shared<wipe_observation>();
    auto backend =
        std::make_shared<wiping_fake_backend>(observation);
    auto vault = make_test_vault(lane, backend);
    store_recorder initial;
    submit_store(*vault, context(key, 350U), original, initial);
    lane.run_next();
    expect_store_success(initial, context(key, 350U));

    struct exception_expectation
    {
        injected_exception Exception = injected_exception::none;
        vault_failure Failure = vault_failure::operation_failed;
    };
    constexpr std::array exceptions{
        exception_expectation{
            .Exception = injected_exception::allocation,
            .Failure = vault_failure::allocation_failure,
        },
        exception_expectation{
            .Exception = injected_exception::other,
            .Failure = vault_failure::operation_failed,
        },
    };
    auto request = std::uint64_t{351U};
    for(const auto expectation : exceptions)
    {
        const auto store_context = context(key, request++);
        backend->NextStoreException = expectation.Exception;
        store_recorder store;
        submit_store(*vault, store_context, replacement, store);
        lane.run_next();
        expect_store_error(store, store_context, expectation.Failure);
        expect(
            backend->matches(key, original),
            "throwing replacement preserves prior material");

        const auto retrieve_context = context(key, request++);
        backend->NextRetrieveException = expectation.Exception;
        retrieve_recorder retrieve;
        submit_retrieve(*vault, retrieve_context, retrieve);
        lane.run_next();
        expect_retrieve_error(
            retrieve,
            retrieve_context,
            expectation.Failure);

        const auto delete_context = context(key, request++);
        backend->NextDeleteException = expectation.Exception;
        delete_recorder deleted;
        submit_delete(*vault, delete_context, deleted);
        lane.run_next();
        expect_delete_error(
            deleted,
            delete_context,
            expectation.Failure);
        expect(
            backend->matches(key, original),
            "throwing delete preserves prior material");
    }

    request = 370U;
    for(const auto expectation : exceptions)
    {
        throwing_effect_executor executor{expectation.Exception};
        auto post_observation = std::make_shared<wipe_observation>();
        auto post_backend =
            std::make_shared<wiping_fake_backend>(post_observation);
        auto result =
            make_windows_credential_manager_vault_with_backend(
                executor,
                post_backend);
        expect(result.has_value(), "throwing executor vault is created");
        auto post_vault = std::move(*result);

        const auto store_context = context(key, request++);
        store_recorder store;
        submit_store(
            *post_vault,
            store_context,
            replacement,
            store);
        expect_store_error(store, store_context, expectation.Failure);

        const auto retrieve_context = context(key, request++);
        retrieve_recorder retrieve;
        submit_retrieve(
            *post_vault,
            retrieve_context,
            retrieve);
        expect_retrieve_error(
            retrieve,
            retrieve_context,
            expectation.Failure);

        const auto delete_context = context(key, request++);
        delete_recorder deleted;
        submit_delete(
            *post_vault,
            delete_context,
            deleted);
        expect_delete_error(
            deleted,
            delete_context,
            expectation.Failure);
        expect(
            post_backend->Calls.empty(),
            "executor post failure never reaches the backend");
    }
}

void test_cancellation_races_and_sibling_isolation()
{
    application::manual_executor lane;
    auto observation = std::make_shared<wipe_observation>();
    auto backend =
        std::make_shared<wiping_fake_backend>(observation);
    auto vault = make_test_vault(lane, backend);
    const auto bytes = generated_bytes(17U);
    const auto key = authority();

    const auto queued_store_context = context(key, 400U);
    store_recorder queued_store;
    submit_store(*vault, queued_store_context, bytes, queued_store);
    vault->cancel(queued_store_context);
    expect_store_cancelled(queued_store, queued_store_context);
    expect(!backend->contains(key), "cancelled store has no side effect");
    lane.run_next();
    expect(
        queued_store.Calls == 1U && backend->Calls.empty(),
        "late queued store task is suppressed");

    const auto queued_retrieve_context = context(key, 401U);
    retrieve_recorder queued_retrieve;
    submit_retrieve(
        *vault,
        queued_retrieve_context,
        queued_retrieve);
    vault->cancel(queued_retrieve_context);
    expect_retrieve_cancelled(
        queued_retrieve,
        queued_retrieve_context);
    lane.run_next();
    expect(
        queued_retrieve.Calls == 1U && backend->Calls.empty(),
        "late queued retrieve task is suppressed");

    const auto queued_delete_context = context(key, 402U);
    delete_recorder queued_delete;
    submit_delete(*vault, queued_delete_context, queued_delete);
    vault->cancel(queued_delete_context);
    expect_delete_cancelled(queued_delete, queued_delete_context);
    lane.run_next();
    expect(
        queued_delete.Calls == 1U && backend->Calls.empty(),
        "late queued delete task is suppressed");

    const auto running_store_context = context(key, 403U);
    store_recorder running_store;
    backend->StoreHook = [&vault, running_store_context] {
        vault->cancel(running_store_context);
    };
    submit_store(*vault, running_store_context, bytes, running_store);
    lane.run_next();
    expect_store_success(running_store, running_store_context);
    expect(
        backend->contains(key),
        "cancel after backend start loses to terminal result");

    const auto running_retrieve_context = context(key, 404U);
    retrieve_recorder running_retrieve;
    backend->RetrieveHook = [&vault, running_retrieve_context] {
        vault->cancel(running_retrieve_context);
    };
    submit_retrieve(
        *vault,
        running_retrieve_context,
        running_retrieve);
    lane.run_next();
    expect_retrieve_success(
        running_retrieve,
        running_retrieve_context,
        bytes);

    const auto sibling_a = context(key, 405U);
    const auto sibling_b = context(key, 406U);
    retrieve_recorder retrieve_a;
    retrieve_recorder retrieve_b;
    submit_retrieve(*vault, sibling_a, retrieve_a);
    submit_retrieve(*vault, sibling_b, retrieve_b);
    vault->cancel(sibling_a);
    expect_retrieve_cancelled(retrieve_a, sibling_a);
    lane.drain();
    expect_retrieve_success(retrieve_b, sibling_b, bytes);
    expect(
        retrieve_a.Calls == 1U,
        "exact cancellation does not receive a late terminal");

    const auto running_delete_context = context(key, 407U);
    delete_recorder running_delete;
    backend->DeleteHook = [&vault, running_delete_context] {
        vault->cancel(running_delete_context);
    };
    submit_delete(
        *vault,
        running_delete_context,
        running_delete);
    lane.run_next();
    expect_delete_success(
        running_delete,
        running_delete_context,
        application::github_credential_delete_disposition::deleted);
}

void test_teardown_and_callback_reentrancy()
{
    application::manual_executor lane;
    auto observation = std::make_shared<wipe_observation>();
    auto backend =
        std::make_shared<wiping_fake_backend>(observation);
    auto vault = make_test_vault(lane, backend);
    const auto bytes = generated_bytes(19U);
    const auto key = authority();

    const auto store_context = context(key, 500U);
    const auto retrieve_context = context(key, 501U);
    store_recorder store;
    retrieve_recorder reentrant_retrieve;
    vault->store(
        application::github_credential_enrollment{
            .Context = store_context,
            .Credential = material_from(bytes),
        },
        [&store,
         &reentrant_retrieve,
         &vault,
         retrieve_context](
            application::github_credential_store_completion terminal) {
            store.record(std::move(terminal));
            submit_retrieve(
                *vault,
                retrieve_context,
                reentrant_retrieve);
        });
    lane.run_next();
    expect_store_success(store, store_context);
    expect(
        lane.pending() == 1U,
        "completion can safely enqueue a reentrant operation");
    lane.run_next();
    expect_retrieve_success(
        reentrant_retrieve,
        retrieve_context,
        bytes);

    const auto teardown_store_context = context(key, 502U);
    const auto teardown_retrieve_context = context(key, 503U);
    const auto teardown_delete_context = context(key, 504U);
    store_recorder teardown_store;
    retrieve_recorder teardown_retrieve;
    delete_recorder teardown_delete;
    submit_store(
        *vault,
        teardown_store_context,
        bytes,
        teardown_store);
    submit_retrieve(
        *vault,
        teardown_retrieve_context,
        teardown_retrieve);
    submit_delete(
        *vault,
        teardown_delete_context,
        teardown_delete);
    const auto calls_before_teardown = backend->Calls.size();
    vault.reset();
    expect_store_cancelled(teardown_store, teardown_store_context);
    expect_retrieve_cancelled(
        teardown_retrieve,
        teardown_retrieve_context);
    expect_delete_cancelled(
        teardown_delete,
        teardown_delete_context);
    lane.drain();
    expect(
        backend->Calls.size() == calls_before_teardown
            && teardown_store.Calls == 1U
            && teardown_retrieve.Calls == 1U
            && teardown_delete.Calls == 1U,
        "teardown terminals queued work once and posted tasks no-op");
}

void test_teardown_atomically_claims_all_queued_work()
{
    application::manual_executor lane;
    auto observation = std::make_shared<wipe_observation>();
    auto backend =
        std::make_shared<wiping_fake_backend>(observation);
    auto vault = make_test_vault(lane, backend);
    const auto bytes = generated_bytes(21U);
    const auto first_context = context(authority(), 550U);
    const auto second_context = context(authority(), 551U);
    store_recorder first;
    store_recorder second;

    std::mutex gate_mutex;
    std::condition_variable gate;
    auto first_callback_entered = false;
    auto release_first_callback = false;

    vault->store(
        application::github_credential_enrollment{
            .Context = first_context,
            .Credential = material_from(bytes),
        },
        [&first,
         &gate_mutex,
         &gate,
         &first_callback_entered,
         &release_first_callback](
            application::github_credential_store_completion terminal) {
            first.record(std::move(terminal));
            auto lock = std::unique_lock{gate_mutex};
            first_callback_entered = true;
            gate.notify_all();
            gate.wait(lock, [&release_first_callback] {
                return release_first_callback;
            });
        });
    submit_store(
        *vault,
        second_context,
        bytes,
        second);
    expect(
        lane.pending() == 2U,
        "two teardown-race operations are queued");

    auto destroyer = std::thread{[&vault] {
        vault.reset();
    }};

    auto entered = false;
    {
        auto lock = std::unique_lock{gate_mutex};
        entered = gate.wait_for(
            lock,
            std::chrono::seconds{2},
            [&first_callback_entered] {
                return first_callback_entered;
            });
    }
    if(!entered)
    {
        {
            const auto lock = std::scoped_lock{gate_mutex};
            release_first_callback = true;
        }
        gate.notify_all();
        destroyer.join();
        expect(false, "teardown reaches the first cancellation callback");
    }

    lane.run(1U);
    {
        const auto lock = std::scoped_lock{gate_mutex};
        release_first_callback = true;
    }
    gate.notify_all();
    destroyer.join();

    expect_store_cancelled(first, first_context);
    expect_store_cancelled(second, second_context);
    expect(
        backend->Calls.empty(),
        "teardown atomically claims every queued operation");

    const auto first_calls = first.Calls;
    const auto second_calls = second.Calls;
    lane.drain();
    expect(
        backend->Calls.empty() && first.Calls == first_calls
            && second.Calls == second_calls,
        "posted tasks no-op after concurrent teardown");
}

void test_fake_backend_wipes_on_replacement_delete_and_teardown()
{
    application::manual_executor lane;
    auto observation = std::make_shared<wipe_observation>();
    auto backend =
        std::make_shared<wiping_fake_backend>(observation);
    auto vault = make_test_vault(lane, backend);
    const auto first = generated_bytes(23U, 37U);
    const auto second = generated_bytes(29U, 43U);

    store_recorder store_first;
    submit_store(
        *vault,
        context(authority(), 600U),
        first,
        store_first);
    lane.run_next();

    store_recorder store_second;
    submit_store(
        *vault,
        context(authority(), 601U),
        second,
        store_second);
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

    store_recorder final_store;
    submit_store(
        *vault,
        context(authority(), 603U),
        first,
        final_store);
    lane.run_next();
    vault.reset();
    backend.reset();
    expect(
        observation->Buffers == 3U
            && observation->Bytes
                == first.size() + second.size() + first.size(),
        "backend teardown overwrites every remaining fake byte");
}
}

auto main() -> int
{
    test_target_and_authority_encoding();
    test_complete_status_classification();
    test_invalid_and_duplicate_requests();
    test_replacement_absence_delete_and_reordering();
    test_failures_preserve_existing_material();
    test_executor_and_backend_exceptions();
    test_cancellation_races_and_sibling_isolation();
    test_teardown_and_callback_reentrancy();
    test_teardown_atomically_claims_all_queued_work();
    test_fake_backend_wipes_on_replacement_delete_and_teardown();
    return EXIT_SUCCESS;
}
