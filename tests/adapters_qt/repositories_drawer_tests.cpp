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
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_tree_groups_the_queue_by_repository_with_reasons();
	test_saved_repositories_are_rows_with_save_and_forget();
	std::cout << "repositories-drawer-tests:pass\n";
	return EXIT_SUCCESS;
}
