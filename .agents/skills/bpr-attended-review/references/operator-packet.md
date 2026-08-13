# Operator Review Packets

Use a markdown packet as a fallback when the runtime cannot present the local
offline review page or an established external workflow explicitly requires a
verbatim statement. New document-only gates — third-party terms acceptance,
dependency acquisition, declared-versus-observed count deltas, repair or
migration authorization — normally use one identity-only offline-page subject
so the operator receives the same interactive, digest-bound export workflow as
media reviews.

## Template

```markdown
# <Gate name> operator review

Status: **pending operator decision; not <adjacent-gate> approval**

This packet presents the bounded human decision required by <RFC/card>. It
does not <enumerate what the packet explicitly does not verify, publish,
or approve>.

## Decision requested

Review and either accept or reject all of these facts as one decision:

1. <fact with exact counts, digests, or identifiers>
2. <fact>

## Evidence

| Artifact | SHA-256 | Bytes |
|---|---|---:|
| <repo-relative path> | `sha256:<64hex>` | <n> |

<declared-vs-observed tables, verification results, links to source pages>

## Operator response

To approve, reply with this exact statement:

> I approve <exact restatement binding the counts, digests, and scope
> above>.

To reject, identify the fact or evidence that must change. Rejection
leaves <state> pending and publishes nothing.

This response applies only to <scope>. <Adjacent gates> remain separate
attended operator gates.
```

## Rules

- The status line names what the packet is **not**, so an approval cannot
  be read wider than the gate.
- The decision is singular: every numbered fact stands or falls together.
  Facts that can be judged separately belong in separate packets or in an
  offline review page with one subject each.
- Every artifact the decision relies on is digest-bound in the packet; the
  verdict is reopened when any bound digest changes.
- The approval statement must be quoted back verbatim by the user. Any
  other reply is a rejection or a question, never a partial approval.
- The recorded operator statement becomes the `reason` on the resolved
  review record, quoted exactly.
- On an issue card, the packet body lives in the card (or is linked from
  it), and the card parks with an `awaiting-user` label until the response
  arrives, per `references/integration.md`.

## BetterPullReview Examples

- Accepting a third-party dependency and its license terms (a syntax
  highlighting or diff library) before it enters the build.
- Authorizing a live GitHub or GHES validation run against a named safe
  repository and pull request.
- Accepting a declared-versus-observed delta in acquired fixtures or
  recorded API contract data.
