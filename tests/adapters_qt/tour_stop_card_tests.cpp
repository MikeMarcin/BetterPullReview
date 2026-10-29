/// \file
/// \brief [[BPR-1806]]. A published tour stop reads as a tour stop wherever
/// its comment shows: its closed note says "Stop 2 of 3" beside its author
/// and leads with its title; open, it has a plate (the tour's numbered route
/// marker, its title, "Stop 2 of 3" and, while the tour can be followed, Open
/// in tour) over its narrative alone. The website's pinned source, heading and
/// navigation are not shown; View source and Copy source keep every byte.
/// Replies, ordinary comments and anything not exactly the published envelope
/// read as they always have.

#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/bundled_code_font.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/markdown_body_view.hpp"

#include "bpr/core/review_tour_markdown.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QImage>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QTextEdit>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using conversation = application::github_conversation_projection;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void settle()
	{
		for (auto round = 0; round < 4; ++round) {
			QApplication::processEvents();
		}
	}

	/// \brief A key pressed and released on a widget.
	void press(QWidget& aWidget, const Qt::Key aKey)
	{
		auto down = QKeyEvent{ QEvent::KeyPress, aKey, Qt::NoModifier, aKey == Qt::Key_Space ? QStringLiteral(" ") : QString{ } };
		QApplication::sendEvent(&aWidget, &down);
		auto up = QKeyEvent{ QEvent::KeyRelease, aKey, Qt::NoModifier, aKey == Qt::Key_Space ? QStringLiteral(" ") : QString{ } };
		QApplication::sendEvent(&aWidget, &up);
	}

	/// \brief Saves a render when `BPR_TOUR_STOP_RENDER_DIR` names a folder:
	/// layout evidence only, never a substitute for the owner's look.
	void save_render(QWidget& aWidget, const QString& aName)
	{
		const auto folder = qEnvironmentVariable("BPR_TOUR_STOP_RENDER_DIR");
		if (!folder.isEmpty()) {
			expect(QDir{ }.mkpath(folder), "the render folder exists");
			expect(aWidget.grab().save(folder + QLatin1Char{ '/' } + aName + QStringLiteral(".png")), "the render is saved");
		}
	}

	constexpr auto overview = std::string_view{ "https://github.com/octo/widgets/pull/7#pullrequestreview-9" };
	constexpr auto source   = std::string_view{ "https://github.com/octo/widgets/blob/1111111111111111111111111111111111111111/src/frame.cpp#L12-L18" };

	[[nodiscard]] auto stop_body(const std::string& aTitle, const std::size_t aOrdinal, const std::string_view aNarrative, const std::size_t aCount) -> std::string
	{
		auto links = std::vector<core::tour_markdown_link>{ };
		if (aOrdinal > 1U) {
			links.push_back({ "Previous", "https://github.com/octo/widgets/pull/7/files#r" + std::to_string(100U + aOrdinal - 1U) });
		}
		links.push_back({ "Tour overview", std::string{ overview } });
		if (aOrdinal < aCount) {
			links.push_back({ "Next", "https://github.com/octo/widgets/pull/7/files#r" + std::to_string(100U + aOrdinal + 1U) });
		}
		const auto body = core::render_tour_stop(aTitle, aOrdinal, aNarrative, links, source);
		expect(body.has_value(), "the publisher renders the stop");
		return *body;
	}

	constexpr auto narrative = std::string_view{ "The frame is clamped **before** layout, so a `nullptr` view never reaches the painter.\n\nThe caller owns the width." };

	void add_thread(std::vector<conversation>& aForest, const std::string& aThread, const std::string& aBody, const std::uint32_t aLine)
	{
		auto thread               = conversation{ };
		thread.Id                 = aThread;
		thread.Kind               = application::github_conversation_kind::review_thread;
		thread.Path               = "src/frame.cpp";
		thread.Line               = aLine;
		thread.DiffSide           = application::github_conversation_diff_side::right;
		thread.AnchorAvailability = application::github_conversation_anchor_availability::current;
		aForest.push_back(std::move(thread));
		auto root        = conversation{ };
		root.Id          = "review-comment:" + aThread;
		root.ParentId    = aThread;
		root.Kind        = application::github_conversation_kind::review_comment;
		root.AuthorLogin = "octocat";
		root.Body        = aBody;
		root.Path        = "src/frame.cpp";
		root.Line        = aLine;
		aForest.push_back(std::move(root));
	}

	[[nodiscard]] auto forest() -> std::vector<conversation>
	{
		auto result = std::vector<conversation>{ };
		add_thread(result, "stop-1", stop_body("Where the frame starts", 1, "Everything begins in `frame::open`.", 3), 4U);
		add_thread(result, "stop-2", stop_body("Why the width is clamped", 2, narrative, 3), 12U);
		add_thread(result, "stop-3", stop_body("Teardown", 3, "The frame is released last.", 3), 30U);
		// A reply to a stop is an ordinary post.
		auto reply        = conversation{ };
		reply.Id          = "review-comment:reply";
		reply.ParentId    = std::string{ "stop-2" };
		reply.Kind        = application::github_conversation_kind::review_comment;
		reply.AuthorLogin = "hubot";
		reply.Body        = "Should the clamp move to the caller?";
		reply.Path        = "src/frame.cpp";
		reply.Line        = 12U;
		result.push_back(std::move(reply));
		add_thread(result, "plain", "An ordinary comment about the frame.", 40U);
		// The heading without the rest of the envelope is not a stop.
		add_thread(result, "broken", "### PR Tour · 4 — Missing navigation\n\nWords only.", 50U);
		return result;
	}

	[[nodiscard]] auto card_of(const qt::conversation_cards_view& aView, const std::string_view aId) -> const qt::thread_card*
	{
		const auto found = std::ranges::find(aView.cards(), std::string{ aId }, &qt::thread_card::Id);
		return found == aView.cards().end() || found->Posts.empty() ? nullptr : &*found;
	}

	[[nodiscard]] auto posts_of(const QWidget& aCard) -> std::vector<QWidget*>
	{
		auto posts = std::vector<QWidget*>{ };
		for (auto* const post : aCard.findChildren<QWidget*>(QStringLiteral("threadPost"))) {
			posts.push_back(post);
		}
		std::ranges::sort(posts, { }, [](const QWidget* aPost) { return aPost->y(); });
		return posts;
	}

	void test_stops_are_recognized_in_the_notes()
	{
		qt::conversation_cards_view view;
		view.resize(420, 900);
		view.show();
		view.adopt(forest());
		settle();
		const auto* const first  = card_of(view, "stop-1");
		const auto* const second = card_of(view, "stop-2");
		const auto* const third  = card_of(view, "stop-3");
		const auto* const plain  = card_of(view, "plain");
		const auto* const broken = card_of(view, "broken");
		expect(first != nullptr && second != nullptr && third != nullptr && plain != nullptr && broken != nullptr, "every thread has its note");
		const auto& stop = second->Posts.front();
		expect(stop.TourStop.has_value() && stop.TourStop->Ordinal == 2U && stop.TourStop->Title == "Why the width is clamped" && stop.TourStopCount == 3U,
			   "a published stop is read as stop 2 of 3, its title unescaped");
		expect(qt::tour_stop_badge(*stop.TourStop, stop.TourStopCount) == QStringLiteral("Stop 2 of 3") && qt::tour_stop_badge(*stop.TourStop, std::nullopt) == QStringLiteral("Stop 2"),
			   "its badge says which stop of how many, or only which while the count is unknown");
		expect(first->Posts.front().TourStopCount == 3U && third->Posts.front().TourStopCount == 3U, "all three stops of the tour know its length");
		expect(stop.Body == QString::fromStdString(stop_body("Why the width is clamped", 2, narrative, 3)), "the post's body stays exactly as published");
		expect(second->Posts.size() == 2U && !second->Posts[1].TourStop.has_value(), "the reply in the stop's thread is an ordinary post");
		expect(!plain->Posts.front().TourStop.has_value() && !broken->Posts.front().TourStop.has_value(), "an ordinary comment and a broken envelope are not stops");
		save_render(view, QStringLiteral("tour-stop-notes"));
	}

	void test_the_open_stop_shows_its_plate_and_narrative()
	{
		qt::conversation_cards_view view;
		view.resize(440, 900);
		view.show();
		view.adopt(forest());
		settle();
		view.open_thread("stop-2");
		settle();
		auto* const card  = view.open_card();
		const auto  posts = card != nullptr ? posts_of(*card) : std::vector<QWidget*>{ };
		expect(posts.size() == 2U, "the stop's thread opens with its two posts");
		auto* const plate = posts[0]->findChild<QWidget*>(QStringLiteral("threadPostTourStop"));
		expect(plate != nullptr && plate->isVisibleTo(card), "the stop has its plate");
		expect(plate->accessibleName() == QStringLiteral("Tour stop: Why the width is clamped") && plate->accessibleDescription() == QStringLiteral("Stop 2 of 3"),
			   "the plate is named for its title and says which stop it is");
		expect(posts[0]->accessibleName() == QStringLiteral("octocat, Stop 2 of 3: Why the width is clamped"), "the post is named as a tour stop");
		expect(posts[1]->findChild<QWidget*>(QStringLiteral("threadPostTourStop")) == nullptr, "the reply has no plate");
		expect(plate->findChild<QAbstractButton*>(QStringLiteral("threadPostTourOpen")) == nullptr, "without a tour to follow there is no Open in tour");
		auto* const reader   = dynamic_cast<qt::markdown_body_view*>(posts[0]->findChild<QWidget*>(QStringLiteral("threadPostReader")));
		const auto  rendered = posts[0]->findChild<QTextEdit*>(QStringLiteral("threadPostBody"))->toPlainText();
		expect(rendered.startsWith(QStringLiteral("The frame is clamped before layout")) && rendered.contains(QStringLiteral("The caller owns the width.")),
			   "the narrative renders, its Markdown read");
		expect(!rendered.contains(QStringLiteral("PR Tour")) && !rendered.contains(QStringLiteral("Tour overview")) && !rendered.contains(QStringLiteral("Previous"))
				   && !rendered.contains(QStringLiteral("frame.cpp")) && !rendered.contains(QStringLiteral("GitHub hides")),
			   "the pinned source, heading, navigation and its note are not shown");
		expect(plate->geometry().bottom() < reader->geometry().top(), "the plate sits over the narrative");
		const auto original = stop_body("Why the width is clamped", 2, narrative, 3);
		expect(reader->original_body() == original && reader->source_view()->toPlainText() == QString::fromStdString(original), "View source keeps every byte");
		reader->copy_source();
		expect(QApplication::clipboard()->text() == QString::fromStdString(original), "and so does Copy source");
		const auto replyText = posts[1]->findChild<QTextEdit*>(QStringLiteral("threadPostBody"))->toPlainText();
		expect(replyText == QStringLiteral("Should the clamp move to the caller?"), "the reply reads as written");

		// Open in tour, once the window has the tour to follow.
		auto asked = std::optional<std::pair<std::string, std::size_t>>{ };
		view.set_tour_open_dispatch([&asked](const std::string& aPost, const core::published_tour_stop& aStop) { asked = std::pair{ aPost, aStop.Ordinal }; });
		view.set_tour_open_offered(true);
		settle();
		auto* const offered = view.open_card();
		const auto  again   = offered != nullptr ? posts_of(*offered) : std::vector<QWidget*>{ };
		auto* const open    = again.empty() ? nullptr : again[0]->findChild<QAbstractButton*>(QStringLiteral("threadPostTourOpen"));
		expect(open != nullptr && open->isVisibleTo(offered) && open->accessibleName() == QStringLiteral("Open stop 2 in the tour") && open->focusPolicy() == Qt::StrongFocus,
			   "Open in tour is offered, named and reachable by keyboard");
		open->click();
		expect(asked.has_value() && asked->first == "review-comment:stop-2" && asked->second == 2U, "Open in tour names the post and the stop");
		view.set_tour_open_offered(false);
		settle();
		const auto withdrawn = posts_of(*view.open_card());
		expect(withdrawn[0]->findChild<QAbstractButton*>(QStringLiteral("threadPostTourOpen")) == nullptr, "withdrawn, Open in tour goes");

		// An ordinary thread opens as it always has.
		view.open_thread("plain");
		settle();
		const auto ordinary = posts_of(*view.open_card());
		expect(ordinary.size() == 1U && ordinary[0]->findChild<QWidget*>(QStringLiteral("threadPostTourStop")) == nullptr
				   && ordinary[0]->findChild<QTextEdit*>(QStringLiteral("threadPostBody"))->toPlainText() == QStringLiteral("An ordinary comment about the frame."),
			   "an ordinary comment has no plate");
		view.open_thread("broken");
		settle();
		const auto broken = posts_of(*view.open_card());
		expect(broken.size() == 1U && broken[0]->findChild<QWidget*>(QStringLiteral("threadPostTourStop")) == nullptr
				   && broken[0]->findChild<QTextEdit*>(QStringLiteral("threadPostBody"))->toPlainText().contains(QStringLiteral("PR Tour")),
			   "a broken envelope reads whole, its heading included");
	}

	struct plate_parts
	{
		QWidget*         Plate    = nullptr;
		QAbstractButton* Previous = nullptr;
		QAbstractButton* Next     = nullptr;
	};

	[[nodiscard]] auto open_plate(const qt::conversation_cards_view& aView) -> plate_parts
	{
		auto* const card  = aView.open_card();
		const auto  posts = card != nullptr ? posts_of(*card) : std::vector<QWidget*>{ };
		auto* const plate = posts.empty() ? nullptr : posts.front()->findChild<QWidget*>(QStringLiteral("threadPostTourStop"));
		return { plate, plate != nullptr ? plate->findChild<QAbstractButton*>(QStringLiteral("threadPostTourPrevious")) : nullptr,
				 plate != nullptr ? plate->findChild<QAbstractButton*>(QStringLiteral("threadPostTourNext")) : nullptr };
	}

	/// \brief [[BPR-1810]]. The plate steps along the tour: Previous and Next
	/// chevrons beside "Stop 2 of 3", named for the stops they reach, open the
	/// neighbouring stop's thread in the same notes, and are unavailable at
	/// the tour's ends or while a stop is not among the comments.
	void test_the_plate_steps_along_the_tour()
	{
		qt::conversation_cards_view view;
		view.resize(440, 900);
		view.show();
		view.adopt(forest());
		settle();
		const auto& middle = card_of(view, "stop-2")->Posts.front();
		expect(middle.TourPrevious == qt::tour_stop_neighbour{ "stop-1", 1U, "Where the frame starts" } && middle.TourNext == qt::tour_stop_neighbour{ "stop-3", 3U, "Teardown" },
			   "a middle stop knows the threads, numbers and titles of its neighbours");
		expect(!card_of(view, "stop-1")->Posts.front().TourPrevious && card_of(view, "stop-3")->Posts.front().TourPrevious.has_value() && !card_of(view, "stop-3")->Posts.front().TourNext,
			   "the first stop has no previous and the last no next");
		expect(!card_of(view, "stop-2")->Posts[1].TourNext && !card_of(view, "plain")->Posts.front().TourNext, "a reply and an ordinary comment step nowhere");

		view.open_thread("stop-2");
		settle();
		auto plate = open_plate(view);
		expect(plate.Plate != nullptr && plate.Previous != nullptr && plate.Next != nullptr, "the middle stop's plate has both chevrons");
		expect(plate.Previous->isEnabled() && plate.Next->isEnabled(), "both lead somewhere");
		expect(plate.Previous->accessibleName() == QStringLiteral("Previous stop: Where the frame starts") && plate.Next->accessibleName() == QStringLiteral("Next stop: Teardown"),
			   "each chevron is named for the stop it reaches");
		expect(plate.Previous->toolTip() == QStringLiteral("Previous stop: Where the frame starts") && plate.Next->toolTip() == QStringLiteral("Next stop: Teardown"), "and says so under the pointer");
		expect(plate.Previous->focusPolicy() == Qt::StrongFocus && plate.Next->focusPolicy() == Qt::StrongFocus, "both are reachable by keyboard");
		expect(plate.Previous->geometry().right() < plate.Next->geometry().left() && plate.Next->geometry().bottom() <= plate.Plate->height(), "the chevrons sit inside the plate, Previous first");
		save_render(*view.open_card(), QStringLiteral("tour-stop-step-middle"));

		// Next opens the following stop's thread, keeping the keyboard on Next.
		plate.Next->setFocus(Qt::OtherFocusReason);
		plate.Next->click();
		settle();
		expect(view.open_thread_id() == "stop-3", "Next opens the next stop's thread");
		plate = open_plate(view);
		expect(plate.Previous != nullptr && plate.Previous->isEnabled() && plate.Next != nullptr && !plate.Next->isEnabled(), "at the last stop Next is unavailable");
		expect(plate.Next->toolTip() == QStringLiteral("This is the tour's last stop") && plate.Next->accessibleName() == QStringLiteral("Next stop"), "and says why");
		expect(QApplication::focusWidget() == plate.Previous, "the keyboard stays on the plate, on the chevron that still leads somewhere");
		save_render(*view.open_card(), QStringLiteral("tour-stop-step-last"));

		// Previous twice, by keyboard, reaches the first stop.
		press(*plate.Previous, Qt::Key_Return);
		settle();
		expect(view.open_thread_id() == "stop-2", "Return on Previous opens the previous stop");
		plate = open_plate(view);
		expect(QApplication::focusWidget() == plate.Previous, "focus follows to the same chevron");
		press(*plate.Previous, Qt::Key_Space);
		settle();
		expect(view.open_thread_id() == "stop-1", "Space steps too");
		plate = open_plate(view);
		expect(plate.Previous != nullptr && !plate.Previous->isEnabled() && plate.Previous->toolTip() == QStringLiteral("This is the tour's first stop") && plate.Next->isEnabled(),
			   "at the first stop Previous is unavailable");
		save_render(*view.open_card(), QStringLiteral("tour-stop-step-first"));
		expect(!view.step_tour(false) && view.open_thread_id() == "stop-1", "stepping past the first stop does nothing");

		// A dispatch decides where a step goes: the window's, for another file.
		auto asked = std::vector<std::string>{ };
		view.set_tour_step_dispatch([&asked](const std::string& aThread) { asked.push_back(aThread); });
		expect(view.step_tour(true) && asked == std::vector<std::string>{ "stop-2" } && view.open_thread_id() == "stop-1", "with a dispatch the view names the thread and opens nothing");
		view.set_tour_step_dispatch({ });

		// A neighbour not among the comments.
		auto partial = forest();
		std::erase_if(partial, [](const conversation& aNode) { return aNode.Id == "stop-3" || aNode.Id == "review-comment:stop-3"; });
		view.adopt(std::move(partial));
		view.open_thread("stop-2");
		settle();
		plate = open_plate(view);
		expect(plate.Next != nullptr && !plate.Next->isEnabled() && plate.Next->toolTip() == QStringLiteral("Stop 3 is not among these comments"), "a stop not at hand is unavailable, and says so");
		expect(plate.Previous != nullptr && plate.Previous->isEnabled(), "its other neighbour still leads");

		// A tour of one has nothing to step along.
		auto single = std::vector<conversation>{ };
		add_thread(single, "only", stop_body("Only stop", 1, "Alone.", 1), 4U);
		view.adopt(std::move(single));
		view.open_thread("only");
		settle();
		plate = open_plate(view);
		expect(plate.Plate != nullptr && plate.Previous == nullptr && plate.Next == nullptr, "a tour of one has no chevrons");
	}

	struct theme
	{
		core::appearance_mode Mode;
		const char*           Name;
	};
	constexpr auto themes = std::array{
		theme{ core::appearance_mode::light, "light" },
		theme{ core::appearance_mode::dark, "dark" },
		theme{ core::appearance_mode::high_contrast_light, "high-contrast-light" },
		theme{ core::appearance_mode::high_contrast_dark, "high-contrast-dark" },
	};

	/// \brief Every built-in theme: the plate's title and badge read against
	/// it, and renders of the notes and the open stop.
	void test_every_theme()
	{
		for (const auto& [mode, name] : themes) {
			auto profile = qt::appearance_profile{ };
			profile.select_mode(mode);
			const auto                  palette = profile.derive(QApplication::palette());
			qt::conversation_cards_view view;
			view.setPalette(palette);
			view.resize(440, 760);
			view.show();
			view.adopt(forest());
			view.set_tour_open_dispatch([](const std::string&, const core::published_tour_stop&) { });
			view.set_tour_open_offered(true);
			settle();
			save_render(view, QStringLiteral("tour-stop-notes-") + QString::fromLatin1(name));
			view.open_thread("stop-2");
			settle();
			auto* const card = view.open_card();
			expect(card != nullptr, "the stop opens in every theme");
			auto* const plate = posts_of(*card).front()->findChild<QWidget*>(QStringLiteral("threadPostTourStop"));
			expect(plate != nullptr, "with its plate");
			save_render(*card, QStringLiteral("tour-stop-open-") + QString::fromLatin1(name));
			save_render(*plate, QStringLiteral("tour-stop-plate-") + QString::fromLatin1(name));
			const auto image = plate->grab().toImage();
			const auto ratio = image.devicePixelRatio();
			// The marker's ring is the tour's route colour in every theme:
			// some pixel across its top edge is the route amber.
			const auto route  = QColor{ 0xD2, 0x99, 0x22 };
			auto       amber  = false;
			const auto centre = static_cast<int>((8 + 11) * ratio);
			for (auto y = static_cast<int>(7 * ratio); y <= static_cast<int>(12 * ratio) && !amber; ++y) {
				const auto ring = QColor{ image.pixel(centre, y) };
				amber           = std::abs(ring.red() - route.red()) <= 24 && std::abs(ring.green() - route.green()) <= 24 && std::abs(ring.blue() - route.blue()) <= 24;
			}
			expect(amber, "the route marker is drawn in the tour's colour");
			// [[BPR-1810]]. The stepper at the tour's first, middle and last stop.
			for (const auto& [thread, place] : std::array{ std::pair{ "stop-1", "first" }, std::pair{ "stop-2", "middle" }, std::pair{ "stop-3", "last" } }) {
				view.open_thread(thread);
				settle();
				const auto parts = open_plate(view);
				expect(parts.Previous != nullptr && parts.Next != nullptr, "every stop of the tour has its stepper in every theme");
				auto* const open = parts.Plate->findChild<QAbstractButton*>(QStringLiteral("threadPostTourOpen"));
				expect(open != nullptr && parts.Previous->nextInFocusChain() == parts.Next && parts.Next->nextInFocusChain() == open,
					   "Tab goes Previous, Next, then Open in tour");
				save_render(*view.open_card(), QStringLiteral("tour-stop-step-%1-%2").arg(QString::fromLatin1(place), QString::fromLatin1(name)));
			}
		}
	}
}

int main(int aArgc, char** aArgv)
{
	QApplication application{ aArgc, aArgv };
	// As the composition does at startup: code reads in the bundled code font.
	expect(qt::register_bundled_code_font(), "the bundled code font registers");
	test_stops_are_recognized_in_the_notes();
	test_the_open_stop_shows_its_plate_and_narrative();
	test_the_plate_steps_along_the_tour();
	test_every_theme();
	std::cout << "tour-stop-card-tests:pass\n";
	return EXIT_SUCCESS;
}
