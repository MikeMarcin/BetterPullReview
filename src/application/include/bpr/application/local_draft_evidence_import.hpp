#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_LOCAL_DRAFT_EVIDENCE_IMPORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_LOCAL_DRAFT_EVIDENCE_IMPORT_HPP

#include "bpr/application/local_draft_store.hpp"
#include "bpr/application/workspace_source_port.hpp"
#include "bpr/core/local_draft_evidence.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>

/// \file
/// \brief Imports one evidence document into a local draft (RFC-BPR-0109,
/// [[BPR-2023]]), for the workbench and for `bpr draft open --evidence`.
/// \details The document's folder is opened as a read-only workspace root:
/// the document and every artifact it names are read beneath it without
/// following links, each within its bound, and nothing else is read. No
/// command is run, no link is fetched and no secret store is read. The draft
/// is re-read and the import applied in one store write, so a restart keeps
/// the draft as it was or with the whole import, never part of it.
namespace bpr::application {
	/// \brief The most artifact bytes one import copies into the store; later
	/// artifacts past it are recorded as too large.
	inline constexpr std::uint64_t maximum_evidence_import_artifact_bytes = 64U * 1024U * 1024U;

	enum class evidence_import_failure : std::uint8_t {
		/// The document could not be read; `Workspace` says why.
		unreadable,
		/// The document was refused whole; `Refusal` says why.
		refused,
		/// The store could not take it; `Store` says why.
		store,
		cancelled,
	};

	struct evidence_import_error
	{
		evidence_import_failure                      Failure = evidence_import_failure::unreadable;
		std::optional<workspace_failure>             Workspace;
		std::optional<core::evidence_import_refused> Refusal;
		std::optional<local_draft_store_error>       Store;
		auto                                         operator<=>(const evidence_import_error&) const = default;
	};

	/// \brief What one import added.
	struct evidence_import_summary
	{
		core::local_draft Draft;
		std::uint64_t     Claims    = 0;
		std::uint64_t     Checks    = 0;
		std::uint64_t     Results   = 0;
		std::uint64_t     Questions = 0;
		/// Results bound to one of the draft's candidates.
		std::uint64_t Bound = 0;
		/// Artifacts copied, and those named but not copied.
		std::uint64_t Stored   = 0;
		std::uint64_t Unstored = 0;
	};

	/// \brief Reads `aDocument` (an absolute path) and applies it to the
	/// draft `aDraft`. A write that loses to another writer is re-read and
	/// applied once more against what was written.
	[[nodiscard]] auto import_local_draft_evidence(local_draft_store& aStore, workspace_source_port& aSource, const core::local_draft_id& aDraft, std::string_view aDocument,
												   std::int64_t aNow, std::stop_token aStop) -> std::expected<evidence_import_summary, evidence_import_error>;
}

#endif
