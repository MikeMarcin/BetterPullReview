#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import subprocess
import sys
import unittest
from unittest import mock


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY_ROOT))

from scripts.contained_temporary_support import ContainedTemporaryDirectory  # noqa: E402

from scripts import verify_composed_hosts as VERIFIER  # noqa: E402


# Whether this platform has any tool that reports a linked binary's defined
# symbols. See test_real_inspector_output_contains_defined_symbols.
INSPECTOR_READS_A_LINKED_BINARY = sys.platform != "win32"


SHARED_LINES = {
    "GitHub application host constructor": "0001 T bpr::application::github_application_host::github_application_host()",
    "epoch application host constructor": "0002 T bpr::application::epoch_application_host::epoch_application_host()",
    "diff application host constructor": "0003 T bpr::application::diff_application_host::diff_application_host()",
    "workbench application host constructor": "0004 T bpr::application::workbench_application_host::workbench_application_host()",
    "review mutation application host constructor": "0008 T bpr::application::review_mutation_application_host::review_mutation_application_host()",
    "review mutation coordinator constructor": "0014 T bpr::application::review_mutation_coordinator::review_mutation_coordinator()",
    "review reconciliation decision": "0017 T bpr::application::reconcile()",
    "appearance application host constructor": "0005 T bpr::application::appearance_application_host::appearance_application_host()",
    "packaged syntax classifier constructor": "0006 T bpr::adapters::syntax::packaged_syntax_classifier::packaged_syntax_classifier()",
    "review ledger application host constructor": "0007 T bpr::application::review_ledger_application_host::review_ledger_application_host()",
    "defect correlation decision": "0009 T bpr::application::correlate()",
    "review queue application host constructor": "0010 T bpr::application::review_queue_application_host::review_queue_application_host()",
    "review queue notifier constructor": "0011 T bpr::application::review_queue_notifier::review_queue_notifier()",
    "agent invocation application host constructor": "0012 T bpr::application::agent_invocation_host::agent_invocation_host()",
    "agent process launcher constructor": "0020 T bpr::adapters::agent::agent_process_launcher::agent_process_launcher()",
    "Qt agent launch panel constructor": "0021 T bpr::qt::agent_launch_panel::agent_launch_panel()",
    "Qt workbench window constructor": "0013 T bpr::qt::workbench_window::workbench_window()",
    # BPR-0504, BPR-0514, BPR-0516, BPR-0526 anchors. The fixture trailed
    # SHARED_REQUIRED by seven for a day and this suite sat red at HEAD with
    # nobody running it (BPR-0529); the guard test below now refuses the gap.
    "agent finding application host constructor": "0022 T bpr::application::agent_finding_host::agent_finding_host()",
    "agent comment workshop application host constructor": "0023 T bpr::application::agent_comment_workshop_host::agent_comment_workshop_host()",
    "MCP context helper supervisor constructor": "0024 T bpr::application::mcp_context_helper_supervisor::mcp_context_helper_supervisor()",
    "Qt side chat panel constructor": "0025 T bpr::qt::side_chat_panel::side_chat_panel()",
    "Qt finding queue view constructor": "0026 T bpr::qt::finding_queue_view::finding_queue_view()",
    "agent chat session application host constructor": "0027 T bpr::application::agent_chat_session_host::agent_chat_session_host()",
    # [[BPR-1179]]. Agent-only, with the six above: forbidden without agent integrations.
    "agent protocol decoder constructor": "0037 T bpr::application::agent_protocol_decoder::agent_protocol_decoder(bpr::core::agent_protocol)",
    # [[BPR-1180]]. The Send to Agent adapter, an agent integration.
    "Send to Agent round sender constructor": "0039 T bpr::application::agent_round_sender::agent_round_sender(bpr::application::agent_process_port&, std::function<std::optional<std::string> (std::string const&)>)",
    # [[BPR-1179]]. Local automation: forbidden without it.
    "cycle owner service": "0038 T bpr::app::cycle_owner_service::serve(std::span<unsigned char const>)",
    "system browser opener constructor": "0028 T bpr::app::system_browser_opener::system_browser_opener()",
    "Qt device authorization panel constructor": "0029 T bpr::qt::device_authorization_panel::device_authorization_panel()",
    "anonymous read transport factory": "0030 T bpr::adapters::github::detail::make_native_https_anonymous_read_transport(bpr::adapters::github::validated_github_host_profile const&, bpr::core::github_session_request_context)",
    "anonymous smart transport factory": "0031 T bpr::adapters::github::detail::make_native_https_anonymous_smart_transport(bpr::adapters::github::validated_github_host_profile const&, bpr::core::github_session_request_context)",
    # BPR-0285: the anchor moved to the device host's own cross-library entry
    # point. A private one-caller composition function is not a linkage anchor:
    # MSVC inlines it, so a Windows Release cannot exhibit it, and on macOS it
    # survives only as a local symbol.
    "device refresh authority binding": "0032 T bpr::application::device_authorization_application_host::bind_refresh_authority(bpr::application::github_credential_authority_key, bpr::core::github_identity_generation)",
}
FIXTURE_LINES = {
    "fixture review mutation provider boundary": "0015 T bpr::app::detail::github_public_path_fixture_components::review_mutation_provider()",
    "fixture review mutation execution grant factory": "0016 T bpr::application::make_review_mutation_testing_grant()",
    "fixture one-shot review mutation execution grant factory": "0018 T bpr::application::make_review_mutation_one_shot_testing_grant()",
}
LIVE_PRODUCTION_LINES = {
    "live review mutation provider": (
        "0018 T bpr::adapters::github::live_review_mutation_provider::submit()"
    ),
    "live review mutation provider factory": (
        "0019 T bpr::adapters::github::make_live_review_mutation_provider()"
    ),
}
CLI_LINES = {
    "GitHub CLI transport": (
        "0033 T bpr::adapters::github::detail::gh_cli_transport::perform()"
    ),
    "GitHub CLI configuration validator": (
        "0034 T bpr::adapters::github::detail::validate_gh_cli_configuration()"
    ),
    "GitHub CLI request compiler": (
        "0035 T bpr::adapters::github::detail::compile_gh_cli_process_request()"
    ),
    "GitHub CLI process runner factory": (
        "0036 T bpr::adapters::github::detail::make_github_cli_process_runner()"
    ),
}
PRODUCTION_LINES = LIVE_PRODUCTION_LINES | (CLI_LINES if sys.platform == "darwin" else {})


def rendered(lines: dict[str, str]) -> str:
    return "\n".join(lines.values()) + "\n"


def audit_nm(lines: str, profile: str) -> list[str]:
    return VERIFIER.audit_symbols(
        VERIFIER.parse_nm_defined_symbols(lines),
        profile,
    )


def symbol_name(line: str) -> str:
    return line.split(" T ", maxsplit=1)[1]


def gnu_objdump_output(lines: dict[str, str]) -> str:
    records = "\n".join(
        f"0000000000001000 g     F .text 0000000000000010 {symbol_name(line)}"
        for line in lines.values()
    )
    return (
        "synthetic: file format elf64-x86-64\n\n"
        "SYMBOL TABLE:\n"
        f"{records}\n"
    )


MAP_DECORATED = {
    "GitHub application host constructor": "??0github_application_host@application@bpr@@QEAA@XZ",
    "review reconciliation decision": "?reconcile@application@bpr@@YA_NXZ",
    "live review mutation provider": "??_7live_review_mutation_provider@github@adapters@bpr@@6B@",
    "live review mutation provider factory": "?make_live_review_mutation_provider@github@adapters@bpr@@YAPEAUlive_review_mutation_provider@123@XZ",
}
MAP_DEMANGLED = {
    "??0github_application_host@application@bpr@@QEAA@XZ": "public: __cdecl bpr::application::github_application_host::github_application_host(void) __ptr64",
    "?reconcile@application@bpr@@YA_NXZ": "bool __cdecl bpr::application::reconcile(void)",
    "??_7live_review_mutation_provider@github@adapters@bpr@@6B@": "const bpr::adapters::github::live_review_mutation_provider::`vftable'",
    "?make_live_review_mutation_provider@github@adapters@bpr@@YAPEAUlive_review_mutation_provider@123@XZ": "struct bpr::adapters::github::live_review_mutation_provider * __ptr64 __cdecl bpr::adapters::github::make_live_review_mutation_provider(void)",
}
MAP_STATIC_SYMBOL = "?hidden@?A0x1234@@YAHH@Z"


def msvc_map_output(decorated: dict[str, str], *, statics: bool = True) -> str:
    """Render a linker map in the shape `link /MAP` writes for an x64 image."""
    publics = "\n".join(
        f" 0001:{index:08x}       {name} 00000001400{index:05x} f   probe.obj"
        for index, name in enumerate(decorated.values(), start=0x1000)
    )
    text = (
        " probe\n\n Timestamp is 6a8a69d7 (Sat Aug 22 20:32:39 2026)\n\n"
        " Preferred load address is 0000000140000000\n\n"
        " Start         Length     Name                   Class\n"
        " 0001:00000000 00001000H .text$mn                CODE\n\n"
        "  Address         Publics by Value              Rva+Base               Lib:Object\n\n"
        " 0000:00000000       __ImageBase                0000000140000000     <linker-defined>\n"
        f"{publics}\n"
        " 0001:00002000       main                       0000000140003000 f   probe.obj\n\n"
        " entry point at        0001:00002000\n"
    )
    if statics:
        text += (
            "\n Static symbols\n\n"
            f" 0001:00003000       {MAP_STATIC_SYMBOL}   0000000140004000 f   probe.obj\n"
        )
    return text


def undname_output(names: list[str]) -> str:
    lines = ["Microsoft (R) C++ Name Undecorator", ""]
    for name in names:
        lines.append(f'Undecoration of :- "{name}"')
        lines.append(f'is :- "{MAP_DEMANGLED.get(name, name)}"')
    return "\n".join(lines) + "\n"


class fake_toolchain:
    def __init__(
        self,
        tools: dict[str, str],
        results: list[subprocess.CompletedProcess[str]],
    ) -> None:
        self.Tools = tools
        self.Results = results
        self.Commands: list[list[str]] = []
        self.Inputs: list[str | None] = []

    def which(self, name: str) -> str | None:
        return self.Tools.get(name)

    def run(self, command: list[str], **kwargs: object) -> subprocess.CompletedProcess[str]:
        self.Commands.append(command)
        input_text = kwargs.get("input")
        self.Inputs.append(input_text if isinstance(input_text, str) else None)
        if not self.Results:
            raise AssertionError(f"unexpected command: {command}")
        return self.Results.pop(0)


def completed(
    stdout: str = "",
    *,
    returncode: int = 0,
) -> subprocess.CompletedProcess[str]:
    return subprocess.CompletedProcess([], returncode, stdout, "synthetic error")


class ComposedHostVerifierTests(unittest.TestCase):
    def test_the_fixture_names_every_shared_anchor(self) -> None:
        """A new SHARED_REQUIRED anchor must be added here in the same change.

        Otherwise every positive case below fails on the new name with a
        message about a synthetic binary, which is what happened for a day
        after BPR-0504 (BPR-0529).
        """
        missing = [
            anchor.name
            for anchor in VERIFIER.SHARED_REQUIRED
            if anchor.name not in SHARED_LINES
        ]
        self.assertEqual(missing, [])

    def setUp(self) -> None:
        self.TemporaryDirectory = ContainedTemporaryDirectory()
        self.Binary = Path(self.TemporaryDirectory.name) / "application"
        self.Binary.write_bytes(b"synthetic-binary")

    def tearDown(self) -> None:
        self.TemporaryDirectory.cleanup()

    def test_development_fixture_profile_accepts_all_exact_anchors(self) -> None:
        contents = rendered(SHARED_LINES | FIXTURE_LINES)
        self.assertEqual(
            audit_nm(
                contents,
                VERIFIER.DEVELOPMENT_FIXTURE_PROFILE,
            ),
            [],
        )

    def test_production_profile_accepts_exact_live_anchors(self) -> None:
        self.assertEqual(
            audit_nm(
                rendered(SHARED_LINES | PRODUCTION_LINES),
                VERIFIER.PRODUCTION_PROFILE,
            ),
            [],
        )

    def test_cli_linkage_is_required_only_in_macos_production(self) -> None:
        base = rendered(SHARED_LINES | LIVE_PRODUCTION_LINES)
        with mock.patch.object(VERIFIER.sys, "platform", "darwin"):
            missing = audit_nm(base, VERIFIER.PRODUCTION_PROFILE)
            self.assertEqual(len(missing), len(CLI_LINES))
            self.assertTrue(all("GitHub CLI" in failure for failure in missing))
            self.assertEqual(
                audit_nm(base + rendered(CLI_LINES), VERIFIER.PRODUCTION_PROFILE),
                [],
            )
        with mock.patch.object(VERIFIER.sys, "platform", "linux"):
            self.assertEqual(audit_nm(base, VERIFIER.PRODUCTION_PROFILE), [])
            present = audit_nm(base + rendered(CLI_LINES), VERIFIER.PRODUCTION_PROFILE)
            self.assertEqual(len(present), len(CLI_LINES))
            self.assertTrue(all("GitHub CLI" in failure for failure in present))

    def test_agent_free_image_passes_without_any_agent_anchor(self) -> None:
        """[[BPR-1179]]. An agent-free image is whole without the agent anchors."""
        agent_names = {anchor.name for anchor in VERIFIER.AGENT_INTEGRATION_REQUIRED}
        self.assertTrue(agent_names)
        self.assertTrue(agent_names <= SHARED_LINES.keys())
        common = {name: line for name, line in SHARED_LINES.items() if name not in agent_names}
        for profile, profile_lines in (
            (VERIFIER.DEVELOPMENT_FIXTURE_PROFILE, FIXTURE_LINES),
            (VERIFIER.PRODUCTION_PROFILE, PRODUCTION_LINES),
        ):
            with self.subTest(profile=profile):
                symbols = VERIFIER.parse_nm_defined_symbols(rendered(common | profile_lines))
                self.assertEqual(VERIFIER.audit_symbols(symbols, profile, agent_integrations=False), [])
                # The same image fails the full audit, once per absent agent anchor.
                missing = VERIFIER.audit_symbols(symbols, profile)
                self.assertEqual(len(missing), len(agent_names), missing)

    def test_agent_free_image_refuses_each_agent_anchor(self) -> None:
        """[[BPR-1179]]. One agent anchor left in an agent-free image fails it, by name."""
        agent_names = {anchor.name for anchor in VERIFIER.AGENT_INTEGRATION_REQUIRED}
        common = {name: line for name, line in SHARED_LINES.items() if name not in agent_names}
        for present in sorted(agent_names):
            with self.subTest(present=present):
                symbols = VERIFIER.parse_nm_defined_symbols(rendered(common | PRODUCTION_LINES | {present: SHARED_LINES[present]}))
                failures = VERIFIER.audit_symbols(symbols, VERIFIER.PRODUCTION_PROFILE, agent_integrations=False)
                self.assertEqual(failures, [f"forbidden exact demangled anchor is present: {present}"])

    def test_automation_off_image_refuses_the_automation_anchor(self) -> None:
        """[[BPR-1179]]. Local automation is a family of its own, independent of agents."""
        automation = {anchor.name for anchor in VERIFIER.LOCAL_AUTOMATION_REQUIRED}
        self.assertTrue(automation and automation <= SHARED_LINES.keys())
        without = {name: line for name, line in SHARED_LINES.items() if name not in automation}
        symbols = VERIFIER.parse_nm_defined_symbols(rendered(without | PRODUCTION_LINES))
        self.assertEqual(VERIFIER.audit_symbols(symbols, VERIFIER.PRODUCTION_PROFILE, local_automation=False), [])
        self.assertEqual(len(VERIFIER.audit_symbols(symbols, VERIFIER.PRODUCTION_PROFILE)), len(automation))
        present = VERIFIER.parse_nm_defined_symbols(rendered(SHARED_LINES | PRODUCTION_LINES))
        self.assertEqual(
            VERIFIER.audit_symbols(present, VERIFIER.PRODUCTION_PROFILE, local_automation=False),
            [f"forbidden exact demangled anchor is present: {name}" for name in sorted(automation)],
        )

    def test_reconciliation_is_shared_while_fixture_authorities_stay_forbidden(self) -> None:
        without_reconciliation = dict(SHARED_LINES)
        without_reconciliation.pop("review reconciliation decision")
        for profile, profile_lines in (
            (VERIFIER.DEVELOPMENT_FIXTURE_PROFILE, FIXTURE_LINES),
            (VERIFIER.PRODUCTION_PROFILE, PRODUCTION_LINES),
        ):
            with self.subTest(profile=profile, condition="reconciliation-required"):
                failures = audit_nm(
                    rendered(without_reconciliation | profile_lines),
                    profile,
                )
                self.assertTrue(
                    any("review reconciliation decision" in failure for failure in failures),
                    failures,
                )

        self.assertEqual(
            audit_nm(
                rendered(SHARED_LINES | PRODUCTION_LINES),
                VERIFIER.PRODUCTION_PROFILE,
            ),
            [],
        )
        for fixture_name, line in FIXTURE_LINES.items():
            with self.subTest(
                profile=VERIFIER.PRODUCTION_PROFILE,
                condition="fixture-authority-forbidden",
                fixture=fixture_name,
            ):
                failures = audit_nm(
                    rendered(SHARED_LINES | PRODUCTION_LINES) + line + "\n",
                    VERIFIER.PRODUCTION_PROFILE,
                )
                self.assertTrue(
                    any("forbidden" in failure for failure in failures),
                    failures,
                )

    def test_every_shared_anchor_is_required_in_both_profiles(self) -> None:
        for missing in SHARED_LINES:
            remaining = {
                name: line
                for name, line in SHARED_LINES.items()
                if name != missing
            }
            with self.subTest(profile="production", missing=missing):
                failures = audit_nm(
                    rendered(remaining | PRODUCTION_LINES),
                    VERIFIER.PRODUCTION_PROFILE,
                )
                self.assertTrue(any(missing in failure for failure in failures))
            with self.subTest(profile="development-fixture", missing=missing):
                failures = audit_nm(
                    rendered(remaining | FIXTURE_LINES),
                    VERIFIER.DEVELOPMENT_FIXTURE_PROFILE,
                )
                self.assertTrue(any(missing in failure for failure in failures))

    def test_every_fixture_anchor_is_required_by_development_profile(self) -> None:
        for missing in FIXTURE_LINES:
            remaining = {
                name: line
                for name, line in FIXTURE_LINES.items()
                if name != missing
            }
            with self.subTest(missing=missing):
                failures = audit_nm(
                    rendered(SHARED_LINES | remaining),
                    VERIFIER.DEVELOPMENT_FIXTURE_PROFILE,
                )
                self.assertTrue(any(missing in failure for failure in failures))

    def test_production_profile_forbids_each_fixture_authority(self) -> None:
        expected_forbidden_names = {
            "fixture review mutation provider boundary": (
                "GitHub public-path fixture components"
            ),
            "fixture review mutation execution grant factory": (
                "fixture review mutation execution grant factory"
            ),
            "fixture one-shot review mutation execution grant factory": (
                "fixture one-shot review mutation execution grant factory"
            ),
        }
        for fixture_name, line in FIXTURE_LINES.items():
            forbidden = expected_forbidden_names[fixture_name]
            with self.subTest(forbidden=forbidden):
                failures = audit_nm(
                    rendered(SHARED_LINES | PRODUCTION_LINES) + line + "\n",
                    VERIFIER.PRODUCTION_PROFILE,
                )
                self.assertTrue(
                    any(forbidden in failure for failure in failures),
                    failures,
                )

    def test_production_requires_and_fixture_forbids_each_live_anchor(self) -> None:
        for missing in LIVE_PRODUCTION_LINES:
            remaining = {
                name: line
                for name, line in PRODUCTION_LINES.items()
                if name != missing
            }
            with self.subTest(profile="production", missing=missing):
                failures = audit_nm(
                    rendered(SHARED_LINES | remaining),
                    VERIFIER.PRODUCTION_PROFILE,
                )
                self.assertTrue(
                    any(missing in failure for failure in failures),
                    failures,
                )
            with self.subTest(profile="development-fixture", forbidden=missing):
                failures = audit_nm(
                    rendered(SHARED_LINES | FIXTURE_LINES)
                    + LIVE_PRODUCTION_LINES[missing]
                    + "\n",
                    VERIFIER.DEVELOPMENT_FIXTURE_PROFILE,
                )
                self.assertTrue(any(missing in failure for failure in failures))

    def test_incidental_substring_does_not_satisfy_exact_anchor(self) -> None:
        lookalike = dict(SHARED_LINES)
        lookalike["GitHub application host constructor"] = (
            "0001 T bpr::application::github_application_host_backup::"
            "github_application_host()"
        )
        failures = audit_nm(
            rendered(lookalike | PRODUCTION_LINES),
            VERIFIER.PRODUCTION_PROFILE,
        )
        self.assertTrue(
            any("GitHub application host constructor" in failure for failure in failures)
        )

    def test_undefined_nm_records_cannot_satisfy_required_anchors(self) -> None:
        undefined = "\n".join(
            f"                 U {symbol_name(line)}"
            for line in SHARED_LINES.values()
        )
        with self.assertRaisesRegex(
            VERIFIER.verification_error,
            "zero defined symbols",
        ):
            VERIFIER.parse_nm_defined_symbols(undefined)

        failures = audit_nm(
            f"00000001 T unrelated_symbol()\n{undefined}\n",
            VERIFIER.PRODUCTION_PROFILE,
        )
        self.assertEqual(
            len(failures),
            len(VERIFIER.SHARED_REQUIRED)
            + len(VERIFIER.PRODUCTION_REQUIRED)
            + (len(VERIFIER.MAC_PRODUCTION_CLI_REQUIRED) if sys.platform == "darwin" else 0),
        )

    def test_embedded_template_reference_does_not_satisfy_function_anchor(self) -> None:
        embedded = dict(SHARED_LINES)
        embedded["defect correlation decision"] = (
            "0009 T void probe<bpr::application::correlate()>()"
        )
        failures = audit_nm(
            rendered(embedded | PRODUCTION_LINES),
            VERIFIER.PRODUCTION_PROFILE,
        )
        self.assertTrue(
            any("defect correlation decision" in failure for failure in failures)
        )

    def test_exact_function_anchor_accepts_overloaded_parameter_suffix(self) -> None:
        overloaded = dict(SHARED_LINES)
        overloaded["defect correlation decision"] = (
            "0009 T bpr::application::correlate(std::span<int const>)"
        )
        self.assertEqual(
            audit_nm(
                rendered(overloaded | PRODUCTION_LINES),
                VERIFIER.PRODUCTION_PROFILE,
            ),
            [],
        )

    def test_windows_declarations_normalize_after_calling_convention(self) -> None:
        windows_shared = dict(SHARED_LINES)
        windows_coordinator = (
            "0008 T public: __cdecl "
            "bpr::application::review_mutation_coordinator::"
            "review_mutation_coordinator(void)"
        )
        windows_shared["review mutation coordinator constructor"] = windows_coordinator
        windows_shared["defect correlation decision"] = (
            "0009 T struct bpr::application::defect_correlation_result __cdecl "
            "bpr::application::correlate(int)"
        )
        windows_shared["review reconciliation decision"] = (
            "0016 T struct bpr::application::reconciliation_result __cdecl "
            "bpr::application::reconcile(int)"
        )
        windows_fixture = {
            "fixture review mutation provider boundary": (
                "0014 T public: virtual class "
                "bpr::application::review_mutation_port & __cdecl "
                "bpr::app::detail::github_public_path_fixture_components::"
                "review_mutation_provider(void)"
            ),
            "fixture review mutation execution grant factory": (
                "0015 T class std::shared_ptr<int> __cdecl "
                "bpr::application::make_review_mutation_testing_grant(int)"
            ),
            "fixture one-shot review mutation execution grant factory": (
                "0017 T class std::shared_ptr<int> __cdecl "
                "bpr::application::make_review_mutation_one_shot_testing_grant(int)"
            ),
        }
        self.assertEqual(
            audit_nm(
                rendered(windows_shared | windows_fixture),
                VERIFIER.DEVELOPMENT_FIXTURE_PROFILE,
            ),
            [],
        )

        windows_production = {
            "live review mutation provider": (
                "0016 T public: virtual void __cdecl "
                "bpr::adapters::github::live_review_mutation_provider::submit(void)"
            ),
            "live review mutation provider factory": (
                "0017 T class std::expected<int, int> __cdecl "
                "bpr::adapters::github::make_live_review_mutation_provider(int)"
            ),
        }
        self.assertEqual(
            audit_nm(
                rendered(windows_shared | windows_production | (CLI_LINES if sys.platform == "darwin" else {})),
                VERIFIER.PRODUCTION_PROFILE,
            ),
            [],
        )

    def test_windows_normalization_does_not_search_return_or_template_text(self) -> None:
        lookalike = dict(SHARED_LINES)
        lookalike["defect correlation decision"] = (
            "0009 T struct probe<bpr::application::correlate(int)> __cdecl "
            "unrelated_decision(void)"
        )
        failures = audit_nm(
            rendered(lookalike | PRODUCTION_LINES),
            VERIFIER.PRODUCTION_PROFILE,
        )
        self.assertTrue(
            any("defect correlation decision" in failure for failure in failures)
        )

        authentic_probe = dict(SHARED_LINES)
        authentic_probe["defect correlation decision"] = (
            "0009 T void __cdecl bpr::application::probe<&struct ... __cdecl "
            "bpr::application::correlate(int)>(void)"
        )
        failures = audit_nm(
            rendered(authentic_probe | PRODUCTION_LINES),
            VERIFIER.PRODUCTION_PROFILE,
        )
        self.assertTrue(
            any("defect correlation decision" in failure for failure in failures)
        )

    def test_windows_normalization_skips_nested_return_conventions(self) -> None:
        nested_return = dict(SHARED_LINES)
        nested_return["defect correlation decision"] = (
            "0009 T void (__cdecl *)(int) __cdecl "
            "bpr::application::correlate(int)"
        )
        self.assertEqual(
            audit_nm(
                rendered(nested_return | PRODUCTION_LINES),
                VERIFIER.PRODUCTION_PROFILE,
            ),
            [],
        )

        operator_declarations = {
            "<": "bool __cdecl P::operator<(int)",
            ">": "bool __cdecl P::operator>(int)",
            "<=": "bool __cdecl P::operator<=(int)",
            ">=": "bool __cdecl P::operator>=(int)",
            "<=>": "int __cdecl P::operator<=>(int)",
            "<<": "P & __cdecl P::operator<<(int)",
            ">>": "P & __cdecl P::operator>>(int)",
            "<<=": "P & __cdecl P::operator<<=(int)",
            ">>=": "P & __cdecl P::operator>>=(int)",
            "->": "public: struct P * __cdecl P::operator->(void)",
            "->*": "public: int __cdecl P::operator->*(int)",
        }
        for operator, declaration in operator_declarations.items():
            with self.subTest(operator=operator):
                normalized = VERIFIER.normalize_symbol_name(declaration)
                self.assertEqual(normalized, declaration.split("__cdecl ", 1)[1])

    def test_windows_normalization_rejects_unbalanced_declarations(self) -> None:
        for declaration in (
            "void (__cdecl *)(int __cdecl bpr::application::correlate(int)",
            "void (__cdecl *)(int) __cdecl bpr::application::correlate(int",
            "void __cdecl bpr::application::correlate<int](void)",
            "public: int __cdecl P::broken>(int)",
        ):
            with self.subTest(declaration=declaration):
                with self.assertRaisesRegex(
                    VERIFIER.verification_error,
                    "unbalanced demangled declaration",
                ):
                    VERIFIER.normalize_symbol_name(declaration)

    def test_forbidden_type_matches_method_and_abi_metadata_only(self) -> None:
        fixture = VERIFIER.parse_nm_defined_symbols(
            rendered(SHARED_LINES | FIXTURE_LINES)
        )
        qualified = "bpr::adapters::github::live_review_mutation_provider"
        for candidate in (
            f"{qualified}::submit()",
            f"vtable for {qualified}",
            f"VTT for {qualified}",
            f"typeinfo for {qualified}",
            f"typeinfo name for {qualified}",
            f"non-virtual thunk to {qualified}::submit()",
            f"const {qualified}::`vftable'",
        ):
            with self.subTest(candidate=candidate):
                failures = VERIFIER.audit_symbols(
                    (*fixture, candidate),
                    VERIFIER.DEVELOPMENT_FIXTURE_PROFILE,
                )
                self.assertTrue(
                    any("live review mutation provider" in failure for failure in failures)
                )

        failures = VERIFIER.audit_symbols(
            (*fixture, f"{qualified}_backup::submit()"),
            VERIFIER.DEVELOPMENT_FIXTURE_PROFILE,
        )
        self.assertFalse(
            any("live review mutation provider" in failure for failure in failures)
        )

    def test_nm_parses_mach_gnu_and_coff_relevant_records(self) -> None:
        qualified = symbol_name(SHARED_LINES["GitHub application host constructor"])
        output = (
            f"0000000100001000 T {qualified}\n"
            f"0000000000401000 W {qualified}\n"
            "0000000140001000 T public: __cdecl "
            "bpr::application::github_application_host::github_application_host(void)\n"
            f"                 U {qualified}\n"
        )
        symbols = VERIFIER.parse_nm_defined_symbols(output)
        self.assertIn(qualified, symbols)
        self.assertIn(
            "bpr::application::github_application_host::github_application_host(void)",
            symbols,
        )

    def test_nm_nonimplementation_kinds_cannot_satisfy_anchors(self) -> None:
        correlate = symbol_name(SHARED_LINES["defect correlation decision"])
        for kind in ("A", "a", "C", "c", "I", "N", "n", "P", "p", "U", "v", "w"):
            with self.subTest(kind=kind):
                symbols = VERIFIER.parse_nm_defined_symbols(
                    f"00000001 {kind} {correlate}\n"
                    "00000002 T unrelated_symbol()\n"
                )
                self.assertNotIn(correlate, symbols)
                failures = VERIFIER.audit_symbols(
                    symbols,
                    VERIFIER.PRODUCTION_PROFILE,
                )
                self.assertTrue(
                    any("defect correlation decision" in failure for failure in failures)
                )

        with self.assertRaisesRegex(
            VERIFIER.verification_error,
            "zero defined symbols",
        ):
            VERIFIER.parse_nm_defined_symbols(
                "00000001 A absolute_symbol\n"
                "00000002 C common_symbol\n"
                "00000003 N debug_symbol\n"
            )

    def test_nm_implementation_kind_allowlist_is_nonvacuous(self) -> None:
        output = "".join(
            f"{index:08x} {kind} defined_{index}()\n"
            for index, kind in enumerate(
                sorted(VERIFIER.NM_IMPLEMENTATION_KINDS),
                start=1,
            )
        )
        self.assertEqual(
            len(VERIFIER.parse_nm_defined_symbols(output)),
            len(VERIFIER.NM_IMPLEMENTATION_KINDS),
        )

    def test_nm_unknown_kind_fails_closed(self) -> None:
        with self.assertRaisesRegex(
            VERIFIER.verification_error,
            "unknown symbol kind",
        ):
            VERIFIER.parse_nm_defined_symbols("00000001 ? unknown_symbol\n")

    def test_objdump_parses_gnu_mach_and_coff_records(self) -> None:
        gnu = (
            "fixture: file format elf64-x86-64\n\n"
            "SYMBOL TABLE:\n"
            "0000000000401000 g     F .text 0000000000000010 "
            "bpr::application::correlate(int)\n"
            "0000000000401010 g     F .text 0000000000000010 "
            "symbol_with_*UND*_text\n"
            "0000000000000000      d  *UND* source.cpp\n"
            "0000000000000000      DF *UND* 0000000000000000 "
            "GLIBC_2.2.5 bpr::application::correlate()\n"
        )
        self.assertEqual(
            VERIFIER.parse_objdump_defined_symbols(gnu),
            ("bpr::application::correlate(int)", "symbol_with_*UND*_text"),
        )

        mach = (
            "fixture: file format mach-o arm64\n\n"
            "SYMBOL TABLE:\n"
            "0000000100001000  w    F __TEXT,__text .hidden "
            "bpr::application::correlate(long)\n"
            "0000000100001010  w    O __DATA,.qtversion .hidden "
            "qt_version_tag\n"
        )
        self.assertEqual(
            VERIFIER.parse_objdump_defined_symbols(mach),
            ("bpr::application::correlate(long)", "qt_version_tag"),
        )

        coff = (
            "fixture.exe: file format coff-x86-64\n\n"
            "SYMBOL TABLE:\n"
            "[  7](sec  1)(fl 0x00)(ty   20)(scl   2) (nx 0) "
            "0x00000000 public: __cdecl "
            "bpr::application::correlate(long)\n"
            "[  8](sec  0)(fl 0x00)(ty   20)(scl   2) (nx 0) "
            "0x00000000 bpr::application::correlate()\n"
        )
        self.assertEqual(
            VERIFIER.parse_objdump_defined_symbols(coff),
            ("bpr::application::correlate(long)",),
        )

    def test_undefined_and_malformed_objdump_output_fail_closed(self) -> None:
        undefined = (
            "fixture: file format elf64-x86-64\n\n"
            "SYMBOL TABLE:\n"
            "0000000000000000      DF *UND* 0000000000000000 "
            "GLIBC_2.2.5 bpr::application::correlate()\n"
        )
        with self.assertRaisesRegex(
            VERIFIER.verification_error,
            "zero defined symbols",
        ):
            VERIFIER.parse_objdump_defined_symbols(undefined)

        with self.assertRaisesRegex(VERIFIER.verification_error, "malformed"):
            VERIFIER.parse_objdump_defined_symbols(
                "fixture: file format elf64-x86-64\n"
                "SYMBOL TABLE:\n"
                "not a symbol record\n"
            )

    def test_objdump_nonimplementation_sections_cannot_satisfy_anchors(self) -> None:
        correlate = "bpr::application::correlate()"
        output = (
            "fixture: file format elf64-x86-64\n\n"
            "SYMBOL TABLE:\n"
            f"00000001 g F *ABS* 00000001 {correlate}\n"
            f"00000002 g O *COM* 00000001 {correlate}\n"
            f"00000003 g F .debug_info 00000001 {correlate}\n"
            f"00000004 g F .gcc_except_table 00000001 {correlate}\n"
            "00000005 g F .text 00000001 unrelated_symbol()\n"
        )
        symbols = VERIFIER.parse_objdump_defined_symbols(output)
        self.assertEqual(symbols, ("unrelated_symbol()",))

        mach_debug = (
            "fixture: file format mach-o arm64\n\n"
            "SYMBOL TABLE:\n"
            f"00000001 l O __DWARF,__debug_info {correlate}\n"
            f"00000002 g O __DATA,__common {correlate}\n"
            "00000003 g F __TEXT,__text unrelated_symbol()\n"
        )
        self.assertEqual(
            VERIFIER.parse_objdump_defined_symbols(mach_debug),
            ("unrelated_symbol()",),
        )

        with self.assertRaisesRegex(
            VERIFIER.verification_error,
            "zero defined symbols",
        ):
            VERIFIER.parse_objdump_defined_symbols(
                "fixture: file format elf64-x86-64\n\n"
                "SYMBOL TABLE:\n"
                f"00000001 g F *ABS* 00000001 {correlate}\n"
                f"00000002 g O *COM* 00000001 {correlate}\n"
                f"00000003 g F .debug_info 00000001 {correlate}\n"
            )

    def test_objdump_nonpositive_coff_sections_cannot_satisfy_anchors(self) -> None:
        correlate = "bpr::application::correlate()"
        records = "".join(
            "[  7](sec {section:2})(fl 0x00)(ty   20)(scl   2) (nx 0) "
            "0x00000000 {name}\n".format(section=section, name=correlate)
            for section in (-2, -1, 0)
        )
        output = (
            "fixture.exe: file format coff-x86-64\n\n"
            "SYMBOL TABLE:\n"
            f"{records}"
            "[  8](sec  1)(fl 0x00)(ty   20)(scl   2) (nx 0) "
            "0x00000000 unrelated_symbol()\n"
        )
        self.assertEqual(
            VERIFIER.parse_objdump_defined_symbols(output),
            ("unrelated_symbol()",),
        )

    def test_objdump_unknown_sections_fail_closed(self) -> None:
        for record in (
            "00000001 g F *UNKNOWN* 00000001 symbol()",
            "00000001 g F .mystery 00000001 symbol()",
            "[  7](sec -3)(fl 0x00)(ty   20)(scl   2) (nx 0) "
            "0x00000000 symbol()",
        ):
            with self.subTest(record=record):
                with self.assertRaisesRegex(
                    VERIFIER.verification_error,
                    "unknown .* section|unknown pseudo-section",
                ):
                    VERIFIER.parse_objdump_defined_symbols(
                        "fixture: file format synthetic\n\n"
                        "SYMBOL TABLE:\n"
                        f"{record}\n"
                    )

    def test_malformed_nm_output_fails_closed(self) -> None:
        with self.assertRaisesRegex(VERIFIER.verification_error, "malformed"):
            VERIFIER.parse_nm_defined_symbols("not a symbol record\n")

    def test_registered_gate_uses_exit_status_as_its_verdict(self) -> None:
        cmake = (REPOSITORY_ROOT / "tests/end_to_end/CMakeLists.txt").read_text(
            encoding="utf-8"
        )
        properties = cmake.split(
            "set_tests_properties(\n    composed_application_hosts",
            maxsplit=1,
        )[1].split("\n)", maxsplit=1)[0]
        self.assertNotIn("PASS_REGULAR_EXPRESSION", properties)

    def test_missing_binary_fails_before_tool_discovery(self) -> None:
        missing = self.Binary.with_name("missing")
        with self.assertRaisesRegex(VERIFIER.verification_error, "does not exist"):
            VERIFIER.inspect_symbols(missing, which=lambda _name: None)

    def test_no_supported_tool_fails_closed(self) -> None:
        with self.assertRaisesRegex(
            VERIFIER.verification_error,
            "no supported demangling symbol inspector",
        ):
            VERIFIER.inspect_symbols(self.Binary, which=lambda _name: None)

    def test_llvm_nm_is_the_first_demangling_inspector(self) -> None:
        tools = fake_toolchain(
            {
                "llvm-nm": "/tools/llvm-nm",
                "nm": "/tools/nm",
                "c++filt": "/tools/c++filt",
                "objdump": "/tools/objdump",
            },
            [completed(rendered(SHARED_LINES))],
        )
        table = VERIFIER.inspect_symbols(
            self.Binary,
            which=tools.which,
            run=tools.run,
        )
        self.assertEqual(table.inspector, "llvm-nm --demangle")
        self.assertEqual(
            tools.Commands,
            [["/tools/llvm-nm", "--demangle", str(self.Binary)]],
        )

    def test_llvm_nm_nonzero_fails_without_fallback(self) -> None:
        tools = fake_toolchain(
            {"llvm-nm": "/tools/llvm-nm", "objdump": "/tools/objdump"},
            [completed(returncode=7)],
        )
        with self.assertRaisesRegex(VERIFIER.verification_error, "status 7"):
            VERIFIER.inspect_symbols(
                self.Binary,
                which=tools.which,
                run=tools.run,
            )
        self.assertEqual(len(tools.Commands), 1)

    def test_llvm_nm_empty_output_fails_closed(self) -> None:
        tools = fake_toolchain(
            {"llvm-nm": "/tools/llvm-nm"},
            [completed(" \n")],
        )
        with self.assertRaisesRegex(VERIFIER.verification_error, "empty symbol table"):
            VERIFIER.inspect_symbols(
                self.Binary,
                which=tools.which,
                run=tools.run,
            )

    def test_nm_output_is_passed_through_a_real_demangler(self) -> None:
        tools = fake_toolchain(
            {"nm": "/tools/nm", "c++filt": "/tools/c++filt"},
            [completed("0001 T __Zsynthetic\n"), completed(rendered(SHARED_LINES))],
        )
        table = VERIFIER.inspect_symbols(
            self.Binary,
            which=tools.which,
            run=tools.run,
        )
        self.assertEqual(table.inspector, "nm | c++filt")
        self.assertEqual(tools.Inputs, [None, "0001 T __Zsynthetic\n"])

    def test_nm_nonzero_fails_closed(self) -> None:
        tools = fake_toolchain(
            {"nm": "/tools/nm", "c++filt": "/tools/c++filt"},
            [completed(returncode=3)],
        )
        with self.assertRaisesRegex(VERIFIER.verification_error, "status 3"):
            VERIFIER.inspect_symbols(
                self.Binary,
                which=tools.which,
                run=tools.run,
            )

    def test_nm_empty_output_fails_before_demangling(self) -> None:
        tools = fake_toolchain(
            {"nm": "/tools/nm", "c++filt": "/tools/c++filt"},
            [completed("\n")],
        )
        with self.assertRaisesRegex(VERIFIER.verification_error, "empty symbol table"):
            VERIFIER.inspect_symbols(
                self.Binary,
                which=tools.which,
                run=tools.run,
            )
        self.assertEqual(len(tools.Commands), 1)

    def test_failed_demangler_fails_without_raw_symbol_fallback(self) -> None:
        tools = fake_toolchain(
            {"nm": "/tools/nm", "c++filt": "/tools/c++filt"},
            [completed("0001 T __Zsynthetic\n"), completed(returncode=4)],
        )
        with self.assertRaisesRegex(VERIFIER.verification_error, "status 4"):
            VERIFIER.inspect_symbols(
                self.Binary,
                which=tools.which,
                run=tools.run,
            )

    def test_empty_demangler_output_fails_closed(self) -> None:
        tools = fake_toolchain(
            {"nm": "/tools/nm", "c++filt": "/tools/c++filt"},
            [completed("0001 T __Zsynthetic\n"), completed(" \n")],
        )
        with self.assertRaisesRegex(VERIFIER.verification_error, "empty symbol table"):
            VERIFIER.inspect_symbols(
                self.Binary,
                which=tools.which,
                run=tools.run,
            )

    def test_raw_mangled_names_cannot_satisfy_demangled_anchors(self) -> None:
        failures = audit_nm(
            "0001 T __ZN3bpr11application23github_application_hostC1Ev\n",
            VERIFIER.PRODUCTION_PROFILE,
        )
        self.assertEqual(
            len(failures),
            len(VERIFIER.SHARED_REQUIRED)
            + len(VERIFIER.PRODUCTION_REQUIRED)
            + (len(VERIFIER.MAC_PRODUCTION_CLI_REQUIRED) if sys.platform == "darwin" else 0),
        )

    def test_nm_without_a_demangler_falls_back_to_objdump(self) -> None:
        tools = fake_toolchain(
            {"nm": "/tools/nm", "objdump": "/tools/objdump"},
            [completed(gnu_objdump_output(SHARED_LINES))],
        )
        table = VERIFIER.inspect_symbols(
            self.Binary,
            which=tools.which,
            run=tools.run,
        )
        self.assertEqual(table.inspector, "objdump -t -C")
        self.assertEqual(
            tools.Commands,
            [["/tools/objdump", "-t", "-C", str(self.Binary)]],
        )

    def test_objdump_nonzero_fails_closed(self) -> None:
        tools = fake_toolchain(
            {"objdump": "/tools/objdump"},
            [completed(returncode=9)],
        )
        with self.assertRaisesRegex(VERIFIER.verification_error, "status 9"):
            VERIFIER.inspect_symbols(
                self.Binary,
                which=tools.which,
                run=tools.run,
            )

    def test_objdump_empty_output_fails_closed(self) -> None:
        tools = fake_toolchain(
            {"objdump": "/tools/objdump"},
            [completed("\n")],
        )
        with self.assertRaisesRegex(VERIFIER.verification_error, "empty symbol table"):
            VERIFIER.inspect_symbols(
                self.Binary,
                which=tools.which,
                run=tools.run,
            )

    def test_linker_map_is_read_before_any_posix_inspector(self) -> None:
        """A map beside the binary describes the linked image and wins."""
        self.Binary.with_suffix(".map").write_text(
            msvc_map_output(MAP_DECORATED), encoding="utf-8"
        )
        decorated = list(MAP_DECORATED.values())
        tools = fake_toolchain(
            {"undname": "/tools/undname", "llvm-nm": "/tools/llvm-nm"},
            [completed(undname_output(decorated))],
        )
        table = VERIFIER.inspect_symbols(self.Binary, which=tools.which, run=tools.run)
        self.assertEqual(table.inspector, "link /MAP | undname")
        self.assertEqual(len(tools.Commands), 1)
        self.assertEqual(tools.Commands[0][0], "/tools/undname")
        # Only names carrying an anchor's scope chain are sent; the anonymous
        # static symbol cannot match any anchor and stays undemangled.
        self.assertEqual(tools.Commands[0][1:], decorated)
        self.assertIn("bpr::application::reconcile(void)", table.symbols)
        self.assertIn("main", table.symbols)
        self.assertNotIn("__ImageBase", table.symbols)
        failures = VERIFIER.audit_symbols(table.symbols, VERIFIER.PRODUCTION_PROFILE)
        for name in MAP_DECORATED:
            self.assertNotIn(
                f"required exact demangled anchor is absent: {name}", failures
            )

    def test_linker_map_without_undname_fails_closed(self) -> None:
        self.Binary.with_suffix(".map").write_text(
            msvc_map_output(MAP_DECORATED), encoding="utf-8"
        )
        tools = fake_toolchain({"llvm-nm": "/tools/llvm-nm"}, [])
        with self.assertRaisesRegex(VERIFIER.verification_error, "needs undname"):
            VERIFIER.inspect_symbols(self.Binary, which=tools.which, run=tools.run)
        self.assertEqual(tools.Commands, [])

    def test_linker_map_malformed_record_fails_closed(self) -> None:
        text = msvc_map_output(MAP_DECORATED).replace(
            " 0001:00002000       main", " garbage line here"
        )
        self.Binary.with_suffix(".map").write_text(text, encoding="utf-8")
        tools = fake_toolchain({"undname": "/tools/undname"}, [])
        with self.assertRaisesRegex(VERIFIER.verification_error, "malformed symbol record"):
            VERIFIER.inspect_symbols(self.Binary, which=tools.which, run=tools.run)

    def test_linker_map_without_a_table_fails_closed(self) -> None:
        self.Binary.with_suffix(".map").write_text(" probe\n\n Timestamp is 0\n", encoding="utf-8")
        tools = fake_toolchain({"undname": "/tools/undname"}, [])
        with self.assertRaisesRegex(VERIFIER.verification_error, "no public-symbol table"):
            VERIFIER.inspect_symbols(self.Binary, which=tools.which, run=tools.run)

    def test_undname_must_answer_every_name(self) -> None:
        self.Binary.with_suffix(".map").write_text(
            msvc_map_output(MAP_DECORATED), encoding="utf-8"
        )
        decorated = list(MAP_DECORATED.values())
        tools = fake_toolchain(
            {"undname": "/tools/undname"}, [completed(undname_output(decorated[:2]))]
        )
        with self.assertRaisesRegex(VERIFIER.verification_error, "demangled 2 of 4"):
            VERIFIER.inspect_symbols(self.Binary, which=tools.which, run=tools.run)

    def test_constructor_and_type_anchors_have_the_class_chain(self) -> None:
        self.assertEqual(
            VERIFIER.decorated_scope_chain(
                "bpr::application::github_application_host::github_application_host"
            ),
            "github_application_host@application@bpr@@",
        )
        self.assertEqual(
            VERIFIER.decorated_scope_chain("bpr::application::reconcile"),
            "reconcile@application@bpr@@",
        )
        self.assertIn(
            VERIFIER.decorated_scope_chain("bpr::adapters::github::live_review_mutation_provider"),
            MAP_DECORATED["live review mutation provider"],
        )
        for name in MAP_DECORATED.values():
            self.assertTrue(any(chain in name for chain in VERIFIER.ANCHOR_SCOPE_CHAINS), name)

    def test_undname_command_lines_stay_under_the_budget(self) -> None:
        names = [f"?n{index}@x@@YAXXZ" + "A" * 3000 for index in range(20)]
        chunks = VERIFIER.undname_command_chunks(names)
        self.assertEqual(sum(len(chunk) for chunk in chunks), len(names))
        for chunk in chunks:
            self.assertLessEqual(
                sum(len(name) + 1 for name in chunk), VERIFIER.UNDNAME_COMMAND_BUDGET
            )
        self.assertGreater(len(chunks), 1)

    def test_undname_nonzero_fails_closed(self) -> None:
        self.Binary.with_suffix(".map").write_text(
            msvc_map_output(MAP_DECORATED), encoding="utf-8"
        )
        tools = fake_toolchain({"undname": "/tools/undname"}, [completed(returncode=5)])
        with self.assertRaisesRegex(VERIFIER.verification_error, "status 5"):
            VERIFIER.inspect_symbols(self.Binary, which=tools.which, run=tools.run)

    def test_parse_map_counts_public_and_static_symbols_once(self) -> None:
        parsed = VERIFIER.parse_map_defined_symbols(msvc_map_output(MAP_DECORATED))
        self.assertEqual(len(parsed), len(MAP_DECORATED) + 2)
        self.assertNotIn("__ImageBase", parsed)
        self.assertIn(MAP_STATIC_SYMBOL, parsed)

    def test_real_inspector_output_contains_defined_symbols(self) -> None:
        """The instrument reads a real binary, or refuses where it cannot.

        `inspect_symbols` wants a linked binary's defined symbols, and on
        Windows nothing can supply them: the MSVC linker consumes the COFF
        table — [[BPR-0361]] measured `dumpbin /symbols` returning a section
        summary and no symbols — and Visual Studio ships no `llvm-nm`, `nm`, or
        `objdump`. The verifier fails closed there, which is the right answer.

        So this asserts the refusal where the capability does not exist and the
        table where it does, rather than asserting a property the platform
        cannot express. `tests/scripts/verify_test_network_isolation_tests.py`
        takes the same position for `os.access(X_OK)`, in the same words.

        Giving the composed-application gate a Windows inspector of its own is
        [[BPR-0364]]; it is not what this case is for.
        """
        if not INSPECTOR_READS_A_LINKED_BINARY:
            with self.assertRaises(VERIFIER.verification_error):
                VERIFIER.inspect_symbols(Path(sys.executable))
            return
        table = VERIFIER.inspect_symbols(Path(sys.executable))
        self.assertTrue(table.symbols)
        self.assertTrue(all(symbol.strip() for symbol in table.symbols))


if __name__ == "__main__":
    unittest.main()
