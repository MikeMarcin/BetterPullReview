#include "bpr/application/pull_request_overview.hpp"

#include "bpr/core/diff_model.hpp"
#include "bpr/core/epoch_model.hpp"

#include <array>
#include <cstdint>
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

	template <typename Identity>
	[[nodiscard]] auto identity(const std::uint8_t aSeed) -> Identity
	{
		std::array<std::uint8_t, 32> bytes{ };
		bytes.front() = aSeed;
		bytes.back()  = static_cast<std::uint8_t>(aSeed ^ 0xa5U);
		return Identity{ bytes };
	}

	[[nodiscard]] auto commit(const char aDigit) -> core::git_commit_oid
	{
		const auto parsed = core::parse_git_commit_oid(std::string(40U, aDigit));
		expect(parsed.has_value(), "fixture commit OID parses");
		return *parsed;
	}

	[[nodiscard]] auto actor(const std::string& aLogin, const std::uint64_t aId)
		-> application::github_actor_record
	{
		return application::github_actor_record{
			.Actor = application::github_actor_id{ std::to_string(aId) },
			.Login = aLogin,
		};
	}

	[[nodiscard]] auto comment(
		const std::string&                                   aLogin,
		const std::uint64_t                                  aId,
		const std::string&                                   aPath,
		const std::optional<application::github_comment_id>& aReplyTo = std::nullopt)
		-> application::github_review_comment_record
	{
		return application::github_review_comment_record{
			.Comment = application::github_comment_id{ std::to_string(aId) },
			.ReplyTo = aReplyTo,
			.Author  = actor(aLogin, aId),
			.Body    = "body",
			.Path    = aPath,
			.Line    = 4U,
		};
	}

	[[nodiscard]] auto update(const std::int64_t aOrdinal, const char aHead)
		-> application::epoch_row_projection
	{
		return application::epoch_row_projection{
			.Id = core::review_epoch_id{
				.Authority   = identity<core::pull_request_authority_scope_id>(1U),
				.Incarnation = identity<core::epoch_catalog_incarnation_id>(2U),
				.Ordinal     = core::epoch_ordinal{ aOrdinal },
			},
			.Base       = commit('a'),
			.Head       = commit(aHead),
			.ObservedAt = core::observed_utc_milliseconds{ 1'000 * aOrdinal },
		};
	}

	[[nodiscard]] auto snapshot() -> application::github_pull_request_snapshot
	{
		auto snapshot       = application::github_pull_request_snapshot{ };
		snapshot.Viewer     = actor("mike", 1U);
		snapshot.Repository = application::github_repository_record{
			.Repository = core::repository_id{ 7U },
			.Owner      = "EpicGames",
			.Name       = "UnrealEngine",
		};
		snapshot.PullRequest = application::github_pull_request_record{
			.PullRequest = core::pull_request_id{ 9U },
			.Repository  = core::repository_id{ 7U },
			.Number      = core::pull_request_number{ 14366U },
			.Title       = "Hair strand lighting fixes",
			.Body        = "Fixes three crashes.",
			.Author      = actor("jiayin", 2U),
			.BaseRef     = "release",
			.HeadRef     = "hair-strand",
		};
		snapshot.Files = {
			application::github_file_record{
				.Path      = "Engine/Renderer/HairStrandsLighting.cpp",
				.Change    = application::github_file_change::modified,
				.Additions = 212U,
				.Deletions = 48U,
			},
			application::github_file_record{
				.Path      = "Engine/Shaders/HairStrandsDeepShadow.usf",
				.Change    = application::github_file_change::added,
				.Additions = 19U,
			},
			application::github_file_record{
				.Path      = "Engine/Renderer/HairStrandsVoxelization.cpp",
				.Change    = application::github_file_change::modified,
				.Additions = 88U,
				.Deletions = 20U,
			},
			application::github_file_record{
				.Path      = "README.md",
				.Change    = application::github_file_change::removed,
				.Deletions = 3U,
			},
		};
		snapshot.Commits = { { }, { }, { } };
		snapshot.Reviews = {
			application::github_review_record{
				.Author = actor("mike", 1U),
				.State  = application::github_review_state::changes_requested,
			},
			application::github_review_record{
				.Author = actor("rcaloca", 3U),
				.State  = application::github_review_state::commented,
			},
			application::github_review_record{
				.Author = actor("rcaloca", 3U),
				.State  = application::github_review_state::approved,
			},
			// The author's own review comment does not make them a reviewer.
			application::github_review_record{
				.Author = actor("jiayin", 2U),
				.State  = application::github_review_state::commented,
			},
		};
		snapshot.ReviewThreadsAvailability
			= application::github_collection_availability::available;
		snapshot.ReviewThreads = {
			application::github_review_thread_record{
				.Path     = "Engine/Renderer/HairStrandsVoxelization.cpp",
				.Resolved = false,
				.Comments = {
					comment("mike", 10U, "Engine/Renderer/HairStrandsVoxelization.cpp"),
					comment("jiayin", 11U, "Engine/Renderer/HairStrandsVoxelization.cpp",
							application::github_comment_id{ "10" }),
					comment("tbutler", 12U, "Engine/Renderer/HairStrandsVoxelization.cpp",
							application::github_comment_id{ "10" }),
				},
			},
			application::github_review_thread_record{
				.Path     = "Engine/Renderer/HairStrandsVoxelization.cpp",
				.Resolved = true,
				.Comments = { comment("rcaloca", 13U, "Engine/Renderer/HairStrandsVoxelization.cpp") },
			},
			application::github_review_thread_record{
				.Path     = "Engine/Renderer/HairStrandsLighting.cpp",
				.Resolved = false,
				.Comments = { comment("jiayin", 14U, "Engine/Renderer/HairStrandsLighting.cpp") },
			},
		};
		snapshot.Checks = {
			application::github_check_record{ .Name = "build", .Status = application::github_check_status::completed, .Conclusion = application::github_check_conclusion::success },
			application::github_check_record{ .Name = "test", .Status = application::github_check_status::completed, .Conclusion = application::github_check_conclusion::success },
			application::github_check_record{ .Name = "lint", .Status = application::github_check_status::completed, .Conclusion = application::github_check_conclusion::skipped },
			application::github_check_record{ .Name = "deploy", .Status = application::github_check_status::in_progress },
		};
		return snapshot;
	}

	[[nodiscard]] auto receipt(const std::string& aPath, const core::file_read_state aRead)
		-> core::file_receipt
	{
		return core::file_receipt{
			.File     = core::file_identity_of(aPath),
			.Material = identity<core::file_material_id>(9U),
			.Read     = aRead,
		};
	}

	[[nodiscard]] auto row_named(
		const application::pull_request_overview_projection& aProjection,
		const std::string_view                               aPath)
		-> const application::overview_file_row&
	{
		for (const auto& row : aProjection.Files) {
			if (row.Path == aPath) {
				return row;
			}
		}
		expect(false, "row exists");
		std::abort();
	}

	auto test_first_visit_starts_at_the_first_file() -> void
	{
		const auto projection = application::project_pull_request_overview(
			snapshot(), application::overview_context{ });
		expect(projection.Title == "Hair strand lighting fixes"
				   && projection.AuthorLogin == "jiayin" && projection.BaseRef == "release"
				   && projection.HeadRef == "hair-strand" && projection.CommitCount == 3U
				   && projection.Number == core::pull_request_number{ 14366U },
			   "the header carries title, author, branches and the commit count");
		expect(projection.Files.size() == 4U
				   && projection.Files[0].Folder == "" && projection.Files[0].Name == "README.md"
				   && projection.Files[1].Folder == "Engine/Renderer"
				   && projection.Files[1].Name == "HairStrandsLighting.cpp"
				   && projection.Files[2].Name == "HairStrandsVoxelization.cpp"
				   && projection.Files[3].Folder == "Engine/Shaders",
			   "rows come in folder order, ready to group");
		const auto& voxel = row_named(projection, "Engine/Renderer/HairStrandsVoxelization.cpp");
		expect(voxel.Operation == application::overview_file_operation::modified
				   && voxel.Additions == 88U && voxel.Deletions == 20U
				   && voxel.ThreadCount == 2U && voxel.UnresolvedThreadCount == 1U
				   && !voxel.UnreadCount.has_value() && !voxel.Read.has_value()
				   && voxel.Changed == application::overview_change_since_review::unchanged,
			   "a row carries operation, counts, threads, and no read state without a receipt");
		expect(row_named(projection, "README.md").Operation
				   == application::overview_file_operation::removed,
			   "a removed file says so");
		expect(projection.Participants.size() == 4U
				   && projection.Participants[0].Login == "jiayin"
				   && projection.Participants[0].Role == application::overview_participant_role::author
				   && projection.Participants[0].Signoff == application::overview_signoff::uploaded
				   && projection.Participants[1].Login == "mike"
				   && projection.Participants[1].Viewer
				   && projection.Participants[1].Role == application::overview_participant_role::reviewer
				   && projection.Participants[1].Signoff == application::overview_signoff::changes_requested
				   && projection.Participants[2].Login == "tbutler"
				   && projection.Participants[2].Role == application::overview_participant_role::observer
				   && projection.Participants[2].Signoff == application::overview_signoff::commented
				   && projection.Participants[3].Login == "rcaloca"
				   && projection.Participants[3].Role == application::overview_participant_role::reviewer
				   && projection.Participants[3].Signoff == application::overview_signoff::approved,
			   "participants: the author first, reviewers by their latest review, commenters as observers");
		expect(projection.Checks.Passed == 2U && projection.Checks.Skipped == 1U
				   && projection.Checks.Pending == 1U && projection.Checks.Failed == 0U,
			   "checks are summed by outcome");
		expect(!projection.SinceLastReview.Known
				   && !projection.SinceLastReview.Uploads.has_value()
				   && !projection.SinceLastReview.FilesChanged.has_value()
				   && !projection.SinceLastReview.NewComments.has_value()
				   && projection.SinceLastReview.RepliesToMe == 2U
				   && projection.SinceLastReview.MyUnresolvedThreads == 1U,
			   "without a last review the since-summary is unknown, while my threads count");
		expect(projection.Progress.FilesRead == 0U && projection.Progress.FilesTotal == 4U
				   && projection.Progress.PrivateNotes == 0U
				   && !projection.Progress.DraftedComments.has_value(),
			   "progress counts the files");
		expect(projection.Entry == application::overview_entry_kind::start_review
				   && projection.EntryPath == std::optional<std::string>{ "README.md" },
			   "a first visit starts at the first file");
	}

	auto test_continue_lands_on_the_first_changed_then_unread_file() -> void
	{
		auto context     = application::overview_context{ };
		context.Receipts = {
			receipt("README.md", core::file_read_state::read),
			receipt("Engine/Renderer/HairStrandsLighting.cpp", core::file_read_state::seen),
			receipt("Engine/Renderer/HairStrandsVoxelization.cpp", core::file_read_state::read),
		};
		auto note       = application::local_note_record{ };
		note.Note.File  = core::file_identity_of("Engine/Renderer/HairStrandsVoxelization.cpp");
		note.Note.Text  = "Re-read the atomic move";
		context.Private = application::private_ledger_observation{
			.Notes            = { note },
			.LastReviewedHead = commit('c'),
		};
		context.Updates         = { update(3, 'd'), update(1, 'b'), update(2, 'c'), update(4, 'e') };
		context.Head            = commit('e');
		context.DraftedComments = 1U;

		// The change set unresolved: the column is unknown, the count absent.
		auto projection = application::project_pull_request_overview(snapshot(), context);
		expect(projection.SinceLastReview.Known
				   && projection.SinceLastReview.ReviewedAt == std::optional{ core::observed_utc_milliseconds{ 2'000 } }
				   && projection.SinceLastReview.Uploads == std::optional<std::size_t>{ 2U }
				   && !projection.SinceLastReview.FilesChanged.has_value(),
			   "the last review is dated by its update and the uploads after it are counted");
		expect(row_named(projection, "README.md").Changed
				   == application::overview_change_since_review::unknown,
			   "an unresolved change set is unknown, never unchanged");
		expect(row_named(projection, "README.md").Read == std::optional{ core::file_read_state::read }
				   && row_named(projection, "Engine/Renderer/HairStrandsLighting.cpp").Read
					   == std::optional{ core::file_read_state::seen }
				   && row_named(projection, "Engine/Renderer/HairStrandsVoxelization.cpp").Note
					   == std::optional<std::string>{ "Re-read the atomic move" },
			   "receipts and notes reach their rows");
		expect(projection.Progress.FilesRead == 2U && projection.Progress.PrivateNotes == 1U
				   && projection.Progress.DraftedComments == std::optional<std::size_t>{ 1U },
			   "progress counts read files, notes and drafts");
		expect(projection.Entry == application::overview_entry_kind::continue_review
				   && projection.EntryPath == std::optional<std::string>{ "Engine/Renderer/HairStrandsLighting.cpp" },
			   "with the change set unknown, continue lands on the first file not read");

		// The change set resolved: changed files lead, a new one is "new".
		context.ChangedSinceReviewPaths = std::vector<std::string>{
			"Engine/Shaders/HairStrandsDeepShadow.usf",
			"Engine/Renderer/HairStrandsVoxelization.cpp",
		};
		projection = application::project_pull_request_overview(snapshot(), context);
		expect(projection.SinceLastReview.FilesChanged == std::optional<std::size_t>{ 2U }
				   && row_named(projection, "Engine/Renderer/HairStrandsVoxelization.cpp").Changed
					   == application::overview_change_since_review::changed
				   && row_named(projection, "Engine/Shaders/HairStrandsDeepShadow.usf").Changed
					   == application::overview_change_since_review::added
				   && row_named(projection, "README.md").Changed
					   == application::overview_change_since_review::unchanged,
			   "a resolved change set marks changed and new files and counts them");
		expect(projection.EntryPath == std::optional<std::string>{ "Engine/Renderer/HairStrandsVoxelization.cpp" },
			   "continue lands on the first changed file even when it was read");

		// The head I reviewed is the head: nothing changed since.
		context.Head = commit('c');
		context.ChangedSinceReviewPaths.reset();
		projection = application::project_pull_request_overview(snapshot(), context);
		expect(projection.SinceLastReview.FilesChanged == std::optional<std::size_t>{ 0U }
				   && row_named(projection, "README.md").Changed
					   == application::overview_change_since_review::unchanged,
			   "a head I already reviewed has no changes since");
	}

	auto test_comment_groups_stand_in_for_threads_when_signed_out() -> void
	{
		auto snap                      = snapshot();
		snap.ReviewThreadsAvailability = application::github_collection_availability::unavailable;
		snap.ReviewThreads.clear();
		snap.ReviewCommentsAvailability = application::github_collection_availability::available;
		snap.ReviewCommentGroups        = {
			application::github_review_comment_group_record{
				.Root    = comment("mike", 10U, "README.md"),
				.Replies = { comment("jiayin", 11U, "README.md", application::github_comment_id{ "10" }) },
			},
		};
		const auto projection = application::project_pull_request_overview(
			snap, application::overview_context{ });
		expect(row_named(projection, "README.md").ThreadCount == 1U
				   && row_named(projection, "README.md").UnresolvedThreadCount == 0U
				   && projection.SinceLastReview.RepliesToMe == 1U
				   && projection.SinceLastReview.MyUnresolvedThreads == 0U,
			   "a signed-out read counts groups without claiming resolution");
	}

	auto test_epoch_row_ledger_keeps_the_published_rows() -> void
	{
		auto       ledger  = application::epoch_row_ledger{ };
		const auto catalog = core::epoch_catalog_identity{
			.Incarnation = identity<core::epoch_catalog_incarnation_id>(2U),
			.Revision    = core::epoch_catalog_revision{ 1 },
		};
		ledger.apply(application::epoch_catalog_projection{
			.Catalog = catalog,
			.Replace = true,
			.Changes = {
				{ .Kind = application::epoch_row_change_kind::insert, .Index = 0U, .Id = update(2, 'c').Id, .Row = update(2, 'c') },
				{ .Kind = application::epoch_row_change_kind::insert, .Index = 1U, .Id = update(1, 'b').Id, .Row = update(1, 'b') },
			},
		});
		expect(ledger.rows().size() == 2U && ledger.rows()[0].Head == commit('c'),
			   "inserts land at their index");
		ledger.apply(application::epoch_catalog_projection{
			.Changes = {
				{ .Kind = application::epoch_row_change_kind::insert, .Index = 0U, .Id = update(3, 'd').Id, .Row = update(3, 'd') },
				{ .Kind = application::epoch_row_change_kind::remove, .Id = update(1, 'b').Id },
				{ .Kind = application::epoch_row_change_kind::update, .Id = update(2, 'c').Id, .Row = update(2, 'f') },
			},
		});
		expect(ledger.rows().size() == 2U && ledger.rows()[0].Head == commit('d')
				   && ledger.rows()[1].Head == commit('f'),
			   "removes and updates apply by identity");
		auto page    = application::epoch_history_page_projection{ };
		page.Phase   = core::epoch_history_page_phase::loaded;
		page.Catalog = catalog;
		page.Rows    = { update(5, '9'), update(3, 'd') };
		ledger.apply(application::epoch_catalog_projection{ .HistoryPage = page });
		expect(ledger.rows().size() == 2U && ledger.rows()[0].Head == commit('9')
				   && ledger.rows()[1].Head == commit('d'),
			   "a loaded page reconciles the rows to the page");
		ledger.clear();
		expect(ledger.rows().empty(), "clear empties the ledger");
	}
} // namespace

auto main() -> int
{
	test_first_visit_starts_at_the_first_file();
	test_continue_lands_on_the_first_changed_then_unread_file();
	test_comment_groups_stand_in_for_threads_when_signed_out();
	test_epoch_row_ledger_keeps_the_published_rows();
	std::cout << "pull-request-overview-tests:pass\n";
	return EXIT_SUCCESS;
}
