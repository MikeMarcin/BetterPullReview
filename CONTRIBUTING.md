# Contributing to BetterPullReview

Start with the [build guide](docs/development/build-and-test.md), then read the
root [`AGENTS.md`](AGENTS.md), which holds the workflow and architecture rules,
and the `AGENTS.md` nearest the files you change.

## Before you change code

Read the [coding standard](_coding-standard/STANDARD.md), the
[product contract](docs/architecture/product-contract.md), the
[system architecture](docs/architecture/system-architecture.md), and the RFC
and issue card your change belongs to ([RFC index](docs/rfcs/README.md),
[issue board](docs/issues/README.md)).

The core decides, the host performs, and Qt presents: a widget or Qt model
never calls GitHub, Git, SQLite, AI, the filesystem, the network or a process.

## Work in slices

1. Move the issue card to `30-in-progress`. The folder a card sits in is its
   status.
2. Make the change, preserving unrelated work in the tree.
3. Run the focused checks named in the RFC's verification ledger, plus
   `git diff --check`. The full command set is in
   [`AGENTS.md`](AGENTS.md#commands).
4. Update any document that owns behaviour you changed.
5. Commit the slice before starting the next.

> [!WARNING]
> Never commit screenshots, logs, build trees, credentials, private repository
> content or anything that carries a secret. For optional pre-commit secret
> scanning, see the [BetterLeaks guide](docs/development/betterleaks.md).
