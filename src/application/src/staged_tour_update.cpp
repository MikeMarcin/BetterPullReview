#include "bpr/application/staged_review_publication.hpp"
#include "bpr/core/review_tour_reconciliation.hpp"
#include <algorithm>
namespace bpr::application {
	namespace {
		auto remote(const staged_review_comment& aComment, bool aCompatibility) -> core::tour_remote_binding
		{
			return { .Kind = aCompatibility ? core::tour_remote_kind::conversation_comment : core::tour_remote_kind::line_comment, .DatabaseId = aComment.Id, .Entity = aComment.NodeId, .Actor = aComment.Actor.Login, .WebUrl = aComment.WebUrl, .Review = std::to_string(aComment.Review.value()), .Own = true };
		}
		auto body_request(const staged_review_plan& aPlan, const core::tour_body_update& aEdit) -> staged_review_request
		{
			staged_review_request result{ .Binding = *aPlan.Submission.LiveBinding, .Body = aEdit.Desired };
			if (aEdit.Remote.Kind == core::tour_remote_kind::review) {
				result.Operation = staged_review_operation::update_summary;
				result.Review    = review_database_id{ aEdit.Remote.DatabaseId };
			} else {
				result.Operation = aEdit.Remote.Kind == core::tour_remote_kind::conversation_comment ? staged_review_operation::update_issue_comment : staged_review_operation::update_comment;
				result.Comment   = aEdit.Remote.DatabaseId;
			}
			return result;
		}
	}
	auto staged_update_bodies(const staged_publication_record& aRecord) -> std::optional<core::tour_update_bodies>
	{
		if (!aRecord.Plan.Update || aRecord.Plan.Stops.size() != aRecord.Comments.size()) {
			return { };
		}
		std::vector<core::tour_created_destination> created;
		for (std::size_t i = 0; i < aRecord.Comments.size(); ++i) {
			// Always plan from the frozen initial body, even after navigation has been patched.
			const auto&                                 stop = aRecord.Plan.Stops[i];
			const std::vector<core::tour_markdown_link> nav{ { "Tour overview", aRecord.Plan.Update->Remote->WebUrl } };
			const auto                                  body = core::render_tour_stop(stop.Title, i + 1, stop.Narrative, nav, stop.SourceLink);
			if (!body) {
				return { };
			}
			created.push_back({ stop.Id, remote(aRecord.Comments[i], aRecord.Plan.Compatibility), *body });
		}
		return core::plan_tour_update(*aRecord.Plan.Update, created);
	}
	auto staged_update_request(const staged_publication_record& aRecord) -> std::optional<staged_review_request>
	{
		if (!aRecord.Plan.Update) {
			return { };
		}
		const auto bodies = staged_update_bodies(aRecord);
		if (!bodies) {
			return { };
		}
		staged_review_request result{ .Binding = *aRecord.Plan.Submission.LiveBinding };
		if (aRecord.Patched < aRecord.Comments.size()) {
			result.Operation = staged_review_operation::update_issue_comment;
			result.Comment   = aRecord.Comments[aRecord.Patched].Id;
			const auto id    = aRecord.Plan.Stops[aRecord.Patched].Id;
			const auto found = std::ranges::find(aRecord.Plan.Update->Stops, id, &core::tour_stop::Id);
			result.Body      = bodies->StopBodies[static_cast<std::size_t>(found - aRecord.Plan.Update->Stops.begin())];
			if (!aRecord.Plan.Compatibility) {
				// [[BPR-1368]]. Added stops are edited before their review is
				// submitted, while GitHub's REST edit answers 404.
				return staged_pending_edit(aRecord, std::move(result.Body));
			}
		} else if (!aRecord.Plan.Stops.empty() && !aRecord.Plan.Compatibility && !aRecord.DestinationsPublished) {
			result.Operation = staged_review_operation::submit_comment;
			result.Review    = aRecord.Review->Id;
			result.Body      = "Additional stops for [Review tour](" + aRecord.Plan.Update->Remote->WebUrl + ").";
		} else if (aRecord.Updated < bodies->Edits.size()) {
			return body_request(aRecord.Plan, bodies->Edits[aRecord.Updated]);
		} else if (aRecord.Plan.Description && !aRecord.DescriptionDone) {
			result.Operation = staged_review_operation::update_description;
			result.Body      = aRecord.Plan.Description->Desired;
		} else {
			return { };
		}
		return result;
	}
	auto confirm_staged_update(const staged_publication_record& aRecord) -> staged_publication_record
	{
		auto next = aRecord;
		if (!next.Pending) {
			return next;
		}
		if (next.Pending->Operation == staged_review_operation::update_description) {
			next.DescriptionDone = true;
		} else if (next.Patched < next.Comments.size()) {
			next.Comments[next.Patched++].Body = next.Pending->Body;
		} else if (next.Pending->Operation == staged_review_operation::submit_comment) {
			next.Review->Body          = next.Pending->Body;
			next.Review->State         = staged_review_state::commented;
			next.DestinationsPublished = true;
		} else {
			++next.Updated;
		}
		const auto bodies = staged_update_bodies(next);
		next.Published    = bodies && next.Updated == bodies->Edits.size() && next.Patched == next.Comments.size() && (next.Plan.Compatibility || next.Plan.Stops.empty() || next.DestinationsPublished);
		return next;
	}
	auto valid_staged_update(const staged_publication_record& aRecord) -> bool
	{
		if (!aRecord.Plan.Update || aRecord.Index || (aRecord.Plan.Compatibility && aRecord.Review)) {
			return false;
		}
		if (!aRecord.Plan.Compatibility && !aRecord.Plan.Stops.empty() && !aRecord.Review) {
			return aRecord.Comments.empty() && !aRecord.Patched && !aRecord.Updated && !aRecord.Published && !aRecord.DestinationsPublished && (!aRecord.Pending || aRecord.Pending == staged_initial_request(aRecord.Plan));
		}
		if (aRecord.Plan.Compatibility && aRecord.Comments.size() < aRecord.Plan.Stops.size()) {
			return !aRecord.Patched && !aRecord.Updated && !aRecord.Published && (!aRecord.Pending || aRecord.Pending == staged_next_request(aRecord));
		}
		const auto bodies = staged_update_bodies(aRecord);
		if (!bodies || aRecord.Updated > bodies->Edits.size() || aRecord.Published != (aRecord.Updated == bodies->Edits.size() && aRecord.Patched == aRecord.Comments.size() && (aRecord.Plan.Compatibility || aRecord.Plan.Stops.empty() || aRecord.DestinationsPublished))) {
			return false;
		}
		if (aRecord.Review) {
			const auto  initial = staged_initial_request(aRecord.Plan);
			const auto& who     = aRecord.Plan.Submission.LiveBinding->ActingIdentity;
			if (!initial || !aRecord.Review->Id.value() || aRecord.Review->Actor != staged_review_actor{ who.DatabaseId, who.NodeId, who.Login } || aRecord.Review->Commit != aRecord.Plan.Submission.Proposal.Target.ExpectedHead || aRecord.Review->State != (aRecord.DestinationsPublished ? staged_review_state::commented : staged_review_state::pending) || aRecord.Review->Body != (aRecord.DestinationsPublished ? "Additional stops for [Review tour](" + aRecord.Plan.Update->Remote->WebUrl + ")." : initial->Body)) {
				return false;
			}
		}
		std::vector<std::uint64_t> ids;
		for (std::size_t i = 0; i < aRecord.Comments.size(); ++i) {
			const auto& c       = aRecord.Comments[i];
			const auto& stop    = aRecord.Plan.Stops[i];
			const auto& who     = aRecord.Plan.Submission.LiveBinding->ActingIdentity;
			const auto  found   = std::ranges::find(aRecord.Plan.Update->Stops, stop.Id, &core::tour_stop::Id);
			const auto  initial = core::render_tour_stop(stop.Title, i + 1, stop.Narrative, std::vector<core::tour_markdown_link>{ { "Tour overview", aRecord.Plan.Update->Remote->WebUrl } }, stop.SourceLink);
			auto        source  = stop.Source;
			source.Blob.reset();
			if (!c.Id || c.NodeId.empty() || c.WebUrl.empty() || c.Reply || c.Actor != staged_review_actor{ who.DatabaseId, who.NodeId, who.Login } || std::ranges::find(ids, c.Id) != ids.end() || found == aRecord.Plan.Update->Stops.end() || !initial || c.Body != (i < aRecord.Patched ? bodies->StopBodies[static_cast<std::size_t>(found - aRecord.Plan.Update->Stops.begin())] : *initial) || (aRecord.Plan.Compatibility ? c.Source.has_value() || c.Review.value() != 0 : !aRecord.Review || c.Review != aRecord.Review->Id || c.Source != std::optional{ source })) {
				return false;
			}
			ids.push_back(c.Id);
		}
		return !aRecord.Pending || aRecord.Pending == staged_next_request(aRecord);
	}
	auto published_tour(const staged_publication_record& aRecord, const core::review_tour& aDraft) -> std::optional<core::review_tour>
	{
		if (!aRecord.Published || aDraft.Owner != aRecord.Plan.Owner || aDraft.Revision != aRecord.Plan.Generation) {
			return { };
		}
		auto result                = aDraft;
		result.HasPriorObservation = true;
		result.ObservedChanges.clear();
		if (aRecord.Plan.Update) {
			const auto bodies = staged_update_bodies(aRecord);
			if (!bodies) {
				return { };
			}
			for (std::size_t i = 0; i < result.Stops.size(); ++i) {
				auto&      stop    = result.Stops[i];
				const auto created = std::ranges::find(aRecord.Plan.Stops, stop.Id, &staged_tour_stop::Id);
				if (created != aRecord.Plan.Stops.end()) {
					if (stop.Remote) {
						stop.Body += "\n\nReplaces [original discussion](" + stop.Remote->WebUrl + ").";
					}
					stop.Remote = remote(aRecord.Comments[static_cast<std::size_t>(created - aRecord.Plan.Stops.begin())], aRecord.Plan.Compatibility);
					if (stop.Mapped) {
						stop.Original = stop.Mapped;
						stop.Mapped.reset();
						stop.Mapping = core::tour_mapping::exact_original;
					}
				}
				stop.Baseline = core::tour_field_baseline{ .Raw = bodies->StopBodies[i], .RemoteEntity = stop.Remote->Entity };
			}
			result.IndexBaseline = core::tour_field_baseline{ .Raw = bodies->Index, .RemoteEntity = result.Remote->Entity };
			result.Retired.clear();
		} else {
			if (result.Stops.size() != aRecord.Comments.size()) {
				return { };
			}
			result.Remote        = aRecord.Index ? remote(*aRecord.Index, true) : core::tour_remote_binding{ .Kind = core::tour_remote_kind::review, .DatabaseId = aRecord.Review->Id.value(), .Entity = aRecord.Review->NodeId, .Actor = aRecord.Review->Actor.Login, .WebUrl = aRecord.Review->WebUrl, .Own = true };
			result.IndexBaseline = core::tour_field_baseline{ .Raw = aRecord.Index ? aRecord.Index->Body : aRecord.Review->Body, .RemoteEntity = result.Remote->Entity };
			for (std::size_t i = 0; i < result.Stops.size(); ++i) {
				result.Stops[i].Remote   = remote(aRecord.Comments[i], aRecord.Plan.Compatibility);
				result.Stops[i].Baseline = core::tour_field_baseline{ .Raw = aRecord.Comments[i].Body, .RemoteEntity = aRecord.Comments[i].NodeId };
			}
		}
		if (aRecord.Plan.Description && aRecord.DescriptionDone) {
			const auto body = staged_description_body(*aRecord.Plan.Description, result.Remote->WebUrl);
			core::retain_tour_brief_baselines(result, body, aRecord.Plan.Submission.LiveBinding->Target.PullRequestNodeId);
		} else if ((!result.ProblemBaseline && !result.SolutionBaseline) || (result.ProblemBaseline && result.ProblemBaseline->RemoteEntity == result.Remote->Entity)) {
			core::retain_tour_brief_baselines(result, result.IndexBaseline->Raw, result.Remote->Entity);
		}
		return result;
	}
}
