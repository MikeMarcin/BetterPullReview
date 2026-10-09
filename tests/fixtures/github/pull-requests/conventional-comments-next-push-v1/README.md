# Conventional-comments pull request, next push, version 1

Synthetic public test data, written for [[BPR-1331]]. Nothing was fetched from
GitHub and no identity is real.

`next-push.pack` holds a third commit on the nested-folders pull request
(`bpr-fixtures/nested-folders#21`, [[BPR-1213]]) that the
`conventional-comments-v1` fixture serves:

| | commit |
|---|---|
| base | `c58160c2ffe27d7cf5e6759d78b80f023547c789` |
| first head | `b20cb443428212543f5911e091a0ffeb407d23cf` |
| next push | `cd24836cfb58a9c14f4d3464d68a57ff8aa2a186` |

The push changes only `src/engine/renderer.cpp`, lines 12 and 13, which clamp
the resize to one pixel as that file's line-12 thread asks. No line moves, so
every review thread stays on its line, and every other file keeps its bytes.
The pack holds every object reachable from the next push, so a cache that
never saw the first head can import it alone.

```sh
python3 scripts/build_conventional_comments_next_push_pack.py tests/fixtures/github/pull-requests/nested-folders-v1/construction.json tests/fixtures/github/pull-requests/conventional-comments-next-push-v1/next-push.pack
```

The builder rebuilds the base and first head with
`build_recorded_pull_request_pack.py`'s fixed identity and date, refuses to
write unless they are the commits `nested-folders-v1/objects.pack` holds and
the push is the pinned commit changing only `renderer.cpp` by two lines, and
prints `PACK_OK:<base>:<head>:<push>:<bytes>`. Two runs write the same bytes.

## Served

Only under an epoch phase. `--github-fixture conventional-comments-v1` with
`--epoch-fixture-root <root> --epoch-fixture-phase single` serves the first
head, as without a root; `--epoch-fixture-phase reuse` over the same root
serves the next push as the pull request's head, as a reviewer sees it after
the author pushes. `application_review_progress_push_contract` runs both over
one root, and the attended launcher's `--next-push` option does the same for a
person.
