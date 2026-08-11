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


SHARED_REQUIRED = (
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
        "agent invocation application host constructor",
        "bpr::application::agent_invocation_host::agent_invocation_host",
    ),
    function_anchor(
        "Qt workbench window constructor",
        "bpr::qt::workbench_window::workbench_window",
    ),
)

FIXTURE_REQUIRED = (
    function_anchor(
        "review mutation coordinator constructor",
        "bpr::application::review_mutation_coordinator::review_mutation_coordinator",
    ),
    function_anchor(
        "fixture review mutation provider boundary",
        "bpr::app::detail::github_public_path_fixture_components::review_mutation_provider",
    ),
    function_anchor(
        "fixture review reconciliation decision",
        "bpr::application::reconcile",
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
    function_anchor(
        "review mutation coordinator constructor",
        "bpr::application::review_mutation_coordinator::review_mutation_coordinator",
    ),
    type_anchor(
        "GitHub public-path fixture components",
        "bpr::app::detail::github_public_path_fixture_components",
    ),
    function_anchor(
        "fixture review reconciliation decision",
        "bpr::application::reconcile",
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
        "(llvm-nm, nm plus c++filt/llvm-cxxfilt, or objdump/llvm-objdump)"
    )


def audit_symbols(symbols: Sequence[str], profile: str) -> list[str]:
    if profile not in PROFILES:
        raise ValueError(f"unknown composition profile: {profile}")

    required = SHARED_REQUIRED
    forbidden: tuple[symbol_anchor, ...]
    if profile == DEVELOPMENT_FIXTURE_PROFILE:
        required += FIXTURE_REQUIRED
        forbidden = FIXTURE_FORBIDDEN
    else:
        required += PRODUCTION_REQUIRED
        forbidden = PRODUCTION_FORBIDDEN

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
    parser.add_argument("binary", type=Path)
    parsed = parser.parse_args(arguments)

    try:
        table = inspect_symbols(parsed.binary)
    except verification_error as error:
        print(f"COMPOSED_HOSTS_FAILED: {error}", flush=True)
        return 1

    failures = audit_symbols(table.symbols, parsed.profile)
    if failures:
        for failure in failures:
            print(f"COMPOSED_HOSTS_FAILED: {failure}", flush=True)
        return 1

    required_count = len(SHARED_REQUIRED) + (
        len(FIXTURE_REQUIRED)
        if parsed.profile == DEVELOPMENT_FIXTURE_PROFILE
        else len(PRODUCTION_REQUIRED)
    )
    forbidden_count = (
        len(FIXTURE_FORBIDDEN)
        if parsed.profile == DEVELOPMENT_FIXTURE_PROFILE
        else len(PRODUCTION_FORBIDDEN)
    )
    print(
        "COMPOSED_HOSTS_OK: "
        f"profile={parsed.profile} "
        f"required={required_count} "
        f"forbidden={forbidden_count} "
        f"inspector={table.inspector}",
        flush=True,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
