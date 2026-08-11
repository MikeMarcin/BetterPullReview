#ifndef BPR_TESTS_NETWORK_ISOLATION_DESIGNATED_LIVE_MUTATION_CHILD_HPP
#define BPR_TESTS_NETWORK_ISOLATION_DESIGNATED_LIVE_MUTATION_CHILD_HPP

#include <cstddef>
#include <string_view>

namespace bpr::tests::network_isolation {

	enum class child_run_status {
		not_started,
		parent_thread_state_failed,
		parent_multithreaded,
		descriptor_inventory_failed,
		descriptor_inventory_unstable,
		succeeded,
		failed,
		timed_out,
	};

	struct child_start_result
	{
		child_run_status Status{ child_run_status::not_started };
		unsigned         ObservedThreadCount{ };
		std::size_t      FirstDescriptorCount{ };
		std::size_t      SecondDescriptorCount{ };
	};

	enum class fixed_canary_behavior {
		run,
		exit_failure_after_protocol,
		timeout_after_protocol,
	};

	class fixed_canary_pre_start_hook
	{
	public:
		virtual ~fixed_canary_pre_start_hook() = default;
		virtual void after_surface_binding()   = 0;
	};

	struct fixed_canary_options
	{
		fixed_canary_pre_start_hook* Hook{ };
		fixed_canary_behavior        Behavior{ fixed_canary_behavior::run };
		int                          TimeoutMilliseconds{ 10'000 };
		int                          ForbiddenDescriptorForTest{ -1 };
	};

	[[nodiscard]] std::string_view   designated_live_canary_surface_sha256();
	[[nodiscard]] child_start_result start_fixed_canary(
		std::string_view            aCapability,
		std::string_view            aExpectedCanarySurfaceSha256,
		const fixed_canary_options& aOptions = { });

} // namespace bpr::tests::network_isolation

#endif
