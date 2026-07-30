#include "linux_secret_service.hpp"
#include "linux_secret_service_test_support.hpp"

#include "bpr/application/manual_executor.hpp"

#include <libsecret/secret.h>

#include <array>
#include <chrono>
#include <condition_variable>
#include <concepts>
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
using namespace bpr::tests::linux_secret_service;

using authority_key =
    application::github_credential_authority_key;
using operation_context =
    application::github_credential_operation_context;
using credential_material =
    application::github_credential_material;
using vault_failure =
    application::github_credential_vault_failure;

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
using injected_factory_signature = auto (*)(
    application::effect_executor&,
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
        if(this != &other)
        {
            reset();
            Bytes = std::move(other.Bytes);
            Size = std::exchange(other.Size, 0U);
            Observation = std::move(other.Observation);
        }
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

class wiping_fake_backend final : public linux_secret_service_backend
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
        -> linux_secret_service_backend_store_result override
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
        -> linux_secret_service_backend_retrieve_result override
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
            return linux_secret_service_credential_absent{};
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
        -> linux_secret_service_backend_delete_result override
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
        make_linux_secret_service_vault_with_backend(lane, backend);
    expect(result.has_value(), "injected Secret Service vault is created");
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

template<typename StatusCode>
void expect_status(
    const linux_secret_service_status_domain domain,
    const StatusCode code,
    const vault_failure failure)
{
    expect(
        classify_linux_secret_service_status({
            .Domain = domain,
            .Code = static_cast<std::int32_t>(code),
        }) == failure,
        "Secret Service status has the exact bounded classification");
}

void test_schema_and_authority_encoding()
{
    constexpr std::string_view production_schema =
        "org.betterpullreview.github-native-credential."
        "secret-service.v1";
    constexpr std::string_view integration_schema =
        "org.betterpullreview.github-native-credential."
        "secret-service.integration-test.v1";
    expect(
        linux_secret_service_schema_identifier(
            linux_secret_service_schema_scope::production)
            == production_schema,
        "production Secret Service schema is exact and stable");
    expect(
        linux_secret_service_schema_identifier(
            linux_secret_service_schema_scope::integration_test)
            == integration_schema,
        "integration schema is isolated from production");
    expect(
        linux_secret_service_schema_identifier(
            static_cast<linux_secret_service_schema_scope>(255U))
            .empty(),
        "malformed schema scope has no identifier");

    const auto base = authority();
    const auto encoded =
        encode_linux_secret_service_authority_account(base);
    expect(encoded.has_value(), "valid authority encodes");
    expect(
        std::ranges::all_of(*encoded, [](const char character) {
            return (character >= '0' && character <= '9')
                || (character >= 'a' && character <= 'f');
        }),
        "authority attribute is fixed-width lowercase hexadecimal");
    expect(
        std::string_view{encoded->data(), encoded->size()}
            == "000000000000000100000000000000020000000000000003"
               "000000000000000401",
        "GitHub.com authority has the exact stable v1 encoding");

    const auto enterprise =
        encode_linux_secret_service_authority_account(authority(
            1U,
            2U,
            3U,
            4U,
            application::github_server_contract::
                ghes_3_18_2022_11_28));
    expect(
        enterprise.has_value()
            && std::string_view{
                   enterprise->data(),
                   enterprise->size()}
                == "000000000000000100000000000000020000000000000003"
                   "000000000000000402",
        "GHES authority has the exact stable v1 encoding");

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
        authority(1U, 23U, 4U, 5U),
        authority(12U, 3U, 4U, 5U),
    };
    for(const auto& variant : variants)
    {
        const auto different =
            encode_linux_secret_service_authority_account(variant);
        expect(
            different.has_value() && *different != *encoded,
            "every authority dimension is attribute-key injective");
    }

    auto malformed = base;
    malformed.Profile = {};
    expect(
        encode_linux_secret_service_authority_account(malformed)
            == std::unexpected{vault_failure::invalid_request},
        "zero profile is rejected before Secret Service");
    malformed = base;
    malformed.ProfileRevision = {};
    expect(
        encode_linux_secret_service_authority_account(malformed)
            == std::unexpected{vault_failure::invalid_request},
        "zero profile revision is rejected before Secret Service");
    malformed = base;
    malformed.Host = {};
    expect(
        encode_linux_secret_service_authority_account(malformed)
            == std::unexpected{vault_failure::invalid_request},
        "zero host is rejected before Secret Service");
    malformed = base;
    malformed.Account = {};
    expect(
        encode_linux_secret_service_authority_account(malformed)
            == std::unexpected{vault_failure::invalid_request},
        "zero account is rejected before Secret Service");
    malformed = base;
    malformed.ServerContract =
        static_cast<application::github_server_contract>(255U);
    expect(
        encode_linux_secret_service_authority_account(malformed)
            == std::unexpected{vault_failure::invalid_request},
        "malformed contract is rejected before Secret Service");
}

void test_status_classification()
{
    using domain = linux_secret_service_status_domain;

    expect_status(
        domain::secret_service,
        SECRET_ERROR_IS_LOCKED,
        vault_failure::locked);
    for(const auto code : {
            SECRET_ERROR_PROTOCOL,
            SECRET_ERROR_INVALID_FILE_FORMAT,
            SECRET_ERROR_MISMATCHED_SCHEMA,
            SECRET_ERROR_NO_MATCHING_ATTRIBUTE,
            SECRET_ERROR_WRONG_TYPE,
            SECRET_ERROR_EMPTY_TABLE,
        })
    {
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

    for(const auto code : {
            G_DBUS_ERROR_ACCESS_DENIED,
            G_DBUS_ERROR_AUTH_FAILED,
            G_DBUS_ERROR_SPAWN_PERMISSIONS_INVALID,
        })
    {
        expect_status(domain::dbus, code, vault_failure::access_denied);
    }
    for(const auto code : {
            G_DBUS_ERROR_SERVICE_UNKNOWN,
            G_DBUS_ERROR_NAME_HAS_NO_OWNER,
            G_DBUS_ERROR_NO_REPLY,
            G_DBUS_ERROR_BAD_ADDRESS,
            G_DBUS_ERROR_NO_SERVER,
            G_DBUS_ERROR_TIMEOUT,
            G_DBUS_ERROR_DISCONNECTED,
            G_DBUS_ERROR_UNKNOWN_OBJECT,
            G_DBUS_ERROR_SPAWN_SERVICE_NOT_FOUND,
        })
    {
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

    for(const auto code : {
            G_IO_ERROR_PERMISSION_DENIED,
            G_IO_ERROR_PROXY_AUTH_FAILED,
            G_IO_ERROR_PROXY_NEED_AUTH,
            G_IO_ERROR_PROXY_NOT_ALLOWED,
        })
    {
        expect_status(domain::io, code, vault_failure::access_denied);
    }
    expect_status(
        domain::io,
        G_IO_ERROR_INVALID_ARGUMENT,
        vault_failure::invalid_request);
    for(const auto code : {
            G_IO_ERROR_PARTIAL_INPUT,
            G_IO_ERROR_INVALID_DATA,
            G_IO_ERROR_MESSAGE_TOO_LARGE,
        })
    {
        expect_status(domain::io, code, vault_failure::corrupt);
    }
    for(const auto code : {
            G_IO_ERROR_NOT_FOUND,
            G_IO_ERROR_NOT_INITIALIZED,
            G_IO_ERROR_CLOSED,
            G_IO_ERROR_TIMED_OUT,
            G_IO_ERROR_DBUS_ERROR,
            G_IO_ERROR_CONNECTION_REFUSED,
            G_IO_ERROR_BROKEN_PIPE,
            G_IO_ERROR_NOT_CONNECTED,
        })
    {
        expect_status(domain::io, code, vault_failure::unavailable);
    }
    expect_status(domain::none, 0, vault_failure::operation_failed);
    expect_status(domain::other, 71, vault_failure::operation_failed);
}

void test_invalid_duplicate_and_exact_authority()
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
        "invalid work never reaches the effect lane or backend");

    auto null_factory =
        make_linux_secret_service_vault_with_backend(lane, {});
    expect(
        !null_factory.has_value()
            && null_factory.error() == vault_failure::invalid_request,
        "null backend fails closed");

    const auto duplicate_context = context(authority(), 100U);
    store_recorder first;
    store_recorder duplicate;
    submit_store(*vault, duplicate_context, bytes, first);
    submit_store(*vault, duplicate_context, bytes, duplicate);
    expect_store_error(
        duplicate,
        duplicate_context,
        vault_failure::invalid_request);

    retrieve_recorder cross_kind;
    submit_retrieve(*vault, duplicate_context, cross_kind);
    expect_retrieve_error(
        cross_kind,
        duplicate_context,
        vault_failure::invalid_request);
    expect(lane.pending() == 1U, "duplicate correlation queues once");
    lane.run_next();
    expect_store_success(first, duplicate_context);
}

void test_replacement_absence_delete_and_reordering()
{
    application::manual_executor lane;
    auto observation = std::make_shared<wipe_observation>();
    auto backend =
        std::make_shared<wiping_fake_backend>(observation);
    auto vault = make_test_vault(lane, backend);
    const auto first = generated_bytes(5U, 73U);
    const auto maximum = generated_bytes(
        7U,
        application::maximum_github_credential_bytes);
    const auto key = authority();

    store_recorder first_store;
    store_recorder maximum_store;
    submit_store(*vault, context(key, 200U), first, first_store);
    submit_store(
        *vault,
        context(key, 201U),
        maximum,
        maximum_store);
    expect(lane.pending() == 2U, "independent contexts can queue");
    lane.run(1U);
    expect_store_success(maximum_store, context(key, 201U));
    expect(
        backend->matches(key, maximum),
        "reordered maximum-size store reaches the backend first");

    retrieve_recorder maximum_retrieve;
    submit_retrieve(
        *vault,
        context(key, 202U),
        maximum_retrieve);
    lane.run(1U);
    expect_retrieve_success(
        maximum_retrieve,
        context(key, 202U),
        maximum);

    lane.run(0U);
    expect_store_success(first_store, context(key, 200U));
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
    auto request = std::uint64_t{210U};
    for(const auto& isolated : isolated_authorities)
    {
        const auto isolated_context = context(isolated, request++);
        retrieve_recorder absent;
        submit_retrieve(*vault, isolated_context, absent);
        lane.run_next();
        expect_retrieve_absent(absent, isolated_context);
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

    delete_recorder delete_absent;
    submit_delete(*vault, context(key, 206U), delete_absent);
    lane.run_next();
    expect_delete_success(
        delete_absent,
        context(key, 206U),
        application::github_credential_delete_disposition::absent);
}

void test_failures_and_malformed_results_preserve_material()
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
    auto request = std::uint64_t{301U};
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
    store_recorder bad_store;
    submit_store(*vault, context(key, request++), replacement, bad_store);
    lane.run_next();
    expect_store_error(
        bad_store,
        context(key, request - 1U),
        vault_failure::operation_failed);

    backend->MalformedRetrieveResult = true;
    retrieve_recorder bad_retrieve;
    submit_retrieve(
        *vault,
        context(key, request++),
        bad_retrieve);
    lane.run_next();
    expect_retrieve_error(
        bad_retrieve,
        context(key, request - 1U),
        vault_failure::operation_failed);

    backend->MalformedDeleteResult = true;
    delete_recorder bad_delete;
    submit_delete(*vault, context(key, request), bad_delete);
    lane.run_next();
    expect_delete_error(
        bad_delete,
        context(key, request),
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

    struct expectation
    {
        injected_exception Exception = injected_exception::none;
        vault_failure Failure = vault_failure::operation_failed;
    };
    constexpr std::array cases{
        expectation{
            .Exception = injected_exception::allocation,
            .Failure = vault_failure::allocation_failure,
        },
        expectation{
            .Exception = injected_exception::other,
            .Failure = vault_failure::operation_failed,
        },
    };
    auto request = std::uint64_t{351U};
    for(const auto expected : cases)
    {
        const auto store_context = context(key, request++);
        backend->NextStoreException = expected.Exception;
        store_recorder store;
        submit_store(*vault, store_context, replacement, store);
        lane.run_next();
        expect_store_error(store, store_context, expected.Failure);

        const auto retrieve_context = context(key, request++);
        backend->NextRetrieveException = expected.Exception;
        retrieve_recorder retrieve;
        submit_retrieve(*vault, retrieve_context, retrieve);
        lane.run_next();
        expect_retrieve_error(
            retrieve,
            retrieve_context,
            expected.Failure);

        const auto delete_context = context(key, request++);
        backend->NextDeleteException = expected.Exception;
        delete_recorder deleted;
        submit_delete(*vault, delete_context, deleted);
        lane.run_next();
        expect_delete_error(
            deleted,
            delete_context,
            expected.Failure);
        expect(
            backend->matches(key, original),
            "backend exceptions preserve prior material");
    }

    request = 380U;
    for(const auto expected : cases)
    {
        throwing_effect_executor executor{expected.Exception};
        auto post_observation = std::make_shared<wipe_observation>();
        auto post_backend =
            std::make_shared<wiping_fake_backend>(post_observation);
        auto result =
            make_linux_secret_service_vault_with_backend(
                executor,
                post_backend);
        expect(result.has_value(), "throwing executor vault is created");
        auto post_vault = std::move(*result);

        const auto store_context = context(key, request++);
        store_recorder store;
        submit_store(*post_vault, store_context, replacement, store);
        expect_store_error(store, store_context, expected.Failure);

        const auto retrieve_context = context(key, request++);
        retrieve_recorder retrieve;
        submit_retrieve(*post_vault, retrieve_context, retrieve);
        expect_retrieve_error(
            retrieve,
            retrieve_context,
            expected.Failure);

        const auto delete_context = context(key, request++);
        delete_recorder deleted;
        submit_delete(*post_vault, delete_context, deleted);
        expect_delete_error(
            deleted,
            delete_context,
            expected.Failure);
        expect(
            post_backend->Calls.empty(),
            "executor post failure never reaches the backend");
    }
}

void test_cancellation_reentrancy_and_teardown()
{
    application::manual_executor lane;
    auto observation = std::make_shared<wipe_observation>();
    auto backend =
        std::make_shared<wiping_fake_backend>(observation);
    auto vault = make_test_vault(lane, backend);
    const auto bytes = generated_bytes(17U);
    const auto key = authority();

    const auto queued_context = context(key, 400U);
    store_recorder queued;
    submit_store(*vault, queued_context, bytes, queued);
    vault->cancel(queued_context);
    expect_store_cancelled(queued, queued_context);
    lane.run_next();
    expect(
        queued.Calls == 1U && backend->Calls.empty(),
        "cancelled queued work has no effect or late completion");

    const auto running_context = context(key, 401U);
    store_recorder running;
    backend->StoreHook = [&vault, running_context] {
        vault->cancel(running_context);
    };
    submit_store(*vault, running_context, bytes, running);
    lane.run_next();
    expect_store_success(running, running_context);
    expect(
        backend->contains(key),
        "cancel after backend start loses to terminal result");

    const auto sibling_a = context(key, 402U);
    const auto sibling_b = context(key, 403U);
    retrieve_recorder retrieve_a;
    retrieve_recorder retrieve_b;
    submit_retrieve(*vault, sibling_a, retrieve_a);
    submit_retrieve(*vault, sibling_b, retrieve_b);
    vault->cancel(sibling_a);
    expect_retrieve_cancelled(retrieve_a, sibling_a);
    lane.drain();
    expect_retrieve_success(retrieve_b, sibling_b, bytes);

    const auto store_context = context(key, 404U);
    const auto reentrant_context = context(key, 405U);
    store_recorder store;
    retrieve_recorder reentrant;
    vault->store(
        application::github_credential_enrollment{
            .Context = store_context,
            .Credential = material_from(bytes),
        },
        [&store, &reentrant, &vault, reentrant_context](
            application::github_credential_store_completion terminal) {
            store.record(std::move(terminal));
            submit_retrieve(*vault, reentrant_context, reentrant);
        });
    lane.run_next();
    expect_store_success(store, store_context);
    expect(lane.pending() == 1U, "callback can enqueue new work");
    lane.run_next();
    expect_retrieve_success(reentrant, reentrant_context, bytes);

    const auto teardown_store_context = context(key, 406U);
    const auto teardown_retrieve_context = context(key, 407U);
    const auto teardown_delete_context = context(key, 408U);
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
    const auto calls_before = backend->Calls.size();
    vault.reset();
    expect_store_cancelled(teardown_store, teardown_store_context);
    expect_retrieve_cancelled(
        teardown_retrieve,
        teardown_retrieve_context);
    expect_delete_cancelled(teardown_delete, teardown_delete_context);
    lane.drain();
    expect(
        backend->Calls.size() == calls_before
            && teardown_store.Calls == 1U
            && teardown_retrieve.Calls == 1U
            && teardown_delete.Calls == 1U,
        "teardown terminals queued work once and tasks no-op");
}

void test_teardown_atomically_claims_queued_work()
{
    application::manual_executor lane;
    auto observation = std::make_shared<wipe_observation>();
    auto backend =
        std::make_shared<wiping_fake_backend>(observation);
    auto vault = make_test_vault(lane, backend);
    const auto bytes = generated_bytes(21U);
    const auto first_context = context(authority(), 500U);
    const auto second_context = context(authority(), 501U);
    store_recorder first;
    store_recorder second;

    std::mutex gate_mutex;
    std::condition_variable gate;
    auto callback_entered = false;
    auto release_callback = false;

    vault->store(
        application::github_credential_enrollment{
            .Context = first_context,
            .Credential = material_from(bytes),
        },
        [&first,
         &gate_mutex,
         &gate,
         &callback_entered,
         &release_callback](
            application::github_credential_store_completion terminal) {
            first.record(std::move(terminal));
            auto lock = std::unique_lock{gate_mutex};
            callback_entered = true;
            gate.notify_all();
            gate.wait(lock, [&release_callback] {
                return release_callback;
            });
        });
    submit_store(*vault, second_context, bytes, second);

    auto destroyer = std::thread{[&vault] {
        vault.reset();
    }};
    auto entered = false;
    {
        auto lock = std::unique_lock{gate_mutex};
        entered = gate.wait_for(
            lock,
            std::chrono::seconds{2},
            [&callback_entered] {
                return callback_entered;
            });
    }
    if(!entered)
    {
        {
            const auto lock = std::scoped_lock{gate_mutex};
            release_callback = true;
        }
        gate.notify_all();
        destroyer.join();
        expect(false, "teardown reaches first cancellation callback");
    }

    lane.run(1U);
    {
        const auto lock = std::scoped_lock{gate_mutex};
        release_callback = true;
    }
    gate.notify_all();
    destroyer.join();

    expect_store_cancelled(first, first_context);
    expect_store_cancelled(second, second_context);
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
    auto observation = std::make_shared<wipe_observation>();
    auto backend =
        std::make_shared<wiping_fake_backend>(observation);
    auto vault = make_test_vault(lane, backend);
    const auto first = generated_bytes(23U, 37U);
    const auto second = generated_bytes(29U, 43U);

    store_recorder first_store;
    submit_store(
        *vault,
        context(authority(), 600U),
        first,
        first_store);
    lane.run_next();
    store_recorder second_store;
    submit_store(
        *vault,
        context(authority(), 601U),
        second,
        second_store);
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
        "backend teardown overwrites remaining fake material");
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
    return EXIT_SUCCESS;
}
