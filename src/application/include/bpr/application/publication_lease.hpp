#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PUBLICATION_LEASE_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PUBLICATION_LEASE_HPP

#include "bpr/application/review_mutation_port.hpp"

#include <memory>
#include <mutex>

namespace bpr::application {
	/// \brief [[BPR-2265]]. A window's publishing lease as the decision lane
	/// last read it, for publication factories to read on their effect lanes.
	/// \details A factory holds a weak reference, never a lease: each call
	/// reads the one held now, so a lease a reread revoked and replaced is
	/// never the one used, and a factory outliving its window reads none.
	class publication_lease final
	{
	public:
		/// \brief Records the window's lease now; on the decision lane.
		void hold(review_mutation_transport aLease)
		{
			const auto lock = std::scoped_lock{ Mutex };
			Lease           = std::move(aLease);
		}
		/// \brief The lease held now when it is still active; otherwise none.
		[[nodiscard]] auto current() const -> review_mutation_transport
		{
			const auto lock = std::scoped_lock{ Mutex };
			return Lease && Lease->active() ? Lease : nullptr;
		}
		/// \brief The active lease of the window `aWindow` refers to, or none
		/// when it has none or is gone.
		[[nodiscard]] static auto read(const std::weak_ptr<const publication_lease>& aWindow) -> review_mutation_transport
		{
			const auto held = aWindow.lock();
			return held ? held->current() : nullptr;
		}

	private:
		mutable std::mutex        Mutex;
		review_mutation_transport Lease;
	};
}

#endif
