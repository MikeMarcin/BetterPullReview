#include "bpr/application/github_application_host.hpp"
#include "bpr/application/manual_executor.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
using namespace bpr;

auto expect(const bool condition, const std::string_view message) -> void
{
    if(!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

struct fake_lease final : application::github_account_transport_lease
{
};

class fake_provider final : public application::github_provider_port
{
public:
    struct profile_call
    {
        application::github_profile_request Request;
        application::github_profile_completion_handler Completion;
    };

    enum class viewer_authentication
    {
        cli,
        bootstrap,
        account,
    };

    struct viewer_call
    {
        application::github_viewer_request Request;
        viewer_authentication Authentication = viewer_authentication::cli;
        application::github_viewer_completion_handler Completion;
    };

    struct capability_call
    {
        application::github_capability_request Request;
        application::github_capability_completion_handler Completion;
    };

    struct pull_request_call
    {
        application::github_pull_request_provider_request Request;
        application::github_pull_request_completion_handler Completion;
    };

    void resolve_profile(
        application::github_profile_request request,
        application::github_profile_completion_handler completion) override
    {
        if(ThrowOnProfileSubmit)
        {
            throw std::runtime_error{"profile submit"};
        }
        Profiles.push_back({
            .Request = request,
            .Completion = std::move(completion),
        });
    }

    void cancel_profile(application::github_profile_request request) override
    {
        CancelledProfiles.push_back(request);
        if(CompleteProfileFromWorkerOnCancel)
        {
            const auto found =
                std::ranges::find(Profiles, request, &profile_call::Request);
            expect(found != Profiles.end(), "cancelled profile call exists");
            auto completion = found->Completion;
            std::thread worker{
                [completion = std::move(completion), request]() mutable
                {
                    completion(application::github_profile_cancelled{
                        .Request = request,
                    });
                }};
            worker.join();
        }
    }

    void observe_viewer(
        application::github_viewer_submission submission,
        application::github_viewer_completion_handler completion) override
    {
        auto authentication = viewer_authentication::cli;
        if(std::holds_alternative<application::github_bootstrap_credential>(
               submission.Authentication))
        {
            authentication = viewer_authentication::bootstrap;
        }
        else if(std::holds_alternative<
                    application::github_credential_retrieve_success>(
                    submission.Authentication))
        {
            authentication = viewer_authentication::account;
        }
        Viewers.push_back({
            .Request = submission.Request,
            .Authentication = authentication,
            .Completion = std::move(completion),
        });
    }

    void cancel_viewer(application::github_viewer_request request) override
    {
        CancelledViewers.push_back(request);
    }

    void probe_capabilities(
        application::github_capability_request request,
        application::github_capability_completion_handler completion) override
    {
        Capabilities.push_back({
            .Request = request,
            .Completion = std::move(completion),
        });
    }

    void
    cancel_capabilities(application::github_capability_request request) override
    {
        CancelledCapabilities.push_back(request);
    }

    void fetch_pull_request(
        application::github_pull_request_provider_request request,
        application::github_pull_request_completion_handler completion) override
    {
        PullRequests.push_back({
            .Request = request,
            .Completion = std::move(completion),
        });
    }

    void cancel_pull_request(
        application::github_pull_request_provider_request request) override
    {
        CancelledPullRequests.push_back(std::move(request));
    }

    std::vector<profile_call> Profiles;
    std::vector<viewer_call> Viewers;
    std::vector<capability_call> Capabilities;
    std::vector<pull_request_call> PullRequests;
    std::vector<application::github_profile_request> CancelledProfiles;
    std::vector<application::github_viewer_request> CancelledViewers;
    std::vector<application::github_capability_request> CancelledCapabilities;
    std::vector<application::github_pull_request_provider_request>
        CancelledPullRequests;
    bool ThrowOnProfileSubmit = false;
    bool CompleteProfileFromWorkerOnCancel = false;
};

class fake_vault final : public application::github_credential_vault_port
{
public:
    struct store_call
    {
        application::github_credential_operation_context Context;
        application::github_credential_store_completion_handler Completion;
    };

    struct retrieve_call
    {
        application::github_credential_operation_context Context;
        application::github_credential_retrieve_completion_handler Completion;
    };

    struct delete_call
    {
        application::github_credential_operation_context Context;
        application::github_credential_delete_completion_handler Completion;
    };

    void store(application::github_credential_enrollment enrollment,
               application::github_credential_store_completion_handler
                   completion) override
    {
        if(ThrowOnStore)
        {
            throw std::runtime_error{"vault store"};
        }
        StoredCredentialBytes.push_back(enrollment.Credential.size());
        Stores.push_back({
            .Context = enrollment.Context,
            .Completion = std::move(completion),
        });
    }

    void retrieve(application::github_credential_operation_context context,
                  application::github_credential_retrieve_completion_handler
                      completion) override
    {
        Retrieves.push_back({
            .Context = context,
            .Completion = std::move(completion),
        });
    }

    void
    delete_credential(application::github_credential_operation_context context,
                      application::github_credential_delete_completion_handler
                          completion) override
    {
        Deletes.push_back({
            .Context = context,
            .Completion = std::move(completion),
        });
    }

    void
    cancel(application::github_credential_operation_context context) override
    {
        Cancellations.push_back(context);
    }

    std::vector<store_call> Stores;
    std::vector<retrieve_call> Retrieves;
    std::vector<delete_call> Deletes;
    std::vector<std::size_t> StoredCredentialBytes;
    std::vector<application::github_credential_operation_context> Cancellations;
    bool ThrowOnStore = false;
};

class throwing_once_lane final : public application::decision_executor
{
public:
    void post(std::function<void()> task) override
    {
        if(ThrowNext)
        {
            ThrowNext = false;
            throw std::runtime_error{"decision post"};
        }
        Inner.post(std::move(task));
    }

    [[nodiscard]] auto pending() const noexcept -> std::size_t
    {
        return Inner.pending();
    }

    void run_next()
    {
        Inner.run_next();
    }

private:
    application::manual_executor Inner;
    bool ThrowNext = true;
};

class recording_projections final : public application::github_projection_port
{
public:
    void publish(const core::github_transition_output& output) override
    {
        if(ThrowNextOutput)
        {
            ThrowNextOutput = false;
            throw std::runtime_error{"transition projection"};
        }
        if(OnNextOutput)
        {
            auto action = std::exchange(OnNextOutput, std::function<void()>{});
            action();
        }
        if(OnEveryOutput)
        {
            OnEveryOutput();
        }
        Outputs.push_back(output);
    }

    void publish_session(
        const application::github_session_read_model& session) override
    {
        if(ThrowNextSession)
        {
            ThrowNextSession = false;
            throw std::runtime_error{"session projection"};
        }
        Sessions.push_back(session);
    }

    void publish_snapshot(
        const application::github_pull_request_snapshot& snapshot) override
    {
        if(ThrowNextSnapshot)
        {
            ThrowNextSnapshot = false;
            throw std::runtime_error{"snapshot projection"};
        }
        Snapshots.push_back(snapshot);
    }

    void publish_diagnostic(
        const application::github_host_diagnostic& diagnostic) override
    {
        if(ThrowNextDiagnostic)
        {
            ThrowNextDiagnostic = false;
            throw std::runtime_error{"diagnostic projection"};
        }
        Diagnostics.push_back(diagnostic);
    }

    std::vector<core::github_transition_output> Outputs;
    std::vector<application::github_session_read_model> Sessions;
    std::vector<application::github_pull_request_snapshot> Snapshots;
    std::vector<application::github_host_diagnostic> Diagnostics;
    bool ThrowNextOutput = false;
    bool ThrowNextSession = false;
    bool ThrowNextSnapshot = false;
    bool ThrowNextDiagnostic = false;
    std::function<void()> OnNextOutput;
    std::function<void()> OnEveryOutput;
};

[[nodiscard]] auto initial_state() -> core::github_state
{
    return core::github_state{
        .ApplicationSession = core::application_session_id{41U},
        .NextRequest = core::request_id{1U},
        .ActiveProfile = std::nullopt,
        .IdentityGeneration = {},
        .PendingSession = std::nullopt,
        .Host = std::nullopt,
        .Account = std::nullopt,
        .Capabilities = {},
        .CapabilityRevision = {},
        .PullRequestGeneration = {},
        .PendingPullRequest = std::nullopt,
        .ActiveLocator = std::nullopt,
        .ActiveNumber = std::nullopt,
        .Repository = std::nullopt,
        .PullRequest = std::nullopt,
        .AcceptedSnapshot = std::nullopt,
    };
}

[[nodiscard]] auto native_profile(const core::host_profile_id profile)
    -> application::github_profile_authority
{
    return {
        .Profile = profile,
        .ProfileRevision = application::github_profile_revision{31U},
        .Host = core::git_host_id{21U},
        .ServerContract =
            application::github_server_contract::github_com_2022_11_28,
        .Authentication =
            application::github_authentication_transport::native_https,
        .ProbeContractRevision =
            application::github_probe_contract_revision{1U},
    };
}

[[nodiscard]] auto cli_profile(const core::host_profile_id profile)
    -> application::github_profile_authority
{
    auto result = native_profile(profile);
    result.Authentication =
        application::github_authentication_transport::github_cli;
    return result;
}

[[nodiscard]] auto ghes_cli_profile(const core::host_profile_id profile)
    -> application::github_profile_authority
{
    auto result = cli_profile(profile);
    result.ServerContract =
        application::github_server_contract::ghes_3_18_2022_11_28;
    return result;
}

[[nodiscard]] auto
viewer(const core::github_account_id account = core::github_account_id{51U})
    -> application::github_viewer_record
{
    return {
        .Account = account,
        .Node = application::github_actor_id{"viewer-node"},
        .Login = "reviewer",
        .DisplayName = "Review Engineer",
    };
}

[[nodiscard]] auto
rate_limit(const application::github_rate_limit_resource resource =
               application::github_rate_limit_resource::core)
    -> application::github_rate_limit_record
{
    return {
        .Resource = resource,
        .Limit = 5'000U,
        .Remaining = 4'999U,
        .ResetUnixSeconds = 2'000'000'000U,
    };
}

[[nodiscard]] auto rate_limits() -> application::github_rate_limit_state
{
    auto search = rate_limit(application::github_rate_limit_resource::search);
    search.Limit = 30U;
    search.Remaining = 29U;
    auto graphql = rate_limit(application::github_rate_limit_resource::graphql);
    graphql.Remaining = 4'998U;
    return {
        .Core = rate_limit(application::github_rate_limit_resource::core),
        .Search = search,
        .Graphql = graphql,
    };
}

[[nodiscard]] auto
rate_limits_for(const application::github_rate_limit_record& record)
    -> application::github_rate_limit_state
{
    application::github_rate_limit_state result;
    expect(application::observe(result, record),
           "single-resource rate fixture is valid");
    return result;
}

[[nodiscard]] auto
rate_limits_for(const application::github_rate_limit_resource resource)
    -> application::github_rate_limit_state
{
    return rate_limits_for(rate_limit(resource));
}

[[nodiscard]] auto capability_facts()
    -> std::array<application::github_capability_fact,
                  application::github_capability_fact_count>
{
    using capability = core::github_capability;
    using fact = application::github_capability_fact;
    using source = application::github_capability_evidence_source;
    using state = core::github_capability_state;
    return {
        fact{
            .Capability = capability::review_threads,
            .State = state::supported,
            .Source = source::observed_contract,
        },
        fact{
            .Capability = capability::viewed_file_mutations,
            .State = state::supported,
            .Source = source::observed_contract,
        },
        fact{
            .Capability = capability::resolve_thread,
            .State = state::supported,
            .Source = source::observed_contract,
        },
        fact{
            .Capability = capability::unresolve_thread,
            .State = state::supported,
            .Source = source::observed_contract,
        },
    };
}

[[nodiscard]] auto schema_facts()
    -> std::array<application::github_schema_capability_fact,
                  application::github_schema_capability_fact_count>
{
    using capability = application::github_schema_capability;
    using fact = application::github_schema_capability_fact;
    using source = application::github_capability_evidence_source;
    using state = core::github_capability_state;
    return {
        fact{capability::review_threads_read,
             state::supported,
             source::observed_contract},
        fact{capability::viewed_file_state_read,
             state::supported,
             source::observed_contract},
        fact{capability::mark_file_as_viewed,
             state::supported,
             source::observed_contract},
        fact{capability::unmark_file_as_viewed,
             state::supported,
             source::observed_contract},
        fact{capability::resolve_permission,
             state::supported,
             source::observed_contract},
        fact{capability::resolve_review_thread,
             state::supported,
             source::observed_contract},
        fact{capability::unresolve_permission,
             state::supported,
             source::observed_contract},
        fact{capability::unresolve_review_thread,
             state::supported,
             source::observed_contract},
    };
}

[[nodiscard]] auto unsupported_capability_facts()
    -> std::array<application::github_capability_fact,
                  application::github_capability_fact_count>
{
    auto result = capability_facts();
    for(auto& fact : result)
    {
        fact.State = core::github_capability_state::unsupported;
    }
    return result;
}

[[nodiscard]] auto unsupported_schema_facts()
    -> std::array<application::github_schema_capability_fact,
                  application::github_schema_capability_fact_count>
{
    auto result = schema_facts();
    for(auto& fact : result)
    {
        fact.State = core::github_capability_state::unsupported;
    }
    return result;
}

[[nodiscard]] auto credential() -> application::github_credential_material
{
    const std::array bytes{
        std::byte{0x01},
        std::byte{0x02},
        std::byte{0x03},
    };
    auto result = application::github_credential_material::create(bytes);
    expect(result.has_value(), "credential fixture allocates");
    return std::move(*result);
}

[[nodiscard]] auto
snapshot(const application::github_pull_request_provider_request& request)
    -> application::github_pull_request_snapshot
{
    return {
        .Profile = request.Context.Profile,
        .Host = request.Context.Host,
        .Account = request.Context.Account,
        .Viewer =
            {
                .Actor = request.Viewer.Node,
                .Login = request.Viewer.Login,
            },
        .Snapshot = core::provider_snapshot_id{91U},
        .Repository =
            {
                .Repository = core::repository_id{71U},
                .Node = application::github_node_id{"repository-node"},
                .Owner = request.Context.Repository.Owner,
                .Name = request.Context.Repository.Name,
                .DefaultBranch = "main",
            },
        .PullRequest =
            {
                .PullRequest = core::pull_request_id{81U},
                .Repository = core::repository_id{71U},
                .Number = request.Context.Number,
                .Node = application::github_node_id{"pull-request-node"},
                .Title = "Host orchestration",
                .Body = "Bounded.",
                .State = application::github_pull_request_state::open,
                .Draft = false,
                .Author = std::nullopt,
                .BaseOid =
                    application::commit_oid{
                        "1111111111111111111111111111111111111111"},
                .HeadOid =
                    application::commit_oid{
                        "2222222222222222222222222222222222222222"},
                .BaseRef = "main",
                .HeadRef = "host",
            },
        .Files = {},
        .Commits = {},
        .Reviews = {},
        .ReviewThreadsAvailability =
            application::github_collection_availability::available,
        .ReviewThreads = {},
        .IssueComments = {},
        .Checks = {},
        .SearchResults = {},
        .CapabilityEvidence =
            {
                .Capabilities = request.CapabilityEvidence.Capabilities,
                .Revision = request.CapabilityEvidence.Revision,
                .ReadShape = request.CapabilityEvidence.ReadShape,
                .ContractVersion = 1U,
            },
        .RateLimits = rate_limits(),
    };
}

struct host_fixture
{
    fake_provider Provider;
    fake_vault Vault;
    application::manual_executor Lane;
    recording_projections Projections;
    application::github_credential_binding_catalog OwnedBindings;
    application::github_credential_binding_catalog& Bindings;
    application::github_application_host Host;

    host_fixture()
        : Bindings{OwnedBindings}
        , Host{initial_state(),
               Provider,
               Provider,
               Provider,
               Provider,
               Vault,
               Bindings,
               Lane,
               Projections}
    {
    }

    explicit host_fixture(
        application::github_credential_binding_catalog& bindings)
        : Bindings{bindings}
        , Host{initial_state(),
               Provider,
               Provider,
               Provider,
               Provider,
               Vault,
               Bindings,
               Lane,
               Projections}
    {
    }
};

auto advance_native_to_store(host_fixture& fixture) -> void
{
    fixture.Host.enroll_profile(core::host_profile_id{11U}, credential());
    const auto profile = fixture.Provider.Profiles.front().Request;
    fixture.Provider.Profiles.front().Completion(
        application::github_profile_success{
            .Request = profile,
            .Authority = native_profile(profile.Context.Profile),
        });
    fixture.Lane.run_next();
    const auto bootstrap = fixture.Provider.Viewers.front().Request;
    fixture.Provider.Viewers.front().Completion(
        application::github_viewer_success{
            .Request = bootstrap,
            .Viewer = viewer(),
            .RateLimits =
                rate_limits_for(application::github_rate_limit_resource::core),
            .AccountTransport = {},
        });
    fixture.Lane.run_next();
}

auto advance_native_to_retrieve(host_fixture& fixture) -> void
{
    advance_native_to_store(fixture);
    const auto store = fixture.Vault.Stores.front().Context;
    fixture.Vault.Stores.front().Completion(
        application::github_credential_store_success{
            .Context = store,
            .Disposition =
                application::github_credential_store_disposition::stored,
        });
    fixture.Lane.run_next();
}

auto establish_native_session(
    host_fixture& fixture,
    const core::host_profile_id profile = core::host_profile_id{11U},
    const core::github_account_id account = core::github_account_id{51U})
    -> void
{
    fixture.Host.enroll_profile(profile, credential());
    expect(fixture.Provider.Profiles.size() == 1U,
           "activation synchronously submits only profile resolution");
    const auto profile_request = fixture.Provider.Profiles.front().Request;
    fixture.Provider.Profiles.front().Completion(
        application::github_profile_success{
            .Request = profile_request,
            .Authority = native_profile(profile_request.Context.Profile),
        });
    expect(fixture.Provider.Viewers.empty(),
           "worker completion cannot advance the host before decision lane");
    fixture.Lane.run_next();
    expect(fixture.Provider.Viewers.size() == 1U
               && fixture.Provider.Viewers.front().Authentication
                      == fake_provider::viewer_authentication::bootstrap,
           "native enrollment performs a typed bootstrap viewer observation");

    const auto bootstrap_request = fixture.Provider.Viewers.front().Request;
    fixture.Provider.Viewers.front().Completion(
        application::github_viewer_success{
            .Request = bootstrap_request,
            .Viewer = viewer(account),
            .RateLimits =
                rate_limits_for(application::github_rate_limit_resource::core),
            .AccountTransport = {},
        });
    expect(fixture.Vault.Stores.empty(),
           "bootstrap result is serialized before vault mutation");
    fixture.Lane.run_next();
    expect(fixture.Vault.Stores.size() == 1U
               && fixture.Vault.Stores.front().Context.Authority.Account
                      == account,
           "vault key uses only the account observed from bootstrap /user");

    const auto store_context = fixture.Vault.Stores.front().Context;
    fixture.Vault.Stores.front().Completion(
        application::github_credential_store_success{
            .Context = store_context,
            .Disposition =
                application::github_credential_store_disposition::stored,
        });
    fixture.Lane.run_next();
    expect(fixture.Vault.Retrieves.size() == 1U
               && fixture.Vault.Retrieves.front().Context.Authority
                      == store_context.Authority,
           "stored bootstrap credential is retrieved under the same authority");

    const auto retrieve_context = fixture.Vault.Retrieves.front().Context;
    fixture.Vault.Retrieves.front().Completion(
        application::github_credential_retrieve_success{
            .Context = retrieve_context,
            .Credential = credential(),
        });
    fixture.Lane.run_next();
    expect(fixture.Provider.Viewers.size() == 2U
               && fixture.Provider.Viewers.back().Authentication
                      == fake_provider::viewer_authentication::account,
           "exact retrieval drives a second account-authenticated /user");

    const auto account_request = fixture.Provider.Viewers.back().Request;
    const auto lease = std::make_shared<fake_lease>();
    fixture.Provider.Viewers.back().Completion(
        application::github_viewer_success{
            .Request = account_request,
            .Viewer = viewer(account),
            .RateLimits =
                rate_limits_for(application::github_rate_limit_resource::core),
            .AccountTransport = lease,
        });
    fixture.Lane.run_next();
    expect(fixture.Provider.Capabilities.size() == 1U
               && fixture.Provider.Capabilities.front().Request.AccountTransport
                      == lease,
           "only the exact account-authenticated lease reaches probing");

    const auto capability_request =
        fixture.Provider.Capabilities.front().Request;
    fixture.Provider.Capabilities.front().Completion(
        application::github_capability_success{
            .Request = capability_request,
            .Facts = capability_facts(),
            .SchemaFacts = schema_facts(),
            .RateLimits = rate_limits_for(
                application::github_rate_limit_resource::graphql),
        });
    expect(!fixture.Host.accepted_session().has_value()
               && fixture.Host.capability_cache_size() == 0U,
           "capability success cannot mutate core or cache off lane");
    fixture.Lane.run_next();
    expect(fixture.Host.accepted_session().has_value()
               && fixture.Host.capability_cache_size() == 1U
               && fixture.Projections.Sessions.size() == 1U,
           "core acceptance precedes one exact capability-cache commit");
}

auto advance_resume_to_capability(
    host_fixture& fixture,
    const application::github_credential_authority_key& expected_authority)
    -> application::github_capability_request
{
    fixture.Host.resume_profile(core::host_profile_id{11U});
    expect(fixture.Provider.Profiles.size() == 1U,
           "resume resolves only the selected profile");
    const auto profile = fixture.Provider.Profiles.front().Request;
    fixture.Provider.Profiles.front().Completion(
        application::github_profile_success{
            .Request = profile,
            .Authority = native_profile(profile.Context.Profile),
        });
    fixture.Lane.run_next();
    expect(fixture.Vault.Retrieves.size() == 1U
               && fixture.Vault.Retrieves.front().Context.Authority
                      == expected_authority
               && fixture.Vault.Stores.empty()
               && fixture.Provider.Viewers.empty(),
           "trusted resume retrieves the exact authority without bootstrap or "
           "store");

    const auto retrieve = fixture.Vault.Retrieves.front().Context;
    fixture.Vault.Retrieves.front().Completion(
        application::github_credential_retrieve_success{
            .Context = retrieve,
            .Credential = credential(),
        });
    fixture.Lane.run_next();
    expect(fixture.Provider.Viewers.size() == 1U
               && fixture.Provider.Viewers.front().Authentication
                      == fake_provider::viewer_authentication::account,
           "resumed material can perform only the account-authenticated /user");

    const auto account = fixture.Provider.Viewers.front().Request;
    fixture.Provider.Viewers.front().Completion(
        application::github_viewer_success{
            .Request = account,
            .Viewer = viewer(),
            .RateLimits =
                rate_limits_for(application::github_rate_limit_resource::core),
            .AccountTransport = std::make_shared<fake_lease>(),
        });
    fixture.Lane.run_next();
    expect(fixture.Provider.Capabilities.size() == 1U,
           "the resumed credential reaches probing only after exact /user");
    return fixture.Provider.Capabilities.front().Request;
}

auto seed_persistent_binding(
    application::github_credential_binding_catalog& bindings,
    const core::host_profile_id profile = core::host_profile_id{11U},
    const core::github_account_id account = core::github_account_id{51U})
    -> application::github_credential_authority_key
{
    const auto prior_bindings = bindings.size();
    host_fixture source{bindings};
    establish_native_session(source, profile, account);
    const auto authority = source.Vault.Stores.front().Context.Authority;
    source.Host.shutdown();
    expect(bindings.size() == prior_bindings + 1U
               && source.Vault.Deletes.empty(),
           "real enrollment seeds one retained host-observed binding");
    return authority;
}

auto test_enrollment_acceptance_snapshot_and_exact_delete() -> void
{
    host_fixture fixture;
    establish_native_session(fixture);

    fixture.Host.dispatch(core::open_pull_request_intent{
        .Repository = {.Owner = "openai", .Name = "better-pull-review"},
        .Number = core::pull_request_number{27U},
    });
    expect(fixture.Provider.PullRequests.size() == 1U,
           "accepted session submits one typed pull-request read");
    const auto request = fixture.Provider.PullRequests.front().Request;
    fixture.Provider.PullRequests.front().Completion(
        application::github_pull_request_success{
            .Request = request,
            .Snapshot = snapshot(request),
        });
    expect(!fixture.Host.accepted_snapshot().has_value(),
           "snapshot remains staged until the decision lane accepts it");
    fixture.Lane.run_next();
    expect(fixture.Host.accepted_snapshot().has_value()
               && fixture.Projections.Snapshots.size() == 1U
               && fixture.Host.rate_limits() == rate_limits(),
           "exact core snapshot delta commits with distinct resource limits");

    fixture.Host.dispatch(core::activate_host_profile_intent{
        .Profile = core::host_profile_id{12U},
    });
    expect(fixture.Vault.Deletes.size() == 1U
               && fixture.Vault.Deletes.front().Context.Authority.Account
                      == viewer().Account
               && fixture.Provider.Profiles.size() == 1U,
           "identity invalidation exact-deletes before resolving the next "
           "profile");

    const auto delete_context = fixture.Vault.Deletes.front().Context;
    fixture.Vault.Deletes.front().Completion(
        application::github_credential_delete_error{
            .Context = delete_context,
            .Failure =
                application::github_credential_vault_failure::operation_failed,
        });
    fixture.Lane.run_next();
    expect(fixture.Provider.Profiles.size() == 1U
               && !fixture.Projections.Diagnostics.empty()
               && fixture.Projections.Diagnostics.back().Reason
                      == core::github_reason_code::vault_unavailable,
           "delete failure is visible and blocks new session establishment");
}

auto test_retries_are_bounded_and_stale_work_is_cancelled() -> void
{
    host_fixture fixture;
    fixture.Host.dispatch(core::activate_host_profile_intent{
        .Profile = core::host_profile_id{11U},
    });
    const auto old_request = fixture.Provider.Profiles.front().Request;
    fixture.Host.dispatch(core::activate_host_profile_intent{
        .Profile = core::host_profile_id{12U},
    });
    expect(fixture.Provider.CancelledProfiles == std::vector{old_request}
               && fixture.Provider.Profiles.size() == 2U,
           "profile switch cancels exact in-flight work");

    fixture.Provider.Profiles.front().Completion(
        application::github_profile_success{
            .Request = old_request,
            .Authority = cli_profile(old_request.Context.Profile),
        });
    fixture.Lane.run_next();
    expect(fixture.Provider.Viewers.empty(),
           "reordered stale profile success cannot start viewer work");

    const auto current_request = fixture.Provider.Profiles.back().Request;
    fixture.Provider.Profiles.back().Completion(
        application::github_profile_success{
            .Request = current_request,
            .Authority = cli_profile(current_request.Context.Profile),
        });
    fixture.Lane.run_next();
    expect(fixture.Provider.Viewers.size() == 1U,
           "current profile success starts the viewer phase");

    const auto viewer_request = fixture.Provider.Viewers.front().Request;
    fixture.Provider.Viewers.front().Completion(
        application::github_viewer_success{
            .Request = viewer_request,
            .Viewer = viewer(),
            .RateLimits =
                rate_limits_for(application::github_rate_limit_resource::core),
            .AccountTransport = std::make_shared<fake_lease>(),
        });
    fixture.Lane.run_next();
    expect(fixture.Provider.Capabilities.size() == 1U,
           "CLI viewer starts capability probing");

    for(std::uint8_t attempt = 1U; attempt <= 3U; ++attempt)
    {
        auto& call = fixture.Provider
                         .Capabilities[static_cast<std::size_t>(attempt - 1U)];
        expect(call.Request.Attempt
                   == application::github_provider_attempt{attempt},
               "retry attempt identity is monotone and exact");
        call.Completion(application::github_capability_error{
            .Request = call.Request,
            .Error =
                {
                    .Failure =
                        attempt == 1U
                            ? application::github_provider_failure::unavailable
                        : attempt == 2U
                            ? application::github_provider_failure::timeout
                            : application::github_provider_failure::network,
                    .RetryAfterSeconds = std::nullopt,
                    .RateLimits = {},
                    .RateLimitCause = std::nullopt,
                },
        });
        expect(fixture.Provider.Capabilities.size()
                   == static_cast<std::size_t>(attempt),
               "retry is never submitted from the worker callback");
        fixture.Lane.run_next();
    }
    expect(
        fixture.Provider.Capabilities.size() == 3U
            && !fixture.Host.accepted_session().has_value()
            && fixture.Host.state().PendingSession == std::nullopt,
        "three total immediate serialized attempts exhaust to a core failure");
}

auto test_retry_after_is_terminal_and_preserved() -> void
{
    {
        host_fixture fixture;
        fixture.Host.dispatch(core::activate_host_profile_intent{
            .Profile = core::host_profile_id{11U},
        });
        const auto request = fixture.Provider.Profiles.front().Request;
        fixture.Provider.Profiles.front().Completion(
            application::github_profile_error{
                .Request = request,
                .Error =
                    {
                        .Failure =
                            application::github_provider_failure::unavailable,
                        .RetryAfterSeconds = 30U,
                        .RateLimits = {},
                        .RateLimitCause = std::nullopt,
                    },
            });
        fixture.Lane.run_next();
        expect(fixture.Provider.Profiles.size() == 1U
                   && fixture.Projections.Diagnostics.size() == 1U
                   && fixture.Projections.Diagnostics.back().Reason
                          == core::github_reason_code::network_unavailable
                   && fixture.Projections.Diagnostics.back().RetryAfterSeconds
                          == 30U,
               "positive Retry-After prevents an immediate retry and is "
               "preserved");
    }

    {
        host_fixture fixture;
        establish_native_session(fixture);
        fixture.Host.dispatch(core::open_pull_request_intent{
            .Repository = {.Owner = "openai", .Name = "better-pull-review"},
            .Number = core::pull_request_number{27U},
        });
        const auto request = fixture.Provider.PullRequests.front().Request;
        const application::github_rate_limit_record exhausted{
            .Resource = application::github_rate_limit_resource::core,
            .Limit = 5'000U,
            .Remaining = 0U,
            .ResetUnixSeconds = 2'000'000'100U,
            .SecondaryLimited = false,
        };
        fixture.Provider.PullRequests.front().Completion(
            application::github_pull_request_error{
                .Request = request,
                .Error =
                    {
                        .Failure =
                            application::github_provider_failure::rate_limit,
                        .RetryAfterSeconds =
                            application::maximum_github_retry_after_seconds,
                        .RateLimits = rate_limits_for(exhausted),
                        .RateLimitCause = exhausted,
                    },
            });
        fixture.Lane.run_next();
        expect(
            fixture.Provider.PullRequests.size() == 1U
                && fixture.Projections.Diagnostics.size() == 1U
                && fixture.Projections.Diagnostics.back().Reason
                       == core::github_reason_code::rate_limited
                && fixture.Projections.Diagnostics.back().RetryAfterSeconds
                       == application::maximum_github_retry_after_seconds
                && fixture.Projections.Diagnostics.back().RateLimits.Core
                       == fixture.Host.rate_limits().Core
                && fixture.Host.rate_limits().Core == exhausted,
            "bounded rate-limit Retry-After reaches the projection advisory");
    }

    {
        host_fixture fixture;
        establish_native_session(fixture);
        fixture.Host.dispatch(core::open_pull_request_intent{
            .Repository = {.Owner = "openai", .Name = "better-pull-review"},
            .Number = core::pull_request_number{27U},
        });
        const auto request = fixture.Provider.PullRequests.front().Request;
        auto exhausted = rate_limit();
        exhausted.Remaining = 0U;
        fixture.Provider.PullRequests.front().Completion(
            application::github_pull_request_error{
                .Request = request,
                .Error =
                    {
                        .Failure =
                            application::github_provider_failure::unavailable,
                        .RetryAfterSeconds = std::nullopt,
                        .RateLimits = rate_limits_for(exhausted),
                        .RateLimitCause = std::nullopt,
                    },
            });
        fixture.Lane.run_next();
        expect(fixture.Provider.PullRequests.size() == 1U
                   && fixture.Host.rate_limits().Core == exhausted,
               "primary exhaustion suppresses immediate unavailable retries");
    }

    {
        host_fixture fixture;
        establish_native_session(fixture);
        fixture.Host.dispatch(core::open_pull_request_intent{
            .Repository = {.Owner = "openai", .Name = "better-pull-review"},
            .Number = core::pull_request_number{27U},
        });
        const auto request = fixture.Provider.PullRequests.front().Request;
        const application::github_rate_limit_record secondary{
            .Resource = application::github_rate_limit_resource::core,
            .Limit = 5'000U,
            .Remaining = 4'999U,
            .ResetUnixSeconds = 2'000'000'200U,
            .SecondaryLimited = true,
        };
        fixture.Provider.PullRequests.front().Completion(
            application::github_pull_request_error{
                .Request = request,
                .Error =
                    {
                        .Failure =
                            application::github_provider_failure::rate_limit,
                        .RetryAfterSeconds = 60U,
                        .RateLimits = rate_limits_for(secondary),
                        .RateLimitCause = secondary,
                    },
            });
        fixture.Lane.run_next();
        expect(fixture.Host.rate_limits().Core == secondary
                   && fixture.Projections.Diagnostics.back().RateLimits.Core
                          == secondary
                   && fixture.Projections.Diagnostics.back().RetryAfterSeconds
                          == 60U,
               "secondary-limit state and fallback delay remain distinct");
    }

    {
        host_fixture fixture;
        establish_native_session(fixture);
        fixture.Host.dispatch(core::open_pull_request_intent{
            .Repository = {.Owner = "openai", .Name = "better-pull-review"},
            .Number = core::pull_request_number{27U},
        });
        auto search =
            rate_limit(application::github_rate_limit_resource::search);
        search.Remaining = 17U;
        const auto first = fixture.Provider.PullRequests.front().Request;
        fixture.Provider.PullRequests.front().Completion(
            application::github_pull_request_error{
                .Request = first,
                .Error =
                    {
                        .Failure =
                            application::github_provider_failure::unavailable,
                        .RetryAfterSeconds = std::nullopt,
                        .RateLimits = rate_limits_for(search),
                        .RateLimitCause = std::nullopt,
                    },
            });
        fixture.Lane.run_next();

        auto core = rate_limit(application::github_rate_limit_resource::core);
        core.Remaining = 4'123U;
        const auto second = fixture.Provider.PullRequests.back().Request;
        fixture.Provider.PullRequests.back().Completion(
            application::github_pull_request_error{
                .Request = second,
                .Error =
                    {
                        .Failure =
                            application::github_provider_failure::unavailable,
                        .RetryAfterSeconds = 30U,
                        .RateLimits = rate_limits_for(core),
                        .RateLimitCause = std::nullopt,
                    },
            });
        fixture.Lane.run_next();
        expect(
            fixture.Provider.PullRequests.size() == 2U
                && fixture.Projections.Diagnostics.size() == 1U
                && fixture.Projections.Diagnostics.back().RateLimits
                       == fixture.Host.rate_limits()
                && fixture.Host.rate_limits().Core == core
                && fixture.Host.rate_limits().Search == search
                && fixture.Host.rate_limits().Graphql.has_value(),
            "terminal advisory projects all resource observations accumulated "
            "across retries");
    }
}

auto test_wrong_correlations_and_submit_exceptions_fail_closed() -> void
{
    {
        host_fixture fixture;
        fixture.Host.dispatch(core::activate_host_profile_intent{
            .Profile = core::host_profile_id{11U},
        });
        const auto current = fixture.Provider.Profiles.front().Request;
        auto wrong = current;
        wrong.Context.Request = core::request_id{999U};
        fixture.Provider.Profiles.front().Completion(
            application::github_profile_success{
                .Request = wrong,
                .Authority = cli_profile(current.Context.Profile),
            });
        fixture.Lane.run_next();
        expect(!fixture.Host.state().PendingSession.has_value()
                   && fixture.Provider.Viewers.empty(),
               "wrong-correlated current provider terminal fails closed");
    }

    {
        host_fixture fixture;
        fixture.Provider.ThrowOnProfileSubmit = true;
        fixture.Host.dispatch(core::activate_host_profile_intent{
            .Profile = core::host_profile_id{11U},
        });
        expect(fixture.Lane.pending() == 1U,
               "synchronous provider throw is converted to a lane completion");
        fixture.Lane.run_next();
        expect(!fixture.Host.state().PendingSession.has_value(),
               "provider submit throw terminates the current session");
    }

    {
        host_fixture fixture;
        fixture.Host.dispatch(core::activate_host_profile_intent{
            .Profile = core::host_profile_id{11U},
        });
        const auto request = fixture.Provider.Profiles.front().Request;
        fixture.Provider.Profiles.front().Completion(
            application::github_profile_error{
                .Request = request,
                .Error =
                    {
                        .Failure =
                            application::github_provider_failure::network,
                        .RetryAfterSeconds = 1U,
                        .RateLimits = {},
                        .RateLimitCause = std::nullopt,
                    },
            });
        fixture.Lane.run_next();
        expect(
            fixture.Provider.Profiles.size() == 1U
                && !fixture.Host.state().PendingSession.has_value()
                && fixture.Projections.Diagnostics.empty(),
            "invalid retry metadata is terminal without an advisory or retry");
    }

    {
        host_fixture fixture;
        fixture.Host.dispatch(core::activate_host_profile_intent{
            .Profile = core::host_profile_id{11U},
        });
        const auto request = fixture.Provider.Profiles.front().Request;
        fixture.Provider.Profiles.front().Completion(
            application::github_profile_error{
                .Request = request,
                .Error =
                    {
                        .Failure =
                            application::github_provider_failure::permission,
                        .RetryAfterSeconds = std::nullopt,
                        .RateLimits = rate_limits_for(
                            application::github_rate_limit_record{
                                .Resource = application::
                                    github_rate_limit_resource::core,
                                .Limit = 5'000U,
                                .Remaining = 4'999U,
                                .ResetUnixSeconds = 2'000'000'000U,
                                .SecondaryLimited = false,
                            }),
                        .RateLimitCause = std::nullopt,
                    },
            });
        fixture.Lane.run_next();
        expect(!fixture.Host.rate_limits().Core.has_value()
                   && !fixture.Host.rate_limits().Search.has_value()
                   && !fixture.Host.rate_limits().Graphql.has_value()
                   && fixture.Projections.Diagnostics.empty()
                   && !fixture.Host.state().PendingSession.has_value(),
               "local profile resolution cannot fabricate remote rate state");
    }

    {
        fake_provider provider;
        fake_vault vault;
        throwing_once_lane lane;
        recording_projections projections;
        application::github_credential_binding_catalog bindings;
        application::github_application_host host{
            initial_state(),
            provider,
            provider,
            provider,
            provider,
            vault,
            bindings,
            lane,
            projections,
        };
        host.dispatch(core::activate_host_profile_intent{
            .Profile = core::host_profile_id{11U},
        });
        const auto request = provider.Profiles.front().Request;
        provider.Profiles.front().Completion(
            application::github_profile_success{
                .Request = request,
                .Authority = cli_profile(request.Context.Profile),
            });
        expect(lane.pending() == 1U,
               "a throwing lane post falls back to one typed completion");
        lane.run_next();
        expect(!host.state().PendingSession.has_value()
                   && provider.Viewers.empty(),
               "lane post failure cannot strand provider work");
    }

    {
        host_fixture fixture;
        fixture.Host.dispatch(core::activate_host_profile_intent{
            .Profile = core::host_profile_id{11U},
        });
        const auto profile = fixture.Provider.Profiles.front().Request;
        fixture.Provider.Profiles.front().Completion(
            application::github_profile_success{
                .Request = profile,
                .Authority = cli_profile(profile.Context.Profile),
            });
        fixture.Lane.run_next();
        const auto request = fixture.Provider.Viewers.front().Request;
        fixture.Provider.Viewers.front().Completion(
            application::github_viewer_error{
                .Request = request,
                .Error =
                    {
                        .Failure =
                            application::github_provider_failure::network,
                        .RetryAfterSeconds = std::nullopt,
                        .RateLimits = rate_limits_for(
                            application::github_rate_limit_resource::search),
                        .RateLimitCause = std::nullopt,
                    },
            });
        fixture.Lane.run_next();
        expect(fixture.Provider.Viewers.size() == 1U
                   && !fixture.Host.rate_limits().Search.has_value()
                   && !fixture.Host.state().PendingSession.has_value(),
               "viewer errors cannot inject a search rate observation");
    }

    {
        host_fixture fixture;
        fixture.Host.dispatch(core::activate_host_profile_intent{
            .Profile = core::host_profile_id{11U},
        });
        const auto profile = fixture.Provider.Profiles.front().Request;
        fixture.Provider.Profiles.front().Completion(
            application::github_profile_success{
                .Request = profile,
                .Authority = cli_profile(profile.Context.Profile),
            });
        fixture.Lane.run_next();
        const auto viewer_request = fixture.Provider.Viewers.front().Request;
        fixture.Provider.Viewers.front().Completion(
            application::github_viewer_success{
                .Request = viewer_request,
                .Viewer = viewer(),
                .RateLimits = rate_limits_for(
                    application::github_rate_limit_resource::core),
                .AccountTransport = std::make_shared<fake_lease>(),
            });
        fixture.Lane.run_next();
        const auto capability = fixture.Provider.Capabilities.front().Request;
        fixture.Provider.Capabilities.front().Completion(
            application::github_capability_error{
                .Request = capability,
                .Error =
                    {
                        .Failure =
                            application::github_provider_failure::permission,
                        .RetryAfterSeconds = std::nullopt,
                        .RateLimits = rate_limits_for(
                            application::github_rate_limit_resource::search),
                        .RateLimitCause = std::nullopt,
                    },
            });
        fixture.Lane.run_next();
        expect(fixture.Provider.Capabilities.size() == 1U
                   && !fixture.Host.rate_limits().Search.has_value()
                   && !fixture.Host.state().PendingSession.has_value(),
               "capability errors cannot inject a search rate observation");
    }
}

auto test_rate_observation_presence_follows_server_contract() -> void
{
    {
        host_fixture fixture;
        fixture.Host.dispatch(core::activate_host_profile_intent{
            .Profile = core::host_profile_id{11U},
        });
        const auto profile = fixture.Provider.Profiles.front().Request;
        fixture.Provider.Profiles.front().Completion(
            application::github_profile_success{
                .Request = profile,
                .Authority = cli_profile(profile.Context.Profile),
            });
        fixture.Lane.run_next();
        const auto viewer_request = fixture.Provider.Viewers.front().Request;
        fixture.Provider.Viewers.front().Completion(
            application::github_viewer_success{
                .Request = viewer_request,
                .Viewer = viewer(),
                .RateLimits = {},
                .AccountTransport = std::make_shared<fake_lease>(),
            });
        fixture.Lane.run_next();
        expect(fixture.Provider.Capabilities.empty()
                   && !fixture.Host.state().PendingSession.has_value(),
               "GitHub.com viewer success requires a core rate observation");
    }

    {
        host_fixture fixture;
        fixture.Host.dispatch(core::activate_host_profile_intent{
            .Profile = core::host_profile_id{11U},
        });
        const auto profile = fixture.Provider.Profiles.front().Request;
        fixture.Provider.Profiles.front().Completion(
            application::github_profile_success{
                .Request = profile,
                .Authority = cli_profile(profile.Context.Profile),
            });
        fixture.Lane.run_next();
        const auto viewer_request = fixture.Provider.Viewers.front().Request;
        fixture.Provider.Viewers.front().Completion(
            application::github_viewer_success{
                .Request = viewer_request,
                .Viewer = viewer(),
                .RateLimits = rate_limits_for(
                    application::github_rate_limit_resource::core),
                .AccountTransport = std::make_shared<fake_lease>(),
            });
        fixture.Lane.run_next();
        const auto capability = fixture.Provider.Capabilities.front().Request;
        fixture.Provider.Capabilities.front().Completion(
            application::github_capability_success{
                .Request = capability,
                .Facts = capability_facts(),
                .SchemaFacts = schema_facts(),
                .RateLimits = {},
            });
        fixture.Lane.run_next();
        expect(!fixture.Host.accepted_session().has_value(),
               "GitHub.com capability success requires a GraphQL rate "
               "observation");
    }

    {
        host_fixture fixture;
        fixture.Host.dispatch(core::activate_host_profile_intent{
            .Profile = core::host_profile_id{11U},
        });
        const auto profile = fixture.Provider.Profiles.front().Request;
        fixture.Provider.Profiles.front().Completion(
            application::github_profile_success{
                .Request = profile,
                .Authority = ghes_cli_profile(profile.Context.Profile),
            });
        fixture.Lane.run_next();
        const auto viewer_request = fixture.Provider.Viewers.front().Request;
        fixture.Provider.Viewers.front().Completion(
            application::github_viewer_success{
                .Request = viewer_request,
                .Viewer = viewer(),
                .RateLimits = {},
                .AccountTransport = std::make_shared<fake_lease>(),
            });
        fixture.Lane.run_next();
        const auto capability = fixture.Provider.Capabilities.front().Request;
        fixture.Provider.Capabilities.front().Completion(
            application::github_capability_success{
                .Request = capability,
                .Facts = capability_facts(),
                .SchemaFacts = schema_facts(),
                .RateLimits = {},
            });
        fixture.Lane.run_next();
        expect(
            fixture.Host.accepted_session().has_value()
                && !application::has_observations(fixture.Host.rate_limits()),
            "GHES with disabled rate limits preserves explicit no-observation "
            "state");

        fixture.Host.dispatch(core::open_pull_request_intent{
            .Repository = {.Owner = "openai", .Name = "better-pull-review"},
            .Number = core::pull_request_number{27U},
        });
        const auto request = fixture.Provider.PullRequests.front().Request;
        auto result = snapshot(request);
        result.RateLimits = {};
        fixture.Provider.PullRequests.front().Completion(
            application::github_pull_request_success{
                .Request = request,
                .Snapshot = std::move(result),
            });
        fixture.Lane.run_next();
        expect(fixture.Host.accepted_snapshot().has_value()
                   && !application::has_observations(
                       fixture.Host.accepted_snapshot()->RateLimits),
               "GHES disabled-limit snapshot remains explicitly unobserved");

        fixture.Host.dispatch(core::open_pull_request_intent{
            .Repository = {.Owner = "openai", .Name = "better-pull-review"},
            .Number = core::pull_request_number{28U},
        });
        const auto partial_request =
            fixture.Provider.PullRequests.back().Request;
        auto partial = snapshot(partial_request);
        partial.RateLimits =
            rate_limits_for(application::github_rate_limit_resource::search);
        fixture.Provider.PullRequests.back().Completion(
            application::github_pull_request_success{
                .Request = partial_request,
                .Snapshot = std::move(partial),
            });
        fixture.Lane.run_next();
        expect(fixture.Host.accepted_snapshot().has_value()
                   && fixture.Host.accepted_snapshot()
                          ->RateLimits.Search.has_value()
                   && !fixture.Host.accepted_snapshot()
                           ->RateLimits.Core.has_value()
                   && !fixture.Host.accepted_snapshot()
                           ->RateLimits.Graphql.has_value(),
               "GHES accepts the valid subset observed when rate limiting is "
               "mixed");
    }

    {
        host_fixture fixture;
        establish_native_session(fixture);
        fixture.Host.dispatch(core::open_pull_request_intent{
            .Repository = {.Owner = "openai", .Name = "better-pull-review"},
            .Number = core::pull_request_number{27U},
        });
        const auto request = fixture.Provider.PullRequests.front().Request;
        auto result = snapshot(request);
        result.RateLimits.Search.reset();
        fixture.Provider.PullRequests.front().Completion(
            application::github_pull_request_success{
                .Request = request,
                .Snapshot = std::move(result),
            });
        fixture.Lane.run_next();
        expect(!fixture.Host.accepted_snapshot().has_value(),
               "GitHub.com snapshots require every exercised resource bucket");
    }
}

auto test_no_graphql_read_rejects_graphql_error_observations() -> void
{
    host_fixture fixture;
    fixture.Host.dispatch(core::activate_host_profile_intent{
        .Profile = core::host_profile_id{11U},
    });
    const auto profile = fixture.Provider.Profiles.front().Request;
    fixture.Provider.Profiles.front().Completion(
        application::github_profile_success{
            .Request = profile,
            .Authority = cli_profile(profile.Context.Profile),
        });
    fixture.Lane.run_next();
    const auto viewer_request = fixture.Provider.Viewers.front().Request;
    fixture.Provider.Viewers.front().Completion(
        application::github_viewer_success{
            .Request = viewer_request,
            .Viewer = viewer(),
            .RateLimits =
                rate_limits_for(application::github_rate_limit_resource::core),
            .AccountTransport = std::make_shared<fake_lease>(),
        });
    fixture.Lane.run_next();
    const auto capability = fixture.Provider.Capabilities.front().Request;
    fixture.Provider.Capabilities.front().Completion(
        application::github_capability_success{
            .Request = capability,
            .Facts = unsupported_capability_facts(),
            .SchemaFacts = unsupported_schema_facts(),
            .RateLimits = rate_limits_for(
                application::github_rate_limit_resource::graphql),
        });
    fixture.Lane.run_next();
    expect(fixture.Host.accepted_session().has_value()
               && !fixture.Host.accepted_session()
                       ->CapabilityEvidence.ReadShape.ReviewThreads
               && !fixture.Host.accepted_session()
                       ->CapabilityEvidence.ReadShape.ViewedFileState,
           "unsupported schema establishes a REST-only read shape");

    fixture.Host.dispatch(core::open_pull_request_intent{
        .Repository = {.Owner = "openai", .Name = "better-pull-review"},
        .Number = core::pull_request_number{27U},
    });
    const auto request = fixture.Provider.PullRequests.front().Request;
    const auto prior_graphql = fixture.Host.rate_limits().Graphql;
    auto injected =
        rate_limit(application::github_rate_limit_resource::graphql);
    injected.Remaining = 123U;
    fixture.Provider.PullRequests.front().Completion(
        application::github_pull_request_error{
            .Request = request,
            .Error =
                {
                    .Failure = application::github_provider_failure::permission,
                    .RetryAfterSeconds = std::nullopt,
                    .RateLimits = rate_limits_for(injected),
                    .RateLimitCause = std::nullopt,
                },
        });
    fixture.Lane.run_next();
    expect(fixture.Host.rate_limits().Graphql == prior_graphql
               && fixture.Projections.Diagnostics.empty()
               && !fixture.Host.state().PendingPullRequest.has_value(),
           "REST-only reads cannot inject a GraphQL error observation");
}

auto test_projection_failures_cannot_interrupt_effects_or_cleanup() -> void
{
    {
        host_fixture fixture;
        fixture.Projections.ThrowNextOutput = true;
        fixture.Host.dispatch(core::activate_host_profile_intent{
            .Profile = core::host_profile_id{11U},
        });
        expect(fixture.Provider.Profiles.size() == 1U
                   && fixture.Host.projection_delivery_failed(),
               "a failed transition projection cannot veto decided effects");
    }

    {
        host_fixture fixture;
        advance_native_to_retrieve(fixture);
        const auto retrieve = fixture.Vault.Retrieves.front().Context;
        fixture.Vault.Retrieves.front().Completion(
            application::github_credential_retrieve_success{
                .Context = retrieve,
                .Credential = credential(),
            });
        fixture.Lane.run_next();

        const auto account = fixture.Provider.Viewers.back().Request;
        fixture.Provider.Viewers.back().Completion(
            application::github_viewer_success{
                .Request = account,
                .Viewer = viewer(),
                .RateLimits = rate_limits_for(
                    application::github_rate_limit_resource::core),
                .AccountTransport = std::make_shared<fake_lease>(),
            });
        fixture.Lane.run_next();
        const auto capability = fixture.Provider.Capabilities.front().Request;

        fixture.Projections.ThrowNextDiagnostic = true;
        fixture.Provider.Capabilities.front().Completion(
            application::github_capability_error{
                .Request = capability,
                .Error =
                    {
                        .Failure =
                            application::github_provider_failure::unavailable,
                        .RetryAfterSeconds = 30U,
                        .RateLimits = {},
                        .RateLimitCause = std::nullopt,
                    },
            });
        fixture.Lane.run_next();
        expect(
            fixture.Host.projection_delivery_failed()
                && fixture.Vault.Deletes.size() == 1U,
            "a failed error advisory cannot strand a provisional credential");

        const auto cleanup = fixture.Vault.Deletes.front().Context;
        fixture.Projections.ThrowNextDiagnostic = true;
        fixture.Vault.Deletes.front().Completion(
            application::github_credential_delete_error{
                .Context = cleanup,
                .Failure = application::github_credential_vault_failure::
                    operation_failed,
            });
        fixture.Lane.run_next();
        expect(!fixture.Host.state().PendingSession.has_value()
                   && fixture.Projections.Diagnostics.empty(),
               "a failed cleanup diagnostic cannot interrupt the deferred "
               "terminal");
    }
}

auto test_projection_reentry_is_serialized_after_the_current_turn() -> void
{
    {
        host_fixture fixture;
        fixture.Projections.OnNextOutput = [&fixture]
        { fixture.Host.dispatch(core::deactivate_github_identity_intent{}); };
        fixture.Host.dispatch(core::activate_host_profile_intent{
            .Profile = core::host_profile_id{11U},
        });
        expect(
            fixture.Provider.Profiles.size() == 1U
                && fixture.Provider.CancelledProfiles
                       == std::vector{fixture.Provider.Profiles.front().Request}
                && !fixture.Host.state().ActiveProfile.has_value()
                && !fixture.Host.state().PendingSession.has_value(),
            "projection reentry is ordered after and cancels the current "
            "effect");
    }

    {
        host_fixture fixture;
        advance_native_to_retrieve(fixture);
        const auto retrieve = fixture.Vault.Retrieves.front().Context;
        fixture.Vault.Retrieves.front().Completion(
            application::github_credential_retrieve_success{
                .Context = retrieve,
                .Credential = credential(),
            });
        fixture.Lane.run_next();
        const auto account = fixture.Provider.Viewers.back().Request;
        fixture.Provider.Viewers.back().Completion(
            application::github_viewer_success{
                .Request = account,
                .Viewer = viewer(),
                .RateLimits = rate_limits_for(
                    application::github_rate_limit_resource::core),
                .AccountTransport = std::make_shared<fake_lease>(),
            });
        fixture.Lane.run_next();

        fixture.Projections.OnNextOutput = [&fixture]
        { fixture.Host.dispatch(core::deactivate_github_identity_intent{}); };
        const auto capability = fixture.Provider.Capabilities.front().Request;
        fixture.Provider.Capabilities.front().Completion(
            application::github_capability_success{
                .Request = capability,
                .Facts = capability_facts(),
                .SchemaFacts = schema_facts(),
                .RateLimits = rate_limits_for(
                    application::github_rate_limit_resource::graphql),
            });
        fixture.Lane.run_next();
        expect(fixture.Projections.Sessions.size() == 1U
                   && !fixture.Host.accepted_session().has_value()
                   && fixture.Vault.Deletes.size() == 1U,
               "session publication finishes before deferred reentrant "
               "invalidation");
    }

    {
        host_fixture fixture;
        fixture.Projections.OnEveryOutput = [&fixture]
        { fixture.Host.dispatch(core::deactivate_github_identity_intent{}); };
        fixture.Host.dispatch(core::activate_host_profile_intent{
            .Profile = core::host_profile_id{11U},
        });
        expect(fixture.Lane.pending() == 1U
                   && !fixture.Host.state().ActiveProfile.has_value(),
               "repeated projection feedback yields after one deferred "
               "generation");

        fixture.Projections.OnEveryOutput = {};
        fixture.Host.dispatch(core::activate_host_profile_intent{
            .Profile = core::host_profile_id{12U},
        });
        expect(fixture.Provider.Profiles.size() == 1U
                   && !fixture.Host.state().ActiveProfile.has_value(),
               "new ingress cannot overtake an older deferred generation");
        fixture.Lane.run_next();
        expect(fixture.Lane.pending() == 0U
                   && fixture.Provider.Profiles.size() == 2U
                   && fixture.Host.state().ActiveProfile
                          == core::host_profile_id{12U},
               "deferred feedback stays FIFO and yields fairly on the decision "
               "lane");
    }

    {
        host_fixture fixture;
        fixture.Projections.OnEveryOutput = [&fixture]
        { fixture.Host.dispatch(core::deactivate_github_identity_intent{}); };
        fixture.Host.dispatch(core::activate_host_profile_intent{
            .Profile = core::host_profile_id{11U},
        });
        expect(fixture.Lane.pending() == 1U,
               "feedback continuation is pending before terminal shutdown");
        fixture.Host.shutdown();
        expect(fixture.Host.is_closing()
                   && fixture.Provider.CancelledProfiles.size() == 1U,
               "shutdown closes immediately despite an older deferred "
               "continuation");
    }
}

auto test_invalid_enrollment_cannot_leak_into_a_later_activation() -> void
{
    host_fixture fixture;
    fixture.Host.enroll_profile({}, credential());
    expect(fixture.Provider.Profiles.empty(),
           "invalid profile enrollment submits no provider work");

    fixture.Host.dispatch(core::activate_host_profile_intent{
        .Profile = core::host_profile_id{11U},
    });
    const auto request = fixture.Provider.Profiles.front().Request;
    fixture.Provider.Profiles.front().Completion(
        application::github_profile_success{
            .Request = request,
            .Authority = native_profile(request.Context.Profile),
        });
    fixture.Lane.run_next();
    expect(fixture.Provider.Viewers.empty()
               && !fixture.Host.state().PendingSession.has_value(),
           "a later activation cannot consume a rejected enrollment secret");
}

auto test_snapshot_authority_and_pr_cancellation_fail_closed() -> void
{
    {
        host_fixture fixture;
        establish_native_session(fixture);
        fixture.Host.dispatch(core::open_pull_request_intent{
            .Repository = {.Owner = "openai", .Name = "better-pull-review"},
            .Number = core::pull_request_number{27U},
        });
        const auto request = fixture.Provider.PullRequests.front().Request;
        auto fabricated = snapshot(request);
        fabricated.CapabilityEvidence.Capabilities = {};
        fabricated.CapabilityEvidence.ReadShape = {};
        fabricated.ReviewThreadsAvailability =
            application::github_collection_availability::unavailable;
        fixture.Provider.PullRequests.front().Completion(
            application::github_pull_request_success{
                .Request = request,
                .Snapshot = std::move(fabricated),
            });
        fixture.Lane.run_next();
        expect(!fixture.Host.accepted_snapshot().has_value()
                   && fixture.Projections.Snapshots.empty(),
               "same-revision fabricated capability evidence cannot be staged");
    }

    {
        host_fixture fixture;
        establish_native_session(fixture);
        fixture.Host.dispatch(core::open_pull_request_intent{
            .Repository = {.Owner = "openai", .Name = "better-pull-review"},
            .Number = core::pull_request_number{27U},
        });
        const auto request = fixture.Provider.PullRequests.front().Request;
        auto wrong_viewer = snapshot(request);
        wrong_viewer.Viewer = {
            .Actor = application::github_actor_id{"other-viewer"},
            .Login = "other",
        };
        fixture.Provider.PullRequests.front().Completion(
            application::github_pull_request_success{
                .Request = request,
                .Snapshot = std::move(wrong_viewer),
            });
        fixture.Lane.run_next();
        expect(!fixture.Host.accepted_snapshot().has_value()
                   && fixture.Projections.Snapshots.empty(),
               "provider snapshot cannot replace the authenticated viewer");
    }

    {
        host_fixture fixture;
        establish_native_session(fixture);
        fixture.Host.dispatch(core::open_pull_request_intent{
            .Repository = {.Owner = "openai", .Name = "better-pull-review"},
            .Number = core::pull_request_number{27U},
        });
        const auto request = fixture.Provider.PullRequests.front().Request;
        fixture.Provider.PullRequests.front().Completion(
            application::github_pull_request_success{
                .Request = request,
                .Snapshot = snapshot(request),
            });
        fixture.Host.dispatch(core::activate_host_profile_intent{
            .Profile = core::host_profile_id{12U},
        });
        expect(fixture.Provider.CancelledPullRequests == std::vector{request},
               "identity switch cancels the exact in-flight PR request");
        fixture.Lane.run_next();
        expect(!fixture.Host.accepted_snapshot().has_value()
                   && fixture.Projections.Snapshots.empty(),
               "queued old-identity PR success is inert after cancellation");
    }

    {
        host_fixture fixture;
        establish_native_session(fixture);
        fixture.Host.dispatch(core::open_pull_request_intent{
            .Repository = {.Owner = "openai", .Name = "better-pull-review"},
            .Number = core::pull_request_number{27U},
        });
        const auto request = fixture.Provider.PullRequests.front().Request;
        fixture.Provider.PullRequests.front().Completion(
            application::github_pull_request_error{
                .Request = request,
                .Error =
                    {
                        .Failure =
                            application::github_provider_failure::rate_limit,
                        .RetryAfterSeconds = std::nullopt,
                        .RateLimits = rate_limits_for(
                            application::github_rate_limit_record{
                                .Resource = application::
                                    github_rate_limit_resource::core,
                                .Limit = 5'000U,
                                .Remaining = 0U,
                                .ResetUnixSeconds = 2'000'000'300U,
                                .SecondaryLimited = false,
                            }),
                        .RateLimitCause =
                            application::github_rate_limit_record{
                                .Resource = application::
                                    github_rate_limit_resource::core,
                                .Limit = 5'000U,
                                .Remaining = 0U,
                                .ResetUnixSeconds = 2'000'000'300U,
                                .SecondaryLimited = false,
                            },
                    },
            });
        fixture.Host.dispatch(core::activate_host_profile_intent{
            .Profile = core::host_profile_id{12U},
        });
        fixture.Lane.run_next();
        expect(!fixture.Host.rate_limits().Core.has_value()
                   && !fixture.Host.rate_limits().Search.has_value()
                   && !fixture.Host.rate_limits().Graphql.has_value()
                   && fixture.Projections.Diagnostics.empty(),
               "stale old-identity rate observations cannot repopulate host "
               "state");
    }

    {
        host_fixture fixture;
        establish_native_session(fixture);
        fixture.Host.dispatch(core::open_pull_request_intent{
            .Repository = {.Owner = "openai", .Name = "better-pull-review"},
            .Number = core::pull_request_number{27U},
        });
        const auto request = fixture.Provider.PullRequests.front().Request;
        fixture.Provider.PullRequests.front().Completion(
            application::github_pull_request_cancelled{
                .Request = request,
            });
        fixture.Lane.run_next();
        expect(
            !fixture.Host.state().PendingPullRequest.has_value()
                && !fixture.Host.accepted_snapshot().has_value(),
            "current PR cancellation clears pending state without a snapshot");
    }
}

auto test_new_open_clears_host_snapshot_but_refresh_retains_it() -> void
{
    host_fixture fixture;
    establish_native_session(fixture);
    fixture.Host.dispatch(core::open_pull_request_intent{
        .Repository = {.Owner = "openai", .Name = "better-pull-review"},
        .Number = core::pull_request_number{27U},
    });
    const auto first = fixture.Provider.PullRequests.front().Request;
    fixture.Provider.PullRequests.front().Completion(
        application::github_pull_request_success{
            .Request = first,
            .Snapshot = snapshot(first),
        });
    fixture.Lane.run_next();
    expect(fixture.Host.accepted_snapshot().has_value(),
           "first PR snapshot is accepted");

    fixture.Host.dispatch(core::refresh_pull_request_intent{});
    expect(fixture.Host.accepted_snapshot().has_value(),
           "refresh retains the last accepted snapshot while loading");
    const auto refresh = fixture.Provider.PullRequests.back().Request;
    fixture.Provider.PullRequests.back().Completion(
        application::github_pull_request_cancelled{
            .Request = refresh,
        });
    fixture.Lane.run_next();
    expect(fixture.Host.accepted_snapshot().has_value(),
           "cancelled refresh preserves the last accepted snapshot");

    fixture.Host.dispatch(core::open_pull_request_intent{
        .Repository = {.Owner = "openai", .Name = "another-repository"},
        .Number = core::pull_request_number{28U},
    });
    expect(!fixture.Host.accepted_snapshot().has_value(),
           "accepted new open clears the old host-owned snapshot immediately");
    const auto second = fixture.Provider.PullRequests.back().Request;
    fixture.Provider.PullRequests.back().Completion(
        application::github_pull_request_error{
            .Request = second,
            .Error =
                {
                    .Failure = application::github_provider_failure::permission,
                    .RetryAfterSeconds = std::nullopt,
                    .RateLimits = {},
                    .RateLimitCause = std::nullopt,
                },
        });
    fixture.Lane.run_next();
    expect(!fixture.Host.accepted_snapshot().has_value(),
           "failed new open cannot revive the previous PR snapshot");
}

auto test_vault_wrong_context_and_submit_throw_fail_closed() -> void
{
    {
        host_fixture fixture;
        advance_native_to_store(fixture);
        const auto expected = fixture.Vault.Stores.front().Context;
        auto wrong = expected;
        wrong.Request = core::request_id{999U};
        fixture.Vault.Stores.front().Completion(
            application::github_credential_store_success{
                .Context = wrong,
                .Disposition =
                    application::github_credential_store_disposition::stored,
            });
        fixture.Lane.run_next();
        expect(
            fixture.Vault.Deletes.size() == 1U
                && fixture.Vault.Deletes.front().Context.Authority
                       == expected.Authority
                && fixture.Vault.Deletes.front().Context != wrong,
            "wrong-context store success cleans only host-captured authority");
        const auto cleanup = fixture.Vault.Deletes.front().Context;
        fixture.Vault.Deletes.front().Completion(
            application::github_credential_delete_success{
                .Context = cleanup,
                .Disposition =
                    application::github_credential_delete_disposition::deleted,
            });
        fixture.Lane.run_next();
        const auto diagnostics = fixture.Projections.Diagnostics.size();
        fixture.Vault.Deletes.front().Completion(
            application::github_credential_delete_success{
                .Context = cleanup,
                .Disposition =
                    application::github_credential_delete_disposition::deleted,
            });
        fixture.Lane.run_next();
        expect(
            fixture.Projections.Diagnostics.size() == diagnostics
                && !fixture.Host.state().PendingSession.has_value(),
            "duplicate delete terminal is stale and cannot rewrite lifecycle");
    }

    {
        host_fixture fixture;
        fixture.Vault.ThrowOnStore = true;
        advance_native_to_store(fixture);
        expect(fixture.Lane.pending() == 1U,
               "synchronous vault throw is converted to a lane completion");
        fixture.Lane.run_next();
        expect(
            !fixture.Host.accepted_session().has_value()
                && !fixture.Vault.Deletes.empty(),
            "vault submit throw terminates and cleans provisional authority");
    }

    {
        host_fixture fixture;
        advance_native_to_retrieve(fixture);
        const auto retrieve = fixture.Vault.Retrieves.front().Context;
        fixture.Vault.Retrieves.front().Completion(
            application::github_credential_retrieve_success{
                .Context = retrieve,
                .Credential = credential(),
            });
        fixture.Lane.run_next();
        const auto viewer_request = fixture.Provider.Viewers.back().Request;
        fixture.Vault.Retrieves.front().Completion(
            application::github_credential_retrieve_success{
                .Context = retrieve,
                .Credential = credential(),
            });
        fixture.Lane.run_next();
        expect(
            fixture.Provider.Viewers.size() == 2U
                && fixture.Provider.CancelledViewers.empty()
                && fixture.Vault.Deletes.empty()
                && fixture.Provider.Viewers.back().Request == viewer_request,
            "duplicate retrieve terminal is stale and cannot tear down viewer");
    }
}

auto test_reenrollment_waits_for_running_vault_cleanup() -> void
{
    {
        host_fixture fixture;
        advance_native_to_store(fixture);
        const auto running = fixture.Vault.Stores.front().Context;

        fixture.Host.enroll_profile(core::host_profile_id{11U}, credential());
        expect(fixture.Vault.Cancellations == std::vector{running}
                   && fixture.Vault.Deletes.empty()
                   && fixture.Provider.Profiles.size() == 1U,
               "same-authority re-enrollment waits for running store terminal");

        fixture.Vault.Stores.front().Completion(
            application::github_credential_store_success{
                .Context = running,
                .Disposition =
                    application::github_credential_store_disposition::stored,
            });
        fixture.Lane.run_next();
        expect(fixture.Vault.Deletes.size() == 1U
                   && fixture.Vault.Deletes.front().Context.Authority
                          == running.Authority
                   && fixture.Provider.Profiles.size() == 1U,
               "store terminal starts exact cleanup before replacement work");

        const auto cleanup = fixture.Vault.Deletes.front().Context;
        fixture.Vault.Deletes.front().Completion(
            application::github_credential_delete_success{
                .Context = cleanup,
                .Disposition =
                    application::github_credential_delete_disposition::deleted,
            });
        fixture.Lane.run_next();
        expect(fixture.Provider.Profiles.size() == 2U,
               "replacement profile starts only after store cleanup succeeds");
    }

    {
        host_fixture fixture;
        advance_native_to_retrieve(fixture);
        const auto running = fixture.Vault.Retrieves.front().Context;

        fixture.Host.enroll_profile(core::host_profile_id{11U}, credential());
        expect(
            fixture.Vault.Cancellations == std::vector{running}
                && fixture.Vault.Deletes.empty()
                && fixture.Provider.Profiles.size() == 1U,
            "same-authority re-enrollment waits for running retrieve terminal");

        fixture.Vault.Retrieves.front().Completion(
            application::github_credential_retrieve_success{
                .Context = running,
                .Credential = credential(),
            });
        fixture.Lane.run_next();
        expect(
            fixture.Vault.Deletes.size() == 1U
                && fixture.Vault.Deletes.front().Context.Authority
                       == running.Authority
                && fixture.Provider.Profiles.size() == 1U,
            "retrieve terminal starts exact cleanup before replacement work");

        const auto cleanup = fixture.Vault.Deletes.front().Context;
        fixture.Vault.Deletes.front().Completion(
            application::github_credential_delete_success{
                .Context = cleanup,
                .Disposition =
                    application::github_credential_delete_disposition::deleted,
            });
        fixture.Lane.run_next();
        expect(
            fixture.Provider.Profiles.size() == 2U,
            "replacement profile starts only after retrieve cleanup succeeds");
    }
}

auto test_bootstrap_lease_and_changed_account_fail_closed() -> void
{
    {
        host_fixture fixture;
        fixture.Host.enroll_profile(core::host_profile_id{11U}, credential());
        const auto profile = fixture.Provider.Profiles.front().Request;
        fixture.Provider.Profiles.front().Completion(
            application::github_profile_success{
                .Request = profile,
                .Authority = native_profile(profile.Context.Profile),
            });
        fixture.Lane.run_next();
        const auto bootstrap = fixture.Provider.Viewers.front().Request;
        fixture.Provider.Viewers.front().Completion(
            application::github_viewer_success{
                .Request = bootstrap,
                .Viewer = viewer(),
                .RateLimits = rate_limits_for(
                    application::github_rate_limit_resource::core),
                .AccountTransport = std::make_shared<fake_lease>(),
            });
        fixture.Lane.run_next();
        expect(fixture.Vault.Stores.empty()
                   && !fixture.Host.accepted_session().has_value()
                   && !fixture.Host.state().PendingSession.has_value(),
               "a bootstrap provider cannot smuggle a reusable account lease");
    }

    {
        host_fixture fixture;
        fixture.Host.enroll_profile(core::host_profile_id{11U}, credential());
        const auto profile = fixture.Provider.Profiles.front().Request;
        fixture.Provider.Profiles.front().Completion(
            application::github_profile_success{
                .Request = profile,
                .Authority = native_profile(profile.Context.Profile),
            });
        fixture.Lane.run_next();
        const auto bootstrap = fixture.Provider.Viewers.front().Request;
        fixture.Provider.Viewers.front().Completion(
            application::github_viewer_success{
                .Request = bootstrap,
                .Viewer = viewer(),
                .RateLimits = rate_limits_for(
                    application::github_rate_limit_resource::core),
                .AccountTransport = {},
            });
        fixture.Lane.run_next();
        const auto store = fixture.Vault.Stores.front().Context;
        fixture.Vault.Stores.front().Completion(
            application::github_credential_store_success{
                .Context = store,
                .Disposition =
                    application::github_credential_store_disposition::stored,
            });
        fixture.Lane.run_next();
        const auto retrieve = fixture.Vault.Retrieves.front().Context;
        fixture.Vault.Retrieves.front().Completion(
            application::github_credential_retrieve_success{
                .Context = retrieve,
                .Credential = credential(),
            });
        fixture.Lane.run_next();
        const auto account = fixture.Provider.Viewers.back().Request;
        fixture.Provider.Viewers.back().Completion(
            application::github_viewer_success{
                .Request = account,
                .Viewer = viewer(core::github_account_id{52U}),
                .RateLimits = rate_limits_for(
                    application::github_rate_limit_resource::core),
                .AccountTransport = std::make_shared<fake_lease>(),
            });
        fixture.Lane.run_next();
        expect(
            fixture.Provider.Capabilities.empty()
                && fixture.Vault.Deletes.size() == 1U
                && fixture.Vault.Deletes.front().Context.Authority.Account
                       == core::github_account_id{51U},
            "changed second /user identity triggers exact provisional cleanup");
        const auto cleanup = fixture.Vault.Deletes.front().Context;
        fixture.Vault.Deletes.front().Completion(
            application::github_credential_delete_success{
                .Context = cleanup,
                .Disposition =
                    application::github_credential_delete_disposition::deleted,
            });
        fixture.Lane.run_next();
        expect(!fixture.Host.accepted_session().has_value()
                   && !fixture.Host.state().PendingSession.has_value(),
               "changed account never becomes an accepted core session");
    }
}

auto test_shutdown_cancels_and_suppresses_late_completion() -> void
{
    host_fixture fixture;
    fixture.Host.dispatch(core::activate_host_profile_intent{
        .Profile = core::host_profile_id{11U},
    });
    const auto request = fixture.Provider.Profiles.front().Request;
    const auto projection_count = fixture.Projections.Outputs.size();
    fixture.Host.shutdown();
    expect(fixture.Host.is_closing() && fixture.Host.is_quiescent()
               && fixture.Provider.CancelledProfiles == std::vector{request}
               && !fixture.Host.state().ActiveProfile.has_value(),
           "shutdown closes ingress, invalidates core, and cancels exact work");
    fixture.Provider.Profiles.front().Completion(
        application::github_profile_success{
            .Request = request,
            .Authority = cli_profile(request.Context.Profile),
        });
    fixture.Lane.run_next();
    expect(fixture.Provider.Viewers.empty()
               && fixture.Projections.Outputs.size() == projection_count,
           "late completion after shutdown cannot publish or start new work");
}

auto test_shutdown_preserves_persistent_credentials() -> void
{
    static_assert(!std::is_trivially_copyable_v<
                  application::github_credential_binding_catalog>);
    static_assert(!std::is_copy_constructible_v<
                  application::github_credential_binding_catalog>);
    static_assert(!std::is_move_constructible_v<
                  application::github_credential_binding_catalog>);

    application::github_credential_binding_catalog bindings;
    host_fixture fixture{bindings};
    establish_native_session(fixture);
    expect(bindings.size() == 1U,
           "exact /user acceptance records one host-owned binding");
    fixture.Host.shutdown();
    expect(fixture.Vault.Deletes.empty() && fixture.Host.is_quiescent()
               && bindings.size() == 1U,
           "ordinary shutdown preserves accepted persistent credentials");
}

auto test_caller_cache_cannot_bypass_capability_probe() -> void
{
    static_assert(!std::is_constructible_v<
                  application::github_application_host,
                  core::github_state,
                  application::github_profile_port&,
                  application::github_viewer_port&,
                  application::github_capability_port&,
                  application::github_pull_request_port&,
                  application::github_credential_vault_port&,
                  application::github_credential_binding_catalog&,
                  application::github_capability_cache,
                  application::decision_executor&,
                  application::github_projection_port&>);

    auto untrusted = application::github_capability_cache::create({});
    expect(untrusted.has_value(), "hostile cache fixture creates");
    const auto profile = native_profile(core::host_profile_id{11U});
    const auto forged =
        untrusted->store(application::github_capability_probe_observation{
            .Authority =
                {
                    .Profile = profile.Profile,
                    .ProfileRevision = profile.ProfileRevision,
                    .Host = profile.Host,
                    .Account = core::github_account_id{51U},
                    .ServerContract = profile.ServerContract,
                    .ProbeContractRevision = profile.ProbeContractRevision,
                },
            .ObservedAt = application::github_freshness_epoch{1U},
            .Facts = capability_facts(),
            .SchemaFacts = schema_facts(),
        });
    expect(forged.stored() && untrusted->size() == 1U,
           "a standalone caller cache can contain syntactically valid facts");

    host_fixture fixture;
    advance_native_to_retrieve(fixture);
    const auto retrieve = fixture.Vault.Retrieves.front().Context;
    fixture.Vault.Retrieves.front().Completion(
        application::github_credential_retrieve_success{
            .Context = retrieve,
            .Credential = credential(),
        });
    fixture.Lane.run_next();
    const auto account = fixture.Provider.Viewers.back().Request;
    fixture.Provider.Viewers.back().Completion(
        application::github_viewer_success{
            .Request = account,
            .Viewer = viewer(),
            .RateLimits =
                rate_limits_for(application::github_rate_limit_resource::core),
            .AccountTransport = std::make_shared<fake_lease>(),
        });
    fixture.Lane.run_next();
    expect(fixture.Provider.Capabilities.size() == 1U
               && fixture.Host.capability_cache_size() == 0U
               && !fixture.Host.accepted_session().has_value(),
           "production host owns an empty cache and still performs the probe");
}

auto test_catalog_resume_and_replacement_revalidate_persistent_credentials()
    -> void
{
    {
        application::github_credential_binding_catalog bindings;
        const auto authority = seed_persistent_binding(bindings);
        host_fixture resumed{bindings};
        const auto capability =
            advance_resume_to_capability(resumed, authority);
        resumed.Provider.Capabilities.front().Completion(
            application::github_capability_success{
                .Request = capability,
                .Facts = capability_facts(),
                .SchemaFacts = schema_facts(),
                .RateLimits = rate_limits_for(
                    application::github_rate_limit_resource::graphql),
            });
        resumed.Lane.run_next();
        expect(resumed.Host.accepted_session().has_value()
                   && resumed.Vault.Stores.empty() && bindings.size() == 1U,
               "fresh host resumes only after exact account and capability "
               "validation");
    }

    {
        application::github_credential_binding_catalog bindings;
        static_cast<void>(seed_persistent_binding(bindings));
        host_fixture mismatched_profile{bindings};
        mismatched_profile.Host.resume_profile(core::host_profile_id{11U});
        const auto request =
            mismatched_profile.Provider.Profiles.front().Request;
        auto changed = native_profile(request.Context.Profile);
        changed.ProfileRevision = application::github_profile_revision{32U};
        mismatched_profile.Provider.Profiles.front().Completion(
            application::github_profile_success{
                .Request = request,
                .Authority = changed,
            });
        mismatched_profile.Lane.run_next();
        expect(mismatched_profile.Vault.Retrieves.empty()
                   && mismatched_profile.Vault.Deletes.size() == 1U
                   && bindings.size() == 1U,
               "profile revision replacement exact-deletes the retained "
               "authority");
        const auto cleanup = mismatched_profile.Vault.Deletes.front().Context;
        mismatched_profile.Vault.Deletes.front().Completion(
            application::github_credential_delete_success{
                .Context = cleanup,
                .Disposition =
                    application::github_credential_delete_disposition::deleted,
            });
        mismatched_profile.Lane.run_next();
        expect(
            bindings.size() == 0U
                && !mismatched_profile.Host.state().PendingSession.has_value(),
            "successful replacement cleanup forgets the trusted binding");
    }

    {
        application::github_credential_binding_catalog bindings;
        static_cast<void>(seed_persistent_binding(bindings));
        host_fixture changed_account{bindings};
        changed_account.Host.resume_profile(core::host_profile_id{11U});
        const auto profile = changed_account.Provider.Profiles.front().Request;
        changed_account.Provider.Profiles.front().Completion(
            application::github_profile_success{
                .Request = profile,
                .Authority = native_profile(profile.Context.Profile),
            });
        changed_account.Lane.run_next();
        const auto retrieve = changed_account.Vault.Retrieves.front().Context;
        changed_account.Vault.Retrieves.front().Completion(
            application::github_credential_retrieve_success{
                .Context = retrieve,
                .Credential = credential(),
            });
        changed_account.Lane.run_next();
        const auto account = changed_account.Provider.Viewers.front().Request;
        changed_account.Provider.Viewers.front().Completion(
            application::github_viewer_success{
                .Request = account,
                .Viewer = viewer(core::github_account_id{52U}),
                .RateLimits = rate_limits_for(
                    application::github_rate_limit_resource::core),
                .AccountTransport = std::make_shared<fake_lease>(),
            });
        changed_account.Lane.run_next();
        expect(changed_account.Provider.Capabilities.empty()
                   && changed_account.Vault.Deletes.size() == 1U
                   && changed_account.Vault.Deletes.front()
                              .Context.Authority.Account
                          == core::github_account_id{51U},
               "resumed /user account mismatch exact-deletes the selected "
               "authority");
    }

    {
        application::github_credential_binding_catalog bindings;
        const auto authority = seed_persistent_binding(bindings);
        host_fixture failed_probe{bindings};
        const auto capability =
            advance_resume_to_capability(failed_probe, authority);
        failed_probe.Provider.Capabilities.front().Completion(
            application::github_capability_error{
                .Request = capability,
                .Error =
                    {
                        .Failure =
                            application::github_provider_failure::permission,
                        .RetryAfterSeconds = std::nullopt,
                        .RateLimits = {},
                        .RateLimitCause = std::nullopt,
                    },
            });
        failed_probe.Lane.run_next();
        expect(
            failed_probe.Vault.Deletes.empty()
                && !failed_probe.Host.accepted_session().has_value()
                && !failed_probe.Host.state().PendingSession.has_value()
                && bindings.size() == 1U,
            "ordinary resume failure preserves the trusted persistent binding");

        failed_probe.Host.resume_profile(core::host_profile_id{11U});
        expect(failed_probe.Vault.Deletes.empty()
                   && failed_probe.Provider.Profiles.size() == 2U,
               "same-profile resume retry preserves rather than deletes its "
               "binding");
        const auto retry_profile =
            failed_probe.Provider.Profiles.back().Request;
        failed_probe.Provider.Profiles.back().Completion(
            application::github_profile_success{
                .Request = retry_profile,
                .Authority = native_profile(retry_profile.Context.Profile),
            });
        failed_probe.Lane.run_next();
        expect(failed_probe.Vault.Retrieves.size() == 2U
                   && failed_probe.Vault.Deletes.empty(),
               "same-profile retry reaches the exact retained vault authority");
        const auto retry_retrieve = failed_probe.Vault.Retrieves.back().Context;
        failed_probe.Vault.Retrieves.back().Completion(
            application::github_credential_retrieve_error{
                .Context = retry_retrieve,
                .Failure = application::github_credential_vault_failure::
                    operation_failed,
            });
        failed_probe.Lane.run_next();

        failed_probe.Host.dispatch(core::deactivate_github_identity_intent{});
        expect(failed_probe.Vault.Deletes.size() == 1U,
               "explicit invalidation after failure exact-deletes retained "
               "material");
        const auto cleanup = failed_probe.Vault.Deletes.front().Context;
        failed_probe.Vault.Deletes.front().Completion(
            application::github_credential_delete_success{
                .Context = cleanup,
                .Disposition =
                    application::github_credential_delete_disposition::deleted,
            });
        failed_probe.Lane.run_next();
        expect(bindings.size() == 0U,
               "explicit invalidation forgets the retained catalog authority");
    }

    {
        application::github_credential_binding_catalog bindings;
        const auto authority_a = seed_persistent_binding(
            bindings, core::host_profile_id{11U}, core::github_account_id{51U});
        const auto authority_b = seed_persistent_binding(
            bindings, core::host_profile_id{12U}, core::github_account_id{52U});
        host_fixture switch_profile{bindings};
        const auto capability =
            advance_resume_to_capability(switch_profile, authority_a);
        switch_profile.Provider.Capabilities.front().Completion(
            application::github_capability_success{
                .Request = capability,
                .Facts = capability_facts(),
                .SchemaFacts = schema_facts(),
                .RateLimits = rate_limits_for(
                    application::github_rate_limit_resource::graphql),
            });
        switch_profile.Lane.run_next();

        switch_profile.Host.resume_profile(core::host_profile_id{12U});
        expect(
            switch_profile.Vault.Deletes.size() == 1U
                && switch_profile.Vault.Deletes.front().Context.Authority
                       == authority_a
                && switch_profile.Provider.Profiles.size() == 1U,
            "switching retained profiles deletes the active authority first");
        const auto cleanup = switch_profile.Vault.Deletes.front().Context;
        switch_profile.Vault.Deletes.front().Completion(
            application::github_credential_delete_success{
                .Context = cleanup,
                .Disposition =
                    application::github_credential_delete_disposition::deleted,
            });
        switch_profile.Lane.run_next();
        expect(bindings.size() == 1U
                   && switch_profile.Provider.Profiles.size() == 2U,
               "successful predecessor cleanup starts the staged retained "
               "profile");

        const auto failed_b = switch_profile.Provider.Profiles.back().Request;
        switch_profile.Provider.Profiles.back().Completion(
            application::github_profile_error{
                .Request = failed_b,
                .Error =
                    {
                        .Failure =
                            application::github_provider_failure::permission,
                        .RetryAfterSeconds = std::nullopt,
                        .RateLimits = {},
                        .RateLimitCause = std::nullopt,
                    },
            });
        switch_profile.Lane.run_next();
        expect(!switch_profile.Host.state().PendingSession.has_value()
                   && switch_profile.Vault.Deletes.size() == 1U,
               "ordinary staged-profile failure preserves its trusted binding");

        switch_profile.Host.dispatch(core::deactivate_github_identity_intent{});
        expect(
            switch_profile.Vault.Deletes.size() == 2U
                && switch_profile.Vault.Deletes.back().Context.Authority
                       == authority_b,
            "later deactivation exact-deletes the staged retained authority");
    }

    {
        application::github_credential_binding_catalog bindings;
        const auto authority_a = seed_persistent_binding(
            bindings, core::host_profile_id{11U}, core::github_account_id{51U});
        const auto authority_b = seed_persistent_binding(
            bindings, core::host_profile_id{12U}, core::github_account_id{52U});
        host_fixture inflight_switch{bindings};
        inflight_switch.Host.resume_profile(core::host_profile_id{11U});
        const auto profile_a =
            inflight_switch.Provider.Profiles.front().Request;
        inflight_switch.Provider.Profiles.front().Completion(
            application::github_profile_success{
                .Request = profile_a,
                .Authority = native_profile(profile_a.Context.Profile),
            });
        inflight_switch.Lane.run_next();
        expect(inflight_switch.Vault.Retrieves.size() == 1U
                   && inflight_switch.Vault.Retrieves.front().Context.Authority
                          == authority_a,
               "profile A resume starts its exact retained retrieval");

        inflight_switch.Host.resume_profile(core::host_profile_id{12U});
        expect(inflight_switch.Vault.Deletes.empty()
                   && inflight_switch.Provider.Profiles.size() == 1U,
               "profile B waits for the running profile A vault terminal");
        const auto retrieve_a = inflight_switch.Vault.Retrieves.front().Context;
        inflight_switch.Vault.Retrieves.front().Completion(
            application::github_credential_retrieve_cancelled{
                .Context = retrieve_a,
            });
        inflight_switch.Lane.run_next();
        expect(inflight_switch.Vault.Deletes.size() == 1U
                   && inflight_switch.Vault.Deletes.front().Context.Authority
                          == authority_a
                   && bindings.size() == 2U,
               "deferred invalidation exact-deletes A rather than staged B");

        const auto cleanup_a = inflight_switch.Vault.Deletes.front().Context;
        inflight_switch.Vault.Deletes.front().Completion(
            application::github_credential_delete_success{
                .Context = cleanup_a,
                .Disposition =
                    application::github_credential_delete_disposition::deleted,
            });
        inflight_switch.Lane.run_next();
        expect(bindings.size() == 1U
                   && inflight_switch.Provider.Profiles.size() == 2U,
               "profile B starts only after A cleanup forgets A");

        const auto profile_b = inflight_switch.Provider.Profiles.back().Request;
        inflight_switch.Provider.Profiles.back().Completion(
            application::github_profile_error{
                .Request = profile_b,
                .Error =
                    {
                        .Failure =
                            application::github_provider_failure::permission,
                        .RetryAfterSeconds = std::nullopt,
                        .RateLimits = {},
                        .RateLimitCause = std::nullopt,
                    },
            });
        inflight_switch.Lane.run_next();
        inflight_switch.Host.dispatch(
            core::deactivate_github_identity_intent{});
        expect(inflight_switch.Vault.Deletes.size() == 2U
                   && inflight_switch.Vault.Deletes.back().Context.Authority
                          == authority_b,
               "failed B remains selected for later exact invalidation");
    }

    {
        application::github_credential_binding_catalog bindings;
        static_cast<void>(seed_persistent_binding(bindings));
        host_fixture replacement{bindings};
        replacement.Host.enroll_profile(core::host_profile_id{11U},
                                        credential());
        const auto profile = replacement.Provider.Profiles.front().Request;
        replacement.Provider.Profiles.front().Completion(
            application::github_profile_success{
                .Request = profile,
                .Authority = native_profile(profile.Context.Profile),
            });
        replacement.Lane.run_next();
        expect(replacement.Vault.Deletes.size() == 1U
                   && replacement.Provider.Viewers.empty(),
               "fresh enrollment deletes an existing trusted binding first");
        const auto cleanup = replacement.Vault.Deletes.front().Context;
        replacement.Vault.Deletes.front().Completion(
            application::github_credential_delete_success{
                .Context = cleanup,
                .Disposition =
                    application::github_credential_delete_disposition::deleted,
            });
        replacement.Lane.run_next();
        expect(bindings.size() == 0U
                   && replacement.Provider.Profiles.size() == 2U,
               "replacement cleanup resumes enrollment with no stale selector");
    }

    {
        application::github_credential_binding_catalog bindings;
        const auto authority = seed_persistent_binding(bindings);
        host_fixture failed_profile{bindings};
        failed_profile.Host.resume_profile(core::host_profile_id{11U});
        const auto resume = failed_profile.Provider.Profiles.front().Request;
        failed_profile.Provider.Profiles.front().Completion(
            application::github_profile_error{
                .Request = resume,
                .Error =
                    {
                        .Failure =
                            application::github_provider_failure::permission,
                        .RetryAfterSeconds = std::nullopt,
                        .RateLimits = {},
                        .RateLimitCause = std::nullopt,
                    },
            });
        failed_profile.Lane.run_next();
        expect(bindings.size() == 1U && failed_profile.Vault.Deletes.empty(),
               "profile failure preserves the retained binding for recovery");

        failed_profile.Host.enroll_profile(core::host_profile_id{12U},
                                           credential());
        expect(
            failed_profile.Vault.Deletes.size() == 1U
                && failed_profile.Vault.Deletes.front().Context.Authority
                       == authority,
            "later profile enrollment exact-deletes a failed resume binding");
        const auto cleanup = failed_profile.Vault.Deletes.front().Context;
        failed_profile.Vault.Deletes.front().Completion(
            application::github_credential_delete_success{
                .Context = cleanup,
                .Disposition =
                    application::github_credential_delete_disposition::deleted,
            });
        failed_profile.Lane.run_next();
        expect(bindings.size() == 0U
                   && failed_profile.Provider.Profiles.size() == 2U
                   && failed_profile.Provider.Profiles.back()
                              .Request.Context.Profile
                          == core::host_profile_id{12U},
               "profile switch continues only after exact retained cleanup");
    }

    {
        application::github_credential_binding_catalog bindings;
        const auto authority = seed_persistent_binding(bindings);
        host_fixture changed_transport{bindings};
        changed_transport.Host.resume_profile(core::host_profile_id{11U});
        const auto request =
            changed_transport.Provider.Profiles.front().Request;
        changed_transport.Provider.Profiles.front().Completion(
            application::github_profile_success{
                .Request = request,
                .Authority = cli_profile(request.Context.Profile),
            });
        changed_transport.Lane.run_next();
        expect(
            changed_transport.Vault.Deletes.size() == 1U
                && changed_transport.Vault.Deletes.front().Context.Authority
                       == authority
                && changed_transport.Provider.Viewers.empty(),
            "native-to-CLI replacement exact-deletes the old native binding");
    }

    {
        application::github_credential_binding_catalog bindings;
        static_cast<void>(seed_persistent_binding(bindings));
        host_fixture absent{bindings};
        absent.Host.resume_profile(core::host_profile_id{11U});
        const auto profile = absent.Provider.Profiles.front().Request;
        absent.Provider.Profiles.front().Completion(
            application::github_profile_success{
                .Request = profile,
                .Authority = native_profile(profile.Context.Profile),
            });
        absent.Lane.run_next();
        const auto retrieve = absent.Vault.Retrieves.front().Context;
        absent.Vault.Retrieves.front().Completion(
            application::github_credential_retrieve_absent{
                .Context = retrieve,
            });
        absent.Lane.run_next();
        expect(bindings.size() == 0U && absent.Vault.Deletes.empty()
                   && !absent.Host.state().PendingSession.has_value(),
               "authoritative vault absence forgets a stale binding without "
               "delete");
    }
}

auto test_shutdown_preserves_inflight_persistent_resume() -> void
{
    enum class retrieve_terminal
    {
        success,
        cancelled,
        error,
    };

    application::github_credential_binding_catalog bindings;
    static_cast<void>(seed_persistent_binding(bindings));

    for(const auto terminal : {retrieve_terminal::success,
                               retrieve_terminal::cancelled,
                               retrieve_terminal::error})
    {
        host_fixture fixture{bindings};
        fixture.Host.resume_profile(core::host_profile_id{11U});
        const auto profile = fixture.Provider.Profiles.front().Request;
        fixture.Provider.Profiles.front().Completion(
            application::github_profile_success{
                .Request = profile,
                .Authority = native_profile(profile.Context.Profile),
            });
        fixture.Lane.run_next();
        const auto running = fixture.Vault.Retrieves.front().Context;

        fixture.Host.shutdown();
        expect(fixture.Vault.Cancellations == std::vector{running}
                   && fixture.Vault.Deletes.empty()
                   && !fixture.Host.is_quiescent(),
               "shutdown cancels persistent retrieval without deleting its "
               "binding");

        switch(terminal)
        {
        case retrieve_terminal::success:
            fixture.Vault.Retrieves.front().Completion(
                application::github_credential_retrieve_success{
                    .Context = running,
                    .Credential = credential(),
                });
            break;
        case retrieve_terminal::cancelled:
            fixture.Vault.Retrieves.front().Completion(
                application::github_credential_retrieve_cancelled{
                    .Context = running,
                });
            break;
        case retrieve_terminal::error:
            fixture.Vault.Retrieves.front().Completion(
                application::github_credential_retrieve_error{
                    .Context = running,
                    .Failure = application::github_credential_vault_failure::
                        operation_failed,
                });
            break;
        }
        fixture.Lane.run_next();
        expect(fixture.Vault.Deletes.empty() && fixture.Host.is_quiescent()
                   && bindings.size() == 1U,
               "every persistent retrieval terminal preserves the trusted "
               "binding");
    }
}

auto test_shutdown_cancels_running_vault_work() -> void
{
    enum class store_terminal
    {
        success,
        cancelled,
        error,
    };

    for(const auto terminal : {
            store_terminal::success,
            store_terminal::cancelled,
            store_terminal::error,
        })
    {
        host_fixture fixture;
        advance_native_to_store(fixture);
        const auto running = fixture.Vault.Stores.front().Context;
        const auto projection_count = fixture.Projections.Outputs.size();

        fixture.Host.shutdown();
        expect(fixture.Vault.Cancellations == std::vector{running}
                   && fixture.Vault.Deletes.empty()
                   && !fixture.Host.is_quiescent(),
               "shutdown cancels a running store without early delete");

        switch(terminal)
        {
        case store_terminal::success:
            fixture.Vault.Stores.front().Completion(
                application::github_credential_store_success{
                    .Context = running,
                    .Disposition = application::
                        github_credential_store_disposition::stored,
                });
            break;
        case store_terminal::cancelled:
            fixture.Vault.Stores.front().Completion(
                application::github_credential_store_cancelled{
                    .Context = running,
                });
            break;
        case store_terminal::error:
            fixture.Vault.Stores.front().Completion(
                application::github_credential_store_error{
                    .Context = running,
                    .Failure = application::github_credential_vault_failure::
                        operation_failed,
                });
            break;
        }
        expect(fixture.Vault.Deletes.empty(),
               "store terminal cannot clean up off the decision lane");
        fixture.Lane.run_next();
        expect(fixture.Vault.Deletes.size() == 1U
                   && fixture.Vault.Deletes.front().Context.Authority
                          == running.Authority
                   && fixture.Provider.Profiles.size() == 1U
                   && fixture.Projections.Outputs.size() == projection_count
                   && !fixture.Host.is_quiescent(),
               "every store terminal exact-cleans before shutdown quiesces");

        const auto cleanup = fixture.Vault.Deletes.front().Context;
        fixture.Vault.Deletes.front().Completion(
            application::github_credential_delete_success{
                .Context = cleanup,
                .Disposition =
                    application::github_credential_delete_disposition::deleted,
            });
        fixture.Lane.run_next();
        expect(fixture.Host.is_quiescent()
                   && fixture.Provider.Profiles.size() == 1U
                   && fixture.Projections.Outputs.size() == projection_count,
               "shutdown quiesces after provisional store cleanup terminates");
    }

    for(const auto terminal : {
            store_terminal::success,
            store_terminal::cancelled,
            store_terminal::error,
        })
    {
        host_fixture fixture;
        advance_native_to_retrieve(fixture);
        const auto running = fixture.Vault.Retrieves.front().Context;
        const auto projection_count = fixture.Projections.Outputs.size();

        fixture.Host.shutdown();
        expect(fixture.Vault.Cancellations == std::vector{running}
                   && fixture.Vault.Deletes.empty()
                   && !fixture.Host.is_quiescent(),
               "shutdown cancels a running retrieve without early delete");

        switch(terminal)
        {
        case store_terminal::success:
            fixture.Vault.Retrieves.front().Completion(
                application::github_credential_retrieve_success{
                    .Context = running,
                    .Credential = credential(),
                });
            break;
        case store_terminal::cancelled:
            fixture.Vault.Retrieves.front().Completion(
                application::github_credential_retrieve_cancelled{
                    .Context = running,
                });
            break;
        case store_terminal::error:
            fixture.Vault.Retrieves.front().Completion(
                application::github_credential_retrieve_error{
                    .Context = running,
                    .Failure = application::github_credential_vault_failure::
                        operation_failed,
                });
            break;
        }
        fixture.Lane.run_next();
        expect(
            fixture.Vault.Deletes.size() == 1U
                && fixture.Vault.Deletes.front().Context.Authority
                       == running.Authority
                && fixture.Provider.Viewers.size() == 1U
                && fixture.Projections.Outputs.size() == projection_count
                && !fixture.Host.is_quiescent(),
            "every retrieve terminal exact-cleans the provisional credential");

        const auto cleanup = fixture.Vault.Deletes.front().Context;
        fixture.Vault.Deletes.front().Completion(
            application::github_credential_delete_success{
                .Context = cleanup,
                .Disposition =
                    application::github_credential_delete_disposition::deleted,
            });
        fixture.Lane.run_next();
        expect(fixture.Host.is_quiescent()
                   && !fixture.Host.shutdown_cleanup_failed(),
               "retrieve shutdown cleanup succeeds before quiescence");
    }
}

auto test_shutdown_exact_cleans_every_provisional_phase() -> void
{
    {
        host_fixture fixture;
        advance_native_to_retrieve(fixture);
        const auto retrieve = fixture.Vault.Retrieves.front().Context;
        fixture.Vault.Retrieves.front().Completion(
            application::github_credential_retrieve_success{
                .Context = retrieve,
                .Credential = credential(),
            });
        fixture.Lane.run_next();
        const auto viewer_request = fixture.Provider.Viewers.back().Request;

        fixture.Host.shutdown();
        expect(
            fixture.Provider.CancelledViewers == std::vector{viewer_request}
                && fixture.Vault.Deletes.size() == 1U
                && !fixture.Host.is_quiescent(),
            "shutdown during account viewer starts exact provisional cleanup");
    }

    {
        host_fixture fixture;
        advance_native_to_retrieve(fixture);
        const auto retrieve = fixture.Vault.Retrieves.front().Context;
        fixture.Vault.Retrieves.front().Completion(
            application::github_credential_retrieve_success{
                .Context = retrieve,
                .Credential = credential(),
            });
        fixture.Lane.run_next();
        const auto viewer_request = fixture.Provider.Viewers.back().Request;
        fixture.Provider.Viewers.back().Completion(
            application::github_viewer_success{
                .Request = viewer_request,
                .Viewer = viewer(),
                .RateLimits = rate_limits_for(
                    application::github_rate_limit_resource::core),
                .AccountTransport = std::make_shared<fake_lease>(),
            });
        fixture.Lane.run_next();
        const auto capability_request =
            fixture.Provider.Capabilities.front().Request;

        fixture.Host.shutdown();
        expect(fixture.Provider.CancelledCapabilities
                       == std::vector{capability_request}
                   && fixture.Vault.Deletes.size() == 1U
                   && !fixture.Host.is_quiescent(),
               "shutdown during capability probe exact-cleans persisted "
               "enrollment");

        const auto cleanup = fixture.Vault.Deletes.front().Context;
        auto wrong = cleanup;
        wrong.Request = core::request_id{999U};
        fixture.Vault.Deletes.front().Completion(
            application::github_credential_delete_success{
                .Context = wrong,
                .Disposition =
                    application::github_credential_delete_disposition::deleted,
            });
        fixture.Lane.run_next();
        expect(fixture.Host.is_quiescent()
                   && fixture.Host.shutdown_cleanup_failed()
                   && fixture.Bindings.size() == 1U,
               "wrong cleanup terminal leaves a recoverable catalog tombstone");

        fixture.Vault.Deletes.front().Completion(
            application::github_credential_delete_success{
                .Context = cleanup,
                .Disposition =
                    application::github_credential_delete_disposition::deleted,
            });
        fixture.Lane.run_next();
        expect(fixture.Host.is_quiescent()
                   && fixture.Host.shutdown_cleanup_failed(),
               "a second terminal cannot rewrite failed shutdown cleanup");
    }

    {
        host_fixture fixture;
        establish_native_session(fixture);
        fixture.Host.dispatch(core::activate_host_profile_intent{
            .Profile = core::host_profile_id{12U},
        });
        const auto cleanup = fixture.Vault.Deletes.front().Context;
        fixture.Host.shutdown();
        expect(fixture.Vault.Cancellations.empty()
                   && fixture.Vault.Deletes.size() == 1U
                   && !fixture.Host.is_quiescent(),
               "shutdown preserves an exact delete already in flight");
        fixture.Vault.Deletes.front().Completion(
            application::github_credential_delete_error{
                .Context = cleanup,
                .Failure = application::github_credential_vault_failure::
                    operation_failed,
            });
        fixture.Lane.run_next();
        expect(
            fixture.Host.is_quiescent()
                && fixture.Host.shutdown_cleanup_failed()
                && fixture.Bindings.size() == 1U
                && !fixture.Projections.Diagnostics.empty()
                && fixture.Projections.Diagnostics.back().Reason
                       == core::github_reason_code::vault_unavailable,
            "terminal cleanup failure is distinct from successful quiescence");
    }
}

auto test_shutdown_cancel_has_no_completion_gate_lock_inversion() -> void
{
    host_fixture fixture;
    fixture.Host.dispatch(core::activate_host_profile_intent{
        .Profile = core::host_profile_id{11U},
    });
    fixture.Provider.CompleteProfileFromWorkerOnCancel = true;
    fixture.Host.shutdown();
    expect(fixture.Host.is_quiescent() && fixture.Lane.pending() == 1U,
           "worker callback can hand off while cancel waits during shutdown");
    fixture.Lane.drain();
}

auto test_destroyed_host_suppresses_late_callbacks_before_posting() -> void
{
    {
        fake_provider provider;
        fake_vault vault;
        application::manual_executor lane;
        recording_projections projections;
        application::github_credential_binding_catalog bindings;
        auto host = std::make_unique<application::github_application_host>(
            initial_state(),
            provider,
            provider,
            provider,
            provider,
            vault,
            bindings,
            lane,
            projections);
        host->dispatch(core::activate_host_profile_intent{
            .Profile = core::host_profile_id{11U},
        });
        const auto request = provider.Profiles.front().Request;
        host.reset();

        provider.Profiles.front().Completion(
            application::github_profile_success{
                .Request = request,
                .Authority = cli_profile(request.Context.Profile),
            });
        expect(lane.pending() == 0U && provider.Viewers.empty(),
               "late provider terminal cannot post after host destruction");
    }

    {
        fake_provider provider;
        fake_vault vault;
        application::manual_executor lane;
        recording_projections projections;
        application::github_credential_binding_catalog bindings;
        auto host = std::make_unique<application::github_application_host>(
            initial_state(),
            provider,
            provider,
            provider,
            provider,
            vault,
            bindings,
            lane,
            projections);
        host->enroll_profile(core::host_profile_id{11U}, credential());
        const auto profile = provider.Profiles.front().Request;
        provider.Profiles.front().Completion(
            application::github_profile_success{
                .Request = profile,
                .Authority = native_profile(profile.Context.Profile),
            });
        lane.run_next();
        const auto bootstrap = provider.Viewers.front().Request;
        provider.Viewers.front().Completion(application::github_viewer_success{
            .Request = bootstrap,
            .Viewer = viewer(),
            .RateLimits =
                rate_limits_for(application::github_rate_limit_resource::core),
            .AccountTransport = {},
        });
        lane.run_next();
        const auto store = vault.Stores.front().Context;
        host.reset();

        vault.Stores.front().Completion(
            application::github_credential_store_success{
                .Context = store,
                .Disposition =
                    application::github_credential_store_disposition::stored,
            });
        expect(lane.pending() == 0U && vault.Deletes.empty()
                   && bindings.size() == 1U,
               "late stored secret retains an exact catalog tombstone");
    }
}
}

auto main() -> int
{
    test_enrollment_acceptance_snapshot_and_exact_delete();
    test_retries_are_bounded_and_stale_work_is_cancelled();
    test_retry_after_is_terminal_and_preserved();
    test_wrong_correlations_and_submit_exceptions_fail_closed();
    test_rate_observation_presence_follows_server_contract();
    test_no_graphql_read_rejects_graphql_error_observations();
    test_projection_failures_cannot_interrupt_effects_or_cleanup();
    test_projection_reentry_is_serialized_after_the_current_turn();
    test_invalid_enrollment_cannot_leak_into_a_later_activation();
    test_snapshot_authority_and_pr_cancellation_fail_closed();
    test_new_open_clears_host_snapshot_but_refresh_retains_it();
    test_vault_wrong_context_and_submit_throw_fail_closed();
    test_reenrollment_waits_for_running_vault_cleanup();
    test_bootstrap_lease_and_changed_account_fail_closed();
    test_shutdown_cancels_and_suppresses_late_completion();
    test_shutdown_preserves_persistent_credentials();
    test_caller_cache_cannot_bypass_capability_probe();
    test_catalog_resume_and_replacement_revalidate_persistent_credentials();
    test_shutdown_preserves_inflight_persistent_resume();
    test_shutdown_cancels_running_vault_work();
    test_shutdown_exact_cleans_every_provisional_phase();
    test_shutdown_cancel_has_no_completion_gate_lock_inversion();
    test_destroyed_host_suppresses_late_callbacks_before_posting();
    std::cout << "application github host tests passed\n";
    return EXIT_SUCCESS;
}
