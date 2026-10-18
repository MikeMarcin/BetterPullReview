#ifndef BPR_ADAPTERS_PROCESS_SUPPORT_PROCESS_SUPERVISION_HPP
#define BPR_ADAPTERS_PROCESS_SUPPORT_PROCESS_SUPERVISION_HPP

#include <QProcess>
#include <QtGlobal>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <utility>

/// \brief QProcess supervision the gh and Git runners share ([[BPR-1436]]).
///
/// \details Adapters may not depend on each other, and the platform target is
/// header-only and free of Qt, so this is its own Qt Core target. It owns how a
/// launched child and its process group are waited for, killed and reaped, and
/// the serialized lane each runner submits its launches through. It decides
/// nothing about what a process runs: each runner keeps its own validation,
/// environment, launch flags, stream handling and typed results.
namespace bpr::adapters::process_support {
	using supervision_clock = std::chrono::steady_clock;

	/// \brief The longest a supervision wait blocks before it looks again.
	inline constexpr auto wait_slice = std::chrono::milliseconds{ 10 };

	/// \brief Which of a child's two output channels a read is for.
	enum class process_stream : std::uint8_t {
		standard_output,
		standard_error,
	};

	/// \brief One wait, in milliseconds, toward \p aDeadline.
	/// \returns Zero once the deadline has passed; otherwise at least one and at
	/// most `wait_slice`.
	[[nodiscard]] auto wait_milliseconds(supervision_clock::time_point aDeadline) noexcept -> int;

	/// \brief Sends SIGKILL to process group \p aProcessGroupId on Unix.
	/// \details Group ids of zero and one, and ids `pid_t` cannot hold, are
	/// ignored. Elsewhere this does nothing.
	auto kill_process_group(qint64 aProcessGroupId) noexcept -> void;

	/// \brief The group to kill: the one already known, or the child's own id.
	/// \details A runner launches each child as the leader of a new session, so
	/// its process id is its group id. Zero means there is no group yet.
	[[nodiscard]] auto resolve_process_group(const QProcess& aProcess, qint64 aKnownProcessGroupId) noexcept -> qint64;

	/// \brief One kill of a launch's tree: its process group, then its child.
	using process_tree_kill = std::function<void(QProcess&, qint64)>;

	/// \brief The default tree kill: `kill_process_group`, then
	/// `QProcess::kill` while the child still runs.
	auto kill_process_tree(QProcess& aProcess, qint64 aProcessGroupId) -> void;

	/// \brief Kills a launch's tree and waits for its child, until \p aTimeout.
	/// \param aKillTree Repeated on each wait slice. The Git runner's adds its
	/// Windows job.
	/// \returns Whether the child was reaped. A child that outlives the timeout
	/// is still running and still owned by the caller.
	[[nodiscard]] auto kill_process_group_and_reap(
		QProcess&                 aProcess,
		qint64                    aKnownProcessGroupId,
		std::chrono::milliseconds aTimeout,
		const process_tree_kill&  aKillTree = process_tree_kill{ &kill_process_tree }) -> bool;

	/// \brief The lane's last attempt on a child a failed launch could not reap.
	/// \details Kills and waits as `kill_process_group_and_reap` does, under its
	/// own \p aTimeout. Before [[BPR-1436]] the gh runner's copy had no deadline,
	/// so one child that never exited held its worker for good.
	/// \returns Whether the child was reaped. One that was not is abandoned.
	[[nodiscard]] auto finish_background_reap(
		QProcess&                 aProcess,
		qint64                    aKnownProcessGroupId,
		std::chrono::milliseconds aTimeout,
		const process_tree_kill&  aKillTree = process_tree_kill{ &kill_process_tree }) -> bool;

	/// \brief Gives up on a child the background reap could not reap.
	/// \details Destroying a running QProcess may wait without bound. The lane
	/// is quarantined for good by then, so it keeps this one wrapper until the
	/// operating system cleans up the process rather than make shutdown
	/// unbounded.
	auto abandon_unreaped_process(std::unique_ptr<QProcess>& aProcess) noexcept -> void;

	/// \brief The failures the lane itself reports, before or around a launch.
	enum class lane_failure : std::uint8_t {
		cancelled,
		timeout,
		reap_failure,
		start_failure,
	};

	/// \brief The completion state every lane work item carries.
	/// \details A runner derives its work item from this and adds its request.
	/// The submitter sets `Stop` and `Deadline` before submitting.
	template <typename TResult>
	struct process_work_state
	{
		std::stop_token               Stop;
		supervision_clock::time_point Deadline;
		std::mutex                    Mutex;
		std::condition_variable       Completed;
		std::optional<TResult>        Result;
	};

	/// \brief One worker thread that runs a runner's launches one at a time.
	///
	/// \details \p TTraits supplies the runner's types and effects:
	/// - `item`: derived from `process_work_state<result>`;
	/// - `result`: the runner's typed result;
	/// - `execution`: with `Result`, `std::unique_ptr<QProcess> AwaitingReap`
	///   and `qint64 ProcessGroupId`;
	/// - `static auto failure(lane_failure) -> result`;
	/// - `auto execute(item&, const std::atomic_bool&) -> execution`, which
	///   launches, supervises and, on failure, kills and reaps under its own
	///   bound, handing back in `AwaitingReap` a child it could not reap;
	/// - `auto finish_reap(execution&) -> bool`, the bounded background reap.
	///
	/// An execution that hands back a child quarantines the lane before its
	/// result is published, so no second launch starts beside a live child. The
	/// background reap then either reopens the lane or abandons the child and
	/// leaves the lane quarantined: every later submission fails with
	/// `reap_failure` without launching. A submitter whose item has not
	/// completed by its deadline plus \p aReapTimeout quarantines the busy lane
	/// and fails with `reap_failure`. The destructor stops and joins the worker.
	template <typename TTraits>
	class serialized_process_lane final
	{
	public:
		using item_type      = typename TTraits::item;
		using result_type    = typename TTraits::result;
		using execution_type = typename TTraits::execution;

		explicit serialized_process_lane(TTraits aTraits)
			: Traits{ std::move(aTraits) }
			, Worker{ [this] {
				worker_loop();
			} }
		{
		}

		serialized_process_lane(const serialized_process_lane&)                    = delete;
		auto operator=(const serialized_process_lane&) -> serialized_process_lane& = delete;

		~serialized_process_lane()
		{
			ShutdownRequested.store(true, std::memory_order_release);
			WorkAvailable.notify_all();
			SlotAvailable.notify_all();
			if (Worker.joinable()) {
				Worker.join();
			}
		}

		/// \brief Runs \p aItem on the worker and waits for its result.
		/// \param aItem Its `Stop` and `Deadline` are already set.
		/// \param aReapTimeout How long past the deadline the execution may take
		/// to kill and reap before this submission stops waiting for it.
		[[nodiscard]] auto submit(
			const std::shared_ptr<item_type>& aItem,
			const std::chrono::milliseconds   aReapTimeout) -> result_type
		{
			{
				std::unique_lock lock{ Mutex };
				if (ShutdownRequested.load(std::memory_order_acquire)) {
					return TTraits::failure(lane_failure::cancelled);
				}
				while (Busy) {
					if (
						ShutdownRequested.load(std::memory_order_acquire)
						|| aItem->Stop.stop_requested()) {
						return TTraits::failure(lane_failure::cancelled);
					}
					if (Quarantined) {
						return TTraits::failure(lane_failure::reap_failure);
					}
					if (supervision_clock::now() >= aItem->Deadline) {
						return TTraits::failure(lane_failure::timeout);
					}
					SlotAvailable.wait_for(lock, wait_slice);
				}
				// A lane whose background reap abandoned a child stays quarantined
				// after its worker is idle.
				if (Quarantined) {
					return TTraits::failure(lane_failure::reap_failure);
				}
				Busy    = true;
				Pending = aItem;
			}
			WorkAvailable.notify_one();

			std::unique_lock resultLock{ aItem->Mutex };
			const auto       completionDeadline = aItem->Deadline + aReapTimeout + wait_slice;
			if (
				!aItem->Completed.wait_until(
					resultLock,
					completionDeadline,
					[&] {
						return aItem->Result.has_value();
					})) {
				{
					std::lock_guard lock{ Mutex };
					if (Busy) {
						Quarantined = true;
					}
				}
				SlotAvailable.notify_all();
				return TTraits::failure(lane_failure::reap_failure);
			}
			return std::move(*aItem->Result);
		}

	private:
		auto publish(
			const std::shared_ptr<item_type>& aItem,
			result_type                       aResult) -> void
		{
			{
				std::lock_guard lock{ aItem->Mutex };
				aItem->Result.emplace(std::move(aResult));
			}
			aItem->Completed.notify_one();
		}

		[[nodiscard]] auto reap_in_background(execution_type& aExecution) noexcept -> bool
		{
			try {
				return Traits.finish_reap(aExecution);
			} catch (...) {
				return false;
			}
		}

		auto worker_loop() -> void
		{
			for (;;) {
				std::shared_ptr<item_type> item;
				{
					std::unique_lock lock{ Mutex };
					WorkAvailable.wait(
						lock,
						[&] {
							return Pending != nullptr
								|| ShutdownRequested.load(
									std::memory_order_acquire);
						});
					if (Pending == nullptr) {
						return;
					}
					item = std::move(Pending);
				}

				std::optional<execution_type> execution;
				try {
					execution.emplace(Traits.execute(*item, ShutdownRequested));
				} catch (...) {
					publish(
						item,
						TTraits::failure(lane_failure::start_failure));
					{
						std::lock_guard lock{ Mutex };
						Quarantined = false;
						Busy        = false;
					}
					SlotAvailable.notify_all();
					continue;
				}

				if (execution->AwaitingReap != nullptr) {
					{
						std::lock_guard lock{ Mutex };
						Quarantined = true;
					}
					SlotAvailable.notify_all();
				}
				publish(item, std::move(execution->Result));

				bool reaped = true;
				if (execution->AwaitingReap != nullptr) {
					reaped = reap_in_background(*execution);
					if (reaped) {
						execution->AwaitingReap.reset();
					} else {
						abandon_unreaped_process(execution->AwaitingReap);
					}
				}
				{
					std::lock_guard lock{ Mutex };
					Quarantined = !reaped;
					Busy        = false;
				}
				SlotAvailable.notify_all();
			}
		}

		TTraits                    Traits;
		std::mutex                 Mutex;
		std::condition_variable    WorkAvailable;
		std::condition_variable    SlotAvailable;
		std::shared_ptr<item_type> Pending;
		bool                       Busy              = false;
		bool                       Quarantined       = false;
		std::atomic_bool           ShutdownRequested = false;
		std::thread                Worker;
	};
}

#endif // BPR_ADAPTERS_PROCESS_SUPPORT_PROCESS_SUPERVISION_HPP
