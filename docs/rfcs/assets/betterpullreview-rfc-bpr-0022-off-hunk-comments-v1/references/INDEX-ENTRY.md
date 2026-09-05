# Proposed RFC index entry

Add RFC-BPR-0022 to the **Deferred post-pack work** table after RFC-BPR-0021;
do not add it to the canonical fourteen-RFC pack-v2 table.

```markdown
| [RFC-BPR-0022](future/RFC-BPR-0022-semantic-source-line-comments-off-hunk-publication.md) | Semantic Source-Line Comments and Off-Hunk Publication Compatibility | Proposed | RFC-BPR-0004, RFC-BPR-0005, and RFC-BPR-0008 implemented at the required seams; cannot close before RFC-BPR-0008, and exact supported/unsupported live mutation evidence remains required |
```

Add this explanatory paragraph near the other post-pack relationships:

```markdown
RFC-BPR-0022 is an additive amendment to RFC-BPR-0008's publication seam, not
an edit to the settled RFC. It preserves one semantic source-line interaction
while choosing either a native GitHub line subject or an ordinary file review
comment with a visible immutable code permalink. It does not replace
RFC-BPR-0021's general historical comment/revision join.
```

Reconcile wording and status against the live index during import.
