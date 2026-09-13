# Synthetic base force-push

`base-rewrite.pack` adds a separate root commit to the graph in `manifest.json`:

- Original base: `757e8f259231f0cb31b6774c14e6acdbf54b48f3`.
- Rewritten base: `80ec898e013c97d16dce3f6eae67e29701a988e5`.
- Both use tree `add25fa19f47a0e005ede520e6f218cc2b236277`.
- The rewritten base has no parents. Neither base is an ancestor of the other.
- Both historical heads retain their original parent, the original base.
  Their merge base with the rewritten base is unavailable.

The public fixture keeps its head and all three timeline events unchanged
between opens. Event 91702 is therefore the same imported occurrence before
and after the base force-push. Event 91701 names the existing disconnected
historical head, which has no computable base on either open. Exact installed
cache topology, retained live anchors, historical provenance and comparison
availability are asserted by `epoch_portable_occurrence_public_path.py`.

To regenerate, import both embedded packs from `github_public_path_fixture.cpp`
into a disposable bare SHA-1 repository with system/global configuration
disabled. Use `git commit-tree` with the tree above, no parent, author and
committer `BetterPullReview Fixture <fixture@betterpullreview.invalid>`, both
dates `1700000400 +0000`, and message
`Synthetic base branch replacement root` followed by a newline. Supply the new
root, `f9108913c053354eab2e4771eef27e957a400966`, and
`6458d96bf0ccddc1ef2fcf60e61c93a28e69d665`, one per line in that order, to
`git pack-objects --revs --stdout`. No real repository or provider is involved.

The frozen pack is 1960 bytes, SHA-256
`effd74585445aad9d292ab4ee1d34df0959cedd636215735f40d376aca6dd8ee`.
Alternative Git/zlib versions may encode the same Git objects differently.
