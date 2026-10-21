#include "../../src/adapters/native/windows_private_storage.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/device_binding_store.hpp"
#include "bpr/adapters/sqlite/insights_storage.hpp"
#include "bpr/adapters/sqlite/profile_runtime_lease.hpp"
#include "bpr/adapters/sqlite/release_maintenance_files.hpp"
#include "bpr/adapters/sqlite/remembered_connection_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/release_diagnostic_snapshot.hpp"
#include "platform_test_support.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#if !defined(_WIN32)
	#include <sys/stat.h>
	#include <sys/wait.h>
	#include <unistd.h>
#endif

namespace {
	using namespace bpr;
	using reason  = core::release_maintenance_reason;
	using command = core::release_maintenance_command;
	namespace fs  = std::filesystem;
	using bpr::tests::expect;
	struct scratch
	{
		fs::path Path;
		scratch()
		{
			Path = fs::temp_directory_path() / ("bpr-maintenance-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
			// [[BPR-1121]]. Removed again if the rest of construction fails.
			const auto created = fs::create_directory(Path);
			auto       made    = bpr::tests::scratch_guard{ created ? Path : fs::path{ } };
			expect(created && adapters::sqlite::make_storage_root_owner_private(Path), "private disposable root");
			Path = fs::canonical(Path);
			made.release();
		}
		~scratch()
		{
			std::error_code ignored;
			fs::remove_all(Path, ignored);
		}
		scratch(const scratch&)                    = delete;
		auto operator=(const scratch&) -> scratch& = delete;
	};
	void write(const fs::path& aPath, std::string_view aBytes)
	{
		fs::create_directories(aPath.parent_path());
		for (auto path = aPath.parent_path(); path != path.parent_path(); path = path.parent_path()) {
			if (path.filename().string().starts_with("bpr-maintenance-")) {
				break;
			}
			expect(adapters::sqlite::make_storage_root_owner_private(path), "private native fixture directory");
		}
		{
			std::ofstream file{ aPath, std::ios::binary };
			file.write(aBytes.data(), static_cast<std::streamsize>(aBytes.size()));
			expect(file.good(), "write fixture");
		}
		fs::permissions(aPath, fs::perms::owner_read | fs::perms::owner_write);
	}
	auto read(const fs::path& aPath) -> std::string
	{
		std::ifstream file{ aPath, std::ios::binary };
		return { std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{ } };
	}
	auto lease(const fs::path& aPath) -> std::shared_ptr<application::profile_runtime_lease>
	{
		auto result = adapters::sqlite::acquire_profile_runtime_lease(aPath);
		expect(result.has_value(), "native lease admitted");
		return *result;
	}
	auto context(const adapters::sqlite::release_maintenance_files& aFiles, command aCommand) -> core::release_maintenance_context
	{
		return { aFiles.authority(), core::request_id{ 1U }, core::release_maintenance_generation{ 1U }, aCommand, { } };
	}
	class credentials final : public application::release_profile_cleanup_port
	{
	public:
		int                   Erased = 0;
		bool                  Fail   = false;
		std::function<void()> During;
		auto                  inspect(std::stop_token) -> std::expected<application::release_profile_resource_inventory, reason> override
		{
			application::release_profile_resource_inventory inventory{ { }, 2U };
			inventory.Digest[0] = 42U;
			return inventory;
		}
		auto erase(const application::release_profile_resource_inventory& aInventory) -> application::release_maintenance_result override
		{
			expect(aInventory == *inspect({ }), "exact remembered credential inventory");
			++Erased;
			if (During) {
				During();
			}
			if (Fail) {
				return std::unexpected(reason::io_failure);
			}
			return { };
		}
	};
	class lifecycle final : public application::release_maintenance_lifecycle_port
	{
	public:
		bool Drained = false;
		void quiesce(const core::release_maintenance_context&, std::stop_token,
					 std::function<void(application::release_maintenance_result)> aCompletion) override
		{
			Drained = true;
			aCompletion({ });
		}
	};
	class projection final : public application::release_maintenance_projection_port
	{
	public:
		application::release_maintenance_projection Last;
		void                                        publish(const application::release_maintenance_projection& aProjection) override { Last = aProjection; }
	};
	class vault final : public application::github_credential_vault_port
	{
	public:
		std::vector<application::github_credential_operation_context> Deleted;
		bool                                                          FailRefresh    = false;
		bool                                                          WrongAuthority = false;
		void                                                          store(application::github_credential_enrollment, application::github_credential_store_completion_handler) override { expect(false, "cleanup must not store credentials"); }
		void                                                          retrieve(application::github_credential_operation_context, application::github_credential_retrieve_completion_handler) override { expect(false, "cleanup must not retrieve credentials"); }
		void                                                          store_refresh(application::github_credential_enrollment, application::github_credential_store_completion_handler) override { expect(false, "cleanup must not store refresh material"); }
		void                                                          retrieve_refresh(application::github_credential_operation_context, application::github_credential_retrieve_completion_handler) override { expect(false, "cleanup must not retrieve refresh material"); }
		void                                                          cancel(application::github_credential_operation_context) override { expect(false, "irreversible vault cleanup owes its terminal"); }
		void                                                          delete_credential(application::github_credential_operation_context aContext, application::github_credential_delete_completion_handler aCompletion) override
		{
			Deleted.push_back(aContext);
			if (WrongAuthority) {
				aContext.Authority.Account = core::github_account_id{ 999U };
			}
			aCompletion(application::github_credential_delete_success{ aContext, application::github_credential_delete_disposition::deleted });
		}
		void delete_refresh(application::github_credential_operation_context aContext, application::github_credential_delete_completion_handler aCompletion) override
		{
			Deleted.push_back(aContext);
			if (FailRefresh) {
				aCompletion(application::github_credential_delete_error{ aContext, application::github_credential_vault_failure::locked });
			} else {
				aCompletion(application::github_credential_delete_success{ aContext, application::github_credential_delete_disposition::absent });
			}
		}
	};
	void seed(const fs::path& aRoot)
	{
		write(aRoot / "logs/diagnostics.log", "2026-09-20T01:00:00Z v=1 op=application outcome=failed request=99 http_status=0 summary=sanitized dropped=0 detail=TOKEN_CANARY SOURCE_CANARY COMMENT_CANARY\nlegacy SECRET_CANARY\n");
		write(aRoot / "state/v1/appearance-settings.sqlite3", "private source fixture");
		write(aRoot / "state/v1/appearance-settings.sqlite3-wal", "sidecar fixture");
		write(aRoot / "state/v1/review-tours.sqlite3", "private tour prose");
		write(aRoot / "state/v1/review-tours.sqlite3-wal", "tour sidecar");
		write(aRoot / "state/v1/review-cycles.sqlite3", "private cycle source");
		write(aRoot / "state/v1/review-cycles.sqlite3-wal", "cycle sidecar");
		write(aRoot / "state/v1/review-overviews.sqlite3", "private cached overview");
		write(aRoot / "state/v1/review-overviews.sqlite3-wal", "cached overview sidecar");
		write(aRoot / "state/v1/markdown-images.sqlite3", "cached body images");
		write(aRoot / "state/v1/markdown-images.sqlite3-wal", "cached body image sidecar");
		{
			auto       connections = adapters::sqlite::make_remembered_connection_store(aRoot, core::default_bpr_profile_id);
			const auto before      = connections->load({ });
			expect(before.has_value(), "actual connection metadata store opens in maintenance fixture");
			const auto disabled = core::disable_connection_reuse(*before, before->Revision, "github.com");
			expect(disabled && connections->save(*disabled, before->Revision, { }).has_value(), "real disable metadata admitted before inventory");
		}
		write(aRoot / "state/v1/remembered-connections.sqlite3-wal", "connection sidecar");
		write(aRoot / "state/v1/profile-registry.sqlite3", "shared registry");
		write(aRoot / "state/v1/release-update.sqlite3", "shared updater");
		write(aRoot / "profiles/profile-2/keep", "sibling fixture");
		write(aRoot / "updates/transaction-1/keep", "shared transaction");
		auto insights = adapters::sqlite::make_insights_storage(aRoot, [] { return std::vector<application::insights_storage_row>{ }; });
		expect(insights->measure().TotalBytes.has_value(), "Insights has a measured local summary");
		core::insights_review_event event;
		event.Kind        = core::insights_review_kind::submitted;
		event.Host        = "github.com";
		event.Repository  = "private/stats-canary";
		event.PullRequest = 7;
		event.Identity    = "review-77";
		event.LocalDay    = 20000;
		event.UnixSeconds = 1728000000;
		insights->record_reviews({ event });
	}
	void test_host_native_public_path()
	{
		scratch root;
		scratch destination;
		seed(root.Path);
		const auto segment = root.Path / "logs/session-00000000000000000000000000000001-00000001.jsonl";
		write(segment, "2026-09-20T01:00:00Z v=1 op=application outcome=started request=99 http_status=0 summary=absent dropped=0 detail=partial diagnostic tail");
		write(root.Path / "logs/session-00000000000000000000000000000002-00000001.jsonl", "2026-09-20T01:00:00Z v=1 op=application outcome=shutdown request=1 http_status=0 summary=absent dropped=0 detail=\n");
		write(root.Path / "logs/.retention.lock", "");
		auto                                              owner = lease(root.Path);
		credentials                                       vault;
		adapters::sqlite::release_maintenance_files       files{ root.Path, owner, core::bpr_profile_id{ 1U }, core::application_session_id{ 1U }, &vault };
		lifecycle                                         barrier;
		projection                                        projection;
		application::manual_executor                      effects;
		application::manual_executor                      decisions;
		application::release_maintenance_application_host host{ files, barrier, effects, decisions, projection };
		auto                                              drain = [&] { for (unsigned turn = 0; turn < 8U && !host.is_quiescent(); ++turn) { effects.drain(); decisions.drain(); } expect(host.is_quiescent(), "native host quiescent"); };
		host.dispatch({ files.authority(), command::preview_diagnostics, { } });
		drain();
		expect(host.state().Prepared && projection.Last.Preview, "native snapshot reaches host projection");
		auto expected = *projection.Last.Preview;
		expect(expected.find("outcome=5") != std::string::npos && expected.find("partial") == std::string::npos, "a partial session tail cannot swallow the following file or escape classification");
		const auto snapshot = std::get<core::release_diagnostic_snapshot>(host.state().Prepared->Selection);
		expect(snapshot.Records == 2U && snapshot.OmittedRecords == 2U, "one partial tail is counted once alongside the malformed legacy record");
		expect(expected.find("CANARY") == std::string::npos && expected.find("99") == std::string::npos, "actual snapshot has no source, comment, token or request id");
		auto output = adapters::sqlite::admit_release_diagnostic_output(destination.Path / "diagnostics.txt");
		expect(output.has_value(), "local output admitted");
		host.dispatch({ files.authority(), command::export_diagnostics, host.state().Prepared }, *output);
		drain();
		expect(host.state().Phase == core::release_maintenance_phase::completed && read(destination.Path / "diagnostics.txt") == expected, "public host exports exactly preview bytes");
#if !defined(_WIN32)
		struct stat status{ };
		expect(::stat((destination.Path / "diagnostics.txt").c_str(), &status) == 0 && (status.st_mode & 0777U) == 0600U, "output owner-only permissions");
#else
		const auto canonical = adapters::native_storage::canonical_windows_root(destination.Path);
		const auto security  = adapters::native_storage::private_security::load();
		expect(canonical && security && adapters::native_storage::open_private_object(*canonical / L"diagnostics.txt", adapters::native_storage::windows_object_kind::regular_file, *security).has_value(), "output exact owner-only DACL and single native link");
#endif
		expect(!adapters::sqlite::admit_release_diagnostic_output(destination.Path / "diagnostics.txt"), "existing output not overwritten");
		auto anonymized                 = context(files, command::preview_diagnostics);
		anonymized.ExportOptions.Policy = core::diagnostic_export_policy::anonymized;
		const auto anonymousPreview     = files.prepare(anonymized, { });
		expect(anonymousPreview && (*anonymousPreview)->preview().find("\"section\":\"insights\"") != std::string_view::npos, "actual frozen anonymized preview carries the Insights section");
		expect((*anonymousPreview)->preview().find("CANARY") == std::string_view::npos && (*anonymousPreview)->preview().find("appearance-settings") == std::string_view::npos, "Insights cannot add identity or store names to the export");
		anonymized.ExportOptions.Policy = core::diagnostic_export_policy::identifying;
		const auto identifyingPreview   = files.prepare(anonymized, { });
		expect(identifyingPreview && (*identifyingPreview)->preview().find("\"section\":\"insights\"") == std::string_view::npos, "identifying export gains nothing from Insights");
		host.dispatch({ files.authority(), command::prepare_clear, { } });
		drain();
		expect(host.state().Prepared.has_value(), "clear diagnostics inventory prepared");
		host.dispatch({ files.authority(), command::clear_diagnostics, host.state().Prepared });
		drain();
		expect(host.state().Phase == core::release_maintenance_phase::completed && vault.Erased == 0
				   && fs::exists(root.Path / "logs/.retention.lock") && !fs::exists(segment) && !fs::exists(root.Path / "logs/diagnostics.log") && fs::exists(root.Path / "state/v1/appearance-settings.sqlite3"),
			   "clear owns only log artifacts and leaves credentials and stores");
		{
			auto kept = adapters::sqlite::make_insights_storage(root.Path, [] { return std::vector<application::insights_storage_row>{ }; });
			expect(kept->reviews(2000000000).Ranges[2][static_cast<std::size_t>(core::insights_review_metric::submitted)] == 1, "actual Clear local diagnostics preserves lifetime review totals");
		}
		host.dispatch({ files.authority(), command::prepare_purge, { } });
		drain();
		expect(barrier.Drained && host.state().Prepared, "native inventory after producer barrier");
		host.dispatch({ files.authority(), command::purge_profile, host.state().Prepared });
		drain();
		expect(host.state().Stopped && vault.Erased == 1, "profile purge stops runtime after exact credential deletion");
		expect(!fs::exists(root.Path / "logs/diagnostics.log") && !fs::exists(root.Path / "state/v1/appearance-settings.sqlite3-wal"), "data and sidecars deleted");
		expect(!fs::exists(root.Path / "state/v1/review-overviews.sqlite3") && !fs::exists(root.Path / "state/v1/review-overviews.sqlite3-wal"), "profile purge removes cached overview and sidecar");
		expect(!fs::exists(root.Path / "state/v1/markdown-images.sqlite3") && !fs::exists(root.Path / "state/v1/markdown-images.sqlite3-wal"), "profile purge removes the image cache and sidecar");
		expect(!fs::exists(root.Path / "state/v1/review-tours.sqlite3") && !fs::exists(root.Path / "state/v1/review-tours.sqlite3-wal"), "profile purge removes tour prose and baselines");
		expect(!fs::exists(root.Path / "state/v1/insights.sqlite3"), "profile purge removes the Insights lifetime store");
		expect(!fs::exists(root.Path / "state/v1/review-cycles.sqlite3") && !fs::exists(root.Path / "state/v1/review-cycles.sqlite3-wal"), "profile purge removes local review cycles");
		expect(!fs::exists(root.Path / "state/v1/remembered-connections.sqlite3") && !fs::exists(root.Path / "state/v1/remembered-connections.sqlite3-wal"), "profile purge removes remembered connection metadata and sidecar");
		expect(read(root.Path / "profiles/profile-2/keep") == "sibling fixture" && read(root.Path / "state/v1/profile-registry.sqlite3") == "shared registry", "sibling and registry preserved");
		expect(read(root.Path / "updates/transaction-1/keep") == "shared transaction" && read(root.Path / "state/v1/release-update.sqlite3") == "shared updater", "desktop updater preserved");
		expect(read(destination.Path / "diagnostics.txt") == expected && !fs::exists(root.Path / ".bpr-maintenance-v1"), "export survives and completed journal removed");
		host.shutdown();
	}
	/// [[BPR-1717]]. Retained logs larger than the export's input limit
	/// leave the oldest files unread and counted. Before, files were read in
	/// name order and the first that overran the limit refused the export.
	void test_export_reads_newest_files()
	{
		scratch    root;
		const auto lines = [](unsigned aDropped, std::size_t aBytes) {
			const auto  line = "2026-09-20T01:00:00Z v=1 op=application outcome=started request=1 http_status=0 summary=absent dropped=" + std::to_string(aDropped) + " detail=" + std::string(4000U, 'x') + "\n";
			std::string bytes;
			while (bytes.size() + line.size() <= aBytes) {
				bytes += line;
			}
			return bytes;
		};
		const auto older = root.Path / "logs/session-11111111111111111111111111111111-00000001.jsonl";
		// [[BPR-1722]]. Sized from the input limit, so the two files
		// overrun it together and the newer one alone fits.
		constexpr auto half = application::release_diagnostic_input_limit / 2U;
		write(older, lines(1U, half + (1024U * 1024U)));
		fs::last_write_time(older, fs::last_write_time(older) - std::chrono::hours{ 1 });
		const auto newer = lines(2U, half);
		write(root.Path / "logs/session-00000000000000000000000000000000-00000001.jsonl", newer);
		auto                                        owner = lease(root.Path);
		adapters::sqlite::release_maintenance_files files{ root.Path, owner, core::bpr_profile_id{ 1U }, core::application_session_id{ 1U } };
		const auto                                  prepared = files.prepare(context(files, command::preview_diagnostics), { });
		expect(prepared.has_value(), "logs over the input limit still export");
		const auto snapshot     = std::get<core::release_diagnostic_snapshot>((*prepared)->selection());
		const auto preview      = (*prepared)->preview();
		const auto newerRecords = static_cast<std::uint32_t>(std::ranges::count(newer, '\n'));
		expect(snapshot.UnreadFiles == 1U && snapshot.Records == std::min(newerRecords, core::release_diagnostic_record_limit) && preview.find("dropped=2\n") != std::string_view::npos && preview.find("dropped=1\n") == std::string_view::npos,
			   "the newest file is read and the older one is left unread and counted");
	}
	void test_inventory_refusals()
	{
		scratch root;
		scratch outside;
		seed(root.Path);
		auto                                        owner = lease(root.Path);
		adapters::sqlite::release_maintenance_files files{ root.Path, owner, core::bpr_profile_id{ 1U }, core::application_session_id{ 1U } };
		const auto                                  prepare = context(files, command::prepare_purge);
		write(root.Path / "unknown", "leave alone");
		expect(!files.prepare(prepare, { }), "unknown entry refuses whole inventory");
		fs::remove(root.Path / "unknown");
#if !defined(_WIN32)
		fs::create_symlink(outside.Path, root.Path / "logs/alias");
		expect(!files.prepare(prepare, { }), "symlink refused before traversal");
		fs::remove(root.Path / "logs/alias");
#endif
		fs::create_hard_link(root.Path / "logs/diagnostics.log", outside.Path / "hardlink");
		expect(!files.prepare(prepare, { }), "multiply linked file refused");
		fs::remove(outside.Path / "hardlink");
#if !defined(_WIN32)
		fs::permissions(root.Path / "logs/diagnostics.log", fs::perms::owner_all | fs::perms::group_read);
		expect(!files.prepare(prepare, { }), "unsafe permissions refused without repair");
		fs::permissions(root.Path / "logs/diagnostics.log", fs::perms::owner_read | fs::perms::owner_write);
#endif
		auto candidate = files.prepare(prepare, { });
		expect(candidate.has_value(), "valid inventory");
		auto erase      = context(files, command::purge_profile);
		erase.Selection = (*candidate)->selection();
		write(root.Path / "logs/diagnostics.log", "changed");
		bool committed = false;
		expect(!files.erase(erase, **candidate, { }, [&] { committed = true; return true; }) && !committed, "changed file refuses before commit");
		expect(fs::exists(root.Path / "state/v1/appearance-settings.sqlite3"), "refusal preserves unrelated admitted file");
		auto other              = context(files, command::preview_diagnostics);
		other.Authority.Profile = core::bpr_profile_id{ 2U };
		expect(!files.prepare(other, { }), "profile authority mismatch refused");
		std::stop_source stop;
		stop.request_stop();
		expect(!files.prepare(prepare, stop.get_token()), "cancelled preparation refused");
		auto displaced = outside.Path / "displaced";
#if !defined(_WIN32)
		fs::rename(root.Path, displaced);
		fs::create_directory(root.Path);
		fs::permissions(root.Path, fs::perms::owner_all);
		expect(!files.prepare(prepare, { }), "same pathname replacement cannot reuse runtime lease");
		fs::remove(root.Path);
		fs::rename(displaced, root.Path);
#else
		std::error_code error;
		fs::rename(root.Path, displaced, error);
		expect(static_cast<bool>(error) && files.prepare(prepare, { }).has_value(), "retained Windows root denies pathname replacement");
#endif
	}
	[[noreturn]] void interrupt_journal(const fs::path& aRoot)
	{
		auto        owner = lease(aRoot);
		credentials vault;
		vault.During = [] { std::_Exit(17); };
		adapters::sqlite::release_maintenance_files files{ aRoot, owner, core::bpr_profile_id{ 1U }, core::application_session_id{ 1U }, &vault };
		auto                                        candidate = files.prepare(context(files, command::prepare_purge), { });
		if (!candidate) {
			std::_Exit(18);
		}
		auto erase      = context(files, command::purge_profile);
		erase.Selection = (*candidate)->selection();
		static_cast<void>(files.erase(erase, **candidate, { }, [] { return true; }));
		std::_Exit(19);
	}
	void test_durable_recovery()
	{
		scratch root;
		seed(root.Path);
#if !defined(_WIN32)
		const auto child = ::fork();
		expect(child >= 0, "fork journal interruption fixture");
		if (child == 0) {
			// [[BPR-1121]]. The child never unwinds into this process's frames.
			bpr::tests::end_forked_child([&] { interrupt_journal(root.Path); });
		}
		int status = 0;
		expect(::waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 17, "process died after durable journal and before file deletion");
#else
		std::array<wchar_t, 32768U> image{ };
		const auto                  length = ::GetModuleFileNameW(nullptr, image.data(), static_cast<DWORD>(image.size()));
		expect(length != 0U && length < image.size(), "native interruption child image");
		auto         commandLine = std::wstring{ L"\"" } + image.data() + L"\" --interrupt-journal \"" + root.Path.native() + L"\"";
		STARTUPINFOW startup{ };
		startup.cb = sizeof(startup);
		PROCESS_INFORMATION process{ };
		expect(::CreateProcessW(image.data(), commandLine.data(), nullptr, nullptr, FALSE, 0U, nullptr, nullptr, &startup, &process) != 0, "launch native journal interruption child");
		adapters::native_storage::windows_handle child{ process.hProcess };
		adapters::native_storage::windows_handle thread{ process.hThread };
		expect(::WaitForSingleObject(child.Value, 10000U) == WAIT_OBJECT_0, "interrupted child exits promptly");
		DWORD status = 0U;
		expect(::GetExitCodeProcess(child.Value, &status) && status == 17U, "native child died after durable journal");
#endif
		expect(fs::exists(root.Path / ".bpr-maintenance-v1") && fs::exists(root.Path / "logs/diagnostics.log"), "interrupted exact inventory durable");
		auto                                        owner = lease(root.Path);
		credentials                                 vault;
		adapters::sqlite::release_maintenance_files files{ root.Path, owner, core::bpr_profile_id{ 1U }, core::application_session_id{ 2U }, &vault };
		expect(!files.prepare(context(files, command::prepare_purge), { }), "journal blocks new preparation");
		expect(files.recover().has_value() && vault.Erased == 1, "fresh process lifetime replays exact journal");
		expect(!fs::exists(root.Path / "logs/diagnostics.log") && !fs::exists(root.Path / ".bpr-maintenance-v1"), "recovery completes deletion");
		expect(files.recover().has_value(), "completed recovery is idempotent");
	}
	void test_credential_cleanup_and_recovery()
	{
		scratch                                  root;
		const application::device_binding_record binding{
			.Authority = { .Profile = core::host_profile_id{ 3U }, .ProfileRevision = application::github_profile_revision{ 2U }, .Host = core::git_host_id{ 4U }, .Hostname = application::github_canonical_hostname{ "github.com" }, .Account = core::github_account_id{ 5U } },
			.Lifetime  = { 1000, 4600, 10000 }
		};
		auto factory = [&root] { return adapters::sqlite::make_device_binding_store(root.Path); };
		expect(factory()->store(binding, { }).has_value(), "remember exact binding in real store");
		auto  owner = lease(root.Path);
		vault vault;
		vault.FailRefresh = true;
		application::release_profile_credentials    credentials{ core::bpr_profile_id{ 1U }, core::application_session_id{ 9U }, factory, vault };
		adapters::sqlite::release_maintenance_files files{ root.Path, owner, core::bpr_profile_id{ 1U }, core::application_session_id{ 9U }, &credentials };
		auto                                        candidate = files.prepare(context(files, command::prepare_purge), { });
		expect(candidate.has_value(), "real remembered binding inventoried after scoped store closes");
		auto erase      = context(files, command::purge_profile);
		erase.Selection = (*candidate)->selection();
		auto result     = files.erase(erase, **candidate, { }, [] { return true; });
		expect(!result && vault.Deleted.size() == 2U && fs::exists(root.Path / ".bpr-maintenance-v1"), "refresh failure retains durable purge journal");
		auto remaining = factory()->load({ });
		expect(remaining && remaining->size() == 1U && remaining->front() == binding, "binding metadata survives failed vault deletion");
		expect(vault.Deleted[0].Authority == binding.Authority && vault.Deleted[1].Authority == binding.Authority
				   && vault.Deleted[0].Request != vault.Deleted[1].Request,
			   "both slots target only exact authority with separate request identity");
		vault.FailRefresh = false;
		expect(files.recover().has_value(), "native recovery retries exact vault keys before deleting metadata");
		expect(vault.Deleted.size() == 4U && !fs::exists(root.Path / "state/v1/device-bindings.sqlite3"), "metadata removed only after both slots succeed");
		expect(factory()->store(binding, { }).has_value(), "seed isolated stale terminal counterexample");
		auto inspected = credentials.inspect({ });
		expect(inspected.has_value(), "exact credential inventory");
		vault.WrongAuthority = true;
		expect(!credentials.erase(*inspected), "wrong-authority vault success is a failure");
		application::release_profile_credentials foreign{ core::bpr_profile_id{ 2U }, core::application_session_id{ 10U }, factory, vault };
		expect(!foreign.inspect({ }), "foreign BPR profile cannot select remembered keys");
	}
	void test_populated_cache_inventory()
	{
		scratch     root;
		const auto  cache      = std::string(64U, 'a');
		const auto  repository = root.Path / "repositories/v1" / (cache + ".git");
		std::string manifest(141U, '\0');
		manifest.replace(0U, 8U, "BPRCACHE");
		manifest[11U] = '\1';
		manifest[12U] = '\1';
		for (std::size_t index = 85U; index < 117U; ++index) {
			manifest[index] = static_cast<char>(0xaaU);
		}
		write(repository / "betterpullreview-cache-manifest-v1", manifest);
		write(repository / "HEAD", "ref: refs/heads/unused\n");
		write(repository / "config", "[core]\n bare = true\n");
		write(repository / "objects/pack" / ("pack-" + std::string(40U, 'b') + ".pack"), "synthetic pack fixture");
#if defined(_WIN32)
		// [[BPR-1038]]. Git writes its packs read-only, and the purge must
		// delete them as they are.
		fs::permissions(repository / "objects/pack" / ("pack-" + std::string(40U, 'b') + ".pack"), fs::perms::owner_read, fs::perm_options::replace);
		expect((::GetFileAttributesW((repository / "objects/pack" / ("pack-" + std::string(40U, 'b') + ".pack")).c_str()) & FILE_ATTRIBUTE_READONLY) != 0U,
			   "the synthetic pack is read-only, as Git writes it");
#endif
		write(repository / "refs/betterpullreview/v1/scope" / std::string(64U, 'c') / "epoch/000000000000000a/head", std::string(40U, 'd'));
		write(root.Path / "locks/v1" / (cache + ".lock"), "");
		for (const auto name : { "config-global", "config-system", "attributes-system" }) {
			write(root.Path / name, "");
		}
		expect(adapters::sqlite::create_storage_root_owner_private(root.Path / "tmp"), "private Git temporary directory");
		auto                                        owner = lease(root.Path);
		adapters::sqlite::release_maintenance_files files{ root.Path, owner, core::bpr_profile_id{ 1U }, core::application_session_id{ 1U } };
		auto                                        candidate = files.prepare(context(files, command::prepare_purge), { });
		expect(candidate.has_value(), "closed cache layout included in full profile inventory");
		write(root.Path / "tmp/unfinished", "unresolved producer data");
		expect(!files.prepare(context(files, command::prepare_purge), { }), "Git isolation descendants must be recovered by their owner");
		fs::remove(root.Path / "tmp/unfinished");
		write(repository / "developer-file.cpp", "must remain unknown");
		expect(!files.prepare(context(files, command::prepare_purge), { }), "unknown repository entry blocks purge");
		fs::remove(repository / "developer-file.cpp");
		auto erase      = context(files, command::purge_profile);
		erase.Selection = (*candidate)->selection();
		expect(files.erase(erase, **candidate, { }, [] { return true; }).has_value(), "confirmed complete profile removes exact cache objects");
		expect(!fs::exists(repository) && fs::exists(root.Path), "empty cache skeleton removed without removing profile root");
	}
	/// \brief [[BPR-1283]]. Publish as Pull Request ([[BPR-1177]]) gives the
	/// installed Git publication catalog its own isolation root,
	/// `publication-git-v1`, with the same closed layout as the profile's and
	/// one bare object store under `publication/v1`. The purge inventory takes
	/// exactly that layout, as a publish leaves it, and refuses anything else
	/// inside it.
	void test_publication_isolation_inventory()
	{
		scratch    root;
		const auto isolation = root.Path / "publication-git-v1";
		const auto store     = isolation / "publication/v1/sha1.git";
		seed(root.Path);
		write(isolation / ".git-process-boundary.lock", "");
		for (const auto name : { "config-global", "config-system", "attributes-system" }) {
			write(isolation / name, "");
		}
		write(store / "HEAD", "ref: refs/heads/main\n");
		write(store / "config", "[core]\n bare = true\n");
		write(store / "objects/45" / std::string(38U, 'a'), "loose object fixture");
		for (const auto* const name : { "home", "xdg", "tmp", "work", "hooks", "templates", "repositories", "repositories/v1", "incoming", "incoming/v1", "publication/v1/sha1.git/objects/info",
										"publication/v1/sha1.git/objects/pack", "publication/v1/sha1.git/refs", "publication/v1/sha1.git/refs/heads", "publication/v1/sha1.git/refs/tags" }) {
			expect(adapters::sqlite::create_storage_root_owner_private((isolation / name).lexically_normal()), "private publication isolation directory");
		}
		auto                                        owner = lease(root.Path);
		adapters::sqlite::release_maintenance_files files{ root.Path, owner, core::bpr_profile_id{ 1U }, core::application_session_id{ 1U } };
		auto                                        candidate = files.prepare(context(files, command::prepare_purge), { });
		expect(candidate.has_value(), "a profile that planned a publication is inventoried for purge");
		write(store / "developer-file.cpp", "must remain unknown");
		const auto unknown = files.prepare(context(files, command::prepare_purge), { });
		expect(!unknown && unknown.error() == reason::incomplete_inventory, "an unknown entry in the publication store blocks purge");
		fs::remove(store / "developer-file.cpp");
		write(isolation / "tmp/unfinished", "unresolved producer data");
		expect(!files.prepare(context(files, command::prepare_purge), { }), "publication isolation descendants must be recovered by their owner");
		fs::remove(isolation / "tmp/unfinished");
		write(isolation / "publication/v1/other.git/HEAD", "ref: refs/heads/main\n");
		expect(!files.prepare(context(files, command::prepare_purge), { }), "only the catalog's own stores are taken");
		fs::remove_all(isolation / "publication/v1/other.git");
		auto erase      = context(files, command::purge_profile);
		erase.Selection = (*candidate)->selection();
		expect(files.erase(erase, **candidate, { }, [] { return true; }).has_value(), "confirmed purge removes the publication isolation root");
		expect(!fs::exists(isolation) && fs::exists(root.Path / "state/v1/profile-registry.sqlite3"), "the publication root is gone and shared registry state is kept");
	}
#if defined(_WIN32)
	/// \brief [[BPR-1038]]. The installed Git owner makes its isolation
	/// directories with `std::filesystem::create_directory`, and Git makes its
	/// own inside the cache, so on Windows they carry the owner-private entries
	/// they inherit rather than a protected descriptor of their own. The purge
	/// inventory admits exactly that, and refuses a directory with any further
	/// entry as not private, not as a changed selection.
	void test_inherited_git_isolation_directories()
	{
		scratch root;
		seed(root.Path);
		for (const auto* const name : { "home", "xdg", "tmp", "work" }) {
			expect(fs::create_directory(root.Path / name), "Git isolation directory made the way the Git owner makes it");
		}
		// Nested: `v1` inherits from `repositories`, which inherits from the root.
		expect(fs::create_directories(root.Path / "repositories" / "v1"), "nested cache directories made the same way");
		auto                                        owner = lease(root.Path);
		adapters::sqlite::release_maintenance_files files{ root.Path, owner, core::bpr_profile_id{ 1U }, core::application_session_id{ 1U } };
		const auto                                  prepare = context(files, command::prepare_purge);
		expect(files.prepare(prepare, { }).has_value(), "inherited owner-private isolation directories are inventoried");
		expect(bpr::tests::sqlite_contract::make_permissive(root.Path / "home", true), "a further entry on one isolation directory");
		const auto refused = files.prepare(prepare, { });
		expect(!refused && refused.error() == reason::storage_not_private,
			   "a directory with a further entry is refused as not private, not as a changed selection");
	}
#endif
	void test_recovery_refuses_replaced_objects()
	{
		scratch root;
		scratch retained;
		seed(root.Path);
		auto        owner = lease(root.Path);
		credentials vault;
		vault.Fail = true;
		adapters::sqlite::release_maintenance_files files{ root.Path, owner, core::bpr_profile_id{ 1U }, core::application_session_id{ 1U }, &vault };
		auto                                        candidate = files.prepare(context(files, command::prepare_purge), { });
		expect(candidate.has_value(), "recovery counterexample candidate");
		auto erase      = context(files, command::purge_profile);
		erase.Selection = (*candidate)->selection();
		expect(!files.erase(erase, **candidate, { }, [] { return true; }), "injected vault failure leaves exact durable journal");
		vault.Fail = false;
		fs::rename(root.Path / "logs/diagnostics.log", retained.Path / "original");
		write(root.Path / "logs/diagnostics.log", "replacement must survive");
		expect(!files.recover() && read(root.Path / "logs/diagnostics.log") == "replacement must survive"
				   && fs::exists(root.Path / "state/v1/appearance-settings.sqlite3"),
			   "recovery refuses substituted native object without deleting another target");
		fs::remove(root.Path / "logs/diagnostics.log");
		fs::rename(retained.Path / "original", root.Path / "logs/diagnostics.log");
		const auto journal = read(root.Path / ".bpr-maintenance-v1");
		write(root.Path / ".bpr-maintenance-v1", "corrupt journal");
		expect(!files.recover() && fs::exists(root.Path / "logs/diagnostics.log"), "corrupt journal never authorizes fallback traversal");
		write(root.Path / ".bpr-maintenance-v1", journal + "BPRCREX1");
		expect(!files.recover() && fs::exists(root.Path / "logs/diagnostics.log"), "foreign credential receipt never authorizes native deletion");
		write(root.Path / ".bpr-maintenance-v1", journal + "BPRC");
		expect(files.recover().has_value(), "restored inventory and torn credential receipt safely retry before native deletion");
	}
}
auto main(int aArgc, char** aArgv) -> int
try {
	if (aArgc == 3 && std::string_view{ aArgv[1] } == "--interrupt-journal") {
		interrupt_journal(fs::path{ aArgv[2] });
	}
	test_host_native_public_path();
	test_export_reads_newest_files();
	test_inventory_refusals();
	test_durable_recovery();
	test_credential_cleanup_and_recovery();
	test_populated_cache_inventory();
	test_publication_isolation_inventory();
#if defined(_WIN32)
	test_inherited_git_isolation_directories();
#endif
	test_recovery_refuses_replaced_objects();
	std::cout << "release-maintenance-files-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
