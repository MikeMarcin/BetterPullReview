#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/review_submission_recovery_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "live_review_submission_provider.hpp"
#include "review_submission_session.hpp"
#if defined(__APPLE__)
	#include "../support/strict_cli_fixture.hpp"
	#include "gh_cli_transport.hpp"
	#include "github_cli_process_runner.hpp"
#endif

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {
	using namespace bpr;
	using namespace bpr::adapters::github::detail;
	using bpr::tests::expect;
	[[nodiscard]] auto commit(const std::uint8_t aSeed) -> core::git_commit_oid
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		std::fill_n(bytes.begin(), 20U, aSeed);
		return core::git_commit_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto target() -> application::review_mutation_target_binding
	{
		return {
			.Semantic = {
				.Host         = core::git_host_id{ 11U },
				.Repository   = core::repository_id{ 12U },
				.PullRequest  = core::pull_request_id{ 13U },
				.ExpectedHead = commit(0x11U),
			},
			.Hostname                 = application::github_canonical_hostname{ "github.com" },
			.ServerContract           = application::github_server_contract::github_com_2022_11_28,
			.Repository               = { .Owner = "MikeMarcin", .Name = "BPR-Test" },
			.Number                   = core::pull_request_number{ 1U },
			.RepositoryDatabaseId     = 1'328'277'631U,
			.RepositoryNodeId         = "R_kgDOTyvofw",
			.PullRequestDatabaseId    = 4'246'446'933U,
			.PullRequestNodeId        = "PR_kwDOTyvof879G6NV",
			.BaseRepositoryDatabaseId = 1'328'277'631U,
			.BaseOid                  = commit(0x10U),
			.BaseRef                  = "main",
			.HeadRef                  = "codex/disposable-live-review-20260810",
			.Open                     = true,
		};
	}

	[[nodiscard]] auto identity() -> application::review_mutation_acting_identity
	{
		return {
			.Account    = core::github_account_id{ 21U },
			.DatabaseId = 22U,
			.NodeId     = "MDQ6VXNlcjIy",
			.Login      = "fixture-reviewer",
		};
	}

	[[nodiscard]] auto credential_context()
		-> application::github_credential_operation_context
	{
		return {
			.Request            = core::request_id{ 31U },
			.ApplicationSession = core::application_session_id{ 32U },
			.Authority          = {
				.Profile         = core::host_profile_id{ 33U },
				.ProfileRevision = application::github_profile_revision{ 34U },
				.Host            = target().Semantic.Host,
				.Hostname        = application::github_canonical_hostname{ "github.com" },
				.Account         = identity().Account,
				.ServerContract  = application::github_server_contract::github_com_2022_11_28,
			},
			.IdentityGeneration = core::github_identity_generation{ 35U },
		};
	}

	[[nodiscard]] auto configuration() -> live_review_mutation_configuration
	{
		return {
			.CredentialContext = credential_context(),
			.Target            = target(),
			.ActingIdentity    = identity(),
		};
	}

	[[nodiscard]] auto target_response(
		const std::uint64_t         aRepository = 1'328'277'631U,
		const core::git_commit_oid& aHead       = target().Semantic.ExpectedHead)
		-> github_mutation_wire_completion
	{
		return github_mutation_wire_success{
			.Response = {
				.Status = 200,
				.Body   = std::string{ "{\"id\":4246446933,"
									   "\"node_id\":\"PR_kwDOTyvof879G6NV\",\"number\":1,"
									   "\"state\":\"open\",\"head\":{\"sha\":\"" }
					+ std::string{ core::canonical_hex(aHead).view() }
					+ "\",\"ref\":\"codex/disposable-live-review-20260810\"},"
					  "\"base\":{\"sha\":\""
					+ std::string{ core::canonical_hex(target().BaseOid).view() }
					+ "\",\"ref\":\"main\",\"repo\":{\"id\":"
					+ std::to_string(aRepository)
					+ ",\"node_id\":\"R_kgDOTyvofw\","
					  "\"full_name\":\"MikeMarcin/BPR-Test\"}}}",
			},
		};
	}

	[[nodiscard]] auto viewer_response() -> github_mutation_wire_completion
	{
		return github_mutation_wire_success{
			.Response = {
				.Status = 200,
				.Body   = "{\"login\":\"fixture-reviewer\",\"id\":22,"
						  "\"node_id\":\"MDQ6VXNlcjIy\"}",
			},
		};
	}

	class lease final : public application::review_mutation_transport_lease
	{ };
	auto request() -> application::review_submission_request
	{
		return { .Proposal = { .Draft = core::review_submission_draft_id{ 41U }, .Attempt = core::review_submission_attempt_id{ 1U }, .Target = target().Semantic, .ActingAccount = identity().Account, .Body = "Review summary.", .Disposition = core::review_disposition::comment }, .IdempotencyKey = "review-attempt-1", .LiveBinding = application::review_submission_live_binding{ .Target = target(), .ActingIdentity = identity() } };
	}
	using publication_state = application::staged_publication_state;
	constexpr auto Pull     = "/repos/MikeMarcin/BPR-Test/pulls/1";
	constexpr auto Web      = "https://github.com/MikeMarcin/BPR-Test/pull/1";
	auto           json_quote(const std::string& aText) -> std::string
	{
		auto bytes = QJsonDocument{ QJsonArray{ QString::fromStdString(aText) } }.toJson(QJsonDocument::Compact).toStdString();
		return bytes.substr(1, bytes.size() - 2);
	}
	auto plan() -> application::staged_review_plan
	{
		auto authority = std::array<std::uint8_t, 32>{ };
		authority.fill(1);
		application::staged_review_plan result{ .Submission = request(), .Owner = { core::bpr_profile_id{ 1 }, target().Semantic.Host, core::host_authority_fingerprint{ authority }, identity().Account, target().Semantic.Repository, target().Semantic.PullRequest }, .Generation = 7 };
		result.Submission.IdempotencyKey = "staged-tour-1";
		result.Stops                     = {
			{ core::tour_stop_id{ 1 }, { .Commit = target().Semantic.ExpectedHead, .Path = "src/right.cpp", .Side = core::diff_side::right, .FirstLine = 3, .LastLine = 4 }, "First stop", "Explain `$(touch nope)` & <why>.", "https://github.com/MikeMarcin/BPR-Test/blob/1111111111111111111111111111111111111111/src/right.cpp#L4-L5" },
			{ core::tour_stop_id{ 2 }, { .Commit = target().BaseOid, .Path = "src/left.cpp", .Side = core::diff_side::left, .FirstLine = 7, .LastLine = 7 }, "Second stop", "Removed behavior.", "https://github.com/MikeMarcin/BPR-Test/blob/1010101010101010101010101010101010101010/src/left.cpp#L8" }
		};
		result.Description = application::staged_description_edit{ "Original description.", "Original description.\n\nTour link separately authorized." };
		expect(application::valid(result), "fixture immutable plan is valid");
		return result;
	}
	struct server final : github_mutation_wire_transport
	{
		bool        Maintenance   = false;
		bool        PagedComments = false, FailSecondPage = false, ForeignNext = false;
		std::size_t PageTwoReads = 0;
		bool        Created = false, Submitted = false, ReadFailure = false, WrongActor = false, WrongHead = false, Extra = false, Duplicate = false, EarlySubmit = false;
		/// [[BPR-1560]]. The base branch took commits the head shares: GitHub's
		/// merge base, which numbers LEFT lines, is no longer the target's.
		bool        MovedMergeBase = false;
		std::size_t MergeBaseReads = 0U;
		bool        DenyWitness    = false;
		/// [[BPR-1367]]. The pull request's collection opens with a full page
		/// of other people's comments; or reports the second stop a line off.
		bool        CollectionNoise = false, WrongLine = false;
		std::size_t CollectionReads = 0, CollectionPageTwoReads = 0;
		std::size_t Writes = 0, Creates = 0, Submits = 0;
		/// [[BPR-1368]]. Pending stop edits by GraphQL, and REST edits GitHub refused.
		std::size_t                                        GraphqlEdits = 0, RestPendingEdits = 0;
		std::string                                        Summary, Description               = "Original description.";
		std::array<std::string, 2>                         Bodies;
		std::vector<std::string>                           IssueBodies;
		std::filesystem::path                              Root;
		std::function<void(std::size_t, std::string_view)> Boundary;
		std::optional<std::size_t>                         LoseAt;
		std::optional<std::size_t>                         SuppressAt;
		std::stop_source*                                  Stop = nullptr;
		std::optional<std::size_t>                         StopAt;
		std::function<void(server&)>                       AfterWrite;
		auto                                               review_json(std::uint64_t aId = 501) const -> std::string
		{
			return "{\"id\":" + std::to_string(aId) + ",\"node_id\":\"PRR_" + std::to_string(aId) + "\",\"html_url\":" + json_quote(std::string{ Web } + "#pullrequestreview-" + std::to_string(aId)) + ",\"user\":{\"id\":22,\"node_id\":\"MDQ6VXNlcjIy\",\"login\":\"fixture-reviewer\"},\"commit_id\":\"1111111111111111111111111111111111111111\",\"body\":" + json_quote(Summary) + ",\"pull_request_url\":\"https://api.github.com/repos/MikeMarcin/BPR-Test/pulls/1\",\"state\":\"" + (Submitted || EarlySubmit ? "COMMENTED" : "PENDING") + "\"" + (Submitted || EarlySubmit ? ",\"submitted_at\":\"2026-09-26T12:34:56Z\"" : "") + "}";
		}
		/// [[BPR-1367]]. GitHub's list of one review's comments carries no
		/// `line`, `side`, `start_line`, `start_side` or `subject_type`
		/// (MikeMarcin/BPR-Test#45); only the pull request's collection
		/// (`aLines`) does.
		auto comment_json(std::size_t aIndex, std::uint64_t aReview = 501, bool aLines = false) const -> std::string
		{
			const auto id     = 601 + aIndex;
			auto       result = "{\"id\":" + std::to_string(id) + ",\"node_id\":\"PRRC_" + std::to_string(id) + "\",\"html_url\":" + json_quote(std::string{ Web } + "#discussion_r" + std::to_string(id)) + ",\"pull_request_url\":\"https://api.github.com/repos/MikeMarcin/BPR-Test/pulls/1\",\"pull_request_review_id\":" + std::to_string(aReview) + ",\"user\":{\"id\":22,\"node_id\":\"MDQ6VXNlcjIy\",\"login\":\"fixture-reviewer\"},\"body\":" + json_quote(Bodies[aIndex]) + ",\"commit_id\":\"1111111111111111111111111111111111111111\",\"path\":\"src/" + (aIndex ? "left" : "right") + ".cpp\",\"position\":" + (aIndex ? "3" : "2");
			if (aLines) {
				result += std::string{ ",\"subject_type\":\"line\",\"side\":\"" } + (aIndex ? "LEFT" : "RIGHT") + "\",\"line\":" + (aIndex ? (WrongLine ? "9" : "8") : "5") + (aIndex ? "" : ",\"start_line\":4,\"start_side\":\"RIGHT\"");
			}
			return result + "}";
		}
		/// [[BPR-1367]]. Another reviewer's comment as the collection lists
		/// it: outdated, on a file, a range across sides, or a reply.
		static auto foreign_comment_json(std::size_t aIndex) -> std::string
		{
			const auto id     = std::to_string(9000 + aIndex);
			auto       result = "{\"id\":" + id + ",\"node_id\":\"PRRC_other_" + id + "\",\"html_url\":\"https://github.com/MikeMarcin/BPR-Test/pull/1#discussion_r" + id + "\",\"pull_request_url\":\"https://api.github.com/repos/MikeMarcin/BPR-Test/pulls/1\",\"pull_request_review_id\":900,\"user\":{\"id\":23,\"node_id\":\"other\",\"login\":\"other\"},\"body\":\"Other reviewer\",\"commit_id\":\"2222222222222222222222222222222222222222\",\"path\":\"src/other.cpp\"";
			switch (aIndex % 4U) {
			case 0:  result += ",\"subject_type\":\"line\",\"side\":\"RIGHT\",\"line\":null,\"original_line\":7"; break;
			case 1:  result += ",\"subject_type\":\"file\",\"side\":null,\"line\":null"; break;
			case 2:  result += ",\"subject_type\":\"line\",\"start_side\":\"LEFT\",\"start_line\":9,\"side\":\"RIGHT\",\"line\":3"; break;
			default: result += ",\"subject_type\":\"line\",\"side\":\"RIGHT\",\"line\":2,\"in_reply_to_id\":9000"; break;
			}
			return result + "}";
		}
		/// [[BPR-1367]]. GitHub's collection lists no comment of a pending
		/// review, not even to its author (MikeMarcin/BPR-Test#45).
		auto collection_json() const -> std::string
		{
			return Submitted || EarlySubmit ? "[" + comment_json(1, 501, true) + "," + comment_json(0, 501, true) + "]" : "[]";
		}
		/// [[BPR-1368]]. A comment of the review while it is pending, which
		/// GitHub's REST `/pulls/comments/{id}` does not find.
		auto pending_comment_path(const std::string& aPath) const -> bool
		{
			return Created && !Submitted && !EarlySubmit && (aPath == "/repos/MikeMarcin/BPR-Test/pulls/comments/601" || aPath == "/repos/MikeMarcin/BPR-Test/pulls/comments/602");
		}
		static auto not_found() -> github_mutation_wire_completion
		{
			return github_mutation_wire_error{ .Failure = application::review_mutation_failure::unavailable, .Transmission = github_mutation_transmission::indeterminate, .HttpStatus = 404U };
		}
		/// [[BPR-1368]]. GitHub's answer to the pending stop edit.
		auto edit_json(std::size_t aIndex) const -> std::string
		{
			const auto id = std::to_string(601 + aIndex);
			return "{\"data\":{\"updatePullRequestReviewComment\":{\"pullRequestReviewComment\":{\"id\":\"PRRC_" + id + "\",\"fullDatabaseId\":\"" + id + "\",\"url\":" + json_quote(std::string{ Web } + "#discussion_r" + id) + ",\"body\":" + json_quote(Bodies[aIndex]) + ",\"state\":\"PENDING\",\"viewerDidAuthor\":true,\"author\":{\"login\":\"fixture-reviewer\",\"databaseId\":22,\"id\":\"MDQ6VXNlcjIy\"},\"pullRequestReview\":{\"id\":\"PRR_501\",\"fullDatabaseId\":\"501\"},\"pullRequest\":{\"id\":\"PR_kwDOTyvof879G6NV\",\"databaseId\":4246446933,\"number\":1,\"repository\":{\"id\":\"R_kgDOTyvofw\",\"databaseId\":1328277631,\"nameWithOwner\":\"MikeMarcin/BPR-Test\"}}}}}}";
		}
		auto issues_json() const -> std::string
		{
			std::string result = "[";
			for (std::size_t i = 0; i < IssueBodies.size(); ++i) {
				if (i) {
					result += ",";
				}
				const auto id = 701 + i;
				result += "{\"id\":" + std::to_string(id) + ",\"node_id\":\"IC_" + std::to_string(id) + "\",\"html_url\":" + json_quote(std::string{ Web } + "#issuecomment-" + std::to_string(id)) + ",\"issue_url\":\"https://api.github.com/repos/MikeMarcin/BPR-Test/issues/1\",\"user\":{\"id\":22,\"node_id\":\"MDQ6VXNlcjIy\",\"login\":\"fixture-reviewer\"},\"body\":" + json_quote(IssueBodies[i]) + "}";
			}
			return result + "]";
		}
		void save() const
		{
			if (Root.empty()) {
				return;
			}
			std::ofstream file{ Root / "server-state", std::ios::binary };
			file << QJsonDocument{ QJsonObject{ { "created", Created }, { "submitted", Submitted }, { "summary", QString::fromStdString(Summary) }, { "first", QString::fromStdString(Bodies[0]) }, { "second", QString::fromStdString(Bodies[1]) }, { "description", QString::fromStdString(Description) } } }.toJson(QJsonDocument::Compact).toStdString();
			file.flush();
			expect(file.good(), "server state persists before checkpoint");
		}
		void load()
		{
			QFile file{ QString::fromStdString((Root / "server-state").string()) };
			if (!file.exists()) {
				return;
			}
			expect(file.open(QIODevice::ReadOnly), "server fixture reopens");
			const auto json = QJsonDocument::fromJson(file.readAll()).object();
			Created         = json["created"].toBool();
			Submitted       = json["submitted"].toBool();
			Summary         = json["summary"].toString().toStdString();
			Bodies          = { json["first"].toString().toStdString(), json["second"].toString().toStdString() };
			Description     = json["description"].toString().toStdString();
		}
		auto perform_review_mutation(const github_mutation_wire_request& aRequest, std::stop_token, github_mutation_transmission_witness aWitness) -> github_mutation_wire_completion override
		{
			const bpr::tests::exit_on_failure insideSession;
			const auto&                       wire = aRequest.Wire;
			const auto                        ok   = [](std::string aBody) { return github_mutation_wire_success{ .Response = { .Status = 200, .Body = std::move(aBody) } }; };
			if (wire.Method == http_method::get) {
				if (ReadFailure) {
					return github_mutation_wire_error{ };
				}
				if (wire.Path == "/repos/MikeMarcin/BPR-Test/issues/1/comments?per_page=100&page=1") {
					return ok(issues_json());
				}
				if (wire.Path == "/user") {
					if (WrongActor) {
						return ok("{\"id\":23,\"node_id\":\"other\",\"login\":\"other\"}");
					}
					return viewer_response();
				}
				if (wire.Path == Pull) {
					auto  response = target_response(target().RepositoryDatabaseId, WrongHead ? commit(0x22) : target().Semantic.ExpectedHead);
					auto& body     = std::get<github_mutation_wire_success>(response).Response.Body;
					body.pop_back();
					body += ",\"body\":" + json_quote(Description) + "}";
					return response;
				}
				if (wire.Path == std::string{ Pull } + "/reviews?per_page=100&page=1") {
					return ok(Created ? "[" + review_json() + (Duplicate ? "," + review_json(502) : "") + "]" : "[]");
				}
				if (wire.Path == std::string{ Pull } + "/reviews/501") {
					return ok(review_json());
				}
				if (wire.Path == std::string{ Pull } + "/comments?per_page=100&page=1") {
					++CollectionReads;
					if (!CollectionNoise) {
						return ok(collection_json());
					}
					std::string page = "[";
					for (std::size_t i = 0; i < 100U; ++i) {
						page += (i ? "," : "") + foreign_comment_json(i);
					}
					return github_mutation_wire_success{ .Response = { .Status = 200, .Headers = { { "Link", "<https://api.github.com" + std::string{ Pull } + "/comments?per_page=100&page=2>; rel=\"next\", <https://api.github.com" + std::string{ Pull } + "/comments?per_page=100&page=2>; rel=\"last\"" } }, .Body = page + "]" } };
				}
				if (CollectionNoise && wire.Path == std::string{ Pull } + "/comments?per_page=100&page=2") {
					++CollectionPageTwoReads;
					return github_mutation_wire_success{ .Response = { .Status = 200, .Headers = { { "Link", "<https://api.github.com" + std::string{ Pull } + "/comments?per_page=100&page=1>; rel=\"prev\", <https://api.github.com" + std::string{ Pull } + "/comments?per_page=100&page=1>; rel=\"first\"" } }, .Body = collection_json() } };
				}
				if (wire.Path == std::string{ Pull } + "/reviews/501/comments?per_page=100&page=1") {
					if (PagedComments) {
						const auto origin = ForeignNext ? "https://credential-canary@foreign.invalid" : "https://api.github.com";
						return github_mutation_wire_success{ .Response = { .Status = 200, .Headers = { { "Link", "<" + std::string{ origin } + Pull + "/reviews/501/comments?per_page=100&page=2>; rel=\"next\"" } }, .Body = "[" + comment_json(1) + "]" } };
					}
					return ok("[" + comment_json(1) + "," + comment_json(0) + (Extra ? "," + comment_json(0) : "") + "]");
				}
				if (PagedComments && wire.Path == std::string{ Pull } + "/reviews/501/comments?per_page=100&page=2") {
					++PageTwoReads;
					if (FailSecondPage) {
						return github_mutation_wire_error{ };
					}
					return ok("[" + comment_json(0) + "]");
				}
				if (Duplicate && wire.Path == std::string{ Pull } + "/reviews/502/comments?per_page=100&page=1") {
					return ok("[" + comment_json(1, 502) + "," + comment_json(0, 502) + "]");
				}
				if (pending_comment_path(wire.Path)) {
					return not_found();
				}
				const auto head = std::string{ core::canonical_hex(target().Semantic.ExpectedHead).view() };
				if (wire.Path == "/repos/MikeMarcin/BPR-Test/compare/" + target().BaseRef + "..." + head + "?page=2&per_page=1") {
					++MergeBaseReads;
					const auto mergeBase = MovedMergeBase ? std::string(40U, 'c') : std::string{ core::canonical_hex(application::comparison_base(target())).view() };
					return ok("{\"url\":\"https://api.github.com/repos/MikeMarcin/BPR-Test/compare/" + target().BaseRef + "..." + head + "\",\"base_commit\":{\"sha\":\"" + std::string(40U, 'b')
							  + "\"},\"merge_base_commit\":{\"sha\":\"" + mergeBase + "\"}}");
				}
				expect(false, "fixture answers only independently named read endpoints");
			}
			const auto index = Writes;
			if (Boundary) {
				Boundary(index, "prepared");
			}
			// [[BPR-1560]]. A denied authorization sends nothing.
			if (DenyWitness && aWitness && !aWitness()) {
				return github_mutation_wire_error{ };
			}
			expect(aWitness && aWitness(), "every write has durable child witness");
			expect(!aWitness(), "child witness is exact one-use");
			if (Boundary) {
				Boundary(index, "witnessed");
			}
			++Writes;
			if (!Root.empty()) {
				std::ofstream trace{ Root / "server-writes", std::ios::app };
				trace << index << '\n';
			}
			if (SuppressAt == index) {
				ReadFailure = true;
				return github_mutation_wire_error{ };
			}
			const auto  json   = QJsonDocument::fromJson(QByteArray::fromStdString(wire.Body)).object();
			std::string answer = "{}";
			if (wire.Method == http_method::post && wire.Path == "/repos/MikeMarcin/BPR-Test/issues/1/comments") {
				expect(!Created && json.size() == 1, "explicit compatibility never creates a native review");
				const auto body = json["body"].toString().toStdString();
				if (IssueBodies.size() == 2) {
					expect(body.contains("#issuecomment-701") && body.contains("#issuecomment-702"), "compatibility index follows actual returned stop identities");
				}
				IssueBodies.push_back(body);
			} else if (wire.Method == http_method::patch && (wire.Path == "/repos/MikeMarcin/BPR-Test/issues/comments/701" || wire.Path == "/repos/MikeMarcin/BPR-Test/issues/comments/702")) {
				expect(IssueBodies.size() == 3 && json.size() == 1, "compatibility links wired after index exists");
				const std::size_t i    = wire.Path.ends_with("701") ? 0 : 1;
				const auto        body = json["body"].toString().toStdString();
				expect(body.contains("#issuecomment-703") && body.contains(i ? "#issuecomment-701" : "#issuecomment-702"), "compatibility navigation uses real returned IDs");
				IssueBodies[i] = body;
			} else if (wire.Method == http_method::post && wire.Path == std::string{ Pull } + "/reviews") {
				expect(!Created && !json.contains("event") && json["commit_id"] == "1111111111111111111111111111111111111111", "batch creates one exact pending review without event");
				const auto comments = json["comments"].toArray();
				expect(comments.size() == 2, "batch has both comments");
				for (int i = 0; i < 2; ++i) {
					const auto item = comments[i].toObject();
					expect(!item.contains("subject_type") && !item.contains("pull_request_review_id"), "batch avoids undocumented fields");
					Bodies[static_cast<std::size_t>(i)] = item["body"].toString().toStdString();
					expect(!Bodies[static_cast<std::size_t>(i)].contains("discussion_r") && !Bodies[static_cast<std::size_t>(i)].contains("files#r"), "unwired initial body has source links only");
				}
				expect(comments[0].toObject()["line"] == 5 && comments[0].toObject()["start_line"] == 4 && comments[1].toObject()["line"] == 8 && comments[1].toObject()["side"] == "LEFT", "batch uses real inclusive one-based source ranges");
				Created = true;
				++Creates;
				Summary = json["body"].toString().toStdString();
			} else if (pending_comment_path(wire.Path)) {
				// [[BPR-1368]]. GitHub answers a REST edit of a pending
				// review's comment 404 (MikeMarcin/BPR-Test#45).
				++RestPendingEdits;
				return not_found();
			} else if (wire.Method == http_method::patch && (wire.Path == "/repos/MikeMarcin/BPR-Test/pulls/comments/601" || wire.Path == "/repos/MikeMarcin/BPR-Test/pulls/comments/602")) {
				expect(Created && Submitted && Maintenance && json.size() == 1, "only a submitted owned body is edited through REST, under the exact maintenance plan");
				const std::size_t i    = wire.Path.ends_with("601") ? 0 : 1;
				const auto        body = json["body"].toString().toStdString();
				expect(body.contains(std::string{ Web } + "#pullrequestreview-501") && body.contains(std::string{ Web } + "/files#r" + (i ? "601" : "602")), "actual returned review/comment URLs wire navigation to the Files changed tab");
				Bodies[i] = body;
			} else if (wire.Method == http_method::post && wire.Path == "/graphql") {
				// [[BPR-1368]]. The pending stop edit: the fixed mutation, by
				// the comment's node ID, while the review is pending.
				const auto variables = json["variables"].toObject();
				const auto node      = variables["comment"].toString();
				expect(Created && !Submitted && json.size() == 2 && variables.size() == 2 && json["query"].toString().startsWith("mutation UpdateReviewComment($comment:ID!,$body:String!){updatePullRequestReviewComment(") && (node == "PRRC_601" || node == "PRRC_602"), "a pending stop is edited only by the fixed GraphQL mutation, by its node ID");
				const std::size_t i    = node == "PRRC_601" ? 0 : 1;
				const auto        body = variables["body"].toString().toStdString();
				expect(body.contains(std::string{ Web } + "#pullrequestreview-501") && body.contains(std::string{ Web } + "/files#r" + (i ? "601" : "602")), "actual returned review/comment URLs wire navigation to the Files changed tab");
				Bodies[i] = body;
				++GraphqlEdits;
				answer = edit_json(i);
			} else if (wire.Method == http_method::post && wire.Path == std::string{ Pull } + "/reviews/501/events") {
				expect(Created && !Submitted && json["event"] == "COMMENT" && json.size() == 2, "only exact review receives COMMENT once");
				expect(Bodies[0].contains("/files#r602") && Bodies[1].contains("/files#r601"), "all navigation edits precede submit");
				Summary = json["body"].toString().toStdString();
				expect(Summary.contains("/files#r601") && Summary.contains("/files#r602") && !Summary.contains("#discussion_r") && !Summary.contains("incomplete"), "final index replaces incomplete warning");
				Submitted = true;
				++Submits;
			} else if (wire.Method == http_method::put && wire.Path == std::string{ Pull } + "/reviews/501") {
				expect(Maintenance && Submitted && json.size() == 1, "maintenance only updates exact submitted root body");
				Summary = json["body"].toString().toStdString();
			} else if (wire.Method == http_method::patch && wire.Path == Pull) {
				expect((Submitted || IssueBodies.size() == 3) && json.size() == 1, "description update is separate and follows publication");
				Description = json["body"].toString().toStdString();
			} else {
				expect(false, "fixture answers only fixed typed mutation endpoints");
			}
			save();
			if (Boundary) {
				Boundary(index, "applied");
			}
			if (AfterWrite) {
				AfterWrite(*this);
			}
			if (Stop && StopAt == index) {
				Stop->request_stop();
			}
			if (LoseAt == index) {
				ReadFailure = true;
				return github_mutation_wire_error{ };
			}
			return ok(answer); // Session must reconcile server reads, never trust a write response.
		}
	};
	struct scratch
	{
		std::filesystem::path Path = std::filesystem::temp_directory_path() / ("bpr-staged-" + std::to_string(std::random_device{ }()));
		scratch()
		{
			expect(std::filesystem::create_directory(Path), "scratch created");
			expect(adapters::sqlite::make_storage_root_owner_private(Path), "scratch private");
		}
		~scratch()
		{
			std::error_code error;
			std::filesystem::remove_all(Path, error);
		}
	};
	auto storage(const std::filesystem::path& aPath) -> std::unique_ptr<application::review_submission_recovery_port>
	{
		auto opened = adapters::sqlite::make_review_submission_recovery_store({ .StorageRoot = aPath, .Host = "github.com", .Account = "21", .Repository = "MikeMarcin/BPR-Test", .PullRequest = 1 });
		expect(opened.has_value(), "production staged journal opens");
		return std::move(*opened);
	}
	auto session(server& aServer, const std::filesystem::path& aPath, live_review_mutation_configuration aConfiguration = configuration()) -> std::unique_ptr<review_submission_session> { return std::make_unique<review_submission_session>(aServer, std::move(aConfiguration), std::make_shared<lease>(), storage(aPath)); }
	void test_staged_sessions()
	{
		{
			scratch root;
			server  remote;
			auto    provider = session(remote, root.Path);
			expect(provider->reconcile_tour(plan()).State == publication_state::outcome_unknown && remote.Writes == 0, "read-only missing journal never starts publication");
			const auto result = provider->publish_tour(plan(), { }, [](const auto&) { return true; });
			expect(result.State == publication_state::published && result.Record && result.Record->DescriptionDone, "exact batch and optional description finish");
			expect(remote.Writes == 5 && remote.Creates == 1 && remote.Submits == 1, "one create, two patches, one submit, one separate description update");
			expect(remote.GraphqlEdits == 2 && remote.RestPendingEdits == 0, "both pending stops are edited by GraphQL, neither through the REST edit GitHub refuses");
			expect(provider->publish_tour(plan(), { }, [](const auto&) { return true; }).State == publication_state::published && remote.Writes == 5, "completed receipt never replays effects");
			provider.reset();
			provider = session(remote, root.Path);
			expect(provider->pending_tours()->empty() && provider->reconcile_tour(plan()).State == publication_state::published && remote.Writes == 5, "fresh session reads exact completed journal");
		}
		{
			// [[BPR-1560]]. A tour with a stop on a LEFT line asks GitHub's merge
			// base before anything is written; when it moved since the tour was
			// written, nothing is written.
			scratch root;
			server  remote;
			remote.MovedMergeBase = true;
			auto       provider   = session(remote, root.Path);
			const auto result     = provider->publish_tour(plan(), { }, [](const auto&) { return true; });
			expect(result.State == publication_state::conflict && remote.Writes == 0 && remote.MergeBaseReads == 1U,
				   "a moved merge base refuses a tour with a LEFT stop before any write");
		}
		for (const std::size_t stopped : { std::size_t{ 0U }, std::size_t{ 2U } }) {
			// [[BPR-1560]]. A resumed tour asks again: the journal was admitted
			// (and, the second time, part of it written) on the merge base it was
			// staged against, then the merge base moved with base.sha and head
			// unchanged. Resume writes nothing until it is back.
			scratch          root;
			server           remote;
			std::stop_source stop;
			auto             provider = session(remote, root.Path);
			if (stopped == 0U) {
				remote.DenyWitness = true;
				const auto denied  = provider->publish_tour(plan(), { }, [](const auto&) { return false; });
				expect(remote.Writes == 0 && !provider->pending_tours()->empty(), "the first write is denied after the journal is admitted");
				remote.DenyWitness = false;
			} else {
				remote.Stop   = &stop;
				remote.StopAt = stopped;
				static_cast<void>(provider->publish_tour(plan(), stop.get_token(), [](const auto&) { return true; }));
				remote.Stop = nullptr;
				expect(remote.Writes == stopped + 1, "Stop leaves part of the tour written");
			}
			const auto written    = remote.Writes;
			remote.MovedMergeBase = true;
			provider.reset();
			provider          = session(remote, root.Path);
			const auto reads  = remote.MergeBaseReads;
			const auto resume = provider->publish_tour(plan(), { }, [](const auto&) { return true; });
			expect(resume.State == publication_state::conflict && remote.Writes == written && remote.MergeBaseReads > reads,
				   "Resume asks GitHub's merge base again and writes no LEFT coordinate from a moved one");
			remote.MovedMergeBase = false;
			provider.reset();
			provider = session(remote, root.Path);
			expect(provider->publish_tour(plan(), { }, [](const auto&) { return true; }).State == publication_state::published && remote.Writes == 5,
				   "with the merge base back, the same Resume completes the tour");
		}
		{
			// [[BPR-1367]]. The submitted review's lines are read from every
			// page of the pull request's collection, past other reviewers'
			// outdated, file, cross-side and reply comments.
			scratch root;
			server  remote;
			remote.CollectionNoise = true;
			auto       provider    = session(remote, root.Path);
			const auto result      = provider->publish_tour(plan(), { }, [](const auto&) { return true; });
			expect(result.State == publication_state::published && remote.Writes == 5 && remote.CollectionReads > 0 && remote.CollectionPageTwoReads > 0, "submitted tour lines are confirmed across every page of the collection");
		}
		{
			// [[BPR-1367]]. Lines are confirmed from the collection once the
			// review is submitted: a stop GitHub reports at another line is a
			// conflict, not a publication.
			scratch root;
			server  remote;
			remote.AfterWrite   = [](auto& aServer) { aServer.WrongLine = aServer.Submitted; };
			auto       provider = session(remote, root.Path);
			const auto result   = provider->publish_tour(plan(), { }, [](const auto&) { return true; });
			expect(result.State == publication_state::conflict && remote.Writes == 4 && remote.Submits == 1 && remote.CollectionReads > 0, "a stop the collection reports at another line after submission is a conflict");
		}
		for (int mode = 0; mode < 3; ++mode) {
			scratch root;
			server  remote;
			remote.PagedComments  = true;
			remote.FailSecondPage = mode == 1;
			remote.ForeignNext    = mode == 2;
			auto       provider   = session(remote, root.Path);
			const auto result     = provider->publish_tour(plan(), { }, [](const auto&) { return true; });
			if (mode == 0) {
				expect(result.State == publication_state::published && remote.Writes == 5 && remote.Creates == 1 && remote.Submits == 1 && remote.PageTwoReads > 0, "complete multi-page membership authorizes only the exact remaining plan");
			} else {
				expect(result.State != publication_state::published && remote.Writes == 1 && remote.Creates == 1 && remote.Submits == 0, "partial or credential-bearing foreign continuation cannot authorize navigation or submit");
				expect(mode == 1 ? remote.PageTwoReads > 0 : remote.PageTwoReads == 0, "foreign continuation is rejected before dispatch; failed owned page stays partial");
			}
		}
		for (std::size_t lost = 0; lost < 5; ++lost) {
			scratch root;
			server  remote;
			remote.LoseAt       = lost;
			auto       provider = session(remote, root.Path);
			const auto unknown  = provider->publish_tour(plan(), { }, [](const auto&) { return true; });
			expect(unknown.State == publication_state::outcome_unknown && remote.Writes == lost + 1, "lost response and failed observation remain unknown");
			expect(provider->publish_tour(plan(), { }, [](const auto&) { return true; }).State == publication_state::outcome_unknown && remote.Writes == lost + 1, "unknown write cannot replay");
			provider.reset();
			remote.ReadFailure = false;
			remote.LoseAt.reset();
			provider        = session(remote, root.Path);
			const auto read = provider->reconcile_tour(plan());
			expect(read.State != publication_state::outcome_unknown && remote.Writes == lost + 1, "restart reconciles applied effect with zero writes");
			expect(provider->publish_tour(plan(), { }, [](const auto&) { return true; }).State == publication_state::published && remote.Writes == 5 && remote.Creates <= 1 && remote.Submits <= 1, "explicit Resume performs only remaining effects");
		}
		for (std::size_t suppressed = 0; suppressed < 5; ++suppressed) {
			scratch root;
			server  remote;
			remote.SuppressAt = suppressed;
			auto provider     = session(remote, root.Path);
			expect(provider->publish_tour(plan(), { }, [](const auto&) { return true; }).State == publication_state::outcome_unknown, "possibly sent but currently absent stays unknown");
			remote.ReadFailure = false;
			provider.reset();
			provider = session(remote, root.Path);
			expect(provider->reconcile_tour(plan()).State == publication_state::outcome_unknown && provider->publish_tour(plan(), { }, [](const auto&) { return true; }).State == publication_state::outcome_unknown && remote.Writes == suppressed + 1, "empty or baseline body is not safe failure proof");
		}
		for (int fault = 0; fault < 5; ++fault) {
			scratch root;
			server  remote;
			remote.AfterWrite   = [fault](auto& r) {if(r.Writes==1){r.Extra=fault==0;r.Duplicate=fault==1;r.EarlySubmit=fault==2;r.WrongHead=fault==3;r.WrongActor=fault==4;} };
			auto       provider = session(remote, root.Path);
			const auto result   = provider->publish_tour(plan(), { }, [](const auto&) { return true; });
			expect(result.State == publication_state::conflict || result.State == publication_state::outcome_unknown, "contamination, ambiguity, premature submit, head and actor movement stop");
			expect(remote.Writes == 1 && remote.Submits == 0, "unsafe state never authorizes another write");
		}
		{
			scratch root;
			server  remote;
			remote.Created = true;
			remote.Summary = "Existing website review";
			auto provider  = session(remote, root.Path);
			expect(provider->publish_tour(plan(), { }, [](const auto&) { return true; }).State == publication_state::conflict && remote.Writes == 0, "existing pending is never adopted or discarded");
		}
		for (std::size_t stopped = 0; stopped < 5; ++stopped) {
			scratch          root;
			server           remote;
			std::stop_source stop;
			remote.Stop   = &stop;
			remote.StopAt = stopped;
			auto provider = session(remote, root.Path);
			static_cast<void>(provider->publish_tour(plan(), stop.get_token(), [](const auto&) { return true; }));
			expect(remote.Writes == stopped + 1, "Stop prevents remaining writes");
			remote.Stop = nullptr;
			provider.reset();
			provider = session(remote, root.Path);
			static_cast<void>(provider->reconcile_tour(plan()));
			expect(remote.Writes == stopped + 1, "restart remains read-only");
			expect(provider->publish_tour(plan(), { }, [](const auto&) { return true; }).State == publication_state::published && remote.Writes == 5, "Resume after Stop completes bounded plan");
		}
	}
	void test_compatibility_sessions()
	{
		auto input                         = plan();
		input.Compatibility                = true;
		input.Description->AppendIndexLink = true;
		for (std::size_t lost = 0; lost < 6; ++lost) {
			scratch root;
			server  remote;
			remote.LoseAt       = lost;
			auto       provider = session(remote, root.Path);
			const auto partial  = provider->publish_tour(input, { }, [](const auto&) { return true; });
			expect(partial.State == publication_state::outcome_unknown && remote.Writes == lost + 1, "lost compatibility child retains partial/unknown identity");
			provider.reset();
			remote.LoseAt.reset();
			remote.ReadFailure = false;
			provider           = session(remote, root.Path);
			const auto read    = provider->reconcile_tour(input);
			expect(remote.Writes == lost + 1 && read.Record && read.Record->Comments.size() <= 2, "compatibility restart reads exact retained receipts");
			expect(provider->publish_tour(input, { }, [](const auto&) { return true; }).State == publication_state::published && remote.Writes == 6 && remote.Creates == 0 && remote.Submits == 0 && remote.IssueBodies.size() == 3, "explicit Resume never duplicates visible compatibility comments or claims review submission");
			expect(remote.Description.contains("#issuecomment-703"), "separate description backlink uses returned compatibility index");
			if (lost == 0) {
				auto malformed      = *provider->reconcile_tour(input).Record;
				malformed.Published = false;
				malformed.Pending   = application::staged_review_request{ .Binding = *input.Submission.LiveBinding, .Operation = application::staged_review_operation::submit_comment };
				expect(!application::valid(malformed), "malformed compatibility completion is refused before deriving a native submit");
				malformed.Published     = true;
				malformed.Index->WebUrl = "invalid";
				expect(!application::valid(malformed), "malformed compatibility link is refused without dereferencing a missing rendering");
			}
		}
	}

	auto maintenance_plan(const application::staged_publication_record& aPublished) -> application::staged_review_plan
	{
		const auto        original = plan();
		core::review_tour draft{ .Id = core::tour_draft_id{ 41 }, .Owner = original.Owner, .Base = target().BaseOid, .Head = target().Semantic.ExpectedHead, .Problem = "Problem", .Solution = "Solution", .Revision = original.Generation, .LastStopId = 2 };
		for (const auto& stop : original.Stops) {
			draft.Stops.push_back({ .Id = stop.Id, .Title = stop.Title, .Body = stop.Narrative, .Destination = stop.Source });
		}
		draft = *application::published_tour(aPublished, draft);
		std::swap(draft.Stops[0], draft.Stops[1]);
		++draft.Revision;
		auto result                        = original;
		result.Submission.IdempotencyKey   = "maintenance-1";
		result.Submission.Proposal.Attempt = core::review_submission_attempt_id{ 8 };
		result.Generation                  = draft.Revision;
		result.Stops.clear();
		result.Description.reset();
		result.Update = draft;
		expect(application::valid(result), "maintenance plan retains exact root and original stop identities");
		return result;
	}
	void test_maintenance_sessions()
	{
		for (std::size_t lost = 0; lost < 3; ++lost) {
			scratch    root;
			server     remote;
			auto       provider = session(remote, root.Path);
			const auto original = provider->publish_tour(plan(), { }, [](const auto&) { return true; });
			expect(original.Record.has_value(), "initial publication exists");
			const auto update  = maintenance_plan(*original.Record);
			remote.Maintenance = true;
			remote.LoseAt      = 5 + lost;
			const auto partial = provider->publish_tour(update, { }, [](const auto&) { return true; });
			expect(partial.State == publication_state::outcome_unknown && remote.Writes == 6 + lost, "maintenance lost response retains exact desired body and witness");
			provider.reset();
			remote.ReadFailure = false;
			remote.LoseAt.reset();
			provider            = session(remote, root.Path);
			const auto observed = provider->reconcile_tour(update);
			expect(observed.Record && observed.State != publication_state::outcome_unknown && remote.Writes == 6 + lost, "maintenance restart only reads confirmed body prefixes");
			expect(provider->publish_tour(update, { }, [](const auto&) { return true; }).State == publication_state::published && remote.Writes == 8 && remote.Creates == 1 && remote.Submits == 1, "maintenance Resume preserves identities without another review");
		}
		for (int race = 0; race < 3; ++race) {
			scratch    root;
			server     remote;
			auto       provider = session(remote, root.Path);
			const auto initial  = provider->publish_tour(plan(), { }, [](const auto&) { return true; });
			const auto update   = maintenance_plan(*initial.Record);
			remote.Maintenance  = true;
			if (race == 0) {
				remote.Bodies[0] += "website text";
			}
			if (race == 1) {
				remote.WrongActor = true;
			}
			if (race == 2) {
				remote.WrongHead = true;
			}
			const auto result = provider->publish_tour(update, { }, [](const auto&) { return true; });
			expect(result.State == publication_state::conflict && remote.Writes == 5, "remote body/actor/head drift refuses maintenance before write");
		}
	}
#if defined(__APPLE__)
	void test_strict_tour_scope()
	{
		for (const bool stopAfterPost : { false, true }) {
			scratch                   root;
			server                    remote;
			std::stop_source          stop;
			tests::strict_cli_fixture process{ root.Path };
			process.Respond   = [&](const auto& aWire) { return remote.perform_review_mutation(aWire, { }, [used = false]() mutable { return !std::exchange(used, true); }); };
			process.Completed = [&](const auto& aWire) {
				if (aWire.Wire.Method == http_method::post) {
					process.available(false);
					if (stopAfterPost) {
						stop.request_stop();
						remote.ReadFailure = true;
					}
				}
			};
			const auto profile = tests::strict_cli_profile(credential_context());
			const auto base    = process.configuration();
			expect(profile && base, "strict tour fixture selects actual configured child");
			const auto pinned = pin_gh_cli_configuration(*base, "github.com", identity().Login);
			expect(pinned.has_value(), "strict tour pins its exact selected login");
			gh_cli_transport        strict{ *profile, *pinned, process };
			tests::strict_cli_relay relay{ strict, process };
			const auto              input = plan();
			{
				review_submission_session initial{ relay, configuration(), std::make_shared<lease>(), storage(root.Path) };
				const auto                result = initial.publish_tour(input, stop.get_token(), [](const auto&) { return true; });
				std::cerr << "strict tour: state=" << static_cast<unsigned>(result.State) << " probes=" << process.Probes << " acquisitions=" << process.Acquisitions << " api=" << process.ApiCalls << " writes=" << process.Writes << '\n';
				expect(result.State == (stopAfterPost ? publication_state::outcome_unknown : publication_state::published)
						   && process.Acquisitions == 1U && process.Writes == (stopAfterPost ? 1U : 5U),
					   "strict tour shares one acquired owner through all writes and reconciliation despite removed lookup");
			}
			expect(process.Credential.expired(), "tour credential is released after operation drain");
			const auto calls = process.ApiCalls;
			for (const auto failure : { github_cli_process_failure::nonzero_exit, github_cli_process_failure::cancelled,
										github_cli_process_failure::timeout, github_cli_process_failure::process_crash, github_cli_process_failure::reap_failure }) {
				process.AcquisitionFailure = failure == github_cli_process_failure::nonzero_exit ? std::nullopt : std::optional{ failure };
				review_submission_session restarted{ relay, configuration(), std::make_shared<lease>(), storage(root.Path) };
				const auto                result   = restarted.publish_tour(input, { }, [](const auto&) { return true; });
				const auto                expected = failure == github_cli_process_failure::nonzero_exit ? application::review_mutation_failure::credential_unavailable
					: failure == github_cli_process_failure::cancelled                                   ? application::review_mutation_failure::cancelled
																										 : application::review_mutation_failure::credential_tool_failed;
				expect(result.State == (stopAfterPost ? publication_state::outcome_unknown : publication_state::published)
						   && result.Failure == expected && result.Record && result.Record->Plan == input && result.Record->PossiblyTransmitted == stopAfterPost
						   && process.ApiCalls == calls && process.Writes == (stopAfterPost ? 1U : 5U),
					   "strict tour credential/tool refusal retains durable unknown or confirmed state with zero further API/writes");
			}
			process.AcquisitionFailure.reset();
			process.available(true);
			process.Completed  = { };
			remote.ReadFailure = false;
			review_submission_session resumed{ relay, configuration(), std::make_shared<lease>(), storage(root.Path) };
			const auto                recovered = resumed.reconcile_tour(input);
			expect(recovered.State != publication_state::outcome_unknown && process.Writes == (stopAfterPost ? 1U : 5U), "explicit strict tour recovery reads only and does not repeat the first POST");
			expect(resumed.publish_tour(input, { }, [](const auto&) { return true; }).State == publication_state::published
					   && process.Writes == 5U && remote.Creates == 1U && remote.Submits == 1U,
				   "a new strict tour Resume reacquires once and publishes only remaining steps");
			process.verify_persistence();
		}
	}

	// [[BPR-1368]]. One native two-stop tour published on the owner's
	// designated live target, MikeMarcin/BPR-Test, by the production session
	// over the production `gh api` transport: create pending, edit both stops
	// while pending, submit. Run by hand: `--live-tour DIR` reads the pull
	// request a probe script made into DIR/state.env and writes what it
	// published to DIR/result.env. Not registered as a row: it writes to
	// GitHub. Every answer's `X-RateLimit-Remaining` is read, and no request
	// leaves once one reports fewer than 500.
	struct live_state
	{
		std::map<std::string, std::string> Values;
		explicit live_state(const std::filesystem::path& aFolder)
		{
			std::ifstream input{ aFolder / "state.env" };
			for (std::string line; std::getline(input, line);) {
				const auto equals = line.find('=');
				if (equals != std::string::npos) {
					Values.emplace(line.substr(0U, equals), line.substr(equals + 1U));
				}
			}
			expect(Values.contains("number") && Values.contains("head") && Values.contains("gh"), "the live probe's state is read");
		}
		[[nodiscard]] auto text(const char* aKey) const -> std::string { return Values.at(aKey); }
		[[nodiscard]] auto number(const char* aKey) const -> std::uint64_t { return std::stoull(Values.at(aKey)); }
	};
	struct live_relay final : github_mutation_wire_transport
	{
		github_mutation_wire_transport& Inner;
		std::ofstream                   Log;
		std::size_t                     Calls = 0, PendingEdits = 0, ParsedEdits = 0, RestPendingEdits = 0;
		long                            Lowest    = 5000;
		bool                            Exhausted = false;
		/// [[BPR-1222]]. The two-account course's own floor, Stop gesture and
		/// write counts. A write is any request that is not a GET.
		long              Floor  = 500;
		std::size_t       Writes = 0, Posts = 0, Patches = 0, StopAfterWrites = 0;
		std::stop_source* Stop = nullptr;
		live_relay(github_mutation_wire_transport& aInner, const std::filesystem::path& aFolder)
			: Inner{ aInner }
			, Log{ aFolder / "requests.log" }
		{ }
		auto perform_review_mutation(const github_mutation_wire_request& aRequest, std::stop_token aStop, github_mutation_transmission_witness aWitness) -> github_mutation_wire_completion override
		{
			if (Exhausted) {
				return github_mutation_wire_error{ };
			}
			const auto& wire = aRequest.Wire;
			if (wire.Operation == review_mutation_operation::staged_update_comment && wire.Method == http_method::patch) {
				++RestPendingEdits;
			}
			auto result = Inner.perform_review_mutation(aRequest, aStop, std::move(aWitness));
			++Calls;
			if (wire.Method != http_method::get) {
				++Writes;
				Posts += wire.Method == http_method::post ? 1U : 0U;
				Patches += wire.Method == http_method::patch ? 1U : 0U;
				if (Stop != nullptr && StopAfterWrites != 0U && Writes == StopAfterWrites) {
					Stop->request_stop();
				}
			}
			Log << static_cast<int>(wire.Operation) << ' ' << static_cast<int>(wire.Method) << ' ' << wire.Path;
			if (const auto* success = std::get_if<github_mutation_wire_success>(&result)) {
				Log << ' ' << success->Response.Status;
				for (const auto& header : success->Response.Headers) {
					auto name = header.Name;
					std::ranges::transform(name, name.begin(), [](unsigned char aCharacter) { return static_cast<char>(std::tolower(aCharacter)); });
					if (name == "x-ratelimit-remaining") {
						Log << " remaining=" << header.Value;
						Lowest    = std::min(Lowest, std::stol(header.Value));
						Exhausted = Exhausted || Lowest < Floor;
					}
				}
				if (wire.Operation == review_mutation_operation::staged_update_pending_comment) {
					++PendingEdits;
					// The production parser holds GitHub's own answer to the
					// edit to what the contract expects of it.
					if (parse_staged_review_response(success->Response, std::get<application::staged_review_request>(wire.Semantic))) {
						++ParsedEdits;
					} else {
						Log << " unparsed:" << success->Response.Body;
					}
				}
			} else if (const auto* error = std::get_if<github_mutation_wire_error>(&result)) {
				Log << " error http=" << error->HttpStatus;
			} else {
				Log << " cancelled";
			}
			Log << '\n';
			Log.flush();
			return result;
		}
	};
	void live_tour(const std::filesystem::path& aFolder)
	{
		const live_state state{ aFolder };
		const auto       profile = adapters::github::validate_github_host_profile({
			.Profile        = credential_context().Authority.Profile,
			.Host           = target().Semantic.Host,
			.Revision       = credential_context().Authority.ProfileRevision,
			.Server         = application::github_server_contract::github_com_2022_11_28,
			.Authentication = application::github_authentication_transport::github_cli,
			.Proxy          = application::github_proxy_mode::system,
			.Trust          = application::github_trust_mode::system,
			.Provenance     = adapters::github::github_profile_provenance::builtin,
			.Hostname       = "github.com",
			.RestRoot       = "https://api.github.com",
			.GraphqlRoot    = "https://api.github.com/graphql",
		});
		expect(profile.has_value(), "the live gh profile validates");
		std::filesystem::create_directory(aFolder / "neutral");
		const auto configuration = validate_gh_cli_configuration({ .Executable = state.text("gh"), .NeutralWorkingDirectory = aFolder / "neutral", .Platform = gh_cli_macos_environment_facts{ .HomeDirectory = state.text("home") }, .Https = { } });
		expect(configuration.has_value(), "the live gh configuration validates");
		auto             process = make_github_cli_process_runner();
		gh_cli_transport transport{ *profile, *configuration, *process };
		live_relay       relay{ transport, aFolder };

		auto binding                     = target();
		binding.Number                   = core::pull_request_number{ static_cast<std::uint32_t>(state.number("number")) };
		binding.Semantic.ExpectedHead    = *core::parse_git_commit_oid(state.text("head"));
		binding.BaseOid                  = *core::parse_git_commit_oid(state.text("base"));
		binding.HeadRef                  = state.text("head_ref");
		binding.BaseRef                  = state.text("base_ref");
		binding.RepositoryDatabaseId     = state.number("repo_id");
		binding.BaseRepositoryDatabaseId = state.number("repo_id");
		binding.RepositoryNodeId         = state.text("repo_node");
		binding.PullRequestDatabaseId    = state.number("pr_id");
		binding.PullRequestNodeId        = state.text("pr_node");
		auto actor                       = identity();
		actor.DatabaseId                 = state.number("user_id");
		actor.NodeId                     = state.text("user_node");
		actor.Login                      = state.text("login");
		live_review_mutation_configuration live{ .CredentialContext = credential_context(), .Target = binding, .ActingIdentity = actor };

		auto input                       = plan();
		input.Submission.LiveBinding     = application::review_submission_live_binding{ .Target = binding, .ActingIdentity = actor };
		input.Submission.Proposal.Target = binding.Semantic;
		input.Submission.Proposal.Body   = "BPR-1368 live check: a two-stop native tour, both stops edited while pending.";
		input.Submission.IdempotencyKey  = "bpr-1368-live-tour";
		input.Description.reset();
		const auto path           = state.text("path");
		const auto blob           = "https://github.com/MikeMarcin/BPR-Test/blob/";
		input.Stops[0].Source     = { .Commit = binding.Semantic.ExpectedHead, .Path = path, .Side = core::diff_side::right, .FirstLine = 8, .LastLine = 10 };
		input.Stops[0].Title      = "Added lines";
		input.Stops[0].Narrative  = "Three lines the head adds.";
		input.Stops[0].SourceLink = blob + state.text("head") + "/" + path + "#L9-L11";
		input.Stops[1].Source     = { .Commit = binding.BaseOid, .Path = path, .Side = core::diff_side::left, .FirstLine = 4, .LastLine = 4 };
		input.Stops[1].Title      = "Removed line";
		input.Stops[1].Narrative  = "The base line the head removes.";
		input.Stops[1].SourceLink = blob + state.text("base") + "/" + path + "#L5";
		expect(application::valid(input), "the live tour plan is valid");

		auto opened = adapters::sqlite::make_review_submission_recovery_store({ .StorageRoot = aFolder, .Host = "github.com", .Account = "21", .Repository = "MikeMarcin/BPR-Test", .PullRequest = static_cast<std::int64_t>(state.number("number")) });
		expect(opened.has_value(), "the live tour journal opens");
		review_submission_session session{ relay, live, std::make_shared<lease>(), std::move(*opened) };
		const auto                result = session.publish_tour(input, { }, [](const auto&) { return true; });

		std::ofstream out{ aFolder / "result.env" };
		out << "state=" << static_cast<int>(result.State) << "\ncalls=" << relay.Calls << "\nlowest=" << relay.Lowest << "\npending_edits=" << relay.PendingEdits << "\nparsed_edits=" << relay.ParsedEdits << "\nrest_pending_edits=" << relay.RestPendingEdits << '\n';
		if (result.Record && result.Record->Review) {
			out << "review=" << result.Record->Review->Id.value() << '\n';
			for (std::size_t i = 0; i < result.Record->Comments.size(); ++i) {
				out << "comment" << i << '=' << result.Record->Comments[i].Id << '\n';
			}
			const auto bodies = application::staged_final_bodies(*result.Record);
			if (bodies) {
				out << "summary_sha=" << std::hash<std::string>{ }(bodies->first) << '\n';
				for (std::size_t i = 0; i < bodies->second.size(); ++i) {
					std::ofstream body{ aFolder / ("stop" + std::to_string(i) + ".md"), std::ios::binary };
					body << bodies->second[i];
				}
				std::ofstream summary{ aFolder / "summary.md", std::ios::binary };
				summary << bodies->first;
			}
		}
		out.flush();
		expect(result.State == publication_state::published, "the live two-stop tour is published");
		expect(relay.PendingEdits == 2 && relay.ParsedEdits == 2 && relay.RestPendingEdits == 0, "both stops were edited by GraphQL while pending, and GitHub's answers parse");
		std::cout << "live-tour:pass\n";
	}
	// [[BPR-1222]]. The two-account tour course on the owner's designated
	// target: MikeMarcin authors and publishes a three-stop tour with the
	// production session over the production `gh api` transport, the test
	// account MikeMarcinTest reads and replies between phases (outside this
	// program), then a reorder update is stopped after its first write in one
	// process and resumed in the next. Run by hand, one phase per process:
	// `--live-tour-course DIR publish-pending|publish|update|resume|observe`. DIR/state.env
	// names the pull request and DIR/description.md holds its exact body. Not
	// registered as a row: it writes to GitHub, and no request leaves once an
	// answer reports fewer than 1500 remaining.
	auto live_binding(const live_state& aState) -> application::review_mutation_target_binding
	{
		auto binding                     = target();
		binding.Number                   = core::pull_request_number{ static_cast<std::uint32_t>(aState.number("number")) };
		binding.Semantic.ExpectedHead    = *core::parse_git_commit_oid(aState.text("head"));
		binding.BaseOid                  = *core::parse_git_commit_oid(aState.text("base"));
		binding.HeadRef                  = aState.text("head_ref");
		binding.BaseRef                  = aState.text("base_ref");
		binding.RepositoryDatabaseId     = aState.number("repo_id");
		binding.BaseRepositoryDatabaseId = aState.number("repo_id");
		binding.RepositoryNodeId         = aState.text("repo_node");
		binding.PullRequestDatabaseId    = aState.number("pr_id");
		binding.PullRequestNodeId        = aState.text("pr_node");
		return binding;
	}
	auto live_actor(const live_state& aState) -> application::review_mutation_acting_identity
	{
		auto actor       = identity();
		actor.DatabaseId = aState.number("user_id");
		actor.NodeId     = aState.text("user_node");
		actor.Login      = aState.text("login");
		return actor;
	}
	struct live_gh final
	{
		std::unique_ptr<github_cli_process_port> Process;
		std::optional<gh_cli_transport>          Transport;
		explicit live_gh(const live_state& aState, const std::filesystem::path& aFolder)
		{
			const auto profile = adapters::github::validate_github_host_profile({
				.Profile        = credential_context().Authority.Profile,
				.Host           = target().Semantic.Host,
				.Revision       = credential_context().Authority.ProfileRevision,
				.Server         = application::github_server_contract::github_com_2022_11_28,
				.Authentication = application::github_authentication_transport::github_cli,
				.Proxy          = application::github_proxy_mode::system,
				.Trust          = application::github_trust_mode::system,
				.Provenance     = adapters::github::github_profile_provenance::builtin,
				.Hostname       = "github.com",
				.RestRoot       = "https://api.github.com",
				.GraphqlRoot    = "https://api.github.com/graphql",
			});
			expect(profile.has_value(), "the live gh profile validates");
			std::filesystem::create_directories(aFolder / "neutral");
			const auto configuration = validate_gh_cli_configuration({ .Executable = aState.text("gh"), .NeutralWorkingDirectory = aFolder / "neutral", .Platform = gh_cli_macos_environment_facts{ .HomeDirectory = aState.text("home") }, .Https = { } });
			expect(configuration.has_value(), "the live gh configuration validates");
			Process = make_github_cli_process_runner();
			Transport.emplace(*profile, *configuration, *Process);
		}
	};
	// [[BPR-0144]]. Comment, Request changes and an empty Approve on a pull
	// request another account authored, each through the production session
	// the desktop's whole-review provider forwards to, over the production
	// `gh api` transport as the account `gh` has active. Run by hand:
	// `--live-review-identity DIR`; DIR/state.env names the pull request and
	// the acting (reviewing) account. Writes DIR/identity.env. Not a row.
	void live_review_identity(const std::filesystem::path& aFolder)
	{
		const live_state state{ aFolder };
		live_gh          gh{ state, aFolder };
		live_relay       relay{ *gh.Transport, aFolder };
		relay.Floor                                      = 1500;
		const auto                               binding = live_binding(state);
		const auto                               actor   = live_actor(state);
		const live_review_mutation_configuration live{ .CredentialContext = credential_context(), .Target = binding, .ActingIdentity = actor };
		auto                                     opened = adapters::sqlite::make_review_submission_recovery_store({ .StorageRoot = aFolder, .Host = "github.com", .Account = "21", .Repository = "MikeMarcin/BPR-Test", .PullRequest = static_cast<std::int64_t>(state.number("number")) });
		expect(opened.has_value(), "the identity journal opens");
		review_submission_session                                                           session{ relay, live, std::make_shared<lease>(), std::move(*opened) };
		const std::array<std::tuple<core::review_disposition, const char*, const char*>, 3> course{ {
			{ core::review_disposition::comment, "COMMENT", "BPR-0144 live check: Comment by the signed-in account." },
			{ core::review_disposition::request_changes, "REQUEST_CHANGES", "BPR-0144 live check: Request changes by the signed-in account." },
			{ core::review_disposition::approve, "APPROVE", "" },
		} };
		std::ofstream                                                                       out{ aFolder / "identity.env" };
		std::uint64_t                                                                       ordinal = 0;
		for (const auto& [disposition, name, body] : course) {
			++ordinal;
			const application::review_submission_request request{ .Proposal = { .Draft = core::review_submission_draft_id{ 100U + ordinal }, .Attempt = core::review_submission_attempt_id{ ordinal }, .Target = binding.Semantic, .ActingAccount = actor.Account, .Body = body, .Disposition = disposition }, .IdempotencyKey = std::string{ "bpr-2acct-0144-" } + name, .LiveBinding = application::review_submission_live_binding{ .Target = binding, .ActingIdentity = actor } };
			expect(application::valid(request), "the identity request is valid");
			const auto result = session.submit_and_reconcile(request);
			out << name << ".publication=" << static_cast<int>(result.Publication) << '\n'
				<< name << ".failure=" << static_cast<int>(result.Failure) << '\n';
			if (result.Receipt) {
				out << name << ".review=" << result.Receipt->ReviewDatabaseId.value() << '\n'
					<< name << ".node=" << result.Receipt->ReviewNodeId << '\n'
					<< name << ".disposition=" << static_cast<int>(result.Receipt->Disposition) << '\n';
			}
			out.flush();
			expect(result.Publication == application::review_submission_publication::published && result.Receipt && result.Receipt->Disposition == disposition, "the review is published with its exact disposition");
		}
		out << "calls=" << relay.Calls << "\nwrites=" << relay.Writes << "\nlowest=" << relay.Lowest << '\n';
		std::cout << "live-review-identity:pass\n";
	}
	constexpr auto course_key        = "bpr-2acct-1222-tour";
	constexpr auto course_update_key = "bpr-2acct-1222-update";
	auto           course_plan(const live_state& aState, const application::review_mutation_target_binding& aBinding, const application::review_mutation_acting_identity& aActor) -> application::staged_review_plan
	{
		auto input                       = plan();
		input.Submission.LiveBinding     = application::review_submission_live_binding{ .Target = aBinding, .ActingIdentity = aActor };
		input.Submission.Proposal.Target = aBinding.Semantic;
		input.Submission.Proposal.Body   = "BPR-1222 two-account course: a three-stop tour for the website-only reviewer MikeMarcinTest.";
		input.Submission.IdempotencyKey  = course_key;
		const auto path                  = aState.text("path");
		const auto head                  = aState.text("head");
		const auto blob                  = "https://github.com/MikeMarcin/BPR-Test/blob/" + head + "/" + path;
		const auto stop                  = [&](std::uint64_t aId, std::uint32_t aFirst, std::uint32_t aLast, std::string aTitle, std::string aNarrative) {
			return application::staged_tour_stop{ core::tour_stop_id{ aId }, { .Commit = aBinding.Semantic.ExpectedHead, .Path = path, .Side = core::diff_side::right, .FirstLine = aFirst - 1U, .LastLine = aLast - 1U }, std::move(aTitle), std::move(aNarrative), blob + "#L" + std::to_string(aFirst) + "-L" + std::to_string(aLast) };
		};
		input.Stops = {
			stop(1, 4, 9, "One header", "A header line without a colon is refused rather than guessed."),
			stop(2, 12, 20, "Repeated headers", "Repeated headers are kept as lists, so a conflict can be seen."),
			stop(3, 23, 31, "Declared length", "One declared length, and never a negative one."),
		};
		std::ifstream     description{ std::filesystem::path{ aState.text("folder") } / "description.md", std::ios::binary };
		const std::string baseline{ std::istreambuf_iterator<char>{ description }, std::istreambuf_iterator<char>{ } };
		expect(!baseline.empty(), "the pull request's exact description is read");
		input.Description = application::staged_description_edit{ baseline, baseline, true };
		expect(application::valid(input), "the course plan is valid");
		return input;
	}
	auto course_update(const application::staged_review_plan& aPlan, const application::staged_publication_record& aPublished) -> application::staged_review_plan
	{
		const auto&       binding = aPlan.Submission.LiveBinding->Target;
		core::review_tour draft{ .Id = core::tour_draft_id{ 41 }, .Owner = aPlan.Owner, .Base = binding.BaseOid, .Head = binding.Semantic.ExpectedHead, .Problem = "Header parsing accepted conflicting and negative lengths.", .Solution = "Keep repeated headers as lists and refuse a conflict or a negative length.", .Revision = aPlan.Generation, .LastStopId = 3 };
		for (const auto& stop : aPlan.Stops) {
			draft.Stops.push_back({ .Id = stop.Id, .Title = stop.Title, .Body = stop.Narrative, .Destination = stop.Source });
		}
		auto published = application::published_tour(aPublished, draft);
		expect(published.has_value(), "the published tour maps onto the course draft");
		draft = *published;
		// The reorder: the last stop moves to the front.
		std::rotate(draft.Stops.begin(), draft.Stops.end() - 1, draft.Stops.end());
		++draft.Revision;
		auto result                        = aPlan;
		result.Submission.IdempotencyKey   = course_update_key;
		result.Submission.Proposal.Attempt = core::review_submission_attempt_id{ 8 };
		result.Generation                  = draft.Revision;
		result.Stops.clear();
		result.Description.reset();
		result.Update = draft;
		expect(application::valid(result), "the reorder update keeps the exact root and stop identities");
		return result;
	}
	void write_course_record(const std::filesystem::path& aFile, const std::string& aPhase, const application::staged_publication_result& aResult, const live_relay& aRelay)
	{
		std::ofstream out{ aFile };
		out << "phase=" << aPhase << "\nstate=" << static_cast<int>(aResult.State) << "\ncalls=" << aRelay.Calls << "\nwrites=" << aRelay.Writes << "\nposts=" << aRelay.Posts << "\npatches=" << aRelay.Patches << "\nlowest=" << aRelay.Lowest << '\n';
		if (aResult.Record) {
			const auto& record = *aResult.Record;
			out << "published=" << record.Published << "\npatched=" << record.Patched << "\nupdated=" << record.Updated << "\ndescription_done=" << record.DescriptionDone << '\n';
			if (record.Review) {
				out << "review=" << record.Review->Id.value() << '\n';
			}
			if (record.Index) {
				out << "index=" << record.Index->Id << ' ' << record.Index->WebUrl << '\n';
			}
			for (std::size_t i = 0; i < record.Comments.size(); ++i) {
				out << "comment" << i << '=' << record.Comments[i].Id << ' ' << record.Comments[i].WebUrl << '\n';
			}
		}
		out.flush();
		expect(out.good(), "the course record is written");
	}
	void live_tour_course(const std::filesystem::path& aFolder, const std::string& aPhase)
	{
		auto state             = live_state{ aFolder };
		state.Values["folder"] = aFolder.string();
		live_gh gh{ state, aFolder };
		std::filesystem::create_directories(aFolder / aPhase);
		live_relay relay{ *gh.Transport, aFolder / aPhase };
		relay.Floor                                      = 1500;
		const auto                               binding = live_binding(state);
		const auto                               actor   = live_actor(state);
		const live_review_mutation_configuration live{ .CredentialContext = credential_context(), .Target = binding, .ActingIdentity = actor };
		const auto                               initial   = course_plan(state, binding, actor);
		const auto                               openStore = [&] {
			auto opened = adapters::sqlite::make_review_submission_recovery_store({ .StorageRoot = aFolder, .Host = "github.com", .Account = "21", .Repository = "MikeMarcin/BPR-Test", .PullRequest = static_cast<std::int64_t>(state.number("number")) });
			expect(opened.has_value(), "the course journal opens");
			return std::move(*opened);
		};
		const auto authorize = [](const auto&) { return true; };
		if (aPhase == "publish-pending") {
			// Stop once the pending review holds every stop's final body, before
			// Submit, so the reviewer account can look while it is pending.
			review_submission_session session{ relay, live, std::make_shared<lease>(), openStore() };
			std::stop_source          stop;
			relay.Stop            = &stop;
			relay.StopAfterWrites = 1U + initial.Stops.size();
			const auto result     = session.publish_tour(initial, stop.get_token(), authorize);
			write_course_record(aFolder / "publish-pending.env", aPhase, result, relay);
			expect(result.State != publication_state::published && relay.Writes == relay.StopAfterWrites && result.Record && result.Record->Review && !result.Record->Published, "Stop leaves the review pending with every stop edited");
			std::cout << "live-tour-course:publish-pending:pass\n";
			return;
		}
		if (aPhase == "publish") {
			review_submission_session session{ relay, live, std::make_shared<lease>(), openStore() };
			const auto                result = session.publish_tour(initial, { }, authorize);
			write_course_record(aFolder / "publish.env", aPhase, result, relay);
			if (const auto bodies = result.Record ? application::staged_final_bodies(*result.Record) : std::nullopt) {
				std::ofstream{ aFolder / "publish-summary.md", std::ios::binary } << bodies->first;
				for (std::size_t i = 0; i < bodies->second.size(); ++i) {
					std::ofstream{ aFolder / ("publish-stop" + std::to_string(i) + ".md"), std::ios::binary } << bodies->second[i];
				}
			}
			expect(result.State == publication_state::published, "the three-stop tour is published");
			std::cout << "live-tour-course:publish:pass\n";
			return;
		}
		if (aPhase == "update") {
			review_submission_session session{ relay, live, std::make_shared<lease>(), openStore() };
			const auto                observed = session.reconcile_tour(initial);
			expect(observed.State == publication_state::published && observed.Record && relay.Writes == 0U, "the published tour is read back without a write");
			const auto       update = course_update(initial, *observed.Record);
			std::stop_source stop;
			relay.Stop            = &stop;
			relay.StopAfterWrites = 1U;
			const auto result     = session.publish_tour(update, stop.get_token(), authorize);
			write_course_record(aFolder / "update.env", aPhase, result, relay);
			expect(result.State != publication_state::published && relay.Writes == 1U, "Stop after the first update write leaves the rest unsent");
			std::cout << "live-tour-course:update-stopped:pass\n";
			return;
		}
		if (aPhase == "resume" || aPhase == "observe") {
			auto saved = std::optional<application::staged_review_plan>{ };
			{
				auto store  = openStore();
				auto record = store->load_staged(course_update_key);
				expect(record.has_value() && record->has_value(), "the stopped update's plan is in the journal");
				saved = (**record).Plan;
			}
			review_submission_session session{ relay, live, std::make_shared<lease>(), openStore() };
			const auto                read = session.reconcile_tour(*saved);
			write_course_record(aFolder / (aPhase + "-read.env"), aPhase, read, relay);
			expect(relay.Writes == 0U, "restart reconciles by reads alone");
			if (aPhase == "observe") {
				std::cout << "live-tour-course:observe:pass\n";
				return;
			}
			const auto result = session.publish_tour(*saved, { }, authorize);
			write_course_record(aFolder / "resume.env", aPhase, result, relay);
			if (const auto bodies = result.Record ? application::staged_final_bodies(*result.Record) : std::nullopt) {
				std::ofstream{ aFolder / "resume-summary.md", std::ios::binary } << bodies->first;
				for (std::size_t i = 0; i < bodies->second.size(); ++i) {
					std::ofstream{ aFolder / ("resume-stop" + std::to_string(i) + ".md"), std::ios::binary } << bodies->second[i];
				}
			}
			expect(result.State == publication_state::published && relay.Posts == 0U, "Resume finishes the update with edits only: no review is created or submitted");
			std::cout << "live-tour-course:resume:pass\n";
			return;
		}
		expect(false, "the phase is publish-pending, publish, update, resume or observe");
	}
#endif
	void checkpoint(const std::filesystem::path& aRoot, const std::string& aPhase)
	{
		{
			std::ofstream ready{ aRoot / "kill-ready" };
			ready << aPhase;
			ready.flush();
			expect(ready.good(), "kill marker durable");
		}
		char input{ };
		std::cin.get(input);
		expect(false, "child must be forcibly killed");
	}
}
auto main(int argc, char** argv) -> int
try {
	QCoreApplication application{ argc, argv };
#if defined(__APPLE__)
	if (std::filesystem::path{ argv[0] }.filename() == "gh") {
		return tests::strict_cli_child(QCoreApplication::arguments());
	}
	if (argc == 2 && std::string_view{ argv[1] } == "--strict-cli-fixture") {
		test_strict_tour_scope();
		std::cout << "strict-tour-scope:pass\n";
		return EXIT_SUCCESS;
	}
#endif
	if (argc == 3 && std::string_view{ argv[1] } == "--live-review-identity") {
#if defined(__APPLE__)
		live_review_identity(argv[2]);
		return EXIT_SUCCESS;
#else
		expect(false, "the live identity course runs on macOS");
#endif
	}
	if (argc == 4 && std::string_view{ argv[1] } == "--live-tour-course") {
#if defined(__APPLE__)
		live_tour_course(argv[2], argv[3]);
		return EXIT_SUCCESS;
#else
		expect(false, "the live tour course runs on macOS");
#endif
	}
	if (argc == 3 && std::string_view{ argv[1] } == "--live-tour") {
#if defined(__APPLE__)
		live_tour(argv[2]);
		return EXIT_SUCCESS;
#else
		expect(false, "the live tour runs on macOS");
#endif
	}
	if (argc == 3) {
		const std::string           phase = argv[1];
		const std::filesystem::path root  = argv[2];
		server                      remote;
		remote.Root = root;
		remote.load();
		auto provider = session(remote, root);
		if (phase == "--init-update") {
			expect(provider->publish_tour(plan(), { }, [](const auto&) { return true; }).State == publication_state::published, "initialize published tour");
		} else if (phase.starts_with("--update-")) {
			remote.Maintenance  = true;
			const auto baseline = storage(root)->load_staged(plan().Submission.IdempotencyKey);
			expect(baseline && *baseline, "original durable publication is retained");
			const auto update = maintenance_plan(**baseline);
			if (phase.starts_with("--update-kill-")) {
				remote.Boundary = [&](std::size_t i, std::string_view boundary) {if(phase=="--update-kill-"+std::to_string(i)+"-"+std::string{boundary}){checkpoint(root,phase);} };
				static_cast<void>(provider->publish_tour(update, { }, [](const auto&) { return true; }));
				expect(false, "maintenance kill boundary reached");
			} else if (phase == "--update-recover") {
				expect(provider->reconcile_tour(update).Record.has_value() && remote.Writes == 0, "maintenance cold recovery is read-only");
			} else if (phase == "--update-resume") {
				expect(provider->publish_tour(update, { }, [](const auto&) { return true; }).State == publication_state::published, "maintenance cold Resume completes remaining operations");
			}
		} else if (phase.starts_with("--kill-")) {
			remote.Boundary = [&](std::size_t i, std::string_view boundary) {if(phase=="--kill-"+std::to_string(i)+"-"+std::string{boundary}){checkpoint(root,phase);
} };
			static_cast<void>(provider->publish_tour(plan(), { }, [](const auto&) { return true; }));
			expect(false, "kill boundary reached");
		} else if (phase == "--recover") {
			const auto observed = provider->reconcile_tour(plan());
			expect(observed.Record.has_value() && remote.Writes == 0, "fresh-process recovery performs reads only");
		} else if (phase == "--resume") {
			expect(provider->publish_tour(plan(), { }, [](const auto&) { return true; }).State == publication_state::published, "explicit fresh-process Resume finishes known remaining effects");
		} else {
			expect(false, "known child phase");
		}
		std::cout << phase << ":pass\n";
	} else {
		test_staged_sessions();
		test_compatibility_sessions();
		test_maintenance_sessions();
		std::cout << "staged-review-session-tests:pass\n";
	}
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
