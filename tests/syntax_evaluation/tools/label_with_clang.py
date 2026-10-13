#!/usr/bin/env python3
"""Label the RFC-BPR-0081 corpus from clang's AST (libclang), offline.

Labels classes 1 (contract), 2 (first-party) and 3 (third-party) of a corpus
written by materialize_corpus.py, then derives fragment (class 5) labels
mechanically from their parents (rule F). Labels never come from the shipped
heuristic or from any candidate: only clang's AST, its detailed preprocessing
record, its diagnostics and its raw token stream are read.

Usage:
    python3 -I label_with_clang.py --repo-root ROOT --corpus DIR \
        --build-dir ROOT/build/dev --out ROOT/tests/syntax_evaluation/labels

The schema and rules are documented in tests/syntax_evaluation/corpus/README.md.
"""

from __future__ import annotations

import argparse
import bisect
import collections
import ctypes
import gzip
import io
import json
import multiprocessing
import os
import re
import shlex
import subprocess
import sys
import time
from pathlib import Path

LLVM_PREFIX = Path(os.environ.get("BPR_LLVM_PREFIX", "/opt/homebrew/opt/llvm"))
LIBCLANG = LLVM_PREFIX / "lib" / "libclang.dylib"
BINDINGS = sorted((LLVM_PREFIX / "lib").glob("python3*/site-packages"))

HEADER_SUFFIXES = {".h", ".hh", ".hpp", ".hxx", ".h++", ".inl", ".ipp", ".tcc"}
LABELLED_CLASSES = ("contract", "first-party", "third-party")
GZIP_THRESHOLD = 1_000_000  # bytes; larger label files are committed gzip-compressed (mtime 0)
ALL_CAPS = re.compile(r"^(?=.*[A-Z])[A-Z0-9_]{2,}$")
QUOTED = re.compile(r"'[^']*'")

PRIORITY = {"ns_decl": 0, "enumerator_decl": 1, "macro_keep": 2, "enum_use": 3, "conservative_negative": 4, "ns_qualifier": 5}

# ---------------------------------------------------------------- libclang


ci = None  # clang.cindex, loaded per process
raw = None  # a private ctypes handle for calls the bindings do not wrap


class CXSourceRangeList(ctypes.Structure):
    _fields_ = [("count", ctypes.c_uint), ("ranges", ctypes.c_void_p)]


def load_clang() -> None:
    global ci, raw
    if ci is not None:
        return
    for path in BINDINGS:
        if str(path) not in sys.path:
            sys.path.insert(0, str(path))
    import clang.cindex as cindex  # noqa: PLC0415

    cindex.Config.set_library_file(str(LIBCLANG))
    ci = cindex
    raw = ctypes.CDLL(str(LIBCLANG))
    raw.clang_getFileLocation.argtypes = [ci.SourceLocation, ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_uint), ctypes.POINTER(ctypes.c_uint), ctypes.POINTER(ctypes.c_uint)]
    raw.clang_getFileLocation.restype = None
    raw.clang_getFile.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    raw.clang_getFile.restype = ctypes.c_void_p
    raw.clang_getSkippedRanges.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    raw.clang_getSkippedRanges.restype = ctypes.POINTER(CXSourceRangeList)
    raw.clang_disposeSourceRangeList.argtypes = [ctypes.POINTER(CXSourceRangeList)]
    raw.clang_disposeSourceRangeList.restype = None
    raw.clang_getRangeStart.argtypes = [ci.SourceRange]
    raw.clang_getRangeStart.restype = ci.SourceLocation
    raw.clang_getRangeEnd.argtypes = [ci.SourceRange]
    raw.clang_getRangeEnd.restype = ci.SourceLocation


def clang_version() -> str:
    load_clang()
    version = ci.conf.lib.clang_getClangVersion()
    return version if isinstance(version, str) else ci._CXString.from_result(version)


def file_location(location) -> tuple[int | None, int]:
    """(file pointer, byte offset) of a location's file position.

    clang_getFileLocation maps a macro argument to where it is spelled and a
    macro body token to its expansion, so offsets name bytes of the file.
    """
    handle = ctypes.c_void_p()
    line = ctypes.c_uint()
    column = ctypes.c_uint()
    offset = ctypes.c_uint()
    raw.clang_getFileLocation(location, ctypes.byref(handle), ctypes.byref(line), ctypes.byref(column), ctypes.byref(offset))
    return handle.value, offset.value


def tu_pointer(tu) -> ctypes.c_void_p:
    return ctypes.cast(tu.obj, ctypes.c_void_p)


def skipped_ranges(tu, main_file) -> list[list[int]]:
    result = []
    ranges = raw.clang_getSkippedRanges(tu_pointer(tu), main_file)
    if not ranges:
        return result
    try:
        array = ctypes.cast(ranges.contents.ranges, ctypes.POINTER(ci.SourceRange))
        for index in range(ranges.contents.count):
            start = file_location(raw.clang_getRangeStart(array[index]))[1]
            end = file_location(raw.clang_getRangeEnd(array[index]))[1]
            result.append([start, end])
    finally:
        raw.clang_disposeSourceRangeList(ranges)
    return result


# ---------------------------------------------------------------- tokens


class Tokens:
    """The raw token stream of the main file, comments included."""

    def __init__(self, tu, path: str, size: int, data: bytes) -> None:
        handle = tu.get_file(path)
        extent = ci.SourceRange.from_locations(ci.SourceLocation.from_offset(tu, handle, 0), ci.SourceLocation.from_offset(tu, handle, size))
        self.kinds: list[str] = []
        self.texts: list[str] = []
        self.starts: list[int] = []
        self.ends: list[int] = []
        for token in tu.get_tokens(extent=extent):
            start = token.extent.start.offset
            end = token.extent.end.offset
            self.kinds.append(token.kind.name)
            self.texts.append(token.spelling)
            self.starts.append(start)
            self.ends.append(end)
        self.at = {start: index for index, start in enumerate(self.starts)}
        self.data = data

    def index_at(self, offset: int) -> int | None:
        return self.at.get(offset)

    def previous(self, index: int) -> int | None:
        index -= 1
        while index >= 0 and self.kinds[index] == "COMMENT":
            index -= 1
        return index if index >= 0 else None

    def following(self, index: int) -> int | None:
        index += 1
        while index < len(self.kinds) and self.kinds[index] == "COMMENT":
            index += 1
        return index if index < len(self.kinds) else None

    def text(self, index: int | None) -> str | None:
        return None if index is None else self.texts[index]


def is_string_literal(text: str) -> bool:
    return re.match(r'^(?:u8|u|U|L)?R?["\']', text) is not None


def directive_ranges(data: bytes, tokens: Tokens) -> list[list[int]]:
    """Whole preprocessor directive lines, with backslash-continued lines and
    any comment that carries the directive across a line ending."""
    comment_end = {}
    for index, kind in enumerate(tokens.kinds):
        if kind == "COMMENT":
            comment_end[tokens.starts[index]] = tokens.ends[index]
    comment_starts = sorted(comment_end)
    result = []
    line_start = 0
    for index, text in enumerate(tokens.texts):
        if text != "#" or tokens.kinds[index] != "PUNCTUATION":
            continue
        start = tokens.starts[index]
        line_start = data.rfind(b"\n", 0, start) + 1
        if data[line_start:start].strip(b" \t\f\v\r") != b"":
            continue
        position = start
        while True:
            newline = data.find(b"\n", position)
            if newline < 0:
                end = len(data)
                break
            at = bisect.bisect_right(comment_starts, newline) - 1
            if at >= 0 and comment_starts[at] < newline < comment_end[comment_starts[at]]:
                position = comment_end[comment_starts[at]]
                continue
            before = data[position:newline].rstrip(b" \t\r")
            if before.endswith(b"\\"):
                position = newline + 1
                continue
            end = newline
            break
        result.append([line_start, end])
    return result


def merge_ranges(ranges: list[list[int]]) -> list[list[int]]:
    merged: list[list[int]] = []
    for start, end in sorted(ranges):
        if end <= start:
            continue
        if merged and start <= merged[-1][1]:
            merged[-1][1] = max(merged[-1][1], end)
        else:
            merged.append([start, end])
    return merged


class RangeSet:
    def __init__(self, ranges: list[list[int]]) -> None:
        self.ranges = merge_ranges(ranges)
        self.starts = [start for start, _ in self.ranges]

    def covers(self, start: int, end: int) -> bool:
        at = bisect.bisect_right(self.starts, start) - 1
        return at >= 0 and self.ranges[at][1] > start and (end <= self.ranges[at][1] or start < self.ranges[at][1])

    def contains(self, offset: int) -> bool:
        at = bisect.bisect_right(self.starts, offset) - 1
        return at >= 0 and offset < self.ranges[at][1]


# ---------------------------------------------------------------- AST walk

SCOPE_KINDS = {
    "TRANSLATION_UNIT", "NAMESPACE", "COMPOUND_STMT", "FUNCTION_DECL", "CXX_METHOD", "CONSTRUCTOR", "DESTRUCTOR", "CONVERSION_FUNCTION",
    "FUNCTION_TEMPLATE", "LAMBDA_EXPR", "FOR_STMT", "CXX_FOR_RANGE_STMT", "IF_STMT", "WHILE_STMT", "SWITCH_STMT", "CXX_CATCH_STMT",
    "STRUCT_DECL", "CLASS_DECL", "UNION_DECL", "CLASS_TEMPLATE", "CLASS_TEMPLATE_PARTIAL_SPECIALIZATION",
}
RECORD_KINDS = {"STRUCT_DECL", "CLASS_DECL", "UNION_DECL", "CLASS_TEMPLATE", "CLASS_TEMPLATE_PARTIAL_SPECIALIZATION"}
KIND_GROUP = {
    "NAMESPACE": "namespace", "NAMESPACE_ALIAS": "namespace_alias",
    "STRUCT_DECL": "record", "CLASS_DECL": "record", "UNION_DECL": "record", "CLASS_TEMPLATE": "record",
    "ENUM_DECL": "enum", "TYPEDEF_DECL": "typedef", "TYPE_ALIAS_DECL": "typedef", "TYPE_ALIAS_TEMPLATE_DECL": "typedef",
    "FUNCTION_DECL": "function", "FUNCTION_TEMPLATE": "function", "CXX_METHOD": "function",
    "VAR_DECL": "variable", "FIELD_DECL": "variable", "ENUM_CONSTANT_DECL": "enumerator",
}
VARIABLE_KINDS = {"VAR_DECL", "PARM_DECL", "FIELD_DECL"}
QUALIFIER_REF_KINDS = {"NAMESPACE_REF", "TYPE_REF", "TEMPLATE_REF"}


def safe_kind(cursor) -> str | None:
    try:
        return cursor.kind.name
    except ValueError:
        return None


class Walker:
    def __init__(self, tu, main_file: int, tokens: Tokens) -> None:
        self.tu = tu
        self.main = main_file
        self.tokens = tokens
        self.namespaces: list[tuple[int, str, str]] = []  # name offset, name, usr
        self.namespace_defs: dict[str, list[int]] = collections.defaultdict(list)
        self.aliases: dict[str, int] = {}
        self.alias_decls: list[tuple[int, str]] = []
        self.enumerators: list[dict] = []
        self.scopes: list[tuple[int, int, str]] = []  # namespace/record extents
        self.enum_extents: list[tuple[int, int, str]] = []
        self.variables: dict[str, list[tuple[int, int, str]]] = collections.defaultdict(list)
        self.kind_groups: dict[tuple[str, str], set[str]] = collections.defaultdict(set)
        self.uses: list[dict] = []
        self.namespace_refs: dict[int, tuple[str, str, str]] = {}
        self.macros: list[tuple[int, str]] = []
        self.next_scope = 0
        self.usr_cache: dict[int, str] = {}

    def main_offset(self, cursor) -> int | None:
        handle, offset = file_location(cursor.location)
        if handle != self.main:
            return None
        return offset

    def spelled_at(self, offset: int, name: str) -> bool:
        index = self.tokens.index_at(offset)
        return index is not None and self.tokens.texts[index] == name

    def extent(self, cursor) -> tuple[int, int] | None:
        start_file, start = file_location(cursor.extent.start)
        end_file, end = file_location(cursor.extent.end)
        if start_file != self.main or end_file != self.main:
            return None
        return start, end

    def parent_usr(self, cursor) -> str:
        parent = cursor.semantic_parent
        if parent is None or safe_kind(parent) == "TRANSLATION_UNIT":
            return ""
        return parent.get_usr() or ""

    def run(self) -> None:
        for child in self.tu.cursor.get_children():
            kind = safe_kind(child)
            if kind is None:
                continue
            handle, offset = file_location(child.location)
            if handle != self.main:
                continue
            if kind == "MACRO_INSTANTIATION":
                self.macros.append((offset, child.spelling))
                continue
            if kind in ("MACRO_DEFINITION", "INCLUSION_DIRECTIVE"):
                continue
            self.visit(child, (0,))

    def visit(self, cursor, stack: tuple[int, ...]) -> None:
        kind = safe_kind(cursor)
        if kind is None:
            return
        name = cursor.spelling
        if kind in KIND_GROUP or kind in VARIABLE_KINDS:
            offset = self.main_offset(cursor)
            if offset is not None and name and self.spelled_at(offset, name):
                self.record_declaration(cursor, kind, name, offset, stack)
        elif kind == "DECL_REF_EXPR":
            referenced = cursor.referenced
            if referenced is not None and safe_kind(referenced) == "ENUM_CONSTANT_DECL":
                offset = self.main_offset(cursor)
                if offset is not None and self.spelled_at(offset, referenced.spelling):
                    self.record_use(cursor, referenced, offset, stack)
        elif kind == "NAMESPACE_REF":
            offset = self.main_offset(cursor)
            referenced = cursor.referenced
            if offset is not None and referenced is not None and offset not in self.namespace_refs:
                target_kind = safe_kind(referenced)
                target_name = referenced.spelling
                if self.spelled_at(offset, target_name) and target_kind in ("NAMESPACE", "NAMESPACE_ALIAS"):
                    self.namespace_refs[offset] = (target_name, target_kind, referenced.get_usr() or "")
        if kind in SCOPE_KINDS:
            self.next_scope += 1
            stack = stack + (self.next_scope,)
            if kind in RECORD_KINDS or kind == "NAMESPACE":
                span = self.extent(cursor)
                if span is not None:
                    self.scopes.append((span[0], span[1], cursor.get_usr() or ""))
        if kind == "ENUM_DECL":
            span = self.extent(cursor)
            if span is not None:
                self.enum_extents.append((span[0], span[1], cursor.get_usr() or ""))
        for child in cursor.get_children():
            self.visit(child, stack)

    def record_declaration(self, cursor, kind: str, name: str, offset: int, stack: tuple[int, ...]) -> None:
        if kind in VARIABLE_KINDS:
            self.variables[name].append((stack[-1], offset, kind))
        group = KIND_GROUP.get(kind)
        if group is not None and kind != "PARM_DECL":
            self.kind_groups[(self.parent_usr(cursor), name)].add(group)
        if kind == "NAMESPACE":
            usr = cursor.get_usr() or ""
            self.namespaces.append((offset, name, usr))
            self.namespace_defs[usr].append(offset)
        elif kind == "NAMESPACE_ALIAS":
            self.aliases[cursor.get_usr() or f"alias@{offset}"] = offset
            self.alias_decls.append((offset, name))
        elif kind == "ENUM_CONSTANT_DECL":
            enum = cursor.semantic_parent
            self.enumerators.append({"offset": offset, "name": name, "usr": cursor.get_usr() or "", "scoped": bool(enum is not None and enum.is_scoped_enum())})

    def record_use(self, cursor, referenced, offset: int, stack: tuple[int, ...]) -> None:
        qualifiers: dict[int, dict] = {}
        for child in cursor.get_children():
            kind = safe_kind(child)
            if kind not in QUALIFIER_REF_KINDS:
                continue
            child_offset = self.main_offset(child)
            target = child.referenced
            if child_offset is None or target is None:
                continue
            qualifiers[child_offset] = {"kind": safe_kind(target), "usr": target.get_usr() or "", "name": target.spelling, "parent": self.parent_usr(target)}
        handle, declaration = file_location(referenced.location)
        in_blob = handle == self.main and self.spelled_at(declaration, referenced.spelling)
        enum = referenced.semantic_parent
        self.uses.append({
            "offset": offset,
            "name": referenced.spelling,
            "usr": referenced.get_usr() or "",
            "declaration": declaration if in_blob else None,
            "enum_usr": enum.get_usr() or "" if enum is not None else "",
            "enum_name": enum.spelling if enum is not None else "",
            "enum_parent": self.parent_usr(enum) if enum is not None else "",
            "scoped": bool(enum is not None and enum.is_scoped_enum()),
            "qualifiers": qualifiers,
            "stack": stack,
        })


# ---------------------------------------------------------------- labelling


class FileLabeller:
    def __init__(self, relative: str, path: str, data: bytes, arguments: list[str]) -> None:
        self.relative = relative
        self.path = path
        self.data = data
        self.arguments = arguments
        self.labels: dict[int, dict] = {}
        self.newlines = [index for index, byte in enumerate(data) if byte == 10]

    def line_of(self, offset: int) -> int:
        return bisect.bisect_left(self.newlines, offset)

    def add(self, offset: int, name: str, category: str, subcategory: str | None, expectation: str, declaration: int | None, rule: str) -> None:
        if self.protected.contains(offset) or self.skipped.contains(offset):
            return
        existing = self.labels.get(offset)
        if existing is not None and PRIORITY.get(existing["c"], 9) <= PRIORITY.get(category, 9):
            return
        self.labels[offset] = {"f": self.relative, "o": offset, "n": len(name.encode("utf-8")), "t": name, "c": category, "s": subcategory, "e": expectation, "d": declaration, "r": rule, "m": "clang"}

    def run(self) -> dict:
        load_clang()
        index = ci.Index.create()
        options = ci.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD | 0x200  # KeepGoing
        started = time.monotonic()
        try:
            tu = index.parse(self.path, args=self.arguments, options=options)
        except ci.TranslationUnitLoadError as error:
            return {"f": self.relative, "status": "none", "reason": str(error), "labels": [], "protected": [], "aux": {}}
        main_file = raw.clang_getFile(tu_pointer(tu), self.path.encode())
        errors = 0
        errors_in_main = 0
        causes = collections.Counter()
        undeclared: list[tuple[int, str]] = []
        conflicts: list[tuple[int, str, str]] = []
        for diagnostic in tu.diagnostics:
            if diagnostic.severity < 3:
                continue
            errors += 1
            handle, offset = file_location(diagnostic.location)
            message = diagnostic.spelling
            causes[QUOTED.sub("'…'", message)] += 1
            if handle != main_file:
                continue
            errors_in_main += 1
            match = re.match(r"^(?:use of undeclared identifier|unknown type name) '([^']+)'", message)
            if match:
                undeclared.append((offset, match.group(1)))
            match = re.match(r"^(?:use of '([^']+)' with tag type that does not match previous declaration|redefinition of '([^']+)' as different kind of symbol)", message)
            if match:
                conflicts.append((offset, match.group(1) or match.group(2), "kind"))
            match = re.match(r"^redefinition of enumerator '([^']+)'", message)
            if match:
                conflicts.append((offset, match.group(1), "enumerator"))
        self.tokens = Tokens(tu, self.path, len(self.data), self.data)
        protected = []
        for position, kind in enumerate(self.tokens.kinds):
            if kind == "COMMENT" or (kind == "LITERAL" and is_string_literal(self.tokens.texts[position])):
                protected.append([self.tokens.starts[position], self.tokens.ends[position]])
        protected.extend(directive_ranges(self.data, self.tokens))
        self.protected = RangeSet(protected)
        skipped = skipped_ranges(tu, main_file)
        self.skipped = RangeSet(skipped)
        walker = Walker(tu, main_file, self.tokens)
        walker.run()
        aux = self.label(walker, undeclared, conflicts)
        labels = [self.labels[offset] for offset in sorted(self.labels)]
        status = "full" if errors == 0 else "partial"
        return {
            "f": self.relative,
            "status": status,
            "errors": errors,
            "errors_in_main": errors_in_main,
            "causes": dict(causes.most_common(5)),
            "seconds": round(time.monotonic() - started, 3),
            "labels": labels,
            "protected": self.protected.ranges,
            "unparsed": self.skipped.ranges,
            "aux": aux,
        }

    # --- individual categories

    def label(self, walker: Walker, undeclared, conflicts) -> dict:
        tokens = self.tokens
        aux: dict[str, dict] = {"ns_defs": {}}
        using_offsets = self.label_using_directives()
        for offset, name, usr in walker.namespaces:
            self.label_namespace(offset, name)
        for offset, name in walker.alias_decls:
            keyword = self.keyword_before(offset, "namespace")
            self.add(offset, name, "ns_decl", "alias", "namespace", keyword, "NS")
        for enumerator in walker.enumerators:
            self.label_enumerator(enumerator)
        for offset, name in walker.macros:
            if ALL_CAPS.match(name) and walker.spelled_at(offset, name):
                self.add(offset, name, "macro_keep", None, "macro", None, "M1")
        for offset, name in undeclared:
            index = tokens.index_at(offset)
            if index is None or tokens.texts[index] != name or not ALL_CAPS.match(name):
                continue
            if tokens.text(tokens.previous(index)) in ("::", ".", "->") or tokens.text(tokens.following(index)) == "::":
                continue
            self.add(offset, name, "macro_keep", None, "macro", None, "M2")
        self.label_uses(walker, conflicts)
        aux["ns_defs"] = self.label_namespace_refs(walker, using_offsets)
        self.label_r7()
        return aux

    def keyword_before(self, offset: int, keyword: str) -> int | None:
        index = self.tokens.index_at(offset)
        steps = 0
        while index is not None and steps < 64:
            index = self.tokens.previous(index)
            if index is None:
                return None
            if self.tokens.kinds[index] == "KEYWORD" and self.tokens.texts[index] == keyword:
                return self.tokens.starts[index]
            if self.tokens.texts[index] in ("{", "}", ";"):
                return None
            steps += 1
        return None

    def label_namespace(self, offset: int, name: str) -> None:
        tokens = self.tokens
        keyword = self.keyword_before(offset, "namespace")
        if keyword is None:
            self.add(offset, name, "ns_decl", "simple", "namespace", offset, "NS")
            return
        index = tokens.index_at(keyword)
        names: list[int] = []
        depth = 0
        while index is not None:
            index = tokens.following(index)
            if index is None:
                break
            text = tokens.texts[index]
            if text == "[":
                depth += 1
            elif text == "]":
                depth -= 1
            elif depth == 0 and text in ("{", "=", ";"):
                break
            elif depth == 0 and tokens.kinds[index] == "IDENTIFIER":
                names.append(tokens.starts[index])
        if not names:
            names = [offset]
        if self.line_of(keyword) != self.line_of(names[-1]):
            subcategory = "multiline"
        elif len(names) > 1:
            subcategory = "nested"
        else:
            subcategory = "simple"
        self.add(offset, name, "ns_decl", subcategory, "namespace", keyword, "NS")

    def label_using_directives(self) -> set[int]:
        tokens = self.tokens
        offsets: set[int] = set()
        for index, text in enumerate(tokens.texts):
            if text != "using" or tokens.kinds[index] != "KEYWORD":
                continue
            following = tokens.following(index)
            if following is None or tokens.texts[following] != "namespace":
                continue
            keyword = tokens.starts[following]
            cursor = following
            while True:
                cursor = tokens.following(cursor)
                if cursor is None:
                    break
                text = tokens.texts[cursor]
                if text == "::":
                    continue
                if tokens.kinds[cursor] == "IDENTIFIER":
                    offsets.add(tokens.starts[cursor])
                    self.add(tokens.starts[cursor], text, "ns_decl", "using", "namespace", keyword, "NS")
                    continue
                break
        return offsets

    def label_enumerator(self, enumerator: dict) -> None:
        tokens = self.tokens
        index = tokens.index_at(enumerator["offset"])
        brace = None
        depth = 0
        cursor = index
        while cursor is not None:
            cursor = tokens.previous(cursor)
            if cursor is None:
                break
            text = tokens.texts[cursor]
            if text in ("}", ")", "]"):
                depth += 1
            elif text in ("(", "["):
                depth -= 1
            elif text == "{":
                if depth == 0:
                    brace = tokens.starts[cursor]
                    break
                depth -= 1
        attribute = False
        nested = False
        depth = 0
        seen_equals = False
        cursor = index
        previous_text = None
        while cursor is not None:
            cursor = tokens.following(cursor)
            if cursor is None:
                break
            text = tokens.texts[cursor]
            if depth == 0 and text in (",", "}"):
                break
            if text == "[" and previous_text == "[" and not seen_equals:
                attribute = True
            if depth == 0 and text == "=":
                seen_equals = True
            elif seen_equals and text in ("(", "[", "{", "<"):
                nested = True
            if text in ("(", "[", "{"):
                depth += 1
            elif text in (")", "]", "}"):
                depth -= 1
            previous_text = text
        if attribute:
            subcategory = "attribute"
        elif nested:
            subcategory = "nested_initializer"
        else:
            subcategory = "scoped" if enumerator["scoped"] else "unscoped"
        self.add(enumerator["offset"], enumerator["name"], "enumerator_decl", subcategory, "enum_member", brace, "EN")

    def qualifier_chain(self, offset: int) -> tuple[list[int], bool]:
        """Written qualifier name offsets before `::Y` (head first), and whether
        the chain holds a template argument list, decltype, `template`, or
        follows `.` or `->` (R6)."""
        tokens = self.tokens
        index = tokens.index_at(offset)
        chain: list[int] = []
        r6 = False
        cursor = tokens.previous(index) if index is not None else None
        if cursor is None or tokens.texts[cursor] != "::":
            return chain, r6
        while cursor is not None and tokens.texts[cursor] == "::":
            cursor = tokens.previous(cursor)
            if cursor is None:
                break
            text = tokens.texts[cursor]
            if tokens.kinds[cursor] == "IDENTIFIER":
                chain.insert(0, tokens.starts[cursor])
                cursor = tokens.previous(cursor)
                if cursor is not None and tokens.texts[cursor] == "template":
                    r6 = True
                    cursor = tokens.previous(cursor)
            elif text in (">", ">>", ")"):
                r6 = True
                break
            else:
                break
        if cursor is not None and tokens.texts[cursor] in (".", "->", "decltype"):
            r6 = True
        return chain, r6

    def scope_at(self, offset: int, scopes: list[tuple[int, int, str]]) -> str:
        """The innermost recorded scope holding `offset`, not counting a
        declaration whose own keyword stands just before it."""
        own = None
        index = self.tokens.index_at(offset)
        if index is not None:
            previous = self.tokens.previous(index)
            if previous is not None and self.tokens.texts[previous] in ("struct", "class", "union", "enum", "namespace"):
                own = self.tokens.starts[previous]
                before = self.tokens.previous(previous)
                if before is not None and self.tokens.texts[before] == "enum":
                    own = self.tokens.starts[before]
        best = ""
        width = None
        for start, end, usr in scopes:
            if start in (own, offset):
                continue
            if start <= offset < end and (width is None or end - start < width):
                best = usr
                width = end - start
        return best

    def label_uses(self, walker: Walker, conflicts) -> None:
        kind_conflicts: set[tuple[str, str]] = set()
        enumerator_conflicts: set[tuple[str, str]] = set()
        for offset, name, which in conflicts:
            if which == "kind":
                kind_conflicts.add((self.scope_at(offset, walker.scopes), name))
            else:
                enumerator_conflicts.add((self.scope_at(offset, walker.enum_extents), name))

        def conflicting(parent: str, name: str) -> bool:
            return len(walker.kind_groups.get((parent, name), ())) > 1 or (parent, name) in kind_conflicts

        for use in walker.uses:
            offset = use["offset"]
            name = use["name"]
            chain, r6 = self.qualifier_chain(offset)
            if not chain and not r6:
                continue  # unqualified: no label
            qualifiers = [use["qualifiers"].get(position) for position in chain]
            declaration = use["declaration"]
            rule = None
            if declaration is None:
                rule = "R2"
            elif any(q is not None and q["kind"] in ("NAMESPACE_ALIAS", "TYPEDEF_DECL", "TYPE_ALIAS_DECL") for q in qualifiers):
                rule = "R4"
            elif r6:
                rule = "R6"
            elif conflicting(use["enum_parent"], use["enum_name"]) or (use["enum_usr"], name) in enumerator_conflicts or any(q is not None and conflicting(q["parent"], q["name"]) for q in qualifiers):
                rule = "R5"
            else:
                head_offset = chain[0]
                head_name = self.tokens.texts[self.tokens.index_at(head_offset)]
                for scope, declared, kind in walker.variables.get(head_name, ()):
                    if scope in use["stack"] and (kind == "FIELD_DECL" or declared < offset):
                        rule = "R3"
                        break
            if rule is None:
                last = qualifiers[-1] if qualifiers else None
                if last is not None and (last["usr"] == use["enum_usr"] or (not use["scoped"] and last["usr"] == use["enum_parent"])):
                    self.add(offset, name, "enum_use", None, "enum_member", declaration, "EU")
                    continue
                rule = "R8"
            self.add(offset, name, "conservative_negative", None, "neutral", declaration, rule)

    def label_namespace_refs(self, walker: Walker, using_offsets: set[int]) -> dict:
        tokens = self.tokens
        alias_refs = {offset for offset, (_, kind, _) in walker.namespace_refs.items() if kind == "NAMESPACE_ALIAS"}
        ns_defs: dict[str, list[int]] = {}
        for offset in sorted(walker.namespace_refs):
            name, kind, usr = walker.namespace_refs[offset]
            if offset in using_offsets:
                continue
            if kind == "NAMESPACE":
                definitions = sorted(walker.namespace_defs.get(usr, ()))
            else:
                definitions = [walker.aliases[usr]] if usr in walker.aliases else []
            if not definitions:
                continue
            after_alias = False
            cursor = tokens.index_at(offset)
            while cursor is not None:
                cursor = tokens.previous(cursor)
                if cursor is None or tokens.texts[cursor] != "::":
                    break
                cursor = tokens.previous(cursor)
                if cursor is None or tokens.kinds[cursor] != "IDENTIFIER":
                    break
                if tokens.starts[cursor] in alias_refs:
                    after_alias = True
                    break
            if after_alias:
                continue
            before = [position for position in definitions if position <= offset]
            declaration = before[-1] if before else definitions[0]
            self.add(offset, name, "ns_qualifier", None, "namespace", declaration, "NQ")
            ns_defs[str(offset)] = definitions
        return ns_defs

    def label_r7(self) -> None:
        tokens = self.tokens
        for index, kind in enumerate(tokens.kinds):
            if kind != "IDENTIFIER" or not ALL_CAPS.match(tokens.texts[index]):
                continue
            offset = tokens.starts[index]
            if offset in self.labels:
                continue
            if tokens.text(tokens.previous(index)) != "::":
                continue
            if tokens.text(tokens.following(index)) in ("(", "::"):
                continue
            self.add(offset, tokens.texts[index], "conservative_negative", None, "neutral", None, "R7")


def label_one(job: dict) -> dict:
    try:
        result = FileLabeller(job["relative"], job["path"], Path(job["path"]).read_bytes(), job["arguments"]).run()
    except Exception as error:  # a file that breaks the labeller is reported, not fatal
        result = {"f": job["relative"], "status": "none", "reason": f"{type(error).__name__}: {error}", "labels": [], "protected": [], "aux": {}}
    result["source"] = job["source"]
    result["flags"] = job["flags"]
    return result


# ---------------------------------------------------------------- flags


def sdk_path() -> str:
    completed = subprocess.run(["xcrun", "--show-sdk-path"], capture_output=True, text=True, check=False)
    return completed.stdout.strip()


RESOURCE_DIRS = sorted((LLVM_PREFIX / "lib" / "clang").glob("*/include"))
COMMON = ["-ferror-limit=0", "-Wno-everything"] + (["-resource-dir", str(RESOURCE_DIRS[-1].parent)] if RESOURCE_DIRS else [])


def suffix(path: str) -> str:
    name = path.rsplit("/", 1)[-1]
    return "." + name.rsplit(".", 1)[-1].lower() if "." in name else ""


def language(path: str) -> list[str]:
    return ["-x", "c++-header"] if suffix(path) in HEADER_SUFFIXES else ["-x", "c++"]


def clean_command(command: str, repo_src: str, corpus_src: str) -> list[str]:
    words = shlex.split(command)[1:]
    result: list[str] = []
    skip_next = False
    for position, word in enumerate(words):
        if skip_next:
            skip_next = False
            continue
        if word in ("-o", "-arch", "-MF", "-MT", "-MQ"):
            skip_next = True
            continue
        if word in ("-c",) or word.startswith(("-W", "-g", "-fstack-protector", "-fno-omit", "-fobjc", "-M")):
            continue
        if word in ("-x",):
            skip_next = True
            continue
        if not word.startswith("-") and position > 0 and words[position - 1] not in ("-isystem", "-iframework", "-I", "-include", "-isysroot", "-F", "-D"):
            continue  # the input file
        if word.startswith(repo_src):
            word = corpus_src + word[len(repo_src):]
        elif word.startswith("-I" + repo_src):
            word = "-I" + corpus_src + word[2 + len(repo_src):]
        result.append(word)
    return result


def first_party_jobs(index: list[dict], corpus: Path, repo: Path, build: Path, sysroot: str) -> list[dict]:
    commands = json.loads((build / "compile_commands.json").read_text(encoding="utf-8"))
    repo_src = str(repo / "src")
    corpus_src = str(corpus / "first-party" / "src")
    by_file: dict[str, str] = {}
    for entry in commands:
        file = entry["file"]
        if file.startswith(repo_src + "/") and (file.endswith((".cpp", ".cc", ".cxx"))):
            by_file[file[len(repo_src) + 1:]] = entry["command"]
    tu_paths = sorted(by_file)
    jobs = []
    for entry in index:
        if entry["class"] != "first-party" or entry["source"] != "first-party":
            continue  # concatenations carry no labels
        relative = entry["path"][len("first-party/src/"):]
        if relative in by_file:
            command = by_file[relative]
            flags = "compile_commands"
        else:
            directory = relative.rsplit("/", 1)[0] if "/" in relative else ""
            borrowed = None
            while borrowed is None:
                prefix = directory + "/" if directory else ""
                candidates = [path for path in tu_paths if path.startswith(prefix)]
                if candidates:
                    borrowed = candidates[0]
                    break
                if not directory:
                    break
                directory = directory.rsplit("/", 1)[0] if "/" in directory else ""
            if borrowed is None:
                command = "c++ -std=c++23"
                flags = "default"
            else:
                command = by_file[borrowed]
                flags = f"borrowed:src/{borrowed}"
        arguments = language(relative) + clean_command(command, repo_src, corpus_src)
        if "-isysroot" not in arguments:
            arguments += ["-isysroot", sysroot]
        arguments += COMMON
        jobs.append({"relative": entry["path"], "path": str(corpus / entry["path"]), "arguments": arguments, "source": "first-party", "flags": flags})
    return jobs


def qt_paths(build: Path) -> tuple[str | None, list[str]]:
    commands = json.loads((build / "compile_commands.json").read_text(encoding="utf-8"))
    for entry in commands:
        words = shlex.split(entry["command"])
        if "-iframework" in words:
            framework_root = words[words.index("-iframework") + 1]
            extra = [words[position + 1] for position, word in enumerate(words) if word == "-isystem" and "bprqt" in words[position + 1] and "framework" not in words[position + 1]]
            return framework_root, extra
    return None, []


def third_party_jobs(index: list[dict], corpus: Path, manifest: dict, build: Path, sysroot: str) -> list[dict]:
    third = next(entry for entry in manifest["classes"] if entry["name"] == "third-party")
    specs = {spec["label_source"]: spec for spec in third["archives"] + third["source_trees"]}
    framework_root, qt_extra = qt_paths(build)
    jobs = []
    for entry in index:
        if entry["class"] != "third-party":
            continue
        source = entry["source"]
        spec = specs[source]
        root = corpus / "third-party" / source
        includes = [str(root / relative) for relative in spec["include_roots"]]
        for need in spec.get("needs", []):
            includes += [str(corpus / "third-party" / need / relative) for relative in specs[need]["include_roots"]]
        includes += [str(corpus / "support" / name / relative) for name in [source, *spec.get("needs", [])] for relative in specs[name]["include_roots"]]
        arguments = language(entry["path"]) + ["-std=c++23"]
        file_dir = str((corpus / entry["path"]).parent)
        arguments += ["-I" + file_dir] + ["-I" + path for path in includes]
        if source == "ksyntaxhighlighting":
            generated = build / "_bpr" / "ksyntaxhighlighting" / "6.28.1"
            autogen = sorted((generated / "syntax-highlighting-build").glob("**/*_autogen/include"))
            for path in [generated / "syntax-highlighting-build" / "src" / "lib", generated / "prefix" / "include" / "KF6" / "KSyntaxHighlighting", *autogen]:
                if path.is_dir():
                    arguments.append("-I" + str(path))
            if framework_root is not None:
                arguments += ["-iframework", framework_root]
                for framework in spec.get("qt_frameworks", []):
                    headers = Path(framework_root) / f"{framework}.framework" / "Headers"
                    if headers.is_dir():
                        arguments += ["-isystem", str(headers)]
                for path in qt_extra:
                    arguments += ["-isystem", path]
        arguments += ["-isysroot", sysroot] + COMMON
        jobs.append({"relative": entry["path"], "path": str(corpus / entry["path"]), "arguments": arguments, "source": source, "flags": "third-party"})
    return jobs


def contract_jobs(index: list[dict], corpus: Path, sysroot: str) -> list[dict]:
    jobs = []
    for entry in index:
        if entry["class"] != "contract":
            continue
        parts = entry["path"].split("/")
        root = corpus / "/".join(parts[:3]) if len(parts) > 3 else corpus / "/".join(parts[:2])
        file_dir = (corpus / entry["path"]).parent
        arguments = language(entry["path"]) + ["-std=c++23", "-I" + str(file_dir), "-I" + str(root), "-isysroot", sysroot] + COMMON
        jobs.append({"relative": entry["path"], "path": str(corpus / entry["path"]), "arguments": arguments, "source": "contract", "flags": "contract"})
    return jobs


# ---------------------------------------------------------------- fragments (rule F)


def parent_line_spans(data: bytes) -> list[tuple[int, int, int]]:
    """(start, content end, next start) per line as display_lines counts them."""
    spans = []
    start = 0
    while True:
        newline = data.find(b"\n", start)
        if newline < 0:
            if start < len(data):
                spans.append((start, len(data), len(data)))
            break
        end = newline - 1 if newline > start and data[newline - 1] == 13 else newline
        spans.append((start, end, newline + 1))
        start = newline + 1
    return spans


def column_map(line: bytes, tab_width: int | None) -> list[int]:
    """Fragment byte column for each parent byte column (length len(line)+1)."""
    if tab_width is None:
        return list(range(len(line) + 1))
    text = line.decode("utf-8")
    mapping: list[int] = []
    out_bytes = 0
    column = 0
    for character in text:
        encoded = character.encode("utf-8")
        for _ in encoded:
            mapping.append(out_bytes)
        if character == "\t":
            pad = tab_width - column % tab_width
            out_bytes += pad
            column += pad
        else:
            out_bytes += len(encoded)
            column += 2 if ord(character) > 0xFFFF else 1
    mapping.append(out_bytes)
    return mapping


def derive_fragment(entry: dict, parent_data: bytes, parent_result: dict, fragment_data: bytes) -> tuple[list[dict], dict]:
    spans = parent_line_spans(parent_data)
    tab = entry.get("tab_width")
    is_hunk = tab is None
    shown: list[tuple[int, int, int, int, list[int]]] = []  # parent start, content end, next, fragment start, map
    position = 0
    placeholders: list[tuple[int, int | None, int | None]] = []
    for row, line_number in enumerate(entry["lines"]):
        if line_number is None:
            placeholders.append((position, shown[-1][2] - 1 if shown else None, None))
            position += 1
            continue
        start, end, following = spans[line_number - 1]
        if is_hunk:
            content = parent_data[start:following]
            mapping = list(range(following - start + 1))
            shown.append((start, following, following, position, mapping))
            position += following - start
        else:
            content = parent_data[start:end]
            mapping = column_map(content, tab)
            shown.append((start, end, following, position, mapping))
            position += mapping[-1] + 1
        if placeholders and placeholders[-1][2] is None:
            placeholders[-1] = (placeholders[-1][0], placeholders[-1][1], start)
    if position != len(fragment_data):
        raise ValueError(f"fragment {entry['path']} reconstructs to {position} bytes, holds {len(fragment_data)}")
    starts = [item[0] for item in shown]

    def remap(offset: int, length: int = 0) -> int | None:
        at = bisect.bisect_right(starts, offset) - 1
        if at < 0:
            return None
        start, end, following, fragment_start, mapping = shown[at]
        if offset + length > end and not (is_hunk and offset + length <= following):
            return None
        return fragment_start + mapping[offset - start]

    def inside(offset: int | None) -> bool:
        return offset is not None and remap(offset) is not None

    labels = []
    drops = collections.Counter()
    ns_defs = parent_result.get("aux", {}).get("ns_defs", {})
    enum_braces = {label["o"]: label["d"] for label in parent_result["labels"] if label["c"] == "enumerator_decl"}
    qualifier_labels = {label["o"]: label for label in parent_result["labels"] if label["c"] == "ns_qualifier"}

    def written_qualifiers(offset: int) -> list[tuple[int, bytes]]:
        """The names written before `::` ahead of `offset`, nearest first."""
        found = []
        at = offset
        while True:
            back = at
            while back > 0 and parent_data[back - 1:back] in (b" ", b"\t", b"\r", b"\n"):
                back -= 1
            if parent_data[back - 2:back] != b"::":
                return found
            back -= 2
            while back > 0 and parent_data[back - 1:back] in (b" ", b"\t", b"\r", b"\n"):
                back -= 1
            end = back
            while back > 0 and (parent_data[back - 1:back].isalnum() or parent_data[back - 1:back] == b"_"):
                back -= 1
            if back == end:
                return found
            found.append((back, parent_data[back:end]))
            at = back

    def chain_visible(label: dict) -> bool:
        """[[BPR-1440]] rule F: the enum's header and every written qualifier
        must be established inside the fragment, not only the enumerator."""
        if not inside(enum_braces.get(label["d"])):
            return False
        for qualifier_offset, name in written_qualifiers(label["o"]):
            if qualifier_offset in qualifier_labels:
                definitions = ns_defs.get(str(qualifier_offset), [qualifier_labels[qualifier_offset]["d"]])
                if not any(inside(position) for position in definitions):
                    return False
            elif not re.search(rb"\b(?:class|struct|union|enum(?:\s+class|\s+struct)?)\s+" + re.escape(name) + rb"\b", fragment_data):
                return False
        return True

    for label in parent_result["labels"]:
        mapped = remap(label["o"], label["n"])
        if mapped is None:
            continue
        derived = dict(label)
        derived["f"] = entry["path"]
        derived["o"] = mapped
        category = label["c"]
        declaration = label["d"]
        if category == "enum_use":
            if not inside(declaration) or not chain_visible(label):
                derived.update({"c": "conservative_negative", "e": "neutral", "r": "F-outside", "d": None})
                drops["enum_use->F-outside"] += 1
            else:
                derived["d"] = remap(declaration)
        elif category == "ns_qualifier":
            visible = [position for position in ns_defs.get(str(label["o"]), [declaration]) if inside(position)]
            if not visible:
                drops["ns_qualifier"] += 1
                continue
            before = [remap(position) for position in visible if position <= label["o"]]
            derived["d"] = before[-1] if before else remap(visible[0])
        elif category in ("ns_decl", "enumerator_decl"):
            if not inside(declaration):
                drops[category] += 1
                continue
            derived["d"] = remap(declaration)
        else:
            derived["d"] = remap(declaration) if inside(declaration) else None
        labels.append(derived)
    protected = RangeSet(parent_result["protected"])
    ranges: list[list[int]] = []
    for start, end, following, fragment_start, mapping in shown:
        limit = following if is_hunk else end
        for range_start, range_end in protected.ranges:
            if range_end <= start or range_start >= limit:
                continue
            clipped_start = max(range_start, start)
            clipped_end = min(range_end, limit)
            ranges.append([fragment_start + mapping[clipped_start - start], fragment_start + mapping[clipped_end - start]])
        if not is_hunk and protected.contains(following - 1) and following - 1 >= end:
            line_end = fragment_start + mapping[-1]
            ranges.append([line_end, line_end + 1])
    for fragment_offset, before, after in placeholders:
        if before is not None and after is not None and protected.contains(before) and protected.contains(after):
            ranges.append([fragment_offset, fragment_offset + 1])
    labels.sort(key=lambda item: item["o"])
    return labels, {"protected": merge_ranges(ranges), "drops": dict(drops)}


# ---------------------------------------------------------------- output


def dumps(record: dict) -> str:
    return json.dumps(record, sort_keys=True, separators=(",", ":"), ensure_ascii=False)


def write_source(out: Path, source: str, lines: list[str]) -> Path:
    text = ("\n".join(lines) + "\n").encode("utf-8") if lines else b""
    plain = out / f"{source}.jsonl"
    packed = out / f"{source}.jsonl.gz"
    for stale in (plain, packed):
        if stale.exists():
            stale.unlink()
    if len(text) > GZIP_THRESHOLD:
        buffer = io.BytesIO()
        with gzip.GzipFile(filename="", mode="wb", fileobj=buffer, mtime=0, compresslevel=9) as stream:
            stream.write(text)
        packed.write_bytes(buffer.getvalue())
        return packed
    plain.write_bytes(text)
    return plain


def file_records(result: dict) -> list[str]:
    record = {"f": result["f"], "c": "protected", "ranges": result.get("protected", [])}
    lines = [dumps(record)]
    lines.extend(dumps(label) for label in result.get("labels", []))
    return lines


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--repo-root", required=True, type=Path)
    parser.add_argument("--corpus", required=True, type=Path)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--jobs", type=int, default=8)
    parser.add_argument("--only", action="append", default=[], help="label only these corpus paths (debugging; writes nothing)")
    parser.add_argument("--fragments-only", action="store_true",
                        help="relabel only fragment parents and rewrite fragments.jsonl and coverage.json's fragment fields")
    arguments = parser.parse_args()
    repo = arguments.repo_root.resolve()
    corpus = arguments.corpus.resolve()
    build = arguments.build_dir.resolve()
    jobs_count = max(1, min(8, arguments.jobs))
    sys.setrecursionlimit(20000)
    manifest = json.loads((repo / "tests/syntax_evaluation/corpus/manifest.json").read_text(encoding="utf-8"))
    index = json.loads((corpus / "index.json").read_text(encoding="utf-8"))["files"]
    sysroot = sdk_path()
    jobs = contract_jobs(index, corpus, sysroot) + first_party_jobs(index, corpus, repo, build, sysroot) + third_party_jobs(index, corpus, manifest, build, sysroot)
    if arguments.only:
        wanted = set(arguments.only)
        for job in [job for job in jobs if job["relative"] in wanted]:
            result = label_one(job)
            print(json.dumps({key: value for key, value in result.items() if key not in ("protected",)}, indent=1, ensure_ascii=False))
        return 0
    if arguments.fragments_only:
        parents = {entry["parent"] for entry in index if entry["class"] == "fragments"}
        jobs = [job for job in jobs if job["relative"] in parents]
    jobs.sort(key=lambda job: -os.path.getsize(job["path"]))
    started = time.monotonic()
    results: dict[str, dict] = {}
    with multiprocessing.get_context("spawn").Pool(jobs_count) as pool:
        for count, result in enumerate(pool.imap_unordered(label_one, jobs, chunksize=1), 1):
            results[result["f"]] = result
            if count % 200 == 0:
                print(f"labelled {count}/{len(jobs)} files in {time.monotonic() - started:.0f}s", file=sys.stderr, flush=True)
    arguments.out.mkdir(parents=True, exist_ok=True)

    by_source: dict[str, list[str]] = collections.defaultdict(list)
    for relative in sorted(results):
        by_source[results[relative]["source"]].extend(file_records(results[relative]))

    fragment_lines: list[str] = []
    fragment_drops = collections.Counter()
    fragment_files = 0
    for entry in index:
        if entry["class"] != "fragments":
            continue
        parent = results.get(entry["parent"])
        if parent is None or parent["status"] == "none":
            continue
        labels, extra = derive_fragment(entry, (corpus / entry["parent"]).read_bytes(), parent, (corpus / entry["path"]).read_bytes())
        fragment_drops.update(extra["drops"])
        fragment_lines.append(dumps({"f": entry["path"], "c": "protected", "ranges": extra["protected"]}))
        fragment_lines.extend(dumps(label) for label in labels)
        fragment_files += 1

    if arguments.fragments_only:
        name = write_source(arguments.out, "fragments", fragment_lines).name
        coverage_path = arguments.out / "coverage.json"
        coverage = json.loads(coverage_path.read_text(encoding="utf-8"))
        coverage["fragment_drops"] = dict(sorted(fragment_drops.items()))
        coverage["fragment_files"] = fragment_files
        coverage["outputs"]["fragments"] = name
        coverage_path.write_text(json.dumps(coverage, sort_keys=True, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")
        print(f"relabelled {len(results)} parents and {fragment_files} fragments in {time.monotonic() - started:.0f}s", file=sys.stderr)
        return 0

    written = {}
    for source in sorted(by_source):
        written[source] = write_source(arguments.out, source, by_source[source]).name
    written["fragments"] = write_source(arguments.out, "fragments", fragment_lines).name

    coverage = {"clang": clang_version(), "sysroot": sysroot, "files": [], "summary": {}, "fragment_drops": dict(sorted(fragment_drops.items())), "fragment_files": fragment_files, "outputs": written}
    summary: dict[str, collections.Counter] = collections.defaultdict(collections.Counter)
    causes = collections.Counter()
    for relative in sorted(results):
        result = results[relative]
        summary[result["source"]][result["status"]] += 1
        causes.update(result.get("causes", {}))
        coverage["files"].append({key: result.get(key) for key in ("f", "source", "status", "errors", "errors_in_main", "flags", "unparsed", "reason")})
    coverage["summary"] = {source: dict(counter) for source, counter in sorted(summary.items())}
    coverage["top_error_causes"] = dict(causes.most_common(25))
    (arguments.out / "coverage.json").write_text(json.dumps(coverage, sort_keys=True, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"labelled {len(results)} files and {fragment_files} fragments in {time.monotonic() - started:.0f}s", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
