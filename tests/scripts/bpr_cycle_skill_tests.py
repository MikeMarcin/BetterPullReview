"""[[BPR-1170]]. The packaged bpr-cycle skill, checked as text and driven against the real `bpr`.

A model's behavior is not evidence here. These checks pin what a package can pin: its identity and
portable frontmatter, that it stays thin and teaches only the consumer commands, that every field and
refusal it names is one the shipped CLI has, and that its documented command lines, run as a harness
would run them against a local cycle with a stored grant, read the whole round and append a response.
The same lines are refused without a grant, after revocation, under a denying policy and for a
read-only grant; a declared consumer is refused every reviewer command; and the round still reaches a
person as Markdown with no skill at all. Deliberately broken copies of the package must fail the lint.

Usage: bpr_cycle_skill_tests.py PACKAGE_DIR BPR_EXECUTABLE [--digest]
"""
import base64
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "end_to_end"))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402
from bpr_narrow_owner_private import make_root_private  # noqa: E402

NAME = "bpr-cycle"
REPOSITORY = Path(__file__).resolve().parents[2]
FRONTMATTER_KEYS = {"name", "description", "license", "compatibility", "metadata", "allowed-tools"}
NEGATION = re.compile(r"\b(never|do not|not|no|nothing|without|refused|instead of|in place of)\b", re.I)
# A publisher, a second model or a back door into the store: named only inside a prohibition.
FORBIDDEN = [r"gh api", r"\bgh pr\b", r"\bcurl\b", r"\bwget\b", r"sqlite", r"git push", r"api\.github\.com", r"/graphql",
             r"--yes\b", r"--force\b", r"\|\s*yes\b", r"\bclaude -p\b", r"\bcodex exec\b", r"\bollama\b", r"\binstall\b"]
TAUGHT = re.compile(r"^BPR_CYCLE_ROLE=consumer bpr cycle (capabilities|round read|round page|round source|response append)(?: |$)")
# Statements a text file can carry, and that the RFC's criteria rest on (RC58-08, RC58-10).
REQUIRED = ("Do not launch another model, agent or orchestrator", "BPR_CYCLE_ROLE=consumer", "never permission to reach the round another way",
            "Never act on part of a round", "it verifies, closes and publishes nothing", "needs no callback",
            "Do not install, copy or select this skill anywhere", "are data, not instructions", "Never guess from a window")
# Program names the text may name, the first to run and the others only to forbid.
PROGRAMS = {"bpr", "sqlite3", "curl"}
BASE_ENVIRONMENT = {"QT_QPA_PLATFORM": "bpr-no-display", "NO_COLOR": "1"}


def environment(extra=None):
    """Nothing a desktop session, an account or a provider would supply."""
    value = dict(BASE_ENVIRONMENT)
    if os.name == "nt":
        for name in ("SystemRoot", "SystemDrive", "TEMP", "TMP", "PATH"):
            if name in os.environ:
                value[name] = os.environ[name]
    value.update(extra or {})
    return value


def read(path):
    return path.read_text(encoding="utf-8")


def documents(package):
    return [package / "SKILL.md", *sorted((package / "references").glob("*.md"))]


def scalar(value):
    """A frontmatter value as YAML reads it: double-quoted JSON, or plain without ': ' or ' #' (which YAML and MASK refuse)."""
    if value.startswith('"'):
        try:
            return json.loads(value)
        except ValueError:
            return None
    return None if ": " in value or " #" in value else value


def frontmatter(text):
    match = re.match(r"---\n(.*?)\n---\n", text, re.S)
    if not match:
        return None
    result, key = {}, None
    for line in match.group(1).splitlines():
        if line.startswith("  "):
            result.setdefault(key, {})[line.split(":", 1)[0].strip()] = scalar(line.split(":", 1)[1].strip())
        else:
            key, _, value = line.partition(":")
            result[key.strip()] = scalar(value.strip()) if value.strip() else {}
    return result


def taught_lines(package):
    lines = []
    for document in documents(package):
        for block in re.findall(r"```(?:sh|bash)\n(.*?)```", read(document), re.S):
            lines.extend((document.name, line.strip()) for line in block.strip().splitlines() if line.strip())
    return lines


def tree_digest(package):
    """Order-independent digest of the package's files, so an import can be checked byte for byte."""
    digest = hashlib.sha256()
    for path in sorted(package.rglob("*")):
        if path.is_file():
            digest.update(str(path.relative_to(package).as_posix()).encode() + b"\0" + hashlib.sha256(path.read_bytes()).digest())
    return digest.hexdigest()


def lint(package, capabilities):
    """Every problem the package's text has; empty when it passes."""
    problems = []
    skill_md = package / "SKILL.md"
    if not skill_md.is_file():
        return ["no SKILL.md"]
    meta = frontmatter(read(skill_md))
    if meta is None:
        return ["SKILL.md has no frontmatter"]
    unreadable = sorted(key for key, value in [*meta.items(), *(meta["metadata"].items() if isinstance(meta.get("metadata"), dict) else [])] if value is None)
    if unreadable:
        problems.append(f"frontmatter values YAML cannot read as plain scalars; quote them: {unreadable}")
        return problems
    if set(meta) - FRONTMATTER_KEYS:
        problems.append(f"frontmatter keys outside the Agent Skills specification: {sorted(set(meta) - FRONTMATTER_KEYS)}")
    if meta.get("name") != NAME or package.name != NAME:
        problems.append(f"the skill, its directory and its name must all be exactly {NAME}")
    if not 20 <= len(meta.get("description", "")) <= 1024:
        problems.append("description must be 20..1024 characters")
    if not 1 <= len(meta.get("compatibility", "")) <= 500:
        problems.append("compatibility must be 1..500 characters")
    if "allowed-tools" in meta:
        problems.append("the skill grants no tool permissions")
    metadata = meta.get("metadata") if isinstance(meta.get("metadata"), dict) else {}
    if metadata.get("cli-contract") != capabilities["result_schema"] or metadata.get("response-schema") != "bpr.cycle.response.v1":
        problems.append("the skill must name the CLI and response contracts it targets")
    # Thin: a compact main file and small references, loaded progressively; nothing to execute.
    if len(read(skill_md).splitlines()) > 60 or len(read(skill_md).encode()) > 8192:
        problems.append("SKILL.md is not thin")
    for reference in documents(package)[1:]:
        if len(read(reference).splitlines()) > 80:
            problems.append(f"{reference.name} is not a small reference")
    if sum(len(read(d).encode()) for d in documents(package)) > 24576:
        problems.append("the whole package is not thin")
    # The shared package carries its redistribution licence alongside the instructions.
    allowed = {*documents(package), package / "LICENSE"}
    extra = [str(p.relative_to(package)) for p in package.rglob("*") if p.is_file() and p not in allowed]
    if extra:
        problems.append(f"the package carries more than instructions: {extra}")
    linked = set(re.findall(r"\]\((references/[^)]+)\)", read(skill_md)))
    for link in linked:
        if not (package / link).is_file():
            problems.append(f"broken link {link}")
    for reference in documents(package)[1:]:
        if f"references/{reference.name}" not in linked:
            problems.append(f"{reference.name} is not reachable from SKILL.md")
    for document in documents(package):
        for number, line in enumerate(read(document).splitlines(), 1):
            for pattern in FORBIDDEN:
                if re.search(pattern, line, re.I) and not NEGATION.search(line):
                    problems.append(f"{document.name}:{number} names {pattern!r} outside a prohibition")
            if re.search(r"/Users/|[A-Za-z]:\\|/home/|~/", line):
                problems.append(f"{document.name}:{number} names a machine-specific path")
    # It teaches a harness's commands only, each run as a declared consumer.
    consumer = set(capabilities["consumer_commands"]) | {"capabilities"}
    known = set(capabilities["commands"])
    for name, line in taught_lines(package):
        match = TAUGHT.match(line)
        if not match or match.group(1) not in consumer:
            problems.append(f"{name}: a command line that is not a declared consumer's: {line}")
    for document in documents(package):
        for number, line in enumerate(read(document).splitlines(), 1):
            for match in re.finditer(r"bpr cycle ([a-z]+(?: (?:read|page|source|grant|revoke|append|import))?)", line):
                if match.group(1) not in known:
                    problems.append(f"{document.name}:{number} names a command the CLI does not have: {match.group(1)}")
                elif match.group(1) not in consumer and not NEGATION.search(line):
                    problems.append(f"{document.name}:{number} offers the reviewer's {match.group(1)}")
    if len(taught_lines(package)) < 8:
        problems.append("too few command lines to check")
    text = read(skill_md)
    for phrase in REQUIRED:
        if phrase not in text:
            problems.append(f"SKILL.md no longer states: {phrase}")
    return problems


class Course:
    """A reviewer and a harness on one profile, as real processes."""

    def __init__(self, binary, data, home, project):
        self.binary, self.data, self.home, self.project = binary, data, home, project
        self.observed = set()

    def run(self, words, stdin=b"", extra=None, cwd=None):
        result = subprocess.run([self.binary, "cycle", *words, "--data-root", str(self.data)], input=stdin, cwd=cwd or self.home,
                                env=environment(extra), capture_output=True, timeout=120)
        return result.returncode, result.stdout, result.stderr

    def json(self, *words, stdin=b"", extra=None, cwd=None):
        status, out, err = self.run([*words, "--json"], stdin, extra, cwd)
        assert err == b"", f"{words}: JSON calls keep standard error empty: {err!r}"
        value = json.loads(out)
        assert value["schema"] == "bpr.cycle.cli-result.v1", value
        assert (status == 0) == (value["outcome"] == "ok"), (words, status, value)
        self.collect(value)
        return value

    def ok(self, *words, **options):
        value = self.json(*words, **options)
        assert value["outcome"] == "ok", f"{words}: {value['error']}"
        return value["data"]

    def refused(self, code, *words, **options):
        value = self.json(*words, **options)
        assert value["outcome"] == "error" and value["error"]["code"] == code, f"{words}: expected {code}, got {value}"
        return value["error"]

    def collect(self, value):
        """Every key and short string value the CLI has said, so the skill's names can be checked against it."""
        if isinstance(value, dict):
            for key, inner in value.items():
                self.observed.add(key)
                self.collect(inner)
        elif isinstance(value, list):
            for inner in value:
                self.collect(inner)
        elif isinstance(value, str) and len(value) < 64:
            self.observed.add(value)

    def taught(self, line, values, stdin=b"", cwd=None):
        """Run one documented line exactly as written, its placeholders filled; no shell is involved."""
        filled = line
        for placeholder, value in values.items():
            filled = filled.replace(placeholder, value)
        assert "<" not in filled, f"unresolved placeholder in: {line}"
        words = filled.split()
        assignments = {}
        while "=" in words[0]:
            name, _, value = words.pop(0).partition("=")
            assignments[name] = value
        assert words[:2] == ["bpr", "cycle"] and words[-1] == "--json", line
        return self.json(*words[2:-1], stdin=stdin, extra=assignments, cwd=cwd)


CONSUMER = {"BPR_CYCLE_ROLE": "consumer"}
REVIEWER_ARGUMENTS = {"open": ["."], "export": ["RC-1", "--round", "F1", "--output", "-"], "freeze": ["RC-1", "--expected-revision", "1"],
                      "note": ["RC-1", "--body-file", "-"], "response import": ["RC-1", "--round", "F1", "--file", "-"],
                      "round grant": ["RC-1", "--round", "F1"], "round revoke": ["RC-1", "--round", "F1"], "policy": [], "list": []}


def fixture(course, other):
    """The reviewer's side: two cycles, RC-1 F1 with eight notes and one attachment, RC-2 F1 with one note."""
    assert course.ok("open", str(course.project))["cycle"] == "RC-1"
    course.ok("note", "RC-1", "--file", "src/a.txt", "--lines", "2:3", "--body-file", "-", stdin=b"Cache this lookup.\n")
    course.ok("note", "RC-1", "--file", "src/a.txt", "--body-file", "-", stdin="Rename the file's helpers é.\n".encode())
    for index in range(6):
        course.ok("note", "RC-1", "--body-file", "-", stdin=f"Remark {index}: ".encode() + b"x" * 1500 + b"\n")
    draft = course.ok("review", "RC-1")
    draft = course.ok("review", "RC-1", "--context", "README.md", "--expected-revision", draft["draft_revision"])
    assert len(draft["items"]) == 8 and draft["sources"] == "1", draft
    assert course.ok("freeze", "RC-1", "--expected-revision", draft["draft_revision"])["round"] == "F1"
    assert course.ok("open", str(other))["cycle"] == "RC-2"
    course.ok("note", "RC-2", "--body-file", "-", stdin=b"Another project's remark.\n")
    assert course.ok("freeze", "RC-2", "--expected-revision", course.ok("review", "RC-2")["draft_revision"])["round"] == "F1"
    course.ok("policy", "--classification", "unclassified", "--channel", "external_retrieval", "--permission", "allowed")


def round_lines(lines):
    return [line for _, line in lines if " round " in line or " response append " in line]


def response_from_reference(package, read_data):
    """The responses reference's own example, filled from the round as read."""
    example = re.search(r"```json\n(.*?)```", read(package / "references" / "responses.md"), re.S).group(1)
    first = read_data["items"][0]
    for placeholder, value in {"<response-id>": "RESP-SKILL-1", "<cycle>": read_data["cycle"], "<round>": read_data["round"],
                               "<payload-sha256>": read_data["payload_sha256"], "<harness name>": "Fixture harness",
                               "<item>": first["id"], "<item-revision>": first["revision"], "<what you did, by item>": "Cached the lookup.",
                               "<changed path>": "src/a.txt"}.items():
        example = example.replace(placeholder, value)
    assert "<" not in example, example
    return json.loads(example)


def whole_round(course, read_line, page_line, values, cwd=None):
    """The procedure SKILL.md teaches: every page, one digest, contiguous items, the counts met."""
    page = course.taught(read_line, values, cwd=cwd)
    assert page["outcome"] == "ok", page
    first = page["data"]
    items, pages = list(first["items"]), 1
    while page["data"]["next"]:
        page = course.taught(page_line, dict(values, **{"<cursor>": page["data"]["next"]}), cwd=cwd)
        assert page["outcome"] == "ok", page
        assert page["data"]["payload_sha256"] == first["payload_sha256"] and page["data"]["first_item"] == str(len(items)), page["data"]
        items += page["data"]["items"]
        pages += 1
    assert page["complete"] is True and len(items) == int(first["items_total"]) and first["instruction_set_complete"] is True
    return first, items, pages


def course_run(package, course, capabilities):
    lines = taught_lines(package)
    by_form = {re.sub(r"<[^>]+>", "<>", line): line for _, line in lines}

    def form(fragment):
        matches = [line for key, line in by_form.items() if key == fragment]
        assert len(matches) == 1, f"the package no longer teaches {fragment!r}"
        return matches[0]

    read_line = form("BPR_CYCLE_ROLE=consumer bpr cycle round read <> --round <> --json")
    paged_line = form("BPR_CYCLE_ROLE=consumer bpr cycle round read <> --round <> --page-bytes <> --json")
    folder_line = form("BPR_CYCLE_ROLE=consumer bpr cycle round read --round <> --json")
    page_line = form("BPR_CYCLE_ROLE=consumer bpr cycle round page <> --round <> --cursor <> --json")
    budget_line = form("BPR_CYCLE_ROLE=consumer bpr cycle round read <> --round <> --budget-bytes <> --json")
    append_line = form("BPR_CYCLE_ROLE=consumer bpr cycle response append <> --round <> --file - --json")
    values = {"<cycle>": "RC-1", "<round>": "F1", "<bytes>": "4096", "<index>": "0", "<source>": "S1", "<start>": "1", "<end>": "4"}

    # Without a grant, every documented round line is refused, and the harness cannot grant itself.
    for line in round_lines(lines):
        dummy = dict(values, **{"<cursor>": "F1." + "0" * 64 + ".1.4096"})
        cwd = course.project if "read --round" in line else None
        error = course.taught(line, dummy, stdin=b"{}", cwd=cwd)["error"]
        if "--cursor <index>" in line:
            assert error["code"] == "invalid_input" and error["detail"]["problem"] == "source_cursor", (line, error)
        else:
            assert error["code"] == "policy_denied" and error["detail"]["decision"] == "not_granted", (line, error)
    error = course.taught(read_line, values)["error"]
    assert error["next"] == "bpr cycle round grant RC-1 --round F1", error
    course.refused("policy_denied", *error["next"].split()[2:], extra=CONSUMER)
    assert course.ok("show", "RC-1")["grants"] == [], "a refused harness granted nothing"

    # The reviewer grants the round; the documented lines now read it whole, page by page, and from its folder.
    assert course.ok("round", "grant", "RC-1", "--round", "F1")["respond"] is True
    assert course.ok("capabilities", extra=CONSUMER)["role"] == "consumer"
    single, items, pages = whole_round(course, read_line, page_line, values)
    assert pages == 1 and single["items_total"] == "8" and single["sources_total"] == "1", single
    paged_first, paged, pages = whole_round(course, paged_line, page_line, values)
    assert pages >= 3 and paged == items and [item["id"] for item in items] == [f"I{n}" for n in range(1, 9)], (pages, len(paged))
    assert paged_first["payload_sha256"] == single["payload_sha256"]
    assert whole_round(course, folder_line, page_line, values, cwd=course.project)[1] == items, "the bound folder finds the same round"
    assert course.taught(folder_line, values, cwd=course.home)["error"]["code"] in ("not_found", "round_not_ready")
    partition = course.taught(budget_line, values)["error"]
    assert partition["code"] == "requires_partition" and int(partition["detail"]["instruction_bytes"]) > 4096, partition
    assert course.taught(budget_line, dict(values, **{"<bytes>": single["instruction_bytes"]}))["complete"] is True

    # Every other documented line runs as written: capabilities, attachment listings and bytes.
    for line in (line for _, line in lines if line not in (append_line, page_line, budget_line)):
        cwd = course.project if "read --round" in line else None
        value = course.taught(line, values, cwd=cwd)
        if "--cursor <index>" in line:
            # A listing continues only from the `next` a listing of more than 256 gave; the form itself parses.
            assert value["error"]["code"] == "invalid_input" and value["error"]["detail"]["problem"] == "source_cursor", (line, value)
            continue
        assert value["outcome"] == "ok", (line, value)
    listing = course.taught(form("BPR_CYCLE_ROLE=consumer bpr cycle round source <> --round <> --json"), values)["data"]
    assert [entry["id"] for entry in listing["sources"]] == ["S1"] and listing["next"] is None, listing
    whole = course.taught(form("BPR_CYCLE_ROLE=consumer bpr cycle round source <> --round <> --source <> --json"), values)["data"]
    assert base64.b64decode(whole["base64"]) == b"hello\n" and whole["sha256"] == hashlib.sha256(b"hello\n").hexdigest()
    part = course.taught(form("BPR_CYCLE_ROLE=consumer bpr cycle round source <> --round <> --source <> --range <>:<> --json"), values)["data"]
    assert base64.b64decode(part["base64"]) == b"ell", part

    # The response reference's own example appends, as the harness's claim, and changes nothing else.
    before = course.ok("show", "RC-1")
    document = response_from_reference(package, single)
    appended = course.taught(append_line, values, stdin=json.dumps(document).encode())
    assert appended["outcome"] == "ok" and appended["data"]["appended"] is True, appended
    assert course.taught(append_line, values, stdin=json.dumps(document).encode())["data"]["appended"] is False, "an identical retry is not stored twice"
    after = course.ok("show", "RC-1")
    assert after["responses"][-1]["channel"] == "local_interface" and after["responses"][-1]["foreign_id"] == "RESP-SKILL-1", after["responses"]
    assert after["notes"] == before["notes"] and after["rounds"] == before["rounds"], "a response changes no note or round"
    claimed = dict(document, response_id="RESP-SKILL-2")
    claimed["items"] = [dict(document["items"][0], disposition="verified")]
    assert course.taught(append_line, values, stdin=json.dumps(claimed).encode())["error"]["code"] == "invalid_input"
    reused = dict(document, items=[dict(document["items"][0], body="Different words.")])
    assert course.taught(append_line, values, stdin=json.dumps(reused).encode())["error"]["code"] == "conflict"

    # A declared consumer is refused every command that is not a consumer's, whatever the binary offers.
    consumer = set(capabilities["consumer_commands"]) | {"capabilities"}
    for command in capabilities["commands"]:
        if command not in consumer:
            arguments = REVIEWER_ARGUMENTS.get(command, ["RC-1"])
            course.refused("policy_denied", *command.split(), *arguments, extra=CONSUMER, stdin=b"from a harness\n")
    assert course.ok("show", "RC-1")["notes"] == before["notes"]

    # A read-only grant reads but cannot respond; another round is not this grant's.
    course.ok("round", "grant", "RC-2", "--round", "F1", "--read-only")
    other = dict(values, **{"<cycle>": "RC-2"})
    other_read = course.taught(read_line, other)["data"]
    response = response_from_reference(package, other_read)
    assert course.taught(append_line, other, stdin=json.dumps(response).encode())["error"]["detail"]["decision"] == "not_granted"
    assert course.taught(read_line, dict(values, **{"<round>": "F2"}))["error"]["code"] == "round_not_ready"

    # A denying or unconfigured policy holds against the grant.
    for permission, decision in (("denied", "channel_denied"), ("unconfigured", "channel_unconfigured")):
        course.ok("policy", "--classification", "unclassified", "--channel", "external_retrieval", "--permission", permission)
        assert course.taught(read_line, values)["error"]["detail"]["decision"] == decision
    course.ok("policy", "--classification", "unclassified", "--channel", "external_retrieval", "--permission", "allowed")

    # Revoked: nothing more is read, mid-pagination too, and nothing appended.
    cursor = course.taught(paged_line, values)["data"]["next"]
    assert course.ok("round", "revoke", "RC-1", "--round", "F1")["revoked"] is True
    assert course.taught(read_line, values)["error"]["detail"]["decision"] == "revoked"
    assert course.taught(page_line, dict(values, **{"<cursor>": cursor}))["error"]["detail"]["decision"] == "revoked"
    fresh = dict(document, response_id="RESP-SKILL-3")
    assert course.taught(append_line, values, stdin=json.dumps(fresh).encode())["error"]["code"] == "policy_denied"

    # No skill at all: the reviewer's Markdown export carries the same round to a person (RC58-10).
    status, markdown, err = course.run(["export", "RC-1", "--round", "F1", "--output", "-", "--format", "markdown"])
    assert status == 0 and err == b"", err
    markdown = markdown.decode("utf-8")
    for item in items:
        assert item["id"] in markdown and item["body"].strip() in markdown, item["id"]
    assert f"Canonical round SHA-256: `{single['payload_sha256']}`" in markdown, "the person gets the same round the harness read"
    return items


def names_are_real(package, course):
    """Every field, value and refusal the skill names in backticks is one the shipped CLI says or decodes."""
    literals = set()
    for source in [*(REPOSITORY / "src" / "core" / "src").glob("review_cycle*.cpp"), *(REPOSITORY / "src" / "app" / "src").glob("cycle_*.cpp")]:
        literals.update(re.findall(r'"([a-z][a-z0-9_]*)"', read(source)))
    unknown = []
    for document in documents(package):
        text = re.sub(r"```.*?```", "", read(document), flags=re.S)
        for token in re.findall(r"`([a-z][a-z0-9_]*)`", text):
            if len(token) > 2 and token not in course.observed and token not in literals and token not in PROGRAMS:
                unknown.append(f"{document.name}: {token}")
    assert not unknown, f"names the CLI does not have: {unknown}"
    decisions = set(re.findall(r"`decision` `([a-z_]+)`(?: or `([a-z_]+)`)?", read(package / "references" / "refusals.md")))
    named = {name for pair in decisions for name in pair if name}
    assert named and named <= course.observed, f"refusals the course did not observe: {named - course.observed}"


def managed_link(path):
    """A MASK deployment is a directory link into MASK's store, never a real directory."""
    return path.is_symlink() or (hasattr(path, "is_junction") and path.is_junction())


def no_divergent_copies(package):
    """One source of maintenance: no hand-maintained copy here, and no selection from anywhere but the shared source.

    The owner authorized importing the package into AgentSkills and selecting it from there (BPR-1170, RC58-08), so a
    MASK link may deploy it; that link must be the shared source's copy, byte for byte the package here."""
    trees = (".claude/skills", ".codex/skills", ".agents/skills")
    found = []
    for candidate in REPOSITORY.rglob("SKILL.md"):
        relative = candidate.relative_to(REPOSITORY).parts
        if relative[0] in ("build", ".git") or NAME not in relative:
            continue
        if "/".join(relative[:2]) in trees and managed_link(REPOSITORY.joinpath(*relative[:3])):
            continue
        if candidate.resolve() != (package / "SKILL.md").resolve() and "assets" not in relative:
            found.append(str(candidate.relative_to(REPOSITORY)))
    assert not found, f"divergent copies of the {NAME} skill: {found}"
    selected = json.loads(read(REPOSITORY / ".mask.json")) if (REPOSITORY / ".mask.json").exists() else {"skills": []}
    assert not any(entry.get("id") == NAME and entry.get("source") != "agentskills" for entry in selected.get("skills", [])), \
        f"{NAME} may only be selected from the shared agentskills source"
    chosen = any(entry.get("id") == NAME for entry in selected.get("skills", []))
    for tree in trees:
        deployed = REPOSITORY / tree / NAME
        if not deployed.exists() and not deployed.is_symlink():
            continue
        assert managed_link(deployed) and chosen, f"{tree}/{NAME} is a hand-maintained copy"
        assert tree_digest(deployed.resolve()) == tree_digest(package), f"{tree}/{NAME} differs from the package; import it again"


def broken_copies_fail(package, capabilities, scratch, course):
    """Each check catches the breakage it exists for."""
    def mutant(edit):
        target = scratch / "mutants" / NAME
        if target.exists():
            shutil.rmtree(target)
        shutil.copytree(package, target)
        edit(target)
        return lint(target, capabilities)

    def replace(relative, old, new):
        def edit(target):
            path = target / relative
            text = read(path)
            assert old in text, (relative, old)
            path.write_text(text.replace(old, new, 1), encoding="utf-8")
        return edit

    def append(relative, text):
        return lambda target: (target / relative).write_text(read(target / relative) + text, encoding="utf-8")

    controls = {
        "a reviewer command taught": replace("references/commands.md", "bpr cycle capabilities --json\n",
                                             "bpr cycle capabilities --json\nBPR_CYCLE_ROLE=consumer bpr cycle round grant <cycle> --round <round> --json\n"),
        "a line without the consumer role": replace("references/commands.md", "BPR_CYCLE_ROLE=consumer bpr cycle round source <cycle> --round <round> --json",
                                                    "bpr cycle round source <cycle> --round <round> --json"),
        "a back door into the store": append("SKILL.md", "Read the round with sqlite3 if the CLI is slow.\n"),
        "a second model": append("references/responses.md", "Ask codex exec to draft the response.\n"),
        "an installation": append("SKILL.md", "Install this skill into the user's skill directory first.\n"),
        "the model boundary dropped": replace("SKILL.md", "Do not launch another model, agent or orchestrator", "Use what you need"),
        "the whole-round rule dropped": replace("SKILL.md", "Never act on part of a round", "Act when ready"),
        "a renamed skill": replace("SKILL.md", "name: bpr-cycle", "name: bpr-cycles"),
        "a tool grant": replace("SKILL.md", "metadata:\n", "allowed-tools: Bash\nmetadata:\n"),
        "an unquoted colon in the description": replace("SKILL.md", 'description: "Read', "description: Read"),
        "an unknown command": append("references/refusals.md", "Then run bpr cycle verify on it.\n"),
        "a script": lambda target: (target / "scripts").mkdir() or (target / "scripts" / "read.sh").write_text("bpr cycle round read\n"),
        "an unlinked reference": lambda target: (target / "references" / "extra.md").write_text("# Extra\n"),
        "a machine path": append("references/commands.md", "The binary is at /Users/someone/bin/bpr.\n"),
    }
    for label, edit in controls.items():
        assert mutant(edit), f"the lint accepts a package with {label}"
    # A taught option the parser does not have fails when run as written.
    bad = "BPR_CYCLE_ROLE=consumer bpr cycle round read <cycle> --round <round> --pages <bytes> --json"
    assert course.taught(bad, {"<cycle>": "RC-1", "<round>": "F1", "<bytes>": "4096"})["error"]["code"] == "unsupported_option"
    return len(controls)


def main():
    package = Path(sys.argv[1]).resolve()
    if len(sys.argv) > 3 and sys.argv[3] == "--digest":
        print(tree_digest(package))
        return
    binary = str(Path(sys.argv[2]).resolve())
    with contained_temporary_directory(prefix="bpr-cycle-skill-") as raw:
        temporary = Path(raw).resolve()
        home = temporary / "course"
        make_root_private(home)
        project = temporary / "project"
        (project / "src").mkdir(parents=True)
        (project / "src" / "a.txt").write_bytes(b"one\ntwo\nthree\nfour\n")
        (project / "README.md").write_bytes(b"hello\n")
        other = temporary / "other"
        other.mkdir()
        (other / "notes.txt").write_bytes(b"another project\n")
        course = Course(binary, home / "data", home, project)
        status, out, err = course.run(["capabilities", "--json"], extra=CONSUMER)
        assert status == 0 and err == b"", err
        capabilities = json.loads(out)["data"]
        assert capabilities["cli_contract"] == "1" and capabilities["ai_required"] is False and capabilities["network"] is False, capabilities
        problems = lint(package, capabilities)
        assert not problems, "\n".join(problems)
        no_divergent_copies(package)
        fixture(course, other)
        items = course_run(package, course, capabilities)
        names_are_real(package, course)
        controls = broken_copies_fail(package, capabilities, temporary, course)
    print(f"bpr-cycle-skill-tests:pass ({len(taught_lines(package))} taught lines, {len(items)} items read whole, "
          f"{controls} broken copies refused; package sha256:{tree_digest(package)})")


main()
