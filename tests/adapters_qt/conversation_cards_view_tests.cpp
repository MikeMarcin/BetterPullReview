#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/ask_menu.hpp"
#include "bpr/qt/code_sample_painter.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/feedback_label_controls.hpp"
#include "bpr/qt/github_cli_consent.hpp"
#include "bpr/qt/keyboard_focus_ring.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/quiet_tool_button.hpp"
#include "bpr/qt/scoped_prose_editor.hpp"
#include "bpr/qt/suggestion_mark.hpp"

#include "owner_186_fixture.hpp"

#include "bpr/core/appearance_contrast.hpp"
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
#include <QHelpEvent>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPointer>
#include <QScrollBar>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextLayout>
#include <QThread>
#include <QToolButton>
#include <QWheelEvent>

#include "bpr/qt/nested_scroll_latch.hpp"
#include <glaze/json.hpp>
#include <qpa/qwindowsysteminterface.h>

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
	// [[BPR-1699]]. A thread someone else started offers Mark all from
	// its author as read, and a post's body menu Mark all from that post's
	// author; each counts that author's new posts over the pull request,
	// names the login as written, an app's [bot] included, and goes out with
	// it. Read state decides when they are offered, and a new state moves
	// the count at once.
	auto test_mark_all_from_an_author_is_offered_from_thread_and_post_menus() -> void
	{
		qt::conversation_cards_view view;
		view.resize(360, 900);
		view.show();
		view.set_viewer_login(QStringLiteral("reviewer-one"));
		view.adopt({
			thread("bot-1", "src/a.cpp", 1U),
			reply("bot-1-a", "bot-1", "coderabbitai[bot]", "Nitpick: rename this."),
			thread("bot-2", "src/a.cpp", 2U),
			reply("bot-2-a", "bot-2", "coderabbitai[bot]", "Consider a guard clause."),
			reply("bot-2-b", "bot-2", "tbutler", "I disagree with the bot here."),
			thread("human", "src/a.cpp", 3U),
			reply("human-a", "human", "tbutler", "Why this order?"),
			thread("mine", "src/a.cpp", 4U),
			reply("mine-a", "mine", "reviewer-one", "My own note."),
		});
		QApplication::processEvents();
		auto logins = std::vector<std::string>{ };
		view.set_author_read_dispatch([&logins](const std::string& aLogin) { logins.push_back(aLogin); });
		const auto authorAction = [&view](const std::string& aThread) -> std::pair<QMenu*, QAction*> {
			auto* const menu = view.thread_menu(aThread);
			return { menu, menu == nullptr ? nullptr : menu->findChild<QAction*>(QStringLiteral("markAuthorReadAction")) };
		};
		{
			auto [menu, action] = authorAction("bot-1");
			expect(action != nullptr && !action->isEnabled() && action->toolTip() == QStringLiteral("Read state is not available yet."),
				   "before any read state the author's Mark all is offered disabled, saying why");
			delete menu;
		}
		view.set_read_state({
			qt::thread_read_state{ .Thread = "bot-1", .UnreadPosts = { "bot-1-a" } },
			qt::thread_read_state{ .Thread = "bot-2", .UnreadPosts = { "bot-2-a", "bot-2-b" } },
			qt::thread_read_state{ .Thread = "human", .UnreadPosts = { "human-a" } },
		});
		QApplication::processEvents();
		expect(view.unread_post_count_from(QStringLiteral("coderabbitai[bot]")) == 2U && view.unread_post_count_from(QStringLiteral("CodeRabbitAI[bot]")) == 2U
				   && view.unread_post_count_from(QStringLiteral("coderabbitai")) == 0U && view.unread_post_count_from(QStringLiteral("tbutler")) == 2U,
			   "an author's new posts are counted over the pull request, the login without case, an app apart from a same-named person");
		{
			auto [menu, action] = authorAction("bot-1");
			expect(action != nullptr && action->isEnabled() && action->text() == QStringLiteral("Mark all from coderabbitai[bot] as read")
					   && action->toolTip() == QStringLiteral("Mark the 2 new comments coderabbitai[bot] wrote on this pull request as read. Only your own read state changes."),
				   "a bot's thread offers Mark all from the bot, counting its new posts everywhere");
			menu->hide();
			action->trigger();
			expect(logins == std::vector<std::string>{ "coderabbitai[bot]" }, "the gesture goes out with the bot's login");
			delete menu;
		}
		{
			auto [menu, action] = authorAction("mine");
			expect(menu != nullptr && action == nullptr, "a thread I started offers no Mark all from me");
			delete menu;
		}
		// A post's own body menu names that post's author, here the reply.
		view.set_feed(true);
		view.open_thread("bot-2");
		QApplication::processEvents();
		auto* const reader = [&view]() -> qt::markdown_body_view* {
			for (auto* const widget : view.open_card()->findChildren<QWidget*>()) {
				if (auto* const body = dynamic_cast<qt::markdown_body_view*>(widget); body != nullptr && body->original_body() == "I disagree with the bot here.") {
					return body;
				}
			}
			return nullptr;
		}();
		expect(reader != nullptr, "the reply has its reader");
		auto context = QContextMenuEvent{ QContextMenuEvent::Keyboard, QPoint{ 5, 5 }, reader->rendered_view()->viewport()->mapToGlobal(QPoint{ 5, 5 }) };
		QApplication::sendEvent(reader->rendered_view()->viewport(), &context);
		QApplication::processEvents();
		auto* const bodyMenu = reader->findChild<QMenu*>(QStringLiteral("markdownReaderContextMenu"));
		auto* const byPost   = bodyMenu == nullptr ? nullptr : bodyMenu->findChild<QAction*>(QStringLiteral("postMarkAuthorRead"));
		expect(byPost != nullptr && byPost->isEnabled() && byPost->text() == QStringLiteral("Mark all from tbutler as read"),
			   "the keyboard's body menu on a reply offers Mark all from the reply's author");
		bodyMenu->hide();
		byPost->trigger();
		expect(logins.back() == "tbutler", "and goes out with that login");
		delete bodyMenu;
		// The composition publishes the state the ledger committed: the
		// bot's posts read, the person's still new. The menus follow at once.
		view.set_read_state({
			qt::thread_read_state{ .Thread = "bot-2", .UnreadPosts = { "bot-2-b" } },
			qt::thread_read_state{ .Thread = "human", .UnreadPosts = { "human-a" } },
		});
		QApplication::processEvents();
		expect(view.unread_post_count() == 2U && view.unread_post_count_from(QStringLiteral("coderabbitai[bot]")) == 0U, "the counts move with the state");
		{
			auto [menu, action] = authorAction("bot-1");
			expect(action != nullptr && !action->isEnabled() && action->toolTip() == QStringLiteral("Nothing coderabbitai[bot] wrote on this pull request is new."),
				   "with nothing of the bot's new its Mark all is disabled and says so");
			delete menu;
		}
		view.hide();
	}

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
		// Read marks can move the New divider above the visible post, without
		// replacing its reader or moving the text being read.
		const auto unreadReply = rootCard->Posts[1].Id;
		view.set_feed(true);
		QApplication::processEvents();
		const auto heldCard = QPointer<QWidget>{ view.open_card() };
		const auto posts    = heldCard->findChildren<QWidget*>(QStringLiteral("threadPost"));
		expect(posts.size() > 3, "the read-state course has a post below the New divider");
		const auto heldBody = QPointer<QTextEdit>{ posts[3]->findChild<QTextEdit*>(QStringLiteral("threadPostBody")) };
		expect(static_cast<bool>(heldBody), "the visible post has its real Markdown body");
		view.verticalScrollBar()->setValue(view.verticalScrollBar()->value() + heldBody->mapTo(view.viewport(), QPoint{ }).y() - 80);
		QApplication::processEvents();
		const auto before = heldBody->mapTo(view.viewport(), QPoint{ });
		static_cast<void>(view.set_thread_read_state(root, { unreadReply }));
		QApplication::processEvents();
		QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		expect(heldCard && heldCard == view.open_card() && heldBody && heldBody->mapTo(view.viewport(), QPoint{ }) == before
				   && heldCard->findChild<QWidget*>(QStringLiteral("threadNewRule")) != nullptr,
			   "moving the New divider preserves the current reader and its viewport position");
		view.set_read_state({ });
		QApplication::processEvents();
		QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		expect(heldCard && heldCard == view.open_card() && heldBody && heldBody->mapTo(view.viewport(), QPoint{ }) == before,
			   "removing the New divider preserves the same reader and viewport position");
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
		// [[BPR-2171]]. A file's read, resolved threads fold into its row,
		// and still count; opened, the row lists them.
		const auto folded = std::ranges::find_if(view.timeline_lines(), [](const qt::timeline_line& aLine) { return !aLine.Fold.empty() && !aLine.Open; });
		expect(view.cards().size() == 2U && view.thread_count() == 3U && folded != view.timeline_lines().end() && folded->Path == QStringLiteral("src/a.cpp") && folded->Folded == 1U,
			   "Discuss folds the read, resolved thread into its file's row, still counted");
		view.toggle_fold(folded->Fold);
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
		expect(actions == std::vector<std::pair<qt::card_action, std::string>>{ { qt::card_action::mark_read, "t-open" } },
			   "opening a New-badged Discuss card asks the host to mark that exact thread read");
		expect(view.open_thread_id() == "t-open" && view.open_card()->findChild<QLabel*>(QStringLiteral("threadExcerpt")) != nullptr,
			   "a press opens the thread in place with its code excerpt above its posts");
		expect(button(view, "threadResolve") == nullptr, "Resolve is not offered before the provider and server both resolve threads");
		expect(button(view, "threadShowLine")->toolTip() == QStringLiteral("Open in Review at src/z.cpp:9")
				   && button(view, "threadShowLine")->accessibleName() == QStringLiteral("Open in Review at src/z.cpp:9"),
			   "Discuss's line button names Review and the exact file and line");
		button(view, "threadShowLine")->click();
		expect(actions == std::vector<std::pair<qt::card_action, std::string>>{ { qt::card_action::mark_read, "t-open" }, { qt::card_action::open_in_code, "t-open" } },
			   "in Discuss the line button opens the thread in the code");
		// RFC-BPR-0128 ([[BPR-2164]]). Ask agent left the header: the open
		// thread's Ask stands where a reply is written, and the thread's menu
		// has one Ask item, both filled by the window with the thread's quick
		// asks.
		expect(button(view, "threadAskAgent") == nullptr && button(view, "threadWasAddressed") == nullptr, "the header has no agent controls");
		expect(button(view, "threadAsk") == nullptr, "no Ask until the window fills its menu");
		{
			auto filled = std::vector<std::string>{ };
			view.set_ask_menu_filler([&filled](QMenu& aMenu, const QString& aThread, const QString& aTitle) {
				filled.push_back(aThread.toStdString());
				aMenu.addAction(aTitle);
			});
			auto* const ask = static_cast<qt::ask_menu_button*>(button(view, "threadAsk"));
			expect(ask != nullptr && ask->text() == QStringLiteral("Ask") && !ask->accessibleName().isEmpty(), "the open thread has Ask at its reply row");
			ask->refresh_menu();
			expect(filled == std::vector<std::string>{ "t-open" } && !ask->menu()->actions().isEmpty() && ask->menu()->actions().front()->text() == QStringLiteral("Thread on line 9"),
				   "its menu is the window's, for this thread");
			auto* const menu = view.thread_menu("t-open");
			auto* const item = menu != nullptr ? menu->findChild<QMenu*>(QStringLiteral("threadAskMenu")) : nullptr;
			expect(item != nullptr && filled.size() == 2U && filled.back() == "t-open", "the thread's menu has one Ask item, filled for the thread");
			expect(menu->findChild<QAction*>(QStringLiteral("threadAskAgentAction")) == nullptr && menu->findChild<QAction*>(QStringLiteral("threadWasAddressedAction")) == nullptr,
				   "the agent items it replaces are gone");
			delete menu;
			view.set_ask_menu_filler({ });
		}
		view.set_resolution_offered(true);
		QApplication::processEvents();
		expect(button(view, "threadResolve") != nullptr && button(view, "threadResolve")->toolTip() == QStringLiteral("Resolve conversation"),
			   "offered, an open thread gains Resolve");
		view.open_thread("t-resolved");
		QApplication::processEvents();
		expect(button(view, "threadResolve") != nullptr && button(view, "threadResolve")->toolTip() == QStringLiteral("Unresolve conversation")

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

	// [[BPR-1824]]. A thread's Resolve says why it cannot go, never only
	// "not available": the pull request's reason, then GitHub's word on this
	// viewer, and a pull request comment has nothing to resolve. A thread
	// another account wrote resolves when GitHub permits it.
	void test_a_thread_menu_names_why_it_cannot_resolve()
	{
		qt::conversation_cards_view view;
		view.resize(700, 700);
		view.set_feed(true);
		view.set_action_dispatch([](const qt::card_action, const std::string&) { });
		auto general = conversation{ .Id = "general", .Kind = application::github_conversation_kind::issue_comment, .Body = "A PR-wide comment" };
		view.adopt({ thread("other", "src/a.cpp", 9U), reply("other-first", "other", "someone-else", "Their comment"),
					 thread("done", "src/a.cpp", 20U, true, true), reply("done-first", "done", "someone-else", "Resolved one"), general });
		view.show();
		QApplication::processEvents();
		const auto resolveAction = [&view](const std::string& aId) {
			auto* const menu   = view.thread_menu(aId);
			auto* const action = menu != nullptr ? menu->findChild<QAction*>(QStringLiteral("threadResolveAction")) : nullptr;
			auto        state  = std::pair{ action != nullptr && action->isEnabled(), action != nullptr ? action->toolTip() : QStringLiteral("<none>") };
			auto        text   = action != nullptr ? action->text() : QString{ };
			delete menu;
			return std::tuple{ state.first, state.second, text };
		};
		using refusal = core::thread_resolution_refusal;
		expect(view.resolution_refusal() == refusal::no_write_connection && !view.resolution_offered(),
			   "before a write connection is composed the cards say there is none");
		for (const auto reason : { refusal::anonymous_read, refusal::no_write_connection, refusal::capability_absent }) {
			view.set_resolution_refusal(reason);
			const auto [enabled, tip, text] = resolveAction("other");
			expect(!enabled && tip == qt::thread_resolution_refusal_text(reason, true) && !tip.isEmpty() && !tip.contains(QStringLiteral("not available")),
				   "a pull request's reason is the disabled Resolve's own words");
		}
		// [[BPR-1826]]. No refusal names a closed pull request: its threads
		// resolve, and no reason sends the reviewer to GitHub's page for it.
		for (const auto reason : { refusal::anonymous_read, refusal::capability_absent, refusal::not_permitted, refusal::already_in_state,
								   refusal::no_write_connection, refusal::not_a_review_thread }) {
			for (const auto resolve : { true, false }) {
				expect(!qt::thread_resolution_refusal_text(reason, resolve).contains(QStringLiteral("closed")),
					   "no resolution refusal says the pull request is closed");
			}
		}
		view.set_resolution_refusal(refusal::none);
		QApplication::processEvents();
		{
			const auto [enabled, tip, text] = resolveAction("other");
			expect(enabled && text == QStringLiteral("Resolve"), "a thread another account wrote resolves when nothing refuses");
			const auto [reopen, reopenTip, reopenText] = resolveAction("done");
			expect(reopen && reopenText == QStringLiteral("Unresolve"), "and a resolved one unresolves");
		}
		view.present_thread_resolutions({ { "other", qt::thread_resolution_presentation{ .CanResolve = false } },
										  { "done", qt::thread_resolution_presentation{ .Resolved = true, .CanUnresolve = false } } });
		QApplication::processEvents();
		{
			const auto [enabled, tip, text] = resolveAction("other");
			expect(!enabled && tip == qt::thread_resolution_refusal_text(refusal::not_permitted, true) && tip.contains(QStringLiteral("write access")),
				   "GitHub's viewerCanResolve false names the permission");
			const auto [reopen, reopenTip, reopenText] = resolveAction("done");
			expect(!reopen && reopenTip == qt::thread_resolution_refusal_text(refusal::not_permitted, false),
				   "and viewerCanUnresolve false the reopening one");
		}
		{
			const auto [enabled, tip, text] = resolveAction("general");
			expect(!enabled && tip == qt::thread_resolution_refusal_text(refusal::not_a_review_thread, true),
				   "a pull request comment says only review threads resolve");
		}
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

	/// [[BPR-1740]]. The owner, 2026-10-22: "Much like the code window
	/// I want to be able to scroll the comments drawer up until only the last
	/// line is visible." The drawer's End leaves its last note's last line at
	/// the top and Home goes back; Discuss's feed still ends at its last note.
	/// An open thread with no line to hang from takes the wheel before the
	/// code does, however much the code could take.
	auto test_the_drawer_scrolls_past_its_end() -> void
	{
		qt::conversation_cards_view view;
		view.resize(420, 600);
		view.show();
		view.adopt({ thread("t1", "src/a.cpp", 3U), reply("p1", "t1", "author", "a short post"),
					 thread("t2", "src/a.cpp", 9U), reply("p2", "t2", "author", "another short post") });
		QApplication::processEvents();
		auto* const bar    = view.verticalScrollBar();
		const auto  line   = view.fontMetrics().lineSpacing();
		const auto  lowest = std::ranges::max(view.cards(), { }, [](const qt::thread_card& aCard) { return aCard.Rect.bottom(); }).Rect.bottom();
		expect(lowest < view.viewport()->height() && bar->maximum() == lowest - 2 * line,
			   "notes shorter than the drawer still scroll until the last note's last line is at the top");
		const auto press = [&view](const int aKey) {
			auto key = QKeyEvent{ QEvent::KeyPress, aKey, Qt::NoModifier };
			QApplication::sendEvent(&view, &key);
			QApplication::processEvents();
		};
		press(Qt::Key_End);
		expect(bar->value() == bar->maximum() && lowest - bar->value() == 2 * line, "End goes to the drawer's end, its last line at the top");
		press(Qt::Key_Home);
		expect(bar->value() == 0, "Home goes back to the top");

		// An open thread that hangs from no line (none here: no comparison
		// places it) moves on the wheel even while the code takes every step.
		auto forwarded = 0;
		view.set_scroll_forward([&forwarded](QWheelEvent&) {
			++forwarded;
			return true;
		});
		view.open_thread("t1");
		QApplication::processEvents();
		const auto centre = QPointF{ view.viewport()->rect().center() };
		auto       wheel  = QWheelEvent{ centre, view.viewport()->mapToGlobal(centre), QPoint{ }, QPoint{ 0, -120 }, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false };
		QApplication::sendEvent(view.viewport(), &wheel);
		expect(bar->value() > 0 && forwarded == 0, "the notes take the wheel first for a thread that does not move with the code");
		for (auto step = 0; step < 100; ++step) {
			auto more = QWheelEvent{ centre, view.viewport()->mapToGlobal(centre), QPoint{ }, QPoint{ 0, -120 }, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false };
			QApplication::sendEvent(view.viewport(), &more);
		}
		expect(bar->value() == bar->maximum() && forwarded > 0, "and the code only what the notes cannot take");

		view.set_feed(true);
		view.adopt({ thread("t1", "src/a.cpp", 3U), reply("p1", "t1", "author", "a short post") });
		QApplication::processEvents();
		expect(bar->maximum() == 0, "Discuss's feed, shorter than its view, does not scroll");
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
		const auto  layouts = view.layouts();
		const auto* posts   = view.cards().front().Posts.data();
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
		expect(view.layouts() == layouts && view.cards().front().Posts.data() == posts,
			   "resizing only remeasures geometry and retains the projected cards");
	}

	/// \brief RFC-BPR-0127 ([[BPR-2158]]) AC02, AC14. On a line with a
	/// published thread, my Local reply in it, my draft reply in it and my
	/// Local comment on the line: the thread's card holds the published posts
	/// then the Local reply, the Local comment is its own card after it, the
	/// line has one gutter mark naming everything on it, and each state is
	/// said in words. A Local card offers Make draft and Delete; the composer's
	/// state control turns its foot to Save, and ⌘↩ saves it as Local.
	auto test_local_comments_share_a_line() -> void
	{
		qt::conversation_cards_view view;
		view.resize(380, 700);
		view.show();
		QApplication::processEvents();
		view.set_line_locator([](core::diff_side, const std::uint32_t aLine) -> std::optional<int> { return aLine == 22U ? std::optional{ 200 } : aLine == 40U ? std::optional{ 480 }
																																							   : std::nullopt; });
		view.set_anchor_file(core::file_identity_of("src/a.cpp"));
		auto root = thread("review-thread:T1", "src/a.cpp", 22U);
		auto post = reply("c-1", "review-thread:T1", "tbutler", "issue: begin_frame() can hand back the frame still on screen.");
		post.Line = 22U;
		view.adopt({ root, post });
		const auto comparison = core::file_comparison_digest{ };
		auto       answer     = core::review_comment_draft{ .Id = core::review_draft_id{ 5U }, .Anchor = core::source_line_ref{ .Comparison = comparison, .Side = core::diff_side::right, .Line = 21U }, .Body = "Thanks. Does begin_frame() skip it after forget()?" };
		answer.ReplyToThread  = "T1";
		view.set_draft(std::nullopt, comparison);
		view.set_saved_drafts({ answer }, { core::reply_draft_decision{ .Draft = answer.Id, .Thread = "T1", .Standing = core::reply_draft_standing::answerable, .Sendable = true } });
		view.set_local_comments({
			qt::local_comment_card{ .Id = 7U, .Revision = 1U, .Body = QStringLiteral("Check retained() after a resize."), .ReplyToThread = std::string{ "T1" } },
			qt::local_comment_card{ .Id = 8U, .Revision = 1U, .Body = QStringLiteral("Height is not clamped here."), .Comparison = comparison, .Side = core::diff_side::right, .Line = 22U, .MakeDraft = QStringLiteral("Make draft") },
			qt::local_comment_card{ .Id = 9U, .Revision = 1U, .Body = QStringLiteral("Written on another head."), .Notice = QStringLiteral("Outdated: the code it was written on has changed."), .MakeDraftWhy = QStringLiteral("Select a line in the code to make it a draft there.") },
		});
		QApplication::processEvents();
		const auto& cards  = view.cards();
		const auto  thread = std::ranges::find(cards, std::string{ "review-thread:T1" }, &qt::thread_card::Id);
		const auto  local  = std::ranges::find(cards, std::string{ "local:8" }, &qt::thread_card::Id);
		const auto  stale  = std::ranges::find(cards, std::string{ "local:9" }, &qt::thread_card::Id);
		expect(thread != cards.end() && thread->Posts.size() == 2U && !thread->Posts.front().Local && thread->Posts.back().Local && thread->Posts.back().Name.contains(QStringLiteral("local reply"))
				   && thread->LocalReplies == 1U && thread->UnsentReply == core::reply_draft_standing::answerable,
			   "AC02: the thread's card holds its published post, then my Local reply; my draft reply is its unsent reply");
		expect(local != cards.end() && local->Local && local->Draft && local->Line == std::optional{ 22U } && local > thread, "my Local comment on the line is its own card, after the published thread");
		expect(stale != cards.end() && stale->Outdated && !stale->Line && stale->Notice.startsWith(QStringLiteral("Outdated")), "AC06: one this comparison does not place is drawn after the placed cards, with its reason, at no line");
		const auto marks = view.line_marks(core::diff_side::right);
		std::cout << "LOCAL_LINE_MARKS " << marks.size() << (marks.empty() ? std::string{ } : " " + marks.front().Description.toStdString()) << '\n';
		expect(marks.size() == 1U && marks.front().Line == 21U && !marks.front().Draft && marks.front().Description == QStringLiteral("Line 22: 1 thread, 2 local comments, 1 draft reply"),
			   "AC02: one gutter mark on the line, the thread's, naming everything on it");
		expect(qt::comment_state_words(true, false, false) == QStringLiteral("Local comment, only on this computer") && qt::comment_state_words(false, true, false) == QStringLiteral("Draft, not in your review")
				   && qt::comment_state_words(false, true, true) == QStringLiteral("Draft, in your review"),
			   "AC14: each state in words");

		auto actions = std::vector<std::pair<qt::card_action, std::string>>{ };
		view.set_action_dispatch([&actions](const qt::card_action aAction, const std::string& aId) { actions.emplace_back(aAction, aId); });
		view.open_thread("local:8");
		QApplication::processEvents();
		auto* const state = view.open_card() != nullptr ? view.open_card()->findChild<QWidget*>(QStringLiteral("threadLocalState")) : nullptr;
		auto* const make  = button(view, "threadMakeDraft");
		expect(state != nullptr && state->accessibleName() == QStringLiteral("Local comment, only on this computer") && state->toolTip() == QStringLiteral("Only on this computer"),
			   "AC14: the open Local card says its state, with the computer's tooltip");
		expect(make != nullptr && make->isEnabled() && make->text() == QStringLiteral("Make draft") && make->focusPolicy() != Qt::NoFocus, "Make draft is offered and reachable by keyboard");
		make->click();
		auto* const remove = view.open_card()->findChild<QToolButton*>(QStringLiteral("threadDeleteLocal"));
		expect(remove != nullptr, "Delete is offered");
		remove->click();
		expect(actions.size() == 2U && actions[0] == std::pair{ qt::card_action::make_draft, std::string{ "local:8" } } && actions[1] == std::pair{ qt::card_action::delete_local, std::string{ "local:8" } },
			   "Make draft and Delete go to the window by the card's identity; nothing is published");
		view.open_thread("local:9");
		QApplication::processEvents();
		expect(button(view, "threadMakeDraft") != nullptr && !button(view, "threadMakeDraft")->isEnabled() && button(view, "threadMakeDraft")->property("draftRefused").toBool()
				   && button(view, "threadMakeDraft")->toolTip() == QStringLiteral("Select a line in the code to make it a draft there."),
			   "AC06: an outdated one offers Make draft only with a line selected, and says so");
	}

	/// \brief RFC-BPR-0127 ([[BPR-2158]]) AC06, AC14. Where a Local comment
	/// can be: one on the pull request is counted on the pull request link and
	/// drawn on Overview's Comments tab; one on the whole file is the file
	/// chip's and drawn after the placed cards; one on another file is the
	/// Other files chip's, whose menu goes to it. None is drawn at a line.
	/// A Local comment is edited in place and saved only on this computer;
	/// my Local reply in a thread offers Make draft, Copy text and Delete; the
	/// reply field's state control is Local and Reply, and Local's ⌘↩ saves.
	auto test_local_comments_edges_and_editing() -> void
	{
		qt::conversation_cards_view view;
		view.resize(420, 700);
		view.show();
		QApplication::processEvents();
		view.set_line_locator([](core::diff_side, const std::uint32_t aLine) -> std::optional<int> { return aLine == 22U ? std::optional{ 200 } : aLine == 40U ? std::optional{ 480 }
																																							   : std::nullopt; });
		view.set_anchor_file(core::file_identity_of("src/a.cpp"));
		auto root = thread("review-thread:T1", "src/a.cpp", 22U);
		view.adopt({ root });
		const auto comparison = core::file_comparison_digest{ };
		view.set_draft(std::nullopt, comparison);
		auto links = std::vector<std::string>{ };
		view.set_pull_request_link_dispatch([&links](const std::string& aId) { links.push_back(aId); });
		view.set_local_comments({
			qt::local_comment_card{ .Id = 1U, .Revision = 1U, .Body = QStringLiteral("Ask about the release note."), .MakeDraftWhy = QStringLiteral("Select a line in the code to make it a draft there."), .Place = qt::local_comment_place::pull_request },
			qt::local_comment_card{ .Id = 2U, .Revision = 1U, .Body = QStringLiteral("This whole file wants a test."), .MakeDraftWhy = QStringLiteral("Select a line in the code to make it a draft there."), .Place = qt::local_comment_place::whole_file },
			qt::local_comment_card{ .Id = 3U, .Revision = 1U, .Body = QStringLiteral("Check the caller here."), .Place = qt::local_comment_place::other_file, .Path = QStringLiteral("src/b.cpp"), .PathLine = 12U },
			qt::local_comment_card{ .Id = 4U, .Revision = 1U, .Body = QStringLiteral("And this one."), .Place = qt::local_comment_place::other_file, .Path = QStringLiteral("src/c.cpp") },
			qt::local_comment_card{ .Id = 7U, .Revision = 1U, .Body = QStringLiteral("Check retained() after a resize."), .ReplyToThread = std::string{ "T1" }, .MakeDraft = QStringLiteral("Make draft") },
			qt::local_comment_card{ .Id = 8U, .Revision = 3U, .Body = QStringLiteral("Height is not clamped here."), .Comparison = comparison, .Side = core::diff_side::right, .Line = 22U, .MakeDraft = QStringLiteral("Make draft") },
		});
		QApplication::processEvents();
		const auto& cards = view.cards();
		const auto  has   = [&cards](const char* aId) { return std::ranges::find(cards, std::string{ aId }, &qt::thread_card::Id) != cards.end(); };
		const auto  whole = std::ranges::find(cards, std::string{ "local:2" }, &qt::thread_card::Id);
		expect(!has("local:1") && !has("local:3") && !has("local:4") && whole != cards.end() && !whole->Line && whole->Notice == QStringLiteral("On this whole file, not on a line."),
			   "AC06: beside the code only this file's are drawn, the whole file's after the placed cards at no line");
		auto* const file      = view.local_file_chip();
		auto* const elsewhere = view.local_elsewhere_chip();
		auto* const pull      = view.pull_request_comments_chip();
		std::cout << "LOCAL_EDGE_CHIPS " << file->accessibleName().toStdString() << " | " << elsewhere->accessibleName().toStdString() << " | " << pull->accessibleName().toStdString() << '\n';
		expect(file->isVisible() && file->accessibleName() == QStringLiteral("This file: 1 local comment, only on this computer"), "AC06, AC14: the file's chip counts its Local comment, in words");
		expect(elsewhere->isVisible() && elsewhere->accessibleName() == QStringLiteral("Other files: 2 local comments, only on this computer") && elsewhere->focusPolicy() != Qt::NoFocus,
			   "AC06, AC14: the Other files chip counts theirs, and is reachable by keyboard");
		expect(pull->accessibleName().endsWith(QStringLiteral(", 1 local comment")), "AC06: the pull request link counts my Local comment on the pull request");
		file->click();
		QApplication::processEvents();
		expect(view.open_thread_id() == "local:2", "the file's chip opens its comment");
		auto actions = std::vector<std::pair<qt::card_action, std::string>>{ };
		view.set_action_dispatch([&actions](const qt::card_action aAction, const std::string& aId) { actions.emplace_back(aAction, aId); });
		elsewhere->click();
		QApplication::processEvents();
		auto* const menu = view.local_elsewhere_menu();
		expect(menu != nullptr && menu->actions().size() == 2U && menu->actions().front()->text().startsWith(QStringLiteral("src/b.cpp:12")), "the Other files chip lists them, each with its file and line");
		menu->actions().front()->trigger();
		menu->close();
		QApplication::processEvents();
		expect(!actions.empty() && actions.back() == std::pair{ qt::card_action::open_local_elsewhere, std::string{ "local:3" } }, "choosing one goes to its file, then its line");

		// Edited in place: saved only on this computer, at its revision.
		auto edits = std::vector<std::tuple<std::uint64_t, std::uint64_t, std::string>>{ };
		view.set_local_edit_dispatch([&edits](const std::uint64_t aId, const std::uint64_t aRevision, const std::string& aBody) { edits.emplace_back(aId, aRevision, aBody); });
		expect(view.comment_editable("local:8") && view.begin_comment_edit("local:8"), "my Local comment is editable in place");
		QApplication::processEvents();
		auto* const editor = view.comment_editor();
		expect(editor != nullptr && view.open_card()->findChild<QLabel*>(QStringLiteral("threadCommentEditHint")) != nullptr
				   && view.open_card()->findChild<QLabel*>(QStringLiteral("threadCommentEditHint"))->text() == QStringLiteral("Only on this computer. Never sent."),
			   "its editor says it is only on this computer");
		editor->setPlainText(QStringLiteral("Height is not clamped; width is."));
		QApplication::processEvents();
		view.comment_edit_save_button()->click();
		QApplication::processEvents();
		expect(edits.size() == 1U && edits.front() == std::tuple{ std::uint64_t{ 8U }, std::uint64_t{ 3U }, std::string{ "Height is not clamped; width is." } },
			   "Save gives its identity, its revision and the words; nothing is sent");
		auto saved                                                                       = view.local_comments();
		std::ranges::find(saved, std::uint64_t{ 8U }, &qt::local_comment_card::Id)->Body = QStringLiteral("Height is not clamped; width is.");
		view.set_local_comments(std::move(saved));
		QApplication::processEvents();
		expect(!view.comment_editing().has_value(), "the stored words end the edit");

		// My Local reply in the thread: its own actions, by its identity.
		view.open_thread("review-thread:T1");
		QApplication::processEvents();
		auto* const make = button(view, "threadLocalReplyMakeDraft");
		auto* const drop = button(view, "threadLocalReplyDelete");
		expect(make != nullptr && make->isEnabled() && drop != nullptr && button(view, "threadLocalReplyCopy") != nullptr, "my Local reply offers Make draft, Copy text and Delete");
		make->click();
		drop->click();
		expect(actions.size() >= 2U && actions[actions.size() - 2U] == std::pair{ qt::card_action::make_draft, std::string{ "local:7" } }
				   && actions.back() == std::pair{ qt::card_action::delete_local, std::string{ "local:7" } },
			   "they go to the window by the reply's identity");

		// The reply field's state control: Local and Reply.
		view.begin_reply("review-thread:T1");
		QApplication::processEvents();
		auto* const localState = view.open_card()->findChild<QToolButton*>(QStringLiteral("threadReplyStateLocal"));
		auto* const replyState = view.open_card()->findChild<QToolButton*>(QStringLiteral("threadReplyStateReply"));
		auto* const saveLocal  = button(view, "threadReplySaveLocal");
		auto* const send       = button(view, "threadReplyPublish");
		expect(localState != nullptr && replyState != nullptr && replyState->isChecked() && saveLocal != nullptr && !saveLocal->isVisibleTo(view.open_card()) && send->isVisibleTo(view.open_card()),
			   "Reply opens on Reply, with Send");
		localState->click();
		QApplication::processEvents();
		expect(localState->isChecked() && saveLocal->isVisibleTo(view.open_card()) && !send->isVisibleTo(view.open_card()) && localState->focusPolicy() != Qt::NoFocus,
			   "Local: Save in Send's place, reachable by keyboard");
		view.reply_editor()->setPlainText(QStringLiteral("Only for me."));
		QApplication::processEvents();
		auto submit = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::ControlModifier };
		QApplication::sendEvent(view.reply_editor(), &submit);
		QApplication::processEvents();
		expect(actions.back() == std::pair{ qt::card_action::save_local, std::string{ "review-thread:T1" } } && view.reply_editor() == nullptr, "⌘↩ saves it as a Local reply and folds the field");

		// On Overview's Comments tab: my Local comment on the pull request.
		view.show_pull_request_comments(true);
		QApplication::processEvents();
		const auto pr = std::ranges::find(view.cards(), std::string{ "local:1" }, &qt::thread_card::Id);
		expect(pr != view.cards().end() && pr->Notice == QStringLiteral("On the pull request. Only on this computer.") && !has("local:8") && !file->isVisible() && !elsewhere->isVisible(),
			   "AC06: Overview's Comments tab draws my Local comment on the pull request, and nothing at a line");
		view.show_pull_request_comments(false);
	}

	/// \brief RFC-BPR-0127 ([[BPR-2158]]) AC01, AC14. The composer's state
	/// control: Draft chosen for a comment; Local turns its foot to Save and
	/// the sentence, ⌘↩ saves it as Local, and Draft brings today's row back.
	/// N's preset (`draft_composer_presentation::Local`) opens it on Local.
	/// \brief Owner look v002 ([[BPR-2158]]), rejected. A new comment's
	/// composer on a line above the drawer's top is whole in view, its notch
	/// still at its line, while an open published thread keeps hanging at
	/// its line. My Local reply's tooltip sits right under its tag; no
	/// control in the open card anchors a tooltip to a large area, which is
	/// how a tooltip ends up far from what it names. Make draft, when it is
	/// not offered, says why in words under it.
	auto test_look_v002_rework() -> void
	{
		qt::conversation_cards_view view;
		view.resize(380, 700);
		view.show();
		QApplication::processEvents();
		auto lineY = std::map<std::uint32_t, int>{ { 5U, -40 }, { 22U, 200 } };
		view.set_line_locator([&lineY](core::diff_side, const std::uint32_t aLine) -> std::optional<int> {
			const auto found = lineY.find(aLine);
			return found == lineY.end() ? std::nullopt : std::optional{ found->second };
		});
		view.set_anchor_file(core::file_identity_of("src/a.cpp"));
		auto root = thread("review-thread:T1", "src/a.cpp", 22U);
		auto post = reply("c-1", "review-thread:T1", "tbutler", "issue: begin_frame() can hand back the frame still on screen.");
		post.Line = 22U;
		view.adopt({ root, post });
		const auto comparison = core::file_comparison_digest{ };
		view.set_draft(core::review_comment_draft{ .Id = core::review_draft_id{ 1U }, .Anchor = core::source_line_ref{ .Comparison = comparison, .Side = core::diff_side::right, .Line = 4U } }, comparison);
		view.begin_draft();
		QApplication::processEvents();
		expect(view.writing_draft() && view.open_card() != nullptr && view.open_card()->geometry().top() >= 0,
			   "a new comment's composer on a line above the drawer's top is whole in view (top " + std::to_string(view.open_card() != nullptr ? view.open_card()->geometry().top() : -999) + ")");
		view.end_draft();
		view.set_draft(std::nullopt, comparison);
		QApplication::processEvents();
		lineY[22U] = -40;
		view.open_thread("review-thread:T1");
		QApplication::processEvents();
		expect(view.open_card() != nullptr && view.open_card()->geometry().top() < 0, "negative control: an open published thread still hangs at its line, above the top");
		lineY[22U] = 200;
		view.close_thread();

		view.set_local_comments({
			qt::local_comment_card{ .Id = 7U, .Revision = 1U, .Body = QStringLiteral("Check retained() after a resize."), .ReplyToThread = std::string{ "T1" }, .MakeDraftWhy = QStringLiteral("Can't make a draft: your draft reply in this thread is already open. A thread holds one draft reply of yours; send or delete it first.") },
			qt::local_comment_card{ .Id = 8U, .Revision = 1U, .Body = QStringLiteral("Height is not clamped here."), .Comparison = comparison, .Side = core::diff_side::right, .Line = 22U, .MakeDraftWhy = QStringLiteral("Can't make a draft: your draft on this line already stands. A line holds one draft of yours; publish or delete it first."), .Path = QStringLiteral("src/a.cpp") },
		});
		view.open_thread("review-thread:T1");
		QApplication::processEvents();
		auto* const card = view.open_card();
		expect(card != nullptr, "the thread with my Local reply is open");
		// The guard: no widget of the open card anchors a tooltip to a large
		// area, the bug the owner has seen again and again.
		const auto offenders = [card] {
			auto found = QStringList{ };
			for (auto* const widget : card->findChildren<QWidget*>()) {
				if (!widget->isVisibleTo(card) || widget->height() <= 40) {
					continue;
				}
				if (const auto target = qt::standard_tooltip_target(*widget, widget->rect().center()); target.has_value() && target->Anchor.height() > 40) {
					found.push_back(QStringLiteral("%1 '%2' (%3 px): %4").arg(QString::fromLatin1(widget->metaObject()->className()), widget->objectName()).arg(target->Anchor.height()).arg(target->Text));
				}
			}
			return found;
		};
		const auto bad = offenders();
		expect(bad.isEmpty(), "no tooltip in the open card is anchored to a large area: " + bad.join(QStringLiteral("; ")).toStdString());
		const auto posts = card->findChildren<QWidget*>(QStringLiteral("threadPost"));
		const auto local = std::ranges::find_if(posts, [](const QWidget* aPost) { return aPost->accessibleDescription() == qt::comment_state_words(true, false, false); });
		expect(local != posts.end() && (*local)->height() > 40, "my Local reply is a post of the thread");
		(*local)->setToolTip(QStringLiteral("negative control"));
		expect(!offenders().isEmpty(), "negative control: a tooltip on the whole post is caught");
		(*local)->setToolTip(QString{ });
		// Its tag's tooltip, right under the tag.
		auto shown = false;
		for (auto x = 0; x < (*local)->width() && !shown; x += 4) {
			const auto point = QPoint{ x, 14 };
			QHelpEvent help{ QEvent::ToolTip, point, (*local)->mapToGlobal(point) };
			QApplication::sendEvent(*local, &help);
			if (const auto* const tip = qobject_cast<const QLabel*>(qt::anchored_tooltip()); tip != nullptr && tip->text().startsWith(QStringLiteral("Only on this computer"))) {
				const auto gap = tip->geometry().top() - (*local)->mapToGlobal(point).y();
				expect(gap > 0 && gap <= 24, "the Local reply's tooltip sits right under its tag (" + std::to_string(gap) + " px below the pointer)");
				shown = true;
			}
		}
		qt::hide_anchored_tooltip();
		expect(shown, "pointing at \"Local reply\" shows what it means");
		// Owner look v003 ("just the tooltip is enough"): a refused Draft is
		// the struck pencil, disabled, its reason in its tooltip alone.
		auto* const replyMake = card->findChild<QAbstractButton*>(QStringLiteral("threadLocalReplyMakeDraft"));
		expect(replyMake != nullptr && !replyMake->isEnabled() && replyMake->property("draftRefused").toBool() && replyMake->toolTip().contains(QStringLiteral("draft reply in this thread"))
				   && card->findChild<QLabel*>(QStringLiteral("threadLocalReplyMakeDraftWhy")) == nullptr,
			   "a Local reply's refused Draft says why in its tooltip alone");
		view.open_thread("local:8");
		QApplication::processEvents();
		expect(!button(view, "threadMakeDraft")->isEnabled() && button(view, "threadMakeDraft")->property("draftRefused").toBool() && button(view, "threadMakeDraft")->toolTip().contains(QStringLiteral("draft on this line"))
				   && view.open_card()->findChild<QLabel*>(QStringLiteral("threadMakeDraftWhy")) == nullptr,
			   "a Local comment's refused Draft says why in its tooltip alone");
		expect(view.open_card()->findChild<QWidget*>(QStringLiteral("conversationPullRequestHeader")) == nullptr, "a Local comment on a line never says Pull request");
	}

	/// \brief The owner: "I have a draft comment sometimes not rendering
	/// when I click away from it." My draft was written in the drawer, then
	/// another thread was opened: the view still took the draft for its
	/// composer, so its closed note carried no post and was drawn as an
	/// empty dashed frame, no author and no words. Clicked away, the draft
	/// is a closed note like any saved one, its author and words painted.
	auto test_a_draft_clicked_away_from_paints_its_words() -> void
	{
		qt::conversation_cards_view view;
		view.resize(380, 700);
		view.set_viewer_login(QStringLiteral("reviewer"));
		view.show();
		QApplication::processEvents();
		view.set_line_locator([](core::diff_side, const std::uint32_t aLine) -> std::optional<int> {
			return aLine == 754U ? std::optional{ 80 } : aLine == 760U ? std::optional{ 260 }
																	   : std::nullopt;
		});
		view.set_anchor_file(core::file_identity_of("src/a.cpp"));
		auto root = thread("review-thread:T1", "src/a.cpp", 760U);
		auto post = reply("c-1", "review-thread:T1", "reviewer-bot", "issue: the pair table is rebuilt on every frame.");
		post.Line = 760U;
		view.adopt({ root, post });
		const auto comparison = core::file_comparison_digest{ };
		const auto body       = std::string{ "suggestion (non-blocking): the cache is rebuilt even when nothing it holds has changed." };
		view.set_draft(core::review_comment_draft{ .Id = core::review_draft_id{ 1U }, .Anchor = core::source_line_ref{ .Comparison = comparison, .Side = core::diff_side::right, .Line = 753U }, .Body = body }, comparison);
		view.begin_draft();
		QApplication::processEvents();
		expect(view.writing_draft(), "my draft is written in the drawer");
		const auto draftCard = [&view]() -> const qt::thread_card* {
			const auto found = std::ranges::find(view.cards(), qt::conversation_cards_view::draft_identity(), &qt::thread_card::Id);
			return found == view.cards().end() ? nullptr : &*found;
		};
		// Clicked away: the thread under it is pressed open.
		const auto thread = std::ranges::find(view.cards(), std::string{ "review-thread:T1" }, &qt::thread_card::Id);
		expect(thread != view.cards().end(), "the thread under my draft has a note");
		press_at(view, thread->Rect.translated(0, -view.verticalScrollBar()->value()).center());
		QApplication::processEvents();
		expect(view.open_thread_id() == "review-thread:T1" && !view.writing_draft(), "the thread is open and my draft is no longer written");
		const auto* const card = draftCard();
		expect(card != nullptr, "my draft keeps its note");
		expect(!card->Writing && !card->Posts.empty() && card->Posts.front().Author == QStringLiteral("reviewer") && card->Posts.front().Body.startsWith(QStringLiteral("suggestion")),
			   "my draft's note carries its author and words, not an empty composer");
		// Its height is a note's with its words, as a fresh view measures it.
		qt::conversation_cards_view fresh;
		fresh.resize(380, 700);
		fresh.set_viewer_login(QStringLiteral("reviewer"));
		fresh.show();
		QApplication::processEvents();
		fresh.set_line_locator([](core::diff_side, const std::uint32_t aLine) -> std::optional<int> {
			return aLine == 754U ? std::optional{ 80 } : aLine == 760U ? std::optional{ 260 }
																	   : std::nullopt;
		});
		fresh.set_anchor_file(core::file_identity_of("src/a.cpp"));
		fresh.adopt({ root, post });
		fresh.set_draft(core::review_comment_draft{ .Id = core::review_draft_id{ 1U }, .Anchor = core::source_line_ref{ .Comparison = comparison, .Side = core::diff_side::right, .Line = 753U }, .Body = body }, comparison);
		QApplication::processEvents();
		const auto freshCard = std::ranges::find(fresh.cards(), qt::conversation_cards_view::draft_identity(), &qt::thread_card::Id);
		expect(freshCard != fresh.cards().end() && freshCard->Rect.height() == card->Rect.height(),
			   "my draft's note is as tall as a fresh view draws it (" + std::to_string(card->Rect.height()) + " px)");
		// Painted: inside its dashed frame there is ink, its avatar, author
		// and words.
		const auto image = view.viewport()->grab().toImage();
		const auto ratio = image.devicePixelRatio();
		const auto box   = card->Rect.translated(0, -view.verticalScrollBar()->value()).adjusted(6, 6, -6, -6);
		const auto paper = image.pixelColor(static_cast<int>((box.left() - 3) * ratio), static_cast<int>((box.center().y()) * ratio));
		auto       inked = 0;
		for (auto y = static_cast<int>(box.top() * ratio); y < static_cast<int>(box.bottom() * ratio) && y < image.height(); ++y) {
			for (auto x = static_cast<int>(box.left() * ratio); x < static_cast<int>(box.right() * ratio) && x < image.width(); ++x) {
				const auto colour = image.pixelColor(x, y);
				if (std::abs(colour.red() - paper.red()) + std::abs(colour.green() - paper.green()) + std::abs(colour.blue() - paper.blue()) > 60) {
					++inked;
				}
			}
		}
		expect(inked > 300, "my draft's note paints its avatar, author and words (" + std::to_string(inked) + " inked pixels)");
		// Closing the thread leaves it so.
		view.close_thread();
		QApplication::processEvents();
		expect(draftCard() != nullptr && !draftCard()->Posts.empty(), "closing the thread leaves my draft's note its words");
		// And opening it again makes it the composer once more.
		view.begin_draft();
		QApplication::processEvents();
		expect(view.writing_draft() && draftCard() != nullptr && draftCard()->Writing, "my draft opens as its composer again");
	}

	/// \brief [[BPR-2262]]. Every way the open card moves off my draft ends
	/// writing it: here the pull request's conversation shown and the lines
	/// shown again, which moved the open card without asking. The draft comes
	/// back as its note with my name and words, never as an open composer
	/// the window did not begin, and its note follows its words.
	auto test_a_draft_left_by_the_conversation_keeps_its_words() -> void
	{
		qt::conversation_cards_view view;
		view.resize(380, 700);
		view.set_viewer_login(QStringLiteral("reviewer"));
		view.show();
		QApplication::processEvents();
		view.set_line_locator([](core::diff_side, const std::uint32_t aLine) -> std::optional<int> {
			return aLine == 41U ? std::optional{ 80 } : aLine == 60U ? std::optional{ 260 }
																	 : std::nullopt;
		});
		view.set_anchor_file(core::file_identity_of("src/a.cpp"));
		auto root = thread("review-thread:T1", "src/a.cpp", 60U);
		auto post = reply("c-1", "review-thread:T1", "someone", "question: is this needed?");
		post.Line = 60U;
		view.adopt({ root, post });
		const auto comparison = core::file_comparison_digest{ };
		auto       draft      = core::review_comment_draft{ };
		draft.Id              = core::review_draft_id{ 1U };
		draft.Anchor          = core::source_line_ref{ .Comparison = comparison, .Side = core::diff_side::right, .Line = 40U };
		draft.Body            = "nit: a draft being written";
		view.set_draft(draft, comparison);
		view.begin_draft();
		QApplication::processEvents();
		expect(view.writing_draft() && view.draft_editor() != nullptr, "my draft is written in the drawer");
		const auto draftCard = [&view]() -> std::optional<qt::thread_card> {
			const auto found = std::ranges::find(view.cards(), qt::conversation_cards_view::draft_identity(), &qt::thread_card::Id);
			return found == view.cards().end() ? std::nullopt : std::optional{ *found };
		};
		view.show_pull_request_comments(true);
		QApplication::processEvents();
		expect(!view.writing_draft() && view.draft_editor() == nullptr, "showing the conversation ends writing my draft");
		view.show_pull_request_comments(false);
		QApplication::processEvents();
		const auto card = draftCard();
		expect(!view.writing_draft() && view.draft_editor() == nullptr && view.open_thread_id().empty() && card.has_value() && !card->Writing
				   && !card->Posts.empty() && card->Posts.front().Author == QStringLiteral("reviewer") && card->Posts.front().Body.startsWith(QStringLiteral("nit:")),
			   "back on the lines, my draft is its note with my name and words, not an open composer");
		// Its note follows its words once it is no longer written.
		draft.Body = "nit: a draft with more words";
		view.set_draft(draft, comparison);
		QApplication::processEvents();
		const auto changed = draftCard();
		expect(changed.has_value() && !changed->Posts.empty() && changed->Posts.front().Body.contains(QStringLiteral("more words")), "my draft's note shows its new words");
		view.begin_draft();
		QApplication::processEvents();
		expect(view.writing_draft() && view.draft_editor() != nullptr, "my draft opens as its composer again");
	}

	/// \brief [[BPR-2173]]. The owner, on look v007 of RFC-BPR-0127: "going
	/// from image 2 and clicking the bottom comment gives image 1". A tall
	/// thread is open at its line and the drawer is scrolled down to the
	/// card under it; opening that card closes the tall thread, and the
	/// drawer kept its scroll, so the opened card was cut at the top with
	/// empty drawer under it. The opened card is whole in view, its top
	/// clear of the edge chips, and nothing but drawer past the end of the
	/// notes is left under it.
	auto test_opening_the_card_below_an_open_thread_shows_it_whole() -> void
	{
		qt::conversation_cards_view view;
		view.resize(380, 520);
		view.show();
		QApplication::processEvents();
		view.set_line_locator([](core::diff_side, const std::uint32_t aLine) -> std::optional<int> { return aLine == 6U ? std::optional{ 100 } : std::nullopt; });
		view.set_anchor_file(core::file_identity_of("src/a.cpp"));
		auto forest = std::vector<conversation>{ thread("review-thread:T1", "src/a.cpp", 6U) };
		for (auto index = 0; index < 6; ++index) {
			auto post = reply("c-" + std::to_string(index), "review-thread:T1", "tbutler",
							  "issue (blocking): draw_scene() runs before the clear finishes on the second view, and the depth buffer is shared by both views.");
			post.Line = 6U;
			forest.push_back(std::move(post));
		}
		view.adopt(forest);
		const auto comparison = core::file_comparison_digest{ };
		// The comparison shown, which places my Local comment at its line.
		view.set_draft(std::nullopt, comparison);
		view.set_local_comments({
			qt::local_comment_card{ .Id = 8U, .Revision = 1U, .Body = QStringLiteral("Height is not clamped here. Re-check once the swap chain change lands."), .Comparison = comparison, .Side = core::diff_side::right, .Line = 6U, .Path = QStringLiteral("src/a.cpp") },
		});
		view.open_thread("review-thread:T1");
		QApplication::processEvents();
		auto* const bar = view.verticalScrollBar();
		expect(view.open_card() != nullptr && view.open_card()->height() > view.viewport()->height(), "the open thread is taller than the drawer");
		// Scrolled until the Local comment under the thread is in view, as
		// in the owner's image 2.
		const auto below = std::ranges::find(view.cards(), std::string{ "local:8" }, &qt::thread_card::Id);
		expect(below != view.cards().end(), "my Local comment hangs under the thread");
		bar->setValue(std::min(bar->maximum(), below->Rect.bottom() + 8 - view.viewport()->height()));
		QApplication::processEvents();
		const auto scrolled = bar->value();
		expect(scrolled > 0, "the drawer is scrolled down to the card under the thread");
		view.open_thread("local:8");
		QApplication::processEvents();
		auto* const card = view.open_card();
		expect(card != nullptr && view.open_thread_id() == "local:8", "the Local comment is open");
		const auto top = card != nullptr ? card->geometry().top() : -999;
		expect(top >= 30, "the opened card's top is in view, clear of the edge chips (top " + std::to_string(top) + ", scroll " + std::to_string(bar->value()) + ")");
		expect(card != nullptr && card->geometry().bottom() <= view.viewport()->height(), "the opened card is whole in view");
		const auto lowest = std::ranges::max(view.cards(), { }, [](const qt::thread_card& aCard) { return aCard.Rect.bottom(); }).Rect.bottom();
		expect(lowest - bar->value() >= view.viewport()->height() - 40 || bar->value() == 0,
			   "no stretch of empty drawer under the opened card (notes end at " + std::to_string(lowest - bar->value()) + " of " + std::to_string(view.viewport()->height()) + ")");
	}

	/// \brief Owner look v005 ("There's a lot of overlap on the UX here").
	/// The agent drafted a long comment on a line GitHub may not place: the
	/// composer's notice arrived after it was laid out, the card kept its
	/// old height, and the label chip and Enhance were drawn over the text.
	/// Every row of the composer stands clear of the next.
	auto test_composer_notice_keeps_rows_apart() -> void
	{
		qt::conversation_cards_view view;
		view.resize(300, 900);
		view.show();
		QApplication::processEvents();
		view.set_line_locator([](core::diff_side, const std::uint32_t aLine) -> std::optional<int> { return aLine == 1U ? std::optional{ 40 } : std::nullopt; });
		view.set_anchor_file(core::file_identity_of("src/a.cpp"));
		view.adopt({ });
		const auto comparison = core::file_comparison_digest{ };
		const auto body       = std::string{ "The budget is reset on every call, because the counter is restored before the check runs, so the budget is effectively unbounded. "
											 "Consider moving the reset into `begin_request` instead:\n\n```cpp\nvoid Renderer::begin_request() {\n    frame_budget_.reset();\n}\n```\n\n"
											 "This keeps per-frame work bounded while still giving each request a fresh budget." };
		view.set_draft(core::review_comment_draft{ .Id = core::review_draft_id{ 1U }, .Anchor = core::source_line_ref{ .Comparison = comparison, .Side = core::diff_side::right, .Line = 0U }, .Body = body }, comparison);
		QApplication::processEvents();
		const auto draftId = std::ranges::find_if(view.cards(), [](const qt::thread_card& aCard) { return aCard.Draft; })->Id;
		view.open_thread(draftId);
		QApplication::processEvents();
		view.present_draft(qt::draft_composer_presentation{ .Body = QString::fromStdString(body), .Publishable = true, .Lines = QStringLiteral("Line 1") });
		QApplication::processEvents();
		view.present_draft(qt::draft_composer_presentation{ .Body = QString::fromStdString(body), .Publishable = true, .Lines = QStringLiteral("Line 1"), .Notice = QStringLiteral("GitHub may not place a comment on this line. If it refuses, the comment is posted as a file comment that links to the line.") });
		for (auto spin = 0; spin < 50; ++spin) {
			QApplication::processEvents();
			QThread::msleep(4);
		}
		auto* const card   = view.open_card();
		auto* const text   = card != nullptr ? card->findChild<QWidget*>(QStringLiteral("threadDraftText")) : nullptr;
		auto* const labels = card != nullptr ? card->findChild<QWidget*>(QStringLiteral("threadDraftLabels")) : nullptr;
		auto* const status = card != nullptr ? card->findChild<QWidget*>(QStringLiteral("threadDraftStatus")) : nullptr;
		expect(text != nullptr && labels != nullptr && status != nullptr && status->isVisibleTo(card), "the composer shows its field, its tools and the notice");
		const auto bottomOf = [card](QWidget* aWidget) { return aWidget->mapTo(card, QPoint{ 0, aWidget->height() }).y(); };
		const auto topOf    = [card](QWidget* aWidget) { return aWidget->mapTo(card, QPoint{ 0, 0 }).y(); };
		std::cout << "composer rows: text " << topOf(text) << "-" << bottomOf(text) << " labels " << topOf(labels) << "-" << bottomOf(labels) << " status " << topOf(status) << "-" << bottomOf(status) << '\n';
		expect(bottomOf(text) <= topOf(labels) && bottomOf(labels) <= topOf(status), "the field, its tool row and the notice do not overlap");
		expect(text->height() >= text->minimumSizeHint().height() && labels->height() >= labels->minimumSizeHint().height(), "nothing is squeezed below its own minimum");
		expect(card->findChild<QWidget*>(QStringLiteral("threadMaximizeDraft")) != nullptr && card->findChild<QWidget*>(QStringLiteral("conversationShowInDiscuss")) == nullptr,
			   "the draft being written has one way to Discuss, not two of the same glyph");
	}

	/// \brief [[BPR-2172]]. The owner, on look v007 of RFC-BPR-0127: "Can
	/// we make this warning take less space, maybe it's just a warning sign
	/// on the row above and expands a disclosure on click". Where the
	/// comment may go is a warning mark beside the label chip; pressing it
	/// (or Space on it) shows the sentence under the row, and again hides
	/// it. A notice about the publication itself keeps its lines.
	auto test_placement_notice_is_a_warning_mark() -> void
	{
		qt::conversation_cards_view view;
		view.resize(320, 900);
		view.show();
		QApplication::processEvents();
		view.set_line_locator([](core::diff_side, const std::uint32_t aLine) -> std::optional<int> { return aLine == 1U ? std::optional{ 40 } : std::nullopt; });
		view.set_anchor_file(core::file_identity_of("src/a.cpp"));
		view.adopt({ });
		const auto comparison = core::file_comparison_digest{ };
		const auto body       = std::string{ "Height is not clamped here." };
		view.set_draft(core::review_comment_draft{ .Id = core::review_draft_id{ 1U }, .Anchor = core::source_line_ref{ .Comparison = comparison, .Side = core::diff_side::right, .Line = 0U }, .Body = body }, comparison);
		QApplication::processEvents();
		const auto draftId = std::ranges::find_if(view.cards(), [](const qt::thread_card& aCard) { return aCard.Draft; })->Id;
		view.open_thread(draftId);
		const auto settle = [] {
			for (auto spin = 0; spin < 30; ++spin) {
				QApplication::processEvents();
				QThread::msleep(2);
			}
		};
		const auto words = QStringLiteral("GitHub may not place a comment on this line. If it refuses, the comment is posted as a file comment that links to the line.");
		view.present_draft(qt::draft_composer_presentation{ .Body = QString::fromStdString(body), .Publishable = true, .Lines = QStringLiteral("Line 1") });
		settle();
		const auto plain = view.open_card()->height();
		view.present_draft(qt::draft_composer_presentation{ .Body = QString::fromStdString(body), .Publishable = true, .Lines = QStringLiteral("Line 1"), .Notice = words, .NoticeFolds = true });
		settle();
		auto* const card   = view.open_card();
		auto* const mark   = card->findChild<QToolButton*>(QStringLiteral("threadDraftNoticeMark"));
		auto* const labels = card->findChild<QWidget*>(QStringLiteral("threadDraftLabels"));
		auto* const status = card->findChild<QLabel*>(QStringLiteral("threadDraftStatus"));
		expect(mark != nullptr && mark->isVisibleTo(card) && status != nullptr && !status->isVisibleTo(card), "the placement notice is a warning mark, its sentence closed");
		expect(std::abs(mark->mapTo(card, mark->rect().center()).y() - labels->mapTo(card, labels->rect().center()).y()) <= 4, "the mark sits in the tool row, beside the label chip");
		expect(card->height() <= plain + 2, "the composer does not grow for it (" + std::to_string(card->height()) + " against " + std::to_string(plain) + ")");
		expect(mark->accessibleName() == QStringLiteral("Placement warning") && mark->accessibleDescription() == words && !mark->toolTip().isEmpty(),
			   "the mark is named for assistive technology, its sentence its description");
		mark->click();
		settle();
		expect(status->isVisibleTo(card) && status->text() == words && view.open_card()->height() > plain + 10, "pressing it discloses the sentence under the row");
		expect(mark->accessibleName() == QStringLiteral("Placement warning, shown"), "and its name says the sentence is shown");
		mark->setFocus();
		auto space = QKeyEvent{ QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" ") };
		QApplication::sendEvent(mark, &space);
		auto release = QKeyEvent{ QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" ") };
		QApplication::sendEvent(mark, &release);
		settle();
		expect(!status->isVisibleTo(card) && !mark->isChecked(), "Space on the mark closes it again");
		// What the last Publish came to is not folded.
		view.present_draft(qt::draft_composer_presentation{ .Body = QString::fromStdString(body), .Publishable = false, .Lines = QStringLiteral("Line 1"), .Notice = QStringLiteral("Publishing your comment to GitHub…") });
		settle();
		expect(!mark->isVisibleTo(card) && status->isVisibleTo(card) && status->text() == QStringLiteral("Publishing your comment to GitHub…"), "a publication notice keeps its lines, and no mark");
		view.present_draft(qt::draft_composer_presentation{ .Body = QString::fromStdString(body), .Publishable = true, .Lines = QStringLiteral("Line 1") });
		settle();
		expect(!mark->isVisibleTo(card) && !status->isVisibleTo(card), "with no notice there is neither");
	}

	auto test_composer_state_control() -> void
	{
		qt::conversation_cards_view view;
		view.resize(380, 640);
		view.show();
		QApplication::processEvents();
		view.set_line_locator([](core::diff_side, const std::uint32_t aLine) -> std::optional<int> { return aLine == 31U ? std::optional{ 200 } : std::nullopt; });
		view.set_anchor_file(core::file_identity_of("src/a.cpp"));
		view.adopt({ });
		const auto comparison = core::file_comparison_digest{ };
		view.set_draft(core::review_comment_draft{ .Id = core::review_draft_id{ 1U }, .Anchor = core::source_line_ref{ .Comparison = comparison, .Side = core::diff_side::right, .Line = 30U }, .Body = "Height is not clamped here." }, comparison);
		QApplication::processEvents();
		auto actions = std::vector<qt::card_action>{ };
		view.set_action_dispatch([&actions](const qt::card_action aAction, const std::string&) { actions.push_back(aAction); });
		const auto draftId = std::ranges::find_if(view.cards(), [](const qt::thread_card& aCard) { return aCard.Draft && aCard.Line == std::optional{ 31U }; })->Id;
		view.open_thread(draftId);
		QApplication::processEvents();
		const auto present = [&view](const bool aLocal) {
			view.present_draft(qt::draft_composer_presentation{ .Body = QStringLiteral("Height is not clamped here."), .Publishable = true, .Lines = QStringLiteral("Line 31"), .Local = aLocal });
			QApplication::processEvents();
		};
		present(false);
		auto* const local = view.open_card()->findChild<QToolButton*>(QStringLiteral("threadStateLocal"));
		auto* const draft = view.open_card()->findChild<QToolButton*>(QStringLiteral("threadStateDraft"));
		auto* const save  = button(view, "threadSaveLocal");
		const auto  shown = [&view](const char* aName) { return button(view, aName) != nullptr && button(view, aName)->isVisibleTo(view.open_card()); };
		expect(local != nullptr && draft != nullptr && save != nullptr && draft->isChecked() && !local->isChecked() && shown("threadReviewDraft") && !shown("threadSaveLocal"),
			   "Comment… opens the composer on Draft, with today's row");
		local->click();
		QApplication::processEvents();
		auto* const words = view.open_card()->findChild<QLabel*>(QStringLiteral("threadLocalWords"));
		expect(local->isChecked() && !draft->isChecked() && shown("threadSaveLocal") && !shown("threadReviewDraft") && !shown("threadPublishDraft") && words != nullptr
				   && words->isVisibleTo(view.open_card()) && words->text() == QStringLiteral("Only on this computer. Never sent.") && actions.back() == qt::card_action::compose_local,
			   "Local: Delete and Save, the sentence, and nothing that sends");
		QKeyEvent key{ QEvent::KeyPress, Qt::Key_Return, Qt::ControlModifier };
		QApplication::sendEvent(view.draft_editor(), &key);
		expect(actions.back() == qt::card_action::save_local, "⌘↩ saves it as Local");
		draft->click();
		QApplication::processEvents();
		expect(draft->isChecked() && shown("threadReviewDraft") && !shown("threadSaveLocal") && actions.back() == qt::card_action::compose_draft, "Draft brings today's row back");
		present(true);
		expect(local->isChecked() && shown("threadSaveLocal"), "AC01: N's preset opens the composer on Local");
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
		const auto words = [&view](const char* aName) { return button(view, aName) != nullptr ? button(view, aName)->text() : QString{ }; };
		// RFC-BPR-0127 ([[BPR-2158]]): the state control stands where the
		// privacy note stood; its Draft segment says where the draft stands.
		auto* const privacy = view.open_card()->findChild<QToolButton*>(QStringLiteral("threadStateDraft"));
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
				   && button(view, "threadReviewDraft")->isEnabled() && privacy != nullptr && privacy->isChecked() && privacy->accessibleName() == QStringLiteral("Draft, not in your review"),
			   "with no review in progress: Add single comment with ⌘⇧↩ and Start a review with ⌘↩, each named in its anchored tooltip");
		// [[BPR-1999]], RFC-BPR-0101 AC03. A comment composer is not a chat:
		// Return alone starts a line and publishes or keeps nothing.
		{
			const auto before = actions.size();
			press(Qt::NoModifier);
			expect(actions.size() == before, "Return alone in a comment composer neither sends nor keeps it");
		}
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
				   && privacy->accessibleName() == QStringLiteral("Draft, not in your review"),
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
		std::cout << "narrow-draft-row: card=" << view.open_card()->width() << " single=" << singleButton->isVisible() << " more=" << more->isVisible() << " review=" << review->geometry().left() << ".." << review->geometry().right()
				  << '@' << middle(review) << " hint=" << review->sizeHint().width() << " width=" << review->width() << " more_at=" << more->geometry().left() << ".." << more->geometry().right() << '@' << middle(more)
				  << " trash@" << middle(trash) << '\n';
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
				   && view.draft_editor()->lineWrapMode() == QPlainTextEdit::WidgetWidth
				   && view.draft_editor()->textCursor().position() == view.draft_editor()->toPlainText().lastIndexOf(QLatin1Char{ '\n' }),
			   "a suggestion shows what it changes, the field still wraps, and the caret is at the end of the block's last line");
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
	// [[BPR-1745]]. The owner, 2026-10-22: "Allow replying to an
	// outdated thread from the drawer, like GitHub does". An outdated thread's
	// menu offers Reply; its reply field opens in place with one quiet line
	// saying its line has changed, in the card's secondary ink. A current
	// thread's reply is unchanged and carries no such line. Before the change
	// the outdated thread's Reply was disabled with "This thread's line isn't
	// in the code shown, so it can't take a reply here."
	void test_an_outdated_thread_takes_a_reply_with_a_quiet_note()
	{
		using availability          = application::github_conversation_anchor_availability;
		auto outdated               = thread("t-outdated", "src/b.cpp", 4U);
		outdated.Line               = std::nullopt;
		outdated.OriginalLine       = 4U;
		outdated.AnchorAvailability = availability::outdated_revision;
		outdated.Outdated           = true;
		const auto current          = thread("t-current", "src/a.cpp", 10U);

		qt::conversation_cards_view view;
		view.resize(420, 900);
		view.show();
		QApplication::processEvents();
		view.adopt({ current, reply("c-current", "t-current", "a", "first"), outdated, reply("c-outdated", "t-outdated", "b", "second") });
		view.set_scope(qt::cards_scope::whole_pull_request);
		auto pressed = std::vector<std::string>{ };
		view.set_action_dispatch([&view, &pressed](const qt::card_action aAction, const std::string& aId) {
			if (aAction == qt::card_action::reply) {
				pressed.push_back(aId);
				view.begin_reply(aId);
			}
		});
		QApplication::processEvents();
		const auto card = std::ranges::find(view.cards(), std::string{ "t-outdated" }, &qt::thread_card::Id);
		expect(card != view.cards().end() && card->ReplyOnThread && card->Outdated, "the core answers the outdated thread by the thread");
		const auto lined = std::ranges::find(view.cards(), std::string{ "t-current" }, &qt::thread_card::Id);
		expect(lined != view.cards().end() && !lined->ReplyOnThread, "a current thread is answered on its line");

		auto* const menu  = view.thread_menu("t-outdated");
		const auto  items = menu != nullptr ? menu->actions() : QList<QAction*>{ };
		const auto  reply = std::ranges::find_if(items, [](const QAction* aAction) { return aAction->text() == QStringLiteral("Reply"); });
		expect(reply != items.end() && (*reply)->isEnabled(), "the outdated thread's menu offers Reply");
		delete menu;

		const auto note = [&view] { return view.open_card() != nullptr ? view.open_card()->findChild<QLabel*>(QStringLiteral("threadReplyOutdatedNote")) : nullptr; };
		view.open_thread("t-outdated");
		QApplication::processEvents();
		auto* const field = view.open_card()->findChild<QAbstractButton*>(QStringLiteral("threadReplyField"));
		expect(field != nullptr && field->isVisibleTo(view.open_card()), "the outdated thread shows its Reply field as a current one does");
		field->click();
		QApplication::processEvents();
		expect(pressed == std::vector<std::string>{ "t-outdated" } && view.reply_editor() != nullptr && view.reply_editor()->isVisibleTo(view.open_card()),
			   "Reply opens the reply field in place");
		expect(note() != nullptr && note()->isVisibleTo(view.open_card()) && note()->text() == QStringLiteral("Outdated: this thread's line has changed since it was written"),
			   "one quiet line says the thread's line has changed since it was written");
		expect(note()->palette().color(QPalette::WindowText) != view.palette().color(QPalette::WindowText) && note()->font().pointSizeF() < view.open_card()->font().pointSizeF(),
			   "the note is set smaller, in the secondary ink");
		expect(view.reply_editor()->accessibleName() == QStringLiteral("Reply to the outdated thread"), "the field is named for the thread it answers");
		const auto noteBelow = note()->mapTo(view.open_card(), QPoint{ 0, note()->height() }).y();
		const auto editorTop = view.reply_editor()->mapTo(view.open_card(), QPoint{ 0, 0 }).y();
		expect(noteBelow <= editorTop, "the note sits above the field, not over it");
		if (const auto path = qEnvironmentVariable("BPR_OUTDATED_REPLY_PNG"); !path.isEmpty()) {
			view.grab().save(path);
		}
		view.close_thread();
		QApplication::processEvents();

		view.open_thread("t-current");
		view.begin_reply("t-current");
		QApplication::processEvents();
		expect(view.reply_editor() != nullptr && note() == nullptr, "a current thread's reply carries no outdated note");
		view.hide();
	}

	// [[BPR-1746]]. GitHub's `viewerCanReply` and the lock, read with
	// the conversation, refuse a reply that could not succeed before it is
	// written: no Reply field, a quiet line saying why, and the menu's Reply
	// disabled with the same words. A collaborator on a locked pull request,
	// and a thread whose permission was not read, keep Reply.
	void test_a_thread_github_refuses_offers_no_reply()
	{
		using availability              = application::github_conversation_anchor_availability;
		auto locked                     = thread("t-locked", "src/a.cpp", 10U);
		locked.ViewerCanReply           = false;
		locked.ConversationLocked       = true;
		auto denied                     = thread("t-denied", "src/a.cpp", 12U);
		denied.ViewerCanReply           = false;
		denied.ConversationLocked       = false;
		auto collaborator               = thread("t-collaborator", "src/a.cpp", 14U);
		collaborator.ViewerCanReply     = true;
		collaborator.ConversationLocked = true;
		auto outdated                   = thread("t-outdated", "src/b.cpp", 4U);
		outdated.Line                   = std::nullopt;
		outdated.OriginalLine           = 4U;
		outdated.AnchorAvailability     = availability::outdated_revision;
		outdated.Outdated               = true;
		outdated.ViewerCanReply         = false;
		outdated.ConversationLocked     = true;
		const auto unread               = thread("t-unread", "src/a.cpp", 16U);

		qt::conversation_cards_view view;
		view.resize(420, 900);
		view.show();
		QApplication::processEvents();
		view.adopt({ locked, reply("c-locked", "t-locked", "a", "first"), denied, reply("c-denied", "t-denied", "b", "second"), collaborator,
					 reply("c-collaborator", "t-collaborator", "c", "third"), outdated, reply("c-outdated", "t-outdated", "d", "fourth"), unread,
					 reply("c-unread", "t-unread", "e", "fifth") });
		view.set_scope(qt::cards_scope::whole_pull_request);
		auto pressed = std::vector<std::string>{ };
		view.set_action_dispatch([&pressed](const qt::card_action aAction, const std::string& aId) {
			if (aAction == qt::card_action::reply) {
				pressed.push_back(aId);
			}
		});
		QApplication::processEvents();
		const auto card = [&view](const std::string& aId) { return *std::ranges::find(view.cards(), aId, &qt::thread_card::Id); };
		expect(card("t-locked").ReplyRefusal == core::thread_reply_refusal::locked && card("t-denied").ReplyRefusal == core::thread_reply_refusal::viewer_cannot_reply
				   && card("t-outdated").ReplyRefusal == core::thread_reply_refusal::locked && !card("t-outdated").ReplyOnThread,
			   "the core refuses the threads GitHub refuses, and says whether the lock is why");
		expect(card("t-collaborator").ReplyRefusal == core::thread_reply_refusal::none && card("t-unread").ReplyRefusal == core::thread_reply_refusal::none,
			   "a collaborator on a locked pull request, and a thread whose permission was not read, are not refused");

		const auto menuReply = [&view](const std::string& aId) -> std::pair<bool, QString> {
			auto* const menu  = view.thread_menu(aId);
			const auto  items = menu != nullptr ? menu->actions() : QList<QAction*>{ };
			const auto  found = std::ranges::find_if(items, [](const QAction* aAction) { return aAction->text() == QStringLiteral("Reply"); });
			auto        state = found != items.end() ? std::pair{ (*found)->isEnabled(), (*found)->toolTip() } : std::pair{ true, QString{ } };
			delete menu;
			return state;
		};
		const auto lockedWords = QStringLiteral("This conversation is locked. Only collaborators can reply.");
		const auto deniedWords = QStringLiteral("GitHub doesn't let you reply in this thread.");
		expect(menuReply("t-locked") == std::pair{ false, lockedWords } && menuReply("t-denied") == std::pair{ false, deniedWords },
			   "the menu's Reply is disabled and says why");
		expect(menuReply("t-collaborator").first && menuReply("t-unread").first, "the others keep the menu's Reply");
		expect(qt::thread_reply_refusal_text(core::thread_reply_refusal::locked) == lockedWords
				   && qt::thread_reply_refusal_text(core::thread_reply_refusal::anonymous_read).isEmpty(),
			   "only GitHub's own refusals have words here");

		for (const auto& [id, words] : { std::pair{ std::string{ "t-locked" }, lockedWords }, std::pair{ std::string{ "t-denied" }, deniedWords }, std::pair{ std::string{ "t-outdated" }, lockedWords } }) {
			view.open_thread(id);
			QApplication::processEvents();
			auto* const field   = view.open_card()->findChild<QAbstractButton*>(QStringLiteral("threadReplyField"));
			auto* const refusal = view.open_card()->findChild<QLabel*>(QStringLiteral("threadReplyRefusal"));
			expect(field == nullptr && refusal != nullptr && refusal->isVisibleTo(view.open_card()) && refusal->text() == words && refusal->accessibleName() == words,
				   "a refused thread shows no Reply field, and one line says why");
			expect(refusal != nullptr && refusal->palette().color(QPalette::WindowText) != view.palette().color(QPalette::WindowText)
					   && refusal->font().pointSizeF() < view.open_card()->font().pointSizeF(),
				   "the line is quiet: smaller, in the secondary ink");
			if (const auto path = qEnvironmentVariable("BPR_REPLY_REFUSAL_PNG"); !path.isEmpty() && id == "t-locked") {
				view.grab().save(path);
			}
			view.close_thread();
			QApplication::processEvents();
		}
		view.open_thread("t-collaborator");
		QApplication::processEvents();
		auto* const field = view.open_card()->findChild<QAbstractButton*>(QStringLiteral("threadReplyField"));
		expect(field != nullptr && view.open_card()->findChild<QLabel*>(QStringLiteral("threadReplyRefusal")) == nullptr, "a collaborator keeps the Reply field");
		field->click();
		QApplication::processEvents();
		expect(pressed == std::vector<std::string>{ "t-collaborator" }, "and Reply asks for the reply");
		view.hide();
	}

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
		// [[BPR-1721]]. The thread is on lines 12 to 13, and says so: its
		// card, its header's line button, and its mark, whose open wash spans
		// both lines. It read as a thread on line 13 alone.
		const auto* const shown = view.open_card()->findChild<QToolButton*>(QStringLiteral("threadShowLine"));
		expect(view.open_card()->accessibleName() == QStringLiteral("Thread on lines 12–13") && shown != nullptr && shown->text() == QStringLiteral("12–13")
				   && shown->toolTip() == QStringLiteral("Show lines 12–13 in the code"),
			   "a published thread on two lines is named for both, and its header shows both");
		const auto marks = view.line_marks(core::diff_side::right);
		const auto mark  = std::ranges::find(marks, 12U, &qt::thread_line_mark::Line);
		expect(mark != marks.end() && mark->FirstLine == std::optional<std::uint32_t>{ 11U } && mark->Open && mark->Description.startsWith(QStringLiteral("Lines 12–13: 1 post")),
			   "its mark spans lines 12 and 13 and its description names both");
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
		expect(view.open_card()->accessibleName() == QStringLiteral("Thread on line 13"), "a thread with no start line is on its one line");

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
	void test_pull_request_comments_live_at_the_edge()
	{
		using namespace bpr;
		qt::conversation_cards_view view;
		view.resize(360, 800);
		auto summary        = application::github_conversation_projection{ };
		summary.Id          = "review:one";
		summary.Kind        = application::github_conversation_kind::review;
		summary.AuthorLogin = "reviewer";
		summary.Body        = "Please check both files.";
		summary.ReviewState = application::github_review_state_projection::changes_requested;
		summary.CreatedAt   = "2026-10-18T01:00:00Z";
		auto old            = application::github_conversation_projection{ };
		old.Id              = "issue-comment:old";
		old.AuthorLogin     = "author";
		old.Body            = "First comment";
		old.CreatedAt       = "2026-10-17T00:00:00Z";
		auto fresh          = old;
		fresh.Id            = "issue-comment:new";
		fresh.Body          = "New comment";
		fresh.CreatedAt     = "2026-10-18T02:00:00Z";
		auto deleted        = fresh;
		deleted.Id          = "gone";
		deleted.Deleted     = true;
		auto file           = thread("thread", "src/a.cpp", 9U);
		auto post           = reply("file-post", "thread", "reviewer", "A file comment");
		post.ReviewId       = summary.Id;
		post.Path           = file.Path;
		post.Line           = file.Line;
		view.adopt({ summary, file, post, fresh, old, deleted }, core::file_identity_of("src/a.cpp"));
		view.set_line_locator([](core::diff_side, std::uint32_t) { return std::optional{ 100 }; });
		view.set_read_state({ { fresh.Id, { fresh.Id } } });
		std::vector<std::pair<qt::card_action, std::string>> actions;
		view.set_action_dispatch([&](qt::card_action aAction, const std::string& aId) { actions.emplace_back(aAction, aId); });
		view.show();
		QApplication::processEvents();
		// [[BPR-1781]]. Without a window to take it there, no link is offered.
		expect(!view.pull_request_comments_chip()->isVisible(), "the link waits for somewhere to go");
		auto links = std::vector<std::string>{ };
		view.set_pull_request_link_dispatch([&links](const std::string& aId) { links.push_back(aId); });
		QApplication::processEvents();
		// [[BPR-1779]] [[BPR-1781]]. One quiet link, "Pull request
		// conversation", counting comments: the two PR comments, the review's
		// body and the thread it carried; not the review.
		expect(view.pull_request_comments_chip()->isVisible() && view.pull_request_comments_chip()->text() == QStringLiteral("Pull request conversation")
				   && view.pull_request_comments_chip()->accessibleName() == QStringLiteral("Pull request conversation: 4 comments, 1 new"),
			   "one quiet link counts every live comment, independent of the file");
		expect(view.cards().size() == 1U, "file view still shows only its thread");
		view.pull_request_comments_chip()->click();
		QApplication::processEvents();
		expect(links == std::vector<std::string>{ std::string{ } } && !view.pull_request_comments_shown() && view.cards().size() == 1U,
			   "the link asks the window for Overview's Comments and leaves the file's threads as they are");
		view.open_thread(old.Id);
		expect(links.size() == 2U && links.back() == old.Id && !view.pull_request_comments_shown(), "opening a pull request comment beside the code asks the window too");
		// The window shows the conversation, as Overview's Comments tab does.
		view.show_pull_request_comments(true);
		QApplication::processEvents();
		expect(!view.pull_request_comments_chip()->isVisible() && view.findChild<QAbstractButton*>(QStringLiteral("backToThreadLines")) == nullptr,
			   "the conversation offers neither the link nor a way back: Review is where the lines are");
		expect(view.pull_request_comments_shown() && view.cards().size() == 4U && view.cards()[0].Id == old.Id && view.cards()[1].Id == summary.Id && view.cards()[2].Id == file.Id && view.cards()[3].Id == fresh.Id,
			   "chip opens the conversation in time order, the review's body and its thread together at the review's time");
		view.open_thread(fresh.Id);
		QApplication::processEvents();
		expect(!actions.empty() && actions.back() == std::pair{ qt::card_action::mark_read, fresh.Id }, "opening a PR comment asks the existing durable read owner");
		view.open_thread(summary.Id);
		QApplication::processEvents();
		const auto& lines  = view.timeline_lines();
		const auto  bundle = std::ranges::find(lines, summary.Id, &qt::timeline_line::Id);
		expect(bundle != lines.end() && bundle->Kind == qt::timeline_line::kind::bundle && bundle->Verdict == QStringLiteral("Changes requested") && bundle->Author == QStringLiteral("reviewer"),
			   "the review's line carries its verdict and who gave it");
		expect(view.open_card()->findChild<QLabel*>(QStringLiteral("conversationReviewVerdict")) == nullptr, "the review's body is a comment, without the verdict repeated");
		const auto path = std::ranges::find(lines, file.Id, &qt::timeline_line::Id);
		expect(path != lines.end() && path->Kind == qt::timeline_line::kind::path && path->Path == QStringLiteral("src/a.cpp")
				   && std::ranges::find(view.cards(), file.Id, &qt::thread_card::Id)->Line == 9U,
			   "the thread the review carried shows under its file ([[BPR-2171]]: named once), its line on the thread");
		const auto at    = path->Rect.translated(0, -view.verticalScrollBar()->value()).center();
		auto       press = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ at }, view.viewport()->mapToGlobal(QPointF{ at }), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(view.viewport(), &press);
		expect(actions.back() == std::pair{ qt::card_action::open_in_code, file.Id }, "its path line opens the thread in Review");
		view.open_card()->findChild<QToolButton*>(QStringLiteral("conversationShowInDiscuss"))->click();
		expect(actions.back() == std::pair{ qt::card_action::show_in_discuss, summary.Id }, "Show in Discuss preserves the singleton identity");
		view.begin_reply(summary.Id);
		view.present_reply(qt::thread_reply_presentation{ .Thread = summary.Id, .Body = QStringLiteral("A private reply"), .Publishable = true, .Notice = QStringLiteral("Saved locally") });
		QApplication::processEvents();
		expect(view.reply_editor() && view.reply_editor()->toPlainText() == QStringLiteral("A private reply"), "PR reply is composed in place");
		auto* send = view.open_card()->findChild<QAbstractButton*>(QStringLiteral("threadReplyPublish"));
		expect(send && send->text() == QStringLiteral("Send"), "PR reply has an explicit Send");
		send->click();
		expect(actions.back() == std::pair{ qt::card_action::publish_reply, summary.Id }, "only the Send press requests publication");
		// [[BPR-1781]]. Each mode keeps its place: the open card and scroll.
		const auto openInConversation = view.open_thread_id();
		view.show_pull_request_comments(false);
		expect(view.cards().size() == 1U && view.cards()[0].Id == file.Id, "Review's view is the file's threads again");
		view.open_thread(file.Id);
		QApplication::processEvents();
		view.show_pull_request_comments(true);
		QApplication::processEvents();
		expect(view.open_thread_id() == openInConversation, "back in the conversation, the card left open there is open again");
		view.show_pull_request_comments(false);
		QApplication::processEvents();
		expect(view.open_thread_id() == file.Id, "and back beside the code, the file's thread is");
		view.resize(280, 800);
		QApplication::processEvents();
		expect(view.pull_request_comments_chip()->text() == QStringLiteral("Pull request conversation") && view.pull_request_comments_chip()->width() <= 280,
			   "a narrow drawer keeps the link's words within it");
		view.set_feed(true);
		QApplication::processEvents();
		expect(!view.pull_request_comments_chip()->isVisible(), "Discuss remains its existing full conversation feed");
		view.clear();
		expect(!view.pull_request_comments_shown(), "authority clear retires the PR list");
	}

}

namespace {
	/// A key pressed and released on a widget, as a keyboard would.
	void key_click(QWidget* const aWidget, const int aKey)
	{
		auto press   = QKeyEvent{ QEvent::KeyPress, aKey, Qt::NoModifier, aKey == Qt::Key_Space ? QStringLiteral(" ") : QString{ } };
		auto release = QKeyEvent{ QEvent::KeyRelease, aKey, Qt::NoModifier, aKey == Qt::Key_Space ? QStringLiteral(" ") : QString{ } };
		QApplication::sendEvent(aWidget, &press);
		QApplication::sendEvent(aWidget, &release);
		QApplication::processEvents();
	}

	/// [[BPR-1741]]. A thread as the composition names it, with two
	/// posts, and the reactions GitHub reported on them.
	[[nodiscard]] auto reacting_view(qt::conversation_cards_view& aView) -> std::map<std::string, core::reaction_summary>
	{
		aView.resize(440, 900);
		aView.show();
		QApplication::processEvents();
		aView.set_viewer_login(QStringLiteral("reviewer-one"));
		aView.adopt({ thread("review-thread:T1", "src/a.cpp", 10U), reply("review-comment:C1", "review-thread:T1", "someone", "Please rename this."),
					  reply("review-comment:C2", "review-comment:C1", "reviewer-one", "Done.") });
		aView.set_feed(true);
		aView.set_scope(qt::cards_scope::whole_pull_request);
		aView.open_thread("review-thread:T1");
		QApplication::processEvents();
		return {
			{ "C1", core::reaction_summary{ .ViewerCanReact = true, .Groups = { core::reaction_group{ .Content = core::reaction_content::thumbs_up, .Count = 2U, .Reactors = { "someone", "octocat" } }, core::reaction_group{ .Content = core::reaction_content::heart, .Count = 1U, .ViewerReacted = true, .Reactors = { "reviewer-one" } } } } },
			{ "C2", core::reaction_summary{ .ViewerCanReact = false } },
		};
	}

	[[nodiscard]] auto post_named(const qt::conversation_cards_view& aView, const QString& aIdentity) -> QWidget*
	{
		for (auto* const post : aView.open_card()->findChildren<QWidget*>(QStringLiteral("threadPost"))) {
			if (post->property("postIdentity").toString() == aIdentity) {
				return post;
			}
		}
		return nullptr;
	}

	[[nodiscard]] auto chips_of(QWidget* aPost) -> std::vector<QAbstractButton*>
	{
		auto chips = std::vector<QAbstractButton*>{ };
		for (auto* const chip : aPost->findChildren<QAbstractButton*>(QStringLiteral("reactionChip"))) {
			if (chip->isVisible()) {
				chips.push_back(chip);
			}
		}
		std::ranges::sort(chips, { }, [](const QAbstractButton* aChip) { return aChip->x(); });
		return chips;
	}

	[[nodiscard]] auto open_picker() -> QWidget*
	{
		for (auto* const widget : QApplication::topLevelWidgets()) {
			if (widget->objectName() == QStringLiteral("reactionPicker") && widget->isVisible()) {
				return widget;
			}
		}
		return nullptr;
	}

	/// [[BPR-1741]]. A post shows its reactions under its body, says
	/// who reacted, and adds or removes mine from a chip or the picker, by
	/// mouse and by keyboard alone; a post GitHub does not let me react to
	/// offers neither.
	auto test_reactions_show_and_toggle_by_mouse_and_keyboard() -> void
	{
		qt::conversation_cards_view view;
		auto                        reactions = reacting_view(view);
		auto                        asked     = std::vector<std::pair<std::string, core::reaction_content>>{ };
		view.set_reaction_dispatch([&asked](const std::string& aPost, const core::reaction_content aContent) { asked.emplace_back(aPost, aContent); });
		auto* const first = post_named(view, QStringLiteral("review-comment:C1"));
		expect(first != nullptr && chips_of(first).empty() && first->findChild<QWidget*>(QStringLiteral("threadPostReact"))->isHidden(),
			   "before reactions are read a post shows none and offers no Add reaction");
		view.present_reactions(reactions);
		QApplication::processEvents();
		const auto chips = chips_of(first);
		expect(chips.size() == 2U && chips[0]->property("reactionContent").toInt() == static_cast<int>(core::reaction_content::thumbs_up)
				   && chips[1]->property("reactionContent").toInt() == static_cast<int>(core::reaction_content::heart),
			   "the post's reactions are its chips, in GitHub's order");
		expect(chips[0]->toolTip() == QStringLiteral("someone and octocat reacted with thumbs up") && chips[1]->toolTip() == QStringLiteral("reviewer-one reacted with heart")
				   && chips[0]->accessibleName() == QStringLiteral("Thumbs up 2") && chips[1]->accessibleDescription().contains(QStringLiteral("Press to remove yours")),
			   "a chip says who reacted, me by my login as anyone, and that pressing it adds or removes mine");
		expect(chips[1]->y() > first->findChild<QWidget*>(QStringLiteral("threadPostReader"))->geometry().bottom() - first->findChild<QWidget*>(QStringLiteral("reactionBar"))->y(),
			   "the chips sit under the body");
		chips[0]->click();
		expect(asked == std::vector<std::pair<std::string, core::reaction_content>>{ { "review-comment:C1", core::reaction_content::thumbs_up } },
			   "pressing a chip asks for that reaction on that post");
		chips[1]->setFocus(Qt::TabFocusReason);
		key_click(chips[1], Qt::Key_Space);
		expect(asked.size() == 2U && asked.back().second == core::reaction_content::heart, "Space on a focused chip asks for it");

		auto* const react = first->findChild<QAbstractButton*>(QStringLiteral("threadPostReact"));
		expect(react != nullptr && !react->isHidden() && react->focusPolicy() == Qt::StrongFocus && react->accessibleName() == QStringLiteral("Add reaction"),
			   "Add reaction is among the post's tools, focusable and named");
		react->click();
		QApplication::processEvents();
		auto* picker = open_picker();
		expect(picker != nullptr, "Add reaction opens the picker");
		auto options = picker->findChildren<QAbstractButton*>(QStringLiteral("reactionPickerOption"));
		std::ranges::sort(options, { }, [](const QAbstractButton* aOption) { return aOption->x(); });
		expect(options.size() == 8U && options[5]->isChecked() && options[5]->accessibleName() == QStringLiteral("Heart, added")
				   && std::ranges::count_if(options, &QAbstractButton::isChecked) == 1 && options[5]->hasFocus(),
			   "the picker offers the eight reactions, mine checked and focused");
		key_click(options[5], Qt::Key_Right);
		expect(options[6]->hasFocus(), "an arrow moves to the next reaction");
		key_click(options[6], Qt::Key_Return);
		QApplication::processEvents();
		expect(asked.size() == 3U && asked.back() == std::pair<std::string, core::reaction_content>{ "review-comment:C1", core::reaction_content::rocket } && open_picker() == nullptr,
			   "Return chooses the focused reaction and closes the picker");
		react->click();
		QApplication::processEvents();
		picker = open_picker();
		expect(picker != nullptr, "the picker opens again");
		key_click(picker->focusWidget(), Qt::Key_Escape);
		QApplication::processEvents();
		expect(open_picker() == nullptr && asked.size() == 3U, "Esc closes the picker without asking for anything");

		auto* const second = post_named(view, QStringLiteral("review-comment:C2"));
		// [[BPR-1803]]. My post is named by my login, as anyone's is.
		const auto* const mine = second == nullptr ? nullptr : second->findChild<QWidget*>(QStringLiteral("threadPostBody"));
		expect(mine != nullptr && mine->accessibleName() == QStringLiteral("Post by reviewer-one") && second->accessibleName() == QStringLiteral("reviewer-one"),
			   "my own post is named by my GitHub login, not \"you\"");
		expect(second != nullptr && chips_of(second).empty() && second->findChild<QWidget*>(QStringLiteral("threadPostReact"))->isHidden(),
			   "a post GitHub does not let me react to offers no Add reaction");
		reactions["C2"].Groups.push_back(core::reaction_group{ .Content = core::reaction_content::eyes, .Count = 3U });
		view.present_reactions(reactions);
		QApplication::processEvents();
		expect(chips_of(second).size() == 1U && !chips_of(second).front()->isEnabled(), "and its chips show the count but cannot be pressed");

		// A change shown at once: my thumbs up pending, the count one more.
		reactions["C1"].Groups[0] = core::reaction_group{ .Content = core::reaction_content::thumbs_up, .Count = 3U, .ViewerReacted = true, .Reactors = { "reviewer-one", "someone", "octocat" }, .Pending = true };
		const auto heldCard       = QPointer<QWidget>{ view.open_card() };
		view.present_reactions(reactions);
		QApplication::processEvents();
		QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		const auto shown = chips_of(post_named(view, QStringLiteral("review-comment:C1")));
		expect(heldCard && heldCard == view.open_card() && shown.size() == 2U && shown[0]->accessibleName() == QStringLiteral("Thumbs up 3")
				   && shown[0]->toolTip() == QStringLiteral("reviewer-one, someone and octocat reacted with thumbs up, waiting for GitHub"),
			   "new counts are shown in place, without rebuilding the thread, and say they wait for GitHub");
		view.hide();
	}

	/// [[BPR-1741]]. Resolve conversation ends the thread beside Reply
	/// once resolution is offered, shows a change at once, and is not offered
	/// to a viewer GitHub does not let resolve the thread.
	auto test_resolve_conversation_beside_reply() -> void
	{
		qt::conversation_cards_view view;
		static_cast<void>(reacting_view(view));
		auto actions = std::vector<std::pair<qt::card_action, std::string>>{ };
		view.set_action_dispatch([&actions](const qt::card_action aAction, const std::string& aId) { actions.emplace_back(aAction, aId); });
		expect(button(view, "threadResolveConversation") == nullptr, "not offered before the provider and server both resolve threads");
		view.set_resolution_offered(true);
		QApplication::processEvents();
		auto* resolve = button(view, "threadResolveConversation");
		expect(resolve != nullptr && resolve->text() == QStringLiteral("Resolve conversation") && resolve->isEnabled() && resolve->focusPolicy() == Qt::StrongFocus
				   && resolve->parentWidget() == button(view, "threadReplyField")->parentWidget(),
			   "Resolve conversation sits beside Reply, focusable");
		resolve->setFocus(Qt::TabFocusReason);
		key_click(resolve, Qt::Key_Space);
		expect(actions == std::vector<std::pair<qt::card_action, std::string>>{ { qt::card_action::resolve, "review-thread:T1" } }, "pressing it, by keyboard too, asks to resolve the thread");
		view.present_thread_resolutions({ { "review-thread:T1", qt::thread_resolution_presentation{ .Resolved = true, .Pending = true } } });
		QApplication::processEvents();
		resolve = button(view, "threadResolveConversation");
		expect(view.cards().front().Resolved && view.cards().front().ResolutionPending && resolve != nullptr && !resolve->isEnabled()
				   && resolve->text() == QStringLiteral("Unresolve conversation") && view.unresolved_count() == 0U,
			   "the thread shows resolved at once, and waits for GitHub before it can be changed again");
		view.present_thread_resolutions({ { "review-thread:T1", qt::thread_resolution_presentation{ .Resolved = true } } });
		QApplication::processEvents();
		expect(button(view, "threadResolveConversation") != nullptr && button(view, "threadResolveConversation")->isEnabled(), "GitHub's answer settles it");
		view.close_thread();
		QApplication::processEvents();
		// [[BPR-2171]]. Closed and read, the resolved thread folds into its
		// file's row in Discuss; opened, the row lists it.
		if (view.cards().empty()) {
			const auto fold = std::ranges::find_if(view.timeline_lines(), [](const qt::timeline_line& aLine) { return !aLine.Fold.empty() && !aLine.Open; });
			expect(fold != view.timeline_lines().end() && fold->Folded == 1U, "closed and read, the resolved thread folds into its file's row");
			const auto key = fold->Fold;
			view.toggle_fold(key);
			QApplication::processEvents();
		}
		expect(!view.cards().empty() && !view.closed_meta(view.cards().front()).Resolved.isEmpty(), "closed, the resolved thread is one line with the green check");
		view.present_thread_resolutions({ { "review-thread:T1", qt::thread_resolution_presentation{ .Resolved = false } } });
		QApplication::processEvents();
		expect(view.closed_meta(view.cards().front()).Resolved.isEmpty() && !view.cards().front().Resolved, "reopened, it loses the check");
		view.open_thread("review-thread:T1");
		view.present_thread_resolutions({ { "review-thread:T1", qt::thread_resolution_presentation{ .Resolved = false, .CanResolve = false } } });
		QApplication::processEvents();
		expect(button(view, "threadResolveConversation") == nullptr && button(view, "threadResolve") == nullptr,
			   "a viewer GitHub does not let resolve the thread is offered neither");
		view.hide();
	}

	/// [[BPR-1741]]. Chips and the picker are drawn from the palette:
	/// mine in the accent, legible in light and dark. With a directory, the
	/// renders are kept for a look.
	auto test_reactions_render_in_light_and_dark(const QString& aDirectory) -> void
	{
		for (const auto dark : { false, true }) {
			auto palette = QApplication::palette();
			if (dark) {
				palette.setColor(QPalette::Base, QColor{ 0x0D, 0x11, 0x17 });
				palette.setColor(QPalette::Window, QColor{ 0x16, 0x1B, 0x22 });
				palette.setColor(QPalette::Text, QColor{ 0xE6, 0xED, 0xF3 });
				palette.setColor(QPalette::WindowText, QColor{ 0xE6, 0xED, 0xF3 });
				palette.setColor(QPalette::ButtonText, QColor{ 0xE6, 0xED, 0xF3 });
				palette.setColor(QPalette::Highlight, QColor{ 0x2F, 0x81, 0xF7 });
			} else {
				palette.setColor(QPalette::Base, Qt::white);
				palette.setColor(QPalette::Text, QColor{ 0x1F, 0x23, 0x28 });
				palette.setColor(QPalette::Highlight, QColor{ 0x09, 0x69, 0xDA });
			}
			qt::conversation_cards_view view;
			view.setPalette(palette);
			auto reactions = reacting_view(view);
			view.set_resolution_offered(true);
			view.present_reactions(reactions);
			QApplication::processEvents();
			auto* const post  = post_named(view, QStringLiteral("review-comment:C1"));
			const auto  chips = chips_of(post);
			expect(chips.size() == 2U, "the chips are drawn");
			const auto mine   = chips[1]->grab().toImage();
			const auto theirs = chips[0]->grab().toImage();
			const auto accent = palette.color(QPalette::Highlight);
			const auto near   = [&accent](const QImage& aImage) {
				auto found = 0;
				for (int y = 0; y < aImage.height(); ++y) {
					for (int x = 0; x < aImage.width(); ++x) {
						const auto pixel = aImage.pixelColor(x, y);
						found += std::abs(pixel.red() - accent.red()) + std::abs(pixel.green() - accent.green()) + std::abs(pixel.blue() - accent.blue()) < 90 ? 1 : 0;
					}
				}
				return found;
			};
			expect(near(mine) > near(theirs) + 4, dark ? "in dark, my reaction is drawn in the accent and another's is not" : "in light, my reaction is drawn in the accent and another's is not");
			if (!aDirectory.isEmpty()) {
				const auto suffix = dark ? QStringLiteral("dark") : QStringLiteral("light");
				view.open_card()->grab().save(aDirectory + QStringLiteral("/thread-%1.png").arg(suffix));
				auto* const react = post->findChild<QAbstractButton*>(QStringLiteral("threadPostReact"));
				react->click();
				QApplication::processEvents();
				if (auto* const picker = open_picker(); picker != nullptr) {
					picker->grab().save(aDirectory + QStringLiteral("/picker-%1.png").arg(suffix));
					picker->close();
				}
			}
			view.hide();
		}
	}
	/// The owner, 2026-11-10: "It's hard to read comments' line number
	/// against the background it's on in dark theme." The open card's line
	/// button reads on the card in every built-in theme: a published
	/// thread's number, and my draft's "Line 40", which was drawn in the
	/// selection colour (`Highlight`) and measured 2.4:1 in Dark. The
	/// draft's "Draft" beside it is held to the same floor. With a
	/// directory, each theme's open thread and open draft are drawn there.
	///
	/// The owner, later the same day, on macOS in Dark: "Line 329" was blue
	/// on a light grey button face. The macOS style gives a plain tool
	/// button a native bezel in the system appearance's grey, which the
	/// offscreen renders in the default style never showed. So the ink is
	/// measured against the face the button is actually painted on, inside
	/// the card as drawn, at rest, hovered, pressed and with keyboard focus,
	/// under the default style and, where the platform has it, macOS.
	///
	/// The card's icon tools had the same native bezel: Resolve, Ask agent,
	/// Mark read, More, Show in Discuss, Maximize, Close and the composer's
	/// Delete were pale squares under a dark profile on a light system. Each
	/// mark is held to the non-text floor on its painted face in the same
	/// states, and the defect badge's words to the text floor.
	auto test_line_numbers_read_on_the_open_card(const QString& aDirectory) -> void
	{
		const auto modes = std::array{
			std::pair{ "light", core::appearance_mode::light },
			std::pair{ "dark", core::appearance_mode::dark },
			std::pair{ "high-contrast-light", core::appearance_mode::high_contrast_light },
			std::pair{ "high-contrast-dark", core::appearance_mode::high_contrast_dark },
		};
		const auto ratio = [](const QColor& aOne, const QColor& aOther) {
			return static_cast<double>(core::contrast_ratio_hundredths(qt::to_appearance_color(aOne), qt::to_appearance_color(aOther))) / 100.0;
		};
		// The ground is the commonest colour in the image, and the ink the
		// pixel that differs from it most.
		const auto measureImage = [&ratio](const QImage& aImage) {
			auto counts = std::map<QRgb, int>{ };
			for (auto y = 0; y < aImage.height(); ++y) {
				for (auto x = 0; x < aImage.width(); ++x) {
					++counts[aImage.pixel(x, y)];
				}
			}
			const auto ground = QColor{ std::ranges::max_element(counts, { }, &std::map<QRgb, int>::value_type::second)->first };
			auto       ink    = ground;
			for (auto y = 0; y < aImage.height(); ++y) {
				for (auto x = 0; x < aImage.width(); ++x) {
					if (ratio(aImage.pixelColor(x, y), ground) > ratio(ink, ground)) {
						ink = aImage.pixelColor(x, y);
					}
				}
			}
			return std::pair{ ground, ink };
		};
		const auto measure = [&measureImage](QWidget& aWidget) {
			return measureImage(aWidget.grab().toImage());
		};
		// The button as the card paints it: its face, whatever the style drew
		// there, and its words, inside the focus ring's room.
		const auto measureOnCard = [&measureImage](QWidget& aCard, QToolButton& aButton) {
			const auto inset = static_cast<int>(std::ceil(qt::focus_ring_width)) + 1;
			const auto place = QRect{ aButton.mapTo(&aCard, QPoint{ 0, 0 }), aButton.size() }.adjusted(inset, inset, -inset, -inset);
			const auto image = aCard.grab().toImage();
			const auto scale = image.devicePixelRatio();
			return measureImage(image.copy(QRectF{ place.x() * scale, place.y() * scale, place.width() * scale, place.height() * scale }.toAlignedRect()));
		};
		// Every state a person sees the button in; each ends as it began.
		// `aWhat` says what is measured: the line's words, or a tool's mark.
		const auto statesRead = [&](QWidget& aCard, QToolButton& aButton, const std::string& aName, const double aFloor, const std::string_view aWhat = "line-number") {
			const auto check = [&](const std::string_view aState) {
				aButton.update();
				QApplication::processEvents();
				const auto [ground, ink] = measureOnCard(aCard, aButton);
				std::cout << aWhat << ' ' << aName << ' ' << aState << ": ground " << ground.name().toStdString() << " ink " << ink.name().toStdString() << " ratio "
						  << ratio(ink, ground) << '\n';
				expect(ratio(ink, ground) + 0.005 >= aFloor, aName + " " + std::string{ aState } + ": it reaches the theme's floor on the face it is painted on");
			};
			check("at rest");
			aButton.setAttribute(Qt::WA_UnderMouse, true);
			check("hovered");
			aButton.setDown(true);
			check("pressed");
			aButton.setDown(false);
			aButton.setAttribute(Qt::WA_UnderMouse, false);
			aButton.clearFocus();
			const auto unfocused = aButton.grab().toImage();
			aButton.setFocus(Qt::TabFocusReason);
			check("with keyboard focus");
			expect(aButton.hasFocus() || QGuiApplication::platformName() != QStringLiteral("offscreen"), aName + ": it takes keyboard focus");
			if (aButton.hasFocus()) {
				expect(aButton.grab().toImage() != unfocused, aName + ": keyboard focus on it is drawn");
			}
			aButton.clearFocus();
		};
		// The card's tools: each named, offered, and its mark held to the
		// non-text floor on its face. A disabled tool is not measured.
		const auto toolsRead = [&](QWidget& aCard, const std::vector<QString>& aNames, const std::string& aName, const double aFloor) {
			for (const auto& objectName : aNames) {
				auto* const tool = aCard.findChild<QToolButton*>(objectName);
				const auto  name = aName + " " + objectName.toStdString();
				expect(tool != nullptr && tool->isVisibleTo(&aCard), name + ": the card offers it");
				if (tool == nullptr || !tool->isEnabled()) {
					continue;
				}
				expect(!tool->accessibleName().isEmpty() && !tool->toolTip().isEmpty() && tool->focusPolicy() == Qt::StrongFocus, name + ": it is named, explained and takes focus");
				statesRead(aCard, *tool, name, aFloor, "card-tool");
			}
		};
		// A style, the system appearance it is run under (unset: whatever the
		// platform says), and the name its results and pictures carry.
		struct look
		{
			QString         Style;
			Qt::ColorScheme Scheme = Qt::ColorScheme::Unknown;
			QString         Key;
		};
		const auto original = QApplication::style()->name();
		const auto macOS    = original.compare(QStringLiteral("macOS"), Qt::CaseInsensitive) == 0;
		// Appended, not a ?: of braced vectors, which GCC 15 misreads as dangling (BPR-2215).
		auto looks = std::vector<look>{ };
		if (!macOS) {
			looks.push_back(look{ .Style = original, .Key = original.toLower() });
		}
		if (macOS || QStyleFactory::keys().contains(QStringLiteral("macOS"), Qt::CaseInsensitive)) {
			// The macOS style paints in the system's appearance, not the
			// profile's, so a dark profile meets a light system and the
			// reverse: both are run. Under the offscreen plugin it paints no
			// native controls at all; under cocoa it paints them as the
			// application does.
			looks.push_back(look{ .Style = QStringLiteral("macOS"), .Scheme = Qt::ColorScheme::Light, .Key = QStringLiteral("macos-system-light") });
			looks.push_back(look{ .Style = QStringLiteral("macOS"), .Scheme = Qt::ColorScheme::Dark, .Key = QStringLiteral("macos-system-dark") });
		}
		for (const auto& look : looks) {
			QApplication::setStyle(QStyleFactory::create(look.Style));
			expect(QApplication::style()->name().compare(look.Style, Qt::CaseInsensitive) == 0, look.Style.toStdString() + ": the style is in use");
			if (look.Scheme != Qt::ColorScheme::Unknown) {
				QGuiApplication::styleHints()->setColorScheme(look.Scheme);
			}
			const auto& styleKey = look.Key;
			for (const auto& [modeName, mode] : modes) {
				const auto name    = styleKey.toStdString() + " " + std::string{ modeName };
				auto       profile = qt::appearance_profile{ };
				static_cast<void>(profile.select_mode(mode));
				const auto                  floor     = static_cast<double>(core::minimum_contrast_hundredths(core::contrast_kind::text, profile.variant())) / 100.0;
				const auto                  indicator = static_cast<double>(core::minimum_contrast_hundredths(core::contrast_kind::indicator, profile.variant())) / 100.0;
				qt::conversation_cards_view view;
				view.setPalette(profile.derive(QApplication::palette()));
				// The window's own two: the theme's unread blue and the marks' amber.
				view.set_colours(profile.color(core::appearance_role::state_unread), QColor{ 0xD2, 0x99, 0x22 });
				// And the theme the conversation panel gives its cards.
				view.set_code_theme({ .Colors = qt::code_role_colors_of(profile.palette()), .Variant = profile.variant() });
				view.resize(440, 760);
				// Never a window on a desktop: the native macOS style draws
				// only under the cocoa plugin, which is run by hand. There no
				// window becomes active, so keyboard focus is tried offscreen.
				view.setAttribute(Qt::WA_DontShowOnScreen, QGuiApplication::platformName() != QStringLiteral("offscreen"));
				view.show();
				// Every header tool a thread can show: Resolve, Ask agent, an
				// enabled Mark read (one post is new), More and Close, and
				// the defect badge's words.
				view.set_resolution_offered(true);
				view.adopt({ thread("t", "src/a.cpp", 10U), reply("c", "t", "someone", "Please rename this.") });
				view.set_read_state({ qt::thread_read_state{ .Thread = "t", .UnreadPosts = { "c" } } });
				view.set_defect_badges({ { "t", { QStringLiteral("[open] minor · correctness"), QStringLiteral("Nobody has addressed this yet.") } } });
				view.open_thread("t");
				QApplication::processEvents();
				auto* const published = view.open_card() == nullptr ? nullptr : view.open_card()->findChild<QToolButton*>(QStringLiteral("threadShowLine"));
				expect(published != nullptr && published->text() == QStringLiteral("10"), name + ": the open thread shows its line");
				statesRead(*view.open_card(), *published, name + " thread", floor);
				toolsRead(*view.open_card(), { QStringLiteral("threadResolve"), QStringLiteral("threadMarkRead"), QStringLiteral("threadMore"), QStringLiteral("threadClose") },
						  name + " thread", indicator);
				auto* const markRead = view.open_card()->findChild<QToolButton*>(QStringLiteral("threadMarkRead"));
				expect(markRead != nullptr && markRead->isEnabled(), name + ": a thread with a new post can be marked read");
				auto* const badge = view.open_card()->findChild<QToolButton*>(QStringLiteral("threadDefectBadge"));
				expect(badge != nullptr, name + ": the classified thread shows its badge");
				if (badge != nullptr) {
					statesRead(*view.open_card(), *badge, name + " thread threadDefectBadge", floor, "card-tool");
				}
				if (!aDirectory.isEmpty()) {
					expect(view.open_card()->grab().save(aDirectory + QStringLiteral("/open-thread-%1-%2.png").arg(QLatin1StringView{ modeName }, styleKey)),
						   name + ": capture the open thread");
					auto* const more = view.open_card()->findChild<QToolButton*>(QStringLiteral("threadMore"));
					more->setAttribute(Qt::WA_UnderMouse, true);
					markRead->setDown(true);
					more->update();
					QApplication::processEvents();
					expect(view.open_card()->grab().save(aDirectory + QStringLiteral("/open-thread-tools-hover-press-%1-%2.png").arg(QLatin1StringView{ modeName }, styleKey)),
						   name + ": capture More hovered and Mark read pressed");
					markRead->setDown(false);
					more->setAttribute(Qt::WA_UnderMouse, false);
				}

				const auto comparison = core::file_comparison_digest{ };
				view.set_draft(core::review_comment_draft{
								   .Anchor = core::source_line_ref{ .Comparison = comparison, .Side = core::diff_side::right, .Line = 39U },
								   .Body   = "Check the default before publishing.",
							   },
							   comparison);
				QApplication::processEvents();
				const auto draft = std::ranges::find_if(view.cards(), &qt::thread_card::Draft);
				expect(draft != view.cards().end(), name + ": my draft is a note");
				const auto draftId = draft->Id;
				view.open_thread(draftId);
				QApplication::processEvents();
				auto* const writing = view.open_card() == nullptr ? nullptr : view.open_card()->findChild<QToolButton*>(QStringLiteral("threadShowLine"));
				expect(writing != nullptr && writing->text() == QStringLiteral("Line 40"), name + ": the open draft names its line");
				expect(writing->accessibleName() == QStringLiteral("Show line 40") && writing->focusPolicy() == Qt::StrongFocus, name + ": the line is named and takes focus");
				// Space on the focused line shows it, as a click does.
				auto shown = 0;
				QObject::connect(writing, &QToolButton::clicked, writing, [&shown] { ++shown; });
				writing->setFocus(Qt::TabFocusReason);
				auto press   = QKeyEvent{ QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" ") };
				auto release = QKeyEvent{ QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" ") };
				QApplication::sendEvent(writing, &press);
				QApplication::sendEvent(writing, &release);
				expect(shown == 1, name + ": Space on the focused line shows it");
				QObject::disconnect(writing, &QToolButton::clicked, writing, nullptr);
				writing->clearFocus();
				if (!aDirectory.isEmpty()) {
					QApplication::processEvents();
					expect(view.open_card()->grab().save(aDirectory + QStringLiteral("/open-draft-%1-%2.png").arg(QLatin1StringView{ modeName }, styleKey)),
						   name + ": capture the open draft");
					writing->setFocus(Qt::TabFocusReason);
					QApplication::processEvents();
					expect(view.open_card()->grab().save(aDirectory + QStringLiteral("/open-draft-focus-%1-%2.png").arg(QLatin1StringView{ modeName }, styleKey)),
						   name + ": capture the open draft's focused line");
					writing->setAttribute(Qt::WA_UnderMouse, true);
					writing->update();
					QApplication::processEvents();
					expect(view.open_card()->grab().save(aDirectory + QStringLiteral("/open-draft-hover-%1-%2.png").arg(QLatin1StringView{ modeName }, styleKey)),
						   name + ": capture the open draft's hovered line");
					writing->setAttribute(Qt::WA_UnderMouse, false);
					writing->clearFocus();
				}
				statesRead(*view.open_card(), *writing, name + " draft", floor);
				// The draft's Maximize (expand; owner look v005: one way to
				// Discuss, not two), Close (collapse), and the composer's Delete.
				toolsRead(*view.open_card(), { QStringLiteral("threadMaximizeDraft"), QStringLiteral("threadClose"), QStringLiteral("threadDeleteDraft") },
						  name + " draft", indicator);
				if (!aDirectory.isEmpty()) {
					auto* const trash = view.open_card()->findChild<QToolButton*>(QStringLiteral("threadDeleteDraft"));
					auto* const close = view.open_card()->findChild<QToolButton*>(QStringLiteral("threadClose"));
					trash->setDown(true);
					close->setAttribute(Qt::WA_UnderMouse, true);
					close->update();
					QApplication::processEvents();
					expect(view.open_card()->grab().save(aDirectory + QStringLiteral("/open-draft-tools-hover-press-%1-%2.png").arg(QLatin1StringView{ modeName }, styleKey)),
						   name + ": capture Close hovered and Delete pressed");
					close->setAttribute(Qt::WA_UnderMouse, false);
					trash->setDown(false);
				}
				auto* const state = view.open_card()->findChild<QWidget*>(QStringLiteral("threadState"));
				expect(state != nullptr, name + ": the open draft says it is a draft");
				const auto [stateGround, stateInk] = measure(*state);
				std::cout << "line-number " << name << " draft state: ground " << stateGround.name().toStdString() << " ink " << stateInk.name().toStdString() << " ratio "
						  << ratio(stateInk, stateGround) << '\n';
				expect(ratio(stateInk, stateGround) + 0.005 >= floor, name + ": my draft's \"Draft\" reaches the theme's text floor on the card");
				{
					// A kept reply's Copy text, a moved suggestion's Copy
					// change and a saved draft's Edit are opened from the
					// window, not in place here. They are the card's quiet
					// button with words beside the mark, on the card's colour,
					// measured as the card builds them.
					auto host = QWidget{ };
					host.setPalette(view.palette());
					host.setBackgroundRole(QPalette::Base);
					host.setAutoFillBackground(true);
					host.setAttribute(Qt::WA_DontShowOnScreen, QGuiApplication::platformName() != QStringLiteral("offscreen"));
					auto* const words = new qt::quiet_tool_button{ &host };
					words->set_surface(QPalette::Base);
					words->setAutoRaise(true);
					words->setText(QStringLiteral("Copy text"));
					words->setToolTip(QStringLiteral("Copy the reply as written"));
					words->setAccessibleName(words->text());
					auto mark = QPixmap{ 16, 16 };
					mark.fill(Qt::transparent);
					{
						auto painter = QPainter{ &mark };
						painter.fillRect(QRect{ 4, 3, 8, 10 }, view.palette().color(QPalette::Active, QPalette::ButtonText));
					}
					words->setIcon(QIcon{ mark });
					words->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
					words->setFocusPolicy(Qt::StrongFocus);
					words->move(8, 8);
					words->resize(words->sizeHint());
					host.resize(words->width() + 16, words->height() + 16);
					host.show();
					QApplication::processEvents();
					statesRead(host, *words, name + " foot threadCopyReply", floor, "card-tool");
				}
				view.hide();
			}
		}
		QGuiApplication::styleHints()->unsetColorScheme();
		QApplication::setStyle(QStyleFactory::create(original));
	}

	/// [[BPR-1803]]. The owner, 2026-10-28: "I don't like the comments or
	/// windows saying "you" id rather just always have our GitHub names like
	/// everyone else". My thread, my reply and my unpublished note are named
	/// by my login; with no login known, a note is an unknown author's, as
	/// other surfaces say, never "you". With a directory, the light list and
	/// my open thread are drawn there.
	auto test_my_posts_are_named_by_my_login(const QString& aDirectory) -> void
	{
		qt::conversation_cards_view view;
		view.resize(420, 640);
		view.show();
		QApplication::processEvents();
		auto lineY = std::map<std::uint32_t, int>{ { 10U, 80 }, { 30U, 300 } };
		view.set_line_locator([&lineY](core::diff_side, const std::uint32_t aLine) -> std::optional<int> {
			const auto found = lineY.find(aLine);
			return found == lineY.end() ? std::nullopt : std::optional{ found->second };
		});
		view.set_anchor_file(core::file_identity_of("src/a.cpp"));
		view.set_viewer_login(QStringLiteral("reviewer-one"));
		view.adopt({ thread("review-thread:T1", "src/a.cpp", 10U), reply("review-comment:C1", "review-thread:T1", "reviewer-one", "Should this cache be per frame?"),
					 reply("review-comment:C2", "review-comment:C1", "someone", "Per frame, yes."), reply("review-comment:C3", "review-comment:C2", "reviewer-one", "Thanks, resolving.") });
		auto note   = core::review_comment_draft{ };
		note.Id     = core::review_draft_id{ 5U };
		note.Anchor = core::source_line_ref{ .Comparison = core::file_comparison_digest{ }, .Side = core::diff_side::right, .Line = 29U };
		note.Body   = "nit: this could be constexpr.";
		view.set_draft(std::nullopt, core::file_comparison_digest{ });
		view.set_saved_drafts({ note });
		QApplication::processEvents();
		const auto cardOn = [&view](const std::uint32_t aLine, const bool aDraft) {
			return std::ranges::find_if(view.cards(), [aLine, aDraft](const qt::thread_card& aCard) { return aCard.Draft == aDraft && aCard.Line == std::optional{ aLine }; });
		};
		const auto mine  = cardOn(10U, false);
		const auto draft = cardOn(30U, true);
		expect(mine != view.cards().end() && mine->Posts.size() == 3U && mine->Posts.front().Mine && mine->Posts.front().Author == QStringLiteral("reviewer-one")
				   && mine->Posts.back().Author == QStringLiteral("reviewer-one"),
			   "my thread and my reply are named by my login");
		expect(draft != view.cards().end() && !draft->Posts.empty() && draft->Posts.front().Author == QStringLiteral("reviewer-one"), "my unpublished note is named by my login");
		if (!aDirectory.isEmpty()) {
			view.grab().save(aDirectory + QStringLiteral("/list-light.png"));
		}
		view.open_thread("review-thread:T1");
		QApplication::processEvents();
		auto* const post = post_named(view, QStringLiteral("review-comment:C3"));
		expect(post != nullptr && post->accessibleName() == QStringLiteral("reviewer-one"), "my open reply is named by my login");
		if (!aDirectory.isEmpty() && view.open_card() != nullptr) {
			view.open_card()->grab().save(aDirectory + QStringLiteral("/open-thread-light.png"));
		}
		view.set_viewer_login({ });
		view.set_saved_drafts({ note });
		QApplication::processEvents();
		const auto unknown = cardOn(30U, true);
		expect(unknown != view.cards().end() && !unknown->Posts.empty() && unknown->Posts.front().Author.isEmpty(),
			   "with no login known my note carries no login, so it is drawn as an unknown author's, not \"you\"");
		if (!aDirectory.isEmpty()) {
			view.close_thread();
			QApplication::processEvents();
			view.grab().save(aDirectory + QStringLiteral("/list-no-login-light.png"));
		}
		view.hide();
	}

	/// [[BPR-1779]]. The owner, 2026-10-27: "to me the review is just a
	/// bundle of comments posted together and what really matters is the
	/// comments themselves on the files and lines". #186's shape, in the Pull
	/// request view and in Discuss over the whole pull request.
	void test_conversation_lists_comments_not_reviews()
	{
		using kind = qt::timeline_line::kind;
		for (const auto discuss : { false, true }) {
			qt::conversation_cards_view view;
			view.resize(420, 900);
			view.set_resolution_offered(true);
			view.set_excerpt_locator([](const QString& aPath, core::diff_side, const std::uint32_t aLine) -> std::optional<QString> {
				return aPath.endsWith(QStringLiteral(".cpp")) ? std::optional{ QStringLiteral("%1     m_Loader.Shutdown();").arg(aLine) } : std::nullopt;
			});
			if (discuss) {
				view.set_feed(true);
				view.set_scope(qt::cards_scope::whole_pull_request);
			}
			auto actions = std::vector<std::pair<qt::card_action, std::string>>{ };
			view.set_action_dispatch([&actions](const qt::card_action aAction, const std::string& aId) { actions.emplace_back(aAction, aId); });
			view.adopt(test::owner_186_forest(), core::file_identity_of("Client/AppCore/src/Instance.cpp"));
			view.set_read_state({ { .Thread = "review-thread:cpp", .UnreadPosts = { "review-comment:cpp-reply" } }, { .Thread = "review:csw", .UnreadPosts = { "review:csw" } } });
			view.show();
			QApplication::processEvents();
			if (!discuss) {
				view.set_pull_request_link_dispatch([](const std::string&) { });
				expect(view.pull_request_comments_chip()->accessibleName() == QStringLiteral("Pull request conversation: 2 comments, 1 new"),
					   "the chip counts the two file comments, not the six reviews, and the unread approval is no comment");
				view.show_pull_request_comments(true);
				QApplication::processEvents();
			}
			const auto where = std::string{ discuss ? "Discuss: " : "Pull request view: " };
			expect(view.timeline(), where + "reads in time order");
			auto ids = std::vector<std::string>{ };
			for (const auto& card : view.cards()) {
				ids.push_back(card.Id);
			}
			expect(ids == std::vector<std::string>{ "review-thread:h", "review-thread:cpp" },
				   where + "the no-body review's two file comments are the conversation's comments, and no review is a card");
			expect(view.thread_count() == 2U && std::ranges::all_of(view.cards(), [](const qt::thread_card& aCard) { return aCard.Bundle == "review:dos"; }),
				   where + "the counts count the comments, each knowing the review it was posted in");
			auto lines = std::vector<std::pair<kind, std::string>>{ };
			for (const auto& line : view.timeline_lines()) {
				lines.emplace_back(line.Kind, line.Kind == kind::path ? line.Path.toStdString() : line.Id);
			}
			// The Pull request view's New rule stands over the first comment
			// with a post not yet read.
			const auto fresh    = std::vector<std::pair<kind, std::string>>{ { kind::fresh, "" } };
			const auto expected = std::vector<std::pair<kind, std::string>>{
				{ kind::bundle, "review:dos" },
				{ kind::path, "Client/AppCore/include/AppCore/Instance.h" },
			};
			const auto tail = std::vector<std::pair<kind, std::string>>{
				{ kind::path, "Client/AppCore/src/Instance.cpp" },
				{ kind::verdict, "review:csw" },
				{ kind::verdict, "review:sau" },
			};
			auto whole = expected;
			if (!discuss) {
				whole.insert(whole.end(), fresh.begin(), fresh.end());
			}
			whole.insert(whole.end(), tail.begin(), tail.end());
			expect(lines == whole, where + "one quiet line over the bundle, each thread under its file, the approvals as compact lines, and nothing for the body-less Commented reviews");
			const auto& bundle = view.timeline_lines().front();
			expect(bundle.Author == QStringLiteral("dosullivan") && bundle.Verdict.isEmpty() && !bundle.When.isEmpty(), where + "a Commented bundle names who and when, and no verdict");
			const auto& approval = view.timeline_lines()[discuss ? 3U : 4U];
			expect(approval.Author == QStringLiteral("cswiedler") && approval.Verdict == QStringLiteral("Approved"), where + "an approval stays a compact line with whoever gave it");
			expect(bundle.Reach == view.cards().back().Rect.bottom(), where + "the bundle's rail runs down to its last comment");
			// Open, read, reply and resolve act on the thread itself.
			view.open_thread("review-thread:cpp");
			QApplication::processEvents();
			expect(!actions.empty() && actions.back() == std::pair{ qt::card_action::mark_read, std::string{ "review-thread:cpp" } }, where + "opening the comment reads its own thread");
			auto* const excerpt = view.open_card()->findChild<QLabel*>(QStringLiteral("threadExcerpt"));
			expect(excerpt != nullptr && excerpt->text().contains(QStringLiteral("m_Loader.Shutdown();")), where + "the open comment shows the lines it is on");
			auto* const open = button(view, "threadShowLine");
			expect(open != nullptr && open->text() == QStringLiteral("Open in Review"), where + "and offers Open in Review");
			open->click();
			expect(actions.back() == std::pair{ qt::card_action::open_in_code, std::string{ "review-thread:cpp" } }, where + "which opens its thread in Review");
			expect(view.open_card()->findChildren<QWidget*>(QStringLiteral("threadPost")).size() == 2U, where + "the comment shows with its reply");
			for (auto* const label : view.findChildren<QLabel*>()) {
				expect(!label->text().startsWith(QStringLiteral("No overall review comment")), where + "no empty-review notice stands where the comments are shown");
			}
			button(view, "threadReplyField")->click();
			expect(actions.back() == std::pair{ qt::card_action::reply, std::string{ "review-thread:cpp" } }, where + "Reply answers the thread");
			button(view, "threadResolveConversation")->click();
			expect(actions.back() == std::pair{ qt::card_action::resolve, std::string{ "review-thread:cpp" } }, where + "Resolve resolves the thread");
			view.close_thread();
			if (discuss) {
				// The filters act on comments: verdict lines are not comments.
				view.set_filters({ .UnreadOnly = true });
				QApplication::processEvents();
				expect(view.cards().size() == 1U && view.cards().front().Id == "review-thread:cpp" && view.thread_count() == 1U, "Unread keeps the unread comment and counts it");
				expect(view.timeline_lines().size() == 2U && view.timeline_lines().front().Kind == kind::bundle, "under its review's line, with no verdict lines");
				auto resolved                                                                              = test::owner_186_forest();
				std::ranges::find(resolved, std::string{ "review-thread:h" }, &conversation::Id)->Resolved = true;
				view.set_filters({ .UnresolvedOnly = true });
				view.adopt(resolved);
				QApplication::processEvents();
				expect(view.cards().size() == 1U && view.cards().front().Id == "review-thread:cpp", "Hide resolved hides the resolved comment");
				view.set_filters({ });
				view.set_scope(qt::cards_scope::this_file);
				QApplication::processEvents();
				expect(!view.timeline() && view.timeline_lines().empty() && view.cards().size() == 1U, "one file keeps its file-and-line order, without review lines");
			}
			view.hide();
		}
	}

	/// [[BPR-1779]]. A review's overall body is one more comment in its
	/// bundle: Approved is named on the bundle's line, not on the body.
	void test_a_review_body_is_a_comment_in_its_bundle()
	{
		qt::conversation_cards_view view;
		view.resize(420, 700);
		view.set_feed(true);
		view.set_scope(qt::cards_scope::whole_pull_request);
		auto review        = conversation{ };
		review.Id          = "review:said";
		review.Kind        = application::github_conversation_kind::review;
		review.AuthorLogin = "jiayin";
		review.Body        = "Looks right to me.";
		review.ReviewState = application::github_review_state_projection::approved;
		review.CreatedAt   = "2026-10-24T00:00:00Z";
		auto said          = thread("review-thread:said", "src/a.cpp", 4U);
		auto post          = reply("review-comment:said", said.Id, "jiayin", "nit: a name");
		post.ReviewId      = review.Id;
		post.CreatedAt     = review.CreatedAt;
		auto issue         = conversation{ .Id = "issue-comment:1", .Kind = application::github_conversation_kind::issue_comment, .AuthorLogin = "author", .Body = "Thanks", .CreatedAt = "2026-10-25T00:00:00Z" };
		view.adopt({ issue, said, post, review });
		view.show();
		QApplication::processEvents();
		expect(view.cards().size() == 3U && view.cards()[0].Id == review.Id && view.cards()[1].Id == said.Id && view.cards()[2].Id == issue.Id,
			   "the body, then the review's thread, then the later pull request comment");
		expect(view.cards()[0].Verdict.isEmpty() && view.cards()[0].Posts.front().Body == QStringLiteral("Looks right to me."), "the body is its own words, the verdict left to the line");
		expect(view.timeline_lines().front().Kind == qt::timeline_line::kind::bundle && view.timeline_lines().front().Verdict == QStringLiteral("Approved"), "Approved is named on the bundle's line");
		expect(view.cards()[2].Bundle.empty(), "a pull request comment belongs to no bundle");
		view.hide();
	}

	/// [[BPR-1779]]. The light or dark palette the renders use.
	[[nodiscard]] auto render_palette(const bool aDark) -> QPalette
	{
		auto palette = QApplication::palette();
		if (aDark) {
			palette.setColor(QPalette::Base, QColor{ 0x0D, 0x11, 0x17 });
			palette.setColor(QPalette::Window, QColor{ 0x16, 0x1B, 0x22 });
			palette.setColor(QPalette::Text, QColor{ 0xE6, 0xED, 0xF3 });
			palette.setColor(QPalette::WindowText, QColor{ 0xE6, 0xED, 0xF3 });
			palette.setColor(QPalette::ButtonText, QColor{ 0xE6, 0xED, 0xF3 });
			palette.setColor(QPalette::Button, QColor{ 0x21, 0x26, 0x2D });
			palette.setColor(QPalette::PlaceholderText, QColor{ 0x8B, 0x94, 0x9E });
			palette.setColor(QPalette::Highlight, QColor{ 0x2F, 0x81, 0xF7 });
		} else {
			palette.setColor(QPalette::Base, Qt::white);
			palette.setColor(QPalette::Text, QColor{ 0x1F, 0x23, 0x28 });
			palette.setColor(QPalette::Highlight, QColor{ 0x09, 0x69, 0xDA });
		}
		return palette;
	}

	/// [[BPR-1779]]. Offscreen renders of #186's shape in the drawer's Pull
	/// request view and in Discuss, closed and with the first comment open,
	/// light and dark: `<dir>/<phase>-<view>-<theme>.png`.
	auto render_owner_186(const QString& aDirectory, const QString& aPhase) -> void
	{
		const auto forest = test::owner_186_forest();
		for (const auto dark : { false, true }) {
			const auto theme = dark ? QStringLiteral("dark") : QStringLiteral("light");
			for (const auto discuss : { false, true }) {
				qt::conversation_cards_view view;
				view.setPalette(render_palette(dark));
				view.resize(discuss ? 520 : 380, 820);
				view.set_pull_request_author(QStringLiteral("jmarcin"));
				view.set_excerpt_locator([](const QString& aPath, core::diff_side, const std::uint32_t aLine) -> std::optional<QString> {
					return aPath.endsWith(QStringLiteral(".h")) ? QStringLiteral("%1     std::string m_Name;").arg(aLine)
																: QStringLiteral("%1     m_Loader.Shutdown();").arg(aLine);
				});
				view.set_resolution_offered(true);
				if (discuss) {
					view.set_feed(true);
					view.set_scope(qt::cards_scope::whole_pull_request);
				}
				view.adopt(forest, core::file_identity_of("Client/AppCore/src/Instance.cpp"));
				view.show();
				QApplication::processEvents();
				if (!discuss) {
					view.show_pull_request_comments(true);
					QApplication::processEvents();
				}
				const auto name = discuss ? QStringLiteral("discuss") : QStringLiteral("pr-view");
				view.grab().save(aDirectory + QStringLiteral("/%1-%2-%3.png").arg(aPhase, name, theme));
				const auto first = std::ranges::find_if(view.cards(), [](const qt::thread_card& aCard) { return aCard.Id == "review:dos" || aCard.Id == "review-thread:cpp"; });
				if (first != view.cards().end()) {
					view.open_thread(first->Id);
					QApplication::processEvents();
					view.grab().save(aDirectory + QStringLiteral("/%1-%2-open-%3.png").arg(aPhase, name, theme));
				}
				view.hide();
			}
		}
	}
} // namespace

namespace {
	/// [[BPR-1775]]. A thread with someone's comment and my reply (CRLF in
	/// its body), and my pull request comment.
	void editing_view(qt::conversation_cards_view& aView)
	{
		aView.resize(440, 900);
		aView.show();
		QApplication::processEvents();
		aView.set_viewer_login(QStringLiteral("reviewer-one"));
		auto mine = conversation{ .Id = "issue-comment:IC1", .Kind = application::github_conversation_kind::issue_comment, .AuthorLogin = "reviewer-one", .Body = "My pull request comment." };
		aView.adopt({ thread("review-thread:T1", "src/a.cpp", 10U), reply("review-comment:C1", "review-thread:T1", "someone", "Please rename this."),
					  reply("review-comment:C2", "review-comment:C1", "reviewer-one", "Done.\r\nRenamed."), mine });
		aView.set_feed(true);
		aView.set_scope(qt::cards_scope::whole_pull_request);
		aView.open_thread("review-thread:T1");
		QApplication::processEvents();
	}

	[[nodiscard]] auto edit_menu_of(QWidget* aPost) -> QMenu*
	{
		auto* const reader   = static_cast<qt::markdown_body_view*>(aPost->findChild<QWidget*>(QStringLiteral("threadPostReader")));
		auto* const viewport = reader->rendered_view()->viewport();
		auto        context  = QContextMenuEvent{ QContextMenuEvent::Mouse, QPoint{ 5, 5 }, viewport->mapToGlobal(QPoint{ 5, 5 }) };
		QApplication::sendEvent(viewport, &context);
		QApplication::processEvents();
		return reader->findChild<QMenu*>(QStringLiteral("markdownReaderContextMenu"));
	}

	void press_in(QWidget* aWidget, const int aKey, const Qt::KeyboardModifiers aModifiers)
	{
		auto overrideEvent = QKeyEvent{ QEvent::ShortcutOverride, aKey, aModifiers };
		QApplication::sendEvent(aWidget, &overrideEvent);
		auto press = QKeyEvent{ QEvent::KeyPress, aKey, aModifiers };
		QApplication::sendEvent(aWidget, &press);
		QApplication::processEvents();
	}

	/// [[BPR-1775]]. Edit is offered on my own published comments alone, in
	/// the post's tools and its body menu; the editor takes the body's place
	/// on GitHub's exact bytes; Save (⌘↩ too) is a gesture with the original;
	/// Esc and Cancel close it and send nothing; while GitHub is asked the
	/// text is held; a conflict keeps the typed text and offers GitHub's
	/// newer text; a withdrawn offer closes it.

	/// \brief Runs the event loop long enough for the card's deferred refits.
	void settle_card()
	{
		for (auto spin = 0; spin < 30; ++spin) {
			QApplication::processEvents();
			QThread::msleep(2);
		}
	}

	/// \brief Opens my draft on line 1 of `src/a.cpp` as its composer.
	void open_composer(qt::conversation_cards_view& aView, const std::string& aBody)
	{
		aView.set_line_locator([](core::diff_side, const std::uint32_t aLine) -> std::optional<int> { return aLine == 1U ? std::optional{ 40 } : std::nullopt; });
		aView.set_anchor_file(core::file_identity_of("src/a.cpp"));
		aView.adopt({ });
		const auto comparison = core::file_comparison_digest{ };
		aView.set_draft(core::review_comment_draft{ .Id = core::review_draft_id{ 1U }, .Anchor = core::source_line_ref{ .Comparison = comparison, .Side = core::diff_side::right, .Line = 0U }, .Body = aBody },
						comparison);
		QApplication::processEvents();
		const auto draft = std::ranges::find_if(aView.cards(), [](const qt::thread_card& aCard) { return aCard.Draft; });
		expect(draft != aView.cards().end(), "my draft has a note");
		aView.open_thread(draft->Id);
		settle_card();
	}

	[[nodiscard]] auto wheel_at(QWidget& aTarget, const QPoint aPixels, const QPoint aAngle) -> QWheelEvent
	{
		const auto centre = QPointF{ aTarget.rect().center() };
		return QWheelEvent{ centre, aTarget.mapToGlobal(centre), aPixels, aAngle, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false };
	}

	/// \brief [[BPR-2271]]. The owner: "Suggested changes diffs in the
	/// comments drawer should scroll like code blocks there do". What a
	/// suggestion changes keeps its long line whole and scrolls sideways in
	/// its own box; the card stays the drawer's width; a sideways wheel and
	/// Left, Right, Home and End move it, and a vertical wheel is the drawer's.
	auto test_a_suggested_change_scrolls_sideways_in_its_box(const QString& aDirectory) -> void
	{
		qt::conversation_cards_view view;
		view.resize(320, 760);
		view.show();
		QApplication::processEvents();
		const auto longLine = std::string{ "\tconst auto widest = measure(first_column, second_column, third_column, fourth_column, fifth_column, sixth_column);" };
		const auto body     = "```suggestion\n" + longLine + "\n```";
		open_composer(view, body);
		view.present_draft(qt::draft_composer_presentation{
			.Body        = QString::fromStdString(body),
			.Publishable = true,
			.Lines       = QStringLiteral("Line 1"),
			.Change      = core::suggested_change{ .Fenced = true, .Removed = { "\tconst auto widest = measure(first_column);" }, .Added = { longLine } },
		});
		settle_card();
		auto* const card   = view.open_card();
		auto* const change = card != nullptr ? card->findChild<QWidget*>(QStringLiteral("threadDraftChange")) : nullptr;
		auto* const rows   = card != nullptr ? card->findChild<QAbstractScrollArea*>(QStringLiteral("threadDraftChangeRows")) : nullptr;
		if (!aDirectory.isEmpty()) {
			expect(view.grab().save(aDirectory + QStringLiteral("/suggested-change-diff.png")), "render the drawer's suggested change");
		}
		expect(change != nullptr && change->isVisible() && rows != nullptr && rows->isVisible(), "the suggestion shows what it changes in its own scrolling box");
		auto* const bar = rows->horizontalScrollBar();
		expect(bar->maximum() > 0, "a line longer than the drawer gives the diff a horizontal scroll range");
		expect(rows->verticalScrollBar()->maximum() == 0, "every row of the diff shows; it never scrolls up and down");
		expect(card->width() <= view.viewport()->width() && change->mapTo(card, QPoint{ change->width(), 0 }).x() <= card->width(),
			   "the card and its diff stay the drawer's width");
		bar->setValue(0);
		auto side = wheel_at(*rows->viewport(), QPoint{ -40, 0 }, QPoint{ -120, 0 });
		QApplication::sendEvent(rows->viewport(), &side);
		expect(bar->value() > 0, "a sideways wheel scrolls the diff");
		const auto at   = bar->value();
		auto       down = wheel_at(*rows->viewport(), QPoint{ 0, -40 }, QPoint{ 0, -120 });
		QApplication::sendEvent(rows->viewport(), &down);
		expect(bar->value() == at, "a vertical wheel leaves the diff where it is: it is the drawer's");
		bar->setValue(0);
		expect(rows->focusPolicy() != Qt::NoFocus, "a diff that scrolls takes focus, as a wide code block's text does");
		rows->setFocus(Qt::OtherFocusReason);
		key(*rows, Qt::Key_Right);
		expect(bar->value() > 0, "Right scrolls the focused diff");
		key(*rows, Qt::Key_End);
		expect(bar->value() == bar->maximum(), "End shows the ends of its lines");
		key(*rows, Qt::Key_Home);
		expect(bar->value() == 0, "Home shows their starts");
		// A short change has nothing to scroll and takes no focus.
		view.present_draft(qt::draft_composer_presentation{
			.Body        = QStringLiteral("```suggestion\nint b = 2;\n```"),
			.Publishable = true,
			.Lines       = QStringLiteral("Line 1"),
			.Change      = core::suggested_change{ .Fenced = true, .Removed = { "int b = 1;" }, .Added = { "int b = 2;" } },
		});
		settle_card();
		expect(bar->maximum() == 0 && rows->focusPolicy() == Qt::NoFocus, "a short change has no horizontal range and no tab stop");
	}

	/// \brief The fonts `aBlock` is laid out in, besides the field's own.
	[[nodiscard]] auto block_fonts(const QTextBlock& aBlock) -> std::vector<QString>
	{
		auto families = std::vector<QString>{ };
		for (const auto& range : aBlock.layout()->formats()) {
			if (range.format.hasProperty(QTextFormat::FontFamilies)) {
				families.push_back(range.format.font().family());
			}
		}
		return families;
	}

	/// \brief [[BPR-2353]] (owner, 2026-11-21: "as soon as you make a
	/// ```suggestion block in a comment draft that whole comment gets rendered
	/// as an un-word-wrapped block including things outside of the code
	/// block"). A fenced block, a suggestion or any language, is code on its
	/// own lines only: the field keeps its font, wraps and never pans
	/// sideways; the words before and after the block wrap in it; the fence
	/// and code lines are in the drawer's code font.
	auto test_a_fenced_block_styles_only_its_own_lines(const QString& aDirectory) -> void
	{
		const auto before = std::string{ "This clamps the width before the measure, so a narrow column never reports a negative span to the layout pass." };
		const auto after  = std::string{ "The same pattern recurs in the footer measurement, which could share this helper rather than repeating the clamp." };
		const auto check  = [&](const std::string& aFence, const std::string& aClosing, const bool aSuggestion, const QString& aShot) {
			qt::conversation_cards_view view;
			view.resize(320, 760);
			view.show();
			QApplication::processEvents();
			const auto body = before + "\n\n" + aFence + "\nint b = std::max(width, 0);\n" + aClosing + "\n\n" + after;
			open_composer(view, body);
			auto presentation = qt::draft_composer_presentation{ .Body = QString::fromStdString(body), .Publishable = true, .Lines = QStringLiteral("Line 1") };
			if (aSuggestion) {
				presentation.Change = core::suggested_change{ .Fenced = true, .Removed = { "int b = width;" }, .Added = { "int b = std::max(width, 0);" } };
			}
			view.present_draft(presentation);
			settle_card();
			auto* const field = view.draft_editor();
			expect(field != nullptr, "the composer has its field");
			if (!aDirectory.isEmpty()) {
				expect(view.grab().save(aDirectory + aShot), "render the composer with a fenced block");
			}
			const auto& what = aFence;
			expect(field->lineWrapMode() == QPlainTextEdit::WidgetWidth && field->horizontalScrollBarPolicy() == Qt::ScrollBarAlwaysOff,
				   what + ": the field wraps at its width and has no sideways scroll");
			expect(field->font() != view.code_font(), what + ": the field keeps its own font, not the code font");
			if (aSuggestion) {
				expect(field->textCursor().block().blockNumber() == 3 && field->textCursor().atBlockEnd(), what + ": the caret is at the end of the block's last line, not after the words under it");
			}
			auto* const document = field->document();
			const auto  first    = document->findBlockByNumber(0);
			const auto  fence    = document->findBlockByNumber(2);
			const auto  code     = document->findBlockByNumber(3);
			const auto  closing  = document->findBlockByNumber(4);
			const auto  last     = document->findBlockByNumber(6);
			expect(first.layout()->lineCount() > 1 && last.layout()->lineCount() > 1, what + ": the words before and after the block wrap");
			expect(block_fonts(first).empty() && block_fonts(last).empty(), what + ": the words keep the field's font");
			const auto codeFamily = view.code_font().family();
			for (const auto& block : { fence, code, closing }) {
				const auto fonts = block_fonts(block);
				expect(!fonts.empty() && std::ranges::all_of(fonts, [&](const QString& aFamily) { return aFamily == codeFamily; }),
					   what + ": each of the block's lines, its fences included, is in the code font");
			}
			expect(code.layout()->formats().front().format.foreground() != fence.layout()->formats().front().format.foreground(),
				   what + ": the fences are quieter than the code between them");
			// Typing after the block keeps the words outside it as words.
			auto cursor = field->textCursor();
			cursor.movePosition(QTextCursor::End);
			field->setTextCursor(cursor);
			field->insertPlainText(QStringLiteral(" Worth a follow-up."));
			QApplication::processEvents();
			expect(block_fonts(field->document()->lastBlock()).empty(), what + ": text typed after the block stays prose");
		};
		check("```suggestion", "```", true, QStringLiteral("/fenced-suggestion-composer.png"));
		check("```cpp", "```", false, QStringLiteral("/fenced-cpp-composer.png"));
		check("~~~python", "~~~", false, QStringLiteral("/fenced-tilde-composer.png"));
	}

	/// \brief [[BPR-2354]] (owner, 2026-11-21: "Can we not do the nested
	/// code block element like we do for agent replies inside the
	/// composer?"). A fenced block in the composer is its own box: its lines
	/// unwrapped, scrolling sideways on their own bar, while the words around
	/// it wrap and the field never scrolls sideways. It is still one text:
	/// the caret and a selection cross into it, undo takes an edit in it
	/// back, and the text is the draft's, fences and all.
	auto test_a_code_block_is_its_own_box(const QString& aDirectory) -> void
	{
		qt::conversation_cards_view view;
		view.resize(320, 760);
		view.show();
		QApplication::processEvents();
		const auto before   = std::string{ "The widest column is measured on every call, which the profile shows as the hot spot in a long table." };
		const auto after    = std::string{ "Measuring once per layout pass, as the header already does, would keep this off the frame." };
		const auto longLine = std::string{ "\tconst auto widest = measure(first_column, second_column, third_column, fourth_column, fifth_column);" };
		const auto body     = before + "\n\n```cpp\n" + longLine + "\nint b = 2;\n```\n\n" + after;
		open_composer(view, body);
		view.present_draft(qt::draft_composer_presentation{ .Body = QString::fromStdString(body), .Publishable = true, .Lines = QStringLiteral("Line 1") });
		settle_card();
		auto* const field = view.draft_editor();
		expect(field != nullptr, "the composer has its field");
		auto* const document = field->document();
		expect(document->toPlainText() == QString::fromStdString(body), "the field's text is the draft's, fences and all");
		const auto prose   = document->findBlockByNumber(0);
		const auto fence   = document->findBlockByNumber(2);
		const auto code    = document->findBlockByNumber(3);
		const auto shorter = document->findBlockByNumber(4);
		const auto last    = document->findBlockByNumber(7);
		expect(prose.layout()->lineCount() > 1 && last.layout()->lineCount() > 1, "the words before and after the block wrap");
		expect(code.layout()->lineCount() == 1 && code.layout()->lineAt(0).naturalTextWidth() > field->viewport()->width(), "the long code line is one line, wider than the field");
		expect(field->horizontalScrollBar()->maximum() == 0 && field->horizontalScrollBarPolicy() == Qt::ScrollBarAlwaysOff, "the field itself never scrolls sideways");
		auto* const bar = field->viewport()->findChild<QScrollBar*>(QStringLiteral("threadCodeBlockScroll"));
		expect(bar != nullptr && bar->isVisible() && bar->maximum() > 0, "the block has its own sideways bar");
		const auto barTop = bar->mapTo(field->viewport(), QPoint{ 0, 0 }).y();
		const auto rowOf  = [field](const QTextBlock& aBlock) {
			auto at = QTextCursor{ aBlock };
			return field->cursorRect(at);
		};
		expect(barTop >= rowOf(shorter).bottom() - 1, "the bar sits under the block's last code line");
		if (!aDirectory.isEmpty()) {
			expect(view.grab().save(aDirectory + QStringLiteral("/code-box-composer.png")), "render the composer's code box");
		}
		const auto proseX = prose.layout()->lineAt(0).x();
		const auto fenceX = fence.layout()->lineAt(0).x();
		const auto codeX  = code.layout()->lineAt(0).x();
		bar->setValue(bar->maximum() / 2);
		QApplication::processEvents();
		expect(code.layout()->lineAt(0).x() < codeX && shorter.layout()->lineAt(0).x() == code.layout()->lineAt(0).x(), "the bar moves every code line of the block together");
		expect(prose.layout()->lineAt(0).x() == proseX && fence.layout()->lineAt(0).x() == fenceX && field->horizontalScrollBar()->value() == 0, "the words, the fence and the field stay where they are");
		if (!aDirectory.isEmpty()) {
			expect(view.grab().save(aDirectory + QStringLiteral("/code-box-composer-scrolled.png")), "render the scrolled code box");
		}
		// The owner, 2026-11-21: "my sideways scrolling gestures are not
		// scrolling it". A trackpad's sideways swipe over the box, delivered
		// as the platform delivers it (phases, pixels, momentum), moves the
		// box and nothing else, through the drawer's gesture latch.
		{
			const auto global = field->viewport()->mapToGlobal(QPoint{ 60, rowOf(code).center().y() });
			const auto phased = [&view, global](const Qt::ScrollPhase aPhase, const QPoint aPixels, const QPoint aAngle) {
				auto* const handle = view.window()->windowHandle();
				QWindowSystemInterface::handleWheelEvent(handle, handle->mapFromGlobal(global), global, aPixels, aAngle, Qt::NoModifier, aPhase);
				QWindowSystemInterface::flushWindowSystemEvents();
				QApplication::processEvents();
			};
			const auto drawer = view.verticalScrollBar()->value();
			bar->setValue(0);
			phased(Qt::ScrollBegin, { }, { });
			for (auto move = 0; move < 4; ++move) {
				phased(Qt::ScrollUpdate, QPoint{ -12, 1 }, QPoint{ -96, 8 });
			}
			const auto swiped = bar->value();
			for (auto coast = 0; coast < 4; ++coast) {
				phased(Qt::ScrollMomentum, QPoint{ -6, 0 }, QPoint{ -48, 0 });
			}
			phased(Qt::ScrollEnd, { }, { });
			std::cout << "code box swipe: " << swiped << " then " << bar->value() << " of " << bar->maximum() << '\n';
			expect(swiped > 0 && bar->value() > swiped, "a sideways trackpad swipe over the box scrolls it, and its momentum carries on");
			expect(field->horizontalScrollBar()->value() == 0 && view.horizontalScrollBar()->value() == 0 && view.verticalScrollBar()->value() == drawer, "the field and the drawer stay where they are");
			const auto far = bar->value();
			phased(Qt::ScrollBegin, { }, { });
			phased(Qt::ScrollUpdate, QPoint{ 24, 0 }, QPoint{ 192, 0 });
			phased(Qt::ScrollEnd, { }, { });
			expect(bar->value() < far, "a swipe the other way scrolls it back");
			// A mouse's sideways notches, with no phase.
			bar->setValue(0);
			phased(Qt::NoScrollPhase, { }, QPoint{ -120, 0 });
			expect(bar->value() > 0, "a sideways wheel notch scrolls the box");
			bar->setValue(bar->maximum() / 2);
			QApplication::processEvents();
		}
		// Clicking in the scrolled box puts the caret on the character shown.
		const auto clicked = field->cursorForPosition(QPoint{ 14, rowOf(code).center().y() });
		expect(clicked.block() == code && clicked.positionInBlock() > 0, "a click in the scrolled box lands on the character it shows");
		// The caret crosses from the words into the block, and the block
		// scrolls to keep it in sight.
		auto cursor = field->textCursor();
		cursor.setPosition(code.position());
		field->setTextCursor(cursor);
		QApplication::processEvents();
		expect(bar->value() == 0, "the caret at the line's start brings its start into sight");
		field->moveCursor(QTextCursor::EndOfBlock);
		QApplication::processEvents();
		const auto caret = field->cursorRect();
		expect(bar->value() == bar->maximum() && caret.right() <= field->viewport()->width() && caret.left() >= 0, "the caret at the line's end scrolls the block, not the field, to show it");
		cursor = field->textCursor();
		cursor.setPosition(document->findBlockByNumber(1).position());
		field->setTextCursor(cursor);
		field->moveCursor(QTextCursor::Down);
		field->moveCursor(QTextCursor::Down);
		expect(field->textCursor().block() == code, "Down moves from the words through the fence into the code");
		// A selection runs across the boundary and copies as one text.
		cursor.setPosition(prose.position() + 4);
		cursor.setPosition(code.position() + 10, QTextCursor::KeepAnchor);
		field->setTextCursor(cursor);
		const auto selected = field->textCursor().selectedText();
		expect(selected.contains(QStringLiteral("```cpp")) && selected.contains(QChar::ParagraphSeparator) && selected.endsWith(QString::fromStdString(longLine.substr(0U, 10U))),
			   "a selection crosses from the words into the block");
		// An edit in the block is undone like any other.
		cursor.setPosition(shorter.position() + shorter.length() - 1);
		field->setTextCursor(cursor);
		field->insertPlainText(QStringLiteral(" // one"));
		expect(document->toPlainText().contains(QStringLiteral("int b = 2; // one")), "the block's code is written in place");
		field->undo();
		expect(document->toPlainText() == QString::fromStdString(body) && document->findBlockByNumber(3).layout()->lineCount() == 1, "undo takes it back, and the block is still unwrapped");
		// Typing a fence in the words makes a box; deleting it makes words.
		cursor.movePosition(QTextCursor::End);
		field->setTextCursor(cursor);
		field->insertPlainText(QStringLiteral("\n\n```\n") + QString::fromStdString(longLine) + QStringLiteral("\n```"));
		QApplication::processEvents();
		const auto typed = document->findBlockByNumber(document->blockCount() - 2);
		expect(typed.layout()->lineCount() == 1 && typed.layout()->textOption().wrapMode() == QTextOption::NoWrap, "a block typed in the field is a box at once");
		field->undo();
		QApplication::processEvents();
		expect(document->toPlainText() == QString::fromStdString(body), "and undo takes the typed block back whole");
	}

	/// \brief [[BPR-2355]] (owner, 2026-11-21: "can we do syntax
	/// highlighting? Also if we could make the language into a picker"). The
	/// composer's code box is coloured by the reader's syntax service once
	/// typing pauses, and its header's picker rewrites the block's language
	/// as one undoable edit.
	auto test_a_code_box_is_coloured_and_names_its_language() -> void
	{
		qt::conversation_cards_view view;
		view.resize(320, 760);
		auto       theme                                                              = qt::markdown_code_theme{ };
		const auto keyword                                                            = QColor{ 0xC0, 0x10, 0x20 };
		theme.Colors[static_cast<std::size_t>(core::appearance_role::syntax_keyword)] = keyword;
		const auto type                                                               = QColor{ 0x10, 0x60, 0xC0 };
		theme.Colors[static_cast<std::size_t>(core::appearance_role::syntax_type)]    = type;
		view.set_code_theme(theme);
		auto sent = std::vector<application::markdown_code_syntax_request>{ };
		view.set_code_syntax_dispatch([&sent](std::vector<application::markdown_code_syntax_request> aRequests) { sent.insert(sent.end(), aRequests.begin(), aRequests.end()); });
		view.show();
		QApplication::processEvents();
		const auto body = std::string{ "Measure once.\n\n```cpp\nint b = 2;\nint c = 3;\n```\n\nThanks." };
		open_composer(view, body);
		view.present_draft(qt::draft_composer_presentation{ .Body = QString::fromStdString(body), .Publishable = true, .Lines = QStringLiteral("Line 1") });
		settle_card();
		const auto pause = [] {
			QElapsedTimer elapsed;
			elapsed.start();
			while (elapsed.elapsed() < 450) {
				QApplication::processEvents();
				QThread::msleep(5);
			}
		};
		pause();
		const auto asked = std::ranges::find(sent, std::string{ "int b = 2;\nint c = 3;" }, [](const application::markdown_code_syntax_request& aRequest) { return *aRequest.Text; });
		expect(asked != sent.end() && asked->Selector == "code.cpp", "once typing pauses the block's code is asked for, as C++");
		// [[BPR-2351]]. The drafts drawer knows its file only by identity, so a
		// new draft's blocks borrow no file's names yet; a reply's do, below.
		expect(asked != sent.end() && asked->Anchor.empty(), "a new draft's block is anchored to no file");
		auto* const field = view.draft_editor();
		const auto  code  = field->document()->findBlockByNumber(3);
		expect(std::ranges::none_of(code.layout()->formats(), [&](const QTextLayout::FormatRange& aRange) { return aRange.format.foreground().color() == keyword; }), "no colour before the answer");
		auto       spans    = std::make_shared<const application::token_span_list>(application::token_span_list{ { .Line = 0U, .Column = 0U, .Length = 3U, .Role = core::appearance_role::syntax_keyword },
																												 { .Line = 1U, .Column = 0U, .Length = 3U, .Role = core::appearance_role::syntax_keyword } });
		const auto coloured = [&](const QTextBlock& aBlock) {
			return std::ranges::any_of(aBlock.layout()->formats(), [&](const QTextLayout::FormatRange& aRange) { return aRange.start == 0 && aRange.length == 3 && aRange.format.foreground().color() == keyword; });
		};
		view.apply_markdown_code_syntax({ .Selector = asked->Selector, .Text = asked->Text, .Spans = spans, .Anchor = asked->Anchor });
		QApplication::processEvents();
		expect(coloured(code) && coloured(field->document()->findBlockByNumber(4)), "the answer colours each code line");
		// [[BPR-2351]]. When the file's names arrive the block is answered
		// again, and the field takes the newer colours.
		auto borrowed = std::make_shared<const application::token_span_list>(application::token_span_list{ { .Line = 0U, .Column = 0U, .Length = 3U, .Role = core::appearance_role::syntax_type },
																										   { .Line = 1U, .Column = 0U, .Length = 3U, .Role = core::appearance_role::syntax_keyword } });
		view.apply_markdown_code_syntax({ .Selector = asked->Selector, .Text = asked->Text, .Spans = borrowed, .Anchor = asked->Anchor });
		QApplication::processEvents();
		expect(std::ranges::any_of(field->document()->findBlockByNumber(3).layout()->formats(), [&](const QTextLayout::FormatRange& aRange) { return aRange.start == 0 && aRange.format.foreground().color() == type; })
				   && coloured(field->document()->findBlockByNumber(4)),
			   "[[BPR-2351]] a newer answer for the block recolours it");
		view.apply_markdown_code_syntax({ .Selector = asked->Selector, .Text = asked->Text, .Spans = spans, .Anchor = asked->Anchor });
		QApplication::processEvents();
		expect(!coloured(field->document()->findBlockByNumber(0)) && !coloured(field->document()->findBlockByNumber(2)), "the words and the fence keep their own colours");
		// Typing in one line keeps the other's colour until the new answer.
		auto cursor = QTextCursor{ field->document()->findBlockByNumber(4) };
		cursor.movePosition(QTextCursor::EndOfBlock);
		field->setTextCursor(cursor);
		field->insertPlainText(QStringLiteral(" // c"));
		QApplication::processEvents();
		expect(coloured(field->document()->findBlockByNumber(3)), "an unchanged line keeps its colour while the block changes");
		pause();
		expect(std::ranges::any_of(sent, [](const application::markdown_code_syntax_request& aRequest) { return *aRequest.Text == "int b = 2;\nint c = 3; // c"; }), "the changed block is asked for again");
		field->undo();
		// The picker names the language and rewrites it as one edit.
		auto* const picker = field->viewport()->findChild<QToolButton*>(QStringLiteral("threadCodeBlockLanguage"));
		expect(picker != nullptr && picker->isVisible() && picker->text() == QStringLiteral("C++") && picker->accessibleName() == QStringLiteral("Language: C++"), "the header's picker names the block's language");
		const auto choices = picker->menu()->actions();
		const auto python  = std::ranges::find_if(choices, [](const QAction* aAction) { return aAction->text() == QStringLiteral("Python"); });
		expect(python != choices.end(), "the picker offers Python");
		(*python)->trigger();
		QApplication::processEvents();
		expect(field->toPlainText() == QString::fromStdString(body).replace(QStringLiteral("```cpp"), QStringLiteral("```python")) && picker->text() == QStringLiteral("Python"),
			   "choosing Python rewrites the fence's language and nothing else");
		field->undo();
		QApplication::processEvents();
		expect(field->toPlainText() == QString::fromStdString(body) && picker->text() == QStringLiteral("C++"), "one undo takes the choice back");
		const auto suggestion = std::ranges::find_if(choices, [](const QAction* aAction) { return aAction->text() == QStringLiteral("Suggested change"); });
		(*suggestion)->trigger();
		QApplication::processEvents();
		expect(field->toPlainText().contains(QStringLiteral("```suggestion\nint b = 2;")) && picker->text() == QStringLiteral("Suggested change"), "a block can be made a suggestion, and back");
	}

	/// \brief [[BPR-2355]], [[BPR-2351]]. A reply's code blocks are anchored
	/// to the thread's file, as its posts are, so they borrow that file's
	/// names; an answer asked under another anchor is not the field's.
	auto test_a_reply_code_box_borrows_its_files_names() -> void
	{
		qt::conversation_cards_view view;
		view.resize(360, 700);
		auto       theme                                                              = qt::markdown_code_theme{ };
		const auto keyword                                                            = QColor{ 0xC0, 0x10, 0x20 };
		theme.Colors[static_cast<std::size_t>(core::appearance_role::syntax_keyword)] = keyword;
		view.set_code_theme(theme);
		auto sent = std::vector<application::markdown_code_syntax_request>{ };
		view.set_code_syntax_dispatch([&sent](std::vector<application::markdown_code_syntax_request> aRequests) { sent.insert(sent.end(), aRequests.begin(), aRequests.end()); });
		view.show();
		QApplication::processEvents();
		view.set_line_locator([](core::diff_side, const std::uint32_t aLine) -> std::optional<int> { return static_cast<int>(aLine) * 6; });
		view.adopt({ thread("t10", "src/a.cpp", 10U), reply("c10", "t10", "a", "first") }, core::file_identity_of("src/a.cpp"));
		view.set_action_dispatch([&view](const qt::card_action aAction, const std::string& aId) {
			if (aAction == qt::card_action::reply) {
				view.begin_reply(aId);
			}
		});
		view.open_thread("t10");
		settle_card();
		view.setFocus(Qt::OtherFocusReason);
		key(view, Qt::Key_R);
		auto* const editor = view.reply_editor();
		expect(editor != nullptr, "the reply field opens");
		editor->setPlainText(QStringLiteral("Like this:\n\n```cpp\nint b = 2;\n```"));
		QElapsedTimer elapsed;
		elapsed.start();
		while (elapsed.elapsed() < 450) {
			QApplication::processEvents();
			QThread::msleep(5);
		}
		const auto asked = std::ranges::find(sent, std::string{ "int b = 2;" }, [](const application::markdown_code_syntax_request& aRequest) { return *aRequest.Text; });
		expect(asked != sent.end() && asked->Anchor == "src/a.cpp", "the reply's block is asked for anchored to the thread's file");
		const auto coloured = [&] {
			return std::ranges::any_of(editor->document()->findBlockByNumber(3).layout()->formats(), [&](const QTextLayout::FormatRange& aRange) { return aRange.start == 0 && aRange.length == 3 && aRange.format.foreground().color() == keyword; });
		};
		const auto spans = std::make_shared<const application::token_span_list>(application::token_span_list{ { .Line = 0U, .Column = 0U, .Length = 3U, .Role = core::appearance_role::syntax_keyword } });
		view.apply_markdown_code_syntax({ .Selector = asked->Selector, .Text = asked->Text, .Spans = spans });
		QApplication::processEvents();
		expect(!coloured(), "an answer under no anchor is not the field's");
		view.apply_markdown_code_syntax({ .Selector = asked->Selector, .Text = asked->Text, .Spans = spans, .Anchor = asked->Anchor });
		QApplication::processEvents();
		expect(coloured(), "the answer under the thread's file colours the block");
	}

	/// \brief [[BPR-2273]]. The owner: "A long draft comment neither scrolls
	/// with 2 finger MacBook gestures (scroll wheel) nor expands to fit the
	/// contents". The composer grows with its text to about 60% of the
	/// drawer, then scrolls inside by wheel and by trackpad pixels; at its end
	/// the drawer takes the wheel. Closed, the long draft ends in an
	/// ellipsis and opens whole.
	auto test_a_long_draft_grows_then_scrolls(const QString& aDirectory) -> void
	{
		qt::conversation_cards_view view;
		view.resize(320, 700);
		view.show();
		QApplication::processEvents();
		auto paragraph = std::string{ };
		for (auto line = 0; line < 40; ++line) {
			paragraph += "The budget is reset on every call, so per-frame work is never bounded (" + std::to_string(line) + ").\n";
		}
		open_composer(view, paragraph);
		view.present_draft(qt::draft_composer_presentation{ .Body = QStringLiteral("short"), .Publishable = true, .Lines = QStringLiteral("Line 1") });
		settle_card();
		auto* const text = view.draft_editor();
		expect(text != nullptr, "the composer has its field");
		const auto shortHeight = text->height();
		view.present_draft(qt::draft_composer_presentation{ .Body = QString::fromStdString(paragraph), .Publishable = true, .Lines = QStringLiteral("Line 1") });
		settle_card();
		if (!aDirectory.isEmpty()) {
			expect(view.grab().save(aDirectory + QStringLiteral("/long-draft-composer.png")), "render the long composer");
		}
		const auto maximum = view.viewport()->height() * 3 / 5;
		auto*      bar     = text->verticalScrollBar();
		std::cout << "long draft: field " << shortHeight << " -> " << text->height() << " of " << maximum << ", range " << bar->maximum() << '\n';
		expect(text->height() > shortHeight + 200 && text->height() <= maximum && text->height() >= maximum - text->fontMetrics().lineSpacing() - 4,
			   "a long draft grows its field to about 60% of the drawer");
		expect(bar->maximum() > 0, "past its maximum the field has a vertical scroll range");
		auto forwarded = 0;
		view.set_scroll_forward([&forwarded](QWheelEvent&) {
			++forwarded;
			return true;
		});
		bar->setValue(0);
		auto angle = wheel_at(*text->viewport(), QPoint{ }, QPoint{ 0, -120 });
		QApplication::sendEvent(text->viewport(), &angle);
		expect(bar->value() > 0 && forwarded == 0, "a wheel's angle delta scrolls the field, and the drawer does not take it");
		const auto afterAngle = bar->value();
		auto       pixels     = wheel_at(*text->viewport(), QPoint{ 0, -3 * text->fontMetrics().lineSpacing() }, QPoint{ });
		QApplication::sendEvent(text->viewport(), &pixels);
		expect(bar->value() > afterAngle && forwarded == 0, "a trackpad's pixel delta scrolls the field");
		auto up = wheel_at(*text->viewport(), QPoint{ 0, 3 * text->fontMetrics().lineSpacing() }, QPoint{ 0, 120 });
		QApplication::sendEvent(text->viewport(), &up);
		expect(bar->value() < afterAngle + 3, "scrolling back up moves the field back");
		bar->setValue(bar->maximum());
		auto past = wheel_at(*text->viewport(), QPoint{ 0, -40 }, QPoint{ 0, -120 });
		QApplication::sendEvent(text->viewport(), &past);
		expect(bar->value() == bar->maximum() && forwarded == 1, "at its end the field hands the wheel to the drawer");
		// [[BPR-2279]] (owner, 2026-11-19: "momentum scrolling that reaches
		// the end of a nested scroll window won't scroll the outer window
		// without an additional gesture"). A trackpad gesture begun in the
		// field stays the field's to its end, momentum included; the next
		// gesture goes to the drawer.
		{
			const auto global = text->viewport()->mapToGlobal(text->viewport()->rect().center());
			// A trackpad's pixels, and the angle the platform gives with them.
			const auto phased = [&view, global](const Qt::ScrollPhase aPhase, const QPoint aPixels) {
				auto* const handle = view.window()->windowHandle();
				QWindowSystemInterface::handleWheelEvent(handle, handle->mapFromGlobal(global), global, aPixels, aPixels * 8, Qt::NoModifier, aPhase);
				QWindowSystemInterface::flushWindowSystemEvents();
				QApplication::processEvents();
			};
			const auto step = QPoint{ 0, -text->fontMetrics().lineSpacing() };
			forwarded       = 0;
			bar->setValue(bar->maximum() - 2);
			phased(Qt::ScrollBegin, { });
			for (auto move = 0; move < 4; ++move) {
				phased(Qt::ScrollUpdate, step);
			}
			for (auto coast = 0; coast < 8; ++coast) {
				phased(Qt::ScrollMomentum, step);
			}
			phased(Qt::ScrollEnd, { });
			std::cout << "BPR-2279 drawer: field " << bar->value() << "/" << bar->maximum() << ", handed to the drawer " << forwarded << '\n';
			expect(bar->value() == bar->maximum() && forwarded == 0, "a gesture begun in the field scrolls it to its end and its momentum never reaches the drawer");
			phased(Qt::ScrollBegin, { });
			phased(Qt::ScrollUpdate, step);
			phased(Qt::ScrollEnd, { });
			expect(forwarded > 0 && bar->value() == bar->maximum() && bpr::qt::nested_scroll_latch::owner() == &view, "the next gesture, at the field's end, goes to the drawer");
		}
		// Closed, the note says words were left out, and opening it shows them all.
		view.end_draft();
		settle_card();
		const auto note = std::ranges::find_if(view.cards(), [](const qt::thread_card& aCard) { return aCard.Draft; });
		expect(note != view.cards().end() && !note->Posts.empty() && note->Posts.front().Body.toStdString() == paragraph, "the closed draft keeps all its words");
		const auto preview = view.closed_preview(note->Id);
		std::cout << "closed long draft: " << preview.toStdString() << '\n';
		expect(preview.endsWith(QChar{ 0x2026 }) && preview.size() < static_cast<qsizetype>(paragraph.size()), "a closed long draft ends in an ellipsis, never cut off silently");
		if (!aDirectory.isEmpty()) {
			expect(view.grab().save(aDirectory + QStringLiteral("/long-draft-closed.png")), "render the closed long draft");
		}
	}

	auto test_my_comments_are_edited_in_place() -> void
	{
		qt::conversation_cards_view view;
		editing_view(view);
		auto saves = std::vector<qt::comment_edit_save>{ };
		view.set_comment_edit_dispatch([&saves](const qt::comment_edit_save& aSave) { saves.push_back(aSave); });
		auto* mine   = post_named(view, QStringLiteral("review-comment:C2"));
		auto* theirs = post_named(view, QStringLiteral("review-comment:C1"));
		expect(mine != nullptr && mine->findChild<QWidget*>(QStringLiteral("threadPostEdit"))->isHidden() && !view.begin_comment_edit("review-comment:C2"),
			   "without the composition's offer no post offers Edit");
		auto* menu = edit_menu_of(mine);
		expect(menu != nullptr && menu->findChild<QAction*>(QStringLiteral("postEdit")) == nullptr, "and the body menu has no Edit");
		delete menu;
		view.set_comment_edit_offered(true);
		QApplication::processEvents();
		mine             = post_named(view, QStringLiteral("review-comment:C2"));
		theirs           = post_named(view, QStringLiteral("review-comment:C1"));
		auto* const edit = mine->findChild<QAbstractButton*>(QStringLiteral("threadPostEdit"));
		expect(edit != nullptr && !edit->isHidden() && edit->focusPolicy() == Qt::StrongFocus && edit->accessibleName() == QStringLiteral("Edit your comment"),
			   "my reply offers Edit among its tools, focusable and named");
		expect(theirs->findChild<QWidget*>(QStringLiteral("threadPostEdit"))->isHidden() && !view.comment_editable("review-comment:C1") && !view.begin_comment_edit("review-comment:C1"),
			   "someone else's comment offers no Edit");
		expect(view.comment_editable("issue-comment:IC1") && view.comment_editable("review-comment:C2"), "my pull request comment and my reply are editable");
		menu = edit_menu_of(theirs);
		expect(menu != nullptr && menu->findChild<QAction*>(QStringLiteral("postEdit")) == nullptr, "someone else's body menu has no Edit");
		delete menu;
		menu              = edit_menu_of(mine);
		auto* const entry = menu != nullptr ? menu->findChild<QAction*>(QStringLiteral("postEdit")) : nullptr;
		expect(entry != nullptr && entry->isEnabled() && entry->text() == QStringLiteral("Edit"), "my body menu offers Edit");
		menu->hide();
		entry->trigger();
		delete menu;
		QApplication::processEvents();
		auto* editor = view.comment_editor();
		expect(view.comment_editing() == std::optional<std::string>{ "review-comment:C2" } && editor != nullptr && editor->isVisible() && editor->raw() == "Done.\r\nRenamed."
				   && editor->window()->focusWidget() == editor && view.comment_edit_original() == "Done.\r\nRenamed.",
			   "Edit opens the editor focused, on GitHub's exact bytes");
		mine = post_named(view, QStringLiteral("review-comment:C2"));
		expect(mine->findChild<QWidget*>(QStringLiteral("threadPostReader"))->isHidden() && mine->isAncestorOf(editor) && mine->findChild<QWidget*>(QStringLiteral("threadPostEdit"))->isHidden(),
			   "the editor takes the body's place in the post, under its name");
		auto end = editor->textCursor();
		end.movePosition(QTextCursor::End);
		editor->setTextCursor(end);
		editor->insertPlainText(QStringLiteral(" Also tested."));
		QApplication::processEvents();
		press_in(editor, Qt::Key_Return, Qt::ControlModifier);
		expect(saves.size() == 1U && saves.back() == qt::comment_edit_save{ .Post = "review-comment:C2", .Original = "Done.\r\nRenamed.", .Body = "Done.\r\nRenamed. Also tested." },
			   "⌘↩ is Save: a gesture carrying the post, the original and the text, untouched bytes kept");
		view.present_comment_edit_status(QStringLiteral("Saving your comment to GitHub…"), true);
		QApplication::processEvents();
		expect(editor->isReadOnly() && !view.comment_edit_save_button()->isEnabled() && view.comment_edit_status()->text() == QStringLiteral("Saving your comment to GitHub…"),
			   "while GitHub is asked the text and Save are held, and the words show");
		press_in(editor, Qt::Key_Escape, Qt::NoModifier);
		expect(view.comment_editing().has_value(), "Esc does not abandon a save in flight");
		view.present_comment_edit_status(QStringLiteral("Someone changed this comment on GitHub since you began. Nothing was saved."));
		view.offer_newer_comment("Changed elsewhere.");
		QApplication::processEvents();
		editor = view.comment_editor();
		expect(!editor->isReadOnly() && editor->raw() == "Done.\r\nRenamed. Also tested." && view.comment_edit_newer_button()->isVisible(),
			   "a conflict keeps the typed text and offers GitHub's newer text");
		// A refresh with GitHub's newer body rebuilds the thread: the edit stays.
		view.adopt({ thread("review-thread:T1", "src/a.cpp", 10U), reply("review-comment:C1", "review-thread:T1", "someone", "Please rename this."),
					 reply("review-comment:C2", "review-comment:C1", "reviewer-one", "Changed elsewhere."),
					 conversation{ .Id = "issue-comment:IC1", .Kind = application::github_conversation_kind::issue_comment, .AuthorLogin = "reviewer-one", .Body = "My pull request comment." } });
		QApplication::processEvents();
		editor = view.comment_editor();
		expect(editor != nullptr && editor->raw() == "Done.\r\nRenamed. Also tested." && view.comment_edit_newer_button()->isVisible(), "a refresh keeps the editor, the typed text and the offer");
		view.comment_edit_newer_button()->click();
		QApplication::processEvents();
		expect(editor->raw() == "Changed elsewhere." && view.comment_edit_original() == "Changed elsewhere." && !view.comment_edit_newer_button()->isVisible()
				   && view.comment_edit_status()->text() == QStringLiteral("Editing GitHub's newer text."),
			   "Edit GitHub's newer text loads it as the new original");
		press_in(editor, Qt::Key_Escape, Qt::NoModifier);
		expect(!view.comment_editing().has_value() && view.comment_editor() == nullptr && saves.size() == 1U, "Esc cancels and sends nothing");
		expect(!post_named(view, QStringLiteral("review-comment:C2"))->findChild<QWidget*>(QStringLiteral("threadPostReader"))->isHidden(), "the body is back");

		// The pull request comment is edited from its own tool; Cancel closes.
		expect(view.begin_comment_edit("issue-comment:IC1") && view.open_thread_id() == "issue-comment:IC1" && view.comment_editor() != nullptr
				   && view.comment_editor()->raw() == "My pull request comment.",
			   "Edit on my pull request comment opens its card and the editor");
		view.comment_edit_cancel_button()->click();
		QApplication::processEvents();
		expect(!view.comment_editing().has_value(), "Cancel closes the editor");
		// Withdrawn while editing: the editor closes and nothing is sent.
		static_cast<void>(view.begin_comment_edit("issue-comment:IC1"));
		view.set_comment_edit_offered(false);
		QApplication::processEvents();
		expect(!view.comment_editing().has_value() && view.comment_editor() == nullptr && saves.size() == 1U, "a withdrawn offer closes the editor");
		// In Review (not the feed) the same post offers it too.
		view.set_comment_edit_offered(true);
		view.set_feed(false);
		view.open_thread("review-thread:T1");
		QApplication::processEvents();
		expect(!post_named(view, QStringLiteral("review-comment:C2"))->findChild<QWidget*>(QStringLiteral("threadPostEdit"))->isHidden() && view.begin_comment_edit("review-comment:C2")
				   && view.comment_editor() != nullptr,
			   "in Review my comment offers Edit and edits in place");
		view.close_comment_edit();
	}

	/// [[BPR-1775]]. Offscreen renders: Edit offered, the editor open, a
	/// conflict.
	auto render_comment_edit(const QString& aDirectory) -> void
	{
		qt::conversation_cards_view view;
		editing_view(view);
		view.set_comment_edit_offered(true);
		QApplication::processEvents();
		post_named(view, QStringLiteral("review-comment:C2"))->findChild<QWidget*>(QStringLiteral("threadPostEdit"))->setFocus(Qt::TabFocusReason);
		QApplication::processEvents();
		expect(view.grab().save(aDirectory + QStringLiteral("/bpr-1775-edit-offered.png")), "render: Edit offered");
		static_cast<void>(view.begin_comment_edit("review-comment:C2"));
		QApplication::processEvents();
		expect(view.grab().save(aDirectory + QStringLiteral("/bpr-1775-editor-open.png")), "render: the editor open");
		view.comment_editor()->insertPlainText(QStringLiteral("Typed while someone else edited. "));
		view.present_comment_edit_status(QStringLiteral("Someone changed this comment on GitHub since you began. Nothing was saved."));
		view.offer_newer_comment("Changed elsewhere.");
		QApplication::processEvents();
		expect(view.grab().save(aDirectory + QStringLiteral("/bpr-1775-conflict.png")), "render: a conflict");
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
	if (argc > 2 && std::string_view{ argv[1] } == "--render-reactions") {
		test_reactions_render_in_light_and_dark(QString::fromLocal8Bit(argv[2]));
		std::cout << "render-reactions:pass\n";
		return EXIT_SUCCESS;
	}
	if (argc > 2 && std::string_view{ argv[1] } == "--render-line-numbers") {
		test_line_numbers_read_on_the_open_card(QString::fromLocal8Bit(argv[2]));
		std::cout << "render-line-numbers:pass\n";
		return EXIT_SUCCESS;
	}
	if (argc > 2 && std::string_view{ argv[1] } == "--render-bpr-1803") {
		test_my_posts_are_named_by_my_login(QString::fromLocal8Bit(argv[2]));
		test_reactions_render_in_light_and_dark(QString::fromLocal8Bit(argv[2]));
		std::cout << "render-bpr-1803:pass\n";
		return EXIT_SUCCESS;
	}
	if (argc > 3 && std::string_view{ argv[1] } == "--render-bpr-1779") {
		render_owner_186(QString::fromLocal8Bit(argv[2]), QString::fromLocal8Bit(argv[3]));
		std::cout << "render-bpr-1779:pass\n";
		return EXIT_SUCCESS;
	}
	if (argc > 2 && std::string_view{ argv[1] } == "--render-bpr-1775") {
		render_comment_edit(QString::fromLocal8Bit(argv[2]));
		std::cout << "render-bpr-1775:pass\n";
		return EXIT_SUCCESS;
	}
	if (argc > 2 && std::string_view{ argv[1] } == "--render-bpr-2271") {
		test_a_suggested_change_scrolls_sideways_in_its_box(QString::fromLocal8Bit(argv[2]));
		std::cout << "render-bpr-2271:pass\n";
		return EXIT_SUCCESS;
	}
	if (argc > 2 && std::string_view{ argv[1] } == "--render-fenced-composer") {
		test_a_fenced_block_styles_only_its_own_lines(QString::fromLocal8Bit(argv[2]));
		std::cout << "render-fenced-composer:pass\n";
		return EXIT_SUCCESS;
	}
	if (argc > 2 && std::string_view{ argv[1] } == "--render-code-box") {
		test_a_code_block_is_its_own_box(QString::fromLocal8Bit(argv[2]));
		std::cout << "render-code-box:pass\n";
		return EXIT_SUCCESS;
	}
	if (argc > 2 && std::string_view{ argv[1] } == "--render-bpr-2273") {
		test_a_long_draft_grows_then_scrolls(QString::fromLocal8Bit(argv[2]));
		std::cout << "render-bpr-2273:pass\n";
		return EXIT_SUCCESS;
	}
	if (argc > 1 && std::string_view{ argv[1] } == "--publication-status-geometry") {

		test_wrapped_publication_status_is_fully_visible();
		std::cout << "publication-status-geometry:pass\n";
		return EXIT_SUCCESS;
	}
	test_pull_request_comments_live_at_the_edge();
	test_wrapped_publication_status_is_fully_visible();
	test_replacing_a_forest_never_reads_the_last_one();
	test_discuss_wheel_moves_readable_lines_without_changing_selection();
	test_thread_menus_and_defect_badges();
	test_one_thread_read_state_matches_the_complete_state();
	test_branching_conversation_reads_in_place();
	test_read_state_marks_unread_posts_and_offers_mark_read();
	test_mark_all_from_an_author_is_offered_from_thread_and_post_menus();
	test_new_boundary_is_never_the_first_thing_in_a_thread();
	test_discuss_lists_threads_that_open_in_place();
	test_a_thread_menu_names_why_it_cannot_resolve();
	test_discuss_post_menus_open_the_exact_comment_in_review();
	test_notes_hang_from_their_lines_and_one_opens_pinned();
	test_start_a_review_in_the_composer();
	test_local_comments_share_a_line();
	test_local_comments_edges_and_editing();
	test_composer_state_control();
	test_composer_notice_keeps_rows_apart();
	test_placement_notice_is_a_warning_mark();
	test_look_v002_rework();
	test_a_draft_clicked_away_from_paints_its_words();
	test_a_draft_left_by_the_conversation_keeps_its_words();
	test_opening_the_card_below_an_open_thread_shows_it_whole();
	test_chips_wheel_and_overflow();
	test_reply_in_place_and_next_new();
	test_reply_in_place_has_the_composers_label_controls();
	test_a_reply_suggestion_is_marked_native_or_text_only();
	test_an_outdated_thread_takes_a_reply_with_a_quiet_note();
	test_a_thread_github_refuses_offers_no_reply();
	test_published_suggestions_show_against_their_lines();
	test_endpoint_changes_retain_exact_body_annotations();
	test_unchanged_refresh_retains_measured_notes();
	test_alternating_widths_keep_both_widths_measured();
	test_discuss_geometry_refresh_updates_excerpts();
	test_the_excerpt_follows_the_supplied_code_font();
	test_the_drawer_scrolls_past_its_end();
	test_cached_geometry_matches_a_fresh_view();
	test_a_dispatch_that_relays_out_still_names_the_pressed_thread();
	test_discuss_new_counts_share_one_column();
	test_reactions_show_and_toggle_by_mouse_and_keyboard();
	test_resolve_conversation_beside_reply();
	test_reactions_render_in_light_and_dark({ });
	test_my_posts_are_named_by_my_login({ });
	test_line_numbers_read_on_the_open_card({ });
	test_conversation_lists_comments_not_reviews();
	test_a_review_body_is_a_comment_in_its_bundle();
	test_my_comments_are_edited_in_place();
	test_a_suggested_change_scrolls_sideways_in_its_box({ });
	test_a_long_draft_grows_then_scrolls({ });
	test_a_fenced_block_styles_only_its_own_lines({ });
	test_a_code_block_is_its_own_box({ });
	test_a_code_box_is_coloured_and_names_its_language();
	test_a_reply_code_box_borrows_its_files_names();
	std::cout << "conversation-cards-view-tests:pass\n";

	return EXIT_SUCCESS;
}
