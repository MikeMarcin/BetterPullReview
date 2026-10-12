#include "bpr/qt/review_landing_model.hpp"

#include "bpr/qt/home_row_delegate.hpp"

#include <QDateTime>
#include <QList>
#include <QLocale>
#include <QStringList>

#include <algorithm>
#include <array>
#include <cstdint>
#include <ranges>
#include <set>
#include <tuple>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto text(const std::string_view aValue) -> QString
		{
			return QString::fromUtf8(
				aValue.data(), static_cast<qsizetype>(aValue.size()));
		}
	} // namespace

	auto badge_for(const core::attention_bucket aBucket) -> QString
	{
		return QStringLiteral("[%1]").arg(text(core::name(aBucket)));
	}

	auto home_group_of(const core::attention_bucket aBucket) noexcept -> home_group
	{
		return core::home_section_for(core::queue_membership{ }, aBucket);
	}

	auto home_group_label(const home_group aGroup) -> QString
	{
		switch (aGroup) {
		case home_group::needs_you:
			return QStringLiteral("Needs me");
		case home_group::my_pull_requests:
			return QStringLiteral("My pull requests");
		case home_group::waiting_on_others:
			return QStringLiteral("Waiting on others");
		case home_group::following:
			break;
		}
		return QStringLiteral("Following");
	}

	auto parse_pull_request_locator(const QString& aText)
		-> std::optional<core::setup_destination>
	{
		if (aText.size() > 4096) {
			return std::nullopt;
		}
		return application::parse_setup_destination(aText.toStdString());
	}

	auto reason_for(const core::queue_item& aItem) -> QString
	{
		auto reason = core::review_waiting(aItem.Facts.Waiting) || core::merge_blocked(aItem.Facts.Waiting) ? home_why_sentence(aItem) : text(core::explanation(aItem.Bucket));
		// The rule is named only when it is why the item is here at all. Saying
		// "you watch src/parser" about a review someone asked for would bury
		// the fact that they were asked.
		if (aItem.Membership.MatchedRule
			&& aItem.Membership.Role == core::github_role::none) {
			reason += QStringLiteral(" You watch %1 %2.")
						  .arg(text(core::name(aItem.Membership.Rule.Kind)))
						  .arg(text(aItem.Membership.Rule.Pattern));
		}
		return reason;
	}

	namespace {
		[[nodiscard]] auto counted(const std::uint64_t aCount, const QString& aOne, const QString& aMany) -> QString
		{
			return QStringLiteral("%1 %2").arg(QString::number(aCount), aCount == 1U ? aOne : aMany);
		}

		[[nodiscard]] auto new_comments(const std::uint64_t aCount) -> QString
		{
			return counted(aCount, QStringLiteral("new comment"), QStringLiteral("new comments"));
		}
	} // namespace

	auto home_reason_word(const core::attention_bucket aBucket) -> QString
	{
		switch (aBucket) {
		case core::attention_bucket::initial_review_requested:
			return QStringLiteral("Requested");
		case core::attention_bucket::changed_since_my_review:
			return QStringLiteral("Changed");
		case core::attention_bucket::new_comments_or_author_replies:
			return QStringLiteral("Replied");
		case core::attention_bucket::fixes_awaiting_verification:
			return QStringLiteral("Verify");
		case core::attention_bucket::authored_awaiting_reviewer_action:
			return QStringLiteral("Waiting");
		case core::attention_bucket::observed_with_new_activity:
			return QStringLiteral("Following");
		case core::attention_bucket::detail_unknown:
			return QStringLiteral("Unknown");
		case core::attention_bucket::quiet:
			break;
		}
		return QStringLiteral("Quiet");
	}

	auto home_identity(const core::queue_item& aItem) -> QString
	{
		return QStringLiteral("%1#%2").arg(text(aItem.Ref.Repository), QString::number(aItem.Ref.Number));
	}

	namespace {
		/// \brief "A", "A and B", "A, B and C", or "A, B and 2 more".
		[[nodiscard]] auto bounded_list(const std::vector<std::string>& aNames, const std::size_t aTotal) -> QString
		{
			constexpr auto shown = std::size_t{ 3U };
			auto           names = QStringList{ };
			for (const auto& name : aNames | std::views::take(aTotal > shown ? shown - 1U : shown)) {
				names.push_back(text(name));
			}
			const auto rest = aTotal - static_cast<std::size_t>(names.size());
			if (rest > 0U) {
				return QStringLiteral("%1 and %2 more").arg(names.join(QStringLiteral(", ")), QString::number(rest));
			}
			if (names.size() < 2) {
				return names.join(QString{ });
			}
			const auto last = names.takeLast();
			return QStringLiteral("%1 and %2").arg(names.join(QStringLiteral(", ")), last);
		}
	} // namespace

	auto home_waiting_clauses(const core::queue_item& aItem) -> QStringList
	{
		const auto& waiting = aItem.Facts.Waiting;
		auto        clauses = QStringList{ };
		// Who else someone else's pull request waits on is its author's
		// business ([[RFC-BPR-0070]] waiting is authored); its blockers are not.
		const auto authored = aItem.Membership.authored();
		const auto requests = authored ? static_cast<std::size_t>(waiting.Requests.value_or(0U)) : 0U;
		const auto named    = authored ? waiting.CodeOwners.size() + waiting.Reviewers.size() : 0U;
		// Requests beyond the retained names are counted with the last list.
		const auto unnamed = waiting.NamesKnown && requests > named ? requests - named : 0U;
		if (authored && !waiting.CodeOwners.empty()) {
			clauses.push_back(QStringLiteral("Waiting on code-owner review from %1")
								  .arg(bounded_list(waiting.CodeOwners, waiting.CodeOwners.size() + (waiting.Reviewers.empty() ? unnamed : 0U))));
		}
		if (authored && !waiting.Reviewers.empty()) {
			clauses.push_back((waiting.CodeOwners.empty() ? QStringLiteral("Waiting on review from %1") : QStringLiteral("review from %1"))
								  .arg(bounded_list(waiting.Reviewers, waiting.Reviewers.size() + unnamed)));
		}
		if (requests > 0U && !waiting.NamesKnown) {
			// Discovery's counts, which tell people from teams but not who.
			auto who = QStringList{ };
			if (waiting.People > 0U) {
				who.push_back(counted(waiting.People, QStringLiteral("person"), QStringLiteral("people")));
			}
			if (waiting.Teams > 0U) {
				who.push_back(counted(waiting.Teams, QStringLiteral("team"), QStringLiteral("teams")));
			}
			clauses.push_back(waiting.People + waiting.Teams == requests
								  ? QStringLiteral("Waiting on %1").arg(who.join(QStringLiteral(" and ")))
								  : QStringLiteral("Waiting on %1").arg(counted(requests, QStringLiteral("review request"), QStringLiteral("review requests"))));
		}
		if (authored && named == 0U && requests == 0U && waiting.RequiredReview.value_or(false)) {
			clauses.push_back(QStringLiteral("Required review outstanding"));
		}
		if (!waiting.FailedChecks.empty()) {
			clauses.push_back(QStringLiteral("%1 failed").arg(bounded_list(waiting.FailedChecks, waiting.FailedChecks.size())));
		} else if (waiting.MergeBlocked.value_or(false)) {
			clauses.push_back(QStringLiteral("Merge blocked"));
		}
		if (!clauses.isEmpty() && (waiting.RequestsStale || waiting.AssessmentStale || waiting.ChecksStale)) {
			clauses.push_back(QStringLiteral("last known"));
		}
		return clauses;
	}

	auto home_why_sentence(const core::queue_item& aItem) -> QString
	{
		const auto unseen  = static_cast<std::uint64_t>(aItem.Facts.UnseenComments);
		const auto author  = text(aItem.Facts.Author);
		auto       clauses = QStringList{ };
		if (aItem.Membership.authored() && aItem.Bucket != core::attention_bucket::authored_awaiting_reviewer_action) {
			clauses.push_back(QStringLiteral("My pull request"));
		}
		// [[BPR-1406]]. Someone else's pull request leads with who opened it,
		// so every such row names its author in the same place.
		if (!aItem.Membership.authored() && !author.isEmpty()) {
			clauses.push_back(QStringLiteral("Opened by %1").arg(author));
		}
		// A clause after a leading one continues its sentence.
		const auto lead = [&clauses](QString aClause) {
			if (!clauses.isEmpty()) {
				aClause[0] = aClause[0].toLower();
			}
			clauses.push_back(aClause);
		};
		switch (aItem.Bucket) {
		case core::attention_bucket::initial_review_requested:
			lead(QStringLiteral("My review is requested"));
			break;
		case core::attention_bucket::changed_since_my_review:
			lead(QStringLiteral("The code moved since my last review"));
			break;
		case core::attention_bucket::new_comments_or_author_replies:
			lead(unseen > 0U ? new_comments(unseen) + QStringLiteral(" I have not read")
							 : QStringLiteral("Something here is unread"));
			break;
		case core::attention_bucket::fixes_awaiting_verification:
			lead(QStringLiteral("A defect I filed is marked fixed"));
			clauses.push_back(QStringLiteral("I have not checked it"));
			break;
		case core::attention_bucket::authored_awaiting_reviewer_action:
			lead(QStringLiteral("My pull request waits on reviewers"));
			break;
		case core::attention_bucket::observed_with_new_activity:
			lead(QStringLiteral("New activity on a pull request I follow"));
			break;
		case core::attention_bucket::quiet:
			if (!core::merge_blocked(aItem.Facts.Waiting)) {
				lead(QStringLiteral("Nothing here is waiting on anyone"));
			}
			break;
		case core::attention_bucket::detail_unknown:
			lead(QStringLiteral("Nothing about it has been read yet"));
			break;
		}
		// [[BPR-1405]]. Who or what is awaited, said once and bounded; the
		// whole roster is in Overview.
		const auto waitingClauses = home_waiting_clauses(aItem);
		if (!waitingClauses.isEmpty() && aItem.Bucket == core::attention_bucket::authored_awaiting_reviewer_action) {
			clauses.removeAll(QStringLiteral("My pull request waits on reviewers"));
		}
		clauses.append(waitingClauses);
		if (aItem.Bucket != core::attention_bucket::new_comments_or_author_replies && unseen > 0U) {
			clauses.push_back(new_comments(unseen));
		}
		// As `reason_for`: the rule is named only when it is why the row is here.
		if (aItem.Membership.MatchedRule && aItem.Membership.Role == core::github_role::none) {
			clauses.push_back(QStringLiteral("I watch %1 %2")
								  .arg(text(core::name(aItem.Membership.Rule.Kind)), text(aItem.Membership.Rule.Pattern)));
		}
		return clauses.join(QStringLiteral("; ")) + QLatin1Char{ '.' };
	}

	auto home_state_summary(const home_group aGroup, const std::span<const core::queue_item> aRows) -> QString
	{
		if (aRows.empty()) {
			return { };
		}
		auto unseen = std::uint64_t{ 0U };
		for (const auto& row : aRows) {
			unseen += row.Facts.UnseenComments;
		}
		auto summary = QString{ };
		switch (aGroup) {
		case home_group::needs_you: {
			auto parts = QStringList{ };
			for (const auto bucket : std::array{ core::attention_bucket::initial_review_requested,
												 core::attention_bucket::changed_since_my_review,
												 core::attention_bucket::new_comments_or_author_replies,
												 core::attention_bucket::fixes_awaiting_verification }) {
				const auto count = std::ranges::count(aRows, bucket, &core::queue_item::Bucket);
				if (count > 0) {
					parts.push_back(QStringLiteral("%1 %2").arg(
						QString::number(count),
						bucket == core::attention_bucket::fixes_awaiting_verification
							? QStringLiteral("to verify")
							: home_reason_word(bucket).toLower()));
				}
			}
			summary = parts.join(QStringLiteral(", "));
			break;
		}
		case home_group::my_pull_requests: {
			const auto actionable = std::ranges::count_if(aRows, [](const core::queue_item& aRow) { return core::home_section_for(core::queue_membership{ }, aRow.Bucket) == core::home_queue_section::needs_you; });
			const auto unknown    = std::ranges::count(aRows, core::attention_bucket::detail_unknown, &core::queue_item::Bucket);
			summary               = counted(static_cast<std::uint64_t>(actionable), QStringLiteral("needs action"), QStringLiteral("need action"));
			if (unknown > 0) {
				summary += QStringLiteral("; %1 unknown").arg(QString::number(unknown));
			}
			break;
		}
		case home_group::waiting_on_others:
			summary = QStringLiteral("%1 of mine").arg(QString::number(aRows.size()));
			break;
		case home_group::following:
			summary = QStringLiteral("%1 followed").arg(QString::number(aRows.size()));
			break;
		}
		const auto blocked = std::ranges::count_if(aRows, [](const auto& aRow) { return core::merge_blocked(aRow.Facts.Waiting); });
		const auto waiting = std::ranges::count_if(aRows, [](const auto& aRow) { return core::review_waiting(aRow.Facts.Waiting); });
		// [[BPR-1405]]. Every row of Waiting on others awaits review; saying
		// so again is noise.
		if (waiting > 0 && aGroup != home_group::waiting_on_others) {
			summary += QStringLiteral("; %1 awaiting review").arg(waiting);
		}
		if (blocked > 0) {
			summary += QStringLiteral("; %1 blocked").arg(blocked);
		}
		if (unseen > 0U) {
			summary += QStringLiteral("; ") + new_comments(unseen);
		}
		return summary;
	}

	auto home_recent_summary(const std::span<const application::recent_review_entry> aEntries) -> QString
	{
		if (aEntries.empty()) {
			return { };
		}
		auto       summary = QStringLiteral("%1 viewed").arg(QString::number(aEntries.size()));
		const auto saved   = std::ranges::count(aEntries, true, &application::recent_review_entry::Resumable);
		if (saved > 0) {
			summary += QStringLiteral("; ")
				+ counted(static_cast<std::uint64_t>(saved), QStringLiteral("saved reading"), QStringLiteral("saved readings"));
		}
		return summary;
	}

	auto home_all_empty_sentence() -> QString
	{
		return QStringLiteral("Nothing needs me. Pull requests from saved repositories and observer rules appear here.");
	}

	auto home_discovery_time(const std::int64_t aSeconds) -> QString
	{
		return QLocale{ }.toString(QDateTime::fromSecsSinceEpoch(aSeconds).time(), QLocale::ShortFormat);
	}

	auto home_discovery_sentence(const application::queue_discovery_status& aStatus) -> QString
	{
		using state             = application::queue_discovery_state;
		using problem           = application::queue_discovery_problem;
		const auto repositories = aStatus.Repositories == 1U
			? QStringLiteral("1 saved repository")
			: QStringLiteral("%1 saved repositories").arg(aStatus.Repositories);
		const auto last         = aStatus.LastSuccess.has_value() ? home_discovery_time(*aStatus.LastSuccess) : QString{ };
		auto       sentence     = QString{ };
		switch (aStatus.State) {
		case state::idle:
			break;
		case state::unavailable:
			if (aStatus.Problem == problem::no_saved_repository) {
				sentence = QStringLiteral("Save a repository to find the pull requests in it that need me.");
			}
			break;
		case state::refreshing:
			sentence = last.isEmpty() ? QStringLiteral("Finding my pull requests in %1…").arg(repositories)
									  : QStringLiteral("Finding my pull requests in %1; last found at %2.").arg(repositories, last);
			break;
		case state::current:
			sentence = QStringLiteral("Found at %1 in %2.").arg(last, repositories);
			break;
		case state::partial: {
			auto why = QString{ };
			switch (aStatus.Problem) {
			case problem::incomplete_results: why = QStringLiteral("GitHub said its search was incomplete"); break;
			case problem::result_limit:       why = QStringLiteral("more matched than one search returns"); break;
			case problem::detail_limit:       why = QStringLiteral("more review requests than could be checked"); break;
			default:                          why = QStringLiteral("part of the search was not read"); break;
			}
			sentence = QStringLiteral("Found at %1 in %2, but the list may be missing some: %3.").arg(last, repositories, why);
			break;
		}
		case state::stale: {
			auto why = QString{ };
			switch (aStatus.Problem) {
			case problem::rate_limited:           why = QStringLiteral("GitHub's rate limit was reached"); break;
			case problem::secondary_rate_limited: why = QStringLiteral("GitHub asked to slow down"); break;
			case problem::offline:                why = QStringLiteral("GitHub could not be reached"); break;
			case problem::authentication:         why = QStringLiteral("the account needs to sign in again"); break;
			case problem::repository_unavailable: why = QStringLiteral("a saved repository could not be read"); break;
			default:                              why = QStringLiteral("GitHub's answer could not be used"); break;
			}
			sentence = last.isEmpty() ? QStringLiteral("Not refreshed: %1.").arg(why)
									  : QStringLiteral("Not refreshed: %1. Showing what was found at %2.").arg(why, last);
			break;
		}
		}
		if (aStatus.RetryAfterSeconds.has_value() && !sentence.isEmpty()) {
			sentence += *aStatus.RetryAfterSeconds == 1U
				? QStringLiteral(" Refresh again in 1 second.")
				: QStringLiteral(" Refresh again in %1 seconds.").arg(*aStatus.RetryAfterSeconds);
		}
		return sentence;
	}

	review_landing_model::review_landing_model(QObject* const aParent)
		: QAbstractListModel{ aParent }
	{
	}

	auto review_landing_model::rowCount(const QModelIndex& aParent) const -> int
	{
		return aParent.isValid() ? 0 : static_cast<int>(Items.size());
	}

	auto review_landing_model::data(
		const QModelIndex& aIndex, const int aRequestedRole) const -> QVariant
	{
		const auto item = item_at(aIndex);
		if (!item) {
			return { };
		}
		switch (aRequestedRole) {
		case Qt::DisplayRole:
			return QStringLiteral("%1 %2 #%3")
				.arg(badge_for(item->Bucket))
				.arg(text(item->Ref.Repository))
				.arg(item->Ref.Number);
		case badge_role:
			return badge_for(item->Bucket);
		case reason_role:
			return reason_for(*item);
		case Qt::AccessibleTextRole: {
			const auto label = item->Facts.Title.empty() ? home_identity(*item) : QStringLiteral("%1 %2").arg(home_identity(*item), text(item->Facts.Title));
			return item->Facts.Draft.value_or(false) ? QStringLiteral("Draft pull request; %1").arg(label) : label;
		}
		case Qt::AccessibleDescriptionRole: {
			const auto why = QStringLiteral("%1 %2").arg(home_why_sentence(*item), item->Bucket == core::attention_bucket::quiet && core::merge_blocked(item->Facts.Waiting) ? QStringLiteral("Blocked") : home_reason_word(item->Bucket));
			// [[BPR-1365]]. The open review is said in words, as it is drawn.
			return Reviewing == item->Ref ? QStringLiteral("%1; %2").arg(home_reviewing_description(), why) : why;
		}
		case repository_role:
			return text(item->Ref.Repository);
		case host_role:
			return text(item->Ref.Host);
		case account_role:
			return text(item->Ref.Account);
		case number_role:
			return QVariant::fromValue(item->Ref.Number);
		case bucket_role:
			return static_cast<int>(item->Bucket);
		case title_role:
			return text(item->Facts.Title);
		case author_role:
			return text(item->Facts.Author);
		case role_role:
			if (item->Membership.authored()) {
				return text(core::name(core::github_role::author));
			}
			return item->Membership.Role == core::github_role::none
				? QString{ }
				: text(core::name(item->Membership.Role));
		case group_role:
			return static_cast<int>(core::home_section_for(item->Membership, item->Bucket));
		case unseen_role:
			return item->Facts.UnseenComments > 0U;
		// [[BPR-0776]]. The roles Home's one row anatomy reads. Time and
		// progress are not projected for a queue row, so they are empty and
		// nothing is drawn for them.
		case home_row_role::identity:
			return home_identity(*item);
		case home_row_role::title:
			return text(item->Facts.Title);
		case home_row_role::why:
			return home_why_sentence(*item);
		case home_row_role::reason_word:
			return item->Bucket == core::attention_bucket::quiet && core::merge_blocked(item->Facts.Waiting) ? QStringLiteral("Blocked") : home_reason_word(item->Bucket);
		case home_row_role::state:
			return static_cast<int>(home_group_of(item->Bucket));
		case home_row_role::account_chip:
			// [[BPR-0783]]. The chip is drawn only when the queue names more than
			// one account; with one there is nothing to tell apart, so every row
			// would carry the same word.
			return Accounts.size() > 1U
				? home_account_chip_text(text(item->Ref.Account), text(item->Ref.Host))
				: QString{ };
		case home_row_role::icon_action:
			// [[BPR-1338]]. Every queue row's pull request has a page on GitHub.
			return QStringLiteral("Open on GitHub");
		case home_row_role::draft:
			return item->Facts.Draft.value_or(false);
		case home_row_role::reviewing:
			return Reviewing == item->Ref;
		case home_row_role::time:
		case home_row_role::progress:
		case home_row_role::row_action:
			return QString{ };
		default:
			break;
		}
		return { };
	}

	auto review_landing_model::roleNames() const -> QHash<int, QByteArray>
	{
		auto names = QAbstractListModel::roleNames();
		names.insert(badge_role, QByteArrayLiteral("badge"));
		names.insert(reason_role, QByteArrayLiteral("reason"));
		names.insert(repository_role, QByteArrayLiteral("repository"));
		names.insert(number_role, QByteArrayLiteral("number"));
		names.insert(bucket_role, QByteArrayLiteral("bucket"));
		names.insert(title_role, QByteArrayLiteral("title"));
		names.insert(author_role, QByteArrayLiteral("author"));
		names.insert(role_role, QByteArrayLiteral("role"));
		names.insert(group_role, QByteArrayLiteral("group"));
		names.insert(unseen_role, QByteArrayLiteral("unseen"));
		names.insert(host_role, QByteArrayLiteral("host"));
		names.insert(account_role, QByteArrayLiteral("account"));
		return names;
	}

	auto review_landing_model::position_of(
		const core::pull_request_ref& aRef) const -> std::optional<std::size_t>
	{
		const auto found = Positions.find(aRef);
		if (found == Positions.end()) {
			return std::nullopt;
		}
		return found->second;
	}

	void review_landing_model::apply(
		const std::span<const core::queue_item> aQueue)
	{
		auto identities = std::set<core::pull_request_ref>{ };
		for (const auto& item : aQueue) {
			identities.insert(item.Ref);
		}
		// Removals first, from the back, so every index stays valid while they
		// are issued. Doing them forwards would leave each subsequent index
		// pointing at the wrong row.
		for (auto index = Items.size(); index > 0U; --index) {
			const auto position = index - 1U;
			const auto present  = identities.contains(Items[position].Ref);
			if (present) {
				continue;
			}
			beginRemoveRows({ }, static_cast<int>(position), static_cast<int>(position));
			Positions.erase(Items[position].Ref);
			Items.erase(Items.begin() + static_cast<std::ptrdiff_t>(position));
			index_positions(position, Items.size());
			endRemoveRows();
		}

		// Then place each item where the core put it, matching by identity. An
		// item that changed bucket moves rather than being removed and
		// re-inserted, so its persistent index survives and a view keeps the
		// reviewer's place.
		for (auto target = std::size_t{ 0U }; target < aQueue.size(); ++target) {
			const auto& wanted = aQueue[target];
			const auto  found  = position_of(wanted.Ref);

			if (!found) {
				beginInsertRows(
					{ }, static_cast<int>(target), static_cast<int>(target));
				Items.insert(
					Items.begin() + static_cast<std::ptrdiff_t>(target), wanted);
				index_positions(target, Items.size());
				endInsertRows();
				continue;
			}

			if (*found != target) {
				beginMoveRows(
					{ },
					static_cast<int>(*found),
					static_cast<int>(*found),
					{ },
					static_cast<int>(target));
				auto moved = Items[*found];
				Items.erase(Items.begin() + static_cast<std::ptrdiff_t>(*found));
				Items.insert(
					Items.begin() + static_cast<std::ptrdiff_t>(target),
					std::move(moved));
				index_positions(target, *found + 1U);
				endMoveRows();
			}

			if (Items[target] != wanted) {
				Items[target]  = wanted;
				const auto row = index(static_cast<int>(target));
				emit       dataChanged(row, row);
			}
		}

		adopt_accounts();
	}

	void review_landing_model::index_positions(const std::size_t aFirst, const std::size_t aEnd)
	{
		for (auto row = aFirst; row < aEnd; ++row) {
			Positions.insert_or_assign(Items[row].Ref, row);
		}
	}

	auto review_landing_model::item_for(const core::pull_request_ref& aRef) const -> std::optional<core::queue_item>
	{
		const auto found = position_of(aRef);
		return found ? std::optional{ Items[*found] } : std::nullopt;
	}

	void review_landing_model::set_reviewing(std::optional<core::pull_request_ref> aRef)
	{
		if (Reviewing == aRef) {
			return;
		}
		const auto left  = Reviewing ? position_of(*Reviewing) : std::nullopt;
		Reviewing        = std::move(aRef);
		const auto came  = Reviewing ? position_of(*Reviewing) : std::nullopt;
		const auto roles = QList<int>{ home_row_role::reviewing, Qt::AccessibleDescriptionRole };
		for (const auto row : { left, came }) {
			if (row.has_value()) {
				const auto changed = index(static_cast<int>(*row), 0);
				emit       dataChanged(changed, changed, roles);
			}
		}
	}

	auto review_landing_model::reviewing() const noexcept -> const std::optional<core::pull_request_ref>&
	{
		return Reviewing;
	}

	void review_landing_model::apply_item(const core::queue_item& aItem)
	{
		const auto found = position_of(aItem.Ref);
		if (!found || Items[*found] == aItem) {
			return;
		}
		const auto insertion = std::ranges::lower_bound(Items, aItem, core::queue_precedes);
		auto       target    = static_cast<std::size_t>(insertion - Items.begin());
		if (target > *found) {
			--target;
		}
		if (target != *found) {
			beginMoveRows({ }, static_cast<int>(*found), static_cast<int>(*found), { }, static_cast<int>(target > *found ? target + 1U : target));
			auto moved = std::move(Items[*found]);
			Items.erase(Items.begin() + static_cast<std::ptrdiff_t>(*found));
			Items.insert(Items.begin() + static_cast<std::ptrdiff_t>(target), std::move(moved));
			index_positions(std::min(target, *found), std::max(target, *found) + 1U);
			endMoveRows();
		}
		Items[target] = aItem;
		emit dataChanged(index(static_cast<int>(target)), index(static_cast<int>(target)));
	}

	void review_landing_model::clear()
	{
		if (Items.empty()) {
			return;
		}
		beginRemoveRows({ }, 0, static_cast<int>(Items.size() - 1U));
		Items.clear();
		Positions.clear();
		endRemoveRows();
		adopt_accounts();
	}

	auto review_landing_model::accounts() const noexcept -> const std::vector<home_account>&
	{
		return Accounts;
	}

	void review_landing_model::adopt_accounts()
	{
		const auto named = Accounts.size();
		Accounts.clear();
		for (const auto& item : Items) {
			auto account = home_account{ .Host = text(item.Ref.Host), .Login = text(item.Ref.Account) };
			if (!std::ranges::contains(Accounts, account)) {
				Accounts.push_back(std::move(account));
			}
		}
		std::ranges::sort(Accounts);
		// [[BPR-0783]]. Whether a chip is drawn at all is the account count, so
		// crossing one changes the chip of rows that did not otherwise change.
		// That is a data change over the existing rows, never a reset.
		if ((named > 1U) != (Accounts.size() > 1U) && !Items.empty()) {
			emit dataChanged(index(0),
							 index(static_cast<int>(Items.size()) - 1),
							 QList<int>{ home_row_role::account_chip });
		}
	}

	auto review_landing_model::item_at(const QModelIndex& aIndex) const
		-> std::optional<core::queue_item>
	{
		if (!aIndex.isValid() || aIndex.parent().isValid()) {
			return std::nullopt;
		}
		const auto row = static_cast<std::size_t>(aIndex.row());
		if (row >= Items.size()) {
			return std::nullopt;
		}
		return Items[row];
	}

	review_landing_filter_model::review_landing_filter_model(
		review_landing_model& aSource, QObject* const aParent)
		: QSortFilterProxyModel{ aParent }
		, Source{ aSource }
	{
		setDynamicSortFilter(true);
		setSourceModel(&Source);
		sort(-1);
	}

	void review_landing_filter_model::apply(
		const application::queue_presentation_preferences& aPreferences)
	{
		if (!application::valid(aPreferences) || aPreferences == Preferences) {
			return;
		}
		adopt(aPreferences, View);
	}

	void review_landing_filter_model::set_home_view(const application::home_view_state& aView)
	{
		if (!application::valid(aView) || aView == View) {
			return;
		}
		adopt(Preferences, aView);
	}

	void review_landing_filter_model::adopt(
		const application::queue_presentation_preferences& aPreferences,
		const application::home_view_state&                aView)
	{
		// [[BPR-0783]]. Home reads one set of preferences: the window's own four
		// once it owns them, otherwise the profile's, with Repository and Bucket
		// cleared because Group and the state order do their job now. The
		// profile's answer is retained unchanged for the Settings page.
		const auto effective   = application::home_preferences(aView, aPreferences);
		const auto sortChanged = effective.Sort != Effective.Sort;
		const auto filtered    = effective != Effective;
		Preferences            = aPreferences;
		View                   = aView;
		if (!filtered) {
			// Nothing Home filters by moved: a Repository or Bucket default the
			// profile keeps for its own store must not disturb Home's rows.
			return;
		}
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
		beginFilterChange();
		Effective = effective;
		endFilterChange(Direction::Rows);
#else
		Effective = effective;
		invalidateFilter();
#endif
		if (sortChanged) {
			// Qt returns early when the requested column and order are unchanged,
			// even though our explicit sort vocabulary changed the comparator.
			// Disabling the proxy sort first makes that transition observable on
			// every supported Qt version.
			sort(-1);
			if (Effective.Sort
				!= application::queue_sort_order::core_priority) {
				sort(0, Qt::AscendingOrder);
			}
		}
	}

	auto review_landing_filter_model::preferences() const noexcept
		-> const application::queue_presentation_preferences&
	{
		return Preferences;
	}

	auto review_landing_filter_model::effective_preferences() const noexcept
		-> const application::queue_presentation_preferences&
	{
		return Effective;
	}

	auto review_landing_filter_model::home_view() const noexcept
		-> const application::home_view_state&
	{
		return View;
	}

	auto review_landing_filter_model::item_at(const QModelIndex& aIndex) const
		-> std::optional<core::queue_item>
	{
		return Source.item_at(mapToSource(aIndex));
	}

	auto review_landing_filter_model::accounts() const noexcept -> const std::vector<home_account>&
	{
		return Source.accounts();
	}

	auto review_landing_filter_model::has_active_filters() const noexcept -> bool
	{
		// [[BPR-0783]]. What Home is filtered by, so the queue's status sentence
		// explains the rows Home is actually hiding.
		return Effective.Role.has_value()
			|| Effective.ObserverReason.has_value()
			|| Effective.Bucket.has_value()
			|| !Effective.Repository.empty()
			|| Effective.Quiet != application::queue_quiet_filter::all;
	}

	auto review_landing_filter_model::filterAcceptsRow(
		const int aSourceRow, const QModelIndex& aSourceParent) const -> bool
	{
		const auto item = Source.item_at(Source.index(aSourceRow, 0, aSourceParent));
		if (!item) {
			return false;
		}
		const auto role = item->Membership.authored() ? core::github_role::author : item->Membership.Role;
		if (Effective.Role.has_value() && role != *Effective.Role) {
			return false;
		}
		if (Effective.ObserverReason.has_value()
			&& (!item->Membership.MatchedRule
				|| item->Membership.Rule.Kind != *Effective.ObserverReason)) {
			return false;
		}
		if (Effective.Bucket.has_value()
			&& item->Bucket != *Effective.Bucket) {
			return false;
		}
		if (!Effective.Repository.empty()
			&& item->Ref.Repository != Effective.Repository) {
			return false;
		}
		if (Effective.Quiet == application::queue_quiet_filter::attention
			&& item->Bucket == core::attention_bucket::quiet && !core::merge_blocked(item->Facts.Waiting)) {
			return false;
		}
		if (Effective.Quiet == application::queue_quiet_filter::quiet
			&& (item->Bucket != core::attention_bucket::quiet || core::merge_blocked(item->Facts.Waiting))) {
			return false;
		}
		return true;
	}

	auto review_landing_filter_model::lessThan(
		const QModelIndex& aLeft, const QModelIndex& aRight) const -> bool
	{
		const auto left  = Source.item_at(aLeft);
		const auto right = Source.item_at(aRight);
		if (!left || !right) {
			return aLeft.row() < aRight.row();
		}
		const auto& leftRef  = left->Ref;
		const auto& rightRef = right->Ref;
		switch (Effective.Sort) {
		case application::queue_sort_order::repository:
			return std::tie(
					   leftRef.Repository,
					   leftRef.Number,
					   leftRef.Host,
					   leftRef.Account)
				< std::tie(
					   rightRef.Repository,
					   rightRef.Number,
					   rightRef.Host,
					   rightRef.Account);
		case application::queue_sort_order::pull_request_number:
			return std::tie(
					   leftRef.Number,
					   leftRef.Repository,
					   leftRef.Host,
					   leftRef.Account)
				< std::tie(
					   rightRef.Number,
					   rightRef.Repository,
					   rightRef.Host,
					   rightRef.Account);
		case application::queue_sort_order::core_priority:
			return aLeft.row() < aRight.row();
		}
		return aLeft.row() < aRight.row();
	}
} // namespace bpr::qt
