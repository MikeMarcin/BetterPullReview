#include "bpr/qt/review_landing_model.hpp"

#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QPersistentModelIndex>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	/// \brief Counts the model signals this suite reasons about.
	///
	/// \details `QSignalSpy` lives in Qt6::Test, which this build does not
	/// link. Connecting directly costs a few lines and adds no dependency.
	struct signal_recorder
	{
		explicit signal_recorder(QAbstractItemModel& aModel)
		{
			QObject::connect(
				&aModel, &QAbstractItemModel::modelReset, [this]() {
					++Resets;
				});
			QObject::connect(
				&aModel,
				&QAbstractItemModel::rowsInserted,
				[this](const QModelIndex&, int, int) { ++Insertions; });
			QObject::connect(
				&aModel,
				&QAbstractItemModel::rowsRemoved,
				[this](const QModelIndex&, int, int) { ++Removals; });
			QObject::connect(
				&aModel,
				&QAbstractItemModel::rowsMoved,
				[this](const QModelIndex&, int, int, const QModelIndex&, int) {
					++Moves;
				});
			QObject::connect(
				&aModel,
				&QAbstractItemModel::dataChanged,
				[this](const QModelIndex&, const QModelIndex&, const QList<int>&) {
					++Changes;
				});
		}

		int Resets     = 0;
		int Insertions = 0;
		int Removals   = 0;
		int Moves      = 0;
		int Changes    = 0;
	};

	[[nodiscard]] auto ref(const std::int64_t aNumber) -> core::pull_request_ref
	{
		return core::pull_request_ref{
			.Host       = "github.com",
			.Account    = "reviewer",
			.Repository = "octo/app",
			.Number     = aNumber,
		};
	}

	[[nodiscard]] auto item(
		const std::int64_t           aNumber,
		const core::attention_bucket aBucket,
		const core::queue_membership aMembership = { }) -> core::queue_item
	{
		return core::queue_item{
			.Ref        = ref(aNumber),
			.Membership = aMembership,
			.Bucket     = aBucket,
		};
	}

	// The requirement the whole model turns on. An item that changes bucket
	// moves; it is not removed and re-inserted, and every other row's
	// persistent index survives.
	//
	// A positional diff would report the same change as a run of data changes
	// on rows that did not change. That is correct only in the sense a reset is
	// correct: the view re-reads everything and the reviewer loses their place.
	void test_a_reordered_item_moves_rather_than_being_replaced()
	{
		qt::review_landing_model model;
		model.apply(std::array{
			item(1, core::attention_bucket::quiet),
			item(2, core::attention_bucket::quiet),
			item(3, core::attention_bucket::quiet),
		});

		const auto first  = QPersistentModelIndex{ model.index(0) };
		const auto second = QPersistentModelIndex{ model.index(1) };
		const auto third  = QPersistentModelIndex{ model.index(2) };

		const signal_recorder recorder{ model };

		// The third item becomes urgent and the core puts it first.
		model.apply(std::array{
			item(3, core::attention_bucket::initial_review_requested),
			item(1, core::attention_bucket::quiet),
			item(2, core::attention_bucket::quiet),
		});

		expect(recorder.Resets == 0, "nothing was reset");
		expect(recorder.Removals == 0, "nothing was removed");
		expect(recorder.Insertions == 0, "nothing was inserted");
		expect(recorder.Moves == 1, "exactly one row moved");
		expect(
			recorder.Changes == 1,
			"and exactly one row's content changed, the one that did");

		expect(third.isValid(), "the moved row's persistent index survives");
		expect(third.row() == 0, "and follows it to its new place");
		expect(
			first.isValid() && first.row() == 1,
			"a row that did not change follows the shift");
		expect(second.isValid() && second.row() == 2, "and so does the other");
	}

	void test_an_added_item_inserts_and_a_departed_one_removes()
	{
		qt::review_landing_model model;
		model.apply(std::array{
			item(1, core::attention_bucket::quiet),
			item(2, core::attention_bucket::quiet),
		});

		{
			const signal_recorder recorder{ model };
			model.apply(std::array{
				item(1, core::attention_bucket::quiet),
				item(2, core::attention_bucket::quiet),
				item(3, core::attention_bucket::quiet),
			});
			expect(recorder.Insertions == 1, "one row was inserted");
			expect(recorder.Removals == 0, "and nothing removed");
			expect(recorder.Resets == 0, "without a reset");
		}

		{
			const signal_recorder recorder{ model };
			model.apply(std::array{
				item(1, core::attention_bucket::quiet),
				item(3, core::attention_bucket::quiet),
			});
			expect(recorder.Removals == 1, "one row was removed");
			expect(recorder.Insertions == 0, "and nothing inserted");
			// Removing row 1 already shifts row 2 into its place, so the
			// survivor needs no move. A model that emitted one anyway would be
			// telling the view about work it had already done.
			expect(recorder.Moves == 0, "and the survivor needed no move");
			expect(recorder.Resets == 0, "still without a reset");
		}

		expect(model.rowCount() == 2, "two rows remain");
		expect(
			model.item_at(model.index(1))->Ref.Number == 3,
			"and they are the right ones in the right order");
	}

	// Applying the same queue again must be silent. A model that re-emitted on
	// every refresh would make a periodic queue update look like constant
	// activity, and a view would repaint on a schedule for no reason.
	void test_reapplying_an_unchanged_queue_says_nothing()
	{
		qt::review_landing_model model;
		const auto               queue = std::array{
			item(1, core::attention_bucket::initial_review_requested),
			item(2, core::attention_bucket::quiet),
		};
		model.apply(queue);

		const signal_recorder recorder{ model };
		model.apply(queue);

		expect(
			recorder.Resets == 0 && recorder.Insertions == 0
				&& recorder.Removals == 0 && recorder.Moves == 0
				&& recorder.Changes == 0,
			"an unchanged queue emits nothing at all");
	}

	// The reason is a core value. A model that composed its own explanation
	// would be deciding policy while looking like presentation.
	void test_the_reason_comes_from_the_core()
	{
		qt::review_landing_model model;
		model.apply(
			std::array{ item(1, core::attention_bucket::fixes_awaiting_verification) });

		const auto reason
			= model.data(model.index(0), qt::review_landing_model::reason_role)
				  .toString();
		expect(
			reason.contains(QString::fromUtf8(
				core::explanation(core::attention_bucket::fixes_awaiting_verification)
					.data(),
				static_cast<qsizetype>(
					core::explanation(
						core::attention_bucket::fixes_awaiting_verification)
						.size()))),
			"the reason is the core's sentence, not one the adapter wrote");
	}

	// An item here only because a rule matched says which rule. An item someone
	// asked for does not: burying "you were asked" under "you watch src/" would
	// tell the reviewer the wrong thing about what is expected of them.
	void test_the_rule_is_named_only_when_it_is_why_the_item_is_here()
	{
		qt::review_landing_model model;

		const auto watched = core::queue_membership{
			.Role        = core::github_role::none,
			.MatchedRule = true,
			.Rule        = core::observer_rule{
				.Kind      = core::observer_rule_kind::path,
				.Pattern   = "src/parser",
				.Excluding = false,
			},
		};
		auto assigned = watched;
		assigned.Role = core::github_role::requested_reviewer;

		model.apply(std::array{
			item(1, core::attention_bucket::observed_with_new_activity, watched),
			item(2, core::attention_bucket::initial_review_requested, assigned),
		});

		expect(
			model.data(model.index(0), qt::review_landing_model::reason_role)
				.toString()
				.contains(QStringLiteral("src/parser")),
			"a watched item names the rule that brought it in");
		expect(
			!model.data(model.index(1), qt::review_landing_model::reason_role)
				 .toString()
				 .contains(QStringLiteral("src/parser")),
			"an assigned one does not, so being asked is not buried");
	}

	// Every state reads without colour and through a screen reader.
	void test_every_bucket_reads_without_colour()
	{
		const auto buckets = std::array{
			core::attention_bucket::initial_review_requested,
			core::attention_bucket::changed_since_my_review,
			core::attention_bucket::new_comments_or_author_replies,
			core::attention_bucket::fixes_awaiting_verification,
			core::attention_bucket::authored_awaiting_reviewer_action,
			core::attention_bucket::observed_with_new_activity,
			core::attention_bucket::quiet,
		};

		auto badges = std::vector<QString>{ };
		for (const auto bucket : buckets) {
			const auto badge = qt::badge_for(bucket);
			expect(!badge.isEmpty(), "every bucket has a badge");
			for (const auto& other : badges) {
				expect(other != badge, "two buckets never share a badge");
			}
			badges.push_back(badge);
		}

		qt::review_landing_model model;
		model.apply(
			std::array{ item(1, core::attention_bucket::changed_since_my_review) });
		expect(
			model.data(model.index(0), Qt::DisplayRole)
				.toString()
				.startsWith(
					qt::badge_for(core::attention_bucket::changed_since_my_review)),
			"the badge leads the display text");
		expect(
			!model.data(model.index(0), Qt::AccessibleDescriptionRole)
				 .toString()
				 .isEmpty(),
			"and the reason is announced, not only drawn");
	}

	// There is no mutating entry point beyond apply and clear. The presentation
	// cannot enroll, depart, or re-bucket anything.
	void test_the_model_cannot_be_edited()
	{
		qt::review_landing_model model;
		model.apply(std::array{ item(1, core::attention_bucket::quiet) });

		expect(
			!model.flags(model.index(0)).testFlag(Qt::ItemIsEditable),
			"no row is editable");
		expect(
			!model.setData(
				model.index(0),
				QStringLiteral("anything"),
				qt::review_landing_model::bucket_role),
			"and the base model refuses a write, because none was implemented");
		expect(
			model.item_at(model.index(0))->Bucket
				== core::attention_bucket::quiet,
			"so the bucket is still the core's");
	}

	void test_clearing_removes_rather_than_resets()
	{
		qt::review_landing_model model;
		model.apply(std::array{
			item(1, core::attention_bucket::quiet),
			item(2, core::attention_bucket::quiet),
		});

		const signal_recorder recorder{ model };
		model.clear();

		expect(recorder.Resets == 0, "clearing does not reset");
		expect(recorder.Removals == 1, "it removes the rows it had");
		expect(model.rowCount() == 0, "and the model is empty");

		model.clear();
		expect(
			recorder.Removals == 1,
			"clearing an empty model says nothing further");
	}

	void test_filters_and_explicit_sorts_preserve_stable_identity()
	{
		auto source    = qt::review_landing_model{ };
		auto proxy     = qt::review_landing_filter_model{ source };
		auto requested = item(
			9,
			core::attention_bucket::initial_review_requested,
			core::queue_membership{
				.Role        = core::github_role::requested_reviewer,
				.MatchedRule = true,
				.Rule        = core::observer_rule{
					.Kind    = core::observer_rule_kind::repository,
					.Pattern = "z/repo" } });
		requested.Ref.Repository = "z/repo";
		auto authored            = item(
			5,
			core::attention_bucket::new_comments_or_author_replies,
			core::queue_membership{
				.Role        = core::github_role::author,
				.MatchedRule = true,
				.Rule        = core::observer_rule{
					.Kind    = core::observer_rule_kind::path,
					.Pattern = "src" } });
		authored.Ref.Repository = "a/repo";
		auto participant        = item(
			2,
			core::attention_bucket::fixes_awaiting_verification,
			core::queue_membership{ .Role = core::github_role::participant });
		participant.Ref.Repository = "a/repo";
		auto observed              = item(
			7,
			core::attention_bucket::quiet,
			core::queue_membership{
				.MatchedRule = true,
				.Rule        = core::observer_rule{
					.Kind    = core::observer_rule_kind::author,
					.Pattern = "octocat" } });
		observed.Ref.Repository = "b/repo";
		source.apply(std::array{ requested, authored, participant, observed });

		expect(
			proxy.item_at(proxy.index(0, 0))->Ref.Number == 9
				&& proxy.item_at(proxy.index(1, 0))->Ref.Number == 5
				&& proxy.item_at(proxy.index(2, 0))->Ref.Number == 2
				&& proxy.item_at(proxy.index(3, 0))->Ref.Number == 7,
			"the default proxy preserves the core's exact priority order");
		auto                  selected = QPersistentModelIndex{ proxy.index(1, 0) };
		const signal_recorder recorder{ proxy };

		proxy.apply(application::queue_presentation_preferences{
			.Repository = "a/repo",
			.Sort       = application::queue_sort_order::repository,
		});
		expect(
			proxy.rowCount() == 2
				&& proxy.item_at(proxy.index(0, 0))->Ref.Number == 2
				&& proxy.item_at(proxy.index(1, 0))->Ref.Number == 5,
			"repository filtering and its explicit alternate sort are deterministic");
		expect(
			selected.isValid()
				&& proxy.item_at(selected)->Ref == authored.Ref,
			"the selected pull-request identity survives filter and sort changes");
		expect(recorder.Resets == 0, "filtering and sorting never reset the model");

		proxy.apply(application::queue_presentation_preferences{
			.Role = core::github_role::author,
		});
		expect(
			proxy.rowCount() == 1
				&& proxy.item_at(proxy.index(0, 0))->Ref == authored.Ref,
			"the exact RFC role filter is applied");
		proxy.apply(application::queue_presentation_preferences{
			.ObserverReason = core::observer_rule_kind::author,
		});
		expect(
			proxy.rowCount() == 1
				&& proxy.item_at(proxy.index(0, 0))->Ref == observed.Ref,
			"the exact retained observer reason is applied");
		proxy.apply(application::queue_presentation_preferences{
			.Bucket = core::attention_bucket::fixes_awaiting_verification,
		});
		expect(
			proxy.rowCount() == 1
				&& proxy.item_at(proxy.index(0, 0))->Ref == participant.Ref,
			"the exact core-decided attention bucket is applied");
		proxy.apply(application::queue_presentation_preferences{
			.Quiet = application::queue_quiet_filter::quiet,
		});
		expect(
			proxy.rowCount() == 1
				&& proxy.item_at(proxy.index(0, 0))->Ref == observed.Ref,
			"quiet/current is explicit rather than inferred from a sort");
		proxy.apply(application::queue_presentation_preferences{
			.Quiet = application::queue_quiet_filter::attention,
			.Sort  = application::queue_sort_order::pull_request_number,
		});
		expect(
			proxy.rowCount() == 3
				&& proxy.item_at(proxy.index(0, 0))->Ref.Number == 2
				&& proxy.item_at(proxy.index(2, 0))->Ref.Number == 9,
			"the other explicit alternate sort keeps a total identity tie-break");
	}

	void test_a_large_queue_filter_emits_one_bounded_change()
	{
		auto source = qt::review_landing_model{ };
		auto proxy  = qt::review_landing_filter_model{ source };
		auto queue  = std::vector<core::queue_item>{ };
		queue.reserve(2'048U);
		for (auto number = std::int64_t{ 1 }; number <= 2'048; ++number) {
			queue.push_back(item(
				number,
				number <= 1'024
					? core::attention_bucket::initial_review_requested
					: core::attention_bucket::quiet));
		}
		source.apply(queue);
		const signal_recorder recorder{ proxy };
		proxy.apply(application::queue_presentation_preferences{
			.Quiet = application::queue_quiet_filter::attention,
		});
		expect(
			proxy.rowCount() == 1'024,
			"the large queue retains the exact matching half");
		expect(
			recorder.Resets == 0 && recorder.Removals == 1,
			"one contiguous large-queue change is one bounded removal, not a reset");
	}

	void test_switching_directly_between_alternate_sorts_reorders()
	{
		auto source           = qt::review_landing_model{ };
		auto proxy            = qt::review_landing_filter_model{ source };
		auto first            = item(9, core::attention_bucket::quiet);
		first.Ref.Repository  = "a/repo";
		auto second           = item(1, core::attention_bucket::quiet);
		second.Ref.Repository = "z/repo";
		source.apply(std::array{ first, second });
		const signal_recorder recorder{ proxy };

		proxy.apply(application::queue_presentation_preferences{
			.Sort = application::queue_sort_order::repository,
		});
		expect(
			proxy.item_at(proxy.index(0, 0))->Ref.Number == 9
				&& proxy.item_at(proxy.index(1, 0))->Ref.Number == 1,
			"the repository alternate sort follows repository identity");

		proxy.apply(application::queue_presentation_preferences{
			.Sort = application::queue_sort_order::pull_request_number,
		});
		expect(
			proxy.item_at(proxy.index(0, 0))->Ref.Number == 1
				&& proxy.item_at(proxy.index(1, 0))->Ref.Number == 9,
			"a direct alternate-sort switch re-evaluates the comparator");
		expect(recorder.Resets == 0, "alternate-sort switching never resets the model");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QCoreApplication application{ argc, argv };

	test_a_reordered_item_moves_rather_than_being_replaced();
	test_an_added_item_inserts_and_a_departed_one_removes();
	test_reapplying_an_unchanged_queue_says_nothing();
	test_the_reason_comes_from_the_core();
	test_the_rule_is_named_only_when_it_is_why_the_item_is_here();
	test_every_bucket_reads_without_colour();
	test_the_model_cannot_be_edited();
	test_clearing_removes_rather_than_resets();
	test_filters_and_explicit_sorts_preserve_stable_identity();
	test_a_large_queue_filter_emits_one_bounded_change();
	test_switching_directly_between_alternate_sorts_reorders();

	std::cout << "review-landing-model-tests:pass\n";
	return EXIT_SUCCESS;
}
