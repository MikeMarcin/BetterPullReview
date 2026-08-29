---
id: BPR-0318
title: Record a distributable device-authorization identity
status: Done
rfc: RFC-BPR-0016
depends_on: [BPR-0276, BPR-0282, BPR-0480]
---

# Record a distributable device-authorization identity

## Required final state

- The private, owner-only `BPR-Test` application remains a bounded verification
  identity and is never silently widened, reused, or represented as the client
  identity for a generally distributed BetterPullReview build.
- Before any production platform becomes available, the product owner gives
  plain direction to proceed with a separate distributable GitHub.com
  application identity. If that direction is deferred, every production
  platform remains visibly `device_flow_unavailable` with zero requests and
  RFC-BPR-0016 remains open.
- The reversible go/no-go choice uses that plain direction only. If the owner
  proceeds, this card creates one bounded
  `rfc0016-production-app-registration-v001` operator packet because the next
  action registers an external production identity and authorizes exact real-
  service traffic. Its validated claims authorize only the named settings,
  permissions, designated read-only repository, and evidence boundary; they
  authorize no mutation, release, distribution, GHES change, or secret-bearing
  evidence.
- The distributable identity is recorded with its owner, public client
  identifier, public-installation policy, device-flow and expiring-token
  settings, exact repository permissions, selected test installation, and
  absence of a shipped client secret. Registration or real-service traffic is
  performed only under separately recorded bounded authority.
- A designated read-only course proves that the exact production client can
  authorize against `MikeMarcin/BPR-Test`, reach the existing operation
  catalog with the approved permissions, and retain no secret-bearing
  evidence. No repository mutation, release, or distribution occurs in this
  card.
- Only after the identity and every BPR-0282 macOS native/security row pass
  does this card own the macOS production capability-selection flip and the
  complete clean Release/public/native/hygiene rerun against the enabled
  artifact. Failure or deferral leaves macOS unavailable and this card
  nonterminal.
- BPR-0310 and BPR-0311 consume this same recorded production identity before
  their independent target-native Linux and Windows enablement courses.

## Verification contract

- This card owns the packet, response, claims contract, and committed
  validation record under
  `docs/verification/reviews/rfc0016-production-app-registration-v001/`.
  Replay uses the checked-in attended-review validator with
  `--validate-existing` and the committed record identity.
- This card creates `scripts/verify_rfc0016_production_identity.py` and its
  isolated tests. The verifier accepts only the exact packet validation,
  sanitized registration observation, route-closed token-success/read-only
  course, expected public client selection, and secret-free artifact
  inventory; it has a no-traffic `--validate-existing` mode for closeout.
- After the identity evidence passes, the exact macOS sequence is the BPR-0282
  unavailable course, the single macOS selection change, and the enabled
  course from `docs/verification/RFC-BPR-0016.md`. The selection change and
  both outputs are committed as distinct provenance before this card becomes
  terminal.

## Parked on the proceed/defer decision — 2026-08-25

Both dependencies are done: [[BPR-0276]]'s attended capture was admitted on
2026-08-24 and [[BPR-0282]]'s macOS native and security course passed on
2026-08-25 with production selection still `device_flow_unavailable` and zero
requests on every platform. Nothing is queued in front of this card and no
unattended work remains in it.

What it waits on is one plain sentence from the product owner:

> Proceed with registering a separate distributable GitHub.com application
> identity for BetterPullReview, or defer.

That is deliberately **not** a digest-bound packet. The choice is reversible
and changes nothing on its own — deferring leaves every platform visibly
`device_flow_unavailable` with zero requests, which is exactly where the tree
already is, and RFC-BPR-0016 simply stays open. A packet binds exact identity
to an authority, and there is no identity to bind until the answer is
"proceed".

If the answer is proceed, this card then creates the bounded
`rfc0016-production-app-registration-v001` operator packet, because the next
action after that registers an external production identity and authorizes
exact real-service traffic — which is precisely the kind of decision that does
need exact identity behind it. `docs/verification/reviews/rfc0016-production-app-registration-v001/`
currently holds only `registration-observation.json`; the packet, response,
claims contract, and validation record are this card's to write.

Nothing downstream can move first. [[BPR-0310]] and [[BPR-0311]] consume the
same recorded identity before their own target-native enablement, and
[[BPR-0285]] cannot replay an enabled final-head course on a build that has no
identity to be enabled with.

## Proceed, and the accepted registration authority — 2026-08-27

The owner answered the plain sentence in the attended session: **proceed**.
The card then wrote the bounded packet this decision called for.
`rfc0016-production-app-registration-v001/operator-packet.md` binds decision
subject `18b867d3a05321cedcd4570227c732459779bebff23728d354de9411c4e5370b`
(nine facts: a second any-account GitHub App owned by `MikeMarcin`, device
flow and expiring tokens on, exactly Metadata/Contents/Checks read plus Pull
requests read-write, no shipped secret, one designated read-only course
against `MikeMarcin/BPR-Test`, the stop rule), and the owner returned the
exact approval statement in the same session. The validation record was
generated at the family's commit and replays:
`OPERATOR_RESPONSE_VALID:rfc0016-production-app-registration-v001`, record
commit `c82e51c2c933887632fb0bb35db876375a151de9`. The ledger's
distributable-identity-authority row is Passed on that evidence.

What runs next under this recorded authority, in order: the owner performs
the registration itself on GitHub exactly per the facts (attended); the
sanitized production observation replaces this directory's seeded
verification-shape template and must show a client identifier distinct from
`Iv23lil08fH3933w9MkW` with any-account installability; this card then
authors `scripts/verify_rfc0016_production_identity.py` and its isolated
tests, runs the designated read-only course, and only after every BPR-0282
row and the identity evidence pass does it reach the macOS
capability-selection flip and the complete clean rerun.

## The distributable identity exists — 2026-08-27

The owner registered the app in the same attended session, exactly per the
facts. One form obstacle arose and resolved inside the approved shape:
GitHub's "at least one callback URL is required" message was the
"Request user authorization (OAuth) during installation" checkbox, and
unchecking it cleared the requirement — the callback ended blank, matching
the verification precedent (`callback_url_present: false`).

The sanitized production observation now replaces the seeded
verification-shape template in this directory, recorded from the owner's
attended read-back: public client **`Iv23liitiYN1WSSbag50`** — distinct
from the verification identity, as fact 8 requires — any-account
installability, device flow enabled, expiring user tokens, webhook
inactive, the exact four-permission set with automatic Metadata read, no
client secret and no private key generated, and the course installation
selected-repositories on `MikeMarcin/BPR-Test` alone. The packet's
decision-time table intentionally still binds the seeded template's digest;
fact 8 is what authorized this replacement.

Remaining on this card, in order: author
`scripts/verify_rfc0016_production_identity.py` with its isolated tests
(unattended); one more attended sitting for the designated read-only
course — real device-flow token success for the new client plus the
read-only catalog probes, evidence under `read-only-course/`; then, on the
macOS host only, the BPR-0282 unavailable-course rerun, the one-time macOS
capability-selection flip, and the complete clean enabled rerun.

## The verifier exists, and it defines the course — 2026-08-29

`scripts/verify_rfc0016_production_identity.py` and
`tests/scripts/verify_rfc0016_production_identity_tests.py` are written, and
the isolated gate is `rfc0016_production_identity_security`, registered in
`tests/CMakeLists.txt`, counted by `scripts/verify_rfc0016_test_inventory.py`
in both `dev` and `core-only`, and named by the ledger row it closes.

The identity half passes at this head. `RFC0016_PRODUCTION_IDENTITY_OK` over
the committed authority, the sanitized observation, and the secret-free
evidence, reporting `RFC0016_PRODUCTION_CLIENT:Iv23liitiYN1WSSbag50` and
`RFC0016_PRODUCTION_RECORD_COMMIT:c82e51c2c933887632fb0bb35db876375a151de9`,
with `--validate-existing` passing as well. `--require-complete` fails closed
on the absent `read-only-course/`, which is exactly the attended sitting that
has not happened.

A gate is only worth its negative cases, so the whole verifier was swept rather
than spot-checked: each of its 100 refusal conditions was neutralized in turn
and the suite rerun. 79 turn the 111-test gate red. All 21 survivors are
shape or existence guards — `isinstance` checks and `is_dir`/`is_file`
probes — whose removal changes which diagnostic is printed and not whether the
run refuses, because the code behind each still fails; they are left as they are
rather than padded with tests that would assert the wording of an error.

The sweep paid for itself three times. A mutation test that looked like it
covered the execution record was really being caught by the probe provenance
beside it; one hand-written anchor never applied at all, so its "pass" meant
nothing; and `check_committed` had a real hole, where a failing `git status`
leaves empty output that read as a clean tree. All three now have isolating
cases, and the authority replay is covered against a throwaway repository so
that the record is demonstrably replayed rather than believed for saying
`pass`.

The verifier also settles something this card had left open. `read-only-course/`
does not exist, so nothing described its shape; the verifier is now that
description, and the attended sitting must produce it: `registration.json`,
`course-execution.json` carrying `gate`, `authority_subject_sha256`, and the
digest of the registration bytes beside it, `outcomes/device_code_success.json`
and `outcomes/token_success.json` with `authorization_pending` permitted but not
required — requiring an outcome that depends on poll timing would be a gate that
fails on correct evidence — and `route-probes.json` over the exact ten-operation
read-only catalog. Every provenance block binds decision subject
`18b867d3a05321cedcd4570227c732459779bebff23728d354de9411c4e5370b`, so course
evidence produced under any other authority fails rather than reads plausibly.

## The sitting needs a runner that does not exist yet — 2026-08-29

The attended sitting cannot be scheduled against the tools in the tree. A read-
only survey of `scripts/run_rfc0016_device_capture_attended.py` and its offline
module `scripts/capture_rfc0016_device_contracts.py` found four independent
refusals standing between them and a production course, not the one pinned
constant this card had assumed:

- `validate_client_id` (runner:687) admits only `Iv23lil08fH3933w9MkW`.
- `load_registration_observation` (runner:1469) rejects any observation that is
  not byte-identical to the hardcoded private shape, which fixes
  `application_visibility: private` and `installation_policy: owner_only` — the
  exact two fields the distributable identity must invert. This fires before the
  client check.
- `make_provenance`/`_validate_provenance` (offline:419) hardcode BPR-0276's
  `authority_subject_sha256` and emit no `gate` key at all, and
  `validate_registration_artifact` (offline:807) again pins private/owner_only.
- `_course_execution_document` (runner:1496) likewise writes BPR-0276's
  authority and no `gate`.

The transport half is clean: `HttpsWire`, `ReadOnlyProbeWire`, `_probe_requests`,
`run_read_only_probes`, `request_device_grant`, `poll_token`, and
`run_authorization_course` all take `client_id`, `wire`, and `probe` as ordinary
parameters, keep route closure in `ALLOWED_URLS`, and read no pinned identity
except through `validate_client_id`. `_write_private` and
`_read_private_canonical_json` are generic. The single `token-success` course is
a closed branch that demands no slow-down reservation, no settings transition,
and no five-course bundle.

That asymmetry decides the shape. Widening the existing evidence writer means
threading a second registration shape, a second authority, and a new key through
two files whose own negative tests assert BPR-0276's shape is exact — which is
the "never silently widened" non-goal at the top of this card, approached from
the inside. The next slice is instead a separate bounded production-course runner
that imports the wire, probe, and course helpers and writes this gate's
provenance, leaving BPR-0276's admitted evidence and its authority untouched.
That work is [[BPR-0480]]; the attended sitting follows it, and nothing else on
this card can move first.

The macOS enablement step also gains a check it did not have. The platform
course proves the shipped artifact does *not* carry the private verification
identity; nothing proved it carries the right one. `--release-binary` on this
verifier requires the enabled artifact to select `Iv23liitiYN1WSSbag50` and not
the verification client, and the ledger row now runs it after the flip.
## Every unattended thing is done; the sitting is the card — 2026-08-29

[[BPR-0480]] is Done: `scripts/run_rfc0016_production_course_attended.py` runs
the designated read-only course as `Iv23liitiYN1WSSbag50` and publishes evidence
`verify_rfc0016_production_identity.py` admits, proving that on every run by
self-checking its staged output before it publishes. Nothing unattended remains
on this card either, so it parks on the one thing left.

What the sitting is. On a macOS host, with the owner present:

```sh
python3 scripts/run_rfc0016_production_course_attended.py \
    --registration-observation docs/verification/reviews/rfc0016-production-app-registration-v001/registration-observation.json \
    --output-root docs/verification/reviews/rfc0016-production-app-registration-v001/read-only-course
```

It opens `https://github.com/login/device`, shows a user code to type, waits for
the authorization, then runs the ten read-only probes against
`MikeMarcin/BPR-Test` pull request 1. It mutates nothing. It needs no new
approval: fact_4 of the accepted `rfc0016-production-app-registration-v001`
decision authorizes exactly this course, and the runner refuses to start unless
that record is committed and still replays.

Then, unattended again and in this order: commit the course evidence; run the
ledger's `--require-complete` admission; rerun the BPR-0282 unavailable course;
make the macOS-only capability-selection change in
`src/app/src/github_application.cpp`, which today returns `{ .Registered = false }`
unconditionally and must become platform-conditional rather than simply true;
rerun the enabled course and the verifier with `--release-binary`.

### The enablement check is wired, and correctly says "not yet"

Run against the current tests-disabled Release artifact
(`build/app-release/.../BetterPullReview`, built 2026-08-25, before any flip):

```
rfc0016-production-identity:fail:release-binary:does-not-select-the-recorded-production-client
```

That is the right answer today and the evidence that the check is real rather
than synthetic. The artifact selects no production client because
`shipped_device_authorization_capability()` still returns `{ .Registered = false }`
for every platform. After the macOS-only change, the same command against the
rebuilt artifact must pass, which is what closes the ledger row's second half.

## The sitting happened — 2026-08-29

The owner ran the designated read-only course at the macOS integrator
station. First launch was lost to a missed dialog (killed unauthorized,
no evidence written, no output root created); the second grant completed:
`RFC0016_PRODUCTION_COURSE_OK` as client `Iv23liitiYN1WSSbag50` with all
ten probes, evidence committed under `read-only-course/` with field-shape
records only — the secret scan over the evidence found no token values.

The complete admission then passed at the evidence commit:
`RFC0016_PRODUCTION_IDENTITY_OK` with `RFC0016_PRODUCTION_COURSE:complete`,
`RFC0016_PRODUCTION_EVIDENCE_FILES:11`, record commit
`c82e51c2c933887632fb0bb35db876375a151de9` replayed rather than believed.

The BPR-0282 unavailable course rerun passed at the same head on a fresh
`build/rfc0016-macos-unavailable` root: `RFC0016_PLATFORM_COURSE_OK`,
`RFC0016_COURSE_SELECTORS:16` (the count grew from 15 when the two
production-identity gates were registered), `RFC0016_DEVICE_STAGE:unavailable`,
`RFC0016_DEVICE_REQUESTS:0`. Remaining, in order: the macOS-only
capability-selection flip and the enabled-course rerun with
`--validate-existing --release-binary` against the rebuilt artifact,
committed as distinct provenance.

## The macOS flip — 2026-08-29

`shipped_device_authorization_capability()` is platform-conditional:
`Registered = true` on `__APPLE__` only. The flip is not the constant alone,
because three gates define what "enabled" must mean:

- The composition now builds the real credential-free native HTTPS wire and
  the GitHub device-authorization transport bound to the embedded
  `Iv23liitiYN1WSSbag50` on registered platforms, failing closed to the
  refusing transport when composition fails. The refusing transport still
  backs every unregistered platform, and its `reached()` counter keeps
  meaning "gate bypassed" everywhere.
- The self-test probe cancels a real flow at `awaiting_authorization`
  (fixture registered course exempt), because that stage is non-terminal and
  the probe's budget cannot contain a human answer. The probe's verdict is
  now capability-aware: `DEVICE_FLOW_REGISTERED_SELECTION_OK` mirrors
  `DEVICE_FLOW_UNAVAILABLE_OK`, chosen by the new
  `DeviceAuthorizationCapabilityRegistered` status fact.
- The two dev-tree selectors asserting the unavailable contract are
  platform-parametrized (`bpr_device_selection_self_test_expression`,
  the `bpr_native_platform_vault_self_test_result` pattern): on macOS the
  network-isolated expectation is `DEVICE_FLOW_STAGE:failed` with
  `DEVICE_FLOW_REQUESTS:0`, deterministic because sandbox-exec denies the
  connect immediately. Selector names stay — the platform-course inventory
  binds them — which leaves `application_device_authorization_unavailable`
  proving registered-selection isolation on macOS under its historical
  name; recorded here for [[BPR-0285]] to adjudicate.

`test_require_complete_fails_until_the_course_exists` encoded the
pre-sitting tree (live course absent → refusal); the live course now exists
and passes, so the fail-closed claim is proven against an absent course root
instead, renamed `test_require_complete_fails_when_the_course_is_absent`.

Focused evidence at the flip head: dev build clean; all 16 platform-course
selectors pass (`flip-course-selectors` run, 100%); both
`rfc0016_production_*_security` gates pass; `RFC0016_TEST_INVENTORY_OK`.
The enabled-course rerun and `--validate-existing --release-binary`
admission follow as their own recorded step.

## The wire had never met github.com — 2026-08-29

The first enabled-course run failed twice, and both failures were gates
meeting the first live traffic this surface has ever produced:

1. The release-artifact scan refused "client identifier-shaped material"
   in the shipped binary — its own comment prescribed moving the recorded
   client to the known-shipped list once this card landed. The scan now
   permits exactly `Iv23liitiYN1WSSbag50`; a foreign identifier beside the
   permitted one still refuses, as do token shapes (proven by direct
   positive/negative payload runs).
2. With that fixed, the shipped probe's real grant terminated `failed`
   with `response_too_large`. Instrumented diagnosis (temporary stderr
   prints, removed before commit) traced it to the wire's header-value
   bound refusing `\n`: Qt merges repeated response headers into one
   rawHeaderPairs value joined with `\n`, and github.com's login endpoints
   send paired `Set-Cookie` headers. The BPR-0276/0480 evidence came from
   Python runners and the contract fixtures speak HTTP/1.1 through a
   controlled proxy with single cookies, so no prior gate could have seen
   this. The bound now permits the join byte and still refuses NUL and
   `\r`, which never have a legitimate source; wire contract and fixture
   suites plus all 16 course selectors pass unchanged.

After the repair the shipped dev probe completes the honest live
sequence: real grant as `Iv23liitiYN1WSSbag50`, `awaiting_authorization`,
probe cancel, `DEVICE_FLOW_STAGE:cancelled`,
`DEVICE_FLOW_REGISTERED_SELECTION_OK`, exit 0.

## The third unregistered-era gate — 2026-08-29

The enabled course passed with `RFC0016_DEVICE_STAGE:failed`, and the course
accepts that (refusal gone, flow started). Direct probing of the enabled
Release artifact from a network-capable shell refused to accept it: the dev
binary completed the live grant (`cancelled`) while the Release binary never
opened a socket. The cause was `credential_free()` in the native wire,
hardwired `false` outside the test graph on the stated theory that "no
production factory can construct" a credential-free binding — false since
`make_native_https_credential_free_transport` became a production factory,
and fatal to a shipped device flow, whose `credential_matches` demands
exactly that binding. The accessor is now truthful in every build, and the
authorization-header omission rides the same predicate unconditionally, so
a shipped device request carries no bogus empty bearer header. Every
consuming refusal keeps its meaning; the smart surface's
`!credential_free()` term becomes strictly tighter in Release. All 18
isolated selectors and 3 loopback wire/smart suites pass; the app-release
probe now completes the live sequence: real grant as
`Iv23liitiYN1WSSbag50`, `awaiting_authorization`, probe cancel,
`DEVICE_FLOW_STAGE:cancelled`, `DEVICE_FLOW_REGISTERED_SELECTION_OK`,
`PUBLIC_PATH_OK`.

## Terminal — 2026-08-29

Every required final state holds at the closing head:

- The private `BPR-Test` verification identity is untouched and still refused
  in shipped artifacts; the distributable identity is recorded, admitted, and
  course-proven under the accepted registration authority.
- The designated read-only course ran attended, mutated nothing, and its
  committed evidence replays: `RFC0016_PRODUCTION_IDENTITY_OK` with
  `RFC0016_PRODUCTION_COURSE:complete` under `--validate-existing`.
- The BPR-0282 unavailable course passed clean pre-flip; the one-time macOS
  capability-selection flip landed with the real credential-free wire; the
  fresh enabled course passed at the final head with a genuine live grant
  (`RFC0016_DEVICE_STAGE:cancelled`) and the release-binary admission proves
  the artifact selects `Iv23liitiYN1WSSbag50` and not the verification
  client. Linux and Windows remain visibly unavailable with zero requests;
  their flips stay with [[BPR-0310]] and [[BPR-0311]], both of which this
  card's recorded identity now unblocks.
- The ledger's distributable-identity row and the macOS platform row in
  `docs/verification/RFC-BPR-0016.md` record the evidence; the runner froze
  as digest-bound evidence when the course committed, exactly as
  [[BPR-0481]]'s self-arming test announces.

### The sitting freezes the runner

Worth knowing before the command is run rather than after: the verifier requires
a course to name the digest of
`scripts/run_rfc0016_production_course_attended.py`, so the moment that course
is committed, that file becomes evidence exactly as [[BPR-0276]]'s runner already
is. A later edit to it — a comment, a lint fix — invalidates the recorded course.
[[BPR-0481]] added a self-arming test that fails with that explanation as soon as
a course exists, so the constraint announces itself; this note is so the operator
is not surprised by it.
