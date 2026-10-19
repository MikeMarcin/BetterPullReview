# BPR-1621 architecture enforcement audit

Mandate `determinism-enforcement-r1`, milestone
`determinism-enforcement-v1/boundary-cleanup`, baseline
`740ec08f84e7ee9755b2c54e0082aedcb008b3ac`.
The policy remains [external determinism](../../../architecture/deterministic-functional-core-imperative-host-qt-presentation-adapter.md#21-external-determinism).
This slice changes mechanical guards and their contracts, without changing the
product, adopting dependencies or closing wider RFC/native/release obligations.

## Changed restrictions

| Previous guard | Revised guard |
|---|---|
| `<thread>`, `<future>`, `<ctime>`, `<condition_variable>` and `<fstream>` rejected by inclusion | Headers and harmless value/reference declarations are allowed. Known nonempty thread/stream constructors, default/async `std::async`, scheduling/thread observations and named standard file-stream `open` forms remain guarded. Empty thread/stream objects acquire nothing. |
| Every `::now()` rejected | Standard chrono and file-clock acquisition are checked by canonical names and simple same-file namespace/type aliases. Deterministic domain `observed_clock::now()` and chrono values/arithmetic are allowed. |
| C effect names matched arbitrary domain member/qualified functions | Unqualified/global/standard acquisition APIs remain guarded; domain members and qualified domain functions named `time`, `clock`, `random` or `connect` do not imply those APIs. |
| Every HTTP(S) string in core rejected twice | Generic URL strings/comments are core data in both guards. Specific provider endpoint/auth/wire ownership and Qt's URL-free projection contract remain. |
| Only C++ suffixes scanned by the core guard | C and Objective-C/Objective-C++ suffixes now receive the same guard, matching the wider architecture scan. This closes an extension bypass, without approving a language/backend capability. |

Explicit `std::async(std::launch::deferred, callable)` creates lazy caller-owned
state without launching a worker. Only that explicit policy prefix is exempted;
the callable still scans for effects and must obey the explicit-state contract.
Default, async and mixed policies remain rejected. Simple namespace, `using`
and `typedef` aliases are normalized for the named capabilities, along with
scope-operator spacing. No general alias/dataflow parser was added.

Core boundary contracts supply positive and negative cases for every revised
class, retaining entropy/environment, filesystem, network/process, vault,
Qt/dependency and exact native-wrapper negatives. The larger architecture
contract accepts generic core links and standard data while retaining Qt and
provider counterexamples. The initial regression run failed on the stale header
ban and previously unguarded header-free worker/file acquisition forms; the
corrected candidate passes these cases.

## Retained policy classes

| Guard family inspected | Reason retained |
|---|---|
| Clock/entropy/environment acquisition; filesystem/network/process/vault effects | Implicit application inputs and host effects remain forbidden. Lexical paths, chrono arithmetic, explicitly seeded PRNGs and caller-owned atomics/mutexes remain allowed. Hidden global decision state and semantic dependence on timing remain review obligations. |
| Qt-free core/application, inward graph, provider/credential contracts | Ownership and reuse are independent of determinism. Raw GitHub endpoint/auth/wire literals and Qt's URL-free projections preserve the adopted transport/presentation split. A generic URL is insufficient evidence of networking. |
| QJson, nlohmann, rapidjson and boost/json restrictions | Approved serialization/dependency choices and wire ownership; JSON computation is not intrinsically nondeterministic. Glaze remains approved. No alternative library permission is granted. |
| Core link inventory, private RE2 include, transitive-only Abseil, exact CommonCrypto/CNG owners and Windows bcrypt branch | Adoption, encapsulation and API ownership remain separate. No arbitrary RE2/Abseil/native OS APIs are approved. Native wrappers still reject clock/entropy capabilities. |
| SHA-1 collision detection, HACL and rapidfuzz owners, dtl's bounded uses, private xdiff wrapper and separately shared xdiff | Approved algorithms/use scope, licensing and replacement/relinking requirements remain. These are not source-purity exceptions. |
| Platform descriptors/handle lists, process-support/workers and native adapter capabilities | Exact effect/lifecycle/platform owners remain. Internal synchronization does not authorize worker creation or host scheduling. |
| Logging/Quill ownership, browser/listener/telemetry/runtime restrictions | Externally observable effects and the native product runtime contract remain. Internal library timing/layout variation grants no application logging/service capability. |
| Central providers and package graph/runtime audits | Pins, controlled acquisition, toolchain discovery, notices, include/link directions and selected runtime inventory remain. These checks do not prove determinism. |

Reviewed surfaces include `cmake/VerifyCoreBoundary.cmake`, core, projection,
dependency, platform and effect-owner checks in
`cmake/VerifyArchitectureBoundaries.cmake`, central `BprDependencies.cmake`,
provider ownership/pins including RE2/Abseil and xdiff, platform/dependency
contracts, Git protocol ownership contracts and selected graph checks in
`scripts/verify_release_package.py` and
`scripts/verify_dif_asset_boundaries.py`. Untouched checks reuse unchanged
implementation; the full architecture contract exercises their composition.

## Limits and self-review

These are textual source/build guards. They do not parse/type-check C++ or prove
determinism, absence of effects/workers, upstream behavior or absence of hidden
global decision state. Macros, indirect calls, aliases from headers, alias chains,
scope shadowing, complicated declarations and resource lifecycle can escape
detection. Raw-text checks elsewhere may flag inert comments/literals.
Nonempty thread/stream construction can be a harmless handle move or declaration;
the guard conservatively rejects that form. Filesystem symbol checks can reject
pure overloads such as `is_directory(file_status)` and inert iterator/entry type
references because distinguishing them from access requires type resolution.
Lexical path computation is explicitly supported.

These limitations are not new semantic prohibitions or permission for effects.
Review must resolve actual semantics under the general rule. Further guard
refinement needs positive and negative public-path evidence. No whole C++ parser,
new dependency/backend adoption or product approval checklist belongs to this
cleanup. Self-review compared each changed restriction with the retained policy
classes and direct regression counterexamples. This scripts/documentation-only
milestone introduces no required human/native/independent acceptance.
