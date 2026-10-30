#include "bpr/application/review_mutation_diagnostics.hpp"
#include "bpr/application/operation_diagnostics.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <utility>

namespace bpr::application {
	namespace {
		using action = core::operation_action;

		[[nodiscard]] auto bounded_count(const std::size_t aCount) noexcept -> std::uint32_t
		{
			return static_cast<std::uint32_t>(std::min<std::size_t>(aCount, std::numeric_limits<std::uint32_t>::max()));
		}

		/// What a call's end adds to its line.
		struct flags
		{
			std::uint32_t Count    = 0U;
			bool          Written  = false;
			bool          ReadBack = false;
		};

		class diagnosed_review_mutations final : public review_mutation_port
		{
		public:
			diagnosed_review_mutations(std::unique_ptr<review_mutation_port> aProvider, std::shared_ptr<diagnostic_log_port> aLog)
				: Provider{ std::move(aProvider) }
				, Log{ std::move(aLog) }
			{
			}

			[[nodiscard]] auto observe_target() -> std::expected<core::review_mutation_target, review_mutation_failure> override
			{
				const auto watch  = operation_stopwatch{ };
				auto       result = Provider->observe_target();
				finish(action::observe_target, watch, failure_of(result));
				return result;
			}
			[[nodiscard]] auto observe_target(std::stop_token aStop) -> std::expected<core::review_mutation_target, review_mutation_failure> override
			{
				const auto watch  = operation_stopwatch{ };
				auto       result = Provider->observe_target(std::move(aStop));
				finish(action::observe_target, watch, failure_of(result));
				return result;
			}
			[[nodiscard]] auto submit(const review_mutation_request& aRequest) -> std::expected<review_mutation_receipt, review_mutation_failure> override
			{
				const auto watch  = start(action::publish_comment);
				auto       result = Provider->submit(aRequest);
				finish(action::publish_comment, watch, failure_of(result), { .Written = true });
				return result;
			}
			[[nodiscard]] auto submit(const review_mutation_request& aRequest, std::stop_token aStop) -> std::expected<review_mutation_receipt, review_mutation_failure> override
			{
				const auto watch  = start(action::publish_comment);
				auto       result = Provider->submit(aRequest, std::move(aStop));
				finish(action::publish_comment, watch, failure_of(result), { .Written = true });
				return result;
			}
			[[nodiscard]] auto submit_and_reconcile(const review_mutation_request& aRequest, std::stop_token aStop) -> review_mutation_submission override
			{
				const auto watch  = start(action::publish_comment);
				auto       result = Provider->submit_and_reconcile(aRequest, std::move(aStop));
				finish(action::publish_comment, watch, failure_of(result.Result), { .Count = reconciled(result), .Written = result.MutationAttempted });
				return result;
			}
			[[nodiscard]] auto resolve_thread(const review_thread_resolution_request& aRequest, std::stop_token aStop)
				-> std::expected<review_thread_resolution_receipt, review_mutation_failure> override
			{
				const auto kind   = aRequest.Resolve ? action::resolve_thread : action::unresolve_thread;
				const auto watch  = start(kind);
				auto       result = Provider->resolve_thread(aRequest, std::move(aStop));
				finish(kind, watch, failure_of(result), { .Written = result.has_value() });
				return result;
			}
			[[nodiscard]] auto set_reaction(const review_reaction_request& aRequest, std::stop_token aStop) -> std::expected<review_reaction_receipt, review_mutation_failure> override
			{
				const auto kind   = aRequest.Add ? action::add_reaction : action::remove_reaction;
				const auto watch  = start(kind);
				auto       result = Provider->set_reaction(aRequest, std::move(aStop));
				finish(kind, watch, failure_of(result), result ? flags{ .Written = result->Written, .ReadBack = result->ConfirmedByReadBack } : flags{ });
				return result;
			}
			[[nodiscard]] auto edit_description(const review_description_request& aRequest, std::stop_token aStop)
				-> std::expected<review_description_receipt, review_mutation_failure> override
			{
				const auto watch  = start(action::edit_description);
				auto       result = Provider->edit_description(aRequest, std::move(aStop));
				finish_edit(action::edit_description, watch, result);
				return result;
			}
			[[nodiscard]] auto edit_comment(const review_comment_edit_request& aRequest, std::stop_token aStop)
				-> std::expected<review_comment_edit_receipt, review_mutation_failure> override
			{
				const auto watch  = start(action::edit_comment);
				auto       result = Provider->edit_comment(aRequest, std::move(aStop));
				finish_edit(action::edit_comment, watch, result);
				return result;
			}
			[[nodiscard]] auto reviewers(const review_reviewer_request& aRequest, std::stop_token aStop) -> std::expected<review_reviewer_receipt, review_mutation_failure> override
			{
				const auto request = aRequest.Operation == review_reviewer_operation::request;
				const auto kind    = request ? action::request_reviewer : action::search_reviewers;
				const auto watch   = request ? start(kind) : operation_stopwatch{ };
				auto       result  = Provider->reviewers(aRequest, std::move(aStop));
				auto       marks   = flags{ };
				if (result) {
					marks = { .Count    = bounded_count(request ? result->Answer.Requested.size() : result->Answer.Matches.size()),
							  .Written  = result->Written,
							  .ReadBack = result->ConfirmedByReadBack };
				}
				finish(kind, watch, failure_of(result), marks);
				return result;
			}
			[[nodiscard]] auto reconcile_publication(const review_mutation_request& aRequest, std::stop_token aStop) -> review_mutation_submission override
			{
				const auto watch  = operation_stopwatch{ };
				auto       result = Provider->reconcile_publication(aRequest, std::move(aStop));
				finish(action::reconcile_publication, watch, failure_of(result.Result), { .Count = reconciled(result) });
				return result;
			}
			void repair_pending_publications(std::stop_token aStop) override
			{
				const auto watch = operation_stopwatch{ };
				const auto stop  = aStop.stop_requested();
				Provider->repair_pending_publications(aStop);
				finish(action::repair_publications, watch, stop || aStop.stop_requested() ? core::operation_failure::cancelled : core::operation_failure::none);
			}
			[[nodiscard]] auto capabilities() const -> review_provider_capabilities override { return Provider->capabilities(); }

		private:
			template <typename TValue>
			[[nodiscard]] static auto failure_of(const std::expected<TValue, review_mutation_failure>& aResult) noexcept -> core::operation_failure
			{
				return aResult ? core::operation_failure::none : operation_failure_of(aResult.error());
			}
			[[nodiscard]] static auto reconciled(const review_mutation_submission& aSubmission) noexcept -> std::uint32_t
			{
				return aSubmission.Reconciliation ? bounded_count(aSubmission.Reconciliation->size()) : 0U;
			}

			[[nodiscard]] auto start(const action aAction) const noexcept -> operation_stopwatch
			{
				record_operation(Log.get(), core::operation_diagnosis{ .Action = aAction }, true);
				return { };
			}
			void finish(const action aAction, const operation_stopwatch& aWatch, const core::operation_failure aFailure, const flags aFlags = { }) const noexcept
			{
				record_operation(Log.get(), core::operation_diagnosis{ .Action = aAction, .Failure = aFailure, .DurationMilliseconds = aWatch.milliseconds(), .Count = aFlags.Count, .Written = aFlags.Written, .ReadBack = aFlags.ReadBack });
			}
			void finish_edit(const action aAction, const operation_stopwatch& aWatch, const std::expected<review_description_receipt, review_mutation_failure>& aResult) const noexcept
			{
				if (!aResult) {
					finish(aAction, aWatch, operation_failure_of(aResult.error()));
					return;
				}
				// GitHub's text changed since editing began: nothing was sent.
				finish(aAction, aWatch, aResult->Conflict ? core::operation_failure::conflict : core::operation_failure::none,
					   { .Written = aResult->Written, .ReadBack = aResult->ConfirmedByReadBack });
			}

			std::unique_ptr<review_mutation_port> Provider;
			std::shared_ptr<diagnostic_log_port>  Log;
		};
	} // namespace

	auto with_review_mutation_diagnostics(std::unique_ptr<review_mutation_port> aProvider, std::shared_ptr<diagnostic_log_port> aLog) -> std::unique_ptr<review_mutation_port>
	{
		if (aProvider == nullptr || aLog == nullptr) {
			return aProvider;
		}
		return std::make_unique<diagnosed_review_mutations>(std::move(aProvider), std::move(aLog));
	}
} // namespace bpr::application
