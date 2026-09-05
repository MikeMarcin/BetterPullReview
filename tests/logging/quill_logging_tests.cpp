#include "bpr/application/git_protocol_v2.hpp"
#include "bpr/logging/quill_log.hpp"
#include "github_capability_probe.hpp"
#include <QFileInfo>
#include <QTemporaryDir>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <thread>
namespace {
	void expect(bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			std::cerr << aMessage << '\n';
			std::exit(1);
		}
	}
	auto read(const std::filesystem::path& aPath) -> std::string
	{
		std::ifstream stream{ aPath };
		return { std::istreambuf_iterator<char>{ stream }, { } };
	}
}
int main(int aArgc, char** aArgv)
{
	if (aArgc == 4 && std::string_view{ aArgv[1] } == "--failure-child") {
		const auto                        root = std::string_view{ aArgv[3] } == "full" ? std::filesystem::path{ aArgv[2] } : std::filesystem::path{ "relative" };
		bpr::adapters::logging::quill_log log{ root, 1U };
		static_cast<void>(log.record({ }));
		log.close();
		expect(log.status() == (std::string_view{ aArgv[3] } == "full" ? bpr::application::diagnostic_log_status::full : bpr::application::diagnostic_log_status::unavailable), "failure child status");
		return 0;
	}
	using namespace bpr;
	using status = application::diagnostic_log_status;
	QTemporaryDir temporary;
	expect(temporary.isValid(), "temporary root");
	const auto root = std::filesystem::path{ QFileInfo{ temporary.path() }.canonicalFilePath().toStdString() };
	const auto file = root / "logs/diagnostics.log";
	{
		adapters::logging::quill_log log{ root };
		expect(log.status() == status::available, "available local sink");
		expect(log.record({ }) == status::available, "accepted startup");
		auto hostile            = application::diagnostic_event{ };
		hostile.Summary.Text[0] = '\n';
		hostile.Summary.Size    = 1;
		expect(log.record(hostile) == status::dropped, "control-bearing event never enters queue");
		const std::string secret  = "runtime-only-file-secret";
		const auto        summary = adapters::github::detail::sanitize_capability_error(R"({"errors":[{"message":"Unknown schema field newField runtime-only-file-secret","extensions":{"code":"NEW_SCHEMA_CODE"}}]})", std::as_bytes(std::span{ secret }));
		expect(log.record({ .Operation = application::diagnostic_operation::capability, .Outcome = application::diagnostic_outcome::response_received, .Request = 42, .HttpStatus = 200, .Summary = summary }) == status::available, "sanitized prose queued for real file");
		log.close();
		expect(log.record({ }) == status::closed, "closed ingress");
	}

	{
		adapters::logging::quill_log log{ root };
		expect(log.record({ .Operation = application::diagnostic_operation::epoch, .Outcome = application::diagnostic_outcome::failed, .Request = 19U, .EpochStage = application::diagnostic_epoch_stage::protocol_acquire, .EpochFailure = application::git_protocol_v2_failure::pack_sink, .EpochPackFailure = application::git_pack_stream_failure::input_limit }) == status::available, "typed epoch detail accepted by real sink");
	}
	const auto first = read(file);
	expect(first.find("epoch_stage=protocol_acquire failure_domain=protocol failure=pack_sink pack_failure=input_limit") != std::string::npos, "fixed protocol and pack failure names persisted");
	expect(first.find("newField") != std::string::npos && first.find("NEW_SCHEMA_CODE") != std::string::npos && first.find("request=42 http_status=200") != std::string::npos, "useful sanitized prose and correlation survive file encoding");
	expect(first.find("runtime-only-file-secret") == std::string::npos, "credential canary absent from persisted file");
	expect(first.find("op=application outcome=started") != std::string::npos, "ordinary close persists startup");
	{
		adapters::logging::quill_log log{ root };
		expect(log.record({ .Outcome = application::diagnostic_outcome::shutdown }) == status::available, "append after reopen");
	}
	expect(read(file).starts_with(first) && read(file).find("outcome=shutdown") != std::string::npos, "reopen preserves prior records");
	{
		const auto                   original = read(file);
		adapters::logging::quill_log log{ root, original.size() + 1U };
		static_cast<void>(log.record({ }));
		log.close();
		expect(log.status() == status::full, "file cap reports full");
		expect(read(file) == original, "crossing-cap record is refused without changing bytes");
	}
	const auto before = std::filesystem::file_size(file);
	{
		adapters::logging::quill_log log{ root, before - 1U };
		expect(log.status() == status::full, "existing full file refused");
	}
	expect(std::filesystem::file_size(file) == before, "cap never appends beyond ceiling");
	const auto unsafe = root / "unsafe";
	std::filesystem::create_directory(unsafe);
	std::filesystem::create_directory_symlink(root / "logs", unsafe / "logs");
	{
		adapters::logging::quill_log log{ unsafe };
		expect(log.status() == status::unavailable, "symlink directory refused");
	}
	std::filesystem::remove(unsafe / "logs");
	std::filesystem::create_directory(unsafe / "logs");
	std::filesystem::permissions(unsafe / "logs", std::filesystem::perms::owner_all);
	std::filesystem::create_symlink(file, unsafe / "logs/diagnostics.log");
	{
		adapters::logging::quill_log log{ unsafe };
		expect(log.status() == status::unavailable, "symlink file refused");
	}
	{
		adapters::logging::quill_log log{ file };
		expect(log.status() == status::unavailable, "non-directory root refused");
	}
	{
		adapters::logging::quill_log log{ "relative" };
		expect(log.status() == status::unavailable, "relative root refused");
	}
	std::cout << "quill-logging-tests:pass\n";
}
