# BPR feedback prefix grammar, revision 1

This is a BPR recognized subset and authoring contract, not a complete Markdown
parser or an upstream grammar specification. Readable unsupported input remains
plain source. The original body is never regenerated from these fields.

## Canonical output

`label (decoration, other-decoration): subject`

The decoration group is optional. One label and a nonblank single-line subject
are required for recognized complete classification. Any subsequent lines remain
discussion content; the parser must not insist on a blank line that would prevent
reading otherwise clear feedback. Canonical creation uses a blank line before new
discussion text but never reformats an existing body to add one.

The canonical label vocabulary is the twelve IDs in LABEL-CATALOG.json. Decoration
tokens are nonempty sequences of ASCII letters/digits plus `-` or `_`; optional
spaces around comma separators are accepted and preserved. Unknown tokens such as
`security`, `test`, `ux`, `performance` remain text and have no privilege or effect.
Canonical newly created header syntax uses lowercase known labels and `, ` separators.
Only newly created/explicitly edited spans are canonicalized.

## Read tolerances

At the body start permit an optional UTF-8 BOM and blank lines. Permit zero through
three leading ASCII spaces before the first nonblank header; four spaces or a tab
at that point indicate an indented block and are not a header. Do not treat Unicode
whitespace or confusable characters as ASCII syntax. ASCII label case is insensitive.
The source bytes are retained despite normalization of the lookup key.

Recognize the plain canonical form and these two explicitly bounded strong forms:

- `**issue:** subject` or `**issue (blocking):** subject` (the complete header is strong).
- `**issue** (blocking): subject` or `**issue**: subject` (only the label is strong).

Do not recursively strip emphasis, arbitrary HTML, heading markers, list markers,
blockquotes, links or inline-code delimiters. Single-star/underscore and other
styling variants remain readable but unclassified in this revision. A candidate
whose decoration list or colon is malformed is not recognized simply because a
known label appeared first. Nested parentheses and empty decoration tokens are
malformed. Preserve all original spelling/order, including duplicate decorations;
duplicates can carry an advisory diagnostic. Standard decoration lookup uses ASCII
case-insensitive keys, preserving raw tokens.

## Boundaries and managed envelopes

Only inspect the first eligible user-body block. If it begins with a quote, fence,
HTML, heading or list, do not search later for a label. A labelled line within a
code sample, suggested-change replacement, quoted reply, link label, or later
paragraph never classifies the entire post. Managed tour/permalink content may
supply an exact validated user-body span from its existing owner. Its bytes and
revision are preconditions; absent/ambiguous boundaries never trigger guessed
prefix stripping. This tolerance does not create a new hidden metadata protocol.

## Parse result and edits

Return status `recognized`, `plain`, `unknown_label`, `malformed`, or `limited`,
optional supported label ID, raw label/decorations, exact header/source spans,
body revision/digest and diagnostics. Unknown labels never borrow an icon or
blocking meaning from a similar known label. Even malformed/limited input retains
its original body and, when useful, an observed raw token for explanation.

Declared expectation is `unspecified`, `blocking`, `non_blocking`, `if_minor`, or
`conflicting`. Recognizing more than one distinct standard expectation produces
`conflicting` and retains each token; duplicate identical tokens alone do not
create a conflict. Label guidance remains separate. Contradictions with guidance
are advisory and not an authorization or automatic lifecycle transition.

Explicit header edits bind the old body revision and change only its supported
header region. Adding a label to plain text uses the existing draft-edit path and
previews any needed restructuring; it never deletes a quote or code block to make
it fit. Removing a valid prefix keeps subject/discussion/patch bytes. Preserve
CRLF/LF and UTF-8; byte spans must be converted correctly for Qt's string indexes.
Rejected limits or stale revisions are actionable and non-destructive.

The parser fixture corpus is normative for this bounded first revision. It is not
a replacement parser; adapt fixtures to the actual C++ API and test the real core.
