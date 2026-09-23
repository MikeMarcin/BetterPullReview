# External API authority and qualification boundaries

Official sources checked **2026-09-23**. These references support public API/editor primitives, not the feature's design choices or any claim that a live tour was published. The supplied BPR snapshot remains the source for project behavior. No private repository content was used as a public search query.

| ID | Official reference | Relevant documented primitive |
|---|---|---|
| GH-REVIEWS | https://docs.github.com/en/rest/pulls/reviews | Review groups, raw bodies, pending creation with omitted event, comment enumeration, summary update, exact-review submission and pending deletion. |
| GH-COMMENTS | https://docs.github.com/en/rest/pulls/comments | Review-comment bodies/IDs/URLs, standalone line/file subjects and body updates. This does not establish that the REST batch accepts every standalone field. |
| GH-PR | https://docs.github.com/en/rest/pulls/pulls | Read PR body/head and update a PR body. No unsupported compare-and-swap guarantee is inferred. |
| GH-ISSUES | https://docs.github.com/en/rest/issues/comments | Ordinary PR conversation comments use issue-comment operations. They are not members of a submitted review simply because they reference that PR. |
| GH-GRAPHQL | https://docs.github.com/en/graphql/reference/pulls | Fixed review, thread, comment-update and submission mutations; explicit review association for adding a thread and file subject type. Node IDs and numeric IDs remain distinct. |
| GHES-REVIEWS | https://docs.github.com/en/enterprise-server@3.18/rest/pulls/reviews | GHES-specific review contract and API-version examples. Qualify the deployed server and exact operation, not a version label alone. |
| GH-CLI-API | https://cli.github.com/manual/gh_api | Authenticated explicit API calls, methods, hostname, JSON input and GraphQL. |
| GH-CLI-REVIEW | https://cli.github.com/manual/gh_pr_review | High-level disposition/body interface; no structured inline batch authoring interface is documented. |
| GH-REVIEW-UX | https://docs.github.com/en/pull-requests/how-tos/review-pull-requests/reviewing-proposed-changes-in-a-pull-request | Pending line comments are author-visible before submission; normal review actions and website editing. |
| GH-PERMALINK | https://docs.github.com/en/repositories/working-with-files/using-files/getting-permanent-links-to-files | Permanent file-version and line-range links. |
| GH-REST-PRACTICE | https://docs.github.com/en/rest/using-the-rest-api/best-practices-for-using-the-rest-api | Request sequencing, pagination, redirects and rate-limit handling. |
| QT-CURSOR | https://doc.qt.io/qt-6/qtextcursor.html | Native edit blocks can group document changes into one undo operation. Exact selection/raw-byte behavior still requires BPR tests. |

## Facts not established by the documentation alone

Pending-comment REST PATCH behavior, the exact mixed REST/GraphQL staging combination, file-level thread membership and visibility, reliable author association, returned comment URLs before/after submission, website permalink/thread appearance, notification effects, host-specific permissions and all private-server capabilities require exact fixtures and designated live qualification.

The documented public GitHub REST examples currently use a newer date-version than the GHES 3.18 examples. Do not copy either globally. Retain BPR's selected server contract/version header and qualify any change as a separate compatibility decision. No claims about current minimum `gh` versions come from this pack; reuse the repository's live admission rules.

Do not send undocumented fields, call internal website endpoints, assume a GraphQL mutation ID provides server idempotency, or use API docs as evidence that a network timeout means failure. The API offers review submission and ordinary edits, not a general transaction across all objects.

## Qualification record expected from implementation

Record host product/version where observable, selected API contract, transport, actual client version when relevant, actor/repository/PR scope (sanitized), each operation and placement form, exact candidate revision, capture method, outcomes and explicit limitations. Use a designated safe repository and the existing live-test authorization path. Never store real access tokens or unrelated private code in fixtures. A production UI acceptance walk and a fixture transport contract are different evidence classes.
