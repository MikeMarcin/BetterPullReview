# v004 logged sign-in result — 2026-09-05

The approved executable was launched once, PID 3736. Viewer observation
succeeded; the capability response was HTTP 200 with a GraphQL error envelope.
The user and native UI reported the same fixed capability failure.

The sanitized local log identified `INTROSPECTION_LIMIT_EXCEEDED`: this endpoint
allows introspection fields at most twice per query, while the compiled document
uses `__Type.fields` 17 times, `__Field.args` 17 times, `__Type.enumValues` three
times and `__Type.inputFields` four times. This is a concrete query-policy cause,
not a token-exchange or viewer-identity failure. The recorded schema-aware
fixture did not exercise GitHub's additional introspection repetition limit.

The agent explicitly clicked Sign out. Native UI then showed “No GitHub host
is connected.” The normal window close completed with exit 0, and the local
logger recorded shutdown. No PR opened, no publication occurred, and no second
sign-in or launch was attempted. V004 authority is consumed.

The complete live log remains local and is not committed. Only this minimal
sanitized contract finding is recorded. Root repair proceeds offline: reduce
introspection repetition, prove the observed endpoint limit with a regression,
and preserve bounded response/cancellation/authority behavior. A changed binary
is not authorized for another live run by this spent approval.
