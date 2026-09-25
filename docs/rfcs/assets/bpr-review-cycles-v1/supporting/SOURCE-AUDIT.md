# Source audit and integration baseline

The supplied snapshot was generated on **2026-09-25 at 00:00:02 -07:00**. It contains
4,223 parsed repository files. The mounted dump is 146,271,464 bytes with SHA-256
`8f9226134e287033a8f35046bc2a9ed7ca4b2480e42d6bace858ea6d26095a52`.
[Machine provenance](SOURCE-BASELINE.json) records exact body hashes and dump line
positions. The original source dump and existing packs are **not** redistributed here.
This is a snapshot audit, not an inspection of live HEAD or a successful build.

## Findings that change the implementation plan

| Observed contract or seam | Consequence for this pack |
|---|---|
| RFC numbering reaches 0052; NEXT_ID contains BPR-1143. | Propose 0053–0061 and use PACK-CYCLE aliases. Reserve nothing until live intake. |
| The product contract is PR-centric and excludes creating PRs and operating as a general Git client. | Explicitly extend current contracts for local subjects and finite new-branch/PR publication. Do not imply the old comment publisher authorizes it. |
| Epoch and Git cache authority is tied to accepted PR/account/repository facts; the cache does not operate on a developer worktree. | Add a local source authority/material seam with exact identity. Do not inject fake PR IDs or give the existing credential-free cache general worktree/network access. |
| Large data belongs in host-owned bounded observations/stores, not a global core state tree. | Reuse typed IDs, generations, exact material and request correlation; keep scanning, persistence and network out of reducers. |
| Native comment/draft and private ledger paths exist, but RFC-0023 remains a broader deferred private-thread/provenance program. | Schedule the required small common private-thread/version/provenance seam explicitly. Do not claim all of 0023 is implemented or create a competing cycle-only system. |
| RFC-0036 concerns native Markdown rendering. | Inventory actual code-editing ownership. Rendering alone does not fulfill virtual-space, multi-caret, suggestion-editing or undo requirements. |
| RFC-0044–0046 specify AI-free terminal services, one local owner and a thin pr-tour skill, but are proposed work in this snapshot. | Reuse their verified live interfaces when present; otherwise coordinate a shared service dependency. A proposed CLI is not an available executable or import API. |
| RFC-0047–0051 specify twelve optional labels and lossless suggested-change coexistence. | Reuse these semantics, shared composer and selected icons; do not invent a second label set. |
| RFC-0052 specifies the owner-selected title row, review bar, folding Updates line, document tabs and tray. | Integrate Review Cycles without adding another permanent toolbar. Preserve exact tab source/compare identities and reserved shortcuts. |
| The owner's September 25 exception activates only RFC-0052 ahead of canonical order. | This new pack does not automatically inherit that exception or release earlier proposed packs from their gates. |
| src/app/CMakeLists.txt composes AI/Agent/MCP adapters and the MCP helper into the existing app graph. | “Removed” requires a real build/composition/install split, not only a hidden panel or absent provider configuration. |
| Security qualification and supported native/platform capabilities are version-bound and incomplete in some lanes. | Preserve honest capability refusals. This archive does not certify a platform, approve a release or close historical security gates. |

## Source locations

The following entries identify audited integration material. A listed path is an
existing source reference; proposed new type/component names in the RFCs are not.

| Repository path | Dump content lines | Extracted body SHA-256 |
|---|---:|---|
| `docs/architecture/product-contract.md` | 4846–5047 | `385f3cb1b17d42fc27dba35c2048428972d1e8338fc39a620b4cb0891ccb8f68` |
| `docs/architecture/system-architecture.md` | 5056–6425 | `3010c13941909d8ebeb2d73e9d32b88423545387ffaa3da5e76fc961accfa505` |
| `docs/architecture/domain-and-state-model.md` | 53770–54385 | `603669ab718113a2505752f85b61ed70057ad662cdc3c057ed1932df33c9ee64` |
| `docs/architecture/git-and-epoch-model.md` | 54455–55829 | `435204d0ce843dcdb23949d1f43a9b2f539821d0328ea837005de7aab36baea0` |
| `docs/architecture/review-ledger.md` | 60075–60443 | `edb2816a94ea7068e6f9c63ad91e12a8ee4cedbf8df01575b6508af37e25a08c` |
| `docs/architecture/review-round-trip.md` | 60898–61618 | `0c64a945b161a1e01cd3696bf82f5f026b6519401837f952462309d4e7b88cf4` |
| `docs/architecture/ai-agent-boundary.md` | 50460–51460 | `a3bff42ef05624db57a08a91a1d04a53e8720aa2fd8d0bdf59ec48fbcecc906a` |
| `docs/architecture/native-markdown.md` | 58853–59024 | `35cc9ae1b1743fd2b32412947c0839dd04b6f81a7cc9a36d68b7f47629b0fe79` |
| `docs/architecture/security-threat-model.md` | 61627–62895 | `93bac8f0dcae38c15be631ac6c07b9b9c0ea914d6e10f1f6291a49a6112dfbf2` |
| `docs/security/corporate-security-overview.md` | 313756–313923 | `ad29650c2f5259e6349c6c5a30ba2d49ad4d6c5958d66f2f41aab8d41ffe0a52` |
| `docs/rfcs/RFC-BPR-0023-private-review-threads-provenance-comment-composition.md` | 249800–250722 | `4ea993d7a16a7cee8542418b06fc62e89ad72826d340506b1afaa01a1d697468` |
| `docs/rfcs/RFC-BPR-0036-native-markdown-rendering.md` | 255153–255298 | `2c0a01abbcd9f0fab28fc541458b2cf02ccf58eebd38f7f8eec3e3bdae5b52b4` |
| `docs/rfcs/RFC-BPR-0044-headless-tour-runtime-and-terminal-cli.md` | 256226–256318 | `e8b82a2d6c8358ed2f3a8ac351a1b5954faa9fff165772051a88b7bdfe2aa166` |
| `docs/rfcs/RFC-BPR-0045-shared-local-tour-drafts-and-cross-interface-recovery.md` | 256327–256409 | `bcbfe2c36c242c8eaa6fd4232e784ddff1b27a1f9a4ee4a97b946dda23078c0d` |
| `docs/rfcs/RFC-BPR-0046-thin-pr-tour-agent-skill.md` | 256418–256498 | `7583b34a0232e820758204b49ddaa4d2837aaef0e2986440ff0149d7aa77a00e` |
| `docs/rfcs/RFC-BPR-0048-feedback-authoring-native-github-integration.md` | 256670–256852 | `7121c8b95163a8c4b1d3f56d45da86068272ae149381818971aeeaa3d22b440f` |
| `docs/rfcs/RFC-BPR-0052-review-workbench-scaffolding.md` | 257356–257575 | `61b0979a57f2bcd7d57e16c90cefc41e67522c43f396a5c51e12fa9711f04696` |
| `docs/issues/README.md` | 6844–6897 | `7ea15a4445e5d5b62f28214ac37bb3a4581271287762cfa77683bd0f3b3cc55c` |
| `docs/issues/NEXT_ID` | 239112–239112 | `d8e542f46c64bd841901132e680f5c400a1e566f134fa142b2a6275d6ac0ea02` |
| `.mask/skill-config/implement-rfc/project.json` | 39034–39136 | `4915f1fc66672180b6ed892ef907b6c0a5c5ef6fd346ed005eab9acf0a8fa805` |
| `src/app/CMakeLists.txt` | 703413–704085 | `d3298bab733dcf95a8fb322a9f466e006bf5094d33d89a5da76942453a862252` |
| `CMakeLists.txt` | 7231–7392 | `f2e63d98df8f07860e9f1a8b53c761ea7c97fe9917a99f211efebd97fd9b9529` |
| `src/core/include/bpr/core/review_ledger.hpp` | 820188–820544 | `3fc1289603b424d9d01c1247fe86ef7784300e4ad6dd0ca1f85c7a18585bde48` |
| `src/core/include/bpr/core/review_round_trip.hpp` | 820912–821171 | `08327211d149c241e207ca9e2c7e234f14b9786d423c3d2696acb3845c0583ab` |
| `src/core/include/bpr/core/workbench_interaction.hpp` | 821941–822376 | `60dcf85f817ed4f7ede79357ac5bb86e9d41988e8d7ca5a5f6d8d1650089793f` |
| `src/application/include/bpr/application/git_cache_port.hpp` | 755115–756079 | `588e48504aeddc1a8b102c3d1506b82f25faa47ec3edb3f689579c7e1872faa9` |
| `src/application/include/bpr/application/review_ledger_port.hpp` | 765367–765675 | `acb6f407258a195efa985b05b57dcf1387ae12706888badffcb911781ef0568e` |
| `src/application/include/bpr/application/workbench_application_host.hpp` | 768322–768439 | `ac0a42902964544e42bf394d96ee84828d819edd87b421a7f287d8f234fed589` |
| `src/adapters/git/src/installed_git_cache.cpp` | 557159–563875 | `541632348a21bfa929fea8d403377e7b843acbc9897e29412e897760cc4cd76c` |
| `src/adapters/sqlite/src/review_ledger_store.cpp` | 695034–695304 | `33ddaa2ae418412934e49ea3b2f303fa1a7cb6831372c9125839710f4dd0494f` |

## Evidence boundaries

A status string, existing test source or imported design is not proof of current
runtime support. At intake record actual HEAD, remaps, component availability, current
native evidence and predecessor gates. Do not run the user's code, repository hooks,
model integrations or remote mutations merely to resolve this audit. Use fixture and
source-level observations until the applicable runtime action is separately authorized.
