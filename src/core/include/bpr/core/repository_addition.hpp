#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REPOSITORY_ADDITION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REPOSITORY_ADDITION_HPP

#include <cstdint>

namespace bpr::core {
	/// \brief Admission and durable-result vocabulary for a private repository addition.
	enum class repository_addition_status : std::uint8_t {
		saving,
		saved,
		already_added,
		invalid_input,
		sign_in_required,
		destination_changed,
		unavailable,
		busy,
		collection_limit,
	};

	/// \brief Bounded host observations; none grants new account or server authority.
	struct repository_addition_observation
	{
		bool SignedIn           = false;
		bool DestinationCurrent = false;
		bool WindowCurrent      = false;
		bool StoreReady         = false;
		bool InputValid         = false;
		bool Pending            = false;
		bool AlreadyAdded       = false;
		bool HasCapacity        = false;
	};

	/// \brief Decides whether the exact displayed destination may start one local save.
	[[nodiscard]] constexpr auto admit_repository_addition(const repository_addition_observation& aFacts) noexcept
		-> repository_addition_status
	{
		if (!aFacts.SignedIn) {
			return repository_addition_status::sign_in_required;
		}
		if (!aFacts.DestinationCurrent || !aFacts.WindowCurrent) {
			return repository_addition_status::destination_changed;
		}
		if (!aFacts.StoreReady) {
			return repository_addition_status::unavailable;
		}
		if (!aFacts.InputValid) {
			return repository_addition_status::invalid_input;
		}
		if (aFacts.Pending) {
			return repository_addition_status::busy;
		}
		if (aFacts.AlreadyAdded) {
			return repository_addition_status::already_added;
		}
		return aFacts.HasCapacity ? repository_addition_status::saving : repository_addition_status::collection_limit;
	}

	/// \brief Accepts a terminal only while its exact destination and source remain current.
	[[nodiscard]] constexpr auto accept_repository_addition_completion(const bool aClosing, const bool aDestinationCurrent, const bool aWindowCurrent) noexcept -> bool
	{
		return !aClosing && aDestinationCurrent && aWindowCurrent;
	}
}
#endif
