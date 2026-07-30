#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
VERIFY_SCRIPT = REPOSITORY_ROOT / "cmake" / "VerifyArchitectureBoundaries.cmake"

ADAPTER_ROOT = """\
add_subdirectory(github)
"""

APPLE_KEYCHAIN_TARGET = """\
if(APPLE)
    find_library(BPR_TEST_SECURITY_FRAMEWORK Security REQUIRED)
    find_library(
        BPR_TEST_CORE_FOUNDATION_FRAMEWORK
        CoreFoundation
        REQUIRED
    )
    target_sources(
        better_pull_review_adapter_github
        INTERFACE
            src/macos_keychain.cpp
    )
    target_link_libraries(
        better_pull_review_adapter_github
        INTERFACE
            ${BPR_TEST_SECURITY_FRAMEWORK}
            ${BPR_TEST_CORE_FOUNDATION_FRAMEWORK}
    )
endif()
"""

WINDOWS_CREDENTIAL_TARGET = """\
if(WIN32)
    target_sources(
        better_pull_review_adapter_github
        INTERFACE
            src/windows_credential_manager.cpp
    )
    target_link_libraries(
        better_pull_review_adapter_github
        INTERFACE
            Advapi32
    )
endif()
"""

GITHUB_TARGET = """\
add_library(better_pull_review_adapter_github INTERFACE)
add_library(BetterPullReview::AdapterGitHub ALIAS better_pull_review_adapter_github)
target_link_libraries(
    better_pull_review_adapter_github
    INTERFACE
        BetterPullReview::Application
)
target_sources(
    better_pull_review_adapter_github
    INTERFACE
        src/github_cli_process_runner.cpp
        src/gh_cli_transport.cpp
        src/native_https_transport.cpp
)
""" + APPLE_KEYCHAIN_TARGET + WINDOWS_CREDENTIAL_TARGET

MACOS_KEYCHAIN_SOURCE = """\
#include <Security/Security.h>
#include <functional>
#include <map>
#include <mutex>

using callback = std::function<void()>;
std::map<int, callback> callbacks;
std::mutex callback_mutex;
namespace application { class effect_executor; }
application::effect_executor* selected_effect_lane = nullptr;

void selected_keychain_lifecycle() {
    SecKeychainRef keychain = nullptr;
    SecKeychainCopyDefault(&keychain);
    const void* query_contract[] = {
        kSecClassGenericPassword,
        kSecAttrService,
        kSecAttrAccount,
        kSecUseKeychain,
        kSecMatchSearchList,
        kSecUseAuthenticationUI,
        kSecUseAuthenticationUIFail,
        kSecReturnData,
        kSecMatchLimit,
        kSecMatchLimitOne,
    };
    SecItemAdd(query, nullptr);
    SecItemCopyMatching(query, nullptr);
    SecItemUpdate(query, attributes);
    SecItemDelete(query);
}
"""

WINDOWS_CREDENTIAL_MANAGER_SOURCE = """\
#include <wincred.h>

namespace application {
class effect_executor;
constexpr std::size_t maximum_github_credential_bytes = 2'560U;
}
application::effect_executor* selected_effect_lane = nullptr;

constexpr std::size_t windows_credential_manager_maximum_blob_bytes =
    5U * 512U;
static_assert(
    application::maximum_github_credential_bytes
    == windows_credential_manager_maximum_blob_bytes);

void selected_credential_manager_lifecycle() {
    CREDENTIALW credential{};
    PCREDENTIALW retrieved = nullptr;
    static_assert(159U <= CRED_MAX_GENERIC_TARGET_NAME_LENGTH);
    static_assert(
        2'560U == windows_credential_manager_maximum_blob_bytes);
    const DWORD blob_size = 512U;
    credential.Type = CRED_TYPE_GENERIC;
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    CredWriteW(&credential, 0U);
    CredReadW(target, CRED_TYPE_GENERIC, 0U, &retrieved);
    const DWORD status = GetLastError();
    CredDeleteW(target, CRED_TYPE_GENERIC, 0U);
    if(
        credential.CredentialBlobSize
        <= windows_credential_manager_maximum_blob_bytes)
    {
        SecureZeroMemory(credential.CredentialBlob, blob_size);
    }
    CredFree(retrieved);
}
"""


class ArchitectureBoundaryTests(unittest.TestCase):
    def run_check(
        self,
        sources: dict[str, str] | None = None,
        *,
        adapter_root: str = ADAPTER_ROOT,
        github_target: str = GITHUB_TARGET,
        omit_macos_source: bool = False,
        omit_windows_source: bool = False,
    ) -> subprocess.CompletedProcess[str]:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            files = {
                "CMakeLists.txt": "",
                "src/application/CMakeLists.txt": "",
                "src/adapters/CMakeLists.txt": adapter_root,
                "src/adapters/github/CMakeLists.txt": github_target,
                "src/adapters/qt/CMakeLists.txt": "",
            }
            if not omit_macos_source:
                files["src/adapters/github/src/macos_keychain.cpp"] = (
                    MACOS_KEYCHAIN_SOURCE
                )
            if not omit_windows_source:
                files[
                    "src/adapters/github/src/windows_credential_manager.cpp"
                ] = WINDOWS_CREDENTIAL_MANAGER_SOURCE
            files.update(sources or {})

            for relative_path, contents in files.items():
                path = root / relative_path
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(contents, encoding="utf-8")

            return subprocess.run(
                [
                    shutil.which("cmake") or "cmake",
                    f"-DBPR_ROOT={root}",
                    "-P",
                    str(VERIFY_SCRIPT),
                ],
                check=False,
                capture_output=True,
                text=True,
            )

    def assert_failure(
        self,
        expected_message: str,
        sources: dict[str, str] | None = None,
        **kwargs: object,
    ) -> None:
        result = self.run_check(sources, **kwargs)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn(expected_message, result.stdout + result.stderr)

    def test_designated_github_effect_sources_are_allowed(self) -> None:
        result = self.run_check(
            {
                "src/adapters/github/src/github_cli_process_runner.cpp":
                    "#include <QProcess>\nQProcess* process;\n",
                "src/adapters/github/src/gh_cli_transport.cpp":
                    "const char* setting = \"GH_TELEMETRY=0\";\n",
                "src/adapters/github/src/native_https_transport.cpp":
                    "#include <QNetworkAccessManager>\n"
                    "QNetworkAccessManager* manager;\n",
            }
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_qprocess_is_rejected_outside_designated_runner(self) -> None:
        self.assert_failure(
            "QProcess ownership violation",
            {"src/adapters/git/process.cpp": "QProcess* process;\n"},
        )

    def test_core_application_and_qt_reject_process_and_network_types(
        self,
    ) -> None:
        for region in ("core", "application", "adapters/qt"):
            for effect in ("QProcess", "QNetworkReply", "QJsonDocument"):
                with self.subTest(region=region, effect=effect):
                    self.assert_failure(
                        "Core/application/Qt effect-boundary violation",
                        {
                            f"src/{region}/hostile.cpp":
                                f"{effect}* forbidden_effect;\n"
                        },
                    )

    def test_core_and_qt_reject_raw_provider_data(self) -> None:
        for region in ("core", "adapters/qt"):
            for provider_data in (
                'const char* endpoint = "https://api.example.test";\n',
                'const char* header = "Authorization: Bearer canary";\n',
            ):
                with self.subTest(
                    region=region,
                    provider_data=provider_data,
                ):
                    expected = (
                        "Core/application standard-C++ boundary violation"
                        if region == "core"
                        and "Authorization" in provider_data
                        else "Provider data crossed the core/Qt boundary"
                    )
                    self.assert_failure(
                        expected,
                        {f"src/{region}/hostile.cpp": provider_data},
                    )
        self.assert_failure(
            "Provider data crossed the core/Qt boundary",
            {"src/adapters/qt/hostile.cpp": "QUrl provider_url;\n"},
        )

    def test_application_rejects_provider_json_libraries(self) -> None:
        self.assert_failure(
            "Core/application/Qt effect-boundary violation",
            {
                "src/application/hostile.cpp":
                    "#include <nlohmann/json.hpp>\n"
            },
        )

    def test_application_rejects_qt_value_types(self) -> None:
        self.assert_failure(
            "Core/application standard-C++ boundary violation",
            {
                "src/application/include/provider.hpp":
                    "#include <QString>\nQString provider_value;\n"
            },
        )

    def test_core_and_application_reject_github_adapter_includes(self) -> None:
        for region in ("core", "application"):
            with self.subTest(region=region):
                self.assert_failure(
                    "Core/application standard-C++ boundary violation",
                    {
                        f"src/{region}/hostile.cpp":
                            '#include "../adapters/github/src/'
                            'github_read_contract.hpp"\n'
                    },
                )

    def test_core_application_and_qt_reject_raw_provider_literals(
        self,
    ) -> None:
        for region in ("core", "application", "adapters/qt"):
            with self.subTest(region=region):
                expected = (
                    "Core/application standard-C++ boundary violation"
                    if region != "adapters/qt"
                    else "Provider data crossed the core/Qt boundary"
                )
                self.assert_failure(
                    expected,
                    {
                        f"src/{region}/hostile.cpp":
                            'const char* raw = "{\\"node_id\\":'
                            '\\"provider-id\\"}";\n'
                    },
                )

    def test_application_rejects_embedded_provider_authority_and_auth(
        self,
    ) -> None:
        for literal in (
            'const char* endpoint = "https://api.github.com";\n',
            'const char* auth = "Authorization: Bearer canary";\n',
        ):
            with self.subTest(literal=literal):
                self.assert_failure(
                    "Core/application standard-C++ boundary violation",
                    {"src/application/hostile.cpp": literal},
                )

    def test_qt_rejects_normalized_provider_headers(self) -> None:
        self.assert_failure(
            "Provider data crossed the core/Qt boundary",
            {
                "src/adapters/qt/provider.cpp":
                    '#include "bpr/application/github_provider_port.hpp"\n'
            },
        )

    def test_core_rejects_application_provider_headers(self) -> None:
        self.assert_failure(
            "Provider data crossed the core/Qt boundary",
            {
                "src/core/provider.cpp":
                    '#include "bpr/application/github_read_model.hpp"\n'
            },
        )

    def test_core_and_qt_reject_credential_vault_contract_types(self) -> None:
        forbidden_surfaces = (
            '#include "bpr/application/github_credential_vault.hpp"\n',
            "github_credential_material material;\n",
            "github_credential_enrollment enrollment;\n",
            "github_credential_vault_port* vault;\n",
            "github_credential_retrieve_success* success;\n",
            "github_credential_retrieve_completion* completion;\n",
            "github_credential_retrieve_completion_handler* handler;\n",
        )
        for region in ("core", "adapters/qt"):
            for surface in forbidden_surfaces:
                with self.subTest(region=region, surface=surface):
                    self.assert_failure(
                        "Credential-vault capability crossed the "
                        "core/Qt boundary",
                        {f"src/{region}/hostile.cpp": surface},
                    )

    def test_application_and_application_tests_may_name_vault_contract(
        self,
    ) -> None:
        contract_use = (
            '#include "bpr/application/github_credential_vault.hpp"\n'
            "github_credential_material material;\n"
            "github_credential_enrollment enrollment;\n"
            "github_credential_vault_port* vault;\n"
        )
        result = self.run_check(
            {
                "src/application/include/contract.hpp": contract_use,
                "tests/application/contract_tests.cpp": contract_use,
            }
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_application_target_cannot_link_qt_or_provider_adapter(self) -> None:
        self.assert_failure(
            "Application target must depend inward on Core only",
            {
                "src/application/CMakeLists.txt":
                    "target_link_libraries(app PRIVATE Qt6::Core)\n"
            },
        )

    def test_qt_target_cannot_link_provider_adapter(self) -> None:
        self.assert_failure(
            "Qt presentation adapter must not link the GitHub provider adapter",
            {
                "src/adapters/qt/CMakeLists.txt":
                    "target_link_libraries(qt PRIVATE "
                    "BetterPullReview::AdapterGitHub)\n"
            },
        )

    def test_designated_effect_source_must_belong_to_github_target(self) -> None:
        self.assert_failure(
            "Designated GitHub effect source",
            {
                "src/adapters/github/src/github_cli_process_runner.cpp":
                    "QProcess* process;\n"
            },
            github_target=GITHUB_TARGET.replace(
                "        src/github_cli_process_runner.cpp\n",
                "",
            ),
        )

    def test_network_manager_is_rejected_outside_designated_transport(self) -> None:
        self.assert_failure(
            "QNetworkAccessManager ownership violation",
            {
                "src/adapters/github/src/other_transport.cpp":
                    "QNetworkAccessManager* manager;\n"
            },
        )

    def test_all_qt_network_and_tls_types_are_owned_by_native_https(self) -> None:
        for effect in ("QNetworkReply", "QSslCertificate"):
            with self.subTest(effect=effect):
                self.assert_failure(
                    "Qt network/TLS type ownership violation",
                    {
                        "src/adapters/github/src/other.cpp":
                            f"{effect}* forbidden_effect;\n"
                    },
                )

    def test_qt_adapter_cannot_own_provider_effects(self) -> None:
        self.assert_failure(
            "Core/application/Qt effect-boundary violation",
            {"src/adapters/qt/provider.cpp": "QProcess* process;\n"},
        )

    def test_telemetry_hardening_is_rejected_outside_cli_transport(self) -> None:
        self.assert_failure(
            "GitHub CLI telemetry hardening ownership violation",
            {
                "src/application/src/provider.cpp":
                    "const char* setting = \"GH_TELEMETRY=0\";\n"
            },
        )

    def test_non_disabling_telemetry_setting_is_rejected_in_transport(
        self,
    ) -> None:
        self.assert_failure(
            "Forbidden telemetry surface",
            {
                "src/adapters/github/src/gh_cli_transport.cpp":
                    "const char* setting = \"GH_TELEMETRY=1\";\n"
            },
        )

    def test_cli_transport_source_requires_telemetry_hardening(self) -> None:
        self.assert_failure(
            "must set GH_TELEMETRY=0",
            {
                "src/adapters/github/src/gh_cli_transport.cpp":
                    "const char* setting = \"GH_PROMPT_DISABLED=1\";\n"
            },
        )

    def test_future_linux_vault_source_is_not_required(self) -> None:
        result = self.run_check()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_windows_credential_manager_source_is_required(self) -> None:
        self.assert_failure(
            "designated Windows Credential Manager source is required",
            omit_windows_source=True,
        )

    def test_windows_source_must_be_owned_by_win32_target(self) -> None:
        self.assert_failure(
            "Windows GitHub adapter must compile",
            github_target=GITHUB_TARGET.replace(
                "            src/windows_credential_manager.cpp\n",
                "",
            ),
        )

    def test_win32_target_requires_advapi32(self) -> None:
        self.assert_failure(
            "must link Advapi32",
            github_target=GITHUB_TARGET.replace(
                "            Advapi32\n",
                "",
            ),
        )

    def test_windows_source_requires_a_win32_target_branch(self) -> None:
        self.assert_failure(
            "must define a WIN32 branch",
            github_target=GITHUB_TARGET.replace(
                WINDOWS_CREDENTIAL_TARGET,
                "",
            ),
        )

    def test_selected_windows_credential_manager_source_is_allowed(
        self,
    ) -> None:
        result = self.run_check()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_windows_vault_requires_the_effect_executor(self) -> None:
        self.assert_failure(
            "requires the host effect executor",
            {
                "src/adapters/github/src/windows_credential_manager.cpp":
                    WINDOWS_CREDENTIAL_MANAGER_SOURCE.replace(
                        "effect_executor",
                        "removed_executor",
                    )
            },
        )

    def test_windows_vault_rejects_the_decision_executor(self) -> None:
        self.assert_failure(
            "Forbidden serialized decision-executor surface",
            {
                "src/adapters/github/src/windows_credential_manager.cpp":
                    WINDOWS_CREDENTIAL_MANAGER_SOURCE
                    + "application::decision_executor* "
                    "forbidden_decision_lane = nullptr;\n"
            },
        )

    def test_windows_vault_requires_selected_wide_lifecycle(self) -> None:
        required_lines = (
            "#include <wincred.h>\n",
            "    CredWriteW(&credential, 0U);\n",
            "    CredReadW(target, CRED_TYPE_GENERIC, 0U, &retrieved);\n",
            "    CredDeleteW(target, CRED_TYPE_GENERIC, 0U);\n",
            "    CredFree(retrieved);\n",
            "    const DWORD status = GetLastError();\n",
            "    SecureZeroMemory(credential.CredentialBlob, blob_size);\n",
        )
        for required_line in required_lines:
            with self.subTest(required_line=required_line):
                self.assert_failure(
                    "selected wide-character",
                    {
                        "src/adapters/github/src/"
                        "windows_credential_manager.cpp":
                            WINDOWS_CREDENTIAL_MANAGER_SOURCE.replace(
                                required_line,
                                "",
                            )
                    },
                )

    def test_windows_vault_enforces_selected_supported_upper_bounds(
        self,
    ) -> None:
        bound_mutations = (
            (
                "    static_assert(159U <= "
                "CRED_MAX_GENERIC_TARGET_NAME_LENGTH);\n",
                "    static_assert(159U == "
                "CRED_MAX_GENERIC_TARGET_NAME_LENGTH);\n",
            ),
            (
                "constexpr std::size_t "
                "windows_credential_manager_maximum_blob_bytes =\n"
                "    5U * 512U;\n",
                "",
            ),
            (
                "    application::maximum_github_credential_bytes\n"
                "    == windows_credential_manager_maximum_blob_bytes);\n",
                "",
            ),
            (
                "        credential.CredentialBlobSize\n"
                "        <= windows_credential_manager_maximum_blob_bytes)\n",
                "",
            ),
        )
        for bound_line, replacement in bound_mutations:
            with self.subTest(bound_line=bound_line):
                self.assert_failure(
                    "Windows credential bound missing",
                    {
                        "src/adapters/github/src/"
                        "windows_credential_manager.cpp":
                            WINDOWS_CREDENTIAL_MANAGER_SOURCE.replace(
                                bound_line,
                                replacement,
                            )
                    },
                )

    def test_windows_vault_requires_generic_local_bounded_properties(
        self,
    ) -> None:
        required_constants = (
            "CRED_TYPE_GENERIC",
            "CRED_PERSIST_LOCAL_MACHINE",
            "CRED_MAX_GENERIC_TARGET_NAME_LENGTH",
            "windows_credential_manager_maximum_blob_bytes",
        )
        for required_constant in required_constants:
            with self.subTest(required_constant=required_constant):
                self.assert_failure(
                    "bounded credential property",
                    {
                        "src/adapters/github/src/"
                        "windows_credential_manager.cpp":
                            WINDOWS_CREDENTIAL_MANAGER_SOURCE.replace(
                                required_constant,
                                "REMOVED_REQUIRED_PROPERTY",
                            )
                    },
                )

    def test_windows_vault_rejects_toolchain_varying_legacy_blob_bound(
        self,
    ) -> None:
        self.assert_failure(
            "Forbidden toolchain-varying legacy credential-blob bound",
            {
                "src/adapters/github/src/windows_credential_manager.cpp":
                    WINDOWS_CREDENTIAL_MANAGER_SOURCE
                    + "static_assert(512U <= "
                    "CRED_MAX_CREDENTIAL_BLOB_SIZE);\n"
            },
        )

    def test_windows_vault_rejects_ansi_and_neutral_aliases(self) -> None:
        forbidden_aliases = (
            "CredWrite(&credential, 0U);\n",
            "CredWriteA(&credential, 0U);\n",
            "CredRead(target, 1U, 0U, &retrieved);\n",
            "CredReadA(target, 1U, 0U, &retrieved);\n",
            "CredDelete(target, 1U, 0U);\n",
            "CredDeleteA(target, 1U, 0U);\n",
            "CREDENTIAL forbidden_neutral{};\n",
            "CREDENTIALA forbidden_ansi{};\n",
            "PCREDENTIAL forbidden_neutral_pointer = nullptr;\n",
            "PCREDENTIALA forbidden_ansi_pointer = nullptr;\n",
        )
        for forbidden_alias in forbidden_aliases:
            with self.subTest(forbidden_alias=forbidden_alias):
                self.assert_failure(
                    "Forbidden ANSI/encoding-neutral Credential Manager "
                    "surface",
                    {
                        "src/adapters/github/src/"
                        "windows_credential_manager.cpp":
                            WINDOWS_CREDENTIAL_MANAGER_SOURCE
                            + forbidden_alias
                    },
                )

    def test_windows_vault_rejects_unselected_credential_apis(self) -> None:
        forbidden_apis = (
            "CredUIPromptForWindowsCredentialsW();\n",
            "CredEnumerateW(nullptr, 0U, nullptr, nullptr);\n",
            "CredFindBestCredentialW(nullptr, 1U, 0U, nullptr);\n",
            "CredGetTargetInfoW(nullptr, 0U, nullptr);\n",
            "CredReadDomainCredentialsW(nullptr, 0U, nullptr, nullptr);\n",
            "CredWriteDomainCredentialsW(nullptr, nullptr, 0U);\n",
            "CredIsProtectedW(nullptr, nullptr);\n",
            "CredProtectW(FALSE, nullptr, 0U, nullptr, nullptr, nullptr);\n",
            "CredUnprotectW(FALSE, nullptr, 0U, nullptr, nullptr);\n",
            "CredRenameW(nullptr, nullptr, 1U, 0U);\n",
            "CredMarshalCredentialW(1U, nullptr, nullptr);\n",
            "CredUnmarshalCredentialW(nullptr, nullptr, nullptr);\n",
        )
        for forbidden_api in forbidden_apis:
            with self.subTest(forbidden_api=forbidden_api):
                self.assert_failure(
                    "Forbidden unselected Credential Manager surface",
                    {
                        "src/adapters/github/src/"
                        "windows_credential_manager.cpp":
                            WINDOWS_CREDENTIAL_MANAGER_SOURCE
                            + forbidden_api
                    },
                )

    def test_windows_vault_rejects_unselected_credential_effects(self) -> None:
        self.assert_failure(
            "credential effect outside",
            {
                "src/adapters/github/src/windows_credential_manager.cpp":
                    WINDOWS_CREDENTIAL_MANAGER_SOURCE
                    + "CredGetSessionTypes(nullptr, 0U);\n"
            },
        )

    def test_windows_vault_rejects_dpapi_and_credential_locker(self) -> None:
        hostile_surfaces = (
            ("CryptProtectData(nullptr, nullptr, nullptr, nullptr, nullptr, "
             "0U, nullptr);\n", "DPAPI"),
            ("CryptUnprotectData(nullptr, nullptr, nullptr, nullptr, nullptr, "
             "0U, nullptr);\n", "DPAPI"),
            ("NCryptProtectSecret(nullptr, 0U, nullptr, 0U, nullptr, nullptr, "
             "nullptr, nullptr);\n", "DPAPI"),
            ("PasswordVault forbidden_vault;\n", "Credential Locker"),
            ("Windows::Security::Credentials::PasswordCredential "
             "forbidden_credential;\n", "Credential Locker"),
        )
        for hostile_source, surface_name in hostile_surfaces:
            with self.subTest(surface_name=surface_name):
                self.assert_failure(
                    f"Forbidden {surface_name} surface",
                    {
                        "src/adapters/github/src/"
                        "windows_credential_manager.cpp":
                            WINDOWS_CREDENTIAL_MANAGER_SOURCE
                            + hostile_source
                    },
                )

    def test_windows_vault_rejects_alternate_storage_policy(self) -> None:
        hostile_surfaces = (
            ("CRED_PERSIST_SESSION", "non-local Credential Manager "
             "persistence"),
            ("CRED_PERSIST_ENTERPRISE", "non-local Credential Manager "
             "persistence"),
            ("CRED_PRESERVE_CREDENTIAL_BLOB", "credential-blob "
             "preservation"),
            ("CRED_TYPE_DOMAIN_PASSWORD", "non-generic credential type"),
        )
        for hostile_constant, surface_name in hostile_surfaces:
            with self.subTest(hostile_constant=hostile_constant):
                self.assert_failure(
                    f"Forbidden {surface_name} surface",
                    {
                        "src/adapters/github/src/"
                        "windows_credential_manager.cpp":
                            WINDOWS_CREDENTIAL_MANAGER_SOURCE
                            + f"const DWORD forbidden = {hostile_constant};\n"
                    },
                )

    def test_windows_vault_rejects_fallback_effect_families(self) -> None:
        hostile_surfaces = (
            ("RegSetValueExW(nullptr, nullptr, 0U, 0U, nullptr, 0U);\n",
             "registry"),
            ("CreateFileW(nullptr, 0U, 0U, nullptr, 0U, 0U, nullptr);\n",
             "filesystem"),
            ("sqlite3* database;\n", "settings/database"),
            ("GetEnvironmentVariableW(nullptr, nullptr, 0U);\n",
             "environment"),
            ("CreateProcessW(nullptr, nullptr, nullptr, nullptr, FALSE, 0U, "
             "nullptr, nullptr, nullptr, nullptr);\n", "command/process"),
            ("WinHttpOpen(nullptr, 0U, nullptr, nullptr, 0U);\n", "network"),
            ("MessageBoxW(nullptr, nullptr, nullptr, 0U);\n", "Qt/UI"),
            ("OutputDebugStringW(nullptr);\n", "logging/printing"),
        )
        for hostile_source, surface_name in hostile_surfaces:
            with self.subTest(surface_name=surface_name):
                self.assert_failure(
                    f"Forbidden {surface_name} surface",
                    {
                        "src/adapters/github/src/"
                        "windows_credential_manager.cpp":
                            WINDOWS_CREDENTIAL_MANAGER_SOURCE
                            + hostile_source
                    },
                )

    def test_windows_vault_rejects_raw_error_formatting(self) -> None:
        forbidden_formatters = (
            "FormatMessageW(0U, nullptr, status, 0U, nullptr, 0U, nullptr);\n",
            "std::to_string(status);\n",
            "snprintf(buffer, sizeof(buffer), \"%lu\", status);\n",
        )
        for formatter in forbidden_formatters:
            with self.subTest(formatter=formatter):
                self.assert_failure(
                    "Forbidden raw operating-system error formatting surface",
                    {
                        "src/adapters/github/src/"
                        "windows_credential_manager.cpp":
                            WINDOWS_CREDENTIAL_MANAGER_SOURCE + formatter
                    },
                )

    def test_macos_keychain_source_is_required(self) -> None:
        self.assert_failure(
            "designated macOS Keychain source is required",
            omit_macos_source=True,
        )

    def test_macos_keychain_source_must_be_owned_by_apple_target(self) -> None:
        self.assert_failure(
            "Apple GitHub adapter must compile src/macos_keychain.cpp",
            github_target=GITHUB_TARGET.replace(
                "            src/macos_keychain.cpp\n",
                "",
            ),
        )

    def test_apple_target_requires_native_frameworks(self) -> None:
        hostile_targets = (
            (
                GITHUB_TARGET.replace(
                    "    find_library(BPR_TEST_SECURITY_FRAMEWORK "
                    "Security REQUIRED)\n",
                    "",
                ),
                "must find Security with REQUIRED",
            ),
            (
                GITHUB_TARGET.replace(
                    "    find_library(\n"
                    "        BPR_TEST_CORE_FOUNDATION_FRAMEWORK\n"
                    "        CoreFoundation\n"
                    "        REQUIRED\n"
                    "    )\n",
                    "",
                ),
                "must find CoreFoundation with REQUIRED",
            ),
            (
                GITHUB_TARGET.replace(
                    "            ${BPR_TEST_SECURITY_FRAMEWORK}\n",
                    "",
                ),
                "must link the required Security framework",
            ),
            (
                GITHUB_TARGET.replace(
                    "            ${BPR_TEST_CORE_FOUNDATION_FRAMEWORK}\n",
                    "",
                ),
                "must link the required CoreFoundation framework",
            ),
        )
        for target, expected_message in hostile_targets:
            with self.subTest(expected_message=expected_message):
                self.assert_failure(
                    expected_message,
                    github_target=target,
                )

    def test_macos_keychain_requires_an_apple_target_branch(self) -> None:
        self.assert_failure(
            "must define an APPLE branch",
            github_target=GITHUB_TARGET.replace(APPLE_KEYCHAIN_TARGET, ""),
        )

    def test_selected_macos_keychain_source_is_allowed(self) -> None:
        result = self.run_check()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_macos_keychain_requires_the_effect_executor(self) -> None:
        self.assert_failure(
            "requires the host effect executor",
            {
                "src/adapters/github/src/macos_keychain.cpp":
                    MACOS_KEYCHAIN_SOURCE.replace(
                        "namespace application { class effect_executor; }\n"
                        "application::effect_executor* "
                        "selected_effect_lane = nullptr;\n",
                        "",
                    )
            },
        )

    def test_macos_keychain_rejects_the_decision_executor(self) -> None:
        self.assert_failure(
            "Forbidden serialized decision-executor surface",
            {
                "src/adapters/github/src/macos_keychain.cpp":
                    MACOS_KEYCHAIN_SOURCE
                    + "application::decision_executor* "
                    "forbidden_decision_lane = nullptr;\n"
            },
        )

    def test_macos_keychain_rejects_backend_switch_attributes(self) -> None:
        forbidden_attributes = (
            "kSecUseDataProtectionKeychain",
            "kSecAttrSynchronizable",
            "kSecAttrAccessGroup",
            "kSecAttrAccessible",
            "kSecAttrAccessibleWhenUnlocked",
        )
        for attribute in forbidden_attributes:
            with self.subTest(attribute=attribute):
                self.assert_failure(
                    "Forbidden Keychain backend-switch surface",
                    {
                        "src/adapters/github/src/macos_keychain.cpp":
                            MACOS_KEYCHAIN_SOURCE
                            + f"const void* forbidden = {attribute};\n"
                    },
                )

    def test_macos_keychain_requires_safe_query_properties(self) -> None:
        required_properties = (
            "kSecClassGenericPassword",
            "kSecAttrService",
            "kSecAttrAccount",
            "kSecUseKeychain",
            "kSecMatchSearchList",
            "kSecUseAuthenticationUI",
            "kSecUseAuthenticationUIFail",
            "kSecReturnData",
            "kSecMatchLimit",
            "kSecMatchLimitOne",
        )
        for query_property in required_properties:
            with self.subTest(query_property=query_property):
                self.assert_failure(
                    "must retain the exact-keychain",
                    {
                        "src/adapters/github/src/macos_keychain.cpp":
                            MACOS_KEYCHAIN_SOURCE.replace(
                                f"        {query_property},\n",
                                "",
                            )
                    },
                )

    def test_macos_keychain_rejects_expansive_query_policy(self) -> None:
        forbidden_properties = (
            "kSecClassInternetPassword",
            "kSecClassCertificate",
            "kSecReturnAttributes",
            "kSecReturnRef",
            "kSecReturnPersistentRef",
            "kSecMatchLimitAll",
            "kSecUseAuthenticationUIAllow",
        )
        for query_property in forbidden_properties:
            with self.subTest(query_property=query_property):
                self.assert_failure(
                    "Forbidden Keychain query-policy surface",
                    {
                        "src/adapters/github/src/macos_keychain.cpp":
                            MACOS_KEYCHAIN_SOURCE
                            + f"const void* forbidden = {query_property};\n"
                    },
                )

    def test_macos_keychain_rejects_unselected_keychain_effects(
        self,
    ) -> None:
        self.assert_failure(
            "Keychain effect outside the selected",
            {
                "src/adapters/github/src/macos_keychain.cpp":
                    MACOS_KEYCHAIN_SOURCE
                    + "SecKeychainGetStatus(keychain, &status);\n"
            },
        )

    def test_macos_keychain_rejects_raw_os_error_formatting(self) -> None:
        forbidden_formatters = (
            "SecCopyErrorMessageString(status, nullptr);\n",
            "std::to_string(status);\n",
            "snprintf(buffer, sizeof(buffer), \"%d\", status);\n",
        )
        for formatter in forbidden_formatters:
            with self.subTest(formatter=formatter):
                self.assert_failure(
                    "Forbidden raw operating-system error formatting surface",
                    {
                        "src/adapters/github/src/macos_keychain.cpp":
                            MACOS_KEYCHAIN_SOURCE + formatter
                    },
                )

    def test_macos_keychain_rejects_non_vault_effect_families(self) -> None:
        hostile_surfaces = (
            ("QWidget* widget;\n", "Qt/UI"),
            ("NSURLSession* session;\n", "network"),
            ("sqlite3* database;\n", "settings/database"),
            ("QSettings* settings;\n", "Qt/UI"),
            ("std::filesystem::path fallback;\n", "filesystem"),
            ("std::ofstream plaintext_file;\n", "filesystem"),
            ("write(descriptor, bytes, size);\n", "filesystem"),
            ("auto* value = getenv(\"BPR_TOKEN\");\n", "environment"),
            ("posix_spawn(nullptr, nullptr, nullptr, nullptr, nullptr, "
             "nullptr);\n", "command/process"),
            ("std::cerr << status;\n", "logging/printing"),
        )
        for hostile_source, surface_name in hostile_surfaces:
            with self.subTest(surface_name=surface_name):
                self.assert_failure(
                    f"Forbidden {surface_name} surface",
                    {
                        "src/adapters/github/src/macos_keychain.cpp":
                            MACOS_KEYCHAIN_SOURCE + hostile_source
                    },
                )

    def test_native_vault_apis_are_rejected_outside_designated_sources(
        self,
    ) -> None:
        hostile_apis = (
            ("SecItemAdd(query, nullptr);\n", "macOS Keychain API"),
            (
                "SecKeychainAddGenericPassword(nullptr, 0, nullptr);\n",
                "macOS Keychain API",
            ),
            ("CredWriteW(&credential, 0);\n", "Windows Credential Manager API"),
            (
                "secret_password_store_sync(schema, nullptr);\n",
                "Linux Secret Service API",
            ),
            (
                "secret_item_delete_sync(item, nullptr);\n",
                "Linux Secret Service API",
            ),
            (
                'auto service = "org.freedesktop.Secret.Service";\n',
                "Linux Secret Service API",
            ),
        )
        for source, ownership_message in hostile_apis:
            with self.subTest(source=source):
                self.assert_failure(
                    f"{ownership_message} ownership violation",
                    {"src/adapters/github/src/other_vault.cpp": source},
                )

    def test_designated_native_vault_sources_are_allowed(self) -> None:
        vault_target = GITHUB_TARGET.replace(
            "        src/native_https_transport.cpp\n",
            "        src/native_https_transport.cpp\n"
            "        src/linux_secret_service.cpp\n",
        )
        result = self.run_check(
            {
                "src/adapters/github/src/linux_secret_service.cpp":
                    "secret_password_store_sync(schema, nullptr);\n",
            },
            github_target=vault_target,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_adapter_root_must_delegate_target_ownership(self) -> None:
        self.assert_failure(
            "Adapter root must delegate",
            adapter_root="",
        )

    def test_github_target_cannot_be_declared_at_adapter_root(self) -> None:
        self.assert_failure(
            "must be declared only",
            adapter_root=(
                ADAPTER_ROOT
                + "add_library(better_pull_review_adapter_github INTERFACE)\n"
            ),
        )


if __name__ == "__main__":
    unittest.main()
