"""[[BPR-1185]]. `docs/development/cycle-cli.md` read against the built `bpr`, so the page says what ships.

The page is parsed, not trusted. Its command table must name exactly the commands `bpr cycle
capabilities` lists, in that order. Each option a row (or the common-options table) gives a command
must be accepted by that command, and every other option the page names must be refused by it with
`unsupported_option`. The exit-code table must use exactly the exit numbers the binary describes,
and every code the course provokes must exit with the number the table gives it. The limits, the
schema name, the owner protocol, the consumer commands and the disclosure channels the prose states
must be the binary's. The page's command examples run as written, in the order its prose requires,
against a local cycle: the grant is refused until the policy example allows its channel, and the
harness's read returns the fields the page names. `bpr` and `bpr cycle` alone print what the page
says they print.

Each check is also run against altered copies of the page, which must fail, so a check that cannot
see a difference is itself a failure.

Usage: cycle_cli_documentation_tests.py BPR_EXECUTABLE [CYCLE_CLI_MD]
"""
import json
import os
from pathlib import Path
import re
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "end_to_end"))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402
from bpr_narrow_owner_private import make_root_private  # noqa: E402

REPOSITORY = Path(__file__).resolve().parents[2]
OPTION = re.compile(r"--[a-z][a-z-]*")
UNDOCUMENTED = "--not-a-documented-option"


def environment():
    """Nothing a desktop session, an account or a provider would supply."""
    value = {"QT_QPA_PLATFORM": "bpr-no-display", "NO_COLOR": "1"}
    if os.name == "nt":
        for name in ("SystemRoot", "SystemDrive", "TEMP", "TMP", "PATH"):
            if name in os.environ:
                value[name] = os.environ[name]
    return value


class Binary:
    def __init__(self, path, data):
        self.path = path
        self.data = data

    def raw(self, arguments, cwd, stdin=b""):
        result = subprocess.run([self.path, *arguments], input=stdin, cwd=cwd, env=environment(), capture_output=True, timeout=120)
        return result.returncode, result.stdout.decode("utf-8"), result.stderr.decode("utf-8")

    def cycle(self, arguments, cwd, stdin=b"", data=True):
        """One `bpr cycle` command with --json: (exit, error code or "ok", data)."""
        command = ["cycle", *arguments, "--json"] + (["--data-root", str(self.data)] if data else [])
        status, out, err = self.raw(command, cwd, stdin)
        assert err == "", f"a JSON call keeps standard error empty: {arguments} {err!r}"
        document = json.loads(out)
        code = document["error"]["code"] if document["error"] else "ok"
        return status, code, document


def section(text, heading):
    start = text.index(f"\n## {heading}\n")
    end = text.find("\n## ", start + 1)
    return text[start:end if end >= 0 else len(text)]


def table_rows(block):
    """The rows of the first Markdown table in `block`, as lists of cells (escaped pipes kept)."""
    rows = []
    for line in block.splitlines():
        if re.match(r"^\|[-| ]+\|$", line):
            continue
        if line.startswith("|"):
            cells = [cell.strip().replace("\\|", "|") for cell in re.split(r"(?<!\\)\|", line.strip())[1:-1]]
            rows.append(cells)
        elif rows:
            break
    return rows[1:]


def options_of(syntax):
    """{option: takes a value} from a row's syntax. `--a/--b X` share X; `[--c]` is a flag."""
    found = {}
    for match in OPTION.finditer(syntax):
        rest = syntax[match.end():]
        while rest.startswith("/--"):
            rest = rest[OPTION.match(rest, 1).end():]
        value = re.match(r" (?![\[-])[^\s\]|]", rest) is not None
        found[match.group()] = value
    return found


def documented(text):
    """What the page claims: commands with their own options, the common options, and exits."""
    commands = {}
    for cells in table_rows(section(text, "Commands")):
        syntax = cells[0].strip("`")
        name = re.match(r"[a-z]+(?: (?:import|append|grant|revoke|read|page|source))?", syntax).group()
        commands[name] = options_of(syntax)
    common_block = text[text.index("Every command also takes these options."):]
    common = {}
    for cells in table_rows(common_block):
        common.update(options_of(cells[0].strip("`")))
    exits = {}
    for cells in table_rows(section(text, "Output")[section(text, "Output").index("| Exit |"):]):
        for code in re.findall(r"`([a-z_]+)`|^(ok)$", cells[1]):
            exits[code[0] or code[1]] = int(cells[0])
    return commands, common, exits


def registry():
    """({command: options}, flags) from the core's own registry, `core::cycle_command_options`."""
    source = (REPOSITORY / "src" / "core" / "src" / "review_cycle_command.cpp").read_text(encoding="utf-8")
    arrays = {name: re.findall(r'"(--[a-z-]+)"sv', body)
              for name, body in re.findall(r"constexpr auto (\w+)\s*=\s*std::array(?:<[^>]*>)?\{([^}]*)\}", source)}
    names = dict(re.findall(r'case cycle_command::(\w+):\s*return "([a-z ]+)";', source))
    body = source[source.index("auto cycle_command_options("):]
    body = body[:body.index("return none;\n\t}") + len("return none;")]
    options, pending = {}, []
    for case, returned in re.findall(r"case cycle_command::(\w+):\s*(?:return (\w+);)?", body):
        pending.append(case)
        if returned:
            for command in pending:
                options[names[command]] = arrays[returned]
            pending = []
    return options, set(arrays["flags"])


def check_commands(text, capabilities):
    commands, common, _ = documented(text)
    problems = []
    if list(commands) != capabilities["commands"]:
        problems.append(f"the command table names {list(commands)}, the binary {capabilities['commands']}")
    expected_common = {"--json", "--non-interactive", "--schema-version", "--data-root", "--profile", "--operation-id"}
    if set(common) != expected_common:
        problems.append(f"the common options are {sorted(common)}")
    owned, _ = registry()
    for name, own in commands.items():
        if name != "capabilities" and sorted(own) != sorted(owned.get(name, [])):
            problems.append(f"the page gives bpr cycle {name} {sorted(own)}; the registry gives it {sorted(owned.get(name, []))}")
    return problems


def check_options(text, binary, cwd, opened, only=None):
    """Each documented option accepted by its command, each other one the page names refused."""
    commands, common, _ = documented(text)
    owned, flags = registry()
    universe = dict(common)
    for name, own in commands.items():
        universe.update(own)
        # An option the registry has and the page omits is probed too.
        for option in owned.get(name, []):
            universe.setdefault(option, option not in flags)
    universe[UNDOCUMENTED] = True
    problems = []
    for name, own in commands.items():
        if only is not None and name not in only:
            continue
        allowed = dict(own) if name != "capabilities" else {}
        allowed.update({key: value for key, value in common.items()
                        if name != "capabilities" or key in ("--json", "--non-interactive", "--schema-version", "--data-root")})
        positional = [str(opened)] if name == "open" else []
        for option, value in universe.items():
            if option in ("--json", "--data-root"):
                continue  # every probe carries these two
            probe = name.split() + positional + [option] + (["x"] if value else [])
            if name != "capabilities" and option != "--profile":
                probe += ["--profile", "999"]  # no such profile: nothing past parsing runs
            status, code, _ = binary.cycle(probe, cwd)
            refused = code == "unsupported_option"
            if option in allowed and refused:
                problems.append(f"bpr cycle {name} refuses {option}, which the page gives it")
            if option not in allowed and not refused:
                problems.append(f"bpr cycle {name} {option} is {code} (exit {status}), not refused as the page implies")
    return problems


def check_exits(text, capabilities, observed):
    _, _, exits = documented(text)
    problems = []
    if sorted({str(number) for number in exits.values()}) != sorted(capabilities["exit_codes"]):
        problems.append(f"the exit table uses {sorted(set(exits.values()))}, the binary {sorted(capabilities['exit_codes'])}")
    for code, status in sorted(observed.items()):
        if exits.get(code) != status:
            problems.append(f"{code} exits {status}; the page says {exits.get(code)}")
    return problems


def check_prose(text, capabilities, policy):
    limits = capabilities["limits"]
    statements = {
        "A note is at most 16 KiB.": limits["note_bytes"] == str(16 * 1024),
        "of at most 16 MiB": limits["response_bytes"] == str(16 * 1024 * 1024),
        "A message to the owner is at most 5 MiB": limits["owner_message_bytes"] == str(5 * 1024 * 1024),
        "(1 MiB by default, 4 KiB to 2 MiB)": (limits["round_page_bytes"], limits["round_page_bytes_minimum"], limits["round_page_bytes_maximum"])
        == (str(1024 * 1024), str(4 * 1024), str(2 * 1024 * 1024)),
        "256 at a time": limits["round_source_list_entries"] == "256",
        "1 MiB at a time": limits["round_source_chunk_bytes"] == str(1024 * 1024),
        "protocol 6": "protocol 6" in capabilities["owner"],
        "`bpr.cycle.cli-result.v1`": capabilities["result_schema"] == "bpr.cycle.cli-result.v1",
    }
    problems = [f"the page says {phrase!r}, which the binary does not" for phrase, true in statements.items() if phrase not in text or not true]
    consumer = re.search(r"refuses every command\s+but (.+?) with `policy_denied`", text, re.S)
    named = re.findall(r"`([a-z ]+)`", consumer.group(1)) if consumer else []
    if sorted(named) != sorted(capabilities["consumer_commands"] + ["capabilities"]):
        problems.append(f"a declared consumer may run {named} by the page, {capabilities['consumer_commands']} and capabilities by the binary")
    disclosure = section(text, "Disclosure policy")
    channels = re.search(r"each channel \((.+?)\) decides", disclosure, re.S)
    kinds = re.search(r"each kind\s+of source \((.+?)\)", disclosure, re.S)
    rows = policy["policy"]
    if not channels or re.findall(r"`([a-z_]+)`", channels.group(1)) != list(dict.fromkeys(row["channel"] for row in rows)):
        problems.append("the page's channels are not the policy's, in its order")
    if not kinds or re.findall(r"`([a-z_]+)`", kinds.group(1)) != list(dict.fromkeys(row["classification"] for row in rows)):
        problems.append("the page's kinds of source are not the policy's, in its order")
    unclassified = {row["channel"]: row["decision"] for row in rows if row["classification"] == "unclassified" and row["configured"] == "unconfigured"}
    another_program = re.search(r"channels that reach\s+another program \((.+?)\) are not allowed", disclosure, re.S)
    reaching = re.findall(r"`([a-z_]+)`", another_program.group(1)) if another_program else []
    if sorted(channel for channel, decision in unclassified.items() if decision != "allowed") != sorted(reaching):
        problems.append(f"by default an unclassified source is refused {sorted(c for c, d in unclassified.items() if d != 'allowed')}; the page says {reaching}")
    return problems


def examples(text):
    """The `bpr cycle` lines of the page's sh blocks, in page order."""
    lines = []
    for block in re.findall(r"```sh\n(.*?)```", text, re.S):
        for line in block.splitlines():
            words = line.split()
            if len(words) > 1 and words[0].endswith("bpr") and words[1] == "cycle":
                lines.append(words[2:])
    return lines


def run_examples(text, binary, project, observed):
    """The examples, run as written. The grant needs the policy example first, as the prose says."""
    lines = examples(text)
    grant = next(words for words in lines if words[:2] == ["round", "grant"])
    allow = next(words for words in lines if words[:1] == ["policy"])
    read = next(words for words in lines if words[:2] == ["round", "read"])
    capabilities = next(words for words in lines if words == ["capabilities"])
    problems = []
    status, code, _ = binary.cycle(capabilities, project)
    if (status, code) != (0, "ok"):
        problems.append(f"bpr cycle capabilities: {code}")
    status, code, _ = binary.cycle(grant, project)
    if code != "policy_denied":
        problems.append(f"the grant before the policy example is {code}, not policy_denied")
    for words in (allow, grant):
        status, code, _ = binary.cycle(words, project)
        if code != "ok":
            problems.append(f"bpr cycle {' '.join(words)}: {code}")
    status, code, document = binary.cycle([word for word in read if word != "--json"], project)
    data = document["data"] or {}
    named = ("cycle", "round", "payload_sha256", "items_total", "suggestions_total", "sources_total", "instruction_bytes", "source_bytes")
    missing = [key for key in named if key not in json.dumps(data)]
    if code != "ok" or missing or data.get("round") != "F2" or data.get("cycle") != "RC-3":
        problems.append(f"bpr cycle {' '.join(read)}: {code}, missing {missing}, {data.get('cycle')} {data.get('round')}")
    if "complete" not in document or "next" not in json.dumps(data):
        problems.append("the read's page carries no complete flag or next cursor")
    # Granted now, so a budget smaller than the instructions is the partition refusal.
    status, code, _ = binary.cycle(["round", "read", "RC-3", "--round", "F2", "--budget-bytes", "1"], project)
    if code != "requires_partition":
        problems.append(f"a read with a one-byte budget is {code}, not requires_partition")
    observed[code] = status
    return problems


def check_help(text, binary, cwd, capabilities):
    problems = []
    status, out, _ = binary.raw([], cwd)
    if status != 0 or "bpr tour" not in out or "bpr cycle" not in out:
        problems.append(f"bpr alone prints {out!r} (exit {status}), not its two families")
    for arguments in (["cycle"], ["cycle", "help"]):
        status, out, _ = binary.raw(arguments, cwd)
        listed = re.search(r"^Commands: (.+)$", out, re.M)
        if status != 0 or not listed or listed.group(1).split(", ") != capabilities["commands"]:
            problems.append(f"bpr {' '.join(arguments)} does not print the command list capabilities gives")
    if "`bpr cycle` alone, or `bpr cycle help`, prints the command list" not in text:
        problems.append("the page does not say what bpr cycle alone prints")
    return problems


def provoke(binary, project, temporary):
    """Codes the course can cause, with their exits."""
    observed = {}

    def note(status, code):
        assert observed.get(code, status) == status, f"{code} exits {status} and {observed[code]}"
        observed[code] = status

    for arguments in (["list", "--bogus"], ["frobnicate"], ["list", "--schema-version", "2"], ["show", "RC-x"],
                      ["show", "RC-99"], ["list", "--profile", "999"], ["note", "RC-3", "--non-interactive"],
                      ["freeze", "RC-3", "--expected-revision", "1"], ["round", "read", "RC-3", "--round", "F1"]):
        status, code, _ = binary.cycle(arguments, project)
        note(status, code)
    taken = temporary / "taken.md"
    taken.write_bytes(b"other bytes\n")
    status, code, _ = binary.cycle(["export", "RC-3", "--round", "F1", "--output", str(taken)], project)
    note(status, code)
    link = temporary / "response-link.json"
    link.symlink_to(taken)
    status, code, _ = binary.cycle(["response", "import", "RC-3", "--round", "F1", "--file", str(link)], project)
    note(status, code)
    damaged = temporary / "damaged"
    make_root_private(damaged)
    other = Binary(binary.path, damaged)
    other.cycle(["open", str(project)], project)
    stores = list(damaged.rglob("review-cycles.sqlite3"))
    assert len(stores) == 1, stores
    stores[0].write_bytes(b"not a database" * 512)
    status, code, _ = other.cycle(["list"], project)
    note(status, code)
    expected = {"unsupported_option", "unsupported_command", "unsupported_version", "invalid_input", "not_found", "profile_unknown",
                "interaction_required", "stale_revision", "policy_denied", "conflict", "source_unavailable", "storage_unavailable"}
    assert expected <= set(observed), f"provoked {sorted(observed)}"
    return observed


def prepare(binary, temporary):
    """Three cycles, so the page's RC-3 exists, and RC-3 with two frozen rounds, F1 and F2."""
    projects = []
    for index in (1, 2, 3):
        project = temporary / f"project-{index}"
        (project / "src").mkdir(parents=True)
        (project / "src" / "a.txt").write_bytes(b"one\ntwo\nthree\n")
        status, code, document = binary.cycle(["open", str(project)], temporary)
        assert code == "ok" and document["data"]["cycle"] == f"RC-{index}", document
        projects.append(project)
    project = projects[2]
    for item, body in (("I1", b"First.\n"), ("I2", b"Second.\n")):
        status, code, _ = binary.cycle(["note", "RC-3", "--file", "src/a.txt", "--lines", "2:2", "--body-file", "-"], project, stdin=body)
        assert code == "ok", code
        status, code, document = binary.cycle(["review", "RC-3"], project)
        assert code == "ok", document
        if item == "I2":
            status, code, document = binary.cycle(["review", "RC-3", "--include", item, "--expected-revision", document["data"]["draft_revision"]], project)
            assert code == "ok", document
        status, code, document = binary.cycle(["freeze", "RC-3", "--expected-revision", document["data"]["draft_revision"]], project)
        assert code == "ok" and document["data"]["round"] == f"F{item[1]}", document
    return project


def negative_controls(text, capabilities, policy, observed, binary, project):
    """Altered pages each fail the check that should see them."""
    commands_section = section(text, "Commands")
    altered = {
        "a command left out": (check_commands, text.replace(commands_section, re.sub(r"\n\| `policy .*?\n", "\n", commands_section)), (capabilities,)),
        "an option the binary refuses": (lambda page: check_options(page, binary, project, project, ["list"]),
                                         text.replace("`list [--cursor RC-n]`", "`list [--cursor RC-n] [--all]`"), ()),
        "an option the page drops": (lambda page: check_options(page, binary, project, project, ["capture"]),
                                     text.replace("`capture [RC-n] [--again]`", "`capture [RC-n]`"), ()),
        "a code under the wrong exit": (check_exits, text.replace("| 3 | `interaction_required` |", "| 3 | `interaction_required`, `policy_denied` |")
                                        .replace("| 5 | `policy_denied`, ", "| 5 | "), (capabilities, observed)),
        "a wrong limit": (check_prose, text.replace("A note is at most 16 KiB.", "A note is at most 32 KiB."), (capabilities, policy)),
        "a channel left out": (check_prose, text.replace("`local_file`,\n`terminal_output`, ", "`local_file`,\n"), (capabilities, policy)),
    }
    for label, (check, page, extra) in altered.items():
        assert page != text, f"the control for {label} changed nothing"
        assert check(page, *extra), f"the checks accept a page with {label}"
    return len(altered)


def main():
    binary_path = str(Path(sys.argv[1]).resolve())
    page = Path(sys.argv[2]) if len(sys.argv) > 2 else REPOSITORY / "docs" / "development" / "cycle-cli.md"
    text = page.read_text(encoding="utf-8")
    with contained_temporary_directory(prefix="bpr-cycle-doc-") as raw:
        temporary = Path(raw).resolve()
        home = temporary / "course"
        make_root_private(home)
        binary = Binary(binary_path, home / "data")
        status, code, document = binary.cycle(["capabilities"], temporary, data=False)
        assert (status, code) == (0, "ok"), document
        capabilities = document["data"]
        project = prepare(binary, temporary)
        status, code, document = binary.cycle(["policy"], project)
        policy = document["data"]
        observed = provoke(binary, project, temporary)
        problems = run_examples(text, binary, project, observed)
        problems += (check_commands(text, capabilities) + check_options(text, binary, project, project)
                     + check_exits(text, capabilities, observed) + check_prose(text, capabilities, policy)
                     + check_help(text, binary, project, capabilities))
        assert not problems, "\n".join(problems)
        controls = negative_controls(text, capabilities, policy, observed, binary, project)
    commands, common, exits = documented(text)
    print(f"cycle-cli-documentation:pass ({len(commands)} commands, {sum(map(len, commands.values())) + len(common)} options, "
          f"{len(exits)} codes, {len(observed)} provoked, {len(examples(text))} examples, {controls} altered pages refused)")


if __name__ == "__main__":
    main()
