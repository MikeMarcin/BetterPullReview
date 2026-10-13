# Header/source navigation mandate r4

Original authorization: implement `docs/rfcs/drafts/header-source-navigation/RFC-DRAFT-0013-header-source-navigation.md` using coordinator/implementer.

Q1 owner answers (2026-10-12), in order:

1. “If there is an obvious candidate go to it, if not do nothing.”
2. “I would say closest match with same basename, i think its pretty common to have someting like src/a/b/c/file.cpp and inc/a/b/c/file.h even if you also had src/a/b/d/file.cpp and inc/a/b/d/file.h. src/a/b/c/file.cpp and inc/project/a/b/c.h are also pretty common”
3. “I meant src/a/b/c/file.cpp and inc/project/a/b/c/file.h are also pretty common”

Coordinator interpretation communicated after clarification: exact same-directory candidates first; otherwise longest common trailing directory-component sequence; unique best jumps, equal best no-op, single candidate jumps even with zero overlap. Same basename stays strict. No chooser, cycling or parent-folder aliases.

Initial assignment owns first bounded milestone only; coordinator dispatches the successor separately. No external mutations or human acceptance are authorized or inferred by these answers.
