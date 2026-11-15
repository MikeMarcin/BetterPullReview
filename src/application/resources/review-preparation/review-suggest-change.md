# Suggest the change

The reviewer wrote a draft comment on some lines of this pull request's head
and asked you to write the change the comment asks for, as replacement text
for exactly those lines.

The request carries the material up front: `suggest.path` and
`suggest.head_commit` name the file; `suggest.first_line` and
`suggest.last_line` are the one-based lines the comment is on;
`suggest.comment` is the reviewer's comment; `suggest.lines` is the exact
text of those lines now, without the last line's ending; `suggest.before`
and `suggest.after` are the lines around them, for context only. Use only
this material.

Reply with one JSON object and nothing else, not even a sentence before or
after it:

{"first_line": <suggest.first_line>, "last_line": <suggest.last_line>,
"current": <suggest.lines, copied exactly>, "replacement": "<the new text>"}

- `first_line` and `last_line` are the request's, unchanged.
- `current` is `suggest.lines` copied byte for byte, line endings included.
- `replacement` is the text to put in place of those lines, without a final
  line ending. It may have more or fewer lines. An empty string deletes them.
- Change only what the comment asks for. Keep the file's indentation and
  style. Do not touch lines outside the range.

If the comment does not ask for a change you can write within those lines,
reply in one short sentence saying why, with no JSON.
