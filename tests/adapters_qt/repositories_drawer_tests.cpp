#include "bpr/qt/repositories_drawer.hpp"

#include "bpr/core/review_queue.hpp"
#include "bpr/core/review_queue_attention.hpp"

#include <QApplication>

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto item(
		const std::string&           aRepository,
		const std::int64_t           aNumber,
		const core::attention_bucket aBucket,
		const core::github_role      aRole   = core::github_role::requested_reviewer,
		const std::string&           aTitle  = { },
		const std::uint32_t          aUnseen = 0U) -> core::queue_item
	{
		return core::queue_item{
			.Ref = core::pull_request_ref{
				.Host       = "github.com",
				.Account    = "mike",
				.Repository = aRepository,
				.Number     = aNumber,
			},
			.Membership = core::queue_membership{ .Role = aRole },
			.Bucket     = aBucket,
			.Facts      = core::queue_item_facts{ .Title = aTitle, .Author = "octocat", .UnseenComments = aUnseen },
		};
	}

	auto test_tree_groups_the_queue_by_repository_with_reasons() -> void
	{
		qt::repositories_tree_view view;
		view.resize(300, 400);
		view.show();
		QApplication::processEvents();
		const auto items = std::vector<core::queue_item>{
			item("EpicGames/UnrealEngine", 15150, core::attention_bucket::authored_awaiting_reviewer_action, core::github_role::author),
			item("EpicGames/UnrealEngine", 14366, core::attention_bucket::changed_since_my_review,
				 core::github_role::requested_reviewer, "Preserve the DRCS font", 3U),
			item("MikeMarcin/BetterPullReview", 1773, core::attention_bucket::quiet, core::github_role::participant),
			item("EpicGames/UnrealEngine", 15157, core::attention_bucket::initial_review_requested),
			item("EpicGames/UnrealEngine", 15059, core::attention_bucket::observed_with_new_activity, core::github_role::none),
		};
		auto opened = std::vector<core::queue_item>{ };
		view.set_open_dispatch([&opened](const core::queue_item& aItem) { opened.push_back(aItem); });
		view.apply(items);
		QApplication::processEvents();

		auto* const model = view.tree_model();
		expect(model->rowCount() == 2 && model->item_count() == 5U,
			   "every repository the queue names is a row with its pull requests beneath");
		const auto& repositories = model->repositories();
		expect(repositories[0].Repository == "EpicGames/UnrealEngine" && repositories[0].Items.size() == 4U
				   && repositories[0].NeedsMe == 2U && repositories[1].Repository == "MikeMarcin/BetterPullReview"
				   && repositories[1].NeedsMe == 0U,
			   "repositories keep Home's order and count the pull requests that need me");
		const auto epic = model->index(0, qt::repositories_tree_model::name_column);
		expect(epic.data().toString() == QStringLiteral("EpicGames/UnrealEngine")
				   && epic.siblingAtColumn(qt::repositories_tree_model::reason_column).data().toString()
					   == QStringLiteral("2 of 4")
				   && model->index(1, qt::repositories_tree_model::reason_column).data().toString()
					   == QStringLiteral("1"),
			   "a repository row reads needs-you of total, or the total alone");
		const auto first = model->index(0, qt::repositories_tree_model::name_column, epic);
		expect(model->rowCount(epic) == 4 && first.isValid()
				   && first.data(qt::repositories_tree_model::number_role).toLongLong() == 15157
				   && first.siblingAtColumn(qt::repositories_tree_model::reason_column).data().toString()
					   == QStringLiteral("requested")
				   && first.data(qt::repositories_tree_model::needs_me_role).toBool(),
			   "pull requests hang beneath in Home's order with a one-word reason that needs me");
		auto reasons = std::vector<QString>{ };
		for (int row = 0; row < model->rowCount(epic); ++row) {
			reasons.push_back(model->index(row, qt::repositories_tree_model::reason_column, epic).data().toString());
		}
		expect(reasons == std::vector<QString>{ QStringLiteral("requested"), QStringLiteral("changed"), QStringLiteral("waiting"), QStringLiteral("following") },
			   "the reason words follow the buckets in Home's order");
		expect(model->needs_me_count() == 2U, "the rail's count is the pull requests that need me");
		// [[BPR-0659]]. The title after the number, the unread dot's role, and
		// both absent when the host read nothing.
		const auto titled = model->index(1, qt::repositories_tree_model::name_column, epic);
		expect(titled.data(qt::repositories_tree_model::number_role).toLongLong() == 14366
				   && titled.data().toString() == QStringLiteral("#14366 Preserve the DRCS font")
				   && titled.data(qt::repositories_tree_model::title_role).toString() == QStringLiteral("Preserve the DRCS font")
				   && titled.data(qt::repositories_tree_model::unseen_role).toBool()
				   && titled.data(Qt::ToolTipRole).toString().contains(QStringLiteral("3 unseen comments")),
			   "a pull request the host read reads as number and title, and carries the unread dot");
		expect(first.data().toString() == QStringLiteral("#15157")
				   && !first.data(qt::repositories_tree_model::unseen_role).toBool()
				   && first.data(qt::repositories_tree_model::title_role).toString().isEmpty(),
			   "a pull request without a title reads as its number alone, with no dot");
		expect(epic.data(qt::repositories_tree_model::title_role).isNull()
				   && !epic.data(qt::repositories_tree_model::unseen_role).toBool(),
			   "a repository row has no title and no dot");
		const auto stress = model->index_for(core::pull_request_ref{
			.Host = "github.com", .Account = "mike", .Repository = "MikeMarcin/BetterPullReview", .Number = 1773 });
		expect(stress.isValid() && model->item_at(stress).has_value()
				   && model->item_at(stress)->Ref.Number == 1773
				   && !model->item_at(model->index(1, 0)).has_value(),
			   "a pull request is found by its ref and a repository row is no item");
		expect(qt::repository_reason_word(core::attention_bucket::fixes_awaiting_verification) == QStringLiteral("verify")
				   && qt::repository_reason_word(core::attention_bucket::quiet) == QStringLiteral("quiet")
				   && !qt::repository_needs_me(core::attention_bucket::quiet),
			   "every bucket has a word and only the first four need me");

		view.setCurrentIndex(first);
		QApplication::processEvents();
		expect(opened.size() == 1U && opened.front().Ref.Number == 15157,
			   "choosing a pull request row reports it");
		view.setCurrentIndex(epic);
		QApplication::processEvents();
		expect(opened.size() == 1U, "choosing a repository row reports nothing");
		view.apply({ });
		expect(model->rowCount() == 0 && model->needs_me_count() == 0U, "an empty queue empties the tree");
		view.hide();
	}

	// [[BPR-0659]]. Saved repositories: an empty row for one with nothing
	// of interest, the saved mark on one that also has items, and Save and
	// Forget on the current repository row leaving as intents.
	auto test_saved_repositories_are_rows_with_save_and_forget() -> void
	{
		qt::repositories_tree_view view;
		view.resize(300, 400);
		view.show();
		QApplication::processEvents();
		const auto items = std::vector<core::queue_item>{
			item("EpicGames/UnrealEngine", 15157, core::attention_bucket::initial_review_requested),
		};
		const auto saved = std::vector<application::saved_repository>{
			{ .Host = "github.com", .Account = "mike", .Repository = "MikeMarcin/BetterPullReview" },
			{ .Host = "github.com", .Account = "mike", .Repository = "EpicGames/UnrealEngine" },
		};
		auto intents = std::vector<application::saved_repository_intent>{ };
		view.set_saved_repository_dispatch([&intents](application::saved_repository_intent aIntent) {
			intents.push_back(std::move(aIntent));
		});
		view.apply(items, saved);
		QApplication::processEvents();
		auto* const model = view.tree_model();
		expect(model->rowCount() == 2 && model->item_count() == 1U,
			   "a saved repository with nothing of interest is a row of its own, after the ones with items");
		const auto epic = model->index(0, qt::repositories_tree_model::name_column);
		const auto mine = model->index(1, qt::repositories_tree_model::name_column);
		expect(epic.data(qt::repositories_tree_model::saved_role).toBool()
				   && mine.data(qt::repositories_tree_model::saved_role).toBool()
				   && mine.data().toString() == QStringLiteral("MikeMarcin/BetterPullReview")
				   && mine.siblingAtColumn(qt::repositories_tree_model::reason_column).data().toString()
					   == QStringLiteral("saved")
				   && model->rowCount(mine) == 0,
			   "both are marked saved; the empty one reads saved with nothing beneath");
		expect(!view.save_repository_action()->isEnabled() && !view.forget_repository_action()->isEnabled(),
			   "with no current row neither Save nor Forget is offered");
		view.setCurrentIndex(mine);
		QApplication::processEvents();
		expect(!view.save_repository_action()->isEnabled() && view.forget_repository_action()->isEnabled(),
			   "a saved repository row offers Forget and not Save");
		view.forget_repository_action()->trigger();
		expect(intents.size() == 1U && intents.back().Action == application::saved_repository_action::remove
				   && intents.back().Repository == saved.front(),
			   "Forget leaves as a remove intent naming the row's host, account and repository");

		view.apply(items, { });
		QApplication::processEvents();
		expect(model->rowCount() == 1 && !model->index(0, 0).data(qt::repositories_tree_model::saved_role).toBool(),
			   "without saved repositories only the ones with items remain, unmarked");
		view.setCurrentIndex(model->index(0, qt::repositories_tree_model::name_column));
		QApplication::processEvents();
		expect(view.save_repository_action()->isEnabled() && !view.forget_repository_action()->isEnabled(),
			   "an unsaved repository row offers Save and not Forget");
		view.save_repository_action()->trigger();
		expect(intents.size() == 2U && intents.back().Action == application::saved_repository_action::add
				   && intents.back().Repository
					   == application::saved_repository{
						   .Host = "github.com", .Account = "mike", .Repository = "EpicGames/UnrealEngine" },
			   "Save leaves as an add intent for the row");
		const auto item15157 = model->index(0, qt::repositories_tree_model::name_column, model->index(0, 0));
		view.setCurrentIndex(item15157);
		QApplication::processEvents();
		expect(!view.save_repository_action()->isEnabled() && !view.forget_repository_action()->isEnabled(),
			   "a pull request row offers neither");
		view.hide();
	}
	// [[BPR-0981]]. RFC-BPR-0025's resident filter: repository, host, title
	// or number, letters in order; a matching pull request keeps its
	// repository row, a matching repository keeps every row beneath, and
	// the chosen pull request is current again after every rebuild without
	// a second open.
	auto test_the_filter_is_resident_keeps_context_and_the_chosen_row() -> void
	{
		qt::repositories_tree_view view;
		view.resize(300, 400);
		view.show();
		QApplication::processEvents();
		const auto items = std::vector<core::queue_item>{
			item("EpicGames/UnrealEngine", 15157, core::attention_bucket::initial_review_requested),
			item("EpicGames/UnrealEngine", 14366, core::attention_bucket::changed_since_my_review,
				 core::github_role::requested_reviewer, "Preserve the DRCS font", 3U),
			item("MikeMarcin/BetterPullReview", 1773, core::attention_bucket::quiet, core::github_role::participant,
				 "Drawer filters"),
		};
		const auto saved = std::vector<application::saved_repository>{
			{ .Host = "github.com", .Account = "mike", .Repository = "MikeMarcin/Quiet" },
		};
		auto opened = std::vector<core::queue_item>{ };
		view.set_open_dispatch([&opened](const core::queue_item& aItem) { opened.push_back(aItem); });
		view.apply(items, saved);
		QApplication::processEvents();
		auto* const model = view.tree_model();
		expect(model->rowCount() == 3 && model->item_count() == 3U && model->needs_me_count() == 2U,
			   "without a filter every repository and pull request is listed");

		const auto chosen = model->index(1, qt::repositories_tree_model::name_column, model->index(0, 0));
		view.setCurrentIndex(chosen);
		QApplication::processEvents();
		expect(opened.size() == 1U && opened.back().Ref.Number == 14366
				   && view.current_pull_request().has_value() && view.current_pull_request()->Number == 14366,
			   "choosing a pull request opens it once and the tree remembers it");

		view.set_filter(QStringLiteral("drcs"));
		QApplication::processEvents();
		expect(model->rowCount() == 1 && model->item_count() == 1U
				   && model->index(0, 0).data().toString() == QStringLiteral("EpicGames/UnrealEngine")
				   && model->index(0, qt::repositories_tree_model::reason_column).data().toString() == QStringLiteral("1 of 1")
				   && model->index(0, 0, model->index(0, 0)).data(qt::repositories_tree_model::number_role).toLongLong() == 14366
				   && model->needs_me_count() == 2U,
			   "a title match keeps its repository row above it, counts what is shown, and the rail's count is the queue's");
		expect(model->item_at(view.currentIndex()).has_value() && model->item_at(view.currentIndex())->Ref.Number == 14366
				   && opened.size() == 1U,
			   "the chosen pull request is current again after the filter rebuilds, without a second open");
		view.set_filter(QStringLiteral("1773"));
		expect(model->rowCount() == 1 && model->item_count() == 1U
				   && model->index(0, 0).data().toString() == QStringLiteral("MikeMarcin/BetterPullReview")
				   && !view.currentIndex().isValid() && opened.size() == 1U,
			   "a number match is found by its digits; a hidden chosen row is simply not current");
		view.set_filter(QStringLiteral("epic"));
		expect(model->rowCount() == 1 && model->item_count() == 2U
				   && model->item_at(view.currentIndex())->Ref.Number == 14366,
			   "a repository match keeps every pull request beneath it and the chosen one current");
		view.set_filter(QStringLiteral("quiet"));
		expect(model->rowCount() == 1 && model->item_count() == 0U
				   && model->index(0, 0).data(qt::repositories_tree_model::saved_role).toBool(),
			   "a saved repository with nothing beneath is found by its own name");
		view.set_filter(QStringLiteral("github.com"));
		expect(model->rowCount() == 3, "a host match keeps every repository on that host");
		view.set_filter(QStringLiteral("nothing here"));
		expect(model->rowCount() == 0 && model->item_count() == 0U && model->needs_me_count() == 2U,
			   "a filter nothing matches leaves the tree honestly empty");
		view.set_filter(QString{ });
		QApplication::processEvents();
		expect(model->rowCount() == 3 && model->item_at(view.currentIndex())->Ref.Number == 14366 && opened.size() == 1U,
			   "clearing the filter restores every row and the chosen pull request, without a second open");
		view.apply(items, saved);
		QApplication::processEvents();
		expect(model->item_at(view.currentIndex())->Ref.Number == 14366 && opened.size() == 1U,
			   "a re-published queue keeps the chosen pull request current without opening it again");
		view.hide();
	}

	// [[BPR-0981]]. The current row is the window's open pull request, not
	// merely the last row chosen here: a pull request opened elsewhere takes
	// the current row without an open, so the row chosen before is a fresh
	// gesture again rather than a current index that swallows the click.
	auto test_the_open_pull_request_is_current_however_it_was_opened() -> void
	{
		qt::repositories_tree_view view;
		view.resize(300, 400);
		view.show();
		QApplication::processEvents();
		const auto items = std::vector<core::queue_item>{
			item("EpicGames/UnrealEngine", 14366, core::attention_bucket::changed_since_my_review,
				 core::github_role::requested_reviewer, "Preserve the DRCS font"),
			item("MikeMarcin/BetterPullReview", 1773, core::attention_bucket::quiet, core::github_role::participant,
				 "Drawer filters"),
		};
		auto opened = std::vector<core::queue_item>{ };
		view.set_open_dispatch([&opened](const core::queue_item& aItem) { opened.push_back(aItem); });
		view.apply(items);
		QApplication::processEvents();
		auto* const model = view.tree_model();
		const auto  row   = [model](const std::string& aRepository, const std::int64_t aNumber) {
			return model->index_for(aRepository, aNumber);
		};

		view.setCurrentIndex(row("EpicGames/UnrealEngine", 14366));
		QApplication::processEvents();
		expect(opened.size() == 1U && view.current_pull_request()->Host == "github.com",
			   "a row chosen here opens once and is kept with its exact host and account");
		view.follow_open_pull_request("EpicGames/UnrealEngine", 14366);
		expect(view.current_pull_request()->Host == "github.com" && opened.size() == 1U,
			   "the window confirming the same pull request keeps the exact choice and opens nothing");

		view.follow_open_pull_request("MikeMarcin/BetterPullReview", 1773);
		QApplication::processEvents();
		expect(model->item_at(view.currentIndex()).has_value()
				   && model->item_at(view.currentIndex())->Ref.Number == 1773 && opened.size() == 1U,
			   "a pull request the window opened elsewhere takes the current row without an open");
		view.setCurrentIndex(row("EpicGames/UnrealEngine", 14366));
		QApplication::processEvents();
		expect(opened.size() == 2U && opened.back().Ref.Number == 14366,
			   "the row chosen before is a fresh gesture again and opens its pull request");

		view.forget_open_pull_request();
		QApplication::processEvents();
		expect(!view.currentIndex().isValid() && !view.current_pull_request().has_value() && opened.size() == 2U,
			   "with no pull request open no row is current and nothing opens");
		view.follow_open_pull_request("octo/elsewhere", 5);
		expect(!view.currentIndex().isValid() && opened.size() == 2U,
			   "an open pull request the queue does not list leaves nothing current");
		auto later = items;
		later.push_back(item("octo/elsewhere", 5, core::attention_bucket::quiet, core::github_role::participant));
		view.apply(later);
		QApplication::processEvents();
		expect(model->item_at(view.currentIndex()).has_value()
				   && model->item_at(view.currentIndex())->Ref.Repository == "octo/elsewhere" && opened.size() == 2U,
			   "once the queue lists the open pull request its row is current, without an open");
		view.hide();
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_tree_groups_the_queue_by_repository_with_reasons();
	test_saved_repositories_are_rows_with_save_and_forget();
	test_the_filter_is_resident_keeps_context_and_the_chosen_row();
	test_the_open_pull_request_is_current_however_it_was_opened();
	std::cout << "repositories-drawer-tests:pass\n";
	return EXIT_SUCCESS;
}
