Role: rfc-reviewer-critical | Model: gpt-6-astra | Effort: xhigh

Reviewed frozen plan at baseline aa3d71fe470627e1e78488eb51a9114a788f33bd. Independent plan review returned two blocking findings:

1. Strict AC01/AC02: BPR admits tenant.ghe.com, github.localhost and subdomains as GHES, but stock gh classifies these into GH_TOKEN use. Supplying only GH_ENTERPRISE_TOKEN permits stored-token fallback. Resolve with verified hostname mapping or typed refusal before credentialed dispatch and negative API/helper evidence.
2. Companion AC03: stock gh 2.101 TokenForUser discards Keychain lookup error and auth token emits identical failure for absent and inaccessible named items with private token-free configuration. Distinct missing/denied text is unobservable through selected mechanism. Requires explicit amendment or separately authorized supported observation before companion acceptance. Does not block independent strict execution.

Non-blocking verification obligations: composed failed Connect/startup drains automatic discover_github_cli_accounts(false), whose ambient auth status can network-check accounts; preserve immutable scope through cancellation-after-write reconciliation with empty stop token and no second POST; native helper evidence must bind actual gh-owned helper invocation and authentication-dependent negative control, because API discovery plus public clone is insufficient.

Plan-only review; no implementation/native credential evidence. Hostname admission syntax assertions passed. No credentials, mutations, edits or background commands. Post-implementation critical review remains pending.
