#include "../support/capability_error_content_canaries.hpp"
#include "../support/diagnostic_files.hpp"
#include "../support/diagnostic_json.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/diagnostic_preferences.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/application/diagnostic_settings.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/logging/quill_log.hpp"
#include "github_capability_probe.hpp"
#include <QTemporaryDir>
#include <atomic>
#include <filesystem>
#include <thread>
using namespace bpr;
using tests::expect;
namespace {
	auto root(const std::filesystem::path& aParent, std::string aName) -> std::filesystem::path
	{
		auto path = aParent / aName;
		std::filesystem::create_directory(path);
		expect(adapters::sqlite::make_storage_root_owner_private(path), "private root");
		return path;
	}
	void combinations(const std::filesystem::path& aParent)
	{
		for (int detail = 0; detail < 2; ++detail) {
			for (int privacy = 0; privacy < 2; ++privacy) {
				const auto                          path = root(aParent, std::to_string(detail) + std::to_string(privacy));
				application::diagnostic_preferences preferences{ true, static_cast<application::diagnostic_detail>(detail), static_cast<application::diagnostic_privacy>(privacy) };
				adapters::logging::quill_log        log{ path, { }, preferences };
				expect(log.record({ .Request = 918273, .WindowSession = 642, .Detail = application::diagnostic_detail::verbose }) == application::diagnostic_log_status::available, "intermediate accepted");
				expect(log.capture_state().RecentEvents == 1, "sanitized intermediate retained in disclosed ring");
				expect(log.record({ .Outcome = application::diagnostic_outcome::failed, .Request = 918273, .WindowSession = 642, .Detail = application::diagnostic_detail::verbose }) == application::diagnostic_log_status::available, "error remains recorded at any detail");
				const auto credential = std::as_bytes(std::span{ tests::capability_error_canary_credential });
				for (const auto& canary : tests::capability_error_content_canaries) {
					const auto summary = adapters::github::detail::sanitize_capability_error(canary.Body, credential);
					static_cast<void>(log.record({ .Operation = application::diagnostic_operation::capability, .Summary = summary }));
					static_cast<void>(log.record({ .Operation = application::diagnostic_operation::capability, .Summary = summary, .Detail = application::diagnostic_detail::verbose }));
				}
				const auto recent = log.recent_context();
				log.close();
				const auto bytes   = tests::read_diagnostic_logs(path);
				auto       records = tests::diagnostic_json_records(bytes);
				expect(records.size() == (detail ? 2U : 1U) + tests::capability_error_content_canaries.size() * (detail ? 2U : 1U), "Standard and Verbose have distinct bounded detail");
				expect((bytes.find("\"request\":918273") != std::string::npos) == (privacy == 1), "raw identifying request retained only by independent privacy choice");
				expect(bytes.find("642") != std::string::npos || privacy == 0, "local window identity is available when selected");
				for (const auto& record : records) {
					if (record.value("request_label") != "none") {
						expect(record.value("request_label").toString().startsWith("request-"), "typed opaque join label");
					}
				}
				if (detail) {
					expect(records[0].value("request_label") == records[1].value("request_label"), "stable within-session request joins");
				}
				for (const auto& canary : tests::capability_error_content_canaries) {
					expect(bytes.find(canary.Marker) == std::string::npos, "forbidden material absent from file in every mode");
					for (const auto& item : recent) {
						expect(item.find(canary.Marker) == std::string::npos, "forbidden material absent from ring in every mode");
					}
				}
			}
		}
	}
	struct settings final : application::diagnostic_settings_port
	{
		application::diagnostic_preferences Saved;
		bool                                Refuse = false;
		bool                                Opened = false;
		auto                                load() -> application::diagnostic_preference_load override { return { Saved, true, false }; }
		auto                                store(application::diagnostic_preferences aValue) -> bool override
		{
			if (Refuse) {
				return false;
			}
			Saved = aValue;
			return true;
		}
		auto open_folder() -> bool override
		{
			Opened = true;
			return false;
		}
	};
	void controls(const std::filesystem::path& aParent)
	{
		const auto                   path = root(aParent, "barriers");
		adapters::logging::quill_log log{ path };
		for (unsigned i = 0; i < 900; ++i) {
			static_cast<void>(log.record({ .Request = i + 1U, .Detail = application::diagnostic_detail::verbose }));
		}
		expect(log.capture_state().RecentEvents == 512 && log.capture_state().RecentBytes <= 2U * 1024U * 1024U, "ring bounded under overload");
		const auto revokedGeneration = log.context_generation();
		log.discard_context();
		expect(log.record({ .Detail = application::diagnostic_detail::verbose, .ContextGeneration = revokedGeneration }) == application::diagnostic_log_status::dropped,
			   "late intermediate producer cannot repopulate signed-out context");
		expect(log.recent_context().empty(), "sign-out immediately revokes recent context");
		application::manual_executor                effects, decisions;
		settings                                    store;
		application::diagnostic_settings_host       host{ log, store, effects, decisions, store.load() };
		application::diagnostic_settings_projection shown;
		host.observe([&](const auto& aValue) { shown = aValue; });
		host.apply({ false, application::diagnostic_detail::verbose, application::diagnostic_privacy::include_locally });
		expect(shown.Busy && shown.Capture.Preferences.Enabled, "pending UI shows old accepted preferences");
		effects.drain();
		expect(shown.Busy, "effect completion cannot mutate projection off lane");
		decisions.drain();
		expect(!shown.Busy && !shown.Capture.Preferences.Enabled && !store.Saved.Enabled && log.recent_context().empty(), "Off acknowledged only after persistent barrier");
		expect(log.record({ }) == application::diagnostic_log_status::closed, "Off admits no record or context");
		store.Refuse = true;
		host.apply({ });
		effects.drain();
		decisions.drain();
		expect(!shown.Capture.Preferences.Enabled && shown.Result == application::diagnostic_control_result::refused, "failed persistence preserves accepted Off");
		store.Refuse = false;
		host.apply({ });
		effects.drain();
		decisions.drain();
		expect(shown.Capture.Preferences.Enabled, "explicit re-enable acknowledged");
		host.open_folder();
		effects.drain();
		decisions.drain();
		expect(store.Opened && shown.Result == application::diagnostic_control_result::refused, "folder failure is projected");
		{
			adapters::logging::quill_log concurrent{ path };
			expect(log.clear() == application::diagnostic_clear_result::busy, "Clear refuses another active session");
		}
		std::atomic<bool> stop{ false };
		std::thread       producer{ [&] { while (!stop.load()) { static_cast<void>(log.record({.Request=12345,.Detail=application::diagnostic_detail::verbose})); } } };
		host.apply({ false });
		effects.drain();
		decisions.drain();
		expect(!shown.Capture.Preferences.Enabled, "Off barrier completes during producer overload");
		stop = true;
		producer.join();
		host.clear();
		effects.drain();
		decisions.drain();
		expect(shown.Result == application::diagnostic_control_result::cleared && log.recent_context().empty() && tests::read_diagnostic_logs(path).empty(), "Clear acknowledges removed files and ring while Off");
		host.refresh();
		host.shutdown();
		expect(!host.quiescent(), "shutdown retains accepted outstanding effect");
		effects.drain();
		decisions.drain();
		expect(host.quiescent(), "shutdown drains exact outstanding effect");
	}
}
auto main(int aArgc, char**) -> int
try {
	QTemporaryDir temporary;
	expect(temporary.isValid(), "scratch root");
	const auto parent = std::filesystem::path{ temporary.path().toStdString() };
	if (aArgc > 1) {
		const auto                   path = root(parent, "age");
		adapters::logging::quill_log log{ path };
		static_cast<void>(log.record({ .Detail = application::diagnostic_detail::verbose }));
		expect(log.capture_state().RecentEvents == 1, "recent context initially present");
		std::this_thread::sleep_for(std::chrono::milliseconds{ 180100 });
		expect(log.recent_context().empty() && log.capture_state().RecentBytes == 0, "real three-minute age expires context without new records");
		return 0;
	}
	combinations(parent);
	controls(parent);
	const auto a = root(parent, "profile-a"), b = root(parent, "profile-b");
	expect(adapters::sqlite::load_diagnostic_preferences(a).Preferences == application::diagnostic_preferences{ }, "fresh defaults Standard Anonymized On");
	const application::diagnostic_preferences chosen{ false, application::diagnostic_detail::verbose, application::diagnostic_privacy::include_locally };
	expect(adapters::sqlite::store_diagnostic_preferences(a, chosen), "persist exact choices");
	expect(adapters::sqlite::load_diagnostic_preferences(a).Preferences == chosen, "reopen preserves Off and independent choices");
	expect(adapters::sqlite::load_diagnostic_preferences(b).Preferences == application::diagnostic_preferences{ }, "profile preferences isolated");
	expect(!adapters::sqlite::store_diagnostic_preferences(a, { true, static_cast<application::diagnostic_detail>(7) }), "invalid choices refused before persistence");
	return 0;
}

catch (...) {
	return tests::failing_exit();
}
