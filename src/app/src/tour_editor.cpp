#include "tour_editor.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/app/native_filesystem_path.hpp"
#include "bpr/core/review_preparation.hpp"
#include "tour_cancellation.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <iostream>
#if defined(_WIN32)
	#include "bpr/platform/windows_standard_handles.hpp"
#else
	#include <fcntl.h>
	#include <sys/stat.h>
	#include <unistd.h>
#endif
namespace bpr::app {
	namespace {
		auto refuse(std::string aCode, std::string aMessage, int aExit = 2) { return std::unexpected(tour_cli_error{ std::move(aCode), std::move(aMessage), aExit }); }
		auto editor_arguments() -> tour_cli_result<QStringList>
		{
			const auto  configured = qEnvironmentVariable("BPR_TOUR_EDITOR_ARGV");
			QStringList arguments;
			if (!configured.isEmpty()) {
				if (configured.toUtf8().size() > 8192) {
					return refuse("editor_configuration", "Editor argv exceeds the configuration bound.");
				}
				const auto document = QJsonDocument::fromJson(configured.toUtf8());
				if (!document.isArray() || document.array().isEmpty() || document.array().size() > 32) {
					return refuse("editor_configuration", "BPR_TOUR_EDITOR_ARGV must be a JSON array containing an absolute executable and its arguments.");
				}
				for (const auto& value : document.array()) {
					if (!value.isString() || value.toString().contains(QChar{ 0 }) || value.toString().size() > 4096) {
						return refuse("editor_configuration", "Editor argv must contain bounded strings without NUL.");
					}
					arguments.append(value.toString());
				}
			} else {
				auto value = qEnvironmentVariable("VISUAL");
				if (value.isEmpty()) {
					value = qEnvironmentVariable("EDITOR");
				}
				if (value.isEmpty()) {
					return refuse("editor_configuration", "Set BPR_TOUR_EDITOR_ARGV to a JSON argv array, or VISUAL/EDITOR to one absolute executable path. Use --file PATH or --file - for text input.", 3);
				}
				// These compatibility variables accept one path, optionally surrounded by double quotes.
				// Arguments and shell syntax require the explicit JSON argv representation.
				if (value.startsWith('"') && value.endsWith('"') && value.size() > 2) {
					value = value.mid(1, value.size() - 2);
				}
				if (value.contains('"') || value.contains('\n') || value.contains('\r') || value.contains('$') || value.contains('`') || value.contains(';') || value.contains('|') || value.contains('&') || value.contains('<') || value.contains('>')) {
					return refuse("editor_configuration", "Ambiguous editor text is refused. Configure BPR_TOUR_EDITOR_ARGV as explicit arguments.");
				}
				arguments.append(value);
			}
			if (!QDir::isAbsolutePath(arguments[0]) || !QFileInfo{ arguments[0] }.isFile() || !QFileInfo{ arguments[0] }.isExecutable()) {
				return refuse("editor_configuration", "The editor executable must be an existing absolute executable file; arguments belong in BPR_TOUR_EDITOR_ARGV.");
			}
			return arguments;
		}
		auto read_projection(const QString& aPath) -> tour_cli_result<std::string>
		{
			std::string result;
#if defined(_WIN32)
			const auto handle = ::CreateFileW(reinterpret_cast<const wchar_t*>(aPath.utf16()), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
			if (handle == INVALID_HANDLE_VALUE) {
				return refuse("editor_candidate_untrusted", "The edited projection could not be opened safely.", 7);
			}
			struct guard
			{
				HANDLE Value;
				~guard() { ::CloseHandle(Value); }
			} close{ handle };
			BY_HANDLE_FILE_INFORMATION info{ };
			if (!::GetFileInformationByHandle(handle, &info) || ::GetFileType(handle) != FILE_TYPE_DISK || (info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) || info.nNumberOfLinks != 1 || info.nFileSizeHigh || info.nFileSizeLow > core::maximum_tour_field_bytes) {
				return refuse("editor_candidate_untrusted", "The edited projection is not a bounded ordinary private file.", 7);
			}
			char  buffer[4096];
			DWORD count = 0;
			do {
				if (!::ReadFile(handle, buffer, sizeof(buffer), &count, nullptr)) {
					return refuse("input_unavailable", "The edited projection could not be read.", 7);
				}
				result.append(buffer, count);
			} while (count && result.size() <= core::maximum_tour_field_bytes);
#else
			const auto native     = native_filesystem_path(aPath);
			const auto descriptor = ::open(native.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
			if (descriptor < 0) {
				return refuse("editor_candidate_untrusted", "The edited projection could not be opened without following links.", 7);
			}
			struct guard
			{
				int Value;
				~guard() { ::close(Value); }
			} close{ descriptor };
			struct stat info{ };
			if (::fstat(descriptor, &info) != 0 || !S_ISREG(info.st_mode) || info.st_uid != ::geteuid() || info.st_nlink != 1 || info.st_size < 0 || static_cast<std::uint64_t>(info.st_size) > core::maximum_tour_field_bytes || (info.st_mode & 0077U) != 0) {
				return refuse("editor_candidate_untrusted", "The edited projection is not a bounded ordinary owner-private file.", 7);
			}
			char buffer[4096];
			for (;;) {
				const auto count = ::read(descriptor, buffer, sizeof(buffer));
				if (count < 0) {
					return refuse("input_unavailable", "The edited projection could not be read.", 7);
				}
				if (!count) {
					break;
				}
				result.append(buffer, static_cast<std::size_t>(count));
				if (result.size() > core::maximum_tour_field_bytes) {
					break;
				}
			}
#endif
			if (result.size() > core::maximum_tour_field_bytes) {
				return refuse("input_too_large", "The edited field exceeds 65536 bytes.");
			}
			if (!core::valid_writing_utf8(result) || result.find('\0') != std::string::npos) {
				return refuse("invalid_utf8", "The edited field is not valid UTF-8 without NUL bytes.");
			}
			return result;
		}
	}
	auto run_tour_editor(std::string_view aOriginal) -> tour_cli_result<tour_editor_candidate>
	{
		auto configured = editor_arguments();
		if (!configured) {
			return std::unexpected(configured.error());
		}
		// [[BPR-1249]]. The projection's folder is checked again after editing
		// with `create_storage_root_owner_private`, which accepts only a path
		// whose canonical spelling is its own. macOS's per-user temporary folder
		// is under /var, a link to /private/var, so a folder made under the
		// spelling Qt reports failed that check after every edit.
		const auto temporary = QFileInfo{ QDir::tempPath() }.canonicalFilePath();
		if (temporary.isEmpty()) {
			return refuse("editor_unavailable", "The temporary folder for the editor projection is unavailable.", 7);
		}
		QTemporaryDir directory{ temporary + "/bpr-tour-edit-XXXXXX" };
		if (!directory.isValid() || !adapters::sqlite::make_storage_root_owner_private(native_filesystem_path(directory.path()))) {
			return refuse("editor_unavailable", "Could not create an owner-private editor projection.", 7);
		}
		const auto path = directory.path() + "/field.md";
		QFile      file{ path };
		if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly) || !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) || file.write(aOriginal.data(), static_cast<qint64>(aOriginal.size())) != static_cast<qint64>(aOriginal.size()) || !file.flush()) {
			return refuse("editor_unavailable", "Could not write the private field projection.", 7);
		}
		file.close();
		directory.setAutoRemove(false);
		const auto retained = [&](tour_cli_error aError) -> tour_cli_result<tour_editor_candidate> {
			aError.Message += " Candidate retained at " + path.toStdString() + ". Reopen the current draft before importing that file.";
			return std::unexpected(std::move(aError));
		};
		std::cerr << "Opening the configured editor. Its plugins, backups and synchronization remain governed by that editor.\n";
		QProcess process;
		process.setWorkingDirectory(directory.path());
		process.setProcessChannelMode(QProcess::ForwardedChannels);
		process.setInputChannelMode(QProcess::ForwardedInputChannel);
#if defined(_WIN32)
		platform::windows_standard_handles::launch_restriction inheritance;
		process.setCreateProcessArgumentsModifier([&inheritance](QProcess::CreateProcessArguments* aArguments) { inheritance.apply(aArguments->startupInfo, aArguments->flags, aArguments->inheritHandles); });
#else
		process.setUnixProcessParameters(QProcess::UnixProcessFlag::CloseFileDescriptors | QProcess::UnixProcessFlag::DisableCoreDumps | QProcess::UnixProcessFlag::ResetSignalHandlers);
		process.setChildProcessModifier([] { ::umask(0077); });
#endif
		const auto program = configured->takeFirst();
		configured->append(path);
		process.start(program, *configured);
		if (!process.waitForStarted(15000)) {
			return retained({ "editor_start_failed", "The configured editor did not start.", 7 });
		}
#if defined(_WIN32)
		if (!inheritance.restricted()) {
			process.kill();
			process.waitForFinished(5000);
			return retained({ "editor_start_failed", "The editor handle restriction was not applied.", 7 });
		}
#endif
		bool cancelled = false;
		while (process.state() != QProcess::NotRunning) {
			if (tour_cancelled()) {
				cancelled = true;
				process.terminate();
				if (!process.waitForFinished(1500)) {
					process.kill();
					process.waitForFinished(5000);
				}
				break;
			}
			process.waitForFinished(100);
			QCoreApplication::processEvents();
		}
		if (cancelled) {
			return retained({ "cancelled", "Editor input was cancelled; the saved draft was not changed.", 130 });
		}
		if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
			return retained({ "editor_failed", "The editor exited unsuccessfully; the saved draft was not changed.", 7 });
		}
		if (!adapters::sqlite::create_storage_root_owner_private(native_filesystem_path(directory.path()))) {
			return retained({ "editor_candidate_untrusted", "The private projection directory changed while editing.", 7 });
		}
		auto edited = read_projection(path);
		if (!edited) {
			return retained(edited.error());
		}
		return tour_editor_candidate{ std::move(*edited), path, directory.path() };
	}
	void cleanup_tour_editor(const tour_editor_candidate& aCandidate)
	{
		QFile::remove(aCandidate.Path);
		QDir{ }.rmdir(aCandidate.Directory);
	}
}
