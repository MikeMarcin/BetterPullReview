#include "bpr/core/file_progress_mark.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/generated_files_view.hpp"
#include "bpr/qt/pending_veil.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/steady_columns.hpp"
#include "overview_summary_splitter.hpp"
#include "pull_request_overview_support.hpp"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QAbstractSlider>
#include <QAccessible>
#include <QAccessibleAnnouncementEvent>
#include <QAnyStringView>
#include <QBoxLayout>
#include <QChar>
#include <QColor>
#include <QDateTime>
#include <QFontMetrics>
#include <QFrame>
#include <QHash>
#include <QLabel>
#include <QLatin1Char>
#include <QLatin1StringView>
#include <QLayoutItem>
#include <QList>
#include <QListSpecialMethods>
#include <QObject>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSet>
#include <QSignalBlocker>
#include <QSizeF>
#include <QSizePolicy>
#include <QString>
#include <QStringDecoder>
#include <QTextDocument>
#include <QTimeZone>
#include <QToolButton>
#include <QTreeView>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto checks_text(const application::overview_check_summary& aChecks) -> QString
		{
			if (aChecks.total() == 0U) {
				return QStringLiteral("none");
			}
			auto parts = QStringList{ };
			if (aChecks.Passed != 0U) {
				parts.push_back(QStringLiteral("%1 passed").arg(count_text(aChecks.Passed)));
			}
			if (aChecks.Failed != 0U) {
				parts.push_back(QStringLiteral("%1 failed").arg(count_text(aChecks.Failed)));
			}
			if (aChecks.Pending != 0U) {
				parts.push_back(QStringLiteral("%1 pending").arg(count_text(aChecks.Pending)));
			}
			if (aChecks.Skipped != 0U) {
				parts.push_back(QStringLiteral("%1 skipped").arg(count_text(aChecks.Skipped)));
			}
			if (aChecks.Other != 0U) {
				parts.push_back(QStringLiteral("%1 other").arg(count_text(aChecks.Other)));
			}
			return parts.join(QStringLiteral(", "));
		}

		void present_heading(QLabel& aHeading, const QString& aText, const QString& aNote = { })
		{
			static_cast<eliding_heading&>(aHeading).present(aText, aNote);
		}

		auto cached_side_text(core::cached_thread_side aSide) -> QString
		{
			switch (aSide) {
			case core::cached_thread_side::left:    return QStringLiteral("left");
			case core::cached_thread_side::right:   return QStringLiteral("right");
			case core::cached_thread_side::unknown: return QStringLiteral("unknown side");
			}
			return QStringLiteral("unknown side");
		}

		auto cached_thread_text(const std::vector<core::cached_review_thread>& aThreads) -> QString
		{
			QString result;
			for (const auto& thread : aThreads) {
				result += QStringLiteral("%1 · %2%3\n")
							  .arg(text(thread.Path), thread.Resolved ? QStringLiteral("was resolved") : QStringLiteral("was unresolved"),
								   thread.Outdated ? QStringLiteral(" · was outdated") : QString{ });
				if (thread.FileLevel) {
					result += QStringLiteral("File discussion\n");
				} else {
					const auto side = cached_side_text(thread.Side);
					result += QStringLiteral("Observed anchor: %1 · line %2 · original line %3\n")
								  .arg(side, thread.Line ? QString::number(*thread.Line) : QStringLiteral("unavailable"),
									   thread.OriginalLine ? QString::number(*thread.OriginalLine) : QStringLiteral("unavailable"));
				}
				if (thread.Comments.empty()) {
					result += QStringLiteral("No comments observed in this thread.\n");
				}
				for (const auto& comment : thread.Comments) {
					result += QStringLiteral("%1%2%3\n%4\n\n")
								  .arg(comment.ReplyTo ? QStringLiteral("Reply · ") : QString{ },
									   comment.AuthorLogin.empty() ? QStringLiteral("Unknown author") : text(comment.AuthorLogin),
									   comment.Outdated ? QStringLiteral(" · was outdated") : QString{ },
									   comment.Deleted ? QStringLiteral("Comment was deleted") : text(comment.Body));
				}
			}
			return result;
		}

		auto cached_discussion_text(const std::optional<std::vector<core::cached_discussion_message>>& aMessages, const QString& aHeading) -> QString
		{
			if (!aMessages) {
				return aHeading + QStringLiteral(" — unavailable offline\n\n");
			}
			auto result = aHeading + QStringLiteral(" — %1 observed · freshness unknown\n").arg(count_text(aMessages->size()));
			for (const auto& message : *aMessages) {
				QString state;
				switch (message.Kind) {
				case core::cached_discussion_kind::general_comment:          break;
				case core::cached_discussion_kind::pending_review:           state = QStringLiteral("was pending"); break;
				case core::cached_discussion_kind::commented_review:         state = QStringLiteral("was commented"); break;
				case core::cached_discussion_kind::approved_review:          state = QStringLiteral("was approved"); break;
				case core::cached_discussion_kind::changes_requested_review: state = QStringLiteral("changes were requested"); break;
				case core::cached_discussion_kind::dismissed_review:         state = QStringLiteral("was dismissed"); break;
				}
				result += text(message.AuthorLogin.empty() ? "Unknown author" : message.AuthorLogin) + QStringLiteral(" · ") + state;
				if (message.Deleted) {
					result += QStringLiteral(" · was deleted");
				}
				if (message.CommitOid) {
					result += QStringLiteral("\nObserved review commit: ") + text(*message.CommitOid);
				}
				result += QStringLiteral("\n") + text(message.Body) + QStringLiteral("\n\n");
			}
			return result + QStringLiteral("\n");
		}

		auto cached_file_rows(const std::optional<std::vector<core::cached_review_file>>& aCatalogue)
			-> std::vector<application::overview_file_row>
		{
			std::vector<application::overview_file_row> rows;
			if (aCatalogue) {
				rows.reserve(aCatalogue->size());
				for (const auto& file : *aCatalogue) {
					auto       row       = application::overview_file_row{ .Path = file.Path, .PreviousPath = file.PreviousPath };
					const auto separator = file.Path.rfind('/');
					row.Folder           = separator == std::string::npos ? std::string{ } : file.Path.substr(0, separator);
					row.Name             = separator == std::string::npos ? file.Path : file.Path.substr(separator + 1);
					switch (file.Change) {
					case core::cached_file_change::added:        row.Operation = application::overview_file_operation::added; break;
					case core::cached_file_change::modified:     row.Operation = application::overview_file_operation::modified; break;
					case core::cached_file_change::removed:      row.Operation = application::overview_file_operation::removed; break;
					case core::cached_file_change::renamed:      row.Operation = application::overview_file_operation::renamed; break;
					case core::cached_file_change::copied:       row.Operation = application::overview_file_operation::copied; break;
					case core::cached_file_change::type_changed: row.Operation = application::overview_file_operation::type_changed; break;
					case core::cached_file_change::unchanged:    row.Operation = application::overview_file_operation::unchanged; break;
					}
					row.Additions = file.Additions;
					row.Deletions = file.Deletions;
					rows.push_back(std::move(row));
				}
			}
			return rows;
		}

		/// \brief [[BPR-1405]]. The merge section said once, for a polite
		/// announcement when an accepted refresh changes it.
		[[nodiscard]] auto merge_announcement(const application::pull_request_overview_projection& aProjection) -> QString
		{
			auto parts = QStringList{ QStringLiteral("Merge: %1").arg(text(aProjection.MergeState)) };
			for (const auto& condition : aProjection.MergeConditions) {
				parts.push_back(text(condition));
			}
			if (!aProjection.ReviewRequests.empty()) {
				auto names = QStringList{ };
				for (const auto& request : aProjection.ReviewRequests) {
					names.push_back(request.Name.empty() ? QStringLiteral("an unavailable reviewer") : text(request.Name));
				}
				parts.push_back(QStringLiteral("Waiting on %1").arg(names.join(QStringLiteral(", "))));
			}
			return parts.join(QStringLiteral(". ")) + QLatin1Char{ '.' };
		}
	}

	void pull_request_overview_page::present(const application::pull_request_overview_projection& aProjection)
	{
		Cached   = false;
		Loading  = false;
		Observed = false;
		CachedBase.clear();
		CachedHead.clear();
		CachedSource->clear();
		CachedSource->hide();
		CachedSourceBack->hide();
		CachedDiscussion->clear();
		CachedDiscussion->hide();
		CachedThreads->clear();
		CachedThreads->hide();
		CachedThreadsHeading->hide();
		const auto bodyChanged         = Projection.has_value() && Projection->Body != aProjection.Body;
		const auto requirementsChanged = Projection && (Projection->MergeState != aProjection.MergeState || Projection->MergeConditions != aProjection.MergeConditions || Projection->ReviewRequests != aProjection.ReviewRequests);
		const auto updateBody          = !Projection.has_value() || bodyChanged;
		// [[BPR-1089]]. Another pull request starts from the Settings default,
		// in file order, with no counts until the host answers for it.
		const auto samePullRequest = Projection.has_value() && Projection->Owner == aProjection.Owner && Projection->RepositoryName == aProjection.RepositoryName
			&& Projection->Number == aProjection.Number;
		if (!samePullRequest) {
			ModeChosen = false;
			Mode       = DefaultMode;
			Sort.reset();
			AskedFeedback.reset();
			Model->apply_feedback(std::nullopt);
		}
		Projection    = aProjection;
		ObservedFiles = false;
		for (int column = overview_file_model::changed_column; column < overview_file_model::column_count; ++column) {
			Files->setColumnHidden(column, false);
		}
		PendingOpen.clear();
		Files->setAccessibleDescription(QStringLiteral("Current file catalogue; activate a file to read it; %1.").arg(files_ends_help()));
		Summary->setToolTip({ });
		Summary->setAccessibleDescription({ });
		Title->setText(text(aProjection.Title));
		auto state = QString{ };
		switch (aProjection.State) {
		case application::github_pull_request_state::merged:
			state = QStringLiteral(" Merged.");
			break;
		case application::github_pull_request_state::closed:
			state = QStringLiteral(" Closed without merge.");
			break;
		case application::github_pull_request_state::open:
			// [[BPR-1852]]. A draft is said by the chip and the banner.
			break;
		}
		ShownDraft        = aProjection.Draft;
		ShownOpen         = aProjection.State == application::github_pull_request_state::open;
		ShownDraftPending = aProjection.DraftPending;
		if (!samePullRequest) {
			ReadyBusy   = false;
			ConvertBusy = false;
			ReadyStatus->clear();
			ConvertStatus->clear();
		}
		present_draft_controls();
		auto commits = aProjection.CommitCount == 1U
			? QStringLiteral("1 commit")
			: QStringLiteral("%1 commits").arg(count_text(aProjection.CommitCount));
		if (aProjection.ReturnedCommitCount < aProjection.CommitCount) {
			commits += QStringLiteral(" (%1 returned by GitHub)")
						   .arg(count_text(aProjection.ReturnedCommitCount));
		}
		Summary->setText(QStringLiteral("%1 wants to merge %2 into %3 from %4.%5")
							 .arg(text(aProjection.AuthorLogin),
								  commits,
								  text(aProjection.BaseRef),
								  text(aProjection.HeadRef),
								  state));
		if (updateBody) {
			Body->present_body(aProjection.Body);
		}
		Body->setVisible(!aProjection.Body.empty());
		SummaryPane->setVisible(!aProjection.Body.empty());
		FullView->setVisible(!aProjection.Body.empty());
		SummarySplit->queue_layout();
		EmptyBody->setVisible(aProjection.Body.empty());
		if (aProjection.Body.empty() && Body->expanded()) {
			set_description_expanded(false);
		}
		// [[BPR-1774]]. Another pull request ends an edit of this one; the
		// same one keeps it, typed text and all, whatever GitHub now says.
		if (!samePullRequest && DescriptionEditing) {
			close_description_editor();
		}
		present_description_controls();
		if (bodyChanged || requirementsChanged) {
			const auto                   announcement = (bodyChanged ? QStringLiteral("Description updated. ") : QString{ }) + (requirementsChanged ? merge_announcement(aProjection) : QString{ });
			QAccessibleAnnouncementEvent event{ Body, announcement };
			event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&event);
		}

		const auto& since = aProjection.SinceLastReview;
		present_files_heading(aProjection);
		apply_rows();
		update_folder_controls();
		present_feedback_controls();
		dispatch_feedback_request();

		rebuild_participants(aProjection.Participants);

		apply_history({ .Known = since.Known, .ReviewedAt = since.ReviewedAt, .Uploads = since.Uploads });
		// [[BPR-1589]]. What happened since my last review, in My review's
		// details: a fact with no value is not drawn.
		set_value(overview_value::files_changed,
				  since.FilesChanged.has_value() ? count_text(*since.FilesChanged) : QStringLiteral("not yet resolved"),
				  since.FilesChanged.value_or(0U) != 0U);
		show_value(overview_value::files_changed, since.Known && since.FilesChanged.value_or(1U) != 0U);
		set_value(overview_value::new_comments, count_text(since.NewComments.value_or(0U)), since.NewComments.value_or(0U) != 0U);
		show_value(overview_value::new_comments, since.NewComments.value_or(0U) != 0U);
		set_value(overview_value::replies_to_me, count_text(since.RepliesToMe), since.RepliesToMe != 0U);
		show_value(overview_value::replies_to_me, since.RepliesToMe != 0U);
		set_value(overview_value::my_unresolved_threads, count_text(since.MyUnresolvedThreads), false);
		show_value(overview_value::my_unresolved_threads, since.MyUnresolvedThreads != 0U);
		present_checks(samePullRequest || ObservedChecks.has_value());
		present_merge(aProjection);
		if (!samePullRequest) {
			RequirementDisclosure->setChecked(false);
		}
		RequirementDetails->setVisible(RequirementDisclosure->isChecked());
		present_status(samePullRequest);
		ObservedChecks.reset();
		ObservedPeople.reset();
		ObservedMerge.reset();

		apply_pending();
	}

	void pull_request_overview_page::set_check_link_dispatch(std::function<void(const std::string&, const std::string&)> aDispatch)
	{
		CheckLinkDispatch = std::move(aDispatch);
	}

	void pull_request_overview_page::present_checks(const bool aSamePullRequest)
	{
		const auto& rows     = Projection ? Projection->CheckRows : ObservedChecks->CheckRows;
		const auto& tally    = Projection ? Projection->Checks : ObservedChecks->Checks;
		const auto  complete = Projection ? Projection->ChecksComplete : ObservedChecks->ChecksComplete;
		const auto& note     = Projection ? Projection->ChecksNote : ObservedChecks->ChecksNote;
		// [[BPR-1589]]. One bar of passed, failed and skipped, and their counts;
		// a pull request with no results draws no Checks at all.
		Sections[static_cast<std::size_t>(overview_status_section::checks)].Frame->setVisible((Loading && ObservedChecks.has_value()) || !rows.empty() || tally.total() != 0U);
		auto summary = tally.total() == 0U ? QStringLiteral("No results yet") : checks_text(tally);
		if (!complete) {
			summary += QStringLiteral(" (partial)");
		}
		set_value(overview_value::checks, summary, false);
		ChecksMeter->set_segments({ { static_cast<double>(tally.Passed), Colours.Accepted },
									{ static_cast<double>(tally.Failed), Colours.Defect },
									{ static_cast<double>(tally.Pending), Colours.Caution } },
								  static_cast<double>(tally.total()));
		ChecksMeter->setAccessibleName(QStringLiteral("Checks: %1").arg(summary));
		ChecksDisclosure->set_count(tally.total() == 0U ? QString{ } : count_text(tally.total()));
		ChecksDisclosure->setAccessibleName(QStringLiteral("Checks: %1").arg(summary));
		ChecksNote->setText(text(note));
		ChecksNote->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		const auto           selected = aSamePullRequest && CheckList->currentItem() ? CheckList->currentItem()->data(0, Qt::UserRole).toString() : QString{ };
		const auto           scroll   = aSamePullRequest ? CheckList->verticalScrollBar()->value() : 0;
		const QSignalBlocker blocked{ CheckList };
		auto                 desired = QSet<QString>{ };
		for (const auto& row : rows) {
			desired.insert(text(row.Identity));
		}
		for (auto it = CheckItems.begin(); it != CheckItems.end();) {
			if (!desired.contains(it.key())) {
				delete it.value();
				it = CheckItems.erase(it);
			} else {
				++it;
			}
		}
		int position = 0;
		for (const auto& row : rows) {
			const auto key  = text(row.Identity);
			auto*      item = CheckItems.value(key, nullptr);
			if (!item) {
				item = new QTreeWidgetItem{ };
				item->setData(0, Qt::UserRole, key);
				CheckItems.insert(key, item);
				CheckList->insertTopLevelItem(position, item);
			} else if (CheckList->topLevelItem(position) != item) {
				CheckList->takeTopLevelItem(CheckList->indexOfTopLevelItem(item));
				CheckList->insertTopLevelItem(position, item);
			}
			const auto label = text(row.Name) + QStringLiteral(" — ") + text(row.Outcome);
			if (item->text(0) != label) {
				item->setText(0, label);
			}
			item->setData(0, Qt::AccessibleTextRole, label);
			++position;
		}
		if (auto* item = CheckItems.value(selected, nullptr)) {
			CheckList->setCurrentItem(item);
		} else if (CheckList->topLevelItemCount() != 0) {
			CheckList->setCurrentItem(CheckList->topLevelItem(0));
		}
		// The list is as tall as its results, up to 180 pixels, then scrolls.
		const auto listed = CheckList->topLevelItemCount() == 0 ? 0 : CheckList->sizeHintForRow(0) * CheckList->topLevelItemCount();
		CheckList->setFixedHeight(std::clamp(listed + (2 * CheckList->frameWidth()) + 4, 48, 180));
		CheckList->verticalScrollBar()->setValue(scroll);
		CheckDetails->setVisible(ChecksDisclosure->isChecked());
		present_selected_check();
	}

	void pull_request_overview_page::present_selected_check()
	{
		const auto                                          identity = CheckList->currentItem() ? CheckList->currentItem()->data(0, Qt::UserRole).toString().toStdString() : std::string{ };
		const std::vector<application::overview_check_row>* rows     = nullptr;
		if (Projection) {
			rows = &Projection->CheckRows;
		} else if (ObservedChecks) {
			rows = &ObservedChecks->CheckRows;
		}
		const auto row = rows != nullptr ? std::ranges::find(*rows, identity, &application::overview_check_row::Identity) : std::vector<application::overview_check_row>::const_iterator{ };
		if (rows == nullptr || row == rows->end()) {
			CheckHeading->clear();
			CheckDescription->clear();
			CheckOpen->setEnabled(false);
			return;
		}
		const auto heading = text(row->Name) + QStringLiteral("\n%1: %2\n%3\n%4").arg(text(row->Kind), text(row->Outcome), row->Producer.empty() ? QStringLiteral("Producer unknown") : text(row->Producer), text(row->Requirement));
		if (CheckHeading->text() != heading) {
			CheckHeading->setText(heading);
		}
		auto description = row->Description.empty() ? QStringLiteral("No details were supplied.") : text(row->Description);
		if (!row->LinkAvailable) {
			description += row->DetailsUrl.empty() ? QStringLiteral("\nNo detail destination was supplied.") : QStringLiteral("\nThe detail destination cannot be opened safely.");
		}
		if (CheckDescription->toPlainText() != description) {
			CheckDescription->setPlainText(description);
		}
		// As tall as its explanation, up to 140 pixels, then it scrolls.
		const auto lines = static_cast<int>(std::ceil(CheckDescription->document()->size().height()));
		CheckDescription->setFixedHeight(std::clamp((std::max(lines, 2) * CheckDescription->fontMetrics().lineSpacing()) + (2 * CheckDescription->frameWidth()) + 12, 48, 140));
		CheckOpen->setEnabled(Projection.has_value() && row->LinkAvailable);
		CheckOpen->setToolTip(row->LinkAvailable ? QStringLiteral("Open the supplied destination in your browser") : QStringLiteral("No admitted detail destination"));
		if (!Projection) {
			CheckOpen->setToolTip(QStringLiteral("Available when the review has loaded"));
		}
	}

	void pull_request_overview_page::present_files_heading(const application::pull_request_overview_projection& aProjection)
	{
		const auto& since   = aProjection.SinceLastReview;
		const auto  notRead = aProjection.Progress.FilesTotal - std::min(aProjection.Progress.FilesRead, aProjection.Progress.FilesTotal);
		// RFC-BPR-0089. A filtered count says so, and why.
		auto heading = aProjection.CommitFilterComparisonFiles
			? QStringLiteral("Files: %1 of %2 in this pull request")
				  .arg(count_text(aProjection.Progress.FilesTotal), count_text(*aProjection.CommitFilterComparisonFiles))
			: QStringLiteral("Files: %1 in this pull request").arg(count_text(aProjection.Progress.FilesTotal));
		if (aProjection.CommitFilterComparisonFiles) {
			if (aProjection.FilteredByCommits) {
				heading += QStringLiteral(", filtered by commits");
			}
			if (aProjection.FileFilteredFiles != 0U) {
				heading += QStringLiteral(", %1 hidden by file filters").arg(count_text(aProjection.FileFilteredFiles));
			}
			if (aProjection.GeneratedFiles != 0U) {
				heading += QStringLiteral(", %1 generated set aside").arg(count_text(aProjection.GeneratedFiles));
			}
		}
		// [[BPR-1692]]. When linguist-generated could not be read, say why:
		// only the reviewer's own marks set files aside then.
		using attributes = application::generated_attributes_read;
		switch (aProjection.GeneratedAttributes) {
		case attributes::pending:
		case attributes::read:                 break;
		case attributes::too_many_directories: heading += QStringLiteral(", linguist-generated not read: files in more than 4,096 folders"); break;
		case attributes::too_many_files:       heading += QStringLiteral(", linguist-generated not read: more than 64 .gitattributes files"); break;
		case attributes::file_too_large:
			heading += aProjection.GeneratedAttributesExceeded && !aProjection.GeneratedAttributesExceeded->empty()
				? QStringLiteral(", linguist-generated not read: %1/.gitattributes is over 512 KiB").arg(QString::fromStdString(*aProjection.GeneratedAttributesExceeded))
				: QStringLiteral(", linguist-generated not read: .gitattributes is over 512 KiB");
			break;
		case attributes::too_many_rules:      heading += QStringLiteral(", linguist-generated not read: too many .gitattributes rules"); break;
		case attributes::objects_unavailable: heading += QStringLiteral(", linguist-generated not read: the head's .gitattributes are not available"); break;
		case attributes::failed:              heading += QStringLiteral(", linguist-generated could not be read"); break;
		}
		const auto historyReady = aProjection.Progress.NotesReadiness == application::overview_readiness::ready;
		if (historyReady && since.Known && since.FilesChanged.has_value()) {
			heading += QStringLiteral(", %1 changed since my last review").arg(count_text(*since.FilesChanged));
		} else if (historyReady && since.Known) {
			heading += QStringLiteral(", changes since my last review not yet resolved");
		}
		switch (aProjection.Progress.FilesReadiness) {
		case application::overview_readiness::ready:       heading += QStringLiteral(", %1 not read").arg(count_text(notRead)); break;
		case application::overview_readiness::loading:     heading += QStringLiteral(", read progress loading…"); break;
		case application::overview_readiness::unavailable: heading += QStringLiteral(", read progress unavailable"); break;
		}
		present_heading(*FilesHeading, heading);
	}

	void pull_request_overview_page::apply_history(const application::overview_history_update& aUpdate)
	{
		if (!Projection) {
			return;
		}
		auto& since      = Projection->SinceLastReview;
		since.Known      = aUpdate.Known;
		since.ReviewedAt = aUpdate.ReviewedAt;
		since.Uploads    = aUpdate.Uploads;
		if (Projection->Progress.NotesReadiness != application::overview_readiness::ready) {
			SinceHeading->setText(Projection->Progress.NotesReadiness == application::overview_readiness::loading
									  ? QStringLiteral("Since my last review: loading…")
									  : QStringLiteral("Since my last review: unavailable"));
			show_value(overview_value::uploads, false);
			return;
		}
		if (!since.Known) {
			SinceHeading->setText(QStringLiteral("Since my last review: none yet"));
		} else if (since.ReviewedAt) {
			const auto when = QDateTime::fromMSecsSinceEpoch(since.ReviewedAt->value(), QTimeZone::UTC).toLocalTime();
			SinceHeading->setText(QStringLiteral("Since my last review on %1").arg(when.toString(QStringLiteral("MMM d"))));
		} else {
			SinceHeading->setText(QStringLiteral("Since my last review"));
		}
		set_value(overview_value::uploads, count_text(since.Uploads.value_or(0U)), since.Uploads.value_or(0U) != 0U);
		show_value(overview_value::uploads, since.Known && since.Uploads.value_or(0U) != 0U);
	}

	void pull_request_overview_page::apply_receipts(const application::overview_receipt_update& aUpdate)
	{
		if (!Projection.has_value()) {
			return;
		}
		// The page keeps the progress and the action it dispatches; its rows
		// live in the model, which updates only the changed files.
		Projection->Progress  = aUpdate.Progress;
		Projection->Entry     = aUpdate.Entry;
		Projection->EntryPath = aUpdate.EntryPath;
		Model->update_rows(aUpdate.Rows);
		FileColumns->apply();
		present_files_heading(*Projection);
		present_my_review();
	}

	void pull_request_overview_page::clear()
	{
		// [[BPR-1601]]. The next pull request waits for its own ledger count.
		NewConversation.reset();
		set_description_expanded(false);
		Projection.reset();
		ObservedChecks.reset();
		ObservedPeople.reset();
		ObservedMerge.reset();
		// [[BPR-1852]]. Nothing presented is a draft, and no change is under way.
		ShownDraft        = false;
		ShownOpen         = false;
		ShownDraftPending = false;
		ReadyBusy         = false;
		ConvertBusy       = false;
		ReadyStatus->clear();
		ConvertStatus->clear();
		present_draft_controls();
		MergeState->hide();
		MergeConditions->hide();
		RequestsNote->hide();
		while (Requests->count() != 0) {
			auto* const item = Requests->takeAt(0);
			delete item->widget();
			delete item;
		}
		RequirementDisclosure->hide();
		RequirementDetails->hide();
		hide_status();
		CheckItems.clear();
		CheckList->clear();
		CheckDescription->clear();
		CheckOpen->setEnabled(false);
		Cached = false;
		CachedBase.clear();
		CachedHead.clear();
		CachedSource->clear();
		CachedSource->hide();
		CachedSourceBack->hide();
		CachedDiscussion->clear();
		CachedDiscussion->hide();
		CachedThreads->clear();
		CachedThreads->hide();
		CachedThreadsHeading->hide();
		Title->setText(QStringLiteral("No pull request"));
		Summary->clear();
		Summary->setToolTip({ });
		Summary->setAccessibleDescription({ });
		Body->reset_reader();
		Body->setVisible(false);
		SummaryPane->setVisible(false);
		EmptyBody->setVisible(false);
		SummaryPane->setVisible(false);
		FullView->setVisible(false);
		// [[BPR-1774]]. An invalidated authority withdraws Edit and ends an
		// edit; nothing is sent.
		DescriptionEditable = false;
		if (DescriptionEditing) {
			close_description_editor();
		}
		present_description_controls();
		present_heading(*FilesHeading, QStringLiteral("Files"));
		ModeChosen = false;
		Mode       = DefaultMode;
		Sort.reset();
		AskedFeedback.reset();
		Model->apply_feedback(std::nullopt);
		Model->apply({ });
		ObservedFiles = false;
		PendingOpen.clear();
		update_folder_controls();
		present_feedback_controls();
		rebuild_participants({ });
		SinceHeading->setText(QStringLiteral("Since my last review"));
		for (auto* const value : Values) {
			if (value != nullptr) {
				value->clear();
				value->setForegroundRole(QPalette::PlaceholderText);
			}
		}
		// An observation clears the page and goes on loading: its waits keep
		// their clocks rather than starting again.
		if (!Observing) {
			Loading       = false;
			Observed      = false;
			ReportedFiles = 0U;
			apply_pending();
		}
	}

	void pull_request_overview_page::present_loading()
	{
		Title->setText(QStringLiteral("Loading pull request…"));
		Summary->setText(QStringLiteral("Details, files and threads are loading in the background."));
		present_heading(*FilesHeading, QStringLiteral("Files — loading…"));
		if (auto* none = findChild<QLabel*>(QStringLiteral("overviewNoParticipants"))) {
			none->setText(QStringLiteral("Loading participants…"));
		}
		// [[BPR-1576]]. The description's place is held while it is unknown,
		// so the placeholders stand where it will.
		SummaryPane->setVisible(true);
		SummarySplit->queue_layout();
		Loading  = true;
		Observed = false;
		apply_pending();
	}

	auto pull_request_overview_page::pending_veils() const noexcept -> overview_pending_veils
	{
		return Veils;
	}

	void pull_request_overview_page::set_read_waiting(const QString& aWaiting)
	{
		if (ReadWaiting == aWaiting) {
			return;
		}
		ReadWaiting = aWaiting;
		apply_pending();
	}

	void pull_request_overview_page::set_pending_appearance(const bool aFullMotion, const bool aHighContrast, const QColor& aFailureInk)
	{
		for (auto* const veil : { Veils.Header, Veils.Description, Veils.Files, Veils.Side }) {
			if (veil != nullptr) {
				veil->set_full_motion(aFullMotion);
				veil->set_high_contrast(aHighContrast);
				veil->set_failure_ink(aFailureInk);
			}
		}
	}

	void pull_request_overview_page::apply_pending()
	{
		if (Veils.Header == nullptr) {
			return;
		}
		const auto waiting = ReadWaiting.isEmpty() ? QString{ } : QStringLiteral("Still waiting for GitHub: %1.").arg(ReadWaiting);
		const auto section = [&waiting](pending_veil& aVeil, const bool aPending, QString aLoading, QString aDone = { }) {
			if (!aPending) {
				aVeil.present_ready();
				return;
			}
			aVeil.present(core::pending_activity::loading, false, false, { .Loading = std::move(aLoading), .Waiting = waiting, .Done = std::move(aDone) });
		};
		// Provisional sections become readable independently; anything not
		// received yet keeps its own loading presentation.
		section(*Veils.Header, Loading && !Observed, QStringLiteral("Opening the pull request…"));
		section(*Veils.Description, Loading && !Observed, QStringLiteral("Loading the description…"));
		if (Loading && has_observed_status()) {
			auto remaining = QStringList{ };
			if (!ObservedPeople) {
				remaining.push_back(QStringLiteral("reviewers"));
			}
			if (!ObservedChecks) {
				remaining.push_back(QStringLiteral("checks"));
			}
			remaining.push_back(QStringLiteral("merge rules"));
			Veils.Side->present(core::pending_activity::loading, true, true,
								{ .Waiting = waiting, .Coming = QStringLiteral("Loading %1 and review progress…").arg(remaining.join(QStringLiteral(", "))) });
		} else {
			section(*Veils.Side, Loading, QStringLiteral("Loading reviewers, checks and progress…"));
		}
		// As many placeholder rows as GitHub reported files, once it has.
		Veils.Files->set_placeholder_count(Observed ? static_cast<int>(std::min<std::size_t>(std::max<std::size_t>(ReportedFiles, 1U), 40U)) : 6);
		if (Loading && ObservedFiles) {
			// [[BPR-1842]]. The rows are there to read; what is still coming is
			// said on the plate, not dimmed over them.
			const auto coming = PendingOpen.isEmpty()
				? QStringLiteral("Loading comments, reviews and checks…")
				: QStringLiteral("Opening %1 when the review has loaded…").arg(PendingOpen.section(QLatin1Char('/'), -1));
			Veils.Files->present(core::pending_activity::loading, true, true,
								 { .Waiting = waiting, .Coming = coming, .Done = QStringLiteral("Overview ready: files, reviewers and checks are shown.") });
			return;
		}
		if (Loading) {
			section(*Veils.Files, true,
					Observed ? QStringLiteral("Loading the file list: %1 reported…").arg(ReportedFiles == 1U ? QStringLiteral("1 file") : QStringLiteral("%1 files").arg(count_text(ReportedFiles)))
							 : QStringLiteral("Loading the file list…"),
					QStringLiteral("Overview ready: files, reviewers and checks are shown."));
			return;
		}
		// The counts by file are the files' last part to arrive: said, not
		// dimmed, while the rows themselves are already there to read.
		const auto& feedback = Model->feedback();
		const auto  counting = Projection.has_value() && !Cached && FeedbackDispatch && AskedFeedback.has_value()
			&& (!feedback.has_value() || feedback->Request != *AskedFeedback);
		if (counting) {
			Veils.Files->present(core::pending_activity::loading, true, true, { .Coming = QStringLiteral("Counting comments by file…") });
		} else {
			Veils.Files->present_ready();
		}
	}

	void pull_request_overview_page::present_observation(const application::github_loading_overview_projection& aObservation)
	{
		Observing = true;
		clear();
		Observing     = false;
		Loading       = true;
		Observed      = true;
		ReportedFiles = aObservation.ReportedFiles;
		restore_status_sections(QStringLiteral("%1/%2#%3").arg(text(aObservation.Owner), text(aObservation.RepositoryName), QString::number(aObservation.Number)));
		Title->setText(text(aObservation.Title));
		Summary->setToolTip({ });
		Summary->setAccessibleDescription({ });
		auto state = aObservation.Draft ? QStringLiteral("Draft") : QStringLiteral("Open");
		if (aObservation.State == application::github_pull_request_state_projection::merged) {
			state = QStringLiteral("Merged");
		} else if (aObservation.State == application::github_pull_request_state_projection::closed) {
			state = QStringLiteral("Closed without merge");
		}
		// [[BPR-1852]]. The chip and the banner as soon as GitHub names the
		// pull request; its author's action waits for the review to load.
		ShownDraft = aObservation.Draft;
		ShownOpen  = aObservation.State == application::github_pull_request_state_projection::open;
		present_draft_controls();
		const auto commits = aObservation.ReportedCommits == 1U ? QStringLiteral("1 commit")
																: QStringLiteral("%1 commits").arg(count_text(aObservation.ReportedCommits));
		Summary->setText(QStringLiteral("%1 · %2 · %3 → %4 · %5. Provisional metadata; review data is still loading.")
							 .arg(aObservation.AuthorLogin.empty() ? QStringLiteral("Unknown author") : text(aObservation.AuthorLogin),
								  commits, text(aObservation.HeadRef), text(aObservation.BaseRef), state));
		Body->present_body(aObservation.Body);
		Body->setVisible(!aObservation.Body.empty());
		SummaryPane->setVisible(!aObservation.Body.empty());
		FullView->setVisible(!aObservation.Body.empty());
		present_description_controls();
		SummarySplit->queue_layout();
		present_heading(*FilesHeading, QStringLiteral("Files — %1 reported; catalogue loading…").arg(count_text(aObservation.ReportedFiles)));
		SinceHeading->setText(QStringLiteral("Review state — loading…"));
		if (auto* none = findChild<QLabel*>(QStringLiteral("overviewNoParticipants"))) {
			none->setText(QStringLiteral("Loading participants…"));
		}
		apply_pending();
	}

	void pull_request_overview_page::present_observed_checks(const application::overview_checks_projection& aChecks)
	{
		if (!Loading || !Observed || Cached || Projection) {
			return;
		}
		ObservedChecks = aChecks;
		present_checks(false);
		apply_pending();
	}

	void pull_request_overview_page::present_observed_people(const application::overview_people_projection& aPeople)
	{
		if (!Loading || !Observed || Cached || Projection) {
			return;
		}
		ObservedPeople = aPeople;
		rebuild_participants(aPeople.Participants);
		present_request_rows();
		present_people();
		Sections[static_cast<std::size_t>(overview_status_section::people)].Frame->show();
		apply_pending();
	}

	void pull_request_overview_page::present_observed_merge(const application::overview_merge_projection& aMerge)
	{
		if (!Loading || !Observed || Cached || Projection) {
			return;
		}
		ObservedMerge = aMerge;
		present_merge_details(aMerge.State, aMerge.Tone, { }, aMerge.Details);
		auto& section = Sections[static_cast<std::size_t>(overview_status_section::attention)];
		section.Frame->show();
		section.Header->setText(QStringLiteral("Merge"));
		section.Header->set_count(text(aMerge.State));
		section.Header->setAccessibleName(QStringLiteral("Merge: %1; rules loading").arg(text(aMerge.State)));
		apply_pending();
	}

	auto pull_request_overview_page::has_observed_status() const noexcept -> bool
	{
		return ObservedChecks.has_value() || ObservedPeople.has_value() || ObservedMerge.has_value();
	}

	void pull_request_overview_page::present_observed_files(const std::vector<core::cached_review_file>& aFiles)
	{
		if (!Loading || !Observed || Cached || Projection) {
			return;
		}
		ObservedFiles = true;
		present_heading(*FilesHeading,
						QStringLiteral("Files — %1; comment counts and progress loading…").arg(aFiles.size() == 1U ? QStringLiteral("1 file") : QStringLiteral("%1 files").arg(count_text(aFiles.size()))),
						QStringLiteral("Change kinds and line counts as GitHub reported them. A file you open now opens when the review has loaded."));
		Model->apply(cached_file_rows(aFiles));
		update_folder_controls();
		for (int column = overview_file_model::changed_column; column < overview_file_model::column_count; ++column) {
			Files->setColumnHidden(column, true);
		}
		FileColumns->apply();
		Files->setAccessibleDescription(QStringLiteral("Changed files as GitHub reported them while the review loads; a file activated now opens once it has loaded; %1.").arg(files_ends_help()));
		apply_pending();
	}

	auto pull_request_overview_page::pending_open() const -> const QString&
	{
		return PendingOpen;
	}

	void pull_request_overview_page::present_cached(const application::github_cached_overview_projection& aOverview)
	{
		present_observation(aOverview.Overview);
		// A cached overview is all there is: nothing more is on its way.
		Loading    = false;
		Cached     = true;
		CachedBase = aOverview.BaseOid;
		CachedHead = aOverview.HeadOid;
		Summary->setText(QStringLiteral("Cached overview · freshness unknown. Open this pull request again to retry."));
		Summary->setToolTip(QStringLiteral("Previously accepted base %1\nPreviously accepted head %2")
								.arg(text(aOverview.BaseOid), text(aOverview.HeadOid)));
		Summary->setAccessibleDescription(Summary->toolTip());
		present_heading(*FilesHeading,
						aOverview.FileCatalogue
							? QStringLiteral("Cached file catalogue — %1 files observed; open a file to read retained source").arg(count_text(aOverview.FileCatalogue->size()))
							: QStringLiteral("File catalogue unavailable offline; source unavailable offline"),
						QStringLiteral("Historical GitHub change kinds and line counts, reported with the retained base/head. These are not a recomputed comparison of the base tip and head."));
		Model->apply(cached_file_rows(aOverview.FileCatalogue));
		update_folder_controls();
		for (int column = overview_file_model::changed_column; column < overview_file_model::column_count; ++column) {
			Files->setColumnHidden(column, true);
		}
		FileColumns->apply();
		Files->setAccessibleDescription(QStringLiteral("Historical file catalogue; freshness unknown. Open a file to read its exact retained base and head when available; %1. Current thread counts and review progress unavailable.").arg(files_ends_help()));
		CachedThreadsHeading->setText(aOverview.ReviewThreads
										  ? QStringLiteral("Cached review threads — %1 observed · freshness unknown").arg(count_text(aOverview.ReviewThreads->size()))
										  : QStringLiteral("Review threads unavailable offline"));
		CachedThreadsHeading->show();
		CachedDiscussion->setPlainText(cached_discussion_text(aOverview.GeneralComments, QStringLiteral("General comments")) + cached_discussion_text(aOverview.ReviewSummaries, QStringLiteral("Review summaries")));
		CachedDiscussion->show();
		CachedThreads->setPlainText(aOverview.ReviewThreads ? cached_thread_text(*aOverview.ReviewThreads) : QString{ });
		CachedThreads->setVisible(aOverview.ReviewThreads && !aOverview.ReviewThreads->empty());
		SinceHeading->setText(QStringLiteral("Review state unavailable offline"));
		if (auto* none = findChild<QLabel*>(QStringLiteral("overviewNoParticipants"))) {
			none->setWordWrap(true);
			none->setText(QStringLiteral("Participants unavailable offline"));
		}
		// [[BPR-1589]]. Offline the Status drawer has nothing current to say,
		// and says that once.
		StatusNote->setText(QStringLiteral("My role, checks and people are unavailable offline. Open this pull request again to retry."));
		StatusNote->show();
		apply_pending();
	}
}
