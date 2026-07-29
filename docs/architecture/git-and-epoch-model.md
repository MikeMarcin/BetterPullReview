# Git Cache and Review Epoch Model

## Architectural allocation

- The core owns current PR/epoch interaction state, independent left/right selection, comparison policy, and stale-result acceptance.
- The host owns Git processes, bare repositories, refs, object residency, epoch persistence, clocks, cancellation, and pruning.
- The Qt adapter owns epoch selectors, exact-OID labels, and unavailable/history presentation.

## App-owned cache

BetterPullReview owns bare repositories under its documented application-data directory. It does not automatically operate on arbitrary developer worktrees.

```text
repositories/<host-hash>/<repository-id>.git
```

Observed heads are retained through private refs:

```text
refs/betterpullreview/pr/<number>/epoch/<ordinal>
refs/betterpullreview/pr/<number>/review/<account>/<review-id>
```

## Git process policy

Use the installed Git executable through `QProcess`. Disable external diff drivers and text conversion. Do not initialize submodules, run hooks, invoke filters, check out executable worktrees, or execute repository-provided commands.

Required diff operations explicitly use safe configuration and arguments such as `--no-ext-diff` and `--no-textconv` where applicable.

## Epoch discovery

An observation records both base and head OIDs. A new epoch is created when either changes in a way that changes review material or when an explicit historical review commit is imported.

The UI distinguishes:

- Author upload: head changed.
- Force-push/rebase: head lineage changed.
- Base update: base changed while head remained stable.
- Base-branch change: target branch identity changed.
- Equivalent material: OIDs changed but compared file material did not.

## Comparisons

The left and right panes independently select epochs. Defaults:

```text
left: branch base
right: latest observed review epoch
```

Named shortcuts:

- Whole PR
- Previous upload -> latest
- My last reviewed head -> latest
- Any retained epoch -> any retained epoch
- Individual commit or review-associated commit when available

A review or AI session always records the exact OIDs it inspected.
