#include "review_submission_fixture.hpp"

#include "review_submission_session.hpp"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <chrono>
#include <iterator>
#include <string>

namespace bpr::app::detail {
	namespace {
		/// [[BPR-1345]]. The production review session over this fixture's
		/// server, journalling in the provider's own store.
		[[nodiscard]] auto make_production_review(std::shared_ptr<review_submission_fixture_state>, application::review_submission_recovery_port&, application::review_submission_live_binding)
			-> std::unique_ptr<application::review_submission_port>;

		class fixture_provider final : public application::review_submission_port
		{
		public:
			fixture_provider(std::shared_ptr<review_submission_fixture_state>              aState,
							 std::unique_ptr<application::review_submission_recovery_port> aStore,
							 application::review_submission_live_binding                   aBinding)
				: State{ std::move(aState) }
				, Store{ std::move(aStore) }
				, Binding{ std::move(aBinding) }
				, Production{ make_production_review(State, *Store, Binding) }
			{ }
			auto last_unknown(const std::string& aKey, std::stop_token aStop) -> std::optional<core::review_submission_unknown_diagnosis> override
			{
				return Production->last_unknown(aKey, aStop);
			}
			auto admitted(const std::string& aKey, std::stop_token aStop) -> std::expected<std::optional<application::review_submission_request>, application::review_mutation_failure> override
			{
				return Production->admitted(aKey, aStop);
			}
			auto pending(std::stop_token aStop) -> application::review_submission_discovery override
			{
				if (aStop.stop_requested()) {
					return std::unexpected(application::review_mutation_failure::cancelled);
				}
				auto records = Store->pending();
				if (!records) {
					return std::unexpected(application::review_mutation_failure::unavailable);
				}
				std::vector<application::review_submission_request> result;
				for (const auto& record : *records) {
					result.push_back(record.Request);
				}
				return result;
			}
			auto submit_and_reconcile(const application::review_submission_request& aRequest, std::stop_token aStop) -> application::review_submission_result override
			{
				// [[BPR-1345]]. A review with line comments: the exact production
				// request, journal and reconciliation, against this server.
				if (State->CliStrictCourse || !aRequest.Proposal.Comments.empty()) {
					return Production->submit_and_reconcile(aRequest, aStop);
				}
				if (aStop.stop_requested() || !aRequest.LiveBinding || *aRequest.LiveBinding != Binding) {
					return { .Request = aRequest, .Publication = application::review_submission_publication::definitely_not_published };
				}
				std::scoped_lock lock{ State->Mutex };
				if (State->RefuseBeforeSend) {
					return { .Request = aRequest, .Publication = application::review_submission_publication::definitely_not_published };
				}
				auto known = Store->load(aRequest.IdempotencyKey);
				if (!known) {
					return { .Request = aRequest };
				}
				if (*known) {
					return reconcile_locked(aRequest);
				}
				std::vector<application::review_database_id> baseline;
				for (std::size_t index = 0; index < State->Reviews.size(); ++index) {
					baseline.emplace_back(index + 1U);
				}
				auto record = application::review_submission_recovery_record{
					.Request              = aRequest,
					.RequestFingerprint   = application::review_submission_request_fingerprint(aRequest),
					.AuthorityFingerprint = application::review_submission_authority_fingerprint(*aRequest.LiveBinding),
					.BaselineReviewIds    = std::move(baseline),
					.Transmission         = application::review_mutation_recovery_transmission::prepared
				};
				auto admitted = Store->admit(record);
				if (!admitted) {
					return { .Request = aRequest };
				}
				if (!admitted->Inserted) {
					return reconcile_locked(aRequest);
				}
				if (!Store->mark_possibly_transmitted(aRequest.IdempotencyKey, record.RequestFingerprint)) {
					return { .Request = aRequest };
				}
				State->Reviews.push_back(aRequest);
				++State->Submissions;
				if (State->LoseNextResponse) {
					State->LoseNextResponse = false;
					return { .Request = aRequest };
				}
				auto result = reconcile_locked(aRequest);
				if (State->StaleNextResult) {
					State->StaleNextResult          = false;
					result.Request.Proposal.Attempt = core::review_submission_attempt_id{ aRequest.Proposal.Attempt.value() + 1U };
				}
				return result;
			}
			auto reconcile(const application::review_submission_request& aRequest, std::stop_token aStop) -> application::review_submission_result override
			{
				if (State->CliStrictCourse || !aRequest.Proposal.Comments.empty()) {
					{
						std::scoped_lock lock{ State->Mutex };
						++State->Reconciliations;
					}
					return Production->reconcile(aRequest, aStop);
				}
				if (aStop.stop_requested()) {
					return { .Request = aRequest };
				}
				std::scoped_lock lock{ State->Mutex };
				++State->Reconciliations;
				return reconcile_locked(aRequest);
			}

		private:
			auto reconcile_locked(const application::review_submission_request& aRequest) -> application::review_submission_result
			{
				auto loaded = Store->load(aRequest.IdempotencyKey);
				if (!loaded || !*loaded || (**loaded).Request != aRequest || !aRequest.LiveBinding || !application::same_review_submission_identity(*aRequest.LiveBinding, Binding)) {
					return { .Request = aRequest };
				}
				const auto& record = **loaded;
				if (record.Receipt) {
					return { .Request = aRequest, .Publication = application::review_submission_publication::published, .Receipt = record.Receipt };
				}
				if (record.Transmission != application::review_mutation_recovery_transmission::possibly_transmitted) {
					return { .Request = aRequest };
				}
				const auto found = std::ranges::find(State->Reviews, aRequest);
				if (found == State->Reviews.end()) {
					return { .Request = aRequest };
				}
				const auto id      = static_cast<std::uint64_t>(std::distance(State->Reviews.begin(), found)) + 1U;
				auto       receipt = application::review_submission_receipt{
					.ReviewDatabaseId = application::review_database_id{ id }, .ReviewNodeId = "PRR_fixture_" + std::to_string(id), .Target = aRequest.Proposal.Target, .ActingAccount = aRequest.Proposal.ActingAccount, .Disposition = aRequest.Proposal.Disposition
				};
				auto resolved = Store->resolve(aRequest.IdempotencyKey, record.RequestFingerprint, receipt);
				return resolved ? application::review_submission_result{ .Request = aRequest, .Publication = application::review_submission_publication::published, .Receipt = receipt } : application::review_submission_result{ .Request = aRequest };
			}
			std::shared_ptr<review_submission_fixture_state>              State;
			std::unique_ptr<application::review_submission_recovery_port> Store;
			application::review_submission_live_binding                   Binding;
			std::unique_ptr<application::review_submission_port>          Production;
		};
	}
	auto make_review_submission_fixture_provider(std::shared_ptr<review_submission_fixture_state>              aState,
												 std::unique_ptr<application::review_submission_recovery_port> aStore,
												 application::review_submission_live_binding                   aBinding) -> std::unique_ptr<application::review_submission_port>
	{
		return std::make_unique<fixture_provider>(std::move(aState), std::move(aStore), std::move(aBinding));
	}
}

namespace bpr::app::detail {
	namespace {
		namespace wire = adapters::github::detail;
		auto json_text(const std::string& aText) -> std::string
		{
			const auto json = QJsonDocument{ QJsonArray{ QString::fromStdString(aText) } }.toJson(QJsonDocument::Compact).toStdString();
			return json.substr(1, json.size() - 2);
		}
		struct staged_fixture_lease final : application::review_mutation_transport_lease
		{ };
		class staged_fixture_wire final : public wire::github_mutation_wire_transport
		{
		public:
			std::shared_ptr<review_submission_fixture_state> State;
			application::review_submission_live_binding      Binding;
			std::string                                      Description;
			staged_fixture_wire(std::shared_ptr<review_submission_fixture_state> aState, application::review_submission_live_binding aBinding, std::string aDescription)
				: State{ std::move(aState) }
				, Binding{ std::move(aBinding) }
				, Description{ std::move(aDescription) }
			{ }
			auto begin_mutation_operation(std::stop_token aStop) -> wire::github_mutation_operation override
			{
				std::scoped_lock lock{ State->Mutex };
				if (!State->CliStrictCourse) {
					return std::shared_ptr<wire::github_mutation_wire_transport>{ };
				}
				++State->CliAcquisitions;
				if (aStop.stop_requested()) {
					return std::unexpected(application::review_mutation_failure::cancelled);
				}
				if (State->CliAcquisitionFailure) {
					return std::unexpected(*State->CliAcquisitionFailure);
				}
				return std::shared_ptr<wire::github_mutation_wire_transport>{ std::make_shared<staged_fixture_wire>(State, Binding, Description) };
			}
			auto actor() const -> application::staged_review_actor
			{
				const auto& a = Binding.ActingIdentity;
				return { a.DatabaseId, a.NodeId, a.Login };
			}
			auto root() const -> std::string
			{
				const auto& t = Binding.Target;
				return "https://" + std::string{ t.Hostname.value() } + "/" + t.Repository.Owner + "/" + t.Repository.Name + "/pull/" + std::to_string(t.Number.value());
			}
			auto repository() const -> std::string { return std::string{ Binding.Target.ServerContract == application::github_server_contract::github_com_2022_11_28 ? "/repos/" : "/api/v3/repos/" } + Binding.Target.Repository.Owner + "/" + Binding.Target.Repository.Name; }
			auto api() const -> std::string { return Binding.Target.ServerContract == application::github_server_contract::github_com_2022_11_28 ? "https://api.github.com" : "https://" + std::string{ Binding.Target.Hostname.value() }; }
			auto pull() const -> std::string { return repository() + "/pulls/" + std::to_string(Binding.Target.Number.value()); }
			auto graphql() const -> std::string { return Binding.Target.ServerContract == application::github_server_contract::github_com_2022_11_28 ? "/graphql" : "/api/graphql"; }
			/// [[BPR-1368]]. A comment of the review while it is pending, which
			/// GitHub's REST `/pulls/comments/{id}` does not find, to read or edit.
			auto pending_comment(const std::string& aPath) const -> bool
			{
				return State->StagedReview && State->StagedReview->State == application::staged_review_state::pending
					&& std::ranges::any_of(State->StagedComments, [&](const application::staged_review_comment& aComment) { return aPath == repository() + "/pulls/comments/" + std::to_string(aComment.Id); });
			}
			static auto not_found() -> wire::github_mutation_wire_completion
			{
				return wire::github_mutation_wire_error{ .Failure = application::review_mutation_failure::unavailable, .Transmission = wire::github_mutation_transmission::indeterminate, .HttpStatus = 404U };
			}
			/// [[BPR-1368]]. GitHub's answer to the review comment edit.
			auto edit_json(const application::staged_review_comment& aComment) const -> std::string
			{
				const auto& t       = Binding.Target;
				const auto  pending = State->StagedReview && State->StagedReview->Id == aComment.Review && State->StagedReview->State == application::staged_review_state::pending;
				const auto  a       = actor();
				return "{\"data\":{\"updatePullRequestReviewComment\":{\"pullRequestReviewComment\":{\"id\":" + json_text(aComment.NodeId) + ",\"fullDatabaseId\":\"" + std::to_string(aComment.Id) + "\",\"url\":" + json_text(aComment.WebUrl) + ",\"body\":" + json_text(aComment.Body) + ",\"state\":\"" + (pending ? "PENDING" : "SUBMITTED") + "\",\"viewerDidAuthor\":true,\"author\":{\"login\":" + json_text(a.Login) + ",\"databaseId\":" + std::to_string(a.DatabaseId) + ",\"id\":" + json_text(a.NodeId) + "},\"pullRequestReview\":{\"id\":\"PRR_" + std::to_string(aComment.Review.value()) + "\",\"fullDatabaseId\":\"" + std::to_string(aComment.Review.value()) + "\"},\"pullRequest\":{\"id\":" + json_text(t.PullRequestNodeId) + ",\"databaseId\":" + std::to_string(t.PullRequestDatabaseId) + ",\"number\":" + std::to_string(t.Number.value()) + ",\"repository\":{\"id\":" + json_text(t.RepositoryNodeId) + ",\"databaseId\":" + std::to_string(t.RepositoryDatabaseId) + ",\"nameWithOwner\":" + json_text(t.Repository.Owner + "/" + t.Repository.Name) + "}}}}}}";
			}
			auto user_json() const -> std::string
			{
				const auto a = actor();
				return "{\"id\":" + std::to_string(a.DatabaseId) + ",\"node_id\":" + json_text(a.NodeId) + ",\"login\":" + json_text(a.Login) + "}";
			}
			auto review_json(const application::staged_review_entity& r) const -> std::string
			{
				return "{\"id\":" + std::to_string(r.Id.value()) + ",\"node_id\":" + json_text(r.NodeId) + ",\"html_url\":" + json_text(r.WebUrl) + ",\"pull_request_url\":" + json_text(api() + pull()) + ",\"user\":" + user_json() + ",\"commit_id\":" + json_text(std::string{ core::canonical_hex(r.Commit).view() }) + ",\"body\":" + json_text(r.Body) + ",\"state\":\"" + std::string{ state_name(r.State) } + "\"" + (r.State == application::staged_review_state::pending ? "" : ",\"submitted_at\":\"2026-09-26T12:34:56Z\"") + "}";
			}
			static auto state_name(const application::staged_review_state aState) -> std::string_view
			{
				switch (aState) {
				case application::staged_review_state::pending:           return "PENDING";
				case application::staged_review_state::commented:         return "COMMENTED";
				case application::staged_review_state::approved:          return "APPROVED";
				case application::staged_review_state::changes_requested: return "CHANGES_REQUESTED";
				case application::staged_review_state::dismissed:         return "DISMISSED";
				}
				return "COMMENTED";
			}
			/// [[BPR-1552]]. The head \p aReview was submitted at; the bound head
			/// for a review this server does not hold.
			auto written_head(const application::review_database_id aReview) const -> core::git_commit_oid
			{
				if (State->StagedReview && State->StagedReview->Id == aReview) {
					return State->StagedReview->Commit;
				}
				const auto review = std::ranges::find(State->PreviousReviews, aReview, &application::staged_review_entity::Id);
				return review == State->PreviousReviews.end() ? Binding.Target.Semantic.ExpectedHead : review->Commit;
			}
			/// [[BPR-1367]]. As GitHub answers: a review's own comment list
			/// (`aLines` false) carries no `line`, `side`, `start_line`,
			/// `start_side` or `subject_type`, pending or submitted; only the
			/// pull request's collection does (probed on MikeMarcin/BPR-Test#45).
			auto comments_json(const std::vector<application::staged_review_comment>& aComments, const bool aLines = false) const -> std::string
			{
				std::string result = "[";
				for (auto i = aComments.rbegin(); i != aComments.rend(); ++i) {
					const auto& c = *i;
					if (result.size() > 1) {
						result += ",";
					}
					result += "{\"id\":" + std::to_string(c.Id) + ",\"node_id\":" + json_text(c.NodeId) + ",\"html_url\":" + json_text(c.WebUrl) + ",\"user\":" + user_json() + ",\"body\":" + json_text(c.Body);
					if (c.Source) {
						const auto& source = *c.Source;
						const auto  side   = source.Side == core::diff_side::left ? "LEFT" : "RIGHT";
						result += ",\"pull_request_url\":" + json_text(api() + pull()) + ",\"pull_request_review_id\":" + std::to_string(c.Review.value()) + ",\"commit_id\":" + json_text(std::string{ core::canonical_hex(Binding.Target.Semantic.ExpectedHead).view() }) + ",\"path\":" + json_text(source.Path);
						if (aLines) {
							result += ",\"subject_type\":\"line\",\"side\":" + json_text(side) + ",\"line\":" + std::to_string(std::uint64_t{ source.LastLine } + 1);
							if (source.FirstLine != source.LastLine) {
								result += ",\"start_line\":" + std::to_string(std::uint64_t{ source.FirstLine } + 1) + ",\"start_side\":" + json_text(side);
							}
							// [[BPR-1552]]. As GitHub answers after a push: `commit_id`
							// follows the head, and where the comment was written
							// stays on `original_commit_id`, the head its review was
							// submitted at, and `original_line` (BPR-1538 matches it).
							result += ",\"original_commit_id\":" + json_text(std::string{ core::canonical_hex(written_head(c.Review)).view() }) + ",\"original_line\":" + std::to_string(std::uint64_t{ source.LastLine } + 1);
							if (source.FirstLine != source.LastLine) {
								result += ",\"original_start_line\":" + std::to_string(std::uint64_t{ source.FirstLine } + 1);
							}
						}
					} else {
						result += ",\"issue_url\":" + json_text(api() + repository() + "/issues/" + std::to_string(Binding.Target.Number.value()));
					}
					if (c.Reply) {
						result += ",\"in_reply_to_id\":601";
					}
					result += "}";
				}
				return result + "]";
			}
			auto perform_review_mutation(const wire::github_mutation_wire_request& request, std::stop_token stop, wire::github_mutation_transmission_witness witness) -> wire::github_mutation_wire_completion override
			{
				std::unique_lock lock{ State->Mutex };
				const auto&      w         = request.Wire;
				const auto       success   = [](std::string body) { return wire::github_mutation_wire_success{ .Response = { .Status = 200, .Body = std::move(body) } }; };
				const auto       issuePath = repository() + "/issues/" + std::to_string(Binding.Target.Number.value()) + "/comments";
				if (w.Method == wire::http_method::get) {
					if (w.Path == (Binding.Target.ServerContract == application::github_server_contract::github_com_2022_11_28 ? "/user" : "/api/v3/user")) {
						return success(user_json());
					}
					if (w.Path == pull()) {
						const auto& t = Binding.Target;
						return success("{\"id\":" + std::to_string(t.PullRequestDatabaseId) + ",\"node_id\":" + json_text(t.PullRequestNodeId) + ",\"number\":" + std::to_string(t.Number.value()) + ",\"state\":\"open\",\"body\":" + json_text(State->StagedDescription.value_or(Description)) + ",\"head\":{\"sha\":" + json_text(std::string{ core::canonical_hex(t.Semantic.ExpectedHead).view() }) + ",\"ref\":" + json_text(t.HeadRef) + "},\"base\":{\"sha\":" + json_text(std::string{ core::canonical_hex(t.BaseOid).view() }) + ",\"ref\":" + json_text(t.BaseRef) + ",\"repo\":{\"id\":" + std::to_string(t.BaseRepositoryDatabaseId) + ",\"node_id\":" + json_text(t.RepositoryNodeId) + ",\"full_name\":" + json_text(t.Repository.Owner + "/" + t.Repository.Name) + "}}}");
					}
					// [[BPR-1345]]. The ordinary review list the submission
					// session reads before and after its POST.
					if (w.Path == pull() + "/reviews?per_page=100") {
						// [[BPR-1541]]. Held until released, or until the
						// operation is stopped.
						while (State->StallReviewReads && !stop.stop_requested()) {
							State->ReadGate.wait_for(lock, std::chrono::milliseconds{ 20 });
						}
						if (State->HideReviewReads) {
							if (State->ReviewReadStatus != 0) {
								return wire::github_mutation_wire_success{ .Response = { .Status = State->ReviewReadStatus, .Body = "{}" } };
							}
							return wire::github_mutation_wire_error{ };
						}
						std::string all = "[";
						for (const auto& review : State->PreviousReviews) {
							if (all.size() > 1) {
								all += ",";
							}
							all += review_json(review);
						}
						return success(all + "]");
					}
					if (w.Path == pull() + "/reviews?per_page=100&page=1") {
						if (State->StagedConflict) {
							application::staged_review_entity existing{ application::review_database_id{ 99 }, "PRR_99", root() + "#pullrequestreview-99", actor(), Binding.Target.Semantic.ExpectedHead, "Existing website review", application::staged_review_state::pending };
							return success("[" + review_json(existing) + "]");
						}
						std::string all = "[";
						for (const auto& review : State->PreviousReviews) {
							if (all.size() > 1) {
								all += ",";
							}
							all += review_json(review);
						}
						if (State->StagedReview) {
							if (all.size() > 1) {
								all += ",";
							}
							all += review_json(*State->StagedReview);
						}
						return success(all + "]");
					}
					if (State->StagedReview) {
						const auto r = pull() + "/reviews/" + std::to_string(State->StagedReview->Id.value());
						if (w.Path == r) {
							return success(review_json(*State->StagedReview));
						}
						if (w.Path == r + "/comments?per_page=100&page=1") {
							return success(comments_json(State->StagedComments));
						}
					}
					for (const auto& review : State->PreviousReviews) {
						const auto path = pull() + "/reviews/" + std::to_string(review.Id.value());
						if (w.Path == path) {
							return success(review_json(review));
						}
						if (w.Path == path + "/comments?per_page=100&page=1") {
							std::vector<application::staged_review_comment> comments;
							for (const auto& c : State->PreviousComments) {
								if (c.Review == review.Id) {
									comments.push_back(c);
								}
							}
							return success(comments_json(comments));
						}
					}
					if (w.Path == issuePath + "?per_page=100&page=1") {
						return success(comments_json(State->CompatibilityComments));
					}
					if (pending_comment(w.Path)) {
						return not_found();
					}
					// [[BPR-1345]] [[BPR-1367]]. The pull request's collection, as
					// the submission session and the tour read it: every
					// submitted review's comments, each naming its review, with
					// lines, 100 to a page. It lists no comment of a pending
					// review, not even to its author.
					if (const auto collection = pull() + "/comments?per_page=100&page="; w.Path.starts_with(collection)) {
						std::vector<application::staged_review_comment> listed;
						const auto                                      submitted = [](const application::staged_review_entity& aReview) { return aReview.State != application::staged_review_state::pending; };
						for (const auto& review : State->PreviousReviews) {
							if (submitted(review)) {
								std::ranges::copy_if(State->PreviousComments, std::back_inserter(listed), [&review](const application::staged_review_comment& aComment) { return aComment.Review == review.Id; });
							}
						}
						if (State->StagedReview && submitted(*State->StagedReview)) {
							listed.insert(listed.end(), State->StagedComments.begin(), State->StagedComments.end());
						}
						const auto page  = static_cast<std::size_t>(std::stoul(w.Path.substr(collection.size())));
						const auto first = std::min(listed.size(), (page - 1U) * 100U);
						const auto last  = std::min(listed.size(), first + 100U);
						auto       slice = std::vector<application::staged_review_comment>(listed.begin() + static_cast<std::ptrdiff_t>(first), listed.begin() + static_cast<std::ptrdiff_t>(last));
						auto       reply = success(comments_json(slice, true));
						if (last < listed.size()) {
							reply.Response.Headers.push_back({ .Name = "Link", .Value = "<" + api() + collection + std::to_string(page + 1U) + ">; rel=\"next\"" });
						}
						return reply;
					}
					return wire::github_mutation_wire_error{ };
				}
				if (State->Boundary) {
					State->Boundary(State->StagedWrites, "prepared");
				}
				if (!witness || !witness()) {
					return wire::github_mutation_wire_error{ };
				}
				if (State->Boundary) {
					State->Boundary(State->StagedWrites, "witnessed");
				}
				if (w.Path == pull() && State->StagedRejectDescription) {
					return wire::github_mutation_wire_success{ .Response = { .Status = 403, .Body = "{}" } };
				}
				const auto  object = QJsonDocument::fromJson(QByteArray::fromStdString(w.Body)).object();
				const auto  body   = object["body"].toString().toStdString();
				std::string answer = "{}";
				if (pending_comment(w.Path)) {
					// [[BPR-1368]]. GitHub answers a REST edit of a pending
					// review's comment 404 (MikeMarcin/BPR-Test#45).
					return not_found();
				}
				if (w.Method == wire::http_method::post && w.Path == pull() + "/reviews" && object.contains("event")) {
					// [[BPR-1345]]. An ordinary review that carries line comments:
					// created whole, its comments in it, or refused whole.
					State->ReviewPosts.push_back(w.Body);
					++State->Submissions;
					if (std::exchange(State->RefuseNextReview, false)) {
						return wire::github_mutation_wire_error{ .Failure = application::review_mutation_failure::unavailable, .Transmission = wire::github_mutation_transmission::indeterminate, .HttpStatus = 422U };
					}
					const auto event = object["event"].toString();
					const auto id    = 801 + State->PreviousReviews.size();
					auto       state = application::staged_review_state::commented;
					if (event == QStringLiteral("APPROVE")) {
						state = application::staged_review_state::approved;
					} else if (event == QStringLiteral("REQUEST_CHANGES")) {
						state = application::staged_review_state::changes_requested;
					}
					State->PreviousReviews.push_back(application::staged_review_entity{ application::review_database_id{ id }, "PRR_" + std::to_string(id), root() + "#pullrequestreview-" + std::to_string(id), actor(), Binding.Target.Semantic.ExpectedHead, body, state });
					for (const auto& entry : object["comments"].toArray()) {
						const auto c     = entry.toObject();
						const auto cid   = 901 + State->PreviousComments.size();
						const auto left  = c["side"] == "LEFT";
						const auto last  = static_cast<std::uint32_t>(c["line"].toInt() - 1);
						const auto first = c.contains("start_line") ? static_cast<std::uint32_t>(c["start_line"].toInt() - 1) : last;
						State->PreviousComments.push_back({ static_cast<std::uint64_t>(cid), "PRRC_" + std::to_string(cid), root() + "#discussion_r" + std::to_string(cid), actor(), application::review_database_id{ id }, core::tour_code_destination{ .Commit = left ? Binding.Target.BaseOid : Binding.Target.Semantic.ExpectedHead, .Path = c["path"].toString().toStdString(), .Side = left ? core::diff_side::left : core::diff_side::right, .FirstLine = first, .LastLine = last }, c["body"].toString().toStdString(), false });
					}
					if (std::exchange(State->LoseNextReview, false)) {
						State->HideReviewReads = true;
						return wire::github_mutation_wire_error{ .Failure = application::review_mutation_failure::unavailable, .Transmission = wire::github_mutation_transmission::indeterminate };
					}
					return success(review_json(State->PreviousReviews.back()));
				}
				if (w.Method == wire::http_method::post && w.Path == pull() + "/reviews" && !object.contains("event")) {
					if (State->StagedReview) {
						State->PreviousReviews.push_back(*State->StagedReview);
						State->PreviousComments.insert(State->PreviousComments.end(), State->StagedComments.begin(), State->StagedComments.end());
					}
					const auto id       = 501 + State->StagedCreates;
					State->StagedReview = application::staged_review_entity{ application::review_database_id{ id }, "PRR_" + std::to_string(id), root() + "#pullrequestreview-" + std::to_string(id), actor(), Binding.Target.Semantic.ExpectedHead, body, application::staged_review_state::pending };
					State->StagedComments.clear();
					for (const auto& entry : object["comments"].toArray()) {
						const auto c     = entry.toObject();
						const auto cid   = 601 + State->PreviousComments.size() + State->StagedComments.size();
						const auto left  = c["side"] == "LEFT";
						const auto last  = static_cast<std::uint32_t>(c["line"].toInt() - 1);
						const auto first = c.contains("start_line") ? static_cast<std::uint32_t>(c["start_line"].toInt() - 1) : last;
						State->StagedComments.push_back({ cid, "PRRC_" + std::to_string(cid), root() + "#discussion_r" + std::to_string(cid), actor(), application::review_database_id{ id }, core::tour_code_destination{ .Commit = left ? Binding.Target.BaseOid : Binding.Target.Semantic.ExpectedHead, .Path = c["path"].toString().toStdString(), .Side = left ? core::diff_side::left : core::diff_side::right, .FirstLine = first, .LastLine = last }, c["body"].toString().toStdString(), false });
					}
					++State->StagedCreates;
				} else if (w.Method == wire::http_method::post && State->StagedReview && w.Path == pull() + "/reviews/" + std::to_string(State->StagedReview->Id.value()) + "/events" && object["event"] == "COMMENT") {
					State->StagedReview->Body  = body;
					State->StagedReview->State = application::staged_review_state::commented;
					++State->StagedSubmits;
				} else if (w.Method == wire::http_method::put) {
					bool found = false;
					if (State->StagedReview && w.Path == pull() + "/reviews/" + std::to_string(State->StagedReview->Id.value())) {
						State->StagedReview->Body = body;
						found                     = true;
					}
					for (auto& review : State->PreviousReviews) {
						if (w.Path == pull() + "/reviews/" + std::to_string(review.Id.value())) {
							review.Body = body;
							found       = true;
						}
					}
					if (!found) {
						return wire::github_mutation_wire_error{ };
					}
				} else if (w.Method == wire::http_method::post && w.Path == issuePath) {
					const auto id = 701 + State->CompatibilityComments.size();
					State->CompatibilityComments.push_back({ id, "IC_" + std::to_string(id), root() + "#issuecomment-" + std::to_string(id), actor(), { }, { }, body, false });
				} else if (w.Method == wire::http_method::post && w.Path == graphql()) {
					// [[BPR-1368]]. The fixed mutation that edits a review
					// comment by its node ID, pending or submitted.
					const auto variables = object["variables"].toObject();
					const auto node      = variables["comment"].toString().toStdString();
					if (!object["query"].toString().startsWith(QStringLiteral("mutation UpdateReviewComment(")) || object.size() != 2 || variables.size() != 2) {
						return wire::github_mutation_wire_error{ };
					}
					application::staged_review_comment* edited = nullptr;
					for (auto* comments : { &State->StagedComments, &State->PreviousComments }) {
						for (auto& c : *comments) {
							if (c.NodeId == node) {
								edited = &c;
							}
						}
					}
					if (edited == nullptr) {
						return wire::github_mutation_wire_error{ };
					}
					edited->Body = variables["body"].toString().toStdString();
					answer       = edit_json(*edited);
				} else if (w.Method == wire::http_method::patch && w.Path == pull()) {
					State->StagedDescription = body;
				} else if (w.Method == wire::http_method::patch) {
					bool found = false;
					for (auto& c : State->StagedComments) {
						if (w.Path == repository() + "/pulls/comments/" + std::to_string(c.Id)) {
							c.Body = body;
							found  = true;
						}
					}
					for (auto& c : State->PreviousComments) {
						if (w.Path == repository() + "/pulls/comments/" + std::to_string(c.Id)) {
							c.Body = body;
							found  = true;
						}
					}
					for (auto& c : State->CompatibilityComments) {
						if (w.Path == repository() + "/issues/comments/" + std::to_string(c.Id)) {
							c.Body = body;
							found  = true;
						}
					}
					if (!found) {
						return wire::github_mutation_wire_error{ };
					}
				} else {
					return wire::github_mutation_wire_error{ };
				}
				State->StagedWritePaths.push_back(w.Path);
				++State->StagedWrites;
				if (State->Boundary) {
					State->Boundary(State->StagedWrites - 1, "applied");
				}
				if (State->StagedBlockNext) {
					State->StagedBlockNext = false;
					State->StagedBlocked   = true;
					State->StagedWait.wait(lock, stop, [&] { return !State->StagedBlocked; });
					State->StagedBlocked = false;
				}
				return success(answer);
			}
		};
		class staged_fixture_provider final : public application::review_submission_port
		{
		public:
			staged_fixture_wire             Wire;
			wire::review_submission_session Session;
			staged_fixture_provider(std::shared_ptr<review_submission_fixture_state> state, std::unique_ptr<application::review_submission_recovery_port> store, application::review_submission_live_binding binding, std::string description)
				: Wire{ std::move(state), binding, std::move(description) }
				, Session{ Wire, { .CredentialContext = { .Request = core::request_id{ 1 }, .ApplicationSession = core::application_session_id{ 1 }, .Authority = { .Profile = core::host_profile_id{ 1 }, .ProfileRevision = application::github_profile_revision{ 1 }, .Host = binding.Target.Semantic.Host, .Hostname = binding.Target.Hostname, .Account = binding.ActingIdentity.Account, .ServerContract = binding.Target.ServerContract }, .IdentityGeneration = core::github_identity_generation{ 1 } }, .Target = binding.Target, .ActingIdentity = binding.ActingIdentity }, std::make_shared<staged_fixture_lease>(), std::move(store) }
			{ }
			auto pending(std::stop_token) -> application::review_submission_discovery override { return std::vector<application::review_submission_request>{ }; }
			auto submit_and_reconcile(const application::review_submission_request& request, std::stop_token) -> application::review_submission_result override { return { .Request = request }; }
			auto reconcile(const application::review_submission_request& request, std::stop_token) -> application::review_submission_result override { return { .Request = request }; }
			auto pending_tours(std::stop_token stop) -> std::expected<std::vector<application::staged_review_plan>, application::review_mutation_failure> override { return Session.pending_tours(stop); }
			auto publish_tour(const application::staged_review_plan& plan, std::stop_token stop, application::staged_child_authorizer authorize) -> application::staged_publication_result override { return Session.publish_tour(plan, stop, std::move(authorize)); }
			auto reconcile_tour(const application::staged_review_plan& plan, std::stop_token stop) -> application::staged_publication_result override { return Session.reconcile_tour(plan, stop); }
		};
	}
	auto make_staged_review_fixture_provider(std::shared_ptr<review_submission_fixture_state> state, std::unique_ptr<application::review_submission_recovery_port> store, application::review_submission_live_binding binding, std::string description) -> std::unique_ptr<application::review_submission_port> { return std::make_unique<staged_fixture_provider>(std::move(state), std::move(store), std::move(binding), std::move(description)); }
	namespace {
		/// [[BPR-1345]]. The ordinary provider's store, lent to the session.
		class borrowed_recovery final : public application::review_submission_recovery_port
		{
		public:
			explicit borrowed_recovery(application::review_submission_recovery_port& aStore)
				: Store{ aStore }
			{ }
			auto pending() -> std::expected<std::vector<application::review_submission_recovery_record>, application::review_mutation_recovery_failure> override { return Store.pending(); }
			auto load(const std::string& aKey) -> std::expected<std::optional<application::review_submission_recovery_record>, application::review_mutation_recovery_failure> override { return Store.load(aKey); }
			auto admit(const application::review_submission_recovery_record& aRecord) -> std::expected<application::review_submission_recovery_admission, application::review_mutation_recovery_failure> override { return Store.admit(aRecord); }
			auto mark_possibly_transmitted(const std::string& aKey, const std::array<std::uint8_t, 32>& aFingerprint) -> std::expected<application::review_submission_recovery_record, application::review_mutation_recovery_failure> override { return Store.mark_possibly_transmitted(aKey, aFingerprint); }
			auto resolve(const std::string& aKey, const std::array<std::uint8_t, 32>& aFingerprint, const application::review_submission_receipt& aReceipt) -> std::expected<application::review_submission_recovery_record, application::review_mutation_recovery_failure> override { return Store.resolve(aKey, aFingerprint, aReceipt); }
			auto withdraw(const std::string& aKey, const std::array<std::uint8_t, 32>& aFingerprint) -> std::expected<void, application::review_mutation_recovery_failure> override { return Store.withdraw(aKey, aFingerprint); }

		private:
			application::review_submission_recovery_port& Store;
		};
		class production_review final : public application::review_submission_port
		{
		public:
			production_review(std::shared_ptr<review_submission_fixture_state> aState, application::review_submission_recovery_port& aStore, const application::review_submission_live_binding& aBinding)
				: Wire{ std::move(aState), aBinding, std::string{ } }
				, Session{ Wire, { .CredentialContext = { .Request = core::request_id{ 1 }, .ApplicationSession = core::application_session_id{ 1 }, .Authority = { .Profile = core::host_profile_id{ 1 }, .ProfileRevision = application::github_profile_revision{ 1 }, .Host = aBinding.Target.Semantic.Host, .Hostname = aBinding.Target.Hostname, .Account = aBinding.ActingIdentity.Account, .ServerContract = aBinding.Target.ServerContract }, .IdentityGeneration = core::github_identity_generation{ 1 } }, .Target = aBinding.Target, .ActingIdentity = aBinding.ActingIdentity }, std::make_shared<staged_fixture_lease>(), std::make_unique<borrowed_recovery>(aStore) }
			{ }
			auto pending(std::stop_token aStop) -> application::review_submission_discovery override { return Session.pending(aStop); }
			auto submit_and_reconcile(const application::review_submission_request& aRequest, std::stop_token aStop) -> application::review_submission_result override { return Session.submit_and_reconcile(aRequest, aStop); }
			auto reconcile(const application::review_submission_request& aRequest, std::stop_token aStop) -> application::review_submission_result override { return Session.reconcile(aRequest, aStop); }
			auto admitted(const std::string& aKey, std::stop_token aStop) -> std::expected<std::optional<application::review_submission_request>, application::review_mutation_failure> override { return Session.admitted(aKey, aStop); }
			auto last_unknown(const std::string& aKey, std::stop_token aStop) -> std::optional<core::review_submission_unknown_diagnosis> override { return Session.last_unknown(aKey, aStop); }

		private:
			staged_fixture_wire             Wire;
			wire::review_submission_session Session;
		};
		auto make_production_review(std::shared_ptr<review_submission_fixture_state> aState, application::review_submission_recovery_port& aStore, application::review_submission_live_binding aBinding)
			-> std::unique_ptr<application::review_submission_port>
		{
			return std::make_unique<production_review>(std::move(aState), aStore, aBinding);
		}
	}
}
