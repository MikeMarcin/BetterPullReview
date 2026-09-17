#!/usr/bin/env python3
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import re
import unittest


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
PROTOCOL_HEADER = (
    REPOSITORY_ROOT
    / "src"
    / "application"
    / "include"
    / "bpr"
    / "application"
    / "git_protocol_v2.hpp"
)
PROTOCOL_SOURCE = (
    REPOSITORY_ROOT / "src" / "application" / "src" / "git_protocol_v2.cpp"
)
NATIVE_SMART_TRANSPORT_SOURCE = (
    REPOSITORY_ROOT
    / "src"
    / "adapters"
    / "github"
    / "src"
    / "native_https_transport.cpp"
)
RAW_HELPERS = (
    "discover_git_protocol_v2",
    "list_git_protocol_v2_refs",
    "fetch_git_protocol_v2",
)

NAMED_RAW_ROUND = re.compile(
    r"\b[A-Za-z_]*transport\s*(?P<operator>\.|->)\s*"
    r"(?:discover|ls_refs|fetch)\s*\("
)
RAW_ROUND = re.compile(
    r"(?P<operator>\.|->)\s*(?:discover|ls_refs|fetch)\s*\("
)
PRIVATE_HELPER = re.compile(
    r"\b(?:discover_git_protocol_v2|list_git_protocol_v2_refs|"
    r"fetch_git_protocol_v2)\s*\("
)


@dataclass(frozen=True)
class RawRoundAllowance:
    """Exact raw smart rounds that one named scope may make.

    Each call in `calls` must occur inside the one definition of
    `scope_name` in `path`, and exactly as many times as `calls` lists it.
    Any other raw round in that file still fails. So does an allowed call
    outside the scope, in another file, missing or repeated. The
    private-helper rule ignores allowances.
    """

    path: str
    scope_kind: str
    scope_name: str
    calls: tuple[str, ...]
    reason: str


RAW_ROUND_ALLOWANCES = (
    RawRoundAllowance(
        path="src/adapters/github/src/github_provider.cpp",
        scope_kind="class",
        scope_name="observed_git_transport",
        calls=(
            "Inner->discover(aRequest, aStop)",
            "Inner->ls_refs(aRequest, aStop)",
            "Inner->fetch(aRequest, aConsumer, aStop)",
        ),
        reason=(
            "[[BPR-0850]] A git_smart_transport_port decorator, not a new "
            "round. Each method forwards to the transport it wraps through "
            "perform_guarded, which adds only stop, revoke and observed-account "
            "checks. The class is private to cli_observed_account_transport, "
            "and only that factory's acquire constructs it, so complete "
            "acquisition receives it through the factory port."
        ),
    ),
    RawRoundAllowance(
        path="src/app/src/github_public_path_fixture.cpp",
        scope_kind="function",
        scope_name="verify_history_fetch_refusals",
        calls=("aTransport.fetch(request, consumer, { })",),
        reason=(
            "[[BPR-0850]] A refusal probe compiled only under BUILD_TESTING "
            "(src/app/CMakeLists.txt). It sends deliberately invalid fetch "
            "bodies to the fixture transport and requires invalid_request "
            "with no consumer calls. Complete acquisition never sends such a "
            "body, so the probe has to call fetch directly."
        ),
    ),
)


def production_sources() -> list[Path]:
    source_suffixes = {
        ".c",
        ".cc",
        ".cpp",
        ".cxx",
        ".h",
        ".hh",
        ".hpp",
        ".hxx",
    }
    return sorted(
        path
        for path in (REPOSITORY_ROOT / "src").rglob("*")
        if path.suffix in source_suffixes
    )


def repository_path(path: Path) -> str:
    return path.relative_to(REPOSITORY_ROOT).as_posix()


_LEXICAL_START = re.compile(r"//|/\*|[\"']")
_DOUBLE_QUOTED = re.compile(r'"(?:[^"\\\n]|\\[\s\S])*"?')
_SINGLE_QUOTED = re.compile(r"'(?:[^'\\\n]|\\[\s\S])*'?")
_RAW_STRING_PREFIXES = frozenset({"R", "u8R", "uR", "UR", "LR"})
_RAW_STRING_DELIMITER = re.compile(r'[^\s()\\"]{0,16}')
_NOT_NEWLINE = re.compile(r"[^\n]")


def _identifier_before(text: str, index: int) -> str:
    begin = index
    while begin > 0 and (text[begin - 1].isalnum() or text[begin - 1] == "_"):
        begin -= 1
    return text[begin:index]


def _literal_end(text: str, begin: int) -> int:
    prefix = _identifier_before(text, begin)
    if text[begin] == "'":
        return _SINGLE_QUOTED.match(text, begin).end()
    if prefix in _RAW_STRING_PREFIXES:
        delimiter = _RAW_STRING_DELIMITER.match(text, begin + 1)
        if text.startswith("(", delimiter.end()):
            closing = ")" + delimiter.group(0) + '"'
            end = text.find(closing, delimiter.end() + 1)
            return len(text) if end < 0 else end + len(closing)
    return _DOUBLE_QUOTED.match(text, begin).end()


def mask_comments_and_literals(text: str) -> str:
    """Blank C++ comments and literals, keeping every offset and newline.

    Scope extraction balances braces and parentheses on the masked text, so
    a brace inside a string, character literal or comment cannot move a
    scope boundary.
    """
    pieces: list[str] = []
    index = 0
    while (start := _LEXICAL_START.search(text, index)) is not None:
        begin = start.start()
        token = start.group(0)
        if token == "'" and _identifier_before(text, begin)[:1].isdigit():
            # A digit separator, as in 91'701.
            pieces.append(text[index : begin + 1])
            index = begin + 1
            continue
        if token == "//":
            end = text.find("\n", begin)
            end = len(text) if end < 0 else end
        elif token == "/*":
            end = text.find("*/", begin + 2)
            end = len(text) if end < 0 else end + 2
        else:
            end = _literal_end(text, begin)
        pieces.append(text[index:begin])
        pieces.append(_NOT_NEWLINE.sub(" ", text[begin:end]))
        index = end
    pieces.append(text[index:])
    return "".join(pieces)


def _matching_close(
    masked: str, open_index: int, opener: str, closer: str
) -> int | None:
    depth = 0
    for index in range(open_index, len(masked)):
        if masked[index] == opener:
            depth += 1
        elif masked[index] == closer:
            depth -= 1
            if depth == 0:
                return index
    return None


_FUNCTION_DECLARATOR_TAIL = re.compile(
    r"\s*(?:(?:const|volatile|noexcept|override|final|&&|&)\s*)*"
    r"(?:->\s*[^;{}()]+?\s*)?\{"
)


def scope_bodies(
    masked: str, scope_kind: str, scope_name: str
) -> list[tuple[int, int]]:
    """Return the (open brace, close brace) offsets of each definition.

    A class or struct definition is its name, an optional `final` and base
    clause, then a brace. A function definition is its name, a balanced
    parameter list, optional qualifiers and trailing return type, then a
    brace, so declarations and call sites are not definitions.
    """
    name = re.escape(scope_name)
    bodies: list[tuple[int, int]] = []
    if scope_kind == "class":
        definition = re.compile(
            rf"\b(?:class|struct)\s+{name}\s*(?:final\s*)?"
            rf"(?::[^;{{}}]*)?\{{"
        )
        for match in definition.finditer(masked):
            open_index = match.end() - 1
            close_index = _matching_close(masked, open_index, "{", "}")
            if close_index is not None:
                bodies.append((open_index, close_index))
    elif scope_kind == "function":
        for match in re.finditer(rf"(?<![\w.>:]){name}\s*\(", masked):
            parameters_close = _matching_close(
                masked, match.end() - 1, "(", ")"
            )
            if parameters_close is None:
                continue
            tail = _FUNCTION_DECLARATOR_TAIL.match(
                masked, parameters_close + 1
            )
            if tail is None:
                continue
            open_index = tail.end() - 1
            close_index = _matching_close(masked, open_index, "{", "}")
            if close_index is not None:
                bodies.append((open_index, close_index))
    else:
        raise ValueError(f"unknown scope kind: {scope_kind}")
    return bodies


_CALL_TOKEN = re.compile(r"[A-Za-z_]\w*|\d[\w']*|->|::|\S")


def call_tokens(call: str) -> tuple[str, ...]:
    return tuple(_CALL_TOKEN.findall(call))


def _line(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def _call_text(
    text: str, masked: str, operator: int, open_parenthesis: int
) -> str | None:
    """Return `receiver->round(arguments)` when the receiver is a bare name.

    A member (`a.b->round`), a qualified name (`a::b->round`) or an
    expression receiver (`f()->round`) returns None, so it can never match
    an allowance.
    """
    receiver_end = operator
    while receiver_end > 0 and text[receiver_end - 1].isspace():
        receiver_end -= 1
    receiver = _identifier_before(text, receiver_end)
    if not receiver or receiver[0].isdigit():
        return None
    receiver_begin = receiver_end - len(receiver)
    before = receiver_begin
    while before > 0 and text[before - 1].isspace():
        before -= 1
    if text[:before].endswith((".", "->", "::")):
        return None
    close = _matching_close(masked, open_parenthesis, "(", ")")
    if close is None:
        return None
    return text[receiver_begin : close + 1]


def raw_smart_round_findings(
    path: str,
    text: str,
    allowances: tuple[RawRoundAllowance, ...] = RAW_ROUND_ALLOWANCES,
) -> list[str]:
    """Return every raw smart-round finding for one repository-relative path."""
    if path == repository_path(PROTOCOL_SOURCE):
        return []
    findings = [
        f"{path}:{_line(text, match.start())}: private protocol helper "
        f"`{match.group(0)}`"
        for match in PRIVATE_HELPER.finditer(text)
    ]
    if path == repository_path(NATIVE_SMART_TRANSPORT_SOURCE):
        return findings

    smart_scope = "git_smart_transport" in text or "git_protocol_v2" in text
    rounds: dict[int, int] = {}
    for match in NAMED_RAW_ROUND.finditer(text):
        rounds[match.start("operator")] = match.end() - 1
    if smart_scope:
        for match in RAW_ROUND.finditer(text):
            rounds[match.start("operator")] = match.end() - 1

    unaccounted = set(rounds)
    masked: str | None = None
    for allowance in allowances:
        if allowance.path != path:
            continue
        if masked is None:
            masked = mask_comments_and_literals(text)
        scope = f"{allowance.scope_kind} {allowance.scope_name}"
        bodies = scope_bodies(masked, allowance.scope_kind, allowance.scope_name)
        if len(bodies) != 1:
            findings.append(
                f"{path}: allowance needs exactly one definition of {scope}, "
                f"found {len(bodies)}"
            )
            continue
        open_index, close_index = bodies[0]
        expected: dict[tuple[str, ...], tuple[str, int]] = {}
        for call in allowance.calls:
            shown, count = expected.get(call_tokens(call), (call, 0))
            expected[call_tokens(call)] = (shown, count + 1)
        seen: dict[tuple[str, ...], list[int]] = {key: [] for key in expected}
        for operator in sorted(unaccounted):
            if not open_index < operator < close_index:
                continue
            call = _call_text(text, masked, operator, rounds[operator])
            if call is not None and call_tokens(call) in seen:
                seen[call_tokens(call)].append(operator)
        for key, (shown, count) in expected.items():
            if len(seen[key]) == count:
                unaccounted.difference_update(seen[key])
                continue
            lines = ", ".join(str(_line(text, offset)) for offset in seen[key])
            findings.append(
                f"{path}: allowance expects `{shown}` {count} time(s) "
                f"in {scope}, found {len(seen[key])}"
                + (f" at line(s) {lines}" if lines else "")
            )

    if unaccounted and masked is None:
        masked = mask_comments_and_literals(text)
    for operator in sorted(unaccounted):
        call = _call_text(text, masked, operator, rounds[operator])
        line_end = text.find("\n", operator)
        shown = call or text[operator : len(text) if line_end < 0 else line_end]
        findings.append(
            f"{path}:{_line(text, operator)}: raw smart round `{shown.strip()}`"
        )
    return findings


class GitProtocolBoundaryTests(unittest.TestCase):
    def test_only_complete_acquisition_is_public(self) -> None:
        header = PROTOCOL_HEADER.read_text(encoding="utf-8")
        self.assertRegex(
            header,
            r"\bacquire_git_protocol_v2_pack\s*\(",
        )
        for helper in RAW_HELPERS:
            self.assertNotRegex(header, rf"\b{helper}\s*\(")

    def test_transport_round_helpers_have_internal_linkage(self) -> None:
        source = PROTOCOL_SOURCE.read_text(encoding="utf-8")
        for helper in RAW_HELPERS:
            self.assertRegex(source, rf"\bstatic\s+auto\s+{helper}\s*\(")

    def test_no_other_production_file_calls_raw_smart_rounds(self) -> None:
        findings: list[str] = []
        for path in production_sources():
            findings.extend(
                raw_smart_round_findings(
                    repository_path(path), path.read_text(encoding="utf-8")
                )
            )
        self.assertEqual(
            findings,
            [],
            "raw smart-transport rounds bypassed complete acquisition:\n"
            + "\n".join(findings),
        )

    def test_raw_round_allowances_are_explained(self) -> None:
        for allowance in RAW_ROUND_ALLOWANCES:
            with self.subTest(allowance=allowance.scope_name):
                self.assertTrue((REPOSITORY_ROOT / allowance.path).is_file())
                self.assertIn(allowance.scope_kind, {"class", "function"})
                self.assertTrue(allowance.calls)
                self.assertRegex(allowance.reason, r"\[\[BPR-\d{4}\]\]")


PROVIDER_PATH = "src/adapters/github/src/github_provider.cpp"
FIXTURE_PATH = "src/app/src/github_public_path_fixture.cpp"

SYNTHETIC_DECORATOR = """\
#include "bpr/application/git_smart_transport_port.hpp"
namespace bpr::adapters::github {
    class observed_git_transport;
    class cli_observed_account_transport final
        : public application::git_smart_transport_factory_port
    {
        class observed_git_transport final
            : public application::git_smart_transport_port
        {
        public:
            auto discover(const request& aRequest, std::stop_token aStop)
                -> result override
            {
                return perform_guarded(aStop, [&] { return Inner->discover(aRequest, aStop); });
            }
            auto ls_refs(const request& aRequest, std::stop_token aStop)
                -> result override
            {
                return perform_guarded(aStop, [&] { return Inner->ls_refs(aRequest, aStop); });
            }
            auto fetch(const request& aRequest, consumer& aConsumer, std::stop_token aStop)
                -> result override
            {
                return perform_guarded(aStop, [&] { return Inner->fetch(aRequest, aConsumer, aStop); });
            }
            // EXTRA_MEMBER
        private:
            application::git_smart_transport Inner;
        };

    public:
        auto acquire(const context& aContext) -> result override
        {
            // OUTSIDE_SCOPE
            return std::make_shared<observed_git_transport>(shared_from_this(), *inner);
        }
    };
}
"""

SYNTHETIC_PROBE = """\
#include "bpr/application/git_smart_transport_port.hpp"
namespace {
    auto verify_history_fetch_refusals(application::git_smart_transport_port& aTransport,
                                       const context& aContext) -> bool;

    [[nodiscard]] auto verify_history_fetch_refusals(application::git_smart_transport_port& aTransport,
                                                     const context& aContext) -> bool
    {
        for (const auto substitute : { true, false }) {
            refusal_consumer consumer;
            // BEFORE_CALL
            const auto result = aTransport.fetch(request, consumer, { });
            if (result) {
                return false;
            }
        }
        return true;
    }

    auto acquire(const context& aContext) -> result
    {
        auto transport = make_transport(aContext);
        if (!verify_history_fetch_refusals(*transport, aContext)) {
            return refused();
        }
        // OUTSIDE_SCOPE
        return transport;
    }
}
"""


def edited(text: str, old: str, new: str) -> str:
    """Replace exactly one occurrence, so a stale anchor fails loudly."""
    if text.count(old) != 1:
        raise AssertionError(f"expected one {old!r}, found {text.count(old)}")
    return text.replace(old, new)


def last_line_of(text: str, needle: str) -> int:
    return text.count("\n", 0, text.rindex(needle)) + 1


class RawRoundAllowanceMatcherTests(unittest.TestCase):
    def assert_clean(self, path: str, text: str) -> None:
        self.assertEqual(raw_smart_round_findings(path, text), [])

    def assert_fails(self, path: str, text: str, *fragments: str) -> None:
        findings = raw_smart_round_findings(path, text)
        self.assertTrue(findings, "expected the matcher to refuse the text")
        report = "\n".join(findings)
        for fragment in fragments:
            self.assertIn(fragment, report)

    def test_decorator_forwarding_calls_are_allowed(self) -> None:
        self.assert_clean(PROVIDER_PATH, SYNTHETIC_DECORATOR)

    def test_extra_decorator_call_in_the_class_fails(self) -> None:
        repeated = edited(
            SYNTHETIC_DECORATOR,
            "// EXTRA_MEMBER",
            "void again(std::stop_token aStop) "
            "{ Inner->fetch(aRequest, aConsumer, aStop); }",
        )
        self.assert_fails(PROVIDER_PATH, repeated, "found 2 at line(s)")
        different = edited(
            SYNTHETIC_DECORATOR,
            "// EXTRA_MEMBER",
            "void again(std::stop_token aStop) "
            "{ Inner->ls_refs(other, aStop); }",
        )
        self.assert_fails(
            PROVIDER_PATH,
            different,
            "raw smart round `Inner->ls_refs(other, aStop)`",
        )

    def test_decorator_call_outside_the_class_fails(self) -> None:
        call = "Inner->discover(aRequest, aStop)"
        text = edited(SYNTHETIC_DECORATOR, "// OUTSIDE_SCOPE", call + ";")
        self.assertEqual(
            raw_smart_round_findings(PROVIDER_PATH, text),
            [
                f"{PROVIDER_PATH}:{last_line_of(text, call)}: "
                f"raw smart round `{call}`"
            ],
        )

    def test_decorator_call_on_another_receiver_fails(self) -> None:
        for receiver in (
            "Other->discover",
            "Owner->Inner->discover",
            "Owner.Inner->discover",
            "Inner()->discover",
        ):
            with self.subTest(receiver=receiver):
                text = edited(SYNTHETIC_DECORATOR, "Inner->discover", receiver)
                self.assert_fails(
                    PROVIDER_PATH, text, "found 0", "raw smart round"
                )

    def test_decorator_call_with_another_round_name_fails(self) -> None:
        text = edited(
            SYNTHETIC_DECORATOR,
            "Inner->ls_refs(aRequest, aStop)",
            "Inner->discover(aRequest, aStop)",
        )
        self.assert_fails(
            PROVIDER_PATH,
            text,
            "`Inner->discover(aRequest, aStop)` 1 time(s) in class "
            "observed_git_transport, found 2",
            "`Inner->ls_refs(aRequest, aStop)` 1 time(s) in class "
            "observed_git_transport, found 0",
        )

    def test_decorator_text_in_another_file_fails(self) -> None:
        findings = raw_smart_round_findings(
            "src/adapters/github/src/other_provider.cpp", SYNTHETIC_DECORATOR
        )
        self.assertEqual(len(findings), 3, findings)

    def test_private_helper_fails_inside_an_allowed_scope(self) -> None:
        text = edited(
            SYNTHETIC_DECORATOR,
            "// EXTRA_MEMBER",
            "void again() { fetch_git_protocol_v2(Inner); }",
        )
        self.assert_fails(PROVIDER_PATH, text, "private protocol helper")

    def test_probe_fetch_is_allowed(self) -> None:
        self.assert_clean(FIXTURE_PATH, SYNTHETIC_PROBE)

    def test_extra_probe_call_in_the_function_fails(self) -> None:
        text = edited(
            SYNTHETIC_PROBE,
            "// BEFORE_CALL",
            "aTransport.fetch(request, consumer, { });",
        )
        self.assert_fails(FIXTURE_PATH, text, "found 2 at line(s)")

    def test_probe_call_outside_the_function_fails(self) -> None:
        call = "aTransport.fetch(request, consumer, { })"
        text = edited(SYNTHETIC_PROBE, "// OUTSIDE_SCOPE", call + ";")
        self.assertEqual(
            raw_smart_round_findings(FIXTURE_PATH, text),
            [
                f"{FIXTURE_PATH}:{last_line_of(text, call)}: "
                f"raw smart round `{call}`"
            ],
        )

    def test_probe_call_on_another_receiver_or_round_fails(self) -> None:
        for call in (
            "aOther.fetch(request, consumer, { })",
            "aTransport.ls_refs(request, { })",
            "aTransport.discover(request, { })",
        ):
            with self.subTest(call=call):
                text = edited(
                    SYNTHETIC_PROBE,
                    "aTransport.fetch(request, consumer, { })",
                    call,
                )
                self.assert_fails(FIXTURE_PATH, text, "found 0", call)

    def test_probe_text_in_another_file_fails(self) -> None:
        self.assert_fails(
            "src/app/src/github_application.cpp",
            SYNTHETIC_PROBE,
            "raw smart round",
        )

    def test_literals_and_comments_do_not_move_scope_boundaries(self) -> None:
        closing_noise = (
            "const auto a = \"}\"; const auto b = '}'; // }\n"
            "            const auto c = R\"json({\"k\": }})json\"; /* } */\n"
            "            const auto d = 91'701;"
        )
        self.assert_clean(
            FIXTURE_PATH, edited(SYNTHETIC_PROBE, "// BEFORE_CALL", closing_noise)
        )
        opening_noise = edited(
            edited(SYNTHETIC_PROBE, "// BEFORE_CALL", "const auto a = \"{\"; // {"),
            "// OUTSIDE_SCOPE",
            "aTransport.fetch(request, consumer, { });",
        )
        call = "aTransport.fetch(request, consumer, { })"
        self.assertEqual(
            raw_smart_round_findings(FIXTURE_PATH, opening_noise),
            [
                f"{FIXTURE_PATH}:{last_line_of(opening_noise, call)}: "
                f"raw smart round `{call}`"
            ],
        )

    def test_missing_or_repeated_scope_fails(self) -> None:
        renamed = edited(
            SYNTHETIC_DECORATOR,
            "class observed_git_transport final",
            "class guarded_git_transport final",
        )
        self.assert_fails(
            PROVIDER_PATH, renamed, "class observed_git_transport, found 0"
        )
        self.assert_fails(
            FIXTURE_PATH,
            SYNTHETIC_PROBE + SYNTHETIC_PROBE,
            "function verify_history_fetch_refusals, found 2",
        )


if __name__ == "__main__":
    unittest.main()
