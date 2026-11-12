#!/bin/sh
# RFC-BPR-0108. Makes a Git worktree whose branch proposes a small change against main,
# with origin naming GitHub and main moved after the branch point, a
# working edit and an untracked file. Used by the attended local draft look
# (`freeze_attended_build.py freeze --generate`); usage: make_worktree.sh ROOT.
set -eu
root="$1"
g() { git -c user.name="BPR fixture" -c user.email=fixture@example.invalid -c init.defaultBranch=main -c core.hooksPath=/nonexistent -c commit.gpgsign=false "$@"; }
mkdir -p "$root/seed"
cd "$root/seed"
g init -q
mkdir -p src docs
printf '# Engine\n\nThe search engine.\n' > README.md
printf 'int engine()\n{\n\treturn 1;\n}\n' > src/engine.cpp
g add -A && g commit -qm "Engine"
cd "$root"
g clone -q --bare seed hosted.git
g clone -q hosted.git engine
cd engine
g remote set-url origin https://github.com/bpr-fixtures/engine.git
cd "$root/seed"
printf 'Main moved after the branch.\n' > docs/main-only.md
g add -A && g commit -qm "Main moves on"
g push -q "$root/hosted.git" main
cd "$root/engine"
g fetch -q "$root/hosted.git" "+refs/heads/main:refs/remotes/origin/main"
g checkout -qb feature/search-results
printf 'int search(const char* aText)\n{\n\treturn aText[0] == 0 ? 0 : 1;\n}\n' > src/search.cpp
printf 'int search(const char* aText);\n\nint engine()\n{\n\treturn search("bpr") + 1;\n}\n' > src/engine.cpp
g add -A && g commit -qm "Engine uses search"
printf '# Engine\n\nThe search engine, with search.\n' > README.md
printf 'A plan for the next step.\n' > PLAN.md
