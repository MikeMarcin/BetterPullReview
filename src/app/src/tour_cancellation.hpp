#ifndef BPR_APP_TOUR_CANCELLATION_HPP
#define BPR_APP_TOUR_CANCELLATION_HPP
#include <csignal>
namespace bpr::app {
	/// One foreground command owns signal handling. No signal handler runs C++ effects.
	class tour_signal_scope final
	{
	public:
		tour_signal_scope();
		~tour_signal_scope();
		tour_signal_scope(const tour_signal_scope&)                    = delete;
		auto operator=(const tour_signal_scope&) -> tour_signal_scope& = delete;

	private:
		using handler = void (*)(int);
		handler Interrupt, Terminate;
	};
	[[nodiscard]] auto tour_cancelled() noexcept -> bool;
}
#endif
