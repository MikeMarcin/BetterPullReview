#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_SEND_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_SEND_HPP

#include "bpr/application/port.hpp"
#include "bpr/application/review_cycle_response.hpp"
#include "bpr/application/review_cycle_store.hpp"
#include "bpr/core/agent_profile.hpp"
#include "bpr/core/review_cycle_send.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

/// \file
/// \brief Send to Agent on the host side ([[BPR-1180]]): the port an
/// installed adapter fills, what the composition gives the workbench, and
/// recording what one send observed.
/// \details Nothing here launches anything: the adapter that does is part of
/// the agent integrations and absent from an agent-free build, where no
/// sender is composed and the route is not offered. Recording needs no
/// adapter, so an agent-free build reads every observation and response a
/// send left.
namespace bpr::application {
	/// How long one send may run before the adapter ends it.
	inline constexpr int review_cycle_send_timeout_milliseconds = 30 * 60 * 1000;

	/// \brief One assignment, as the adapter hands it to the harness.
	struct cycle_send_assignment
	{
		core::agent_profile      Profile;
		core::cycle_send_session Session;
		/// The round's readable document: the bytes Copy and Save give.
		std::string Document;
		/// Where the harness runs: the folder the cycle is bound to.
		std::string WorkingDirectory;
	};

	/// \brief An installed agent adapter: the existing launch policy, process
	/// and protocol handling, given one round.
	class review_cycle_sender
	{
	public:
		virtual ~review_cycle_sender() = default;
		/// \brief Runs one assignment to its end on the calling thread.
		virtual auto send(const cycle_send_assignment& aAssignment, std::stop_token aStop) -> core::cycle_send_result = 0;
		/// \brief Ends the running send, from any thread.
		virtual void cancel() noexcept = 0;
	};

	/// \brief What Send to Agent may use, set by the composition. Without a
	/// sender and a lane the route is not offered.
	struct review_cycle_send_environment
	{
		/// The installed profiles, as every other launch of them sees them.
		std::vector<core::agent_profile>     Profiles;
		std::shared_ptr<review_cycle_sender> Sender;
		/// Where a send runs: never the store's lane, which it would hold.
		effect_executor* Lane = nullptr;
		/// The host of the account this profile acts as, asked at each send.
		std::function<std::optional<std::string>()> AccountHost;
	};

	/// \brief What one send leaves to be recorded about its own round.
	struct cycle_send_record
	{
		core::cycle_round           Round;
		core::cycle_delivery_output Output;
		core::agent_protocol        Protocol = core::agent_protocol::external_terminal;
		core::cycle_send_result     Result;
		/// One per observation, then one for the response.
		std::vector<cycle_operation_id> Operations;
	};
	struct cycle_send_recorded
	{
		std::vector<cycle_write_receipt> Deliveries;
		/// The structured response the finished reply held: appended, or why not.
		std::optional<cycle_response_result> Response;
	};
	/// \brief Records one send against round `aRecord.Round` of `aSubject`,
	/// whatever is open now: its observations in order, then the structured
	/// response its finished reply holds, checked against exactly that round
	/// as any response is. A response about another round, or with other
	/// content, is refused and nothing is appended. Prose is left for the
	/// reviewer to add as a reply; nothing is captured or verified.
	[[nodiscard]] auto record_send(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, const cycle_send_record& aRecord, std::stop_token aStop)
		-> cycle_store_result<cycle_send_recorded>;
}

#endif
