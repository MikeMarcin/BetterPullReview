#include "../support/diagnostic_files.hpp"
#include "../support/diagnostic_json.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/logging/quill_log.hpp"
#include <QFileInfo>
#include <QTemporaryDir>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>
#if !defined(_WIN32)
	#include <csignal>
	#include <sys/resource.h>
#endif
namespace {
	using bpr::adapters::logging::diagnostic_file_limits;
	using bpr::adapters::logging::quill_log;
	using bpr::tests::expect;
	using status = bpr::application::diagnostic_log_status;
	auto root(const std::filesystem::path& aPath) -> std::filesystem::path
	{
		std::filesystem::create_directory(aPath);
		expect(bpr::adapters::sqlite::make_storage_root_owner_private(aPath), "private test root");
		return std::filesystem::canonical(aPath);
	}
	void wait_written(quill_log& aLog, const std::filesystem::path& aRoot)
	{
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{ 3 };
		while (aLog.status() == status::available && bpr::tests::read_diagnostic_logs(aRoot).empty() && std::chrono::steady_clock::now() < deadline) {
			std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
		}
		expect(!bpr::tests::read_diagnostic_logs(aRoot).empty(), "real backend drains the first record");
	}
	void bounded_rotation(const std::filesystem::path& aRoot)
	{
		quill_log log{ aRoot, { .FileBytes = 1400U, .TotalBytes = 2800U, .Files = 3U }, { .Privacy = bpr::application::diagnostic_privacy::include_locally } };
		for (std::uint64_t index = 1U; index <= 20U; ++index) {
			expect(log.record({ .Request = index }) == status::available, "bounded rotation ingress");
		}
		log.close();
		expect(log.status() == status::available, "rotation continues recording instead of filling permanently");
		const auto    files = bpr::tests::diagnostic_files(aRoot);
		std::uint64_t total = 0U;
		for (const auto& file : files) {
			const auto size = std::filesystem::file_size(file);
			expect(size <= 1400U, "each segment respects its byte budget");
			total += size;
		}
		expect(files.size() <= 3U && total <= 2800U, "aggregate and file count budgets hold");
		const auto records = bpr::tests::diagnostic_json_records(bpr::tests::read_diagnostic_logs(aRoot));
		expect(records.back().value("request").toInteger() == 20 && records.size() < 20U, "oldest completed segments pruned, newest event retained");
	}
	void age_and_partial_tail(const std::filesystem::path& aRoot)
	{
		{
			quill_log log{ aRoot };
			static_cast<void>(log.record({ }));
		}
		const auto old = bpr::tests::diagnostic_files(aRoot).front();
		std::filesystem::last_write_time(old, std::filesystem::file_time_type::clock::now() - std::chrono::hours{ 24 * 31 });
		{
			quill_log log{ aRoot };
			expect(log.status() == status::available, "aged history does not prevent admission");
		}
		expect(!std::filesystem::exists(old), "startup prunes history older than maximum age");
		const auto partial = bpr::tests::diagnostic_files(aRoot).front();
		{
			std::ofstream stream{ partial, std::ios::app };
			stream << "{\"v\":2,\"partial\":";
		}
		{
			quill_log log{ aRoot };
			static_cast<void>(log.record({ .Request = 27U }));
		}
		std::ifstream     stream{ partial };
		const std::string bytes{ std::istreambuf_iterator<char>{ stream }, { } };
		expect(bytes == "{\"v\":2,\"partial\":", "crash tail is preserved without appending another session to it");
		expect(bpr::tests::diagnostic_files(aRoot).size() == 2U, "restart uses a fresh session file");
	}
	void protected_capacity(const std::filesystem::path& aRoot)
	{
		const diagnostic_file_limits limits{ .FileBytes = 1400U, .TotalBytes = 2800U, .Files = 2U };
		quill_log                    first{ aRoot, limits };
		quill_log                    second{ aRoot, limits, { .Privacy = bpr::application::diagnostic_privacy::include_locally } };
		expect(first.status() == status::available && second.status() == status::available, "two active sessions coexist");
		const auto firstFiles = bpr::tests::diagnostic_files(aRoot);
		quill_log  refused{ aRoot, limits };
		expect(refused.open_failure() == bpr::adapters::logging::diagnostic_file_failure::busy, "capacity never deletes an active session file");
		expect(bpr::tests::diagnostic_files(aRoot) == firstFiles, "refused startup changes no active paths");
		first.close();
		quill_log replacement{ aRoot, limits };
		expect(replacement.status() == status::available, "finished session can be pruned to admit a new one");
		static_cast<void>(second.record({ .Request = 1216U }));
		second.close();
		expect(bpr::tests::read_diagnostic_logs(aRoot).find("\"request\":1216") != std::string::npos, "surviving active session retains its own descriptor");
	}
	void rotation_failure(const std::filesystem::path& aRoot)
	{
		quill_log log{ aRoot, { .FileBytes = 1400U } };
		static_cast<void>(log.record({ }));
		wait_written(log, aRoot);
		const auto first     = bpr::tests::diagnostic_files(aRoot).front();
		auto       collision = first.filename().string();
		collision.replace(41U, 8U, "00000002");
		std::filesystem::create_directory(aRoot / "logs" / collision);
		static_cast<void>(log.record({ }));
		log.close();
		expect(log.status() == status::unavailable && std::filesystem::is_directory(aRoot / "logs" / collision), "unsafe rotation target fails without replacement");
	}
	void unsafe_targets(const std::filesystem::path& aRoot)
	{
		{
			quill_log log{ aRoot };
			static_cast<void>(log.record({ }));
		}
		const auto file = bpr::tests::diagnostic_files(aRoot).front();
		std::filesystem::create_hard_link(file, aRoot / "outside-link");
		{
			quill_log log{ aRoot };
			expect(log.status() == status::unavailable, "multiply linked retained file refuses pruning");
		}
		expect(std::filesystem::exists(file) && std::filesystem::exists(aRoot / "outside-link"), "unsafe file is preserved");
	}
#if !defined(_WIN32)
	void disk_failure(const std::filesystem::path& aRoot)
	{
		quill_log     log{ aRoot };
		struct rlimit original{ };
		expect(::getrlimit(RLIMIT_FSIZE, &original) == 0, "read native write limit");
		const struct rlimit limited{ 1U, original.rlim_max };
		const auto          previous = std::signal(SIGXFSZ, SIG_IGN);
		expect(::setrlimit(RLIMIT_FSIZE, &limited) == 0, "constrain actual native file writes");
		static_cast<void>(log.record({ }));
		log.close();
		expect(::setrlimit(RLIMIT_FSIZE, &original) == 0, "restore native file limit");
		std::signal(SIGXFSZ, previous);
		expect(log.status() == status::unavailable, "real short write then file-size failure is unavailable");
		const auto partial = bpr::tests::diagnostic_files(aRoot).front();
		expect(std::filesystem::file_size(partial) == 1U, "only an incomplete final record survives failed write");
		{
			quill_log reopened{ aRoot };
			static_cast<void>(reopened.record({ .Request = 27U }));
		}
		expect(std::filesystem::file_size(partial) == 1U && bpr::tests::diagnostic_files(aRoot).size() == 2U, "restart leaves failed tail separate and records again");
	}
#endif
	int concurrent_child(const std::filesystem::path& aRoot)
	{
		quill_log log{ aRoot, { .FileBytes = 1400U, .TotalBytes = 8400U, .Files = 8U } };
		expect(log.status() == status::available, "concurrent process admitted");
		std::cout << "ready" << '\n'
				  << std::flush;
		std::string command;
		std::getline(std::cin, command);
		for (std::uint64_t index = 1U; index <= 30U; ++index) {
			static_cast<void>(log.record({ .Request = index }));
			std::this_thread::sleep_for(std::chrono::milliseconds{ 2 });
		}
		log.close();
		expect(log.status() == status::available, "concurrent process drains without file failure");
		return 0;
	}
	int recovery_child(const std::filesystem::path& aRoot)
	{
		quill_log log{ root(aRoot) };
		expect(log.status() == status::available, "native recovery logger admitted");
		expect(log.record({ .Request = 1219U }) == status::available, "pre-failure record admitted");
		expect(log.flush_for_export(false).has_value(), "pre-failure record flushed");
		std::cout << "flushed" << '\n'
				  << std::flush;
		std::string command;
		std::getline(std::cin, command);
		if (command == "failure") {
			for (auto index = 0; index < 100; ++index) {
				static_cast<void>(log.record({ .Request = 1220U }));
			}
			log.close();
			expect(log.status() == status::unavailable, "native exhausted volume reports unavailable");
			std::cout << "unavailable" << '\n'
					  << std::flush;
		} else {
			log.close();
			expect(log.status() == status::available, "restarted native logger drains normally");
		}
		return 0;
	}
}
int main(int aArgc, char** aArgv)
try {
	if (aArgc == 3 && std::string_view{ aArgv[1] } == "--concurrent-child") {
		return concurrent_child(aArgv[2]);
	}
	if (aArgc == 3 && std::string_view{ aArgv[1] } == "--recovery-child") {
		return recovery_child(aArgv[2]);
	}
	QTemporaryDir temporary;
	expect(temporary.isValid(), "owned scratch directory");
	const auto base = root(QFileInfo{ temporary.path() }.canonicalFilePath().toStdString());
	bounded_rotation(root(base / "rotation"));
	age_and_partial_tail(root(base / "recovery"));
	protected_capacity(root(base / "protected"));
	rotation_failure(root(base / "failure"));
	unsafe_targets(root(base / "unsafe"));
#if !defined(_WIN32)
	disk_failure(root(base / "disk"));
#endif
	std::cout << "session-retention:pass\n";
	return 0;
} catch (...) {
	return bpr::tests::failing_exit();
}
