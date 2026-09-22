# BPR-0242 composition contract reseal

Owner: `/root/rfc0008_owner_closeout`, mandate r4. This is an implementer
self-review of the source covered by a stale tripwire, not independent review
or new native/release acceptance. No product source changes in this slice.

The initial eight-selector run at source `2a64e901` passed seven checks and
failed `application_review_round_trip_composition_contract` before its runtime
course: the full application pin expected `e04f598b…e0d5` from `d6dfca2b` and
observed `b6139008…855`. Total run: 113.31 seconds. The other seven included
core, coordinator, synchronization, Qt editor, both isolation checks and the
whole-review public workflow. A focused current-source build succeeded first.

## Reviewed range

Read `git diff d6dfca2b..2a64e901 -- src/app` for the pinned application/header,
entry point and desktop scopes, and the complete new release-maintenance,
update-desktop, installed-runtime and platform-vault composition files. The
following list covers every commit in that range which changed `src/app`;
it deliberately includes changes to fixture and build files beyond the digest.

| Commits | Existing record and current review finding |
|---|---|
| `b7e92258` | BPR-0908 private release preparation; app build identity/package path, no review publication route. |
| `1210eecc`, `4efd549c`, `1b73d61f` | BPR-0909/0910/0913 CLI discovery; bounded configured process and account selection. The later version requirement remains 2.96. No token extraction or publishing grant is added. |
| `1a74b36d`, `cbd50b74` | BPR-0914/0915 shared sign-in and connected CLI destination selection. Exact accepted mode/profile/host and cancellation generations remain admission checks; discovery does not mint publication authority. |
| `5b95485c`, `cc038748`, `e73831f5`, `8a017743` | RFC-0037/BPR-0927 source-entry/conversation/attended fixtures and sign-in refusal. Review reading uses existing source/host controls; fixture replay is under testing. The production inline branch is unchanged. |
| `9c670d88`, `ca000e79`, `e21476cc`, `10e9716d`, `a9492592` | RFC-0013 BPR-0794/0795/0935–0943 source-backed handoff: bounded loading/Overview, font/tab/colors and exact missing-endpoint refusal. BPR-0944 retains the unfixed pane/basis anomaly. No mutation coordinator, grant issuer or publication target is replaced. |
| `5ed24a8c`, `d1e0f46c`, `5cf310c4`, `b7e2df46`, `518c384a` | RFC-0035 BPR-0947/0948/0949 and milestone ledger: native-only separately selected release account, retained-root update transaction, draft preservation and native menus. Reviewed the added account lease's exact authority/generation checks and revocation, shutdown counts, inert saved draft strings and read-only recovery dialog. No saved draft is submitted by these paths; installer authority is distinct from review authority. Pending private-release/vault and Intel qualifications remain pending. |
| `a3ab46ee`, `cf490c5a` | RFC-0036/BPR-0917 recorded self-review and body/link tests. Markdown gestures go through the read host's exact window/snapshot admission before the browser; HTTP(S) browser support changes no provider destination or publication payload. |
| `a34fc07f`, `13a7d052`, `65dd7dd4`, `277a2c57` | RFC-0014 M5/M6 BPR-0955/0956 and handoffs: securely seeded maintenance identity, retained profile lease, separate vault worker, startup recovery, all producer retirement and reachable Quit. Reviewed exact prepared selection and retirement gates; maintenance invokes its own host and never a GitHub write. |
| `9409ff61` | RFC-0014 M9/hardening ledger: bounded IPC and installed-runtime policy. The new pre-QApplication restriction refuses plugin-loading arguments and fixes plugin paths; it creates no review/provider issuer. Native/package acceptance retains its existing gaps. |
| `92c4e159` | RFC-0014 M9 Qt 6.11.2 change: build/package inputs and Windows title-row version comment. This reseal is not new Windows visual evidence. |

The current production `compose_review_mutation` branch digest is still
`a7e2081e57d4344efdf0caf8d6ad63568ba5717a6a9b0a61d6e57ddf053d1e8a`.
The full-file/source-tree contract continues to enforce the host-only path,
language-level issuer seals and hostile source substitutions. No condition,
negative case, allowlist token count or runtime assertion is removed.

## Inventory and reseal

The source-tree inventory adds all nine new app source/header paths: the two
release-controls headers, maintenance/update implementations, installed-runtime
pair, platform-vault pair and update-helper main. They also enter the production
content manifest, so changes to their bytes are now bound. Existing inventories
remain intact. The isolated source-cut tests copy the full app source tree.

Values come from the contract's own `BPR_PRINT_PINNED_DIGESTS=ON` computation:

- Full application: `b6139008a8dd0873eded8bf5c362e7f2c72220cbafb6636c3779a937d71a5855`.
- Expanded production manifest: `33f9ce4b69561afc4a2eec398b6d9ab207723936255ce2f409a6910fe1522404`.
- Production mutation branch: unchanged, as above.

The compiler rebuilt no C++ from the initial `2a64e901` source candidate.
Only the test inventory/pins and documentation are changed. The current
verification result is recorded on BPR-0242 and the RFC-0008 ledger after replay.
