# Concrete accounting examples

The fixture has exact normalized conversation identities, duplicate page input,
all twelve labels, differently labelled replies, unknown roots/resolution,
plain/custom labels, unanchored native threads, top-level/review-summary bodies,
private notes, drafts, deleted units and an outdated anchor. Duplicate identical
page records collapse by identity. Private/draft/deleted units never enter shared
counts. Classification uncertainty is separate from an intentionally plain body.

For `src/cache.cpp`, the complete retained scope contains ten units: seven
positively unresolved, two resolved and one of unknown resolution. Issue and
Question are selected as dedicated columns.

| Mode | Issue | Question | Catchall | Included |
| --- | ---: | ---: | ---: | ---: |
| Unresolved | 2 | 1 | 4 | 7 |
| All states | 4 | 1 | 5 | 10 |

The unknown-resolution issue is absent from the unresolved count but discoverable
as unknown. A root-missing thread contributes to catchall and makes label coverage
incomplete; therefore the Issue value must not be presented as a proven exhaustive
count of all real issues in that scope. The values are counts of currently
classified units with the stated coverage. A non-blocking suggestion remains in
the unresolved population until native resolution changes.

With no selected columns, the two catchall counts become 7 and 10 respectively.
With all twelve selected, the unresolved breakdown is Issue 2, Question 1,
Suggestion 1, catchall 3. The all-state breakdown adds resolved Note 1 and includes
Issue 4; catchall remains 3. Other dedicated columns are zero *known classified*
units, qualified by the root-coverage limitation.

Changing Issue to an unselected label moves one unit from Issue to catchall without
changing included total. A differently labelled reply changes no thread count.
A provider-confirmed resolve moves one unit from unresolved to resolved; all-state
total remains unchanged. A website root edit reclassifies without requiring any
commit change. A missing root is not replaced with its newest reply.

PR-wide counts also include unanchored units and not-applicable top-level bodies,
so the sum of visible file rows may be smaller than the PR population. Present
that remainder explicitly. Unknown and not-applicable units are not labelled
resolved merely to fit an unresolved/total fraction.

The local pack verifier checks these explicit expected cases and all 4,096 label
subsets across both modes and all four fixture scopes. Actual C++/host tests must
verify their own normalization, coverage propagation and projections; the Python
oracle does not establish those implementations.
