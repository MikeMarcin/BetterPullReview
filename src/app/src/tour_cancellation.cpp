#include "tour_cancellation.hpp"
namespace bpr::app {
	namespace {
		volatile std::sig_atomic_t Cancelled = 0;
		void                       cancel(int) { Cancelled = 1; }
	}
	tour_signal_scope::tour_signal_scope()
		: Interrupt{ std::signal(SIGINT, cancel) }
		, Terminate{ std::signal(SIGTERM, cancel) }
	{
		Cancelled = 0;
	}
	tour_signal_scope::~tour_signal_scope()
	{
		std::signal(SIGINT, Interrupt);
		std::signal(SIGTERM, Terminate);
	}
	auto tour_cancelled() noexcept -> bool { return Cancelled != 0; }
}
