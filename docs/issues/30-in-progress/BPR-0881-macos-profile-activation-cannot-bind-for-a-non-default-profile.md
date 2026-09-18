---
schema: "implement-rfc.card.v1"
id: "BPR-0881"
title: "macOS: profile activation can never bind for a non-default profile, because the socket path exceeds sun_path"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"found_by":"r5 whole-project critical review, Claude/fable sqlite/agent/qt partition, by arithmetic against the code's own limit","found_at":"20418bfe","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0881: macOS: profile activation can never bind for a non-default profile, because the socket path exceeds sun_path

## Defect

On macOS, opening a second copy of a non-default profile gives `owner_busy`
instead of activating the window that is already running. The handoff cannot
work, and it is arithmetic rather than a race:

`src/adapters/sqlite/src/profile_activation.cpp:89-93` refuses when
`(root / "activate.sock")` reaches `sizeof(sun_path)`, which is **104** on
Darwin. The app sets both organisation and application name to
"BetterPullReview" (`src/app/main.cpp:7608-7613`), so the base root is
`~/Library/Application Support/BetterPullReview/BetterPullReview`
(confirmed at `docs/development/build-and-test.md:2662`), and a non-legacy
profile root is `<base>/profiles/profile-N`
(`profile_storage_root.cpp:39-46`, `src/application/src/profile_registry.cpp:36-39`).

    7 + len(user) + 28 + 34 + 9 + 10 + 14 = 102 + len(user)

So **every username longer than one character overflows.**

The consequence is silent. `src/app/src/github_application.cpp:12578-12601`
calls `make_profile_activation_server`, gets `unavailable`, and skips it with
a bare `if (endpoint)`. Nothing is reported, so the feature simply does not
exist for these profiles.

The contract test passes because its fixtures assert "fixture socket path
fits" (`tests/sqlite_contract/profile_activation_tests.cpp:161`) — that is,
the test pins the one condition the real root cannot meet. This is the sort of
fixture that makes a green row actively misleading, and it is the reason the
defect survived.

**Reproduction:** on macOS, create a second profile, launch it, launch it
again. Expect the running window to come forward; observe owner-busy.

## Scope

- Give the runtime endpoint a short, owner-private location rather than
  placing it beside the data root — a `$TMPDIR`-style per-user runtime
  directory is the usual answer — while keeping the identity and ownership
  checks the current path relies on. The endpoint is a rendezvous, not durable
  state, so it does not have to live in Application Support.
- Do not solve it by shortening the profile directory names: that trades one
  arbitrary limit for another and breaks existing roots.
- Make the refusal **visible**. An activation endpoint that cannot bind is
  worth a diagnostic, not an `if (endpoint)`; had it reported, this would have
  been found the first time anyone made a second profile.
- Replace the fixture's "socket path fits" assertion with a realistic-length
  root, so the contract exercises the real geometry.

## Acceptance

- A second launch of a non-default macOS profile activates the running window.
- The contract uses a root of realistic length, and fails if the limit is
  exceeded again.
- An endpoint that cannot bind reports why, rather than being skipped
  silently.
- Linux and Windows activation are unaffected.

## Findings (dsp-006, run-20260916-stations)

### The arithmetic, confirmed on the machine

Measured, not restated. For the six-character user name this station runs as,
the non-legacy profile root
`/Users/<user>/Library/Application Support/BetterPullReview/BetterPullReview/profiles/profile-N`
is **94** bytes, so the in-root `activate.sock` is **108** bytes against Darwin's
`sun_path` of **104**: refused by four bytes, and refused for every user name of
two characters or more. The same 94-byte root also reaches exactly Linux's 108,
which the code refuses with `>=`, so the real macOS geometry would have failed on
Linux too. The contract now prints what it measured:

    profile-activation geometry: root 94 bytes, in-root endpoint 108 bytes,
    runtime endpoint 83 bytes, sun_path 104 bytes

### The endpoint moved out of the data root

`profile_activation.cpp` now resolves the rendezvous to a short per-user runtime
directory and names it by a digest, instead of placing it beside the data root:

- Directory: Linux's `/run/user/<uid>` first, then `$TMPDIR` (what macOS gives
  each user, `/var/folders/<a>/<b>/T/`, 49 bytes here), then an owner-private
  `bpr-<uid>` child of `/tmp` when the candidate is shared. Every candidate is
  verified owner-private (`S_ISDIR`, `st_uid == geteuid()`, mode exactly `0700`)
  rather than assumed; the created child is verified with `lstat`, so a planted
  symlink is refused rather than followed, and no existing directory's
  permissions are widened.
- Name: `bpr-activate-<16 hex>.sock`, the first eight bytes of `sha256` over the
  canonical root. Both launches derive the same name from the same root, which
  is what makes the rendezvous work without shared state.
- Result on macOS: **83** bytes, 20 bytes of margin inside 104. The endpoint's
  length no longer depends on the root's length at all.

Linux's path changed too, from `<profile root>/activate.sock` to
`/run/user/<uid>/bpr-activate-<digest>.sock`. That is deliberate: one rendezvous
location on both Unixes, in the directory freedesktop specifies for exactly this
(0700, per-user, discarded at logout), and it removes root-length sensitivity on
both platforms rather than leaving Linux one long user name away from the same
defect. Activation behaviour -- outcomes, ownership checks, framing, stale
recovery -- is unchanged on Linux, and Windows still returns `unavailable` with
no native endpoint at all.

`/run/user/<uid>` is probed directly rather than read from the environment
variable that names it: `cmake/VerifyArchitectureBoundaries.cmake` reserves that
variable's name to `src/adapters/github/src/gh_cli_transport.cpp`, and the
architecture-boundary gate fails any other file that spells it. Probing the
standard location and verifying it is owner-private reaches the same directory
without taking a name this adapter does not own, and a system that puts it
elsewhere falls through to `$TMPDIR`.

Every guarantee the old in-root path relied on is kept:
`detail::validate_private_storage_root(aRoot)` still gates resolution, so a
caller cannot rendezvous on a root it does not own; `private_socket`'s
`S_ISSOCK` + `st_uid == geteuid()` + `0600` check is untouched; `same_user`'s
`getpeereid`/`SO_PEERCRED` check is untouched; and the server's destructor still
removes only an entry whose `st_dev`/`st_ino` match what it bound, and only
while `validates_profile_runtime_lease` still holds for the root.

### The refusal is no longer silent

`src/app/src/github_application.cpp` reported nothing when
`make_profile_activation_server` returned `unavailable`. It now names the reason
-- `root-refused`, `runtime-directory-absent`, `endpoint-path-too-long`,
`platform-unsupported`, or `bind-refused` -- through `qWarning`, using the new
`profile_activation_endpoint` / `describe_profile_activation_endpoint_failure`
pair. Had that line existed, this defect would have announced itself the first
time anyone made a second profile.

### The fixture tests the real geometry

`tests/sqlite_contract/profile_activation_tests.cpp:161` asserted "fixture socket
path fits" -- the one condition the product could never meet. The fixture now
grows its scratch root to at least the real 94-byte profile-root length, and at
least the length at which an in-root endpoint stops fitting `sun_path`, and asks
the product where the endpoint is instead of assuming. A new `test_stale_endpoint`
course leaves a bound, owner-private, never-drained socket at the resolved
endpoint and proves the next launch still binds and hands off.

Falsifiability was checked rather than assumed: with `resolve_endpoint` forced
back to `aRoot / "activate.sock"`, the row fails with
`FAILED: a realistic-length root still resolves an endpoint`
(`build/r2-logs/dsp6-0881-sabotage.log`), and passes again on revert
(`dsp6-0881-restored.log`).

### Consequence outside this card's paths

`tests/end_to_end/profile_activation_contract.py:143` finds the endpoint with
`root.rglob("activate.sock")` and unlinks it to drive the `unavailable` course.
Nothing is under the data root any more, so that `assert endpoints` now fails.
The driver has to derive the endpoint the way the product does: the digest is
`hashlib.sha256(os.fsencode(str(profile_root))).hexdigest()[:16]`, the name is
`bpr-activate-<digest>.sock`, and the directory is the first of
`/run/user/<uid>`, `$TMPDIR`, `/tmp/bpr-<uid>` that contains it. The default
profile's root is the data root itself (legacy root mode) and the second
profile's is `<root>/profiles/profile-2`. Not fixed here: `tests/end_to_end/**`
belongs to another owner in this working tree.

## Pin reseal, with the review it records (dsp-006 integrator)

`src/app/src/github_application.cpp` is pinned three ways by
`tests/end_to_end/application_review_round_trip_composition_contract.cmake`,
so the six-line diagnostic above turns two of them red. Resealed here, in the
commit that records the review, per the procedure at the top of that file.

**Reviewed range: `2b2e5549..HEAD` plus this change, and it contains exactly
one thing.** The pins last matched in the `2b2e5549` merge.
`git log 2b2e5549..HEAD` over `src/app/main.cpp`, `src/app/src/` and the five
pinned `include/bpr/app/` headers is **empty**, and `git diff --stat -- src/app/`
is exactly `github_application.cpp | 6 ++++++`. So this reseal covers one
reviewed hunk and nothing else.

**The review.** The hunk adds an `else` branch to an existing
`if (endpoint)`. It performs no effect: it re-derives the endpoint through
`profile_activation_endpoint`, which is the same resolution the constructor
just attempted, and prints the reason through `qWarning`. It introduces no
request, credential, mutation or filesystem write on the success path, and it
cannot change whether the activation server is installed — `ActivationServer`
is assigned only in the unchanged `if` arm. The one side effect worth naming is
that `profile_activation_endpoint` may create the owner-private `bpr-<uid>`
directory when it is called purely to explain a failure; that is documented on
the declaration, and the directory is created 0700 and verified without
following links.

Compiled under the dev preset's full warning set
(`-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
-Wshadow-uncaptured-local -Werror`): clean, exit 0
(`build/r2-logs/dsp6-0881-app-syntax.log`). `qWarning` needed no new include,
and `describe_profile_activation_endpoint_failure` returns `const char*`, so
both ternary arms and the `%s` agree.

**New values**, printed by `-DBPR_PRINT_PINNED_DIGESTS=ON` at this tree:

- whole file `a5fbff44a1f2a2b46d019cac17aaa41f9dba7203e589787b2fc22b7aff2642bc`
- manifest `2fc5aea26c8c13863494678204830e547e61b6f352777717dc5391a648641cb6`
- mutation branch `a7e2081e…`, **unchanged**, as it has been since it was
  first pinned. The tests-disabled production branch of
  `compose_review_mutation` is untouched by this card.

A second reseal is owed when `bpr-win`'s [[BPR-0875]] instrumentation merges,
which edits the same file and `src/profile_desktop.cpp`. It gets its own
recorded review rather than riding this one.

## The end-to-end driver had to move with the product

`tests/end_to_end/profile_activation_contract.py` found the endpoint with
`root.rglob("activate.sock")` and unlinked it to drive the `unavailable`
course. Nothing lives under the data root any more, so that assertion would
have failed the row — the worker found this and correctly reported it rather
than reaching outside its slice.

The driver now derives the endpoint the way the product does
(`owner_private_directory`, `runtime_directory`, `activation_endpoint`),
mirroring `profile_activation.cpp`'s candidate order and its
`lexically_normal()` digest — **neither side resolves symlinks, so both sides
must decline to**. The course's own root is already `Path(temporary).resolve()`
at the point it is handed to the product, so the two agree.

A wrong derivation now fails by naming both the path it expected and the
`bpr-activate-*.sock` files actually present, rather than silently finding
nothing to unlink.

## The real desktop course passes, so the first acceptance line is a run

`application_profile_activation_public_path` — **Passed, 5.02 s**, on the dev
preset at `68f8152e` (`build/r2-logs/dsp6-0879-test-activation.log`; run by the
BPR-0879 worker, which already had the app built, rather than paying for a
second app build).

That course drives the real desktop application, not a fixture. It starts an
owner on **profile 2**, a non-default profile, then launches profile 2 again
and waits for `ACTIVATION_QUEUED` while asserting the durable window rows do
not change and that profile 1's do not either. So the card's first acceptance
line — "a second launch of a non-default macOS profile activates the running
window" — is now met by a real second launch rather than by argument.

Two halves, and it is worth being exact about which proves what:

- the **course** proves the handoff works, on a short `/tmp` root. That root is
  why the course passed before this card existed: it never reached the limit.
- the **contract** proves the geometry, on the real 94-byte root, and a forced
  revert to `aRoot / "activate.sock"` fails it
  (`build/r2-logs/dsp6-0881-sabotage.log`).

Neither alone would be enough. Together they cover the defect and the fix.

Still open on this card, which is why it stays In Progress: **Linux**.
`bpr-linux` has measured its endpoint directory — `/run/user/1000`, mode 0700,
owner `mmarcin`, with `XDG_RUNTIME_DIR` set and systemd as PID 1, giving a
45-byte endpoint against the 108 limit — but has not yet run
`sqlite_profile_activation` or the end-to-end course there. Since this card
deliberately moved Linux's path on a platform whose suite was 367/367, that run
is a criterion, not a courtesy.

Noted for whoever reads this next: that station also cannot measure the
fall-through candidates. With `TMPDIR` unset and `/tmp` at 1777 root-owned, its
first candidate verifies immediately, so `$TMPDIR` and the product-created
`/tmp/bpr-<uid>` branches stay untested on Linux by construction. A WSL2 host
with systemd disabled is the configuration that would exercise them.

## The Linux criterion is closed, and it was a criterion worth running (bpr-linux, dsp-007)

Base `271bb452`. Both rows pass on the first attempt, `ctest` exit **0**
(`build/r2-logs/dsp7-0881-activation.log`, verbose in
`dsp7-0881-activation-verbose.log`):

| Row (resolved against `ctest -N`, not prose) | Result |
| --- | --- |
| `sqlite_profile_activation` (#147) | **Passed, 4.13 s** |
| `application_profile_activation_public_path` (#362) | **Passed, 2.79 s** (2.75 s on the verbose rerun) |

### The geometry, printed by the contract on this host

```text
profile-activation geometry: root 94 bytes, in-root endpoint 108 bytes, runtime endpoint 49 bytes, sun_path 108 bytes
```

**The in-root endpoint is 108 against a `sun_path` of 108, and Linux refuses
with `>=`.** So the real macOS root geometry would have failed on Linux too,
exactly as this card predicted from the Mac — measured here rather than argued.
Moving the endpoint out of the data root did not merely tidy Linux, it removed a
refusal Linux was one long user name away from.

The runtime endpoint is **49** bytes, not the 45 estimated above from the host
survey; 49 is the correct figure and it is a constant:

```text
/run/user/1000/bpr-activate-<16 hex>.sock
```

14 + 1 + 13 + 16 + 5 = 49, with **59 bytes of margin** inside 108, and it does
not vary with the root's length. Two worked examples through the driver's own
`activation_endpoint`, one legacy-root and one `profiles/profile-2`:
`…/bpr-activate-b2a75921f1ffa0ee.sock` and `…/bpr-activate-1bdaba24724cfcdb.sock`
— different digests, both 49 bytes.

### The Python mirror of `lexically_normal()` agrees on Linux

`application_profile_activation_public_path` now derives the endpoint the way
the product does instead of globbing for `activate.sock`. A wrong derivation
fails the row naming both the expected path and the `bpr-activate-*.sock` files
actually present. **No such message was produced**: the row printed
`PROFILE_ACTIVATION_OK` and passed. So the C++ digest over
`aRoot.lexically_normal().string()` and the Python digest over
`os.fsencode(str(profile_root))` agree on this platform, and neither side
resolved symlinks.

### The candidate order resolved as the host survey predicted

`runtime_directory()` returns `/run/user/1000` — the first candidate, verified
owner-private (mode `0700`, uid 1000) rather than read from the environment.
`TMPDIR` is unset and `/tmp` is 1777 root-owned here, so the two fall-through
branches remain untested on Linux by construction, as recorded above. Both
rows left **no** `bpr-activate-*.sock` behind in `/run/user/1000`, so the
server's destructor removed what it bound.

### What the tree still says `activate.sock`

Grepped and **not fixed**, as instructed. Outside `docs/issues/**` — which is
history and should keep saying it — there is exactly one reference left, and it
is deliberate:

- `tests/sqlite_contract/profile_activation_tests.cpp:38,42` —
  `constexpr std::string_view legacy_endpoint_name{ "/activate.sock" }`, used to
  compute the "in-root endpoint 108 bytes" figure above. It is the arithmetic
  the contract exists to print, not a leftover caller.

**No product source refers to `activate.sock` any more.** The only other hits
are card and RFC prose in `docs/issues/`.

### Disposition

Against this card's four acceptance lines, the fourth's Linux half is now met by
a run rather than by argument, and the station has no remaining Linux work on
it. The card is left **In Progress** for `bpr-main-mac` to dispose of: moving it
is a main-side judgment, and a lane does not move a card whose other three lines
were met on another platform.
