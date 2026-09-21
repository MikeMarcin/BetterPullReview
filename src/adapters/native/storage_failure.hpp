#ifndef BPR_ADAPTERS_NATIVE_STORAGE_FAILURE_HPP
#define BPR_ADAPTERS_NATIVE_STORAGE_FAILURE_HPP

namespace bpr::adapters::native_storage {
	// Existing private-store failures, shared by the native admission substrate.
	enum class failure {
		invalid_path,
		invalid_permissions,
		unavailable,
		schema_mismatch,
		integrity_failure,
		quota_exceeded,
		cancelled,
		platform_unsupported
	};
}
#endif
