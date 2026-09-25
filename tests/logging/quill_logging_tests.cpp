#include "../support/capability_error_content_canaries.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/application/git_protocol_v2.hpp"
#include "bpr/application/github_read_model.hpp"
#include "bpr/application/release_diagnostic_snapshot.hpp"
#include "bpr/logging/quill_log.hpp"
#include "github_capability_probe.hpp"
#include <QFileInfo>
#include <QTemporaryDir>
#include <atomic>
#include <barrier>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>
#if !defined(_WIN32)
	#include <unistd.h>
#endif
namespace {
	using bpr::tests::expect;
	auto read(const std::filesystem::path& aPath) -> std::string
	{
		std::ifstream stream{ aPath };
		return { std::istreambuf_iterator<char>{ stream }, { } };
	}
	auto private_root(const std::filesystem::path& aPath) -> std::filesystem::path
	{
		std::filesystem::create_directory(aPath);
		expect(bpr::adapters::sqlite::make_storage_root_owner_private(aPath), "test owner creates private root before logger admission");
		return std::filesystem::canonical(aPath);
	}

	// Opt-in acceptance probe: a nonzero result preserves the open BPR-0971
	// finding without treating the present retention policy as a regression contract.
	auto audit_content_policy(const std::filesystem::path& aRoot) -> int
	{
		using namespace bpr;
		adapters::logging::quill_log log{ aRoot };
		const auto                   credential = std::as_bytes(std::span{ tests::capability_error_canary_credential });
		for (const auto& canary : tests::capability_error_content_canaries) {
			const auto summary = adapters::github::detail::sanitize_capability_error(canary.Body, credential);
			expect(log.record({ .Operation = application::diagnostic_operation::capability, .Outcome = application::diagnostic_outcome::response_received, .HttpStatus = 200U, .Summary = summary }) == application::diagnostic_log_status::available, "content audit reaches the real diagnostic sink");
		}
		log.close();
		const auto bytes    = read(aRoot / "logs/diagnostics.log");
		const auto exported = application::prepare_release_diagnostics(bytes, core::release_maintenance_revision{ 1U });
		expect(exported && exported->Descriptor.Records == tests::capability_error_content_canaries.size(), "content audit reaches the actual export classifier");
		const bool credentialRetained = bytes.find(tests::capability_error_canary_credential) != std::string::npos;
		bool       contentRetained    = false;
		bool       exportRetained     = false;
		for (const auto& canary : tests::capability_error_content_canaries) {
			const bool retained        = bytes.find(canary.Marker) != std::string::npos;
			const bool exportedContent = exported->Bytes.find(canary.Marker) != std::string::npos;
			contentRetained            = retained || contentRetained;
			exportRetained             = exportedContent || exportRetained;
			std::cout << "diagnostic-content-case=" << canary.Name << " retained=" << retained << " exported=" << exportedContent << '\n';
		}
		std::cout << "diagnostic-content-audit credential_retained=" << credentialRetained
				  << " content_retained=" << contentRetained << " export_retained=" << exportRetained << '\n';
		return credentialRetained || contentRetained || exportRetained ? EXIT_FAILURE : EXIT_SUCCESS;
	}

	void hostile_summary_limits(const std::filesystem::path& aRoot)
	{
		using namespace bpr;
		const std::string credential = "synthetic-bound-credential";
		const std::array  bodies{
			std::string(1024U * 1024U + 1U, 'x'),
			std::string{ R"({"errors":[{"message":")" } + std::string(8193U, 'x') + R"("}]})",
			std::string{ R"({"errors":[{"message":")" } + std::string(2048U, ' ') + R"("}]})",
			std::string{ R"({"errors":[{"message":"private content canary"}],"errors":[]})" },
			std::string{ R"({"errors":[{"message":42,"source":"private content canary"}]})" }
		};
		adapters::logging::quill_log log{ aRoot };
		for (const auto& body : bodies) {
			const auto summary = adapters::github::detail::sanitize_capability_error(body, std::as_bytes(std::span{ credential }));
			expect(summary.text().empty() && (summary.State == application::diagnostic_summary_state::omitted_limit || summary.State == application::diagnostic_summary_state::omitted_invalid), "oversized or malformed summary has only an omission reason");
			expect(log.record({ .Operation = application::diagnostic_operation::capability, .Summary = summary }) == application::diagnostic_log_status::available, "omitted summary reaches the real diagnostic file without raw fallback");
		}
		log.close();
		const auto bytes = read(aRoot / "logs/diagnostics.log");
		expect(bytes.find("canary") == std::string::npos && bytes.find("xxxxxxxx") == std::string::npos, "malformed and oversized source is absent from retained diagnostics");
	}

	void stress_loss_reporting(const std::filesystem::path& aRoot)
	{
		using namespace bpr;
		using clock                               = std::chrono::steady_clock;
		using status                              = application::diagnostic_log_status;
		constexpr std::uint64_t      maximumBytes = 256U * 1024U * 1024U;
		adapters::logging::quill_log log{ aRoot, maximumBytes };
		std::atomic<std::uint64_t>   dropped{ 0U };
		std::barrier                 ready{ 8 };
		std::vector<std::jthread>    writers;
		writers.reserve(8U);
		const auto started = clock::now();
		for (std::size_t writer = 0U; writer < 8U; ++writer) {
			writers.emplace_back([&] {
				auto event = application::diagnostic_event{ };
				event.Summary.Text.fill('x');
				event.Summary.Size = static_cast<std::uint16_t>(event.Summary.Text.size());
				ready.arrive_and_wait();
				for (std::size_t index = 0U; index < 10000U; ++index) {
					if (log.record(event) == status::dropped) {
						++dropped;
					}
				}
			});
		}
		writers.clear();
		expect(dropped != 0U, "concurrent pressure produces observed loss");
		const auto deadline    = clock::now() + std::chrono::seconds{ 5 };
		auto       finalStatus = status::dropped;
		while (finalStatus == status::dropped && clock::now() < deadline) {
			finalStatus = log.record({ .Request = 971U });
			if (finalStatus == status::dropped) {
				++dropped;
				std::this_thread::yield();
			}
		}
		expect(finalStatus == status::available, "diagnostic loss is followed by an admitted final summary");
		const auto closeStarted = clock::now();
		log.close();
		const auto closeMs = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - closeStarted).count();
		const auto bytes   = read(aRoot / "logs/diagnostics.log");
		const auto marker  = bytes.find("request=971 ");
		expect(marker != std::string::npos, "final accepted record survives ordinary close");
		const auto    count    = bytes.find("dropped=", marker);
		std::uint64_t reported = 0U;
		expect(count != std::string::npos && std::from_chars(bytes.data() + count + 8U, bytes.data() + bytes.size(), reported).ec == std::errc{ }, "final record carries a numeric loss count");
		expect(reported == dropped.load(), "every observed drop is reported exactly once after pressure subsides");
		expect(bytes.size() <= maximumBytes, "stress preserves the configured file ceiling");
		std::cout << "logging-stress attempts=80000 dropped=" << reported << " bytes=" << bytes.size()
				  << " elapsed_ms=" << std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - started).count()
				  << " close_ms=" << closeMs << '\n';
	}
}
int main(int aArgc, char** aArgv)
try {
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
	const auto root = private_root(std::filesystem::path{ QFileInfo{ temporary.path() }.canonicalFilePath().toStdString() });
	const auto file = root / "logs/diagnostics.log";
	if (aArgc == 2 && std::string_view{ aArgv[1] } == "--audit-content-policy") {
		return audit_content_policy(root);
	}
	hostile_summary_limits(private_root(root / "limits"));
	stress_loss_reporting(private_root(root / "stress"));
	{
		adapters::logging::quill_log log{ root };
		expect(log.status() == status::available, "available local sink");
		expect(log.record({ }) == status::available, "accepted startup");
#if !defined(_WIN32)
		// [[BPR-1051]]. Quill's singleton check creates this file when the
		// backend starts, and only a clean stop removes it, so a killed process
		// left it behind. The adapter turns the check off.
		expect(!std::filesystem::exists("/tmp/QuillBackendLock" + std::to_string(::getpid())), "a running backend creates no Quill lock file in /tmp");
#endif
		auto hostile            = application::diagnostic_event{ };
		hostile.Summary.Text[0] = '\n';
		hostile.Summary.Size    = 1;
		expect(log.record(hostile) == status::dropped, "control-bearing event never enters queue");
		const std::string secret  = "runtime-only-file-secret";
		const auto        summary = adapters::github::detail::sanitize_capability_error(R"({"errors":[{"message":"Unknown schema field newField runtime-only-file-secret","extensions":{"code":"NEW_SCHEMA_CODE"}}]})", std::as_bytes(std::span{ secret }));
		expect(log.record({ .Operation = application::diagnostic_operation::capability, .Outcome = application::diagnostic_outcome::response_received, .Request = 42, .HttpStatus = 200, .Summary = summary }) == status::available, "sanitized prose queued for real file");
		expect(log.record({ .Operation = application::diagnostic_operation::pull_request, .Outcome = application::diagnostic_outcome::response_received, .Request = 43, .WindowSession = 2, .HttpStatus = 403, .PagedOperation = application::diagnostic_paged_operation::list_pull_request_files }) == status::available, "typed paged response queued");
		expect(log.record({ .Operation = application::diagnostic_operation::pull_request, .Outcome = application::diagnostic_outcome::failed, .Request = 43, .PagedOperation = application::diagnostic_paged_operation::list_pull_request_files, .GithubFailure = core::github_failure::rate_limit }) == status::available, "typed pull request failure queued");
		expect(log.record({ .Operation = application::diagnostic_operation::pull_request, .Outcome = application::diagnostic_outcome::failed, .Request = 44, .GithubFailure = application::github_read_validation_error::missing_required_text }) == status::available, "typed validation failure queued");
		expect(log.record({ .Operation = application::diagnostic_operation::pull_request, .Outcome = application::diagnostic_outcome::failed, .Request = 45, .PagedOperation = application::diagnostic_paged_operation::search_pull_requests, .GithubFailure = application::github_read_validation_error::invalid_relationship, .RelationshipDetail = application::github_read_validation_error::relationship_actor_login }) == status::available, "fixed relationship diagnostic queued");
		expect(log.record({ .Operation = application::diagnostic_operation::pull_request, .Outcome = application::diagnostic_outcome::failed, .Request = 46, .HttpStatus = 404, .PagedOperation = application::diagnostic_paged_operation::get_repository, .GithubFailure = core::github_failure::permission, .TransportFailureSource = application::diagnostic_transport_failure_source::http_response }) == status::available, "HTTP denial metadata queued");
		expect(log.record({ .Operation = application::diagnostic_operation::pull_request, .Outcome = application::diagnostic_outcome::failed, .Request = 47, .PagedOperation = application::diagnostic_paged_operation::get_repository, .GithubFailure = core::github_failure::permission, .TransportFailureSource = application::diagnostic_transport_failure_source::network_reply }) == status::available, "network denial has no fabricated HTTP status");
		expect(log.record({ .TransportFailureSource = static_cast<application::diagnostic_transport_failure_source>(255U) }) == status::dropped, "unknown source never enters log");
		expect(log.record({ .HttpStatus = 600U }) == status::dropped, "invalid HTTP status never enters log");
		log.close();
		expect(log.record({ }) == status::closed, "closed ingress");
	}

	{
		adapters::logging::quill_log log{ root };
		expect(log.record({ .Operation = application::diagnostic_operation::epoch, .Outcome = application::diagnostic_outcome::failed, .Request = 19U, .EpochStage = application::diagnostic_epoch_stage::protocol_acquire, .EpochFailure = application::git_protocol_v2_failure::pack_sink, .EpochPackFailure = application::git_pack_stream_failure::input_limit }) == status::available, "typed epoch detail accepted by real sink");
	}
	{
		const auto                   firstRoot  = private_root(root / "profile-first");
		const auto                   secondRoot = private_root(root / "profile-second");
		adapters::logging::quill_log firstLog{ firstRoot };
		adapters::logging::quill_log secondLog{ secondRoot };
		expect(firstLog.status() == status::available && secondLog.status() == status::available, "concurrent profiles both have logging");
		expect(firstLog.record({ .Request = 7001 }) == status::available, "first profile event");
		expect(secondLog.record({ .Request = 7002 }) == status::available, "second profile event");
		{
			adapters::logging::quill_log unavailable{ "relative" };
			expect(unavailable.status() == status::unavailable, "invalid sibling stays unavailable");
			adapters::logging::quill_log full{ private_root(root / "profile-full"), 1U };
			static_cast<void>(full.record({ }));
			full.close();
			expect(full.status() == status::full, "one profile's cap is isolated");
		}
		firstLog.close();
		{
			adapters::logging::quill_log reopened{ firstRoot };
			expect(reopened.record({ .Request = 7003 }) == status::available, "closed profile releases its file while sibling remains live");
		}
		expect(secondLog.record({ .Request = 7004 }) == status::available, "closing initial logger leaves sibling recording");
		secondLog.close();
		const auto firstText  = read(firstRoot / "logs/diagnostics.log");
		const auto secondText = read(secondRoot / "logs/diagnostics.log");
		expect(firstText.find("request=7001 ") != std::string::npos && firstText.find("request=7003 ") != std::string::npos
				   && firstText.find("request=7002 ") == std::string::npos && firstText.find("request=7004 ") == std::string::npos,
			   "first log contains only its own events");
		expect(secondText.find("request=7002 ") != std::string::npos && secondText.find("request=7004 ") != std::string::npos
				   && secondText.find("request=7001 ") == std::string::npos && secondText.find("request=7003 ") == std::string::npos,
			   "second log contains only its own events");
		adapters::logging::quill_log firstAgain{ firstRoot };
		adapters::logging::quill_log secondAgain{ secondRoot };
		secondAgain.close();
		expect(firstAgain.record({ .Request = 7005 }) == status::available, "reverse close order also keeps survivor live");
		firstAgain.close();
		expect(read(firstRoot / "logs/diagnostics.log").find("request=7005 ") != std::string::npos, "backend restarts after final owner closes");
	}
	{
		std::barrier              ready{ 4 };
		std::vector<std::jthread> writers;
		writers.reserve(4);
		for (auto index = 0; index < 4; ++index) {
			writers.emplace_back([&, index] {
				adapters::logging::quill_log log{ private_root(root / ("concurrent-" + std::to_string(index))) };
				expect(log.status() == status::available, "concurrent logger admission");
				ready.arrive_and_wait();
				expect(log.record({ .Request = static_cast<std::uint64_t>(8000 + index) }) == status::available, "concurrent logger record");
				log.close();
			});
		}
		writers.clear();
		for (auto index = 0; index < 4; ++index) {
			const auto text = read(root / ("concurrent-" + std::to_string(index)) / "logs/diagnostics.log");
			expect(text.find("request=" + std::to_string(8000 + index) + " ") != std::string::npos, "concurrent close drains each owned logger");
		}
	}
	{
		adapters::logging::quill_log busy{ private_root(root / "busy-profile"), 256U * 1024U * 1024U };
		adapters::logging::quill_log healthy{ private_root(root / "healthy-profile") };
		auto                         event = application::diagnostic_event{ };
		event.Summary.Text.fill('x');
		event.Summary.Size  = static_cast<std::uint16_t>(event.Summary.Text.size());
		event.Summary.State = application::diagnostic_summary_state::sanitized;
		auto dropped        = false;
		for (auto index = 0; index < 100000 && !dropped; ++index) {
			dropped = busy.record(event) == status::dropped;
		}
		expect(dropped, "actual bounded frontend queue overflow exercised");
		busy.close();
		expect(healthy.record({ .Request = 9001 }) == status::available, "overflow does not disable healthy sibling ingress");
		healthy.close();
		// Final backend drainage checks Quill's delayed failure counters too.
		expect(healthy.status() == status::available, "informational queue-drop report is not a shared backend failure");
		expect(read(root / "healthy-profile/logs/diagnostics.log").find("request=9001 ") != std::string::npos, "healthy sibling event survives overflow drainage");
	}
	const auto first    = read(file);
	const auto exported = application::prepare_release_diagnostics(first, core::release_maintenance_revision{ 1U });
	expect(exported && exported->Descriptor.Records != 0U && exported->Bytes.find("runtime-only-file-secret") == std::string::npos
			   && exported->Bytes.find("newField") == std::string::npos && exported->Bytes.find("request=") == std::string::npos,
		   "real Quill log yields bounded metadata-only maintenance preview without prose or identity");
	expect(first.find("epoch_stage=protocol_acquire failure_domain=protocol failure=pack_sink pack_failure=input_limit") != std::string::npos, "fixed protocol and pack failure names persisted");
	expect(first.find("newField") != std::string::npos && first.find("NEW_SCHEMA_CODE") != std::string::npos && first.find("request=42 http_status=200") != std::string::npos, "useful sanitized prose and correlation survive file encoding");
	expect(first.find("runtime-only-file-secret") == std::string::npos, "credential canary absent from persisted file");
	expect(first.find("op=github.pull_request outcome=response_received request=43 http_status=403") != std::string::npos
			   && first.find("failure_domain=github failure=rate_limit pack_failure=none page_operation=ListPullRequestFiles") != std::string::npos
			   && first.find("failure_domain=validation failure=missing_required_text") != std::string::npos
			   && first.find("page_operation=ListPullRequestFiles window_session=2") != std::string::npos,
		   "typed HTTP, provider and validation facts persist without arbitrary text");
	expect(first.find("failure_domain=validation failure=invalid_relationship pack_failure=none page_operation=SearchPullRequests window_session=0 relationship_detail=relationship_actor_login") != std::string::npos, "relationship reason is independent of the final fetched operation");
	expect(first.find("request=46 http_status=404") != std::string::npos
			   && first.find("failure=permission pack_failure=none page_operation=GetRepository window_session=0 relationship_detail=none transport_source=http_response") != std::string::npos
			   && first.find("request=47 http_status=0") != std::string::npos
			   && first.find("transport_source=network_reply") != std::string::npos,
		   "persisted permission diagnostics distinguish HTTP response from network reply");
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
	using failure = adapters::logging::diagnostic_file_failure;
	// [[BPR-0885]]. Every refusal names its cause. POSIX pins the exact cause.
	// Windows admits a root through its own chain of checks, which can name
	// the same refusal differently, so there the cause only has to be named.
	const auto refusedNaming = [](const adapters::logging::quill_log& aLog, const failure aPosixCause) {
#if defined(_WIN32)
		static_cast<void>(aPosixCause);
		return aLog.status() == status::unavailable && aLog.open_failure() != failure::none;
#else
		return aLog.status() == status::unavailable && aLog.open_failure() == aPosixCause;
#endif
	};
#if !defined(_WIN32)
	const auto unsafe = private_root(root / "unsafe");
	std::filesystem::create_directory_symlink(root / "logs", unsafe / "logs");
	{
		adapters::logging::quill_log log{ unsafe };
		expect(refusedNaming(log, failure::invalid_permissions), "symlink directory refused, and named");
	}
	std::filesystem::remove(unsafe / "logs");
	std::filesystem::create_directory(unsafe / "logs");
	std::filesystem::permissions(unsafe / "logs", std::filesystem::perms::owner_all);
	std::filesystem::create_symlink(file, unsafe / "logs/diagnostics.log");
	{
		adapters::logging::quill_log log{ unsafe };
		expect(refusedNaming(log, failure::invalid_permissions), "symlink file refused, and named");
	}
	{
		const auto target = private_root(root / "link-target");
		std::filesystem::create_directory_symlink(target, root / "link-root");
		adapters::logging::quill_log log{ root / "link-root" };
		expect(refusedNaming(log, failure::invalid_root) && !std::filesystem::exists(target / "logs"), "a root that is itself a link is refused, named, and left untouched");
	}
	{
		const auto shared = root / "shared-root";
		std::filesystem::create_directory(shared);
		std::filesystem::permissions(shared, std::filesystem::perms::owner_all | std::filesystem::perms::group_all, std::filesystem::perm_options::replace);
		adapters::logging::quill_log log{ shared };
		expect(refusedNaming(log, failure::invalid_permissions) && !std::filesystem::exists(shared / "logs"), "a root its group can write is refused, named, and left untouched");
	}
#endif
	{
		adapters::logging::quill_log log{ file };
		expect(refusedNaming(log, failure::invalid_root), "non-directory root refused, and named");
	}
	{
		adapters::logging::quill_log log{ "relative" };
		expect(refusedNaming(log, failure::invalid_root), "relative root refused, and named");
	}
	{
		adapters::logging::quill_log missing{ root / "missing-parent/missing-root" };
		expect(refusedNaming(missing, failure::invalid_root) && !std::filesystem::exists(root / "missing-parent"), "logger never creates unowned ancestors or root");
		adapters::logging::quill_log firstWriter{ root };
		adapters::logging::quill_log secondWriter{ root };
		expect(firstWriter.status() == status::available && secondWriter.open_failure() == failure::busy, "native sink has exactly one writer");
	}
#if !defined(_WIN32)
	{
		const auto realParent = private_root(root / "real-parent");
		private_root(realParent / "profile");
		std::filesystem::create_directory_symlink(realParent, root / "alias-parent");
		adapters::logging::quill_log aliased{ root / "alias-parent/profile" };
		expect(aliased.status() == status::available && aliased.open_failure() == failure::none, "existing symlinked ancestor resolves without creating shared directories");
	}
#endif
	std::cout << "quill-logging-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
