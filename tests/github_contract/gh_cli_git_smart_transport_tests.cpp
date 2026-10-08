#include "../support/test_expectation.hpp"
#include "bpr/application/git_protocol_v2.hpp"
#include "gh_cli_git_smart_transport.hpp"
#include "github_cli_process_runner.hpp"
#include "github_paged_read.hpp"
#include <QCoreApplication>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QTextStream>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>
#include <thread>
#include <utility>
#include <vector>
namespace {
	using namespace bpr;
	using namespace bpr::adapters::github;
	using namespace bpr::adapters::github::detail;
	using bpr::tests::expect;
	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		std::array<std::uint8_t, 32> bytes{ };
		bytes[0]  = aSeed;
		bytes[31] = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return Identity{ bytes };
	}

	template <typename Oid>
	[[nodiscard]] constexpr auto oid(const std::uint8_t aSeed) -> Oid
	{
		std::array<std::uint8_t, 32> bytes{ };
		for (std::size_t index = 0U; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + static_cast<std::uint8_t>(index));
		}
		return Oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto cloud_candidate() -> github_host_profile_candidate
	{
		return {
			.Profile        = core::host_profile_id{ 11 },
			.Host           = core::git_host_id{ 21 },
			.Revision       = application::github_profile_revision{ 31 },
			.Server         = application::github_server_contract::github_com_2022_11_28,
			.Authentication = application::github_authentication_transport::github_cli,
			.Proxy          = application::github_proxy_mode::system,
			.Trust          = application::github_trust_mode::system,
			.Provenance     = github_profile_provenance::builtin,
			.Hostname       = "github.com",
			.RestRoot       = "https://api.github.com",
			.GraphqlRoot    = "https://api.github.com/graphql",
		};
	}

	[[nodiscard]] auto smart_context()
		-> application::git_smart_request_context
	{
		const auto baseRef = application::parse_git_base_branch_ref("refs/heads/main");
		expect(baseRef.has_value(), "base-ref");
		return {
			.ApplicationSession = core::application_session_id{ 202U },
			.Authority          = core::pull_request_authority{
				.Profile         = core::host_profile_id{ 11U },
				.ProfileRevision = core::github_profile_revision{ 31U },
				.Host            = core::git_host_id{ 21U },
				.HostAuthority   = opaque<core::host_authority_fingerprint>(1U),
				.Account         = core::github_account_id{ 203U },
				.Viewer          = core::github_viewer_actor_id{ 204U },
				.Repository      = core::repository_id{ 205U },
				.Cache           = opaque<core::repository_cache_scope_id>(2U),
				.PullRequest     = core::pull_request_id{ 206U },
				.Number          = core::pull_request_number{ 17U },
				.Scope           = opaque<core::pull_request_authority_scope_id>(3U),
			},
			.Revision = core::accepted_pull_request_revision{
				.Source = core::pull_request_source_revision{
					.Observation           = opaque<core::epoch_source_observation_id>(9U),
					.Snapshot              = core::provider_snapshot_id{ 301U },
					.IdentityGeneration    = core::github_identity_generation{ 207U },
					.PullRequestGeneration = core::request_generation{ 208U },
				},
				.Origin     = core::pull_request_revision_origin::live,
				.BaseBranch = opaque<core::base_branch_id>(4U),
				.Base       = oid<core::git_commit_oid>(10U),
				.Head       = oid<core::git_commit_oid>(30U),
			},
			.Repository = core::repository_locator{
				.Owner = "octo-org",
				.Name  = "review-repository",
			},
			.BaseRef = *baseRef,
		};
	}

	class recording_pack_sink final : public application::git_pack_stream_sink
	{
	public:
		explicit recording_pack_sink(const core::git_hash_algorithm aFormat)
			: Format{ aFormat }
		{
		}

		[[nodiscard]] auto append(
			const std::span<const std::byte> aInput,
			const std::stop_token            aStop)
			-> application::git_pack_stream_result<void> override
		{
			if (AppendFailure.has_value()) {
				return std::unexpected(*AppendFailure);
			}
			if (aStop.stop_requested()) {
				return std::unexpected(
					application::git_pack_stream_failure::cancelled);
			}
			Received.insert(Received.end(), aInput.begin(), aInput.end());
			return { };
		}

		[[nodiscard]] auto finish(const std::stop_token aStop)
			-> application::git_pack_stream_result<
				application::completed_git_pack_receipt> override
		{
			if (FinishFailure.has_value()) {
				return std::unexpected(*FinishFailure);
			}
			if (aStop.stop_requested()) {
				return std::unexpected(
					application::git_pack_stream_failure::cancelled);
			}
			Finished = true;
			application::git_pack_oid pack{
				.Algorithm = Format,
			};
			pack.Digest.front() = 0x42U;
			auto observed       = pack;
			if (MismatchedReceipt) {
				observed.Digest.front() = 0x43U;
			}
			auto token                  = std::array<std::uint8_t, 32U>{ };
			token.front()               = 0x71U;
			auto spoolIdentity          = application::git_artifact_identity{ };
			spoolIdentity.Bytes.front() = 1U;
			spoolIdentity.Bytes[1U]     = 1U;
			spoolIdentity.Bytes[2U]     = 0x7fU;
			spoolIdentity.Bytes.back()  = 0x51U;
			return application::completed_git_pack_receipt{
				.Operation    = opaque<core::epoch_operation_id>(41U),
				.ExpectedPack = pack,
				.ObservedPack = observed,
				.Spool        = {
					.Role = application::git_artifact_role::
						completed_spool,
					.Identity = spoolIdentity,
					.Bytes    = Received.size(),
				},
				.OpaqueToken = token,
			};
		}

		auto abort() noexcept -> void override
		{
			Aborted = true;
			++AbortCalls;
		}

		core::git_hash_algorithm                            Format;
		std::vector<std::byte>                              Received;
		std::optional<application::git_pack_stream_failure> AppendFailure;
		std::optional<application::git_pack_stream_failure> FinishFailure;
		bool                                                MismatchedReceipt = false;
		bool                                                Finished          = false;
		bool                                                Aborted           = false;
		std::size_t                                         AbortCalls        = 0U;
	};

	QByteArray git(const std::filesystem::path& aRoot, QStringList aArguments, const QByteArray& aInput = { })
	{
		QProcess            process;
		QProcessEnvironment env;
		for (const auto& pair : std::array<std::pair<const char*, const char*>, 7>{ { { "GIT_CONFIG_NOSYSTEM", "1" }, { "GIT_CONFIG_GLOBAL", "/dev/null" }, { "GIT_ALLOW_PROTOCOL", "" }, { "GIT_AUTHOR_NAME", "Fixture" }, { "GIT_AUTHOR_EMAIL", "fixture@example.invalid" }, { "GIT_COMMITTER_NAME", "Fixture" }, { "GIT_COMMITTER_EMAIL", "fixture@example.invalid" } } }) {
			env.insert(pair.first, pair.second);
		}
		process.setProcessEnvironment(env);
		process.setWorkingDirectory(QString::fromStdString(aRoot.string()));
		process.start("/usr/bin/git", aArguments);
		expect(process.waitForStarted(5000), "fixture local Git starts");
		process.write(aInput);
		process.closeWriteChannel();
		expect(process.waitForFinished(10000) && process.exitCode() == 0, "fixture local Git succeeds");
		return process.readAllStandardOutput().trimmed();
	}
	void write(const std::filesystem::path& aPath, std::string_view aValue)
	{
		std::ofstream file(aPath);
		file << aValue;
		expect(file.good(), "fixture file");
	}
	int fake_gh(const QStringList& aArguments)
	{
		const auto executable = std::filesystem::path{ aArguments[0].toStdString() };
		const auto root       = executable.parent_path();
		if (aArguments.size() >= 3 && aArguments.back() == "--help") {
			if (std::filesystem::exists(root / "wait-gh-help")) {
				std::this_thread::sleep_for(std::chrono::seconds{ 20 });
			}
			if (std::filesystem::exists(root / "overflow-gh-help")) {
				std::cout << std::string(64U * 1024U, 'x');
				return 0;
			}
			if (std::filesystem::exists(root / "stderr-gh-help")) {
				std::cerr << "warning\n";
			}
			const auto    topic = aArguments.mid(1, aArguments.size() - 2).join('-').toStdString();
			std::ifstream help{ root / ("gh-help-" + topic) };
			if (help) {
				std::cout << help.rdbuf();
			} else if (topic == "api") {
				std::cout << "FLAGS\n  -H, --header key:value\n      --hostname string\n  -i, --include\n      --input file\n  -X, --method string\n";
			} else if (topic == "repo-clone") {
				std::cout << "USAGE\n  gh repo clone <repository> [<directory>] [-- <gitflags>...]\n\nFLAGS\n      --no-upstream   Do not add an upstream remote\n";
			} else if (topic == "auth-git-credential") {
				std::cout << "USAGE\n  gh auth git-credential [flags]\n";
			} else {
				return 1;
			}
			int           exit = 0;
			std::ifstream status{ root / "gh-help-exit" };
			status >> exit;
			return exit;
		}
		if (aArguments.size() == 2 && aArguments[1] == "--version") {
			std::ifstream version{ root / "version" };
			if (version) {
				std::cout << version.rdbuf();
			} else {
				std::cout << "gh version 2.100.0 (fixture)\n";
			}
			return 0;
		}
		if (aArguments.size() > 2 && aArguments[1] == "api" && aArguments[2].contains("/git/blobs/")) {
			if (std::filesystem::exists(root / "wait-blob")) {
				std::this_thread::sleep_for(std::chrono::seconds{ 10 });
			}
			std::ifstream response{ root / "blob-response" };
			if (!response) {
				return 1;
			}
			std::cout << response.rdbuf();
			return 0;
		}
		// [[BPR-1327]]. The batched blob read: GraphQL on standard input.
		if (aArguments.size() > 2 && aArguments[1] == "api" && aArguments[2] == "graphql" && aArguments.contains("--input")
			&& !aArguments.contains("--include")) {
			// The child may not write; it checks its own request instead. Each
			// line of graphql-expect must appear in the body, and the query
			// text must carry no object ID.
			const auto    request = QTextStream{ stdin }.readAll().toStdString();
			std::ifstream expected{ root / "graphql-expect" };
			for (std::string line; std::getline(expected, line);) {
				if (!line.empty() && request.find(line) == std::string::npos) {
					return 7;
				}
			}
			const auto query = request.substr(0U, request.find("\"variables\""));
			if (query.find("oid:\\\"") != std::string::npos || query.find(std::string(40U, 'b')) != std::string::npos) {
				return 8;
			}
			std::ifstream response{ root / "graphql-response" };
			if (!response) {
				return 1;
			}
			std::cout << response.rdbuf();
			return 0;
		}
		if (aArguments.size() > 2 && aArguments[1] == "api") {
			if (std::filesystem::exists(root / "fail-viewer")) {
				return 1;
			}
			const bool switched = std::filesystem::exists(root / "switch-viewer");
			std::cout << "HTTP/2.0 200 OK\r\nContent-Type: application/json\r\nX-RateLimit-Limit: 5000\r\nX-RateLimit-Remaining: 4999\r\nX-RateLimit-Used: 1\r\nX-RateLimit-Reset: 2000000000\r\nX-RateLimit-Resource: core\r\n\r\n{\"id\":" << (switched ? 999 : 203) << ",\"node_id\":\"U_fixture\",\"login\":\"fixture\",\"name\":\"Fixture\"}";
			return 0;
		}
		if ((aArguments.size() == 19 || aArguments.size() == 21) && aArguments[1] == "repo" && aArguments[2] == "clone") {
			if (std::filesystem::exists(root / "fail-clone")) {
				return 1;
			}
			if (aArguments.size() == 21 && (aArguments[19] != "--depth=1" || aArguments[20] != "--filter=blob:none")) {
				return 6;
			}
			const std::filesystem::path target{ aArguments[4].toStdString() };
			if (!target.parent_path().filename().string().starts_with("bpr-cli-git-") || target.parent_path().parent_path() != "/private/tmp") {
				return 3;
			}
			if (std::filesystem::exists(root / "wait-clone")) {
				std::this_thread::sleep_for(std::chrono::seconds{ 10 });
			}
			if (std::filesystem::exists(root / "overflow")) {
				std::ofstream out(target.parent_path() / "oversize");
				std::string   block(65536, 'x');
				for (int i = 0; i < 600; ++i) {
					out << block;
				}
				return 4;
			}
			std::error_code error;
			std::filesystem::copy(root / "source.git", target, std::filesystem::copy_options::recursive, error);
			if (error) {
				return 5;
			}
			if (aArguments.size() == 21) {
				std::ifstream omitted{ root / "sparse-omitted" };
				std::string   oid;
				while (omitted >> oid) {
					std::filesystem::remove(target / "objects" / oid.substr(0, 2) / oid.substr(2));
				}
			}
			if (aArguments.size() == 21 && std::filesystem::exists(root / "missing-sparse")) {
				std::filesystem::remove_all(target / "objects");
			}
			if (aArguments.size() == 21 && std::filesystem::exists(root / "missing-sparse-tree")) {
				std::ifstream input{ root / "missing-sparse-tree" };
				std::string   tree;
				input >> tree;
				std::filesystem::remove(target / "objects" / tree.substr(0, 2) / tree.substr(2));
			}
			std::cerr << "bounded fixture progress\n";
			return 0;
		}
		return 2;
	}
	int fake_git(const QStringList& aArguments)
	{
		const auto root = std::filesystem::path{ aArguments[0].toStdString() }.parent_path();
		if (aArguments.size() == 2 && aArguments[1] == "--version") {
			if (std::filesystem::exists(root / "fail-git-version")) {
				return 1;
			}
			std::ifstream version{ root / "git-version" };
			std::cout << version.rdbuf();
			return 0;
		}
		if (aArguments.size() == 3 && aArguments[1] == "clone" && aArguments[2] == "-h") {
			if (std::filesystem::exists(root / "wait-git-help")) {
				std::this_thread::sleep_for(std::chrono::seconds{ 20 });
			}
			if (std::filesystem::exists(root / "overflow-git-help")) {
				std::cout << std::string(32U * 1024U, 'x');
				return 129;
			}
			std::ifstream help{ root / "git-help" };
			if (help) {
				std::cout << help.rdbuf();
				int           exit = 129;
				std::ifstream status{ root / "git-help-exit" };
				status >> exit;
				return exit;
			}
		}
		QProcess process;
		process.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
		process.start("/usr/bin/git", aArguments.mid(1));
		if (!process.waitForStarted(5000)) {
			return 2;
		}
		const std::string input{ std::istreambuf_iterator<char>{ std::cin }, std::istreambuf_iterator<char>{ } };
		process.write(QByteArray::fromStdString(input));
		process.closeWriteChannel();
		if (!process.waitForFinished(10000)) {
			return 3;
		}
		const auto output = process.readAllStandardOutput();
		std::cout.write(output.constData(), output.size());
		return process.exitStatus() == QProcess::NormalExit ? process.exitCode() : 4;
	}
	class discard final : public application::git_smart_wire_consumer
	{
	public:
		bool consume(std::span<const std::byte>) noexcept override
		{
			++Calls;
			return true;
		}
		unsigned Calls = 0;
	};
	void contract()
	{
		QTemporaryDir temporary{ QStringLiteral("/private/tmp/bpr-bridge-test-XXXXXX") };
		expect(temporary.isValid(), "test root");
		const std::filesystem::path root{ temporary.path().toStdString() };
		const auto                  executable = root / "gh";
		std::filesystem::copy_file(QCoreApplication::applicationFilePath().toStdString(), executable);
		std::filesystem::permissions(executable, std::filesystem::perms::owner_all);
		std::filesystem::create_directory(root / "neutral");
		std::filesystem::create_directory(root / "home");
		const auto source = root / "source.git";
		git(root, { "init", "--bare", QString::fromStdString(source.string()) });
		const auto scope                = "--git-dir=" + QString::fromStdString(source.string());
		const auto blob                 = git(root, { scope, "hash-object", "-w", "--stdin" }, "complete local contents\n");
		const auto changedBlob          = git(root, { scope, "hash-object", "-w", "--stdin" }, "changed local contents\n");
		const auto secondaryBlob        = git(root, { scope, "hash-object", "-w", "--stdin" }, "secondary base contents\n");
		const auto changedSecondaryBlob = git(root, { scope, "hash-object", "-w", "--stdin" }, "secondary head contents\n");
		const auto baseTree             = git(root, { scope, "mktree" }, "100644 blob " + blob + "\tfile.txt\n100644 blob " + secondaryBlob + "\tsecondary.txt\n");
		const auto tree                 = git(root, { scope, "mktree" }, "100644 blob " + changedBlob + "\tfile.txt\n100644 blob " + changedSecondaryBlob + "\tsecondary.txt\n");
		const auto ancestor             = git(root, { scope, "commit-tree", baseTree }, "ancestor\n");
		const auto base                 = git(root, { scope, "commit-tree", baseTree, "-p", ancestor }, "base\n");
		const auto head                 = git(root, { scope, "commit-tree", tree, "-p", base }, "head\n");
		auto       context              = smart_context();
		context.Revision.Base           = *core::parse_git_commit_oid(base.toStdString());
		context.Revision.Head           = *core::parse_git_commit_oid(head.toStdString());
		const auto profile              = validate_github_host_profile(cloud_candidate());
		expect(profile.has_value(), "profile");
		const auto syntheticGit = root / "git";
		std::filesystem::copy_file(QCoreApplication::applicationFilePath().toStdString(), syntheticGit);
		std::filesystem::permissions(syntheticGit, std::filesystem::perms::owner_all);
		write(root / "git-version", "git version 2.54.0 (Apple Git-157)\n");
		const auto configuration = validate_gh_cli_configuration({ .Executable = executable, .NeutralWorkingDirectory = root / "neutral", .Platform = gh_cli_macos_environment_facts{ .HomeDirectory = root / "home" }, .Https = { } });
		expect(configuration.has_value(), "configuration");
		const core::github_session_request_context session{ .Request = core::request_id{ 101 }, .ApplicationSession = context.ApplicationSession, .Profile = profile->profile(), .IdentityGeneration = core::github_identity_generation{ 207 } };
		auto                                       fixtureProcess = make_github_cli_process_runner();
		const auto                                 versionCheck   = fixtureProcess->run(compile_gh_cli_version_request(*configuration), { });
		if (!versionCheck) {
			std::cerr << "version failure " << static_cast<int>(versionCheck.error()) << '\n';
		}
		expect(versionCheck.has_value(), "fixture sandbox version launch");
		gh_cli_transport identity{ *profile, *configuration, *fixtureProcess };
		const auto       wire   = compile_read_request(semantic_read_request{ .Server = server_contract::github_com, .Operation = read_operation::get_viewer_identity });
		const auto       viewer = identity.perform({ .Correlation = { .Parent = session, .ProfileRevision = profile->revision(), .Host = profile->host(), .Operation = read_operation::get_viewer_identity, .Page = 1 }, .Wire = *wire }, { });
		if (const auto* error = std::get_if<github_wire_error>(&viewer)) {
			std::cerr << "viewer failure " << static_cast<int>(error->Error.Failure) << '\n';
		}
		expect(std::holds_alternative<github_wire_success>(viewer), "fixture sandbox viewer launch");
		const auto normalized = normalize_observed_github_viewer(std::get<github_wire_success>(viewer).Response, server_contract::github_com);
		expect(normalized.has_value(), "fixture viewer normalized");

		constexpr application::git_protocol_v2_fetch_limits limits{ .MaximumWireBytes = 1048576, .MaximumPackBytes = 1048576, .MaximumProgressBytes = 1024, .MaximumPackets = 1024 };

		auto enterpriseCandidate        = cloud_candidate();
		enterpriseCandidate.Server      = application::github_server_contract::ghes_3_18_2022_11_28;
		enterpriseCandidate.Provenance  = github_profile_provenance::explicit_user;
		enterpriseCandidate.Hostname    = "ghe.example.invalid";
		enterpriseCandidate.RestRoot    = "https://ghe.example.invalid/api/v3";
		enterpriseCandidate.GraphqlRoot = "https://ghe.example.invalid/api/graphql";
		const auto enterpriseProfile    = validate_github_host_profile(enterpriseCandidate);
		expect(enterpriseProfile.has_value(), "synthetic GHES CLI profile validates");
		for (const bool restarted : { false, true }) {
			static_cast<void>(restarted);
			auto qualifiedTransport = make_gh_cli_git_smart_transport(*enterpriseProfile, *configuration, syntheticGit, session, context, [] { return true; });
			expect(qualifiedTransport.has_value(), "synthetic selected Git is admitted under exact authority");
			recording_pack_sink qualified{ core::git_hash_algorithm::sha1 };
			expect(application::acquire_git_protocol_v2_pack(**qualifiedTransport, context, { }, qualified, limits, { }).has_value() && qualified.Finished,
				   "newer Apple Git qualifies by required capabilities and operations across recreated authority");
		}
		for (const std::string_view banner : { "git version 2.49.0\n", "git version 2.54.0.windows.1\n", "git version 3.0.0 (future vendor)\n" }) {
			write(root / "git-version", banner);
			auto                qualifiedTransport = make_gh_cli_git_smart_transport(*enterpriseProfile, *configuration, syntheticGit, session, context, [] { return true; });
			recording_pack_sink qualified{ core::git_hash_algorithm::sha1 };
			expect(application::acquire_git_protocol_v2_pack(**qualifiedTransport, context, { }, qualified, limits, { }).has_value() && qualified.Finished,
				   "minimum, upstream and other vendor versions require the same direct capability and object operation proof");
		}
		for (const std::string_view banner : { "git version 2.48.9\n", "git version 2.54\n", "git version 2.54.0junk\n", "git version 999999999999999999999.0.0\n", "unclassified fixture output\n" }) {
			write(root / "git-version", banner);
			auto                refusedTransport = make_gh_cli_git_smart_transport(*enterpriseProfile, *configuration, syntheticGit, session, context, [] { return true; });
			recording_pack_sink refused{ core::git_hash_algorithm::sha1 };
			const auto          result = application::acquire_git_protocol_v2_pack(**refusedTransport, context, { }, refused, limits, { });
			const auto*         error  = result ? nullptr : std::get_if<application::git_smart_transport_error>(&result.error());
			expect(error && error->Step == application::git_smart_transport_step::git_qualification && refused.Received.empty(),
				   "older and malformed numeric Git versions cannot acquire objects");
		}
		write(root / "git-version", "git version 2.54.0 (Apple Git-157)\n");
		auto                       probeTransport = make_gh_cli_git_smart_transport(*enterpriseProfile, *configuration, syntheticGit, session, context, [] { return true; });
		constexpr std::string_view completeHelp   = " --revision <rev>\n --bare\n --template <dir>\n --config <key>\n --filter <args>\n --depth <n>\n";
		for (const std::string_view missing : { "revision", "bare", "template", "config" }) {
			auto       incompleteHelp = std::string{ completeHelp };
			const auto position       = incompleteHelp.find(missing);
			incompleteHelp.replace(position, missing.size(), "unavailable");
			write(root / "git-help", incompleteHelp);
			recording_pack_sink refused{ core::git_hash_algorithm::sha1 };
			const auto          result = application::acquire_git_protocol_v2_pack(**probeTransport, context, { }, refused, limits, { });
			const auto*         error  = result ? nullptr : std::get_if<application::git_smart_transport_error>(&result.error());
			expect(error && error->Step == application::git_smart_transport_step::git_qualification && refused.Received.empty(), "every required clone capability is checked before acquisition");
		}
		auto sparseProbeContext            = context;
		sparseProbeContext.AcquisitionMode = application::git_smart_acquisition_mode::sparse_commit_trees;
		auto sparseProbeTransport          = make_gh_cli_git_smart_transport(*enterpriseProfile, *configuration, syntheticGit, session, sparseProbeContext, [] { return true; });
		for (const std::string_view missing : { "depth", "filter" }) {
			auto       incompleteHelp = std::string{ completeHelp };
			const auto position       = incompleteHelp.find(missing);
			incompleteHelp.replace(position, missing.size(), "unavailable");
			write(root / "git-help", incompleteHelp);
			recording_pack_sink refused{ core::git_hash_algorithm::sha1 };
			const auto          result = application::acquire_git_protocol_v2_pack(**sparseProbeTransport, sparseProbeContext, { }, refused, limits, { });
			const auto*         error  = result ? nullptr : std::get_if<application::git_smart_transport_error>(&result.error());
			expect(error && error->Step == application::git_smart_transport_step::git_qualification && refused.Received.empty(), "sparse acquisition additionally requires depth and filter capabilities");
		}
		write(root / "git-help", completeHelp);
		write(root / "git-help-exit", "2");
		recording_pack_sink badHelpExit{ core::git_hash_algorithm::sha1 };
		expect(!application::acquire_git_protocol_v2_pack(**probeTransport, context, { }, badHelpExit, limits, { }) && badHelpExit.Received.empty(), "arbitrary nonzero help exit cannot qualify even with required tokens");
		for (const std::string_view exit : { "0", "129" }) {
			write(root / "git-help-exit", exit);
			recording_pack_sink qualified{ core::git_hash_algorithm::sha1 };
			expect(application::acquire_git_protocol_v2_pack(**probeTransport, context, { }, qualified, limits, { }).has_value() && qualified.Finished, "only completed success and documented help exit qualify");
		}
		std::filesystem::remove(root / "git-help");
		std::filesystem::remove(root / "git-help-exit");
		write(root / "overflow-git-help", "1");
		recording_pack_sink boundedHelp{ core::git_hash_algorithm::sha1 };
		const auto          overflowingHelp      = application::acquire_git_protocol_v2_pack(**probeTransport, context, { }, boundedHelp, limits, { });
		const auto*         overflowingHelpError = overflowingHelp ? nullptr : std::get_if<application::git_smart_transport_error>(&overflowingHelp.error());
		expect(overflowingHelpError && overflowingHelpError->Failure == application::git_smart_transport_failure::response_limit && overflowingHelpError->Step == application::git_smart_transport_step::git_capability_probe && boundedHelp.Received.empty(), "capability output has a strict 16 KiB bound and never publishes partial pack bytes");
		std::filesystem::remove(root / "overflow-git-help");
		write(root / "wait-git-help", "1");
		{
			std::stop_source    stop;
			std::jthread        cancel{ [&] { std::this_thread::sleep_for(std::chrono::milliseconds{400}); stop.request_stop(); } };
			recording_pack_sink cancelledSink{ core::git_hash_algorithm::sha1 };
			const auto          started = std::chrono::steady_clock::now();
			const auto          result  = application::acquire_git_protocol_v2_pack(**probeTransport, context, { }, cancelledSink, limits, stop.get_token());
			const auto*         error   = result ? nullptr : std::get_if<application::git_smart_transport_error>(&result.error());
			expect(error && error->Failure == application::git_smart_transport_failure::cancelled && cancelledSink.Received.empty() && std::chrono::steady_clock::now() - started < std::chrono::seconds{ 3 }, "running capability cancellation is bounded, reaped and publishes nothing");
		}
		recording_pack_sink timedOutSink{ core::git_hash_algorithm::sha1 };
		const auto          timedOutHelp  = application::acquire_git_protocol_v2_pack(**probeTransport, context, { }, timedOutSink, limits, { });
		const auto*         timedOutError = timedOutHelp ? nullptr : std::get_if<application::git_smart_transport_error>(&timedOutHelp.error());
		expect(timedOutError && timedOutError->Failure == application::git_smart_transport_failure::timeout && timedOutError->Step == application::git_smart_transport_step::git_capability_probe && timedOutSink.Received.empty(), "capability probe deadline preserves a typed timeout without publishing content");
		std::filesystem::remove(root / "wait-git-help");
		write(root / "fail-git-version", "1");
		auto                failedProbeTransport = make_gh_cli_git_smart_transport(*enterpriseProfile, *configuration, syntheticGit, session, context, [] { return true; });
		recording_pack_sink failedProbeSink{ core::git_hash_algorithm::sha1 };
		const auto          failedProbe      = application::acquire_git_protocol_v2_pack(**failedProbeTransport, context, { }, failedProbeSink, limits, { });
		const auto*         failedProbeError = failedProbe ? nullptr : std::get_if<application::git_smart_transport_error>(&failedProbe.error());
		expect(failedProbeError && failedProbeError->Step == application::git_smart_transport_step::git_version && failedProbeSink.Received.empty(),
			   "failed executable probe is separate from successful unsupported banner");
		std::filesystem::remove(root / "fail-git-version");
		write(root / "git-version", "git version 2.50.1 (Apple Git-155)\n");
		recording_pack_sink retriedSink{ core::git_hash_algorithm::sha1 };
		expect(application::acquire_git_protocol_v2_pack(**failedProbeTransport, context, { }, retriedSink, limits, { }).has_value() && retriedSink.Finished,
			   "probe recovery retries the same exact authority without resetting profile state");
		auto                reopenedTransport = make_gh_cli_git_smart_transport(*enterpriseProfile, *configuration, syntheticGit, session, context, [] { return true; });
		recording_pack_sink reopenedSink{ core::git_hash_algorithm::sha1 };
		expect(application::acquire_git_protocol_v2_pack(**reopenedTransport, context, { }, reopenedSink, limits, { }).has_value() && reopenedSink.Finished,
			   "recreated authority recovers with capabilities instead of exact version equality");

		auto transport = make_gh_cli_git_smart_transport(*profile, *configuration, "/usr/bin/git", session, context, [] { return true; });
		expect(transport.has_value(), "acquire");
		const application::git_smart_exact_blob_request blobRequest{ context, *core::parse_git_blob_oid(blob.toStdString()), 1024U };
		const auto                                      goodBlob = "{\"sha\":\"" + blob.toStdString() + "\",\"size\":24,\"encoding\":\"base64\",\"content\":\"" + QByteArray{ "complete local contents\n" }.toBase64().toStdString() + "\"}";
		// Git hash-object above independently supplies the expected identity.
		write(root / "blob-response", goodBlob);
		auto exact = (*transport)->read_exact_blob(blobRequest, { });
		expect(exact && exact->size() == 24U, "exact blob validated against local Git hash");
		for (const auto& bad : std::vector<std::string>{ "{}", "not json",
														 "{\"sha\":\"" + blob.toStdString() + "\",\"size\":1,\"encoding\":\"base64\",\"content\":\"eA==\"}",
														 "{\"sha\":\"" + blob.toStdString() + "\",\"size\":1025,\"encoding\":\"base64\",\"content\":\"\"}" }) {
			write(root / "blob-response", bad);
			expect(!(*transport)->read_exact_blob(blobRequest, { }), "malformed/oversize/hash mismatch blob refused");
		}
		write(root / "blob-response", "{\"sha\":\"" + blob.toStdString() + "\",\"size\":1025,\"encoding\":\"base64\",\"content\":\"\"}");
		const auto oversizedSize = (*transport)->read_exact_blob(blobRequest, { });
		expect(!oversizedSize && oversizedSize.error().Failure == application::git_smart_transport_failure::response_limit, "declared blob size above materialization bound reports response limit");
		write(root / "blob-response", "{\"size\":1025.5}");
		const auto fractionalSize = (*transport)->read_exact_blob(blobRequest, { });
		expect(!fractionalSize && fractionalSize.error().Failure == application::git_smart_transport_failure::malformed_response, "fractional blob size remains malformed");
		for (const auto& replacement : std::vector<std::pair<std::string, std::string>>{
				 { "base64", "utf-8" }, { blob.toStdString(), std::string(40U, 'a') }, { "Y29tcGxldGU", "!29tcGxldGU" }, { "24", "23" } }) {
			auto       bad = goodBlob;
			const auto at  = bad.find(replacement.first);
			expect(at != std::string::npos, "negative fixture replacement exists");
			bad.replace(at, replacement.first.size(), replacement.second);
			write(root / "blob-response", bad);
			expect(!(*transport)->read_exact_blob(blobRequest, { }), "encoding, SHA, base64 and size mismatches refuse");
		}
		write(root / "blob-response", std::string(8192U, 'x'));
		const auto largeResponse = (*transport)->read_exact_blob(blobRequest, { });
		expect(!largeResponse && largeResponse.error().Failure == application::git_smart_transport_failure::response_limit, "blob stdout quota enforced");
		write(root / "blob-response", goodBlob);
		auto wrongBlobContext = blobRequest;
		++wrongBlobContext.MaximumBytes;
		wrongBlobContext.Context.ApplicationSession = core::application_session_id{ 999 };
		expect(!(*transport)->read_exact_blob(wrongBlobContext, { }), "wrong blob context refused");
		unsigned   observations         = 0U;
		auto       changedConfiguration = make_gh_cli_git_smart_transport(*profile, *configuration, "/usr/bin/git", session, context,
																		  [&] { return ++observations < 3U; });
		const auto changedRead          = (*changedConfiguration)->read_exact_blob(blobRequest, { });
		expect(!changedRead && (*changedConfiguration)->revoked() && observations == 3U, "post-read configuration change discards blob and revokes");
		auto oversizedBlob         = blobRequest;
		oversizedBlob.MaximumBytes = application::maximum_git_exact_blob_bytes + 1U;
		expect(!(*transport)->read_exact_blob(oversizedBlob, { }), "oversize request refused");
		std::filesystem::remove(root / "blob-response");
		expect(!(*transport)->read_exact_blob(blobRequest, { }), "missing blob refused");
		write(root / "blob-response", goodBlob);
		write(root / "wait-blob", "1");
		{
			std::stop_source blobStop;
			std::jthread     cancelBlob{ [&] { std::this_thread::sleep_for(std::chrono::milliseconds{400}); blobStop.request_stop(); } };
			const auto       cancelledBlob = (*transport)->read_exact_blob(blobRequest, blobStop.get_token());
			expect(!cancelledBlob && cancelledBlob.error().Failure == application::git_smart_transport_failure::cancelled, "blob child cancelled and reaped");
		}
		std::filesystem::remove(root / "wait-blob");
		{
			// [[BPR-1327]]. A batch is one GraphQL read; each answer is checked
			// against its own object ID and a bad one does not spoil the rest.
			const auto                                       other = *core::parse_git_blob_oid(std::string(40U, 'b'));
			const application::git_smart_exact_blobs_request batch{ context, { *core::parse_git_blob_oid(blob.toStdString()), other }, 1024U };
			write(root / "graphql-response",
				  "{\"data\":{\"repository\":{\"b0\":{\"oid\":\"" + blob.toStdString()
					  + "\",\"byteSize\":24,\"isBinary\":false,\"isTruncated\":false,\"text\":\"complete local contents\\n\"},"
					  + "\"b1\":{\"oid\":\"" + std::string(40U, 'b') + "\",\"byteSize\":5,\"isBinary\":false,\"isTruncated\":false,\"text\":\"forge\"}}}}");
			write(root / "graphql-expect",
				  "\"o0\":\"" + blob.toStdString() + "\"\n\"o1\":\"" + std::string(40U, 'b')
					  + "\"\noid:$o0\noid:$o1\n\"owner\":\n\"name\":\n");
			const auto read = (*transport)->read_exact_blobs(batch, { });
			expect(read && read->size() == 2U && (*read)[0] && (*read)[0]->size() == 24U && !(*read)[1],
				   "a batch returns the verified blob and leaves the one whose bytes do not hash to its ID absent; "
				   "its object IDs travel as variables, never in the query text");
			write(root / "graphql-expect", "\"o2\":\n");
			expect(!(*transport)->read_exact_blobs(batch, { }), "control: the child refuses a body without what it expects");
			std::filesystem::remove(root / "graphql-expect");
			for (const auto& bad : std::vector<std::string>{ "not json", "{}", "{\"data\":{\"repository\":null}}" }) {
				write(root / "graphql-response", bad);
				expect(!(*transport)->read_exact_blobs(batch, { }), "a malformed batch response is refused whole");
			}
			write(root / "graphql-response",
				  "{\"data\":{\"repository\":{\"b0\":{\"oid\":\"" + blob.toStdString()
					  + "\",\"byteSize\":24,\"isBinary\":false,\"isTruncated\":true,\"text\":\"complete local contents\\n\"},\"b1\":null}}}");
			const auto truncated = (*transport)->read_exact_blobs(batch, { });
			expect(truncated && !(*truncated)[0] && !(*truncated)[1], "truncated and missing objects are absent");
			auto oversized = batch;
			oversized.ExpectedBlobs.assign(application::maximum_git_exact_blob_batch + 1U, other);
			expect(!(*transport)->read_exact_blobs(oversized, { }), "a batch over the bound is refused");
			auto wrongContext                       = batch;
			wrongContext.Context.ApplicationSession = core::application_session_id{ 999 };
			expect(!(*transport)->read_exact_blobs(wrongContext, { }), "a batch for another context is refused");
			std::filesystem::remove(root / "graphql-response");
		}
		recording_pack_sink sink{ core::git_hash_algorithm::sha1 };
		auto                result = application::acquire_git_protocol_v2_pack(**transport, context, { }, sink, limits, { });
		if (!result) {
			if (const auto* error = std::get_if<application::git_smart_transport_error>(&result.error())) {
				std::cerr << "transport failure " << static_cast<int>(error->Failure) << '\n';
			}
		}
		expect(result.has_value() && sink.Finished && !sink.Received.empty(), "real bridge obeys application discovery/refs/sideband parser");
		const QByteArray pack{ reinterpret_cast<const char*>(sink.Received.data()), static_cast<qsizetype>(sink.Received.size()) };
		const auto       verify = root / "verify.git";
		git(root, { "init", "--bare", QString::fromStdString(verify.string()) });
		const auto verifyScope = "--git-dir=" + QString::fromStdString(verify.string());
		git(root, { verifyScope, "index-pack", "--strict", "--stdin" }, pack);
		for (const auto& [revision, expectedPrimary, expectedSecondary] : std::array<std::array<QByteArray, 3U>, 2U>{ { { base, "complete local contents", "secondary base contents" }, { head, "changed local contents", "secondary head contents" } } }) {
			expect(git(root, { verifyScope, "cat-file", "-p", QString::fromUtf8(revision) + ":file.txt" }) == expectedPrimary
					   && git(root, { verifyScope, "cat-file", "-p", QString::fromUtf8(revision) + ":secondary.txt" }) == expectedSecondary,
				   "strictly imported pack contains exact independently created contents for both files at both accepted endpoints");
		}
		expect(git(root, { verifyScope, "rev-parse", QString::fromUtf8(base) + ":file.txt" }) == blob
				   && git(root, { verifyScope, "rev-parse", QString::fromUtf8(head) + ":file.txt" }) == changedBlob
				   && git(root, { verifyScope, "rev-parse", QString::fromUtf8(base) + ":secondary.txt" }) == secondaryBlob
				   && git(root, { verifyScope, "rev-parse", QString::fromUtf8(head) + ":secondary.txt" }) == changedSecondaryBlob,
			   "each imported file preserves its independently generated exact blob identity");
		write(root / "sparse-omitted", blob.toStdString() + "\n" + changedBlob.toStdString() + "\n" + secondaryBlob.toStdString() + "\n" + changedSecondaryBlob.toStdString() + "\n" + ancestor.toStdString() + "\n");
		write(root / "version", "gh version 2.101.0 (fixture)\n");
		auto sparseContext                  = context;
		sparseContext.AcquisitionMode       = application::git_smart_acquisition_mode::sparse_commit_trees;
		auto                sparseTransport = make_gh_cli_git_smart_transport(*profile, *configuration, "/usr/bin/git", session, sparseContext, [] { return true; });
		recording_pack_sink sparseSink{ core::git_hash_algorithm::sha1 };
		const auto          sparseResult = application::acquire_git_protocol_v2_pack(**sparseTransport, sparseContext, { }, sparseSink, limits, { });
		expect(sparseResult && sparseSink.Finished, "sparse bridge emits bounded pack");
		const auto sparseVerify = root / "sparse-verify.git";
		git(root, { "init", "--bare", QString::fromStdString(sparseVerify.string()) });
		const auto       sparseScope = "--git-dir=" + QString::fromStdString(sparseVerify.string());
		const QByteArray sparsePack{ reinterpret_cast<const char*>(sparseSink.Received.data()), static_cast<qsizetype>(sparseSink.Received.size()) };
		git(root, { sparseScope, "index-pack", "--stdin" }, sparsePack);
		expect(git(root, { sparseScope, "cat-file", "-t", head }) == "commit", "sparse pack contains exact head");
		expect(git(root, { sparseScope, "cat-file", "-t", tree }) == "tree", "sparse pack contains root tree");
		const auto inventory = git(root, { sparseScope, "cat-file", "--batch-all-objects", "--batch-check=%(objecttype)" });
		expect(!inventory.contains("blob") && inventory.count("commit") == 2, "sparse pack excludes blobs and retains exact wants only");
		const auto expectCliRefusal = [&](std::string_view aWhy, application::git_smart_transport_step aStep, application::git_smart_transport_failure aFailure) {
			// The sandboxed fake cannot record calls, so a viewer read or clone
			// reached before refusal would fail and change the reported step.
			write(root / "fail-viewer", "1");
			write(root / "fail-clone", "1");
			recording_pack_sink refused{ core::git_hash_algorithm::sha1 };
			const auto          refusal = application::acquire_git_protocol_v2_pack(**sparseTransport, sparseContext, { }, refused, limits, { });
			const auto*         error   = refusal ? nullptr : std::get_if<application::git_smart_transport_error>(&refusal.error());
			std::filesystem::remove(root / "fail-viewer");
			std::filesystem::remove(root / "fail-clone");
			expect(error && error->Step == aStep && error->Failure == aFailure && refused.Received.empty() && !refused.Finished,
				   std::string{ "refused gh keeps its typed boundary before any viewer read or remote acquisition: " } + std::string{ aWhy });
		};
		for (const std::string_view banner : { "gh version 2.95.9 (fixture)\n", "gh version 2.96\n", "gh version 2.101.0junk\n",
											   "gh version 99999999999999999999.0.0\n", "gh 2.101.0\n", "unclassified fixture output\n" }) {
			write(root / "version", banner);
			expectCliRefusal(banner, application::git_smart_transport_step::cli_qualification, application::git_smart_transport_failure::unavailable);
		}
		for (const std::string_view banner : { "gh version 2.96.0 (fixture)\n", "gh version 2.101.0 (2026-09-15)\n", "gh version 2.120.0 (2027-03-01)\n",
											   "gh version 3.0.0 (future)\n", "gh version 2.130.1-rc.1\n" }) {
			write(root / "version", banner);
			recording_pack_sink admitted{ core::git_hash_algorithm::sha1 };
			expect(application::acquire_git_protocol_v2_pack(**sparseTransport, sparseContext, { }, admitted, limits, { }).has_value() && admitted.Finished,
				   "the minimum, the owner's 2.101.0 and newer or later-major gh are admitted once their operations are proven");
		}
		write(root / "version", "gh version 2.120.0 (fixture)\n");
		for (const auto& [topic, help] : std::array<std::pair<std::string_view, std::string_view>, 9U>{ {
				 { "api", "FLAGS\n      --hostname string\n  -i, --include\n      --input file\n  -X, --method string\n" },
				 { "api", "FLAGS\n  -H, --header key:value\n  -i, --include\n      --input file\n  -X, --method string\n" },
				 { "api", "FLAGS\n  -H, --header key:value\n      --hostname string\n      --input file\n  -X, --method string\n" },
				 { "api", "FLAGS\n  -H, --header key:value\n      --hostname string\n  -i, --include\n  -X, --method string\n" },
				 { "api", "FLAGS\n  -H, --header key:value\n      --hostname string\n  -i, --include\n      --input file\n" },
				 { "repo-clone", "USAGE\n  gh repo clone <repository> [<directory>]\n      --no-upstream\n" },
				 { "repo-clone", "USAGE\n  gh repo clone <repository> [<directory>] [-- <gitflags>...]\n" },
				 { "auth-git-credential", "USAGE\n  gh auth login [flags]\n" },
				 { "auth-git-credential", "" },
			 } }) {
			write(root / ("gh-help-" + std::string{ topic }), help);
			expectCliRefusal(topic, application::git_smart_transport_step::cli_qualification, application::git_smart_transport_failure::unavailable);
			std::filesystem::remove(root / ("gh-help-" + std::string{ topic }));
		}
		write(root / "gh-help-exit", "1");
		expectCliRefusal("nonzero help exit", application::git_smart_transport_step::cli_capability_probe, application::git_smart_transport_failure::unavailable);
		std::filesystem::remove(root / "gh-help-exit");
		write(root / "stderr-gh-help", "1");
		expectCliRefusal("help on standard error", application::git_smart_transport_step::cli_capability_probe, application::git_smart_transport_failure::unavailable);
		std::filesystem::remove(root / "stderr-gh-help");
		write(root / "overflow-gh-help", "1");
		expectCliRefusal("oversized help", application::git_smart_transport_step::cli_capability_probe, application::git_smart_transport_failure::response_limit);
		std::filesystem::remove(root / "overflow-gh-help");
		write(root / "wait-gh-help", "1");
		{
			std::stop_source    stop;
			std::jthread        cancel{ [&] { std::this_thread::sleep_for(std::chrono::milliseconds{ 400 }); stop.request_stop(); } };
			recording_pack_sink cancelledSink{ core::git_hash_algorithm::sha1 };
			const auto          started   = std::chrono::steady_clock::now();
			const auto          cancelled = application::acquire_git_protocol_v2_pack(**sparseTransport, sparseContext, { }, cancelledSink, limits, stop.get_token());
			const auto*         error     = cancelled ? nullptr : std::get_if<application::git_smart_transport_error>(&cancelled.error());
			expect(error && error->Failure == application::git_smart_transport_failure::cancelled && cancelledSink.Received.empty()
					   && std::chrono::steady_clock::now() - started < std::chrono::seconds{ 3 },
				   "cancelling a running gh probe is bounded, reaped and publishes nothing");
		}
		expectCliRefusal("help timeout", application::git_smart_transport_step::cli_capability_probe, application::git_smart_transport_failure::timeout);
		std::filesystem::remove(root / "wait-gh-help");
		recording_pack_sink repairedSink{ core::git_hash_algorithm::sha1 };
		expect(application::acquire_git_protocol_v2_pack(**sparseTransport, sparseContext, { }, repairedSink, limits, { }).has_value() && repairedSink.Finished,
			   "a repaired gh retries through the same exact authority");
		write(root / "version", "gh version 2.101.0 (fixture)\n");
		write(root / "missing-sparse", "1");
		recording_pack_sink missingSparseSink{ core::git_hash_algorithm::sha1 };
		expect(!application::acquire_git_protocol_v2_pack(**sparseTransport, sparseContext, { }, missingSparseSink, limits, { }) && missingSparseSink.Received.empty(), "missing sparse commit/tree refuses before consumer");
		std::filesystem::remove(root / "missing-sparse");
		write(root / "missing-sparse-tree", tree.toStdString());
		recording_pack_sink missingTreeSink{ core::git_hash_algorithm::sha1 };
		expect(!application::acquire_git_protocol_v2_pack(**sparseTransport, sparseContext, { }, missingTreeSink, limits, { }) && missingTreeSink.Received.empty(), "missing exact root tree refuses before consumer");
		std::filesystem::remove(root / "missing-sparse-tree");
		expect(!(*sparseTransport)->discover({ context }, { }), "complete request cannot reuse sparse authority");
		const application::detail::git_protocol_v2_capabilities capabilities{ .ObjectFormat = core::git_hash_algorithm::sha1, .LsRefs = true, .Fetch = true };
		auto                                                    request = application::detail::make_git_protocol_v2_fetch_request(context, capabilities, { });
		expect(request.has_value(), "fetch request");
		for (const auto field : { 0U, 1U, 2U, 3U }) {
			auto staleContext = context;
			switch (field) {
			case 0U: staleContext.Authority.Account = core::github_account_id{ 999U }; break;
			case 1U: staleContext.Authority.Host = core::git_host_id{ 999U }; break;
			case 2U: staleContext.Revision.Head = oid<core::git_commit_oid>(99U); break;
			case 3U: staleContext.Revision.Source.Snapshot = core::provider_snapshot_id{ 999U }; break;
			default: expect(false, "known stale authority field"); break;
			}
			recording_pack_sink refused{ core::git_hash_algorithm::sha1 };
			const auto          staleResult = application::acquire_git_protocol_v2_pack(**transport, staleContext, { }, refused, limits, { });
			expect(!staleResult && refused.Received.empty() && !refused.Finished, "qualification cannot widen account, host, accepted head or source-observation authority");
		}
		discard     consumer;
		auto        wrong    = *request;
		const auto  baseText = base.toStdString();
		const auto  headText = head.toStdString();
		std::string wrongText{ reinterpret_cast<const char*>(wrong.data()), wrong.size() };
		wrongText.replace(wrongText.find(baseText), baseText.size(), headText);
		const auto wrongSpan = std::as_bytes(std::span{ wrongText.data(), wrongText.size() });
		auto       denied    = (*transport)->fetch({ context, wrongSpan, 1048576 }, consumer, { });
		expect(!denied && denied.error().Failure == application::git_smart_transport_failure::invalid_request && consumer.Calls == 0, "duplicate/reordered current wants refused before child");
		auto limited = (*transport)->fetch({ context, *request, 32 }, consumer, { });
		expect(!limited && limited.error().Failure == application::git_smart_transport_failure::response_limit && consumer.Calls == 0, "wire budget refuses before consumer");
		write(root / "fail-viewer", "1");
		auto failed = (*transport)->fetch({ context, *request, 1048576 }, consumer, { });
		expect(!failed && failed.error().Failure == application::git_smart_transport_failure::unavailable, "viewer transport failure is not account mismatch");
		expect(failed.error().Step == application::git_smart_transport_step::identity_before, "viewer refusal is distinguished from clone and version qualification");
		std::filesystem::remove(root / "fail-viewer");
		write(root / "fail-clone", "1");
		auto cloneFailure = (*transport)->fetch({ context, *request, 1048576 }, consumer, { });
		expect(!cloneFailure && cloneFailure.error().Failure == application::git_smart_transport_failure::unavailable
				   && cloneFailure.error().Step == application::git_smart_transport_step::clone,
			   "nonzero clone failure keeps its step without retaining stderr");
		std::filesystem::remove(root / "fail-clone");
		write(root / "overflow", "1");
		auto overflow = (*transport)->fetch({ context, *request, 1048576 }, consumer, { });
		expect(!overflow && overflow.error().Failure == application::git_smart_transport_failure::response_limit, "kernel file ceiling maps to response limit");
		std::filesystem::remove(root / "overflow");
		write(root / "wait-clone", "1");
		std::stop_source stop;
		std::jthread     cancel{ [&] {std::this_thread::sleep_for(std::chrono::milliseconds{400});stop.request_stop(); } };
		const auto       started   = std::chrono::steady_clock::now();
		auto             cancelled = (*transport)->fetch({ context, *request, 1048576 }, consumer, stop.get_token());
		expect(!cancelled && cancelled.error().Failure == application::git_smart_transport_failure::cancelled && std::chrono::steady_clock::now() - started < std::chrono::seconds{ 3 }, "running clone cancellation is bounded and reaped");
		std::filesystem::remove(root / "wait-clone");
		write(root / "switch-viewer", "1");
		auto       blobAuthority = make_gh_cli_git_smart_transport(*profile, *configuration, "/usr/bin/git", session, context, [] { return true; });
		const auto wrongAccount  = (*blobAuthority)->read_exact_blob(blobRequest, { });
		expect(!wrongAccount && wrongAccount.error().Failure == application::git_smart_transport_failure::authentication && (*blobAuthority)->revoked(), "blob read rejects and revokes switched account");
		auto switched = (*transport)->fetch({ context, *request, 1048576 }, consumer, { });
		expect(!switched && switched.error().Failure == application::git_smart_transport_failure::authentication && (*transport)->revoked(), "account mismatch revokes");
		auto revoked = (*transport)->discover({ context }, { });
		expect(!revoked && revoked.error().Failure == application::git_smart_transport_failure::cancelled, "revocation persists");
		std::cout << "bridge contract passed: complete pack, strict index, closed wants, bounds, viewer errors and revocation\n";
	}
}
int main(int argc, char** argv)
try {
	QCoreApplication app{ argc, argv };
	if (std::filesystem::path{ argv[0] }.filename() == "gh") {
		return fake_gh(QCoreApplication::arguments());
	}
	if (std::filesystem::path{ argv[0] }.filename() == "git") {
		return fake_git(QCoreApplication::arguments());
	}
	contract();
	return 0;
} catch (...) {
	return bpr::tests::failing_exit();
}
