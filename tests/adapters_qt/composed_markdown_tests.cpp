#include "bpr/application/device_authorization_application_host.hpp"
#include "bpr/application/github_window_read_host.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/pull_request_overview.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/workbench_window.hpp"
#include <QMouseEvent>
#include <QTextCursor>

#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextEdit>
#include <QTextFragment>
#include <QTextTable>
#include <QTreeView>

#include <algorithm>
#include <cstdlib>
#include <iostream>

namespace {
	using namespace bpr;
	void expect(const bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAIL: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	void settle()
	{
		QApplication::processEvents();
		QApplication::processEvents();
	}
	void key(QWidget* aWidget, const int aKey, const Qt::KeyboardModifiers aModifiers = Qt::NoModifier)
	{
		QKeyEvent event{ QEvent::KeyPress, aKey, aModifiers };
		QApplication::sendEvent(aWidget, &event);
		settle();
	}
	void standard_key(QWidget* aWidget, const QKeySequence::StandardKey aKey)
	{
		const auto bindings = QKeySequence::keyBindings(aKey);
		expect(!bindings.empty(), "native reading shortcut is available");
		const auto chord = bindings.front()[0];
		aWidget->setFocus();
		key(aWidget, chord.key(), chord.keyboardModifiers());
	}
	template <typename TView>
	void reach_end(TView* aView)
	{
		const auto chord = QKeySequence::keyBindings(QKeySequence::MoveToEndOfDocument).front()[0];
		aView->setFocus();
		QKeyEvent event{ QEvent::KeyPress, chord.key(), chord.keyboardModifiers() };
		QApplication::sendEvent(aView, &event);
		const auto immediate = aView->textCursor().position();
		settle();
		const auto visible = aView->viewport()->rect().contains(aView->cursorRect());
		if (aView->textCursor().position() != aView->document()->characterCount() - 1 || !visible) {
			std::cerr << "document-end immediate=" << immediate << " position=" << aView->textCursor().position() << " last=" << aView->document()->characterCount() - 1
					  << " cursor=" << aView->cursorRect().x() << ',' << aView->cursorRect().y() << ',' << aView->cursorRect().width() << ',' << aView->cursorRect().height()
					  << " viewport=" << aView->viewport()->width() << ',' << aView->viewport()->height() << '\n';
		}
		expect(aView->textCursor().position() == aView->document()->characterCount() - 1 && visible,
			   "native document-end shortcut reaches and shows the final content");
	}
	constexpr auto corpus = R"MD(# Native review content

Paragraph with *emphasis*, **strong**, ~~struck~~ and `inline code`.

> Read the quotation in context.

1. First item
2. Second item
   - Nested bullet

- [x] Complete
- [ ] Pending

```cpp
    int value = 42;
    return value;
```

| Column | Result |
| --- | --- |
| native | **Readable** |
| source | Preserved |

---

[Web link](https://example.invalid/review)
[Repository guide](./docs/guide.md)
[Local refusal](file:///tmp/markdown-canary)
[Credential refusal](https://user:secret@example.invalid)
![Diagram](https://images.example.invalid/diagram.png)
![Local diagram](file:///tmp/image-canary)

<script>alert('inert')</script>
<img src="file:///tmp/no-resource.png">

Final content: Ω 日本語 🦊
)MD";
	struct transport final : application::github_account_transport_lease
	{ };
	struct provider final : application::github_pull_request_port
	{
		application::github_pull_request_provider_request   Request;
		application::github_pull_request_completion_handler Complete;
		void                                                fetch_pull_request(application::github_pull_request_provider_request aRequest, application::github_pull_request_completion_handler aComplete) override
		{
			Request  = std::move(aRequest);
			Complete = std::move(aComplete);
		}
		void cancel_pull_request(application::github_pull_request_provider_request) override { }
	};
	struct browser final : application::device_authorization_browser_port
	{
		std::vector<std::string> Addresses;
		auto                     open(const std::string_view aAddress) -> bool override
		{
			Addresses.emplace_back(aAddress);
			return true;
		}
	};
	auto session() -> application::github_profile_session
	{
		auto result = application::github_profile_session{
			.BprProfile = core::default_bpr_profile_id, .ApplicationSession = core::application_session_id{ 1 }, .ProfileAuthority = { .Profile = core::host_profile_id{ 1 }, .ProfileRevision = application::github_profile_revision{ 1 }, .Host = core::git_host_id{ 1 }, .Hostname = application::github_canonical_hostname{ "ghe.example.invalid" }, .ProbeContractRevision = application::github_probe_contract_revision{ 1 } }, .Session = { .Profile = core::host_profile_id{ 1 }, .Host = core::git_host_id{ 1 }, .Viewer = { .Account = core::github_account_id{ 1 }, .Node = application::github_actor_id{ "actor" }, .Login = "reader" }, .CapabilityEvidence = { .Revision = core::capability_revision{ 1 }, .ContractVersion = 1 } }, .Generation = core::github_identity_generation{ 1 }, .CapabilityEvidence = { .Authority = { .Profile = core::host_profile_id{ 1 }, .ProfileRevision = application::github_profile_revision{ 1 }, .Host = core::git_host_id{ 1 }, .Account = core::github_account_id{ 1 }, .ProbeContractRevision = application::github_probe_contract_revision{ 1 } }, .Revision = core::capability_revision{ 1 } }, .AccountTransport = std::make_shared<transport>()
		};
		result.CapabilityEvidence.Capabilities.ReviewThreads = core::github_capability_state::supported;
		result.CapabilityEvidence.ReadShape.ReviewThreads    = true;
		result.Session.CapabilityEvidence.Capabilities       = result.CapabilityEvidence.Capabilities;
		result.Session.CapabilityEvidence.ReadShape          = result.CapabilityEvidence.ReadShape;
		return result;
	}
	auto snapshot(const application::github_pull_request_provider_request& aRequest) -> application::github_pull_request_snapshot
	{
		application::github_pull_request_snapshot result{
			.Profile                    = aRequest.Context.Profile,
			.Host                       = aRequest.Context.Host,
			.Account                    = aRequest.Context.Account,
			.Viewer                     = { .Actor = aRequest.Viewer.Node, .Login = aRequest.Viewer.Login },
			.Snapshot                   = core::provider_snapshot_id{ 91 },
			.Repository                 = { .Repository = core::repository_id{ 71 }, .Node = application::github_node_id{ "repo" }, .Owner = aRequest.Context.Repository.Owner, .Name = aRequest.Context.Repository.Name, .DefaultBranch = "main" },
			.PullRequest                = { .PullRequest = core::pull_request_id{ 81 }, .Repository = core::repository_id{ 71 }, .Number = aRequest.Context.Number, .Node = application::github_node_id{ "pr" }, .Title = "Native Markdown review", .Body = corpus, .BaseOid = application::commit_oid{ std::string(40, '1') }, .HeadOid = application::commit_oid{ std::string(40, '2') }, .BaseRef = "main", .HeadRef = "topic" },
			.Reviews                    = { { .DatabaseId = 1U, .Review = application::github_review_id{ "summary" }, .State = application::github_review_state::commented, .Body = corpus } },
			.ReviewThreadsAvailability  = application::github_collection_availability::available,
			.ReviewThreads              = { { .Thread = application::github_thread_id{ "thread" }, .Path = "src/file.cpp", .Line = 7U, .DiffSide = application::github_diff_side::right, .Comments = { { .Comment = application::github_comment_id{ "root" }, .Body = corpus, .Path = "src/file.cpp", .Line = 7U }, { .Comment = application::github_comment_id{ "reply" }, .ReplyTo = application::github_comment_id{ "root" }, .Body = corpus, .Path = "src/file.cpp", .Line = 7U } } } },
			.ReviewCommentsAvailability = application::github_collection_availability::available,
			.ReviewCommentGroups        = { { .Root    = { .Comment = application::github_comment_id{ "rest-root" }, .Body = corpus, .Path = "src/file.cpp", .Line = 7U },
											  .Replies = { { .Comment = application::github_comment_id{ "rest-reply" }, .ReplyTo = application::github_comment_id{ "rest-root" }, .Body = corpus, .Path = "src/file.cpp", .Line = 7U } } } },
			.IssueComments              = { { .Comment = application::github_comment_id{ "issue" }, .Body = corpus } },
			.CapabilityEvidence         = { .Capabilities = aRequest.CapabilityEvidence.Capabilities, .Revision = aRequest.CapabilityEvidence.Revision, .ReadShape = aRequest.CapabilityEvidence.ReadShape, .ContractVersion = 1U },
			.RateLimits                 = {
				.Core    = application::github_rate_limit_record{ application::github_rate_limit_resource::core, 5000U, 4999U, 100U },
				.Search  = application::github_rate_limit_record{ application::github_rate_limit_resource::search, 30U, 29U, 100U },
				.Graphql = application::github_rate_limit_record{ application::github_rate_limit_resource::graphql, 5000U, 4999U, 100U },
			},
		};
		return result;
	}
	void structures(qt::markdown_body_view& aReader)
	{
		expect(aReader.rendered_available() && !aReader.source_visible(), "accepted corpus is rendered");
		auto* doc   = aReader.rendered_view()->document();
		int   tasks = 0, lists = 0, code = 0;
		for (auto block = doc->begin(); block.isValid(); block = block.next()) {
			tasks += block.blockFormat().marker() != QTextBlockFormat::MarkerType::NoMarker ? 1 : 0;
			lists += block.textList() != nullptr ? 1 : 0;
			code += block.blockFormat().hasProperty(QTextFormat::BlockCodeFence) ? 1 : 0;
		}
		expect(doc->begin().blockFormat().headingLevel() == 1 && tasks == 2 && lists >= 5 && code > 0, "native heading, task, list and code structures reach the composed reader");
		expect(std::ranges::any_of(doc->rootFrame()->childFrames(), [](auto* aFrame) { return dynamic_cast<QTextTable*>(aFrame) != nullptr; }), "native table reaches the composed reader");
		auto* accessible = QAccessible::queryAccessibleInterface(aReader.rendered_view());
		expect(accessible && accessible->textInterface() && !accessible->text(QAccessible::Name).isEmpty(), "native read-only text and accessible naming are exposed");
	}
	void activate(qt::markdown_body_view& aReader, const QString& aTarget)
	{
		auto* view = aReader.rendered_view();
		for (auto block = view->document()->begin(); block.isValid(); block = block.next()) {
			for (auto it = block.begin(); !it.atEnd(); ++it) {
				const auto fragment = it.fragment();
				if (fragment.charFormat().anchorHref() == aTarget) {
					auto cursor = QTextCursor{ view->document() };
					cursor.setPosition(fragment.position() + 1);
					view->setTextCursor(cursor);
					view->setFocus();
					key(view, Qt::Key_Return);
					return;
				}
			}
		}
		expect(false, "required composed link exists");
	}
	void capture(qt::workbench_window& aWindow, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_MARKDOWN_RENDER_DIR");
		if (root.isEmpty()) {
			return;
		}
		expect(QDir{ }.mkpath(root), "native capture directory created");
		settle();
		expect(aWindow.grab().save(root + '/' + aName + ".png"), "composed native capture saved");
	}
}
int main(int argc, char** argv)
{
	QApplication                          app{ argc, argv };
	provider                              reads;
	browser                               opener;
	application::manual_executor          lane;
	application::github_window_read_host* hostPtr = nullptr;
	qt::workbench_window                  window{ { }, { }, { }, { { .Profile = core::host_profile_id{ 1 }, .Label = "Synthetic host", .Hostname = "ghe.example.invalid" } }, { } };
	window.resize(1180, 960);
	window.show();
	window.activateWindow();
	application::github_window_read_host host{ { core::default_bpr_profile_id, core::bpr_window_id{ 2 }, core::bpr_window_generation{ 1 } }, core::application_session_id{ 2 }, reads, lane, [&](const auto& aUpdate) {
												  window.publish(aUpdate.Presentation);
												  if (aUpdate.Presentation.PullRequest) {
													  window.publish(application::project_pull_request_overview(*hostPtr->accepted_snapshot(), { }));
												  }
											  } };
	hostPtr = &host;
	std::optional<core::markdown_link_intent> lastGesture;
	window.set_markdown_link_dispatch([&](const auto& aIntent) { lastGesture = aIntent; return host.open_markdown_link(aIntent, &opener); });
	expect(host.adopt(session()), "synthetic account accepted by real read host");
	window.set_setup_destination_dispatch([&](const auto& aIntent) {
		if (std::holds_alternative<application::submit_setup_destination>(aIntent)) {
			host.dispatch(core::open_pull_request_intent{ { "owner", "repo" }, core::pull_request_number{ 42 } });
		}
	});
	window.home_open_bar()->setText(QStringLiteral("https://ghe.example.invalid/owner/repo/pull/42"));
	expect(window.findChild<QAction*>(QStringLiteral("homeOpenAction"))->isEnabled(), "Open becomes available from the composed destination input");
	window.home_open_bar()->setFocus();
	key(window.home_open_bar(), Qt::Key_Return);
	expect(static_cast<bool>(reads.Complete), "Open gesture reaches provider through the accepted read host");
	auto       accepted   = snapshot(reads.Request);
	const auto validation = application::validate(accepted, reads.Request.Context);
	if (!validation.valid()) {
		std::cerr << "validation=" << static_cast<int>(validation.Error) << '\n';
	}
	expect(validation.valid(), "synthetic full public corpus meets read-model admission");
	reads.Complete(application::github_pull_request_success{ reads.Request, accepted });
	lane.drain();
	settle();
	expect(host.accepted_snapshot().has_value(), "provider completion is accepted through exact capability authority");
	auto* overview = window.overview_page();
	auto* reader   = overview->body_view();
	structures(*reader);
	overview->findChild<QPushButton*>(QStringLiteral("overviewReadDescription"))->click();
	settle();
	expect(reader->height() > 700, "expanded overview fills the workbench without a six-line cap");
	expect(opener.Addresses.empty(), "rendering, layout and native accessibility perform no browser effect");
	activate(*reader, QStringLiteral("./docs/guide.md"));
	expect(opener.Addresses.back() == "https://ghe.example.invalid/owner/repo/blob/2222222222222222222222222222222222222222/docs/guide.md", "real composed keyboard gesture binds repository link to accepted head");
	const auto stale = *lastGesture;
	const auto count = opener.Addresses.size();
	activate(*reader, QStringLiteral("file:///tmp/markdown-canary"));
	activate(*reader, QStringLiteral("https://user:secret@example.invalid"));
	activate(*reader, QStringLiteral("file:///tmp/image-canary"));
	expect(opener.Addresses.size() == count, "direct and image local links plus userinfo cause no handoff");
	activate(*reader, QStringLiteral("https://images.example.invalid/diagram.png"));
	expect(opener.Addresses.size() == count + 1, "image placeholder uses the same explicit host admission");
	reader->copy_source();
	expect(QApplication::clipboard()->mimeData()->data("text/plain").toStdString() == corpus, "composed Copy source retains exact bytes");
	reader->show_source(true);
	settle();
	expect(reader->source_view()->toPlainText().toStdString() == corpus, "source switch retains complete original body");
	reader->show_source(false);
	settle();
	standard_key(reader->rendered_view(), QKeySequence::MoveToStartOfDocument);
	capture(window, QStringLiteral("overview-native"));
	auto* panel = window.conversation_panel();
	window.choose_modality(qt::review_modality::discuss);
	settle();
	// [[BPR-1290]]. Every public post reads in place in its open thread, as
	// written, with its web links admitted through the same host.
	const auto postBody = [panel](const std::string& aPost) -> QTextEdit* {
		const auto& cards = panel->cards()->cards();
		const auto  card  = std::ranges::find_if(cards, [&aPost](const qt::thread_card& aCard) {
			return std::ranges::any_of(aCard.Posts, [&aPost](const qt::card_post& aEntry) { return aEntry.Id == aPost; });
		});
		if (card == cards.end()) {
			return nullptr;
		}
		panel->cards()->open_thread(card->Id);
		QApplication::processEvents();
		auto* const open = panel->cards()->open_card();
		if (open == nullptr) {
			return nullptr;
		}
		for (auto* const post : open->findChildren<QWidget*>(QStringLiteral("threadPost"))) {
			if (post->property("postIdentity").toString() == QString::fromStdString(aPost)) {
				return post->findChild<QTextEdit*>(QStringLiteral("threadPostBody"));
			}
		}
		return nullptr;
	};
	const auto pressLink = [](QTextEdit& aBody, const QString& aTarget) {
		const auto found = aBody.document()->find(aTarget);
		expect(!found.isNull(), "the link's address is in the post as written");
		auto cursor = QTextCursor{ aBody.document() };
		cursor.setPosition(found.selectionStart() + 2);
		const auto point   = QPointF{ aBody.cursorRect(cursor).center() };
		auto       press   = QMouseEvent{ QEvent::MouseButtonPress, point, aBody.viewport()->mapToGlobal(point), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		auto       release = QMouseEvent{ QEvent::MouseButtonRelease, point, aBody.viewport()->mapToGlobal(point), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(aBody.viewport(), &press);
		QApplication::sendEvent(aBody.viewport(), &release);
	};
	const std::vector<std::string> ids{ "review:summary", "review-comment:root", "review-comment:reply", "public-preview-comment:rest-root", "public-preview-comment:rest-reply", "issue-comment:issue" };
	for (const auto& id : ids) {
		expect(panel->model()->index_for_id(id).isValid(), "every public body and reply is in the conversation model");
		auto* const body = postBody(id);
		expect(body != nullptr && body->isVisible(), "every public body and reply reads in place in its open thread");
		expect(body->toPlainText().trimmed().endsWith(QStringLiteral("Final content: Ω 日本語 🦊")) && body->toPlainText().contains(QStringLiteral("Native review content")),
			   "each post reads complete, to its final content");
		pressLink(*body, QStringLiteral("https://example.invalid/review"));
		settle();
		expect(lastGesture.has_value() && lastGesture->Body == id, "a link gesture carries the exact public post identity");
	}
	// Shared signed-out status is not a window body publication.
	application::public_preview_presentation sibling;
	sibling.Mode                = application::public_preview_mode::signed_out;
	sibling.ReviewComments      = application::public_preview_observation::observed;
	sibling.ReviewCommentGroups = { { .Root = { .Id = "foreign", .Body = "SIBLING BODY", .Path = "foreign.cpp" } } };
	window.publish(sibling);
	settle();
	expect(std::ranges::none_of(panel->cards()->cards(), [](const qt::thread_card& aCard) { return aCard.Id == "foreign"; }),
		   "sibling profile metadata cannot replace this window's accepted conversation");
	window.publish(application::public_preview_presentation{ });
	settle();
	for (const auto mode : { core::appearance_mode::light, core::appearance_mode::dark, core::appearance_mode::high_contrast_light, core::appearance_mode::high_contrast_dark }) {
		qt::appearance_profile appearance;
		static_cast<void>(appearance.select_mode(mode));
		appearance.apply_to(window);
		overview->adopt_appearance(appearance);
		panel->adopt_appearance(appearance);
		settle();
		expect(panel->cards()->code_font() == appearance.code_font(), "composed code typography comes from appearance profile");
		capture(window, QStringLiteral("conversation-%1").arg(static_cast<int>(mode)));
	}
	// Same accepted content and new publication preserve the reader's selection.
	auto* text   = postBody("issue-comment:issue");
	auto  cursor = text->textCursor();
	cursor.setPosition(10);
	cursor.setPosition(20, QTextCursor::KeepAnchor);
	text->setTextCursor(cursor);
	auto* document = text->document();
	host.dispatch(core::refresh_pull_request_intent{ });
	reads.Complete(application::github_pull_request_success{ reads.Request, snapshot(reads.Request) });
	lane.drain();
	settle();
	expect(text->document() == document && text->textCursor().anchor() == 10 && text->textCursor().position() == 20,
		   "unchanged accepted refresh preserves the selection in the post being read");
	expect(host.open_markdown_link(stale, &opener) == core::markdown_link_result::stale, "old composed gesture is invalid after new publication");
	window.choose_modality(qt::review_modality::overview);
	settle();
	QElapsedTimer elapsed;
	elapsed.start();
	host.dispatch(core::refresh_pull_request_intent{ });
	accepted                  = snapshot(reads.Request);
	accepted.PullRequest.Body = "# Long description\n\n";
	for (int n = 0; n < 650; ++n) {
		accepted.PullRequest.Body += "Paragraph of native review prose with readable wrapping and retained source.\n\n";
	}
	accepted.PullRequest.Body += "FINAL LONG DESCRIPTION";
	reads.Complete(application::github_pull_request_success{ reads.Request, accepted });
	lane.drain();
	settle();
	expect(reader->rendered_available() && elapsed.elapsed() < 1500, "admitted long composed layout is bounded on this native run");
	reach_end(reader->rendered_view());
	expect(reader->rendered_view()->cursorRect().bottom() < reader->rendered_view()->viewport()->height() && reader->rendered_view()->toPlainText().endsWith(QStringLiteral("FINAL LONG DESCRIPTION")), "last long-description content is visible");
	std::cout << "composed-long-body bytes=" << accepted.PullRequest.Body.size() << " elapsed-ms=" << elapsed.elapsed() << " scale=" << window.devicePixelRatioF() << " platform=" << QGuiApplication::platformName().toStdString() << '\n';
	capture(window, QStringLiteral("overview-long-final"));
	application::workbench_projection wideReader;
	wideReader.Drawers = core::workbench_drawer_delta{ core::workbench_drawer_state::collapsed, core::workbench_drawer_state::collapsed, core::workbench_drawer_state::collapsed };
	window.publish(wideReader);
	settle();
	reach_end(reader->rendered_view());
	capture(window, QStringLiteral("overview-long-final-wide"));
	wideReader.Drawers = core::workbench_drawer_delta{ };
	window.publish(wideReader);
	settle();
	const auto replaceBody = [&](const std::string& aBody) {
		host.dispatch(core::refresh_pull_request_intent{ });
		accepted                  = snapshot(reads.Request);
		accepted.PullRequest.Body = aBody;
		reads.Complete(application::github_pull_request_success{ reads.Request, accepted });
		lane.drain();
		settle();
		expect(host.accepted_snapshot()->PullRequest.Body == aBody && reader->original_body() == aBody, "composed presentation never rewrites admitted payload bytes");
	};
	const auto beforeWidth = window.width();
	replaceBody("# Wide code\n\n```text\n" + std::string(240, 'W') + " END OF CODE\n```\n\nFinal wide content");
	expect(reader->rendered_available() && window.width() == beforeWidth && reader->rendered_view()->horizontalScrollBar()->maximum() > 0,
		   "wide code scrolls inside its reader without widening the workbench");
	reader->rendered_view()->horizontalScrollBar()->setValue(reader->rendered_view()->horizontalScrollBar()->maximum());
	capture(window, QStringLiteral("wide-code"));
	std::string wideTable = "|";
	for (int column = 0; column < 8; ++column) {
		wideTable += std::string(80, 'W') + "|";
	}
	wideTable += "\n|---|---|---|---|---|---|---|---|\n|a|b|c|d|e|f|g|END OF TABLE|\n\nFinal wide table";
	replaceBody(wideTable);
	expect(reader->rendered_available() && window.width() == beforeWidth && reader->rendered_view()->horizontalScrollBar()->maximum() > 0
			   && reader->rendered_view()->document()->size().height() < reader->rendered_view()->viewport()->height(),
		   "wide table scrolls locally instead of squeezing two rows into a tall column of characters");
	reader->rendered_view()->horizontalScrollBar()->setValue(reader->rendered_view()->horizontalScrollBar()->maximum());
	reach_end(reader->rendered_view());
	const auto lastCell = reader->rendered_view()->document()->find(QStringLiteral("END OF TABLE"));
	expect(!lastCell.isNull(), "last wide-table cell is retained alongside its reachable final paragraph");
	reader->rendered_view()->setTextCursor(lastCell);
	reader->rendered_view()->ensureCursorVisible();
	settle();
	expect(reader->rendered_view()->viewport()->rect().contains(reader->rendered_view()->cursorRect()), "last wide-table cell is locally reachable");
	capture(window, QStringLiteral("wide-table"));
	const auto exactSource = std::string{ "# Exact source\r\n\r\nText Ω\r\n```\r\n\tindent\r\n```\r\n" };
	replaceBody(exactSource);
	reader->copy_source();
	expect(QApplication::clipboard()->mimeData()->data("text/plain").toStdString() == exactSource, "composed CRLF/tab/Unicode copy retains original bytes");
	elapsed.restart();
	const auto pathological = std::string(1000, '>') + " nested text\nFINAL FALLBACK";
	replaceBody(pathological);
	expect(reader->source_visible() && !reader->rendered_available() && !reader->fallback_reason().isEmpty() && elapsed.elapsed() < 1500,
		   "pathological body has immediate explained complete-source fallback through the same composition");
	reach_end(reader->source_view());
	expect(reader->source_view()->toPlainText().endsWith(QStringLiteral("FINAL FALLBACK")), "fallback retains complete final source");
	standard_key(reader->source_view(), QKeySequence::SelectPreviousChar);
	expect(reader->source_view()->textCursor().selectedText() == QStringLiteral("K"), "native source selection remains read-only and keyboard reachable");
	std::cout << "composed-pathological bytes=" << pathological.size() << " elapsed-ms=" << elapsed.elapsed() << '\n';
	capture(window, QStringLiteral("source-fallback"));
	replaceBody("# malformed\n\n[broken](\n<script>inert</script>\nEND MALFORMED");
	expect(reader->rendered_view()->toPlainText().contains(QStringLiteral("END MALFORMED")), "malformed input never blanks admitted content");
	replaceBody("");
	expect(reader->original_body().empty() && overview->findChild<QLabel*>(QStringLiteral("overviewNoDescription"))->isVisible(), "empty overview exposes its explicit empty state");
	host.dispatch(core::open_pull_request_intent{ { "owner", "other" }, core::pull_request_number{ 43 } });
	settle();
	expect(reader->original_body().empty() && panel->cards()->cards().empty(), "authority invalidation clears the overview body and every conversation before new completion");
	host.shutdown();
	reads.Complete(application::github_pull_request_cancelled{ reads.Request });
	lane.drain();
	expect(host.is_quiescent(), "composed reader host is quiescent");
	std::cout << "composed-markdown-tests:pass\n";
}
