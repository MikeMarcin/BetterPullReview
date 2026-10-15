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
/// is not granted by QTemporaryDir's descriptor inside the test container
/// ([[BPR-0833]], remeasured [[BPR-1525]]), so a course that uses this runs
/// uncontained there. The owner-private C++ scratch descriptor grants the
/// container control; QTemporaryDir's descriptor is a different root shape.

#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/app/native_filesystem_path.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QEvent>
#include <QFileInfo>
#include <QString>
#include <QTemporaryDir>
#include <QThread>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <optional>

namespace bpr::e2e {
	/// \brief Narrows `aTemporary` to owner-private and creates the course's
	/// application data root inside it.
	/// \returns The root, `<aTemporary>/data`, canonical and in the host's
	/// native spelling. Nothing when `aTemporary` is invalid or either step is
	/// refused. The root, and the agent workspace neighbourhood the
	/// application creates beside it, are removed with `aTemporary`; a course
	/// that composes a desktop removes it with `remove_course_root`.
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

	/// \brief Removes a course's temporary directory once the application has
	/// let go of it, or says what is left.
	/// \details [[BPR-1103]]. Destroying a profile desktop does not close the
	/// profile's stores at once. Its runtimes are deleted from the event loop,
	/// and until then the profile's runtime lock and every SQLite store stay
	/// open. POSIX unlinks an open file, so a `QTemporaryDir` removed straight
	/// after the desktop leaves nothing. Windows refuses to delete an open file,
	/// and the destructor's failure is silent, so every run left its root in
	/// the system temporary directory. This delivers the deferred deletions
	/// and posted events and retries the removal until it succeeds, bounded.
	/// Call it after the desktop is destroyed and before the application is.
	/// \returns Whether the directory is gone. When it is not, every entry left
	/// is printed on standard error after `COURSE_ROOT_NOT_REMOVED`, so the
	/// course can fail on it instead of leaking it.
	[[nodiscard]] inline auto remove_course_root(QTemporaryDir& aTemporary, const std::chrono::milliseconds aBound = std::chrono::seconds{ 10 }) -> bool
	{
		const auto path = aTemporary.path();
		if (!aTemporary.isValid()) {
			std::cerr << "COURSE_ROOT_NOT_REMOVED: the temporary directory was never created\n";
			return false;
		}
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (true) {
			QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
			QCoreApplication::processEvents();
			if (aTemporary.remove() || !QFileInfo::exists(path)) {
				return true;
			}
			if (elapsed.elapsed() >= aBound.count()) {
				break;
			}
			QThread::msleep(5);
		}
		std::cerr << "COURSE_ROOT_NOT_REMOVED:" << QDir::toNativeSeparators(path).toStdString() << " after " << elapsed.elapsed() << " ms\n";
		auto left = QDirIterator{ path, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDirIterator::Subdirectories };
		while (left.hasNext()) {
			std::cerr << "COURSE_ROOT_LEFT:" << QDir::toNativeSeparators(left.next()).toStdString() << '\n';
		}
		return false;
	}
} // namespace bpr::e2e

#endif // BPR_TESTS_END_TO_END_PRIVATE_DATA_ROOT_HPP
