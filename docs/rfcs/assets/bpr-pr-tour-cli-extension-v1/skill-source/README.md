# pr-tour candidate source

This folder contains the proposed portable skill payload for RFC-BPR-0046. It is not installed here, and its CLI dependency is not implemented by this archive. Integrate it only after contract tests pass against the real new `bpr tour` binary.

The source snapshot uses the shared AgentSkills source through MASK. The implementation must locate that canonical source once, add `skills/pr-tour` there through the normal workflow, preserve source/version provenance, and install selections through MASK rather than create divergent BPR-local Claude/Codex skill copies. No exact MASK command syntax is invented in this pack; use the installed tool's current interfaces. Do not change implement-rfc or introduce new worker roles.

The BPR repository owns CLI contracts and runtime tests. The shared skill repository owns the thin skill text and its integration tests. Generated distribution copies, if used by existing packaging, must identify their source revision. Missing authorized upstream access blocks that distribution slice, not the manual CLI implementation. Package privacy/licensing and release publication follow existing project policies; this archive does not grant remote push authority.

The four writing hooks are intentionally not duplicated here. `assist context` obtains the effective settings/defaults from the existing review-preparation service. The skill adds workflow guidance only.
