#!/bin/sh
# RFC-BPR-0112. The attended workflow look's fixture: the local draft
# worktree (tests/fixtures/local-draft/make_worktree.sh) and a writable copy
# of the synthetic organization companion beside it, whose upstream folder
# the owner may change. Usage, from the source tree: make_look.sh ROOT.
set -eu
mkdir -p "$1"
root="$(cd "$1" && pwd)"
here="$(cd "$(dirname "$0")" && pwd)"
sh "$here/../local-draft/make_worktree.sh" "$root"
rm -rf "$root/companion"
cp -R "$here" "$root/companion"
rm -f "$root/companion/make_look.sh"
chmod +x "$root/companion/synthetic_org_companion.py" "$root/companion/upstream/helpers/validate"
