#!/usr/bin/env python3
"""Write RFC-BPR-0100 AC08's flag-move pull request (no remote data).

[[BPR-1996]]. A change that moves an existing configuration loader, unchanged,
behind a new `--strict-toml` flag and adds the new loader that the flag
selects. The legacy key=value path is deleted from `loader.cpp` and added,
byte for byte, to `legacy_loader.cpp`: a drafted tour that narrates what that
path does is narrating code the change only moved. The owner judges a drafted
tour of this pull request against the moved-code rule.

Usage: build_flag_move_fixture.py <construction.json>
Then write the served tree and Git objects with
`build_recorded_pull_request_contract.py` and
`build_recorded_pull_request_pack.py`, as the other constructed fixtures do.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

import build_nested_folders_fixture as nested

# The commits `build_recorded_pull_request_pack.py` builds from the files below.
BASE_OID = "9f5812311f7c6b00b87197a5ab81703624f870b3"
HEAD_OID = "07540b0c68876f9f8e35c967447c38b55d591a12"


def lines(*text: str) -> str:
    return "".join(f"{line}\n" for line in text)


LEGACY_BODY = (
    "\tauto file   = std::ifstream{ aPath };",
    "\tauto config = Config{ };",
    "\tauto line   = std::string{ };",
    "\twhile (std::getline(file, line)) {",
    "\t\tif (line.empty() || line.front() == '#') {",
    "\t\t\tcontinue;",
    "\t\t}",
    "\t\tconst auto equals = line.find('=');",
    "\t\tif (equals == std::string::npos) {",
    "\t\t\tcontinue;",
    "\t\t}",
    "\t\t// A later key replaces an earlier one; values keep inner spaces.",
    "\t\tconfig.Values[trim(line.substr(0, equals))] = trim(line.substr(equals + 1));",
    "\t}",
    "\treturn config;",
)

FILES: tuple[tuple[str, str | None, str], ...] = (
    (
        "README.md",
        lines("# Settings loader", "", "Reads `settings.conf`: one `key = value` per line, `#` starts a comment."),
        lines("# Settings loader", "", "Reads `settings.conf`: one `key = value` per line, `#` starts a comment.", "",
              "Pass `--strict-toml` to read `settings.toml` with the TOML loader instead: tables, quoted strings,",
              "and an error for anything it cannot read."),
    ),
    (
        "src/config/legacy_loader.cpp",
        None,
        lines('#include "loader.h"', "", "#include <fstream>", "", "// The key=value reader every release has used, unchanged.",
              "Config load_legacy_config(const std::string& aPath)", "{", *LEGACY_BODY, "}"),
    ),
    (
        "src/config/loader.cpp",
        lines('#include "loader.h"', "", "#include <fstream>", "", "Config load_config(const std::string& aPath)", "{", *LEGACY_BODY, "}"),
        lines('#include "loader.h"', "", "Config load_config(const std::string& aPath, const LoadOptions& aOptions)", "{",
              "\tif (aOptions.StrictToml) {", "\t\treturn load_toml_config(aPath);", "\t}", "\treturn load_legacy_config(aPath);", "}"),
    ),
    (
        "src/config/loader.h",
        lines("#pragma once", "", "#include <map>", "#include <string>", "", "struct Config", "{", "\tstd::map<std::string, std::string> Values;", "};", "",
              "[[nodiscard]] auto trim(const std::string& aText) -> std::string;", "Config load_config(const std::string& aPath);"),
        lines("#pragma once", "", "#include <map>", "#include <string>", "", "struct Config", "{", "\tstd::map<std::string, std::string> Values;", "};", "",
              "struct LoadOptions", "{", "\t/// Read TOML with `load_toml_config`; off keeps the key=value reader.", "\tbool StrictToml = false;", "};", "",
              "[[nodiscard]] auto trim(const std::string& aText) -> std::string;", "Config load_config(const std::string& aPath, const LoadOptions& aOptions = { });",
              "Config load_legacy_config(const std::string& aPath);", "/// Throws `config_error` naming the line it could not read.",
              "Config load_toml_config(const std::string& aPath);"),
    ),
    (
        "src/config/toml_loader.cpp",
        None,
        lines('#include "loader.h"', "", "#include <fstream>", "#include <stdexcept>", "", "struct config_error : std::runtime_error", "{",
              "\tusing std::runtime_error::runtime_error;", "};", "", "Config load_toml_config(const std::string& aPath)", "{",
              "\tauto file    = std::ifstream{ aPath };", "\tauto config  = Config{ };", "\tauto table   = std::string{ };", "\tauto line    = std::string{ };",
              "\tauto number  = 0;", "\twhile (std::getline(file, line)) {", "\t\t++number;", "\t\tconst auto text = trim(line);",
              "\t\tif (text.empty() || text.front() == '#') {", "\t\t\tcontinue;", "\t\t}", "\t\tif (text.front() == '[') {",
              "\t\t\tif (text.back() != ']') {", "\t\t\t\tthrow config_error{ \"line \" + std::to_string(number) + \": a table name needs ]\" };", "\t\t\t}",
              "\t\t\ttable = trim(text.substr(1, text.size() - 2)) + \".\";", "\t\t\tcontinue;", "\t\t}", "\t\tconst auto equals = text.find('=');",
              "\t\tif (equals == std::string::npos) {", "\t\t\tthrow config_error{ \"line \" + std::to_string(number) + \": expected key = value\" };", "\t\t}",
              "\t\tauto value = trim(text.substr(equals + 1));", "\t\tif (value.size() >= 2 && value.front() == '\"' && value.back() == '\"') {",
              "\t\t\tvalue = value.substr(1, value.size() - 2);", "\t\t}", "\t\tconfig.Values[table + trim(text.substr(0, equals))] = value;", "\t}",
              "\treturn config;", "}"),
    ),
    (
        "src/main.cpp",
        lines('#include "config/loader.h"', "", "#include <iostream>", "", "int main(int argc, char** argv)", "{",
              "\tconst auto config = load_config(argc > 1 ? argv[1] : \"settings.conf\");",
              "\tstd::cout << config.Values.size() << \" settings\\n\";", "\treturn 0;", "}"),
        lines('#include "config/loader.h"', "", "#include <iostream>", "#include <string_view>", "", "int main(int argc, char** argv)", "{",
              "\tauto options = LoadOptions{ };", "\tauto path    = std::string{ \"settings.conf\" };", "\tfor (int index = 1; index < argc; ++index) {",
              "\t\tif (std::string_view{ argv[index] } == \"--strict-toml\") {", "\t\t\toptions.StrictToml = true;", "\t\t\tpath               = \"settings.toml\";",
              "\t\t} else {", "\t\t\tpath = argv[index];", "\t\t}", "\t}", "\tconst auto config = load_config(path, options);",
              "\tstd::cout << config.Values.size() << \" settings\\n\";", "\treturn 0;", "}"),
    ),
    (
        "tests/config/toml_loader_tests.cpp",
        None,
        lines('#include "config/loader.h"', "", "#include <cassert>", "#include <fstream>", "", "int main()", "{",
              "\tstd::ofstream{ \"t.toml\" } << \"[window]\\nwidth = 800\\ntitle = \\\"Main view\\\"\\n\";",
              "\tconst auto config = load_toml_config(\"t.toml\");", "\tassert(config.Values.at(\"window.width\") == \"800\");",
              "\tassert(config.Values.at(\"window.title\") == \"Main view\");", "\treturn 0;", "}"),
    ),
)


def construction() -> dict:
    nested.FILES = FILES
    nested.BASE_OID = BASE_OID
    nested.HEAD_OID = HEAD_OID
    result = nested.construction()
    result["constructed"] = "Synthetic RFC-BPR-0100 AC08 flag-move course, written by scripts/build_flag_move_fixture.py; not a recording"
    result["repository"] = {"name": "flag-move", "owner": "bpr-fixtures"}
    result["commits"][0]["message"] = "Read settings as TOML behind --strict-toml"
    request = result["pull_request"]
    request["number"] = 41
    request["head"]["ref"] = "strict-toml"
    request["title"] = "Read settings as TOML behind --strict-toml"
    request["body"] = "Adds a TOML settings loader selected by `--strict-toml`. Without the flag the existing key=value loader runs as before; it moves to its own file.\n"
    return result


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    output = Path(sys.argv[1])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(construction(), indent="\t", sort_keys=True) + "\n", encoding="utf-8")
    print(f"CONSTRUCTION_OK:{len(FILES)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
