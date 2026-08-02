# Reference Baseline

This dated baseline records external interfaces assumed by the RFCs. Revalidate it during implementation; live server introspection, installed tool behavior, repository policy, and current official documentation override this file.

Baseline date: 2026-07-30

## Pack-local architecture authority

- [Deterministic Functional Core, Imperative Host, and Qt Presentation Adapter](architecture/deterministic-functional-core-imperative-host-qt-presentation-adapter.md)
- [BetterPullReview System Architecture](architecture/system-architecture.md)
- [Architecture Conformance Plan](verification/architecture-conformance.md)

## Native platform and rendering

- Qt 6.11 supported platforms: https://doc.qt.io/qt-6/supported-platforms.html
- Qt 6.11 `QProcess` contract and Unix process flags:
  https://doc.qt.io/qt-6/qprocess.html
- Qt 6.11 `QProcess` lifecycle implementation:
  https://github.com/qt/qtbase/blob/6.11/src/corelib/io/qprocess.cpp
- Qt 6.11 Unix process implementation:
  https://github.com/qt/qtbase/blob/6.11/src/corelib/io/qprocess_unix.cpp
- Qt 6.11 Darwin environment observation:
  https://github.com/qt/qtbase/blob/6.11/src/corelib/io/qprocess_darwin.mm
- Qt 6.11 Windows pipe reader:
  https://github.com/qt/qtbase/blob/6.11/src/corelib/io/qwindowspipereader.cpp
- `QAbstractScrollArea`: https://doc.qt.io/qt-6/qabstractscrollarea.html
- `QTextLayout`: https://doc.qt.io/qt-6/qtextlayout.html
- `QStyleHints`: https://doc.qt.io/qt-6/qstylehints.html
- `QAccessibilityHints`: https://doc.qt.io/qt-6/qaccessibilityhints.html
- `QFontDatabase`: https://doc.qt.io/qt-6/qfontdatabase.html

## Git process, smart transport, and retained refs

- Git executable and environment contract:
  https://git-scm.com/docs/git
- Git configuration scopes and environment:
  https://git-scm.com/docs/git-config
- Git HTTP smart protocol:
  https://git-scm.com/docs/gitprotocol-http
- Git protocol version 2:
  https://git-scm.com/docs/gitprotocol-v2
- `git index-pack` bounded pack ingestion:
  https://git-scm.com/docs/git-index-pack
- Git pack format:
  https://git-scm.com/docs/gitformat-pack
- Atomic ref transactions:
  https://git-scm.com/docs/git-update-ref
- GHES 3.18 pull-request ref checkout:
  https://docs.github.com/en/enterprise-server@3.18/pull-requests/how-tos/review-pull-requests/checking-out-pull-requests-locally

## Syntax highlighting

- KSyntaxHighlighting module: https://api.kde.org/ksyntaxhighlighting-module.html
- `AbstractHighlighter`: https://api.kde.org/ksyntaxhighlighting-abstracthighlighter.html
- `Repository`: https://api.kde.org/ksyntaxhighlighting-repository.html

## GitHub and GHES

- GitHub.com pull-request reviews, including chronological response order:
  https://docs.github.com/en/rest/pulls/reviews?apiVersion=2022-11-28#list-reviews-for-a-pull-request
- GHES 3.18 pull-request reviews, including chronological response order:
  https://docs.github.com/en/enterprise-server@3.18/rest/pulls/reviews?apiVersion=2022-11-28#list-reviews-for-a-pull-request
- GHES 3.18 REST API versions: https://docs.github.com/en/enterprise-server@3.18/rest/about-the-rest-api/api-versions
- GHES 3.18 pull-request GraphQL surface: https://docs.github.com/en/enterprise-server@3.18/graphql/reference/pulls
- GHES 3.18 GraphQL mutations: https://docs.github.com/en/enterprise-server@3.18/graphql/reference/mutations
- GHES 3.18 REST OpenAPI description: https://docs.github.com/en/enterprise-server@3.18/rest/about-the-rest-api/about-the-openapi-description-for-the-rest-api
- GitHub CLI authentication: https://cli.github.com/manual/gh_auth_login
- GitHub CLI active-account selection:
  https://cli.github.com/manual/gh_auth_switch
- GitHub CLI API command: https://cli.github.com/manual/gh_api
- GitHub CLI respected persisted configuration:
  https://cli.github.com/manual/gh_config
- GitHub CLI environment and default configuration directories:
  https://cli.github.com/manual/gh_help_environment
- GitHub CLI 2.96.0 API URL construction:
  https://github.com/cli/cli/blob/v2.96.0/pkg/cmd/api/http.go
- GitHub CLI 2.96.0 HTTP client and redirect-auth handling:
  https://github.com/cli/cli/blob/v2.96.0/api/http_client.go
- Historical GitHub CLI 2.95.0 API URL construction comparison:
  https://github.com/cli/cli/blob/v2.95.0/pkg/cmd/api/http.go
- Historical GitHub CLI 2.95.0 HTTP client and redirect-auth comparison:
  https://github.com/cli/cli/blob/v2.95.0/api/http_client.go

## SQLite epoch catalog

- SQLite open flags and `sqlite3_open_v2`:
  https://sqlite.org/c3ref/open.html
- Defensive connection configuration:
  https://sqlite.org/c3ref/c_dbconfig_defensive.html
- Foreign-key enforcement:
  https://sqlite.org/foreignkeys.html
- Write-ahead logging:
  https://sqlite.org/wal.html
- `application_id`, `user_version`, `synchronous`, and `trusted_schema`
  pragmas:
  https://sqlite.org/pragma.html
- SQLite atomic commit behavior:
  https://sqlite.org/atomiccommit.html
- SQLite VFS registration and file-operation contract:
  https://sqlite.org/c3ref/vfs.html
- SQLite runtime limits:
  https://sqlite.org/limits.html
- Windows `CreateFileW`, including reparse-point and share-mode behavior:
  https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew
- Windows file identity and delete-pending/link-count information:
  https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfileinformationbyhandleex
- Windows access tokens and file security information:
  https://learn.microsoft.com/en-us/windows/win32/secauthz/access-tokens
  and
  https://learn.microsoft.com/en-us/windows/win32/api/aclapi/nf-aclapi-getsecurityinfo
- Windows volume capability inspection:
  https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getvolumeinformationbyhandlew
- Windows handle-relative, no-replace file rename information:
  https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_rename_info

BetterPullReview acquires pinned SQLite 3.53.4 with download-only CPM 0.43.1,
released 2026-07-24, from the official
`sqlite-amalgamation-3530400.zip`. Its `SQLITE_SOURCE_ID` is
`2026-07-24 19:02:57 bf7c7f30031888f4e796e429ab3978879485813aaca6f641c7b33e4e09459bcc`.
The official archive SHA3-256 is
`628a44cfe82c66aed1ccbbe85a562d2e33ebe64b3288981ed76285612227934e`,
and the published `sqlite3.c` SHA3-256 is
`67f423e9ebbbdc473cbc4772c872ee6b89f31fde4ed0279a5c25d5f65c043a16`.
The repository intake measurement for `sqlite3.h` is
`89d0de498b1012938bb78dc18b68fea52e26f7f7b6db6347d126a2e58b86a05c`.
Reproducible intake, official URLs, and the compile/runtime policy are recorded
in [`PROVENANCE.md`](../third_party/sqlite/PROVENANCE.md) and
[`COMPILE_OPTIONS.md`](../third_party/sqlite/COMPILE_OPTIONS.md). The static
target has no system-library fallback, and no amalgamation input is tracked in
the repository.

## AI agent interfaces

- Codex App Server: https://developers.openai.com/codex/app-server
- Codex non-interactive mode: https://developers.openai.com/codex/non-interactive-mode
- Codex sandboxing: https://developers.openai.com/codex/sandboxing
- Claude Code CLI: https://docs.anthropic.com/en/docs/claude-code/cli-usage
- Claude Code SDK/subprocess mode: https://docs.anthropic.com/en/docs/claude-code/sdk
- Claude Code on Amazon Bedrock: https://docs.anthropic.com/en/docs/claude-code/amazon-bedrock
- Model Context Protocol: https://modelcontextprotocol.io/
