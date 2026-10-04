#!/usr/bin/env python3
"""Verify exact host and decision symbols linked into an application binary.

This is linkage evidence only. A required symbol proves that the selected
binary contains the named implementation; it does not prove construction or
runtime reachability. The corresponding executable contracts provide that
separate evidence.
"""

from __future__ import annotations

import argparse
from collections.abc import Callable, Sequence
from dataclasses import dataclass
from pathlib import Path
import re
import shutil
import subprocess
import sys


DEVELOPMENT_FIXTURE_PROFILE = "development-fixture"
PRODUCTION_PROFILE = "production"
PROFILES = (DEVELOPMENT_FIXTURE_PROFILE, PRODUCTION_PROFILE)


@dataclass(frozen=True)
class symbol_anchor:
    name: str
    qualified_name: str
    is_type: bool = False

    def matches(self, candidate: str) -> bool:
        if not self.is_type:
            return candidate.startswith(f"{self.qualified_name}(")

        if candidate.startswith(f"{self.qualified_name}::"):
            return True

        metadata_names = (
            f"vtable for {self.qualified_name}",
            f"VTT for {self.qualified_name}",
            f"typeinfo for {self.qualified_name}",
            f"typeinfo name for {self.qualified_name}",
        )
        if candidate in metadata_names:
            return True

        thunk_prefixes = (
            "non-virtual thunk to ",
            "virtual thunk to ",
            "covariant return thunk to ",
        )
        return any(
            candidate.startswith(f"{prefix}{self.qualified_name}::")
            for prefix in thunk_prefixes
        ) or candidate == f"const {self.qualified_name}::`vftable'"


@dataclass(frozen=True)
class symbol_table:
    symbols: tuple[str, ...]
    inspector: str


class verification_error(RuntimeError):
    """A fail-closed symbol inspection error."""


def function_anchor(name: str, qualified_name: str) -> symbol_anchor:
    return symbol_anchor(name, qualified_name)


def type_anchor(name: str, qualified_name: str) -> symbol_anchor:
    return symbol_anchor(name, qualified_name, is_type=True)


# [[BPR-1179]]. The agent integrations a full graph composes and an agent-free
# graph (BPR_BUILD_AGENT_INTEGRATIONS=OFF) must not contain: the hosts that run
# an agent, the process launcher that starts one, the decoder that reads a
# provider's protocol and the MCP helper's supervisor. Required in a full image, forbidden in an agent-free one. The Qt
# panels stay shared: an agent-free window builds them and withdraws them.
AGENT_INTEGRATION_REQUIRED = (
    function_anchor(
        "agent invocation application host constructor",
        "bpr::application::agent_invocation_host::agent_invocation_host",
    ),
    function_anchor(
        "agent process launcher constructor",
        "bpr::adapters::agent::agent_process_launcher::agent_process_launcher",
    ),
    function_anchor(
        "agent finding application host constructor",
        "bpr::application::agent_finding_host::agent_finding_host",
    ),
    function_anchor(
        "agent chat session application host constructor",
        "bpr::application::agent_chat_session_host::agent_chat_session_host",
    ),
    function_anchor(
        "agent comment workshop application host constructor",
        "bpr::application::agent_comment_workshop_host::agent_comment_workshop_host",
    ),
    function_anchor(
        "MCP context helper supervisor constructor",
        "bpr::application::mcp_context_helper_supervisor::mcp_context_helper_supervisor",
    ),
    function_anchor(
        "agent protocol decoder constructor",
        "bpr::application::agent_protocol_decoder::agent_protocol_decoder",
    ),
)

# [[BPR-1179]]. Local automation: the service that runs a forwarded bpr cycle
# command in the desktop. A graph built with BPR_BUILD_LOCAL_AUTOMATION=OFF
# binds no owner command endpoint and compiles no such service.
LOCAL_AUTOMATION_REQUIRED = (
    function_anchor(
        "cycle owner service",
        "bpr::app::cycle_owner_service::serve",
    ),
)

COMMON_REQUIRED = (
    function_anchor(
        "GitHub application host constructor",
        "bpr::application::github_application_host::github_application_host",
    ),
    function_anchor(
        "epoch application host constructor",
        "bpr::application::epoch_application_host::epoch_application_host",
    ),
    function_anchor(
        "diff application host constructor",
        "bpr::application::diff_application_host::diff_application_host",
    ),
    function_anchor(
        "workbench application host constructor",
        "bpr::application::workbench_application_host::workbench_application_host",
    ),
    function_anchor(
        "review mutation application host constructor",
        "bpr::application::review_mutation_application_host::review_mutation_application_host",
    ),
    function_anchor(
        "review mutation coordinator constructor",
        "bpr::application::review_mutation_coordinator::review_mutation_coordinator",
    ),
    function_anchor(
        "review reconciliation decision",
        "bpr::application::reconcile",
    ),
    function_anchor(
        "appearance application host constructor",
        "bpr::application::appearance_application_host::appearance_application_host",
    ),
    function_anchor(
        "packaged syntax classifier constructor",
        "bpr::adapters::syntax::packaged_syntax_classifier::packaged_syntax_classifier",
    ),
    function_anchor(
        "review ledger application host constructor",
        "bpr::application::review_ledger_application_host::review_ledger_application_host",
    ),
    function_anchor(
        "defect correlation decision",
        "bpr::application::correlate",
    ),
    function_anchor(
        "review queue application host constructor",
        "bpr::application::review_queue_application_host::review_queue_application_host",
    ),
    function_anchor(
        "review queue notifier constructor",
        "bpr::application::review_queue_notifier::review_queue_notifier",
    ),
    function_anchor(
        "Qt agent launch panel constructor",
        "bpr::qt::agent_launch_panel::agent_launch_panel",
    ),
    function_anchor(
        "Qt side chat panel constructor",
        "bpr::qt::side_chat_panel::side_chat_panel",
    ),
    function_anchor(
        "Qt finding queue view constructor",
        "bpr::qt::finding_queue_view::finding_queue_view",
    ),
    function_anchor(
        "Qt device authorization panel constructor",
        "bpr::qt::device_authorization_panel::device_authorization_panel",
    ),
    function_anchor(
        "Qt workbench window constructor",
        "bpr::qt::workbench_window::workbench_window",
    ),
    function_anchor(
        "system browser opener constructor",
        "bpr::app::system_browser_opener::system_browser_opener",
    ),
    # BPR-0305: the public-preview candidate is composed in both profiles
    # with its selection unavailable; the two shipped credential-free
    # factories it reaches are the linkage proof that the anonymous provider
    # and the anonymous Git path exist in the image the audit inspects.
    function_anchor(
        "anonymous read transport factory",
        "bpr::adapters::github::detail::make_native_https_anonymous_read_transport",
    ),
    function_anchor(
        "anonymous smart transport factory",
        "bpr::adapters::github::detail::make_native_https_anonymous_smart_transport",
    ),
    # BPR-0536: the device-authorization refresh lifecycle is composed in both
    # profiles, proven by the entry point the composition calls rather than by
    # the composition function that calls it. The anchor is the device host's
    # own out-of-line member, defined in the application library and
    # referenced only from the composition: no compiler inlines it across that
    # boundary without whole-program optimization, which these presets do not
    # enable, and a linker that drops unreferenced sections removes it the
    # moment composition stops binding -- which is the falsification this
    # anchor exists for.
    #
    # BPR-0285 moved it here. The former anchor was the composition's own
    # private `bind_device_refresh_authority`, which has exactly one call
    # site: MSVC inlines it, so a Windows Release image cannot exhibit it at
    # all (0 occurrences in the Release map against 5 in Debug), and on macOS
    # it survives only as a local symbol, which proves presence rather than
    # linkage. A private one-caller function is not a linkage anchor on any
    # compiler; Windows made that visible first.
    function_anchor(
        "device refresh authority binding",
        "bpr::application::device_authorization_application_host::bind_refresh_authority",
    ),
)

# A full graph, built with both removable families, requires all three.
SHARED_REQUIRED = COMMON_REQUIRED + AGENT_INTEGRATION_REQUIRED + LOCAL_AUTOMATION_REQUIRED

FIXTURE_REQUIRED = (
    function_anchor(
        "fixture review mutation provider boundary",
        "bpr::app::detail::github_public_path_fixture_components::review_mutation_provider",
    ),
    function_anchor(
        "fixture review mutation execution grant factory",
        "bpr::application::make_review_mutation_testing_grant",
    ),
    function_anchor(
        "fixture one-shot review mutation execution grant factory",
        "bpr::application::make_review_mutation_one_shot_testing_grant",
    ),
)

PRODUCTION_REQUIRED = (
    type_anchor(
        "live review mutation provider",
        "bpr::adapters::github::live_review_mutation_provider",
    ),
    function_anchor(
        "live review mutation provider factory",
        "bpr::adapters::github::make_live_review_mutation_provider",
    ),
)

PRODUCTION_FORBIDDEN = (
    type_anchor(
        "GitHub public-path fixture components",
        "bpr::app::detail::github_public_path_fixture_components",
    ),
    function_anchor(
        "fixture review mutation execution grant factory",
        "bpr::application::make_review_mutation_testing_grant",
    ),
    function_anchor(
        "fixture one-shot review mutation execution grant factory",
        "bpr::application::make_review_mutation_one_shot_testing_grant",
    ),
)

# RFC-BPR-0015 permits the opt-in configured-CLI read profile only in the
# tests-disabled macOS product. The other release targets must still omit it.
MAC_PRODUCTION_CLI_REQUIRED = (
    type_anchor(
        "GitHub CLI transport",
        "bpr::adapters::github::detail::gh_cli_transport",
    ),
    function_anchor(
        "GitHub CLI configuration validator",
        "bpr::adapters::github::detail::validate_gh_cli_configuration",
    ),
    function_anchor(
        "GitHub CLI request compiler",
        "bpr::adapters::github::detail::compile_gh_cli_process_request",
    ),
    function_anchor(
        "GitHub CLI process runner factory",
        "bpr::adapters::github::detail::make_github_cli_process_runner",
    ),
)

FIXTURE_FORBIDDEN = (
    type_anchor(
        "live review mutation provider",
        "bpr::adapters::github::live_review_mutation_provider",
    ),
    function_anchor(
        "live review mutation provider factory",
        "bpr::adapters::github::make_live_review_mutation_provider",
    ),
)


NM_DEFINED_RECORD = re.compile(
    r"^\s*(?P<address>(?:0[xX])?[0-9A-Fa-f]+)\s+"
    r"(?P<kind>[^\s])\s+(?P<name>\S.*)\s*$"
)
NM_NO_ADDRESS_RECORD = re.compile(
    r"^\s*(?P<kind>[^\s])\s+(?P<name>\S.*)\s*$"
)
OBJDUMP_COFF_RECORD = re.compile(
    r"^\s*\[\s*\d+\]\(sec\s+(?P<section>-?\d+)\)"
    r".*?\s0x[0-9A-Fa-f]+\s+(?P<name>\S.*)\s*$"
)
OBJDUMP_UNDEFINED_RECORD = re.compile(
    r"^\s*(?:0[xX])?[0-9A-Fa-f]+\s+.+?\s+\*UND\*"
    r"(?:\s+\S.*)?\s*$"
)
OBJDUMP_MACH_RECORD = re.compile(
    r"^\s*(?:0[xX])?[0-9A-Fa-f]+\s+.*?\s+"
    r"(?P<section>__[A-Za-z0-9_]+,[^\s]+)\s+"
    r"(?P<name>\S.*)\s*$"
)
OBJDUMP_ELF_RECORD = re.compile(
    r"^\s*(?:0[xX])?[0-9A-Fa-f]+\s+.*?\s+"
    r"(?P<section>\*[^*]+\*|[._/][^\s]+)\s+"
    r"(?:0[xX])?[0-9A-Fa-f]+\s+(?P<name>\S.*)\s*$"
)

# An MSVC-linked image has no symbol table for nm or objdump to read; the
# linker consumed it. What the linker writes instead, on request, is a map of
# the image it produced: every public symbol by section and offset with the
# object it came from, then the static symbols. `link /MAP` puts it beside the
# executable as `<name>.map`, and `undname` -- shipped in the same toolchain
# directory as `link` -- turns its decorated names into the declarations the
# anchors above are written against. The map describes the linked image, so a
# required anchor found here is present in the binary the gate audits, not
# merely in an object the linker might have discarded (BPR-0364).
MAP_PUBLICS_HEADER = re.compile(
    r"^\s*Address\s+Publics by Value\s+Rva\+Base\s+Lib:Object\s*$"
)
MAP_STATICS_HEADER = re.compile(r"^\s*Static symbols\s*$")
MAP_ENTRY_POINT = re.compile(r"^\s*entry point at\s+\S+\s*$")
MAP_SYMBOL_RECORD = re.compile(
    r"^\s*(?P<section>[0-9A-Fa-f]{4}):(?P<offset>[0-9A-Fa-f]{8,16})\s+"
    r"(?P<name>\S+)\s+(?P<rva>[0-9A-Fa-f]{16})(?P<flags>(?:\s+[fi])*)\s+"
    r"(?P<object>\S.*?)\s*$"
)
UNDNAME_DECORATED_RECORD = re.compile(
    r'^Undecoration of :- "(?P<decorated>[^"]*)"\s*$'
)
UNDNAME_DEMANGLED_RECORD = re.compile(r'^is :- "(?P<demangled>.*)"\s*$')
# A Windows command line holds at most 32 767 characters and the real map
# lists two hundred thousand decorated names, so `undname` is asked only
# about the names that can match an anchor, in command lines under this
# many characters.
UNDNAME_COMMAND_BUDGET = 20000

# LLVM's Microsoft demangler can retain an access specifier, member qualifier,
# and calling convention before a constructor or method. Only these documented
# declaration prefixes are removed; arbitrary return types or surrounding
# template text are not searched or stripped.
MSVC_ACCESS_PREFIXES = ("public: ", "protected: ", "private: ")
MSVC_MEMBER_PREFIXES = ("virtual ", "static ")
MSVC_CALLING_CONVENTIONS = (
    "__cdecl",
    "__thiscall",
    "__stdcall",
    "__fastcall",
    "__vectorcall",
)
MSVC_CALLING_CONVENTION = re.compile(
    rf"(?<!\S)(?:{'|'.join(MSVC_CALLING_CONVENTIONS)})(?=\s)"
)
CPP_ANGLE_OPERATOR = re.compile(
    r"\boperator\s*(?P<token>->\*?|<=>|<<=?|>>=?|<=|>=|<|>)"
)

# Only symbol kinds backed by a section that can contain code, data, or ABI
# type metadata count as linkage evidence. GNU unique globals and ELF indirect
# functions are definitions; weak symbols count only in their defined uppercase
# forms. The second set is intentionally skipped as a documented non-definition
# rather than accepted merely because nm printed an address.
NM_IMPLEMENTATION_KINDS = frozenset("BbDdGgRrSsTtVWiu")
NM_NONIMPLEMENTATION_KINDS = frozenset("-AaCcINnPpUvw")

OBJDUMP_PSEUDO_NONIMPLEMENTATION_SECTIONS = frozenset(
    ("*UND*", "*ABS*", "*COM*")
)
ELF_IMPLEMENTATION_SECTIONS = (
    ".bss",
    ".ctors",
    ".data",
    ".dtors",
    ".fini",
    ".fini_array",
    ".got",
    ".init",
    ".init_array",
    ".plt",
    ".rodata",
    ".sbss",
    ".sdata",
    ".tbss",
    ".tdata",
    ".text",
)
ELF_IMPLEMENTATION_PREFIXES = (".gnu.linkonce.",)
ELF_NONIMPLEMENTATION_SECTIONS = (
    ".comment",
    ".debug",
    ".eh_frame",
    ".gcc_except_table",
    ".llvm_addrsig",
    ".note",
    ".stab",
    ".zdebug",
)
MACH_IMPLEMENTATION_SECTIONS = frozenset(
    (
        "__DATA,.qtversion",
        "__DATA,__bss",
        "__DATA,__data",
        "__DATA_CONST,__const",
        "__TEXT,__const",
        "__TEXT,__text",
    )
)
MACH_NONIMPLEMENTATION_SECTIONS = frozenset(
    ("__DATA,__common", "__TEXT,__gcc_except_tab")
)
MACH_NONIMPLEMENTATION_SEGMENTS = frozenset(("__DWARF",))


def normalize_symbol_name(name: str) -> str:
    normalized = name.strip()
    if normalized.startswith(".hidden "):
        normalized = normalized.removeprefix(".hidden ")

    for prefix in MSVC_ACCESS_PREFIXES:
        if normalized.startswith(prefix):
            normalized = normalized.removeprefix(prefix)
            break
    for prefix in MSVC_MEMBER_PREFIXES:
        if normalized.startswith(prefix):
            normalized = normalized.removeprefix(prefix)
            break
    return normalize_msvc_declaration(normalized)


def normalize_msvc_declaration(declaration: str) -> str:
    conventions = {
        match.start(): match for match in MSVC_CALLING_CONVENTION.finditer(declaration)
    }
    if not conventions:
        return declaration

    operator_angles = {
        position
        for match in CPP_ANGLE_OPERATOR.finditer(declaration)
        for position in range(match.start("token"), match.end("token"))
        if declaration[position] in "<>"
    }
    opening = {"<": ">", "(": ")", "[": "]"}
    closing = frozenset(opening.values())
    delimiters: list[str] = []
    top_level_convention_end: int | None = None

    for position, character in enumerate(declaration):
        convention = conventions.get(position)
        if convention is not None and not delimiters:
            if top_level_convention_end is None:
                top_level_convention_end = convention.end()

        if position in operator_angles:
            continue
        if character in opening:
            delimiters.append(opening[character])
        elif character in closing:
            if not delimiters or delimiters.pop() != character:
                raise verification_error("unbalanced demangled declaration")

    if delimiters:
        raise verification_error("unbalanced demangled declaration")
    if top_level_convention_end is None:
        return declaration
    return declaration[top_level_convention_end:].lstrip()


def section_name_matches(
    section: str,
    names: Sequence[str],
    separators: Sequence[str] = (".",),
) -> bool:
    return any(
        section == name
        or any(section.startswith(f"{name}{separator}") for separator in separators)
        for name in names
    )


def objdump_section_is_implementation(section: str, line_number: int) -> bool:
    if section in OBJDUMP_PSEUDO_NONIMPLEMENTATION_SECTIONS:
        return False
    if section.startswith("*"):
        raise verification_error(
            f"objdump produced an unknown pseudo-section on line {line_number}"
        )

    if section.startswith("__"):
        if section in MACH_IMPLEMENTATION_SECTIONS:
            return True
        if section in MACH_NONIMPLEMENTATION_SECTIONS:
            return False
        segment = section.partition(",")[0]
        if segment in MACH_NONIMPLEMENTATION_SEGMENTS:
            return False
        raise verification_error(
            f"objdump produced an unknown Mach-O section on line {line_number}"
        )

    if section_name_matches(section, ELF_IMPLEMENTATION_SECTIONS) or section.startswith(
        ELF_IMPLEMENTATION_PREFIXES
    ):
        return True
    if section_name_matches(
        section,
        ELF_NONIMPLEMENTATION_SECTIONS,
        separators=(".", "_"),
    ):
        return False
    raise verification_error(
        f"objdump produced an unknown ELF section on line {line_number}"
    )


def unique_symbols(names: Sequence[str]) -> tuple[str, ...]:
    return tuple(dict.fromkeys(names))


def parse_nm_defined_symbols(contents: str) -> tuple[str, ...]:
    symbols: list[str] = []
    for line_number, line in enumerate(contents.splitlines(), start=1):
        if not line.strip():
            continue

        defined = NM_DEFINED_RECORD.fullmatch(line)
        if defined is not None:
            kind = defined.group("kind")
            if kind in NM_NONIMPLEMENTATION_KINDS:
                continue
            if kind not in NM_IMPLEMENTATION_KINDS:
                raise verification_error(
                    f"nm produced an unknown symbol kind on line {line_number}"
                )
            symbols.append(normalize_symbol_name(defined.group("name")))
            continue

        no_address = NM_NO_ADDRESS_RECORD.fullmatch(line)
        if no_address is not None:
            if no_address.group("kind") in NM_NONIMPLEMENTATION_KINDS:
                continue
            raise verification_error(
                f"nm produced a symbol kind without an address on line {line_number}"
            )

        raise verification_error(
            f"nm produced a malformed symbol record on line {line_number}"
        )

    parsed = unique_symbols(symbols)
    if not parsed:
        raise verification_error("nm produced zero defined symbols")
    return parsed


def parse_objdump_record(line: str, line_number: int) -> str | None:
    if OBJDUMP_UNDEFINED_RECORD.fullmatch(line) is not None:
        return None

    coff = OBJDUMP_COFF_RECORD.fullmatch(line)
    if coff is not None:
        section = int(coff.group("section"))
        if section in (-2, -1, 0):
            return None
        if section < -2:
            raise verification_error(
                f"objdump produced an unknown COFF section on line {line_number}"
            )
        return normalize_symbol_name(coff.group("name"))

    for pattern in (OBJDUMP_MACH_RECORD, OBJDUMP_ELF_RECORD):
        parsed = pattern.fullmatch(line)
        if parsed is not None:
            if not objdump_section_is_implementation(
                parsed.group("section"), line_number
            ):
                return None
            return normalize_symbol_name(parsed.group("name"))

    raise verification_error(
        f"objdump produced a malformed symbol record on line {line_number}"
    )


def parse_objdump_defined_symbols(contents: str) -> tuple[str, ...]:
    symbols: list[str] = []
    saw_table = False
    for line_number, line in enumerate(contents.splitlines(), start=1):
        stripped = line.strip()
        if not saw_table:
            if stripped == "SYMBOL TABLE:":
                saw_table = True
            elif stripped and "file format" not in stripped and not stripped.endswith(":"):
                raise verification_error(
                    f"objdump produced a malformed header on line {line_number}"
                )
            continue

        if not stripped:
            continue
        parsed = parse_objdump_record(line, line_number)
        if parsed is not None:
            symbols.append(parsed)

    if not saw_table:
        raise verification_error("objdump output has no symbol-table header")
    parsed_symbols = unique_symbols(symbols)
    if not parsed_symbols:
        raise verification_error("objdump produced zero defined symbols")
    return parsed_symbols


def parse_map_defined_symbols(contents: str) -> tuple[str, ...]:
    """Return the decorated names a linker map lists for the linked image.

    Only the public and static symbol tables count, and only records whose
    section is a real section: `0000:` is the absolute pseudo-section that
    carries `__ImageBase` and the debug directory, never implementation.
    """
    symbols: list[str] = []
    in_table = False
    saw_table = False
    for line_number, line in enumerate(contents.splitlines(), start=1):
        stripped = line.strip()
        if MAP_PUBLICS_HEADER.fullmatch(line) or MAP_STATICS_HEADER.fullmatch(line):
            in_table = True
            saw_table = True
            continue
        if MAP_ENTRY_POINT.fullmatch(line):
            in_table = False
            continue
        if not in_table or not stripped:
            continue
        record = MAP_SYMBOL_RECORD.fullmatch(line)
        if record is None:
            raise verification_error(
                f"linker map produced a malformed symbol record on line {line_number}"
            )
        if int(record.group("section"), 16) == 0:
            continue
        symbols.append(record.group("name"))

    if not saw_table:
        raise verification_error("linker map has no public-symbol table")
    parsed = unique_symbols(symbols)
    if not parsed:
        raise verification_error("linker map lists zero defined symbols")
    return parsed


def decorated_scope_chain(qualified_name: str) -> str:
    """The reversed scope chain an MSVC-decorated name carries for this anchor.

    `bpr::application::reconcile` is decorated `?reconcile@application@bpr@@…`
    and every member or metadata symbol of `bpr::adapters::github::provider`
    carries `provider@github@adapters@bpr@@`. A constructor is written
    `??0Class@scope@@`, not `Class@Class@scope@@`, so an anchor of the form
    `X::X` uses the class's own chain, which the matcher then narrows.
    """
    parts = qualified_name.split("::")
    if len(parts) >= 2 and parts[-1] == parts[-2]:
        parts = parts[:-1]
    return parts[-1] + "@" + "@".join(reversed(parts[:-1])) + "@@"


ANCHOR_SCOPE_CHAINS = tuple(
    dict.fromkeys(
        decorated_scope_chain(anchor.qualified_name)
        for anchor in (
            *SHARED_REQUIRED,
            *FIXTURE_REQUIRED,
            *PRODUCTION_REQUIRED,
            *FIXTURE_FORBIDDEN,
            *PRODUCTION_FORBIDDEN,
            *MAC_PRODUCTION_CLI_REQUIRED,
        )
    )
)


def undname_command_chunks(names: Sequence[str]) -> list[list[str]]:
    """Split names into command lines that stay under the Windows limit."""
    chunks: list[list[str]] = []
    current: list[str] = []
    size = 0
    for name in names:
        if current and size + len(name) + 1 > UNDNAME_COMMAND_BUDGET:
            chunks.append(current)
            current = []
            size = 0
        current.append(name)
        size += len(name) + 1
    if current:
        chunks.append(current)
    return chunks


def demangle_map_symbols(
    undname: str,
    decorated: Sequence[str],
    run: Callable[..., subprocess.CompletedProcess[str]],
) -> tuple[str, ...]:
    """Demangle the decorated names that can match an anchor; keep C names as is.

    The audit asks only whether each anchor is present, and an anchor's
    decorated form always contains its reversed scope chain, so a name without
    any anchor's chain cannot satisfy or violate the audit and is not sent to
    `undname`. `undname` answers each name with a two-line record, and the
    count of answers must equal the count of questions: a name it cannot
    decorate is a refusal, not a symbol to skip.
    """
    demangled: list[str] = []
    pending = [
        name
        for name in decorated
        if name.startswith("?") and any(chain in name for chain in ANCHOR_SCOPE_CHAINS)
    ]
    for chunk in undname_command_chunks(pending):
        output = checked_output((undname, *chunk), "undname", run)
        answered = 0
        expecting: str | None = None
        for line in output.splitlines():
            question = UNDNAME_DECORATED_RECORD.fullmatch(line)
            if question is not None:
                expecting = question.group("decorated")
                continue
            answer = UNDNAME_DEMANGLED_RECORD.fullmatch(line)
            if answer is not None and expecting is not None:
                demangled.append(normalize_symbol_name(answer.group("demangled")))
                answered += 1
                expecting = None
        if answered != len(chunk):
            raise verification_error(
                f"undname demangled {answered} of {len(chunk)} names"
            )
    demangled.extend(name for name in decorated if not name.startswith("?"))
    return unique_symbols(demangled)


def checked_output(
    command: Sequence[str],
    description: str,
    run: Callable[..., subprocess.CompletedProcess[str]],
    *,
    input_text: str | None = None,
) -> str:
    completed = run(
        list(command),
        input=input_text,
        capture_output=True,
        text=True,
        check=False,
    )
    if completed.returncode != 0:
        raise verification_error(
            f"{description} exited with status {completed.returncode}"
        )
    if not completed.stdout.strip():
        raise verification_error(f"{description} produced an empty symbol table")
    return completed.stdout


def inspect_symbols(
    binary: Path,
    *,
    which: Callable[[str], str | None] | None = None,
    run: Callable[..., subprocess.CompletedProcess[str]] | None = None,
) -> symbol_table:
    if not binary.is_file():
        raise verification_error(f"binary does not exist or is not a file: {binary}")

    find_tool = which if which is not None else shutil.which
    run_tool = run if run is not None else subprocess.run

    linker_map = binary.with_suffix(".map")
    if linker_map.is_file():
        undname = find_tool("undname")
        if undname is None:
            raise verification_error(
                f"linker map {linker_map.name} needs undname to demangle it; "
                "run inside the Visual Studio developer environment"
            )
        contents = linker_map.read_text(encoding="utf-8", errors="strict")
        return symbol_table(
            demangle_map_symbols(undname, parse_map_defined_symbols(contents), run_tool),
            "link /MAP | undname",
        )

    llvm_nm = find_tool("llvm-nm")
    if llvm_nm is not None:
        contents = checked_output(
            (llvm_nm, "--demangle", str(binary)),
            "llvm-nm --demangle",
            run_tool,
        )
        return symbol_table(
            parse_nm_defined_symbols(contents),
            "llvm-nm --demangle",
        )

    nm = find_tool("nm")
    if nm is not None:
        demangler = find_tool("c++filt") or find_tool("llvm-cxxfilt")
        if demangler is not None:
            raw = checked_output(
                (nm, str(binary)),
                "nm",
                run_tool,
            )
            contents = checked_output(
                (demangler,),
                Path(demangler).name,
                run_tool,
                input_text=raw,
            )
            return symbol_table(
                parse_nm_defined_symbols(contents),
                f"nm | {Path(demangler).name}",
            )

    objdump = find_tool("llvm-objdump") or find_tool("objdump")
    if objdump is not None:
        contents = checked_output(
            (objdump, "-t", "-C", str(binary)),
            f"{Path(objdump).name} -t -C",
            run_tool,
        )
        return symbol_table(
            parse_objdump_defined_symbols(contents),
            f"{Path(objdump).name} -t -C",
        )

    raise verification_error(
        "no supported demangling symbol inspector is available "
        "(a linker map beside the binary plus undname, llvm-nm, nm plus "
        "c++filt/llvm-cxxfilt, or objdump/llvm-objdump)"
    )


def required_anchors(profile: str, *, agent_integrations: bool = True, local_automation: bool = True) -> tuple[symbol_anchor, ...]:
    required = COMMON_REQUIRED
    required += AGENT_INTEGRATION_REQUIRED if agent_integrations else ()
    required += LOCAL_AUTOMATION_REQUIRED if local_automation else ()
    if profile == DEVELOPMENT_FIXTURE_PROFILE:
        return required + FIXTURE_REQUIRED
    required += PRODUCTION_REQUIRED
    if sys.platform == "darwin":
        required += MAC_PRODUCTION_CLI_REQUIRED
    return required


def forbidden_anchors(profile: str, *, agent_integrations: bool = True, local_automation: bool = True) -> tuple[symbol_anchor, ...]:
    forbidden = () if agent_integrations else AGENT_INTEGRATION_REQUIRED
    forbidden += () if local_automation else LOCAL_AUTOMATION_REQUIRED
    if profile == DEVELOPMENT_FIXTURE_PROFILE:
        return forbidden + FIXTURE_FORBIDDEN
    forbidden += PRODUCTION_FORBIDDEN
    if sys.platform != "darwin":
        forbidden += MAC_PRODUCTION_CLI_REQUIRED
    return forbidden


def audit_symbols(
    symbols: Sequence[str], profile: str, *, agent_integrations: bool = True, local_automation: bool = True
) -> list[str]:
    if profile not in PROFILES:
        raise ValueError(f"unknown composition profile: {profile}")

    families = {"agent_integrations": agent_integrations, "local_automation": local_automation}
    required = required_anchors(profile, **families)
    forbidden = forbidden_anchors(profile, **families)

    failures = [
        f"required exact demangled anchor is absent: {anchor.name}"
        for anchor in required
        if not any(anchor.matches(symbol) for symbol in symbols)
    ]
    failures.extend(
        f"forbidden exact demangled anchor is present: {anchor.name}"
        for anchor in forbidden
        if any(anchor.matches(symbol) for symbol in symbols)
    )
    return failures


def main(arguments: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--profile", choices=PROFILES, required=True)
    parser.add_argument(
        "--without-agent-integrations",
        action="store_true",
        help="the binary comes from a BPR_BUILD_AGENT_INTEGRATIONS=OFF graph: agent anchors are forbidden",
    )
    parser.add_argument(
        "--without-local-automation",
        action="store_true",
        help="the binary comes from a BPR_BUILD_LOCAL_AUTOMATION=OFF graph: automation anchors are forbidden",
    )
    parser.add_argument("binary", type=Path)
    parsed = parser.parse_args(arguments)
    families = {
        "agent_integrations": not parsed.without_agent_integrations,
        "local_automation": not parsed.without_local_automation,
    }

    try:
        table = inspect_symbols(parsed.binary)
    except verification_error as error:
        print(f"COMPOSED_HOSTS_FAILED: {error}", flush=True)
        return 1

    failures = audit_symbols(table.symbols, parsed.profile, **families)
    if failures:
        for failure in failures:
            print(f"COMPOSED_HOSTS_FAILED: {failure}", flush=True)
        return 1

    required_count = len(required_anchors(parsed.profile, **families))
    forbidden_count = len(forbidden_anchors(parsed.profile, **families))
    print(
        "COMPOSED_HOSTS_OK: "
        f"profile={parsed.profile} "
        f"agent_integrations={'present' if families['agent_integrations'] else 'absent'} "
        f"local_automation={'present' if families['local_automation'] else 'absent'} "
        f"required={required_count} "
        f"forbidden={forbidden_count} "
        f"inspector={table.inspector}",
        flush=True,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
