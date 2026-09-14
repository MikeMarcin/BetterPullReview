#include "bpr/qt/pull_request_overview_page.hpp"

#include "bpr/application/pull_request_overview.hpp"

#include <QApplication>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTreeView>

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
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

	[[nodiscard]] auto row(
		const std::string&                              aPath,
		const application::overview_file_operation      aOperation,
		const application::overview_change_since_review aChanged,
		const std::optional<core::file_read_state>      aRead,
		const std::size_t                               aThreads    = 0U,
		const std::size_t                               aUnresolved = 0U,
		const std::optional<std::string>&               aNote       = std::nullopt)
		-> application::overview_file_row
	{
		const auto slash = aPath.rfind('/');
		return application::overview_file_row{
			.Path                  = aPath,
			.Folder                = slash == std::string::npos ? std::string{ } : aPath.substr(0U, slash),
			.Name                  = slash == std::string::npos ? aPath : aPath.substr(slash + 1U),
			.Operation             = aOperation,
			.Additions             = 12U,
			.Deletions             = aOperation == application::overview_file_operation::added ? 0U : 3U,
			.Changed               = aChanged,
			.Read                  = aRead,
			.ThreadCount           = aThreads,
			.UnresolvedThreadCount = aUnresolved,
			.Note                  = aNote,
		};
	}

	[[nodiscard]] auto projection() -> application::pull_request_overview_projection
	{
		auto projection                = application::pull_request_overview_projection{ };
		projection.Owner               = "EpicGames";
		projection.RepositoryName      = "UnrealEngine";
		projection.Number              = core::pull_request_number{ 14366U };
		projection.Title               = "Hair strand lighting fixes";
		projection.Body                = "Fixes three crashes.";
		projection.AuthorLogin         = "jiayin";
		projection.BaseRef             = "release";
		projection.HeadRef             = "hair-strand";
		projection.CommitCount         = 4U;
		projection.ReturnedCommitCount = 4U;
		projection.Files               = {
			row("Engine/Renderer/HairStrandsLighting.cpp", application::overview_file_operation::modified,
				application::overview_change_since_review::unchanged, core::file_read_state::seen, 3U, 1U),
			row("Engine/Renderer/HairStrandsVoxelization.cpp", application::overview_file_operation::modified,
				application::overview_change_since_review::changed, core::file_read_state::read, 2U, 0U,
				std::string{ "Re-read the atomic move" }),
			row("Engine/Shaders/HairStrandsDeepShadow.usf", application::overview_file_operation::added,
				application::overview_change_since_review::added, std::nullopt),
		};
		projection.Participants = {
			{ .Login = "jiayin", .Role = application::overview_participant_role::author, .Signoff = application::overview_signoff::uploaded },
			{ .Login = "mike", .Role = application::overview_participant_role::reviewer, .Signoff = application::overview_signoff::changes_requested, .Viewer = true },
			{ .Login = "rcaloca", .Role = application::overview_participant_role::reviewer, .Signoff = application::overview_signoff::approved },
		};
		projection.SinceLastReview = application::overview_since_last_review{
			.Known               = true,
			.ReviewedAt          = core::observed_utc_milliseconds{ 1'770'000'000'000 },
			.Uploads             = 2U,
			.FilesChanged        = 2U,
			.RepliesToMe         = 1U,
			.MyUnresolvedThreads = 2U,
		};
		projection.Checks    = { .Passed = 3U, .Skipped = 1U };
		projection.Progress  = { .FilesRead = 1U, .FilesTotal = 3U, .PrivateNotes = 1U, .DraftedComments = 1U };
		projection.Entry     = application::overview_entry_kind::continue_review;
		projection.EntryPath = "Engine/Renderer/HairStrandsVoxelization.cpp";
		return projection;
	}

	auto test_page_presents_the_projection_and_reports_its_gestures() -> void
	{
		qt::pull_request_overview_page page;
		page.resize(1'100, 700);
		page.show();
		QApplication::processEvents();
		expect(!page.presented() && !page.entry_button()->isEnabled()
				   && page.title_label()->text() == QStringLiteral("No pull request"),
			   "before a projection the page names nothing and offers no entry");

		auto entered   = std::vector<QString>{ };
		auto activated = std::vector<QString>{ };
		page.set_entry_dispatch([&entered](const QString& aPath) { entered.push_back(aPath); });
		page.set_file_dispatch([&activated](const QString& aPath) { activated.push_back(aPath); });
		page.present(projection());
		QApplication::processEvents();

		expect(page.presented()
				   && page.title_label()->text() == QStringLiteral("Hair strand lighting fixes")
				   && page.summary_label()->text()
					   == QStringLiteral("jiayin wants to merge 4 commits into release from hair-strand.")
				   && page.body_view()->toPlainText() == QStringLiteral("Fixes three crashes.")
				   && page.body_view()->isVisible(),
			   "the header carries the title, the author and branches, and the description");
		expect(page.files_heading()->text()
				   == QStringLiteral("Files: 3 in this pull request, 2 changed since my last review, 2 not read"),
			   "the files heading counts the files, the changed and the not read");

		auto* const model = page.file_model();
		expect(model->folder_count() == 2U && model->file_count() == 3U
				   && model->rowCount() == 2 && model->rowCount(model->index(0, 0)) == 2
				   && model->rowCount(model->index(1, 0)) == 1
				   && model->index(0, qt::overview_file_model::file_column).data().toString()
					   == QStringLiteral("Engine/Renderer")
				   && model->index(1, qt::overview_file_model::file_column).data().toString()
					   == QStringLiteral("Engine/Shaders"),
			   "files are grouped under their folders in projection order");
		const auto voxel = model->index_for_path(QStringLiteral("Engine/Renderer/HairStrandsVoxelization.cpp"));
		expect(voxel.isValid() && voxel.parent().row() == 0 && voxel.row() == 1
				   && voxel.siblingAtColumn(qt::overview_file_model::operation_column).data().toString() == QStringLiteral("M")
				   && voxel.data().toString() == QStringLiteral("HairStrandsVoxelization.cpp")
				   && voxel.siblingAtColumn(qt::overview_file_model::added_column).data().toString() == QStringLiteral("+12")
				   && voxel.siblingAtColumn(qt::overview_file_model::removed_column).data().toString() == QStringLiteral("−3")
				   && voxel.siblingAtColumn(qt::overview_file_model::changed_column).data().toString() == QStringLiteral("changed")
				   && voxel.siblingAtColumn(qt::overview_file_model::read_column).data(Qt::AccessibleTextRole).toString() == QStringLiteral("read")
				   && voxel.siblingAtColumn(qt::overview_file_model::comments_column).data().toString() == QStringLiteral("2")
				   && voxel.siblingAtColumn(qt::overview_file_model::note_column).data().toString() == QStringLiteral("Re-read the atomic move")
				   && voxel.data(qt::overview_file_model::path_role).toString() == QStringLiteral("Engine/Renderer/HairStrandsVoxelization.cpp"),
			   "a file row answers every column of the accepted table");
		const auto lighting = model->index_for_path(QStringLiteral("Engine/Renderer/HairStrandsLighting.cpp"));
		const auto shadow   = model->index_for_path(QStringLiteral("Engine/Shaders/HairStrandsDeepShadow.usf"));
		expect(lighting.siblingAtColumn(qt::overview_file_model::comments_column).data().toString() == QStringLiteral("1 / 3")
				   && lighting.siblingAtColumn(qt::overview_file_model::changed_column).data().toString().isEmpty()
				   && shadow.siblingAtColumn(qt::overview_file_model::operation_column).data().toString() == QStringLiteral("A")
				   && shadow.siblingAtColumn(qt::overview_file_model::changed_column).data().toString() == QStringLiteral("new")
				   && shadow.siblingAtColumn(qt::overview_file_model::removed_column).data().toString().isEmpty()
				   && shadow.siblingAtColumn(qt::overview_file_model::read_column).data(Qt::AccessibleTextRole).toString() == QStringLiteral("unread"),
			   "unresolved counts lead the total, a new file says new, and no receipt reads unread");

		const auto participants = page.participant_rows();
		expect(participants.size() == 3U && participants[0]->login() == QStringLiteral("jiayin")
				   && participants[0]->standing_text() == QStringLiteral("uploaded")
				   && participants[1]->login() == QStringLiteral("mike")
				   && participants[1]->standing_text() == QStringLiteral("changes requested")
				   && participants[2]->standing_text() == QStringLiteral("approved")
				   && participants[1]->isVisible(),
			   "participants are rows with their login and standing");

		expect(page.since_heading()->text().startsWith(QStringLiteral("Since my last review on "))
				   && page.value_label(qt::overview_value::uploads)->text() == QStringLiteral("2")
				   && page.value_label(qt::overview_value::files_changed)->text() == QStringLiteral("2")
				   && page.value_label(qt::overview_value::new_comments)->text() == QStringLiteral("not available")
				   && page.value_label(qt::overview_value::replies_to_me)->text() == QStringLiteral("1")
				   && page.value_label(qt::overview_value::my_unresolved_threads)->text() == QStringLiteral("2")
				   && page.value_label(qt::overview_value::checks)->text() == QStringLiteral("3 passed, 1 skipped")
				   && page.value_label(qt::overview_value::files_read)->text() == QStringLiteral("1 of 3")
				   && page.value_label(qt::overview_value::private_notes)->text() == QStringLiteral("1")
				   && page.value_label(qt::overview_value::drafted_comments)->text() == QStringLiteral("1 unpublished"),
			   "the side column states what happened since my last review and my progress, "
			   "saying not available where the host cannot answer");
		expect(page.entry_button()->isEnabled()
				   && page.entry_button()->text() == QStringLiteral("Continue review")
				   && page.entry_button()->toolTip().contains(QStringLiteral("HairStrandsVoxelization.cpp")),
			   "the one action continues the review at the named file");

		page.entry_button()->click();
		QApplication::processEvents();
		expect(entered.size() == 1U
				   && entered.front() == QStringLiteral("Engine/Renderer/HairStrandsVoxelization.cpp"),
			   "Continue review reports the entry path");
		page.files_view()->activated(lighting);
		QApplication::processEvents();
		expect(activated.size() == 1U
				   && activated.front() == QStringLiteral("Engine/Renderer/HairStrandsLighting.cpp"),
			   "activating a file row reports its path");
		page.files_view()->activated(model->index(0, 0));
		QApplication::processEvents();
		expect(activated.size() == 1U, "activating a folder row reports nothing");

		auto unknown                     = projection();
		unknown.SinceLastReview          = { .Known = true };
		unknown.Files.front().Changed    = application::overview_change_since_review::unknown;
		unknown.Progress.DraftedComments = std::nullopt;
		unknown.Entry                    = application::overview_entry_kind::start_review;
		page.present(unknown);
		QApplication::processEvents();
		expect(page.files_heading()->text().contains(QStringLiteral("not yet resolved"))
				   && page.value_label(qt::overview_value::uploads)->text() == QStringLiteral("not retained")
				   && page.value_label(qt::overview_value::files_changed)->text() == QStringLiteral("not yet resolved")
				   && page.value_label(qt::overview_value::drafted_comments)->text() == QStringLiteral("not available")
				   && page.file_model()->index_for_path(QStringLiteral("Engine/Renderer/HairStrandsLighting.cpp")).siblingAtColumn(qt::overview_file_model::changed_column).data().toString()
					   == QStringLiteral("?")
				   && page.entry_button()->text() == QStringLiteral("Start review"),
			   "what the host cannot answer reads as not known, never as none");

		auto partial                = projection();
		partial.CommitCount         = 9'311U;
		partial.ReturnedCommitCount = 250U;
		page.present(partial);
		expect(page.summary_label()->text().contains(
				   QStringLiteral("9311 commits (250 returned by GitHub)")),
			   "large totals do not label the returned commit slice as complete");

		page.clear();
		QApplication::processEvents();
		expect(!page.presented() && page.file_model()->file_count() == 0U
				   && !page.entry_button()->isEnabled() && page.participant_rows().empty(),
			   "clearing empties the page");
		page.hide();
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_page_presents_the_projection_and_reports_its_gestures();
	std::cout << "pull-request-overview-page-tests:pass\n";
	return EXIT_SUCCESS;
}
