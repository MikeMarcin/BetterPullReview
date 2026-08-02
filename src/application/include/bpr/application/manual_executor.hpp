#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_MANUAL_EXECUTOR_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_MANUAL_EXECUTOR_HPP

#include "bpr/application/port.hpp"

#include <cstddef>
#include <deque>

namespace bpr::application {
	/// \brief Provides the manual executor contract.
	class manual_executor final : public decision_executor, public effect_executor
	{
	public:
		void post(std::function<void()> aTask) override;

		[[nodiscard]] auto pending() const noexcept -> std::size_t;
		void               run(std::size_t aIndex);
		void               run_next();
		void               drain();

	private:
		std::deque<std::function<void()>> Tasks;
	};
}

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_MANUAL_EXECUTOR_HPP
