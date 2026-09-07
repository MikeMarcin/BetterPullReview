#include "bpr/application/pull_request_overview.hpp"

#include "bpr/core/diff_model.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace bpr::application {
	namespace {
		[[nodiscard]] auto operation_of(const github_file_change aChange) noexcept
			-> overview_file_operation
		{
			switch (aChange) {
			case github_file_change::added:
				return overview_file_operation::added;
			case github_file_change::removed:
				return overview_file_operation::removed;
			case github_file_change::renamed:
				return overview_file_operation::renamed;
			case github_file_change::copied:
				return overview_file_operation::copied;
			case github_file_change::type_changed:
				return overview_file_operation::type_changed;
			case github_file_change::unchanged:
				return overview_file_operation::unchanged;
			case github_file_change::uninitialized:
			case github_file_change::modified:
				break;
			}
			return overview_file_operation::modified;
		}

		[[nodiscard]] auto signoff_of(const github_review_state aState) noexcept
			-> overview_signoff
		{
			switch (aState) {
			case github_review_state::approved:
				return overview_signoff::approved;
			case github_review_state::changes_requested:
				return overview_signoff::changes_requested;
			case github_review_state::dismissed:
				return overview_signoff::dismissed;
			case github_review_state::pending:
				return overview_signoff::pending;
			case github_review_state::commented:
				return overview_signoff::commented;
			case github_review_state::uninitialized:
				break;
			}
			return overview_signoff::none;
		}

		/// \brief Splits a path at its last separator.
		[[nodiscard]] auto split_path(const std::string_view aPath)
			-> std::pair<std::string, std::string>
		{
			const auto slash = aPath.rfind('/');
			if (slash == std::string_view::npos) {
				return { std::string{ }, std::string{ aPath } };
			}
			return { std::string{ aPath.substr(0U, slash) },
					 std::string{ aPath.substr(slash + 1U) } };
		}

		[[nodiscard]] auto login_of(
			const std::optional<github_actor_record>& aActor) -> std::string
		{
			return aActor.has_value() ? aActor->Login : std::string{ };
		}

		/// \brief A participant list that keeps first-seen order.
		class participant_roster final
		{
		public:
			explicit participant_roster(std::string aViewer)
				: Viewer{ std::move(aViewer) }
			{
			}

			void place(
				const std::string&              aLogin,
				const overview_participant_role aRole,
				const overview_signoff          aSignoff)
			{
				if (aLogin.empty()) {
					return;
				}
				auto* const existing = find(aLogin);
				if (existing == nullptr) {
					Participants.push_back(overview_participant{
						.Login   = aLogin,
						.Role    = aRole,
						.Signoff = aSignoff,
						.Viewer  = aLogin == Viewer,
					});
					return;
				}
				// A stronger role wins; the author stays the author however
				// much they comment, and a reviewer who also commented is a
				// reviewer. A later review replaces an earlier standing.
				if (aRole < existing->Role) {
					existing->Role = aRole;
				}
				if (aSignoff != overview_signoff::commented
					|| existing->Signoff == overview_signoff::none) {
					if (existing->Role != overview_participant_role::author
						|| aSignoff != overview_signoff::commented) {
						existing->Signoff = aSignoff;
					}
				}
			}

			[[nodiscard]] auto take() -> std::vector<overview_participant>
			{
				return std::move(Participants);
			}

		private:
			[[nodiscard]] auto find(const std::string& aLogin) -> overview_participant*
			{
				const auto found = std::ranges::find(
					Participants, aLogin, &overview_participant::Login);
				return found == Participants.end() ? nullptr : &*found;
			}

			std::string                       Viewer;
			std::vector<overview_participant> Participants;
		};

		struct thread_facts
		{
			std::size_t Threads             = 0U;
			std::size_t Unresolved          = 0U;
			std::size_t RepliesToMe         = 0U;
			std::size_t MyUnresolvedThreads = 0U;
		};

		[[nodiscard]] auto same_incarnation(
			const core::epoch_catalog_identity& aLeft,
			const core::epoch_catalog_identity& aRight) noexcept -> bool
		{
			return aLeft.Incarnation == aRight.Incarnation;
		}
	} // namespace

	auto project_pull_request_overview(
		const github_pull_request_snapshot& aSnapshot,
		const overview_context&             aContext)
		-> pull_request_overview_projection
	{
		auto        projection = pull_request_overview_projection{ };
		const auto& request    = aSnapshot.PullRequest;
		const auto  viewer     = aSnapshot.Viewer.Login;

		projection.Owner          = aSnapshot.Repository.Owner;
		projection.RepositoryName = aSnapshot.Repository.Name;
		projection.Number         = request.Number;
		projection.Title          = request.Title;
		projection.Body           = request.Body;
		projection.AuthorLogin    = login_of(request.Author);
		projection.BaseRef        = request.BaseRef;
		projection.HeadRef        = request.HeadRef;
		projection.State          = request.State;
		projection.Draft          = request.Draft;
		projection.CommitCount    = aSnapshot.Commits.size();

		// My last review, and what the retained updates say about it.
		const auto lastReviewed = aContext.Private.has_value()
			? aContext.Private->LastReviewedHead
			: std::nullopt;
		auto&      since        = projection.SinceLastReview;
		since.Known             = lastReviewed.has_value();
		auto updates            = aContext.Updates;
		std::ranges::sort(updates, [](const epoch_row_projection& aLeft, const epoch_row_projection& aRight) {
			return std::pair{ aLeft.ObservedAt, aLeft.Id } < std::pair{ aRight.ObservedAt, aRight.Id };
		});
		if (lastReviewed.has_value()) {
			const auto reviewed = std::ranges::find(updates, *lastReviewed, &epoch_row_projection::Head);
			if (reviewed != updates.end()) {
				since.ReviewedAt = reviewed->ObservedAt;
				since.Uploads    = static_cast<std::size_t>(std::distance(reviewed, updates.end())) - 1U;
			}
		}
		const auto headMoved = lastReviewed.has_value()
			&& (!aContext.Head.has_value() || *aContext.Head != *lastReviewed);
		auto changedPaths = std::unordered_set<std::string_view>{ };
		if (aContext.ChangedSinceReviewPaths.has_value()) {
			for (const auto& path : *aContext.ChangedSinceReviewPaths) {
				changedPaths.insert(path);
			}
		}
		// A head I already reviewed has nothing changed since; a moved head has
		// a count only once its change set is resolved.
		if (lastReviewed.has_value() && (!headMoved || aContext.ChangedSinceReviewPaths.has_value())) {
			since.FilesChanged = 0U;
		}

		// Threads, per file and mine.
		auto factsByPath = std::vector<std::pair<std::string, thread_facts>>{ };
		auto factsFor    = [&factsByPath](const std::string& aPath) -> thread_facts& {
			const auto found = std::ranges::find(factsByPath, aPath, &std::pair<std::string, thread_facts>::first);
			if (found != factsByPath.end()) {
				return found->second;
			}
			factsByPath.emplace_back(aPath, thread_facts{ });
			return factsByPath.back().second;
		};
		auto roster = participant_roster{ viewer };
		roster.place(projection.AuthorLogin, overview_participant_role::author, overview_signoff::uploaded);
		if (aSnapshot.ReviewThreadsAvailability == github_collection_availability::available) {
			for (const auto& thread : aSnapshot.ReviewThreads) {
				auto& facts = factsFor(thread.Path);
				++facts.Threads;
				if (!thread.Resolved) {
					++facts.Unresolved;
				}
				const auto rootLogin = thread.Comments.empty()
					? std::string{ }
					: login_of(thread.Comments.front().Author);
				if (!thread.Resolved && !viewer.empty() && rootLogin == viewer) {
					++since.MyUnresolvedThreads;
				}
				for (const auto& comment : thread.Comments) {
					const auto login = login_of(comment.Author);
					roster.place(login, overview_participant_role::observer, overview_signoff::commented);
					if (comment.ReplyTo.has_value() && !viewer.empty() && login != viewer) {
						const auto parent = std::ranges::find(
							thread.Comments, *comment.ReplyTo, &github_review_comment_record::Comment);
						if (parent != thread.Comments.end() && login_of(parent->Author) == viewer) {
							++since.RepliesToMe;
						}
					}
				}
			}
		} else if (aSnapshot.ReviewCommentsAvailability == github_collection_availability::available) {
			for (const auto& group : aSnapshot.ReviewCommentGroups) {
				auto& facts = factsFor(group.Root.Path);
				++facts.Threads;
				const auto rootLogin = login_of(group.Root.Author);
				roster.place(rootLogin, overview_participant_role::observer, overview_signoff::commented);
				for (const auto& reply : group.Replies) {
					const auto login = login_of(reply.Author);
					roster.place(login, overview_participant_role::observer, overview_signoff::commented);
					if (!viewer.empty() && rootLogin == viewer && login != viewer) {
						++since.RepliesToMe;
					}
				}
			}
		}
		for (const auto& comment : aSnapshot.IssueComments) {
			roster.place(login_of(comment.Author), overview_participant_role::observer, overview_signoff::commented);
		}
		for (const auto& review : aSnapshot.Reviews) {
			const auto login = login_of(review.Author);
			if (login == projection.AuthorLogin) {
				continue;
			}
			roster.place(login, overview_participant_role::reviewer, signoff_of(review.State));
		}
		projection.Participants = roster.take();

		// Checks.
		for (const auto& check : aSnapshot.Checks) {
			auto& checks = projection.Checks;
			if (check.Status != github_check_status::completed) {
				++checks.Pending;
				continue;
			}
			switch (check.Conclusion) {
			case github_check_conclusion::success:
				++checks.Passed;
				break;
			case github_check_conclusion::failure:
			case github_check_conclusion::timed_out:
			case github_check_conclusion::action_required:
			case github_check_conclusion::cancelled:
				++checks.Failed;
				break;
			case github_check_conclusion::skipped:
				++checks.Skipped;
				break;
			case github_check_conclusion::uninitialized:
			case github_check_conclusion::none:
			case github_check_conclusion::neutral:
			case github_check_conclusion::stale:
				++checks.Other;
				break;
			}
		}

		// The files.
		projection.Files.reserve(aSnapshot.Files.size());
		for (const auto& file : aSnapshot.Files) {
			auto row                       = overview_file_row{ };
			row.Path                       = file.Path;
			row.PreviousPath               = file.PreviousPath;
			std::tie(row.Folder, row.Name) = split_path(file.Path);
			row.Operation                  = operation_of(file.Change);
			row.Additions                  = file.Additions;
			row.Deletions                  = file.Deletions;
			// Unchanged unless the head moved since my last review: then unknown
			// until the change set is resolved, and changed or new by that set.
			if (lastReviewed.has_value() && headMoved && !aContext.ChangedSinceReviewPaths.has_value()) {
				row.Changed = overview_change_since_review::unknown;
			} else if (lastReviewed.has_value() && headMoved && changedPaths.contains(file.Path)) {
				row.Changed = row.Operation == overview_file_operation::added
					? overview_change_since_review::added
					: overview_change_since_review::changed;
				++*since.FilesChanged;
			}
			const auto identity = core::file_identity_of(file.Path);
			const auto receipt  = std::ranges::find(aContext.Receipts, identity, &core::file_receipt::File);
			if (receipt != aContext.Receipts.end()) {
				row.Read = receipt->Read;
				if (receipt->Read == core::file_read_state::read) {
					++projection.Progress.FilesRead;
				}
			}
			const auto facts = std::ranges::find(factsByPath, file.Path, &std::pair<std::string, thread_facts>::first);
			if (facts != factsByPath.end()) {
				row.ThreadCount           = facts->second.Threads;
				row.UnresolvedThreadCount = facts->second.Unresolved;
			}
			if (aContext.Private.has_value()) {
				const auto note = std::ranges::find_if(
					aContext.Private->Notes, [&identity](const local_note_record& aNote) {
						return aNote.Note.File.has_value() && *aNote.Note.File == identity;
					});
				if (note != aContext.Private->Notes.end()) {
					row.Note = note->Note.Text;
				}
			}
			projection.Files.push_back(std::move(row));
		}
		std::ranges::stable_sort(projection.Files, [](const overview_file_row& aLeft, const overview_file_row& aRight) {
			return std::pair{ std::string_view{ aLeft.Folder }, std::string_view{ aLeft.Name } }
			< std::pair{ std::string_view{ aRight.Folder }, std::string_view{ aRight.Name } };
		});

		projection.Progress.FilesTotal      = projection.Files.size();
		projection.Progress.PrivateNotes    = aContext.Private.has_value() ? aContext.Private->Notes.size() : 0U;
		projection.Progress.DraftedComments = aContext.DraftedComments;

		// The one action. A first visit starts at the first file; otherwise
		// the review continues at the first file that changed since my last
		// review, else the first file I have not read.
		const auto firstVisit = !lastReviewed.has_value() && aContext.Receipts.empty();
		projection.Entry      = firstVisit ? overview_entry_kind::start_review : overview_entry_kind::continue_review;
		const auto changed    = std::ranges::find_if(projection.Files, [](const overview_file_row& aRow) {
			return aRow.Changed == overview_change_since_review::changed
				|| aRow.Changed == overview_change_since_review::added;
		});
		const auto unread     = std::ranges::find_if(projection.Files, [](const overview_file_row& aRow) {
			return !aRow.Read.has_value() || *aRow.Read != core::file_read_state::read;
		});
		if (!firstVisit && changed != projection.Files.end()) {
			projection.EntryPath = changed->Path;
		} else if (!firstVisit && unread != projection.Files.end()) {
			projection.EntryPath = unread->Path;
		} else if (!projection.Files.empty()) {
			projection.EntryPath = projection.Files.front().Path;
		}
		return projection;
	}

	void epoch_row_ledger::apply(const epoch_catalog_projection& aProjection)
	{
		if (aProjection.Replace) {
			Catalog = aProjection.Catalog;
			Rows.clear();
		} else if (aProjection.Catalog.has_value()) {
			if (Catalog.has_value() && !same_incarnation(*Catalog, *aProjection.Catalog)) {
				Rows.clear();
			}
			Catalog = aProjection.Catalog;
		}
		const auto remove = [this](const core::review_epoch_id& aId) {
			std::erase_if(Rows, [&aId](const epoch_row_projection& aRow) { return aRow.Id == aId; });
		};
		const auto insertOrMove = [this, &remove](const std::size_t aIndex, epoch_row_projection aRow) {
			remove(aRow.Id);
			const auto target = std::min(aIndex, Rows.size());
			Rows.insert(Rows.begin() + static_cast<std::ptrdiff_t>(target), std::move(aRow));
		};
		for (const auto& change : aProjection.Changes) {
			switch (change.Kind) {
			case epoch_row_change_kind::insert:
				if (change.Row.has_value() && change.Row->Id == change.Id) {
					insertOrMove(change.Index, *change.Row);
				}
				break;
			case epoch_row_change_kind::remove:
				remove(change.Id);
				break;
			case epoch_row_change_kind::update:
				if (change.Row.has_value() && change.Row->Id == change.Id) {
					const auto existing = std::ranges::find(Rows, change.Id, &epoch_row_projection::Id);
					if (existing != Rows.end()) {
						*existing = *change.Row;
					}
				}
				break;
			}
		}
		while (Rows.size() > core::maximum_epoch_history_page_size) {
			Rows.pop_back();
		}
		if (aProjection.HistoryPage.has_value()
			&& aProjection.HistoryPage->Phase == core::epoch_history_page_phase::loaded
			&& aProjection.HistoryPage->Rows.size() <= core::maximum_epoch_history_page_size
			&& (!Catalog.has_value() || same_incarnation(*Catalog, aProjection.HistoryPage->Catalog))) {
			Catalog          = aProjection.HistoryPage->Catalog;
			const auto& page = aProjection.HistoryPage->Rows;
			std::erase_if(Rows, [&page](const epoch_row_projection& aRow) {
				return std::ranges::find(page, aRow.Id, &epoch_row_projection::Id) == page.end();
			});
			for (std::size_t index = 0U; index < page.size(); ++index) {
				insertOrMove(index, page[index]);
			}
		}
	}

	void epoch_row_ledger::clear()
	{
		Catalog.reset();
		Rows.clear();
	}

	auto epoch_row_ledger::rows() const noexcept -> const std::vector<epoch_row_projection>&
	{
		return Rows;
	}
} // namespace bpr::application
