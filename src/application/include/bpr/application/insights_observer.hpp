#ifndef BPR_APPLICATION_INSIGHTS_OBSERVER_HPP
#define BPR_APPLICATION_INSIGHTS_OBSERVER_HPP
#include "bpr/core/insights.hpp"
#include <memory>
namespace bpr::application {
	/// \brief Nonblocking, content-free passive observation. Collection failure never changes an operation.
	class insights_observer
	{
	public:
		virtual ~insights_observer()                              = default;
		virtual void record(core::insights_event aEvent) noexcept = 0;
		virtual void select(core::insights_scope aScope) noexcept = 0;
	};
}
#endif
