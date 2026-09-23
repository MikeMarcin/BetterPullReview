#!/usr/bin/env python3
"""Find names a rename reached in one branch and not another.

A rename applied only to the code the local compiler builds leaves the old
spelling everywhere else, and "everywhere else" is exactly the
`#if defined(_WIN32)` and `#elif defined(__linux__)` branches the preprocessor
removes before the compiler on this host ever sees them. Nothing fails, because
nothing looks.

That is what happened before BPR-0155 and again before BPR-0157: translation
units that did not compile for Windows or Linux, with the local gate green
throughout.

A cross-compiler catches part of the Windows half and none of the Linux half,
so this reads the source instead. It looks only where the local compiler
cannot — inside conditional branches that are false for the host running it —
and leaves everything the compiler already checks to the compiler.

"The host" is its platform, not one build of it ([[BPR-0866]]). A branch that a
platform macro removes (`_WIN32`, `__APPLE__`, `Q_OS_WIN` and the others in
`PLATFORM_MACROS`) is removed from every build on this host, so the gate
polices it. That includes such a branch nested inside a testing region. A
branch that only a build switch removes, such as `BPR_BUILD_TESTING`, is a
branch this host's builds disagree on. The tests-enabled build compiles one
side, and the tests-disabled build that `application_composition_contract`
makes compiles the other. So the compiler checks both, and the gate polices
neither. Treating such a switch as undefined polices code the `dev` build
compiles, and that is where all eleven of BPR-0854's findings were.

What a green result asserts is narrower than it reads. The gate reports a name
whose spelling, folded for case and underscores, matches a different name
declared beside it. A typo that matches nothing declared is not reported, so
`CONDITIONAL_BRANCH_IDENTIFIERS_OK` does not mean the branches it read are free
of misspellings. Shapes 1 and 2 below are read only inside a function with an
`aXxx` parameter. And the gate sees preprocessor branches, not CMake: a file
built only on another platform is read only inside its own platform branches.

Three shapes, each one a rename that stopped halfway:

1. A parameter's old bare name. Parameters here are `aXxx`; the branch still
   says `xxx`.
2. Any name under a different spelling. The branch says `expected_bytes` where
   the declaration now reads `aExpectedBytes`, or `final_path` where the local
   is `finalPath`.
3. A name a conditional chain declares in only some of its branches and uses
   after the `#endif`. `#if defined(__linux__)` declaring `requested_vfs`
   beside an `#else` declaring `requestedVfs` compiles wherever the `#else` is
   taken and nowhere else — both spellings exist, so nothing looks undefined
   until a compiler on the other platform tries.
"""

from __future__ import annotations

import argparse
import pathlib
import platform
import re
import sys

PARAMETER = re.compile(r"\ba([A-Z][A-Za-z0-9_]*)\b")

SOURCE_SUFFIXES = (".cpp", ".hpp")

DIRECTIVE = re.compile(r"^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)$")

# A statement keyword cannot be a type, so `return _read(x)` declares nothing.
# Without this the call reads as a declaration of `_read`, and because
# `spelling_key` strips underscores that collides with the POSIX `read` in the
# opposite branch — reporting a rename where there are two correct spellings of
# two different functions.
# Deliberately excludes `using` and `typedef`: `using reason = ...;` really does
# declare `reason`, and dropping them would lose that.
STATEMENT_KEYWORDS = (
    "return|else|do|try|case|goto|throw|delete"
    "|co_return|co_await|co_yield"
)

DECLARATION = re.compile(
    rf"\b(?!(?:{STATEMENT_KEYWORDS})\b)"
    r"(?:auto|[A-Za-z_]\w*(?:::\w+)*(?:\s*<[^;{}\n]*?>)?"
    r"(?:\s*(?:const\s*)?[*&]+)?)\s+(?:const\s+)?([a-z_][A-Za-z0-9_]*)\s*[=({;]")

# A lambda's init-capture declares a name with no type in front of it, so
# `DECLARATION` cannot see it. `[options = std::move(aOptions)]` then reads as a
# use of `options` that nothing declares, and because `spelling_key` folds case
# that collides with the very `aOptions` it was initialized from -- a rename
# reported against the capture's own initializer.
# Strict in both directions, because only the name to the left of `=` is taken:
# the initializer is ordinary code, so `[value = expectedBytes]` beside an
# `aExpectedBytes` is still a finding, as is a stale spelling in the body the
# lambda goes on to write. A capture list whose initializer carries a bracket or
# a brace is not recognized at all, which reports rather than excuses.
CAPTURE_LIST = re.compile(r"\[([^\[\]{};]*=[^\[\]{};]*)\]\s*[({]")

INIT_CAPTURE = re.compile(
    r"(?:^|,)\s*(?:&\s*)?(?:\.\.\.\s*)?([a-z_][A-Za-z0-9_]*)\s*=(?!=)")

# The braced body of a declarator: what tells a definition from everything else
# that ends in `)`.
BODY_BRACE = re.compile(r"\)\s*(?:const\s+)?(?:noexcept\s*)?(?:->[^{;()]+)?\{")

# The name immediately left of a declarator's `(`. A lambda's `](` has none, so
# an introducer is never read as a function name.
DECLARATOR = re.compile(r"([A-Za-z_]\w*)\s*$")

CALL = re.compile(r"\s*\(")

IDENTIFIER = re.compile(r"(?<![\w:.>])([a-z_][A-Za-z0-9_]*)\b(?!\s*::)")

# Names that read like identifiers and are not.
KEYWORDS = frozenset(
    """auto const constexpr consteval constinit return if else for while switch
    case break continue true false nullptr static sizeof alignof new delete this
    using namespace struct class enum union template typename void int char bool
    unsigned signed long short float double do try catch throw noexcept operator
    public private protected virtual override final inline explicit mutable goto
    default typedef friend extern volatile register thread_local static_assert
    and or not xor bitand bitor compl static_cast reinterpret_cast const_cast
    dynamic_cast decltype requires concept co_await co_return co_yield std""".split()
)


def spelling_key(aName: str) -> str:
    """What two spellings of one name have in common."""
    return aName.replace("_", "").lower()


# The macros whose value the host's platform fixes: its operating system, Qt's
# names for that system, and the compiler every Windows build here uses. Each
# is defined on the host if `host_macros()` names it and undefined otherwise,
# and a branch one of them removes is compiled by no build on this host.
#
# Every other macro is undecided: `BPR_BUILD_TESTING` and the other testing,
# fault-injection and fixture switches, compiler identity (`__clang__`,
# `__GNUC__`), feature and include-guard macros. Builds on one host disagree
# about them, so some build on the host compiles a branch that only they remove
# ([[BPR-0866]]). A platform macro missing from this list would be undecided
# too, which is the quiet direction, so `unknown_platform_macros` refuses one
# by name until it is added here and to `host_macros()`.
PLATFORM_MACROS = frozenset(
    {
        "_WIN32",
        "_MSC_VER",
        "__APPLE__",
        "__linux__",
        "__unix__",
        "Q_OS_WIN",
        "Q_OS_MACOS",
        "Q_OS_DARWIN",
        "Q_OS_LINUX",
        "Q_OS_UNIX",
    }
)


# What a platform macro looks like: Qt's `Q_OS_*`, the compilers' operating-system
# macros, and the Windows compiler's. A condition naming one that
# `PLATFORM_MACROS` lacks would leave its branch undecided and unpoliced on
# every host that does not compile it, so the gate refuses it by name instead.
PLATFORM_SPELLING = re.compile(
    r"^(?:Q_OS_\w+|_WIN(?:32|64)|WIN(?:32|64)|_MSC_VER|__MINGW(?:32|64)__"
    r"|__(?:APPLE|MACH|linux|gnu_linux|unix|ANDROID|CYGWIN|FreeBSD|NetBSD|OpenBSD)__"
    r"|__linux|__unix|linux|unix|TARGET_OS_\w+)$"
)


def unknown_platform_macros(aText: str) -> list[tuple[int, str, str]]:
    """Conditions that name a platform-looking macro the gate does not decide."""
    found: list[tuple[int, str, str]] = []
    for number, line in enumerate(aText.split("\n"), start=1):
        match = DIRECTIVE.match(line)
        if match is None or match.group(1) in ("else", "endif"):
            continue
        rest = match.group(2)
        names = (
            re.findall(r"^\s*(\w+)", rest)
            if match.group(1) in ("ifdef", "ifndef")
            else re.findall(r"\bdefined\s*\(?\s*(\w+)", rest)
        )
        for name in names:
            if PLATFORM_SPELLING.match(name) and name not in PLATFORM_MACROS:
                found.append((number, name, line.strip()))
    return found


def host_macros() -> set[str]:
    """The platform macros that are defined where this is running.

    Qt defines `Q_OS_UNIX` everywhere except Windows. Apple's compiler does not
    define `__unix__`, which is why the tree writes
    `defined(__unix__) || defined(__APPLE__)`. Windows builds here use MSVC
    only, so `_MSC_VER` follows `_WIN32`.
    """
    system = platform.system()
    if system == "Darwin":
        return {"__APPLE__", "Q_OS_MACOS", "Q_OS_DARWIN", "Q_OS_UNIX"}
    if system == "Windows":
        return {"_WIN32", "_MSC_VER", "Q_OS_WIN"}
    if system == "Linux":
        return {"__linux__", "__unix__", "Q_OS_LINUX", "Q_OS_UNIX"}
    return set()


TOKEN = re.compile(r"\s*(\(|\)|&&|\|\||!|defined\s*\(\s*\w+\s*\)|defined\s+\w+|\S)")


class Unparsed(Exception):
    """A condition that is not built only from `defined`, `!`, `&&` and `||`."""


def negation(aValue: bool | None) -> bool | None:
    return None if aValue is None else not aValue


def conjoined(aLeft: bool | None, aRight: bool | None) -> bool | None:
    """`&&` over undecided values: false if either side is false, whatever the other."""
    if aLeft is False or aRight is False:
        return False
    if aLeft is None or aRight is None:
        return None
    return True


def disjoined(aLeft: bool | None, aRight: bool | None) -> bool | None:
    """`||` over undecided values: true if either side is true, whatever the other."""
    return negation(conjoined(negation(aLeft), negation(aRight)))


def evaluate(aCondition: str, aDefined: set[str]) -> bool | None:
    """Evaluate a preprocessor condition on this host, or None when the host does not decide it.

    `defined(X)` is decided only for a platform macro. Any other macro is
    undecided, and so is its branch, unless a platform macro decides the whole
    condition: `defined(_WIN32) && defined(BPR_BUILD_TESTING)` is false on
    Linux whatever the build switches are. Only `defined(X)`, `!`, `&&`, `||`
    and parentheses are understood. Anything else is undecided and its branch
    is treated as compiled here, which is the safe direction: the compiler is
    then the check.

    Parsed rather than evaluated. Nothing in this repository executes text it
    read from a file, and a gate that reads every source file is the last place
    to start.
    """
    tokens: list[str] = []
    position = 0
    while position < len(aCondition):
        match = TOKEN.match(aCondition, position)
        if match is None:
            break
        tokens.append(re.sub(r"\s+", "", match.group(1)))
        position = match.end()
    if not tokens or not any(token.startswith("defined") for token in tokens):
        return None

    index = 0

    def primary() -> bool | None:
        nonlocal index
        if index >= len(tokens):
            raise Unparsed
        token = tokens[index]
        if token == "!":
            index += 1
            return negation(primary())
        if token == "(":
            index += 1
            value = disjunction()
            if index >= len(tokens) or tokens[index] != ")":
                raise Unparsed
            index += 1
            return value
        if token.startswith("defined"):
            index += 1
            name = re.sub(r"^defined\(?|\)?$", "", token)
            return name in aDefined if name in PLATFORM_MACROS else None
        raise Unparsed

    def conjunction() -> bool | None:
        nonlocal index
        value = primary()
        while index < len(tokens) and tokens[index] == "&&":
            index += 1
            value = conjoined(value, primary())
        return value

    def disjunction() -> bool | None:
        nonlocal index
        value = conjunction()
        while index < len(tokens) and tokens[index] == "||":
            index += 1
            value = disjoined(value, conjunction())
        return value

    try:
        result = disjunction()
    except Unparsed:
        return None
    return result if index == len(tokens) else None


def inactive_lines(aText: str, aDefined: set[str]) -> set[int]:
    """Line numbers (1-based) inside a branch this host does not compile.

    A branch of a chain is compiled when its own condition holds and no earlier
    branch's did, so an `#else` after a taken `#elif` is not compiled, and
    neither is an `#elif` whose condition is false after an undecided `#if`.
    Each is tracked with undecided values, so a branch is inactive only when
    that is false whatever the build switches are.
    """
    inactive: set[int] = set()
    # Per open chain: whether this branch is compiled, and whether an earlier
    # branch of the chain already was.
    stack: list[dict[str, bool | None]] = []
    for number, line in enumerate(aText.split("\n"), start=1):
        match = DIRECTIVE.match(line)
        if match is not None:
            keyword, rest = match.group(1), match.group(2)
            if keyword in ("if", "ifdef", "ifndef"):
                if keyword == "ifdef":
                    rest = f"defined({rest.strip()})"
                elif keyword == "ifndef":
                    rest = f"!defined({rest.strip()})"
                value = evaluate(rest, aDefined)
                stack.append({"compiled": value, "earlier": value})
            elif keyword == "elif" and stack:
                frame = stack[-1]
                value = evaluate(rest, aDefined)
                frame["compiled"] = conjoined(negation(frame["earlier"]), value)
                frame["earlier"] = disjoined(frame["earlier"], value)
            elif keyword == "else" and stack:
                frame = stack[-1]
                frame["compiled"] = negation(frame["earlier"])
                frame["earlier"] = True
            elif keyword == "endif" and stack:
                stack.pop()
            continue
        if any(frame["compiled"] is False for frame in stack):
            inactive.add(number)
    return inactive


def is_digit_separator(aText: str, aIndex: int) -> bool:
    """Whether the `'` at `aIndex` is a digit separator, as in `10'000`.

    Read as a quote, it opens a character literal that runs to the next `'` in
    the file, often lines away, and blanks every brace in between. The bodies
    around it then have the wrong extents: a local reads as a stale spelling of
    a parameter it never belonged to, and a real rename inside the blanked
    span is hidden ([[BPR-0854]]). A separator sits inside a numeric literal,
    so the token before it starts with a digit and a digit or letter follows.
    A prefixed character literal (`u8'x'`, `L'x'`) starts with a letter, and a
    bare one follows punctuation, so both still open a literal.
    """
    start = aIndex
    while start > 0 and (aText[start - 1].isalnum() or aText[start - 1] in "_.'"):
        start -= 1
    token = aText[start:aIndex]
    following = aText[aIndex + 1 : aIndex + 2]
    return bool(token) and token[0].isdigit() and following.isalnum()


def blank_comments_and_literals(aText: str) -> str:
    """Replace comment and string bodies with spaces, keeping every offset."""
    out = list(aText)
    index = 0
    length = len(aText)
    while index < length:
        character = aText[index]
        if character == "/" and index + 1 < length and aText[index + 1] == "/":
            while index < length and aText[index] != "\n":
                out[index] = " "
                index += 1
            continue
        if character == "/" and index + 1 < length and aText[index + 1] == "*":
            while index < length and not (
                aText[index] == "*" and index + 1 < length and aText[index + 1] == "/"
            ):
                if aText[index] != "\n":
                    out[index] = " "
                index += 1
            for _ in range(2):
                if index < length:
                    out[index] = " "
                    index += 1
            continue
        if character == "'" and is_digit_separator(aText, index):
            index += 1
            continue
        if character in "\"'":
            quote = character
            index += 1
            while index < length and aText[index] != quote:
                if aText[index] == "\\":
                    out[index] = " "
                    index += 1
                if index < length:
                    if aText[index] != "\n":
                        out[index] = " "
                    index += 1
            if index < length:
                index += 1
            continue
        index += 1
    return "".join(out)


def declarator_parentheses(aText: str, aEnd: int) -> tuple[int, int] | None:
    """The `(` and `)` of the declarator whose body brace ends at `aEnd`."""
    closing = aText.rindex(")", 0, aEnd)
    depth = 0
    for index in range(closing, -1, -1):
        if aText[index] == ")":
            depth += 1
        elif aText[index] == "(":
            depth -= 1
            if depth == 0:
                return index, closing
    return None


def defined_functions(aText: str) -> set[str]:
    """The names this file defines as functions.

    Defines, not merely declares: the declarator's own closing parenthesis has
    to reach a body brace. That is what separates `void run(int aValue) { ... }`
    from `auto run(compute());`, a variable whose parenthesized initializer
    reads the same to a regular expression, and it is the same bracket walk
    `function_bodies` uses, so a lambda introducer cannot pass for a name.
    """
    names: set[str] = set()
    for match in BODY_BRACE.finditer(aText):
        parentheses = declarator_parentheses(aText, match.end())
        if parentheses is None:
            continue
        opening = parentheses[0]
        declarator = DECLARATOR.search(aText[max(0, opening - 200):opening])
        if declarator is not None and declarator.group(1) not in KEYWORDS:
            names.add(declarator.group(1))
    return names


def function_bodies(aText: str):
    """Yield (parameters, body_start, body_end) for every braced body.

    A statement's parentheses are not a parameter list. `for (; prefix !=
    aPrefix.end(); ++prefix)` names `aPrefix` without declaring it, and the
    loop's block does not hold the `auto prefix` declared just above it. Read
    as a function, the block has a parameter `aPrefix` and an undeclared
    `prefix`, a rename that is not there ([[BPR-0866]]). The enclosing
    function's own body already covers the block with its real parameters.
    """
    for match in BODY_BRACE.finditer(aText):
        parentheses = declarator_parentheses(aText, match.end())
        if parentheses is None:
            continue
        opening, closing = parentheses
        declarator = DECLARATOR.search(aText[max(0, opening - 200):opening])
        if declarator is not None and declarator.group(1) in KEYWORDS:
            continue
        parameters = {m.group(1) for m in PARAMETER.finditer(aText[opening:closing])}
        if not parameters:
            continue
        start = aText.index("{", match.end() - 1)
        depth = 0
        for index in range(start, len(aText)):
            if aText[index] == "{":
                depth += 1
            elif aText[index] == "}":
                depth -= 1
                if depth == 0:
                    yield parameters, start, index
                    break


def declared_in(aRegion: str, aName: str) -> bool:
    """Whether the region itself introduces `aName`."""
    name = re.escape(aName)
    patterns = (
        rf"\bauto\b[\w\s:<>,*&]*?\b{name}\b\s*[=({{;:]",
        rf"\b[A-Za-z_]\w*(?:::\w+)*\s*[<>\w\s,:*&]*?\b{name}\b\s*[=({{;]",
        rf"\[[^\]]*\b{name}\b[^\]]*\]\s*\(",
        rf"\(\s*[^)]*\b{name}\b\s*\)\s*(?:mutable\s*)?(?:->[^{{]+)?\{{",
    )
    return any(re.search(pattern, aRegion) for pattern in patterns)


def captured_in(aRegion: str, aName: str) -> bool:
    """Whether a lambda in `aRegion` captures the outer `aName` as it is.

    Only a simple capture counts, `name` or `&name`. An init-capture's
    initializer is ordinary code, so `[bytes = expectedBytes]` excuses nothing.
    """
    for introducer in re.finditer(r"\[([^\[\]{};]*)\]\s*\(", aRegion):
        for capture in introducer.group(1).split(","):
            if re.sub(r"\s+", "", capture) in (aName, f"&{aName}"):
                return True
    return False


def init_captures(aRegion: str) -> set[str]:
    """The names lambda init-captures introduce in `aRegion`."""
    return {
        capture.group(1)
        for introducer in CAPTURE_LIST.finditer(aRegion)
        for capture in INIT_CAPTURE.finditer(introducer.group(1))
    }


def conditional_chains(aText: str):
    """Yield (start_line, end_line, [declared names per branch]) per chain."""
    stack: list[dict] = []
    chains: list[tuple[int, int, list[set[str]]]] = []
    for number, line in enumerate(aText.split("\n"), start=1):
        match = DIRECTIVE.match(line)
        if match is None:
            if stack:
                for declaration in DECLARATION.finditer(line):
                    name = declaration.group(1)
                    if name not in KEYWORDS:
                        stack[-1]["branches"][-1].add(name)
            continue
        keyword = match.group(1)
        if keyword in ("if", "ifdef", "ifndef"):
            stack.append({"branches": [set()], "start": number})
        elif keyword in ("elif", "else") and stack:
            stack[-1]["branches"].append(set())
        elif keyword == "endif" and stack:
            frame = stack.pop()
            chains.append((frame["start"], number, frame["branches"]))
    return chains


def escaped_names(aText: str, aLines: list[str]) -> list[tuple[int, str, str, str]]:
    """Names a chain declares in only some branches and uses after `#endif`.

    `#if defined(__linux__)` declaring `requested_vfs` while its `#else`
    declares `requestedVfs` compiles wherever the `#else` is taken and nowhere
    else. Both spellings exist, so nothing looks undefined until a compiler on
    the other platform tries.
    """
    lines = aText.split("\n")
    found: list[tuple[int, str, str, str]] = []
    for start, end, branches in conditional_chains(aText):
        if len(branches) < 2:
            continue
        union: set[str] = set().union(*branches)
        for name in sorted(union):
            if all(name in branch for branch in branches):
                continue
            siblings = sorted(
                other
                for other in union
                if other != name and spelling_key(other) == spelling_key(name)
            )
            if not siblings:
                continue
            pattern = re.compile(rf"(?<![\w:.>]){re.escape(name)}\b(?!\s*::)")
            for number in range(end + 1, min(end + 200, len(lines) + 1)):
                if pattern.search(lines[number - 1]):
                    found.append((number, name, siblings[0], aLines[number - 1].strip()))
                    break
    return found


def findings_for(aPath: pathlib.Path, aDefined: set[str]) -> list[tuple[int, str, str, str]]:
    raw = aPath.read_text(encoding="utf-8")
    skipped = inactive_lines(raw, aDefined)
    if not skipped:
        return []
    text = blank_comments_and_literals(raw)
    lines = raw.split("\n")
    # `schedule_native_notification_run(...)` beside a local
    # `scheduleNativeNotificationRun` is a call to a file-scope function, not a
    # stale spelling of the local: C++ tells the two apart by exactly the
    # underscores and the case `spelling_key` folds away, and both are supposed
    # to exist. The exemption below needs both halves -- the name is in call
    # position, and this file defines a function of exactly that spelling -- so
    # a call to a name nothing here declares is still a finding, and so is a
    # call-position name whose only same-spelling declaration is a variable.
    # Neither of those would compile in the branch that holds it.
    functions = defined_functions(text)
    found: set[tuple[int, str, str, str]] = set()
    for parameters, start, end in function_bodies(text):
        body = text[start:end]
        first = text.count("\n", 0, start) + 1
        last = text.count("\n", 0, end) + 1
        region = "\n".join(
            lines[number - 1] for number in range(first, last + 1) if number in skipped
        )
        if not region:
            continue

        declared = {f"a{parameter}" for parameter in parameters}
        declared |= {
            declaration.group(1)
            for declaration in DECLARATION.finditer(body)
            if declaration.group(1) not in KEYWORDS
        }
        declared |= {
            name for name in init_captures(body) if name not in KEYWORDS
        }
        by_spelling: dict[str, set[str]] = {}
        for name in declared:
            by_spelling.setdefault(spelling_key(name), set()).add(name)
        # A parameter's bare form is not a declaration, but it is what the old
        # spelling normalizes to, so it has to resolve to the parameter rather
        # than to itself. Reporting `expectedBytes` when the fix is
        # `aExpectedBytes` sends the reader to the wrong edit.
        for parameter in parameters:
            bare = parameter[0].lower() + parameter[1:]
            by_spelling.setdefault(spelling_key(bare), set()).add(f"a{parameter}")

        for parameter in parameters:
            bare = parameter[0].lower() + parameter[1:]
            # A bare form the body declares as its own local -- `auto address
            # = validated(aAddress);` before the chain -- is that local, not a
            # stale spelling of the parameter, wherever the branch that uses it
            # sits. Only a bare form nothing in the body or the branch declares
            # is the rename this check exists for.
            if bare in declared or declared_in(region, bare):
                continue
            # Not after `::`, `.`, or `->`, and not before `::`: a namespace
            # qualifier that happens to match a parameter's bare name is not a
            # reference to that parameter.
            for match in re.finditer(
                rf"(?<![\w:.>]){re.escape(bare)}\b(?!\s*::)", body):
                offset = start + match.start()
                line = text.count("\n", 0, offset) + 1
                if line not in skipped:
                    continue
                if bare in functions and CALL.match(body, match.end()):
                    continue
                found.add((line, bare, f"a{parameter}", lines[line - 1].strip()))

        # The same name under a different spelling. A rename that reached the
        # declaration but not this branch leaves `expected_bytes` beside an
        # `aExpectedBytes` that no longer answers to it.
        for match in IDENTIFIER.finditer(body):
            name = match.group(1)
            if name in KEYWORDS or name in declared:
                continue
            # A lambda in the branch that captures an outer local names that
            # local, not a stale spelling of its own parameter:
            # `[&session](const auto& aSession)` compares the two
            # ([[BPR-0854]]). Only the capture list excuses a name here, so a
            # different stale spelling in the same lambda is still reported.
            if captured_in(region, name):
                continue
            line = text.count("\n", 0, start + match.start()) + 1
            if line not in skipped:
                continue
            if name in functions and CALL.match(body, match.end()):
                continue
            siblings = by_spelling.get(spelling_key(name))
            if siblings:
                found.add((line, name, sorted(siblings)[0], lines[line - 1].strip()))

    for entry in escaped_names(text, lines):
        found.add(entry)
    return sorted(found)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", default=".")
    arguments = parser.parse_args()

    root = pathlib.Path(arguments.root).resolve()
    defined = host_macros()
    sources = sorted(
        path
        for directory in ("src", "tests")
        for path in (root / directory).rglob("*")
        if path.suffix in SOURCE_SUFFIXES
    )

    total = 0
    policed_lines = 0
    policed_files = 0
    for path in sources:
        text = path.read_text(encoding="utf-8")
        policed = len(inactive_lines(text, defined))
        policed_lines += policed
        policed_files += policed > 0
        for line, name, source in unknown_platform_macros(text):
            total += 1
            print(
                f"{path.relative_to(root)}:{line}: `{name}` names a platform this "
                f"gate does not decide; add it to PLATFORM_MACROS and "
                f"host_macros(): {source}"
            )
        for line, bare, expected, source in findings_for(path, defined):
            total += 1
            print(
                f"{path.relative_to(root)}:{line}: `{bare}` is used where the "
                f"declared name is `{expected}`: {source}"
            )

    if total:
        print(
            f"CONDITIONAL_BRANCH_IDENTIFIERS_FAILED:{total}\n"
            "Each of these is in a branch this host does not compile, under a "
            "spelling the declaration no longer answers to, or names a platform "
            "the gate cannot decide."
        )
    else:
        print(f"CONDITIONAL_BRANCH_IDENTIFIERS_OK:{len(sources)}")
    # [[BPR-0866]]. What the verdict covers, printed where it is read.
    print(
        "CONDITIONAL_BRANCH_IDENTIFIERS_SCOPE:"
        f"host_defines={','.join(sorted(defined)) or 'none'} "
        f"policed_lines={policed_lines} policed_files={policed_files}\n"
        "Read: branches a platform macro removes on this host ("
        f"{', '.join(sorted(PLATFORM_MACROS))}), including those inside "
        "testing regions. Not read: a branch that only a build switch such as "
        "BPR_BUILD_TESTING removes, which the build that sets or clears the "
        "switch compiles; and a file CMake builds only on another platform, "
        "outside its own platform branches. Reported: a name whose spelling, "
        "folded for case and underscores, matches a different declared name, "
        "within a function that has an aXxx parameter or across a chain. A "
        "misspelling that matches no declared name is not reported."
    )
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())
