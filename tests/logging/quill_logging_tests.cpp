#include "../../src/adapters/logging/src/queued_diagnostic_record.hpp"
#include "../support/capability_error_content_canaries.hpp"
#include "../support/diagnostic_files.hpp"
#include "../support/diagnostic_json.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/profile_runtime_lease.hpp"
#include "bpr/adapters/sqlite/release_maintenance_files.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/application/agent_run_diagnosis.hpp"
#include "bpr/application/git_protocol_v2.hpp"
#include "bpr/application/github_read_model.hpp"
#include "bpr/application/release_diagnostic_snapshot.hpp"
#include "bpr/core/discovery_diagnosis.hpp"
#include "bpr/core/operation_diagnosis.hpp"
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
#include <memory>
#include <print>
#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#if !defined(_WIN32)
	#include <unistd.h>
#endif
// Deliberately throwing test value exercises Quill13's REAL deferred-decode
// catch/placeholder, while the production owning handle remains noexcept.
struct failed_deferred_record
{
	static inline bool                                       FailMove = false;
	bpr::adapters::logging::detail::queued_diagnostic_record Record;
	failed_deferred_record()                                       = default;
	failed_deferred_record(const failed_deferred_record&) noexcept = default;
	failed_deferred_record(failed_deferred_record&& aOther) noexcept(false)
	{
		if (FailMove) {
			throw std::runtime_error{ "Synthetic deferred decode failure" };
		}
		Record = std::move(aOther.Record);
	}
};
template <>
struct fmtquill::formatter<failed_deferred_record> : fmtquill::formatter<bpr::adapters::logging::detail::queued_diagnostic_record>
{
	auto format(const failed_deferred_record& aRecord, format_context& aContext) const
	{
		return fmtquill::formatter<bpr::adapters::logging::detail::queued_diagnostic_record>::format(aRecord.Record, aContext);
	}
};

template <>
struct quill::Codec<failed_deferred_record> : quill::DeferredFormatCodec<failed_deferred_record>
{ };

namespace {
	using bpr::tests::expect;
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
		adapters::logging::quill_log log{ aRoot, { }, { .Privacy = application::diagnostic_privacy::include_locally } };
		const auto                   credential = std::as_bytes(std::span{ tests::capability_error_canary_credential });
		for (const auto& canary : tests::capability_error_content_canaries) {
			const auto summary = adapters::github::detail::sanitize_capability_error(canary.Body, credential);
			expect(log.record({ .Operation = application::diagnostic_operation::capability, .Outcome = application::diagnostic_outcome::response_received, .HttpStatus = 200U, .Summary = summary }) == application::diagnostic_log_status::available, "content audit reaches the real diagnostic sink");
		}
		log.close();
		const auto bytes    = tests::read_diagnostic_logs(aRoot);
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

	void deferred_record_ownership()
	{
		using namespace bpr::adapters::logging::detail;
		const std::string                            transport  = "1|{\"schema\":3}";
		const auto                                   charge     = encode_record_charge(transport.size());
		const auto                                   chargeView = std::string_view{ charge.data(), charge.size() };
		quill::detail::SizeCacheVector               cache;
		std::array<std::byte, 256U>                  queue{ };
		std::weak_ptr<const diagnostic_record_bytes> weak;
		pending_diagnostic_records                   pending;
		expect(pending.reserve(transport.size(), false), "logical transport charge reserved once");
		auto*         output = queue.data();
		std::uint32_t index  = 0U;
		{
			auto record = queued_diagnostic_record{ std::make_shared<const diagnostic_record_bytes>(transport) };
			weak        = record.Bytes;
			expect(record.Bytes->Size == transport.size(), "owned buffer requests exact logical capacity");
			quill::Codec<std::string_view>::encode(output, cache, index, chargeView);
			quill::Codec<queued_diagnostic_record>::encode(output, cache, index, std::move(record));
			expect(weak.use_count() == 1 && !weak.expired(), "committed queue exclusively owns the moved record");
		}
		auto*                        input = queue.data();
		quill::DynamicFormatArgStore store;
		quill::Codec<std::string_view>::decode_and_store_arg(input, &store);
		quill::Codec<queued_diagnostic_record>::decode_and_store_arg(input, &store);
		expect(!weak.expired(), "decoded argument store owns the record after queue destruction");
		const auto statement = fmtquill::vformat("{}{}", fmtquill::basic_format_args<fmtquill::format_context>{ store.data(), store.size() });
		store.clear();
		expect(weak.expired() && pending.Events == 1U && pending.Bytes == transport.size(), "arg-store clear destroys ownership WITHOUT releasing the pending transit message");
		const auto decodedCharge = decode_record_charge(statement);
		expect(decodedCharge && *decodedCharge == transport.size() && statement.substr(charge.size()) == transport, "ordinary first codec and deferred body preserve charge and canonical transport");
		const auto sinkStatement = statement + "\n";
		const auto consumed      = consume_queued_record(pending, sinkStatement);
		expect(consumed.Statement && *consumed.Statement == transport + "\n" && consumed.Released && pending.Events == 0U && pending.Bytes == 0U && !pending.release(*decodedCharge), "sink releases known charge exactly once without underflow");

		output = queue.data();
		index  = 0U;
		expect(pending.reserve(transport.size(), false), "decode failure retains an admitted logical charge");
		{
			failed_deferred_record record;
			record.Record.Bytes = std::make_shared<const diagnostic_record_bytes>(transport);
			weak                = record.Record.Bytes;
			quill::Codec<std::string_view>::encode(output, cache, index, chargeView);
			quill::DeferredFormatCodec<failed_deferred_record>::encode(output, cache, index, record);
		}
		failed_deferred_record::FailMove = true;
		input                            = queue.data();
		quill::Codec<std::string_view>::decode_and_store_arg(input, &store);
		quill::DeferredFormatCodec<failed_deferred_record>::decode_and_store_arg(input, &store);
		failed_deferred_record::FailMove = false;
		expect(weak.expired(), "failed deferred decode destroys its queued ownership exactly once");
		const auto fallback = fmtquill::vformat("{}{}", fmtquill::basic_format_args<fmtquill::format_context>{ store.data(), store.size() });
		store.clear();
		const auto fallbackCharge = decode_record_charge(fallback);
		expect(fallbackCharge && *fallbackCharge == transport.size() && fallback.substr(charge.size()) == "[Quill deferred decode failed]", "actual upstream fallback preserves FIRST ordinary codec charge without custom format error");
		expect(!consume_queued_record(pending, fallback + "\n").Statement && pending.Events == 0U && pending.Bytes == 0U, "actual fallback fails closed and releases original charge rather than placeholder size");
		expect(pending.reserve(transport.size(), false), "fatal whole-format case holds its admitted charge");
		expect(!consume_queued_record(pending, "[Could not format log statement]\n").Statement && pending.Events == 1U && pending.Bytes == transport.size(), "missing charge fails closed WITHOUT resetting live accounting");
		expect(pending.release(transport.size()), "test retires its sole owned value after the fatal-format observation");
		expect(!decode_record_charge("") && !decode_record_charge("[Could not format log statement]") && !decode_record_charge("0000000000000000") && !decode_record_charge("000000000000FFFF") && !decode_record_charge("000000000000ffff"), "missing noncanonical zero and over-bound charges fail closed");
		{
			auto refused = queued_diagnostic_record{ std::make_shared<const diagnostic_record_bytes>(transport) };
			weak         = refused.Bytes;
			expect(pending.reserve(transport.size(), false) && pending.release(transport.size()), "false-enqueue rollback releases the captured logical charge");
		}
		expect(weak.expired() && pending.Events == 0U && pending.Bytes == 0U, "refused uncommitted owning value releases its exact buffer");
		std::size_t normal = 0U;
		while (pending.reserve(1U, false)) {
			++normal;
		}
		expect(normal == 3584U, "normal pending event bound is unchanged");
		std::size_t priority = 0U;
		while (pending.reserve(1U, true)) {
			++priority;
		}
		expect(priority == 512U && pending.Events == 4096U, "priority event reserve and total bound are unchanged");
		while (pending.Events != 0U) {
			expect(pending.release(1U), "bounded pending record drains once");
		}
		std::size_t normalBytes = 0U;
		while (pending.reserve(maximum_queued_record_bytes, false)) {
			normalBytes += maximum_queued_record_bytes;
		}
		expect(normalBytes <= 7U * 1024U * 1024U && normalBytes + maximum_queued_record_bytes > 7U * 1024U * 1024U, "normal pending byte bound is unchanged");
		std::size_t priorityBytes = normalBytes;
		while (pending.reserve(maximum_queued_record_bytes, true)) {
			priorityBytes += maximum_queued_record_bytes;
		}
		expect(priorityBytes <= 8U * 1024U * 1024U && priorityBytes + maximum_queued_record_bytes > 8U * 1024U * 1024U, "priority pending byte bound is unchanged");
		while (pending.Events != 0U) {
			expect(pending.release(maximum_queued_record_bytes), "bounded byte accounting drains once");
		}
		const auto frame = 4U * sizeof(std::uint64_t) + quill::Codec<std::string_view>::compute_encoded_size(cache, chargeView) + quill::Codec<queued_diagnostic_record>::compute_encoded_size(cache, queued_diagnostic_record{ });
		std::print(stderr, "deferred-record:pass frame_bytes={} inline_handle_bytes={} holder_bytes={} framing_bytes={} owned_capacity={} normal_events={} priority_reserve={} normal_bytes={} priority_bytes={} actual_fallback=1\n", frame, sizeof(queued_diagnostic_record), sizeof(diagnostic_record_bytes), charge.size(), transport.size(), normal, priority, normalBytes, priorityBytes);
	}

	struct accounting_frontend_options : quill::FrontendOptions
	{
		static constexpr auto        queue_type             = quill::QueueType::BoundedDropping;
		static constexpr std::size_t initial_queue_capacity = 64U * 1024U;
	};
	class accounting_sink final : public quill::Sink
	{
	public:
		bpr::adapters::logging::detail::pending_diagnostic_records Pending;
		std::atomic<std::uint64_t>                                 Dropped{ 0U };
		std::atomic<bool>                                          Unavailable{ false };
		std::atomic<std::size_t>                                   Callbacks{ 0U };

	protected:
		void write_log(const quill::MacroMetadata*, std::uint64_t, std::string_view, std::string_view, const std::string&, std::string_view, quill::LogLevel, std::string_view, std::string_view, const std::vector<std::pair<std::string, std::string>>*, std::string_view, std::string_view aStatement) override
		{
			++Callbacks;
			if (!bpr::adapters::logging::detail::consume_record_for_sink(Pending, Dropped, aStatement)) {
				Unavailable = true;
			}
		}
		void flush_sink() override { }
		void run_periodic_tasks() override { }
	};
	void sink_failure_retirement_conservation()
	{
		using namespace bpr::adapters::logging::detail;
		using frontend = quill::FrontendImpl<accounting_frontend_options>;
		std::atomic<std::size_t> backendErrors{ 0U };
		quill::BackendOptions    options;
		options.check_backend_singleton_instance = false;
		options.error_notifier                   = [&](const std::string&) { ++backendErrors; };
		quill::Backend::start(options);
		struct backend_guard
		{
			~backend_guard() { quill::Backend::stop(); }
		} guard;
		const std::string transport = "1|{\"schema\":3}";
		const auto        charge    = encode_record_charge(transport.size());
		const auto        field     = std::string_view{ charge.data(), charge.size() };
		for (const bool known : { true, false }) {
			const auto name   = known ? "bpr1988.known-charge" : "bpr1988.unknown-charge";
			auto       sink   = std::static_pointer_cast<accounting_sink>(frontend::create_or_get_sink<accounting_sink>(name));
			auto*      logger = frontend::create_or_get_logger(name, sink, quill::PatternFormatterOptions{ "%(message)" });
			expect(sink->Pending.reserve(transport.size(), false), "real sink probe owns exactly one pending logical record");
			bool submitted = false;
			if (known) {
				static constexpr quill::MacroMetadata metadata{ "", "", "{}{}", nullptr, quill::LogLevel::Info, quill::MacroMetadata::Event::Log };
				failed_deferred_record                record;
				record.Record.Bytes              = std::make_shared<const diagnostic_record_bytes>(transport);
				failed_deferred_record::FailMove = true;
				submitted                        = logger->log_statement<false>(&metadata, field, record);
			} else {
				// The pinned backend catches this REAL whole-format error and
				// replaces the entire message, losing its charge prefix.
				static constexpr quill::MacroMetadata metadata{ "", "", "{:b}", nullptr, quill::LogLevel::Info, quill::MacroMetadata::Event::Log };
				submitted = logger->log_statement<false>(&metadata, std::string_view{ "synthetic" });
			}
			logger->flush_log();
			failed_deferred_record::FailMove = false;
			const auto beforeEvents          = sink->Pending.Events.load();
			const auto beforeBytes           = sink->Pending.Bytes.load();
			const auto beforeDropped         = sink->Dropped.load();
			const auto callbacks             = sink->Callbacks.load();
			const auto unavailable           = sink->Unavailable.load();
			frontend::remove_logger_blocking(logger);
			// Only NOW has the real backend proved no callback can remain.
			const auto retired = retire_queued_records(sink->Pending, sink->Dropped);
			expect(submitted && callbacks == 1U && unavailable, "real Quill sink receives and refuses each exact malformed record");
			expect(known ? beforeEvents == 0U && beforeBytes == 0U && beforeDropped == 1U && !retired : beforeEvents == 1U && beforeBytes == transport.size() && beforeDropped == 0U && retired, "known charge counts once at sink; unknown charge stays held and uncounted until callback retirement");
			expect(sink->Pending.Events == 0U && sink->Pending.Bytes == 0U && sink->Dropped == 1U && !retire_queued_records(sink->Pending, sink->Dropped) && sink->Dropped == 1U, "real sink plus blocking retirement conserves one drop and zero residual without double count");
		}
		expect(backendErrors == 1U, "only actual whole-format failure reaches backend error notifier");
		std::print(stderr, "sink-retirement:pass known_drops=1 unknown_drops=1 residual_events=0 residual_bytes=0 real_backend=1\n");
	}

	void physical_priority_reserve(const std::filesystem::path& aRoot)
	{
		using namespace bpr;
		using status = application::diagnostic_log_status;
		adapters::logging::quill_log log{ aRoot };
		expect(log.status() == status::available, "sole priority-reserve logger pair is open");
		struct restart_guard
		{
			bool Paused = true;
			void restart()
			{
				quill::Backend::start();
				Paused = false;
			}
			~restart_guard() noexcept
			{
				if (Paused) {
					try {
						quill::Backend::start();
					} catch (...) {
						std::terminate();
					}
				}
			}
		} restart;
		quill::Backend::stop();
		std::size_t normal = 0U, priority = 0U;
		bool        normalRefused = false;
		for (std::size_t index = 0U; index < 4096U; ++index) {
			const auto result = log.record({ .Request = 10000U + index });
			if (result == status::available) {
				++normal;
			} else {
				normalRefused = result == status::dropped;
				break;
			}
		}
		for (std::size_t index = 0U; index < 512U; ++index) {
			if (log.record({ .Outcome = application::diagnostic_outcome::failed, .Request = 20000U + index }) == status::available) {
				++priority;
			}
		}
		restart.restart();
		log.close();
		const auto records           = tests::diagnostic_json_records(tests::read_diagnostic_logs(aRoot));
		const auto priorityPersisted = std::ranges::count_if(records, [](const auto& aRecord) { return aRecord.value("severity").toString() == "error"; });
		expect(normalRefused && normal >= 128U && priority == 512U && records.size() == normal + priority && priorityPersisted == 512, "paused physical queue preserves all512 priority slots after factual normal refusal");
		expect(log.status() == status::available && log.capture_state().Dropped == 1U, "priority reserve needs no consumer progress or retry and close has no residual charge");
		std::print(stderr, "physical-priority-reserve:pass normal={} priority={} persisted={} frame_bytes={} reserved_bytes={} queue_bytes=65536\n", normal, priority, records.size(), adapters::logging::detail::queued_record_frame_bytes(), 512U * adapters::logging::detail::queued_record_frame_bytes());
	}

	void below_budget_tour_burst(const std::filesystem::path& aRoot)
	{
		using namespace bpr;
		constexpr std::size_t                         burstCount = 128U;
		std::array<application::diagnostic_event, 4U> events;
		std::size_t                                   prepared = 0U;
		for (const auto stage : { core::tour_publication_stage::update_issue_comment, core::tour_publication_stage::update_description }) {
			for (const auto started : { true, false }) {
				auto diagnosis    = core::operation_diagnosis{ };
				diagnosis.Action  = core::operation_action::tour_publish;
				diagnosis.Written = !started;
				auto trace        = core::tour_publication_trace{ };
				trace.Stage       = stage;
				trace.Attempt.assign(64U, 'a');
				trace.HttpStatus = started ? 0U : 200U;
				diagnosis.Tour   = std::move(trace);
				const auto text  = core::describe(diagnosis);
				expect(!text.empty() && core::parse_operation_diagnosis(text).has_value(), "burst target diagnosis is valid");
				auto& event     = events[prepared++];
				event.Operation = application::diagnostic_operation::tour_operation;
				event.Outcome   = started ? application::diagnostic_outcome::started : application::diagnostic_outcome::succeeded;
				std::ranges::copy(text, event.Summary.Text.begin());
				event.Summary.Size  = static_cast<std::uint16_t>(text.size());
				event.Summary.State = application::diagnostic_summary_state::sanitized;
			}
		}
		adapters::logging::quill_log log{ aRoot };
		expect(log.status() == application::diagnostic_log_status::available, "sole burst logger is open");
		const auto generation = log.context_generation();
		struct restart_guard
		{
			bool Paused = true;
			void restart()
			{
				quill::Backend::start();
				Paused = false;
			}
			~restart_guard() noexcept
			{
				if (Paused) {
					try {
						quill::Backend::start();
					} catch (...) {
						std::terminate();
					}
				}
			}
		} restart;
		quill::Backend::stop();
		std::size_t admitted = 0U, dropped = 0U, other = 0U;
		for (std::size_t index = 0U; index < burstCount; ++index) {
			const auto status = log.record(events[index % events.size()]);
			if (status == application::diagnostic_log_status::available) {
				++admitted;
			} else if (status == application::diagnostic_log_status::dropped) {
				++dropped;
			} else {
				++other;
			}
		}
		restart.restart();
		const auto state = log.capture_state();
		log.close();
		const auto  bytes                = tests::read_diagnostic_logs(aRoot);
		const auto  records              = tests::diagnostic_json_records(bytes);
		std::size_t acceptedEncodedBytes = 0U, minimumFrame = static_cast<std::size_t>(-1), maximumFrame = 0U;
		for (std::string_view remaining{ bytes }; !remaining.empty();) {
			const auto                     end       = remaining.find('\n');
			const auto                     transport = std::to_string(generation) + "|" + std::string{ remaining.substr(0U, end) };
			quill::detail::SizeCacheVector cache;
			// Pinned Quill13 encodes four uint64 header words before its arguments.
			const auto encoded = 4U * sizeof(std::uint64_t) + quill::Codec<std::string_view>::compute_encoded_size(cache, std::string_view{ transport });
			acceptedEncodedBytes += encoded;
			minimumFrame = std::min(minimumFrame, encoded);
			maximumFrame = std::max(maximumFrame, encoded);
			remaining.remove_prefix(end + 1U);
		}
		// Only bounded numeric fields and the four fixed diagnosis forms vary.
		// 128bytes conservatively exceeds their entire possible width variation.
		const auto                     encodedBurstLowerBound = burstCount * (minimumFrame > 128U ? minimumFrame - 128U : 0U);
		const auto                     logicalBurstUpperBound = burstCount * (16U * 1024U - 1U + 21U);
		quill::detail::SizeCacheVector compactCache;
		const auto                     compactCharge = adapters::logging::detail::encode_record_charge(maximumFrame - 32U);
		const auto                     compactFrame  = 4U * sizeof(std::uint64_t) + quill::Codec<std::string_view>::compute_encoded_size(compactCache, std::string_view{ compactCharge.data(), compactCharge.size() }) + quill::Codec<adapters::logging::detail::queued_diagnostic_record>::compute_encoded_size(compactCache, adapters::logging::detail::queued_diagnostic_record{ });
		std::print(stderr, "tour-compact-queue frame_bytes={} burst_queue_bytes={} handle_bytes={} holder_bytes={} bounded_holders=4096\n", compactFrame, burstCount * compactFrame, sizeof(adapters::logging::detail::queued_diagnostic_record), sizeof(adapters::logging::detail::diagnostic_record_bytes));
		std::print(stderr, "tour-queue-pressure burst={} admitted={} dropped={} other={} persisted={} dropped_counter={} accepted_old_frame_bytes_exact={} old_frame_min={} old_frame_max={} burst_old_frame_lower_bound={} logical_burst_upper_bound={}\n", burstCount, admitted, dropped, other, records.size(), state.Dropped, acceptedEncodedBytes, minimumFrame, maximumFrame, encodedBurstLowerBound, logicalBurstUpperBound);
		expect(other == 0U && records.size() == admitted, "burst persistence binds every admitted record");
		expect(encodedBurstLowerBound > 64U * 1024U && logicalBurstUpperBound < 7U * 1024U * 1024U && burstCount < 3584U, "controlled burst exceeds upstream queue bytes while below unchanged global budgets");
		expect(log.status() == application::diagnostic_log_status::available, "paused backend restarts and drains before assertions");
		expect(admitted == burstCount && dropped == 0U && records.size() == burstCount, "below-global-budget burst retains every target lifecycle record");
		for (const auto stage : { core::tour_publication_stage::update_issue_comment, core::tour_publication_stage::update_description }) {
			for (const auto started : { true, false }) {
				std::size_t count = 0U;
				for (const auto& record : records) {
					const auto diagnosis = core::parse_operation_diagnosis(record.value("detail").toString().toStdString());
					if (diagnosis && diagnosis->Tour && diagnosis->Tour->Stage == stage && record.value("outcome").toString() == (started ? "started" : "succeeded")) {
						++count;
					}
				}
				expect(count == burstCount / 4U, "every exact target start and success persists once");
			}
		}
		std::print(stderr, "tour-queue-pressure:pass records=128\n");
	}

	void structured_record_contract(const std::filesystem::path& aRoot)
	{
		using namespace bpr;
		using status = application::diagnostic_log_status;
		application::diagnostic_summary escaped;
		escaped.State = application::diagnostic_summary_state::sanitized;
		escaped.Text.fill('\\');
		escaped.Text.front() = '"';
		escaped.Size         = static_cast<std::uint16_t>(escaped.Text.size());
		std::string capturedBuild;
		{
			adapters::logging::quill_log log{ aRoot, { }, { .Privacy = application::diagnostic_privacy::include_locally } };
			expect(log.record({ .Operation = application::diagnostic_operation::capability, .Outcome = application::diagnostic_outcome::failed, .Request = 123U, .HttpStatus = 403U, .Summary = escaped }) == status::available, "maximum escaped classified field is admitted");
			auto invalid    = escaped;
			invalid.Text[1] = '\n';
			expect(log.record({ .Summary = invalid }) == status::dropped, "line injection is refused before enqueue");
			invalid.Text[1] = static_cast<char>(0xff);
			expect(log.record({ .Summary = invalid }) == status::dropped, "invalid UTF-8 cannot reach JSON transport");
			expect(log.record({ .Outcome = application::diagnostic_outcome::response_received, .HttpStatus = 429U }) == status::available, "HTTP warning admitted");
			expect(log.record({ .Outcome = application::diagnostic_outcome::cancelled }) == status::available, "cancellation is informational");
			const auto capture = log.flush_for_export(false);
			expect(capture && !capture->Build.empty(), "export captures the configured build identity");
			capturedBuild = capture->Build;
		}
		{
			adapters::logging::quill_log log{ aRoot, { }, { .Privacy = application::diagnostic_privacy::include_locally } };
			expect(log.record({ }) == status::available, "next logger session admitted");
		}
		const auto bytes   = tests::read_diagnostic_logs(aRoot);
		const auto records = tests::diagnostic_json_records(bytes);
		expect(records.size() == 4U && records[0].value("detail").toString().toStdString() == escaped.text(), "escaping round trips one full classified field without extra records");
		expect(records[0].value("build").toString().toStdString() == capturedBuild, "records and exports carry the same build identity");
		expect(records[0].value("severity") == "error" && records[1].value("severity") == "warning" && records[2].value("severity") == "info", "error, warning and cancellation severity survive JSON");
		expect(records[1].value("dropped").toInteger() == 2 && records[2].value("sequence").toInteger() == 3, "invalid records are counted without entering session sequence");
		expect(records[0].value("session") != records[3].value("session") && records[3].value("sequence").toInteger() == 1, "a new session cannot alias appended history");
		const auto exported = application::prepare_release_diagnostics(bytes, core::release_maintenance_revision{ 1U });
		expect(exported && exported->Descriptor.Records == 4U && exported->Descriptor.OmittedRecords == 0U && exported->Bytes.find('\\') == std::string::npos, "JSONL export reconstructs metadata without escaped prose");
		const auto       first = bytes.substr(0U, bytes.find('\n') + 1U);
		const std::array corruptions{
			std::pair{ std::string{ "\"v\":3" }, std::string{ "\"v\":99" } },
			std::pair{ std::string{ "\"http_status\":403" }, std::string{ "\"http_status\":999" } },
			std::pair{ std::string{ "\"request\":123" }, std::string{ "\"request\":0123" } },
			std::pair{ std::string{ "\"request\":123" }, std::string{ "\"request\":18446744073709551616" } },
			std::pair{ std::string{ "\"summary\":\"sanitized\"" }, std::string{ "\"summary\":\"SECRET_CANARY\"" } },
			std::pair{ std::string{ "\"v\":3" }, std::string{ "\"v\":3,\"v\":3" } },
			std::pair{ std::string{ "}\n" }, std::string{ ",\"unknown\":\"SECRET_CANARY\"}\n" } },
			std::pair{ std::string{ "}\n" }, std::string{ "} trailing SECRET_CANARY\n" } },
			std::pair{ std::string{ "}\n" }, std::string{ "}" } }
		};
		for (const auto& [from, to] : corruptions) {
			auto       corrupted = first;
			const auto position  = corrupted.find(from);
			expect(position != std::string::npos, "negative fixture changes its intended field");
			corrupted.replace(position, from.size(), to);
			const auto refused = application::prepare_release_diagnostics(corrupted, core::release_maintenance_revision{ 1U });
			expect(refused && refused->Descriptor.Records == 0U && refused->Descriptor.OmittedRecords == 1U && refused->Bytes == "bpr-diagnostics-v1\n", "unknown, duplicate, malformed or partial JSON is omitted without pass-through");
		}
	}
	void hostile_summary_limits(const std::filesystem::path& aRoot)
	{
		using namespace bpr;
		const std::string credential = "synthetic-bound-credential";
		const std::array  bodies{
			std::string(1024U * 1024U + 1U, 'x'),
			std::string{ R"({"errors":[{"message":")" } + std::string(8193U, 'x') + R"("}]})",
			std::string{ R"({"errors":[{"message":")" } + std::string(8193U, ' ') + R"("}]})",
			std::string{ R"({"errors":[{"message":"private content canary"}],"errors":[]})" },
			std::string{ R"({"errors":[{"message":42,"source":"private content canary"}]})" }
		};
		adapters::logging::quill_log log{ aRoot, { }, { .Privacy = application::diagnostic_privacy::include_locally } };
		for (const auto& body : bodies) {
			const auto summary = adapters::github::detail::sanitize_capability_error(body, std::as_bytes(std::span{ credential }));
			expect(summary.text().empty() && (summary.State == application::diagnostic_summary_state::omitted_limit || summary.State == application::diagnostic_summary_state::omitted_invalid), "oversized or malformed summary has only an omission reason");
			expect(log.record({ .Operation = application::diagnostic_operation::capability, .Summary = summary }) == application::diagnostic_log_status::available, "omitted summary reaches the real diagnostic file without raw fallback");
		}
		log.close();
		const auto bytes = tests::read_diagnostic_logs(aRoot);
		expect(bytes.find("canary") == std::string::npos && bytes.find("xxxxxxxx") == std::string::npos, "malformed and oversized source is absent from retained diagnostics");
	}

	/// [[BPR-1717]]. An export taken while a session runs holds that
	/// session's records up to the moment of export, behind older sessions
	/// that fill the export's limits. Before, the exporter read files in name
	/// order and kept the first records, so a running session whose random
	/// identity sorted later was left out entirely.
	void live_session_export(const std::filesystem::path& aRoot)
	{
		using namespace bpr;
		using status = application::diagnostic_log_status;
		{
			adapters::logging::quill_log older{ aRoot };
			for (unsigned index = 1U; index <= 700U; ++index) {
				expect(older.record({ .Operation = application::diagnostic_operation::pull_request, .Outcome = application::diagnostic_outcome::response_received, .Request = index, .HttpStatus = 200U }) == status::available, "an older session's record is admitted");
				if (index % 40U == 0U) {
					expect(older.flush_for_export(false).has_value(), "the older session drains as it goes");
				}
			}
			older.close();
		}
		const auto olderFiles = tests::diagnostic_files(aRoot);
		expect(olderFiles.size() == 1U, "the older session wrote one file");
		// The lowest possible name, an hour old: read first by name, never by age.
		const auto renamed = aRoot / "logs/session-00000000000000000000000000000000-00000001.jsonl";
		std::filesystem::rename(olderFiles.front(), renamed);
		std::filesystem::last_write_time(renamed, std::filesystem::last_write_time(renamed) - std::chrono::hours{ 1 });

		auto live = std::make_shared<adapters::logging::quill_log>(aRoot);
		for (unsigned index = 1U; index <= 30U; ++index) {
			expect(live->record({ .Operation = application::diagnostic_operation::pull_request, .Outcome = application::diagnostic_outcome::response_received, .Request = index, .HttpStatus = 418U }) == status::available, "a live session's record is admitted");
		}
		auto diagnosis             = core::review_submission_unknown_diagnosis{ };
		diagnosis.Reason           = core::review_submission_unknown_reason::review_not_listed;
		diagnosis.PostAnswer       = core::review_submission_post_answer::no_answer;
		diagnosis.CommentsExpected = 2U;
		diagnosis.PagesRead        = 1U;
		auto       event           = application::diagnostic_event{ .Operation = application::diagnostic_operation::review_submission, .Outcome = application::diagnostic_outcome::failed };
		const auto text            = core::describe(diagnosis);
		std::ranges::copy(text, event.Summary.Text.begin());
		event.Summary.Size     = static_cast<std::uint16_t>(text.size());
		event.Summary.State    = application::diagnostic_summary_state::sanitized;
		event.ReviewSubmission = diagnosis;
		expect(live->record(event) == status::available, "the live session's review submission is admitted");
		// [[BPR-1722]]. A later Check status found it refused.
		const auto ending    = core::review_submission_ending_diagnosis{ .Ending = core::review_submission_ending::refused, .Run = core::review_submission_run::check, .PostAnswer = core::review_submission_post_answer::error_status, .PostStatusClass = 4U, .Refusal = core::review_submission_ending_refusal::unplaceable_line, .Comments = 2U };
		auto       ended     = application::diagnostic_event{ .Operation = application::diagnostic_operation::review_submission, .Outcome = application::diagnostic_outcome::failed };
		const auto endedText = core::describe(ending);
		std::ranges::copy(endedText, ended.Summary.Text.begin());
		ended.Summary.Size  = static_cast<std::uint16_t>(endedText.size());
		ended.Summary.State = application::diagnostic_summary_state::sanitized;
		ended.ReviewEnding  = ending;
		expect(live->record(ended) == status::available, "the live session's review submission ending is admitted");
		std::string liveSession;
		for (const auto& path : tests::diagnostic_files(aRoot)) {
			if (path != renamed) {
				liveSession = path.filename().string().substr(8U, 32U);
			}
		}
		expect(liveSession.size() == 32U, "the live session has its own file");

		auto lease = adapters::sqlite::acquire_profile_runtime_lease(aRoot);
		expect(lease.has_value(), "the profile lease is admitted beside the live logger");
		adapters::sqlite::release_maintenance_files files{ aRoot, *lease, core::bpr_profile_id{ 1U }, core::application_session_id{ 1U }, nullptr, live };
		core::release_maintenance_context           context{ files.authority(), core::request_id{ 1U }, core::release_maintenance_generation{ 1U }, core::release_maintenance_command::preview_diagnostics, { } };
		context.ExportOptions.Policy = core::diagnostic_export_policy::anonymized;
		// No close and no explicit flush: preparation drains the live session itself.
		const auto prepared = files.prepare(context, { });
		expect(prepared.has_value(), "a live export prepares");
		const auto  preview     = std::string{ (*prepared)->preview() };
		const auto  descriptor  = std::get<core::release_diagnostic_snapshot>((*prepared)->selection());
		std::size_t liveRecords = 0U;
		for (auto found = preview.find("\"http_status\":418,"); found != std::string::npos; found = preview.find("\"http_status\":418,", found + 1U)) {
			++liveRecords;
		}
		expect(liveRecords == 30U, "every live record is in the export");
		expect(preview.contains("\"op\":\"github.review_submission\"") && preview.contains("\"detail\":\"" + text + "\"")
				   && preview.contains("\"failure_domain\":\"review_submission\",\"failure\":\"review_not_listed\""),
			   "with its review submission's reason and counts");
		expect(preview.contains("\"detail\":\"" + endedText + "\"") && preview.contains("\"failure_domain\":\"review_submission\",\"failure\":\"none\""),
			   "and its ending, as exactly its counts-only line");
		expect(!preview.contains(liveSession), "and no session identity survives anonymization");
		// [[BPR-1722]]. Under the raised limits both sessions fit whole;
		// over them the oldest are left out (application_release_maintenance).
		expect(descriptor.LimitOmittedRecords == 0U && descriptor.OmittedRecords == 0U && descriptor.Sessions == 2U && preview.contains("\"limit_keeps\":\"newest\""), "both sessions fit the export whole");
		expect(preview.contains("\"sequence\":700,") && preview.contains("\"sequence\":1,") && preview.contains("\"sequence\":100,"), "the older session's oldest records are kept too");
		live->close();
	}

	void stress_loss_reporting(const std::filesystem::path& aRoot)
	{
		using namespace bpr;
		using clock                               = std::chrono::steady_clock;
		using status                              = application::diagnostic_log_status;
		constexpr std::uint64_t      maximumBytes = 50U * 1024U * 1024U;
		adapters::logging::quill_log log{ aRoot, { }, { .Privacy = application::diagnostic_privacy::include_locally } };
		std::atomic<std::uint64_t>   dropped{ 0U };
		std::barrier                 ready{ 8 };
		std::vector<std::jthread>    writers;
		writers.reserve(8U);
		const auto started = clock::now();
		for (std::size_t writer = 0U; writer < 8U; ++writer) {
			writers.emplace_back([&] {
				auto event = application::diagnostic_event{ };
				event.Summary.Text.fill('x');
				event.Summary.State = application::diagnostic_summary_state::sanitized;
				event.Summary.Size  = static_cast<std::uint16_t>(event.Summary.Text.size());
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
		const auto bytes   = tests::read_diagnostic_logs(aRoot);
		const auto marker  = bytes.find("\"request\":971,");
		expect(marker != std::string::npos, "final accepted record survives ordinary close");
		const auto    count    = bytes.find("\"dropped\":", marker);
		std::uint64_t reported = 0U;
		expect(count != std::string::npos && std::from_chars(bytes.data() + count + 10U, bytes.data() + bytes.size(), reported).ec == std::errc{ }, "final record carries a numeric loss count");
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
		bpr::adapters::logging::quill_log log{ root, { .FileBytes = 1U } };
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
	deferred_record_ownership();
	sink_failure_retirement_conservation();
	physical_priority_reserve(private_root(root / "physical-priority-reserve"));
	below_budget_tour_burst(private_root(root / "tour-queue-burst"));
	structured_record_contract(private_root(root / "structured"));
	hostile_summary_limits(private_root(root / "limits"));
	stress_loss_reporting(private_root(root / "stress"));
	live_session_export(private_root(root / "live-export"));
	{
		adapters::logging::quill_log log{ root, { }, { .Privacy = application::diagnostic_privacy::include_locally } };
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
		expect(log.record({ .GitStep = static_cast<application::git_smart_transport_step>(255U) }) == status::dropped, "unknown Git acquisition step never enters log");
		expect(log.record({ .TransportFailureSource = static_cast<application::diagnostic_transport_failure_source>(255U) }) == status::dropped, "unknown source never enters log");
		expect(log.record({ .Operation = application::diagnostic_operation::review_comment, .Outcome = application::diagnostic_outcome::response_received, .HttpStatus = 422 }) == status::available, "review comment status survives real logger vocabulary");

		expect(log.record({ .HttpStatus = 600U }) == status::dropped, "invalid HTTP status never enters log");
		{
			// [[BPR-1540]]. A review submission left unknown: its reason and
			// counts, and nothing else, reach the file.
			auto diagnosis             = core::review_submission_unknown_diagnosis{ };
			diagnosis.Reason           = core::review_submission_unknown_reason::comment_page_too_large;
			diagnosis.PostAnswer       = core::review_submission_post_answer::error_status;
			diagnosis.PostStatusClass  = 5U;
			diagnosis.CommentsExpected = 3U;
			diagnosis.PagesRead        = 1U;
			auto       event           = application::diagnostic_event{ .Operation = application::diagnostic_operation::review_submission, .Outcome = application::diagnostic_outcome::failed };
			const auto text            = core::describe(diagnosis);
			std::ranges::copy(text, event.Summary.Text.begin());
			event.Summary.Size     = static_cast<std::uint16_t>(text.size());
			event.Summary.State    = application::diagnostic_summary_state::sanitized;
			event.ReviewSubmission = diagnosis;
			expect(log.record(event) == status::available, "a review submission reason reaches the real log");
			auto other            = event;
			other.Summary.Text[0] = 'X';
			expect(log.record(other) == status::dropped, "a reason whose detail is anything but its own line never enters the log");
			auto invalid                              = event;
			invalid.ReviewSubmission->HttpStatusClass = 9U;
			expect(log.record(invalid) == status::dropped, "an invalid reason never enters the log");
			auto misplaced      = event;
			misplaced.Operation = application::diagnostic_operation::pull_request;
			expect(log.record(misplaced) == status::dropped, "a reason rides only its own operation");
			// [[BPR-1722]]. A known ending: exactly its own line, on its own operation, never with a reason.
			const auto ending = core::review_submission_ending_diagnosis{ .Ending = core::review_submission_ending::published, .PostAnswer = core::review_submission_post_answer::success, .PostStatusClass = 2U, .Comments = 3U };
			auto       ended  = application::diagnostic_event{ .Operation = application::diagnostic_operation::review_submission, .Outcome = application::diagnostic_outcome::succeeded, .HttpStatus = 200U };
			const auto line   = core::describe(ending);
			std::ranges::copy(line, ended.Summary.Text.begin());
			ended.Summary.Size  = static_cast<std::uint16_t>(line.size());
			ended.Summary.State = application::diagnostic_summary_state::sanitized;
			ended.ReviewEnding  = ending;
			expect(log.record(ended) == status::available, "a review submission ending reaches the real log");
			auto forged            = ended;
			forged.Summary.Text[0] = 'X';
			expect(log.record(forged) == status::dropped, "an ending whose detail is anything but its own line never enters the log");
			auto both             = ended;
			both.ReviewSubmission = diagnosis;
			expect(log.record(both) == status::dropped, "a record carries an ending or a reason, never both");
			auto elsewhere      = ended;
			elsewhere.Operation = application::diagnostic_operation::pull_request;
			expect(log.record(elsewhere) == status::dropped, "an ending rides only its own operation");
			auto impossible               = ended;
			impossible.ReviewEnding->Stop = core::review_submission_stop::cancelled;
			expect(log.record(impossible) == status::dropped, "an invalid ending never enters the log");
		}
		{
			// [[BPR-1680]]. A Home discovery read's end: its counts and
			// timings, and nothing else, reach the file.
			const auto diagnosis = core::discovery_diagnosis{ .Scope = core::discovery_diagnosis_scope::read, .Refresh = 2U, .Read = 5U, .Reason = core::discovery_diagnosis_reason::requested, .Repositories = 1U, .Requests = 6U, .WaitMilliseconds = 40U, .RunMilliseconds = 3900U, .CredentialMilliseconds = 650U, .WireMilliseconds = 3100U, .DecisionMilliseconds = 12U, .Forbidden = 1U, .Timeouts = 1U, .Work = { .SearchCandidates = 39U, .RetainedReuse = 9U, .Rejected = 30U, .Completeness = core::discovery_completeness_reason::complete } };
			auto       event     = application::diagnostic_event{ .Operation = application::diagnostic_operation::discovery, .Outcome = application::diagnostic_outcome::failed, .GithubFailure = core::github_failure::permission };
			const auto text      = core::describe(diagnosis);
			std::ranges::copy(text, event.Summary.Text.begin());
			event.Summary.Size  = static_cast<std::uint16_t>(text.size());
			event.Summary.State = application::diagnostic_summary_state::sanitized;
			expect(log.record(event) == status::available, "a discovery read's end reaches the real log");
			// [[BPR-1827]]. An agent run's end: its protocol, outcome, cause,
			// exit and byte counts, and nothing else, reach the file.
			{
				auto agentDiagnosis                = application::agent_run_diagnosis{ };
				agentDiagnosis.Outcome             = application::agent_run_outcome::protocol_failure;
				agentDiagnosis.Failure             = application::agent_protocol_failure::frame_too_large;
				agentDiagnosis.ExitCode            = 0;
				agentDiagnosis.StandardOutputBytes = 223211U;
				agentDiagnosis.StandardErrorBytes  = 12U;
				const auto agentText               = application::describe(agentDiagnosis);
				auto       agentEvent              = application::diagnostic_event{ .Operation = application::diagnostic_operation::agent_run, .Outcome = application::diagnostic_outcome::failed };
				std::ranges::copy(agentText, agentEvent.Summary.Text.begin());
				agentEvent.Summary.Size  = static_cast<std::uint16_t>(agentText.size());
				agentEvent.Summary.State = application::diagnostic_summary_state::sanitized;
				expect(log.record(agentEvent) == status::available, "an agent run's end reaches the real log");
				auto                       prose = agentEvent;
				constexpr std::string_view note  = "agent wrote: private answer";
				std::ranges::copy(note, prose.Summary.Text.begin());
				prose.Summary.Size = static_cast<std::uint16_t>(note.size());
				expect(log.record(prose) == status::dropped, "an agent run line whose detail is not a diagnosis is dropped");
			}
			for (const auto operation : { application::diagnostic_paged_operation::list_requested_reviewers, application::diagnostic_paged_operation::list_home_pull_requests, application::diagnostic_paged_operation::list_home_review_membership, application::diagnostic_paged_operation::list_review_requests }) {
				auto observed           = event;
				observed.PagedOperation = operation;
				observed.HttpStatus     = 200U;
				observed.GithubFailure  = core::github_failure::response_too_large;
				expect(log.record(observed) == status::available, "a complete discovery diagnosis preserves closed operation and HTTP fields");
			}
			auto other            = event;
			other.Summary.Text[0] = 'X';
			expect(log.record(other) == status::dropped, "a discovery event whose detail is not its own line never enters the log");
			auto bare          = event;
			bare.Summary       = { };
			bare.GithubFailure = { };
			expect(log.record(bare) == status::dropped, "a discovery event always carries its line");
		}
		{
			// [[BPR-1828]]. Agent setup, workspace and review mutation lines:
			// exactly an operation diagnosis on its own family's operation.
			const auto line = [](const application::diagnostic_operation aOperation, const application::diagnostic_outcome aOutcome, const core::operation_diagnosis& aDiagnosis) {
				auto       event = application::diagnostic_event{ .Operation = aOperation, .Outcome = aOutcome };
				const auto text  = core::describe(aDiagnosis);
				std::ranges::copy(text, event.Summary.Text.begin());
				event.Summary.Size  = static_cast<std::uint16_t>(text.size());
				event.Summary.State = application::diagnostic_summary_state::sanitized;
				return event;
			};
			using operation  = application::diagnostic_operation;
			using outcome    = application::diagnostic_outcome;
			const auto check = line(operation::agent_setup_check, outcome::failed, { .Action = core::operation_action::agent_status_check, .Failure = core::operation_failure::not_signed_in, .Process = core::operation_process::nonzero_exit, .ExitCode = 3, .DurationMilliseconds = 812U, .Count = 1U });
			expect(log.record(check) == status::available, "an agent setup check's end reaches the real log");
			expect(log.record(line(operation::agent_workspace, outcome::failed, { .Action = core::operation_action::workspace_materialize, .Failure = core::operation_failure::too_large, .DurationMilliseconds = 2400U })) == status::available,
				   "a workspace preparation's end reaches the real log");
			expect(log.record(line(operation::review_mutation, outcome::started, { .Action = core::operation_action::resolve_thread })) == status::available, "a resolve's start reaches the real log");
			expect(log.record(line(operation::review_mutation, outcome::cancelled, { .Action = core::operation_action::add_reaction, .Failure = core::operation_failure::cancelled })) == status::available, "a cancelled reaction reaches the real log");
			auto forged            = check;
			forged.Summary.Text[0] = 'X';
			expect(log.record(forged) == status::dropped, "an operation line that is not exactly a diagnosis never enters the log");
			auto elsewhere      = check;
			elsewhere.Operation = operation::review_mutation;
			expect(log.record(elsewhere) == status::dropped, "an operation line rides only its own family's operation");
			expect(log.record(line(operation::review_mutation, outcome::succeeded, { .Action = core::operation_action::edit_comment, .Failure = core::operation_failure::conflict })) == status::dropped,
				   "a success never carries a failure");
			expect(log.record(line(operation::review_mutation, outcome::failed, { .Action = core::operation_action::edit_comment })) == status::dropped, "a failure always names one");
			auto bare    = check;
			bare.Summary = { };
			expect(log.record(bare) == status::dropped, "an operation event always carries its line");

			// [[BPR-1831]]. Storage, clone scan, search, image and notification
			// lines: exactly an operation diagnosis on their own operation.
			const auto storage = line(operation::storage, outcome::failed, { .Action = core::operation_action::storage_write, .Failure = core::operation_failure::busy, .Count = 3U, .Store = core::operation_store::review_ledger, .Schema = 7U });
			expect(log.record(storage) == status::available, "a storage failure reaches the real log");
			expect(log.record(line(operation::clone_scan, outcome::succeeded, { .Action = core::operation_action::clone_scan, .DurationMilliseconds = 90U, .Count = 2U })) == status::available, "a clone scan's end reaches the real log");
			expect(log.record(line(operation::source_search, outcome::failed, { .Action = core::operation_action::source_search, .Failure = core::operation_failure::unavailable, .Count = 1U })) == status::available, "a search failure reaches the real log");
			expect(log.record(line(operation::image_fetch, outcome::succeeded, { .Action = core::operation_action::avatar_fetch, .DurationMilliseconds = 40U, .Count = 50U })) == status::available, "aggregated avatar fetches reach the real log");
			expect(log.record(line(operation::notification, outcome::failed, { .Action = core::operation_action::notification_deliver, .Failure = core::operation_failure::permission_refused })) == status::available,
				   "a refused notification reaches the real log");
			auto wrongFamily      = storage;
			wrongFamily.Operation = operation::image_fetch;
			expect(log.record(wrongFamily) == status::dropped, "a storage line rides only the storage operation");
			auto       forgedStorage = storage;
			const auto tail          = std::string_view{ " path=PRIVATE" };
			std::ranges::copy(tail, forgedStorage.Summary.Text.begin() + forgedStorage.Summary.Size);
			forgedStorage.Summary.Size = static_cast<std::uint16_t>(forgedStorage.Summary.Size + tail.size());
			expect(log.record(forgedStorage) == status::dropped, "a storage line with anything after its schema never enters the log");
			expect(log.record(line(operation::image_fetch, outcome::succeeded, { .Action = core::operation_action::image_fetch, .Failure = core::operation_failure::blocked })) == status::dropped, "an image success never carries a failure");
			auto bareStorage    = storage;
			bareStorage.Summary = { };
			expect(log.record(bareStorage) == status::dropped, "a storage event always carries its line");
			// [[BPR-1830]]. The MCP helper, tour and Review Cycle round lines.
			expect(log.record(line(operation::mcp_helper, outcome::failed, { .Action = core::operation_action::mcp_helper_request, .Failure = core::operation_failure::helper_failed, .Process = core::operation_process::nonzero_exit, .ExitCode = 2, .DurationMilliseconds = 40U })) == status::available,
				   "an MCP helper run's end reaches the real log");
			expect(log.record(line(operation::tour_operation, outcome::started, { .Action = core::operation_action::tour_publish })) == status::available, "a tour publication's start reaches the real log");
			expect(log.record(line(operation::cycle_round, outcome::failed, { .Action = core::operation_action::cycle_round_read, .Failure = core::operation_failure::revoked, .DurationMilliseconds = 3U })) == status::available,
				   "a revoked round read reaches the real log");
			expect(log.record(line(operation::tour_operation, outcome::failed, { .Action = core::operation_action::cycle_round_read, .Failure = core::operation_failure::revoked })) == status::dropped,
				   "a round line never rides the tour operation");
			expect(log.record(line(operation::cycle_round, outcome::cancelled, { .Action = core::operation_action::cycle_round_grant, .Failure = core::operation_failure::not_granted })) == status::dropped,
				   "a cancellation names only a cancellation");
		}
		{
			// [[BPR-1829]]. Sign-in, credential vault and updater lines: the
			// same operation diagnosis on their own operations.
			const auto line = [](const application::diagnostic_operation aOperation, const application::diagnostic_outcome aOutcome, const core::operation_diagnosis& aDiagnosis) {
				auto       event = application::diagnostic_event{ .Operation = aOperation, .Outcome = aOutcome };
				const auto text  = core::describe(aDiagnosis);
				std::ranges::copy(text, event.Summary.Text.begin());
				event.Summary.Size  = static_cast<std::uint16_t>(text.size());
				event.Summary.State = application::diagnostic_summary_state::sanitized;
				return event;
			};
			using operation   = application::diagnostic_operation;
			using outcome     = application::diagnostic_outcome;
			const auto denied = line(operation::sign_in, outcome::failed, { .Action = core::operation_action::device_sign_in, .Failure = core::operation_failure::authorization_denied, .DurationMilliseconds = 31000U, .Count = 6U });
			expect(log.record(denied) == status::available, "a denied device sign-in reaches the real log");
			expect(log.record(line(operation::credential_vault, outcome::failed, { .Action = core::operation_action::vault_load, .Failure = core::operation_failure::locked, .DurationMilliseconds = 4U })) == status::available,
				   "a locked keychain read reaches the real log");
			expect(log.record(line(operation::updater, outcome::succeeded, { .Action = core::operation_action::update_download, .DurationMilliseconds = 5200U, .Count = 48000000U })) == status::available,
				   "an updater download reaches the real log");
			auto elsewhere      = denied;
			elsewhere.Operation = operation::updater;
			expect(log.record(elsewhere) == status::dropped, "a sign-in line rides only the sign-in operation");
			expect(log.record(line(operation::credential_vault, outcome::failed, { .Action = core::operation_action::vault_store, .Failure = core::operation_failure::offline })) == status::dropped,
				   "an updater failure on a vault line is refused");
			auto       forged = denied;
			const auto text   = std::string{ "action=device_sign_in failure=authorization_denied process=none exit=none duration_ms=1 count=0 written=0 read_back=0 login=PRIVATE" };
			std::ranges::copy(text, forged.Summary.Text.begin());
			forged.Summary.Size = static_cast<std::uint16_t>(text.size());
			expect(log.record(forged) == status::dropped, "a sign-in line carrying a login never enters the log");
		}
		log.close();
		expect(log.record({ }) == status::closed, "closed ingress");
	}

	{
		adapters::logging::quill_log log{ root, { }, { .Privacy = application::diagnostic_privacy::include_locally } };
		expect(log.record({ .Operation = application::diagnostic_operation::epoch, .Outcome = application::diagnostic_outcome::failed, .Request = 20U, .WindowSession = 2U, .EpochStage = application::diagnostic_epoch_stage::protocol_acquire, .EpochFailure = application::git_smart_transport_failure::unavailable, .GitStep = application::git_smart_transport_step::cli_qualification }) == status::available, "classified CLI refusal reaches the real log");
		expect(log.record({ .Operation = application::diagnostic_operation::epoch, .Outcome = application::diagnostic_outcome::failed, .Request = 19U, .EpochStage = application::diagnostic_epoch_stage::protocol_acquire, .EpochFailure = application::git_protocol_v2_failure::pack_sink, .EpochPackFailure = application::git_pack_stream_failure::input_limit }) == status::available, "typed epoch detail accepted by real sink");
	}
	{
		const auto                   firstRoot  = private_root(root / "profile-first");
		const auto                   secondRoot = private_root(root / "profile-second");
		adapters::logging::quill_log firstLog{ firstRoot, { }, { .Privacy = application::diagnostic_privacy::include_locally } };
		adapters::logging::quill_log secondLog{ secondRoot, { }, { .Privacy = application::diagnostic_privacy::include_locally } };
		expect(firstLog.status() == status::available && secondLog.status() == status::available, "concurrent profiles both have logging");
		expect(firstLog.record({ .Request = 7001 }) == status::available, "first profile event");
		expect(secondLog.record({ .Request = 7002 }) == status::available, "second profile event");
		{
			adapters::logging::quill_log unavailable{ "relative" };
			expect(unavailable.status() == status::unavailable, "invalid sibling stays unavailable");
			adapters::logging::quill_log full{ private_root(root / "profile-full"), { .FileBytes = 1U } };
			static_cast<void>(full.record({ }));
			full.close();
			expect(full.status() == status::full, "one profile's cap is isolated");
		}
		firstLog.close();
		{
			adapters::logging::quill_log reopened{ firstRoot, { }, { .Privacy = application::diagnostic_privacy::include_locally } };
			expect(reopened.record({ .Request = 7003 }) == status::available, "closed profile releases its file while sibling remains live");
		}
		expect(secondLog.record({ .Request = 7004 }) == status::available, "closing initial logger leaves sibling recording");
		secondLog.close();
		const auto firstText  = tests::read_diagnostic_logs(firstRoot);
		const auto secondText = tests::read_diagnostic_logs(secondRoot);
		expect(firstText.find("\"request\":7001,") != std::string::npos && firstText.find("\"request\":7003,") != std::string::npos
				   && firstText.find("\"request\":7002,") == std::string::npos && firstText.find("\"request\":7004,") == std::string::npos,
			   "first log contains only its own events");
		expect(secondText.find("\"request\":7002,") != std::string::npos && secondText.find("\"request\":7004,") != std::string::npos
				   && secondText.find("\"request\":7001,") == std::string::npos && secondText.find("\"request\":7003,") == std::string::npos,
			   "second log contains only its own events");
		adapters::logging::quill_log firstAgain{ firstRoot, { }, { .Privacy = application::diagnostic_privacy::include_locally } };
		adapters::logging::quill_log secondAgain{ secondRoot, { }, { .Privacy = application::diagnostic_privacy::include_locally } };
		secondAgain.close();
		expect(firstAgain.record({ .Request = 7005 }) == status::available, "reverse close order also keeps survivor live");
		firstAgain.close();
		expect(tests::read_diagnostic_logs(firstRoot).find("\"request\":7005,") != std::string::npos, "backend restarts after final owner closes");
	}
	{
		std::barrier              ready{ 4 };
		std::vector<std::jthread> writers;
		writers.reserve(4);
		for (auto index = 0; index < 4; ++index) {
			writers.emplace_back([&, index] {
				adapters::logging::quill_log log{ private_root(root / ("concurrent-" + std::to_string(index))), { }, { .Privacy = application::diagnostic_privacy::include_locally } };
				expect(log.status() == status::available, "concurrent logger admission");
				ready.arrive_and_wait();
				expect(log.record({ .Request = static_cast<std::uint64_t>(8000 + index) }) == status::available, "concurrent logger record");
				log.close();
			});
		}
		writers.clear();
		for (auto index = 0; index < 4; ++index) {
			const auto text = tests::read_diagnostic_logs(root / ("concurrent-" + std::to_string(index)));
			expect(text.find("\"request\":" + std::to_string(8000 + index) + ",") != std::string::npos, "concurrent close drains each owned logger");
		}
	}
	{
		adapters::logging::quill_log busy{ private_root(root / "busy-profile") };
		adapters::logging::quill_log healthy{ private_root(root / "healthy-profile"), { }, { .Privacy = application::diagnostic_privacy::include_locally } };
		auto                         event = application::diagnostic_event{ };
		event.Summary.Text.fill('x');
		event.Summary.State = application::diagnostic_summary_state::sanitized;
		event.Summary.Size  = static_cast<std::uint16_t>(event.Summary.Text.size());
		auto dropped        = false;
		for (auto index = 0; index < 100000 && !dropped; ++index) {
			dropped = busy.record(event) == status::dropped;
		}
		expect(dropped, "actual bounded frontend queue overflow exercised");
		auto priority = status::dropped;
		for (auto attempt = 0; attempt < 100 && priority == status::dropped; ++attempt) {
			priority = busy.record({ .Outcome = application::diagnostic_outcome::failed });
		}
		expect(priority == status::available, "warning/error reserve accepts after normal queue exhaustion");
		busy.close();
		expect(tests::read_diagnostic_logs(root / "busy-profile").find("\"severity\":\"error\"") != std::string::npos,
			   "reserved error survives normal-pressure drainage");
		expect(healthy.record({ .Request = 9001 }) == status::available, "overflow does not disable healthy sibling ingress");
		healthy.close();
		// Final backend drainage checks Quill's delayed failure counters too.
		expect(healthy.status() == status::available, "informational queue-drop report is not a shared backend failure");
		expect(tests::read_diagnostic_logs(root / "healthy-profile").find("\"request\":9001,") != std::string::npos, "healthy sibling event survives overflow drainage");
	}
	const auto first    = tests::read_diagnostic_logs(root);
	const auto exported = application::prepare_release_diagnostics(first, core::release_maintenance_revision{ 1U });
	expect(first.find("github.review_comment") != std::string::npos, "review comment operation retained locally");
	expect(first.find("\"op\":\"github.review_submission\"") != std::string::npos
			   && first.find("\"detail\":\"review_submission=comment_page_too_large post=error_status post_class=5 http_class=0 expected=3 found=0 matched=0 candidates=0 pages=1\"") != std::string::npos
			   && first.find("\"failure_domain\":\"review_submission\",\"failure\":\"comment_page_too_large\"") != std::string::npos,
		   "[[BPR-1540]] the reason is kept locally as its domain, name and counts");
	expect(first.find("\"event\":\"agent.run.failed\"") != std::string::npos && first.find("\"category\":\"agent\"") != std::string::npos
			   && first.find("\"detail\":\"protocol=claude_stream_json outcome=protocol_failure failure=frame_too_large exit=0 stdout_bytes=223211 stderr_bytes=12\"") != std::string::npos
			   && first.find("private answer") == std::string::npos,
		   "[[BPR-1827]] an agent run's end is kept locally as enums and counts only");
	expect(first.find("\"event\":\"github.discovery.failed\"") != std::string::npos
			   && first.find("\"detail\":\"discovery=read refresh=2 read=5 reason=requested state=none repositories=1 pull_requests=0 reads=0 failed=0 requests=6 wait_ms=40 run_ms=3900 credential_ms=650 wire_ms=3100 decision_ms=12 http_403=1 http_429=0 http_5xx=0 timeouts=1 backoff_s=0 candidates=39 reused=9 direct=0 rejected=30 roster_recoveries=0 completeness=complete\"") != std::string::npos
			   && first.find("\"failure_domain\":\"github\",\"failure\":\"permission\"") != std::string::npos,
		   "[[BPR-1680]] a discovery read's end is kept locally with its counts, timings and failure");
	expect(first.find("\"event\":\"agent.setup_check.failed\",\"category\":\"agent\",\"severity\":\"error\"") != std::string::npos
			   && first.find("\"detail\":\"action=agent_status_check failure=not_signed_in process=nonzero_exit exit=3 duration_ms=812 count=1 written=0 read_back=0\"") != std::string::npos
			   && first.find("\"event\":\"agent.workspace.failed\",\"category\":\"agent\"") != std::string::npos
			   && first.find("\"event\":\"github.review_mutation.started\",\"category\":\"github\"") != std::string::npos
			   && first.find("\"event\":\"github.review_mutation.cancelled\"") != std::string::npos,
		   "[[BPR-1828]] agent setup, workspace and review mutation lines are kept with their category and exact line");
	expect(first.find("\"event\":\"github.sign_in.failed\",\"category\":\"github\",\"severity\":\"error\"") != std::string::npos
			   && first.find("\"detail\":\"action=device_sign_in failure=authorization_denied process=none exit=none duration_ms=31000 count=6 written=0 read_back=0\"") != std::string::npos
			   && first.find("\"event\":\"credential.vault.failed\",\"category\":\"credential\"") != std::string::npos
			   && first.find("\"event\":\"updater.release.succeeded\",\"category\":\"updater\"") != std::string::npos && first.find("login=PRIVATE") == std::string::npos,
		   "[[BPR-1829]] sign-in, vault and updater lines are kept with their category and exact line");
	expect(exported && exported->Bytes.find("operation=12 outcome=2 ") != std::string::npos && exported->Bytes.find("operation=13 outcome=2 ") != std::string::npos
			   && exported->Bytes.find("operation=14 outcome=1 ") != std::string::npos,
		   "[[BPR-1829]] the metadata-only preview classifies the sign-in, vault and updater operations without their lines");
	expect(exported && exported->Bytes.find("operation=9 outcome=2 ") != std::string::npos && exported->Bytes.find("operation=10 outcome=2 ") != std::string::npos
			   && exported->Bytes.find("operation=11 outcome=0 ") != std::string::npos && exported->Bytes.find("action=") == std::string::npos,
		   "[[BPR-1828]] the metadata-only preview classifies the new operations without their lines");
	expect(first.find("\"event\":\"storage.database.failed\",\"category\":\"storage\",\"severity\":\"error\"") != std::string::npos
			   && first.find("\"detail\":\"action=storage_write failure=busy process=none exit=none duration_ms=0 count=3 written=0 read_back=0 store=review_ledger schema=7\"") != std::string::npos
			   && first.find("\"event\":\"workspace.clone_scan.succeeded\",\"category\":\"workspace\"") != std::string::npos
			   && first.find("\"event\":\"search.source.failed\",\"category\":\"search\"") != std::string::npos
			   && first.find("\"event\":\"image.fetch.succeeded\",\"category\":\"image\"") != std::string::npos
			   && first.find("\"event\":\"notification.delivery.failed\",\"category\":\"notification\"") != std::string::npos && first.find("path=PRIVATE") == std::string::npos,
		   "[[BPR-1831]] storage, clone scan, search, image and notification lines are kept with their category and exact line");
	expect(exported && exported->Bytes.find("operation=15 outcome=2 ") != std::string::npos && exported->Bytes.find("operation=19 outcome=2 ") != std::string::npos && exported->Bytes.find("store=") == std::string::npos,
		   "[[BPR-1831]] the metadata-only preview classifies them without their lines");
	expect(first.find("\"event\":\"agent.mcp_helper.failed\",\"category\":\"agent\",\"severity\":\"error\"") != std::string::npos
			   && first.find("\"detail\":\"action=mcp_helper_request failure=helper_failed process=nonzero_exit exit=2 duration_ms=40 count=0 written=0 read_back=0\"") != std::string::npos
			   && first.find("\"event\":\"tour.operation.started\",\"category\":\"tour\"") != std::string::npos
			   && first.find("\"event\":\"cycle.round.failed\",\"category\":\"cycle\"") != std::string::npos
			   && first.find("\"detail\":\"action=cycle_round_read failure=revoked process=none exit=none duration_ms=3 count=0 written=0 read_back=0\"") != std::string::npos,
		   "[[BPR-1830]] MCP helper, tour and round lines are kept with their category and exact line");
	expect(exported && exported->Bytes.find("operation=20 outcome=2 ") != std::string::npos && exported->Bytes.find("operation=21 outcome=0 ") != std::string::npos
			   && exported->Bytes.find("operation=22 outcome=2 ") != std::string::npos,
		   "[[BPR-1830]] the metadata-only preview classifies the helper, tour and round operations without their lines");
	expect(exported && exported->Bytes.find("operation=6 outcome=2 ") != std::string::npos && exported->Bytes.find("review_submission=") == std::string::npos,
		   "[[BPR-1540]] and the metadata-only maintenance preview classifies its operation without its detail");
	expect(exported && exported->Bytes.find("http_status=422") != std::string::npos, "bounded review status remains exportable");
	expect(exported && exported->Descriptor.Records != 0U && exported->Bytes.find("runtime-only-file-secret") == std::string::npos
			   && exported->Bytes.find("newField") == std::string::npos && exported->Bytes.find("request=") == std::string::npos,
		   "real Quill log yields bounded metadata-only maintenance preview without prose or identity");
	expect(first.find("\"git_step\":\"cli_qualification\"") != std::string::npos && first.find("\"window_session\":2") != std::string::npos, "closed acquisition step and window identity persist without raw executable or provider output");
	expect(first.find("\"epoch_stage\":\"protocol_acquire\",\"failure_domain\":\"protocol\",\"failure\":\"pack_sink\",\"pack_failure\":\"input_limit\"") != std::string::npos, "fixed protocol and pack failure names persisted");
	expect(first.find("Unknown schema field") != std::string::npos && first.find("unclassified-code") != std::string::npos && first.find("newField") == std::string::npos && first.find("NEW_SCHEMA_CODE") == std::string::npos && first.find("\"request\":42,\"http_status\":200") != std::string::npos, "useful sanitized prose and correlation survive file encoding");
	expect(first.find("runtime-only-file-secret") == std::string::npos, "credential canary absent from persisted file");
	expect(first.find("\"page_operation\":\"ListRequestedReviewers\"") != std::string::npos && first.find("\"page_operation\":\"ListHomePullRequests\"") != std::string::npos && first.find("\"page_operation\":\"ListHomeReviewMembership\"") != std::string::npos && first.find("\"page_operation\":\"ListReviewRequests\"") != std::string::npos, "new discovery operation names survive persistence with the existing redaction checks");
	expect(first.find("\"op\":\"github.pull_request\",\"outcome\":\"response_received\",\"request\":43,\"http_status\":403") != std::string::npos
			   && first.find("\"failure_domain\":\"github\",\"failure\":\"rate_limit\",\"pack_failure\":\"none\",\"page_operation\":\"ListPullRequestFiles\"") != std::string::npos
			   && first.find("\"failure_domain\":\"validation\",\"failure\":\"missing_required_text\"") != std::string::npos
			   && first.find("\"page_operation\":\"ListPullRequestFiles\",\"window_session\":2") != std::string::npos,
		   "typed HTTP, provider and validation facts persist without arbitrary text");
	expect(first.find("\"failure_domain\":\"validation\",\"failure\":\"invalid_relationship\",\"pack_failure\":\"none\",\"page_operation\":\"SearchPullRequests\",\"window_session\":0,\"relationship_detail\":\"relationship_actor_login\"") != std::string::npos, "relationship reason is independent of the final fetched operation");
	expect(first.find("\"request\":46,\"http_status\":404") != std::string::npos
			   && first.find("\"failure\":\"permission\",\"pack_failure\":\"none\",\"page_operation\":\"GetRepository\",\"window_session\":0,\"relationship_detail\":\"none\",\"transport_source\":\"http_response\"") != std::string::npos
			   && first.find("\"request\":47,\"http_status\":0") != std::string::npos
			   && first.find("\"transport_source\":\"network_reply\"") != std::string::npos,
		   "persisted permission diagnostics distinguish HTTP response from network reply");
	expect(first.find("\"op\":\"application\",\"outcome\":\"started\"") != std::string::npos, "ordinary close persists startup");
	{
		adapters::logging::quill_log log{ root, { }, { .Privacy = application::diagnostic_privacy::include_locally } };
		expect(log.record({ .Outcome = application::diagnostic_outcome::shutdown }) == status::available, "append after reopen");
	}
	expect(tests::read_diagnostic_logs(root).find(first) != std::string::npos && tests::read_diagnostic_logs(root).find("\"outcome\":\"shutdown\"") != std::string::npos, "reopen preserves prior records");
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
		adapters::logging::quill_log log{ tests::diagnostic_files(root).front() };
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
		expect(firstWriter.status() == status::available && secondWriter.status() == status::available, "native sessions have independent files in the same root");
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
