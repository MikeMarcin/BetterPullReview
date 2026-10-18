#include "bpr/adapters/sqlite/profile_activation.hpp"
#include "bpr/adapters/sqlite/profile_runtime_lease.hpp"
#include "bpr/adapters/sqlite/tour_owner_channel.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#if defined(_WIN32)
	#include "scratch_root.hpp"
#else
	#include <sys/socket.h>
	#include <sys/stat.h>
	#include <sys/un.h>
	#include <unistd.h>
#endif

namespace {
	using namespace bpr;
	using namespace bpr::adapters::sqlite;
	using application::tour_channel_bytes;
	using application::tour_channel_failure;
	using application::tour_owner_operation;
	using application::tour_owner_request;
	using application::tour_owner_response;
	constexpr core::bpr_profile_id profile{ 1U };
	constexpr auto                 draft = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

	/// \brief What a failed expectation throws, once its `FAILED:` line is out. [[BPR-1118]].
	struct expectation_failed
	{
	};

	[[noreturn]] void fail(const std::string_view aMessage)
	{
		std::cerr << "FAILED: " << aMessage << '\n';
		throw expectation_failed{ };
	}

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			fail(aMessage);
		}
	}

	auto load_request_value() -> tour_owner_request { return { .Operation = tour_owner_operation::load, .Draft = draft }; }

	auto load_request() -> tour_channel_bytes
	{
		return encode_tour_request(load_request_value());
	}

	auto edit_request(std::string aId, std::string aText, core::tour_command_kind aKind = core::tour_command_kind::problem) -> tour_owner_request
	{
		tour_owner_request result{ .Operation = tour_owner_operation::mutate, .Draft = draft };
		result.Edit.RequestId                = std::move(aId);
		result.Edit.Command.Kind             = aKind;
		result.Edit.Command.Text             = std::move(aText);
		result.Edit.Command.ExpectedRevision = 1U;
		return result;
	}

	auto response_bytes(std::string aError) -> tour_channel_bytes
	{
		return encode_tour_response({ .Error = std::move(aError) });
	}

	void test_codec()
	{
		const auto request = edit_request("req-1", "Exact problem \xce\xb1\n");
		const auto bytes   = encode_tour_request(request);
		expect(!bytes.empty(), "a finite local edit encodes");
		const auto decoded = decode_tour_request(bytes);
		expect(decoded && decoded->Draft == draft && decoded->Edit.RequestId == "req-1" && decoded->Edit.Command.Text == request.Edit.Command.Text && decoded->Operation == tour_owner_operation::mutate, "the request round-trips exactly");
		expect(decoded->Edit.Digest == tour_request_digest(request.Edit), "the receiver computes the payload identity itself");
		auto changed = edit_request("req-1", "Another problem\n");
		expect(tour_request_digest(changed.Edit) != decoded->Edit.Digest, "another payload has another identity under the same request ID");
		auto rerouted        = request;
		rerouted.Edit.Origin = application::tour_presentation_origin::command_client;
		expect(tour_request_digest(rerouted.Edit) != decoded->Edit.Digest, "the presentation origin is part of the request identity");
		auto forged = bytes;
		forged.push_back(0);
		expect(!decode_tour_request(forged), "trailing bytes are refused");
		auto truncated = bytes;
		truncated.pop_back();
		expect(!decode_tour_request(truncated), "a truncated frame is refused");
		expect(!decode_tour_request({ }), "an empty frame is refused");
		tour_channel_bytes oversized(application::maximum_tour_channel_bytes + 1U, 1);
		expect(!decode_tour_request(oversized), "an oversized frame is refused before parsing");
		expect(encode_tour_request(edit_request("req-2", std::string(application::maximum_tour_channel_bytes + 1U, 'x'))).empty(), "an oversized request does not encode");
		for (const auto kind : { core::tour_command_kind::observe_head, core::tour_command_kind::restore, core::tour_command_kind::insert_outline }) {
			expect(encode_tour_request(edit_request("req-3", "x", kind)).empty(), "observation, restore and imported state cannot be requested over the channel");
		}
		auto restore                 = edit_request("req-4", "x");
		restore.Edit.Command.Restore = core::review_tour{ };
		expect(encode_tour_request(restore).empty(), "an arbitrary restore payload cannot ride a permitted kind");
		auto badOperation           = edit_request("req-5", "x");
		badOperation.Edit.Operation = application::tour_service_operation::import_selected;
		expect(encode_tour_request(badOperation).empty(), "import selection is not a channel operation");
		expect(encode_tour_request({ .Operation = tour_owner_operation::load, .Draft = "not-a-token" }).empty(), "only an exact opaque draft selector is admitted");
		// [[BPR-1064]]. The assist operations are finite; the caller supplies an answer or a query, nothing else.
		application::tour_owner_request query{ .Operation = tour_owner_operation::assist_issue, .Draft = draft };
		query.Assist            = { .Hook = core::writing_hook::problem, .HasQuote = true, .Quote = "stale\xc3\xa9", .Session = "cli" };
		const auto queryBytes   = encode_tour_request(query);
		const auto decodedQuery = decode_tour_request(queryBytes);
		expect(!queryBytes.empty() && decodedQuery && decodedQuery->Assist.Quote == query.Assist.Quote && decodedQuery->Assist.Hook == core::writing_hook::problem && decodedQuery->Assist.HasQuote, "an assist query round-trips");
		auto badQuery            = query;
		badQuery.Assist.HasRange = true;
		expect(encode_tour_request(badQuery).empty(), "a quote and a range are not both accepted");
		badQuery             = query;
		badQuery.Assist.Hook = core::writing_hook::comment;
		expect(encode_tour_request(badQuery).empty(), "a comment query must name its stop and no other hook may");
		badQuery              = query;
		badQuery.Assist.Quote = "\xff";
		expect(encode_tour_request(badQuery).empty(), "a malformed quote is refused");
		application::tour_owner_request answer{ .Operation = tour_owner_operation::mutate, .Draft = draft };
		answer.Edit.RequestId         = "assist-1";
		answer.Edit.Operation         = application::tour_service_operation::assist_complete;
		answer.Edit.Assist.Id         = 7;
		answer.Edit.Assist.ResultJson = "{\"schema\":\"bpr.review-preparation.result.v1\"}";
		const auto answerBytes        = encode_tour_request(answer);
		const auto decodedAnswer      = decode_tour_request(answerBytes);
		expect(!answerBytes.empty() && decodedAnswer && decodedAnswer->Edit.Assist.Id == 7 && decodedAnswer->Edit.Assist.ResultJson == answer.Edit.Assist.ResultJson && decodedAnswer->Edit.Operation == application::tour_service_operation::assist_complete, "an assist answer round-trips");
		auto clock            = answer;
		clock.Edit.Assist.Now = 5;
		expect(encode_tour_request(clock).empty(), "the caller cannot supply the owner's clock");
		auto hook                            = answer;
		hook.Edit.Assist.CurrentHookRevision = 5;
		expect(encode_tour_request(hook).empty(), "nor the hook revision");
		auto smuggled              = answer;
		smuggled.Edit.Command.Text = "written by the caller";
		expect(encode_tour_request(smuggled).empty(), "an answer carries no command text");
		smuggled                   = answer;
		smuggled.Edit.Command.Stop = core::tour_stop_id{ 3 };
		expect(encode_tour_request(smuggled).empty(), "an answer names no target of its own");
		auto oversizedAnswer = answer;
		oversizedAnswer.Edit.Assist.ResultJson.assign(32769U, 'x');
		expect(encode_tour_request(oversizedAnswer).empty(), "an answer is bounded");
		auto chosen                   = answer;
		chosen.Edit.Operation         = application::tour_service_operation::outline_use;
		chosen.Edit.Assist.ResultJson = { };
		chosen.Edit.Assist.Indices    = { 0, 2 };
		chosen.Edit.Assist.Position   = 1;
		const auto decodedChoice      = decode_tour_request(encode_tour_request(chosen));
		expect(decodedChoice && decodedChoice->Edit.Assist.Indices == std::vector<std::size_t>{ 0, 2 } && decodedChoice->Edit.Assist.Position == 1, "an outline choice round-trips");
		chosen.Edit.Assist.Indices.assign(25U, 0);
		expect(encode_tour_request(chosen).empty(), "a choice is bounded");
		application::tour_owner_request hooks{ .Operation = tour_owner_operation::hooks_show };
		expect(!encode_tour_request(hooks).empty() && decode_tour_request(encode_tour_request(hooks)), "hooks are profile-wide and name no draft");
		hooks.Draft = draft;
		expect(encode_tour_request(hooks).empty(), "and may not be given one");
		const auto reply = decode_tour_response(response_bytes("revision_conflict"));
		expect(reply && reply->Error == "revision_conflict" && !reply->Draft, "a refusal response round-trips");
		expect(!decode_tour_response({ }), "an empty response is refused");
	}

	/// [[BPR-1168]]. A cycle command is a registry name and that entry's options; nothing else rides along.
	auto cycle_request(core::cycle_command aCommand = core::cycle_command::note) -> tour_owner_request
	{
		tour_owner_request result{ .Operation = tour_owner_operation::cycle };
		result.Cycle.Command    = aCommand;
		result.Cycle.Operation  = "op-1";
		result.Cycle.Step       = 2U;
		result.Cycle.Positional = { "RC-1" };
		result.Cycle.Options    = { { "--file", "src/a.cpp" }, { "--lines", "2:3" }, { "--body-file", "-" } };
		result.Cycle.Directory  = "/work/project";
		result.Cycle.Text       = "Cache this lookup.\n";
		return result;
	}

	void test_cycle_codec()
	{
		const auto request = cycle_request();
		const auto decoded = decode_tour_request(encode_tour_request(request));
		expect(decoded && decoded->Operation == tour_owner_operation::cycle && decoded->Cycle.Command == core::cycle_command::note && decoded->Cycle.Operation == "op-1" && decoded->Cycle.Step == 2U
				   && decoded->Cycle.Options == request.Cycle.Options && decoded->Cycle.Text == request.Cycle.Text && decoded->Cycle.Directory == "/work/project",
			   "a cycle command round-trips exactly");
		const auto refused = [](tour_owner_request aRequest, const char* aWhat) { expect(encode_tour_request(std::move(aRequest)).empty(), aWhat); };
		auto       changed = request;
		changed.Cycle.Options.emplace_back("--expected-revision", "3");
		refused(changed, "an option of another command is refused");
		changed = request;
		changed.Cycle.Options.emplace_back("--profile", "2");
		refused(changed, "the caller's own profile choice never reaches the owner");
		changed = request;
		changed.Cycle.Options.emplace_back("--yes", "true");
		refused(changed, "there is no bypass to carry");
		changed = request;
		changed.Cycle.Options.emplace_back("--file", "other");
		refused(changed, "a repeated option is refused");
		changed               = cycle_request(core::cycle_command::capture);
		changed.Cycle.Options = { { "--again", "yes" } };
		refused(changed, "a flag carries no value");
		changed                 = request;
		changed.Cycle.Operation = "op 1";
		refused(changed, "an operation identity is letters, digits and hyphens");
		changed.Cycle.Operation = "";
		refused(changed, "and is never empty");
		changed       = request;
		changed.Draft = draft;
		refused(changed, "a cycle command names no tour draft");
		changed                = request;
		changed.Edit.RequestId = "req-1";
		refused(changed, "nor a tour request");
		changed                  = request;
		changed.Cycle.Positional = { "RC-1", "RC-2" };
		refused(changed, "at most one cycle");
		changed            = request;
		changed.Cycle.Text = std::string(application::maximum_cycle_owner_text_bytes + 1U, 'x');
		refused(changed, "text is bounded");
		changed            = request;
		changed.Cycle.Text = std::string{ "bad\xff" };
		refused(changed, "text is UTF-8");
		changed                = request;
		changed.Cycle.Document = std::string(application::maximum_cycle_owner_document_bytes + 1U, 'x');
		refused(changed, "a response document is bounded");
		// [[BPR-1169]]. Protocol 4 carries the round commands, each with its own options only.
		auto reading           = cycle_request(core::cycle_command::round_read);
		reading.Cycle.Options  = { { "--round", "F1" }, { "--page-bytes", "4096" } };
		reading.Cycle.Text     = std::nullopt;
		const auto readDecoded = decode_tour_request(encode_tour_request(reading));
		expect(readDecoded && readDecoded->Cycle.Command == core::cycle_command::round_read && readDecoded->Cycle.Options == reading.Cycle.Options, "a round read round-trips");
		changed = reading;
		changed.Cycle.Options.emplace_back("--output", "/tmp/x");
		refused(changed, "a round read carries no export destination");
		auto granting          = cycle_request(core::cycle_command::round_grant);
		granting.Cycle.Options = { { "--round", "F1" }, { "--read-only", "true" } };
		expect(static_cast<bool>(decode_tour_request(encode_tour_request(granting))), "a grant round-trips");
		// [[BPR-1279]]. Protocol 5 carries rebind, with its location and confirmation only.
		auto rebinding           = cycle_request(core::cycle_command::rebind);
		rebinding.Cycle.Options  = { { "--to", "/work/moved" }, { "--confirm", "0123456789abcdef" } };
		rebinding.Cycle.Text     = std::nullopt;
		const auto rebindDecoded = decode_tour_request(encode_tour_request(rebinding));
		expect(rebindDecoded && rebindDecoded->Cycle.Command == core::cycle_command::rebind && rebindDecoded->Cycle.Options == rebinding.Cycle.Options, "a rebind round-trips");
		changed = rebinding;
		changed.Cycle.Options.emplace_back("--force", "true");
		refused(changed, "a rebind carries nothing that stands in for its confirmation");
		auto bytes = encode_tour_request(request);
		bytes[17]  = static_cast<std::uint8_t>(static_cast<std::uint8_t>(core::last_cycle_command) + 1U); // after the operation and two empty texts
		expect(!decode_tour_request(bytes), "an unknown cycle command does not decode");

		application::tour_owner_response answer;
		answer.Cycle        = application::cycle_owner_response{ .Operation = "op-1", .Step = 2U, .Ok = true, .Data = "{\"item\":\"I3\"}", .Human = "Noted I3.\n", .Output = "#" };
		const auto answered = decode_tour_response(encode_tour_response(answer));
		expect(answered && answered->Cycle && answered->Cycle->Operation == "op-1" && answered->Cycle->Step == 2U && answered->Cycle->Data == answer.Cycle->Data && answered->Cycle->Output == "#", "a cycle answer round-trips");
		answer.Cycle->Code = "conflict";
		expect(encode_tour_response(answer).empty(), "an answer is either a result or a refusal");
	}

	/// [[BPR-1227]]. A source read names one pinned destination and nothing else; its answer is the
	/// exact range and bounded text.
	void test_source_codec()
	{
		const auto commit = core::parse_git_commit_oid("1111111111111111111111111111111111111111");
		const auto blob   = core::parse_git_blob_oid("2222222222222222222222222222222222222222");
		expect(commit.has_value() && blob.has_value(), "fixture object IDs parse");
		const tour_owner_request request{ .Operation = tour_owner_operation::source, .Draft = draft, .Source = { .Commit = *commit, .Blob = blob, .Path = "src/\xce\xb1.cpp", .Side = core::diff_side::left, .FirstLine = 2U, .LastLine = 4U } };
		const auto               decoded = decode_tour_request(encode_tour_request(request));
		expect(decoded && decoded->Operation == tour_owner_operation::source && decoded->Draft == draft && decoded->Source == request.Source && decoded->Edit.RequestId.empty(), "a source request round-trips");
		const auto refused     = [](tour_owner_request aRequest, const char* aWhat) { expect(encode_tour_request(std::move(aRequest)).empty(), aWhat); };
		auto       changed     = request;
		changed.Edit.RequestId = "read-1";
		refused(changed, "a read carries no request ID, so it can leave no request record");
		changed = request;
		changed.Draft.clear();
		refused(changed, "a read names its exact draft");
		changed             = request;
		changed.Source.Path = "/etc/passwd";
		refused(changed, "an absolute path is no repository source");
		changed             = request;
		changed.Source.Path = "a/../b.cpp";
		refused(changed, "nor is a path that climbs");
		changed                  = request;
		changed.Source.FirstLine = 5U;
		refused(changed, "the range is ordered");
		changed               = request;
		changed.Source.Commit = { };
		refused(changed, "an unpinned read names no commit");
		auto bytes = encode_tour_request(load_request_value());
		expect(!bytes.empty(), "a load encodes");
		bytes[0] = static_cast<std::uint8_t>(static_cast<std::uint8_t>(tour_owner_operation::source) + 1U);
		expect(!decode_tour_request(bytes), "no operation after source decodes");

		const core::file_comparison_digest       digest{ std::array<std::uint8_t, 32>{ 7U } };
		application::tour_owner_response         answer;
		application::review_tour_source_material material;
		material.Source.First  = { digest, core::diff_side::left, 2U };
		material.Source.Last   = { digest, core::diff_side::left, 4U };
		material.Source.Commit = *commit;
		material.Source.Blob   = *blob;
		material.Source.Path   = "src/\xce\xb1.cpp";
		material.Text          = "one\ntwo\nthree\n";
		answer.Source          = std::move(material);
		const auto answered    = decode_tour_response(encode_tour_response(answer));
		expect(answered && answered->Source && answered->Source->Source == answer.Source->Source && answered->Source->Text == answer.Source->Text && answered->Error.empty(), "source material round-trips");
		auto broken         = answer;
		broken.Source->Text = "\xff";
		expect(encode_tour_response(broken).empty(), "material is UTF-8");
		broken              = answer;
		broken.Source->Text = std::string(core::maximum_tour_field_bytes + 1U, 'x');
		expect(encode_tour_response(broken).empty(), "material is bounded");
		broken                                = answer;
		broken.Source->Source.Last.Comparison = core::file_comparison_digest{ std::array<std::uint8_t, 32>{ 8U } };
		expect(encode_tour_response(broken).empty(), "material names one comparison");
	}

#if !defined(_WIN32)
	auto scratch_pattern() -> std::string
	{
		// POSIX-only, but the MSVC gate reads text rather than branches (BPR-0519).
	#if defined(_MSC_VER)
		#pragma warning(push)
		#pragma warning(disable : 4996)
	#endif
		const auto* socketRoot = std::getenv("BPR_TEST_SOCKET_ROOT");
	#if defined(_MSC_VER)
		#pragma warning(pop)
	#endif
		return (std::filesystem::path{ socketRoot ? socketRoot : "/tmp" } / "bpr-tour-XXXXXX").string();
	}

	class scratch_directory
	{
	public:
		scratch_directory()
		{
			auto pattern = scratch_pattern();
			expect(::mkdtemp(pattern.data()) != nullptr, "make private scratch");
			Path = std::filesystem::canonical(pattern);
		}
		~scratch_directory()
		{
			std::error_code ignored;
			if (const auto endpoint = profile_activation_endpoint(Path)) {
				std::filesystem::remove(tour_path(*endpoint), ignored);
			}
			std::filesystem::remove_all(Path, ignored);
		}
		scratch_directory(const scratch_directory&)                    = delete;
		auto operator=(const scratch_directory&) -> scratch_directory& = delete;
		/// \brief The tour endpoint is the activation rendezvous with its own fixed-length name.
		static auto tour_path(std::filesystem::path aActivation) -> std::filesystem::path
		{
			auto       name = aActivation.native();
			const auto at   = name.find("bpr-activate-");
			expect(at != std::string::npos, "the activation endpoint names its rendezvous");
			name.replace(at, 13, "bpr-tourcmd--");
			return name;
		}
		auto endpoint() const -> std::filesystem::path
		{
			const auto activation = profile_activation_endpoint(Path);
			expect(activation.has_value(), "the scratch root resolves an endpoint");
			return tour_path(*activation);
		}
		std::filesystem::path Path;
	};

	/// \brief One client on its own thread; the request blocks until the owner answers or closes.
	class client
	{
	public:
		client(const std::filesystem::path& aRoot, core::bpr_profile_id aProfile, tour_channel_bytes aPayload, std::stop_token aStop = { })
			: Thread{ [this, aRoot, aProfile, payload = std::move(aPayload), aStop] {
				Result = send_tour_owner(aRoot, aProfile, payload, aStop);
				Done   = true;
			} }
		{ }
		auto wait() -> const std::expected<tour_channel_bytes, tour_channel_failure>&
		{
			Thread.join();
			return Result;
		}
		std::atomic<bool> Done{ false };

	private:
		std::expected<tour_channel_bytes, tour_channel_failure> Result{ std::unexpected(tour_channel_failure::unavailable) };
		std::thread                                             Thread;
	};

	auto receive(application::tour_owner_server& aServer, std::chrono::milliseconds aWithin = std::chrono::seconds{ 3 }) -> std::optional<application::tour_owner_message>
	{
		const auto until = std::chrono::steady_clock::now() + aWithin;
		while (std::chrono::steady_clock::now() < until) {
			if (auto message = aServer.receive()) {
				return message;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
		}
		return std::nullopt;
	}

	void pump_for(application::tour_owner_server& aServer, std::chrono::milliseconds aFor)
	{
		const auto until = std::chrono::steady_clock::now() + aFor;
		while (std::chrono::steady_clock::now() < until) {
			expect(!aServer.receive(), "nothing is admitted while pumping a hostile or idle peer");
			std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
		}
	}

	void test_authority()
	{
		scratch_directory root, other;
		auto              lease = acquire_profile_runtime_lease(root.Path);
		expect(lease.has_value(), "acquire the profile lease");
		expect(!make_tour_owner_server(root.Path, profile, { }), "a null lease is refused");
		expect(!make_tour_owner_server(root.Path, profile, std::make_shared<application::profile_runtime_lease>()), "a forged lease is refused");
		expect(!make_tour_owner_server(other.Path, profile, *lease), "another root's lease is refused");
		expect(!make_tour_owner_server(root.Path, core::bpr_profile_id{ }, *lease), "the zero profile is refused");
		auto server = make_tour_owner_server(root.Path, profile, *lease);
		expect(server.has_value(), "the admitted owner binds its tour endpoint");
		expect(!make_tour_owner_server(root.Path, profile, *lease), "a duplicate listener is refused");
		expect(std::filesystem::exists(root.endpoint()), "the endpoint is named separately from activation");
		lease->reset();
		expect(!acquire_profile_runtime_lease(root.Path), "the server retains the lease");
		server->reset();
		expect(!std::filesystem::exists(root.endpoint()), "the socket is removed when the owner drains");
		expect(acquire_profile_runtime_lease(root.Path).has_value(), "the lease is released after the server");
		const auto missing = send_tour_owner(root.Path, profile, load_request());
		expect(!missing && missing.error() == tour_channel_failure::unavailable, "a missing owner is unavailable");
	}

	void test_round_trip()
	{
		scratch_directory root;
		auto              lease  = acquire_profile_runtime_lease(root.Path);
		auto              server = make_tour_owner_server(root.Path, profile, *lease);
		expect(lease && server, "owner endpoint");
		client caller{ root.Path, profile, load_request() };
		auto   message = receive(**server);
		expect(message.has_value(), "the request arrives");
		const auto decoded = decode_tour_request(message->Payload);
		expect(decoded && decoded->Draft == draft && decoded->Operation == tour_owner_operation::load, "the admitted bytes are the exact request");
		expect(!(*server)->receive(), "one pending request bounds the owner");
		message->Reply->complete(response_bytes("target_ambiguous"));
		const auto& result = caller.wait();
		expect(result.has_value(), "the answer reaches the caller");
		const auto reply = decode_tour_response(*result);
		expect(reply && reply->Error == "target_ambiguous", "the answer is the exact owner response");
		client second{ root.Path, profile, load_request() };
		auto   next = receive(**server);
		expect(next.has_value(), "the owner serves the next caller after the first completes");
		next->Reply->complete(response_bytes("second"));
		expect(second.wait().has_value(), "second caller answered");
	}

	void test_binding()
	{
		scratch_directory root, other;
		auto              lease  = acquire_profile_runtime_lease(root.Path);
		auto              server = make_tour_owner_server(root.Path, profile, *lease);
		expect(lease && server, "owner endpoint");
		// A caller that names another profile is refused by the owner's advertised binding and sends nothing.
		client wrongProfile{ root.Path, core::bpr_profile_id{ 2U }, load_request() };
		pump_for(**server, std::chrono::milliseconds{ 100 });
		const auto& refused = wrongProfile.wait();
		expect(!refused && refused.error() == tour_channel_failure::refused, "the wrong profile is refused before any payload");
		// A caller resolving another root finds no endpoint at all.
		const auto elsewhere = send_tour_owner(other.Path, profile, load_request());
		expect(!elsewhere && elsewhere.error() == tour_channel_failure::unavailable, "another root has no endpoint");
		// Malformed payloads never leave the caller.
		expect(send_tour_owner(root.Path, profile, { }).error() == tour_channel_failure::refused, "an empty payload is refused locally");
		expect(send_tour_owner(root.Path, profile, tour_channel_bytes(application::maximum_tour_channel_bytes + 1U, 1)).error() == tour_channel_failure::refused, "an oversized payload is refused locally");
		tour_channel_bytes junk(64U, 7);
		expect(send_tour_owner(root.Path, profile, junk).error() == tour_channel_failure::refused, "an undecodable payload is refused locally");
		expect(!(*server)->receive(), "none of it reached the owner");
	}

	auto raw_connect(const std::filesystem::path& aEndpoint) -> int
	{
		const int socket = ::socket(AF_UNIX, SOCK_STREAM, 0);
		expect(socket >= 0, "raw socket");
		sockaddr_un address{ };
		address.sun_family = AF_UNIX;
		const auto path    = aEndpoint.string();
		expect(path.size() < sizeof(address.sun_path), "the endpoint fits sun_path");
		std::copy(path.begin(), path.end(), address.sun_path);
		expect(::connect(socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0, "raw connect");
		return socket;
	}

	void test_hostile_framing()
	{
		scratch_directory root;
		auto              lease  = acquire_profile_runtime_lease(root.Path);
		auto              server = make_tour_owner_server(root.Path, profile, *lease);
		expect(lease && server, "owner endpoint");
		const auto readHello = [&](int aSocket) {
			std::array<std::uint8_t, 64> hello{ };
			std::size_t                  have  = 0;
			const auto                   until = std::chrono::steady_clock::now() + std::chrono::seconds{ 2 };
			while (have < hello.size() && std::chrono::steady_clock::now() < until) {
				(void)(*server)->receive();
				const auto read = ::recv(aSocket, hello.data() + have, hello.size() - have, MSG_DONTWAIT);
				if (read > 0) {
					have += static_cast<std::size_t>(read);
				} else {
					std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
				}
			}
			expect(have == hello.size(), "the owner advertises its 64-byte binding");
			return hello;
		};
		const auto sendAll = [&](int aSocket, const std::vector<std::uint8_t>& aBytes) { expect(::send(aSocket, aBytes.data(), aBytes.size(), 0) == static_cast<ssize_t>(aBytes.size()), "hostile send"); };
		const auto frame   = [&](const std::array<std::uint8_t, 64>& aHello, std::uint64_t aLength, std::size_t aBody) {
			std::vector<std::uint8_t> bytes{ aHello.begin(), aHello.end() };
			for (unsigned i = 0; i < 8; ++i) {
				bytes.push_back(static_cast<std::uint8_t>(aLength >> (i * 8)));
			}
			bytes.insert(bytes.end(), aBody, 0xAA);
			return bytes;
		};
		// Wrong binding: a frame that does not echo the owner's advertisement is dropped.
		{
			const auto socket = raw_connect(root.endpoint());
			auto       hello  = readHello(socket);
			auto       forged = hello;
			forged[20]        = forged[20] == 0 ? 1 : 0;
			sendAll(socket, frame(forged, 4, 4));
			::shutdown(socket, SHUT_WR);
			pump_for(**server, std::chrono::milliseconds{ 50 });
			::close(socket);
		}
		// Oversized declared length.
		{
			const auto socket = raw_connect(root.endpoint());
			const auto hello  = readHello(socket);
			sendAll(socket, frame(hello, application::maximum_tour_channel_bytes + 1U, 0));
			::shutdown(socket, SHUT_WR);
			pump_for(**server, std::chrono::milliseconds{ 50 });
			::close(socket);
		}
		// Zero length.
		{
			const auto socket = raw_connect(root.endpoint());
			const auto hello  = readHello(socket);
			sendAll(socket, frame(hello, 0, 0));
			::shutdown(socket, SHUT_WR);
			pump_for(**server, std::chrono::milliseconds{ 50 });
			::close(socket);
		}
		// Declared length shorter than the bytes sent.
		{
			const auto socket = raw_connect(root.endpoint());
			const auto hello  = readHello(socket);
			sendAll(socket, frame(hello, 8, 32));
			::shutdown(socket, SHUT_WR);
			pump_for(**server, std::chrono::milliseconds{ 50 });
			::close(socket);
		}
		// Well framed but not a tour request: command confusion (an activation-shaped body).
		{
			const auto socket = raw_connect(root.endpoint());
			const auto hello  = readHello(socket);
			sendAll(socket, frame(hello, 8, 8));
			::shutdown(socket, SHUT_WR);
			pump_for(**server, std::chrono::milliseconds{ 50 });
			::close(socket);
		}
		// Truncated at EOF: the declared body never completes.
		{
			const auto socket = raw_connect(root.endpoint());
			const auto hello  = readHello(socket);
			sendAll(socket, frame(hello, 64, 10));
			::shutdown(socket, SHUT_WR);
			pump_for(**server, std::chrono::milliseconds{ 50 });
			::close(socket);
		}
		// The owner still serves an honest caller afterwards.
		client caller{ root.Path, profile, load_request() };
		auto   message = receive(**server);
		expect(message.has_value(), "hostile peers did not wedge the endpoint");
		message->Reply->complete(response_bytes("ok"));
		expect(caller.wait().has_value(), "honest caller answered");
		// A stalled peer that never sends a frame expires and frees the endpoint.
		const auto stalled = raw_connect(root.endpoint());
		pump_for(**server, std::chrono::milliseconds{ 200 });
		pump_for(**server, std::chrono::milliseconds{ 5100 });
		::close(stalled);
		client after{ root.Path, profile, load_request() };
		auto   again = receive(**server);
		expect(again.has_value(), "the stalled peer expired");
		again->Reply->complete(response_bytes("ok"));
		expect(after.wait().has_value(), "caller after the stall answered");
	}

	void test_version_mismatch()
	{
		scratch_directory root;
		auto              lease = acquire_profile_runtime_lease(root.Path);
		expect(lease.has_value(), "lease");
		// A listener that advertises another protocol version stands in for a newer or older owner.
		const auto path   = root.endpoint();
		const int  listen = ::socket(AF_UNIX, SOCK_STREAM, 0);
		expect(listen >= 0, "create the stand-in owner socket");
		sockaddr_un address{ };
		address.sun_family = AF_UNIX;
		const auto named   = path.string();
		std::copy(named.begin(), named.end(), address.sun_path);
		expect(::bind(listen, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0 && ::chmod(path.c_str(), 0600) == 0 && ::listen(listen, 1) == 0, "bind the stand-in owner");
		client    caller{ root.Path, profile, load_request() };
		const int accepted = ::accept(listen, nullptr, nullptr);
		expect(accepted >= 0, "the caller connects");
		// Version 5 is the owner before the tour source read (BPR-1227).
		std::array<std::uint8_t, 64> hello{ 'B', 'P', 'R', 'T', 5 };
		expect(::send(accepted, hello.data(), hello.size(), 0) == 64, "advertise version 5");
		const auto& result = caller.wait();
		expect(!result && result.error() == tour_channel_failure::version_mismatch, "another protocol version is named as such, before a payload is sent");
		std::array<std::uint8_t, 1> probe{ };
		expect(::recv(accepted, probe.data(), 1, MSG_DONTWAIT) <= 0, "no request bytes reached the mismatched owner");
		::close(accepted);
		::close(listen);
		std::filesystem::remove(path);
	}

	void test_stale_and_hostile_endpoints()
	{
		scratch_directory root;
		const auto        path = root.endpoint();
		{
			// A crashed owner leaves its socket behind with nothing listening.
			const int listen = ::socket(AF_UNIX, SOCK_STREAM, 0);
			expect(listen >= 0, "create the stand-in owner socket");
			sockaddr_un address{ };
			address.sun_family = AF_UNIX;
			const auto named   = path.string();
			std::copy(named.begin(), named.end(), address.sun_path);
			expect(::bind(listen, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0 && ::chmod(path.c_str(), 0600) == 0, "leave a stale socket");
			::close(listen);
		}
		expect(std::filesystem::exists(path), "the stale endpoint exists");
		const auto stale = send_tour_owner(root.Path, profile, load_request());
		expect(!stale && stale.error() == tour_channel_failure::unavailable, "a stale endpoint is unavailable, not a delivery");
		auto lease = acquire_profile_runtime_lease(root.Path);
		expect(lease.has_value(), "the crashed owner's lease is gone");
		auto server = make_tour_owner_server(root.Path, profile, *lease);
		expect(server.has_value(), "the next owner replaces a stale rendezvous");
		client caller{ root.Path, profile, load_request() };
		auto   message = receive(**server);
		expect(message.has_value(), "the replaced endpoint serves");
		message->Reply->complete(response_bytes("ok"));
		expect(caller.wait().has_value(), "answered");
		server->reset();
		expect(!std::filesystem::exists(path), "the socket is removed on drain");
		// A symlink or regular file at the rendezvous is hostile: both sides refuse and preserve it.
		std::filesystem::create_symlink(root.Path, path);
		expect(!make_tour_owner_server(root.Path, profile, *lease), "a symlink endpoint is refused by the owner");
		expect(std::filesystem::is_symlink(path), "the refusal preserves the hostile entry");
		expect(!send_tour_owner(root.Path, profile, load_request()), "the caller refuses a symlink endpoint");
		expect(std::filesystem::remove(path), "the fixture removes its own hostile entry");
		{
			std::ofstream file{ path };
			file << "not a socket";
		}
		expect(!make_tour_owner_server(root.Path, profile, *lease), "a regular file endpoint is refused by the owner");
		expect(!send_tour_owner(root.Path, profile, load_request()), "the caller refuses a regular file endpoint");
		expect(std::filesystem::exists(path), "the refusal preserves the file");
	}

	void test_owner_death_and_cancellation()
	{
		scratch_directory root;
		auto              lease  = acquire_profile_runtime_lease(root.Path);
		auto              server = make_tour_owner_server(root.Path, profile, *lease);
		expect(lease && server, "owner endpoint");
		// The owner accepts the request and dies (drains) before replying: delivery is uncertain.
		client lost{ root.Path, profile, load_request() };
		auto   message = receive(**server);
		expect(message.has_value(), "the request arrives");
		message.reset();
		server->reset();
		const auto& outcome = lost.wait();
		expect(!outcome && outcome.error() == tour_channel_failure::indeterminate, "an owner lost after admission is indeterminate, never a retry signal");
		expect(!std::filesystem::exists(root.endpoint()), "the dead owner's endpoint was removed");
		// An owner that abandons a reply object is uncertain too.
		server = make_tour_owner_server(root.Path, profile, *lease);
		expect(server.has_value(), "the next owner");
		client abandoned{ root.Path, profile, load_request() };
		auto   again = receive(**server);
		expect(again.has_value(), "the request arrives at the next owner");
		again->Reply->complete({ });
		const auto& closed = abandoned.wait();
		expect(!closed && closed.error() == tour_channel_failure::indeterminate, "an empty completion is a close without an answer");
		// Cancellation before delivery is a clean, effect-free cancel.
		std::stop_source stopped;
		stopped.request_stop();
		const auto cancelled = send_tour_owner(root.Path, profile, load_request(), stopped.get_token());
		expect(!cancelled && cancelled.error() == tour_channel_failure::cancelled, "cancellation before send is cancelled");
		// Cancellation while awaiting the answer cannot claim the request was not delivered.
		std::stop_source waiting;
		client           interrupted{ root.Path, profile, load_request(), waiting.get_token() };
		auto             pending = receive(**server);
		expect(pending.has_value(), "the request arrives");
		waiting.request_stop();
		const auto& uncertain = interrupted.wait();
		expect(!uncertain && uncertain.error() == tour_channel_failure::indeterminate, "cancellation after delivery is uncertain");
		pending->Reply->complete(response_bytes("late"));
	}
#else
	// [[BPR-1168]]. The Windows owner command pipe: the same authority, binding,
	// version, framing and lifetime as the POSIX socket, with a cycle command
	// carried end to end. There is no stale-endpoint course because a pipe dies
	// with the process that bound it; the name is shown bindable again instead.
	class scratch_directory
	{
	public:
		scratch_directory()
			: Root{ "bpr-tourpipe" }
			, Path{ std::filesystem::canonical(Root.path()) }
		{ }
		tests::sqlite_contract::scratch_root Root;
		std::filesystem::path                Path;
	};

	class client
	{
	public:
		client(const std::filesystem::path& aRoot, core::bpr_profile_id aProfile, tour_channel_bytes aPayload, std::stop_token aStop = { })
			: Thread{ [this, aRoot, aProfile, payload = std::move(aPayload), aStop] { Result = send_tour_owner(aRoot, aProfile, payload, aStop); } }
		{ }
		auto wait() -> const std::expected<tour_channel_bytes, tour_channel_failure>&
		{
			Thread.join();
			return Result;
		}

	private:
		std::expected<tour_channel_bytes, tour_channel_failure> Result{ std::unexpected(tour_channel_failure::unavailable) };
		std::thread                                             Thread;
	};

	auto receive(application::tour_owner_server& aServer, std::chrono::milliseconds aWithin = std::chrono::seconds{ 5 }) -> std::optional<application::tour_owner_message>
	{
		const auto until = std::chrono::steady_clock::now() + aWithin;
		while (std::chrono::steady_clock::now() < until) {
			if (auto message = aServer.receive()) {
				return message;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
		}
		return std::nullopt;
	}

	void test_windows_pipe()
	{
		const scratch_directory root;
		const scratch_directory other;
		auto                    lease = acquire_profile_runtime_lease(root.Path);
		expect(lease.has_value(), "acquire the profile lease");
		expect(!make_tour_owner_server(root.Path, profile, { }), "a null lease is refused");
		expect(!make_tour_owner_server(root.Path, profile, std::make_shared<application::profile_runtime_lease>()), "a forged lease is refused");
		expect(!make_tour_owner_server(other.Path, profile, *lease), "another root's lease is refused");
		const auto missing = send_tour_owner(root.Path, profile, load_request());
		expect(!missing && missing.error() == tour_channel_failure::unavailable, "no owner, no endpoint");
		auto server = make_tour_owner_server(root.Path, profile, *lease);
		expect(server.has_value(), "the admitted owner binds its command pipe");
		const auto endpoint = tour_owner_endpoint(root.Path);
		expect(endpoint && endpoint->wstring().starts_with(L"\\\\.\\pipe\\bpr-tourcmd--"), "a local named pipe beside activation");
		expect(!make_tour_owner_server(root.Path, profile, *lease), "a duplicate listener is refused");

		// A tour request and its answer.
		{
			client caller{ root.Path, profile, load_request() };
			auto   message = receive(**server);
			expect(message.has_value(), "the request arrives");
			const auto decoded = decode_tour_request(message->Payload);
			expect(decoded && decoded->Draft == draft, "the admitted bytes are the exact request");
			expect(!(*server)->receive(), "one pending request bounds the owner");
			message->Reply->complete(encode_tour_response({ .Error = "target_ambiguous" }));
			const auto& answer = caller.wait();
			const auto  reply  = answer ? decode_tour_response(*answer) : std::nullopt;
			expect(reply && reply->Error == "target_ambiguous", "the answer reaches the caller");
		}
		// A cycle command larger than one pipe buffer, both ways.
		{
			tour_owner_request request{ .Operation = tour_owner_operation::cycle };
			request.Cycle.Command   = core::cycle_command::response_import;
			request.Cycle.Operation = "pipe-1";
			request.Cycle.Options   = { { "--round", "F1" }, { "--file", "-" } };
			request.Cycle.Document  = std::string(300U * 1024U, 'r');
			client caller{ root.Path, profile, encode_tour_request(request) };
			auto   message = receive(**server);
			expect(message.has_value(), "the large request arrives whole");
			const auto decoded = decode_tour_request(message->Payload);
			expect(decoded && decoded->Cycle.Document == request.Cycle.Document, "every byte of it");
			application::tour_owner_response answer;
			answer.Cycle = application::cycle_owner_response{ .Operation = "pipe-1", .Ok = true, .Data = "{}", .Output = std::string(400U * 1024U, 'o') };
			message->Reply->complete(encode_tour_response(answer));
			const auto& bytes = caller.wait();
			const auto  reply = bytes ? decode_tour_response(*bytes) : std::nullopt;
			expect(reply && reply->Cycle && reply->Cycle->Output == answer.Cycle->Output, "and the large answer");
		}
		// A caller naming another profile is refused by the advertised binding and sends nothing.
		{
			client     caller{ root.Path, core::bpr_profile_id{ 2U }, load_request() };
			const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{ 500 };
			while (std::chrono::steady_clock::now() < deadline) {
				expect(!(*server)->receive(), "nothing from the wrong profile is admitted");
				std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
			}
			const auto& refused = caller.wait();
			expect(!refused && refused.error() == tour_channel_failure::refused, "the wrong profile is refused");
		}
		// The owner drains before answering: the delivery is uncertain, never a retry signal.
		{
			client caller{ root.Path, profile, load_request() };
			auto   message = receive(**server);
			expect(message.has_value(), "the request arrives");
			server->reset();
			message.reset();
			const auto& lost = caller.wait();
			expect(!lost && lost.error() == tour_channel_failure::indeterminate, "an owner lost after admission is indeterminate");
		}
		// Cancellation before delivery is a clean cancel.
		{
			std::stop_source stop;
			stop.request_stop();
			const auto cancelled = send_tour_owner(root.Path, profile, load_request(), stop.get_token());
			expect(!cancelled && cancelled.error() == tour_channel_failure::cancelled, "cancelled before send");
		}
		lease->reset();
		expect(acquire_profile_runtime_lease(root.Path).has_value(), "the lease is released after the server");
		lease       = acquire_profile_runtime_lease(root.Path);
		auto rebind = make_tour_owner_server(root.Path, profile, *lease);
		expect(rebind.has_value(), "the drained name binds again");
	}
#endif
}

int main()
{
	try {
		std::cout << "test_codec" << '\n'
				  << std::flush;
		test_codec();
		test_cycle_codec();
		std::cout << "test_source_codec" << '\n'
				  << std::flush;
		test_source_codec();
#if defined(_WIN32)
		std::cout << "test_windows_pipe" << '\n'
				  << std::flush;
		test_windows_pipe();
#else
		std::cout << "test_authority" << '\n'
				  << std::flush;
		test_authority();
		std::cout << "test_round_trip" << '\n'
				  << std::flush;
		test_round_trip();
		std::cout << "test_binding" << '\n'
				  << std::flush;
		test_binding();
		std::cout << "test_hostile_framing" << '\n'
				  << std::flush;
		test_hostile_framing();
		std::cout << "test_version_mismatch" << '\n'
				  << std::flush;
		test_version_mismatch();
		std::cout << "test_stale_and_hostile_endpoints" << '\n'
				  << std::flush;
		test_stale_and_hostile_endpoints();
		std::cout << "test_owner_death_and_cancellation" << '\n'
				  << std::flush;
		test_owner_death_and_cancellation();
#endif
	} catch (const expectation_failed&) {
		return EXIT_FAILURE;
	} catch (const std::exception& aError) {
		std::cerr << "FAILED: unexpected exception: " << aError.what() << '\n';
		return EXIT_FAILURE;
	} catch (...) {
		std::cerr << "FAILED: unexpected exception\n";
		return EXIT_FAILURE;
	}
	std::cout << "tour-owner-channel-tests:pass\n";
}
