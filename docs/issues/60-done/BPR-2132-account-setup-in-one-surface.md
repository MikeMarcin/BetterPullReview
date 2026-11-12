---
schema: "implement-rfc.card.v1"
id: "BPR-2132"
title: "Account connection is one setup progression on Home for every way in"
status: "Done"
rfcs: ["RFC-BPR-0032"]
depends_on: []
priority: "P1"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2132--BPR-2132--5eca3baab9b5.json","source_commits":["4573c7ddd","118bd0b34","4a85c29c9","a2fa8fb71","104231396","924f08a5a","8d2210540","f0dfd707c","e7cd2843e","5d5bfc554"],"amendment":"docs/rfcs/amendments/RFC-BPR-0032-account-setup-in-window-r1.md","attended_session":{"id":"dcb3c9f70ca09b5cb2169ff96d811309","state":"/Users/jaeger/bpr-attended-review/account-setup-r1.an__pwhm/attended-review","status":"handed back, host stopped, frozen root removed"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"8d2210540","evidence":{"kind":"observation","detail":"Owner attended session dcb3c9f7: progression, browser code, gh consent in place, problem wording and Agent-tab language all accepted; q1 retire the dialog everywhere, q2 server first step with an Enterprise way, q3 preselect gh's active account."}},{"criterion":"AC2","outcome":"pass","revision":"5d5bfc554","evidence":{"kind":"command","detail":"core_account_setup, application_account_setup_host, qt_account_setup_view and composed/home/public-preview Qt suites pass; migrated end-to-end sign-in courses (device flow, gh consent, setup destination, remembered connection, review submission, saved server, device binding, two-account isolation, review tour publication) pass; composition contracts resealed and passing; architecture, core-only, format and tidy pass."}}]
---

# Account connection is one setup progression on Home for every way in

Owner report: first-run account connection stacked several pop-ups and the gh
path was disorienting. Home's first run, Settings › Add account, a server
card's Sign in, Use another account, the signed-out account button, a refusal's
Sign in and a pasted link needing an account now all open the same in-window
setup: server → way → sign in / choose account / token → confirm → ready. The
shared sign-in dialog, the modal gh consent question and the token prompt are
removed; Connect as LOGIN records consent.

## Acceptance

- AC1. The owner accepts the progression and its decisions.
- AC2. Every sign-in entry point uses the one setup, and the migrated courses
  pass.

Pending owner look: the new server step and Settings › Add account renders
(amendment AS-5). Not run: live second-account course, full tests-off Release
composition contract, Windows and Linux.
