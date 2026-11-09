---
schema: "implement-rfc.card.v1"
id: "BPR-1957"
title: "Compact Go to line and support line:column"
status: "Done"
rfcs: ["RFC-BPR-0097"]
depends_on: ["BPR-1948"]
priority: "P1"
metadata: {"mandate_revision":"go-to-line-columns-r4","assigned_implementer":"/root/go_to_line_design","reservation":"docs/issues/_reservations/BPR-1957--BPR-1957--ea6d03ed4be4.json","milestone_plan":{"id":"go-to-line-v1","path":"docs/rfcs/plans/go-to-line-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC05","outcome":"pass","revision":"bb06010ffe29011cf3529fb6c3efff4b89130f00","evidence":{"kind":"observation","detail":"Exact native block text shared by dialog validation and jump; split/unified BOM/UTF-16 columns, presentation-cache refresh and subsequent single-line draft pass through installed Git/cache at 100/125/200%. Native focused column courses pass at all three scales. Deliberately dropping leading U+FEFF fails the exact composed assertion; restored candidate passes. Fixture/pack, architecture, format22, tidy22 and documentation checks pass. See docs/verification/RFC-BPR-0097.md."}},{"criterion":"AC06","outcome":"pass","revision":"dd968668313c5abe343273de960ea9f00bfefae1","evidence":{"kind":"observation","detail":"Human Accept at decision6, handback7, session9827b9b1187675faf91e0e097db5a23f; exact v005 fingerprint130b758e3524f698d848e79d95122de62a834b222bcf07c7f0aec76e21cefc24, binary d16532fb9fca631e4f54f8339c5a6c532bb60938ee4e893e556942d2801e9765. No notes. Later functional correction leaves accepted composition/controls/styling unchanged; three-scale keyboard/accessibility/theme checks pass. Frozen bytes verified unchanged."}}]
---

# Compact Go to line and support line:column

Owner amendment to [RFC-BPR-0097](../../rfcs/RFC-BPR-0097-go-to-line.md):
reuse Find's close control, remove the repeated filename and support optional
one-based UTF-16 columns. The source-side selection and single-line comment
anchor remain exact in split and unified views. This is the `line-column-panel`
milestone; BPR-1948's completed original acceptance remains historical.

## Acceptance

- AC05: Exact plain-line and line:column caret navigation, validation,
  Unicode/tab/CRLF/empty/end-of-line semantics, horizontal visibility,
  single-line draft anchoring and unchanged bounded work.
- AC06: Shared Find close control, compact layout without filename or Cancel,
  keyboard/accessibility/theme/scale checks, and owner acceptance of the exact
  frozen appearance candidate.

## Evidence and completion

[Verification](../../verification/RFC-BPR-0097.md) and
[closeout](../_checkpoints/go-to-line-v1/closeout.md) retain exact candidates,
checks and review identity. AC05 passes at `bb06010ffe29011cf3529fb6c3efff4b89130f00` after correcting the
later-BOM column-limit mismatch. Split/unified columns and presentation-cache
refresh pass through installed Git/cache at 100/125/200%; native column checks,
fixture reproducibility/pack, architecture, format/tidy and documentation pass.
The regression demonstrably fails when the faulty BOM stripping is restored.

AC06 appearance was **accepted** without notes for the exact frozen v005 in
session `9827b9b1187675faf91e0e097db5a23f`, decision6 / handback7. Its
400×72 logical-pixel composition, shared Find close control and styling are
unchanged by the functional correction. Keyboard/accessibility/theme/scale
checks pass on the final source. The accepted frozen binary hash is unchanged.
Consumed cursor8 includes the scoped mutable protection release; its host is
stopped. No acceptance is transferred between historical frozen candidates.

BPR-1948 and its original acceptance remain intact. All branch-local work is
complete; main integration belongs to the coordinator. Native platform,
VoiceOver, package-trust and Release-performance claims are not made.
