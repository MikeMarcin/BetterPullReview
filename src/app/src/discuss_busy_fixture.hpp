#ifndef BPR_SRC_APP_SRC_DISCUSS_BUSY_FIXTURE_HPP
#define BPR_SRC_APP_SRC_DISCUSS_BUSY_FIXTURE_HPP

/// \file
/// \brief [[BPR-2171]]. conversations-v1 (`bpr-fixtures/conversations#418`)
/// as a busy pull request: reviews whose summaries hold Markdown, bundles of
/// its threads, bot reports in a row and an author a reader may mark as a
/// bot. Served over the fixture's own threads by the review submission
/// fixture, for the Discuss render course and its attended look. Synthetic:
/// no identity or repository is real.

#include "review_submission_fixture.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <string>

namespace bpr::app::detail {
	inline void serve_busy_discuss(review_submission_fixture_state& aState)
	{
		constexpr auto head  = "f05373e1184e7067994839ecbe6e2db7a933fa97";
		const auto     user  = [](const char* aLogin, const bool aBot) { return QJsonObject{ { "node_id", QStringLiteral("U_%1").arg(QString::fromLatin1(aLogin)) }, { "login", QString::fromLatin1(aLogin) }, { "type", aBot ? "Bot" : "User" } }; };
		auto           count = 6100;
		const auto     review = [&](const char* aNode, const char* aLogin, const char* aState, const char* aAt, const QString& aBody) {
            return QJsonObject{ { "id", ++count }, { "node_id", QString::fromLatin1(aNode) }, { "state", QString::fromLatin1(aState) }, { "submitted_at", QString::fromLatin1(aAt) }, { "body", aBody }, { "commit_id", head }, { "user", user(aLogin, false) } };
		};
		auto reviews = QJsonArray{ };
		reviews.append(review("busy-review-jiayin", "jiayin", "CHANGES_REQUESTED", "2026-10-28T09:00:00Z",
							  QStringLiteral("Thanks for taking this on. The selection survives a refresh now, but two things block it.\n\n## Blocking\n\n"
											 "1. `set_current_thread()` keeps a reply's id when no card holds it, so Current can name nothing.\n"
											 "2. The refresh drops Open **before** the model is rebuilt, so the open card closes and reopens.\n\n"
											 "Keeping the root is enough:\n\n```cpp\nif (const auto root = root_of(aId)) {\n\tCurrent = *root;\n}\n```\n\n"
											 "> The selection is the reader's, not the model's.\n\n![Before and after](https://example.invalid/refresh.png)\n")));
		reviews.append(review("busy-review-tbutler", "tbutler", "COMMENTED", "2026-10-29T15:20:00Z", QString{ }));
		reviews.append(review("busy-review-author", "fixture-author", "COMMENTED", "2026-10-31T11:00:00Z",
							  QStringLiteral("Reworked along the lines jiayin asked for:\n\n- the root is kept, not the reply\n- Open survives the rebuild\n\n> Keeping the root is enough\n\nIt was.")));
		reviews.append(review("busy-review-approve", "tbutler", "APPROVED", "2026-11-01T08:30:00Z", QStringLiteral("Much better. One real problem left in the finder; the rest reads well.")));
		auto comments = QJsonArray{ };
		auto comment  = 7100;
		const auto add = [&](const char* aLogin, const bool aBot, const char* aAt, const QString& aBody) {
			comments.append(QJsonObject{ { "id", ++comment }, { "node_id", QStringLiteral("busy-comment-%1").arg(comment) }, { "created_at", QString::fromLatin1(aAt) }, { "updated_at", QString::fromLatin1(aAt) }, { "body", aBody }, { "user", user(aLogin, aBot) } });
		};
		const auto build = [](const char* aWhen) {
			return QStringLiteral("## Build report\n\n| Target | Result | Time |\n|---|---|---|\n| macos-arm64 | passed | 6m 12s |\n| windows-x64 | passed | 9m 40s |\n| linux-x64 | passed | 7m 03s |\n\n"
								  "<details><summary>Warnings (3)</summary>\n\n- -Wshadow in conversation_cards_view.cpp:612\n\n</details>\n\nRun %1.")
				.arg(QString::fromLatin1(aWhen));
		};
		const auto coverage = QStringLiteral("## Coverage report\n\nAll modified and coverable lines are covered by tests.\n\n| File | Coverage |\n|---|---|\n| conversation_cards_view.cpp | 91.2% |\n");
		add("github-actions[bot]", true, "2026-10-27T10:00:00Z", build("1 of 2"));
		add("codecov[bot]", true, "2026-10-27T10:05:00Z", coverage);
		add("renovate-helper", false, "2026-10-30T07:00:00Z", QStringLiteral("**Dependency dashboard**: two updates are waiting.\n\n- [ ] Update `qt` to 6.12.1\n- [ ] Update `md4c` to 0.5.3"));
		add("github-actions[bot]", true, "2026-10-31T12:00:00Z", build("2 of 2"));
		add("codecov[bot]", true, "2026-10-31T12:04:00Z", coverage);
		add("observer-three", false, "2026-11-02T09:15:00Z",
			QStringLiteral("> Both are covered by `conversation_read_public_path` now.\n\nIt does not cover a refresh that *removes* the thread. Worth a case?"));
		const std::scoped_lock lock{ aState.Mutex };
		aState.ReviewHistory        = QJsonDocument{ reviews }.toJson(QJsonDocument::Compact).toStdString();
		aState.ConversationComments = QJsonDocument{ comments }.toJson(QJsonDocument::Compact).toStdString();
		// The fixture's threads, by their place in its list, bundled in the
		// reviews above; the rest stand alone.
		aState.AttendedThreadReviews = {
			{ 1, "busy-review-jiayin" }, { 2, "busy-review-jiayin" }, { 3, "busy-review-jiayin" }, { 10, "busy-review-jiayin" }, { 11, "busy-review-jiayin" },
			{ 5, "busy-review-tbutler" }, { 7, "busy-review-tbutler" }, { 15, "busy-review-tbutler" }, { 16, "busy-review-tbutler" },
			{ 13, "busy-review-approve" }, { 17, "busy-review-approve" }, { 18, "busy-review-approve" },
		};
	}
}

#endif
