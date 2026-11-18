# Recorded review of the Lantern promo pull request

[review.md](review.md) is the final message from a real Codex CLI 0.167.0 run on November 18, 2026. The reviewer selected **Review this pull request** in BetterPullReview 0.5.0 build 64. The recording used the prompt, review instructions and PR context emitted by that build, in a read-only checkout of the synthetic [Lantern PR](../../github/pull-requests/promo-v1/README.md). The private context count was zero.

The PR compares `c8d078714e30667585ceffdf7f4c20d9131cd4a2` with `069cbf66992b253bf0bd73ed3e46e4a8019a0dc8`. The Markdown includes the original four structured findings. Its SHA-256 is `78d604cfff49d11c0c446f43c246fd503ceccb64502be7b67ceaf76ef01db057`.

## Film the report

Run the test-enabled application with `--github-fixture promo-v1 --attended-launch --attended-open-file src/app/game_loop.cpp --attended-agent-look /absolute/path/to/this/codex`. Keep the executable beside `review.md`. Select the suggested **Review this pull request** prompt and open the report attachment. Choose **Draft comment** under the finding about the rounded target rate. The app opens a local draft on line 40 of `tests/render/frame_pacer_tests.cpp`. Reopen the report and choose **Go to draft** to return to that comment.

The local replay executable returns the recorded answer and token counts. It checks the PR, revisions and prompt before answering. Retakes use the fixture and the application's report and draft controls. Only the final message and usage are retained from the recording. The JSONL envelope uses a fixed session ID.
