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

namespace
{
using namespace bpr;

using authority_key =
    application::github_credential_authority_key;
using operation_context =
    application::github_credential_operation_context;
using credential_material =
    application::github_credential_material;
using credential_vault_port =
    application::github_credential_vault_port;

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

auto expect(const bool condition, const std::string_view message) -> void
{
    if(!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

auto runtime_bytes(
    const std::size_t size,
    const std::uint8_t seed) -> std::vector<std::byte>
{
    auto bytes = std::vector<std::byte>(size);
    auto value = seed;
    for(auto& byte : bytes)
    {
        value = static_cast<std::uint8_t>(
            static_cast<std::uint16_t>(value) * 29U + 17U);
        byte = static_cast<std::byte>(value);
    }
    return bytes;
}

auto same_bytes(
    const std::span<const std::byte> actual,
    const std::span<const std::byte> expected) -> bool
{
    return std::ranges::equal(actual, expected);
}

auto material_from(const std::span<const std::byte> bytes)
    -> credential_material
{
    auto material = credential_material::create(bytes);
    expect(material.has_value(), "valid credential material is created");
    return std::move(*material);
}

auto authority(
    const std::uint64_t profile = 1U,
    const std::uint64_t profile_revision = 2U,
    const std::uint64_t host = 3U,
    const std::uint64_t account = 4U,
    const application::github_server_contract server =
        application::github_server_contract::
            github_com_2022_11_28,
    const std::string_view hostname = {}) -> authority_key
{
    return {
        .Profile = core::host_profile_id{profile},
        .ProfileRevision =
            application::github_profile_revision{profile_revision},
        .Host = core::git_host_id{host},
        .Hostname = application::github_canonical_hostname{
            hostname.empty()
                ? (server
                           == application::github_server_contract::
                               github_com_2022_11_28
                       ? "github.com"
                       : "ghe.example.invalid")
                : hostname},
        .Account = core::github_account_id{account},
        .ServerContract = server,
    };
}

auto context(
    const authority_key& key = authority(),
    const std::uint64_t request = 10U,
    const std::uint64_t session = 11U,
    const std::uint64_t generation = 12U) -> operation_context
{
    return {
        .Request = core::request_id{request},
        .ApplicationSession = core::application_session_id{session},
        .Authority = key,
        .IdentityGeneration =
            core::github_identity_generation{generation},
    };
}

class queued_fake_vault final
    : public application::github_credential_vault_port
{
public:
    void store(
        application::github_credential_enrollment enrollment,
        application::github_credential_store_completion_handler completion)
        override
    {
        if(!valid(enrollment) || pending(enrollment.Context))
        {
            publish_store_without_credential(
                enrollment.Credential,
                completion,
                application::github_credential_store_error{
                    .Context = enrollment.Context,
                    .Failure =
                        application::github_credential_vault_failure::
                            invalid_request,
                });
            return;
        }

        PendingStores.push_back(pending_store{
            .Enrollment = std::move(enrollment),
            .Completion = std::move(completion),
        });
    }

    void retrieve(
        operation_context operation,
        application::github_credential_retrieve_completion_handler
            completion) override
    {
        if(!valid(operation) || pending(operation))
        {
            completion(application::github_credential_retrieve_error{
                .Context = operation,
                .Failure =
                    application::github_credential_vault_failure::
                        invalid_request,
            });
            return;
        }

        PendingRetrieves.push_back(pending_retrieve{
            .Context = operation,
            .Completion = std::move(completion),
        });
    }

    void delete_credential(
        operation_context operation,
        application::github_credential_delete_completion_handler completion)
        override
    {
        if(!valid(operation) || pending(operation))
        {
            completion(application::github_credential_delete_error{
                .Context = operation,
                .Failure =
                    application::github_credential_vault_failure::
                        invalid_request,
            });
            return;
        }

        PendingDeletes.push_back(pending_delete{
            .Context = operation,
            .Completion = std::move(completion),
        });
    }

    void cancel(const operation_context operation) override
    {
        const auto store = find_store(operation);
        if(store != PendingStores.end())
        {
            auto pending_operation = std::move(*store);
            PendingStores.erase(store);
            publish_store_without_credential(
                pending_operation.Enrollment.Credential,
                pending_operation.Completion,
                application::github_credential_store_cancelled{
                    .Context = pending_operation.Enrollment.Context,
                });
            return;
        }

        const auto retrieve = find_retrieve(operation);
        if(retrieve != PendingRetrieves.end())
        {
            auto pending_operation = std::move(*retrieve);
            PendingRetrieves.erase(retrieve);
            pending_operation.Completion(
                application::github_credential_retrieve_cancelled{
                    .Context = pending_operation.Context,
                });
            return;
        }

        const auto deletion = find_delete(operation);
        if(deletion != PendingDeletes.end())
        {
            auto pending_operation = std::move(*deletion);
            PendingDeletes.erase(deletion);
            pending_operation.Completion(
                application::github_credential_delete_cancelled{
                    .Context = pending_operation.Context,
                });
        }
    }

    [[nodiscard]] auto complete_store(
        const operation_context operation,
        const application::github_credential_vault_failure failure =
            application::github_credential_vault_failure::none) -> bool
    {
        const auto found = find_store(operation);
        if(found == PendingStores.end())
        {
            return false;
        }

        auto pending_operation = std::move(*found);
        PendingStores.erase(found);
        if(failure != application::github_credential_vault_failure::none)
        {
            publish_store_without_credential(
                pending_operation.Enrollment.Credential,
                pending_operation.Completion,
                application::github_credential_store_error{
                    .Context = pending_operation.Enrollment.Context,
                    .Failure = failure,
                });
            return true;
        }

        Stored.insert_or_assign(
            pending_operation.Enrollment.Context.Authority,
            std::move(pending_operation.Enrollment.Credential));
        pending_operation.Completion(
            application::github_credential_store_success{
                .Context = pending_operation.Enrollment.Context,
                .Disposition =
                    application::github_credential_store_disposition::
                        stored,
            });
        return true;
    }

    [[nodiscard]] auto complete_retrieve(
        const operation_context operation,
        const application::github_credential_vault_failure failure =
            application::github_credential_vault_failure::none) -> bool
    {
        const auto found = find_retrieve(operation);
        if(found == PendingRetrieves.end())
        {
            return false;
        }

        auto pending_operation = std::move(*found);
        PendingRetrieves.erase(found);
        if(failure != application::github_credential_vault_failure::none)
        {
            pending_operation.Completion(
                application::github_credential_retrieve_error{
                    .Context = pending_operation.Context,
                    .Failure = failure,
                });
            return true;
        }

        const auto stored = Stored.find(operation.Authority);
        if(stored == Stored.end())
        {
            pending_operation.Completion(
                application::github_credential_retrieve_absent{
                    .Context = pending_operation.Context,
                });
            return true;
        }

        auto credential =
            credential_material::create(stored->second.bytes());
        if(!credential.has_value())
        {
            pending_operation.Completion(
                application::github_credential_retrieve_error{
                    .Context = pending_operation.Context,
                    .Failure =
                        application::github_credential_vault_failure::
                            allocation_failure,
                });
            return true;
        }

        pending_operation.Completion(
            application::github_credential_retrieve_success{
                .Context = pending_operation.Context,
                .Credential = std::move(*credential),
            });
        return true;
    }

    [[nodiscard]] auto complete_delete(
        const operation_context operation,
        const application::github_credential_vault_failure failure =
            application::github_credential_vault_failure::none) -> bool
    {
        const auto found = find_delete(operation);
        if(found == PendingDeletes.end())
        {
            return false;
        }

        auto pending_operation = std::move(*found);
        PendingDeletes.erase(found);
        if(failure != application::github_credential_vault_failure::none)
        {
            pending_operation.Completion(
                application::github_credential_delete_error{
                    .Context = pending_operation.Context,
                    .Failure = failure,
                });
            return true;
        }

        const auto erased = Stored.erase(operation.Authority);
        pending_operation.Completion(
            application::github_credential_delete_success{
                .Context = pending_operation.Context,
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
        const authority_key& key,
        const std::span<const std::byte> expected) const -> bool
    {
        const auto found = Stored.find(key);
        return found != Stored.end()
            && same_bytes(found->second.bytes(), expected);
    }

private:
    static void publish_store_without_credential(
        credential_material& credential,
        application::github_credential_store_completion_handler& completion,
        application::github_credential_store_completion terminal)
    {
        credential.reset();
        completion(std::move(terminal));
    }

    struct pending_store
    {
        application::github_credential_enrollment Enrollment;
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
        operation_context Context;
        application::github_credential_delete_completion_handler Completion;
    };

    using store_iterator = std::vector<pending_store>::iterator;
    using retrieve_iterator = std::vector<pending_retrieve>::iterator;
    using delete_iterator = std::vector<pending_delete>::iterator;

    [[nodiscard]] auto find_store(
        const operation_context& operation) -> store_iterator
    {
        return std::ranges::find_if(
            PendingStores,
            [&operation](const pending_store& pending_operation) {
                return pending_operation.Enrollment.Context == operation;
            });
    }

    [[nodiscard]] auto find_retrieve(
        const operation_context& operation) -> retrieve_iterator
    {
        return std::ranges::find_if(
            PendingRetrieves,
            [&operation](const pending_retrieve& pending_operation) {
                return pending_operation.Context == operation;
            });
    }

    [[nodiscard]] auto find_delete(
        const operation_context& operation) -> delete_iterator
    {
        return std::ranges::find_if(
            PendingDeletes,
            [&operation](const pending_delete& pending_operation) {
                return pending_operation.Context == operation;
            });
    }

    [[nodiscard]] auto pending(
        const operation_context& operation) -> bool
    {
        return find_store(operation) != PendingStores.end()
            || find_retrieve(operation) != PendingRetrieves.end()
            || find_delete(operation) != PendingDeletes.end();
    }

    std::map<authority_key, credential_material> Stored;
    std::vector<pending_store> PendingStores;
    std::vector<pending_retrieve> PendingRetrieves;
    std::vector<pending_delete> PendingDeletes;
};

template<typename Completion>
struct completion_observation
{
    std::size_t Calls = 0U;
    std::optional<Completion> Terminal;
};

using store_observation = completion_observation<
    application::github_credential_store_completion>;
using retrieve_observation = completion_observation<
    application::github_credential_retrieve_completion>;
using delete_observation = completion_observation<
    application::github_credential_delete_completion>;

template<typename Completion>
auto observe_completion(completion_observation<Completion>& observation)
    -> std::function<void(Completion)>
{
    return [&observation](Completion terminal) {
        ++observation.Calls;
        observation.Terminal.emplace(std::move(terminal));
    };
}

auto observe_successful_retrieve(
    retrieve_observation& observation,
    std::vector<operation_context>& completion_order,
    const std::string_view failure_message)
    -> application::github_credential_retrieve_completion_handler
{
    return [&observation, &completion_order, failure_message](
               application::github_credential_retrieve_completion terminal) {
        const auto* success =
            std::get_if<application::github_credential_retrieve_success>(
                &terminal);
        expect(success != nullptr, failure_message);
        completion_order.push_back(success->Context);
        ++observation.Calls;
        observation.Terminal.emplace(std::move(terminal));
    };
}

void queue_store(
    queued_fake_vault& vault,
    const operation_context operation,
    const std::span<const std::byte> bytes,
    store_observation& observation)
{
    vault.store(
        application::github_credential_enrollment{
            .Context = operation,
            .Credential = material_from(bytes),
        },
        observe_completion(observation));
}

template<typename Alternative, typename Completion>
auto terminal_as(
    const completion_observation<Completion>& observation)
    -> const Alternative*
{
    if(!observation.Terminal.has_value())
    {
        return nullptr;
    }
    return std::get_if<Alternative>(&*observation.Terminal);
}

auto store_success(
    const store_observation& observation)
    -> const application::github_credential_store_success*
{
    return terminal_as<application::github_credential_store_success>(
        observation);
}

auto retrieve_success(
    const retrieve_observation& observation)
    -> const application::github_credential_retrieve_success*
{
    return terminal_as<application::github_credential_retrieve_success>(
        observation);
}

auto delete_success(
    const delete_observation& observation)
    -> const application::github_credential_delete_success*
{
    return terminal_as<application::github_credential_delete_success>(
        observation);
}

void test_material_contract()
{
    const auto empty = credential_material::create({});
    expect(!empty.has_value(), "empty credential material is rejected");
    expect(
        empty.error()
            == application::github_credential_material_error::empty,
        "empty material reports its bounded error");

    const auto maximum = runtime_bytes(
        application::maximum_github_credential_bytes,
        1U);
    auto at_limit = credential_material::create(maximum);
    expect(at_limit.has_value(), "credential material accepts the limit");
    expect(valid(*at_limit), "at-limit credential material is valid");
    expect(
        at_limit->size() == maximum.size(),
        "credential material reports its exact size");
    expect(
        same_bytes(at_limit->bytes(), maximum),
        "credential material exposes exact read-only bytes");

    const auto oversized = runtime_bytes(
        application::maximum_github_credential_bytes + 1U,
        2U);
    const auto too_large = credential_material::create(oversized);
    expect(!too_large.has_value(), "oversized credential is rejected");
    expect(
        too_large.error()
            == application::github_credential_material_error::too_large,
        "oversized material reports its bounded error");

    const auto first_bytes = runtime_bytes(31U, 3U);
    auto first = material_from(first_bytes);
    auto moved = std::move(first);
    expect(first.empty(), "move construction empties the source");
    expect(!valid(first), "moved-from material is invalid");
    expect(
        same_bytes(moved.bytes(), first_bytes),
        "move construction preserves exact bytes");

    const auto second_bytes = runtime_bytes(29U, 4U);
    auto second = material_from(second_bytes);
    second = std::move(moved);
    expect(moved.empty(), "move assignment empties the source");
    expect(
        same_bytes(second.bytes(), first_bytes),
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
    const auto maximum_hostname =
        std::string(63U, 'a') + "." + std::string(63U, 'b') + "."
        + std::string(63U, 'c') + "." + std::string(61U, 'd');
    const application::github_canonical_hostname owned_maximum{
        maximum_hostname};
    expect(
        maximum_hostname.size()
                == application::maximum_github_hostname_bytes
            && owned_maximum.valid()
            && owned_maximum.value() == maximum_hostname,
        "maximum canonical hostname is retained byte-exactly");
    const auto oversized_hostname = maximum_hostname + "e";
    const application::github_canonical_hostname rejected_oversized{
        oversized_hostname};
    const application::github_canonical_hostname rejected_noncanonical{
        "GitHub.com"};
    expect(
        oversized_hostname.size()
                == application::maximum_github_hostname_bytes + 1U
            && !rejected_oversized.valid()
            && rejected_oversized.value().empty()
            && !rejected_noncanonical.valid()
            && rejected_noncanonical.value().empty(),
        "oversized and non-canonical hostnames have no owned authority");

    const auto base_authority = authority();
    const auto base_context = context(base_authority);
    expect(valid(base_authority), "complete credential authority is valid");
    expect(valid(base_context), "complete operation context is valid");

    auto invalid_authority = base_authority;
    invalid_authority.Profile = core::host_profile_id{};
    expect(!valid(invalid_authority), "zero profile is rejected");

    invalid_authority = base_authority;
    invalid_authority.ProfileRevision =
        application::github_profile_revision{};
    expect(!valid(invalid_authority), "zero profile revision is rejected");

    invalid_authority = base_authority;
    invalid_authority.Host = core::git_host_id{};
    expect(!valid(invalid_authority), "zero host is rejected");

    invalid_authority = base_authority;
    invalid_authority.Hostname = {};
    expect(!valid(invalid_authority), "empty hostname is rejected");

    invalid_authority = base_authority;
    invalid_authority.Hostname =
        application::github_canonical_hostname{"GitHub.com"};
    expect(!valid(invalid_authority), "non-canonical hostname is rejected");

    const auto enterprise_authority = authority(
        1U,
        2U,
        3U,
        4U,
        application::github_server_contract::
            ghes_3_18_2022_11_28);
    auto different_hostname = enterprise_authority;
    different_hostname.Hostname =
        application::github_canonical_hostname{"ghe.other.invalid"};
    expect(
        valid(enterprise_authority) && valid(different_hostname)
            && different_hostname != enterprise_authority,
        "canonical hostname is an exact credential authority dimension");
    invalid_authority = base_authority;
    invalid_authority.Hostname =
        application::github_canonical_hostname{"ghe.example.invalid"};
    expect(
        !valid(invalid_authority),
        "GitHub.com contract rejects an enterprise hostname");
    invalid_authority = enterprise_authority;
    invalid_authority.Hostname =
        application::github_canonical_hostname{"api.github.com"};
    expect(
        !valid(invalid_authority),
        "GHES contract rejects a GitHub.com subdomain");

    invalid_authority = base_authority;
    invalid_authority.Account = core::github_account_id{};
    expect(!valid(invalid_authority), "zero account is rejected");

    invalid_authority = base_authority;
    invalid_authority.ServerContract =
        static_cast<application::github_server_contract>(0xffU);
    expect(!valid(invalid_authority), "malformed server contract is rejected");

    auto invalid_context = base_context;
    invalid_context.Request = core::request_id{};
    expect(!valid(invalid_context), "zero request is rejected");

    invalid_context = base_context;
    invalid_context.ApplicationSession = core::application_session_id{};
    expect(!valid(invalid_context), "zero session is rejected");

    invalid_context = base_context;
    invalid_context.Authority = invalid_authority;
    expect(!valid(invalid_context), "invalid authority is rejected in context");

    invalid_context = base_context;
    invalid_context.IdentityGeneration =
        core::github_identity_generation{};
    expect(!valid(invalid_context), "zero identity generation is rejected");

    const auto bytes = runtime_bytes(8U, 5U);
    auto enrollment = application::github_credential_enrollment{
        .Context = base_context,
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
    using delete_error = application::github_credential_delete_error;
    using retrieve_error = application::github_credential_retrieve_error;
    using store_error = application::github_credential_store_error;

    queued_fake_vault vault;
    const auto key = authority();
    const auto stored_bytes = runtime_bytes(18U, 6U);
    const auto replacement_bytes = runtime_bytes(19U, 7U);
    const auto initial = context(key, 20U, 21U, 22U);
    store_observation initial_store;
    queue_store(vault, initial, stored_bytes, initial_store);
    expect(vault.complete_store(initial), "invalid-operation fixture is stored");

    const auto invalid_store_context = context(key, 0U, 21U, 22U);
    store_observation invalid_store;
    queue_store(
        vault,
        invalid_store_context,
        replacement_bytes,
        invalid_store);
    const auto* rejected_store = terminal_as<store_error>(invalid_store);
    expect(
        invalid_store.Calls == 1U && rejected_store != nullptr,
        "store rejects an invalid context once");
    expect(
        rejected_store->Context == invalid_store_context
            && rejected_store->Failure
                == application::github_credential_vault_failure::
                    invalid_request,
        "store rejection preserves the supplied invalid context");
    expect(
        !valid(*invalid_store.Terminal),
        "a terminal echoing invalid store context remains invalid");

    auto invalid_material = material_from(replacement_bytes);
    invalid_material.reset();
    const auto invalid_material_context =
        context(key, 23U, 21U, 22U);
    store_observation invalid_material_store;
    vault.store(
        application::github_credential_enrollment{
            .Context = invalid_material_context,
            .Credential = std::move(invalid_material),
        },
        observe_completion(invalid_material_store));
    expect(
        invalid_material_store.Calls == 1U
            && terminal_as<store_error>(invalid_material_store) != nullptr,
        "store rejects invalid credential material once");
    expect(
        valid(*invalid_material_store.Terminal),
        "material rejection publishes a valid bounded terminal");

    const auto invalid_retrieve_context =
        context(key, 24U, 0U, 22U);
    retrieve_observation invalid_retrieve;
    vault.retrieve(
        invalid_retrieve_context,
        observe_completion(invalid_retrieve));
    const auto* rejected_retrieve =
        terminal_as<retrieve_error>(invalid_retrieve);
    expect(
        invalid_retrieve.Calls == 1U && rejected_retrieve != nullptr,
        "retrieve rejects an invalid context once");
    expect(
        rejected_retrieve->Context == invalid_retrieve_context
            && rejected_retrieve->Failure
                == application::github_credential_vault_failure::
                    invalid_request,
        "retrieve rejection preserves the supplied invalid context");
    expect(
        !valid(*invalid_retrieve.Terminal),
        "a terminal echoing invalid retrieve context remains invalid");

    const auto invalid_delete_context =
        context(key, 25U, 21U, 0U);
    delete_observation invalid_delete;
    vault.delete_credential(
        invalid_delete_context,
        observe_completion(invalid_delete));
    const auto* rejected_delete = terminal_as<delete_error>(invalid_delete);
    expect(
        invalid_delete.Calls == 1U && rejected_delete != nullptr,
        "delete rejects an invalid context once");
    expect(
        rejected_delete->Context == invalid_delete_context
            && rejected_delete->Failure
                == application::github_credential_vault_failure::
                    invalid_request,
        "delete rejection preserves the supplied invalid context");
    expect(
        !valid(*invalid_delete.Terminal),
        "a terminal echoing invalid delete context remains invalid");

    expect(vault.pending_count() == 0U, "invalid operations queue no work");
    expect(
        vault.stored_count() == 1U
            && vault.stored_matches(key, stored_bytes),
        "invalid operations do not mutate stored credential material");
}

void test_store_retrieve_replace_and_delete()
{
    queued_fake_vault vault;
    const auto operation = context();
    const auto first_bytes = runtime_bytes(23U, 6U);
    const auto replacement_bytes = runtime_bytes(27U, 7U);

    store_observation first_store;
    queue_store(vault, operation, first_bytes, first_store);
    expect(first_store.Calls == 0U, "store completion is queued");
    expect(vault.complete_store(operation), "queued store completes");
    expect(first_store.Calls == 1U, "store has one terminal completion");
    const auto* stored = store_success(first_store);
    expect(stored != nullptr, "store succeeds");
    expect(stored->Context == operation, "store preserves exact context");
    expect(valid(*first_store.Terminal), "store terminal is valid");
    expect(
        vault.stored_matches(operation.Authority, first_bytes),
        "store persists exact bytes");

    retrieve_observation first_retrieve;
    vault.retrieve(operation, observe_completion(first_retrieve));
    expect(vault.complete_retrieve(operation), "queued retrieval completes");
    const auto* retrieved = retrieve_success(first_retrieve);
    expect(retrieved != nullptr, "stored credential is retrieved");
    expect(retrieved->Context == operation, "retrieve preserves context");
    expect(
        same_bytes(retrieved->Credential.bytes(), first_bytes),
        "retrieve returns exact stored bytes");
    expect(valid(*first_retrieve.Terminal), "retrieve terminal is valid");

    const auto replacement_operation =
        context(operation.Authority, 13U, 11U, 12U);
    store_observation replacement_store;
    queue_store(
        vault,
        replacement_operation,
        replacement_bytes,
        replacement_store);
    expect(
        vault.complete_store(replacement_operation),
        "replacement store completes");
    expect(
        vault.stored_count() == 1U,
        "replacement preserves one authority record");
    expect(
        vault.stored_matches(operation.Authority, replacement_bytes),
        "second store atomically replaces authority material");

    const auto absent_operation =
        context(authority(91U), 14U, 11U, 12U);
    retrieve_observation absent_retrieve;
    vault.retrieve(absent_operation, observe_completion(absent_retrieve));
    expect(
        vault.complete_retrieve(absent_operation),
        "absent retrieval completes");
    const auto* absent =
        std::get_if<application::github_credential_retrieve_absent>(
            &*absent_retrieve.Terminal);
    expect(absent != nullptr, "missing authority reports explicit absence");
    expect(absent->Context == absent_operation, "absence preserves context");

    const auto delete_operation =
        context(operation.Authority, 15U, 11U, 12U);
    delete_observation deletion;
    vault.delete_credential(delete_operation, observe_completion(deletion));
    expect(vault.complete_delete(delete_operation), "delete completes");
    const auto* deleted = delete_success(deletion);
    expect(deleted != nullptr, "existing credential delete succeeds");
    expect(
        deleted->Disposition
            == application::github_credential_delete_disposition::deleted,
        "existing credential reports deleted");
    expect(vault.stored_count() == 0U, "delete removes authority material");

    const auto absent_delete_operation =
        context(operation.Authority, 16U, 11U, 12U);
    delete_observation absent_deletion;
    vault.delete_credential(
        absent_delete_operation,
        observe_completion(absent_deletion));
    expect(
        vault.complete_delete(absent_delete_operation),
        "absent delete completes");
    const auto* absent_delete = delete_success(absent_deletion);
    expect(absent_delete != nullptr, "absent delete is successful");
    expect(
        absent_delete->Disposition
            == application::github_credential_delete_disposition::absent,
        "absent delete reports absence");
}

void test_authority_isolation()
{
    queued_fake_vault vault;
    const auto authorities = std::vector<authority_key>{
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

    auto credentials = std::vector<std::vector<std::byte>>{};
    credentials.reserve(authorities.size());
    for(std::size_t index = 0U; index < authorities.size(); ++index)
    {
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

    for(std::size_t index = 0U; index < authorities.size(); ++index)
    {
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
    const auto key = authority();
    const auto bytes = runtime_bytes(19U, 41U);
    const auto initial = context(key, 500U, 501U, 502U);
    store_observation stored;
    queue_store(vault, initial, bytes, stored);
    expect(vault.complete_store(initial), "correlation fixture is stored");

    const auto first = context(key, 510U, 501U, 502U);
    const auto request_sibling = context(key, 511U, 501U, 502U);
    const auto session_sibling = context(key, 510U, 512U, 502U);
    const auto generation_sibling = context(key, 510U, 501U, 513U);

    retrieve_observation first_observation;
    retrieve_observation request_observation;
    retrieve_observation session_observation;
    retrieve_observation generation_observation;
    auto completion_order = std::vector<operation_context>{};

    vault.retrieve(
        first,
        observe_successful_retrieve(
            first_observation,
            completion_order,
            "first correlated retrieval succeeds"));
    vault.retrieve(
        request_sibling,
        observe_successful_retrieve(
            request_observation,
            completion_order,
            "request sibling retrieval succeeds"));
    vault.retrieve(
        session_sibling,
        observe_successful_retrieve(
            session_observation,
            completion_order,
            "session sibling retrieval succeeds"));
    vault.retrieve(
        generation_sibling,
        observe_successful_retrieve(
            generation_observation,
            completion_order,
            "generation sibling retrieval succeeds"));

    expect(
        !vault.complete_retrieve(context(key, 999U, 501U, 502U)),
        "unknown correlation does not complete a sibling");
    expect(
        vault.complete_retrieve(generation_sibling),
        "generation sibling completes out of order");
    expect(
        vault.complete_retrieve(request_sibling),
        "request sibling completes out of order");
    expect(
        vault.complete_retrieve(first),
        "first request completes out of order");
    expect(
        vault.complete_retrieve(session_sibling),
        "session sibling completes out of order");

    expect(completion_order.size() == 4U, "all reordered operations complete");
    expect(
        completion_order[0] == generation_sibling
            && completion_order[1] == request_sibling
            && completion_order[2] == first
            && completion_order[3] == session_sibling,
        "terminal order follows selected exact contexts");
    expect(
        retrieve_success(first_observation)->Context == first,
        "request context is exactly correlated");
    expect(
        retrieve_success(request_observation)->Context == request_sibling,
        "request identity is exactly correlated");
    expect(
        retrieve_success(session_observation)->Context == session_sibling,
        "session identity is exactly correlated");
    expect(
        retrieve_success(generation_observation)->Context
            == generation_sibling,
        "identity generation is exactly correlated");
}

void test_failures_preserve_state()
{
    queued_fake_vault vault;
    const auto key = authority();
    const auto old_bytes = runtime_bytes(21U, 51U);
    const auto new_bytes = runtime_bytes(22U, 52U);
    const auto initial = context(key, 600U, 601U, 602U);
    store_observation initial_store;
    queue_store(vault, initial, old_bytes, initial_store);
    expect(vault.complete_store(initial), "failure fixture is stored");

    const auto failed_store_operation =
        context(key, 610U, 601U, 602U);
    store_observation failed_store;
    queue_store(
        vault,
        failed_store_operation,
        new_bytes,
        failed_store);
    expect(
        vault.complete_store(
            failed_store_operation,
            application::github_credential_vault_failure::
                operation_failed),
        "injected store failure completes");
    const auto* store_error =
        std::get_if<application::github_credential_store_error>(
            &*failed_store.Terminal);
    expect(store_error != nullptr, "store failure is explicit");
    expect(
        store_error->Failure
            == application::github_credential_vault_failure::
                operation_failed,
        "store failure preserves bounded reason");
    expect(
        vault.stored_matches(key, old_bytes),
        "failed replacement preserves prior material");

    const auto failed_retrieve_operation =
        context(key, 611U, 601U, 602U);
    retrieve_observation failed_retrieve;
    vault.retrieve(
        failed_retrieve_operation,
        observe_completion(failed_retrieve));
    expect(
        vault.complete_retrieve(
            failed_retrieve_operation,
            application::github_credential_vault_failure::locked),
        "injected retrieve failure completes");
    const auto* retrieve_error =
        std::get_if<application::github_credential_retrieve_error>(
            &*failed_retrieve.Terminal);
    expect(retrieve_error != nullptr, "retrieve failure is explicit");
    expect(
        !std::holds_alternative<
            application::github_credential_retrieve_success>(
            *failed_retrieve.Terminal),
        "retrieve failure exposes no credential material");

    const auto failed_delete_operation =
        context(key, 612U, 601U, 602U);
    delete_observation failed_delete;
    vault.delete_credential(
        failed_delete_operation,
        observe_completion(failed_delete));
    expect(
        vault.complete_delete(
            failed_delete_operation,
            application::github_credential_vault_failure::access_denied),
        "injected delete failure completes");
    const auto* delete_error =
        std::get_if<application::github_credential_delete_error>(
            &*failed_delete.Terminal);
    expect(delete_error != nullptr, "delete failure is explicit");
    expect(
        vault.stored_matches(key, old_bytes),
        "failed delete preserves prior material");
}

void test_cancellation_and_late_completion()
{
    queued_fake_vault vault;
    const auto key = authority();
    const auto old_bytes = runtime_bytes(25U, 61U);
    const auto replacement_bytes = runtime_bytes(26U, 62U);
    const auto initial = context(key, 700U, 701U, 702U);
    store_observation initial_store;
    queue_store(vault, initial, old_bytes, initial_store);
    expect(vault.complete_store(initial), "cancellation fixture is stored");

    const auto cancelled_store_operation =
        context(key, 710U, 701U, 702U);
    store_observation cancelled_store;
    queue_store(
        vault,
        cancelled_store_operation,
        replacement_bytes,
        cancelled_store);
    vault.cancel(cancelled_store_operation);
    expect(
        cancelled_store.Calls == 1U,
        "store cancellation has one terminal");
    const auto* store_cancelled =
        std::get_if<application::github_credential_store_cancelled>(
            &*cancelled_store.Terminal);
    expect(store_cancelled != nullptr, "store reports cancellation");
    expect(
        store_cancelled->Context == cancelled_store_operation,
        "store cancellation preserves exact context");
    expect(
        !vault.complete_store(cancelled_store_operation),
        "late store completion is suppressed");
    expect(
        cancelled_store.Calls == 1U,
        "late store completion emits no second terminal");
    expect(
        vault.stored_matches(key, old_bytes),
        "cancelled store does not mutate material");

    const auto cancelled_retrieve_operation =
        context(key, 711U, 701U, 702U);
    const auto sibling_retrieve_operation =
        context(key, 712U, 701U, 702U);
    retrieve_observation cancelled_retrieve;
    retrieve_observation sibling_retrieve;
    vault.retrieve(
        cancelled_retrieve_operation,
        observe_completion(cancelled_retrieve));
    vault.retrieve(
        sibling_retrieve_operation,
        observe_completion(sibling_retrieve));
    vault.cancel(cancelled_retrieve_operation);
    expect(
        cancelled_retrieve.Calls == 1U,
        "retrieve cancellation has one terminal");
    const auto* retrieve_cancelled =
        std::get_if<application::github_credential_retrieve_cancelled>(
            &*cancelled_retrieve.Terminal);
    expect(retrieve_cancelled != nullptr, "retrieve reports cancellation");
    expect(
        !std::holds_alternative<
            application::github_credential_retrieve_success>(
            *cancelled_retrieve.Terminal),
        "cancelled retrieval exposes no credential");
    expect(
        sibling_retrieve.Calls == 0U,
        "exact cancellation leaves sibling pending");
    expect(
        !vault.complete_retrieve(cancelled_retrieve_operation),
        "late retrieve completion is suppressed");
    expect(
        vault.complete_retrieve(sibling_retrieve_operation),
        "unaffected retrieve sibling completes");
    expect(
        retrieve_success(sibling_retrieve) != nullptr,
        "unaffected retrieve sibling succeeds");
    expect(
        cancelled_retrieve.Calls == 1U,
        "late retrieve emits no second terminal");

    const auto cancelled_delete_operation =
        context(key, 713U, 701U, 702U);
    delete_observation cancelled_delete;
    vault.delete_credential(
        cancelled_delete_operation,
        observe_completion(cancelled_delete));
    vault.cancel(cancelled_delete_operation);
    expect(
        cancelled_delete.Calls == 1U,
        "delete cancellation has one terminal");
    const auto* delete_cancelled =
        std::get_if<application::github_credential_delete_cancelled>(
            &*cancelled_delete.Terminal);
    expect(delete_cancelled != nullptr, "delete reports cancellation");
    expect(
        !vault.complete_delete(cancelled_delete_operation),
        "late delete completion is suppressed");
    expect(
        cancelled_delete.Calls == 1U,
        "late delete emits no second terminal");
    expect(
        vault.stored_matches(key, old_bytes),
        "cancelled delete preserves material");
    expect(vault.pending_count() == 0U, "cancellation leaves no pending work");
}

void test_duplicate_pending_context()
{
    queued_fake_vault vault;
    const auto operation = context(authority(), 800U, 801U, 802U);
    retrieve_observation original;
    retrieve_observation duplicate;

    vault.retrieve(operation, observe_completion(original));
    vault.retrieve(operation, observe_completion(duplicate));

    expect(original.Calls == 0U, "original duplicate context remains pending");
    expect(duplicate.Calls == 1U, "duplicate context is rejected once");
    const auto* error =
        std::get_if<application::github_credential_retrieve_error>(
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
    using failure = application::github_credential_vault_failure;
    using store_disposition =
        application::github_credential_store_disposition;
    using delete_disposition =
        application::github_credential_delete_disposition;

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

    const auto malformed_store_success =
        application::github_credential_store_success{
            .Context = operation,
            .Disposition = static_cast<store_disposition>(0xffU),
        };
    expect(
        !valid(malformed_store_success),
        "store success rejects malformed result enum");

    const auto malformed_store_error =
        application::github_credential_store_error{
            .Context = operation,
            .Failure = static_cast<failure>(0xffU),
        };
    expect(
        !valid(malformed_store_error),
        "store error rejects malformed failure");

    const auto malformed_retrieve_error =
        application::github_credential_retrieve_error{
            .Context = operation,
            .Failure = failure::none,
        };
    expect(
        !valid(malformed_retrieve_error),
        "retrieve error rejects failure sentinel");

    const auto malformed_delete_success =
        application::github_credential_delete_success{
            .Context = operation,
            .Disposition = static_cast<delete_disposition>(0xffU),
        };
    expect(
        !valid(malformed_delete_success),
        "delete success rejects malformed result enum");

    const auto malformed_delete_error =
        application::github_credential_delete_error{
            .Context = operation,
            .Failure = static_cast<failure>(0xffU),
        };
    expect(
        !valid(malformed_delete_error),
        "delete error rejects malformed failure");

    const auto invalid_operation = context(authority(), 0U, 11U, 12U);
    expect(
        !valid(application::github_credential_store_cancelled{
            .Context = invalid_operation,
        }),
        "store cancellation rejects invalid context");
    expect(
        !valid(application::github_credential_retrieve_absent{
            .Context = invalid_operation,
        }),
        "retrieve absence rejects invalid context");
    expect(
        !valid(application::github_credential_delete_cancelled{
            .Context = invalid_operation,
        }),
        "delete cancellation rejects invalid context");

    const auto valid_store =
        application::github_credential_store_completion{
            application::github_credential_store_success{
                .Context = operation,
                .Disposition = store_disposition::stored,
            }};
    expect(valid(valid_store), "valid store variant is accepted");

    const auto valid_delete =
        application::github_credential_delete_completion{
            application::github_credential_delete_success{
                .Context = operation,
                .Disposition = delete_disposition::absent,
            }};
    expect(valid(valid_delete), "valid delete variant is accepted");

    const auto invalid_store =
        application::github_credential_store_completion{
            malformed_store_success};
    expect(!valid(invalid_store), "invalid store variant is rejected");

    const auto bytes = runtime_bytes(7U, 71U);
    const auto valid_retrieve =
        application::github_credential_retrieve_completion{
            application::github_credential_retrieve_success{
                .Context = operation,
                .Credential = material_from(bytes),
            }};
    expect(valid(valid_retrieve), "valid retrieve variant is accepted");
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
