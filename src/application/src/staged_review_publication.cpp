#include "bpr/application/staged_review_publication.hpp"
#include <algorithm>
#include <limits>
namespace bpr::application {
	namespace {
		auto initial_navigation(const staged_review_plan& aPlan) -> std::vector<core::tour_markdown_link>
		{
			const auto& target = aPlan.Submission.LiveBinding->Target;
			if (aPlan.Update) {
				return { { "Tour overview", aPlan.Update->Remote->WebUrl } };
			}
			return { { "Tour overview", "https://" + std::string{ target.Hostname.value() } + "/" + target.Repository.Owner + "/" + target.Repository.Name + "/pull/" + std::to_string(target.Number.value()) } };
		}
		auto initial_stop_body(const staged_review_plan& aPlan, std::size_t aIndex) -> std::optional<std::string>
		{
			const auto& stop = aPlan.Stops[aIndex];
			return core::render_tour_stop(stop.Title, aIndex + 1, stop.Narrative, initial_navigation(aPlan), stop.SourceLink);
		}
		auto description_body(const staged_publication_record& aRecord) -> std::string
		{
			auto result = aRecord.Plan.Description->Desired;
			if (aRecord.Plan.Description->AppendIndexLink) {
				result += "\n\n[Review tour](" + (aRecord.Index ? aRecord.Index->WebUrl : aRecord.Review->WebUrl) + ")";
			}
			return result;
		}
	}
	auto valid(const staged_review_plan& aPlan) -> bool
	{
		if (!valid(aPlan.Submission) || !aPlan.Submission.LiveBinding || !core::valid(aPlan.Owner) || !aPlan.Generation || (aPlan.Stops.empty() && !aPlan.Update) || aPlan.Stops.size() > maximum_staged_review_comments || aPlan.Submission.Proposal.Disposition != core::review_disposition::comment) {
			return false;
		}
		const auto& target = aPlan.Submission.LiveBinding->Target;
		if (aPlan.Owner.Host != target.Semantic.Host || aPlan.Owner.Repository != target.Semantic.Repository || aPlan.Owner.PullRequest != target.Semantic.PullRequest || aPlan.Owner.Account != aPlan.Submission.LiveBinding->ActingIdentity.Account) {
			return false;
		}
		if (aPlan.Update) {
			const auto& draft = *aPlan.Update;
			if (core::validate(draft) != core::tour_refusal::none || draft.Owner != aPlan.Owner || draft.Revision != aPlan.Generation || !draft.Remote || !draft.Remote->Own || !draft.IndexBaseline || draft.ReadOnly || draft.RemoteStale || !draft.Conflicts.empty() || draft.Stops.empty()) {
				return false;
			}
			std::size_t count = 0;
			for (const auto& stop : draft.Stops) {
				if (core::needs_tour_destination(stop)) {
					const auto found  = std::ranges::find(aPlan.Stops, stop.Id, &staged_tour_stop::Id);
					const auto source = (stop.Mapping == core::tour_mapping::author_selected && stop.Mapped) ? std::optional{ core::destination_of(*stop.Mapped) } : (stop.Original ? std::optional{ core::destination_of(*stop.Original) } : stop.Destination);
					if (found == aPlan.Stops.end() || found->Title != stop.Title || found->Narrative != stop.Body || !source || found->Source != *source) {
						return false;
					}
					++count;
				} else if (!stop.Remote->Own || !stop.Baseline) {
					return false;
				}
			}
			if (count != aPlan.Stops.size()) {
				return false;
			}
		}
		std::vector<core::tour_stop_id> ids;
		std::size_t                     bytes = aPlan.Submission.Proposal.Body.size();
		for (const auto& stop : aPlan.Stops) {
			if (!stop.Id.value() || !core::valid(stop.Source) || stop.Title.empty() || stop.Narrative.empty() || stop.SourceLink.empty() || std::ranges::find(ids, stop.Id) != ids.end()) {
				return false;
			}
			ids.push_back(stop.Id);
			bytes += stop.Title.size() + stop.Narrative.size() + stop.SourceLink.size();
			if (bytes > core::maximum_tour_field_bytes || (!aPlan.Compatibility && stop.Source.Commit != (stop.Source.Side == core::diff_side::right ? target.Semantic.ExpectedHead : target.BaseOid))) {
				return false;
			}
			const auto noNavigation = initial_navigation(aPlan);
			if (!core::render_tour_stop(stop.Title, ids.size(), stop.Narrative, noNavigation, stop.SourceLink)) {
				return false;
			}
		}
		return !aPlan.Description || (aPlan.Description->Baseline.size() <= core::maximum_tour_field_bytes && aPlan.Description->Desired.size() <= core::maximum_tour_field_bytes && !aPlan.Description->Desired.empty() && (aPlan.Description->AppendIndexLink || aPlan.Description->Baseline != aPlan.Description->Desired));
	}
	auto staged_initial_request(const staged_review_plan& aPlan) -> std::optional<staged_review_request>
	{
		if (!valid(aPlan)) {
			return std::nullopt;
		}
		if (aPlan.Update && aPlan.Stops.empty()) {
			staged_publication_record record{ .Plan = aPlan };
			return staged_update_request(record);
		}
		if (aPlan.Compatibility) {
			return staged_review_request{ .Binding = *aPlan.Submission.LiveBinding, .Operation = staged_review_operation::create_issue_comment, .Body = *initial_stop_body(aPlan, 0) };
		}
		staged_review_request result{ .Binding = *aPlan.Submission.LiveBinding, .Operation = staged_review_operation::create_pending, .Body = "Tour preparation is incomplete.\n\n" + aPlan.Submission.Proposal.Body };
		for (const auto& stop : aPlan.Stops) {
			const auto noNavigation = initial_navigation(aPlan);
			const auto body         = core::render_tour_stop(stop.Title, result.Lines.size() + 1, stop.Narrative, noNavigation, stop.SourceLink);
			if (!body) {
				return std::nullopt;
			}
			auto source = stop.Source;
			source.Blob.reset();
			result.Lines.push_back({ std::move(source), *body });
		}
		return result;
	}
	auto staged_final_bodies(const staged_publication_record& aRecord) -> std::optional<std::pair<std::string, std::vector<std::string>>>
	{
		if (aRecord.Plan.Update) {
			const auto bodies = staged_update_bodies(aRecord);
			if (!bodies) {
				return { };
			}
			std::vector<std::string> created;
			for (const auto& stop : aRecord.Plan.Stops) {
				const auto found = std::ranges::find(aRecord.Plan.Update->Stops, stop.Id, &core::tour_stop::Id);
				if (found == aRecord.Plan.Update->Stops.end()) {
					return { };
				}
				created.push_back(bodies->StopBodies[static_cast<std::size_t>(found - aRecord.Plan.Update->Stops.begin())]);
			}
			return std::pair{ bodies->Index, std::move(created) };
		}
		if (!valid(aRecord.Plan) || (!aRecord.Plan.Compatibility && !aRecord.Review) || aRecord.Comments.size() != aRecord.Plan.Stops.size()) {
			return std::nullopt;
		}
		std::vector<core::tour_markdown_link> entries;
		entries.reserve(aRecord.Comments.size());
		for (std::size_t i = 0; i < aRecord.Comments.size(); ++i) {
			// [[BPR-1222]]. Only an admitted HTTPS comment URL is rewritten to
			// its Files changed anchor; the core reads the path alone.
			const auto& web = aRecord.Comments[i].WebUrl;
			entries.push_back({ aRecord.Plan.Stops[i].Title, web.starts_with("https://") ? core::tour_stop_link(web) : web });
		}
		const auto index = core::render_tour_index(entries);
		if (!index) {
			return std::nullopt;
		}
		std::pair<std::string, std::vector<std::string>> result{ aRecord.Plan.Submission.Proposal.Body + "\n\n" + *index, { } };
		for (std::size_t i = 0; i < entries.size(); ++i) {
			std::vector<core::tour_markdown_link> navigation;
			if (i) {
				navigation.push_back({ "Previous", entries[i - 1].Destination });
			}
			navigation.push_back({ "Tour overview", aRecord.Plan.Compatibility ? (aRecord.Index ? aRecord.Index->WebUrl : initial_navigation(aRecord.Plan).front().Destination) : aRecord.Review->WebUrl });
			if (i + 1 < entries.size()) {
				navigation.push_back({ "Next", entries[i + 1].Destination });
			}
			const auto& stop = aRecord.Plan.Stops[i];
			const auto  body = core::render_tour_stop(stop.Title, i + 1, stop.Narrative, navigation, stop.SourceLink);
			if (!body) {
				return std::nullopt;
			}
			result.second.push_back(*body);
		}
		return result;
	}
	auto staged_pending_edit(const staged_publication_record& aRecord, std::string aBody) -> staged_review_request
	{
		const auto& comment = aRecord.Comments[aRecord.Patched];
		return { .Binding = *aRecord.Plan.Submission.LiveBinding, .Operation = staged_review_operation::update_pending_comment, .Review = aRecord.Review->Id, .Comment = comment.Id, .CommentNode = comment.NodeId, .Body = std::move(aBody) };
	}
	auto staged_next_request(const staged_publication_record& aRecord) -> std::optional<staged_review_request>
	{
		if (aRecord.Plan.Compatibility && aRecord.Comments.size() < aRecord.Plan.Stops.size()) {
			return staged_review_request{ .Binding = *aRecord.Plan.Submission.LiveBinding, .Operation = staged_review_operation::create_issue_comment, .Body = *initial_stop_body(aRecord.Plan, aRecord.Comments.size()) };
		}
		if (!aRecord.Review && !aRecord.Plan.Compatibility && !aRecord.Plan.Stops.empty()) {
			return staged_initial_request(aRecord.Plan);
		}
		if (aRecord.Plan.Update) {
			return staged_update_request(aRecord);
		}
		const auto bodies = staged_final_bodies(aRecord);
		if (!bodies) {
			return std::nullopt;
		}
		staged_review_request result{ .Binding = *aRecord.Plan.Submission.LiveBinding };
		if (aRecord.Plan.Compatibility && !aRecord.Index) {
			result.Operation = staged_review_operation::create_issue_comment;
			result.Body      = bodies->first;
		} else if (aRecord.Patched < aRecord.Comments.size()) {
			if (!aRecord.Plan.Compatibility) {
				return staged_pending_edit(aRecord, bodies->second[aRecord.Patched]);
			}
			result.Operation = staged_review_operation::update_issue_comment;
			result.Comment   = aRecord.Comments[aRecord.Patched].Id;
			result.Body      = bodies->second[aRecord.Patched];
		} else if (!aRecord.Published) {
			result.Operation = staged_review_operation::submit_comment;
			result.Review    = aRecord.Review->Id;
			result.Body      = bodies->first;
		} else if (aRecord.Plan.Description && !aRecord.DescriptionDone) {
			result.Operation = staged_review_operation::update_description;
			result.Body      = description_body(aRecord);
		} else {
			return std::nullopt;
		}
		return result;
	}
	auto valid(const staged_publication_record& aRecord) -> bool
	{
		if (!valid(aRecord.Plan) || !aRecord.Revision || aRecord.BaselineIds.size() > maximum_staged_review_pages * 100U || !std::ranges::is_sorted(aRecord.BaselineIds) || std::ranges::adjacent_find(aRecord.BaselineIds) != aRecord.BaselineIds.end() || std::ranges::find(aRecord.BaselineIds, 0U) != aRecord.BaselineIds.end()
			|| aRecord.Comments.size() > aRecord.Plan.Stops.size() || aRecord.Patched > aRecord.Comments.size() || (aRecord.PossiblyTransmitted && !aRecord.Pending) || (aRecord.DescriptionDone && (!aRecord.Published || !aRecord.Plan.Description))) {
			return false;
		}
		if (aRecord.Plan.Update) {
			return valid_staged_update(aRecord);
		}
		if (aRecord.Updated || aRecord.DestinationsPublished) {
			return false;
		}
		if (aRecord.Plan.Compatibility) {
			if (aRecord.Review || (aRecord.Patched && !aRecord.Index) || (aRecord.Index && aRecord.Comments.size() != aRecord.Plan.Stops.size()) || aRecord.Published != (aRecord.Index && aRecord.Patched == aRecord.Plan.Stops.size())) {
				return false;
			}
			const auto&                binding = *aRecord.Plan.Submission.LiveBinding;
			const staged_review_actor  actor{ binding.ActingIdentity.DatabaseId, binding.ActingIdentity.NodeId, binding.ActingIdentity.Login };
			const auto                 root = initial_navigation(aRecord.Plan).front().Destination;
			std::vector<std::uint64_t> ids;
			const auto                 identity = [&](const staged_review_comment& comment) { return comment.Id && !comment.NodeId.empty() && comment.Actor == actor && !comment.Source && !comment.Review.value() && !comment.Reply && comment.WebUrl == root + "#issuecomment-" + std::to_string(comment.Id); };
			const auto                 bodies   = staged_final_bodies(aRecord);
			if (aRecord.Patched && !bodies) {
				return false;
			}
			for (std::size_t i = 0; i < aRecord.Comments.size(); ++i) {
				const auto& comment = aRecord.Comments[i];
				if (!identity(comment) || std::ranges::find(ids, comment.Id) != ids.end() || std::ranges::binary_search(aRecord.BaselineIds, comment.Id) || comment.Body != (i < aRecord.Patched ? bodies->second[i] : *initial_stop_body(aRecord.Plan, i))) {
					return false;
				}
				ids.push_back(comment.Id);
			}
			if (aRecord.Pending && aRecord.Pending != staged_next_request(aRecord)) {
				return false;
			}
			return !aRecord.Index || (identity(*aRecord.Index) && std::ranges::find(ids, aRecord.Index->Id) == ids.end() && !std::ranges::binary_search(aRecord.BaselineIds, aRecord.Index->Id) && bodies && aRecord.Index->Body == bodies->first);
		}
		if (aRecord.Index) {
			return false;
		}
		if (aRecord.Review) {
			if (!aRecord.Review->Id.value() || aRecord.Review->NodeId.empty() || aRecord.Review->WebUrl.empty() || aRecord.Comments.size() != aRecord.Plan.Stops.size()) {
				return false;
			}
			const auto                initial = staged_initial_request(aRecord.Plan);
			const auto                bodies  = staged_final_bodies(aRecord);
			const auto&               binding = *aRecord.Plan.Submission.LiveBinding;
			const staged_review_actor actor{ binding.ActingIdentity.DatabaseId, binding.ActingIdentity.NodeId, binding.ActingIdentity.Login };
			if (!initial || !bodies || aRecord.Review->Actor != actor || aRecord.Review->Commit != binding.Target.Semantic.ExpectedHead
				|| aRecord.Review->State != (aRecord.Published ? staged_review_state::commented : staged_review_state::pending)
				|| aRecord.Review->Body != (aRecord.Published ? bodies->first : initial->Body)) {
				return false;
			}
			std::vector<std::uint64_t> ids;
			for (std::size_t i = 0; i < aRecord.Comments.size(); ++i) {
				const auto& comment = aRecord.Comments[i];
				if (!comment.Id || comment.NodeId.empty() || comment.WebUrl.empty() || comment.Actor != actor || comment.Review != aRecord.Review->Id || comment.Reply
					|| comment.Source != std::optional{ initial->Lines[i].Source }
					|| comment.Body != (i < aRecord.Patched ? bodies->second[i] : initial->Lines[i].Body)
					|| std::ranges::find(ids, comment.Id) != ids.end()) {
					return false;
				}
				ids.push_back(comment.Id);
			}
		} else if (!aRecord.Comments.empty() || aRecord.Patched || aRecord.Published) {
			return false;
		}
		if (aRecord.Pending && aRecord.Pending != staged_next_request(aRecord)) {
			return false;
		}
		return !aRecord.Published || (aRecord.Patched == aRecord.Comments.size() && aRecord.Review->State == staged_review_state::commented);
	}
	auto valid_staged_transition(const std::optional<staged_publication_record>& aBefore, const staged_publication_record& aAfter) -> bool
	{
		if (!valid(aAfter)) {
			return false;
		}
		if (!aBefore) {
			return aAfter.Revision == 1 && !aAfter.Review && !aAfter.Index && aAfter.Comments.empty() && !aAfter.PossiblyTransmitted && !aAfter.Updated && !aAfter.DestinationsPublished && aAfter.Pending == staged_initial_request(aAfter.Plan);
		}
		if (!valid(*aBefore) || aBefore->Plan != aAfter.Plan || aBefore->Revision == std::numeric_limits<std::uint64_t>::max() || aAfter.Revision != aBefore->Revision + 1 || aBefore->BaselineIds != aAfter.BaselineIds || aBefore->Patched > aAfter.Patched || aAfter.Patched > aBefore->Patched + 1 || (aBefore->Published && !aAfter.Published) || (aBefore->DescriptionDone && !aAfter.DescriptionDone)) {
			return false;
		}
		if (aBefore->Review && (!aAfter.Review || aBefore->Review->Id != aAfter.Review->Id || aBefore->Review->NodeId != aAfter.Review->NodeId || aBefore->Review->WebUrl != aAfter.Review->WebUrl)) {
			return false;
		}
		if (aBefore->Pending) {
			if (aAfter.Pending) {
				auto exact = *aBefore;
				exact.Revision++;
				exact.PossiblyTransmitted = true;
				return !aBefore->PossiblyTransmitted && exact == aAfter;
			}
			if (!aBefore->PossiblyTransmitted || aAfter.PossiblyTransmitted) {
				return false;
			}
			auto expected = *aBefore;
			expected.Revision++;
			expected.Pending.reset();
			expected.PossiblyTransmitted = false;
			if (aBefore->Plan.Update && aBefore->Pending->Operation != staged_review_operation::create_pending && aBefore->Pending->Operation != staged_review_operation::create_issue_comment) {
				expected = confirm_staged_update(*aBefore);
				expected.Revision++;
				expected.Pending.reset();
				expected.PossiblyTransmitted = false;
				return expected == aAfter;
			}
			switch (aBefore->Pending->Operation) {
			case staged_review_operation::create_pending:
				if (!aAfter.Review || std::ranges::binary_search(aBefore->BaselineIds, aAfter.Review->Id.value())) {
					return false;
				}
				expected.Review   = aAfter.Review;
				expected.Comments = aAfter.Comments;
				break;
			case staged_review_operation::create_issue_comment:
				if (!expected.Plan.Compatibility) {
					return false;
				}
				if (expected.Comments.size() < expected.Plan.Stops.size()) {
					if (aAfter.Comments.size() != expected.Comments.size() + 1) {
						return false;
					}
					expected.Comments.push_back(aAfter.Comments.back());
				} else {
					expected.Index = aAfter.Index;
				}
				break;
			case staged_review_operation::update_issue_comment:
				if (!expected.Plan.Compatibility) {
					return false;
				}
				expected.Comments[expected.Patched].Body = aBefore->Pending->Body;
				++expected.Patched;
				expected.Published = expected.Patched == expected.Comments.size();
				break;
			case staged_review_operation::update_comment:
			case staged_review_operation::update_pending_comment:
				expected.Comments[expected.Patched].Body = aBefore->Pending->Body;
				++expected.Patched;
				break;
			case staged_review_operation::submit_comment:
				expected.Review->Body  = aBefore->Pending->Body;
				expected.Review->State = staged_review_state::commented;
				expected.Published     = true;
				break;
			case staged_review_operation::update_description: expected.DescriptionDone = true; break;
			default:                                          return false;
			}
			return expected == aAfter;
		}
		return aAfter.Pending.has_value() && !aAfter.PossiblyTransmitted && aBefore->Review == aAfter.Review && aBefore->Index == aAfter.Index && aBefore->Comments == aAfter.Comments && aBefore->Patched == aAfter.Patched && aBefore->Published == aAfter.Published && aBefore->DescriptionDone == aAfter.DescriptionDone && aBefore->Updated == aAfter.Updated && aBefore->DestinationsPublished == aAfter.DestinationsPublished;
	}
}
