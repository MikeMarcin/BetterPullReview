#ifndef BPR_TESTS_END_TO_END_PRIVATE_DATA_ROOT_HPP
#define BPR_TESTS_END_TO_END_PRIVATE_DATA_ROOT_HPP

/// \file
/// \brief A course's application data root, one owned level below its
/// temporary directory.
///
/// \details [[BPR-1044]]. The application keeps its agent workspace
/// neighbourhood beside the data root, as `<root-leaf>-agent-workspaces-v1`,
/// and startup recovery refuses a neighbourhood whose parent another principal
/// can write. A `QTemporaryDir` that is itself the data root puts that parent
/// in the system temporary directory. On Linux that is a shared, root-owned
/// `1777` `/tmp`, so every startup printed
/// `AGENT_WORKSPACE_RECOVERY_REFUSED:…:repair-required` and composed no
/// disposable agent workspaces. A real install reaches that state only when
/// its data folder needs repair.
///
/// So the temporary directory is narrowed to owner-private, and the root is
/// its `data` child, created owner-private under the storage policy. This is
/// the arrangement [[BPR-0874]] made uniform for the large-review courses and
/// [[BPR-1027]] used for the second-window course. The root is spelled through
/// `native_filesystem_path`, which Windows requires ([[BPR-1031]]). Narrowing
/// the temporary directory needs `WRITE_DAC`, which a Windows AppContainer
/// lacks ([[BPR-0833]]), so a course that uses this runs uncontained there.

#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/app/native_filesystem_path.hpp"

#include <QFileInfo>
#include <QString>
#include <QTemporaryDir>

#include <filesystem>
#include <optional>

namespace bpr::e2e {
	/// \brief Narrows `aTemporary` to owner-private and creates the course's
	/// application data root inside it.
	/// \returns The root, `<aTemporary>/data`, canonical and in the host's
	/// native spelling. Nothing when `aTemporary` is invalid or either step is
	/// refused. The root, and the agent workspace neighbourhood the
	/// application creates beside it, are removed with `aTemporary`.
	[[nodiscard]] inline auto private_data_root(const QTemporaryDir& aTemporary) -> std::optional<std::filesystem::path>
	{
		const auto canonical = aTemporary.isValid() ? QFileInfo{ aTemporary.path() }.canonicalFilePath() : QString{ };
		if (canonical.isEmpty()) {
			return std::nullopt;
		}
		const auto parent = app::native_filesystem_path(canonical);
		auto       root   = parent / "data";
		if (!adapters::sqlite::make_storage_root_owner_private(parent) || !adapters::sqlite::create_storage_root_owner_private(root)) {
			return std::nullopt;
		}
		return root;
	}
} // namespace bpr::e2e

#endif // BPR_TESTS_END_TO_END_PRIVATE_DATA_ROOT_HPP
