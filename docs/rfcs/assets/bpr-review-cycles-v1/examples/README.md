# Synthetic wire and portable examples

`round.json` is a complete eight-item review over four small source fixtures, with two
manually authored suggestions: one exact interface replacement and one explicitly
illustrative implementation sketch. It is **not a compilable cache implementation**,
a product execution transcript, a model response, or evidence of tests run.

`source/src/cache.cpp` intentionally uses tabs and CRLF. Its exact bytes, ranges and
SHA-256 values appear in the round; the readable output discloses presentation limits
and carries exact base64 where necessary. `review.md` is generated from the same
canonical payload. `example.bpr-review.zip` contains that document, `round.json` and a
bounded manifest; it is a portable review example, not the RFC pack itself.

`response.json` demonstrates an explicitly self-reported, discussion-only reply. It
claims no implementation or tests and does not supply local verification authority.
`cli-complete.json` and `cli-refusal.json` are schema fixtures, not available runtime
commands or observed tool results. The CLI syntax is proposed until implemented.

Open the Markdown without BPR to judge readability. Run the reference checks to verify
canonical identity, bytes, suggestions and payload consistency. Actual native exports,
imports, recipients and filesystems require the implementation acceptance courses.
