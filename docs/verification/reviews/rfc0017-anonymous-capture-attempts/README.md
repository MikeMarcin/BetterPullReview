# RFC-0017 Anonymous Capture Attempt Registry

This directory is the durable, repository-wide single-issuance registry for
RFC-0017 anonymous contract-capture packets. Capture mode creates exactly one
`<packet-sha256>.json` receipt here with exclusive creation, flushes and syncs
the receipt, then syncs this directory before issuing any request. When this
directory is first created in another checkout, its parent is synced too.

Each receipt contains only schema metadata, the authorized course identity,
and SHA-256 bindings for the packet, response, authority, and recorder. A
receipt records that the attended authority was attempted, including when a
later network or sanitization step fails; it must not be deleted to retry a
consumed authorization. A fresh packet and exact operator response are needed
for another attempt.
