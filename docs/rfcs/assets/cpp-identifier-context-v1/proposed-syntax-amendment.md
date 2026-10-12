# Proposed amendment: namespace and enum categories precede macro casing

This accompanies [RFC-DRAFT-0011](RFC-DRAFT-0011-cpp-identifier-context.md).
It proposes a correction to the adopted C++ identifier convention and adds
the namespace and enum-member categories clarified by the owner's references.
Live RFCs, cards, plans, statuses and historical verdicts are unchanged.

## Sources inspected

These SHA-256 content identities identify the adopted sources read during
authoring. Reconcile drift before adoption; they are not an implementation
baseline or acceptance evidence.

| Source | SHA-256 |
|---|---|
| [RFC-BPR-0006](../../RFC-BPR-0006-syntax-highlighting-themes-appearance.md) | `c5cbae883fdbaae8f4aaef0627b23f22236670bca2f0350597967c89428a3516` |
| [RFC-BPR-0034](../../RFC-BPR-0034-appearance-page-and-per-theme-colours.md) | `78de942e85be938a04e0847bf0363aeebeddf2d08b4ca2efda0f51333e0383aa` |
| [BPR-0726](../../../issues/60-done/BPR-0726-functions-types-and-macros-are-uncoloured-in-c-and-c-plus-plus.md) | `dd1628e8d78e3baee3dbfda668bb5aea7114402c5d6e2900b646945648f57c2b` |
| [BPR-0738](../../../issues/40-parked/BPR-0738-macros-and-functions-take-the-visual-assist-colours-on-the-dark-palette.md) | `9597706581201cc5c19c86921976b9d418518675cc0f9904f45dfb902904637d` |

## Before and after

RFC-BPR-0006's 2026-09-11 owner-direction section records that the C/C++
classifier gives ALL_CAPS words `syntax_macro`. For C++, qualify that rule:
namespace and enumerator declarations, recognizable qualified uses and the
defined uncertain qualified-use fallback follow the new draft before
capitalization is considered. The owner's `GCT`, `ID` and `DNA` examples
motivate the correction. Other ALL_CAPS cases retain the convention.

The follow-up answer, "Ideally they're colored like an enum", and the five
supplied reference images supersede the initial draft's undecided
constant-versus-normal treatment and declaration-only scope. Add independent
`syntax_namespace` and `syntax_enum_member` color roles. The reference editor
distinguishes these from types, functions and macros, and shows enum-member
treatment at declarations and qualified uses. Recognition remains bounded to
available lexical evidence, with no guarantee for external symbol identities.

RFC-BPR-0034's Colours section currently specifies 58 displayed roles from
60 total. Reconcile those counts and add the two syntax rows through its
existing per-theme controls, preview, persistence and exchange behavior.
Existing customized roles and stored flags must survive; older documents
missing the new roles receive variant-specific defaults. This extends the
role set without redesigning the page or resetting palettes. Reconcile the
system architecture's closed-role and default/import descriptions as needed.

Preserve the existing syntax RFC's nine unnumbered acceptance obligations,
especially whole-blob/hunk agreement, theme-independent lexical caching,
safe packaged definitions, contrast and architecture boundaries. Its
distinct macro role, approved existing colors and historical verdicts stand. Add new
acceptance for the corrected context and cache compatibility through the
draft's criteria; historical evidence does not prove those changes.

## Reconciliation and outstanding scope

BPR-0726 is terminal and remains historical evidence. BPR-0738 and BPR-0720's
parked historical review obligations are not answered, reopened or replaced
by this proposal. No existing milestone plan is changed. Implementation
should register a separate issue slice and add the narrow owner-direction
amendment to RFC-BPR-0006 without rewriting prior decisions. The new RFC's
dependency on the existing syntax foundation does not reopen that foundation.

No material product question remains open in the revised draft. The next
authorized implementation session should reconcile these source identities,
adopt the correction and verify classification, cache retirement, role
compatibility, customization persistence and composed offscreen rendering.
Exact new palette values require contrast validation; the sampled swatches
are reference targets rather than an audit or an acceptance verdict.

For visual work, use the repository-required frontend-design skill at the
path supplied by the owner:
`/Users/jaeger/.claude/plugins/marketplaces/claude-plugins-official/plugins/frontend-design/skills/frontend-design/SKILL.md`.
Any later explicit human judgment follows
the existing short attended-review workflow.

## Supplied reference identities

The originals are listed under the draft's Supplied visual references and
preserved outside the repository, without alteration. Their chat attachment
names and SHA-256 identities are:

| Attachment | SHA-256 |
|---|---|
| `codex-clipboard-d0a852ff-0c49-4cae-88ce-85231220f913.png` | `89f29210e241fafa6d36e2a30577fef223b4ac85ef49e3e06a862b9dc7a1632a` |
| `codex-clipboard-4c87d5ec-c870-4bf2-9d01-2776c253aa82.png` | `90306f27854e5d6b9b9a5a66dedff516f1d745c8e2b73c030a4cb4bdc5b0b12b` |
| `codex-clipboard-7f34162a-dcb7-456c-b2e4-daa3710b0792.png` | `3d3e27f505f372f6ecb691bf35b4ed1f689072bf969b3707ce63d8f22fe49bf4` |
| `codex-clipboard-b3e288bb-507b-473d-bc9b-9f022b760ce7.png` | `74e4daf742cfc6c635fdebb94b48591ba084a800dff79f5446af2d88694e2d1a` |
| `codex-clipboard-43682094-35f6-4076-82f6-377230d2c2ad.png` | `122eb13b71feea2430b6de5e66135bd2a33a579db9915d97164159cadedebf9f` |
