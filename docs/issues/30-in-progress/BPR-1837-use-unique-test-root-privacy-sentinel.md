---
schema: "implement-rfc.card.v1"
id: "BPR-1837"
title: "Use the unique temporary parent as the log privacy sentinel"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: ["BPR-1831"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"release_issue":"BPR-1811","related_cards":["BPR-1836"],"mandate":"release-v041-full-stable-N8-oracle-followup","external_state":"/Users/jaeger/.codex/release-runs/v0.4.1-20261029","frozen_release_source":"d9d4a822dbf603c7bb22c83574f5a64d2bee5645","integration":"Test privacy-oracle correction only; frozen product sources, publisher, configuration and producers remain D9."}
---

# Use the unique temporary parent as the log privacy sentinel

After the independent category predicate is corrected, the private macOS
Release application-logging course reaches its later privacy assertion and
fails. The course assumes root.filename() is a unique random token, but
private_data_root deliberately returns <QTemporaryDir>/data. Searching every
log byte for the fixed word data rejects path-free storage.database events.

## Acceptance

- AC1: Preserve full native-root, repository and all other privacy/schema
  checks. Compare case-folded full-root spellings and the actual unique
  randomized temporary-parent leaf, with safe storage.database positive
  evidence and full/random-root disclosure, slash and case negative controls.
- AC2: Replay complete affected native public courses against unchanged
  product inputs. Preserve the old private image, failure and source identity;
  no privacy or terminal acceptance is inferred before the full courses pass.
- AC3: Rebuild only this changed test translation unit/object/link in a distinct
  private version, with original flags, helpers, headers and libraries sealed.
  Retain valid unrelated images/objects, complete focused format/documentation
  checks and leave the future-main commit unpushed until stable compatibility
  is reconciled.
