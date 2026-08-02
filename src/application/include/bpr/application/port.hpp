#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PORT_HPP

#include "bpr/core/model.hpp"

#include <functional>

namespace bpr::application {
	using completion_callback = std::function<void(core::application_event)>;

	/// \brief Provides the foundation observation port contract.
	class foundation_observation_port
	{
	public:
		virtual ~foundation_observation_port() = default;

		[[nodiscard]] virtual auto observe() const
			-> core::foundation_observation = 0;
	};

	/// \brief Provides the foundation effect port contract.
	class foundation_effect_port
	{
	public:
		virtual ~foundation_effect_port() = default;

		virtual void submit(
			const core::refresh_foundation_request& aRequest,
			completion_callback                     aCompletion) = 0;
		virtual void cancel(core::request_id aRequest)           = 0;
	};

	/// \brief Provides the decision executor contract.
	class decision_executor
	{
	public:
		virtual ~decision_executor() = default;

		// Enqueue without executing the task inline or waiting for it to finish.
		// While a host is live and until it is quiescent, this lane is a required
		// service and must accept every post without throwing. A successful return
		// retains the task exactly once for eventual invocation. Completion
		// callbacks may post while a host lifetime gate or application turn is
		// active; inline execution or a cross-thread wait would violate
		// serialization or invert that gate.
		virtual void post(std::function<void()> aTask) = 0;
	};

	/// \brief Provides the effect executor contract.
	class effect_executor
	{
	public:
		virtual ~effect_executor() = default;

		// A successful return retains the task exactly once for later off-lane
		// invocation. If post throws, it must do so before retaining the task.
		// This gives hosts an exact synchronous rejection boundary without a
		// possible enqueue-then-throw duplicate terminal.
		virtual void post(std::function<void()> aTask) = 0;
	};

	/// \brief Provides the projection port contract.
	class projection_port
	{
	public:
		virtual ~projection_port() = default;

		virtual void publish(const core::transition_output& aOutput) = 0;
	};
}

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PORT_HPP
