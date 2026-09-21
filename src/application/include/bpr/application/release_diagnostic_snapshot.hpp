#ifndef BPR_APPLICATION_RELEASE_DIAGNOSTIC_SNAPSHOT_HPP
#define BPR_APPLICATION_RELEASE_DIAGNOSTIC_SNAPSHOT_HPP

#include "bpr/core/release_maintenance.hpp"
#include <expected>
#include <string>
#include <string_view>

namespace bpr::application {
	inline constexpr std::size_t release_diagnostic_input_limit   = 16U * 1024U * 1024U;
	inline constexpr std::size_t release_diagnostic_input_records = 65536U;
	/// \brief Immutable serialized allow-listed metadata; no legacy prose, IDs or paths survive.
	struct release_diagnostic_document
	{
		core::release_diagnostic_snapshot Descriptor;
		std::string                       Bytes;
	};
	/// \brief Classifies the bounded Quill v1 prefix and omits unknown/malformed legacy records.
	/// \details Only closed operation/outcome vocabulary, HTTP status and dropped count are
	/// emitted. The unframed detail suffix is never parsed or copied, including failure-like
	/// text embedded in that suffix. Input/output work and counts have independent bounds.
	[[nodiscard]] auto prepare_release_diagnostics(std::string_view aLog, core::release_maintenance_revision aRevision)
		-> std::expected<release_diagnostic_document, core::release_maintenance_reason>;
}
#endif
