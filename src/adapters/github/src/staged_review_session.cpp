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
			return aRecord.Review ? state::staged : state::ready;
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
		auto ordered_comments(const std::vector<application::staged_review_comment>& aObserved, const std::vector<application::staged_review_comment>& aExpected) -> std::optional<std::vector<application::staged_review_comment>>
		{
			if (aObserved.size() != aExpected.size()) {
				return std::nullopt;
			}
			std::vector<application::staged_review_comment> result;
			for (const auto& expected : aExpected) {
				const auto found = std::ranges::find(aObserved, expected.Id, &application::staged_review_comment::Id);
				if (found == aObserved.end() || *found != expected) {
					return std::nullopt;
				}
				result.push_back(*found);
			}
			return result;
		}
		auto initial_comments(const application::staged_review_plan& aPlan, application::review_database_id aReview, const std::vector<application::staged_review_comment>& aObserved) -> std::optional<std::vector<application::staged_review_comment>>
		{
			const auto initial = application::staged_initial_request(aPlan);
			if (!initial || aObserved.size() != initial->Lines.size()) {
				return std::nullopt;
			}
			std::vector<application::staged_review_comment> result;
			for (const auto& line : initial->Lines) {
				std::optional<application::staged_review_comment> match;
				for (const auto& observed : aObserved) {
					if (observed.Body == line.Body && observed.Source == std::optional{ line.Source } && observed.Actor == actor(aPlan) && observed.Review == aReview && !observed.Reply) {
						if (match) {
							return std::nullopt;
						}
						match = observed;
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
	auto review_submission_session::verify_staged(const record& aRecord, std::stop_token aStop) -> bool
	{
		if (!observe_exact(aStop)) {
			return false;
		}
		if (!aRecord.Review) {
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
		const auto reviews  = observe_staged(request(aRecord, operation::read_review), aStop);
		const auto comments = observe_staged(request(aRecord, operation::list_comments), aStop);
		if (!reviews || reviews->Reviews.size() != 1 || reviews->Reviews.front() != *aRecord.Review || !comments || !ordered_comments(comments->Comments, aRecord.Comments)) {
			return false;
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
		if (!aRecord.Review) {
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
				auto read           = request(aRecord, operation::list_reviews);
				read.Operation      = operation::list_comments;
				read.Review         = review.Id;
				const auto comments = observe_staged(read, aStop);
				if (!comments) {
					return state::outcome_unknown;
				}
				const auto matched = initial_comments(aRecord.Plan, review.Id, comments->Comments);
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
			if (*parsed->Description != aRecord.Plan.Description->Desired) {
				return state::conflict;
			}
			next.DescriptionDone = true;
		} else {
			const auto reviews  = observe_staged(request(aRecord, operation::read_review), aStop);
			const auto comments = observe_staged(request(aRecord, operation::list_comments), aStop);
			if (!reviews || reviews->Reviews.size() != 1 || !comments) {
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
				if (!ordered_comments(comments->Comments, expected)) {
					return ordered_comments(comments->Comments, aRecord.Comments) ? state::outcome_unknown : state::conflict;
				}
				next.Comments = std::move(expected);
				++next.Patched;
			} else if (aRecord.Pending->Operation == operation::submit_comment) {
				if (!ordered_comments(comments->Comments, aRecord.Comments)) {
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
			if ((aWrite && !aAuthorize) || !application::valid(aPlan) || aPlan.Compatibility || !Recovery || !Lease || !Lease->active() || !valid(Configuration)
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
				const auto baseline = observe_staged(request(current, operation::list_reviews), aStop);
				if (!baseline) {
					return { };
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
			for (std::size_t step = 0; step < aPlan.Stops.size() + 4; ++step) {
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
