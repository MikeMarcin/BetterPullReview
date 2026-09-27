#ifndef BPR_APPLICATION_RELEASE_DIAGNOSTIC_SNAPSHOT_HPP
#define BPR_APPLICATION_RELEASE_DIAGNOSTIC_SNAPSHOT_HPP

#include "bpr/application/diagnostic_log_port.hpp"
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
	/// \brief Classifies canonical JSONL v2 and the legacy Quill v1 prefix; omits unknown or malformed records.
	/// \details Only closed operation/outcome vocabulary, HTTP status and dropped count are
	/// emitted. JSON fields require the canonical order and bounded scalar grammar. The legacy
	/// unframed detail suffix is never parsed or copied, including failure-like
	/// text embedded in that suffix. Input/output work and counts have independent bounds.
	[[nodiscard]] auto prepare_release_diagnostics(std::string_view aLog, core::release_maintenance_revision aRevision)
		-> std::expected<release_diagnostic_document, core::release_maintenance_reason>;
	/// \brief Rebuilds a frozen bounded JSONL export, with a fresh host-generated mapping namespace.
	/// \details Legacy input retains metadata only. Every current field is classified again;
	/// unknown prose/records are omitted, never copied. Capture supplies this session's flush cutoff.
	[[nodiscard]] auto prepare_diagnostic_export(std::string_view aLog, core::release_maintenance_revision aRevision,
												 core::diagnostic_export_options aOptions, const diagnostic_export_capture& aCapture, std::string_view aNamespace)
		-> std::expected<release_diagnostic_document, core::release_maintenance_reason>;
}
#endif
