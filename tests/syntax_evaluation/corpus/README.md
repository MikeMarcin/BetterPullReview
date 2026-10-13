# C++ identifier evaluation corpus

The corpus and labels for
[RFC-BPR-0081](../../../docs/rfcs/RFC-BPR-0081-cpp-identifier-parser-evaluation.md)
("Corpus" and "Accuracy"), card BPR-1440. `manifest.json` describes the seven
classes. `tools/materialize_corpus.py` writes them as files. `tools/label_with_clang.py`
labels classes 1 to 3 from clang's AST and derives the fragment labels from
those. `tools/sample_for_review.py` draws the hand-review sheets.

Only the manifest, the tools, this file and the labels under `../labels/` are
committed. The materialized tree, third-party source and review sheets stay
in a scratch directory.

## Classes

| # | Class | What it holds | Label source |
|---|---|---|---|
| 1 | `contract` | `tests/fixtures/syntax/*.cpp`. Every C++ path of every commit in the `cpp-identifiers-v1`, `header-source-v1`, `uiforetw-74` and `large-review-v1` packs, deduplicated by blob id. The head side of the `directxmath-336` patches. | `contract` |
| 2 | `first-party` | Every C++ file under `src/` at commit `530d92eaff1578413c0e669f476e740387441152`, read with `git archive`, never from the working tree. | `first-party` |
| 2 | `first-party` (source `first-party-concat`) | Four L-class inputs in `first-party-concat/`, each made of whole first-party files in sorted path order. Files are added while the total stays at or under a target of 1,500,000, 3,000,000, 6,000,000 or 8,000,000 bytes, so each input stops before the first file that would pass its target. The entry records its `parents`. These inputs have no accuracy labels: joining files changes what one blob declares. | none |
| 3 | `third-party` | The C++ members of the Abseil, RE2, Glaze and Quill archives the build pins, plus the verified KSyntaxHighlighting 6.28.1 source tree in the CPM cache. Never committed. | `abseil`, `re2`, `glaze`, `quill`, `ksyntaxhighlighting` |
| 4 | `gap` | `tests/fixtures/syntax/evaluation/gap/*.cpp`, when that directory exists. | the integrator's `gap.jsonl` |
| 5 | `fragments` | Seeded share-sheet excerpts and hunks built from classes 1 to 3. | `fragments` (rule F) |
| 6 | `malformed` | Seeded malformed and pathological input. | none |
| 7 | `composer` | Six seeded edit sequences, each 150 whole-text steps. | none |

"C++" means a name ending in `.cpp .cc .cxx .c++ .C .h .hh .hpp .hxx .h++ .inl .ipp .tcc`,
matched without regard to case. A lowercase `.c` is C and is left out. The
evaluation harness drops any file that the classifier does not give the C++
definition.

`directxmath-336` records patches, not whole files. Its two entries are the head
side of each hunk (context and added lines) in order, so they hold the patched
region and not the whole head file.

### Third-party verification

Each archive is checked against the SHA-256 and byte size in the manifest
before any member is read. The values are the ones in
`cmake/BprRe2Provider.cmake`, `cmake/BprGlazeProvider.cmake` and
`cmake/BprQuillProvider.cmake`. KSyntaxHighlighting is not cached as an
archive. Its extracted source under `bprksyntaxhighlighting/<hash>/` is
accepted only when its source manifest digest equals
`BPR_KSYNTAXHIGHLIGHTING_SOURCE_SHA256`. That digest is computed as
`bpr_syntax_source_digest` in `cmake/BprSyntaxHighlightingProvider.cmake` does:
it is the SHA-256 of a text with one `<file sha256>  <relative path>\n` line
per regular file, sorted by path. The archive digest constant is recorded
beside it.

Some third-party C++ files include files that are not C++ (`.inc`, `.def`). The
materializer writes those under `support/<source>/` for the labeller's include
path. `index.json` lists them under `support`, and they are not corpus files.

### Fragments

Excerpts follow `note_code_view::show_excerpt` over `core::excerpt_note`. An
excerpt has one to four anchors, each one to three lines long, with two
context lines on each side (`cycle_anchor_context_lines`). Windows that meet,
or are one line apart, join. The excerpt holds only the shown rows, each
followed by LF. A row's line ending (LF or CRLF) is not part of it. Tabs expand
to the next multiple of 4 columns, counted in UTF-16 units from the code's
first column. Each omitted run between two windows becomes one empty line. The
row limit is the larger view's (20,000), so no anchor is cut.

Hunks are contiguous original lines, 20 to 200 of them, with their bytes and
line endings unchanged. Each begins strictly inside an enum body, a namespace
body, a block comment or a raw string, found by plain text search. There are
30 of each kind.

Each fragment's `index.json` entry records its `parent`, its `kind`, its
`tab_width` (4 for excerpts, `null` for hunks), and in `lines` the 1-based
parent line of every fragment line, with `null` for an omitted-run line.

### Malformed and pathological

- 16 parent files cut at a seeded byte offset, moved back to a UTF-8 boundary
- 16 parent files with 1 to 24 seeded deletions or insertions of `{ } ( ) [ ] < >`
- each of `( [ { <` nested 100,000 deep, both closed and open only
- a single 1 MiB line of namespace and enum declarations
- `{` repeated 8,388,600 times, and repeated exactly 8,388,608 times (the
  classifier's limit)
- an 8,388,608-byte enum body of short names
- 1 MiB of seeded random valid UTF-8

Every generated file is valid UTF-8 and is in the `pathological` size class.

### Composer replay

Three sequences start from contract files and three from first-party files,
each a parent of 4,000 to 80,000 bytes. Step `000` is the unedited parent.
Steps `001` to `150` each apply one edit:

- typing an enum or namespace declaration one to six characters at a time
- deleting a block of 5 to 20 lines and then adding it back
- pasting a 30-line function
- undoing back to one of the last five steps

Each step is a whole text in `composer/<sequence>/<step>.cpp`.

## Materializing

```sh
python3 -I tests/syntax_evaluation/tools/materialize_corpus.py \
    --repo-root "$PWD" \
    --cpm-cache "$PWD/build/.cpm-cache" \
    --out /absolute/scratch/corpus
```

The output directory must be new or empty. The tool makes no network access.
Running it again with the same inputs gives byte-identical output, including
`index.json`. Every random choice comes from a seed derived from the
manifest's `master_seed`, and the seed is recorded on the file it made.

Each `index.json` file entry has these fields:

- `id` and `path`: the path relative to the output directory
- `class` and `source`
- `bytes` and `sha256`
- `size_class`: one of `S` (under 64 KiB), `M` (64 KiB to 1 MiB), `L` (1 to
  8 MiB), `pathological` or `fragment`
- `parent` and `seed`: set on derived files
- `utf8`
- the class-specific fields described above

## Labelling

```sh
python3 -I tests/syntax_evaluation/tools/label_with_clang.py \
    --repo-root "$PWD" --corpus /absolute/scratch/corpus \
    --build-dir "$PWD/build/dev" --out tests/syntax_evaluation/labels --jobs 8
```

Labels come only from clang's AST through libclang, with the detailed
preprocessing record on, from clang's diagnostics and from its raw token
stream. Neither the shipped heuristic nor any candidate is consulted. Every
file is parsed like this:

```text
clang -x c++ (or -x c++-header for header suffixes) <flags>
      -isysroot <macOS SDK> -resource-dir <llvm>/lib/clang/<major>
      -ferror-limit=0 -Wno-everything
      (libclang options: detailed preprocessing record, keep going)
```

The `<flags>` depend on the class:

- **First-party:** the file's own command from `build/dev/compile_commands.json`.
  `-I` and `-isystem` paths under the worktree's `src/` are remapped onto the
  materialized `first-party/src/`. Output, dependency-file, architecture,
  warning, debug and hardening flags are removed. A file with no command
  borrows the first command, in path order, from the same directory or the
  nearest ancestor directory.
- **Third-party:** `-std=c++23`, the file's own directory, and the library's
  include roots from the manifest. RE2 also gets Abseil's roots. Each library
  also gets its `support/` directory. KSyntaxHighlighting also gets the Qt
  framework and include paths that the `dev` build's compile commands name,
  and the generated, export and `moc` headers under
  `build/dev/_bpr/ksyntaxhighlighting/6.28.1`.
- **Contract:** `-std=c++23`, the file's own directory, and the root of its
  blob or fixture directory.

A file with clang errors keeps its labels. `labels/coverage.json` records, for
every file:

- its error count, and how many of those errors are in the file itself
- which flags it was given (`compile_commands`, `borrowed:<path>`, or the
  class name)
- its status: `full` (no errors), `partial` (errors), or `none` (no
  translation unit)
- `unparsed`, the byte ranges of preprocessor branches clang skipped

Skipped branches have no AST, so the labeller gives them no labels. It also
records the clang version, a summary by status for each source, the most
common error messages (with quoted names elided) and the fragment drop counts.

### Label schema

`labels/<source>.jsonl` holds one compact JSON object per line, with its keys
sorted. A file larger than 1,000,000 bytes is written instead as `<source>.jsonl.gz`,
compressed with gzip with mtime 0. Records are sorted by file. Each file
starts with one protected record, and its labels follow in offset order.

```json
{"c":"protected","f":"<corpus path>","ranges":[[start,end],...]}
{"c":"<category>","d":<offset|null>,"e":"<expectation>","f":"<corpus path>","m":"clang","n":<bytes>,"o":<offset>,"r":"<rule>","s":<subcategory|null>,"t":"<text>"}
```

Offsets and lengths count bytes of the materialized file. Protected ranges are
half-open and merged. They cover:

- comment tokens
- string and character literal tokens, raw strings included
- whole preprocessor directive lines, with backslash-continued lines and any
  comment that carries a directive onto another line

No label lies inside a protected range or a skipped branch.

| Category | `e` | `s` | `r` | `d` |
|---|---|---|---|---|
| `ns_decl` | `namespace` | `simple`, `nested` (each name of `A::B::C`, `inline` skipped), `multiline` (the chain from `namespace` to the last name spans lines), `alias` (a namespace alias's new name), `using` (each name of `using namespace X::Y`) | `NS` | the offset of the `namespace` keyword |
| `enumerator_decl` | `enum_member` | the most specific of: `attribute` (carries `[[...]]`), `nested_initializer` (the initializer holds `( [ { <`), then `scoped` or `unscoped` | `EN` | the offset of the enum's opening brace |
| `enum_use` | `enum_member` | null | `EU` | the enumerator's declaration offset |
| `ns_qualifier` | `namespace` | null | `NQ` | the in-blob definition of the namespace or alias: the last one before the use, or else the first |
| `conservative_negative` | `neutral` | null | `R2` to `R8`, `F-outside` | the enumerator's declaration offset when it is in the blob, otherwise null |
| `macro_keep` | `macro` | null | `M1`, `M2` | null |

**`enum_use`.** This is the final name `Y` of a qualified use `Q::Y`: a
`DeclRefExpr` whose written qualifier chain precedes `::Y` and that clang
resolves to an enumerator declared in this blob. The last written qualifier
must name the enum itself or, for an unscoped enum, the scope that encloses
the enum. None of the rules below may apply.

**`ns_qualifier`.** This is each written name in a nested-name-specifier
(`NAMESPACE_REF`) that refers to a namespace or a namespace alias defined in
this blob. A name in a `using namespace` chain is an `ns_decl` instead.

**`ns_decl` with `using`.** These names are read from the token stream (the
`using namespace` keywords followed by a chain of names) rather than from
clang's AST. The contract labels `using namespace External::NAMESPACE`, whose
target clang cannot resolve.

**`macro_keep`.** `M1` is every macro expansion in the main file whose name is
ALL_CAPS: at least two characters, only `A-Z 0-9 _`, and at least one letter.
It comes from clang's preprocessing record, so it covers invocations,
object-like references and macros inside enum initializers. Directive lines
are protected and need no record. `M2` is an ALL_CAPS unqualified name that
clang reports as an undeclared identifier or unknown type name, and that is
not followed by `::`. The contract keeps its macro role through the ALL_CAPS
fallback (`GCT_ASSERT(chosen)`, `AFTER_ENUM`).

### Conservative rules

Each of these makes the final name `Y` of a qualified use neutral even when
clang resolves it to an enumerator. When several apply, the first in this
order is recorded:

- **R2:** the enumerator is declared outside this blob.
- **R4:** the chain passes through an alias: a namespace alias or, as the
  contract's `AliasReason::ID` shows, a type alias (`typedef` or `using`).
  Names after a namespace alias get no `ns_qualifier` label.
- **R6:** the qualifier is a template specialization, a `decltype`, or uses
  `template`, or the use follows `.` or `->`. These are read from the written
  tokens of the chain.
- **R5:** the enum or a qualifier is declared more than once with different
  kinds in the same scope of this blob, or the enumerator's name is
  duplicated in its enum. The evidence is the blob's AST declarations and
  clang's own errors for the cases its recovery hides from the AST: "use of
  'X' with tag type that does not match previous declaration", "redefinition
  of 'X' as different kind of symbol" and "redefinition of enumerator 'X'".
- **R3:** the head qualifier's spelling is also declared as a variable,
  parameter or data member in a scope that lexically encloses the use in this
  blob. A variable or parameter counts only if it is declared before the use.
  This is the contract's `auto Reason = ...; Reason::ID`, which a compiler
  resolves to the enum.
- **R8:** the last written qualifier names neither the enum nor, for an
  unscoped enum, its enclosing scope. Examples are a derived class, an
  imported name, or a qualifier clang could not resolve.
- **R7:** a qualified ALL_CAPS final name that is not invoked (the next token
  is not `(`), is not followed by `::`, and has no other label. It must not
  keep a macro role. It is read from the token stream, so it also covers
  uses clang could not resolve.

### Rule F: fragment labels

A fragment inherits each parent label whose bytes fall wholly on a shown row.
The offset is remapped through the row's tab expansion, or unchanged for a
hunk. Then:

- An `enum_use` becomes a `conservative_negative` with rule `F-outside` and
  `d` null unless the enumerator's declaration, its enum's opening brace and
  every written qualifier are established on shown rows: a namespace qualifier
  by one of its definitions, any other qualifier by a `class`, `struct`,
  `union` or `enum` head naming it in the fragment. The hand review found the
  first version of this rule, which checked only the enumerator, wrong on 18
  of the 43 fragment uses: an enum whose head was cut away cannot be resolved.
- An `ns_qualifier` is dropped when none of its namespace's in-blob
  definitions is on a shown row.
- An `ns_decl` survives only if its `namespace` keyword is shown, and an
  `enumerator_decl` only if its enum's opening brace is shown. Otherwise it is
  dropped.
- Every other label is kept, with `d` remapped, or null when the declaration
  is not shown.

`coverage.json` counts what was dropped and converted.

A fragment's protected ranges are the parent's ranges clipped to the shown
rows and remapped. So a fragment that begins inside a block comment or a raw
string keeps that text protected. A row's LF is protected when the parent's
line ending was. An omitted-run line is protected when both of its
neighbouring parent bytes were.

Labels are deterministic for a given clang version and SDK, and they never
depend on classifier output.

## Hand review

```sh
python3 -I tests/syntax_evaluation/tools/sample_for_review.py \
    --corpus /absolute/scratch/corpus --labels tests/syntax_evaluation/labels \
    --out /absolute/scratch/review
```

For each third-party source and for fragments, the tool draws a seeded sample
of up to 50 labels per category. It also draws 50 identifier-like words from
inside protected ranges. Each item goes to `sheets/<source>.jsonl`:

- `id`, `f`, `line` and `col`
- `t`, the word
- `context`: six lines before and after, with the word marked `⟦ ⟧`

The sheet does not show the machine label. `keys/<source>.keys.json` maps each
`id` to its machine label. Reviewers label the sheets against the contract
rules above, and the adjudicator compares their labels with the keys.

### The review that was made

Twelve independent labellers, each given one half of a sheet, the rubric and
the corpus but no key, labelled 1,994 items (2026-10-13). They agreed with the
machine labels on 1,963 (98.4%). The 31 disagreements were adjudicated:

- 18 fragment `enum_use` items whose enum head had been cut away: the
  labellers were right, and rule F was corrected as above.
- 11 items where the labeller misapplied the rubric (an enum declared in the
  same file but missed, `type::X` uses whose `using type` aliases are members
  of other classes, and two `absl::` uses reaching an enumerator through the
  inline namespace `ABSL_NAMESPACE_BEGIN` opens, which no syntax in the file
  establishes): the machine label stands.
- 1 alias target (`namespace Alias = GCT::Network;`) whose last name the
  rubric did not cover: the machine label (`namespace`) stands.
- 1 qualifier `detail` after the macro-opened namespace `fmtquill`: the
  labeller was right, so the item is `neutral`.

`labels/hand-review.jsonl` records every item: its machine label before and
after the rule F correction, the hand label, the adjudicated final label and
the reason. The scorer applies a final label wherever it differs from the
machine label; one item differs after the correction.
