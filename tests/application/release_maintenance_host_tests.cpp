#include "bpr/application/manual_executor.hpp"
#include "bpr/application/release_diagnostic_snapshot.hpp"
#include "bpr/application/release_maintenance_application_host.hpp"
#include "bpr/application/release_profile_cleanup.hpp"
#include "bpr/core/release_maintenance.hpp"
#include "bpr/core/review_cycle_zip.hpp"

#include <charconv>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <tuple>
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
		for (unsigned index = 0U; index < core::release_diagnostic_record_limit + 3U; ++index) {
			many += log_line();
		}
		auto bounded = application::prepare_release_diagnostics(many, core::release_maintenance_revision{ 1U });
		expect(bounded && bounded->Descriptor.Records == core::release_diagnostic_record_limit && bounded->Descriptor.OmittedRecords == 3U, "record bound counts omissions");
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
				const application::diagnostic_export_capture capture{ std::string{ session }, 1U, { }, { } };
				const auto                                   document = application::prepare_diagnostic_export(line, core::release_maintenance_revision{ 1U }, { policy }, capture, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
				const bool                                   allowed  = detail == "actor_conflict=viewer,review,ascii_case_only" || detail == "actor_conflict=pull_request,issue_comment,different";
				expect(document && document->Descriptor.Records == 1U && (document->Bytes.find(detail) != std::string::npos) == allowed,
					   "both export policies retain only the closed actor-origin and difference grammar");
				expect(document->Bytes.find("PRIVATE_LOGIN") == std::string::npos && document->Bytes.find("U_PRIVATE_NODE") == std::string::npos,
					   "actor provenance export never admits login or node canaries");
			}
		}
	}

	/// [[BPR-1540]]. A review submission's reason exports as its closed
	/// domain, name and counts-only line; anything else in it is omitted.
	void test_review_submission_reason_export()
	{
		for (const auto policy : { core::diagnostic_export_policy::anonymized, core::diagnostic_export_policy::identifying }) {
			for (const std::string_view failure : { "comment_page_too_large", "review_not_listed", "rate_limited", "PRIVATE_CANARY" }) {
				for (const std::string_view detail : { "review_submission=comment_page_too_large post=error_status post_class=5 http_class=0 expected=3 found=0 matched=0 candidates=0 pages=1",
													   "review_submission=comment_page_too_large post=error_status post_class=5 http_class=0 expected=3 found=0 matched=0 candidates=0 pages=1 host=PRIVATE_HOST",
													   "review_submission=PRIVATE_REPOSITORY post=not_sent post_class=0 http_class=0 expected=0 found=0 matched=0 candidates=0 pages=0" }) {
					auto       line    = structured_line(1U, true);
					const auto replace = [&line](std::string_view aFrom, std::string_view aTo) {
						const auto at = line.find(aFrom);
						expect(at != std::string::npos, "review submission fixture field exists");
						line.replace(at, aFrom.size(), aTo);
					};
					replace(R"("event":"github.capability.failed")", R"("event":"github.review_submission.failed")");
					replace(R"("op":"github.capability")", R"("op":"github.review_submission")");
					replace(R"("http_status":403)", R"("http_status":0)");
					replace(R"("failure_domain":"none","failure":"none")", R"("failure_domain":"review_submission","failure":")" + std::string{ failure } + "\"");
					const auto start = line.find("\"detail\":\"") + std::string_view{ "\"detail\":\"" }.size();
					line.replace(start, line.find("\",\"epoch_stage\"", start) - start, detail);
					const application::diagnostic_export_capture capture{ std::string{ session }, 1U, { }, { } };
					const auto                                   document = application::prepare_diagnostic_export(line, core::release_maintenance_revision{ 1U }, { policy }, capture, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
					const bool                                   known    = failure != "PRIVATE_CANARY";
					const bool                                   exact    = !detail.contains("PRIVATE");
					expect(document && document->Descriptor.Records == (known ? 1U : 0U), "a review submission record exports only with a closed reason");
					expect(!known || (document->Bytes.find(detail) != std::string::npos) == exact, "and keeps exactly its counts-only line");
					expect(document->Bytes.find("PRIVATE_") == std::string::npos, "and nothing private");
				}
			}
		}
	}

	/// [[BPR-1680]]. A Home discovery line exports as its enums and counts;
	/// one with anything else in it, or a word outside its vocabulary, is
	/// omitted.
	void test_discovery_export()
	{
		for (const auto policy : { core::diagnostic_export_policy::anonymized, core::diagnostic_export_policy::identifying }) {
			for (const std::string_view detail : { "discovery=read refresh=3 read=7 reason=authored state=none repositories=1 pull_requests=0 reads=0 failed=0 requests=6 wait_ms=120 run_ms=3900",
												   "discovery=refresh refresh=3 read=0 reason=none state=stale repositories=5 pull_requests=20 reads=30 failed=1 requests=220 wait_ms=4100 run_ms=143000",
												   "discovery=read refresh=3 read=7 reason=authored state=none repositories=1 pull_requests=0 reads=0 failed=0 requests=6 wait_ms=120 run_ms=3900 repository=PRIVATE_REPOSITORY",
												   "discovery=read refresh=3 read=7 reason=PRIVATE_LOGIN state=none repositories=1 pull_requests=0 reads=0 failed=0 requests=6 wait_ms=120 run_ms=3900" }) {
				auto       line    = structured_line(1U, true);
				const auto replace = [&line](std::string_view aFrom, std::string_view aTo) {
					const auto at = line.find(aFrom);
					expect(at != std::string::npos, "discovery fixture field exists");
					line.replace(at, aFrom.size(), aTo);
				};
				replace(R"("event":"github.capability.failed")", R"("event":"github.discovery.failed")");
				replace(R"("op":"github.capability")", R"("op":"github.discovery")");
				replace(R"("http_status":403)", R"("http_status":0)");
				replace(R"("failure_domain":"none","failure":"none")", R"("failure_domain":"github","failure":"permission")");
				const auto start = line.find("\"detail\":\"") + std::string_view{ "\"detail\":\"" }.size();
				line.replace(start, line.find("\",\"epoch_stage\"", start) - start, detail);
				const application::diagnostic_export_capture capture{ std::string{ session }, 1U, { }, { } };
				const auto                                   document = application::prepare_diagnostic_export(line, core::release_maintenance_revision{ 1U }, { policy }, capture, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
				const bool                                   exact    = !detail.contains("PRIVATE");
				expect(document && document->Descriptor.Records == 1U, "a discovery record exports under either policy");
				expect((document->Bytes.find(detail) != std::string::npos) == exact && document->Bytes.contains(R"("op":"github.discovery")"),
					   "and keeps exactly its counts-only line");
				expect(document->Bytes.find("PRIVATE_") == std::string::npos, "and nothing private");
			}
		}
	}

	/// [[BPR-1722]]. A record of `aOp`.`aOutcome` at `aUtc`, with the
	/// given status, failure and detail, as the logger writes it.
	auto event_line(unsigned aSequence, std::uint64_t aUtc, std::string_view aOp, std::string_view aOutcome, unsigned aHttp, std::string_view aDomain, std::string_view aFailure, std::string_view aDetail)
		-> std::string
	{
		auto       line    = structured_line(aSequence);
		const auto replace = [&line](std::string_view aFrom, std::string_view aTo) {
			const auto at = line.find(aFrom);
			expect(at != std::string::npos, "record fixture field exists");
			line.replace(at, aFrom.size(), aTo);
		};
		const auto severity = aOutcome == "failed" ? "error" : aHttp >= 400U ? "warning"
																			 : "info";
		replace(R"("event":"github.capability.failed")", "\"event\":\"" + std::string{ aOp } + "." + std::string{ aOutcome } + "\"");
		replace(R"("severity":"error")", std::string{ "\"severity\":\"" } + severity + "\"");
		replace("1780000000000", std::to_string(aUtc));
		replace(R"("op":"github.capability","outcome":"failed")", "\"op\":\"" + std::string{ aOp } + "\",\"outcome\":\"" + std::string{ aOutcome } + "\"");
		replace(R"("http_status":403)", "\"http_status\":" + std::to_string(aHttp));
		replace(R"("failure_domain":"none","failure":"none")", "\"failure_domain\":\"" + std::string{ aDomain } + "\",\"failure\":\"" + std::string{ aFailure } + "\"");
		const auto start = line.find("\"detail\":\"") + std::string_view{ "\"detail\":\"" }.size();
		line.replace(start, line.find("\",\"epoch_stage\"", start) - start, aDetail);
		return line;
	}

	/// [[BPR-1722]]. A review's known ending exports as exactly its
	/// line of enums and counts; one with anything else in it is omitted.
	void test_review_submission_ending_export()
	{
		for (const auto policy : { core::diagnostic_export_policy::anonymized, core::diagnostic_export_policy::identifying }) {
			for (const auto& [outcome, http, detail] : std::initializer_list<std::tuple<std::string_view, unsigned, std::string_view>>{
					 { "succeeded", 200U, "review_submission_ending=published run=submit post=success post_class=2 refusal=none stop=none comments=9 duplicate=0" },
					 { "failed", 422U, "review_submission_ending=refused run=submit post=error_status post_class=4 refusal=unplaceable_line stop=none comments=9 duplicate=0" },
					 { "failed", 0U, "review_submission_ending=settled run=check post=no_answer post_class=0 refusal=none stop=none comments=9 duplicate=0" },
					 { "failed", 0U, "review_submission_ending=not_sent run=submit post=not_sent post_class=0 refusal=none stop=stale_head comments=2 duplicate=0" },
					 { "failed", 422U, "review_submission_ending=refused run=submit post=error_status post_class=4 refusal=review stop=none comments=9 duplicate=0 repository=PRIVATE_REPOSITORY" },
					 { "succeeded", 200U, "review_submission_ending=PRIVATE_HOST run=submit post=success post_class=2 refusal=none stop=none comments=9 duplicate=0" } }) {
				const auto                                   line = event_line(1U, 1780000000000ULL, "github.review_submission", outcome, http, "review_submission", "none", detail);
				const application::diagnostic_export_capture capture{ std::string{ session }, 1U, { }, { } };
				const auto                                   document = application::prepare_diagnostic_export(line, core::release_maintenance_revision{ 1U }, { policy }, capture, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
				const bool                                   exact    = !detail.contains("PRIVATE");
				expect(document && document->Descriptor.Records == 1U, "a review submission ending exports under either policy");
				expect((document->Bytes.find(detail) != std::string::npos) == exact && document->Bytes.contains("\"http_status\":" + std::to_string(http) + ","),
					   "and keeps exactly its counts-only line and the POST's status");
				expect(document->Bytes.find("PRIVATE_") == std::string::npos && (policy == core::diagnostic_export_policy::identifying || !document->Bytes.contains(session)), "and nothing private");
			}
		}
	}

	/// [[BPR-1722]]. An eight-hour session at the measured heavy rate
	/// of about 1,000 records an hour, with Home refreshes and review
	/// submissions among them, exports whole. Before, the 256 KiB output held
	/// about 300 such records and left out the rest.
	void test_export_keeps_long_session()
	{
		constexpr unsigned      records  = 8U * 1000U;
		constexpr std::uint64_t start    = 1780000000000ULL;
		constexpr std::uint64_t interval = 3600U;
		std::string             input;
		for (unsigned sequence = 1U; sequence <= records; ++sequence) {
			const auto utc = start + (std::uint64_t{ sequence } * interval);
			if (sequence % 500U == 0U) {
				input += event_line(sequence, utc, "github.review_submission", "failed", 422U, "review_submission", "none",
									"review_submission_ending=refused run=submit post=error_status post_class=4 refusal=unplaceable_line stop=none comments=9 duplicate=0");
			} else if (sequence % 25U == 0U) {
				input += event_line(sequence, utc, "github.discovery", "succeeded", 0U, "none", "none",
									"discovery=refresh refresh=" + std::to_string(sequence / 25U) + " read=0 reason=none state=current repositories=5 pull_requests=20 reads=6 failed=0 requests=9 wait_ms=40 run_ms=5200");
			} else {
				auto line = structured_line(sequence);
				line.replace(line.find("1780000000000"), 13U, std::to_string(utc));
				input += line;
			}
		}
		const application::diagnostic_export_capture capture{ std::string{ session }, records, { }, "0.3.1-0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef-Release" };
		const auto                                   document = application::prepare_diagnostic_export(input, core::release_maintenance_revision{ 1U }, { core::diagnostic_export_policy::anonymized }, capture,
																									   "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 0U, { .UtcMs = start + (std::uint64_t{ records + 1U } * interval), .OffsetMinutes = -420 });
		expect(document.has_value(), "an eight-hour session prepares");
		const auto& descriptor = document->Descriptor;
		std::cerr << "eight-hour session: " << records << " records, " << input.size() << " input bytes, " << document->Bytes.size() << " export bytes\n";
		expect(descriptor.Records == records && descriptor.OmittedRecords == 0U && descriptor.LimitOmittedRecords == 0U, "every record of an eight-hour session is kept");
		expect(descriptor.FirstUtcMs == start + interval && descriptor.LastUtcMs == start + (std::uint64_t{ records } * interval), "from its first record to its last");
		expect(document->Bytes.contains("\"limit_omitted_records\":0,") && document->Bytes.contains("discovery=refresh refresh=319 ")
				   && document->Bytes.contains("review_submission_ending=refused run=submit post=error_status post_class=4 refusal=unplaceable_line"),
			   "with its Home refreshes and review endings, and the header says nothing was left out");
		expect(!document->Bytes.contains(session), "and anonymized");
	}

	/// [[BPR-PENDING-AG]]. A typical export saved as its archive: about 316
	/// records of a working session, as in the owner's two 256 KiB exports
	/// (801 bytes a record), with pull request pages, Home refreshes, review
	/// endings and capability failures, and times and durations that vary.
	/// The archive reads back through zlib's inflate as exactly the text and
	/// is a small fraction of it; the owner's exports zipped to about 8 KB.
	void test_export_archive_compresses()
	{
		const auto sessionText = [](const unsigned aRecords) {
			std::string   input;
			std::uint64_t utc  = 1780000000000ULL;
			std::uint32_t seed = 2166136261U;
			for (unsigned sequence = 1U; sequence <= aRecords; ++sequence) {
				seed = (seed * 1664525U) + 1013904223U;
				utc += 200U + (seed % 9000U);
				std::string line;
				if (sequence % 97U == 0U) {
					line = event_line(sequence, utc, "github.review_submission", "failed", 422U, "review_submission", "none",
									  "review_submission_ending=refused run=submit post=error_status post_class=4 refusal=unplaceable_line stop=none comments=" + std::to_string(1U + (seed % 9U)) + " duplicate=0");
				} else if (sequence % 11U == 0U) {
					line = event_line(sequence, utc, "github.discovery", "succeeded", 0U, "none", "none",
									  "discovery=refresh refresh=" + std::to_string(sequence / 11U) + " read=0 reason=none state=current repositories=" + std::to_string(3U + (seed % 5U)) + " pull_requests="
										  + std::to_string(10U + (seed % 30U)) + " reads=6 failed=0 requests=" + std::to_string(5U + (seed % 9U)) + " wait_ms=" + std::to_string(seed % 400U) + " run_ms=" + std::to_string(800U + (seed % 6000U)));
				} else if (sequence % 3U == 0U) {
					line = event_line(sequence, utc, "github.pull_request", "succeeded", 200U, "none", "none", "");
				} else {
					line = structured_line(sequence);
					line.replace(line.find("1780000000000"), 13U, std::to_string(utc));
				}
				line.replace(line.find("\"elapsed_us\":5,"), 15U, "\"elapsed_us\":" + std::to_string(seed % 2000000U) + ",");
				input += line;
			}
			return input;
		};
		const application::diagnostic_export_capture capture{ std::string{ session }, 400U, { }, "0.3.1-0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef-Release" };
		const auto                                   when = core::diagnostic_export_local_time{ .Year = 2026, .Month = 10, .Day = 21, .Hour = 14, .Minute = 32, .Second = 5 };
		for (const auto policy : { core::diagnostic_export_policy::anonymized, core::diagnostic_export_policy::identifying, core::diagnostic_export_policy::metadata }) {
			const auto input    = sessionText(316U);
			const auto document = policy == core::diagnostic_export_policy::metadata
				? application::prepare_release_diagnostics(input, core::release_maintenance_revision{ 1U })
				: application::prepare_diagnostic_export(input, core::release_maintenance_revision{ 1U }, { policy }, capture, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 0U, { .UtcMs = 1780000000000ULL, .OffsetMinutes = -420 });
			expect(document.has_value(), "a working session's export prepares");
			const auto file    = core::diagnostic_export_file_name(when, "0.3.1");
			const auto archive = core::diagnostic_export_archive(file, document->Bytes, when);
			const auto read    = core::read_cycle_zip({ reinterpret_cast<const std::uint8_t*>(archive.data()), archive.size() });
			expect(read && read->size() == 1U && read->front().Name == "BetterPullReview-diagnostics-2026-10-21-1432-0.3.1.txt" && read->front().Bytes == document->Bytes,
				   "the archive's one entry, under the .txt name, inflates to exactly the previewed text");
			std::cerr << "export archive (policy " << static_cast<int>(policy) << "): " << document->Descriptor.Records << " records, " << document->Bytes.size() << " text bytes, " << archive.size() << " archive bytes, ratio "
					  << (document->Bytes.size() / archive.size()) << ":1\n";
			if (policy != core::diagnostic_export_policy::metadata) {
				expect(document->Bytes.size() > 200U * 1024U && document->Bytes.starts_with("{\"format\":\"bpr-diagnostics-v2\",\"exported_at\":\"") && read->front().Bytes.starts_with("{\"format\":\"bpr-diagnostics-v2\""),
					   "a typical export is about 256 KiB and keeps its header inside the archive");
				expect(policy == core::diagnostic_export_policy::identifying || !read->front().Bytes.contains(session), "and the anonymized one keeps its labels inside");
			}
			expect(archive.size() * 10U < document->Bytes.size(), "and compresses at least ten times");
		}
		// The largest text an export holds still fits its archive.
		const auto heavy    = sessionText(20000U);
		const auto document = application::prepare_diagnostic_export(heavy, core::release_maintenance_revision{ 1U }, { core::diagnostic_export_policy::anonymized }, { std::string{ session }, 1000000U, { }, { } }, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
		expect(document && document->Descriptor.Records == core::release_diagnostic_record_limit, "a full export prepares");
		const auto archive = core::diagnostic_export_archive("full.zip", document->Bytes, when);
		const auto read    = core::read_cycle_zip({ reinterpret_cast<const std::uint8_t*>(archive.data()), archive.size() });
		std::cerr << "full export archive: " << document->Descriptor.Records << " records, " << document->Bytes.size() << " text bytes, " << archive.size() << " archive bytes\n";
		expect(read && read->size() == 1U && read->front().Bytes == document->Bytes && archive.size() * 10U < document->Bytes.size(), "a full export's archive reads back whole and compresses as well");
	}

	/// [[BPR-1722]]. The raised limits still hold: past the record limit
	/// the oldest are left out, wide records stop at the byte limit, and input
	/// past its bounds is refused.
	void test_export_bounds()
	{
		const application::diagnostic_export_capture capture{ std::string{ session }, 1000000U, { }, { } };
		std::string                                  many;
		for (unsigned sequence = 1U; sequence <= core::release_diagnostic_record_limit + 10U; ++sequence) {
			many += event_line(sequence, 1780000000000ULL + sequence, "github.capability", "succeeded", 0U, "none", "none", "");
		}
		const auto counted = application::prepare_diagnostic_export(many, core::release_maintenance_revision{ 1U }, { core::diagnostic_export_policy::anonymized }, capture, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
		expect(counted && counted->Descriptor.Records == core::release_diagnostic_record_limit && counted->Descriptor.LimitOmittedRecords == 10U
				   && counted->Bytes.contains("\"utc_ms\":" + std::to_string(1780000000000ULL + 11U) + ",") && !counted->Bytes.contains("\"utc_ms\":" + std::to_string(1780000000000ULL + 10U) + ","),
			   "past the record limit the oldest are left out and counted");
		// A safe detail as wide as the classifier admits: about 4 KiB a record.
		std::string wide;
		while (wide.size() + 19U <= 4000U) {
			wide += "code=\\\"FORBIDDEN\\\" ";
		}
		std::string heavy;
		unsigned    lines = 0U;
		while (heavy.size() < core::release_diagnostic_byte_limit + (1024U * 1024U)) {
			++lines;
			heavy += event_line(lines, 1780000000000ULL + lines, "github.capability", "failed", 0U, "none", "none", wide);
		}
		const auto sized = application::prepare_diagnostic_export(heavy, core::release_maintenance_revision{ 1U }, { core::diagnostic_export_policy::anonymized }, capture, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
		expect(sized && sized->Bytes.size() <= core::release_diagnostic_byte_limit && sized->Descriptor.LimitOmittedRecords > 0U && sized->Descriptor.Records + sized->Descriptor.LimitOmittedRecords == lines
				   && sized->Bytes.contains("\"utc_ms\":" + std::to_string(1780000000000ULL + lines) + ","),
			   "wide records stop at the byte limit, keeping the newest");
		expect(!application::prepare_diagnostic_export(std::string(application::release_diagnostic_input_limit + 1U, '\n'), core::release_maintenance_revision{ 1U }, { core::diagnostic_export_policy::anonymized }, capture, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
			   "input past its byte limit is refused");
		expect(!application::prepare_diagnostic_export(std::string(application::release_diagnostic_input_records + 1U, '\n'), core::release_maintenance_revision{ 1U }, { core::diagnostic_export_policy::anonymized }, capture, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
			   "input past its line limit is refused");
		static_assert(core::release_diagnostic_byte_limit <= std::uint64_t{ 16U } * 1024U * 1024U && core::release_diagnostic_record_limit <= 16384U && application::release_diagnostic_input_limit <= std::size_t{ 32U } * 1024U * 1024U,
					  "an export stays something a person attaches to a report");
	}

	/// [[BPR-1722]]. The header names when the export was taken, in
	/// local time with its offset, and the build that took it.
	void test_export_stamp()
	{
		// 2026-10-21T21:32:05.250Z.
		constexpr std::uint64_t                      utc = 1792618325250ULL;
		const application::diagnostic_export_capture capture{ std::string{ session }, 1U, { }, "0.3.1-0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef-Release" };
		const auto                                   document = application::prepare_diagnostic_export(structured_line(1U), core::release_maintenance_revision{ 1U }, { core::diagnostic_export_policy::anonymized }, capture, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 0U, { .UtcMs = utc, .OffsetMinutes = -420 });
		expect(document && document->Bytes.starts_with(R"({"format":"bpr-diagnostics-v2","exported_at":"2026-10-21T14:32:05-07:00","app_version":"0.3.1","app_build":"0.3.1-0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef-Release","policy":"anonymized",)"),
			   "the header names the local export time with its offset and the build");
		expect(application::format_diagnostic_export_time({ .UtcMs = utc, .OffsetMinutes = 330 }) == "2026-10-22T03:02:05+05:30" && application::format_diagnostic_export_time({ .UtcMs = utc, .OffsetMinutes = 0 }) == "2026-10-21T21:32:05+00:00",
			   "offsets east and at UTC");
		expect(application::format_diagnostic_export_time({ }) == "unknown" && application::format_diagnostic_export_time({ .UtcMs = utc, .OffsetMinutes = 19 * 60 }) == "unknown", "no time, or an impossible offset, is unknown");
		const application::diagnostic_export_capture unbuilt{ std::string{ session }, 1U, { }, "PRIVATE_HOST" };
		const auto                                   plain = application::prepare_diagnostic_export(structured_line(1U), core::release_maintenance_revision{ 1U }, { core::diagnostic_export_policy::anonymized }, unbuilt, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
		expect(plain && plain->Bytes.starts_with(R"({"format":"bpr-diagnostics-v2","exported_at":"unknown","app_version":"unknown","app_build":"unknown","policy":)") && !plain->Bytes.contains("PRIVATE"),
			   "without a stamp or a well-formed build the header says unknown");
	}

	void test_git_capability_step_export()
	{
		for (const auto policy : { core::diagnostic_export_policy::anonymized, core::diagnostic_export_policy::identifying }) {
			for (const std::string_view step : { "git_capability_probe", "cli_capability_probe", "git_capability_probe_PRIVATE_CANARY", "cli_capability_probe_PRIVATE_CANARY" }) {
				auto       line     = structured_line(1U, true);
				const auto position = line.find("\"git_step\":\"none\"");
				expect(position != std::string::npos, "closed Git step fixture field exists");
				line.replace(position, std::string_view{ "\"git_step\":\"none\"" }.size(), "\"git_step\":\"" + std::string{ step } + "\"");
				const application::diagnostic_export_capture capture{ std::string{ session }, 1U, { }, { } };
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
				const application::diagnostic_export_capture capture{ std::string{ session }, 1U, { }, { } };
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
		application::diagnostic_export_capture capture{ std::string{ session }, 3U, { structured_line(2), structured_line(3) }, { } };
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
		for (unsigned sequence = 1U; sequence <= core::release_diagnostic_record_limit + 1U; ++sequence) {
			many += structured_line(sequence);
		}
		auto unlimitedCapture     = capture;
		unlimitedCapture.Sequence = core::release_diagnostic_record_limit + 1U;
		const auto bounded        = application::prepare_diagnostic_export(many, core::release_maintenance_revision{ 2U }, { policy::anonymized }, unlimitedCapture, "dddddddddddddddddddddddddddddddd");
		expect(bounded && bounded->Descriptor.Records + bounded->Descriptor.OmittedRecords == core::release_diagnostic_record_limit + 1U && bounded->Descriptor.OmittedRecords > 0U && bounded->Bytes.size() <= core::release_diagnostic_byte_limit, "rich export byte bound counts every omitted record");

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

	/// [[BPR-1717]]. An export over its limits keeps the newest records,
	/// whatever order its input comes in, and says how many older ones it left
	/// out. Before, it kept the first records it read: files came in name
	/// order, which is a random session identity, so a running session's
	/// records could all be dropped behind older sessions'.
	void test_export_keeps_newest()
	{
		using policy                   = core::diagnostic_export_policy;
		constexpr std::string_view old = "1111222233334444aaaabbbbccccdddd";
		const auto                 at  = [](unsigned aSequence, std::string_view aSession, std::uint64_t aUtc) {
			auto       line    = structured_line(aSequence);
			const auto replace = [&line](std::string_view aFrom, std::string_view aTo) {
				for (auto found = line.find(aFrom); found != std::string::npos; found = line.find(aFrom, found + aTo.size())) {
					line.replace(found, aFrom.size(), aTo);
				}
			};
			replace(session, aSession);
			replace("1780000000000", std::to_string(aUtc));
			return line;
		};
		// The current session's review submission left unknown, as BPR-1540 logs it.
		auto submission = at(3U, session, 1780000600000ULL);
		{
			const auto replace = [&submission](std::string_view aFrom, std::string_view aTo) {
				const auto found = submission.find(aFrom);
				expect(found != std::string::npos, "review submission fixture field exists");
				submission.replace(found, aFrom.size(), aTo);
			};
			replace(R"("event":"github.capability.failed")", R"("event":"github.review_submission.failed")");
			replace(R"("op":"github.capability")", R"("op":"github.review_submission")");
			replace(R"("http_status":403)", R"("http_status":0)");
			replace(R"("failure_domain":"none","failure":"none")", R"("failure_domain":"review_submission","failure":"comment_page_too_large")");
			const auto start = submission.find("\"detail\":\"") + std::string_view{ "\"detail\":\"" }.size();
			submission.replace(start, submission.find("\",\"epoch_stage\"", start) - start, "review_submission=comment_page_too_large post=error_status post_class=5 http_class=0 expected=3 found=0 matched=0 candidates=0 pages=1");
		}
		// Older sessions come first in input, as a lower-sorting file name puts them.
		constexpr unsigned older = core::release_diagnostic_record_limit + 200U;
		std::string        input;
		for (unsigned sequence = 1U; sequence <= older; ++sequence) {
			input += at(sequence, old, 1770000000000ULL + sequence);
		}
		input += at(1U, session, 1780000500000ULL) + at(2U, session, 1780000550000ULL) + submission;
		const application::diagnostic_export_capture capture{ std::string{ session }, 3U, { }, { } };
		const auto                                   document = application::prepare_diagnostic_export(input, core::release_maintenance_revision{ 1U }, { policy::anonymized }, capture, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
		expect(document.has_value(), "an export over its limits still prepares");
		const auto& bytes      = document->Bytes;
		const auto& descriptor = document->Descriptor;
		expect(descriptor.LastUtcMs == 1780000600000ULL && descriptor.Sessions == 2U, "the current session's records are in the export");
		expect(bytes.contains(R"("op":"github.review_submission")") && bytes.contains("review_submission=comment_page_too_large post=error_status post_class=5 http_class=0 expected=3 found=0 matched=0 candidates=0 pages=1")
				   && bytes.contains(R"("failure_domain":"review_submission","failure":"comment_page_too_large")"),
			   "its review submission keeps its domain, reason and counts-only line");
		expect(!bytes.contains(session) && !bytes.contains(old), "and no session identity survives anonymization");
		expect(descriptor.LimitOmittedRecords > 0U && descriptor.OmittedRecords == descriptor.LimitOmittedRecords && descriptor.Records + descriptor.LimitOmittedRecords == older + 3U,
			   "every record left out is counted, and all of them for the limit");
		expect(bytes.contains("\"unclassified_records\":0,\"limit_omitted_records\":" + std::to_string(descriptor.LimitOmittedRecords) + ",\"limit_keeps\":\"newest\",\"unread_files\":0,"),
			   "the header says what the limit left out and that the newest are kept");
		const auto oldest = older - (descriptor.Records - 3U) + 1U;
		expect(bytes.contains("\"utc_ms\":" + std::to_string(1770000000000ULL + oldest) + ",") && !bytes.contains("\"utc_ms\":" + std::to_string(1770000000000ULL + oldest - 1U) + ","),
			   "the older session's newest records are the ones kept");
		std::uint64_t previous = 0U;
		bool          ordered  = true;
		for (auto found = bytes.find("\"utc_ms\":"); found != std::string::npos; found = bytes.find("\"utc_ms\":", found + 1U)) {
			std::uint64_t utc = 0U;
			std::from_chars(bytes.data() + found + 9U, bytes.data() + bytes.size(), utc);
			ordered  = ordered && utc >= previous;
			previous = utc;
		}
		expect(ordered && bytes.size() <= core::release_diagnostic_byte_limit, "records are written oldest first within the byte limit");
		// Version-one lines carry no time, so they are the oldest and go first.
		std::string metadata;
		for (unsigned index = 0U; index < core::release_diagnostic_record_limit + 3U; ++index) {
			metadata += "2026-09-20T00:00:00Z v=1 op=github.pull_request outcome=failed request=1 http_status=403 summary=sanitized dropped=" + std::to_string(index) + " detail=\n";
		}
		const auto newest = application::prepare_release_diagnostics(metadata, core::release_maintenance_revision{ 1U });
		expect(newest && newest->Descriptor.Records == core::release_diagnostic_record_limit && newest->Descriptor.LimitOmittedRecords == 3U && !newest->Bytes.contains("dropped=2\n")
				   && newest->Bytes.contains("dropped=3\n") && newest->Bytes.ends_with("dropped=" + std::to_string(core::release_diagnostic_record_limit + 2U) + "\n"),
			   "the metadata export keeps the newest records too");
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
	test_review_submission_reason_export();
	test_discovery_export();
	test_git_capability_step_export();
	test_git_tool_failure_export();
	test_rich_export();
	test_export_keeps_newest();
	test_review_submission_ending_export();
	test_export_keeps_long_session();
	test_export_archive_compresses();
	test_export_bounds();
	test_export_stamp();
	test_preview_export();
	test_cancel_invalidate_and_shutdown();
	test_lifecycle_failure_and_reentrancy();
	test_refused_effects_and_retired_barrier();
	std::cout << "release-maintenance-host-tests:pass\n";
}
