# External authority and evidence limits

Consulted **September 23, 2026**. These official sources support narrow external facts. The extension's architecture, CLI syntax, shared-draft behavior, skill workflow and tests are authored design, not capabilities claimed by these pages. Live service/agent/native qualification is not performed by reading documentation.

| ID | Source | Supported fact / design use |
|---|---|---|
| GH-REVIEWS | [GitHub REST pull-request reviews](https://docs.github.com/en/rest/pulls/reviews) | Reviews group comments; omission of an event creates a pending review; explicit submission and review/comment reads exist. The parent runtime owns the actual finite calls and compatibility. |
| GH-REVIEW-UI | [GitHub reviewing proposed changes](https://docs.github.com/en/pull-requests/how-tos/review-pull-requests/reviewing-proposed-changes-in-a-pull-request) | Pending line comments are visible only to the author before submission. This is remote staging, not evidence of full local draft synchronization. |
| GH-COMMENTS | [GitHub REST review comments](https://docs.github.com/en/rest/pulls/comments) | Readable normal comment bodies, identities and URLs underpin inherited reconstruction; specific host capability still needs evidence. |
| GH-CLI | [GitHub CLI gh api](https://cli.github.com/manual/gh_api) | Structured authenticated API transport exists. The skill must still use the shared BPR publisher rather than directly invoke arbitrary API calls. |
| QT-CORE | [Qt QCoreApplication](https://doc.qt.io/qt-6/qcoreapplication.html) | Qt provides a non-GUI event loop. Keeping Core/Network is a design choice to reuse existing runtime adapters, not a requirement to link Widgets or use an offscreen GUI. |
| SKILLS-SPEC | [Agent Skills specification](https://agentskills.io/specification) | SKILL.md metadata/instructions plus progressively loaded references are a supported packaging shape. The pr-tour skill name must match its directory. This does not establish any provider-specific permission attestation. |

Do not copy a new web sample's API-version header or latest Qt version into the application automatically. The live product's admitted host-specific versions, dependency pins and observed capabilities remain authoritative. This pack does not enable unsupported GHES operations, qualify an installed gh binary, or guarantee a provider's agent permission behavior.

Public documentation does not promise a global atomic transaction covering PR bodies, pending review assembly and maintenance. Keep the parent publication boundary and residual race disclosures unchanged. Do not use current documentation to overwrite private project design or pretend the September 22 source snapshot contains the newly imported tour implementation.
