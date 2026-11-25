#ifndef BPR_SRC_ADAPTERS_AGENT_SRC_NATIVE_AGENT_CONNECTION_HPP
#define BPR_SRC_ADAPTERS_AGENT_SRC_NATIVE_AGENT_CONNECTION_HPP

#include "bpr/adapters/agent/agent_process_launcher.hpp"

#include <atomic>
#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

namespace bpr::adapters::agent::detail {
	/// Owns one process thread. A completed turn releases its caller while the
	/// supervised process stays here, polling bounded idle output.
	class native_agent_connection final : public application::agent_native_connection_port
	{
	public:
		struct turn
		{
			application::agent_launch_request           Request;
			int                                         Timeout = 0;
			application::agent_output_sink              Sink;
			std::stop_token                             Stop;
			std::promise<application::agent_run_result> Answer;
		};

		explicit native_agent_connection(const application::agent_launch_request& aTemplate)
			: Template{ aTemplate }
		{ }
		~native_agent_connection() override
		{
			stop();
			if (Worker.joinable()) {
				Worker.join();
			}
		}
		native_agent_connection(const native_agent_connection&)                    = delete;
		auto operator=(const native_agent_connection&) -> native_agent_connection& = delete;

		[[nodiscard]] auto matches(const application::agent_launch_request& aRequest) const -> bool
		{
			return !Stopping.load() && Template.Protocol == aRequest.Protocol && Template.Executable == aRequest.Executable
				&& Template.Arguments == aRequest.Arguments && Template.Environment == aRequest.Environment
				&& Template.WorkingDirectory == aRequest.WorkingDirectory && Template.ReadOnlyTools == aRequest.ReadOnlyTools
				&& Template.RunChecks == aRequest.RunChecks && Template.Metadata.Requested == aRequest.Metadata.Requested
				&& Template.Metadata.Configured == aRequest.Metadata.Configured;
		}
		[[nodiscard]] auto done() const noexcept -> bool { return Done.load(); }
		/// \brief Where its agent works.
		[[nodiscard]] auto working_directory() const noexcept -> const std::string& { return Template.WorkingDirectory; }
		[[nodiscard]] auto busy() const -> bool
		{
			const auto lock = std::scoped_lock{ Mutex };
			return Current || Pending;
		}
		void stop() noexcept { Stopping = true; }
		void retire() noexcept override { stop(); }

		[[nodiscard]] auto submit(const application::agent_launch_request& aRequest, const int aTimeout,
								  const application::agent_output_sink& aSink, const std::stop_token aStop)
			-> application::agent_run_result
		{
			auto job     = std::make_shared<turn>();
			job->Request = aRequest;
			job->Timeout = aTimeout;
			job->Sink    = aSink;
			job->Stop    = aStop;
			auto answer  = job->Answer.get_future();
			{
				const auto lock = std::scoped_lock{ Mutex };
				if (Done || Stopping || Current || Pending) {
					return { .Session = aRequest.Session, .Outcome = application::agent_run_outcome::failed_to_start, .Run = aRequest.Run };
				}
				Pending = job;
				if (!Worker.joinable()) {
					Worker = std::thread{ [this] { serve(); } };
				}
			}
			return answer.get();
		}

		[[nodiscard]] auto next() -> std::optional<application::agent_launch_request> override
		{
			const auto lock = std::scoped_lock{ Mutex };
			if (Stopping || !Pending || Current) {
				return std::nullopt;
			}
			Current                  = std::exchange(Pending, { });
			auto request             = Current->Request;
			request.NativeConnection = this;
			return request;
		}
		void complete(application::agent_run_result aResult) override
		{
			auto job    = std::shared_ptr<turn>{ };
			auto steers = std::deque<application::agent_pending_steer>{ };
			{
				const auto lock = std::scoped_lock{ Mutex };
				job             = std::exchange(Current, { });
				steers          = std::exchange(Steers, { });
			}
			// [[BPR-2153]]. A correction the loop did not take before the turn
			// ended was never written: it may be queued instead.
			refuse(steers, core::agent_steer_support::not_running);
			if (job) {
				job->Answer.set_value(std::move(aResult));
			}
		}

		/// \brief [[BPR-2153]], RFC-BPR-0126 Q14. Holds a correction for the
		/// running turn's protocol loop, which writes it after its current
		/// read; refuses it at once when no turn of `aRequest.Run` runs here.
		void steer(application::agent_steer_request aRequest, application::agent_steer_answer aAnswer)
		{
			auto refusal = core::agent_steer_support::not_running;
			{
				const auto lock = std::scoped_lock{ Mutex };
				if (!Stopping && !Done && Current && Current->Request.Run == aRequest.Run && Current->Request.NativeTurn == aRequest.NativeTurn) {
					if (Steers.size() < core::maximum_steers_per_run) {
						Steers.push_back({ .Request = std::move(aRequest), .Answer = std::move(aAnswer) });
						return;
					}
					refusal = core::agent_steer_support::exhausted;
				} else if (!Stopping && !Done && Current) {
					refusal = core::agent_steer_support::other_run;
				}
			}
			auto refused = std::deque<application::agent_pending_steer>{ };
			refused.push_back({ .Request = std::move(aRequest), .Answer = std::move(aAnswer) });
			refuse(refused, refusal);
		}

		void observe_tool_version(const std::string& aVersion) override
		{
			const auto lock = std::scoped_lock{ Mutex };
			ToolVersion     = aVersion;
		}

		/// \brief [[BPR-2153]]. Whether the turn `aRequest` names runs here
		/// and could take a correction, by the version its runtime reported.
		[[nodiscard]] auto steer_support(const application::agent_steer_request& aRequest) const -> core::agent_steer_support
		{
			const auto lock = std::scoped_lock{ Mutex };
			if (Stopping || Done || !Current) {
				return core::agent_steer_support::not_running;
			}
			if (Current->Request.Run != aRequest.Run || Current->Request.NativeTurn != aRequest.NativeTurn) {
				return core::agent_steer_support::other_run;
			}
			if (Steers.size() >= core::maximum_steers_per_run) {
				return core::agent_steer_support::exhausted;
			}
			return core::agent_steer_support_of(Template.Protocol, true, ToolVersion);
		}

		[[nodiscard]] auto take_steer() -> std::optional<application::agent_pending_steer> override
		{
			const auto lock = std::scoped_lock{ Mutex };
			if (Steers.empty()) {
				return std::nullopt;
			}
			auto steer = std::move(Steers.front());
			Steers.pop_front();
			return steer;
		}
		[[nodiscard]] auto timeout_milliseconds() const -> int override
		{
			const auto lock = std::scoped_lock{ Mutex };
			return Current ? Current->Timeout : 0;
		}
		[[nodiscard]] auto cancelled() const noexcept -> bool override
		{
			const auto lock = std::scoped_lock{ Mutex };
			return Stopping || (Current && Current->Stop.stop_requested());
		}

	private:
		static void refuse(std::deque<application::agent_pending_steer>& aSteers, const core::agent_steer_support aWhy) noexcept
		{
			for (auto& steer : aSteers) {
				try {
					if (steer.Answer) {
						steer.Answer({ .Disposition = core::agent_steer_disposition::unsent, .Support = aWhy });
					}
				} catch (...) { }
			}
		}
		void stream(const application::agent_stream_event& aEvent)
		{
			auto sink = application::agent_output_sink{ };
			{
				const auto lock = std::scoped_lock{ Mutex };
				if (Current) {
					sink = Current->Sink;
				}
			}
			if (sink) {
				sink(aEvent);
			}
		}
		void serve() noexcept
		{
			try {
				auto first = next();
				if (first) {
					auto prepared = application::prepare_native_agent_turn(std::move(*first), false);
					if (prepared) {
						auto launcher = agent_process_launcher{ };
						auto result   = launcher.run(*prepared, timeout_milliseconds(),
													 [this](const application::agent_stream_event& aEvent) { stream(aEvent); });
						// The protocol loop settles each turn. Launch failures occur
						// before that loop and still owe the first caller a terminal.
						complete(std::move(result));
					}
				}
			} catch (...) { }
			auto pending = std::shared_ptr<turn>{ };
			auto current = std::shared_ptr<turn>{ };
			auto steers  = std::deque<application::agent_pending_steer>{ };
			{
				const auto lock = std::scoped_lock{ Mutex };
				Done            = true;
				pending         = std::exchange(Pending, { });
				current         = std::exchange(Current, { });
				steers          = std::exchange(Steers, { });
			}
			refuse(steers, core::agent_steer_support::not_running);
			for (const auto& job : { current, pending }) {
				if (job) {
					try {
						job->Answer.set_value({ .Session = job->Request.Session, .Outcome = application::agent_run_outcome::failed_to_start, .Run = job->Request.Run });
					} catch (...) { }
				}
			}
		}
		application::agent_launch_request            Template;
		mutable std::mutex                           Mutex;
		std::shared_ptr<turn>                        Pending;
		std::shared_ptr<turn>                        Current;
		std::deque<application::agent_pending_steer> Steers;
		std::string                                  ToolVersion;
		std::atomic_bool                             Done     = false;
		std::atomic_bool                             Stopping = false;
		std::thread                                  Worker;
	};
}
#endif
