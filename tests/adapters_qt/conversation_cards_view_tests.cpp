#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/feedback_label_controls.hpp"
#include "bpr/qt/github_cli_consent.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/suggestion_mark.hpp"

#include "bpr/core/diff_model.hpp"
#include "bpr/core/reference_page.hpp"

#include <QAbstractButton>
#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QElapsedTimer>
#include <QFile>
#include <QFontMetrics>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QThread>
#include <QToolButton>
#include <QWheelEvent>

#include <glaze/json.hpp>

#include <tuple>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using conversation = application::github_conversation_projection;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void test_wrapped_publication_status_is_fully_visible()
	{
		qt::conversation_cards_view view;
		view.resize(320, 900);
		view.show();
		const auto comparison = core::file_comparison_digest{ };
		view.set_draft(core::review_comment_draft{
						   .Id     = core::review_draft_id{ 1U },
						   .Anchor = { .Comparison = comparison, .Side = core::diff_side::right, .Line = 3U },
						   .Body   = "Keep this exact publication.",
						   .State  = core::review_draft_state::publishing,
					   },
					   comparison);
		const auto draft = std::ranges::find_if(view.cards(), &qt::thread_card::Draft);
		expect(draft != view.cards().end(), "the publishing draft has a card");
		const auto identity = draft->Id;
		view.open_thread(identity);
		QApplication::processEvents();
		view.draft_editor()->setFocus();
		auto* const focused = QApplication::focusWidget();
		view.present_draft(qt::draft_composer_presentation{ .Body = QStringLiteral("Keep this exact publication."), .Editable = false, .Lines = QStringLiteral("Line 4"), .Notice = QStringLiteral("Publishing your comment to GitHub…") });
		QApplication::processEvents();
		for (const auto width : { 320, 560, 320 }) {
			view.resize(width, 900);
			for (const auto failure : { application::review_mutation_failure::credential_unavailable, application::review_mutation_failure::credential_tool_failed }) {
				const auto notice = qt::github_cli_publication_recovery_text(failure, QStringLiteral("octo-reviewer"), QStringLiteral("github.com"));
				view.present_draft(qt::draft_composer_presentation{ .Body = QStringLiteral("Keep this exact publication."), .Publishable = true, .Editable = false, .Lines = QStringLiteral("Line 4"), .Notice = notice, .Retry = true });
				QElapsedTimer elapsed;
				elapsed.start();
				auto* const label = view.open_card()->findChild<QLabel*>(QStringLiteral("threadDraftStatus"));
				expect(label != nullptr, "the publication notice is its native wrapped label");
				const auto readable = [label] {
					const auto textHeight = QFontMetrics{ label->font() }.boundingRect(QRect{ 0, 0, label->contentsRect().width(), 32768 }, Qt::TextWordWrap, label->text()).height();
					return label->isVisible() && label->height() >= label->heightForWidth(label->width())
						&& label->contentsRect().height() >= textHeight && label->visibleRegion().contains(label->rect());
				};
				while (!readable() && elapsed.elapsed() < 1400) {
					QApplication::processEvents();
					QThread::msleep(1);
				}
				std::cout << "PUBLICATION_STATUS_GEOMETRY scale=" << view.devicePixelRatioF() << " width=" << label->width()
						  << " height=" << label->height() << " required=" << label->heightForWidth(label->width())
						  << " visible=" << label->visibleRegion().boundingRect().height() << " card_hfw=" << view.open_card()->heightForWidth(view.open_card()->width()) << '\n';
				expect(readable(), "all wrapped outcome/locked-draft/GET-only guidance fits and is visible after layout settles");
				expect(label->text() == notice && label->accessibleName() == notice && QApplication::focusWidget() == focused,
					   "height changes preserve the exact accessible status and native focus");
				expect(view.draft_editor()->isReadOnly() && view.open_thread_id() == identity, "responsive layout preserves the same locked publishing draft");
			}
		}
		view.hide();
	}

	struct fixture_json_options : glz::opts
	{
		// Glaze discovers these options by their required member names.
		// NOLINTBEGIN(readability-identifier-naming)
		bool validate_skipped             = true;
		bool validate_trailing_whitespace = true;
		bool escape_control_characters    = true;
		// NOLINTEND(readability-identifier-naming)
		constexpr fixture_json_options()
		{
			null_terminated       = false;
			error_on_missing_keys = true;
			error_on_unknown_keys = false;
		}
	};

	struct thread_node_json
	{
		std::string                  Id;
		std::optional<std::string>   ParentId;
		std::string                  Kind;
		std::string                  AuthorLogin;
		std::string                  Body;
		std::string                  Path;
		std::optional<std::uint32_t> Line;
		struct glaze
		{
			static constexpr auto value = glz::object("Id", &thread_node_json::Id, "ParentId", &thread_node_json::ParentId, "Kind", &thread_node_json::Kind, "AuthorLogin", &thread_node_json::AuthorLogin, "Body", &thread_node_json::Body, "Path", &thread_node_json::Path, "Line", &thread_node_json::Line);
		};
	};

	struct thread_case_json
	{
		std::string                   Id;
		std::vector<thread_node_json> Projection;
		struct glaze
		{
			static constexpr auto value = glz::object("id", &thread_case_json::Id, "projection", &thread_case_json::Projection);
		};
	};

	struct thread_provenance_json
	{
		bool RecordedEvidence;
		struct glaze
		{
			static constexpr auto value = glz::object("recorded_evidence", &thread_provenance_json::RecordedEvidence);
		};
	};

	struct thread_fixture_json
	{
		thread_provenance_json        Provenance;
		std::vector<thread_case_json> Cases;
		struct glaze
		{
			static constexpr auto value = glz::object("provenance", &thread_fixture_json::Provenance, "cases", &thread_fixture_json::Cases);
		};
	};

	/// \brief The AC-08 conversation: one thread, twenty-four posts, three branches.
	[[nodiscard]] auto branching_fixture() -> std::vector<conversation>
	{
		auto file = QFile{ QStringLiteral(BPR_BRANCHING_THREAD_FIXTURE) };
		expect(file.open(QIODevice::ReadOnly), "the branching-thread fixture opens");
		const auto          bytes = file.readAll();
		thread_fixture_json fixture;
		expect(!glz::read<fixture_json_options{ }>(fixture, std::string_view{ bytes.constData(), static_cast<std::size_t>(bytes.size()) }),
			   "the fixture has its required typed JSON fields");
		expect(!fixture.Provenance.RecordedEvidence, "the fixture declares itself constructed");
		auto found = std::vector<conversation>{ };
		for (const auto& entry : fixture.Cases) {
			for (const auto& node : entry.Projection) {
				found.push_back(conversation{
					.Id                 = node.Id,
					.ParentId           = node.ParentId,
					.Kind               = node.Kind == "review_thread" ? application::github_conversation_kind::review_thread : application::github_conversation_kind::review_comment,
					.AuthorLogin        = node.AuthorLogin,
					.Body               = node.Body,
					.Path               = node.Path,
					.Line               = node.Line,
					.DiffSide           = application::github_conversation_diff_side::right,
					.AnchorAvailability = application::github_conversation_anchor_availability::current,
				});
			}
		}
		expect(found.size() == 25U, "the fixture holds one root and twenty-four posts");
		return found;
	}

	[[nodiscard]] auto thread(
		const std::string&  aId,
		const std::string&  aPath,
		const std::uint32_t aLine,
		const bool          aCurrent  = true,
		const bool          aResolved = false) -> conversation
	{
		return conversation{
			.Id                 = aId,
			.Kind               = application::github_conversation_kind::review_thread,
			.Path               = aPath,
			.Line               = aLine,
			.DiffSide           = application::github_conversation_diff_side::right,
			.AnchorAvailability = aCurrent
				? application::github_conversation_anchor_availability::current
				: application::github_conversation_anchor_availability::missing_current_line,
			.Resolved           = aResolved,
		};
	}

	[[nodiscard]] auto reply(const std::string& aId, const std::string& aParent, const std::string& aAuthor, const std::string& aBody)
		-> conversation
	{
		return conversation{
			.Id          = aId,
			.ParentId    = aParent,
			.Kind        = application::github_conversation_kind::review_comment,
			.AuthorLogin = aAuthor,
			.Body        = aBody,
			.Path        = "src/a.cpp",
			.Line        = 10U,
		};
	}

	[[nodiscard]] auto overlapping(const std::vector<qt::thread_card>& aCards) -> bool
	{
		for (std::size_t first = 0U; first < aCards.size(); ++first) {
			for (std::size_t second = first + 1U; second < aCards.size(); ++second) {
				if (aCards[first].Rect.intersects(aCards[second].Rect)) {
					return true;
				}
			}
		}
		return false;
	}

	/// \brief A press at a viewport point of the notes.
	void press_at(qt::conversation_cards_view& aView, const QPoint aPoint)
	{
		auto press = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ aPoint }, aView.viewport()->mapToGlobal(QPointF{ aPoint }),
								  Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(aView.viewport(), &press);
		QApplication::processEvents();
	}

	void key(QWidget& aTarget, const int aKey, const Qt::KeyboardModifiers aModifiers = Qt::NoModifier)
	{
		auto event = QKeyEvent{ QEvent::KeyPress, aKey, aModifiers };
		QApplication::sendEvent(&aTarget, &event);
		QApplication::processEvents();
	}

	[[nodiscard]] auto button(const qt::conversation_cards_view& aView, const char* aName) -> QAbstractButton*
	{
		return aView.open_card() == nullptr ? nullptr : aView.open_card()->findChild<QAbstractButton*>(QString::fromLatin1(aName));
	}

	/// \brief AC-08 and BPR-1290:depth. The twenty-four-post, three-branch
	/// conversation is one note that opens in place to every post in posting
	/// order, each reply to anything but the first post naming whom it answers.
	auto test_branching_conversation_reads_in_place() -> void
	{
		qt::conversation_cards_view view;
		view.resize(360, 900);
		view.show();
		QApplication::processEvents();
		const auto fixture = branching_fixture();
		view.set_viewer_login(QStringLiteral("reviewer-one"));
		view.adopt(fixture);
		QApplication::processEvents();
		expect(view.cards().size() == 1U && view.post_count() == 24U,
			   "the twenty-four posts of the three branches are one note");
		const auto& card  = view.cards().front();
		auto        order = std::vector<std::string>{ };
		for (const auto& post : card.Posts) {
			order.push_back(post.Id);
		}
		auto expected = std::vector<std::string>{ };
		for (const auto& node : fixture) {
			if (node.Kind == application::github_conversation_kind::review_comment) {
				expected.push_back(node.Id);
			}
		}
		expect(order == expected, "posts keep the projection's parent-before-child order");
		auto depths = std::map<std::string, std::size_t>{ };
		for (const auto& post : card.Posts) {
			depths.emplace(post.Id, post.Depth);
		}
		expect(depths.at("review-comment:branching-thread-v1-c01") == 1U
				   && depths.at("review-comment:branching-thread-v1-a08") == 9U
				   && depths.at("review-comment:branching-thread-v1-cc03-aside") == 5U,
			   "every branch's depth is counted from the root");
		auto answering = std::size_t{ 0U };
		for (const auto& post : card.Posts) {
			answering += post.ReplyingTo.isEmpty() ? 0U : 1U;
		}
		expect(card.Posts.front().ReplyingTo.isEmpty() && card.Posts[1].ReplyingTo.isEmpty() && card.Posts[2].ReplyingTo == QStringLiteral("author-two")
				   && answering == 20U,
			   "a reply that answers anything but the first post names whom it answers; the first post's direct replies do not");
		expect(card.Posts.front().Mine && !card.Posts[1].Mine, "my posts are marked mine by login");
		const auto closedHeight = card.Rect.height();
		view.open_thread(card.Id);
		QApplication::processEvents();
		auto* const open = view.open_card();
		expect(open != nullptr && open->findChildren<QWidget*>(QStringLiteral("threadPost")).size() == 24
				   && view.cards().front().Rect.height() > 24 * 20 && view.cards().front().Rect.height() > closedHeight * 5,
			   "the open thread shows all twenty-four posts in place, as tall as they are");
		auto namedAnswer = false;
		for (auto* const post : open->findChildren<QWidget*>(QStringLiteral("threadPost"))) {
			namedAnswer = namedAnswer || post->accessibleName().contains(QStringLiteral("reviewer"));
		}
		expect(namedAnswer, "each post is named for screen readers by its author and mark");
		expect(view.marked_lines(core::diff_side::right) == std::vector<std::uint32_t>{ 41U }
				   && view.marked_lines(core::diff_side::left).empty(),
			   "the thread's line is marked, zero-based, on its side");
		view.close_thread();
		expect(view.open_card() == nullptr || !view.open_card()->isVisible(), "closing the thread folds it back to its note");
		view.hide();
	}

	// [[BPR-0656]]. The read state the ledger host decided: unread posts carry
	// the dot and count, the open thread offers Mark read, the first post
	// unread makes the thread new for the gutter, and the strip walks the
	// unread threads of the whole pull request in file order.
	auto test_read_state_marks_unread_posts_and_offers_mark_read() -> void
	{
		qt::conversation_cards_view view;
		view.resize(360, 900);
		view.show();
		QApplication::processEvents();
		auto fixture = branching_fixture();
		fixture.push_back(thread("t-after", "src/z.cpp", 5U));
		fixture.push_back(reply("c-after", "t-after", "someone", "later file"));
		fixture.push_back(thread("t-before", "src/a.cpp", 1U));
		fixture.push_back(reply("c-before", "t-before", "someone", "earlier file"));
		view.set_viewer_login(QStringLiteral("reviewer-one"));
		view.adopt(fixture);
		QApplication::processEvents();
		const auto root = fixture.front().Id;
		view.open_thread(root);
		QApplication::processEvents();
		expect(view.unread_post_count() == 0U && view.unread_threads_in_file_order().empty()
				   && button(view, "threadMarkRead") != nullptr && !button(view, "threadMarkRead")->isEnabled(),
			   "before any read state nothing is unread and Mark read has nothing to mark");

		view.set_read_state({
			qt::thread_read_state{ .Thread = root, .UnreadPosts = { "review-comment:branching-thread-v1-c01", "review-comment:branching-thread-v1-a03" } },
			qt::thread_read_state{ .Thread = "t-after", .UnreadPosts = { "c-after" } },
		});
		QApplication::processEvents();
		const auto rootCard = std::ranges::find(view.cards(), root, &qt::thread_card::Id);
		expect(rootCard != view.cards().end() && rootCard->UnreadCount == 2U && rootCard->RootUnread
				   && rootCard->Posts.front().Unread && !rootCard->Posts[1].Unread,
			   "the unread posts are the ones the state names, and the first post unread makes the thread new");
		expect(view.unread_post_count() == 3U, "the count is over every note");
		expect(view.open_card()->findChild<QWidget*>(QStringLiteral("threadNewRule")) == nullptr
				   && view.open_card()->findChild<QWidget*>(QStringLiteral("threadNewPill")) != nullptr
				   && view.open_card()->findChild<QWidget*>(QStringLiteral("threadNewPill"))->accessibleName() == QStringLiteral("2 new posts"),
			   "the open thread's first post is new, so its header counts the new posts and no rule opens it");
		expect(view.unread_threads_in_file_order() == std::vector<std::string>{ root, "t-after" },
			   "the unread threads of the whole pull request come in file order, the read one absent");
		expect(view.unread_marked_lines(core::diff_side::right) == std::vector<std::uint32_t>{ 4U, 41U },
			   "the lines of the new threads are the gutter's blue marks");
		auto actions = std::vector<std::pair<qt::card_action, std::string>>{ };
		view.set_action_dispatch([&actions](const qt::card_action aAction, const std::string& aId) {
			actions.emplace_back(aAction, aId);
		});
		auto* const markRead = button(view, "threadMarkRead");
		expect(markRead != nullptr && markRead->isEnabled() && markRead->toolTip() == QStringLiteral("Mark 2 new as read"),
			   "with unread posts the open thread offers Mark read and says how many");
		markRead->click();
		expect(actions == std::vector<std::pair<qt::card_action, std::string>>{ { qt::card_action::mark_read, root } },
			   "Mark read reports the action for its thread");
		view.set_read_state({ });
		QApplication::processEvents();
		expect(view.unread_post_count() == 0U && !button(view, "threadMarkRead")->isEnabled()
				   && view.open_card()->findChild<QWidget*>(QStringLiteral("threadNewRule")) == nullptr
				   && view.open_card()->findChild<QWidget*>(QStringLiteral("threadNewPill")) == nullptr,
			   "an empty state reads everything, withdraws Mark read and the header's new count");
		view.hide();
	}

	// [[BPR-0649]], [[BPR-1290]]. Discuss: every thread in file order under its
	// path, each opening in place with its code excerpt and Open in code, the
	// Unread and Unresolved filters, and Resolve once offered.
	auto test_discuss_lists_threads_that_open_in_place() -> void
	{
		qt::conversation_cards_view view;
		view.resize(420, 900);
		view.show();
		QApplication::processEvents();
		auto fixture = branching_fixture();
		fixture.push_back(thread("t-resolved", "src/a.cpp", 3U, true, true));
		fixture.push_back(reply("c-resolved", "t-resolved", "someone", "settled"));
		fixture.push_back(thread("t-open", "src/z.cpp", 9U));
		fixture.push_back(reply("c-open", "t-open", "someone", "open"));
		view.set_viewer_login(QStringLiteral("reviewer-one"));
		view.set_pull_request_author(QStringLiteral("reviewer-one"));
		view.set_excerpt_locator([](const QString& aPath, core::diff_side, const std::uint32_t aLine) -> std::optional<QString> {
			if (aPath != QStringLiteral("src/z.cpp")) {
				return std::nullopt;
			}
			return QStringLiteral("line %1\nline %2\nline %3").arg(aLine - 1).arg(aLine).arg(aLine + 1);
		});
		view.adopt(fixture);
		view.set_read_state({ qt::thread_read_state{ .Thread = "t-open", .UnreadPosts = { "c-open" } } });
		view.set_feed(true);
		view.set_scope(qt::cards_scope::whole_pull_request);
		QApplication::processEvents();
		expect(view.feed() && view.cards().size() == 3U
				   && view.cards()[0].Id == "t-resolved" && view.cards()[2].Id == "t-open"
				   && std::ranges::none_of(view.cards(), &qt::thread_card::Placed),
			   "Discuss lists every thread in file order, never placed at a line");
		expect(view.cards()[1].Posts.front().Role == QStringLiteral("author") && view.cards()[1].Posts[1].Role == QStringLiteral("reviewer"),
			   "the pull request's author is headed author and everyone else reviewer");
		expect(view.cards()[2].Excerpt == QStringLiteral("line 8\nline 9\nline 10") && view.cards()[1].Excerpt.isEmpty(),
			   "a thread on the compared file carries its excerpt; another file's carries none");
		auto actions = std::vector<std::pair<qt::card_action, std::string>>{ };
		view.set_action_dispatch([&actions](const qt::card_action aAction, const std::string& aId) {
			actions.emplace_back(aAction, aId);
		});
		press_at(view, view.note_rect("t-open").center());
		expect(view.open_thread_id() == "t-open" && view.open_card()->findChild<QLabel*>(QStringLiteral("threadExcerpt")) != nullptr,
			   "a press opens the thread in place with its code excerpt above its posts");
		expect(button(view, "threadResolve") == nullptr, "Resolve is not offered before the provider and server both resolve threads");
		expect(button(view, "threadShowLine")->toolTip() == QStringLiteral("Open in Review at src/z.cpp:9")
				   && button(view, "threadShowLine")->accessibleName() == QStringLiteral("Open in Review at src/z.cpp:9"),
			   "Discuss's line button names Review and the exact file and line");
		button(view, "threadShowLine")->click();
		expect(actions == std::vector<std::pair<qt::card_action, std::string>>{ { qt::card_action::open_in_code, "t-open" } },
			   "in Discuss the line button opens the thread in the code");
		view.set_resolution_offered(true);
		QApplication::processEvents();
		expect(button(view, "threadResolve") != nullptr && button(view, "threadResolve")->toolTip() == QStringLiteral("Resolve"),
			   "offered, an open thread gains Resolve");
		view.open_thread("t-resolved");
		QApplication::processEvents();
		expect(button(view, "threadResolve") != nullptr && button(view, "threadResolve")->toolTip() == QStringLiteral("Unresolve")
				   && view.open_card()->findChild<QWidget*>(QStringLiteral("threadState")) != nullptr,
			   "a resolved thread offers Unresolve and says it is resolved");
		expect(view.thread_count() == 3U && view.unresolved_count() == 2U && view.unread_thread_count() == 1U,
			   "the counts the summary restates");
		view.set_filters(qt::discuss_filters{ .UnreadOnly = true });
		QApplication::processEvents();
		expect(view.cards().size() == 2U && view.cards()[1].Id == "t-open" && view.cards()[0].Id == "t-resolved",
			   "Unread keeps only the threads with new posts, and never the open one away");
		view.close_thread();
		expect(view.cards().size() == 1U && view.cards().front().Id == "t-open",
			   "closed, a thread without new posts is filtered out like any other");
		view.set_filters(qt::discuss_filters{ .UnresolvedOnly = true });
		QApplication::processEvents();
		expect(view.cards().size() == 2U && std::ranges::none_of(view.cards(), &qt::thread_card::Resolved),
			   "Unresolved drops the resolved thread");
		view.set_filters(qt::discuss_filters{ });
		view.set_feed(false);
		view.set_scope(qt::cards_scope::this_file);
		QApplication::processEvents();
		expect(!view.feed() && view.cards().size() == 3U && view.cards()[0].Excerpt.isEmpty(),
			   "beside the code the notes carry no excerpt");
		view.hide();
	}

	void test_discuss_post_menus_open_the_exact_comment_in_review()
	{
		qt::conversation_cards_view view;
		view.resize(700, 700);
		view.set_feed(true);
		auto actions = std::vector<std::pair<qt::card_action, std::string>>{ };
		view.set_action_dispatch([&](const qt::card_action aAction, const std::string& aId) { actions.emplace_back(aAction, aId); });
		view.adopt({ thread("inline", "src/a.cpp", 9U), reply("first", "inline", "reviewer", "First comment"), reply("answer", "inline", "author", "A reply") });
		view.show();
		QApplication::processEvents();
		const auto point            = view.note_rect("inline").center();
		auto       collapsedContext = QContextMenuEvent{ QContextMenuEvent::Mouse, point, view.viewport()->mapToGlobal(point) };
		QApplication::sendEvent(view.viewport(), &collapsedContext);
		QApplication::processEvents();
		auto* const collapsedMenu = view.findChild<QMenu*>(QStringLiteral("threadContextMenu"));
		expect(collapsedMenu != nullptr, "a right-click on the collapsed Discuss note opens its thread menu");
		const auto collapsedActions = collapsedMenu->actions();
		expect(std::ranges::any_of(collapsedActions, [](const QAction* aAction) { return aAction->text() == QStringLiteral("Open in Review") && aAction->isEnabled(); }),
			   "the collapsed note offers the Review destination before opening the thread");
		delete collapsedMenu;
		view.open_thread("inline");
		QApplication::processEvents();
		auto* const inReview = button(view, "threadShowLine");
		expect(inReview && inReview->isVisible() && inReview->text() == QStringLiteral("Open in Review") && inReview->focusPolicy() == Qt::StrongFocus,
			   "Discuss has an explicitly named, visible and keyboard accessible Open in Review button");
		auto* const location = view.open_card()->findChild<QLabel*>(QStringLiteral("threadReviewLocation"));
		expect(location && location->text() == QStringLiteral("a.cpp:9") && location->toolTip() == QStringLiteral("src/a.cpp:9"),
			   "the file and line stay visible beside the action");
		const auto readersOf = [](QWidget* aCard) {
			auto readers = std::vector<qt::markdown_body_view*>{ };
			for (auto* const widget : aCard->findChildren<QWidget*>()) {
				if (auto* const reader = dynamic_cast<qt::markdown_body_view*>(widget)) {
					readers.push_back(reader);
				}
			}
			return readers;
		};
		const auto menuFor = [](qt::markdown_body_view& aReader, const bool aSource) {
			aReader.show_source(aSource);
			QApplication::processEvents();
			auto* const viewport = aSource ? aReader.source_view()->viewport() : aReader.rendered_view()->viewport();
			auto        context  = QContextMenuEvent{ aSource ? QContextMenuEvent::Keyboard : QContextMenuEvent::Mouse, QPoint{ 5, 5 }, viewport->mapToGlobal(QPoint{ 5, 5 }) };
			QApplication::sendEvent(viewport, &context);
			QApplication::processEvents();
			return aReader.findChild<QMenu*>(QStringLiteral("markdownReaderContextMenu"));
		};
		for (auto* const reader : readersOf(view.open_card())) {
			for (const auto source : { false, true }) {
				auto* const menu = menuFor(*reader, source);
				expect(menu != nullptr, "the comment reader owns the actual body context menu");
				auto* const open = menu->findChild<QAction*>(QStringLiteral("postOpenInReview"));
				expect(open && open->isEnabled() && open->text() == QStringLiteral("Open in Review"), "first posts and replies offer Open in Review in rendered and source menus");
				menu->hide();
				open->trigger();
				expect(actions.back() == std::pair{ qt::card_action::open_in_code, reader->original_body() == "First comment" ? std::string{ "first" } : std::string{ "answer" } },
					   "the gesture dispatches the exact first post or reply, never only its thread");
				delete menu;
			}
		}
		view.set_feed(false);
		QApplication::processEvents();
		// [[BPR-1581]]. Review's reader menu is the native one with Show in
		// Discuss added, never a second Review action.
		expect(readersOf(view.open_card()).front()->rendered_view()->contextMenuPolicy() == Qt::CustomContextMenu,
			   "Review's reader menu is the native menu, extended");
		// [[BPR-1581]]. In Review a comment's own menu shows it in Discuss.
		for (auto* const reader : readersOf(view.open_card())) {
			auto* const menu = menuFor(*reader, false);
			expect(menu != nullptr, "the reader opens its menu in Review");
			auto* const show = menu->findChild<QAction*>(QStringLiteral("postShowInDiscuss"));
			expect(show && show->isEnabled() && show->text() == QStringLiteral("Show in Discuss") && menu->findChild<QAction*>(QStringLiteral("postOpenInReview")) == nullptr,
				   "a comment's menu in Review offers Show in Discuss");
			menu->hide();
			show->trigger();
			expect(actions.back() == std::pair{ qt::card_action::show_in_discuss, reader->original_body() == "First comment" ? std::string{ "first" } : std::string{ "answer" } },
				   "Show in Discuss names the exact post");
			delete menu;
		}
		view.set_feed(true);
		auto unavailable               = thread("unavailable", "src/a.cpp", 9U);
		unavailable.AnchorAvailability = application::github_conversation_anchor_availability::missing_current_line;
		auto general                   = conversation{ .Id = "general", .Kind = application::github_conversation_kind::issue_comment, .Body = "A PR-wide comment" };
		view.adopt({ unavailable, reply("unavailable-first", "unavailable", "reviewer", "A missing anchor"), general });
		for (const auto& id : { std::string{ "unavailable" }, std::string{ "general" } }) {
			view.open_thread(id);
			QApplication::processEvents();
			auto* const menu = menuFor(*readersOf(view.open_card()).front(), false);
			auto* const open = menu->findChild<QAction*>(QStringLiteral("postOpenInReview"));
			// [[BPR-1581]]. On a file with no current line it opens among the
			// file's unplaced threads; a PR-wide comment explains why not.
			expect(open && open->isEnabled() == (id == "unavailable") && !open->toolTip().isEmpty(), "a missing anchor still opens in Review; a PR-wide comment explains its disabled body action");
			delete menu;
		}

		view.hide();
	}

	auto test_cached_geometry_matches_a_fresh_view() -> void
	{
		auto projection = std::vector<conversation>{
			thread("first", "src/a.cpp", 1U),
			reply("p1", "first", "author", "Original post"),
			thread("second", "src/a.cpp", 2U),
			reply("p2", "second", "reviewer", "Another post"),
		};
		auto                        state      = std::vector<qt::thread_read_state>{ };
		auto                        draft      = std::optional<core::review_comment_draft>{ };
		const auto                  comparison = core::file_comparison_digest{ };
		auto                        width      = 420;
		qt::conversation_cards_view cached;
		auto                        font      = cached.font();
		const auto                  configure = [&](qt::conversation_cards_view& aView) {
			aView.resize(width, 900);
			aView.setFont(font);
			aView.set_resolution_offered(true);
			aView.set_read_state(state);
			aView.set_draft(draft, comparison);
			aView.adopt(projection);
			aView.show();
		};
		const auto verify = [&] {
			configure(cached);
			qt::conversation_cards_view fresh;
			configure(fresh);
			QApplication::processEvents();
			expect(cached.cards().size() == fresh.cards().size(), "cached and fresh views have the same note count");
			for (std::size_t index = 0U; index < fresh.cards().size(); ++index) {
				const auto& actual   = cached.cards()[index];
				const auto& expected = fresh.cards()[index];
				expect(actual.Id == expected.Id && actual.Rect == expected.Rect && actual.Posts.size() == expected.Posts.size(),
					   "cached note geometry matches an independently constructed view");
			}
		};
		verify();
		projection[1].Body = std::string(350U, 'W');
		verify();
		projection.push_back(reply("new-reply", "first", "reviewer", "A new reply takes another row."));
		verify();
		state = { qt::thread_read_state{ .Thread = "first", .UnreadPosts = { "p1" } } };
		verify();
		projection.front().Resolved = true;
		verify();
		draft = core::review_comment_draft{ .Anchor = core::source_line_ref{ .Comparison = comparison }, .Body = "Draft text" };
		verify();
		draft->Body = std::string(200U, 'M');
		verify();
		width = 300;
		verify();
		font.setPointSizeF(font.pointSizeF() + 2.0);
		verify();
		auto styleChange = QEvent{ QEvent::StyleChange };
		QApplication::sendEvent(&cached, &styleChange);
		verify();
		std::erase_if(projection, [](const conversation& aNode) { return aNode.Id == "second" || aNode.Id == "p2"; });
		verify();
		draft.reset();
		state.clear();
		verify();
	}

	auto test_discuss_geometry_refresh_updates_excerpts() -> void
	{
		qt::conversation_cards_view view;
		view.resize(420, 900);
		auto excerpt = QStringLiteral("old code");
		view.set_excerpt_locator([&excerpt](const QString&, core::diff_side, std::uint32_t) -> std::optional<QString> {
			return excerpt.isEmpty() ? std::nullopt : std::optional{ excerpt };
		});
		view.set_feed(true);
		view.adopt({ thread("t", "src/a.cpp", 1U), reply("p", "t", "author", "post") });
		view.open_thread("t");
		excerpt = QStringLiteral("new code\nsecond line\nthird line");
		view.refresh_geometry();
		auto* label = view.open_card()->findChild<QLabel*>(QStringLiteral("threadExcerpt"));
		expect(view.cards().front().Excerpt == excerpt && label != nullptr && label->text() == excerpt && view.current_thread() == "t",
			   "a comparison frame refreshes the open thread's excerpt and keeps it current");
		excerpt.clear();
		view.refresh_geometry();
		expect(view.cards().front().Excerpt.isEmpty() && view.open_card()->findChild<QLabel*>(QStringLiteral("threadExcerpt")) == nullptr,
			   "an unavailable comparison removes the excerpt");
	}

	// [[BPR-1270]]. The excerpt is set in the code font the view was handed.
	auto test_the_excerpt_follows_the_supplied_code_font() -> void
	{
		qt::conversation_cards_view view;
		view.resize(420, 900);
		view.set_excerpt_locator([](const QString&, core::diff_side, std::uint32_t) -> std::optional<QString> {
			return QStringLiteral("first line\nsecond line\nthird line");
		});
		view.set_feed(true);
		view.adopt({ thread("t", "src/a.cpp", 2U), reply("p", "t", "author", "post") });
		view.open_thread("t");
		const auto before = view.cards().front().Rect.height();
		auto       larger = view.code_font();
		larger.setPointSizeF(larger.pointSizeF() * 2.0);
		view.set_code_font(larger);
		expect(view.code_font() == larger, "the view keeps the code font it was handed");
		expect(view.open_card()->findChild<QLabel*>(QStringLiteral("threadExcerpt"))->font().pointSizeF() == larger.pointSizeF()
				   && view.cards().front().Rect.height() > before,
			   "a larger code font lays the excerpt out taller at once");
	}

	auto test_endpoint_changes_retain_exact_body_annotations() -> void
	{
		qt::conversation_cards_view view;
		auto                        projection = std::vector<conversation>{ thread("t", "src/a.cpp", 1U), reply("p", "t", "reviewer", "issue: First body") };
		view.adopt(projection);
		const auto annotations          = view.body_annotations();
		projection.front().OriginalLine = 99U;
		view.adopt(projection);
		view.adopt({ });
		view.adopt(projection);
		expect(view.body_annotations() == annotations && view.cards().front().Posts.front().Label.has_value(),
			   "endpoint placement and an empty forest reuse exact body annotations");
		projection[1].ReviewerProse = "Plain replacement";
		view.adopt(projection);
		expect(view.body_annotations() == annotations + projection.size() && !view.cards().front().Posts.front().Label.has_value()
				   && view.cards().front().Posts.front().Body == QStringLiteral("Plain replacement"),
			   "accepted reviewer prose replaces the prior body's annotation");
		projection[1].ReviewerProse.reset();
		projection[1].Body = "suggestion: Changed body";
		view.adopt(projection);
		expect(view.cards().front().Posts.front().Label.has_value() && view.cards().front().Posts.front().Body == QStringLiteral("suggestion: Changed body"),
			   "changed published text is parsed again after reviewer prose is removed");
		std::ranges::reverse(projection);
		view.adopt(projection);
		expect(view.cards().front().Posts.front().Label.has_value(), "reordered nodes retain the annotation belonging to their body");
		const auto beforeClear = view.body_annotations();
		view.clear();
		view.adopt(projection);
		expect(view.body_annotations() == beforeClear + projection.size(), "authority clear releases the retained body annotations");
	}

	auto test_unchanged_refresh_retains_measured_notes() -> void
	{
		qt::conversation_cards_view view;
		view.resize(400, 600);
		view.show();
		QApplication::processEvents();
		auto projection = std::vector<conversation>{ };
		for (auto index = 0; index < 1000; ++index) {
			const auto id = std::to_string(index);
			projection.push_back(thread("t" + id, "src/a.cpp", static_cast<std::uint32_t>(index + 1)));
			projection.push_back(reply("p" + id, "t" + id, "author", "A retained post whose text measurement must survive refresh."));
		}
		view.adopt(projection, core::file_identity_of("src/a.cpp"));
		view.set_current_thread("p10");
		QApplication::processEvents();
		const auto  layouts = view.layouts();
		const auto* posts   = view.cards().front().Posts.data();
		for (auto refresh = 0; refresh < 10; ++refresh) {
			view.adopt(projection);
			view.refresh_geometry();
		}
		expect(view.layouts() == layouts && view.cards().front().Posts.data() == posts
				   && view.post_count() == 1000U && view.current_thread() == "t10",
			   "unchanged thousand-thread refreshes retain measurements, post storage and selection");
		const auto measurements = view.measurements();
		for (auto refresh = 0; refresh < 10; ++refresh) {
			view.set_anchor_file(std::nullopt);
			view.set_anchor_file(core::file_identity_of("src/a.cpp"));
		}
		expect(view.measurements() == measurements && view.post_count() == 1000U && view.current_thread() == "t10",
			   "temporary anchor clear and restore reuse all thousand retained note measurements");
		projection.front().OriginalLine = 99U;
		view.adopt(projection);
		expect(view.measurements() == measurements, "a changed snapshot with identical measurement inputs shapes no text");
		const auto layoutsBeforeContent = view.layouts();
		projection[1].Body              = "Changed post";
		view.adopt(projection);
		expect(view.layouts() == layoutsBeforeContent + 1U && view.cards().front().Posts.front().Body == QStringLiteral("Changed post"),
			   "a changed post rebuilds its visible content");
		expect(view.measurements() == measurements + 1U, "one changed post remeasures exactly its one note");
		const auto beforeClear = view.measurements();
		view.clear();
		view.adopt(projection);
		expect(view.measurements() > beforeClear, "authority clear releases the previous measurement cache");
		const auto beforeFont = view.measurements();
		auto       larger     = view.font();
		larger.setPointSizeF(larger.pointSizeF() + 2.0);
		view.setFont(larger);
		expect(view.measurements() > beforeFont, "font changes invalidate cached text measurements");
	}

	auto test_alternating_widths_keep_both_widths_measured() -> void
	{
		// [[BPR-0673]]. Each of two alternating widths keeps its measurements.
		qt::conversation_cards_view view;
		view.resize(400, 600);
		view.show();
		QApplication::processEvents();
		auto projection = std::vector<conversation>{ };
		for (auto index = 0; index < 1000; ++index) {
			const auto id = std::to_string(index);
			projection.push_back(thread("t" + id, "src/a.cpp", static_cast<std::uint32_t>(index + 1)));
			projection.push_back(reply("p" + id, "t" + id, "author", "A retained post whose text measurement must survive a width change."));
		}
		view.adopt(projection, std::nullopt);
		QApplication::processEvents();
		view.resize(320, 600);
		QApplication::processEvents();
		const auto measurements = view.measurements();
		for (auto cycle = 0; cycle < 10; ++cycle) {
			view.resize(400, 600);
			view.resize(320, 600);
		}
		QApplication::processEvents();
		expect(view.measurements() == measurements && view.cards().size() == 1000U,
			   "alternating between two widths re-measures no note at either width");
		view.resize(360, 600);
		QApplication::processEvents();
		expect(view.measurements() == measurements + 1000U, "a third width measures its notes afresh");
	}

	/// \brief [[BPR-1345]]. Start a review, as the frames draw it: with no
	/// review in progress the composer offers Add single comment (today's
	/// Publish) and Start a review; while one is, Add review comment is the
	/// main action; a pending draft's main action closes it, still pending.
	/// The privacy note says so. Saved pending drafts are notes at their lines
	/// that say they are pending and publish nothing on their own.
	/// v002 (owner, discard-review and finish-review v001): ⌘↩ is the main
	/// action in every state, ⌘⇧↩ is Add single comment, ⌘⌥↩ asks to finish
	/// the review and ⌘⇧M maximizes; each tooltip names its key, anchored to
	/// its button. The actions keep one row: side by side with room, and in a
	/// narrow drawer the main action with a split arrow that carries Add
	/// single comment, never stacked.
	auto test_start_a_review_in_the_composer() -> void
	{
		qt::conversation_cards_view view;
		view.resize(360, 640);
		view.show();
		QApplication::processEvents();
		auto lineY = std::map<std::uint32_t, int>{ { 12U, 100 }, { 30U, 300 }, { 40U, 420 } };
		view.set_line_locator([&lineY](core::diff_side, const std::uint32_t aLine) -> std::optional<int> {
			const auto found = lineY.find(aLine);
			return found == lineY.end() ? std::nullopt : std::optional{ found->second };
		});
		view.set_anchor_file(core::file_identity_of("src/a.cpp"));
		view.adopt({ });
		const auto comparison = core::file_comparison_digest{ };
		auto       pending    = core::review_comment_draft{ };
		pending.Id            = core::review_draft_id{ 2U };
		pending.Anchor        = core::source_line_ref{ .Comparison = comparison, .Side = core::diff_side::right, .Line = 29U };
		pending.Body          = "question: Where does 0.01 come from?";
		pending.InReview      = core::review_pending_mark{ .Path = "src/a.cpp", .Head = core::git_commit_oid{ core::git_hash_algorithm::sha1, std::array<std::uint8_t, 32>{ 1U } } };
		auto plain            = pending;
		plain.Id              = core::review_draft_id{ 3U };
		plain.Anchor.Line     = 11U;
		plain.Body            = "Not in the review.";
		plain.InReview.reset();
		view.set_saved_drafts({ pending, plain });
		view.set_draft(core::review_comment_draft{ .Id = core::review_draft_id{ 1U }, .Anchor = core::source_line_ref{ .Comparison = comparison, .Side = core::diff_side::right, .Line = 39U }, .Body = "nit: scale could be constexpr." }, comparison);
		QApplication::processEvents();
		const auto note = [&view](const std::uint32_t aLine) { return std::ranges::find_if(view.cards(), [aLine](const qt::thread_card& aCard) { return aCard.Draft && aCard.Line == std::optional{ aLine }; }); };
		expect(note(30U) != view.cards().end() && note(30U)->Pending && note(30U)->Placed && note(30U)->Posts.front().Name.contains(QStringLiteral("Pending in your review"))
				   && note(12U) != view.cards().end() && !note(12U)->Pending,
			   "a saved pending draft is a note at its line that says it is pending in the review; another draft is not");

		auto actions = std::vector<qt::card_action>{ };
		view.set_action_dispatch([&actions](const qt::card_action aAction, const std::string&) { actions.push_back(aAction); });
		view.open_thread(note(30U)->Id);
		QApplication::processEvents();
		expect(actions == std::vector{ qt::card_action::edit_draft } && view.open_card() == nullptr,
			   "opening a pending note asks to edit it, through its line; nothing publishes it from the note");
		actions.clear();
		const auto draftId = std::ranges::find_if(view.cards(), [](const qt::thread_card& aCard) { return aCard.Draft && aCard.Line == std::optional{ 40U }; })->Id;
		view.open_thread(draftId);
		QApplication::processEvents();
		const auto present = [&view](const qt::draft_review_state aReview) {
			view.present_draft(qt::draft_composer_presentation{ .Body = QStringLiteral("nit: scale could be constexpr."), .Publishable = true, .Lines = QStringLiteral("Line 40"), .Review = aReview });
			QApplication::processEvents();
		};
		const auto  words   = [&view](const char* aName) { return button(view, aName) != nullptr ? button(view, aName)->text() : QString{ }; };
		auto* const privacy = view.open_card()->findChild<QWidget*>(QStringLiteral("threadDraftPrivacy"));
		const auto  press   = [&view](const Qt::KeyboardModifiers aModifiers = Qt::ControlModifier, const int aKey = Qt::Key_Return) {
			QKeyEvent key{ QEvent::KeyPress, aKey, aModifiers };
			QApplication::sendEvent(view.draft_editor(), &key);
		};
		const auto main     = QKeySequence{ Qt::CTRL | Qt::Key_Return }.toString(QKeySequence::NativeText);
		const auto single   = QKeySequence{ Qt::CTRL | Qt::SHIFT | Qt::Key_Return }.toString(QKeySequence::NativeText);
		const auto anchored = [](QAbstractButton* aButton) {
			const auto target = qt::standard_tooltip_target(*aButton, aButton->rect().center());
			return target.has_value() && target->Anchor == aButton->rect() && target->Text == aButton->toolTip();
		};

		present(qt::draft_review_state::none);
		expect(words("threadPublishDraft") == QStringLiteral("Add single comment") && words("threadReviewDraft") == QStringLiteral("Start a review")
				   && button(view, "threadPublishDraft")->toolTip() == QStringLiteral("Post this one comment to the pull request now  %1").arg(single)
				   && button(view, "threadReviewDraft")->toolTip() == QStringLiteral("Keep this comment pending in a review you finish later  %1").arg(main)
				   && anchored(button(view, "threadPublishDraft")) && anchored(button(view, "threadReviewDraft"))
				   && button(view, "threadReviewDraft")->isEnabled() && privacy != nullptr && privacy->accessibleName() == QStringLiteral("Private until you publish"),
			   "with no review in progress: Add single comment with ⌘⇧↩ and Start a review with ⌘↩, each named in its anchored tooltip");
		press();
		expect(actions.back() == qt::card_action::review_draft, "⌘↩ is Start a review, the default (owner: \"Start a review, add to review should be the default for cmd+enter\")");
		press(Qt::ControlModifier | Qt::ShiftModifier);
		expect(actions.back() == qt::card_action::publish_draft, "⌘⇧↩ is Add single comment");
		press(Qt::ControlModifier | Qt::AltModifier);
		expect(actions.back() == qt::card_action::finish_review, "⌘⌥↩ asks to finish the review; the window decides whether one is in progress");
		press(Qt::ControlModifier | Qt::ShiftModifier, Qt::Key_M);
		expect(actions.back() == qt::card_action::maximize_draft, "⌘⇧M maximizes, now that ⌘⇧↩ is Add single comment");
		button(view, "threadReviewDraft")->click();
		expect(actions.back() == qt::card_action::review_draft, "Start a review asks to keep it pending");

		present(qt::draft_review_state::in_progress);
		expect(words("threadPublishDraft") == QStringLiteral("Add single comment") && words("threadReviewDraft") == QStringLiteral("Add review comment")
				   && button(view, "threadReviewDraft")->toolTip().endsWith(main) && button(view, "threadPublishDraft")->toolTip().endsWith(single)
				   && privacy->accessibleName() == QStringLiteral("Pending until you finish the review"),
			   "while a review is in progress Add review comment is the main action, with ⌘↩, and the note says the review is pending");
		press();
		expect(actions.back() == qt::card_action::review_draft, "and ⌘↩ adds the comment to the review");

		present(qt::draft_review_state::pending);
		auto* const state = view.open_card()->findChild<QWidget*>(QStringLiteral("threadState"));
		expect(words("threadReviewDraft") == QStringLiteral("Update review comment") && state != nullptr && state->accessibleName() == QStringLiteral("Pending"),
			   "a pending draft's main action is Update review comment, and its header says Pending");
		press();
		button(view, "threadReviewDraft")->click();
		expect(actions[actions.size() - 2U] == qt::card_action::close_draft && actions.back() == qt::card_action::close_draft,
			   "which only closes it, still pending: ⌘↩ and the button alike");

		present(qt::draft_review_state::none);
		auto* const singleButton = button(view, "threadPublishDraft");
		auto* const review       = button(view, "threadReviewDraft");
		auto* const trash        = button(view, "threadDeleteDraft");
		auto* const more         = button(view, "threadReviewDraftMore");
		const auto  middle       = [](QWidget* aWidget) { return aWidget->mapTo(aWidget->window(), aWidget->rect().center()).y(); };
		expect(trash != nullptr && trash->toolTip() == QStringLiteral("Delete this comment") && trash->accessibleName() == QStringLiteral("Delete comment"),
			   "Delete is named for what it does");
		view.resize(520, 640);
		QApplication::processEvents();
		QApplication::processEvents();
		expect(singleButton->isVisible() && more != nullptr && !more->isVisible()
				   && std::abs(middle(singleButton) - middle(review)) <= 1 && std::abs(middle(trash) - middle(review)) <= 2
				   && singleButton->geometry().right() < review->geometry().left() && singleButton->width() >= singleButton->sizeHint().width(),
			   "with room: one row, the privacy note, Delete, then Add single comment beside the main action");
		view.resize(260, 640);
		QApplication::processEvents();
		QApplication::processEvents();
		expect(!singleButton->isVisible() && more->isVisible() && std::abs(middle(more) - middle(review)) <= 1
				   && std::abs(middle(trash) - middle(review)) <= 2 && more->geometry().left() - review->geometry().right() <= 1
				   && review->width() >= review->sizeHint().width() && view.open_card()->rect().contains(more->geometry().translated(more->parentWidget()->mapTo(view.open_card(), QPoint{ }))),
			   "in a narrow drawer: still one row, the main action with its split arrow joined to it, nothing stacked or cut off");
		more->click();
		QApplication::processEvents();
		auto* const menu = view.findChild<QMenu*>(QStringLiteral("threadReviewDraftMenu"));
		auto* const item = menu != nullptr ? menu->findChild<QAction*>(QStringLiteral("threadReviewDraftSingle")) : nullptr;
		expect(menu != nullptr && item != nullptr && item->text() == QStringLiteral("Add single comment") && item->shortcut() == QKeySequence{ Qt::CTRL | Qt::SHIFT | Qt::Key_Return },
			   "the split arrow's menu carries Add single comment with its key");
		item->trigger();
		menu->close();
		QApplication::processEvents();
		expect(actions.back() == qt::card_action::publish_draft, "and it adds the single comment");
	}

	/// \brief [[BPR-1290]]. Notes hang from their lines; one open thread is
	/// pinned exactly at its line and the others make room above and below.
	auto test_notes_hang_from_their_lines_and_one_opens_pinned() -> void
	{
		qt::conversation_cards_view view;
		view.resize(320, 600);
		view.show();
		QApplication::processEvents();
		const auto conversations = std::vector<conversation>{
			thread("t-late", "src/a.cpp", 40U),
			reply("c-late", "t-late", "author", "Late thread at line 40."),
			thread("t-early", "src/a.cpp", 10U),
			reply("c-early-1", "t-early", "author", "issue (blocking): Early thread at line 10."),
			reply("c-early-2", "t-early", "reviewer", "A reply that makes the thread taller when it is open."),
			thread("t-near", "src/a.cpp", 12U),
			reply("c-near", "t-near", "observer", "Two lines beneath the early thread."),
			thread("t-other", "src/b.cpp", 5U),
			reply("c-other", "t-other", "author", "Another file's thread."),
			thread("t-outdated", "src/a.cpp", 20U, false),
			reply("c-outdated", "t-outdated", "author", "Not on this comparison."),
		};
		auto lineY = std::map<std::uint32_t, int>{ { 10U, 100 }, { 12U, 130 }, { 40U, 500 } };
		view.set_line_locator([&lineY](core::diff_side, const std::uint32_t aLine) -> std::optional<int> {
			const auto found = lineY.find(aLine);
			return found == lineY.end() ? std::nullopt : std::optional{ found->second };
		});
		view.set_anchor_file(core::file_identity_of("src/a.cpp"));
		view.adopt(conversations);
		QApplication::processEvents();
		const auto top = [&view](const std::string& aId) { return view.note_rect(aId).top(); };

		const auto& cards = view.cards();
		expect(cards.size() == 4U, "this file's threads are shown, the other file's is not");
		expect(cards[0].Id == "t-early" && cards[0].Placed && top("t-early") == 96
				   && cards[1].Id == "t-near" && cards[1].Placed
				   && cards[2].Id == "t-late" && cards[2].Placed && top("t-late") == 496
				   && cards[3].Id == "t-outdated" && !cards[3].Placed && cards[3].Outdated,
			   "notes hang from their lines, nudged up four pixels, in line order, the unplaceable one last");
		expect(top("t-near") > 126 && top("t-near") >= view.note_rect("t-early").bottom() && !overlapping(cards),
			   "a note whose line is under the note above sits beneath it instead");
		expect(top("t-outdated") >= view.note_rect("t-late").bottom(), "the unplaced note follows the last placed one");
		expect(view.line_marks(core::diff_side::right).size() == 3U
				   && view.line_marks(core::diff_side::right).front().Label == core::feedback_label::issue,
			   "each placed thread has a gutter marker with its first post's mark; the outdated one has none");

		// The comparison scrolls: the lines move, the notes follow.
		lineY                          = { { 10U, 40 }, { 12U, 70 }, { 40U, 440 } };
		const auto layoutsBeforeScroll = view.layouts();
		view.refresh_geometry();
		expect(view.layouts() == layoutsBeforeScroll, "following the code retains the forest and measured text");
		expect(top("t-early") == 36 && top("t-late") == 436 && !overlapping(view.cards()),
			   "relaying against moved lines moves the notes with them");

		// Opening t-near pins it at its line; t-early makes room above it.
		auto chosen = std::vector<std::pair<std::string, bool>>{ };
		view.set_thread_dispatch([&chosen](const std::string& aId, const bool aOpen) { chosen.emplace_back(aId, aOpen); });
		press_at(view, view.note_rect("t-near").center());
		expect(chosen == std::vector<std::pair<std::string, bool>>{ { "t-near", true } } && view.open_thread_id() == "t-near"
				   && view.current_thread() == "t-near",
			   "a press on a note opens its thread, makes it current and reports it");
		expect(top("t-near") == 66 && view.open_card()->geometry().top() == 66, "the open thread is pinned exactly at its line");
		expect(view.note_rect("t-early").bottom() < top("t-near") && top("t-early") < 36,
			   "the note above makes room by moving up");
		expect(top("t-late") >= view.note_rect("t-near").bottom() && !overlapping(view.cards()),
			   "the notes below make room by moving down");
		const auto marks = view.line_marks(core::diff_side::right);
		expect(std::ranges::count_if(marks, &qt::thread_line_mark::Open) == 1 && std::ranges::find_if(marks, &qt::thread_line_mark::Open)->Line == 11U,
			   "the open thread's gutter marker says it is open");
		// Negative control: with the thread closed, nothing is pinned and the
		// early note returns to its own line.
		view.close_thread();
		expect(top("t-early") == 36 && view.open_thread_id().empty()
				   && std::ranges::none_of(view.line_marks(core::diff_side::right), &qt::thread_line_mark::Open),
			   "negative control: closed, the early note hangs from its own line again and nothing is open");

		// Keys: Down moves without opening, Return opens, Esc closes.
		view.setFocus(Qt::OtherFocusReason);
		view.set_current_thread("t-early");
		key(view, Qt::Key_Down);
		expect(view.current_thread() == "t-near" && view.open_thread_id().empty(), "Down moves to the next thread without opening it");
		key(view, Qt::Key_Return);
		expect(view.open_thread_id() == "t-near", "Return opens the current thread");
		key(view, Qt::Key_Escape);
		expect(view.open_thread_id().empty() && view.current_thread() == "t-near", "Esc closes the open thread and keeps it current");
		expect(view.hit_test(view.note_rect("t-late").center()) == std::optional<std::size_t>{ 2U }, "hit testing names the note");
		view.set_current_thread("c-late");
		expect(view.current_thread() == "t-late", "a reply's identity names its thread");

		// My draft on this comparison is a dashed note at its line. [[BPR-1212]]:
		// opening it makes it editable again, in place, as its composer.
		const auto comparison = core::file_comparison_digest{ };
		view.set_draft(core::review_comment_draft{
						   .Anchor = core::source_line_ref{ .Comparison = comparison, .Side = core::diff_side::right, .Line = 39U },
						   .Body   = "Same question; check the default before publishing.",
					   },
					   comparison);
		QApplication::processEvents();
		const auto draft = std::ranges::find_if(view.cards(), &qt::thread_card::Draft);
		expect(draft != view.cards().end() && draft->Placed && draft->Line == std::optional<std::uint32_t>{ 40U } && !overlapping(view.cards()),
			   "the draft is a note at its one-based line");
		const auto draftId = draft->Id;
		auto       actions = std::vector<std::pair<qt::card_action, std::string>>{ };
		view.set_action_dispatch([&actions](const qt::card_action aAction, const std::string& aId) { actions.emplace_back(aAction, aId); });
		auto typed = std::vector<QString>{ };
		view.set_draft_text_dispatch([&typed](const QString& aText) { typed.push_back(aText); });
		view.open_thread(draftId);
		QApplication::processEvents();
		expect(view.writing_draft() && view.draft_editor() != nullptr && button(view, "threadPublishDraft") != nullptr
				   && button(view, "threadDeleteDraft") != nullptr && button(view, "threadMaximizeDraft") != nullptr
				   && button(view, "threadEditDraft") == nullptr && button(view, "threadReplyField") == nullptr,
			   "the open draft is its composer: a field, Publish, Discard and Maximize, and no Edit or Reply");
		expect(button(view, "threadPublishDraft")->toolTip().startsWith(QStringLiteral("Post this one comment to the pull request now")),
			   "Publish says it posts this one comment now (owner, 2026-10-08)");
		expect(view.post_count() == 5U, "the draft is not counted as a post");
		view.present_draft(qt::draft_composer_presentation{
			.Body        = QStringLiteral("Same question; check the default before publishing."),
			.Publishable = true,
			.Lines       = QStringLiteral("Line 40"),
		});
		QApplication::processEvents();
		expect(view.draft_editor()->toPlainText() == QStringLiteral("Same question; check the default before publishing.") && typed.empty()
				   && button(view, "threadPublishDraft")->isEnabled(),
			   "the composer shows the draft editor's text without echoing it back, and Publish is enabled");
		const auto writingMarks = view.line_marks(core::diff_side::right);
		expect(std::ranges::any_of(writingMarks, [](const qt::thread_line_mark& aMark) { return aMark.Line == 39U && aMark.Writing; }),
			   "the line being written on carries the writing mark for its blue bracket");
		view.draft_editor()->insertPlainText(QStringLiteral(" Now."));
		expect(!typed.empty() && typed.back().endsWith(QStringLiteral(" Now.")), "each edit goes to the draft editor");
		const auto layouts = view.layouts();
		view.set_draft(core::review_comment_draft{
						   .Anchor = core::source_line_ref{ .Comparison = comparison, .Side = core::diff_side::right, .Line = 39U },
						   .Body   = "Same question; check the default before publishing. Now.",
					   },
					   comparison);
		expect(view.layouts() == layouts && view.draft_editor() != nullptr && view.draft_editor()->hasFocus(),
			   "typing the draft lays nothing out again and keeps the composer and its focus");
		// [[BPR-1345]] v002: ⌘⇧M maximizes, ⌘⇧↩ adds the single comment.
		QKeyEvent maximize{ QEvent::KeyPress, Qt::Key_M, Qt::ControlModifier | Qt::ShiftModifier };
		QApplication::sendEvent(view.draft_editor(), &maximize);
		QKeyEvent publish{ QEvent::KeyPress, Qt::Key_Return, Qt::ControlModifier | Qt::ShiftModifier };
		QApplication::sendEvent(view.draft_editor(), &publish);
		QKeyEvent escape{ QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier };
		QApplication::sendEvent(view.draft_editor(), &escape);
		expect(actions.size() == 3U && actions[0].first == qt::card_action::maximize_draft && actions[1].first == qt::card_action::publish_draft
				   && actions[2].first == qt::card_action::close_draft,
			   "in the draft, Ctrl+Shift+M maximizes, Ctrl+Shift+Return publishes it alone and Esc asks to close; the window decides");
		view.present_draft(qt::draft_composer_presentation{
			.Body        = QStringLiteral("```suggestion\nint b = 2;\n```"),
			.Publishable = true,
			.Lines       = QStringLiteral("Line 40"),
			.Change      = core::suggested_change{ .Fenced = true, .Removed = { "int b = 1;" }, .Added = { "int b = 2;" } },
		});
		QApplication::processEvents();
		auto* const change = view.findChild<QWidget*>(QStringLiteral("threadDraftChange"));
		expect(change != nullptr && change->isVisible() && change->accessibleDescription().contains(QStringLiteral("removes 1, adds 1"))
				   && view.draft_editor()->lineWrapMode() == QPlainTextEdit::NoWrap
				   && view.draft_editor()->textCursor().position() == view.draft_editor()->toPlainText().lastIndexOf(QLatin1Char{ '\n' }),
			   "a suggestion shows what it changes, unwrapped, with the caret at the end of the block's last line");
		view.end_draft();
		QApplication::processEvents();
		expect(!view.writing_draft() && view.open_thread_id().empty() && std::ranges::any_of(view.cards(), &qt::thread_card::Draft),
			   "leaving the written draft keeps it as its note at its line");
		// A reply draft is written inside its thread and draws no note.
		view.set_draft(core::review_comment_draft{
						   .Anchor        = core::source_line_ref{ .Comparison = comparison, .Side = core::diff_side::right, .Line = 9U },
						   .Body          = "A reply",
						   .ReplyToThread = std::string{ "early" },
					   },
					   comparison);
		expect(std::ranges::none_of(view.cards(), &qt::thread_card::Draft), "a reply draft draws no note of its own");
		// [[BPR-1326]]. My other saved drafts are notes at their lines,
		// whichever line the editor holds; the editor's draft is drawn as the
		// editor holds it, never twice.
		const auto saved = [&comparison](const std::uint64_t aId, const std::uint32_t aLine, std::string aBody) {
			auto value   = core::review_comment_draft{ };
			value.Id     = core::review_draft_id{ aId };
			value.Anchor = core::source_line_ref{ .Comparison = comparison, .Side = core::diff_side::right, .Line = aLine };
			value.Body   = std::move(aBody);
			return value;
		};
		view.set_draft(saved(7U, 39U, "Edited in the editor."), comparison);
		auto savedReply          = saved(9U, 4U, "A saved reply");
		savedReply.ReplyToThread = std::string{ "early" };
		auto savedPublished      = saved(11U, 6U, "Already published");
		savedPublished.State     = core::review_draft_state::published;
		view.set_saved_drafts({ saved(7U, 39U, "Saved before the edit."), saved(8U, 2U, "A draft on another line."), savedReply, saved(10U, 5U, ""), savedPublished });
		QApplication::processEvents();
		const auto bodyOf = [](const qt::thread_card& aCard) { return aCard.Posts.empty() ? QString{ } : aCard.Posts.front().Body; };
		auto       notes  = std::vector<qt::thread_card>{ };
		std::ranges::copy_if(view.cards(), std::back_inserter(notes), &qt::thread_card::Draft);
		expect(notes.size() == 2U
				   && std::ranges::any_of(notes, [&](const qt::thread_card& aCard) { return aCard.Line == std::optional<std::uint32_t>{ 40U } && bodyOf(aCard) == QStringLiteral("Edited in the editor."); })
				   && std::ranges::any_of(notes, [&](const qt::thread_card& aCard) { return aCard.Line == std::optional<std::uint32_t>{ 3U } && bodyOf(aCard) == QStringLiteral("A draft on another line."); }),
			   "a saved draft on another line is a note; the editor's draft shows its own text once; empty, reply and published drafts draw none");
		expect(std::ranges::any_of(view.line_marks(core::diff_side::right), [](const qt::thread_line_mark& aMark) { return aMark.Draft && aMark.Line == 2U; }),
			   "the saved draft has its gutter marker");
		// Negative control: with no saved drafts only the editor's draft shows.
		view.set_saved_drafts({ });
		expect(std::ranges::count_if(view.cards(), &qt::thread_card::Draft) == 1, "negative control: without saved drafts only the editor's draft is a note");
		view.set_draft(std::nullopt, std::nullopt);
		lineY.clear();
		view.refresh_geometry();
		expect(std::ranges::none_of(view.cards(), &qt::thread_card::Placed),
			   "invalidated comparison geometry withdraws every placed note");
		view.clear();
		expect(view.cards().empty(), "clearing empties the notes");
		view.hide();
	}

	/// \brief [[BPR-1290]]. Threads beyond the drawer's edges are chips, nearest
	/// first; a wheel scrolls the code; only what the code cannot take scrolls
	/// the notes.
	auto test_chips_wheel_and_overflow() -> void
	{
		qt::conversation_cards_view view;
		view.resize(320, 400);
		view.show();
		QApplication::processEvents();
		auto conversations = std::vector<conversation>{ };
		for (const auto line : { 5U, 30U, 60U, 90U, 120U, 150U, 180U, 210U }) {
			const auto id = "t" + std::to_string(line);
			conversations.push_back(thread(id, "src/a.cpp", line));
			conversations.push_back(reply(id + "-c", id, "author", "suggestion: thread at line " + std::to_string(line)));
		}
		auto offset = 0;
		view.set_line_locator([&offset](core::diff_side, const std::uint32_t aLine) -> std::optional<int> {
			return static_cast<int>(aLine) * 20 - offset;
		});
		auto codeMoves = true;
		auto wheels    = 0;
		view.set_scroll_forward([&](QWheelEvent& aEvent) {
			++wheels;
			if (codeMoves) {
				offset -= aEvent.angleDelta().y();
			}
			return codeMoves;
		});
		view.adopt(conversations, core::file_identity_of("src/a.cpp"));
		view.refresh_geometry();
		QApplication::processEvents();
		auto chips = view.edge_chips();
		expect(chips.size() == 4U && chips[0]->text() == QStringLiteral("30") && chips[1]->text() == QStringLiteral("60")
				   && chips[2]->text() == QStringLiteral("90") && chips[3]->text() == QStringLiteral("+4"),
			   "threads below the drawer are chips, nearest first, three and then a count");
		offset = 2000;
		view.refresh_geometry();
		chips = view.edge_chips();
		expect(!chips.empty() && chips.front()->text() == QStringLiteral("90"),
			   "scrolled down, the threads above are chips, nearest first");
		offset = 0;
		view.refresh_geometry();
		view.edge_chips().front()->click();
		expect(view.open_thread_id() == "t30", "a chip opens its thread");

		const auto centre = QPointF{ view.viewport()->rect().center() };
		auto       down   = QWheelEvent{ centre, view.viewport()->mapToGlobal(centre), QPoint{ }, QPoint{ 0, -120 }, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false };
		QApplication::sendEvent(view.viewport(), &down);
		expect(wheels == 1 && view.overflow() == 0, "a wheel over the notes is the code's");
		codeMoves = false;
		view.refresh_geometry();
		QApplication::sendEvent(view.viewport(), &down);
		expect(wheels == 2 && view.overflow() > 0, "when the code cannot move, the notes take the overflow");
		auto up = QWheelEvent{ centre, view.viewport()->mapToGlobal(centre), QPoint{ }, QPoint{ 0, 120 }, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false };
		QApplication::sendEvent(view.viewport(), &up);
		expect(view.overflow() == 0 && wheels == 2, "scrolling back gives the overflow up before the code moves");
		view.hide();
	}

	/// \brief [[BPR-1290]]. R, N and the reply written in place.
	auto test_reply_in_place_and_next_new() -> void
	{
		qt::conversation_cards_view view;
		view.resize(360, 700);
		view.show();
		QApplication::processEvents();
		view.set_line_locator([](core::diff_side, const std::uint32_t aLine) -> std::optional<int> { return static_cast<int>(aLine) * 6; });
		view.adopt({ thread("t10", "src/a.cpp", 10U), reply("c10", "t10", "a", "first"),
					 thread("t30", "src/a.cpp", 30U), reply("c30", "t30", "b", "second"),
					 thread("t50", "src/a.cpp", 50U), reply("c50", "t50", "c", "third") },
				   core::file_identity_of("src/a.cpp"));
		view.set_read_state({ { .Thread = "t30", .UnreadPosts = { "c30" } }, { .Thread = "t50", .UnreadPosts = { "c50" } } });
		auto actions = std::vector<std::pair<qt::card_action, std::string>>{ };
		view.set_action_dispatch([&](const qt::card_action aAction, const std::string& aId) {
			actions.emplace_back(aAction, aId);
			if (aAction == qt::card_action::reply) {
				view.begin_reply(aId);
			}
		});
		auto typed = std::vector<std::pair<std::string, QString>>{ };
		view.set_reply_text_dispatch([&typed](const std::string& aThread, const QString& aText) { typed.emplace_back(aThread, aText); });
		view.setFocus(Qt::OtherFocusReason);
		key(view, Qt::Key_N);
		expect(view.open_thread_id() == "t30", "N opens the first thread with new posts");
		key(view, Qt::Key_N);
		expect(view.open_thread_id() == "t50", "N again opens the next");
		key(view, Qt::Key_N);
		expect(view.open_thread_id() == "t30", "after the last it wraps to the first");
		key(view, Qt::Key_N, Qt::ShiftModifier);
		expect(view.open_thread_id() == "t50", "Shift+N goes back");
		key(view, Qt::Key_R);
		auto* const editor = view.reply_editor();
		expect(!actions.empty() && actions.back() == std::pair{ qt::card_action::reply, std::string{ "t50" } } && editor != nullptr && editor->hasFocus(),
			   "R asks to reply to the open thread, and the editor opens in place with focus");
		auto* const publish = button(view, "threadReplyPublish");
		expect(publish != nullptr && !publish->isEnabled(), "Publish waits for text and a draft that can publish");
		editor->setPlainText(QStringLiteral("question: Is this still needed?"));
		expect(!typed.empty() && typed.back() == std::pair{ std::string{ "t50" }, QStringLiteral("question: Is this still needed?") },
			   "each edit of the reply goes to the draft editor");
		auto* const labels = view.reply_label_controls();
		expect(labels != nullptr && labels->chip()->accessibleName() == QStringLiteral("Label: question"),
			   "the reply's label follows its first words, on the composer's own chip");
		view.present_reply(qt::thread_reply_presentation{ .Thread = "t50", .Body = QStringLiteral("question: Is this still needed?"), .Publishable = true });
		expect(publish->isEnabled(), "a draft that can publish enables Publish");
		auto submit = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::ControlModifier };
		QApplication::sendEvent(editor, &submit);
		QApplication::processEvents();
		expect(actions.back() == std::pair{ qt::card_action::publish_reply, std::string{ "t50" } } && view.reply_editor() == nullptr,
			   "Command+Return publishes the reply in place and folds the editor");
		view.present_reply(qt::thread_reply_presentation{ .Thread = "t50", .Body = QStringLiteral("kept"), .Publishable = true });
		view.close_thread();
		view.open_thread("t50");
		QApplication::processEvents();
		expect(view.reply_editor() != nullptr && view.reply_editor()->toPlainText() == QStringLiteral("kept"),
			   "a kept reply draft reopens in its thread with its text");
		view.present_reply(std::nullopt);
		auto escape = QKeyEvent{ QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier };
		QApplication::sendEvent(view.reply_editor(), &escape);
		QApplication::processEvents();
		expect(view.reply_editor() != nullptr && view.hasFocus(), "Esc in a reply with text keeps it and returns to the notes");
		view.hide();
	}
} // namespace

namespace {
	// [[BPR-0972]]. One thread's read state reaches the notes as that thread
	// alone: a thread whose note is not shown lays nothing out, a shown one
	// lays out its file's notes, and both agree with the complete state.
	/// \brief Types one character into a widget, as a key press does.
	void type(QWidget& aTarget, const QChar aCharacter)
	{
		const auto key   = aCharacter == QLatin1Char{ ':' } ? Qt::Key_Colon : Qt::Key_A + (aCharacter.unicode() - u'a');
		auto       event = QKeyEvent{ QEvent::KeyPress, key, Qt::NoModifier, QString{ aCharacter } };
		QApplication::sendEvent(&aTarget, &event);
		QApplication::processEvents();
	}

	/// \brief [[BPR-1290]]. The reply written in place has the composer's own
	/// label controls ([[BPR-1083]]): the chip, and ⌘L opening the label menu.
	/// [[BPR-1565]] removed the `:xx:` codes (owner, 2026-10-16), which the
	/// owner's v003 Reject (": chortcodes arent working in the reply
	/// comments") had brought here; a typed code now stays as typed.
	auto test_reply_in_place_has_the_composers_label_controls() -> void
	{
		qt::conversation_cards_view view;
		view.resize(320, 700);
		view.show();
		QApplication::processEvents();
		view.set_line_locator([](core::diff_side, const std::uint32_t aLine) -> std::optional<int> { return static_cast<int>(aLine) * 6; });
		view.adopt({ thread("t10", "src/a.cpp", 10U), reply("c10", "t10", "a", "first") }, core::file_identity_of("src/a.cpp"));
		auto typed = std::vector<QString>{ };
		view.set_reply_text_dispatch([&typed](const std::string&, const QString& aText) { typed.push_back(aText); });
		// [[BPR-1566]]. The view's reference dispatch reaches the reply's menu.
		auto asked = std::vector<core::reference_page>{ };
		view.set_reference_dispatch([&asked](const core::reference_page aPage) { asked.push_back(aPage); });
		view.open_thread("t10");
		view.begin_reply("t10");
		QApplication::processEvents();
		auto* const editor = view.reply_editor();
		auto* const labels = view.reply_label_controls();
		expect(editor != nullptr && labels != nullptr && editor->hasFocus(), "the reply opens in place with the composer's label controls");
		expect(labels->chip()->isVisible() && labels->chip()->accessibleName() == QStringLiteral("Label: Plain comment"),
			   "the reply's chip is the composer's chip and starts plain");
		expect(!labels->blocking_toggle()->isVisible(), "the drawer leaves the decoration toggles in the chip's menu");
		view.present_reply(qt::thread_reply_presentation{ .Thread = "t10", .Body = QString{ }, .Publishable = true });

		// Negative control for the removed codes: `:qu:` stays as typed.
		for (const auto character : QStringLiteral(":qu:")) {
			type(*editor, character);
		}
		QApplication::processEvents();
		expect(editor->toPlainText() == QStringLiteral(":qu:") && !labels->state().Label.has_value() && labels->statistics().Edits == 0U,
			   "negative control: a reply's :qu: stays as typed, with no label");
		expect(!typed.empty() && typed.back() == QStringLiteral(":qu:"), "and the draft editor is given exactly that text");
		editor->clear();
		QApplication::processEvents();
		for (const auto character : QStringLiteral("why")) {
			type(*editor, character);
		}

		// ⌘L opens the label menu from the reply.
		auto command = QKeyEvent{ QEvent::KeyPress, Qt::Key_L, Qt::ControlModifier };
		QApplication::sendEvent(editor, &command);
		QApplication::processEvents();
		expect(labels->menu()->isVisible(), "⌘L opens the label menu from the reply");
		labels->menu()->label_button(core::feedback_label::suggestion)->click();
		QApplication::processEvents();
		expect(editor->toPlainText() == QStringLiteral("suggestion: why") && !labels->menu()->isVisible(), "the menu changes the reply's label");
		expect(labels->chip()->accessibleName() == QStringLiteral("Label: suggestion"), "the chip names the label chosen");
		expect(!typed.empty() && typed.back() == QStringLiteral("suggestion: why"), "the labelled text goes to the draft editor");

		// [[BPR-1566]]. About Conventional Comments in the reply's menu names
		// the page through the view and leaves the reply as it is.
		QApplication::sendEvent(editor, &command);
		QApplication::processEvents();
		auto* const about = labels->menu()->about_button();
		expect(labels->reference_offered() && about->isVisible(), "the reply's menu ends with About Conventional Comments");
		const auto replies = typed.size();
		about->click();
		QApplication::processEvents();
		expect(asked == std::vector<core::reference_page>{ core::reference_page::conventional_comments } && !labels->menu()->isVisible()
				   && editor->toPlainText() == QStringLiteral("suggestion: why") && typed.size() == replies,
			   "it names the page once, closes the menu and changes nothing in the reply");
		view.set_reference_dispatch({ });
		expect(!labels->reference_offered(), "unbinding the view takes it from the open reply's menu");
		view.hide();
	}

	void test_one_thread_read_state_matches_the_complete_state()
	{
		for (const auto threads : { 32U, 4096U }) {
			auto conversations = std::vector<conversation>{ };
			auto state         = std::vector<qt::thread_read_state>{ };
			for (unsigned index = 0; index < threads; ++index) {
				const auto id = "t" + std::to_string(index);
				conversations.push_back(thread(id, "src/file" + std::to_string(index % (threads / 4U)) + ".cpp", index + 1U));
				conversations.push_back(reply(id + "-c", id, "someone", "a remark"));
				state.push_back(qt::thread_read_state{ .Thread = id, .UnreadPosts = { id + "-c" } });
			}
			qt::conversation_cards_view view;
			view.resize(360, 900);
			view.adopt(conversations, core::file_identity_of("src/file0.cpp"));
			view.set_read_state(state);
			static_cast<void>(view.unread_threads_in_file_order());
			const auto layouts = view.layouts();
			const auto visited = view.layout_nodes_visited();
			const auto hidden  = view.set_thread_read_state("t1", { });
			expect(hidden == std::vector<std::string>{ "t1-c" } && view.layouts() == layouts,
				   "a hidden thread's read state lays nothing out and returns its previous posts");
			static_cast<void>(view.set_thread_read_state("t0", { }));
			std::cout << "cards-read-one threads=" << threads << " layouts=" << view.layouts() - layouts
					  << " nodes_visited=" << view.layout_nodes_visited() - visited << '\n';
			expect(view.layouts() == layouts + 1U && view.layout_nodes_visited() - visited == 8U,
				   "a shown thread's read state lays out its own file's conversations only");
			state[0].UnreadPosts.clear();
			state[1].UnreadPosts.clear();
			auto complete = qt::conversation_cards_view{ };
			complete.resize(360, 900);
			complete.adopt(conversations, core::file_identity_of("src/file0.cpp"));
			complete.set_read_state(state);
			expect(view.unread_threads_in_file_order() == complete.unread_threads_in_file_order()
					   && view.unread_post_count() == complete.unread_post_count()
					   && view.unread_marked_lines(core::diff_side::right) == complete.unread_marked_lines(core::diff_side::right),
				   "one thread at a time agrees with the complete read state");
			for (unsigned index = 0U; index < threads / 4U; ++index) {
				const auto file = core::file_identity_of("src/file" + std::to_string(index) + ".cpp");
				expect(view.unread_thread_count(file) == complete.unread_thread_count(file)
						   && view.unread_thread_count(file) == (index < 2U ? 3U : 4U),
					   "incremental per-file unread counts match a complete read-state projection");
			}
		}
	}

	void test_discuss_wheel_moves_readable_lines_without_changing_selection()
	{
		qt::conversation_cards_view view;
		view.resize(360, 180);
		view.set_feed(true);
		view.set_scope(qt::cards_scope::whole_pull_request);
		auto data = std::vector<conversation>{ };
		for (auto i = 0; i < 12; ++i) {
			const auto id = "t" + std::to_string(i);
			data.push_back(thread(id, "src/file.cpp", static_cast<std::uint32_t>(i + 1)));
			data.push_back(reply(id + "-c", id, "someone", "a remark"));
		}
		view.adopt(data);
		view.set_current_thread("t0");
		view.show();
		QApplication::processEvents();
		auto* bar = view.verticalScrollBar();
		bar->setValue(0);
		QWheelEvent wheel{ QPointF{ 10, 10 }, QPointF{ 10, 10 }, QPoint{ }, QPoint{ 0, -120 }, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false };
		QApplication::sendEvent(view.viewport(), &wheel);
		expect(bar->value() > 0 && view.current_thread() == "t0", "in Discuss a wheel scrolls the list and keeps the current thread");
	}

	void test_thread_menus_and_defect_badges()
	{
		qt::conversation_cards_view view;
		view.resize(430, 420);
		view.show();
		view.set_action_dispatch([](auto, const auto&) { });
		view.set_context_menu_builder([](QMenu& aMenu, const std::string&) { aMenu.addAction(QStringLiteral("Classify as defect…")); });
		view.adopt({ thread("a", "src/a.cpp", 10), reply("a-1", "a", "reviewer", "One thread"), thread("b", "src/a.cpp", 20), reply("b-1", "b", "reviewer", "Another") });
		view.set_defect_badges({ { "a", { QStringLiteral("[open] minor · correctness"), QStringLiteral("Nobody has addressed this yet. Shared context available.") } } });
		QApplication::processEvents();
		view.open_thread("a");
		QApplication::processEvents();
		auto* const badge = view.open_card()->findChild<QToolButton*>(QStringLiteral("threadDefectBadge"));
		expect(badge != nullptr && badge->isVisible(), "the classified open thread shows its badge");
		auto* accessible = QAccessible::queryAccessibleInterface(badge);
		expect(accessible && accessible->role() == QAccessible::Button
				   && accessible->text(QAccessible::Name).contains(QStringLiteral("correctness"))
				   && accessible->text(QAccessible::Description).contains(QStringLiteral("Shared context")),
			   "badge accessibility carries the classification and explained context");
		for (const auto& identity : { std::string{ "a" }, std::string{ "b" } }) {
			auto* menu = view.thread_menu(identity);
			expect(menu && menu->actions().front()->text() == QStringLiteral("Reply")
					   && menu->actions().back()->text() == QStringLiteral("Classify as defect…"),
				   "every thread offers existing actions and the host-supplied defect actions");
			expect(!menu->actions()[1]->isEnabled() && !menu->actions()[1]->toolTip().isEmpty(), "unavailable existing actions explain their refusal");
			delete menu;
		}
		view.set_current_thread("a");
		for (const auto pressed : { Qt::Key_Menu, Qt::Key_F10 }) {
			QKeyEvent event{ QEvent::KeyPress, pressed, pressed == Qt::Key_F10 ? Qt::ShiftModifier : Qt::NoModifier };
			QApplication::sendEvent(&view, &event);
			QApplication::processEvents();
			auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
			expect(menu && menu->isVisible(), "context-menu key and Shift+F10 open the selected thread actions");
			menu->close();
			QApplication::processEvents();
		}
		const auto        point = view.note_rect("b").center();
		QContextMenuEvent rightClick{ QContextMenuEvent::Mouse, point, view.viewport()->mapToGlobal(point) };
		QApplication::sendEvent(view.viewport(), &rightClick);
		QApplication::processEvents();
		auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
		expect(menu && menu->isVisible(), "right-click opens the same thread menu");
		menu->close();
		QApplication::processEvents();
		view.set_defect_badges({ });
		QApplication::processEvents();
		expect(view.open_card()->findChild<QToolButton*>(QStringLiteral("threadDefectBadge")) == nullptr,
			   "a withdrawn badge leaves no stale action");
		view.set_feed(true);
		auto unavailable               = thread("unavailable", "src/a.cpp", 10U);
		unavailable.AnchorAvailability = application::github_conversation_anchor_availability::missing_current_line;
		auto general                   = application::github_conversation_projection{ };
		general.Id                     = "general";
		general.Kind                   = application::github_conversation_kind::issue_comment;
		general.Body                   = "A PR-wide comment";
		view.adopt({ unavailable, reply("unavailable-post", "unavailable", "reviewer", "No current line"), general });
		// [[BPR-1581]]. The core's route: a thread on a file with no current
		// line opens in Review among its file's unplaced threads; a PR-wide
		// comment has no place there and says why.
		for (const auto& identity : { std::string{ "unavailable" }, std::string{ "general" } }) {
			auto* const unavailableMenu = view.thread_menu(identity);
			const auto  menuActions     = unavailableMenu->actions();
			const auto  openInReview    = std::ranges::find_if(menuActions, [](const QAction* aAction) { return aAction->text() == QStringLiteral("Open in Review"); });
			const auto  onFile          = identity == "unavailable";
			expect(openInReview != menuActions.end() && (*openInReview)->isEnabled() == onFile && (onFile || !(*openInReview)->toolTip().isEmpty()),
				   "a thread on a file offers Open in Review even with no current line; a PR-wide comment keeps it disabled with a reason");
			delete unavailableMenu;
		}
		// [[BPR-1581]]. Beside the code the same menu offers Show in Discuss,
		// at the open card's picked post.
		view.set_feed(false);
		view.adopt({ unavailable, reply("unavailable-post", "unavailable", "reviewer", "No current line") }, core::file_identity_of("src/a.cpp"));
		QApplication::processEvents();
		auto* const reviewMenu = view.thread_menu("unavailable");
		expect(reviewMenu != nullptr, "the thread has a menu beside the code");
		auto* const show = reviewMenu->findChild<QAction*>(QStringLiteral("threadShowInDiscussAction"));
		expect(show != nullptr && show->text() == QStringLiteral("Show in Discuss") && show->isEnabled()
				   && std::ranges::none_of(reviewMenu->actions(), [](const QAction* aAction) { return aAction->text() == QStringLiteral("Open in Review"); }),
			   "Review's thread menu offers Show in Discuss and no Open in Review");
		delete reviewMenu;
	}

	// [[BPR-1029]]. A dispatch that relays the notes out still names the
	// thread whose button was pressed.
	auto test_a_dispatch_that_relays_out_still_names_the_pressed_thread() -> void
	{
		qt::conversation_cards_view view;
		view.resize(420, 900);
		view.show();
		QApplication::processEvents();
		view.set_feed(true);
		view.set_scope(qt::cards_scope::whole_pull_request);
		const auto first    = std::string{ "review-thread:pressed-first-v1" };
		const auto second   = std::string{ "review-thread:pressed-second-v1" };
		const auto arranged = [&view, &first, &second](const std::string& aFirstPath, const std::string& aSecondPath) {
			view.adopt({ thread(first, aFirstPath, 1U), reply("review-comment:first-v1", first, "someone", "first"),
						 thread(second, aSecondPath, 1U), reply("review-comment:second-v1", second, "someone", "second") });
		};
		arranged("src/a.cpp", "src/z.cpp");
		view.open_thread(first);
		QApplication::processEvents();
		auto reported = std::vector<std::string>{ };
		view.set_action_dispatch([&reported, &arranged](const qt::card_action, const std::string& aId) {
			arranged("src/z.cpp", "src/a.cpp");
			reported.push_back(aId);
		});
		button(view, "threadShowLine")->click();
		expect(view.cards().size() == 2U && view.cards()[0].Id == second && reported == std::vector<std::string>{ first },
			   "an action whose dispatch relays the notes out still names the pressed thread");
		view.close_thread();
		view.set_thread_dispatch([&reported, &view, &first](const std::string& aId, const bool) {
			view.set_current_thread(first);
			reported.push_back(aId);
		});
		press_at(view, view.note_rect(second).topLeft() + QPoint{ 2, 2 });
		expect(reported == std::vector<std::string>{ first, second },
			   "a press whose dispatch makes another thread current still names the pressed thread");
		view.hide();
	}

	/// \brief One open thread's New boundary, as the card shows it.
	struct new_boundary
	{
		QString               Pill;
		QWidget*              Rule = nullptr;
		std::vector<QWidget*> Posts;
		std::vector<QString>  Spoken;
		QImage                Image;
		QWidget*              Card = nullptr;
	};

	/// \brief Opens a thread of `aPosts` posts, the ones `aUnread` names new,
	/// and reads its New boundary: the header's pill, the rule, the posts.
	[[nodiscard]] auto open_with_unread(qt::conversation_cards_view& aView, const std::size_t aPosts, const std::vector<std::size_t>& aUnread) -> new_boundary
	{
		auto conversations = std::vector<conversation>{ thread("t-new", "src/a.cpp", 10U) };
		for (std::size_t index = 0U; index < aPosts; ++index) {
			conversations.push_back(reply("c-new-" + std::to_string(index), "t-new", index % 2U == 0U ? "reviewer-four" : "author-two",
										  "Post " + std::to_string(index) + " of the thread, long enough to wrap onto a second line in the drawer."));
		}
		aView.adopt(conversations);
		auto unread = std::vector<std::string>{ };
		for (const auto index : aUnread) {
			unread.push_back("c-new-" + std::to_string(index));
		}
		aView.set_read_state({ qt::thread_read_state{ .Thread = "t-new", .UnreadPosts = unread } });
		aView.open_thread("t-new");
		QApplication::processEvents();
		auto* const card = aView.open_card();
		expect(card != nullptr, "the thread opens in place");
		auto found = new_boundary{ };
		found.Card = card;
		found.Rule = card->findChild<QWidget*>(QStringLiteral("threadNewRule"));
		if (const auto* const pill = card->findChild<QWidget*>(QStringLiteral("threadNewPill")); pill != nullptr && pill->isVisible()) {
			found.Pill = pill->accessibleName();
		}
		for (auto* const post : card->findChildren<QWidget*>(QStringLiteral("threadPost"))) {
			found.Posts.push_back(post);
			found.Spoken.push_back(post->accessibleDescription());
		}
		std::ranges::sort(found.Posts, { }, [](const QWidget* aPost) { return aPost->geometry().top(); });
		found.Image = card->grab().toImage();
		return found;
	}

	/// \brief Whether a pixel is the New colour (pure blue in this test) or
	/// a blend of it over the panel.
	[[nodiscard]] auto bluish(const QColor& aColour) -> bool
	{
		return aColour.blue() - std::max(aColour.red(), aColour.green()) > 60;
	}

	/// \brief [[BPR-1290]], the owner's v002 Reject ("I don't think that
	/// horizontal new line looks good before the start of the thread"). The
	/// New rule is never a thread's first element: a thread whose first post
	/// is new says so with a pill in its header, and the rule stands only
	/// between a read post and the first new one, inset inside the card.
	auto test_new_boundary_is_never_the_first_thing_in_a_thread() -> void
	{
		qt::conversation_cards_view view;
		view.resize(360, 900);
		view.set_colours(QColor{ 0, 0, 255 }, QColor{ 0xD2, 0x99, 0x22 });
		view.set_line_locator([](core::diff_side, const std::uint32_t aLine) -> std::optional<int> { return static_cast<int>(aLine) * 10; });
		view.set_anchor_file(core::file_identity_of("src/a.cpp"));
		view.show();
		QApplication::processEvents();

		// All new: the header says so; no rule under it.
		auto allNew = open_with_unread(view, 3U, { 0U, 1U, 2U });
		expect(allNew.Pill == QStringLiteral("3 new posts") && allNew.Rule == nullptr,
			   "a thread whose posts are all new says so in its header and draws no rule under it");
		expect(allNew.Spoken == std::vector<QString>(3U, QStringLiteral("New")), "each new post keeps its own unread dot");

		// First read, then new: the rule stands between them, inside the card.
		auto readThenNew = open_with_unread(view, 3U, { 1U, 2U });
		expect(readThenNew.Pill == QStringLiteral("2 new posts") && readThenNew.Rule != nullptr,
			   "a read post followed by new ones keeps the count in the header and the rule between them");
		expect(readThenNew.Posts.size() == 3U && readThenNew.Rule->geometry().top() >= readThenNew.Posts[0]->geometry().bottom()
				   && readThenNew.Rule->geometry().bottom() <= readThenNew.Posts[1]->geometry().top(),
			   "the rule sits between the read post and the first new one, not above the thread's first post");
		expect(readThenNew.Spoken == std::vector<QString>{ QString{ }, QStringLiteral("New"), QStringLiteral("New") }, "only the new posts have dots");
		{
			const auto& image = readThenNew.Image;
			const auto  ratio = image.devicePixelRatio();
			const auto  postY = static_cast<int>(std::lround((readThenNew.Posts[0]->geometry().center().y()) * ratio));
			const auto  width = image.width();
			// The rule's row is the one of its rows with the most of the New
			// colour across it.
			auto ruleY = -1;
			auto most  = 0;
			for (auto y = static_cast<int>(readThenNew.Rule->geometry().top() * ratio); y < static_cast<int>((readThenNew.Rule->geometry().bottom() + 1) * ratio); ++y) {
				auto count = 0;
				for (auto x = 0; x < width; ++x) {
					count += bluish(image.pixelColor(x, y)) ? 1 : 0;
				}
				if (count > most) {
					most  = count;
					ruleY = y;
				}
			}
			expect(ruleY >= 0, "the rule draws in the New colour");
			auto first = -1;
			auto last  = -1;
			for (auto x = 0; x < width; ++x) {
				if (bluish(image.pixelColor(x, ruleY))) {
					first = first < 0 ? x : first;
					last  = x;
				}
			}
			std::cout << "new-boundary:rule-span=" << first << ".." << last << " of " << width << '\n';
			expect(first >= static_cast<int>(10 * ratio) && first < width / 4 && last <= width - static_cast<int>(8 * ratio) && most > width / 2,
				   "the rule runs across the card, inset: it starts inside the card's edge and stops short of its right side");
			auto edgeContinuous = true;
			for (auto x = 0; x < static_cast<int>(4 * ratio); ++x) {
				edgeContinuous = edgeContinuous && image.pixelColor(x, ruleY) == image.pixelColor(x, postY);
			}
			expect(edgeContinuous, "the card's border and amber edge run through the rule's row as they do beside a post");
		}

		// All read: nothing says new.
		auto allRead = open_with_unread(view, 3U, { });
		expect(allRead.Pill.isEmpty() && allRead.Rule == nullptr, "a thread with nothing new has no pill and no rule");

		// A single post, new and then read.
		auto singleNew = open_with_unread(view, 1U, { 0U });
		expect(singleNew.Pill == QStringLiteral("1 new post") && singleNew.Rule == nullptr, "a single new post: the header says so, no rule");
		auto singleRead = open_with_unread(view, 1U, { });
		expect(singleRead.Pill.isEmpty() && singleRead.Rule == nullptr, "a single read post: neither");

		// Negative control: new, read, new. The first post is new, so the
		// header says so and no rule appears before a later new post either.
		auto mixed = open_with_unread(view, 3U, { 0U, 2U });
		expect(mixed.Pill == QStringLiteral("2 new posts") && mixed.Rule == nullptr,
			   "negative control: when the first post is new, no rule appears anywhere in the thread");
		{
			auto noRule = true;
			for (auto y = 0; y < mixed.Image.height() && noRule; ++y) {
				auto run = 0;
				for (auto x = 0; x < mixed.Image.width(); ++x) {
					run    = bluish(mixed.Image.pixelColor(x, y)) ? run + 1 : 0;
					noRule = noRule && run < mixed.Image.width() / 3;
				}
			}
			expect(noRule, "negative control: no row of the card carries a long blue line");
		}
		view.hide();
	}
} // namespace

namespace {
	// [[BPR-1084]]. The reply written in place shows a small mark when its
	// body carries a suggested change: native in a thread on a current head
	// line, text only in an outdated thread, a file comment or on the base
	// side, each from the accepted projection's own facts. The mark changes
	// nothing in the text and is not shown for prose.
	void test_a_reply_suggestion_is_marked_native_or_text_only()
	{
		using availability          = application::github_conversation_anchor_availability;
		auto outdated               = thread("t-outdated", "src/b.cpp", 4U);
		outdated.Line               = std::nullopt;
		outdated.OriginalLine       = 4U;
		outdated.AnchorAvailability = availability::outdated_revision;
		outdated.Outdated           = true;
		auto file                   = thread("t-file", "src/c.cpp", 7U);
		file.RemoteSubject          = application::github_conversation_remote_subject::file;
		file.SemanticAnchor         = core::source_line_ref{ .Side = core::diff_side::right, .Line = 6U };
		auto left                   = thread("t-left", "src/d.cpp", 5U);
		left.DiffSide               = application::github_conversation_diff_side::left;
		const auto line             = thread("t-line", "src/a.cpp", 10U);

		using subject = core::suggestion_subject;
		expect(qt::suggestion_placement_of(line) == core::suggestion_placement_observation{ .Subject = subject::current_line, .Side = core::diff_side::right }, "a current line thread is a line");
		expect(qt::suggestion_placement_of(outdated).Subject == subject::outdated_line, "an outdated thread");
		expect(qt::suggestion_placement_of(file).Subject == subject::file, "a file comment its link places at a line is still a file comment");
		expect(qt::suggestion_placement_of(left) == core::suggestion_placement_observation{ .Subject = subject::current_line, .Side = core::diff_side::left }, "a base-side thread keeps its side");
		auto missing = line;
		missing.Line = std::nullopt;
		expect(qt::suggestion_placement_of(missing).Subject == subject::unavailable, "no line, no claim of one");
		auto general = conversation{ .Id = "i-1", .Kind = application::github_conversation_kind::issue_comment };
		expect(qt::suggestion_placement_of(general).Subject == subject::conversation, "a pull request comment has no line");

		qt::conversation_cards_view view;
		view.resize(420, 900);
		view.show();
		QApplication::processEvents();
		view.adopt({ line, reply("c-line", "t-line", "a", "first"), outdated, reply("c-outdated", "t-outdated", "b", "second"),
					 file, reply("c-file", "t-file", "c", "third"), left, reply("c-left", "t-left", "d", "fourth") });
		view.set_scope(qt::cards_scope::whole_pull_request);
		view.set_action_dispatch([&view](const qt::card_action aAction, const std::string& aId) {
			if (aAction == qt::card_action::reply) {
				view.begin_reply(aId);
			}
		});
		QApplication::processEvents();
		const auto                                               patch = QStringLiteral("issue: Clamp it.\n\n```suggestion:-1+0\n\tclamp(width, 1);\n```\n");
		const auto                                               prose = QStringLiteral("suggestion: Consider clamping the width.");
		const std::array<std::pair<std::string, std::string>, 4> expected{ {
			{ "t-line", "native" },
			{ "t-outdated", "textual_only" },
			{ "t-file", "textual_only" },
			{ "t-left", "textual_only" },
		} };
		for (const auto& [identity, state] : expected) {
			view.open_thread(identity);
			view.begin_reply(identity);
			QApplication::processEvents();
			auto* const editor = view.reply_editor();
			expect(editor != nullptr && view.open_card() != nullptr, identity + ": the reply opens in place");
			auto* const mark = dynamic_cast<qt::suggestion_mark*>(view.open_card()->findChild<QWidget*>(QStringLiteral("threadReplySuggestionMark")));
			expect(mark != nullptr && !mark->isVisibleTo(view.open_card()), identity + ": an empty reply shows no mark");
			editor->setPlainText(prose);
			expect(!mark->isVisibleTo(view.open_card()) && mark->decision() == core::suggestion_applicability_decision{ }, identity + ": prose, even a suggestion label, shows no mark");
			editor->setPlainText(patch);
			QApplication::processEvents();
			expect(mark->isVisibleTo(view.open_card()) && mark->property("suggestionApplicability").toString().toStdString() == state, std::string{ identity }.append(": the mark says ").append(state));
			expect(!mark->toolTip().isEmpty() && mark->accessibleDescription() == mark->toolTip() && mark->accessibleName().startsWith(QStringLiteral("Suggested change, ")),
				   identity + ": named, and its tooltip says what GitHub will do");
			expect(editor->toPlainText() == patch, identity + ": the mark leaves every byte of the text as written");
			editor->clear();
			view.close_thread();
			QApplication::processEvents();
		}
		view.open_thread("t-outdated");
		view.begin_reply("t-outdated");
		expect(view.reply_editor() != nullptr && view.open_card() != nullptr, "the outdated reply opens in place");
		view.reply_editor()->setPlainText(patch);
		auto* const mark = dynamic_cast<qt::suggestion_mark*>(view.open_card()->findChild<QWidget*>(QStringLiteral("threadReplySuggestionMark")));
		expect(mark != nullptr, "the outdated reply has its suggestion mark");
		expect(mark->decision().Reason == core::suggestion_text_reason::outdated_line && mark->toolTip().contains(QStringLiteral("outdated"))
				   && mark->toolTip().contains(QStringLiteral("nothing moves to another line")),
			   "text only says why, and that nothing is moved");
		expect(mark->sizeHint() == QSize{ 16, 16 } && mark->focusPolicy() == Qt::NoFocus, "a 16 px mark, not a control");
		view.reply_editor()->clear();
		view.hide();
	}
} // namespace

namespace {
	/// [[BPR-1362]]. A large forest, then two smaller ones with other
	/// identities. Each identity is longer than a short-string buffer, so the
	/// replaced forest's strings are freed. MSVC's clear() then re-hashes every
	/// key, because the smaller forests leave few keys in many buckets;
	/// \brief [[BPR-1471]]. In Discuss the closed notes' blue new-post
	/// counts stand in one column whether or not a note carries the green
	/// resolved check. `BPR_DISCUSS_ALIGNMENT_PNG` names an offscreen render.
	// [[BPR-1564]], [[BPR-1562]]. A published comment on lines shows its
	// suggestion against the commented lines the locator answers, from its
	// start line; an outdated one, the suggested lines alone; a pull request
	// comment, plain code. Its code is asked for through the view's dispatch.
	void test_published_suggestions_show_against_their_lines()
	{
		qt::conversation_cards_view view;
		view.resize(480, 900);
		view.show();
		auto asked = std::vector<std::tuple<std::string, std::uint32_t, std::uint32_t>>{ };
		auto lines = std::optional<std::vector<std::string>>{ std::vector<std::string>{ "\tWidth  = aWidth;", "\tHeight = aHeight;" } };
		auto sent  = std::vector<application::markdown_code_syntax_request>{ };
		view.set_suggestion_locator([&](const QString& aPath, core::diff_side, const std::uint32_t aFirst, const std::uint32_t aLast) {
			asked.emplace_back(aPath.toStdString(), aFirst, aLast);
			return lines;
		});
		view.set_code_syntax_dispatch([&sent](std::vector<application::markdown_code_syntax_request> aRequests) {
			sent.insert(sent.end(), aRequests.begin(), aRequests.end());
		});
		auto root      = thread("t", "src/engine/renderer.cpp", 13U);
		root.StartLine = 12U;
		auto post      = reply("p", "t", "jiayin", "issue: zero sizes.\n\n```suggestion\n\tWidth  = std::max(aWidth, 1);\n\tHeight = std::max(aHeight, 1);\n```\n");
		post.Path      = root.Path;
		post.Line      = 13U;
		post.StartLine = 12U;
		view.adopt({ root, post });
		view.open_thread("t");
		QApplication::processEvents();
		expect(asked.size() == 1U && std::get<0>(asked[0]) == "src/engine/renderer.cpp" && std::get<1>(asked[0]) == 12U && std::get<2>(asked[0]) == 13U,
			   "the commented lines are asked for from the start line to the line");
		auto* const reader = dynamic_cast<qt::markdown_body_view*>(view.open_card()->findChild<QWidget*>(QStringLiteral("threadPostReader")));
		expect(reader != nullptr, "the post reads through the shared reader");
		const auto rows = [reader] {
			auto found = std::vector<std::pair<int, int>>{ };
			for (auto block = reader->rendered_view()->document()->begin(); block.isValid(); block = block.next()) {
				if (const auto row = block.blockFormat().intProperty(QTextFormat::UserProperty + 0x0B62); row == 1 || row == 2) {
					found.emplace_back(row, block.blockFormat().intProperty(QTextFormat::UserProperty + 0x0B63));
				}
			}
			return found;
		};
		expect(rows() == std::vector<std::pair<int, int>>{ { 1, 12 }, { 1, 13 }, { 2, 12 }, { 2, 13 } }, "two removed rows then two added rows, numbered 12 and 13");
		expect(std::ranges::any_of(sent, [](const application::markdown_code_syntax_request& aRequest) { return aRequest.Selector == "src/engine/renderer.cpp" && aRequest.Text && aRequest.Text->starts_with("\tWidth  = aWidth;"); }),
			   "the commented lines are classified by the file's language");

		// Outdated: the suggested lines alone, and nothing asked of the locator.
		asked.clear();
		auto outdated = thread("o", "src/engine/renderer.cpp", 13U, false);
		auto old      = reply("q", "o", "jiayin", "```suggestion\n\tWidth = 1;\n```\n");
		old.Path      = outdated.Path;
		view.adopt({ outdated, old });
		view.open_thread("o");
		QApplication::processEvents();
		auto* const stale = dynamic_cast<qt::markdown_body_view*>(view.open_card()->findChild<QWidget*>(QStringLiteral("threadPostReader")));
		auto        kinds = std::vector<int>{ };
		for (auto block = stale->rendered_view()->document()->begin(); block.isValid(); block = block.next()) {
			if (const auto row = block.blockFormat().intProperty(QTextFormat::UserProperty + 0x0B62); row != 0) {
				kinds.push_back(row);
			}
		}
		expect(asked.empty() && kinds == std::vector<int>{ 3, 2 }, "an outdated thread shows its header and the added row alone");

		// A pull request comment in Discuss: plain code, as on GitHub.
		view.set_feed(true);
		auto comment = conversation{ .Id = "c", .Kind = application::github_conversation_kind::issue_comment, .AuthorLogin = "tbutler", .Body = "```suggestion\nx\n```\n" };
		view.adopt({ comment });
		view.open_thread("c");
		QApplication::processEvents();
		auto* const plain = dynamic_cast<qt::markdown_body_view*>(view.open_card()->findChild<QWidget*>(QStringLiteral("threadPostReader")));
		expect(plain != nullptr && !plain->rendered_view()->document()->toPlainText().contains(QStringLiteral("Suggested change")), "a pull request comment's suggestion is plain code");
	}

	void test_discuss_new_counts_share_one_column()
	{
		qt::conversation_cards_view view;
		view.resize(420, 700);
		view.show();
		QApplication::processEvents();
		auto fixture = std::vector<conversation>{ };
		auto state   = std::vector<qt::thread_read_state>{ };
		for (const auto& [id, resolved] : std::array<std::pair<const char*, bool>, 4U>{
				 { { "review-thread:t-open-a", false }, { "review-thread:t-resolved-a", true }, { "review-thread:t-open-b", false }, { "review-thread:t-resolved-b", true } } }) {
			const auto line = static_cast<std::uint32_t>(10U + (10U * fixture.size()));
			fixture.push_back(thread(id, "src/a.cpp", line, true, resolved));
			fixture.push_back(reply(std::string{ id } + "-r", id, "author", resolved ? "Resolved, with a new reply." : "Open, with a new reply."));
			fixture.push_back(reply(std::string{ id } + "-c", id, "someone", "a remark"));
			state.push_back(qt::thread_read_state{ .Thread = id, .UnreadPosts = { std::string{ id } + "-c" } });
		}
		view.set_viewer_login(QStringLiteral("reviewer-one"));
		view.adopt(fixture);
		view.set_read_state(state);
		// [[BPR-1473]]. An unsent reply on one open and one resolved note must
		// not move their blue counts either.
		view.set_saved_drafts({ }, { core::reply_draft_decision{ .Draft = core::review_draft_id{ 41U }, .Thread = "t-open-b", .Standing = core::reply_draft_standing::none, .Sendable = true }, core::reply_draft_decision{ .Draft = core::review_draft_id{ 42U }, .Thread = "t-resolved-a", .Standing = core::reply_draft_standing::thread_resolved, .Sendable = true } });
		view.set_feed(true);
		view.set_scope(qt::cards_scope::whole_pull_request);
		QApplication::processEvents();
#if defined(_MSC_VER)
	#pragma warning(push)
	#pragma warning(disable : 4996)
#endif
		const auto* const path = std::getenv("BPR_DISCUSS_ALIGNMENT_PNG");
#if defined(_MSC_VER)
	#pragma warning(pop)
#endif
		if (path != nullptr) {
			expect(view.grab().save(QString::fromLocal8Bit(path)), "the Discuss alignment render is written");
		}
		expect(view.cards().size() == 4U && std::ranges::count_if(view.cards(), &qt::thread_card::Resolved) == 2,
			   "two notes are resolved and two are not, each with a new reply");
		expect(std::ranges::count_if(view.cards(), [](const qt::thread_card& aCard) { return aCard.UnsentReply.has_value(); }) == 2,
			   "one open and one resolved note carry an unsent reply");
		const auto first = view.closed_meta(view.cards().front());
		expect(!first.NewCount.isEmpty(), "a note with a new reply draws its blue count");
		for (const auto& card : view.cards()) {
			const auto meta = view.closed_meta(card);
			expect(meta.Resolved.isEmpty() != card.Resolved, "only a resolved note draws the green check");
			expect(meta.NewCount == first.NewCount.translated(0.0, meta.NewCount.top() - first.NewCount.top()),
				   "the blue count stands in one column whether or not the note is resolved");
			expect(meta.UnsentReply.isEmpty() != card.UnsentReply.has_value(), "only a note with an unsent reply draws \"Your reply\"");
			expect(meta.UnsentReply.isEmpty() || meta.UnsentReply.right() <= meta.NewCount.left(), "\"Your reply\" stands left of the blue count");
			expect(card.UnsentReply.has_value() || meta.Replies == first.Replies, "the reply count and faces stand in one column without an unsent reply");
			expect(!card.Resolved || (meta.Resolved.right() <= view.note_rect(card.Id).right() && meta.Resolved.left() > meta.NewCount.right()),
				   "the green check keeps its slot right of the blue count");
		}
	}

	/// a key that viewed a replaced string crashed the third adopt.
	void test_replacing_a_forest_never_reads_the_last_one()
	{
		const auto forest = [](const std::string& aPrefix, const std::size_t aThreads) {
			auto nodes = std::vector<conversation>{ };
			for (std::size_t index = 0U; index < aThreads; ++index) {
				const auto id = aPrefix + "-thread-identity-long-enough-to-allocate-" + std::to_string(index);
				nodes.push_back(thread(id, "src/a.cpp", static_cast<std::uint32_t>(index + 1U)));
				nodes.push_back(reply(id + "-reply", id, "author", "post"));
			}
			return nodes;
		};
		qt::conversation_cards_view view;
		view.resize(420, 900);
		view.adopt(forest("large", 3000U));
		expect(view.thread_count() == 3000U, "the large forest is retained");
		view.adopt(forest("first", 250U));
		expect(view.thread_count() == 250U, "a smaller forest replaces it");
		view.adopt(forest("second", 240U));
		expect(view.thread_count() == 240U, "another smaller forest replaces that one");
		view.clear();
		view.adopt(forest("third", 2U));
		expect(view.thread_count() == 2U, "a forest adopted after clearing is retained");
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	if (argc > 1 && std::string_view{ argv[1] } == "--discuss-alignment") {
		test_discuss_new_counts_share_one_column();
		std::cout << "discuss-alignment:pass\n";
		return EXIT_SUCCESS;
	}
	if (argc > 1 && std::string_view{ argv[1] } == "--publication-status-geometry") {
		test_wrapped_publication_status_is_fully_visible();
		std::cout << "publication-status-geometry:pass\n";
		return EXIT_SUCCESS;
	}
	test_wrapped_publication_status_is_fully_visible();
	test_replacing_a_forest_never_reads_the_last_one();
	test_discuss_wheel_moves_readable_lines_without_changing_selection();
	test_thread_menus_and_defect_badges();
	test_one_thread_read_state_matches_the_complete_state();
	test_branching_conversation_reads_in_place();
	test_read_state_marks_unread_posts_and_offers_mark_read();
	test_new_boundary_is_never_the_first_thing_in_a_thread();
	test_discuss_lists_threads_that_open_in_place();
	test_discuss_post_menus_open_the_exact_comment_in_review();
	test_notes_hang_from_their_lines_and_one_opens_pinned();
	test_start_a_review_in_the_composer();
	test_chips_wheel_and_overflow();
	test_reply_in_place_and_next_new();
	test_reply_in_place_has_the_composers_label_controls();
	test_a_reply_suggestion_is_marked_native_or_text_only();
	test_published_suggestions_show_against_their_lines();
	test_endpoint_changes_retain_exact_body_annotations();
	test_unchanged_refresh_retains_measured_notes();
	test_alternating_widths_keep_both_widths_measured();
	test_discuss_geometry_refresh_updates_excerpts();
	test_the_excerpt_follows_the_supplied_code_font();
	test_cached_geometry_matches_a_fresh_view();
	test_a_dispatch_that_relays_out_still_names_the_pressed_thread();
	test_discuss_new_counts_share_one_column();
	std::cout << "conversation-cards-view-tests:pass\n";
	return EXIT_SUCCESS;
}
