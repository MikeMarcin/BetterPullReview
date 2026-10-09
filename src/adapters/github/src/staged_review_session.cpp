#include "review_submission_session.hpp"
#include <algorithm>
#include <atomic>
namespace bpr::adapters::github::detail {
	namespace {
		using record    = application::staged_publication_record;
		using state     = application::staged_publication_state;
		using operation = application::staged_review_operation;
		auto status(const record& aRecord) -> state
		{
			if (aRecord.PossiblyTransmitted) {
				return state::outcome_unknown;
			}
			if (aRecord.Published) {
				return aRecord.Plan.Description && !aRecord.DescriptionDone ? state::description_attention : state::published;
			}
			return aRecord.Plan.Compatibility && !aRecord.Comments.empty() ? state::partial : (aRecord.Review ? state::staged : state::ready);
		}
		auto actor(const application::staged_review_plan& aPlan) -> application::staged_review_actor
		{
			const auto& who = aPlan.Submission.LiveBinding->ActingIdentity;
			return { who.DatabaseId, who.NodeId, who.Login };
		}
		auto request(const record& aRecord, operation aOperation) -> application::staged_review_request
		{
			application::staged_review_request result{ .Binding = *aRecord.Plan.Submission.LiveBinding, .Operation = aOperation };
			if (aOperation == operation::read_review || aOperation == operation::list_comments) {
				result.Review = aRecord.Review->Id;
			}
			return result;
		}
		/// [[BPR-1367]]. A pending review's comments are listed without lines
		/// (`aLines` false): such a comment is the expected one when everything
		/// but its source agrees. Its lines are confirmed from the pull
		/// request's collection once the review is submitted.
		auto same_comment(const application::staged_review_comment& aObserved, const application::staged_review_comment& aExpected, const bool aLines) -> bool
		{
			if (aLines) {
				return aObserved == aExpected;
			}
			auto adopted   = aObserved;
			adopted.Source = aExpected.Source;
			return !aObserved.Source && adopted == aExpected;
		}
		auto ordered_comments(const std::vector<application::staged_review_comment>& aObserved, const std::vector<application::staged_review_comment>& aExpected, const bool aLines) -> std::optional<std::vector<application::staged_review_comment>>
		{
			if (aObserved.size() != aExpected.size()) {
				return std::nullopt;
			}
			std::vector<application::staged_review_comment> result;
			for (const auto& expected : aExpected) {
				const auto found = std::ranges::find(aObserved, expected.Id, &application::staged_review_comment::Id);
				if (found == aObserved.end() || !same_comment(*found, expected, aLines)) {
					return std::nullopt;
				}
				result.push_back(expected);
			}
			return result;
		}
		auto owned_comments(const std::vector<application::staged_review_comment>& aObserved, const record& aExpected) -> bool
		{
			auto owned = aExpected.Comments;
			if (aExpected.Index) {
				owned.push_back(*aExpected.Index);
			}
			return std::ranges::all_of(owned, [&](const auto& wanted) {const auto found=std::ranges::find(aObserved,wanted.Id,&application::staged_review_comment::Id);return found!=aObserved.end() && *found==wanted; });
		}
		auto initial_comments(const application::staged_review_plan& aPlan, application::review_database_id aReview, const std::vector<application::staged_review_comment>& aObserved, const bool aLines) -> std::optional<std::vector<application::staged_review_comment>>
		{
			const auto initial = application::staged_initial_request(aPlan);
			if (!initial || aObserved.size() != initial->Lines.size()) {
				return std::nullopt;
			}
			std::vector<application::staged_review_comment> result;
			for (const auto& line : initial->Lines) {
				std::optional<application::staged_review_comment> match;
				for (const auto& observed : aObserved) {
					// [[BPR-1367]]. Listed without lines, the comment takes the
					// line it was created at; each stop's body is distinct.
					if (observed.Body == line.Body && (aLines ? observed.Source == std::optional{ line.Source } : !observed.Source) && observed.Actor == actor(aPlan) && observed.Review == aReview && !observed.Reply) {
						if (match) {
							return std::nullopt;
						}
						match         = observed;
						match->Source = line.Source;
					}
				}
				if (!match || std::ranges::any_of(result, [&](const auto& r) { return r.Id == match->Id; })) {
					return std::nullopt;
				}
				result.push_back(*match);
			}
			return result;
		}
	}
	auto review_submission_session::observe_staged(application::staged_review_request aRequest, std::stop_token aStop) -> review_mutation_parse_result<application::staged_review_observation>
	{
		application::staged_review_observation result;
		std::size_t                            bytes = 0;
		for (std::uint32_t page = 1; page <= application::maximum_staged_review_pages; ++page) {
			aRequest.Page   = page;
			const auto wire = compile_staged_review_request(aRequest);
			if (!wire) {
				return std::unexpected(wire.error());
			}
			const auto  completion = perform(*wire, aStop);
			const auto* success    = std::get_if<github_mutation_wire_success>(&completion);
			if (!success) {
				return std::unexpected(review_mutation_contract_failure::invalid_status);
			}
			bytes += success->Response.Body.size();
			if (bytes > maximum_review_mutation_response_bytes * application::maximum_staged_review_pages) {
				return std::unexpected(review_mutation_contract_failure::response_limit);
			}
			auto parsed = parse_staged_review_response(success->Response, aRequest);
			if (!parsed) {
				return std::unexpected(parsed.error());
			}
			for (auto& review : parsed->Reviews) {
				if (std::ranges::any_of(result.Reviews, [&](const auto& r) { return r.Id == review.Id || r.NodeId == review.NodeId; })) {
					return std::unexpected(review_mutation_contract_failure::identity_mismatch);
				}
				result.Reviews.push_back(std::move(review));
			}
			for (auto& comment : parsed->Comments) {
				if (std::ranges::any_of(result.Comments, [&](const auto& c) { return c.Id == comment.Id || c.NodeId == comment.NodeId; })) {
					return std::unexpected(review_mutation_contract_failure::identity_mismatch);
				}
				result.Comments.push_back(std::move(comment));
			}
			if (!parsed->HasNext) {
				return result;
			}
		}
		return std::unexpected(review_mutation_contract_failure::response_limit);
	}
	auto review_submission_session::observe_staged_comments(const record& aRecord, const application::staged_review_entity& aReview, std::stop_token aStop, std::optional<std::vector<review_comment_collection_entry>>* aCollection) -> review_mutation_parse_result<staged_comments>
	{
		auto read      = request(aRecord, operation::list_reviews);
		read.Operation = operation::list_comments;
		read.Review    = aReview.Id;
		if (aReview.State == application::staged_review_state::pending) {
			// GitHub lists a pending review's comments only here, without lines
			// (MikeMarcin/BPR-Test#45): the collection has none of them.
			auto observed = observe_staged(read, aStop);
			if (!observed) {
				return std::unexpected(observed.error());
			}
			return staged_comments{ .Comments = std::move(observed->Comments), .Lines = false };
		}
		auto  local      = std::optional<std::vector<review_comment_collection_entry>>{ };
		auto& collection = aCollection != nullptr ? *aCollection : local;
		if (!collection) {
			auto observed = observe_comment_collection(aStop);
			if (!observed) {
				return std::unexpected(observed.error());
			}
			collection = std::move(*observed);
		}
		auto mine = staged_review_comments(*collection, read);
		if (!mine) {
			return std::unexpected(mine.error());
		}
		return staged_comments{ .Comments = std::move(*mine), .Lines = true };
	}
	auto review_submission_session::verify_tour_update(const record& aRecord, std::stop_token aStop) -> bool
	{
		const auto& draft  = *aRecord.Plan.Update;
		const auto  bodies = application::staged_update_bodies(aRecord);
		if (aRecord.Updated && !bodies) {
			return false;
		}
		const auto expected = [&](const core::tour_remote_binding& remote, const std::string& baseline) {
			if (bodies) {
				for (std::size_t i = 0; i < aRecord.Updated; ++i) {
					if (bodies->Edits[i].Remote.Entity == remote.Entity) {
						return bodies->Edits[i].Desired;
					}
				}
			}
			return baseline;
		};
		const auto reviews = observe_staged(request(aRecord, operation::list_reviews), aStop);
		if (!reviews) {
			return false;
		}
		std::vector<application::staged_review_comment>             comments;
		std::vector<const core::tour_stop*>                         stops;
		std::optional<std::vector<review_comment_collection_entry>> collection;
		for (const auto& stop : draft.Stops) {
			if (stop.Remote) {
				stops.push_back(&stop);
			}
		}
		for (const auto& stop : draft.Retired) {
			stops.push_back(&stop);
		}
		for (const auto& review : reviews->Reviews) {
			if (review.State == application::staged_review_state::pending && (!aRecord.Review || review.Id != aRecord.Review->Id)) {
				return false;
			}
			const bool needed = std::ranges::any_of(stops, [&](const auto* stop) { return stop->Remote->Kind != core::tour_remote_kind::conversation_comment && (stop->Remote->Review == review.NodeId || stop->Remote->Review == std::to_string(review.Id.value())); });
			if (!needed) {
				continue;
			}
			const auto observed = observe_staged_comments(aRecord, review, aStop, &collection);
			if (!observed) {
				return false;
			}
			comments.insert(comments.end(), observed->Comments.begin(), observed->Comments.end());
		}
		if (draft.Remote->Kind == core::tour_remote_kind::conversation_comment || std::ranges::any_of(stops, [](const auto* stop) { return stop->Remote->Kind == core::tour_remote_kind::conversation_comment; })) {
			const auto observed = observe_staged(request(aRecord, operation::list_issue_comments), aStop);
			if (!observed) {
				return false;
			}
			comments.insert(comments.end(), observed->Comments.begin(), observed->Comments.end());
		}
		const auto matches = [&](const auto& observed, const core::tour_remote_binding& remote) { return observed.NodeId == remote.Entity && observed.WebUrl == remote.WebUrl && observed.Actor == actor(aRecord.Plan) && remote.Own && remote.Actor == observed.Actor.Login; };
		if (draft.Remote->Kind == core::tour_remote_kind::review) {
			const auto found = std::ranges::find(reviews->Reviews, application::review_database_id{ draft.Remote->DatabaseId }, &application::staged_review_entity::Id);
			if (found == reviews->Reviews.end() || !matches(*found, *draft.Remote) || found->State == application::staged_review_state::pending || found->Body != expected(*draft.Remote, draft.IndexBaseline->Raw)) {
				return false;
			}
		} else {
			const auto found = std::ranges::find(comments, draft.Remote->DatabaseId, &application::staged_review_comment::Id);
			if (found == comments.end() || !matches(*found, *draft.Remote) || found->Reply || found->Body != expected(*draft.Remote, draft.IndexBaseline->Raw)) {
				return false;
			}
		}
		for (const auto* stop : stops) {
			if (!stop->Baseline) {
				return false;
			}
			const auto found = std::ranges::find(comments, stop->Remote->DatabaseId, &application::staged_review_comment::Id);
			if (found == comments.end() || !matches(*found, *stop->Remote) || found->Reply || found->Body != expected(*stop->Remote, stop->Baseline->Raw)) {
				return false;
			}
			auto source = stop->Original ? std::optional{ core::destination_of(*stop->Original) } : stop->Destination;
			if (source) {
				source->Blob.reset();
			}
			if (stop->Remote->Kind != core::tour_remote_kind::conversation_comment && found->Source != source) {
				return false;
			}
		}
		return true;
	}
	auto review_submission_session::verify_staged(const record& aRecord, std::stop_token aStop) -> bool
	{
		if (!observe_exact(aStop)) {
			return false;
		}
		if (aRecord.Plan.Update && !verify_tour_update(aRecord, aStop)) {
			return false;
		}
		if (aRecord.Plan.Compatibility) {
			const auto comments = observe_staged(request(aRecord, operation::list_issue_comments), aStop);
			if (!comments || !owned_comments(comments->Comments, aRecord)) {
				return false;
			}
			if (aRecord.Comments.empty()) {
				std::vector<std::uint64_t> ids;
				for (const auto& c : comments->Comments) {
					ids.push_back(c.Id);
				}
				std::ranges::sort(ids);
				if (ids != aRecord.BaselineIds) {
					return false;
				}
			}
		} else {
			if (!aRecord.Review && (!aRecord.Plan.Update || !aRecord.Plan.Stops.empty())) {
				const auto reviews = observe_staged(request(aRecord, operation::list_reviews), aStop);
				if (!reviews) {
					return false;
				}
				std::vector<std::uint64_t> ids;
				for (const auto& review : reviews->Reviews) {
					if (review.State == application::staged_review_state::pending) {
						return false;
					}
					ids.push_back(review.Id.value());
				}
				std::ranges::sort(ids);
				return ids == aRecord.BaselineIds;
			}
			if (aRecord.Review) {
				const auto reviews = observe_staged(request(aRecord, operation::read_review), aStop);
				if (!reviews || reviews->Reviews.size() != 1 || reviews->Reviews.front() != *aRecord.Review) {
					return false;
				}
				const auto comments = observe_staged_comments(aRecord, reviews->Reviews.front(), aStop);
				if (!comments || !ordered_comments(comments->Comments, aRecord.Comments, comments->Lines)) {
					return false;
				}
			}
		}
		if (aRecord.Pending && aRecord.Pending->Operation == operation::update_description) {
			const auto wire = compile_target_observation_request(Configuration.Target, Configuration.Target.ServerContract);
			if (!wire) {
				return false;
			}
			const auto  answer  = perform(*wire, aStop);
			const auto* success = std::get_if<github_mutation_wire_success>(&answer);
			if (!success) {
				return false;
			}
			const auto parsed = parse_staged_review_response(success->Response, *aRecord.Pending);
			return parsed && parsed->Description == std::optional{ aRecord.Plan.Description->Baseline };
		}
		return true;
	}
	auto review_submission_session::reconcile_staged(record& aRecord, std::stop_token aStop) -> state
	{
		if (!application::valid(aRecord) || !aRecord.Pending || !aRecord.PossiblyTransmitted) {
			return state::outcome_unknown;
		}
		if (!observe_exact(aStop, true)) {
			return state::outcome_unknown;
		}
		auto next = aRecord;
		if (aRecord.Plan.Update && aRecord.Pending->Operation != operation::create_pending && aRecord.Pending->Operation != operation::create_issue_comment && aRecord.Pending->Operation != operation::update_description) {
			next = application::confirm_staged_update(aRecord);
			next.Pending.reset();
			next.PossiblyTransmitted = false;
			if (!verify_staged(next, aStop)) {
				return verify_staged(aRecord, aStop) ? state::outcome_unknown : state::conflict;
			}
		} else if (aRecord.Plan.Compatibility && aRecord.Pending->Operation != operation::update_description) {
			const auto comments = observe_staged(request(aRecord, operation::list_issue_comments), aStop);
			if (!comments) {
				return state::outcome_unknown;
			}
			if (aRecord.Pending->Operation == operation::create_issue_comment) {
				if (!owned_comments(comments->Comments, aRecord)) {
					return state::conflict;
				}
				std::optional<application::staged_review_comment> candidate;
				for (const auto& comment : comments->Comments) {
					if (std::ranges::binary_search(aRecord.BaselineIds, comment.Id) || std::ranges::find(aRecord.Comments, comment.Id, &application::staged_review_comment::Id) != aRecord.Comments.end() || (aRecord.Index && aRecord.Index->Id == comment.Id) || comment.Actor != actor(aRecord.Plan) || comment.Body != aRecord.Pending->Body) {
						continue;
					}
					if (candidate) {
						return state::outcome_unknown;
					}
					candidate = comment;
				}
				if (!candidate) {
					return state::outcome_unknown;
				}
				if (next.Comments.size() < next.Plan.Stops.size()) {
					next.Comments.push_back(*candidate);
				} else {
					next.Index = *candidate;
				}
			} else if (aRecord.Pending->Operation == operation::update_issue_comment) {
				next.Comments[next.Patched].Body = aRecord.Pending->Body;
				if (!owned_comments(comments->Comments, next)) {
					return owned_comments(comments->Comments, aRecord) ? state::outcome_unknown : state::conflict;
				}
				++next.Patched;
				next.Published = !next.Plan.Update && next.Patched == next.Comments.size();
			} else {
				return state::conflict;
			}
		} else if (!aRecord.Review && !aRecord.Plan.Compatibility && aRecord.Pending->Operation == operation::create_pending) {
			const auto reviews = observe_staged(request(aRecord, operation::list_reviews), aStop);
			if (!reviews) {
				return state::outcome_unknown;
			}
			std::optional<application::staged_review_entity> candidate;
			std::vector<application::staged_review_comment>  membership;
			for (const auto& review : reviews->Reviews) {
				if (std::ranges::binary_search(aRecord.BaselineIds, review.Id.value()) || review.Actor != actor(aRecord.Plan) || review.Commit != aRecord.Plan.Submission.LiveBinding->Target.Semantic.ExpectedHead || review.Body != aRecord.Pending->Body) {
					continue;
				}
				const auto comments = observe_staged_comments(aRecord, review, aStop);
				if (!comments) {
					return state::outcome_unknown;
				}
				const auto matched = initial_comments(aRecord.Plan, review.Id, comments->Comments, comments->Lines);
				if (!matched) {
					continue;
				}
				if (candidate) {
					return state::outcome_unknown;
				}
				candidate  = review;
				membership = *matched;
			}
			if (!candidate) {
				return state::outcome_unknown;
			}
			if (candidate->State != application::staged_review_state::pending) {
				return state::conflict;
			}
			next.Review   = *candidate;
			next.Comments = std::move(membership);
		} else if (aRecord.Pending->Operation == operation::update_description) {
			const auto wire = compile_target_observation_request(Configuration.Target, Configuration.Target.ServerContract);
			if (!wire) {
				return state::outcome_unknown;
			}
			const auto  answer  = perform(*wire, aStop);
			const auto* success = std::get_if<github_mutation_wire_success>(&answer);
			if (!success) {
				return state::outcome_unknown;
			}
			const auto parsed = parse_staged_review_response(success->Response, *aRecord.Pending);
			if (!parsed || !parsed->Description) {
				return state::outcome_unknown;
			}
			if (*parsed->Description == aRecord.Plan.Description->Baseline) {
				return state::outcome_unknown;
			}
			if (*parsed->Description != aRecord.Pending->Body) {
				return state::conflict;
			}
			next.DescriptionDone = true;
		} else {
			const auto reviews = observe_staged(request(aRecord, operation::read_review), aStop);
			if (!reviews || reviews->Reviews.size() != 1) {
				return state::outcome_unknown;
			}
			const auto comments = observe_staged_comments(aRecord, reviews->Reviews.front(), aStop);
			if (!comments) {
				return state::outcome_unknown;
			}
			const auto& review = reviews->Reviews.front();
			const auto  bodies = application::staged_final_bodies(aRecord);
			if (!bodies) {
				return state::conflict;
			}
			auto expected = aRecord.Comments;
			if (aRecord.Pending->Operation == operation::update_comment) {
				if (review != *aRecord.Review) {
					return state::conflict;
				}
				expected[aRecord.Patched].Body = aRecord.Pending->Body;
				if (!ordered_comments(comments->Comments, expected, comments->Lines)) {
					return ordered_comments(comments->Comments, aRecord.Comments, comments->Lines) ? state::outcome_unknown : state::conflict;
				}
				next.Comments = std::move(expected);
				++next.Patched;
			} else if (aRecord.Pending->Operation == operation::submit_comment) {
				if (!ordered_comments(comments->Comments, aRecord.Comments, comments->Lines)) {
					return state::conflict;
				}
				auto expectedReview  = *aRecord.Review;
				expectedReview.Body  = bodies->first;
				expectedReview.State = application::staged_review_state::commented;
				if (review != expectedReview) {
					return review == *aRecord.Review ? state::outcome_unknown : state::conflict;
				}
				next.Review    = review;
				next.Published = true;
			} else {
				return state::conflict;
			}
		}
		if (next.Plan.Update && !verify_tour_update(next, aStop)) {
			return state::conflict;
		}
		next.Pending.reset();
		next.PossiblyTransmitted = false;
		++next.Revision;
		const auto saved = Recovery->advance_staged(aRecord, next);
		if (!saved || *saved != next) {
			return state::outcome_unknown;
		}
		aRecord = next;
		return status(aRecord);
	}
	auto review_submission_session::execute_tour(const application::staged_review_plan& aPlan, std::stop_token aStop, bool aWrite, application::staged_child_authorizer aAuthorize) -> application::staged_publication_result
	{
		std::scoped_lock lock{ Mutex };
		try {
			if ((aWrite && !aAuthorize) || !application::valid(aPlan) || !Recovery || !Lease || !Lease->active() || !valid(Configuration)
				|| !application::same_review_submission_identity(*aPlan.Submission.LiveBinding, { Configuration.Target, Configuration.ActingIdentity })) {
				return { };
			}
			auto loaded = Recovery->load_staged(aPlan.Submission.IdempotencyKey);
			if (!loaded) {
				return { };
			}
			record current;
			if (*loaded) {
				current = **loaded;
				if (current.Plan != aPlan || !application::valid(current)) {
					return { state::conflict, current };
				}
			} else {
				if (!aWrite) {
					return { state::outcome_unknown, { } };
				}
				if (aPlan.Submission.LiveBinding->Target != Configuration.Target || !observe_exact(aStop)) {
					return { state::conflict, { } };
				}
				current.Plan    = aPlan;
				current.Pending = application::staged_initial_request(aPlan);
				if (!current.Pending || !compile_staged_review_request(*current.Pending)) {
					return { };
				}
				const auto baseline = observe_staged(request(current, aPlan.Compatibility ? operation::list_issue_comments : operation::list_reviews), aStop);
				if (!baseline) {
					return { };
				}
				for (const auto& comment : baseline->Comments) {
					current.BaselineIds.push_back(comment.Id);
				}
				for (const auto& review : baseline->Reviews) {
					if (review.State == application::staged_review_state::pending) {
						return { state::conflict, { } };
					}
					current.BaselineIds.push_back(review.Id.value());
				}
				std::ranges::sort(current.BaselineIds);
				const auto admitted = Recovery->advance_staged(std::nullopt, current);
				if (!admitted || *admitted != current) {
					return { };
				}
			}
			for (std::size_t step = 0; step < 2 * aPlan.Stops.size() + (aPlan.Update ? aPlan.Update->Stops.size() + aPlan.Update->Retired.size() + 8 : 4); ++step) {
				if (current.PossiblyTransmitted) {
					const auto reconciled = reconcile_staged(current, aStop);
					if (reconciled == state::outcome_unknown || reconciled == state::conflict) {
						return { reconciled, current };
					}
				}
				if (!aWrite || aStop.stop_requested()) {
					if (!verify_staged(current, aStop)) {
						return { state::conflict, current };
					}
					return { status(current), current };
				}
				if (!current.Pending) {
					const auto nextRequest = application::staged_next_request(current);
					if (!nextRequest) {
						return { status(current), current };
					}
					auto next    = current;
					next.Pending = *nextRequest;
					++next.Revision;
					const auto saved = Recovery->advance_staged(current, next);
					if (!saved || *saved != next) {
						return { state::outcome_unknown, current };
					}
					current = next;
				}
				if (aPlan.Submission.LiveBinding->Target != Configuration.Target || !verify_staged(current, aStop)) {
					return { current.Published ? state::description_attention : state::conflict, current };
				}
				const auto wire = compile_staged_review_request(*current.Pending);
				if (!wire) {
					return { state::conflict, current };
				}
				const auto before = current;
				auto       used   = std::make_shared<std::atomic_bool>(false);
				try {
					static_cast<void>(perform(*wire, aStop, [&, used, before] {
						if (used->exchange(true) || aStop.stop_requested() || !Lease->active() || !aAuthorize(before)) {
							return false;
						}
						auto next                = before;
						next.PossiblyTransmitted = true;
						++next.Revision;
						const auto saved = Recovery->advance_staged(before, next);
						return saved && *saved == next;
					}));
				} catch (...) { /* Durable witness decides whether a write may have escaped. */
				}
				const auto after = Recovery->load_staged(aPlan.Submission.IdempotencyKey);
				if (!after || !*after || (**after).Plan != aPlan || !application::valid(**after)) {
					return { state::outcome_unknown, current };
				}
				current = **after;
				if (!current.PossiblyTransmitted) {
					return { status(current), current };
				}
				const auto reconciled = reconcile_staged(current, { });
				if (reconciled == state::outcome_unknown || reconciled == state::conflict) {
					return { reconciled, current };
				}
			}
			return { status(current), current };
		} catch (...) {
			return { state::outcome_unknown, { } };
		}
	}
	auto review_submission_session::publish_tour(const application::staged_review_plan& aPlan, std::stop_token aStop, application::staged_child_authorizer aAuthorize) -> application::staged_publication_result { return execute_tour(aPlan, aStop, true, std::move(aAuthorize)); }
	auto review_submission_session::reconcile_tour(const application::staged_review_plan& aPlan, std::stop_token aStop) -> application::staged_publication_result { return execute_tour(aPlan, aStop, false); }
	auto review_submission_session::pending_tours(std::stop_token aStop) -> std::expected<std::vector<application::staged_review_plan>, application::review_mutation_failure>
	{
		std::scoped_lock lock{ Mutex };
		try {
			if (aStop.stop_requested() || !Recovery || !Lease || !Lease->active()) {
				return std::unexpected(application::review_mutation_failure::unavailable);
			}
			const auto records = Recovery->pending_staged();
			if (!records) {
				return std::unexpected(application::review_mutation_failure::unavailable);
			}
			std::vector<application::staged_review_plan> plans;
			for (const auto& value : *records) {
				if (!application::valid(value) || !application::same_review_submission_identity(*value.Plan.Submission.LiveBinding, { Configuration.Target, Configuration.ActingIdentity })) {
					return std::unexpected(application::review_mutation_failure::unavailable);
				}
				plans.push_back(value.Plan);
			}
			return plans;
		} catch (...) {
			return std::unexpected(application::review_mutation_failure::unavailable);
		}
	}
}
