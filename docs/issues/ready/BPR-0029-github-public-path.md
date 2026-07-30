---
id: BPR-0029
title: Wire the RFC-BPR-0002 public application path
status: Ready
rfc: RFC-BPR-0002
depends_on: [BPR-0027, BPR-0028]
---

# Wire the RFC-BPR-0002 public application path

Select explicit host profiles, transport, credential adapter, provider facade,
application host, core transitions, and Qt projection in the composition root.
Prove a read-only fixture PR traverses the real QAction-to-provider-to-rendered
application path for GitHub.com and GHES 3.18.

The composition root is the only layer allowed to select concrete transports
and platform credential adapters.

Native credential persistence must not be composed until the public path can
recover its exact profile/profile-revision/host/account/server authority after
process restart without accepting a caller-supplied account selector. Provide
either a vault-provenance or integrity-backed durable binding-rehydration
boundary, or exact-delete every retained credential before normal shutdown
completes. Settings, SQLite, login text, profile text, and an untrusted raw
account value cannot seed that selector. If neither design is available,
native credential persistence remains unavailable even though BPR-0027 proves
same-process host recreation through a shared host-owned catalog.

The composition must also keep the decision executor alive and accepting
posts until the application host is quiescent, and keep the credential
binding catalog, provider, transports, vault, and projections alive for the
lifetimes documented by their ports.
