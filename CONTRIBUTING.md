# Contributing to BetterPullReview

This page is the on-ramp for changing BetterPullReview: what to read first, how
work is tracked, the architecture rules every change follows, and how to verify
a change before you commit it.

To get a working build first, follow the
[build guide](docs/development/build-and-test.md).

## Read first

Before changing code, read:

1. The root [`AGENTS.md`](AGENTS.md) and the `AGENTS.md` nearest the files you
   are changing. Each layer (`src/core/`, `src/application/`, `src/adapters/`,
   `tests/`, `packaging/` and others) has its own.
2. The coding standard, [`_coding-standard/STANDARD.md`](_coding-standard/STANDARD.md).
3. The [product contract](docs/architecture/product-contract.md) and the
   [system architecture](docs/architecture/system-architecture.md).
4. The RFC your change belongs to, from the [RFC index](docs/rfcs/README.md),
   and its live issue card on the [issue board](docs/issues/README.md).

The [documentation index](docs/README.md) maps everything else.

## How work is tracked

- **RFCs** describe a design. Their status lives only in the RFC's front matter
  and the [RFC index](docs/rfcs/README.md).
- **Issue cards** under `docs/issues/` are the units of work. The folder a card
  sits in *is* its status (`00-inbox`, `10-backlog`, `20-ready`, `30-in-progress`,
  `40-parked`, `50-blocked`, `60-done`); move the card as the work moves.
  `60-done/` is final.
- **Dependencies are gates.** Do not start an RFC while one of its prerequisites
  is still open, unless the RFC index records a narrow product-owner exception.
  The current exception lets work on the available targets advance past an
  open real-Windows verification card. It never waives that Windows evidence,
  never allows closing an RFC with unmet explicit dependencies, and never lets
  hosted workflow output stand in for it.

Work in narrow slices, one issue card at a time:

1. Move the card to in-progress.
2. Make the change, preserving unrelated work in the tree.
3. Verify the public and runtime surface you changed.
4. Commit the completed slice before starting the next one.

## Architecture rules

The core decides, the host performs, and Qt presents.

- **Core** (`src/core/`) is deterministic: equal inputs and state give equal
  results, identities, ordering and diagnostics. All effects stay explicit and
  belong to the host. See
  [external determinism](docs/architecture/deterministic-functional-core-imperative-host-qt-presentation-adapter.md#21-external-determinism).
  A library's internal timing or layout variation does not by itself rule it
  out, but Qt independence, dependency approval and the
  [implementation inventory](docs/architecture/system-architecture.md#core-implementation-inventory)
  are separate requirements it must also meet.
- **Host** (`src/application/`) owns effects, authority sessions, jobs, caches,
  cancellation and lifecycle. It must not duplicate core policy.
- **Qt** (`src/adapters/qt/`) owns widgets, actions, focus, layout,
  accessibility and event translation. A widget or Qt model never calls GitHub,
  Git, SQLite, AI, the filesystem, the network or a process directly.
- **Asynchronous results carry identity.** Every completion carries its request,
  session, revision and generation identity, as applicable, and returns to one
  serialized decision lane.
- **Small changes stay small.** A bounded change updates a bounded projection.
  Do not reset a whole model or scan a whole review for one interaction.

Ask the owner before hand-writing a major general-purpose system in the core
(a parser, codec, hash, regex or diff engine, and the like); a library may be
the better answer. The root [`AGENTS.md`](AGENTS.md) has the details.

## Verify your change

The full command set:

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
cmake --build --preset dev --target coding-format-check coding-tidy

cmake --preset core-only
cmake --build --preset core-only
ctest --preset core-only --output-on-failure

python3 scripts/verify_documentation.py --root .
git diff --check
```

This is a reference, not a matrix to run on every slice:

- For an ordinary slice, choose the focused commands named in the affected
  RFC's verification ledger.
- At pack or milestone completion, run one integration-focused pass. Full
  relevant suites and a critical review are appropriate when the scope
  warrants them.
- Reuse earlier passing results whose inputs have not changed.

The format check needs clang-format 22 exactly, and the documentation scripts
need Python 3.10 or newer; see the [build guide](docs/development/build-and-test.md#prerequisites).

> [!WARNING]
> Never commit generated screenshots, logs, build trees, credentials, private
> repository content or anything that carries a secret.

## Optional local secret scanning

To scan for secrets before each commit:

1. Run `python3 scripts/betterleaks.py setup` to install the pinned, verified
   BetterLeaks binary.
2. Run `python3 scripts/betterleaks.py hook-install` to opt this checkout into
   scanning the index before commits.

The `staged`, `tracked` and `history` commands scan local inputs without
network access or credential validation. This tooling is not part of any CI
entry point. The [BetterLeaks guide](docs/development/betterleaks.md) covers
setup, policy, troubleshooting and removing the hook.

## Documentation impact

Every change asks whether documentation must change with it. Check whether you
touched any of:

- public APIs
- provider or authentication behavior
- identity or persistence
- threading or lifecycle
- UI commands
- build or package configuration
- security
- launch behavior

If so, update the document that owns that behavior, or record an
evidence-backed `no-impact` disposition in the RFC ledger. Editing a nearby
document does not satisfy an obligation that belongs to another one.
