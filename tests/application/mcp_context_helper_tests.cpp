/// \file
/// \brief Hostile stdio canary for the read-only MCP context helper.
///
/// \details Every assertion here drives the shipped executable over real
/// newline-delimited JSON-RPC 2.0, not an in-process object. The point of the
/// helper is what a separate process can and cannot reach, and an in-process
/// call could not tell the difference.

#include "bpr/adapters/mcp/mcp_context_helper.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#if !defined(_WIN32)
	#include <sys/wait.h>
#endif

namespace {
	using namespace bpr;
	namespace mcp = bpr::adapters::mcp;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	/// \brief The three credentials the parent holds for every launch below.
	constexpr std::array<std::pair<std::string_view, std::string_view>, 3U>
		planted_secrets{ {
			{ "GITHUB_TOKEN", "ghp_the_reviewers_own_authority" },
			{ "ANTHROPIC_API_KEY", "sk-ant-the-reviewers-own-account" },
			{ "AWS_SECRET_ACCESS_KEY", "aws-the-reviewers-own-account" },
		} };

	void set_environment(const std::string& aName, const std::string& aValue)
	{
#if defined(_WIN32)
		expect(
			::_putenv_s(aName.c_str(), aValue.c_str()) == 0,
			"the parent environment accepts the value under test");
#else
		expect(
			::setenv(aName.c_str(), aValue.c_str(), 1) == 0,
			"the parent environment accepts the value under test");
#endif
	}

	[[nodiscard]] auto environment_value(const std::string& aName) -> std::string
	{
		// `std::getenv` is the only portable spelling this Qt-free suite can
		// use; the MSVC CRT deprecates it in favour of the nonportable
		// `_dupenv_s`. The suppression covers exactly this read, as
		// tests/fixtures/agent/agent_canary_main.cpp does (BPR-0519).
#if defined(_MSC_VER)
	#pragma warning(push)
	#pragma warning(disable : 4996)
#endif
		const auto* const value = std::getenv(aName.c_str());
#if defined(_MSC_VER)
	#pragma warning(pop)
#endif
		return value == nullptr ? std::string{ } : std::string{ value };
	}

	/// \brief The canary's scratch directory, which this process creates and
	/// removes.
	///
	/// \details [[BPR-1051]]. The name is random and the directory is created
	/// exclusively, so it belongs to this run. A name that already exists is
	/// passed over and never removed: two runs that start in the same second
	/// used to delete each other's scratch. On POSIX it is owner-private; on
	/// Windows it keeps the per-user temporary directory's entries. `main`
	/// removes it before it reports a pass. A failed `expect` leaves through
	/// `std::exit`, which runs this function-local static's destructor, so a
	/// failing run removes it too. Only a run that is killed leaves it.
	class canary_scratch final
	{
	public:
		canary_scratch()
		{
			const auto parent = std::filesystem::temp_directory_path();
			auto       random = std::random_device{ };
			for (auto attempt = 0; attempt < 16 && Root.empty(); ++attempt) {
				const auto      name      = (static_cast<std::uint64_t>(random()) << 32U) | random();
				const auto      candidate = parent / ("bpr-mcp-canary-" + std::to_string(name));
				std::error_code filesystemError;
				if (std::filesystem::create_directory(candidate, filesystemError)) {
					Root = candidate;
				}
			}
			expect(!Root.empty(), "the canary creates its own scratch directory");
#if !defined(_WIN32)
			std::error_code filesystemError;
			std::filesystem::permissions(Root, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace, filesystemError);
			if (filesystemError) {
				static_cast<void>(remove());
			}
			expect(!filesystemError, "the canary's scratch directory is owner-private");
#endif
		}
		~canary_scratch() { static_cast<void>(remove()); }
		canary_scratch(const canary_scratch&)                    = delete;
		auto operator=(const canary_scratch&) -> canary_scratch& = delete;

		[[nodiscard]] auto root() const -> const std::filesystem::path& { return Root; }

		/// \returns Whether the directory this process created is gone.
		[[nodiscard]] auto remove() noexcept -> bool
		{
			if (Root.empty()) {
				return true;
			}
			std::error_code filesystemError;
			std::filesystem::remove_all(Root, filesystemError);
			if (filesystemError || std::filesystem::exists(Root, filesystemError)) {
				return false;
			}
			Root.clear();
			return true;
		}

	private:
		std::filesystem::path Root;
	};

	[[nodiscard]] auto scratch() -> canary_scratch&
	{
		static auto directory = canary_scratch{ };
		return directory;
	}

	[[nodiscard]] auto scratch_root() -> const std::filesystem::path&
	{
		return scratch().root();
	}

	[[nodiscard]] auto scratch_path(const std::string& aName)
		-> std::filesystem::path
	{
		static auto counter = 0U;
		++counter;
		return scratch_root() / (std::to_string(counter) + "-" + aName);
	}

	void write_file(const std::filesystem::path& aPath, const std::string& aText)
	{
		std::error_code filesystemError;
		std::filesystem::create_directories(aPath.parent_path(), filesystemError);
		auto output = std::ofstream{ aPath, std::ios::binary | std::ios::trunc };
		expect(output.is_open(), "the canary writes its own fixture files");
		output << aText;
		output.close();
		expect(!output.fail(), "the canary fixture file is complete on disk");
	}

	[[nodiscard]] auto read_file(const std::filesystem::path& aPath) -> std::string
	{
		auto input = std::ifstream{ aPath, std::ios::binary };
		return std::string{
			std::istreambuf_iterator<char>{ input },
			std::istreambuf_iterator<char>{ },
		};
	}

	[[nodiscard]] auto split_lines(const std::string& aText)
		-> std::vector<std::string>
	{
		auto lines    = std::vector<std::string>{ };
		auto position = std::size_t{ 0U };
		while (position < aText.size()) {
			const auto next = aText.find('\n', position);
			if (next == std::string::npos) {
				lines.push_back(aText.substr(position));
				break;
			}
			lines.push_back(aText.substr(position, next - position));
			position = next + 1U;
		}
		// A Windows text-mode stream writes the helper's '\n' as "\r\n", and
		// the file is read back in binary, so each line would carry a
		// trailing carriage return that no allowlist entry has (BPR-0522).
		for (auto& line : lines) {
			if (!line.empty() && line.back() == '\r') {
				line.pop_back();
			}
		}
		return lines;
	}

	struct helper_run
	{
		std::string StandardOutput;
		std::string StandardError;
		int         ExitCode = -1;
	};

	/// \brief Runs the shipped helper over one batch of request lines.
	///
	/// \details Requests arrive on stdin from a file and responses are read
	/// from the child's real stdout, so nothing here can answer a request the
	/// helper did not answer. The parent's environment - planted credentials
	/// included - is inherited exactly as a careless host would leave it.
	[[nodiscard]] auto run_helper(
		const std::string&              aRequests,
		const std::vector<std::string>& aArguments = { }) -> helper_run
	{
		const auto requestPath = scratch_path("requests.jsonl");
		const auto errorPath   = scratch_path("stderr.txt");
		write_file(requestPath, aRequests);

		auto command = std::string{ "\"" } + BPR_MCP_HELPER_EXECUTABLE + "\"";
		for (const auto& argument : aArguments) {
			command += " " + argument;
		}
		command += " < \"" + requestPath.string() + "\" 2> \""
			+ errorPath.string() + "\"";

#if defined(_WIN32)
		// `_popen` hands the whole string to `cmd.exe /c`, whose quoting rule
		// strips the outer quote pair from a string that begins with one and
		// carries more quotes, mangling the executable path and both
		// redirections so the helper never starts. Wrapping the entire
		// command in one more pair is cmd's documented remedy (BPR-0522).
		command          = "\"" + command + "\"";
		auto* const pipe = ::_popen(command.c_str(), "r");
#else
		// The test's whole job is to drive the shipped helper through a pipe;
		// the command is the quoted path it computed above, never input.
		// NOLINTNEXTLINE(bugprone-command-processor)
		auto* const pipe = ::popen(command.c_str(), "r");
#endif
		expect(pipe != nullptr, "the shipped MCP helper starts");
		auto output = std::string{ };
		auto buffer = std::array<char, 4096U>{ };
		while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)
			   != nullptr) {
			output += buffer.data();
		}
#if defined(_WIN32)
		const auto status = ::_pclose(pipe);
		const auto code   = status;
#else
		const auto status = ::pclose(pipe);
		const auto code   = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
		return {
			.StandardOutput = output,
			.StandardError  = read_file(errorPath),
			.ExitCode       = code,
		};
	}

	[[nodiscard]] auto json_quote(const std::string_view aValue) -> std::string
	{
		auto output = std::string{ "\"" };
		for (const auto character : aValue) {
			if (character == '"' || character == '\\') {
				output.push_back('\\');
			}
			output.push_back(character);
		}
		output.push_back('"');
		return output;
	}

	[[nodiscard]] auto tool_call(
		const std::size_t aId, const std::string_view aTool) -> std::string
	{
		return std::string{ R"({"jsonrpc":"2.0","id":)" } + std::to_string(aId)
			+ R"(,"method":"tools/call","params":{"name":)" + json_quote(aTool)
			+ ",\"arguments\":{}}}";
	}

	[[nodiscard]] auto tool_call_with_path(
		const std::size_t      aId,
		const std::string_view aTool,
		const std::string_view aPath) -> std::string
	{
		return std::string{ R"({"jsonrpc":"2.0","id":)" } + std::to_string(aId)
			+ R"(,"method":"tools/call","params":{"name":)" + json_quote(aTool)
			+ R"(,"arguments":{"path":)" + json_quote(aPath) + "}}}";
	}

	[[nodiscard]] auto raw_method(
		const std::size_t aId, const std::string_view aMethod) -> std::string
	{
		return std::string{ R"({"jsonrpc":"2.0","id":)" } + std::to_string(aId)
			+ R"(,"method":)" + json_quote(aMethod) + R"(,"params":{}})";
	}

	/// \brief The exact typed refusal one response carries, or nothing.
	[[nodiscard]] auto refusal_of(const std::string& aResponse) -> std::string
	{
		constexpr std::string_view marker = "\"refusal\":\"";
		const auto                 begin  = aResponse.find(marker);
		if (begin == std::string::npos) {
			return { };
		}
		const auto start = begin + marker.size();
		const auto end   = aResponse.find('"', start);
		if (end == std::string::npos) {
			return { };
		}
		return aResponse.substr(start, end - start);
	}

	[[nodiscard]] auto carries_result(const std::string& aResponse) -> bool
	{
		return aResponse.find("\"result\":") != std::string::npos;
	}

	[[nodiscard]] auto commit(const char aDigit) -> core::git_commit_oid
	{
		const auto parsed = core::parse_git_commit_oid(std::string(40U, aDigit));
		expect(parsed.has_value(), "the fixture commit identifier parses");
		return *parsed;
	}

	[[nodiscard]] auto pinned_binding() -> core::agent_revision_binding
	{
		return {
			.Host        = core::git_host_id{ 10U },
			.Repository  = core::repository_id{ 20U },
			.PullRequest = core::pull_request_id{ 30U },
			.BaseOid     = commit('1'),
			.HeadOid     = commit('2'),
			.LeftEpoch   = core::epoch_ordinal{ 1 },
			.RightEpoch  = core::epoch_ordinal{ 2 },
		};
	}

	[[nodiscard]] auto identity(const char aDigit) -> std::string
	{
		return std::string(64U, aDigit);
	}

	/// \brief The frozen projection, its binding record, and an escape hatch.
	///
	/// \details `files/escape` is a symlink out of the snapshot. It exists so
	/// the refusal sweep measures a real symlink escape rather than a lexical
	/// one: `..` normalizes away, a symlink does not.
	struct snapshot_fixture
	{
		std::filesystem::path Root;
		std::filesystem::path Outside;
		bool                  SymlinkAvailable = false;
	};

	[[nodiscard]] auto build_snapshot() -> snapshot_fixture
	{
		auto fixture    = snapshot_fixture{ };
		fixture.Root    = scratch_path("snapshot");
		fixture.Outside = scratch_path("outside");

		write_file(
			fixture.Root / "binding",
			mcp::encode_revision_binding(pinned_binding()) + "\n");
		write_file(
			fixture.Root / "alignment",
			std::string{ "file\tsrc/retry.cpp\ncomparison\t" } + identity('a')
				+ "\nleft_line_count\t42\nright_line_count\t44\nsegment\t"
				+ identity('b') + "\ngroup\t" + identity('c') + "\nmove\t"
				+ identity('d') + "\n");
		write_file(
			fixture.Root / "files" / "src" / "retry.cpp", "return retry();\n");
		write_file(
			fixture.Outside / "secret.txt",
			"outside-the-snapshot-and-nobody-asked\n");

		std::error_code filesystemError;
		std::filesystem::create_directory_symlink(
			fixture.Outside, fixture.Root / "files" / "escape", filesystemError);
		fixture.SymlinkAvailable = !filesystemError;
#if !defined(_WIN32)
		expect(
			fixture.SymlinkAvailable,
			"the symlink escape hatch exists, so its refusal is not vacuous");
#endif
		return fixture;
	}

	void plant_parent_secrets()
	{
		for (const auto& [name, value] : planted_secrets) {
			set_environment(std::string{ name }, std::string{ value });
			expect(
				environment_value(std::string{ name }) == value,
				"the parent really holds each planted credential");
		}
	}

	void pin_helper_environment(
		const snapshot_fixture&             aFixture,
		const core::agent_revision_binding& aPinned)
	{
		set_environment("BPR_MCP_SNAPSHOT_ROOT", aFixture.Root.string());
		set_environment(
			"BPR_MCP_PINNED_BINDING", mcp::encode_revision_binding(aPinned));
	}

	[[nodiscard]] auto reported_names(
		const std::string& aStandardError, const std::string_view aTag)
		-> std::vector<std::string>
	{
		auto       names  = std::vector<std::string>{ };
		const auto prefix = std::string{ aTag } + "\t";
		for (const auto& line : split_lines(aStandardError)) {
			if (line.starts_with(prefix)) {
				names.push_back(line.substr(prefix.size()));
			}
		}
		return names;
	}

	/// \brief Every tool name the helper must not have, by mutation family.
	[[nodiscard]] auto forbidden_tool_names() -> std::vector<std::string>
	{
		return {
			// network
			"fetch_url",
			"serve_http",
			// write
			"write_snapshot_file",
			"apply_patch",
			// draft
			"create_review_draft",
			"update_review_draft",
			// publish
			"publish_review",
			"submit_review",
			// note
			"store_private_note",
			"delete_private_note",
		};
	}

	void test_the_helper_holds_only_its_allowlisted_environment()
	{
		plant_parent_secrets();
		const auto fixture = build_snapshot();
		pin_helper_environment(fixture, pinned_binding());

		const auto run = run_helper(
			raw_method(1U, "tools/list") + "\n");

		auto received = reported_names(
			run.StandardError, "mcp-environment-received");
		auto retained = reported_names(
			run.StandardError, "mcp-environment-retained");

		std::ranges::sort(received);
		std::ranges::sort(retained);
		auto allowlist = std::vector<std::string>{ };
		for (const auto& allowed : mcp::mcp_environment_allowlist) {
			allowlist.emplace_back(allowed);
		}
		std::ranges::sort(allowlist);
		// Asserted before the exit code, so a removed reduction is reported as
		// the environment it left behind rather than as a helper that stopped.
		expect(
			retained == allowlist,
			"the child's complete environment equals the helper's allowlist");
		expect(run.ExitCode == 0, "the helper completes a batch cleanly");

		expect(received == allowlist, "only fixed allowlist names may be reported before reduction");
		const auto counts = reported_names(run.StandardError, "mcp-environment-counts");
		expect(counts.size() == 1U, "the complete environment has one count observation");
		const auto firstSeparator = counts.front().find('\t');
		expect(firstSeparator != std::string::npos, "the environment counts have fixed framing");
		expect(std::stoull(counts.front().substr(0U, firstSeparator)) >= allowlist.size() + planted_secrets.size(),
			   "the child observes all planted names before reduction");
		expect(counts.front().substr(firstSeparator) == "\t2\t0",
			   "the complete retained environment has exactly two names and no unexpected name");
		expect(run.StandardError.size() <= 512U, "the complete diagnostic is bounded");
		for (const auto& [name, value] : planted_secrets) {
			expect(run.StandardError.find(name) == std::string::npos,
				   "untrusted environment names are not diagnostic content");
			expect(
				std::ranges::find(retained, name) == retained.end(),
				"no planted credential survives into the running helper");
			expect(
				environment_value(std::string{ name }) == value,
				"each credential still exists in the parent");
			expect(
				run.StandardOutput.find(value) == std::string::npos
					&& run.StandardError.find(value) == std::string::npos,
				"no credential value is written to either child channel");
		}
	}

	void test_every_refusal_in_the_table_is_swept()
	{
		plant_parent_secrets();
		const auto fixture = build_snapshot();
		pin_helper_environment(fixture, pinned_binding());

		struct refusal_row
		{
			std::string Label;
			std::string Request;
			std::string Expected;
		};

		auto       rows = std::vector<refusal_row>{ };
		const auto add =
			[&rows](std::string aLabel, std::string aRequest, std::string aExpected) {
				rows.push_back({
					.Label    = std::move(aLabel),
					.Request  = std::move(aRequest),
					.Expected = std::move(aExpected),
				});
			};

		const auto next = [&rows] { return rows.size() + 1U; };

		add("outbound_connect_url",
			tool_call_with_path(
				next(),
				"read_snapshot_file",
				"https://api.github.com/repos/octo/app/pulls/1"),
			"network_location_refused");
		add("outbound_connect_method",
			raw_method(next(), "net/connect"),
			"unknown_method");
		add("listener_bind_authority",
			tool_call_with_path(
				next(), "read_snapshot_file", "//127.0.0.1:8080/queue"),
			"network_location_refused");
		add("listener_bind_unc_authority",
			tool_call_with_path(
				next(), "read_snapshot_file", "\\\\127.0.0.1\\share\\queue"),
			"network_location_refused");
		add("listener_bind_method",
			raw_method(next(), "net/listen"),
			"unknown_method");
		add("read_above_snapshot_root",
			tool_call_with_path(
				next(), "read_snapshot_file", "../../../etc/passwd"),
			"path_outside_snapshot");
		add("read_absolute_outside_snapshot",
			tool_call_with_path(next(), "read_snapshot_file", "/etc/passwd"),
			"path_outside_snapshot");
		add("read_snapshot_metadata_above_files_root",
			tool_call_with_path(next(), "read_snapshot_file", "../binding"),
			"path_outside_snapshot");
		if (fixture.SymlinkAvailable) {
			add("symlink_escape_read",
				tool_call_with_path(
					next(), "read_snapshot_file", "escape/secret.txt"),
				"path_outside_snapshot");
			add("symlink_escape_alignment",
				tool_call_with_path(
					next(), "snapshot_file_alignment", "escape/secret.txt"),
				"path_outside_snapshot");
		}
		for (const auto& forbidden : forbidden_tool_names()) {
			add("forbidden_tool_" + forbidden,
				tool_call(next(), forbidden),
				"unknown_tool");
		}

		auto requests = std::string{ };
		for (const auto& row : rows) {
			requests += row.Request + "\n";
		}
		const auto run = run_helper(requests);
		expect(run.ExitCode == 0, "the swept batch runs to completion");

		const auto responses = split_lines(run.StandardOutput);
		expect(
			responses.size() == rows.size(),
			"every entry of the refusal table is answered exactly once - a "
			"missing answer would let a row pass by not being asked");

		// Accumulated rather than exited on, so a single removed check reports
		// every entry of the table it broke instead of only the first.
		auto       failures = std::vector<std::string>{ };
		const auto require
			= [&failures](const bool aCondition, const std::string& aMessage) {
				  if (!aCondition) {
					  failures.push_back(aMessage);
				  }
			  };

		for (auto index = std::size_t{ 0U }; index < rows.size(); ++index) {
			const auto& row      = rows[index];
			const auto& response = responses[index];
			const auto  message  = "refusal table entry '" + row.Label
				+ "' must be refused as " + row.Expected;
			require(
				response.find("\"id\":" + std::to_string(index + 1U) + ",")
					!= std::string::npos,
				message + " (and answered under its own request identity)");
			require(refusal_of(response) == row.Expected, message);
			require(
				!carries_result(response),
				message + " with no result beside the refusal");
		}
		for (const auto& failure : failures) {
			std::cerr << "FAILED: " << failure << '\n';
		}
		expect(
			failures.empty(),
			"every entry of the refusal table is refused by its own exact cause");

		// Nothing outside the snapshot may appear anywhere in the transcript,
		// whatever the refusal said.
		expect(
			run.StandardOutput.find("outside-the-snapshot-and-nobody-asked")
					== std::string::npos
				&& run.StandardOutput.find("root:") == std::string::npos,
			"no refused read leaks the bytes it refused to serve");

		for (const auto& forbidden : forbidden_tool_names()) {
			expect(
				std::ranges::none_of(
					mcp::mcp_tools,
					[&forbidden](const mcp::mcp_tool_descriptor& aDescriptor) {
						return aDescriptor.Name == forbidden;
					}),
				"no write, draft, publish, or note name is a declared tool - "
				"not even a disabled one");
		}

		// The last entry of the table is not a request at all: the helper takes
		// no option, so no listener, port, or endpoint can be named to it.
		const auto refusedArgument = run_helper(
			raw_method(1U, "tools/list") + "\n", { "--listen", "127.0.0.1:8080" });
		expect(
			refusedArgument.ExitCode != 0
				&& refusedArgument.StandardError.find(
					   "mcp-startup-refusal\tunexpected_argument")
					!= std::string::npos
				&& refusedArgument.StandardOutput.empty(),
			"refusal table entry 'listener_bind_argument' must refuse a helper "
			"asked to take a listener endpoint, and answer nothing");
	}

	void test_head_movement_makes_the_pinned_snapshot_stale()
	{
		plant_parent_secrets();
		const auto fixture = build_snapshot();
		pin_helper_environment(fixture, pinned_binding());

		const auto request = tool_call_with_path(
								 1U, "read_snapshot_file", "src/retry.cpp")
			+ "\n" + tool_call(2U, "list_snapshot_files") + "\n";

		const auto current = run_helper(request);
		expect(
			current.StandardOutput.find("return retry();") != std::string::npos,
			"the pinned snapshot serves its own frozen bytes while current");

		// The head moves. The pin does not: the host that froze this projection
		// is still asking about the revision it froze.
		auto moved    = pinned_binding();
		moved.HeadOid = commit('9');
		write_file(
			fixture.Root / "binding", mcp::encode_revision_binding(moved) + "\n");

		const auto stale     = run_helper(request);
		const auto responses = split_lines(stale.StandardOutput);
		expect(
			responses.size() == 2U,
			"every call after the head moved is still answered");
		for (const auto& response : responses) {
			expect(
				refusal_of(response) == "revision_binding_stale",
				"a call whose observed binding is not the pinned one is refused "
				"as stale");
			expect(
				!carries_result(response),
				"a stale call carries no result beside its refusal");
		}
		expect(
			stale.StandardOutput.find("return retry();") == std::string::npos
				&& stale.StandardOutput.find("src/retry.cpp")
					== std::string::npos,
			"a stale helper re-reads nothing - it refuses instead of serving "
			"whichever revision exists now");
	}

	[[nodiscard]] auto source_enumerators(const std::string& aSource)
		-> std::vector<std::string>
	{
		const auto begin = aSource.find("enum class mcp_tool : std::uint8_t {");
		expect(
			begin != std::string::npos,
			"the closed tool enumeration is where the canary reads it");
		const auto end = aSource.find("\n\t};", begin);
		expect(
			end != std::string::npos,
			"the tool enumeration is closed by a brace the canary can find");

		auto       names = std::vector<std::string>{ };
		const auto block = aSource.substr(begin, end - begin);
		for (const auto& line : split_lines(block)) {
			auto trimmed = std::string_view{ line };
			while (!trimmed.empty()
				   && (trimmed.front() == '\t' || trimmed.front() == ' ')) {
				trimmed.remove_prefix(1U);
			}
			if (trimmed.starts_with("///") || !trimmed.ends_with(',')) {
				continue;
			}
			names.emplace_back(trimmed.substr(0U, trimmed.size() - 1U));
		}
		return names;
	}

	void test_advertised_tools_equal_the_closed_source_enumeration()
	{
		plant_parent_secrets();
		const auto fixture = build_snapshot();
		pin_helper_environment(fixture, pinned_binding());

		const auto source = read_file(BPR_MCP_TOOL_SOURCE);
		expect(!source.empty(), "the helper's tool enumeration source is readable");

		// Three lists, from three places, compared exactly. The advertised list
		// comes off the wire; the enumeration comes from the source text; the
		// third is written out here, so a tool added to both of the others
		// still fails until a person updates this one.
		const auto expected = std::vector<std::string>{
			"snapshot_binding",
			"list_snapshot_files",
			"read_snapshot_file",
			"snapshot_file_alignment",
		};
		expect(
			source_enumerators(source) == expected,
			"the source enumeration is exactly the reviewed list");
		expect(
			mcp::mcp_tool_count == expected.size(),
			"the declared tool count covers exactly the reviewed list");

		auto compiled = std::vector<std::string>{ };
		for (auto index = std::size_t{ 0U }; index < mcp::mcp_tools.size();
			 ++index) {
			expect(
				static_cast<std::size_t>(mcp::mcp_tools[index].Tool) == index,
				"each descriptor sits at its own enumerator's index");
			compiled.emplace_back(mcp::mcp_tools[index].Name);
		}
		expect(
			compiled == expected,
			"the compiled descriptor table is exactly the reviewed list");

		const auto run       = run_helper(raw_method(1U, "tools/list") + "\n");
		const auto responses = split_lines(run.StandardOutput);
		expect(responses.size() == 1U, "tools/list is answered once");

		auto advertised = std::vector<std::string>{ };
		auto position   = std::size_t{ 0U };
		while (true) {
			constexpr std::string_view marker = "{\"name\":\"";
			const auto                 begin  = responses.front().find(marker, position);
			if (begin == std::string::npos) {
				break;
			}
			const auto start = begin + marker.size();
			const auto end   = responses.front().find('"', start);
			expect(end != std::string::npos, "each advertised name is complete");
			advertised.push_back(responses.front().substr(start, end - start));
			position = end;
		}
		expect(
			advertised == expected,
			"the advertised tool list equals the source enumeration exactly");

		// Nothing can add a tool at runtime, so the list above is the whole
		// story rather than the story at one moment.
		expect(
			source.find("std::vector<mcp_tool_descriptor>") == std::string::npos
				&& source.find("push_back") == std::string::npos
				&& source.find(
					   "inline constexpr std::array<mcp_tool_descriptor, "
					   "mcp_tool_count> mcp_tools")
					!= std::string::npos,
			"the tool list is a compile-time array, not a registry something "
			"can add to");
	}

	void test_the_stdio_protocol_answers_only_its_declared_methods()
	{
		plant_parent_secrets();
		const auto fixture = build_snapshot();
		pin_helper_environment(fixture, pinned_binding());

		const auto requests = std::string{ }
			+ R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"canary","version":"1"}}})"
			+ "\n" + R"({"jsonrpc":"2.0","method":"notifications/initialized"})"
			+ "\n" + tool_call(2U, "snapshot_binding") + "\n"
			+ tool_call(3U, "list_snapshot_files") + "\n"
			+ tool_call_with_path(4U, "read_snapshot_file", "src/retry.cpp")
			+ "\n"
			+ tool_call_with_path(5U, "snapshot_file_alignment", "src/retry.cpp")
			+ "\n" + "this line is not JSON at all\n"
			+ tool_call_with_path(6U, "read_snapshot_file", "src/absent.cpp")
			+ "\n" + R"({"jsonrpc":"2.0","id":7,"method":"\uDC00"})" + "\n";

		const auto run       = run_helper(requests);
		const auto responses = split_lines(run.StandardOutput);
		expect(
			responses.size() == 8U,
			"a notification is answered with silence and every request with "
			"exactly one line");

		expect(
			responses[0].find(R"("protocolVersion":"2025-06-18")")
					!= std::string::npos
				&& responses[0].find(R"("name":"better-pull-review-context")")
					!= std::string::npos
				&& responses[0].find(R"("tools":{"listChanged":false})")
					!= std::string::npos,
			"initialize answers with the one declared revision and the tools "
			"capability");
		expect(
			responses[1].find(
				mcp::encode_revision_binding(pinned_binding()))
				!= std::string::npos,
			"snapshot_binding answers with the pinned revision binding");
		expect(
			responses[2].find("src/retry.cpp") != std::string::npos,
			"list_snapshot_files answers with the frozen projection's paths");
		expect(
			responses[3].find("return retry();") != std::string::npos,
			"read_snapshot_file answers with the frozen bytes");
		expect(
			responses[4].find(identity('a')) != std::string::npos
				&& responses[4].find(identity('b')) != std::string::npos
				&& responses[4].find(identity('c')) != std::string::npos
				&& responses[4].find(identity('d')) != std::string::npos
				&& responses[4].find("left_line_count\\t42") != std::string::npos,
			"snapshot_file_alignment answers with the frozen comparison, line "
			"counts, and alignment identities");
		expect(
			responses[5].find(R"("id":null)") != std::string::npos
				&& refusal_of(responses[5]) == "invalid_request",
			"a line that is not JSON-RPC is refused under a null identity");
		expect(
			refusal_of(responses[6]) == "snapshot_entry_missing",
			"a path inside the snapshot that is not in it is missing, not "
			"outside it - the two refusals stay distinguishable");
		expect(
			responses[7].find(R"("id":null)") != std::string::npos
				&& refusal_of(responses[7]) == "invalid_request",
			"[[BPR-1434]] a lone low surrogate escape is malformed JSON, never "
			"an invalid UTF-8 sequence");

		// The refusal enumeration is closed, and every value is reachable by a
		// distinguishable number, so no two causes can collapse onto one.
		auto values = std::vector<std::uint8_t>{ };
		for (const auto refusal : {
				 mcp::mcp_refusal::invalid_request,
				 mcp::mcp_refusal::unknown_method,
				 mcp::mcp_refusal::unknown_tool,
				 mcp::mcp_refusal::invalid_arguments,
				 mcp::mcp_refusal::network_location_refused,
				 mcp::mcp_refusal::path_outside_snapshot,
				 mcp::mcp_refusal::snapshot_entry_missing,
				 mcp::mcp_refusal::snapshot_entry_too_large,
				 mcp::mcp_refusal::revision_binding_stale,
				 mcp::mcp_refusal::snapshot_unavailable,
			 }) {
			values.push_back(static_cast<std::uint8_t>(refusal));
		}
		std::ranges::sort(values);
		expect(
			std::ranges::adjacent_find(values) == values.end()
				&& values.size() == mcp::mcp_refusal_count,
			"collapsing any two refusals onto one value fails here");
	}
} // namespace

auto main() -> int
{
	test_the_helper_holds_only_its_allowlisted_environment();
	test_every_refusal_in_the_table_is_swept();
	test_head_movement_makes_the_pinned_snapshot_stale();
	test_advertised_tools_equal_the_closed_source_enumeration();
	test_the_stdio_protocol_answers_only_its_declared_methods();
	expect(scratch().remove(), "the canary removes the scratch directory it created");
	std::cout << "mcp-context-helper-tests:pass\n";
	return EXIT_SUCCESS;
}
