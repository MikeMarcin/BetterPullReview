# Agent replies for the BetterPullReview promo

[review.md](review.md) is the final message from a real Codex CLI 0.167.0 run on November 18, 2026. The reviewer selected **Review this pull request** in BetterPullReview 0.5.0 build 64. The recording used the prompt, review instructions and PR context emitted by that build, in a read-only checkout of the synthetic [Lantern PR](../../github/pull-requests/promo-v1/README.md). The private context count was zero.

The PR compares `c8d078714e30667585ceffdf7f4c20d9131cd4a2` with `069cbf66992b253bf0bd73ed3e46e4a8019a0dc8`. The Markdown includes the original four structured findings. Its SHA-256 is `78d604cfff49d11c0c446f43c246fd503ceccb64502be7b67ceaf76ef01db057`.

## Film the report

Run the test-enabled application with `--github-fixture promo-v1 --attended-launch --attended-open-file src/app/game_loop.cpp --attended-agent-look /absolute/path/to/this/codex`. Keep the executable beside `review.md`. Select the suggested **Review this pull request** prompt and open the report attachment. Choose **Draft comment** under the finding about the rounded target rate. The app opens a local draft on line 40 of `tests/render/frame_pacer_tests.cpp`. Reopen the report and choose **Go to draft** to return to that comment.

The local replay executable returns the recorded answer and token counts. It checks the PR, revisions and prompt before answering. Retakes use the fixture and the application's report and draft controls. Only the final message and usage are retained from the recording. The JSONL envelope uses a fixed session ID.

## Film the revision question

The timeline scene uses the [next push on the renderer PR](../../github/pull-requests/conventional-comments-next-push-v1/README.md). The Codex editing session wrote [the revision answer](revision-answer.md) from the diff BetterPullReview sent when the reviewer chose **What changed since my review**. The earlier head is `b20cb443` and the new head is `cd24836c`. Only the width and height assignments in `Renderer::resize` changed.

Open `src/engine/renderer.cpp` with `--github-fixture returning-review-v1 --attended-launch --attended-open-file src/engine/renderer.cpp --epoch-fixture-root <private-directory> --epoch-fixture-phase seed`. Close the app after the file opens. Launch again with the same directory, changing `seed` to `reuse` and adding `--attended-agent-look /absolute/path/to/this/codex`. Keep `revision-answer.md` beside the executable.

From the whole-PR comparison, choose **Ask → What changed since my review**. The conversation shows the answer. Open the report attachment to see its reference to `src/engine/renderer.cpp:12–13`. The replay checks the question, both heads and the digest of the diff supplied by the application. The returning turn reports zero tokens for replay.
