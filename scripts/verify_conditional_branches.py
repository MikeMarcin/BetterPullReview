#!/usr/bin/env python3
"""Find parameters referred to by a name they no longer have.

Parameters in this repository are named `aXxx`. A rename applied only to the
code the local compiler builds leaves `xxx` behind everywhere else, and
"everywhere else" is exactly the `#if defined(_WIN32)` and
`#elif defined(__linux__)` branches the preprocessor removes before the
compiler on this host ever sees them. Nothing fails, because nothing looks.

That is what happened before BPR-0155: three translation units did not compile
for Windows or Linux, and the local gate was green throughout.

A cross-compiler catches the Windows half. Nothing on a macOS host compiles the
Linux half, so this reads the source instead.

It looks only where the local compiler cannot: inside conditional branches that
are false for the host running it. Code the compiler already checks is left to
the compiler. Within those branches it reports `xxx` only where the enclosing
function has a parameter `aXxx` and the branch itself declares no `xxx` — the
exact shape a half-finished rename leaves behind.
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


def host_macros() -> set[str]:
    """The platform macros that are defined where this is running."""
    system = platform.system()
    if system == "Darwin":
        return {"__APPLE__", "__unix__"}
    if system == "Windows":
        return {"_WIN32"}
    if system == "Linux":
        return {"__linux__", "__unix__"}
    return set()


TOKEN = re.compile(r"\s*(\(|\)|&&|\|\||!|defined\s*\(\s*\w+\s*\)|defined\s+\w+|\S)")


def evaluate(aCondition: str, aDefined: set[str]) -> bool | None:
    """Evaluate a preprocessor condition, or None when it is not this shape.

    Only `defined(X)`, `!`, `&&`, `||`, and parentheses are understood.
    Anything else returns None and its branch is treated as compiled here,
    which is the safe direction: the compiler is then the check.

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
            return None
        token = tokens[index]
        if token == "!":
            index += 1
            value = primary()
            return None if value is None else not value
        if token == "(":
            index += 1
            value = disjunction()
            if index >= len(tokens) or tokens[index] != ")":
                return None
            index += 1
            return value
        if token.startswith("defined"):
            index += 1
            name = re.sub(r"^defined\(?|\)?$", "", token)
            return name in aDefined
        return None

    def conjunction() -> bool | None:
        nonlocal index
        value = primary()
        while value is not None and index < len(tokens) and tokens[index] == "&&":
            index += 1
            right = primary()
            value = None if right is None else (value and right)
        return value

    def disjunction() -> bool | None:
        nonlocal index
        value = conjunction()
        while value is not None and index < len(tokens) and tokens[index] == "||":
            index += 1
            right = conjunction()
            value = None if right is None else (value or right)
        return value

    result = disjunction()
    return result if index == len(tokens) else None


def inactive_lines(aText: str, aDefined: set[str]) -> set[int]:
    """Line numbers (1-based) inside a branch this host does not compile."""
    inactive: set[int] = set()
    stack: list[dict] = []
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
                stack.append({"taken": value is True, "decided": value is not None, "value": value})
            elif keyword == "elif" and stack:
                value = evaluate(rest, aDefined)
                frame = stack[-1]
                frame["value"] = value
                frame["decided"] = frame["decided"] and value is not None
            elif keyword == "else" and stack:
                frame = stack[-1]
                frame["value"] = None if not frame["decided"] else (not frame["taken"])
                if frame["decided"]:
                    frame["value"] = not frame["taken"]
            elif keyword == "endif" and stack:
                stack.pop()
            continue
        if any(frame["decided"] and frame["value"] is False for frame in stack):
            inactive.add(number)
    return inactive


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


def function_bodies(aText: str):
    """Yield (parameters, body_start, body_end) for every braced body."""
    for match in re.finditer(r"\)\s*(?:const\s+)?(?:noexcept\s*)?(?:->[^{;()]+)?\{", aText):
        closing = aText.rindex(")", 0, match.end())
        depth = 0
        opening = -1
        for index in range(closing, -1, -1):
            if aText[index] == ")":
                depth += 1
            elif aText[index] == "(":
                depth -= 1
                if depth == 0:
                    opening = index
                    break
        if opening < 0:
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


def findings_for(aPath: pathlib.Path, aDefined: set[str]) -> list[tuple[int, str, str, str]]:
    raw = aPath.read_text(encoding="utf-8")
    skipped = inactive_lines(raw, aDefined)
    if not skipped:
        return []
    text = blank_comments_and_literals(raw)
    lines = raw.split("\n")
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
        for parameter in parameters:
            bare = parameter[0].lower() + parameter[1:]
            if declared_in(region, bare):
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
                found.add((line, bare, f"a{parameter}", lines[line - 1].strip()))
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
    for path in sources:
        for line, bare, expected, source in findings_for(path, defined):
            total += 1
            print(
                f"{path.relative_to(root)}:{line}: `{bare}` is used where the "
                f"parameter is `{expected}`: {source}"
            )

    if total:
        print(
            f"CONDITIONAL_BRANCH_IDENTIFIERS_FAILED:{total}\n"
            "Each of these is in a branch this host does not compile. "
            "The name is the parameter's, without its prefix."
        )
        return 1

    print(f"CONDITIONAL_BRANCH_IDENTIFIERS_OK:{len(sources)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
