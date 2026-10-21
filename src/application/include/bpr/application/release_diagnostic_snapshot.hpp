#ifndef BPR_APPLICATION_RELEASE_DIAGNOSTIC_SNAPSHOT_HPP
#define BPR_APPLICATION_RELEASE_DIAGNOSTIC_SNAPSHOT_HPP

#include "bpr/application/diagnostic_log_port.hpp"
#include "bpr/core/release_maintenance.hpp"
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace bpr::application {
	/// \brief [[BPR-PENDING-AD]]. The log bytes and lines one export reads.
	/// \details Files are read whole, newest first, and a session file is at
	/// most 5 MiB, so at least 27 MiB (about 35,000 records) is read whenever
	/// the logs hold that much: more than the output keeps. Lines are bounded
	/// at one per 256 input bytes, as before.
	inline constexpr std::size_t release_diagnostic_input_limit   = 32U * 1024U * 1024U;
	inline constexpr std::size_t release_diagnostic_input_records = 131072U;
	/// \brief [[BPR-PENDING-AD]]. When an export was taken, by the host's
	/// clock: UTC milliseconds since the Unix epoch (0 when not known) and
	/// the local offset from UTC in minutes.
	struct diagnostic_export_stamp
	{
		std::uint64_t UtcMs         = 0U;
		std::int32_t  OffsetMinutes = 0;
	};
	/// \brief [[BPR-PENDING-AD]]. The stamp as ISO 8601 local time with its
	/// offset, `2026-10-21T14:32:05-07:00`; `unknown` without a time or with
	/// an offset beyond 18 hours.
	[[nodiscard]] auto format_diagnostic_export_time(const diagnostic_export_stamp& aStamp) -> std::string;
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
	/// [[BPR-1717]]. Records are written oldest first by time; over the record or byte
	/// limit the oldest are left out, so the newest (the session being exported) are kept.
	/// The header counts unclassified and over-limit omissions apart, with the `aUnreadFiles`
	/// the reader left out to stay within the input limit. [[BPR-PENDING-AD]]. The
	/// header names when it was taken (`aStamp`) and the running build
	/// (`aCapture.Build`), each `unknown` when not supplied or not well formed.
	[[nodiscard]] auto prepare_diagnostic_export(std::string_view aLog, core::release_maintenance_revision aRevision,
												 core::diagnostic_export_options aOptions, const diagnostic_export_capture& aCapture, std::string_view aNamespace,
												 std::uint32_t aUnreadFiles = 0U, const diagnostic_export_stamp& aStamp = { })
		-> std::expected<release_diagnostic_document, core::release_maintenance_reason>;
}
#endif
