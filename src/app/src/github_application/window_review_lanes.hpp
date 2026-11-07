#ifndef BPR_APP_GITHUB_APPLICATION_WINDOW_REVIEW_LANES_HPP
#define BPR_APP_GITHUB_APPLICATION_WINDOW_REVIEW_LANES_HPP
#include "bpr/application/port.hpp"
#include "bpr/application/review_queue_enrollment.hpp"
#include "bpr/application/window_review_restoration_ports.hpp"
#include "bpr/core/diff_model.hpp"
#include "bpr/core/profile_window.hpp"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <utility>
#include <vector>

// Per-window queue projection, read decision lane and review store, and
// the review ledger's receipt changes and exclusive file executor.
namespace bpr::app {
	class window_queue_projection final : public application::queue_window_projection_port
	{
	public:
		using sink = std::function<void(const core::bpr_window_context&, const application::queue_landing_projection&)>;
		explicit window_queue_projection(sink aSink);
		void publish(const core::bpr_window_context& aContext, const application::queue_landing_projection& aProjection) override;

	private:
		sink Sink;
	};

	class window_read_decision_lane final : public application::decision_executor
	{
	public:
		explicit window_read_decision_lane(application::decision_executor& aDecision);
		void               post(std::function<void()> aTask) override;
		void               hold(bool aHold);
		[[nodiscard]] auto held() const noexcept -> std::size_t;

	private:
		struct state
		{
			bool                               Held = false;
			std::vector<std::function<void()>> Pending;
		};
		application::decision_executor& Decision;
		std::shared_ptr<state>          State = std::make_shared<state>();
	};

	class effect_window_review_store final : public application::window_review_restoration_port
	{
	public:
		explicit effect_window_review_store(std::filesystem::path aRoot);
		auto load(std::stop_token aStop) -> application::window_review_store_result<application::window_review_snapshot> override;
		auto save(const application::window_review_snapshot& aCandidate, std::uint64_t aRevision, std::stop_token aStop) -> application::window_review_store_result<application::window_review_snapshot> override;

	private:
		std::filesystem::path Root;
	};

	/// [[BPR-0972]]. More changed receipt files than this in one coalesced
	/// delivery invalidate every retained receipt instead of naming each one.
	inline constexpr std::size_t maximum_named_receipt_changes = 256U;

	/// Shared services outlive every window session that borrows them.
	/// Accepted ledger invalidations contain no candidate text or navigation state.
	struct pending_ledger_changes
	{
		std::mutex                       Mutex;
		bool                             Scheduled      = false;
		bool                             Receipts       = false;
		bool                             AllReceipts    = false;
		bool                             Conversations  = false;
		bool                             PrivateRecords = false;
		std::vector<core::file_identity> ReceiptFiles;
	};

	struct ledger_changes
	{
		bool Receipts = false;
		/// [[BPR-0972]]. The files whose receipts changed, unless `AllReceipts`
		/// says a change named none or too many were coalesced.
		bool                             AllReceipts    = false;
		bool                             Conversations  = false;
		bool                             PrivateRecords = false;
		std::vector<core::file_identity> ReceiptFiles;
		/// [[BPR-0972]]. The window whose ledger store accepted the writes.
		/// Its own host already applied its receipt and conversation writes.
		std::optional<core::bpr_window_context> Origin;
	};

	/// \brief [[BPR-1254]]. Runs each task of one lane while holding the
	/// exclusion that every other in-process user of the same file holds too.
	///
	/// \details The profile's review ledger is one SQLite file. Each window's
	/// ledger host reaches it through this lane, and each window's private
	/// review store reaches it from the mutation lane and from agent turns.
	/// Opening it validates a copy of the file and then refuses the file if it
	/// changed meanwhile, so a read or write by another connection during that
	/// window turned an ordinary reopen into `unavailable`. Holding one
	/// exclusion around every task here, and around every private store
	/// operation, means only one of them touches the file at a time.
	class exclusive_file_effect_executor final : public application::effect_executor
	{
	public:
		exclusive_file_effect_executor(application::effect_executor& aLane, std::mutex& aFile) noexcept;

		void post(std::function<void()> aTask) override;

	private:
		application::effect_executor& Lane;
		std::mutex&                   File;
	};
}
#endif
