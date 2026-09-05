---
schema: "implement-rfc.card.v1"
id: "BPR-0429"
title: "The syntax provider installs four configurations over one library"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: []
---

# The syntax provider installs four configurations over one library

With [[BPR-0428]] letting the nested KSyntaxHighlighting configure see all
four Visual Studio configurations, the `windows-dev` build got as far as
linking the application and stopped there:

```
KF6SyntaxHighlighting.lib(definition.obj) : error LNK2038: mismatch detected for
  '_ITERATOR_DEBUG_LEVEL': value '0' doesn't match value '2' in main.obj
KF6SyntaxHighlighting.lib(definition.obj) : error LNK2038: mismatch detected for
  'RuntimeLibrary': value 'MD_DynamicRelease' doesn't match value 'MDd_DynamicDebug' in main.obj
better_pull_review.exe : fatal error LNK1319: 32 mismatches detected
```

The provider builds and installs each configuration into one prefix, and
KSyntaxHighlighting names its library the same way in every one of them. So
the prefix holds exactly one `lib/KF6SyntaxHighlighting.lib`, all four
`KF6SyntaxHighlightingTargets-<config>.cmake` files point
`IMPORTED_LOCATION_<CONFIG>` at it, and whichever install ran last — here
RelWithDebInfo, at 20:41, three minutes after Debug's — is the one every
configuration links. A Debug application therefore links a release-runtime
library, and the linker refuses.

This is the first time the Qt graph has linked on Windows at all. The main
checkout's 2026-08-17 `windows-dev` tree carries the same single library and
no application executable; [[BPR-0338]] recorded compiling the graph, not
linking it. On macOS and Linux the generator is single-configuration and the
prefix holds the one library the build asked for, so nothing there can reach
this.

Measured on this host: the prefix's four `Targets-*.cmake` files all name
`${_IMPORT_PREFIX}/lib/KF6SyntaxHighlighting.lib`; the install logs are
timestamped Debug 20:37, Release 20:38, MinSizeRel 20:40, RelWithDebInfo
20:41; and the only errors in the whole 573-line build log are the 64
`LNK2038` lines on the two targets that link the library — every compile
unit in the Qt graph built.

## Required final state

- On a multi-configuration generator the nested configure receives a
  distinct library postfix for every configuration other than Release
  (`CMAKE_DEBUG_POSTFIX`, `CMAKE_MINSIZEREL_POSTFIX`,
  `CMAKE_RELWITHDEBINFO_POSTFIX`), so each install writes its own library
  and its own `IMPORTED_LOCATION_<CONFIG>` names it.
- The postfixes are part of the provider's stamp signature, so a prefix
  built before this change is rebuilt rather than trusted.
- Single-configuration generators forward nothing new and the macOS and
  Linux prefixes are byte-for-byte unchanged.
- A test fails without the change: it reads the installed
  `Targets-<config>.cmake` files of a multi-configuration prefix and requires
  the imported locations to be pairwise distinct, and it records itself as
  not applicable rather than passing on a single-configuration tree.
- `windows-dev` links `better_pull_review.exe` in Debug, which [[BPR-0412]]
  records.

## Verification

```sh
cmake --preset windows-dev
cmake --build --preset windows-dev
ctest --preset core-only --output-on-failure -R '^dependency_modernization_contract$'
```

## Result

The provider now forwards `CMAKE_<CONFIG>_POSTFIX=_<config>` for every
configuration but Release when the generator is multi-configuration, and the
rebuilt prefix holds four libraries with four exports naming them:

```
DEBUG           ${_IMPORT_PREFIX}/lib/KF6SyntaxHighlighting_debug.lib
MINSIZEREL      ${_IMPORT_PREFIX}/lib/KF6SyntaxHighlighting_minsizerel.lib
RELEASE         ${_IMPORT_PREFIX}/lib/KF6SyntaxHighlighting.lib
RELWITHDEBINFO  ${_IMPORT_PREFIX}/lib/KF6SyntaxHighlighting_relwithdebinfo.lib
```

`test_syntax_prefix_installs_one_library_per_configuration` failed on the
previous prefix (`1 != 4: configurations share an installed library`) and
passes on this one; on the `core-only` tree it records itself as not
applicable. The `windows-dev` build then completed with zero link errors and
produced `build/windows-dev/src/app/Debug/better_pull_review.exe` — the first
time the Qt graph has linked on Windows. The rebuild had to be run twice,
because Smart App Control refused the framework's freshly built indexer on
the first attempt ([[BPR-0424]]); nothing in this change is affected by that.
