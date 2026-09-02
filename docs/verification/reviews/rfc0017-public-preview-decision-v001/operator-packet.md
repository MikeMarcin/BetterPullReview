# RFC-BPR-0017 public-preview definitions operator packet

Status: **pending owner verdict; not RFC-BPR-0017 closeout, not implementation
authorization, and not credential, mutation, private-read, release, or
platform-enablement approval**

This packet carries one bounded decision to the product owner: bind the four
definitions that [[BPR-0299]]'s accepting verdict left owed. It reopens
nothing. The build decision is already taken and recorded, and this packet
states it as a fact rather than asking it again.

It authorizes no request, no credential, no private repository, no mutation,
no release, and no platform flip. It does not start any RFC-BPR-0017
implementation card; [[BPR-0312]] is the gate those cards depend on, and this
packet is that gate's question.

## The decision already taken

On 2026-09-01 the product owner chose to build anonymous public preview as a
credential-optional profile on the existing read path — no second read path,
normalizer, fixture set, or provider. It was recorded on the
`run-boundary-decisions-v001` page, batch
`bpr:/attended-review/betterpullreview/c7e77830ef4fa92c6047c99b`, subject
`decision:/betterpullreview/bpr-0299/build-credential-optional-preview`
accepted with `do-not-build` recorded as not chosen. That verdict and its
validation record are bound by digest below.

That acceptance carried no note. The subject had asked for one, defining the
public-preview name, how the limits and the missing capabilities are
presented, the sign-in handoff, and the supported-platform policy. Those four
definitions are what this packet binds, and nothing else.

## The measured facts the definitions rest on

These are recorded, sanitized captures of the real service, not assumptions.
Every file behind them is bound by digest below.

| Fact | Where it was measured |
|---|---|
| Unauthenticated REST core budget is 60 requests per hour per source address, shared by every client behind that address | `captures/rate-limit.json` |
| A conditional GET answered `304 Not Modified` still consumed one core unit (`used` 3 → 4) | `rfc0017-anonymous-probe-v005` fresh/conditional pair |
| GraphQL refuses anonymous callers with `403` and `x-ratelimit-limit: 0` | `captures/graphql-refusal.json` |
| REST pull-request review comments answer `200` anonymously, but no read-path operation calls that source today | `captures/pull-request-comments.json` |
| A private or missing repository answers `404`, so absent and private are indistinguishable | `captures/absent-not-disclosed.json` |
| GHES in private mode rejects all unauthenticated access, so the mode is GitHub.com-only | RFC-BPR-0017 findings |

## The four definitions, as proposed

Each is stated below as the question it answers and the exact text proposed to
bind. Accepting binds all four as written; replacing one leaves the other
three as proposed.

### 1. Public-preview name — `public_preview_name`

**Question.** What is the mode called wherever the product names it?

**Proposed answer.** “Public preview”, with “signed out” stated beside it
wherever the mode is named — the profile selector, the window title, and the
workbench header all read “Public preview — signed out”. It is never presented
as the authenticated workbench with fields missing.

### 2. Limit presentation — `limit_presentation`

**Question.** How are the shared hourly budget, and the things the preview
cannot do, presented?

**Proposed answer.** Not a persistent bar. A small ring indicator in the
workbench fills as the shared hourly core budget is consumed, in the manner of
Claude Desktop’s usage ring. Hovering it shows a tooltip carrying the consumed
fraction and the reset time — “32% of the hourly limit · Resets in 3 hr 4 min”
— and, in that same tooltip, what the preview cannot do: GitHub.com only, no
review threads, no triage or publishing. A conditional request answered 304
spends one unit like any other, so the ring moves on refresh. Budget
exhaustion is a typed refusal naming the limit and the reset time, never a
network or authentication error.

### 3. Sign-in handoff — `sign_in_handoff`

**Question.** How does the preview hand off to signing in?

**Proposed answer.** The RFC-BPR-0016 device authorization flow is offered
from the ring’s tooltip and from every refusal — budget exhausted, review
threads unavailable, and any triage or publishing attempt — and returns to the
same pull request after sign-in.

### 4. Supported-platform policy — `supported_platform_policy`

**Question.** Which platforms and profiles does the preview support?

**Proposed answer.** All three desktop platforms — macOS, Linux, and Windows
x64 — because the preview needs no credential store. GitHub.com public
repositories only; every GHES profile stays unavailable and sends zero
requests. The sign-in handoff is shown only where device authorization is
enabled on that platform: macOS and Linux today, since BPR-0310 is terminal,
and Windows once BPR-0311 — now ready on the Windows lane — is terminal too.

## The bound block

This is the exact block `scripts/validate_rfc0017_public_preview_decision.py`
parses. Its bytes are what the four definitions mean, and the batch and
subject identity below is what the returned verdict must answer. Editing any
character of it changes the question.

```json
{
  "schema_name": "bpr.rfc0017.public_preview_definitions",
  "schema_version": 1,
  "decision": "build",
  "batch_id": "bpr:/attended-review/betterpullreview/479439050b23e8fe4415ce6a",
  "subject_id": "decision:/betterpullreview/bpr-0312/public-preview-definitions",
  "subject_digest": "sha256:657e5459996d7e8adc4a64197072da9b624ebdabb592961ee58f16c418a5d5af",
  "definitions": [
    {
      "key": "public_preview_name",
      "term": "Public-preview name",
      "question": "What is the mode called wherever the product names it?",
      "proposed": "“Public preview”, with “signed out” stated beside it wherever the mode is named — the profile selector, the window title, and the workbench header all read “Public preview — signed out”. It is never presented as the authenticated workbench with fields missing."
    },
    {
      "key": "limit_presentation",
      "term": "Limit presentation",
      "question": "How are the shared hourly budget, and the things the preview cannot do, presented?",
      "proposed": "Not a persistent bar. A small ring indicator in the workbench fills as the shared hourly core budget is consumed, in the manner of Claude Desktop’s usage ring. Hovering it shows a tooltip carrying the consumed fraction and the reset time — “32% of the hourly limit · Resets in 3 hr 4 min” — and, in that same tooltip, what the preview cannot do: GitHub.com only, no review threads, no triage or publishing. A conditional request answered 304 spends one unit like any other, so the ring moves on refresh. Budget exhaustion is a typed refusal naming the limit and the reset time, never a network or authentication error."
    },
    {
      "key": "sign_in_handoff",
      "term": "Sign-in handoff",
      "question": "How does the preview hand off to signing in?",
      "proposed": "The RFC-BPR-0016 device authorization flow is offered from the ring’s tooltip and from every refusal — budget exhausted, review threads unavailable, and any triage or publishing attempt — and returns to the same pull request after sign-in."
    },
    {
      "key": "supported_platform_policy",
      "term": "Supported-platform policy",
      "question": "Which platforms and profiles does the preview support?",
      "proposed": "All three desktop platforms — macOS, Linux, and Windows x64 — because the preview needs no credential store. GitHub.com public repositories only; every GHES profile stays unavailable and sends zero requests. The sign-in handoff is shown only where device authorization is enabled on that platform: macOS and Linux today, since BPR-0310 is terminal, and Windows once BPR-0311 — now ready on the Windows lane — is terminal too."
    }
  ],
  "probe_evidence": [
    {
      "path": "docs/verification/reviews/rfc0017-anonymous-probe-v001/captures/rate-limit.json",
      "sha256": "sha256:9b52844edae8de5cd728ed59a7f205a0b432a7c1418953d8d94bc1e3adc41e76",
      "byte_length": 1119
    },
    {
      "path": "docs/verification/reviews/rfc0017-anonymous-probe-v001/captures/graphql-refusal.json",
      "sha256": "sha256:67def7232c8fff7413238dca7181879539dda26cdc42cd0f832eb90e112094bd",
      "byte_length": 986
    },
    {
      "path": "docs/verification/reviews/rfc0017-anonymous-probe-v001/captures/pull-request.json",
      "sha256": "sha256:0f0737d8572f16cb77cd6a7d6c8734cd69adf15d37de73567e83d7b33316445f",
      "byte_length": 17077
    },
    {
      "path": "docs/verification/reviews/rfc0017-anonymous-probe-v001/captures/pull-request-comments.json",
      "sha256": "sha256:dd16dd6948458db8bc12c20e4882a4d78d93952ae0df4cd08c011898a5ddae7d",
      "byte_length": 3402
    },
    {
      "path": "docs/verification/reviews/rfc0017-anonymous-probe-v001/captures/absent-not-disclosed.json",
      "sha256": "sha256:2d7e0b9088acd8c6763ab541b56ac6ea8f69676d6d7bc56dd076e664f05ddc0e",
      "byte_length": 864
    },
    {
      "path": "docs/verification/reviews/rfc0017-anonymous-probe-v001/capture-bindings.json",
      "sha256": "sha256:a54477d9f4181a8c088db5ed530c4eef5eb1c09976bb9e0edd8df31ab893baab",
      "byte_length": 1986
    },
    {
      "path": "docs/verification/reviews/rfc0017-anonymous-probe-v005/captures/pull-request-fresh.json",
      "sha256": "sha256:7f99c19116ccc892a4a0b5a435bd8d5b51357e067509f508e6111d3e514bb290",
      "byte_length": 17083
    },
    {
      "path": "docs/verification/reviews/rfc0017-anonymous-probe-v005/captures/pull-request-conditional.json",
      "sha256": "sha256:3f9006250099e9c294da28a8b24b460b8a2d67a1c7b6299dd3c09f2df55e2424",
      "byte_length": 856
    },
    {
      "path": "docs/verification/reviews/rfc0017-anonymous-probe-v005/capture-bindings.json",
      "sha256": "sha256:a1bc0c7fb42aa2e2ea161601f2479f52b97129f41709457351fb3280dbbde986",
      "byte_length": 832
    }
  ],
  "decision_evidence": [
    {
      "path": "docs/verification/reviews/run-boundary-decisions-v001/human-verdict.json",
      "sha256": "sha256:e46474c5ae2856541c592c5513ef8c64b56c2f97f37913f0734f674fdac762fb",
      "byte_length": 1789
    },
    {
      "path": "docs/verification/reviews/run-boundary-decisions-v001/verdict-validation.json",
      "sha256": "sha256:62d955fe44e4adcec9360374aa4730a3275a1ae972bf6691c7f2943c7a349ddd",
      "byte_length": 2700
    }
  ]
}
```

## Bound evidence

Measured probe evidence:

| Artifact | SHA-256 | Bytes |
|---|---|---:|
| `docs/verification/reviews/rfc0017-anonymous-probe-v001/captures/rate-limit.json` | `sha256:9b52844edae8de5cd728ed59a7f205a0b432a7c1418953d8d94bc1e3adc41e76` | 1119 |
| `docs/verification/reviews/rfc0017-anonymous-probe-v001/captures/graphql-refusal.json` | `sha256:67def7232c8fff7413238dca7181879539dda26cdc42cd0f832eb90e112094bd` | 986 |
| `docs/verification/reviews/rfc0017-anonymous-probe-v001/captures/pull-request.json` | `sha256:0f0737d8572f16cb77cd6a7d6c8734cd69adf15d37de73567e83d7b33316445f` | 17077 |
| `docs/verification/reviews/rfc0017-anonymous-probe-v001/captures/pull-request-comments.json` | `sha256:dd16dd6948458db8bc12c20e4882a4d78d93952ae0df4cd08c011898a5ddae7d` | 3402 |
| `docs/verification/reviews/rfc0017-anonymous-probe-v001/captures/absent-not-disclosed.json` | `sha256:2d7e0b9088acd8c6763ab541b56ac6ea8f69676d6d7bc56dd076e664f05ddc0e` | 864 |
| `docs/verification/reviews/rfc0017-anonymous-probe-v001/capture-bindings.json` | `sha256:a54477d9f4181a8c088db5ed530c4eef5eb1c09976bb9e0edd8df31ab893baab` | 1986 |
| `docs/verification/reviews/rfc0017-anonymous-probe-v005/captures/pull-request-fresh.json` | `sha256:7f99c19116ccc892a4a0b5a435bd8d5b51357e067509f508e6111d3e514bb290` | 17083 |
| `docs/verification/reviews/rfc0017-anonymous-probe-v005/captures/pull-request-conditional.json` | `sha256:3f9006250099e9c294da28a8b24b460b8a2d67a1c7b6299dd3c09f2df55e2424` | 856 |
| `docs/verification/reviews/rfc0017-anonymous-probe-v005/capture-bindings.json` | `sha256:a1bc0c7fb42aa2e2ea161601f2479f52b97129f41709457351fb3280dbbde986` | 832 |

The BPR-0299 build decision:

| Artifact | SHA-256 | Bytes |
|---|---|---:|
| `docs/verification/reviews/run-boundary-decisions-v001/human-verdict.json` | `sha256:e46474c5ae2856541c592c5513ef8c64b56c2f97f37913f0734f674fdac762fb` | 1789 |
| `docs/verification/reviews/run-boundary-decisions-v001/verdict-validation.json` | `sha256:62d955fe44e4adcec9360374aa4730a3275a1ae972bf6691c7f2943c7a349ddd` | 2700 |

The page presented and the tooling that reads the answer:

| Artifact | SHA-256 | Bytes |
|---|---|---:|
| `docs/verification/reviews/rfc0017-public-preview-decision-v001/offline-review-input.json` | `sha256:47cc6e8755b9124a8ca3a071184258fbe81dddfd59d40c920ac56440930ed8de` | 10408 |
| `docs/verification/reviews/rfc0017-public-preview-decision-v001/offline-package/attended-review-manifest.json` | `sha256:e9f18df8b10def3737028a00d68d5294b7bae91ddeead9983086a4bf9f2bcb7d` | 12352 |
| `scripts/validate_rfc0017_public_preview_decision.py` | `sha256:959ec8d155a7fdff86d4b18e3c199ea6b80064eaa8851297f89cec09fd87893b` | 35749 |
| `tests/scripts/validate_rfc0017_public_preview_decision_tests.py` | `sha256:844973f0705d3750205ca44fedd69274d3490056424268d9d23c5e034bdd9048` | 31169 |

Context, at the digests these definitions were written against:

| Artifact | SHA-256 | Bytes |
|---|---|---:|
| `docs/rfcs/future/RFC-BPR-0017-unauthenticated-public-review-read.md` | `sha256:c4473f5369030d5bd2ef496c6f83d10b9e4b1664b629bda618e3504fb709c7f6` | 26327 |
| `docs/issues/60-done/BPR-0299-decide-anonymous-public-preview.md` | `sha256:eac4b473c4290fe570e46963823c77e761fb53e869328c01858de25ff0ea9c1d` | 3730 |

## What the validator refuses, mechanically

Each of these is covered by a case in
`tests/scripts/validate_rfc0017_public_preview_decision_tests.py`:

- a `--require` value other than `build`; this gate validates only the
  positive branch, and a rejection is recorded as evidence rather than
  softened into a record of itself;
- a response that is not an exported `bpr.attended_review_verdict` — prose,
  a question, or another schema;
- a verdict for another batch, another subject, or a stale subject digest;
- a verdict that rejects the subject, or decides no subject at all;
- a verdict with missing or unsupported top-level fields, or altered
  `downstream_claims`;
- a note that is prose rather than definition assignments, names an unknown
  definition key, repeats one, or names one and leaves it empty;
- a note carrying a `do not build` verdict;
- a packet whose proposed definitions are not the four the review page
  presented, or whose bound evidence no longer matches the tree;
- a second write over an existing validation record.

## Operator response

Open
[`offline-package/index.html`](offline-package/index.html), resolve the one
subject, and paste the exported verdict back. It lands byte-preserved at
`docs/verification/reviews/rfc0017-public-preview-decision-v001/human-verdict.json`
and feeds exactly this command:

```sh
python3 scripts/validate_rfc0017_public_preview_decision.py \
  --packet docs/verification/reviews/rfc0017-public-preview-decision-v001/operator-packet.md \
  --response docs/verification/reviews/rfc0017-public-preview-decision-v001/human-verdict.json \
  --require build \
  --output docs/verification/reviews/rfc0017-public-preview-decision-v001/validation-record.json
```

To bind all four definitions as written, accept with an empty note or with the
note `as proposed`. To replace one, accept and write
`<key>: <replacement text>` on its own line in the note; the four keys are
`public_preview_name`, `limit_presentation`, `sign_in_handoff`, and
`supported_platform_policy`. Replacement text may wrap, but no continuation
line may begin with `word:` — that reads as a new key and is refused rather
than folded silently into the previous answer.

Rejecting is a recordable answer: it leaves [[BPR-0312]] unresolved and every
RFC-BPR-0017 implementation card dependency-blocked. Neither preparing this
packet nor a partial answer is an approval.
