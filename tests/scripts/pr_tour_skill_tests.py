"""[[BPR-1065]]. Static and syntactic checks of the packaged pr-tour skill against the real CLI parser.

A model's behavior is not evidence here. These checks pin what a text file can pin: the package identity,
that the wrapper stays thin, that it names no publisher or database path of its own, and that every
command it teaches is one the shipped CLI accepts.
"""
import json
import os
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

SKILL = Path(sys.argv[1]).resolve()
CLI = str(Path(sys.argv[2]).resolve())
FRONTMATTER_KEYS = {"name", "description", "license", "compatibility", "metadata", "allowed-tools"}
NEGATION = re.compile(r"\b(never|do not|not|without|no)\b", re.I)
FORBIDDEN = [r"gh api", r"\bcurl\b", r"sqlite", r"--yes\b", r"echo yes", r"\|\s*yes\b", r"git push", r"api\.github\.com", r"/graphql", r"\bgh pr\b"]
PLACEHOLDERS = {
    "<PR-URL>": "https://github.com/o/r/pull/7", "<profile>": "1", "<draft>": "0" * 64, "<stop>": "1", "<other-stop>": "2",
    "<revision>": "1", "<ticket>": "1", "<proposal>": "1", "<id>": "req-1", "<path>": "src/a.cpp", "<first>": "1", "<last>": "3",
    "<exact passage>": "an exact passage",
}
SYNTAX_FAILURES = {"unsupported_command", "unsupported_option", "unsupported_schema", "input_too_large"}


def fail(message):
    print("FAILED:", message, file=sys.stderr)
    sys.exit(1)


def read(path):
    return path.read_text(encoding="utf-8")


def frontmatter(text):
    match = re.match(r"---\n(.*?)\n---\n", text, re.S)
    if not match:
        fail("SKILL.md has no frontmatter")
    result, key = {}, None
    for line in match.group(1).splitlines():
        if line.startswith("  "):
            result.setdefault(key, {})[line.split(":", 1)[0].strip()] = line.split(":", 1)[1].strip()
        else:
            key, _, value = line.partition(":")
            result[key.strip()] = value.strip() if value.strip() else {}
    return result


def tree_digest(root=SKILL):
    """Order-independent digest of the package's files, so an import can be checked byte for byte."""
    import hashlib
    digest = hashlib.sha256()
    for path in sorted(root.rglob("*")):
        if path.is_file():
            digest.update(str(path.relative_to(root)).encode() + b"\0" + hashlib.sha256(path.read_bytes()).digest())
    return digest.hexdigest()


def managed_link(path):
    """A MASK deployment is a directory link into MASK's store, never a real directory."""
    return path.is_symlink() or (hasattr(path, "is_junction") and path.is_junction())


def main():
    if len(sys.argv) > 3 and sys.argv[3] == "--digest":
        print(tree_digest())
        return
    skill_md = SKILL / "SKILL.md"
    documents = [skill_md, *sorted((SKILL / "references").glob("*.md"))]
    meta = frontmatter(read(skill_md))
    if set(meta) - FRONTMATTER_KEYS:
        fail(f"frontmatter keys outside the Agent Skills specification: {sorted(set(meta) - FRONTMATTER_KEYS)}")
    if meta.get("name") != "pr-tour" or SKILL.name != "pr-tour":
        fail("the skill, its directory and its name must all be exactly pr-tour")
    if not 20 <= len(meta.get("description", "")) <= 1024 or "allowed-tools" in meta:
        fail("description must be 20..1024 characters and the skill grants no tool permissions")
    if meta.get("metadata", {}).get("cli-contract") != "bpr.tour.cli-result.v1":
        fail("the skill must name the CLI contract it targets")
    # Thin: a compact main file and small references, loaded progressively.
    if len(read(skill_md).splitlines()) > 60 or len(read(skill_md).encode()) > 8192:
        fail("SKILL.md is not thin")
    for reference in documents[1:]:
        if len(read(reference).splitlines()) > 80:
            fail(f"{reference.name} is not a small reference")
    if sum(len(read(d).encode()) for d in documents) > 24576:
        fail("the whole package is not thin")
    for link in re.findall(r"\]\((references/[^)]+)\)", read(skill_md)):
        if not (SKILL / link).is_file():
            fail(f"broken link {link}")
    # No publisher, database or machine-specific path of its own; each mention must be a prohibition.
    for document in documents:
        for number, line in enumerate(read(document).splitlines(), 1):
            for pattern in FORBIDDEN:
                if re.search(pattern, line) and not NEGATION.search(line):
                    fail(f"{document.name}:{number} names {pattern!r} outside a prohibition")
            if re.search(r"/Users/|C:\\|/home/|~/", line):
                fail(f"{document.name}:{number} names a machine-specific path")
    for document in documents:
        text = read(document)
        for block in re.findall(r"```(\w*)\n(.*?)```", text, re.S):
            for line in block[1].strip().splitlines():
                if block[0] in ("sh", "bash") and not line.startswith("bpr tour "):
                    fail(f"{document.name}: a command example that is not bpr tour: {line}")
    capabilities = subprocess.run([CLI, "tour", "capabilities", "--json"], capture_output=True, timeout=30)
    data = json.loads(capabilities.stdout)["data"]
    if capabilities.returncode != 0 or data["cli_contract"] != "1" or "assist" not in data or data["assist"]["model_launched_by_bpr"]:
        fail("the CLI does not offer the contract this skill targets")
    known = set(data["commands"])
    subcommands = {"assist": {"context", "complete", "cancel"}, "hooks": {"show", "edit", "reset"}, "outline": {"use"}}
    examples = []
    for document in documents:
        for match in re.finditer(r"`?bpr tour ([a-z]+)(?: ([a-z]+))?", read(document)):
            if match.group(1) not in known:
                fail(f"{document.name} teaches a command the CLI does not have: {match.group(1)}")
            if match.group(1) in subcommands and match.group(2) and match.group(2) not in subcommands[match.group(1)] and match.group(2) not in ("<proposal>",):
                fail(f"{document.name} teaches an unknown {match.group(1)} form: {match.group(2)}")
        for block in re.findall(r"```(?:sh|bash)\n(.*?)```", read(document), re.S):
            examples.extend(line.strip() for line in block.strip().splitlines())
    if len(examples) < 20:
        fail("too few command examples to check")
    # Every taught command line is accepted by the real parser. Semantic failures (an unknown draft) are fine here.
    with contained_temporary_directory(prefix="bpr-skill-") as root:
        private = Path(root) / "data"
        for example in examples:
            line = example
            for placeholder, value in PLACEHOLDERS.items():
                line = line.replace(placeholder, value)
            if "<" in line:
                fail(f"unresolved placeholder in: {example}")
            words = re.findall(r'"[^"]*"|\S+', line)
            words = [w[1:-1] if w.startswith('"') else w for w in words]
            argv = [CLI, *words[1:], "--data-root", str(private)]
            result = subprocess.run(argv, input=b"{}", capture_output=True, timeout=60)
            try:
                error = json.loads(result.stdout).get("error")
            except ValueError:
                fail(f"no JSON envelope from: {example}")
            if error and error["code"] in SYNTAX_FAILURES:
                fail(f"the CLI does not accept the taught syntax {example!r}: {error}")
    # One source of maintenance: no hand-maintained copy of the skill lives in this repository outside the
    # candidate package and the imported pack asset. The owner authorized landing the package in AgentSkills and
    # selecting it from there (BPR-1067, 2026-10-06), so a MASK link may deploy it; that link must be the shared
    # source's copy, byte for byte the package here.
    root = SKILL.parents[2]
    trees = (".claude/skills", ".codex/skills", ".agents/skills")
    allowed = {SKILL / "SKILL.md"}
    found = []
    for directory, children, files in os.walk(root):
        current = Path(directory)
        # A registered nested worktree is another checkout, not a divergent
        # skill implementation in this checkout. Never inspect its contents.
        if current != root and (current / ".git").exists():
            children.clear()
            continue
        if current == root:
            children[:] = [name for name in children if name not in ("build", ".git")]
        if "SKILL.md" not in files:
            continue
        candidate = current / "SKILL.md"
        relative = candidate.relative_to(root).parts
        if "pr-tour" not in relative:
            continue
        if "/".join(relative[:2]) in trees and managed_link(root.joinpath(*relative[:3])):
            continue
        if candidate not in allowed and "assets" not in relative:
            found.append(str(candidate.relative_to(root)))
    if found:
        fail(f"divergent copies of the pr-tour skill: {found}")
    selected = json.loads((root / ".mask.json").read_text()) if (root / ".mask.json").exists() else {"skills": []}
    if any(entry.get("id") == "pr-tour" and entry.get("source") != "agentskills" for entry in selected["skills"]):
        fail("pr-tour may only be selected from the shared agentskills source")
    chosen = any(entry.get("id") == "pr-tour" for entry in selected["skills"])
    for tree in trees:
        deployed = root / tree / "pr-tour"
        if not deployed.exists() and not deployed.is_symlink():
            continue
        if not (managed_link(deployed) and chosen):
            fail(f"{tree}/pr-tour is a hand-maintained copy")
        if tree_digest(deployed.resolve()) != tree_digest():
            fail(f"{tree}/pr-tour differs from the package; import it again")
    # The boundaries a text file can carry stay stated.
    text = read(skill_md)
    for phrase in ("never permission to fall back", "Never mint approval", "not permission to publish", "A tool read is not a visit", "no Apply/Dismiss", "Do not launch another model"):
        if phrase not in text:
            fail(f"SKILL.md no longer states: {phrase}")
    print("pr-tour-skill-tests:pass")


main()
