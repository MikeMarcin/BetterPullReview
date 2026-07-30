#include "macos_keychain.hpp"

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#include <map>
#include <mutex>
#include <new>
#include <optional>
#include <utility>

namespace bpr::adapters::github
{
namespace
{
constexpr std::string_view production_service =
    "org.betterpullreview.github-native-credential.file-keychain.v2";
constexpr std::string_view integration_test_service =
    "org.betterpullreview.github-native-credential.file-keychain."
    "integration-test.v2";

template<typename... Callables>
struct overloaded : Callables...
{
    using Callables::operator()...;
};

template<typename... Callables>
overloaded(Callables...) -> overloaded<Callables...>;

template<typename Reference>
class cf_owner final
{
public:
    cf_owner() = default;
    explicit cf_owner(Reference value) noexcept
        : Value{value}
    {
    }

    ~cf_owner() noexcept
    {
        reset();
    }

    cf_owner(const cf_owner&) = delete;
    auto operator=(const cf_owner&) -> cf_owner& = delete;

    cf_owner(cf_owner&& other) noexcept
        : Value{std::exchange(other.Value, nullptr)}
    {
    }

    auto operator=(cf_owner&& other) noexcept -> cf_owner&
    {
        if(this == &other)
        {
            return *this;
        }

        reset();
        Value = std::exchange(other.Value, nullptr);
        return *this;
    }

    [[nodiscard]] auto get() const noexcept -> Reference
    {
        return Value;
    }

    [[nodiscard]] auto release() noexcept -> Reference
    {
        return std::exchange(Value, nullptr);
    }

    void reset(Reference value = nullptr) noexcept
    {
        if(Value != nullptr)
        {
            CFRelease(Value);
        }
        Value = value;
    }

private:
    Reference Value = nullptr;
};

[[nodiscard]] constexpr auto hex_digit(const std::uint8_t value) noexcept
    -> char
{
    constexpr std::array<char, 16U> digits{
        '0',
        '1',
        '2',
        '3',
        '4',
        '5',
        '6',
        '7',
        '8',
        '9',
        'a',
        'b',
        'c',
        'd',
        'e',
        'f',
    };
    return digits[value];
}

void append_hex_u64(
    macos_keychain_authority_account& output,
    std::size_t& offset,
    const std::uint64_t value) noexcept
{
    for(std::size_t nibble = 0U; nibble < 16U; ++nibble)
    {
        const auto shift = static_cast<unsigned int>((15U - nibble) * 4U);
        const auto digit = static_cast<std::uint8_t>(
            (value >> shift) & 0xFU);
        output[offset] = hex_digit(digit);
        ++offset;
    }
}

void append_hex_u8(
    macos_keychain_authority_account& output,
    std::size_t& offset,
    const std::uint8_t value) noexcept
{
    output[offset] = hex_digit(static_cast<std::uint8_t>(value >> 4U));
    ++offset;
    output[offset] = hex_digit(static_cast<std::uint8_t>(value & 0xFU));
    ++offset;
}

[[nodiscard]] constexpr auto server_contract_discriminator(
    const application::github_server_contract contract) noexcept
    -> std::uint8_t
{
    switch(contract)
    {
    case application::github_server_contract::github_com_2022_11_28:
        return 0x01U;
    case application::github_server_contract::ghes_3_18_2022_11_28:
        return 0x02U;
    }
    return 0x00U;
}

[[nodiscard]] auto failure_mapping(
    const application::github_credential_vault_failure failure) noexcept
    -> macos_keychain_status_mapping
{
    return {
        .Kind = macos_keychain_status_kind::failure,
        .Failure = failure,
    };
}

[[nodiscard]] constexpr auto normalize_failure(
    const application::github_credential_vault_failure failure) noexcept
    -> application::github_credential_vault_failure
{
    return application::valid(failure)
        ? failure
        : application::github_credential_vault_failure::operation_failed;
}

[[nodiscard]] auto mapped_failure(const OSStatus status) noexcept
    -> application::github_credential_vault_failure
{
    const auto mapping =
        classify_macos_keychain_status(static_cast<std::int32_t>(status));
    if(mapping.Kind != macos_keychain_status_kind::failure)
    {
        return application::github_credential_vault_failure::
            operation_failed;
    }
    return normalize_failure(mapping.Failure);
}

void add_noninteractive_policy(CFMutableDictionaryRef dictionary) noexcept
{
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    CFDictionarySetValue(
        dictionary,
        kSecUseAuthenticationUI,
        kSecUseAuthenticationUIFail);
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
}

[[nodiscard]] auto make_mutable_dictionary() noexcept
    -> cf_owner<CFMutableDictionaryRef>
{
    return cf_owner<CFMutableDictionaryRef>{CFDictionaryCreateMutable(
        kCFAllocatorDefault,
        0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks)};
}

[[nodiscard]] auto make_cf_string(
    const std::span<const char> bytes) noexcept -> cf_owner<CFStringRef>
{
    return cf_owner<CFStringRef>{CFStringCreateWithBytes(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(bytes.data()),
        static_cast<CFIndex>(bytes.size()),
        kCFStringEncodingASCII,
        false)};
}

[[nodiscard]] auto make_cf_string(
    const std::string_view bytes) noexcept -> cf_owner<CFStringRef>
{
    return make_cf_string(std::span<const char>{bytes});
}

[[nodiscard]] auto make_borrowed_data(
    const application::github_credential_material& credential) noexcept
    -> cf_owner<CFDataRef>
{
    const auto bytes = credential.bytes();
    return cf_owner<CFDataRef>{CFDataCreateWithBytesNoCopy(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(bytes.data()),
        static_cast<CFIndex>(bytes.size()),
        kCFAllocatorNull)};
}

class native_macos_keychain_backend final : public macos_keychain_backend
{
public:
    native_macos_keychain_backend(
        SecKeychainRef keychain,
        CFArrayRef search_list,
        CFStringRef service) noexcept
        : Keychain{keychain}
        , SearchList{search_list}
        , Service{service}
    {
    }

    ~native_macos_keychain_backend() override = default;

    [[nodiscard]] auto store(
        const application::github_credential_authority_key& authority,
        const application::github_credential_material& credential)
        -> macos_keychain_backend_store_result override
    {
        const auto lock = std::scoped_lock{OperationMutex};
        if(!application::valid(authority) || !application::valid(credential))
        {
            return std::unexpected{
                application::github_credential_vault_failure::
                    invalid_request};
        }

        const auto account = account_string(authority);
        if(account.get() == nullptr)
        {
            return std::unexpected{
                application::github_credential_vault_failure::
                    allocation_failure};
        }

        const auto value = make_borrowed_data(credential);
        if(value.get() == nullptr)
        {
            return std::unexpected{
                application::github_credential_vault_failure::
                    allocation_failure};
        }

        auto add = add_dictionary(account.get(), value.get());
        if(add.get() == nullptr)
        {
            return std::unexpected{
                application::github_credential_vault_failure::
                    allocation_failure};
        }

        auto status = SecItemAdd(add.get(), nullptr);
        auto mapping = classify_macos_keychain_status(
            static_cast<std::int32_t>(status));
        if(mapping.Kind == macos_keychain_status_kind::success)
        {
            return application::github_credential_store_disposition::
                stored;
        }
        if(mapping.Kind != macos_keychain_status_kind::duplicate_item)
        {
            return std::unexpected{mapped_failure(status)};
        }

        auto query = exact_query(account.get());
        auto update = value_dictionary(value.get());
        if(query.get() == nullptr || update.get() == nullptr)
        {
            return std::unexpected{
                application::github_credential_vault_failure::
                    allocation_failure};
        }

        status = SecItemUpdate(query.get(), update.get());
        mapping = classify_macos_keychain_status(
            static_cast<std::int32_t>(status));
        if(mapping.Kind == macos_keychain_status_kind::success)
        {
            return application::github_credential_store_disposition::
                stored;
        }
        if(mapping.Kind != macos_keychain_status_kind::item_not_found)
        {
            return std::unexpected{mapped_failure(status)};
        }

        status = SecItemAdd(add.get(), nullptr);
        mapping = classify_macos_keychain_status(
            static_cast<std::int32_t>(status));
        if(mapping.Kind == macos_keychain_status_kind::success)
        {
            return application::github_credential_store_disposition::
                stored;
        }
        return std::unexpected{mapped_failure(status)};
    }

    [[nodiscard]] auto retrieve(
        const application::github_credential_authority_key& authority)
        -> macos_keychain_backend_retrieve_result override
    {
        const auto lock = std::scoped_lock{OperationMutex};
        if(!application::valid(authority))
        {
            return application::github_credential_vault_failure::
                invalid_request;
        }

        const auto account = account_string(authority);
        if(account.get() == nullptr)
        {
            return application::github_credential_vault_failure::
                allocation_failure;
        }

        auto query = exact_query(account.get());
        if(query.get() == nullptr)
        {
            return application::github_credential_vault_failure::
                allocation_failure;
        }
        CFDictionarySetValue(
            query.get(),
            kSecReturnData,
            kCFBooleanTrue);
        CFDictionarySetValue(
            query.get(),
            kSecMatchLimit,
            kSecMatchLimitOne);

        CFTypeRef raw_result = nullptr;
        const auto status =
            SecItemCopyMatching(query.get(), &raw_result);
        cf_owner<CFTypeRef> result{raw_result};
        const auto mapping = classify_macos_keychain_status(
            static_cast<std::int32_t>(status));
        if(mapping.Kind == macos_keychain_status_kind::item_not_found)
        {
            return macos_keychain_credential_absent{};
        }
        if(mapping.Kind != macos_keychain_status_kind::success)
        {
            return mapped_failure(status);
        }
        if(
            result.get() == nullptr
            || CFGetTypeID(result.get()) != CFDataGetTypeID())
        {
            return application::github_credential_vault_failure::corrupt;
        }

        const auto data = static_cast<CFDataRef>(result.get());
        const auto length = CFDataGetLength(data);
        if(
            length <= 0
            || static_cast<std::uint64_t>(length)
                > application::maximum_github_credential_bytes)
        {
            return application::github_credential_vault_failure::corrupt;
        }

        const auto* bytes = CFDataGetBytePtr(data);
        if(bytes == nullptr)
        {
            return application::github_credential_vault_failure::corrupt;
        }

        auto material = application::github_credential_material::create(
            std::span<const std::byte>{
                reinterpret_cast<const std::byte*>(bytes),
                static_cast<std::size_t>(length)});
        if(!material.has_value())
        {
            return material.error()
                    == application::github_credential_material_error::
                        allocation_failure
                ? application::github_credential_vault_failure::
                    allocation_failure
                : application::github_credential_vault_failure::corrupt;
        }
        return std::move(*material);
    }

    [[nodiscard]] auto delete_credential(
        const application::github_credential_authority_key& authority)
        -> macos_keychain_backend_delete_result override
    {
        const auto lock = std::scoped_lock{OperationMutex};
        if(!application::valid(authority))
        {
            return std::unexpected{
                application::github_credential_vault_failure::
                    invalid_request};
        }

        const auto account = account_string(authority);
        if(account.get() == nullptr)
        {
            return std::unexpected{
                application::github_credential_vault_failure::
                    allocation_failure};
        }

        const auto query = exact_query(account.get());
        if(query.get() == nullptr)
        {
            return std::unexpected{
                application::github_credential_vault_failure::
                    allocation_failure};
        }

        const auto status = SecItemDelete(query.get());
        const auto mapping = classify_macos_keychain_status(
            static_cast<std::int32_t>(status));
        if(mapping.Kind == macos_keychain_status_kind::success)
        {
            return application::github_credential_delete_disposition::
                deleted;
        }
        if(mapping.Kind == macos_keychain_status_kind::item_not_found)
        {
            return application::github_credential_delete_disposition::
                absent;
        }
        return std::unexpected{mapped_failure(status)};
    }

private:
    [[nodiscard]] auto account_string(
        const application::github_credential_authority_key& authority) const
        noexcept -> cf_owner<CFStringRef>
    {
        const auto encoded =
            encode_macos_keychain_authority_account(authority);
        if(!encoded.has_value())
        {
            return {};
        }
        return make_cf_string(std::span<const char>{*encoded});
    }

    [[nodiscard]] auto add_dictionary(
        CFStringRef account,
        CFDataRef value) const noexcept
        -> cf_owner<CFMutableDictionaryRef>
    {
        auto dictionary = make_mutable_dictionary();
        if(dictionary.get() == nullptr)
        {
            return {};
        }

        CFDictionarySetValue(
            dictionary.get(),
            kSecClass,
            kSecClassGenericPassword);
        CFDictionarySetValue(
            dictionary.get(),
            kSecAttrService,
            Service.get());
        CFDictionarySetValue(dictionary.get(), kSecAttrAccount, account);
        CFDictionarySetValue(dictionary.get(), kSecValueData, value);
        CFDictionarySetValue(
            dictionary.get(),
            kSecUseKeychain,
            Keychain.get());
        add_noninteractive_policy(dictionary.get());
        return dictionary;
    }

    [[nodiscard]] auto exact_query(CFStringRef account) const noexcept
        -> cf_owner<CFMutableDictionaryRef>
    {
        auto dictionary = make_mutable_dictionary();
        if(dictionary.get() == nullptr)
        {
            return {};
        }

        CFDictionarySetValue(
            dictionary.get(),
            kSecClass,
            kSecClassGenericPassword);
        CFDictionarySetValue(
            dictionary.get(),
            kSecAttrService,
            Service.get());
        CFDictionarySetValue(dictionary.get(), kSecAttrAccount, account);
        CFDictionarySetValue(
            dictionary.get(),
            kSecMatchSearchList,
            SearchList.get());
        add_noninteractive_policy(dictionary.get());
        return dictionary;
    }

    [[nodiscard]] static auto value_dictionary(CFDataRef value) noexcept
        -> cf_owner<CFMutableDictionaryRef>
    {
        auto dictionary = make_mutable_dictionary();
        if(dictionary.get() != nullptr)
        {
            CFDictionarySetValue(
                dictionary.get(),
                kSecValueData,
                value);
        }
        return dictionary;
    }

    cf_owner<SecKeychainRef> Keychain;
    cf_owner<CFArrayRef> SearchList;
    cf_owner<CFStringRef> Service;
    std::mutex OperationMutex;
};

[[nodiscard]] auto make_native_backend(
    const macos_keychain_service_scope scope) noexcept
    -> std::expected<
        std::shared_ptr<macos_keychain_backend>,
        application::github_credential_vault_failure>
{
    if(!valid(scope))
    {
        return std::unexpected{
            application::github_credential_vault_failure::invalid_request};
    }

    SecKeychainRef raw_keychain = nullptr;
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    const auto status = SecKeychainCopyDefault(&raw_keychain);
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    cf_owner<SecKeychainRef> keychain{raw_keychain};
    if(status != errSecSuccess)
    {
        return std::unexpected{mapped_failure(status)};
    }
    if(keychain.get() == nullptr)
    {
        return std::unexpected{
            application::github_credential_vault_failure::corrupt};
    }

    const void* values[]{keychain.get()};
    cf_owner<CFArrayRef> search_list{CFArrayCreate(
        kCFAllocatorDefault,
        values,
        1,
        &kCFTypeArrayCallBacks)};
    if(search_list.get() == nullptr)
    {
        return std::unexpected{
            application::github_credential_vault_failure::
                allocation_failure};
    }

    const auto service_identifier =
        macos_keychain_service_identifier(scope);
    auto service = make_cf_string(service_identifier);
    if(service.get() == nullptr)
    {
        return std::unexpected{
            application::github_credential_vault_failure::
                allocation_failure};
    }

    auto* backend = new(std::nothrow) native_macos_keychain_backend{
        keychain.release(),
        search_list.release(),
        service.release()};
    if(backend == nullptr)
    {
        return std::unexpected{
            application::github_credential_vault_failure::
                allocation_failure};
    }

    try
    {
        return std::shared_ptr<macos_keychain_backend>{backend};
    }
    catch(const std::bad_alloc&)
    {
        delete backend;
        return std::unexpected{
            application::github_credential_vault_failure::
                allocation_failure};
    }
    catch(...)
    {
        delete backend;
        return std::unexpected{
            application::github_credential_vault_failure::
                operation_failed};
    }
}

struct pending_store
{
    application::github_credential_enrollment Enrollment;
    application::github_credential_store_completion_handler Completion;
};

struct pending_retrieve
{
    application::github_credential_operation_context Context;
    application::github_credential_retrieve_completion_handler Completion;
};

struct pending_delete
{
    application::github_credential_operation_context Context;
    application::github_credential_delete_completion_handler Completion;
};

using pending_operation =
    std::variant<pending_store, pending_retrieve, pending_delete>;
using pending_operations = std::map<
    application::github_credential_operation_context,
    pending_operation>;

[[nodiscard]] auto extract_pending(
    pending_operations& operations,
    const application::github_credential_operation_context& context)
    -> std::optional<pending_operation>
{
    auto node = operations.extract(context);
    if(node.empty())
    {
        return std::nullopt;
    }
    return std::move(node.mapped());
}

void publish_store(
    pending_store& pending,
    application::github_credential_store_completion terminal)
{
    pending.Enrollment.Credential.reset();
    auto completion = std::move(pending.Completion);
    if(completion)
    {
        completion(std::move(terminal));
    }
}

void publish_retrieve(
    pending_retrieve& pending,
    application::github_credential_retrieve_completion terminal)
{
    auto completion = std::move(pending.Completion);
    if(completion)
    {
        completion(std::move(terminal));
    }
}

void publish_delete(
    pending_delete& pending,
    application::github_credential_delete_completion terminal)
{
    auto completion = std::move(pending.Completion);
    if(completion)
    {
        completion(std::move(terminal));
    }
}

void publish_failure(
    pending_operation& operation,
    const application::github_credential_vault_failure failure)
{
    const auto effective_failure = normalize_failure(failure);
    std::visit(
        overloaded{
            [effective_failure](pending_store& pending) {
                publish_store(
                    pending,
                    application::github_credential_store_error{
                        .Context = pending.Enrollment.Context,
                        .Failure = effective_failure,
                    });
            },
            [effective_failure](pending_retrieve& pending) {
                publish_retrieve(
                    pending,
                    application::github_credential_retrieve_error{
                        .Context = pending.Context,
                        .Failure = effective_failure,
                    });
            },
            [effective_failure](pending_delete& pending) {
                publish_delete(
                    pending,
                    application::github_credential_delete_error{
                        .Context = pending.Context,
                        .Failure = effective_failure,
                    });
            },
        },
        operation);
}

void publish_cancelled(pending_operation& operation)
{
    std::visit(
        overloaded{
            [](pending_store& pending) {
                publish_store(
                    pending,
                    application::github_credential_store_cancelled{
                        .Context = pending.Enrollment.Context,
                    });
            },
            [](pending_retrieve& pending) {
                publish_retrieve(
                    pending,
                    application::github_credential_retrieve_cancelled{
                        .Context = pending.Context,
                    });
            },
            [](pending_delete& pending) {
                publish_delete(
                    pending,
                    application::github_credential_delete_cancelled{
                        .Context = pending.Context,
                    });
            },
        },
        operation);
}

class macos_keychain_vault final
    : public application::github_credential_vault_port
{
private:
    struct shared_state
    {
        explicit shared_state(
            std::shared_ptr<macos_keychain_backend> backend) noexcept
            : Backend{std::move(backend)}
        {
        }

        std::mutex Mutex;
        pending_operations Pending;
        std::shared_ptr<macos_keychain_backend> Backend;
        bool Closing = false;
    };

public:
    macos_keychain_vault(
        application::effect_executor& effect_lane,
        std::shared_ptr<macos_keychain_backend> backend)
        : EffectLane{effect_lane}
        , State{std::make_shared<shared_state>(std::move(backend))}
    {
    }

    ~macos_keychain_vault() override
    {
        pending_operations cancelled;
        {
            const auto lock = std::scoped_lock{State->Mutex};
            State->Closing = true;
            cancelled.swap(State->Pending);
        }

        while(!cancelled.empty())
        {
            auto node = cancelled.extract(cancelled.begin());
            try
            {
                publish_cancelled(node.mapped());
            }
            catch(...)
            {
            }
        }
    }

    void store(
        application::github_credential_enrollment enrollment,
        application::github_credential_store_completion_handler completion)
        override
    {
        const auto context = enrollment.Context;
        if(!application::valid(enrollment) || !completion)
        {
            enrollment.Credential.reset();
            if(completion)
            {
                completion(application::github_credential_store_error{
                    .Context = context,
                    .Failure = application::
                        github_credential_vault_failure::invalid_request,
                });
            }
            return;
        }

        queue(
            context,
            pending_store{
                .Enrollment = std::move(enrollment),
                .Completion = std::move(completion),
            });
    }

    void retrieve(
        application::github_credential_operation_context context,
        application::github_credential_retrieve_completion_handler completion)
        override
    {
        if(!application::valid(context) || !completion)
        {
            if(completion)
            {
                completion(application::github_credential_retrieve_error{
                    .Context = context,
                    .Failure = application::
                        github_credential_vault_failure::invalid_request,
                });
            }
            return;
        }

        queue(
            context,
            pending_retrieve{
                .Context = context,
                .Completion = std::move(completion),
            });
    }

    void delete_credential(
        application::github_credential_operation_context context,
        application::github_credential_delete_completion_handler completion)
        override
    {
        if(!application::valid(context) || !completion)
        {
            if(completion)
            {
                completion(application::github_credential_delete_error{
                    .Context = context,
                    .Failure = application::
                        github_credential_vault_failure::invalid_request,
                });
            }
            return;
        }

        queue(
            context,
            pending_delete{
                .Context = context,
                .Completion = std::move(completion),
            });
    }

    void cancel(
        application::github_credential_operation_context context) override
    {
        std::optional<pending_operation> pending;
        {
            const auto lock = std::scoped_lock{State->Mutex};
            pending = extract_pending(State->Pending, context);
        }
        if(!pending.has_value())
        {
            return;
        }
        publish_cancelled(*pending);
    }

private:
    void queue(
        const application::github_credential_operation_context& context,
        pending_operation operation)
    {
        auto rejected = false;
        auto closing = false;
        try
        {
            const auto lock = std::scoped_lock{State->Mutex};
            closing = State->Closing;
            if(!closing)
            {
                rejected = !State->Pending.try_emplace(
                    context,
                    std::move(operation)).second;
            }
        }
        catch(const std::bad_alloc&)
        {
            publish_failure(
                operation,
                application::github_credential_vault_failure::
                    allocation_failure);
            return;
        }
        catch(...)
        {
            publish_failure(
                operation,
                application::github_credential_vault_failure::
                    operation_failed);
            return;
        }

        if(closing)
        {
            publish_failure(
                operation,
                application::github_credential_vault_failure::unavailable);
            return;
        }
        if(rejected)
        {
            publish_failure(
                operation,
                application::github_credential_vault_failure::
                    invalid_request);
            return;
        }

        const auto weak_state = std::weak_ptr<shared_state>{State};
        try
        {
            EffectLane.post([weak_state, context] {
                const auto state = weak_state.lock();
                if(state != nullptr)
                {
                    execute(state, context);
                }
            });
        }
        catch(const std::bad_alloc&)
        {
            fail_queued(
                context,
                application::github_credential_vault_failure::
                    allocation_failure);
        }
        catch(...)
        {
            fail_queued(
                context,
                application::github_credential_vault_failure::
                    operation_failed);
        }
    }

    void fail_queued(
        const application::github_credential_operation_context& context,
        const application::github_credential_vault_failure failure)
    {
        std::optional<pending_operation> pending;
        {
            const auto lock = std::scoped_lock{State->Mutex};
            pending = extract_pending(State->Pending, context);
        }
        if(!pending.has_value())
        {
            return;
        }
        publish_failure(*pending, failure);
    }

    static void execute(
        const std::shared_ptr<shared_state>& state,
        const application::github_credential_operation_context& context)
    {
        std::optional<pending_operation> pending;
        std::shared_ptr<macos_keychain_backend> backend;
        {
            const auto lock = std::scoped_lock{state->Mutex};
            pending = extract_pending(state->Pending, context);
            if(pending.has_value())
            {
                backend = state->Backend;
            }
        }
        if(!pending.has_value())
        {
            return;
        }

        std::visit(
            overloaded{
                [&backend](pending_store& operation) {
                    execute_store(backend, operation);
                },
                [&backend](pending_retrieve& operation) {
                    execute_retrieve(backend, operation);
                },
                [&backend](pending_delete& operation) {
                    execute_delete(backend, operation);
                },
            },
            *pending);
    }

    static void execute_store(
        const std::shared_ptr<macos_keychain_backend>& backend,
        pending_store& pending)
    {
        try
        {
            const auto result = backend->store(
                pending.Enrollment.Context.Authority,
                pending.Enrollment.Credential);
            if(result.has_value() && application::valid(*result))
            {
                publish_store(
                    pending,
                    application::github_credential_store_success{
                        .Context = pending.Enrollment.Context,
                        .Disposition = *result,
                    });
                return;
            }

            const auto failure =
                result.has_value()
                ? application::github_credential_vault_failure::
                    operation_failed
                : result.error();
            publish_store(
                pending,
                application::github_credential_store_error{
                    .Context = pending.Enrollment.Context,
                    .Failure = normalize_failure(failure),
                });
        }
        catch(const std::bad_alloc&)
        {
            publish_store(
                pending,
                application::github_credential_store_error{
                    .Context = pending.Enrollment.Context,
                    .Failure = application::
                        github_credential_vault_failure::allocation_failure,
                });
        }
        catch(...)
        {
            publish_store(
                pending,
                application::github_credential_store_error{
                    .Context = pending.Enrollment.Context,
                    .Failure = application::
                        github_credential_vault_failure::operation_failed,
                });
        }
    }

    static void execute_retrieve(
        const std::shared_ptr<macos_keychain_backend>& backend,
        pending_retrieve& pending)
    {
        try
        {
            auto result = backend->retrieve(pending.Context.Authority);
            std::visit(
                overloaded{
                    [&pending](
                        application::github_credential_material& credential) {
                        if(!application::valid(credential))
                        {
                            credential.reset();
                            publish_retrieve(
                                pending,
                                application::github_credential_retrieve_error{
                                    .Context = pending.Context,
                                    .Failure = application::
                                        github_credential_vault_failure::
                                            corrupt,
                                });
                            return;
                        }
                        publish_retrieve(
                            pending,
                            application::github_credential_retrieve_success{
                                .Context = pending.Context,
                                .Credential = std::move(credential),
                            });
                    },
                    [&pending](macos_keychain_credential_absent) {
                        publish_retrieve(
                            pending,
                            application::github_credential_retrieve_absent{
                                .Context = pending.Context,
                            });
                    },
                    [&pending](
                        const application::github_credential_vault_failure
                            failure) {
                        publish_retrieve(
                            pending,
                            application::github_credential_retrieve_error{
                                .Context = pending.Context,
                                .Failure = normalize_failure(failure),
                            });
                    },
                },
                result);
        }
        catch(const std::bad_alloc&)
        {
            publish_retrieve(
                pending,
                application::github_credential_retrieve_error{
                    .Context = pending.Context,
                    .Failure = application::
                        github_credential_vault_failure::allocation_failure,
                });
        }
        catch(...)
        {
            publish_retrieve(
                pending,
                application::github_credential_retrieve_error{
                    .Context = pending.Context,
                    .Failure = application::
                        github_credential_vault_failure::operation_failed,
                });
        }
    }

    static void execute_delete(
        const std::shared_ptr<macos_keychain_backend>& backend,
        pending_delete& pending)
    {
        try
        {
            const auto result =
                backend->delete_credential(pending.Context.Authority);
            if(result.has_value() && application::valid(*result))
            {
                publish_delete(
                    pending,
                    application::github_credential_delete_success{
                        .Context = pending.Context,
                        .Disposition = *result,
                    });
                return;
            }

            const auto failure =
                result.has_value()
                ? application::github_credential_vault_failure::
                    operation_failed
                : result.error();
            publish_delete(
                pending,
                application::github_credential_delete_error{
                    .Context = pending.Context,
                    .Failure = normalize_failure(failure),
                });
        }
        catch(const std::bad_alloc&)
        {
            publish_delete(
                pending,
                application::github_credential_delete_error{
                    .Context = pending.Context,
                    .Failure = application::
                        github_credential_vault_failure::allocation_failure,
                });
        }
        catch(...)
        {
            publish_delete(
                pending,
                application::github_credential_delete_error{
                    .Context = pending.Context,
                    .Failure = application::
                        github_credential_vault_failure::operation_failed,
                });
        }
    }

    application::effect_executor& EffectLane;
    std::shared_ptr<shared_state> State;
};

[[nodiscard]] auto make_vault(
    application::effect_executor& effect_lane,
    std::shared_ptr<macos_keychain_backend> backend) noexcept
    -> macos_keychain_vault_factory_result
{
    if(backend == nullptr)
    {
        return std::unexpected{
            application::github_credential_vault_failure::invalid_request};
    }

    try
    {
        return std::unique_ptr<application::github_credential_vault_port>{
            std::make_unique<macos_keychain_vault>(
                effect_lane,
                std::move(backend))};
    }
    catch(const std::bad_alloc&)
    {
        return std::unexpected{
            application::github_credential_vault_failure::
                allocation_failure};
    }
    catch(...)
    {
        return std::unexpected{
            application::github_credential_vault_failure::
                operation_failed};
    }
}
}

auto macos_keychain_service_identifier(
    const macos_keychain_service_scope scope) noexcept -> std::string_view
{
    switch(scope)
    {
    case macos_keychain_service_scope::production:
        return production_service;
    case macos_keychain_service_scope::integration_test:
        return integration_test_service;
    }
    return {};
}

auto encode_macos_keychain_authority_account(
    const application::github_credential_authority_key& authority) noexcept
    -> std::expected<
        macos_keychain_authority_account,
        application::github_credential_vault_failure>
{
    if(!application::valid(authority))
    {
        return std::unexpected{
            application::github_credential_vault_failure::invalid_request};
    }

    macos_keychain_authority_account encoded{};
    std::size_t offset = 0U;
    append_hex_u64(encoded, offset, authority.Profile.value());
    append_hex_u64(encoded, offset, authority.ProfileRevision.value());
    append_hex_u64(encoded, offset, authority.Host.value());
    append_hex_u64(encoded, offset, authority.Account.value());
    append_hex_u8(
        encoded,
        offset,
        server_contract_discriminator(authority.ServerContract));
    const auto hostname = authority.Hostname.value();
    append_hex_u8(
        encoded, offset, static_cast<std::uint8_t>(hostname.size()));
    for(std::size_t index = 0U;
        index < application::maximum_github_hostname_bytes;
        ++index)
    {
        const std::uint8_t value =
            index < hostname.size()
            ? static_cast<std::uint8_t>(
                  static_cast<unsigned char>(hostname[index]))
            : std::uint8_t{0U};
        append_hex_u8(encoded, offset, value);
    }
    return encoded;
}

auto classify_macos_keychain_status(const std::int32_t status) noexcept
    -> macos_keychain_status_mapping
{
    switch(status)
    {
    case errSecSuccess:
        return {
            .Kind = macos_keychain_status_kind::success,
            .Failure =
                application::github_credential_vault_failure::none,
        };
    case errSecItemNotFound:
        return {
            .Kind = macos_keychain_status_kind::item_not_found,
            .Failure =
                application::github_credential_vault_failure::none,
        };
    case errSecDuplicateItem:
        return {
            .Kind = macos_keychain_status_kind::duplicate_item,
            .Failure =
                application::github_credential_vault_failure::none,
        };
    case errSecNotAvailable:
    case errSecNoDefaultKeychain:
    case errSecNoSuchKeychain:
    case errSecNoStorageModule:
        return failure_mapping(
            application::github_credential_vault_failure::unavailable);
    case errSecInteractionNotAllowed:
    case errSecInteractionRequired:
    case errSecInDarkWake:
        return failure_mapping(
            application::github_credential_vault_failure::locked);
    case errSecAuthFailed:
    case errSecUserCanceled:
    case errSecWrPerm:
    case errSecReadOnly:
    case errSecReadOnlyAttr:
    case errSecMissingEntitlement:
    case errSecRestrictedAPI:
    case errSecNoAccessForItem:
    case errSecInvalidOwnerEdit:
    case errSecDataNotModifiable:
        return failure_mapping(
            application::github_credential_vault_failure::access_denied);
    case errSecDecode:
    case errSecInvalidKeychain:
    case errSecDataNotAvailable:
        return failure_mapping(
            application::github_credential_vault_failure::corrupt);
    case errSecAllocate:
        return failure_mapping(
            application::github_credential_vault_failure::
                allocation_failure);
    case errSecParam:
    case errSecBadReq:
        return failure_mapping(
            application::github_credential_vault_failure::
                invalid_request);
    default:
        return failure_mapping(
            application::github_credential_vault_failure::
                operation_failed);
    }
}

auto make_macos_keychain_vault(
    application::effect_executor& effect_lane)
    -> macos_keychain_vault_factory_result
{
    auto backend =
        make_native_backend(macos_keychain_service_scope::production);
    if(!backend.has_value())
    {
        return std::unexpected{backend.error()};
    }
    return make_vault(effect_lane, std::move(*backend));
}

auto make_macos_keychain_integration_test_vault(
    application::effect_executor& effect_lane)
    -> macos_keychain_vault_factory_result
{
    auto backend =
        make_native_backend(macos_keychain_service_scope::integration_test);
    if(!backend.has_value())
    {
        return std::unexpected{backend.error()};
    }
    return make_vault(effect_lane, std::move(*backend));
}

auto make_macos_keychain_vault_with_backend(
    application::effect_executor& effect_lane,
    std::shared_ptr<macos_keychain_backend> backend) noexcept
    -> macos_keychain_vault_factory_result
{
    return make_vault(effect_lane, std::move(backend));
}
}
