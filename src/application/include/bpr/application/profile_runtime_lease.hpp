#ifndef BPR_APPLICATION_PROFILE_RUNTIME_LEASE_HPP
#define BPR_APPLICATION_PROFILE_RUNTIME_LEASE_HPP

namespace bpr::application {
	/// \brief Lifetime ownership of one admitted profile runtime.
	/// \details Retain until all profile consumers and effects have drained.
	class profile_runtime_lease
	{
	public:
		virtual ~profile_runtime_lease() = default;
	};

	enum class profile_runtime_lease_failure {
		busy,
		unavailable,
		cancelled
	};
}
#endif
