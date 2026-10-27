# CLI setup

Install the macOS desktop app in `/Applications`, then invoke its CLI directly:

```text
/Applications/BetterPullReview.app/Contents/MacOS/bpr tour capabilities --json
```

For commands typed in a terminal, optionally put the app's executable directory
on that terminal's PATH:

```text
export PATH="/Applications/BetterPullReview.app/Contents/MacOS:$PATH"
bpr tour capabilities --json
```

To keep this after opening another terminal, the user can add that export line
to their shell's startup file: `$HOME/.zshrc` for interactive zsh, or
`$HOME/.bashrc` for interactive Bash (a Bash login shell must source it from
`$HOME/.bash_profile`). Open a new terminal or source the edited file. This is
an explicit user action; the skill never edits those files or creates a symlink.
If the app is elsewhere, substitute its actual absolute path in both commands.
A GUI-launched agent may still lack that PATH; use the locator's returned
absolute executable path throughout the session.

The `agent-free` app still includes `bpr`; `agent-free-automation-off` does not.
Windows and Linux keep their separately recorded native qualification limits.
A macOS package proves nothing about them. The product's
`docs/development/tour-cli.md` describes first-run profiles and sign-in.

For development or a platform without a qualified app CLI, from a source
checkout:

```text
cmake --preset tour-cli
cmake --build --preset tour-cli
cmake --install build/tour-cli --component TourCLI --prefix "$HOME/.local/opt/bpr-tour-cli"
"$HOME/.local/opt/bpr-tour-cli/bin/bpr" tour capabilities --json
```

This is a developer fallback, not a separate CLI release promise. Provide its
absolute `bin/bpr` path to the agent, or put that `bin` directory on PATH before
starting it. A missing or incompatible executable never permits another publisher.
