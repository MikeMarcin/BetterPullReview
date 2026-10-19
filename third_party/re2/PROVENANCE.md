RE2 2025-11-05

Source: https://codeload.github.com/google/re2/tar.gz/refs/tags/2025-11-05

SHA256: 87f6029d2f6de8aa023654240a03ada90e876ce9a4676e258dd01ea4c26ffd67

Archive size: 397504 bytes. Built as a static library by `cmake/BprRe2Provider.cmake`
from the verified archive, without tests, benchmarks, install rules, ICU or PCRE.
It is source search's regex engine ([[BPR-1431]]). Since [[BPR-1618]], core's
private `source_regex_engine.cpp` implements the same `source_regex_engine`
interface; no RE2 type crosses the core API into the application host or Qt.
The owner's 2026-10-18 exception requires externally deterministic matching
and no worker creation. BSD-3-Clause license retained alongside this record.

License SHA256: 6040cda75d90b1738292a631d89934c411ef7ffd543c4d6a1b7edfc8edf29449.
