---
id: "RFC-BPR-0072"
title: "Context-aware C++ namespace and enum highlighting"
status: "Auto-Verified"
depends_on: ["RFC-BPR-0006", "RFC-BPR-0034"]
area: "BPR"
metadata: {"mandate_revision":"cpp-identifier-context-r1","rfc_base_sha":"6ebd33e57479a24aed91b930624bab030a436045","milestone_plan":{"id":"cpp-identifier-context-v1","path":"docs/rfcs/plans/cpp-identifier-context-v1/milestone-plan.json"}}
---

# Context-aware C++ namespace and enum highlighting

## Adoption

Adopted on 2026-10-12 under mandate `cpp-identifier-context-r1` with the supplied syntax amendment. Immutable RFC_BASE_SHA: `6ebd33e57479a24aed91b930624bab030a436045`. The [live import](assets/cpp-identifier-context-v1.LIVE_IMPORT.md) retains the original proposal and all eight criteria. Historical palette verdicts remain separate.

## Closeout, 2026-10-13

All eight criteria are accepted through [[BPR-1411]] and [[BPR-1412]]. Product
candidates `c58bdebdac18da64a3b7629c0499d200f36007ae` and
`bc4b4aafe6f51020aa469c519e936d616e124e89` carry the lexical refinement,
additive colors and older-theme admission correction. The
[qualification ledger](../verification/RFC-BPR-0072.md) binds complete-blob,
composed both-pane Light/Dark pixels, all-variant contrast and independent
four-variant customization through fresh-process restart and exchange.
No native, installed, human or independent verdict is inferred. Historical
palette obligations and the documented external-symbol limits remain intact.

## Problem and grounded current behavior

On 2026-10-12 the owner reported that `GCT` in `namespace GCT::Network`
and `ID` and `DNA` in this enum receive macro coloring:

```cpp
enum class Reason {
	Node = 9,
	ID = 1,
	Player = 64,
	DNA
};
```

These are C++ examples, regardless of the chat's code-fence labels. The
installed-app rendering was not independently reproduced during authoring.

The owner subsequently clarified: "Ideally they're colored like an enum",
acknowledging that full semantic context may be unavailable. The supplied
screenshots show enum members colored consistently at declarations and at
qualified uses such as `fuzzy_picker_key_code::Escape`,
`fuzzy_picker_event::Cancelled` and `preview_emit_mode::Full`.
`GCT_ASSERT(...)` retains macro coloring. Further Visual Assist references
show separate settings for namespaces and enum members in light and dark
themes. These images describe the desired treatment, not observations of
BetterPullReview. Their originals are preserved locally outside the repository
and listed below.

The [classifier](../../src/adapters/syntax/src/packaged_syntax_classifier.cpp)
explains the report: `identifier_role` tests `is_macro_case` before its `::`
scope test. That rule accepts words of at least two characters containing
only capitals, digits and underscores, with at least one capital. There is
no namespace-declaration or enum-body state in the identifier refinement.
The [contract tests](../../tests/syntax_contract/syntax_classification_contract_tests.cpp)
explicitly expect ALL_CAPS macro classification, including an invocation
whose definition is absent from the blob.

The [appearance vocabulary](../../src/core/include/bpr/core/appearance_model.hpp)
has no namespace or enum-member role. Its
[dark palette](../../src/adapters/qt/src/appearance_profile.cpp)
assigns `syntax_constant` pale green (`B8D7A3`), which does not express the
reference's amber enum-member treatment. Reusing that role alone would not
fulfill the clarified request.

The adopted [syntax RFC](RFC-BPR-0006-syntax-highlighting-themes-appearance.md#owner-direction-2026-09-11)
also records this convention. The accompanying
[proposed amendment](assets/cpp-identifier-context-v1/proposed-syntax-amendment.md) narrows it; historical
acceptance and live trackers remain unchanged during authoring.

## Proposed behavior and boundaries

### Prefer structural context

For the C++ definition, namespace declaration names receive a distinct
`syntax_namespace` role, consistently across capitalization, simple names
and nested namespace chains. Thus both `GCT` and `Network` in
`namespace GCT::Network` are namespaces. Use that role where syntax itself
establishes a namespace, including namespace alias names and the namespace
chain in `using namespace`.

Enumerator declaration names in C++ enum bodies receive a distinct
`syntax_enum_member` role, regardless of spelling or whether an initializer
is present. This applies to scoped and unscoped enums. The enum name keeps
its type role. Only the declared name receives this treatment; initializer
expressions retain their own classification, including macro references.
Nested delimiters in an initializer must not end the enum or mistake an
expression operand for the next enumerator. Context ends at the matching
enum closure and cannot recolor subsequent declarations.

Use complete-blob lexical context across line breaks, comments and hunk
boundaries. Comments, strings and preprocessor directives cannot open or
close namespace/enum context by containing lookalike tokens. Existing
grammar classifications retain authority over their own styled content.

### Recognize uses with available lexical evidence

Use declarations in the complete blob to recognize namespace qualifiers and
qualified enum-member references outside their declaration. At minimum,
`Reason::ID` and `Reason::DNA` receive the enum-member role when an
unambiguous matching enum/member declaration is available in that blob.
This also covers the reference's qualified-use patterns in a self-contained
file. Match qualified scope and declared members; a globally matching word
such as `ID`, `Full` or `Cancelled` alone is insufficient. Ambiguous scope,
shadowing or aliases must not propagate a guessed identity across the blob.

When an ordinary qualifier before `::` cannot be identified more precisely,
retain the existing generic type/scope treatment before applying the ALL_CAPS
fallback. A terminal qualified name without enum evidence keeps its ordinary
grammar/function/identifier treatment; its capitalization alone cannot make
a non-invoked qualified name a macro. For example, an otherwise unknown
`ExternalReason::ID` stays neutral rather than being promised enum coloring.
Qualified invocations retain the existing function/macro conventions unless
stronger recognized context applies.

Recognition is best effort within one blob. Includes, external declarations,
compiler/AST analysis and cross-file symbol lookup are outside this proposal.
Unqualified enum uses, including `using enum` imports, need not acquire enum
coloring. Better local evidence improves the result; perfect semantic
classification is not an acceptance requirement.

### Preserve the remaining macro convention

Outside these structural exceptions, retain existing ALL_CAPS macro
classification, including unqualified object-like references, invocations such as
`VERIFY_ARE_EQUAL(...)`, predefined macros and directive operands. This
includes a real macro used in an enum initializer.

The change is scoped to C++ classification. C, Windows Resource and other
packaged languages retain their current token roles.

### Give the two categories their own color slots

Expose Namespaces and Enum members as independent syntax colors through
[the existing per-theme appearance controls](RFC-BPR-0034-appearance-page-and-per-theme-colours.md#colours),
preview, persistence and theme exchange. Keep the current code font, pane
layout and existing role colors. Enum members must not borrow the function
slot merely because both appear orange in the dark reference.

The supplied swatches give these reference targets:

| New category | Dark reference | Light reference |
|---|---|---|
| Namespaces | Pale green `B8D7A3` | Teal `216F85` |
| Enum members | Amber `B9771E` | Dark slate `2F4F4F` |

These are sampled source values, not a contrast-audited palette. Follow their
color families while meeting RFC-BPR-0006's existing 4.5:1 text contrast floor
on the required diff fills and selection states; adjust the new defaults
where necessary and provide high-contrast variants. No requirement copies
the reference editor's backgrounds or overrides existing role colors.

Older stored palettes and imported themes that omit the two new roles use
the new built-in defaults for their own variant. Preserve every existing
customized role, stable identity and per-theme stored state; perform no palette
reset. New exports include the new roles, and their customization survives
restart independently per variant. Exact palette tuning and storage mechanics
belong to implementation under these requirements.

### Ownership and cache compatibility

The syntax adapter owns lexical recognition and per-blob declaration facts;
it performs no repository reads or compiler queries. The core owns the two
additional role identities and theme-validation compatibility, without parsing
source or selecting colors. The application host owns cache delivery and
appearance persistence; Qt owns palette values, controls and rendering. Preserve
the
[appearance allocation](../architecture/system-architecture.md#rfc-bpr-0006-appearance-allocation)
and the adapter's existing byte, span and cancellation limits. Bound temporary
declaration tracking by the admitted blob and retain cancellation during that
work. Retire span sets produced under the old identifier rules using the existing
[definition-version mechanism](../../src/adapters/syntax/include/bpr/adapters/syntax/packaged_syntax_classifier.hpp).
Internal parsing mechanics are left to implementation.

### Owner direction and remaining limits

The owner's enum-specific coloring answer and subsequent category references
resolve the earlier constant-versus-normal question. This revision expands
the original declaration-only draft to recognizable qualified uses and two
independent color slots; it is a proposal, not implementation acceptance.
No material product choice remains open. External symbol resolution remains
deferred as described above.

## Acceptance criteria

- **AC01:** Simple and nested namespace declarations, namespace aliases and
  `using namespace` receive the defined namespace treatment with identical
  roles for uppercase and mixed-case spellings; the reported `GCT` is not a macro.
- **AC02:** The supplied enum gives all four enumerator names the enum-member role.
  Scoped and unscoped declarations, multiline bodies and nested initializers
  distinguish names from expression operands, and classification resumes
  normally after the enum closes.
- **AC03:** A hunk beginning after the namespace/enum opener has exactly the
  roles projected from the complete blob. Lookalike text in comments,
  strings and directives does not alter that context.
- **AC04:** Existing macro invocations, unqualified standalone references, directive
  operands and predefined macros keep their roles, including a macro inside
  an enum initializer. Other language definitions retain their roles.
- **AC05:** Old identifier-rule cache keys cannot supply stale spans. Theme
  changes continue to reuse lexical classification.
- **AC06:** A composed offscreen comparison displays declaration and recognized
  use-site categories in both panes, including uppercase examples, across light
  and dark themes. New defaults meet the existing contrast floor, including
  high-contrast variants; a neighboring genuine macro retains its presentation.
- **AC07:** Qualified uses of namespaces and enum members declared in the blob
  receive their category roles. An unrelated identically named member is not
  recolored by global spelling, and the defined uncertain-use fallback keeps
  an unknown qualified uppercase value out of the macro role.
- **AC08:** Each new color can be customized independently per variant and
  survives restart and export/import. Loading an older palette or theme supplies
  both new defaults while preserving all existing customized values and stored states.

## Supplied visual references

The five original attachments are preserved byte-for-byte outside Git at
`/Users/jaeger/.codex/visualizations/2026/10/12/01a13229-e829-7ec1-b646-1fc24fb2ff70/cpp-identifier-context/`.
They are design inputs, not runtime evidence. SHA-256 identities are retained
in the accompanying proposed amendment. The files in that directory are:

- `owner-enum-declaration.png`: dark enum declaration.
- `owner-enum-uses.png`: dark qualified uses and macro.
- `owner-light-color-slots.png`: light color categories.
- `owner-light-code.png`: light namespace and enum code.
- `owner-dark-color-slots.png`: dark color categories.
