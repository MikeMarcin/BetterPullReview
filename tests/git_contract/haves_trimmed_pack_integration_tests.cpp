// [[BPR-0846]] A server that honours `have` lines omits the objects reachable
// from them, so the pack for a pull request update is complete for the cache
// but not self-contained. These courses drive the live path through a
// transport seam that forwards the product's own protocol v2 bodies to a
// local `git upload-pack --stateless-rpc`, which honours haves exactly as a
// Git server does. Everything else is the production composition: the epoch
// application host, the protocol request and parser, the installed Git cache
// and its process runner, and the SQLite epoch catalog.
#include "../support/scratch_directory.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/git/installed_git_cache.hpp"
#include "bpr/adapters/sqlite/epoch_catalog.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/application/epoch_application_host.hpp"
#include "bpr/application/git_cache_port.hpp"
#include "bpr/application/review_tour_source.hpp"
#include "windows_system_root.hpp"
#include <QDir>
#include <QFile>
#include <future>
#include <mutex>
#include <thread>

#include <QByteArray>
#include <QCoreApplication>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	namespace application    = bpr::application;
	namespace core           = bpr::core;
	namespace git_adapter    = bpr::adapters::git;
	namespace sqlite_adapter = bpr::adapters::sqlite;

	auto require(const bool aCondition, const std::string_view aMessage) -> void
	{
		bpr::tests::expect(aCondition, aMessage);
	}

	template <typename Identity>
	[[nodiscard]] auto identity(const std::uint8_t aSeed) -> Identity
	{
		std::array<std::uint8_t, 32> bytes{ };
		bytes.fill(aSeed);
		return Identity{ bytes };
	}

	[[nodiscard]] auto git_executable() -> std::filesystem::path
	{
		return std::filesystem::canonical(BPR_TEST_GIT_EXECUTABLE);
	}

	struct git_output
	{
		int        ExitCode = -1;
		QByteArray Output;
		QByteArray Error;
	};

	// The server side of the seam. It runs the test's Git with a fixed,
	// configuration-free environment; nothing here reaches the product.
	[[nodiscard]] auto run_git(
		const std::filesystem::path& aRoot,
		const QStringList&           aArguments,
		const QByteArray&            aInput     = { },
		const bool                   aProtocol2 = false) -> git_output
	{
		auto environment = QProcessEnvironment{ };
		environment.insert("HOME", QString::fromStdString((aRoot / "home").string()));
		environment.insert("XDG_CONFIG_HOME", QString::fromStdString((aRoot / "xdg").string()));
		environment.insert("GIT_CONFIG_NOSYSTEM", "1");
		environment.insert("GIT_CONFIG_GLOBAL", "/dev/null");
		environment.insert("GIT_AUTHOR_NAME", "BetterPullReview Fixture");
		environment.insert("GIT_AUTHOR_EMAIL", "fixture.invalid@example.invalid");
		environment.insert("GIT_AUTHOR_DATE", "2001-01-01T00:00:00Z");
		environment.insert("GIT_COMMITTER_NAME", "BetterPullReview Fixture");
		environment.insert("GIT_COMMITTER_EMAIL", "fixture.invalid@example.invalid");
		environment.insert("GIT_COMMITTER_DATE", "2001-01-01T00:00:00Z");
		environment.insert("GIT_USER_AGENT", "git/bpr-fixture");
		environment.insert("LC_ALL", "C");
		environment.insert("TZ", "UTC");
		if (aProtocol2) {
			environment.insert("GIT_PROTOCOL", "version=2");
		}
		QProcess process;
#if defined(Q_OS_WIN)
		auto arguments = aArguments;
		arguments.prepend(QStringLiteral("core.longpaths=true"));
		arguments.prepend(QStringLiteral("-c"));
		process.setArguments(arguments);
#else
		process.setArguments(aArguments);
#endif
		process.setProgram(QString::fromStdString(git_executable().string()));
		process.setWorkingDirectory(QString::fromStdString(aRoot.string()));
		process.setProcessEnvironment(environment);
		process.start();
		require(process.waitForStarted(10'000), "start fixture Git");
		if (!aInput.isEmpty()) {
			require(process.write(aInput) == aInput.size(), "write fixture Git input");
		}
		process.closeWriteChannel();
		require(process.waitForFinished(60'000), "finish fixture Git");
		return {
			.ExitCode = process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1,
			.Output   = process.readAllStandardOutput(),
			.Error    = process.readAllStandardError(),
		};
	}

	[[nodiscard]] auto git_text(
		const std::filesystem::path& aRoot,
		const QStringList&           aArguments,
		const QByteArray&            aInput = { }) -> std::string
	{
		const auto result = run_git(aRoot, aArguments, aInput);
		if (result.ExitCode != 0) {
			std::cerr << "fixture Git failed: exit=" << result.ExitCode << " argv="
					  << aArguments.join(' ').toStdString() << " stderr=["
					  << result.Error.toStdString() << "]\n";
		}
		require(result.ExitCode == 0, "fixture Git succeeds");
		return result.Output.trimmed().toStdString();
	}

	[[nodiscard]] auto to_bytes(const QByteArray& aValue) -> std::vector<std::byte>
	{
		std::vector<std::byte> output(static_cast<std::size_t>(aValue.size()));
		std::transform(aValue.begin(), aValue.end(), output.begin(), [](const char aCharacter) {
			return static_cast<std::byte>(static_cast<unsigned char>(aCharacter));
		});
		return output;
	}

	[[nodiscard]] auto to_qbytes(const std::span<const std::byte> aValue) -> QByteArray
	{
		return QByteArray{ reinterpret_cast<const char*>(aValue.data()), static_cast<qsizetype>(aValue.size()) };
	}

	[[nodiscard]] auto packet(const std::string_view aPayload) -> QByteArray
	{
		static constexpr std::string_view digits = "0123456789abcdef";
		const auto                        length = aPayload.size() + 4U;
		QByteArray                        output;
		for (const auto shift : { 12U, 8U, 4U, 0U }) {
			output.push_back(digits[(length >> shift) & 0xfU]);
		}
		output.append(aPayload.data(), static_cast<qsizetype>(aPayload.size()));
		return output;
	}

	// Reassembles the pack from a protocol v2 fetch response's sideband 1.
	[[nodiscard]] auto served_pack(const QByteArray& aResponse) -> QByteArray
	{
		QByteArray pack;
		bool       inPack = false;
		qsizetype  offset = 0;
		while (offset + 4 <= aResponse.size()) {
			bool       parsed = false;
			const auto length = aResponse.mid(offset, 4).toUInt(&parsed, 16);
			require(parsed, "served response packet length parses");
			if (length <= 2U) {
				offset += 4;
				continue;
			}
			const auto payload = aResponse.mid(offset + 4, static_cast<qsizetype>(length) - 4);
			offset += static_cast<qsizetype>(length);
			if (!inPack) {
				inPack = payload == "packfile\n";
			} else if (!payload.isEmpty() && payload.front() == '\x01') {
				pack.append(payload.mid(1));
			}
		}
		return pack;
	}

	class temporary_root final
	{
	public:
		explicit temporary_root(const std::string_view aLabel)
		{
			// [[BPR-1128]], [[BPR-1138]]. Made at a name nothing had, owner-private from creation,
			// and owned from then, so a failure below removes it.
			Path      = bpr::tests::make_scratch_directory(std::string{ "bpr-haves-" } + std::string{ aLabel });
			auto made = bpr::tests::scratch_guard{ Path };
			for (const auto* child : { "git", "catalog", "server", "home", "xdg" }) {
				std::filesystem::create_directories(Path / child);
			}
			for (const auto* child : { "", "git", "catalog" }) {
				std::filesystem::permissions(Path / child, std::filesystem::perms::owner_all,
											 std::filesystem::perm_options::replace);
			}
			require(sqlite_adapter::make_storage_root_owner_private(Path / "catalog"),
					"narrow the catalog root to owner-private");
			Path = std::filesystem::canonical(Path);
			made.release();
		}

		~temporary_root()
		{
			std::error_code error;
			std::filesystem::remove_all(Path, error);
		}

		temporary_root(const temporary_root&)                    = delete;
		auto operator=(const temporary_root&) -> temporary_root& = delete;

		std::filesystem::path Path;
	};

	// base <- head1 <- head2. head2 adds one file and keeps head1's subtree,
	// so its trimmed pack references resident objects without repeating them.
	struct server_history
	{
		std::filesystem::path Repository;
		std::string           Readme;
		std::string           Base;
		std::string           Head1;
		std::string           Head1Tree;
		std::string           Subtree;
		std::string           Head2;
	};

	[[nodiscard]] auto make_server_history(const std::filesystem::path& aRoot) -> server_history
	{
		server_history history{ .Repository = aRoot / "server" / "fixture.git" };
		static_cast<void>(git_text(aRoot, { "init", "--bare", "--quiet", "--object-format=sha1", QString::fromStdString(history.Repository.string()) }));
		const auto gitDir = "--git-dir=" + QString::fromStdString(history.Repository.string());
		const auto blob   = [&](const std::string_view aText) {
			return git_text(aRoot, { gitDir, "hash-object", "-w", "--stdin" }, QByteArray{ aText.data(), static_cast<qsizetype>(aText.size()) });
		};
		const auto tree = [&](const std::string& aEntries) {
			return git_text(aRoot, { gitDir, "mktree" }, QByteArray::fromStdString(aEntries));
		};
		const auto commit = [&](const std::string& aTree, const std::string& aParent, const char* aMessage) {
			auto arguments = QStringList{ gitDir, "commit-tree", QString::fromStdString(aTree) };
			if (!aParent.empty()) {
				arguments << "-p" << QString::fromStdString(aParent);
			}
			return git_text(aRoot, arguments, QByteArray{ aMessage });
		};
		const auto readme       = blob("fixture base\n");
		const auto nested       = blob("nested file kept by every head\n");
		const auto added        = blob("added by the second head\n");
		history.Readme          = readme;
		const auto unicodeEntry = "100644 blob " + readme + "\t資料 α.txt\n";
		history.Subtree         = tree("100644 blob " + nested + "\tnested.txt\n");
		history.Base            = commit(tree("100644 blob " + readme + "\tREADME\n" + unicodeEntry), { }, "base\n");
		history.Head1Tree       = tree("100644 blob " + readme + "\tREADME\n040000 tree " + history.Subtree + "\tsrc\n" + unicodeEntry);
		history.Head1           = commit(history.Head1Tree, history.Base, "first head\n");
		history.Head2           = commit(tree("100644 blob " + readme + "\tREADME\n100644 blob " + added + "\tadded.txt\n040000 tree "
											  + history.Subtree + "\tsrc\n" + unicodeEntry),
										 history.Head1, "second head\n");
		static_cast<void>(git_text(aRoot, { gitDir, "update-ref", "refs/heads/main", QString::fromStdString(history.Base) }));
		static_cast<void>(git_text(aRoot, { gitDir, "update-ref", "refs/pull/81/head", QString::fromStdString(history.Head1) }));
		return history;
	}

	struct served_fetch
	{
		bool       SentHaves = false;
		QByteArray Pack;
	};

	// The transport seam. Discovery, ls-refs and fetch bodies are the
	// product's own; the responses are upload-pack's, framed as smart HTTP
	// frames them. `ForgedHave` adds a `have` the client never offered, so
	// the server omits objects the cache does not hold.
	class upload_pack_transport final : public application::git_smart_transport_port
	{
	public:
		upload_pack_transport(application::git_smart_request_context aContext, std::filesystem::path aRoot,
							  std::filesystem::path aRepository, std::optional<std::string> aForgedHave,
							  std::vector<served_fetch>& aFetches)
			: Context{ std::move(aContext) }
			, Root{ std::move(aRoot) }
			, Repository{ std::move(aRepository) }
			, ForgedHave{ std::move(aForgedHave) }
			, Fetches{ &aFetches }
		{
		}

		void revoke() noexcept override { Revoked = true; }

		[[nodiscard]] auto revoked() const noexcept -> bool override { return Revoked; }

		[[nodiscard]] auto discover(const application::git_smart_discovery_request& aRequest, const std::stop_token aStop)
			-> application::git_smart_transport_result<application::git_smart_discovery_response> override
		{
			if (const auto refused = refuse(aRequest.Context, aStop)) {
				return std::unexpected{ *refused };
			}
			const auto advertised = upload_pack({ "--stateless-rpc", "--advertise-refs" }, { });
			auto       body       = packet("# service=git-upload-pack\n") + QByteArray{ "0000" } + advertised;
			return application::git_smart_discovery_response{ .Body = to_bytes(body) };
		}

		[[nodiscard]] auto ls_refs(const application::git_smart_ls_refs_request& aRequest, const std::stop_token aStop)
			-> application::git_smart_transport_result<application::git_smart_ls_refs_response> override
		{
			if (const auto refused = refuse(aRequest.Context, aStop)) {
				return std::unexpected{ *refused };
			}
			return application::git_smart_ls_refs_response{
				.Body = to_bytes(upload_pack({ "--stateless-rpc" }, to_qbytes(aRequest.Body))),
			};
		}

		[[nodiscard]] auto fetch(const application::git_smart_fetch_request& aRequest,
								 application::git_smart_wire_consumer& aConsumer, const std::stop_token aStop)
			-> application::git_smart_transport_result<void> override
		{
			if (const auto refused = refuse(aRequest.Context, aStop)) {
				return std::unexpected{ *refused };
			}
			auto       body = to_qbytes(aRequest.Body);
			const auto sent = body.contains("have ");
			if (ForgedHave) {
				const auto done = body.lastIndexOf(packet("done\n"));
				require(done > 0, "fetch body ends with done");
				body.insert(done, packet("have " + *ForgedHave + "\n"));
			}
			const auto response = upload_pack({ "--stateless-rpc" }, body);
			Fetches->push_back({ .SentHaves = sent, .Pack = served_pack(response) });
			const auto wire = to_bytes(response);
			if (!aConsumer.consume(wire)) {
				return std::unexpected{ application::git_smart_transport_error{
					.Failure = application::git_smart_transport_failure::consumer_rejected,
				} };
			}
			return { };
		}

	private:
		[[nodiscard]] auto refuse(const application::git_smart_request_context& aContext, const std::stop_token aStop) const
			-> std::optional<application::git_smart_transport_error>
		{
			if (aStop.stop_requested() || aContext != Context || Revoked) {
				return application::git_smart_transport_error{ .Failure = application::git_smart_transport_failure::cancelled };
			}
			return std::nullopt;
		}

		[[nodiscard]] auto upload_pack(QStringList aArguments, const QByteArray& aBody) const -> QByteArray
		{
			aArguments.prepend("upload-pack");
			aArguments << QString::fromStdString(Repository.string());
			const auto result = run_git(Root, aArguments, aBody, true);
			if (result.ExitCode != 0) {
				std::cerr << "upload-pack failed: exit=" << result.ExitCode << " stderr=[" << result.Error.toStdString() << "]\n";
			}
			require(result.ExitCode == 0, "local upload-pack answers the product's request");
			return result.Output;
		}

		application::git_smart_request_context Context;
		std::filesystem::path                  Root;
		std::filesystem::path                  Repository;
		std::optional<std::string>             ForgedHave;
		std::vector<served_fetch>*             Fetches;
		bool                                   Revoked = false;
	};

	class upload_pack_authority final : public application::git_smart_authority_lease
	{
	public:
		upload_pack_authority(application::git_smart_authority_binding aBinding, std::function<std::shared_ptr<upload_pack_transport>(const application::git_smart_request_context&)> aMake)
			: Binding{ std::move(aBinding) }
			, Make{ std::move(aMake) }
		{
		}

		[[nodiscard]] auto binding() const noexcept -> const application::git_smart_authority_binding& override { return Binding; }

		[[nodiscard]] auto acquire(const application::git_smart_request_context& aContext)
			-> application::git_smart_transport_result<application::git_smart_transport> override
		{
			if (Revoked) {
				return std::unexpected{ application::git_smart_transport_error{ .Failure = application::git_smart_transport_failure::cancelled } };
			}
			return Make(aContext);
		}

		void revoke() noexcept override { Revoked = true; }

		[[nodiscard]] auto revoked() const noexcept -> bool override { return Revoked; }

	private:
		application::git_smart_authority_binding                                                             Binding;
		std::function<std::shared_ptr<upload_pack_transport>(const application::git_smart_request_context&)> Make;
		bool                                                                                                 Revoked = false;
	};

	class queued_executor final : public application::effect_executor, public application::decision_executor
	{
	public:
		void post(std::function<void()> aTask) override
		{
			const std::lock_guard lock{ Mutex };
			Tasks.push_back(std::move(aTask));
		}

		[[nodiscard]] auto empty() const noexcept -> bool
		{
			const std::lock_guard lock{ Mutex };
			return Tasks.empty();
		}

		void run_next()
		{
			std::function<void()> task;
			{
				const std::lock_guard lock{ Mutex };
				task = std::move(Tasks.front());
				Tasks.pop_front();
			}
			task();
		}

	private:
		mutable std::mutex                Mutex;
		std::deque<std::function<void()>> Tasks;
	};

	class deterministic_entropy final : public application::epoch_entropy_port
	{
	public:
		[[nodiscard]] auto generate() noexcept -> std::optional<application::epoch_entropy_bytes> override
		{
			std::array<std::uint8_t, 32> result{ };
			result.front() = Next++;
			result.back()  = 0xa5U;
			return result;
		}

	private:
		std::uint8_t Next = 1U;
	};

	class deterministic_clock final : public application::epoch_clock_port
	{
	public:
		[[nodiscard]] auto observe_utc() noexcept -> std::optional<core::observed_utc_milliseconds> override
		{
			return core::observed_utc_milliseconds{ Next++ };
		}

	private:
		std::int64_t Next = 1'000;
	};

	// Keeps the reason codes the host projects, which is where a cache
	// failure's mapped code reaches presentation.
	class recording_projection final : public application::epoch_projection_port
	{
	public:
		void publish(const application::epoch_presentation_update& aUpdate) override
		{
			if (aUpdate.Transition) {
				for (const auto& diagnostic : aUpdate.Transition->Diagnostics) {
					Reasons.push_back(diagnostic.Code);
				}
			}
		}

		[[nodiscard]] auto saw(const core::epoch_reason_code aReason) const -> bool
		{
			return std::ranges::find(Reasons, aReason) != Reasons.end();
		}

		std::vector<core::epoch_reason_code> Reasons;
	};

	// Every file under `aRoot` with index-pack's temporary name.
	[[nodiscard]] auto index_residue(const std::filesystem::path& aRoot) -> std::size_t
	{
		return static_cast<std::size_t>(std::ranges::count_if(
			std::filesystem::recursive_directory_iterator{ aRoot },
			[](const auto& aEntry) {
				const auto name = aEntry.path().filename().string();
				return name.starts_with("tmp_pack_") || name.starts_with("tmp_idx_");
			}));
	}

	[[nodiscard]] auto catalog_key() -> application::epoch_catalog_authority_key
	{
		return {
			.Profile         = core::host_profile_id{ 11U },
			.ProfileRevision = core::github_profile_revision{ 21U },
			.Host            = core::git_host_id{ 31U },
			.HostAuthority   = identity<core::host_authority_fingerprint>(2U),
			.Account         = core::github_account_id{ 41U },
			.Viewer          = core::github_viewer_actor_id{ 51U },
			.Repository      = core::repository_id{ 61U },
			.PullRequest     = core::pull_request_id{ 71U },
			.Number          = core::pull_request_number{ 81U },
			.ObjectFormat    = core::git_hash_algorithm::sha1,
		};
	}

	[[nodiscard]] auto commit_oid(const std::string& aText) -> core::git_commit_oid
	{
		const auto parsed = core::parse_git_commit_oid(aText);
		require(parsed.has_value(), "fixture commit parses");
		return *parsed;
	}

	struct composed_live_path final
	{
		explicit composed_live_path(const std::string_view aLabel)
			: Root{ aLabel }
			, History{ make_server_history(Root.Path) }
		{
			auto madeCache   = git_adapter::make_installed_git_cache({
				.GitExecutable     = git_executable(),
				.StorageRoot       = Root.Path / "git",
				.WindowsSystemRoot = bpr::tests::git_contract::windows_system_root(),
			});
			auto madeCatalog = sqlite_adapter::make_epoch_catalog({ .StorageRoot = Root.Path / "catalog" });
			require(madeCache.has_value() && madeCatalog.has_value(), "construct the real cache and catalog");
			Caches   = std::move(*madeCache);
			Catalogs = std::move(*madeCatalog);
			start();
		}

		// Starts a host over the same cache and catalog, as a relaunch would.
		void start()
		{
			Host = std::make_unique<application::epoch_application_host>(
				core::epoch_state{ .ApplicationSession = core::application_session_id{ 91U } }, *Caches, *Catalogs,
				Entropy, Clock, Effects, Decisions, Projection, application::epoch_host_limits{ });
		}

		// Points the pull request ref at `aHead` and hands the host the
		// revision a GitHub read of that snapshot would accept.
		void accept(const std::uint64_t aSnapshot, const std::string& aHead, std::optional<std::string> aForgedHave = { })
		{
			const auto gitDir = "--git-dir=" + QString::fromStdString(History.Repository.string());
			static_cast<void>(git_text(Root.Path, { gitDir, "update-ref", "refs/pull/81/head", QString::fromStdString(aHead) }));
			const auto baseRef = application::parse_git_base_branch_ref("refs/heads/main");
			require(baseRef.has_value(), "fixture base ref parses");
			auto handoff = application::accepted_pull_request_handoff{
				.ApplicationSession    = core::application_session_id{ 91U },
				.Profile               = core::host_profile_id{ 11U },
				.ProfileRevision       = core::github_profile_revision{ 21U },
				.Host                  = core::git_host_id{ 31U },
				.HostAuthority         = identity<core::host_authority_fingerprint>(2U),
				.Hostname              = application::github_canonical_hostname{ "github.com" },
				.Account               = core::github_account_id{ 41U },
				.Viewer                = core::github_viewer_actor_id{ 51U },
				.Repository            = core::repository_id{ 61U },
				.Locator               = { .Owner = "octo-org", .Name = "haves-fixture" },
				.PullRequest           = core::pull_request_id{ 71U },
				.Number                = core::pull_request_number{ 81U },
				.Snapshot              = core::provider_snapshot_id{ aSnapshot },
				.IdentityGeneration    = core::github_identity_generation{ 3U },
				.PullRequestGeneration = core::request_generation{ 3U + aSnapshot },
				.Origin                = core::pull_request_revision_origin::live,
				.ObjectFormat          = core::git_hash_algorithm::sha1,
				.BaseBranch            = identity<core::base_branch_id>(30U),
				.BaseRef               = *baseRef,
				.Base                  = commit_oid(History.Base),
				.Head                  = commit_oid(aHead),
			};
			handoff.SmartGit = std::make_unique<upload_pack_authority>(
				application::authority_binding(handoff),
				[this, forged = std::move(aForgedHave)](const application::git_smart_request_context& aContext) {
					return std::make_shared<upload_pack_transport>(aContext, Root.Path, History.Repository, forged, Fetches);
				});
			require(application::valid(handoff), "fixture handoff is valid");
			Authority = { handoff.ApplicationSession, handoff.Profile, handoff.ProfileRevision, handoff.Host, handoff.HostAuthority, handoff.Account, handoff.Viewer, handoff.Repository, handoff.PullRequest, handoff.Number, handoff.Snapshot, handoff.IdentityGeneration, handoff.PullRequestGeneration };
			Host->accept(std::move(handoff));
			std::size_t turns = 0U;
			while (!Effects.empty() || !Decisions.empty()) {
				require(++turns <= 2'000U, "composed host reaches quiescence");
				if (!Effects.empty()) {
					Effects.run_next();
				}
				if (!Decisions.empty()) {
					Decisions.run_next();
				}
			}
		}

		[[nodiscard]] auto latest_live_head() -> std::optional<core::git_commit_oid>
		{
			Host->shutdown();
			while (!Effects.empty() || !Decisions.empty()) {
				if (!Effects.empty()) {
					Effects.run_next();
				}
				if (!Decisions.empty()) {
					Decisions.run_next();
				}
			}
			require(Host->is_quiescent(), "composed host shuts down quiescently");
			Host.reset();
			auto catalog = Catalogs->open({ .ApplicationSession = core::application_session_id{ 91U }, .Authority = catalog_key() }, { });
			require(catalog && *catalog, "reopen the real catalog");
			const auto snapshot = (*catalog)->snapshot({ });
			require(snapshot.has_value(), "read the real catalog snapshot");
			return snapshot->LatestLive ? std::optional{ snapshot->LatestLive->Head } : std::nullopt;
		}

		// Indexes a served pack, with the product's argv, into a scratch
		// repository holding exactly `aResident`'s closure, and returns
		// index-pack's exit code.
		[[nodiscard]] auto index_pack_exit(const QByteArray& aPack, const std::string& aResident, const std::string_view aLabel) -> int
		{
			const auto scratch = Root.Path / "server" / (std::string{ aLabel } + ".git");
			static_cast<void>(git_text(Root.Path, { "init", "--bare", "--quiet", QString::fromStdString(scratch.string()) }));
			const auto source   = "--git-dir=" + QString::fromStdString(History.Repository.string());
			const auto target   = "--git-dir=" + QString::fromStdString(scratch.string());
			const auto resident = run_git(Root.Path, { source, "pack-objects", "--stdout", "--revs" }, QByteArray::fromStdString(aResident + "\n"));
			require(resident.ExitCode == 0, "pack the resident closure");
			static_cast<void>(git_text(Root.Path, { target, "index-pack", "--stdin", "--strict" }, resident.Output));
			return run_git(Root.Path, { target, "index-pack", "--stdin", "--strict", "--check-self-contained-and-connected" }, aPack).ExitCode;
		}

		application::workbench_pull_request_authority        Authority;
		temporary_root                                       Root;
		server_history                                       History;
		std::unique_ptr<application::git_cache_port>         Caches;
		std::unique_ptr<application::epoch_catalog_port>     Catalogs;
		deterministic_entropy                                Entropy;
		deterministic_clock                                  Clock;
		queued_executor                                      Effects;
		queued_executor                                      Decisions;
		recording_projection                                 Projection;
		std::unique_ptr<application::epoch_application_host> Host;
		std::vector<served_fetch>                            Fetches;
	};

	void verify_tour_pinned_source()
	{
		composed_live_path value{ "tour-source" };
		value.accept(1U, value.History.Head1);
		value.accept(2U, value.History.Head2);
		require(value.Host->state().Status == core::epoch_status::ready, "tour source has a real retained epoch");
		const auto worktree = value.Root.Path / "dirty checkout";
		static_cast<void>(git_text(value.Root.Path, { "clone", "--quiet", QString::fromStdString(value.History.Repository.string()), QString::fromStdString(worktree.string()) }));
		QFile dirty{ QString::fromStdString((worktree / "README").string()) };
		require(dirty.open(QIODevice::WriteOnly), "open dirty checkout text");
		dirty.write("WRONG dirty worktree text\n");
		dirty.close();
		const auto oldDirectory = QDir::currentPath();
		require(QDir::setCurrent(QString::fromStdString(worktree.string())), "enter dirty worktree for source check");
		auto read = [&](const std::string& commit, std::string path, core::diff_side side, std::uint32_t last = 0) {
			const auto                        oid = commit_oid(commit);
			const core::tour_code_destination destination{ .Commit = oid, .Path = path, .Side = side, .LastLine = last };
			const auto                        context = application::review_tour_source_context(*value.Host, value.Authority, destination);
			require(context.has_value(), "pinned source context is admitted by the shared service");
			const std::u8string                                native{ reinterpret_cast<const char8_t*>(path.data()), path.size() };
			const application::workbench_selected_diff_request request{ .Context = { core::request_id{ 1 }, value.Authority.ApplicationSession, core::request_generation{ 1 } }, .WorkbenchAuthority = value.Authority, .Diff = *context, .File = { .File = core::file_identity_of(path), .PresentationPath = path, .CurrentPath = std::filesystem::path{ native }, .Change = application::workbench_file_change_kind::unchanged, .ContentKind = application::workbench_file_content_kind::text } };
			auto                                               result = std::async(std::launch::async, [&] { return application::materialize_review_tour_source(*value.Host, *value.Host, request, destination, { }); });
			while (result.wait_for(std::chrono::milliseconds{ 0 }) != std::future_status::ready) {
				if (!value.Effects.empty()) {
					value.Effects.run_next();
				}
				if (!value.Decisions.empty()) {
					value.Decisions.run_next();
				}
				std::this_thread::yield();
			}
			return result.get();
		};
		for (const auto side : { core::diff_side::left, core::diff_side::right }) {
			const auto commit = side == core::diff_side::left ? value.History.Base : value.History.Head2;
			auto       result = read(commit, "README", side);
			require(result && result->Text == "fixture base\n" && result->Source.Commit == commit_oid(commit) && result->Source.First.Side == side && result->Source.Blob.has_value(), "left/current unchanged source is pinned and ignores dirty checkout");
		}
		auto current = read(value.History.Head2, "added.txt", core::diff_side::right);
		require(current && current->Text == "added by the second head\n", "current source uses current commit");
		auto historical = read(value.History.Head1, "src/nested.txt", core::diff_side::left);
		require(historical && historical->Text == "nested file kept by every head\n" && historical->Source.Commit == commit_oid(value.History.Head1), "historical left destination resolves the retained historical commit");
		auto unicode = read(value.History.Head2, "資料 α.txt", core::diff_side::right);
		require(unicode && unicode->Source.Path == "資料 α.txt" && unicode->Text == "fixture base\n", "Unicode source path survives actual Git resolution");
		require(!read(value.History.Head2, "missing.txt", core::diff_side::right), "missing source is unavailable instead of worktree fallback");
		auto range = read(value.History.Head2, "README", core::diff_side::right, 1);
		require(!range && range.error() == application::review_tour_source_failure::range_unavailable, "source range cannot silently truncate");
		require(QDir::setCurrent(oldDirectory), "restore source test directory");
		require(value.latest_live_head() == commit_oid(value.History.Head2), "source reads leave epoch authority unchanged and quiescent");
		std::cout << "tour-pinned-source:pass\n";
	}

	// The update of a pull request whose earlier head is resident loads.
	auto verify_update_over_haves_loads() -> void
	{
		composed_live_path value{ "update" };
		value.accept(1U, value.History.Head1);
		require(value.Host->state().Status == core::epoch_status::ready, "the first head loads");
		require(value.Fetches.size() == 1U && !value.Fetches.front().SentHaves, "the first fetch offers no haves");

		value.accept(2U, value.History.Head2);
		require(value.Fetches.size() == 2U && value.Fetches.back().SentHaves, "the update's fetch offers the resident head as a have");
		const auto exit = value.index_pack_exit(value.Fetches.back().Pack, value.History.Head1, "update-check");
		std::cout << "UPDATE_SERVED_PACK_INDEX_PACK_EXIT:" << exit << '\n';
		require(exit == 1, "the server trimmed the update's pack: complete, but not self-contained");
		const auto status = value.Host->state().Status;
		std::cout << "UPDATE_EPOCH_STATUS:" << static_cast<int>(status) << '\n';
		require(status == core::epoch_status::ready, "the updated head loads over a transport that honours haves");
		require(value.latest_live_head() == commit_oid(value.History.Head2), "the catalog's latest live head is the update");
		std::cout << "haves-update-loads:pass\n";
	}

	// A pack that lacks an object the cache does not hold is refused as
	// index-pack's failure, the earlier head stays the latest live revision,
	// and the cache stays usable (BPR-0851).
	auto verify_missing_object_refused() -> void
	{
		composed_live_path value{ "missing" };
		value.accept(1U, value.History.Base);
		require(value.Host->state().Status == core::epoch_status::ready, "the base-only head loads");
		value.Projection.Reasons.clear();
		value.accept(2U, value.History.Head2, value.History.Head1);
		require(value.Fetches.size() == 2U, "the update fetched");
		const auto exit = value.index_pack_exit(value.Fetches.back().Pack, value.History.Base, "missing-check");
		std::cout << "MISSING_SERVED_PACK_INDEX_PACK_EXIT:" << exit << '\n';
		require(exit != 0 && exit != 1, "the forged have left the served pack missing an object");
		require(value.Host->state().Status != core::epoch_status::ready || value.Host->state().AcceptedRevision->Source.Snapshot != core::provider_snapshot_id{ 2U },
				"a pack with a missing object never loads its head");
		std::cout << "MISSING_REFUSAL_REASONS:";
		for (const auto reason : value.Projection.Reasons) {
			std::cout << static_cast<int>(reason) << ' ';
		}
		std::cout << '\n';
		require(value.Projection.saw(core::epoch_reason_code::process_failure) && !value.Projection.saw(core::epoch_reason_code::repair_required),
				"the refusal is index-pack's process failure, not a repair");
		require(index_residue(value.Root.Path / "git") == 0U, "the refused import leaves no index-pack temporary file");
		require(value.latest_live_head() == commit_oid(value.History.Base), "the refused update leaves the earlier head latest");

		value.start();
		value.Projection.Reasons.clear();
		value.accept(3U, value.History.Head2);
		require(value.Fetches.size() == 3U, "the later update fetched");
		require(value.Host->state().Status == core::epoch_status::ready && !value.Projection.saw(core::epoch_reason_code::repair_required),
				"a later valid import loads without repair");
		require(value.latest_live_head() == commit_oid(value.History.Head2), "the later update is the latest live head");
		std::cout << "haves-missing-object-refused:pass\n";
	}

	struct cache_import
	{
		std::optional<application::git_prepared_revision>     Prepared;
		std::optional<application::git_cache_error>           Failure;
		bool                                                  Witnessed = false;
		std::optional<application::git_preimport_observation> Inventory;
	};

	auto import_pack(application::git_cache_session& aSession, const QByteArray& aPack, const std::uint8_t aSeed,
					 const core::git_commit_oid aHead, const bool aSparse, const core::epoch_catalog_revision aRevision) -> cache_import
	{
		const auto operation = identity<core::epoch_operation_id>(aSeed);
		auto       sink      = aSession.create_pack_sink(operation, static_cast<std::uint64_t>(aPack.size()), { });
		require(sink.has_value(), "create the import spool");
		const auto bytes = to_bytes(aPack);
		require((*sink)->append(bytes, { }).has_value(), "spool the pack");
		const auto receipt = (*sink)->finish({ });
		require(receipt.has_value(), "finish the import spool");
		sink->reset();
		const auto inventory = aSession.observe_preimport(*receipt, { });
		require(inventory.has_value(), "observe the preimport inventory");
		std::array<std::uint8_t, 32U> token{ };
		token.fill(aSeed);
		const auto request = application::git_prepare_revision_request{
			.Receipt       = *receipt,
			.Inventory     = *inventory,
			.Authorization = {
				.CatalogIncarnation      = identity<core::epoch_catalog_incarnation_id>(0x46U),
				.ExpectedCatalogRevision = aRevision,
				.Operation               = operation,
				.ExpectedPack            = receipt->ExpectedPack,
				.ReceiptToken            = receipt->OpaqueToken,
				.InventoryToken          = inventory->OpaqueToken,
				.DurableRecordToken      = token,
			},
			.Base = aHead,
			.Head = aHead,
		};
		cache_import result{ .Inventory = *inventory };
		const auto   witness = [&](const application::git_prepared_revision&) {
			result.Witnessed = true;
			return true;
		};
		auto prepared = aSparse ? aSession.prepare_sparse_revision(request, { }, witness) : aSession.prepare_revision(request, { }, witness);
		if (prepared) {
			result.Prepared = *prepared;
		} else {
			result.Failure = prepared.error();
		}
		return result;
	}

	// A complete-closure import whose pack references resident objects is
	// refused when that resident history is itself not connected: here the
	// first head is resident only as a sparse commit, its trees and README,
	// so the base commit and the nested blob are absent.
	auto verify_unconnected_residency_refused() -> void
	{
		temporary_root root{ "unconnected" };
		const auto     history = make_server_history(root.Path);
		const auto     source  = "--git-dir=" + QString::fromStdString(history.Repository.string());
		const auto     sparse  = run_git(root.Path, { source, "pack-objects", "--stdout" },
										 QByteArray::fromStdString(history.Head1 + "\n" + history.Head1Tree + "\n" + history.Readme + "\n" + history.Subtree + "\n"));
		const auto     trimmed = run_git(root.Path, { source, "pack-objects", "--stdout", "--revs" },
										 QByteArray::fromStdString(history.Head2 + "\n^" + history.Head1 + "\n"));
		require(sparse.ExitCode == 0 && trimmed.ExitCode == 0, "pack the sparse residency and the trimmed update");

		const auto scratch = root.Path / "server" / "unconnected-check.git";
		const auto target  = "--git-dir=" + QString::fromStdString(scratch.string());
		static_cast<void>(git_text(root.Path, { "init", "--bare", "--quiet", QString::fromStdString(scratch.string()) }));
		static_cast<void>(git_text(root.Path, { target, "index-pack", "--stdin", "--fsck-objects" }, sparse.Output));
		const auto exit = run_git(root.Path, { target, "index-pack", "--stdin", "--strict", "--check-self-contained-and-connected" }, trimmed.Output).ExitCode;
		std::cout << "UNCONNECTED_PACK_INDEX_PACK_EXIT:" << exit << '\n';
		require(exit == 1, "index-pack alone accepts the trimmed pack against the sparse residency");

		auto madeCache = git_adapter::make_installed_git_cache({
			.GitExecutable     = git_executable(),
			.StorageRoot       = root.Path / "git",
			.WindowsSystemRoot = bpr::tests::git_contract::windows_system_root(),
		});
		require(madeCache.has_value(), "construct the real cache");
		auto request = application::git_cache_open_request{
			.ApplicationSession = core::application_session_id{ 1U },
			.Authority          = {
				.Profile         = core::host_profile_id{ 2U },
				.ProfileRevision = core::github_profile_revision{ 3U },
				.Host            = core::git_host_id{ 4U },
				.HostAuthority   = identity<core::host_authority_fingerprint>(5U),
				.Account         = core::github_account_id{ 6U },
				.Viewer          = core::github_viewer_actor_id{ 7U },
				.Repository      = core::repository_id{ 8U },
				.Cache           = identity<core::repository_cache_scope_id>(0x46U),
				.PullRequest     = core::pull_request_id{ 9U },
				.Number          = core::pull_request_number{ 10U },
				.Scope           = identity<core::pull_request_authority_scope_id>(0x47U),
			},
			.ObjectFormat = core::git_hash_algorithm::sha1,
		};
		auto opened = (*madeCache)->open(request, { });
		require(opened.has_value(), "open the real cache");
		auto&      session = **opened;
		const auto first   = import_pack(session, sparse.Output, 0x50U, commit_oid(history.Head1), true, core::epoch_catalog_revision{ 0 });
		require(first.Prepared.has_value(), "the sparse residency imports");
		require(session.abort_operation(identity<core::epoch_operation_id>(0x50U), { }).has_value(), "release the sparse staging refs");

		const auto update = import_pack(session, trimmed.Output, 0x51U, commit_oid(history.Head2), false, core::epoch_catalog_revision{ 1 });
		std::cout << "UNCONNECTED_CACHE_FAILURE:"
				  << (update.Failure ? static_cast<int>(update.Failure->Code) : -1) << ':'
				  << (update.Failure ? update.Failure->Detail : std::string{ }) << '\n';
		require(!update.Prepared && update.Failure && !update.Witnessed,
				"a pack not connected to resident history is refused before anything is journaled");
		require(update.Failure->Code == application::git_cache_failure::process_failure,
				"the refusal is the connectivity verification's, and the new pack is rolled back");
		// fsck names the closure's gaps on standard output; the detail counts
		// them by kind and repeats no object name (BPR-0852).
		require(update.Failure->Detail.find("1 missing commit") != std::string::npos
					&& update.Failure->Detail.find(history.Base) == std::string::npos,
				"the connectivity refusal names the missing object's kind");
		const auto head = session.inspect_commit_object(commit_oid(history.Head2), { });
		require(!head.has_value() || !application::valid(*head), "the refused head is not resident");

		// The refusal is the closure's, not index-pack's: once the absent base
		// commit, its tree and the nested blob are resident, the same pack,
		// which still exits 1, imports.
		const auto baseTree = git_text(root.Path, { source, "rev-parse", QString::fromStdString(history.Base + "^{tree}") });
		const auto nested   = git_text(root.Path, { source, "rev-parse", QString::fromStdString(history.Subtree + ":nested.txt") });
		const auto missing  = run_git(root.Path, { source, "pack-objects", "--stdout" },
									  QByteArray::fromStdString(history.Base + "\n" + baseTree + "\n" + nested + "\n"));
		require(missing.ExitCode == 0, "pack the absent history");
		const auto completed = import_pack(session, missing.Output, 0x52U, commit_oid(history.Base), true, core::epoch_catalog_revision{ 1 });
		require(completed.Prepared.has_value(), "the absent history imports");
		require(session.abort_operation(identity<core::epoch_operation_id>(0x52U), { }).has_value(), "release the history staging refs");
		const auto connected = import_pack(session, trimmed.Output, 0x53U, commit_oid(history.Head2), false, core::epoch_catalog_revision{ 2 });
		std::cout << "CONNECTED_CACHE_FAILURE:" << (connected.Failure ? static_cast<int>(connected.Failure->Code) : -1) << '\n';
		require(connected.Prepared.has_value() && connected.Witnessed
					&& connected.Prepared->Residency == application::git_revision_residency::complete_closure,
				"the same not-self-contained pack imports once resident history is connected");
		std::cout << "haves-unconnected-residency-refused:pass\n";
	}
} // namespace

auto main(int argc, char** argv) -> int
try {
	QCoreApplication application{ argc, argv };
	const auto       course = argc == 2 ? std::string_view{ argv[1] } : std::string_view{ };
	if (course == "--tour-source") {
		verify_tour_pinned_source();
	}
	if (course.empty() || course == "--update") {
		verify_update_over_haves_loads();
	}
	if (course.empty() || course == "--missing-object") {
		verify_missing_object_refused();
	}
	if (course.empty() || course == "--unconnected") {
		verify_unconnected_residency_refused();
	}
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
