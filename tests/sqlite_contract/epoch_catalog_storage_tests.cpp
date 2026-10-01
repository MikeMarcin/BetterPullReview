#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/epoch_catalog.hpp"
#include "bpr/adapters/sqlite/testing.hpp"
#include "platform_test_support.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if defined(__linux__)
	#include <fcntl.h>
	#include <sys/file.h>
	#include <unistd.h>
#endif

namespace {
	namespace app           = bpr::application;
	namespace core          = bpr::core;
	namespace sqlite        = bpr::adapters::sqlite;
	namespace test_platform = bpr::tests::sqlite_contract;

	constexpr std::uint64_t HighProfile         = 0xF102030405060701ULL;
	constexpr std::uint64_t HighProfileRevision = 0xF102030405060702ULL;
	constexpr std::uint64_t HighHost            = 0xF102030405060703ULL;
	constexpr std::uint64_t HighAccount         = 0xF102030405060704ULL;
	constexpr std::uint64_t HighViewer          = 0xF102030405060705ULL;
	constexpr std::uint64_t HighRepository      = 0xF102030405060706ULL;
	constexpr std::uint64_t HighPullRequest     = 0xF102030405060707ULL;
	constexpr std::uint64_t HighSession         = 0xF102030405060708ULL;

	using bpr::tests::expect;

	template <typename Value>
	void expect_failure(
		const app::epoch_catalog_result<Value>& aResult,
		const app::epoch_catalog_failure        aFailure,
		const std::string_view                  aMessage)
	{
		expect(!aResult && aResult.error() == aFailure, aMessage);
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		bytes[0]   = aSeed;
		bytes[17]  = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return Identity{ bytes };
	}

	class temporary_root final
	{
	public:
		explicit temporary_root(const std::string_view aLabel)
		{
			const auto base = std::filesystem::temp_directory_path();
			for (std::uint32_t attempt = 0U; attempt < 1'000U; ++attempt) {
				const auto name = std::string{ "bpr-sqlite-" }
					+ std::string{ aLabel } + "-"
					+ std::to_string(test_platform::process_id())
					+ "-" + std::to_string(attempt);
				auto            candidate = base / name;
				std::error_code error;
				if (std::filesystem::create_directory(candidate, error)) {
					// [[BPR-1121]]. Removed again if the rest of construction fails.
					auto made = bpr::tests::scratch_guard{ candidate };
					expect(
						test_platform::make_owner_private(candidate, true),
						"temporary root becomes owner-private");
					PresentedPath = candidate;
					Path          = std::filesystem::canonical(candidate);
					made.release();
					return;
				}
				expect(
					!error
						|| error == std::errc::file_exists,
					"temporary root creation fails only for a collision");
			}
			expect(false, "a unique temporary root is available");
		}

		temporary_root(const temporary_root&)                    = delete;
		auto operator=(const temporary_root&) -> temporary_root& = delete;

		~temporary_root()
		{
			if (!Path.empty()) {
				std::error_code ignored;
				(void)test_platform::make_owner_private(Path, true);
				std::filesystem::remove_all(Path, ignored);
			}
		}

		[[nodiscard]] auto path() const -> const std::filesystem::path&
		{
			return Path;
		}

		[[nodiscard]] auto presented_path() const
			-> const std::filesystem::path&
		{
			return PresentedPath;
		}

	private:
		std::filesystem::path PresentedPath;
		std::filesystem::path Path;
	};

	class direct_database final
	{
	public:
		explicit direct_database(const std::filesystem::path& aPath)
		{
			const auto native = test_platform::sqlite_path(aPath);
			const auto code   = ::sqlite3_open_v2(
				native.c_str(),
				&Handle,
				SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX
					| SQLITE_OPEN_NOFOLLOW,
				test_platform::sqlite_vfs());
			expect(
				code == SQLITE_OK && Handle != nullptr,
				"the catalog is directly inspectable through pinned SQLite");
		}

		direct_database(const direct_database&)                    = delete;
		auto operator=(const direct_database&) -> direct_database& = delete;

		~direct_database()
		{
			if (Handle != nullptr) {
				// [[BPR-1121]]. A destructor must not throw, so a failure here exits.
				const bpr::tests::exit_on_failure inDestructor;
				expect(
					::sqlite3_close_v2(Handle) == SQLITE_OK,
					"direct SQLite inspection closes cleanly");
			}
		}

		void execute(const std::string_view aSql)
		{
			char*      message   = nullptr;
			const auto statement = std::string{ aSql };
			const auto code      = ::sqlite3_exec(
				Handle, statement.c_str(), nullptr, nullptr, &message);
			if (code != SQLITE_OK) {
				const auto detail = message == nullptr ? std::string{ }
													   : std::string{ message };
				::sqlite3_free(message);
				std::cerr << "FAIL: SQLite execution: " << detail << '\n';
				bpr::tests::fail();
			}
		}

		[[nodiscard]] auto execute_code(const std::string_view aSql) const -> int
		{
			const auto statement = std::string{ aSql };
			char*      message   = nullptr;
			const auto code      = ::sqlite3_exec(
				Handle, statement.c_str(), nullptr, nullptr, &message);
			::sqlite3_free(message);
			return code;
		}

		[[nodiscard]] auto foreign_key_violation_count() const -> std::size_t
		{
			::sqlite3_stmt*            statement = nullptr;
			constexpr std::string_view query     = "PRAGMA foreign_key_check";
			expect(
				::sqlite3_prepare_v3(
					Handle,
					query.data(),
					static_cast<int>(query.size()),
					0,
					&statement,
					nullptr)
					== SQLITE_OK,
				"foreign-key inspection prepares");
			auto count = std::size_t{ 0 };
			auto code  = ::sqlite3_step(statement);
			while (code == SQLITE_ROW) {
				++count;
				code = ::sqlite3_step(statement);
			}
			expect(
				code == SQLITE_DONE && ::sqlite3_finalize(statement) == SQLITE_OK,
				"foreign-key inspection completes");
			return count;
		}

		[[nodiscard]] auto integer(const std::string_view aSql) const
			-> std::int64_t
		{
			::sqlite3_stmt* statement = nullptr;
			const auto      query     = std::string{ aSql };
			expect(
				::sqlite3_prepare_v3(
					Handle,
					query.c_str(),
					static_cast<int>(query.size()),
					SQLITE_PREPARE_NO_VTAB,
					&statement,
					nullptr)
					== SQLITE_OK,
				"integer inspection query prepares");
			expect(
				::sqlite3_step(statement) == SQLITE_ROW
					&& ::sqlite3_column_type(statement, 0) == SQLITE_INTEGER,
				"integer inspection query returns one integer");
			const auto value = ::sqlite3_column_int64(statement, 0);
			expect(
				::sqlite3_step(statement) == SQLITE_DONE,
				"integer inspection query returns one row");
			expect(
				::sqlite3_finalize(statement) == SQLITE_OK,
				"integer inspection query finalizes");
			return value;
		}

		[[nodiscard]] auto text(const std::string_view aSql) const -> std::string
		{
			::sqlite3_stmt* statement = nullptr;
			const auto      query     = std::string{ aSql };
			expect(
				::sqlite3_prepare_v3(
					Handle,
					query.c_str(),
					static_cast<int>(query.size()),
					SQLITE_PREPARE_NO_VTAB,
					&statement,
					nullptr)
					== SQLITE_OK,
				"text inspection query prepares");
			expect(
				::sqlite3_step(statement) == SQLITE_ROW
					&& ::sqlite3_column_type(statement, 0) == SQLITE_TEXT,
				"text inspection query returns one text value");
			const auto* raw = reinterpret_cast<const char*>(
				::sqlite3_column_text(statement, 0));
			const auto count = ::sqlite3_column_bytes(statement, 0);
			expect(raw != nullptr && count >= 0, "text result is readable");
			auto value = std::string{ raw, static_cast<std::size_t>(count) };
			expect(
				::sqlite3_step(statement) == SQLITE_DONE,
				"text inspection query returns one row");
			expect(
				::sqlite3_finalize(statement) == SQLITE_OK,
				"text inspection query finalizes");
			return value;
		}

		void expect_no_text_cells() const
		{
			::sqlite3_stmt*            objects = nullptr;
			constexpr std::string_view query   = "SELECT name FROM sqlite_schema WHERE type = 'table' "
												 "AND name NOT LIKE 'sqlite_%' ORDER BY name";
			expect(
				::sqlite3_prepare_v3(
					Handle,
					query.data(),
					static_cast<int>(query.size()),
					SQLITE_PREPARE_NO_VTAB,
					&objects,
					nullptr)
					== SQLITE_OK,
				"table inventory prepares");
			std::vector<std::string> tables;
			while (::sqlite3_step(objects) == SQLITE_ROW) {
				const auto* raw = reinterpret_cast<const char*>(
					::sqlite3_column_text(objects, 0));
				const auto count = ::sqlite3_column_bytes(objects, 0);
				expect(raw != nullptr && count > 0, "table name is readable");
				auto name = std::string{ raw, static_cast<std::size_t>(count) };
				for (const auto character : name) {
					expect(
						(character >= 'a' && character <= 'z')
							|| character == '_',
						"schema table names are trusted identifiers");
				}
				tables.push_back(std::move(name));
			}
			expect(
				::sqlite3_errcode(Handle) == SQLITE_DONE
					|| ::sqlite3_errcode(Handle) == SQLITE_OK,
				"table inventory completes");
			expect(
				::sqlite3_finalize(objects) == SQLITE_OK,
				"table inventory finalizes");

			for (const auto& table : tables) {
				const auto      scanQuery = "SELECT * FROM \"" + table + "\"";
				::sqlite3_stmt* rows      = nullptr;
				expect(
					::sqlite3_prepare_v3(
						Handle,
						scanQuery.c_str(),
						static_cast<int>(scanQuery.size()),
						SQLITE_PREPARE_NO_VTAB,
						&rows,
						nullptr)
						== SQLITE_OK,
					"typed table scan prepares");
				while (::sqlite3_step(rows) == SQLITE_ROW) {
					const auto columns = ::sqlite3_column_count(rows);
					for (int column = 0; column < columns; ++column) {
						expect(
							::sqlite3_column_type(rows, column) != SQLITE_TEXT,
							"durable catalog rows contain no strings");
					}
				}
				expect(
					::sqlite3_finalize(rows) == SQLITE_OK,
					"typed table scan finalizes");
			}
		}

	private:
		::sqlite3* Handle = nullptr;
	};

	[[nodiscard]] auto authority_key(
		const std::uint64_t aRepository  = HighRepository,
		const std::uint64_t aPullRequest = HighPullRequest,
		const std::uint32_t aNumber      = 7001U)
		-> app::epoch_catalog_authority_key
	{
		return {
			.Profile         = core::host_profile_id{ HighProfile },
			.ProfileRevision = core::github_profile_revision{ HighProfileRevision },
			.Host            = core::git_host_id{ HighHost },
			.HostAuthority   = opaque<core::host_authority_fingerprint>(0x31U),
			.Account         = core::github_account_id{ HighAccount },
			.Viewer          = core::github_viewer_actor_id{ HighViewer },
			.Repository      = core::repository_id{ aRepository },
			.PullRequest     = core::pull_request_id{ aPullRequest },
			.Number          = core::pull_request_number{ aNumber },
			.ObjectFormat    = core::git_hash_algorithm::sha1,
		};
	}

	[[nodiscard]] auto open_request(
		const app::epoch_catalog_authority_key& aAuthority,
		const std::uint64_t                     aSession = HighSession)
		-> app::epoch_catalog_open_request
	{
		return {
			.ApplicationSession = core::application_session_id{ aSession },
			.Authority          = aAuthority,
		};
	}

	[[nodiscard]] auto database_path(const std::filesystem::path& aRoot)
		-> std::filesystem::path
	{
		return aRoot / "state" / "v1" / "epoch-catalog.sqlite3";
	}

	[[nodiscard]] auto hex_u64(const std::uint64_t aValue) -> std::string
	{
		constexpr std::string_view digits = "0123456789ABCDEF";
		auto                       output = std::string{ };
		output.reserve(16U);
		for (int shift = 56; shift >= 0; shift -= 8) {
			const auto byte = static_cast<std::uint8_t>(aValue >> static_cast<unsigned>(shift));
			output.push_back(digits[byte >> 4U]);
			output.push_back(digits[byte & 0x0FU]);
		}
		return output;
	}

	template <typename Identity>
	[[nodiscard]] auto hex_identity(const Identity& aIdentity) -> std::string
	{
		constexpr std::string_view digits = "0123456789ABCDEF";
		auto                       output = std::string{ };
		output.reserve(aIdentity.bytes().size() * 2U);
		for (const auto byte : aIdentity.bytes()) {
			output.push_back(digits[byte >> 4U]);
			output.push_back(digits[byte & 0x0FU]);
		}
		return output;
	}

	[[nodiscard]] auto read_binary(const std::filesystem::path& aPath)
		-> std::string
	{
		auto input = std::ifstream{ aPath, std::ios::binary };
		expect(input.good(), "catalog bytes are readable for canary scan");
		return {
			std::istreambuf_iterator<char>{ input },
			std::istreambuf_iterator<char>{ }
		};
	}

#if defined(__linux__)
	// BPR-1257. On Linux a catalog keeps `state/v1` open, and its `flock`, for
	// as long as the catalog or any session lives, so reopening the root while
	// one is alive waits on this process forever. The probe does not block, so
	// a session left alive fails by name here rather than at the row's timeout.
	auto catalog_root_released(const std::filesystem::path& aRoot) -> bool
	{
		const auto version = aRoot / "state" / "v1";
		const auto handle  = ::open(version.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
		if (handle < 0) {
			return false;
		}
		const auto released = ::flock(handle, LOCK_EX | LOCK_NB) == 0;
		(void)::close(handle);
		return released;
	}
#endif

	void bootstrap_authority_and_storage_contract()
	{
		temporary_root root{
			"provider-hostname-url-json-login-body-token-canary"
		};
		auto made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "owner-private root bootstraps a catalog");
		auto port = std::move(*made);

		const auto authority           = authority_key();
		auto       historicalRequest   = open_request(authority);
		historicalRequest.ExistingOnly = true;
		expect(!port->open(historicalRequest, { }), "historical open cannot create absent cache or PR scope");
		auto opened = port->open(open_request(authority), { });
		expect(opened.has_value(), "complete high-bit authority opens");
		auto       first        = std::move(*opened);
		const auto firstBinding = first->binding();
		auto       historical   = port->open(historicalRequest, { });
		expect(historical && (*historical)->binding() == firstBinding, "historical open reuses exact retained identities without revision changes");
		historicalRequest.Authority.PullRequest = core::pull_request_id{ 999 };
		expect(!port->open(historicalRequest, { }), "historical open cannot allocate a PR in an existing repository");
		expect(app::valid(firstBinding), "allocated catalog binding is valid");
		expect(
			firstBinding.Authority.Cache.valid()
				&& firstBinding.Authority.Scope.valid(),
			"catalog allocates opaque cache and PR scopes");
		expect(
			firstBinding.Authority.Cache.bytes()
				!= firstBinding.Authority.Scope.bytes(),
			"cache and PR scopes are distinct identities");
		expect(
			firstBinding.Catalog.Incarnation.valid()
				&& firstBinding.Catalog.Revision.value() == 0,
			"new catalog has nonzero incarnation and revision zero");

		auto sameOpen = port->open(open_request(authority, HighSession + 1U), { });
		expect(sameOpen.has_value(), "same authority reopens in-process");
		auto same = std::move(*sameOpen);
		expect(
			same->binding().Authority.Cache == firstBinding.Authority.Cache
				&& same->binding().Authority.Scope
					== firstBinding.Authority.Scope
				&& same->binding().Catalog == firstBinding.Catalog,
			"same authority resolves exact allocated identities");

		auto secondPrOpen = port->open(
			open_request(
				authority_key(HighRepository, HighPullRequest + 1U, 7002U),
				HighSession + 2U),
			{ });
		expect(secondPrOpen.has_value(), "second PR authority opens");
		auto secondPr = std::move(*secondPrOpen);
		expect(
			secondPr->binding().Authority.Cache
					== firstBinding.Authority.Cache
				&& secondPr->binding().Authority.Scope
					!= firstBinding.Authority.Scope,
			"same repository shares only its cache scope");

		auto secondRepositoryOpen = port->open(
			open_request(
				authority_key(
					HighRepository + 1U,
					HighPullRequest + 2U,
					7003U),
				HighSession + 3U),
			{ });
		expect(
			secondRepositoryOpen.has_value(),
			"second repository authority opens");
		auto secondRepository = std::move(*secondRepositoryOpen);
		expect(
			secondRepository->binding().Authority.Cache
					!= firstBinding.Authority.Cache
				&& secondRepository->binding().Authority.Scope
					!= firstBinding.Authority.Scope,
			"different repository receives distinct scopes");

		auto mismatched   = authority;
		mismatched.Number = core::pull_request_number{ 7011U };
		expect_failure(
			port->open(open_request(mismatched, HighSession + 4U), { }),
			app::epoch_catalog_failure::authority_mismatch,
			"existing PR ID cannot be rebound to another number");

		const auto dbPath = database_path(root.path());
		expect(
			std::filesystem::is_regular_file(dbPath),
			"catalog uses the fixed state/v1 database path");
		expect(
			test_platform::is_owner_private(root.path(), true),
			"storage root remains owner-private");
		expect(
			test_platform::is_owner_private(root.path() / "state", true)
				&& test_platform::is_owner_private(
					root.path() / "state" / "v1", true),
			"catalog directories are owner-private");
		expect(
			test_platform::is_owner_private(dbPath, false),
			"catalog file is owner-private");

		{
			direct_database database{ dbPath };
			expect(
				database.integer("PRAGMA application_id") == 0x42504543,
				"BPEC application ID is exact");
			expect(
				database.integer("PRAGMA user_version") == 7,
				"BPEC user version is exact");
			expect(
				database.integer("PRAGMA page_size") == 4'096,
				"catalog fixes the SQLite page size at four KiB");
			expect(
				database.integer("PRAGMA max_page_count") == 262'144,
				"catalog fixes the database ceiling at one GiB");
			expect(
				database.text("PRAGMA journal_mode") == "wal",
				"catalog selects WAL mode");
			expect(
				database.integer(
					"SELECT count(*) FROM sqlite_schema "
					"WHERE type IN ('view', 'trigger')")
					== 0,
				"catalog has no views or triggers");
			expect(
				database.integer(
					"SELECT count(*) FROM sqlite_schema WHERE type = 'table' "
					"AND name NOT LIKE 'sqlite_%'")
						== 24
					&& database.integer(
						   "SELECT count(*) FROM sqlite_schema "
						   "WHERE type = 'index' AND sql IS NOT NULL "
						   "AND name NOT LIKE 'sqlite_%'")
						== 17
					&& database.integer(
						   "SELECT count(*) FROM sqlite_schema "
						   "WHERE type = 'index' "
						   "AND name = 'operation_source_observation' "
						   "AND instr(sql, 'WHERE kind = 1') != 0")
						== 1
					&& database.integer(
						   "SELECT count(*) FROM sqlite_schema "
						   "WHERE type = 'table' AND name = 'append_install' "
						   "AND instr(sql, 'head_lineage INTEGER NOT NULL "
						   "CHECK(head_lineage BETWEEN 1 AND 4)') != 0")
						== 1,
				"closed BPEC v7 schema includes the append-source identity index, "
				"the access-mode uniqueness index, and checked durable head "
				"lineage");
			expect(
				database.integer(
					"SELECT count(*) FROM sqlite_schema WHERE sql IS NOT NULL "
					"AND instr(upper(sql), ' TEXT') != 0")
					== 0,
				"catalog schema exposes no text-bearing durable columns");

			const auto highRepositoryQuery = std::string{
				"SELECT count(*) FROM repository_scope WHERE "
			} + "hex(profile_id)='"
				+ hex_u64(HighProfile) + "' AND " + "hex(profile_revision)='" + hex_u64(HighProfileRevision) + "' AND hex(host_id)='" + hex_u64(HighHost) + "' AND hex(account_id)='" + hex_u64(HighAccount) + "' AND hex(repository_id)='" + hex_u64(HighRepository) + "' AND typeof(profile_id)='blob' "
																																																																		"AND typeof(repository_id)='blob'";
			expect(
				database.integer(highRepositoryQuery) == 1,
				"unsigned authority values above INT64_MAX round-trip as BLOB8");
			const auto highPrQuery = std::string{
				"SELECT count(*) FROM pull_request_scope WHERE "
			} + "hex(viewer_id)='"
				+ hex_u64(HighViewer) + "' AND hex(pull_request_id)='" + hex_u64(HighPullRequest) + "' AND typeof(viewer_id)='blob' "
																									"AND typeof(pull_request_id)='blob'";
			expect(
				database.integer(highPrQuery) == 1,
				"viewer and PR unsigned identities round-trip as BLOB8");
			expect(
				database.text(
					"SELECT hex(incarnation) FROM catalog_meta "
					"WHERE singleton = 1")
					== hex_identity(firstBinding.Catalog.Incarnation),
				"public binding carries the exact persisted incarnation");
			expect(
				database.text(
					"SELECT hex(cache_scope) FROM repository_scope "
					"WHERE repository_id = x'F102030405060706'")
					== hex_identity(firstBinding.Authority.Cache),
				"public binding carries the exact persisted cache scope");
			expect(
				database.text(
					"SELECT hex(pr_scope) FROM pull_request_scope "
					"WHERE pull_request_id = x'F102030405060707'")
					== hex_identity(firstBinding.Authority.Scope),
				"public binding carries the exact persisted PR scope");
			database.expect_no_text_cells();
		}

		expect(
			sqlite::detail::testing::classify_sqlite_result_code(SQLITE_FULL)
				== app::epoch_catalog_failure::quota_exceeded,
			"the test-only pure classifier maps SQLITE_FULL to quota_exceeded");

		const auto forbidden = "provider-hostname-url-json-login-body-token-canary";
		const auto bytes     = read_binary(dbPath);
		expect(
			bytes.find(forbidden) == std::string::npos
				&& bytes.find(test_platform::sqlite_path(root.path()))
					== std::string::npos,
			"database bytes contain neither provider/string canary nor path");
		const auto walPath = test_platform::with_ascii_suffix(dbPath, "-wal");
		if (std::filesystem::exists(walPath)) {
			const auto wal = read_binary(walPath);
			expect(
				wal.find(forbidden) == std::string::npos
					&& wal.find(test_platform::sqlite_path(root.path()))
						== std::string::npos,
				"WAL bytes contain neither provider/string canary nor path");
		}

		secondRepository.reset();
		secondPr.reset();
		same.reset();
		historical->reset();
		first.reset();
		port.reset();

#if defined(__linux__)
		expect(
			catalog_root_released(root.path()),
			"every catalog and session on the root is released before it reopens");
#endif
		auto reopenedPortResult = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(reopenedPortResult.has_value(), "WAL catalog reopens");
		auto reopenedPort   = std::move(*reopenedPortResult);
		auto reopenedResult = reopenedPort->open(
			open_request(authority, HighSession + 5U), { });
		expect(reopenedResult.has_value(), "authority reopens after restart");
		auto reopened = std::move(*reopenedResult);
		expect(
			reopened->binding() == firstBinding,
			"incarnation, revision, and allocated scopes survive restart");
	}

	void cancellation_and_path_contract()
	{
		temporary_root   cancelledRoot{ "cancelled" };
		std::stop_source cancelled;
		cancelled.request_stop();
		expect_failure(
			sqlite::make_epoch_catalog(
				{ .StorageRoot = cancelledRoot.path() }, cancelled.get_token()),
			app::epoch_catalog_failure::cancelled,
			"pre-cancelled bootstrap returns typed cancellation");
		expect(
			!std::filesystem::exists(cancelledRoot.path() / "state"),
			"cancelled bootstrap creates no catalog state");

		expect_failure(
			sqlite::make_epoch_catalog({ .StorageRoot = "relative/catalog" }),
			app::epoch_catalog_failure::invalid_path,
			"relative storage roots fail closed");

#if defined(_WIN32)
		auto forward_spelling = cancelledRoot.path().native();
		std::ranges::replace(forward_spelling, L'\\', L'/');
		expect_failure(
			sqlite::make_epoch_catalog(
				{ .StorageRoot = std::filesystem::path{ forward_spelling } }),
			app::epoch_catalog_failure::invalid_path,
			"forward-separated Windows root fails closed");
		expect_failure(
			sqlite::make_epoch_catalog(
				{ .StorageRoot = std::filesystem::path{ L"C:relative" } }),
			app::epoch_catalog_failure::invalid_path,
			"drive-relative Windows root fails closed");
		expect_failure(
			sqlite::make_epoch_catalog(
				{ .StorageRoot = std::filesystem::path{
					  L"\\\\localhost\\C$\\bpr-catalog" } }),
			app::epoch_catalog_failure::invalid_path, "UNC Windows root fails closed");
		expect_failure(
			sqlite::make_epoch_catalog(
				{ .StorageRoot = std::filesystem::path{
					  L"\\\\?\\C:\\bpr-catalog" } }),
			app::epoch_catalog_failure::invalid_path, "device-prefixed Windows root fails closed");
		expect_failure(
			sqlite::make_epoch_catalog(
				{ .StorageRoot = std::filesystem::path{
					  cancelledRoot.path().native() + L":alternate" } }),
			app::epoch_catalog_failure::invalid_path, "alternate-data-stream Windows root fails closed");
		expect_failure(
			sqlite::make_epoch_catalog(
				{ .StorageRoot = cancelledRoot.path() / L"reserved." }),
			app::epoch_catalog_failure::invalid_path,
			"trailing-dot Windows component fails closed");
		expect_failure(
			sqlite::make_epoch_catalog(
				{ .StorageRoot = cancelledRoot.path() / L"reserved " }),
			app::epoch_catalog_failure::invalid_path,
			"trailing-space Windows component fails closed");
		expect_failure(
			sqlite::make_epoch_catalog(
				{ .StorageRoot = cancelledRoot.path() / L"CON" }),
			app::epoch_catalog_failure::invalid_path,
			"reserved DOS device component fails closed");
		const auto dot_spelling = cancelledRoot.path().parent_path()
			/ L"." / cancelledRoot.path().filename();
		expect_failure(
			sqlite::make_epoch_catalog({ .StorageRoot = dot_spelling }),
			app::epoch_catalog_failure::invalid_path,
			"dot-containing Windows root fails closed");
		const auto dotdot_spelling = cancelledRoot.path()
			/ L"unused" / L"..";
		expect_failure(
			sqlite::make_epoch_catalog({ .StorageRoot = dotdot_spelling }),
			app::epoch_catalog_failure::invalid_path,
			"dot-dot-containing Windows root fails closed");
		const auto drive     = cancelledRoot.path().root_name().native();
		const auto overbound = std::filesystem::path{
			drive + L"\\" + std::wstring(33'000U, L'a')
		};
		expect_failure(
			sqlite::make_epoch_catalog({ .StorageRoot = overbound }),
			app::epoch_catalog_failure::invalid_path,
			"over-bound Windows root fails before filesystem access");
#endif

		temporary_root noncanonical{ "noncanonical" };
		if (noncanonical.presented_path() != noncanonical.path()) {
			expect_failure(
				sqlite::make_epoch_catalog(
					{ .StorageRoot = noncanonical.presented_path() }),
				app::epoch_catalog_failure::invalid_path,
				"absolute root containing a symlink component fails closed");
		}

		temporary_root root{ "cancelled-session" };
		auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "cancellation fixture bootstraps");
		auto port = std::move(*made);
		expect_failure(
			port->open(open_request(authority_key()), cancelled.get_token()),
			app::epoch_catalog_failure::cancelled,
			"pre-cancelled authority open returns typed cancellation");
		auto opened = port->open(open_request(authority_key()), { });
		expect(opened.has_value(), "uncancelled authority opens");
		auto session = std::move(*opened);
		expect_failure(
			session->snapshot(cancelled.get_token()),
			app::epoch_catalog_failure::cancelled,
			"pre-cancelled read returns typed cancellation");
	}

	void permission_and_symlink_contract()
	{
		temporary_root permissive{ "permissive" };
		expect(
			test_platform::make_permissive(permissive.path(), true),
			"permissive storage-root fixture applies");
		expect_failure(
			sqlite::make_epoch_catalog({ .StorageRoot = permissive.path() }),
			app::epoch_catalog_failure::invalid_permissions,
			"storage root with additional reader fails closed");
		expect(
			test_platform::make_owner_private(permissive.path(), true),
			"permissive storage-root fixture restores private access");

		temporary_root stateLink{ "state-link" };
		const auto     target = stateLink.path() / "outside-state";
		std::filesystem::create_directory(target);
		expect(
			test_platform::make_owner_private(target, true),
			"reparse target becomes owner-private");
#if defined(_WIN32)
		expect(
			test_platform::create_directory_junction(
				stateLink.path() / "state", target),
			"directory junction fixture creates without symlink privilege");
#else
		std::filesystem::create_directory_symlink(target, stateLink.path() / "state");
#endif
		expect_failure(
			sqlite::make_epoch_catalog({ .StorageRoot = stateLink.path() }),
			app::epoch_catalog_failure::invalid_path,
			"reparse-backed state directory fails closed");

		temporary_root databaseLink{ "database-link" };
		std::filesystem::create_directories(
			databaseLink.path() / "state" / "v1");
		expect(
			test_platform::make_owner_private(
				databaseLink.path() / "state", true)
				&& test_platform::make_owner_private(
					databaseLink.path() / "state" / "v1", true),
			"database-link directories become owner-private");
		const auto victim = databaseLink.path() / "victim";
		{
			auto output = std::ofstream{ victim };
			output << "must remain untouched";
		}
		expect(
			test_platform::make_owner_private(victim, false),
			"database-link victim becomes owner-private");
#if defined(_WIN32)
		const auto link_result = test_platform::create_file_symbolic_link(
			database_path(databaseLink.path()), victim);
		if (link_result
			== test_platform::symbolic_link_result::privilege_unavailable) {
			std::cout
				<< "SKIP: file-symlink rejection requires Windows symlink "
				   "privilege; directory-junction rejection remains covered\n";
		} else {
			expect(
				link_result == test_platform::symbolic_link_result::created,
				"Windows file-symlink fixture creates");
			expect_failure(
				sqlite::make_epoch_catalog({ .StorageRoot = databaseLink.path() }),
				app::epoch_catalog_failure::invalid_path,
				"symlinked database file fails closed");
		}
#else
		std::filesystem::create_symlink(
			victim, database_path(databaseLink.path()));
		expect_failure(
			sqlite::make_epoch_catalog({ .StorageRoot = databaseLink.path() }),
			app::epoch_catalog_failure::invalid_permissions,
			"symlinked database file fails closed");
#endif
		expect(
			read_binary(victim) == "must remain untouched",
			"symlink target is not opened or modified");

#if defined(_WIN32)
		temporary_root catalog_permissions{ "catalog-permissions" };
		{
			auto made = sqlite::make_epoch_catalog(
				{ .StorageRoot = catalog_permissions.path() });
			expect(made.has_value(), "catalog-permission fixture bootstraps");
		}
		expect(
			test_platform::make_permissive(
				database_path(catalog_permissions.path()), false),
			"permissive catalog-file DACL fixture applies");
		expect_failure(
			sqlite::make_epoch_catalog(
				{ .StorageRoot = catalog_permissions.path() }),
			app::epoch_catalog_failure::invalid_permissions,
			"catalog file with additional reader fails closed");
		expect(
			test_platform::make_owner_private(
				database_path(catalog_permissions.path()), false),
			"catalog-file DACL fixture restores private access");

		temporary_root hardlinked_catalog{ "catalog-hardlink" };
		{
			auto made = sqlite::make_epoch_catalog(
				{ .StorageRoot = hardlinked_catalog.path() });
			expect(made.has_value(), "hardlink fixture bootstraps");
		}
		const auto extra_link = hardlinked_catalog.path() / L"catalog-link.sqlite3";
		expect(
			::CreateHardLinkW(
				extra_link.c_str(),
				database_path(hardlinked_catalog.path()).c_str(),
				nullptr)
				!= 0,
			"second catalog hardlink creates");
		expect_failure(
			sqlite::make_epoch_catalog({ .StorageRoot = hardlinked_catalog.path() }),
			app::epoch_catalog_failure::invalid_path,
			"multiply-linked catalog fails closed");
		expect(::DeleteFileW(extra_link.c_str()) != 0, "hardlink fixture removes");
#endif
	}

#if defined(__linux__)
	void linux_live_catalog_directory_contract()
	{
		temporary_root root{ "linux-live-catalog-directory" };
		auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "Linux live-directory fixture bootstraps");
		auto port = std::move(*made);

		const auto state            = root.path() / "state";
		const auto version          = state / "v1";
		const auto retained_version = state / "v1-retained";
		std::filesystem::rename(version, retained_version);
		std::filesystem::create_directory(version);
		expect(
			test_platform::make_owner_private(version, true),
			"replacement catalog directory becomes owner-private");
		const auto replacement_catalog = version / "epoch-catalog.sqlite3";
		{
			auto output = std::ofstream{ replacement_catalog, std::ios::binary };
			output << "replacement-catalog-canary";
		}
		expect(
			test_platform::make_owner_private(replacement_catalog, false),
			"replacement catalog canary becomes owner-private");

		auto opened = port->open(open_request(authority_key()), { });
		expect(
			opened.has_value(),
			"live Linux catalog continues through the retained directory");
		expect(
			read_binary(replacement_catalog) == "replacement-catalog-canary",
			"replacement catalog namespace receives no SQLite effect");
		expect(
			std::filesystem::is_regular_file(
				retained_version / "epoch-catalog.sqlite3"),
			"live SQLite state remains in the retained catalog directory");
	}
#endif

#if defined(_WIN32)
	void windows_live_namespace_and_sidecar_contract()
	{
		const auto delete_access_denied = [](
											  const std::filesystem::path& path,
											  const bool                   directory) {
			test_platform::unique_handle attempted{ ::CreateFileW(
				path.c_str(),
				DELETE,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				nullptr,
				OPEN_EXISTING,
				directory ? FILE_FLAG_BACKUP_SEMANTICS : 0,
				nullptr) };
			if (attempted) {
				return false;
			}
			const auto error = ::GetLastError();
			return error == ERROR_SHARING_VIOLATION
				|| error == ERROR_ACCESS_DENIED;
		};
		const auto rename_denied = [](
									   const std::filesystem::path& from,
									   const std::filesystem::path& to) {
			if (::MoveFileExW(from.c_str(), to.c_str(), 0) != 0) {
				return false;
			}
			const auto error = ::GetLastError();
			return error == ERROR_SHARING_VIOLATION
				|| error == ERROR_ACCESS_DENIED;
		};

		temporary_root live_namespace{ "live-namespace" };
		auto           live = sqlite::make_epoch_catalog(
			{ .StorageRoot = live_namespace.path() });
		expect(live.has_value(), "live-namespace fixture bootstraps");
		const auto state        = live_namespace.path() / L"state";
		const auto version      = state / L"v1";
		const auto catalog      = database_path(live_namespace.path());
		const auto renamed_root = live_namespace.path().parent_path()
			/ std::filesystem::path{
				  live_namespace.path().filename().native() + L"-renamed"
			  };
		const auto renamed_state   = live_namespace.path() / L"state-renamed";
		const auto renamed_version = state / L"v1-renamed";
		const auto renamed_catalog = test_platform::with_ascii_suffix(catalog, ".renamed");
		expect(
			delete_access_denied(live_namespace.path(), true)
				&& delete_access_denied(state, true)
				&& delete_access_denied(version, true)
				&& delete_access_denied(catalog, false),
			"live catalog retains no-delete-share handles for every ancestor and "
			"the catalog leaf");
		expect(
			rename_denied(live_namespace.path(), renamed_root)
				&& rename_denied(state, renamed_state)
				&& rename_denied(version, renamed_version)
				&& rename_denied(catalog, renamed_catalog),
			"live catalog prevents ancestor, catalog-directory, and leaf rename");
		live->reset();
		expect(
			::MoveFileExW(
				catalog.c_str(),
				renamed_catalog.c_str(),
				MOVEFILE_WRITE_THROUGH)
					!= 0
				&& ::MoveFileExW(
					   renamed_catalog.c_str(),
					   catalog.c_str(),
					   MOVEFILE_WRITE_THROUGH)
					!= 0,
			"catalog leaf rename is released when the catalog closes");

		temporary_root sidecar_junction{ "sidecar-junction" };
		{
			auto made = sqlite::make_epoch_catalog(
				{ .StorageRoot = sidecar_junction.path() });
			expect(made.has_value(), "sidecar-junction fixture bootstraps");
		}
		temporary_root junction_target_root{ "junction-target" };
		const auto     junction_target = junction_target_root.path();
		const auto     sentinel        = junction_target / L"sentinel.bin";
		{
			auto output = std::ofstream{ sentinel, std::ios::binary };
			output << "junction target must remain untouched";
		}
		expect(
			test_platform::make_owner_private(sentinel, false),
			"sidecar-junction sentinel becomes owner-private");
		const auto target_identity   = test_platform::identity_of(junction_target);
		const auto sentinel_identity = test_platform::identity_of(sentinel);
		expect(
			target_identity && sentinel_identity,
			"sidecar-junction target identities are inspectable");
		const auto wal = test_platform::with_ascii_suffix(
			database_path(sidecar_junction.path()), "-wal");
		expect(
			!std::filesystem::exists(wal)
				&& test_platform::create_directory_junction(
					wal, junction_target),
			"preexisting WAL junction fixture creates without symlink privilege");
		expect_failure(
			sqlite::make_epoch_catalog({ .StorageRoot = sidecar_junction.path() }),
			app::epoch_catalog_failure::invalid_path,
			"preexisting reparse-backed WAL fails closed");
		auto target_entries = std::vector<std::filesystem::path>{ };
		for (const auto& entry : std::filesystem::directory_iterator{ junction_target }) {
			target_entries.push_back(entry.path().filename());
		}
		expect(
			::RemoveDirectoryW(wal.c_str()) != 0
				&& test_platform::identity_of(junction_target) == target_identity
				&& target_entries
					== std::vector<std::filesystem::path>{ sentinel.filename() }
				&& test_platform::identity_of(sentinel) == sentinel_identity
				&& read_binary(sentinel)
					== "junction target must remain untouched",
			"WAL junction is removed without changing the external target's "
			"identity, inventory, or sentinel bytes");

		temporary_root sidecar_hardlink{ "sidecar-hardlink" };
		{
			auto made = sqlite::make_epoch_catalog(
				{ .StorageRoot = sidecar_hardlink.path() });
			expect(made.has_value(), "sidecar-hardlink fixture bootstraps");
		}
		const auto hardlink_victim = sidecar_hardlink.path() / L"sidecar-hardlink-victim";
		{
			auto output = std::ofstream{ hardlink_victim, std::ios::binary };
			output << "sidecar hardlink victim must remain untouched";
		}
		expect(
			test_platform::make_owner_private(hardlink_victim, false),
			"sidecar-hardlink victim becomes owner-private");
		const auto hardlinked_wal = test_platform::with_ascii_suffix(
			database_path(sidecar_hardlink.path()), "-wal");
		expect(
			::CreateHardLinkW(
				hardlinked_wal.c_str(), hardlink_victim.c_str(), nullptr)
				!= 0,
			"preexisting multiply-linked WAL fixture creates");
		expect_failure(
			sqlite::make_epoch_catalog({ .StorageRoot = sidecar_hardlink.path() }),
			app::epoch_catalog_failure::invalid_path,
			"preexisting multiply-linked WAL fails closed");
		expect(
			::DeleteFileW(hardlinked_wal.c_str()) != 0
				&& read_binary(hardlink_victim)
					== "sidecar hardlink victim must remain untouched",
			"rejected WAL hardlink is removed without touching its peer");

		temporary_root sidecar_permissions{ "sidecar-permissions" };
		{
			auto made = sqlite::make_epoch_catalog(
				{ .StorageRoot = sidecar_permissions.path() });
			expect(made.has_value(), "sidecar-permission fixture bootstraps");
		}
		const auto permissive_wal = test_platform::with_ascii_suffix(
			database_path(sidecar_permissions.path()), "-wal");
		{
			auto output = std::ofstream{ permissive_wal, std::ios::binary };
			output << "hostile sidecar";
		}
		expect(
			test_platform::make_permissive(permissive_wal, false),
			"permissive WAL DACL fixture applies");
		expect_failure(
			sqlite::make_epoch_catalog({ .StorageRoot = sidecar_permissions.path() }),
			app::epoch_catalog_failure::invalid_permissions,
			"preexisting WAL with an additional reader fails closed");

		temporary_root sidecar_symlink{ "sidecar-symlink" };
		{
			auto made = sqlite::make_epoch_catalog(
				{ .StorageRoot = sidecar_symlink.path() });
			expect(made.has_value(), "sidecar-symlink fixture bootstraps");
		}
		const auto victim = sidecar_symlink.path() / L"sidecar-victim";
		{
			auto output = std::ofstream{ victim, std::ios::binary };
			output << "sidecar victim must remain untouched";
		}
		expect(
			test_platform::make_owner_private(victim, false),
			"sidecar-symlink victim becomes owner-private");
		const auto shared_memory = test_platform::with_ascii_suffix(
			database_path(sidecar_symlink.path()), "-shm");
		const auto link_result = test_platform::create_file_symbolic_link(
			shared_memory, victim);
		if (link_result
			== test_platform::symbolic_link_result::privilege_unavailable) {
			std::cout
				<< "SKIP: sidecar-symlink rejection requires Windows symlink "
				   "privilege; mandatory WAL-junction rejection remains covered\n";
		} else {
			expect(
				link_result == test_platform::symbolic_link_result::created,
				"preexisting SHM symlink fixture creates");
			expect_failure(
				sqlite::make_epoch_catalog({ .StorageRoot = sidecar_symlink.path() }),
				app::epoch_catalog_failure::invalid_path,
				"preexisting symlinked SHM fails closed");
			expect(
				::DeleteFileW(shared_memory.c_str()) != 0,
				"SHM symlink fixture removes");
		}
		expect(
			read_binary(victim) == "sidecar victim must remain untouched",
			"rejected sidecar links never open or modify their targets");
	}

	// BPR-0425 re-holds the storage root with data access so its share mode is
	// enforced, which is what denies the delete-access open above. That re-hold
	// has to stay invisible to a second store on the same root: a sibling asks
	// for exactly the access the first store holds and for no deletion, so it
	// must open, read the same durable catalog, and outlive the first store's
	// release. There is no POSIX counterpart — a store there holds
	// `flock(LOCK_EX)` on `state/v1` for its whole lifetime, so a sibling on
	// one root is serialized by design rather than refused by sharing
	// (BPR-0053).
	void windows_sibling_store_root_rehold_contract()
	{
		temporary_root root{ "sibling-store-root" };
		auto           firstMade = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(firstMade.has_value(), "the first store bootstraps the shared root");
		auto       firstPort   = std::move(*firstMade);
		const auto authority   = authority_key();
		auto       firstOpened = firstPort->open(open_request(authority), { });
		expect(
			firstOpened.has_value(),
			"the first store opens the shared authority");
		auto       first   = std::move(*firstOpened);
		const auto binding = first->binding();

		// The exact directory open a sibling store performs on the root, made
		// directly so a sharing violation is distinguishable from any typed
		// catalog failure.
		{
			test_platform::unique_handle siblingRoot{ ::CreateFileW(
				root.path().c_str(),
				FILE_READ_ATTRIBUTES | READ_CONTROL | FILE_LIST_DIRECTORY
					| FILE_TRAVERSE,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				nullptr,
				OPEN_EXISTING,
				FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS,
				nullptr) };
			expect(
				static_cast<bool>(siblingRoot),
				"the re-held root admits a sibling's non-deleting directory open");
		}

		auto secondMade = sqlite::make_epoch_catalog(
			{ .StorageRoot = root.path() });
		expect(
			secondMade.has_value(),
			"a sibling store opens the same re-held root");
		auto secondPort   = std::move(*secondMade);
		auto secondOpened = secondPort->open(
			open_request(authority, HighSession + 30U), { });
		expect(
			secondOpened.has_value(),
			"the sibling store opens the same authority");
		auto second = std::move(*secondOpened);
		expect(
			second->binding() == binding,
			"the sibling store resolves the same durable catalog identities");

		second.reset();
		first.reset();
		firstPort.reset();
		auto afterRelease = secondPort->open(
			open_request(authority, HighSession + 31U), { });
		expect(
			afterRelease.has_value()
				&& (*afterRelease)->binding() == binding,
			"the sibling store keeps its own retained root after the first store releases");
	}
#endif

	void schema_fail_closed_contract()
	{
		temporary_root wrongKindDetail{ "wrong-kind-detail" };
		{
			auto made = sqlite::make_epoch_catalog(
				{ .StorageRoot = wrongKindDetail.path() });
			expect(made.has_value(), "wrong-kind detail fixture bootstraps");
			auto opened = (*made)->open(open_request(authority_key()), { });
			expect(opened.has_value(), "wrong-kind detail authority opens");
		}
		{
			direct_database database{ database_path(wrongKindDetail.path()) };
			database.execute("PRAGMA foreign_keys = ON");
			const auto operationHex = hex_identity(
				opaque<core::epoch_operation_id>(0x71U));
			database.execute(
				std::string{
					"INSERT INTO operation(incarnation, operation, kind, phase, "
					"last_evidence, operation_revision, cache_scope, pr_scope, "
					"expected_revision, application_session, request_id, "
					"epoch_generation, source_observation, source_snapshot, "
					"identity_generation, pull_request_generation, "
					"reserved_ordinal, detail_count, "
					"retained_epoch_count, terminal) SELECT incarnation, x'" }
				+ operationHex
				+ "', 1, 1, 1, 1, cache_scope, pr_scope, catalog_revision, "
				  "x'0000000000000001', x'0000000000000002', "
				  "x'0000000000000003', "
				  "x'0000000000000000000000000000000000000000000000000000000000000004', "
				  "x'0000000000000005', x'0000000000000006', "
				  "x'0000000000000007', 1, 0, 0, 0 "
				  "FROM pull_request_scope LIMIT 1");
			const auto code = database.execute_code(
				std::string{
					"INSERT INTO cache_delete_scope(incarnation, operation, "
					"cache_scope, pr_scope, expected_revision, epoch_count) "
					"SELECT incarnation, x'" }
				+ operationHex
				+ "', cache_scope, pr_scope, catalog_revision, 0 "
				  "FROM pull_request_scope LIMIT 1");
			expect(
				(static_cast<unsigned>(code) & 0xFFU) == SQLITE_CONSTRAINT
					&& database.integer(
						   "SELECT count(*) FROM cache_delete_scope")
						== 0
					&& database.integer(
						   "SELECT count(*) FROM operation WHERE kind=1")
						== 1
					&& database.foreign_key_violation_count() == 0,
				"fixed-kind composite foreign key rejects a cache-delete detail "
				"attached to an append operation without mutating either row");
		}

		temporary_root extraObject{ "unexpected-object" };
		{
			auto made = sqlite::make_epoch_catalog({ .StorageRoot = extraObject.path() });
			expect(made.has_value(), "unexpected-object fixture bootstraps");
		}
		{
			direct_database database{ database_path(extraObject.path()) };
			database.execute(
				"CREATE TABLE unexpected_object(value INTEGER) STRICT");
		}
		expect_failure(
			sqlite::make_epoch_catalog({ .StorageRoot = extraObject.path() }),
			app::epoch_catalog_failure::schema_mismatch,
			"unexpected schema object fails closed on reopen");

		temporary_root wrongVersion{ "wrong-version" };
		{
			auto made = sqlite::make_epoch_catalog({ .StorageRoot = wrongVersion.path() });
			expect(made.has_value(), "wrong-version fixture bootstraps");
		}
		{
			direct_database database{ database_path(wrongVersion.path()) };
			// An unsupported future version must be refused without migration
			// or recreation, even though all other schema facts remain valid.
			database.execute("PRAGMA user_version = 99");
		}
		expect_failure(
			sqlite::make_epoch_catalog({ .StorageRoot = wrongVersion.path() }),
			app::epoch_catalog_failure::schema_mismatch,
			"wrong user version fails closed on reopen");

		temporary_root wrongApplication{ "wrong-application" };
		{
			auto made = sqlite::make_epoch_catalog(
				{ .StorageRoot = wrongApplication.path() });
			expect(made.has_value(), "wrong-application fixture bootstraps");
		}
		{
			direct_database database{ database_path(wrongApplication.path()) };
			database.execute("PRAGMA application_id = 0");
		}
		expect_failure(
			sqlite::make_epoch_catalog({ .StorageRoot = wrongApplication.path() }),
			app::epoch_catalog_failure::schema_mismatch,
			"wrong application ID fails closed on reopen");
	}

	void bootstrap_orphan_recovery_contract()
	{
		temporary_root root{ "bootstrap-orphan" };
		const auto     state   = root.path() / "state";
		const auto     version = state / "v1";
		std::filesystem::create_directories(version);
		expect(
			test_platform::make_owner_private(state, true)
				&& test_platform::make_owner_private(version, true),
			"bootstrap-orphan directories become owner-private");
		const auto orphanName = std::string{
			".epoch-catalog.bootstrap-"
			"0123456789abcdef0123456789abcdef"
			"0123456789abcdef0123456789abcdef.sqlite3"
		};
		const auto orphan = version / orphanName;
		{
			auto output = std::ofstream{ orphan, std::ios::binary };
			output << "interrupted bootstrap is not a catalog";
		}
		expect(
			test_platform::make_owner_private(orphan, false),
			"bootstrap orphan becomes owner-private");

		auto made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(
			made.has_value(),
			"bootstrap recovers when only an owner-private orphan exists");
		expect(
			std::filesystem::is_regular_file(database_path(root.path()))
				&& !std::filesystem::exists(orphan),
			"canonical publication removes the stale bootstrap orphan");
		made->reset();

		const auto laterOrphanName = std::string{
			".epoch-catalog.bootstrap-"
			"fedcba9876543210fedcba9876543210"
			"fedcba9876543210fedcba9876543210.sqlite3"
		};
		const auto laterOrphan = version / laterOrphanName;
		{
			auto output = std::ofstream{ laterOrphan, std::ios::binary };
			output << "orphan left after canonical publication";
		}
		expect(
			test_platform::make_owner_private(laterOrphan, false),
			"later bootstrap orphan becomes owner-private");
		auto reopened = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(
			reopened.has_value() && !std::filesystem::exists(laterOrphan),
			"reopen removes a later stale bootstrap orphan without replacing the catalog");
	}

	void bootstrap_publication_fault_contract()
	{
		temporary_root interrupted{ "bootstrap-interruption" };
		sqlite::detail::testing::interrupt_next_bootstrap_before_publication();
		expect_failure(
			sqlite::make_epoch_catalog({ .StorageRoot = interrupted.path() }),
			app::epoch_catalog_failure::unavailable,
			"test seam interrupts bootstrap before canonical publication");
		const auto version = interrupted.path() / "state" / "v1";
		expect(
			!std::filesystem::exists(database_path(interrupted.path())),
			"interrupted bootstrap never exposes a canonical catalog");
		std::vector<std::filesystem::path> orphans;
		for (const auto& entry : std::filesystem::directory_iterator{ version }) {
			const auto name = entry.path().filename().string();
			if (name.starts_with(".epoch-catalog.bootstrap-")
				&& name.ends_with(".sqlite3")) {
				orphans.push_back(entry.path());
			}
		}
		expect(
			orphans.size() == 1U
				&& test_platform::is_owner_private(orphans.front(), false),
			"interruption leaves one complete owner-private bootstrap orphan");
		{
			direct_database database{ orphans.front() };
			expect(
				database.integer("PRAGMA application_id") == 0x42504543
					&& database.integer("PRAGMA user_version") == 7
					&& database.text("PRAGMA integrity_check(1)") == "ok",
				"fault seam interrupts only after the orphan is a valid catalog");
		}

		auto recovered = sqlite::make_epoch_catalog({ .StorageRoot = interrupted.path() });
		expect(
			recovered.has_value()
				&& std::filesystem::is_regular_file(
					database_path(interrupted.path()))
				&& !std::filesystem::exists(orphans.front()),
			"restart cleans the orphan and publishes one canonical catalog");
		auto firstOpen = (*recovered)->open(open_request(authority_key(), HighSession + 20U), { });
		expect(firstOpen.has_value(), "recovered canonical authority opens");
		const auto recoveredBinding = (*firstOpen)->binding();
		const auto before           = test_platform::identity_of(database_path(interrupted.path()));
		expect(
			before.has_value(),
			"canonical catalog inode is inspectable before reopen");
		firstOpen->reset();
		recovered->reset();

		sqlite::detail::testing::interrupt_next_bootstrap_before_publication();
		auto existing = sqlite::make_epoch_catalog({ .StorageRoot = interrupted.path() });
		expect(
			existing.has_value(),
			"armed bootstrap fault does not affect an existing canonical catalog");
		auto secondOpen = (*existing)->open(
			open_request(authority_key(), HighSession + 21U), { });
		const auto after = test_platform::identity_of(database_path(interrupted.path()));
		expect(
			secondOpen && (*secondOpen)->binding() == recoveredBinding
				&& after && after == before,
			"existing canonical catalog is reopened in place and never replaced");

		temporary_root consumesPendingFault{ "bootstrap-next-fresh" };
		expect_failure(
			sqlite::make_epoch_catalog(
				{ .StorageRoot = consumesPendingFault.path() }),
			app::epoch_catalog_failure::unavailable,
			"existing-catalog reopen leaves the next-fresh-bootstrap fault armed");
		auto finalRecovery = sqlite::make_epoch_catalog(
			{ .StorageRoot = consumesPendingFault.path() });
		expect(
			finalRecovery.has_value(),
			"fresh bootstrap recovers normally after consuming the test fault");
	}
} // namespace

int main()
try {
	bootstrap_authority_and_storage_contract();
	cancellation_and_path_contract();
	permission_and_symlink_contract();
#if defined(__linux__)
	linux_live_catalog_directory_contract();
#endif
#if defined(_WIN32)
	windows_live_namespace_and_sidecar_contract();
	windows_sibling_store_root_rehold_contract();
#endif
	schema_fail_closed_contract();
	bootstrap_orphan_recovery_contract();
	bootstrap_publication_fault_contract();
	std::cout << "sqlite-epoch-catalog-storage-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
