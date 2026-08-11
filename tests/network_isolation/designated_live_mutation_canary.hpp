#ifndef BPR_TESTS_NETWORK_ISOLATION_DESIGNATED_LIVE_MUTATION_CANARY_HPP
#define BPR_TESTS_NETWORK_ISOLATION_DESIGNATED_LIVE_MUTATION_CANARY_HPP

namespace bpr::tests::network_isolation {

	[[nodiscard]] int run_designated_live_mutation_canary(
		int  aInputDescriptor,
		int  aOutputDescriptor,
		bool aRequireEmptyEnvironment) noexcept;

} // namespace bpr::tests::network_isolation

#endif
