---
id: BPR-0114
title: Verify System mode follows a live OS scheme change
status: Backlog
rfc: RFC-BPR-0006
depends_on: [BPR-0106]
---

# Verify System mode follows a live OS scheme change

On the real macOS composition, prove that System mode follows an operating-system colour-scheme change while an explicit mode does not.

DEFERRED CLASS: owner-attended. It needs the desktop owner to authorise changing and restoring the global appearance, and to observe the result. The static fresh-install-defaults-to-System claim is separate and is proven unattended by BPR-0106. Batch this with the other attended checks.
