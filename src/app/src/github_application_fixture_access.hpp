#pragma once

#if !defined(BPR_BUILD_TESTING)
#error "The GitHub application fixture access is test-build only"
#endif

#include "bpr/app/github_application.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <filesystem>
#include <optional>
#include <string>

namespace bpr::app::detail
{
enum class github_public_path_fixture
{
    github_com,
    ghes_3_18,
};

enum class github_epoch_fixture_phase
{
    single,
    seed,
    reuse,
};

struct github_public_path_fixture_options
{
    std::optional<std::filesystem::path> ApplicationDataRoot;
    github_epoch_fixture_phase EpochPhase =
        github_epoch_fixture_phase::single;
};

struct github_public_path_fixture_evidence
{
    bool RecordingConsumed = false;
    bool CapabilityProbeConsumed = false;
    bool BootstrapTransportSelected = false;
    bool AccountTransportSelected = false;
    bool CredentialStored = false;
    bool CredentialRetrieved = false;
    bool CredentialDeleted = false;
    bool SecretRetained = false;
    int RecordingFailure = -1;
    int AccountReadCount = 0;
    int LastReadOperation = -1;
    int SmartAcquireCount = 0;
    int SmartDiscoveryCount = 0;
    int SmartLsRefsCount = 0;
    int SmartFetchCount = 0;
    std::string BaseOid;
    std::string HeadOneOid;
    std::string HeadTwoOid;
};

struct github_epoch_maintenance_fixture_evidence
{
    std::size_t PruneIntentDispatchCount = 0U;
    std::size_t DeleteIntentDispatchCount = 0U;
    std::size_t CancelIntentDispatchCount = 0U;
    std::int64_t CatalogRevision = 0;
    bool TargetBaseRefPresent = false;
    bool TargetHeadRefPresent = false;
    bool NonTargetBaseRefPresent = false;
    bool NonTargetHeadRefPresent = false;
    bool RepositoryPresent = false;
    bool CacheDeletionMarkerPresent = false;
    bool TargetBaseCacheDeleted = false;
    bool TargetHeadCacheDeleted = false;
    bool RetiredRepositoryPresent = false;
    bool RetiredCacheDeletionMarkerPresent = false;
    int TargetBaseAvailability = -1;
    int TargetHeadAvailability = -1;
};

class github_application_fixture_access final
{
public:
    [[nodiscard]] static auto make(
        github_public_path_fixture fixture,
        github_public_path_fixture_options options = {})
        -> std::unique_ptr<github_application>;
    [[nodiscard]] static auto evidence(
        const github_application& application) noexcept
        -> github_public_path_fixture_evidence;
    [[nodiscard]] static auto maintenance_evidence(
        const github_application& application,
        core::review_epoch_id target) noexcept
        -> github_epoch_maintenance_fixture_evidence;
};
}
