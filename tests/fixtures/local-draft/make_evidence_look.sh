#!/bin/sh
# RFC-BPR-0109. The attended local draft evidence look: the worktree of
# make_worktree.sh, and beside it an evidence folder as an author's tools
# would leave it: one bpr.evidence-import document bound to the branch head,
# with a kept test log, a link, a model check and two proposed questions.
# Usage: make_evidence_look.sh ROOT. Nothing here runs a check.
set -eu
root="$1"
here="$(cd "$(dirname "$0")" && pwd)"
sh "$here/make_worktree.sh" "$root"
head="$(git -C "$root/engine" rev-parse HEAD)"
mkdir -p "$root/evidence/logs"
printf 'search_tests: 6 passed, 0 failed (Debug, macOS 26)\n' > "$root/evidence/logs/search_tests.txt"
printf 'TLC2 Version 2.18\nModel checking completed. No error has been found.\n  4 distinct states found.\n' > "$root/evidence/logs/tlc.txt"
cat > "$root/evidence/look.json" <<EOF
{
  "format": "bpr.evidence-import",
  "version": 1,
  "producer": { "kind": "tool", "name": "search-bridge", "version": "0.4" },
  "claims": [
    { "key": "empty", "kind": "bug_fix", "text": "An empty query returns no results instead of reading past the string." },
    { "key": "order", "kind": "formal_property", "text": "Equal scores keep their insertion order." }
  ],
  "checks": [
    { "key": "unit", "method": "test", "title": "search_tests", "command": "ctest -R search", "claims": ["empty"] },
    { "key": "tla", "method": "model_check", "title": "order.tla", "command": "tlc order.tla", "claims": ["order"],
      "formal": { "property": "StableOrder", "subject": "order.tla, a model of the ranking", "tool": "TLC 2.18", "bounds": "4 results, 3 equal scores" } }
  ],
  "results": [
    { "check": "unit", "outcome": "pass", "class": "reported", "material": { "head": "$head" }, "inputs": ["src/"],
      "environment": "macOS 26, Debug", "artifacts": [ { "name": "Test log", "path": "logs/search_tests.txt" }, { "name": "CI run", "url": "https://ci.example/runs/42" } ] },
    { "check": "tla", "outcome": "pass", "class": "reported", "material": { "head": "$head" },
      "limitations": "Four results at most.", "artifacts": [ { "name": "TLC output", "path": "logs/tlc.txt" } ] }
  ],
  "questions": [
    { "text": "Should ties break by name instead?", "claim": "order" },
    { "text": "Does the CLI path need the same empty-query fix?", "claim": "empty" }
  ]
}
EOF
