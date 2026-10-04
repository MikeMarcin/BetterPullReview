#ifndef BPR_TESTS_WORKSPACE_CONTRACT_STORED_TOUR_PORT_HPP
#define BPR_TESTS_WORKSPACE_CONTRACT_STORED_TOUR_PORT_HPP

#include "bpr/adapters/sqlite/review_tour_store.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/review_cycle_promotion.hpp"
#include "bpr/application/review_tour_host.hpp"

#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>

/// \file
/// \brief [[BPR-1273]]. The tour owner a course gives Add Notes to Review
/// Tour when no window holds the draft: the profile's real tour store and a
/// short-lived `review_tour_host` per request, as the profile owner's stored
/// path does (`tour_owner_service`). Every reply is delivered once, after the
/// edit and its receipt are durable.
namespace bpr::tests::workspace_contract {
	class stored_tour_port : public application::review_cycle_tour_port
	{
	public:
		stored_tour_port(std::filesystem::path aRoot, const core::bpr_profile_id aProfile)
			: Root{ std::move(aRoot) }
			, Profile{ aProfile }
		{
		}

		void targets(targets_reply aReply) override
		{
			auto listed = adapters::sqlite::make_review_tour_store(Root, Profile)->targets({ });
			if (!listed) {
				aReply(std::unexpected(application::review_cycle_tour_failure::unanswered));
				return;
			}
			aReply(std::move(*listed));
		}

		void load(const application::tour_saved_target& aTarget, std::string aRequest, reply aReply) override
		{
			auto stored = adapters::sqlite::make_review_tour_store(Root, Profile)->load_state(aTarget.Owner, aTarget.Draft, { });
			if (!stored) {
				aReply(std::unexpected(application::review_cycle_tour_failure::unanswered));
				return;
			}
			auto answer = application::review_cycle_tour_answer{ .Draft = *stored ? (*stored)->Draft : seed(aTarget) };
			if (*stored && !aRequest.empty()) {
				const auto& receipts = (*stored)->Receipts;
				if (const auto found = std::ranges::find(receipts, aRequest, &application::tour_request_receipt::Id); found != receipts.end()) {
					answer.Receipt = *found;
				}
			}
			aReply(std::move(answer));
		}

		void execute(const application::tour_saved_target& aTarget, application::tour_service_request aRequest, reply aReply) override
		{
			aReply(run(aTarget, aRequest));
		}

		/// One request on a short-lived host, drained until its save is durable.
		auto run(const application::tour_saved_target& aTarget, const application::tour_service_request& aRequest) -> application::review_cycle_tour_result
		{
			auto lanes = application::manual_executor{ };
			auto host  = application::review_tour_host{ seed(aTarget), adapters::sqlite::make_review_tour_store(Root, Profile), lanes, lanes, { } };
			host.start();
			lanes.drain();
			const auto result  = host.execute(aRequest);
			const auto receipt = host.request_status(aRequest.RequestId);
			host.shutdown();
			while (!host.is_quiescent()) {
				lanes.drain();
			}
			const auto shown = host.presentation();
			if (result.Status == application::tour_service_status::busy || result.Status == application::tour_service_status::storage_failure || shown.Failure || !shown.Draft) {
				return std::unexpected(application::review_cycle_tour_failure::unanswered);
			}
			if (result.Status == application::tour_service_status::refused) {
				return std::unexpected(result.Refusal == core::tour_refusal::conflict ? application::review_cycle_tour_failure::conflict : application::review_cycle_tour_failure::refused);
			}
			return application::review_cycle_tour_answer{ .Draft = *shown.Draft, .Receipt = receipt };
		}

	private:
		static auto seed(const application::tour_saved_target& aTarget) -> core::review_tour
		{
			return { .Id = aTarget.Draft, .Owner = aTarget.Owner, .Base = aTarget.Base, .Head = aTarget.Head };
		}

		std::filesystem::path Root;
		core::bpr_profile_id  Profile;
	};
}

#endif
