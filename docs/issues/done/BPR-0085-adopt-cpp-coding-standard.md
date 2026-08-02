---
id: BPR-0085
title: Adopt and apply the portable C++ coding standard
status: Done
rfc: RFC-BPR-0004
depends_on: [BPR-0084]
---

# Adopt and apply the portable C++ coding standard

Render the supplied project-neutral C++ coding-standard package with stable
BetterPullReview identifiers, integrate its local formatting and clang-tidy
targets with the centralized dependency graph, and make the resulting standard
the repository source of truth for first-party C++ style.

Migrate the complete first-party C++ source and test tree as one coherent
boundary. Preserve serialized keys, protocol text, command-line behavior,
third-party APIs, architectural ownership, and existing public binary names.
Document any deliberate project-specific adaptation to the neutral package.

## Required direct evidence

- The rendered standard contains no template tokens and the source-discovery
  scripts exclude build, generated, and vendored code.
- Every first-party C++ file passes the installed clang-format check.
- Strict clang-tidy passes against a fresh compilation database, including the
  adopted naming checks, or any toolchain false positive has a narrow,
  documented configuration exception.
- Clean developer and core-only configure/build/test matrices pass, along with
  the documentation validator and `git diff --check`.
- The public build and contribution documentation identifies the canonical
  standard, local targets, direct scripts, prerequisites, and adoption scope.

## Closeout

Run the required behavior-preserving simplification review over the migrated
surface. Record the actual verification result before moving this card to
`done/` and committing the completed slice.

## Result

The project-neutral package is rendered under `_coding-standard/` with
BetterPullReview identifiers and `NOASSERTION` licensing, while the root
`.clang-format`, `.clang-tidy`, and `.editorconfig` files are the canonical
editor and analysis configuration. CMake exposes opt-in-aware
`coding-format`, `coding-format-check`, and `coding-tidy` targets through the
existing Python dependency owner. Target categories now distinguish the app
and tests without changing their emitted executable names.

All 167 first-party C++ files were canonically formatted. The source and test
tree now follows the rendered identifier policy, uses project include guards,
and documents public types and namespace-level functions. Narrow adaptations
for Qt-generated names, compiler/toolchain diagnostics, and intentional
moved-from test probes are recorded beside the policy or suppression.
Source-scanning contracts were made whitespace-tolerant where formatting was
never part of the asserted behavior.

The final simplification review found no remaining behavior-changing cleanup:
branch consolidation and string construction changes are direct, local, and
covered by the existing contracts. Verification passed with 74/74 developer
tests, 33/33 core-only tests, zero strict first-party clang-tidy diagnostics,
167/167 formatted files, `DOCUMENTATION_OK`, and a clean `git diff --check`.
