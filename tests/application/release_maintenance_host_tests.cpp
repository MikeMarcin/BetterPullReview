#include "bpr/application/manual_executor.hpp"
#include "bpr/application/release_diagnostic_snapshot.hpp"
#include "bpr/application/release_maintenance_application_host.hpp"
#include "bpr/application/release_profile_cleanup.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
	using namespace bpr;
	using command = core::release_maintenance_command;
	using reason  = core::release_maintenance_reason;
	using phase   = core::release_maintenance_phase;
	void expect(bool aValue, std::string_view aMessage)
	{
		if (!aValue) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	auto log_line() -> std::string
	{
		return "2026-09-20T00:00:00Z v=1 op=github.pull_request outcome=failed request=987 http_status=403 summary=sanitized dropped=2 detail=SOURCE_CANARY TOKEN_CANARY COMMENT_CANARY failure=PRIVATE_CANARY\n";
	}
	class preparation final : public application::release_maintenance_preparation
	{
	public:
		application::release_diagnostic_document Document  = *application::prepare_release_diagnostics(log_line(), core::release_maintenance_revision{ 1U });
		core::release_maintenance_selection      Selection = Document.Descriptor;
		auto                                     selection() const -> core::release_maintenance_selection override { return Selection; }
		auto                                     preview() const noexcept -> std::string_view override { return Document.Bytes; }
	};
	class storage final : public application::release_maintenance_storage_port
	{
	public:
		std::shared_ptr<preparation> Candidate  = std::make_shared<preparation>();
		int                          Prepares   = 0;
		int                          Erases     = 0;
		bool                         Fail       = false;
		bool                         Changed    = false;
		bool                         NotPrivate = false;
		std::function<void()>        During;
		auto                         authority() const noexcept -> core::release_maintenance_authority override { return { core::bpr_profile_id{ 1U }, core::application_session_id{ 2U }, core::release_maintenance_root_id{ 3U } }; }
		auto                         capabilities() const noexcept -> core::release_maintenance_capabilities override { return { true, true, true, true }; }
		auto                         revalidate(const core::release_maintenance_context&, const application::release_maintenance_preparation&) -> application::release_maintenance_result override
		{
			if (NotPrivate) {
				return std::unexpected(reason::storage_not_private);
			}
			return Changed ? application::release_maintenance_result{ std::unexpected(reason::invalid_authority) } : application::release_maintenance_result{ };
		}
		auto prepare(const core::release_maintenance_context& aContext, std::stop_token) -> std::expected<std::shared_ptr<const application::release_maintenance_preparation>, reason> override
		{
			++Prepares;
			if (During) {
				During();
			}
			if (Fail) {
				throw std::runtime_error{ "private canary exception" };
			}
			if (aContext.Operation != command::preview_diagnostics) {
				core::release_maintenance_inventory inventory{ core::release_maintenance_revision{ 1U }, { }, aContext.Operation == command::prepare_purge ? core::release_profile_purge_categories : 1U, 1U, 0U, true };
				inventory.Digest[0]  = 1U;
				Candidate->Selection = inventory;
			}
			return Candidate;
		}
		auto erase(const core::release_maintenance_context&, const application::release_maintenance_preparation&, std::stop_token,
				   const std::function<bool()>& aCommit) -> application::release_maintenance_result override
		{
			if (!aCommit()) {
				return std::unexpected(reason::cancelled);
			}
			++Erases;
			if (Fail) {
				return std::unexpected(reason::io_failure);
			}
			return { };
		}
	};
	class lifecycle final : public application::release_maintenance_lifecycle_port
	{
	public:
		std::function<void(application::release_maintenance_result)> Complete;
		void                                                         quiesce(const core::release_maintenance_context&, std::stop_token, std::function<void(application::release_maintenance_result)> aComplete) override { Complete = std::move(aComplete); }
	};
	class projection final : public application::release_maintenance_projection_port
	{
	public:
		application::release_maintenance_projection Last;
		std::function<void()>                       Feedback;
		bool                                        Throw = false;
		void                                        publish(const application::release_maintenance_projection& aProjection) override
		{
			Last = aProjection;
			if (Feedback) {
				auto feedback = std::move(Feedback);
				feedback();
			}
			if (Throw) {
				throw std::runtime_error{ "observer failure" };
			}
		}
	};
	class output final : public application::release_diagnostic_output
	{
	public:
		std::string           Bytes;
		std::function<void()> During;
		auto                  write(std::string_view aBytes, std::stop_token, const std::function<bool()>& aCommit) -> application::release_maintenance_result override
		{
			if (During) {
				During();
			}
			if (!aCommit()) {
				return std::unexpected(reason::cancelled);
			}
			Bytes = aBytes;
			return { };
		}
	};
	struct fixture
	{
		storage                                           Storage;
		lifecycle                                         Lifecycle;
		application::manual_executor                      Effects;
		application::manual_executor                      Decisions;
		projection                                        Projection;
		application::release_maintenance_application_host Host{ Storage, Lifecycle, Effects, Decisions, Projection };
		void                                              intent(command aCommand, std::shared_ptr<output> aOutput = { })
		{
			Host.dispatch({ Storage.authority(), aCommand, aCommand == command::cancel ? Host.state().Pending : Host.state().Prepared }, std::move(aOutput));
		}
		void drain()
		{
			Effects.drain();
			Decisions.drain();
		}
		void preview()
		{
			intent(command::preview_diagnostics);
			drain();
			expect(Host.state().Prepared.has_value(), "preview prepared");
		}
		~fixture()
		{
			Host.shutdown();
			if (Lifecycle.Complete && Effects.pending() == 0U) {
				Lifecycle.Complete(std::unexpected(reason::cancelled));
			}
			drain();
		}
	};
	void test_parser()
	{
		auto document = application::prepare_release_diagnostics(log_line() + "legacy SECRET_CANARY\n" + "incomplete", core::release_maintenance_revision{ 9U });
		expect(document && document->Descriptor.Records == 1U && document->Descriptor.OmittedRecords == 2U, "known prefix and truthful omissions");
		expect(document->Bytes == "bpr-diagnostics-v1\noperation=3 outcome=2 http_status=403 dropped=2\n", "only fixed metadata survives");
		expect(document->Bytes.find("CANARY") == std::string::npos && document->Bytes.find("987") == std::string::npos, "no body or request identity");
		auto bad = log_line();
		bad.replace(bad.find("403"), 3U, "999");
		auto rejected = application::prepare_release_diagnostics(bad, core::release_maintenance_revision{ 1U });
		expect(rejected && rejected->Descriptor.Records == 0U && rejected->Descriptor.OmittedRecords == 1U, "invalid typed fields omitted");
		std::string many;
		for (unsigned index = 0U; index < 1027U; ++index) {
			many += log_line();
		}
		auto bounded = application::prepare_release_diagnostics(many, core::release_maintenance_revision{ 1U });
		expect(bounded && bounded->Descriptor.Records == 1024U && bounded->Descriptor.OmittedRecords == 3U, "record bound counts omissions");
		expect(!application::prepare_release_diagnostics(std::string(application::release_diagnostic_input_limit + 1U, 'x'), core::release_maintenance_revision{ 1U }), "input byte cap");
		expect(!application::prepare_release_diagnostics(std::string(application::release_diagnostic_input_records + 1U, '\n'), core::release_maintenance_revision{ 1U }), "input work cap");
	}
	constexpr std::string_view session = "fedcba9876543210fedcba9876543210";
	auto                       structured_line(unsigned aSequence, bool aIdentifying = false) -> std::string
	{
		return std::string{ R"({"v":3,"event":"github.capability.failed","category":"github","severity":"error","utc_ms":1780000000000,"elapsed_us":5,"session":")" }
		+ std::string{ session } + R"(","sequence":)" + std::to_string(aSequence)
			+ R"(,"build":"0.1.0-0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef-Debug","op":"github.capability","outcome":"failed","request":)"
			+ (aIdentifying ? "991" : "0") + R"(,"http_status":403,"summary":"sanitized","dropped":2,"detail":"message=\"Field fields exceeds a service repetition limit [omitted:unclassified-content]\" code=\"[omitted:unclassified-code]\" ","epoch_stage":"none","failure_domain":"none","failure":"none","pack_failure":"none","page_operation":"none","window_session":)"
			+ (aIdentifying ? "776" : "0") + R"(,"relationship_detail":"none","transport_source":"none","git_step":"none","request_label":"request-)"
			+ std::string{ session } + R"(-1","window_label":"window-)" + std::string{ session }
		+ R"(-2","capture_privacy":")" + (aIdentifying ? "include_locally" : "anonymized") + R"(","capture_detail":"standard","event_detail":"standard"})" + "\n";
	}
	void test_actor_conflict_export()
	{
		for (const auto policy : { core::diagnostic_export_policy::anonymized, core::diagnostic_export_policy::identifying }) {
			for (const std::string_view detail : { "actor_conflict=viewer,review,ascii_case_only", "actor_conflict=pull_request,issue_comment,different",
												   "actor_conflict=PRIVATE_LOGIN,review,different", "actor_conflict=viewer,U_PRIVATE_NODE,different", "actor_conflict=viewer,review,different,PRIVATE_LOGIN",
												   "actor_conflict=viewer,review,unknown", "actor_conflict=viewer,review", "actor_conflict=viewer,,different", "actor_conflict=viewer,review,ascii_case_only " }) {
				auto       line  = structured_line(1U, true);
				const auto start = line.find("\"detail\":\"") + std::string_view{ "\"detail\":\"" }.size();
				const auto end   = line.find("\",\"epoch_stage\"", start);
				expect(end != std::string::npos, "structured diagnostic detail is bounded");
				line.replace(start, end - start, detail);
				const application::diagnostic_export_capture capture{ std::string{ session }, 1U, { } };
				const auto                                   document = application::prepare_diagnostic_export(line, core::release_maintenance_revision{ 1U }, { policy }, capture, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
				const bool                                   allowed  = detail == "actor_conflict=viewer,review,ascii_case_only" || detail == "actor_conflict=pull_request,issue_comment,different";
				expect(document && document->Descriptor.Records == 1U && (document->Bytes.find(detail) != std::string::npos) == allowed,
					   "both export policies retain only the closed actor-origin and difference grammar");
				expect(document->Bytes.find("PRIVATE_LOGIN") == std::string::npos && document->Bytes.find("U_PRIVATE_NODE") == std::string::npos,
					   "actor provenance export never admits login or node canaries");
			}
		}
	}

	void test_git_capability_step_export()
	{
		for (const auto policy : { core::diagnostic_export_policy::anonymized, core::diagnostic_export_policy::identifying }) {
			for (const std::string_view step : { "git_capability_probe", "cli_capability_probe", "git_capability_probe_PRIVATE_CANARY", "cli_capability_probe_PRIVATE_CANARY" }) {
				auto       line     = structured_line(1U, true);
				const auto position = line.find("\"git_step\":\"none\"");
				expect(position != std::string::npos, "closed Git step fixture field exists");
				line.replace(position, std::string_view{ "\"git_step\":\"none\"" }.size(), "\"git_step\":\"" + std::string{ step } + "\"");
				const application::diagnostic_export_capture capture{ std::string{ session }, 1U, { } };
				const auto                                   document = application::prepare_diagnostic_export(line, core::release_maintenance_revision{ 1U }, { policy }, capture, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
				expect(document && document->Descriptor.Records == (step.contains("PRIVATE_CANARY") ? 0U : 1U)
						   && document->Bytes.find("PRIVATE_CANARY") == std::string::npos,
					   "both export policies admit only the closed Git and GitHub CLI capability-probe stage names");
			}
		}
	}

	void test_git_tool_failure_export()
	{
		for (const auto policy : { core::diagnostic_export_policy::anonymized, core::diagnostic_export_policy::identifying }) {
			for (const std::string_view failure : { "git_tool_unsupported", "git_tool_probe_failed", "gh_tool_unsupported", "gh_tool_probe_failed", "git_tool_PRIVATE_CANARY", "gh_tool_PRIVATE_CANARY" }) {
				auto       line     = structured_line(1U, true);
				const auto position = line.find("\"failure\":\"none\"");
				expect(position != std::string::npos, "closed Git failure fixture field exists");
				line.replace(position, std::string_view{ "\"failure\":\"none\"" }.size(), "\"failure\":\"" + std::string{ failure } + "\"");
				const application::diagnostic_export_capture capture{ std::string{ session }, 1U, { } };
				const auto                                   document = application::prepare_diagnostic_export(line, core::release_maintenance_revision{ 1U }, { policy }, capture, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
				expect(document && document->Descriptor.Records == (failure.contains("PRIVATE_CANARY") ? 0U : 1U)
						   && document->Bytes.find("PRIVATE_CANARY") == std::string::npos,
					   "both export policies admit only the four closed actionable Git and GitHub CLI failure names");
			}
		}
	}

	void test_rich_export()
	{
		using policy = core::diagnostic_export_policy;
		using scope  = core::diagnostic_export_scope;
		application::diagnostic_export_capture capture{ std::string{ session }, 3U, { structured_line(2), structured_line(3) } };
		for (auto& line : capture.Recent) {
			line.pop_back();
		}
		const auto original = structured_line(1) + structured_line(2, true) + structured_line(4) + log_line() + "malformed CANARY\n";
		const auto prepare  = [&](policy aPolicy, bool aRecent, scope aScope, std::string_view aNamespace) {
			return application::prepare_diagnostic_export(original, core::release_maintenance_revision{ 2U }, { aPolicy, aScope, aRecent }, capture, aNamespace);
		};
		const auto document = prepare(policy::anonymized, true, scope::retained_sessions, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
		expect(document && document->Descriptor.Records == 4U && document->Descriptor.RecentRecords == 1U && document->Descriptor.OmittedRecords == 1U, "frozen files plus explicit unique ring, cutoff and legacy metadata");
		expect(document->Descriptor.CaptureRedactedRecords == 2U && document->Descriptor.Sessions == 1U && document->Descriptor.Categories == 2U, "capture omissions and scope summary are factual");
		expect(document->Bytes.find("Field fields exceeds a service repetition limit") != std::string::npos && document->Bytes.find("\"detail_omissions\":0") != std::string::npos, "unknown code keeps useful reclassified explanation");
		expect(document->Bytes.find(session) == std::string::npos && document->Bytes.find("991") == std::string::npos && document->Bytes.find("776") == std::string::npos && document->Bytes.find("CANARY") == std::string::npos, "anonymized output removes original identities and forbidden content");
		const std::string requestLabel = "request-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa-2";
		const auto        first        = document->Bytes.find(requestLabel);
		expect(first != std::string::npos && document->Bytes.find(requestLabel, first + requestLabel.size()) != std::string::npos, "fresh labels preserve event joins");
		const auto next = prepare(policy::anonymized, false, scope::retained_sessions, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
		expect(next && next->Descriptor.Records == 3U && next->Descriptor.RecentRecords == 0U && next->Bytes.find(requestLabel) == std::string::npos, "ring opt-out and fresh export mapping");
		const auto identifying = prepare(policy::identifying, true, scope::current_session, "cccccccccccccccccccccccccccccccc");
		expect(identifying && identifying->Descriptor.Records == 3U && identifying->Bytes.find("\"request\":991") != std::string::npos && identifying->Bytes.find("\"request\":0") != std::string::npos, "identifying export cannot recover capture-time redaction and current scope excludes legacy");
		expect(original.find("CANARY") != std::string::npos, "original log preserved");
		auto older = structured_line(1, true);
		older.replace(older.find("\"v\":3"), 5U, "\"v\":2");
		older.erase(older.find(",\"request_label\""));
		older += "}\n";
		older.replace(older.find(session), session.size(), "1234567890abcdef1234567890abcdef");
		older.replace(older.find("1780000000000"), 13U, "0");
		const auto mixed = application::prepare_diagnostic_export(older + original, core::release_maintenance_revision{ 2U }, { policy::anonymized }, capture, "dddddddddddddddddddddddddddddddd");
		expect(mixed && mixed->Descriptor.Sessions == 2U && mixed->Descriptor.FirstUtcMs == 0U && mixed->Descriptor.LastUtcMs == 1780000000000ULL, "v2 and v3 sessions retain exact time range including epoch zero");
		expect(mixed->Bytes.find("legacy_identifying") != std::string::npos && mixed->Bytes.find("1234567890abcdef1234567890abcdef") == std::string::npos, "v2 identities are freshly anonymized");
		const auto legacy = application::prepare_diagnostic_export(log_line(), core::release_maintenance_revision{ 2U }, { policy::anonymized }, capture, "dddddddddddddddddddddddddddddddd");
		expect(legacy && legacy->Descriptor.Categories == 2U && legacy->Descriptor.Sessions == 0U, "legacy metadata reports its known category without inventing a session");
		std::string many;
		for (unsigned sequence = 1U; sequence <= 1025U; ++sequence) {
			many += structured_line(sequence);
		}
		auto unlimitedCapture     = capture;
		unlimitedCapture.Sequence = 1025U;
		const auto bounded        = application::prepare_diagnostic_export(many, core::release_maintenance_revision{ 2U }, { policy::anonymized }, unlimitedCapture, "dddddddddddddddddddddddddddddddd");
		expect(bounded && bounded->Descriptor.Records + bounded->Descriptor.OmittedRecords == 1025U && bounded->Descriptor.OmittedRecords > 0U && bounded->Bytes.size() <= core::release_diagnostic_byte_limit, "rich export byte bound counts every omitted record");

		for (const std::string_view field : { "event", "category", "severity", "session", "build", "op", "outcome", "summary", "detail", "epoch_stage", "failure_domain", "failure", "pack_failure", "page_operation", "relationship_detail", "transport_source", "git_step", "request_label", "window_label", "capture_privacy", "capture_detail", "event_detail" }) {
			auto       poisoned = structured_line(1);
			const auto begin    = poisoned.find(std::string{ "\"" } + std::string{ field } + "\":\"") + field.size() + 4U;
			// Prefixing every source string with forbidden content must never copy it through.
			poisoned.insert(begin, "SOURCE_COMMENT_SECRET_CANARY");
			const auto refused = application::prepare_diagnostic_export(poisoned, core::release_maintenance_revision{ 1U }, { policy::identifying }, capture, "dddddddddddddddddddddddddddddddd");
			expect(refused && refused->Bytes.find("CANARY") == std::string::npos, "every string field is reprocessed under identifying export too");
			expect(field == "detail" ? refused->Descriptor.Records == 1U : refused->Descriptor.Records == 0U, "unclassifiable detail has an omission reason; invalid operational fields omit whole record");
		}
		auto malformed = structured_line(1);
		malformed.pop_back();
		const auto tail = application::prepare_diagnostic_export(malformed, core::release_maintenance_revision{ 1U }, { policy::anonymized }, capture, "dddddddddddddddddddddddddddddddd");
		expect(tail && tail->Descriptor.Records == 0U && tail->Descriptor.OmittedRecords == 1U, "partial tail is never exported");
	}

	void test_preview_export()
	{
		fixture test;
		test.preview();
		const auto bytes       = *test.Projection.Last.Preview;
		auto       destination = std::make_shared<output>();
		test.intent(command::export_diagnostics, destination);
		expect(test.Effects.pending() == 1U && destination->Bytes.empty(), "export only on effect lane");
		test.Effects.drain();
		expect(test.Decisions.pending() == 2U && !test.Host.is_quiescent(), "commit and completion owed");
		test.Decisions.run(1U);
		expect(test.Host.state().Phase == phase::completed && !test.Host.is_quiescent(), "reordered terminal still binds real commit, queued task retained");
		test.Decisions.drain();
		expect(test.Host.is_quiescent() && destination->Bytes == bytes, "preview and exported bytes identical");
		test.intent(command::export_diagnostics, destination);
		expect(test.Effects.pending() == 0U, "consumed preview cannot export twice");
	}
	void test_cancel_invalidate_and_shutdown()
	{
		fixture test;
		test.intent(command::preview_diagnostics);
		const auto old = test.Host.state().Pending;
		test.Host.invalidate();
		test.intent(command::preview_diagnostics);
		expect(test.Storage.Prepares == 0 && test.Effects.pending() == 1U, "retired physical job blocks replacement overlap");
		test.drain();
		expect(!test.Host.state().Prepared && test.Host.is_quiescent(), "stale preparation cannot publish");
		test.preview();
		test.Host.dispatch({ test.Storage.authority(), command::cancel, old });
		expect(test.Host.state().Prepared.has_value(), "stale cancellation cannot dismiss new preview");
		auto destination    = std::make_shared<output>();
		destination->During = [&test] { test.Host.invalidate(); };
		test.intent(command::export_diagnostics, destination);
		test.drain();
		expect(destination->Bytes.empty(), "invalidation before commit prevents output");
		test.preview();
		test.intent(command::export_diagnostics, destination = std::make_shared<output>());
		test.Host.shutdown();
		test.drain();
		expect(test.Host.state().Stopped && destination->Bytes.empty() && test.Host.is_quiescent(), "shutdown drains without output");
	}
	void test_lifecycle_failure_and_reentrancy()
	{
		fixture test;
		test.Projection.Throw = true;
		test.intent(command::prepare_purge);
		expect(test.Storage.Prepares == 0 && test.Effects.pending() == 0U && !test.Host.is_quiescent(), "inventory waits for producer/store barrier");
		test.Lifecycle.Complete({ });
		test.Lifecycle.Complete({ });
		test.Decisions.drain();
		test.drain();
		expect(test.Storage.Prepares == 1 && test.Host.state().Prepared.has_value(), "duplicate lifecycle terminal ignored");
		test.Storage.Fail = true;
		test.intent(command::purge_profile);
		test.drain();
		expect(test.Storage.Erases == 1 && test.Host.state().RecoveryRequired, "partial irreversible failure requires recovery");
		test.intent(command::prepare_purge);
		expect(test.Effects.pending() == 0U, "recovery blocks readmission");
		fixture reentrant;
		reentrant.Projection.Feedback = [&reentrant] { reentrant.Host.shutdown(); };
		reentrant.intent(command::preview_diagnostics);
		reentrant.drain();
		expect(reentrant.Host.state().Stopped && reentrant.Host.is_quiescent(), "reentrant shutdown survives projection delivery");
		fixture malformed;
		malformed.Storage.Candidate->Document.Bytes.append("bad");
		malformed.intent(command::preview_diagnostics);
		malformed.drain();
		expect(malformed.Host.state().Reason == reason::privacy_refusal && !malformed.Projection.Last.Preview, "incorrect byte digest refuses publication");
	}
	class rejecting_executor final : public application::effect_executor
	{
		void post(std::function<void()>) override { throw std::runtime_error{ "off-lane executor stopped" }; }
	};
	void test_refused_effects_and_retired_barrier()
	{
		fixture changed;
		changed.preview();
		changed.Storage.Changed = true;
		auto destination        = std::make_shared<output>();
		changed.intent(command::export_diagnostics, destination);
		changed.drain();
		expect(destination->Bytes.empty() && !changed.Host.state().Pending && changed.Host.state().Reason == reason::selection_changed, "native authority rechecked before frozen export with a consumed failure terminal");
		// [[BPR-1038]]. A native refusal for privacy is its own operational
		// failure, not a changed selection.
		fixture notPrivate;
		notPrivate.preview();
		notPrivate.Storage.NotPrivate = true;
		auto notPrivateDestination    = std::make_shared<output>();
		notPrivate.intent(command::export_diagnostics, notPrivateDestination);
		notPrivate.drain();
		expect(notPrivateDestination->Bytes.empty() && notPrivate.Host.state().Reason == reason::storage_not_private,
			   "a storage privacy refusal keeps its own reason through the host");
		fixture cancelled;
		cancelled.intent(command::prepare_clear);
		cancelled.intent(command::cancel);
		cancelled.Lifecycle.Complete({ });
		cancelled.Decisions.drain();
		cancelled.drain();
		expect(cancelled.Storage.Prepares == 0 && cancelled.Host.is_quiescent() && !cancelled.Host.state().Prepared, "late barrier after cancel owes completion but no inventory");
		fixture stopped;
		stopped.intent(command::prepare_purge);
		stopped.Host.shutdown();
		stopped.Lifecycle.Complete(std::unexpected(reason::cancelled));
		stopped.Decisions.drain();
		expect(stopped.Host.is_quiescent() && stopped.Host.state().Stopped, "shutdown retires and drains lifecycle barrier");
		storage                                           storage;
		lifecycle                                         lifecycle;
		projection                                        projection;
		rejecting_executor                                effects;
		application::manual_executor                      decisions;
		application::release_maintenance_application_host host{ storage, lifecycle, effects, decisions, projection };
		host.dispatch({ storage.authority(), command::preview_diagnostics, { } });
		expect(host.is_quiescent() && host.state().Reason == reason::unavailable, "executor rejection produces one truthful terminal");
	}
}
auto main() -> int
{
	test_parser();
	test_actor_conflict_export();
	test_git_capability_step_export();
	test_git_tool_failure_export();
	test_rich_export();
	test_preview_export();
	test_cancel_invalidate_and_shutdown();
	test_lifecycle_failure_and_reentrancy();
	test_refused_effects_and_retired_barrier();
	std::cout << "release-maintenance-host-tests:pass\n";
}
