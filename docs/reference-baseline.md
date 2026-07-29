# Reference Baseline

This dated baseline records external interfaces assumed by the RFCs. Revalidate it during implementation; live server introspection, installed tool behavior, repository policy, and current official documentation override this file.

Baseline date: 2026-07-29

## Pack-local architecture authority

- [Deterministic Functional Core, Imperative Host, and Qt Presentation Adapter](architecture/deterministic-functional-core-imperative-host-qt-presentation-adapter.md)
- [BetterPullReview System Architecture](architecture/system-architecture.md)
- [Architecture Conformance Plan](verification/architecture-conformance.md)

## Native platform and rendering

- Qt 6.11 supported platforms: https://doc.qt.io/qt-6/supported-platforms.html
- `QAbstractScrollArea`: https://doc.qt.io/qt-6/qabstractscrollarea.html
- `QTextLayout`: https://doc.qt.io/qt-6/qtextlayout.html
- `QStyleHints`: https://doc.qt.io/qt-6/qstylehints.html
- `QAccessibilityHints`: https://doc.qt.io/qt-6/qaccessibilityhints.html
- `QFontDatabase`: https://doc.qt.io/qt-6/qfontdatabase.html

## Syntax highlighting

- KSyntaxHighlighting module: https://api.kde.org/ksyntaxhighlighting-module.html
- `AbstractHighlighter`: https://api.kde.org/ksyntaxhighlighting-abstracthighlighter.html
- `Repository`: https://api.kde.org/ksyntaxhighlighting-repository.html

## GitHub and GHES

- GHES 3.18 REST API versions: https://docs.github.com/en/enterprise-server@3.18/rest/about-the-rest-api/api-versions
- GHES 3.18 pull-request GraphQL surface: https://docs.github.com/en/enterprise-server@3.18/graphql/reference/pulls
- GHES 3.18 GraphQL mutations: https://docs.github.com/en/enterprise-server@3.18/graphql/reference/mutations
- GHES 3.18 REST OpenAPI description: https://docs.github.com/en/enterprise-server@3.18/rest/about-the-rest-api/about-the-openapi-description-for-the-rest-api
- GitHub CLI authentication: https://cli.github.com/manual/gh_auth_login
- GitHub CLI API command: https://cli.github.com/manual/gh_api

## AI agent interfaces

- Codex App Server: https://developers.openai.com/codex/app-server
- Codex non-interactive mode: https://developers.openai.com/codex/non-interactive-mode
- Codex sandboxing: https://developers.openai.com/codex/sandboxing
- Claude Code CLI: https://docs.anthropic.com/en/docs/claude-code/cli-usage
- Claude Code SDK/subprocess mode: https://docs.anthropic.com/en/docs/claude-code/sdk
- Claude Code on Amazon Bedrock: https://docs.anthropic.com/en/docs/claude-code/amazon-bedrock
- Model Context Protocol: https://modelcontextprotocol.io/
