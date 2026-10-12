#include "bpr/qt/review_landing_model.hpp"

#include "bpr/qt/home_row_delegate.hpp"

#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QPersistentModelIndex>
#include <QString>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
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
		// Every connection names Context as its receiver. Without one, a
		// connection outlives the recorder that owns the counters, because it
		// is the sender that keeps it alive. A later block on the same model
		// would then fire this recorder's lambda too, incrementing a `this`
		// whose storage the next recorder has already reused. Destroying
		// Context with the recorder ends the connection with it.
		explicit signal_recorder(QAbstractItemModel& aModel)
		{
			QObject::connect(
				&aModel, &QAbstractItemModel::modelReset, &Context, [this]() {
					++Resets;
				});
			QObject::connect(
				&aModel,
				&QAbstractItemModel::rowsInserted,
				&Context,
				[this](const QModelIndex&, int, int) { ++Insertions; });
			QObject::connect(
				&aModel,
				&QAbstractItemModel::rowsRemoved,
				&Context,
				[this](const QModelIndex&, int, int) { ++Removals; });
			QObject::connect(
				&aModel,
				&QAbstractItemModel::rowsMoved,
				&Context,
				[this](const QModelIndex&, int, int, const QModelIndex&, int) {
					++Moves;
				});
			QObject::connect(
				&aModel,
				&QAbstractItemModel::dataChanged,
				&Context,
				[this](const QModelIndex&, const QModelIndex&, const QList<int>&) {
					++Changes;
				});
		}

		QObject Context;
		int     Resets     = 0;
		int     Insertions = 0;
		int     Removals   = 0;
		int     Moves      = 0;
		int     Changes    = 0;
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
	// [[BPR-0650]]. Home's groups follow the buckets, the action list's
	// columns come from the item's facts and membership, and the open bar
	// reads exactly the forms the design names.
	void test_home_groups_columns_and_the_open_bar_grammar()
	{
		using bucket = core::attention_bucket;
		expect(qt::home_group_of(bucket::initial_review_requested) == qt::home_group::needs_you
				   && qt::home_group_of(bucket::changed_since_my_review) == qt::home_group::needs_you
				   && qt::home_group_of(bucket::new_comments_or_author_replies) == qt::home_group::needs_you
				   && qt::home_group_of(bucket::fixes_awaiting_verification) == qt::home_group::needs_you
				   && qt::home_group_of(bucket::authored_awaiting_reviewer_action) == qt::home_group::waiting_on_others
				   && qt::home_group_of(bucket::observed_with_new_activity) == qt::home_group::following
				   && qt::home_group_of(bucket::quiet) == qt::home_group::following,
			   "the four buckets that need me, the one that waits on others, and the two I follow");
		expect(qt::home_group_label(qt::home_group::needs_you) == QStringLiteral("Needs me")
				   && qt::home_group_label(qt::home_group::waiting_on_others) == QStringLiteral("Waiting on others")
				   && qt::home_group_label(qt::home_group::following) == QStringLiteral("Following"),
			   "the headings are first person and one line");

		qt::review_landing_model model;
		auto                     titled = item(7, bucket::changed_since_my_review,
											   core::queue_membership{ .Role = core::github_role::reviewer });
		titled.Facts                    = core::queue_item_facts{ .Title = "Parse faster", .Author = "octocat", .UnseenComments = 2U };
		model.apply(std::vector{ titled, item(8, bucket::quiet) });
		const auto first = model.index(0);
		expect(first.data(qt::review_landing_model::title_role).toString() == QStringLiteral("Parse faster")
				   && first.data(qt::review_landing_model::author_role).toString() == QStringLiteral("octocat")
				   && first.data(qt::review_landing_model::role_role).toString() == QStringLiteral("reviewer")
				   && first.data(qt::review_landing_model::group_role).toInt() == static_cast<int>(qt::home_group::needs_you)
				   && first.data(qt::review_landing_model::unseen_role).toBool(),
			   "the columns read the facts and the membership");
		const auto second = model.index(1);
		expect(second.data(qt::review_landing_model::title_role).toString().isEmpty()
				   && second.data(qt::review_landing_model::role_role).toString().isEmpty()
				   && second.data(qt::review_landing_model::group_role).toInt() == static_cast<int>(qt::home_group::following)
				   && !second.data(qt::review_landing_model::unseen_role).toBool(),
			   "a role of none reads as nothing, and a quiet item follows");

		const auto expected = core::open_pull_request_intent{
			.Repository = core::repository_locator{ .Owner = "octo", .Name = "app" },
			.Number     = core::pull_request_number{ 42U },
		};
		for (const auto* const text : { "octo/app#42", " octo/app#42 ", "octo/app/pull/42",
										"https://github.com/octo/app/pull/42", "https://ghe.example.com/octo/app/pull/42/files" }) {
			const auto parsed = qt::parse_pull_request_locator(QString::fromUtf8(text));
			expect(parsed.has_value() && parsed->Repository == expected.Repository && parsed->Number == expected.Number, "every named form opens the same pull request");
		}
		const auto exact = qt::parse_pull_request_locator(QStringLiteral("https://ghe.example.com/octo/app/pull/42/files#diff-abcd"));
		expect(exact && exact->Hostname == "ghe.example.com" && exact->Location == "/files#diff-abcd", "Qt translation preserves exact server and requested location");

		for (const auto* const text : { "", "42", "#42", "octo#42", "octo/app", "octo/app#zero", "octo/app/issues/42", "https://github.com" }) {
			expect(!qt::parse_pull_request_locator(QString::fromUtf8(text)).has_value(),
				   "a form that names no repository and number opens nothing");
		}
	}

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

		// [[BPR-0783]]. Home no longer filters by Repository: Group and the state
		// order do that job. The profile still keeps the value, so the proxy
		// reports it and every row stays; the explicit alternate sort still applies.
		proxy.apply(application::queue_presentation_preferences{
			.Repository = "a/repo",
			.Sort       = application::queue_sort_order::repository,
		});
		expect(
			proxy.rowCount() == 4
				&& proxy.item_at(proxy.index(0, 0))->Ref.Number == 2
				&& proxy.item_at(proxy.index(1, 0))->Ref.Number == 5,
			"the explicit alternate sort is deterministic and the repository value filters nothing");
		expect(
			proxy.preferences().Repository == "a/repo"
				&& proxy.effective_preferences().Repository.empty(),
			"the profile's repository is retained and is not one of Home's filters");
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
		// [[BPR-0783]]. Nor by Bucket, for the same reason: the state order already
		// puts the core's attention first.
		proxy.apply(application::queue_presentation_preferences{
			.Bucket = core::attention_bucket::fixes_awaiting_verification,
		});
		expect(
			proxy.rowCount() == 4 && proxy.preferences().Bucket == core::attention_bucket::fixes_awaiting_verification
				&& !proxy.effective_preferences().Bucket.has_value(),
			"the profile's attention bucket is retained and is not one of Home's filters");
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

	// [[BPR-0776]]. Home's rows say why in projected facts only, end in one word
	// per bucket, and collapse to a summary of the same facts.
	void test_home_rows_say_why_in_projected_facts()
	{
		using bucket       = core::attention_bucket;
		const auto buckets = std::array{
			bucket::initial_review_requested,
			bucket::changed_since_my_review,
			bucket::new_comments_or_author_replies,
			bucket::fixes_awaiting_verification,
			bucket::authored_awaiting_reviewer_action,
			bucket::observed_with_new_activity,
			bucket::quiet,
			bucket::detail_unknown,
		};
		auto words = std::vector<QString>{ };
		for (const auto value : buckets) {
			const auto word = qt::home_reason_word(value);
			expect(!word.isEmpty() && !word.contains(QLatin1Char{ ' ' }) && word != QStringLiteral("Approved"),
				   "every bucket ends its row in one word, and none is Approved");
			expect(std::ranges::find(words, word) == words.end(), "no two buckets share a word");
			words.push_back(word);
			const auto sentence = qt::home_why_sentence(item(9, value));
			expect(sentence.endsWith(QLatin1Char{ '.' }) && !sentence.contains(QStringLiteral("pushed"))
					   && !sentence.contains(QStringLiteral(" ago")) && !sentence.contains(QStringLiteral(" of ")),
				   "no sentence claims a push, a time or a progress the projection does not carry");
		}
		expect(words
				   == std::vector<QString>{ QStringLiteral("Requested"), QStringLiteral("Changed"), QStringLiteral("Replied"),
											QStringLiteral("Verify"), QStringLiteral("Waiting"), QStringLiteral("Following"),
											QStringLiteral("Quiet"), QStringLiteral("Unknown") },
			   "the words are the accepted ones, in bucket order");

		auto requested  = item(1, bucket::initial_review_requested, core::queue_membership{ .Role = core::github_role::requested_reviewer });
		requested.Facts = core::queue_item_facts{ .Title = "Parse faster", .Author = "octocat", .UnseenComments = 1U };
		expect(qt::home_why_sentence(requested) == QStringLiteral("Opened by octocat; my review is requested; 1 new comment."),
			   "a requested review names its author and what I have not read");
		auto changed                 = item(2, bucket::changed_since_my_review);
		changed.Facts.UnseenComments = 3U;
		expect(qt::home_why_sentence(changed) == QStringLiteral("The code moved since my last review; 3 new comments."),
			   "a changed review says the code moved, and counts what is unread");
		auto replied                 = item(3, bucket::new_comments_or_author_replies);
		replied.Facts.UnseenComments = 2U;
		expect(qt::home_why_sentence(replied) == QStringLiteral("2 new comments I have not read."),
			   "a reply says how much I have not read");
		const auto watched = item(4, bucket::observed_with_new_activity,
								  core::queue_membership{ .MatchedRule = true, .Rule = core::observer_rule{ .Kind = core::observer_rule_kind::path, .Pattern = "src/parser" } });
		expect(qt::home_why_sentence(watched)
				   == QStringLiteral("New activity on a pull request I follow; I watch %1 src/parser.")
						  .arg(QString::fromUtf8(core::name(core::observer_rule_kind::path).data())),
			   "a row here only because of a rule names the rule");
		const auto verify = item(5, bucket::fixes_awaiting_verification);
		const auto quiet  = item(6, bucket::quiet);
		expect(qt::home_state_summary(qt::home_group::needs_you, std::array{ requested, changed, replied, verify })
				   == QStringLiteral("1 requested, 1 changed, 1 replied, 1 to verify; 6 new comments"),
			   "a collapsed Needs me counts its rows by reason and what I have not read");
		expect(qt::home_state_summary(qt::home_group::waiting_on_others, std::array{ item(7, bucket::authored_awaiting_reviewer_action) })
				   == QStringLiteral("1 of mine"),
			   "Waiting on others counts mine and claims no reviewer count");
		expect(qt::home_state_summary(qt::home_group::following, std::array{ watched, quiet }) == QStringLiteral("2 followed"),
			   "Following counts what I follow");
		// [[BPR-0989]]. A row nothing has been read about is followed, says so,
		// and is not quiet: the Quiet-only filter leaves it out.
		const auto unknown = item(8, bucket::detail_unknown, core::queue_membership{ .Authored = true });
		expect(qt::home_group_of(bucket::detail_unknown) == qt::home_group::following
				   && qt::home_why_sentence(unknown) == QStringLiteral("My pull request; Nothing about it has been read yet."),
			   "an authored unread row retains its unknown explanation and identifies ownership");
		{
			auto source = qt::review_landing_model{ };
			auto proxy  = qt::review_landing_filter_model{ source };
			source.apply(std::vector{ quiet, unknown });
			proxy.apply(application::queue_presentation_preferences{ .Quiet = application::queue_quiet_filter::quiet });
			expect(proxy.rowCount() == 1 && proxy.item_at(proxy.index(0, 0))->Ref == quiet.Ref, "the Quiet-only filter does not show an unread row");
			proxy.apply(application::queue_presentation_preferences{ .Quiet = application::queue_quiet_filter::attention });
			expect(proxy.rowCount() == 1 && proxy.item_at(proxy.index(0, 0))->Ref == unknown.Ref, "hiding quiet rows keeps it");
		}
		expect(qt::home_state_summary(qt::home_group::needs_you, std::span<const core::queue_item>{ }).isEmpty(),
			   "a state with no row has no summary");
		const auto entries = std::array{
			application::recent_review_entry{ .Title = "One", .Resumable = true },
			application::recent_review_entry{ .Title = "Two" },
		};
		expect(qt::home_recent_summary(entries) == QStringLiteral("2 viewed; 1 saved reading"),
			   "Recently viewed summarises what was viewed and what kept a reading");
		expect(qt::home_all_empty_sentence()
				   == QStringLiteral("Nothing needs me. Pull requests from saved repositories and observer rules appear here."),
			   "the all-empty sentence is the RFC's");

		qt::review_landing_model model;
		model.apply(std::array{ requested });
		const auto row = model.index(0);
		expect(row.data(qt::home_row_role::identity).toString() == QStringLiteral("octo/app#1")
				   && row.data(qt::home_row_role::title).toString() == QStringLiteral("Parse faster")
				   && row.data(qt::home_row_role::why).toString() == qt::home_why_sentence(requested)
				   && row.data(qt::home_row_role::reason_word).toString() == QStringLiteral("Requested")
				   && row.data(qt::home_row_role::state).toInt() == static_cast<int>(qt::home_group::needs_you)
				   && row.data(qt::home_row_role::time).toString().isEmpty()
				   && row.data(qt::home_row_role::progress).toString().isEmpty()
				   && row.data(qt::home_row_role::account_chip).toString().isEmpty()
				   && row.data(qt::home_row_role::row_action).toString().isEmpty(),
			   "the queue model answers Home's row roles and invents no time, progress, chip or action");
		expect(row.data(Qt::AccessibleDescriptionRole).toString() == qt::home_why_sentence(requested) + QStringLiteral(" Requested"),
			   "a row describes itself as it is drawn");
	}
	void test_waiting_blockers_and_quiet_filters()
	{
		auto waiting                         = item(31, core::attention_bucket::authored_awaiting_reviewer_action, { .Authored = true });
		waiting.Facts.Waiting.Requests       = 3;
		waiting.Facts.Waiting.Teams          = 3;
		waiting.Facts.Waiting.NamesKnown     = true;
		waiting.Facts.Waiting.CodeOwners     = { "@org/team-a", "@org/team-b", "@org/team-c" };
		waiting.Facts.Waiting.RequiredReview = true;
		waiting.Facts.Waiting.MergeBlocked   = true;
		waiting.Facts.Waiting.ChecksKnown    = true;
		waiting.Facts.Waiting.FailedChecks   = { "Branch Freshness" };
		const auto text                      = qt::home_why_sentence(waiting);
		expect(text.contains(QStringLiteral("@org/team-c")) && text.contains(QStringLiteral("Branch Freshness failed")) && text.contains(QStringLiteral("0 people, 3 teams")), "waiting reason names code-owner teams, separates people/team counts and preserves concurrent failure");
		auto blocked                         = waiting;
		blocked.Ref.Number                   = 32;
		blocked.Bucket                       = core::attention_bucket::quiet;
		blocked.Facts.Waiting.Requests       = 0;
		blocked.Facts.Waiting.RequiredReview = false;
		blocked.Facts.Waiting.CodeOwners.clear();
		auto source = qt::review_landing_model{ };
		auto proxy  = qt::review_landing_filter_model{ source };
		source.apply(std::vector{ waiting, blocked });
		expect(!source.data(source.index(1, 0), Qt::AccessibleDescriptionRole).toString().contains(QStringLiteral("Quiet")) && !qt::home_why_sentence(blocked).contains(QStringLiteral("Nothing here")), "independent remaining block never claims quiet or settled");
		proxy.apply(application::queue_presentation_preferences{ .Role = core::github_role::author, .Quiet = application::queue_quiet_filter::attention });
		expect(proxy.rowCount() == 2, "My role author and Hide quiet retain authored discovery and known blockers");
		proxy.apply(application::queue_presentation_preferences{ .Quiet = application::queue_quiet_filter::quiet });
		expect(proxy.rowCount() == 0, "Only quiet excludes known blockers");
		expect(qt::home_state_summary(qt::home_group::waiting_on_others, std::array{ waiting }).contains(QStringLiteral("1 blocked")), "collapsed summary carries the blocker count");
	}

} // namespace

auto main(int argc, char** argv) -> int
{
	QCoreApplication application{ argc, argv };

	test_home_groups_columns_and_the_open_bar_grammar();
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
	test_home_rows_say_why_in_projected_facts();
	test_waiting_blockers_and_quiet_filters();

	std::cout << "review-landing-model-tests:pass\n";
	return EXIT_SUCCESS;
}
