#pragma once

#if !defined(BPR_BUILD_TESTS)
#error "The GitHub application fixture access is test-build only"
#endif

#include "bpr/app/github_application.hpp"

#include <memory>

namespace bpr::app::detail
{
enum class github_public_path_fixture
{
    github_com,
    ghes_3_18,
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
};

class github_application_fixture_access final
{
public:
    [[nodiscard]] static auto make(github_public_path_fixture fixture)
        -> std::unique_ptr<github_application>;
    [[nodiscard]] static auto evidence(
        const github_application& application) noexcept
        -> github_public_path_fixture_evidence;
};
}
