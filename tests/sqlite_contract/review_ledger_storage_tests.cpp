#include "platform_test_support.hpp"

#include "../../src/adapters/sqlite/src/review_ledger_database.hpp"
#include "../../src/adapters/sqlite/src/sqlite_connection.hpp"
#include "../../src/adapters/sqlite/src/state_directory.hpp"

#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/review_ledger_store.hpp"
#include "review_ledger_legacy_fixture.hpp"
#include "scratch_root.hpp"

#include <sqlite3.h>

#if !defined(_WIN32)
	#include <csignal>
	#include <sys/stat.h>
	#include <sys/wait.h>
	#include <unistd.h>
#endif

#include <chrono>
#include <fstream>
#include <thread>

#include <algorithm>
#include <array>
#include <barrier>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

#if defined(BPR_SQLITE_TESTING)
namespace bpr::adapters::sqlite::detail {
	using review_ledger_preflight_test_hook
		= void (*)(const std::filesystem::path&);
	void set_review_ledger_preflight_boundary_hook_for_testing(
		review_ledger_preflight_test_hook aHook) noexcept;
	void set_review_ledger_preflight_cleanup_hook_for_testing(
		review_ledger_preflight_test_hook aHook) noexcept;
} // namespace bpr::adapters::sqlite::detail
#endif

namespace {
	using namespace bpr;
	using namespace bpr::adapters::sqlite::detail;

	namespace test_platform = bpr::tests::sqlite_contract;

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes.fill(aSeed);
		return Identity{ bytes };
	}

	[[nodiscard]] auto describe(const catalog_database_failure aFailure)
		-> std::string_view
	{
		switch (aFailure) {
		case catalog_database_failure::invalid_path:
			return "invalid_path";
		case catalog_database_failure::invalid_permissions:
			return "invalid_permissions";
		case catalog_database_failure::unavailable:
			return "unavailable";
		case catalog_database_failure::schema_mismatch:
			return "schema_mismatch";
		case catalog_database_failure::integrity_failure:
			return "integrity_failure";
		case catalog_database_failure::quota_exceeded:
			return "quota_exceeded";
		case catalog_database_failure::cancelled:
			return "cancelled";
		case catalog_database_failure::platform_unsupported:
			return "platform_unsupported";
		}
		return "unknown";
	}

	/// \brief Reports the failure a refused open carried, not merely that it failed.
	auto opened_ok(
		const std::expected<review_ledger_database, catalog_database_failure>&
			aOpened) -> bool
	{
		if (!aOpened) {
			std::cerr << "  open failed: " << describe(aOpened.error()) << '\n';
		}
		return aOpened.has_value();
	}

	using bpr::tests::expect;

	/// \brief This contract's scratch root: `bpr-ledger-` and a random part.
	/// \details [[BPR-1126]]. Made at a name nothing had, owner-private from
	/// creation, and removed only by the course that made it.
	class scratch_directory final : public test_platform::scratch_root
	{
	public:
		scratch_directory()
			: scratch_root{ "ledger" }
		{
		}

		/// \brief Narrows the root to owner-only, as the application does.
		///
		/// \details `prepare_application_data_root` gives the real storage
		/// root `owner_all` before either store sees it. The scratch root is
		/// already owner-private when it is made ([[BPR-1126]]). A course that
		/// depends on that calls this too, so the precondition is stated where
		/// it matters and set by this suite's own definition.
		///
		/// On Windows `std::filesystem::permissions` toggles only
		/// `FILE_ATTRIBUTE_READONLY` and writes no DACL, so it reported success
		/// and left the root exactly as open as it found it. Every store then
		/// refused the root, correctly, and the refusal read like a product
		/// defect. The suite's own Win32 helper is used instead, the same one
		/// the epoch-catalog tests use.
		///
		/// The result is returned rather than discarded: a root that could not
		/// be made private is not a root these tests can say anything about.
		[[nodiscard]] auto make_private() const -> bool
		{
#if defined(_WIN32)
			return test_platform::make_owner_private(path(), true);
#else
			std::error_code failed;
			std::filesystem::permissions(
				path(),
				std::filesystem::perms::owner_all,
				std::filesystem::perm_options::replace,
				failed);
			return !failed;
#endif
		}
	};

	[[nodiscard]] auto scope_of(
		const std::string_view aHost,
		const std::string_view aAccount,
		const std::string_view aRepository,
		const std::int64_t     aPullRequest) -> review_scope
	{
		return review_scope{
			.Host        = aHost,
			.Account     = aAccount,
			.Repository  = aRepository,
			.PullRequest = aPullRequest,
		};
	}

	[[nodiscard]] auto note(
		const std::uint64_t    aId,
		const std::uint8_t     aSeed,
		const std::string_view aText) -> application::local_note_record
	{
		return application::local_note_record{
			.Id         = application::local_note_id{ aId },
			.Attachment = application::local_note_attachment{
				.File       = opaque<core::file_identity>(aSeed),
				.Comparison = opaque<core::file_comparison_digest>(
					static_cast<std::uint8_t>(aSeed + 1U)),
			},
			.Note     = { .Text = std::string{ aText } },
			.Revision = application::local_note_revision{ 1U },
		};
	}

	[[nodiscard]] auto agent_session_admission(const std::uint8_t aSeed = 1U)
		-> application::agent_session_admission
	{
		auto base = std::array<std::uint8_t, 32>{ };
		auto head = std::array<std::uint8_t, 32>{ };
		base.fill(aSeed);
		head.fill(static_cast<std::uint8_t>(aSeed + 1U));
		return {
			.Profile = core::agent_profile_id{ static_cast<std::uint64_t>(aSeed) + 10U },
			.Binding = {
				.Host        = core::git_host_id{ static_cast<std::uint64_t>(aSeed) + 20U },
				.Repository  = core::repository_id{ static_cast<std::uint64_t>(aSeed) + 30U },
				.PullRequest = core::pull_request_id{ static_cast<std::uint64_t>(aSeed) + 40U },
				.BaseOid     = core::git_commit_oid{ core::git_hash_algorithm::sha256, base },
				.HeadOid     = core::git_commit_oid{ core::git_hash_algorithm::sha256, head },
				.LeftEpoch   = core::epoch_ordinal{ static_cast<std::int64_t>(aSeed) + 50 },
				.RightEpoch  = core::epoch_ordinal{ static_cast<std::int64_t>(aSeed) + 60 },
			},
		};
	}

	[[nodiscard]] auto head(
		const char        aDigit,
		const std::size_t aLength = 40U) -> core::git_commit_oid
	{
		const auto text   = std::string(aLength, aDigit);
		const auto parsed = core::parse_git_commit_oid(text);
		expect(parsed.has_value(), "the test head is canonical");
		return *parsed;
	}

	[[nodiscard]] auto defect(
		const std::uint64_t      aId,
		const std::uint64_t      aConversation,
		const std::uint8_t       aSeed,
		const core::defect_state aState
		= core::defect_state::open)
		-> application::durable_defect_record
	{
		const auto anchor = core::source_line_ref{
			.Comparison = opaque<core::file_comparison_digest>(
				static_cast<std::uint8_t>(aSeed + 1U)),
			.Side = core::diff_side::right,
			.Line = static_cast<std::uint32_t>(aId),
		};
		return application::durable_defect_record{
			.File         = opaque<core::file_identity>(aSeed),
			.Conversation = core::review_conversation_id{ aConversation },
			.History      = {
				.Defect = {
					.Id       = core::defect_id{ aId },
					.Anchor   = anchor,
					.Severity = core::defect_severity::major,
					.Category = core::defect_category::maintainability,
					.State    = aState,
				},
				.OriginalAnchor = anchor,
				.Availability   = core::defect_anchor_availability::available,
			},
		};
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
		expect(input.good(), "ledger bytes are readable for preservation evidence");
		return {
			std::istreambuf_iterator<char>{ input },
			std::istreambuf_iterator<char>{ },
		};
	}

	struct file_image
	{
		bool        Exists{ false };
		std::string Bytes;

		auto operator<=>(const file_image&) const = default;
	};

	[[nodiscard]] auto capture_file(const std::filesystem::path& aPath)
		-> file_image
	{
		auto       error  = std::error_code{ };
		const auto exists = std::filesystem::exists(aPath, error);
		expect(!error, "ledger-file existence is inspectable");
		return {
			.Exists = exists,
			.Bytes  = exists ? read_binary(aPath) : std::string{ },
		};
	}

	[[nodiscard]] auto capture_ledger_files(const std::filesystem::path& aPath)
		-> std::array<file_image, 3U>
	{
		return {
			capture_file(aPath),
			capture_file(aPath.string() + "-wal"),
			capture_file(aPath.string() + "-shm"),
		};
	}

	enum class cleanup_hook_mode : std::uint8_t {
		substitute,
		nonempty,
	};

	struct cleanup_hook_state
	{
		cleanup_hook_mode     Mode{ cleanup_hook_mode::substitute };
		std::filesystem::path Clone;
		std::filesystem::path Held;
		std::filesystem::path Sentinel;
	};

	inline std::optional<cleanup_hook_state> CleanupHookState;

	struct boundary_hook_state
	{
		std::filesystem::path Target;
		std::filesystem::path Original;
		std::filesystem::path Substitute;
	};

	inline std::optional<boundary_hook_state> BoundaryHookState;

	void rename_existing(
		const std::filesystem::path& aSource,
		const std::filesystem::path& aDestination)
	{
		auto error = std::error_code{ };
		std::filesystem::rename(aSource, aDestination, error);
		expect(!error, "the exact preflight namespace entry is replaced");
	}

	void replace_preflight_source(const std::filesystem::path& aPath)
	{
		expect(
			BoundaryHookState.has_value() && aPath == BoundaryHookState->Target,
			"the preflight boundary hook targets the captured ledger");
		for (const auto suffix : { "", "-wal", "-shm" }) {
			rename_existing(
				BoundaryHookState->Target.string() + suffix,
				BoundaryHookState->Original.string() + suffix);
			rename_existing(
				BoundaryHookState->Substitute.string() + suffix,
				BoundaryHookState->Target.string() + suffix);
		}
	}

	void exercise_cleanup_hook(const std::filesystem::path& aClone)
	{
		expect(CleanupHookState.has_value(), "the cleanup hook has exact state");
		CleanupHookState->Clone = aClone;
		if (CleanupHookState->Mode == cleanup_hook_mode::substitute) {
			CleanupHookState->Held = aClone.string() + ".held";
			auto error             = std::error_code{ };
			std::filesystem::rename(aClone, CleanupHookState->Held, error);
			expect(!error, "the exact preflight directory is retained by rename");
			std::filesystem::create_directory(aClone, error);
			expect(!error, "a same-path preflight substitute is created");
			CleanupHookState->Sentinel = aClone / "substitute-sentinel";
		} else {
			CleanupHookState->Sentinel = aClone / "foreign-child";
		}
		auto sentinel = std::ofstream{ CleanupHookState->Sentinel, std::ios::binary };
		sentinel << "must-survive";
		expect(sentinel.good(), "the cleanup refusal sentinel is durable");
	}

	/// \brief Removes the preflight directory a refused cleanup correctly kept.
	///
	/// \details [[BPR-1047]]. The hook made the product's cleanup unsafe, so the
	/// product kept the whole preflight directory in the system temporary
	/// directory. The test caused that, so the test removes it, and only after
	/// it has asserted what the refusal preserved. It removes nothing but the
	/// directory the product itself named there.
	void remove_retained_preflight_root(const std::filesystem::path& aClone)
	{
		const auto root = aClone.parent_path();
		expect(
			aClone.filename() == "clone"
				&& root.filename().string().starts_with("bpr-ledger-preflight-")
				&& root == std::filesystem::temp_directory_path() / root.filename(),
			"the retained preflight root is the product's own temporary directory");
		auto error = std::error_code{ };
		std::filesystem::remove_all(root, error);
		expect(!error, "the test removes the preflight root it caused the product to keep: " + error.message());
		expect(!std::filesystem::exists(root, error) && !error, "nothing of the preflight root remains");
	}

	void copy_file_exactly(
		const std::filesystem::path& aSource,
		const std::filesystem::path& aDestination)
	{
		auto error = std::error_code{ };
		std::filesystem::copy_file(
			aSource,
			aDestination,
			std::filesystem::copy_options::overwrite_existing,
			error);
		expect(!error, "the retained-WAL fixture file is copied");
	}

	void create_retained_wal_fixture(
		const std::filesystem::path& aPath,
		const std::string_view       aMutation,
		const bool                   aRetainShm)
	{
		const auto source
			= aPath.parent_path() / (aPath.filename().string() + ".source");
		auto opened = review_ledger_database::open(source, { });
		expect(opened_ok(opened), "the retained-WAL source opens");
		expect(
			execute(opened->handle(), "PRAGMA wal_autocheckpoint = 0", { })
					.has_value()
				&& execute(opened->handle(), aMutation, { }).has_value(),
			"the incompatible fact is committed only to retained WAL");
		const auto wal = std::filesystem::path{ source.string() + "-wal" };
		expect(
			std::filesystem::exists(wal),
			"the incompatible fixture retains a WAL sidecar");
		copy_file_exactly(source, aPath);
		copy_file_exactly(wal, aPath.string() + "-wal");
		if (aRetainShm) {
			const auto shm = std::filesystem::path{ source.string() + "-shm" };
			expect(
				std::filesystem::exists(shm),
				"the retained-WAL source exposes shared memory");
			copy_file_exactly(shm, aPath.string() + "-shm");
		}
		expect(
			std::filesystem::exists(aPath.string() + "-shm") == aRetainShm,
			"the copied retained-WAL fixture has the chosen SHM existence");
	}

	void create_prior_version_fixture(
		const std::filesystem::path& aPath,
		const std::int32_t           aApplicationId,
		const core::file_receipt&    aReceipt,
		const std::int64_t           aSchemaVersion)
	{
		expect(
			aReceipt.File.valid() && aReceipt.Material.valid()
				&& !(aReceipt.Accepted && aReceipt.Excluded),
			"the prior-version fixture uses valid identities and legal receipt state");
		expect(
			aSchemaVersion == 1 || aSchemaVersion == 2,
			"the fixture represents a supported prior schema");

		::sqlite3* raw  = nullptr;
		const auto code = ::sqlite3_open_v2(
			aPath.string().c_str(),
			&raw,
			SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
			nullptr);
		expect(code == SQLITE_OK && raw != nullptr, "the version-one fixture opens");
		expect(configure_connection(raw).has_value(), "the fixture is configured");

		for (const auto& sql : {
				 std::string{
					 "CREATE TABLE ledger_meta("
					 "singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
					 "schema_format INTEGER NOT NULL) STRICT" },
				 std::string{
					 "CREATE TABLE review_scope("
					 "scope_id INTEGER PRIMARY KEY,"
					 "host TEXT NOT NULL, account TEXT NOT NULL,"
					 "repository TEXT NOT NULL, pull_request INTEGER NOT NULL,"
					 "UNIQUE(host, account, repository, pull_request)) STRICT" },
				 std::string{
					 "CREATE TABLE file_receipt("
					 "scope_id INTEGER NOT NULL "
					 "REFERENCES review_scope(scope_id) ON DELETE CASCADE,"
					 "file_identity BLOB NOT NULL CHECK(length(file_identity) = 32),"
					 "material_id BLOB NOT NULL CHECK(length(material_id) = 32),"
					 "read_state INTEGER NOT NULL CHECK(read_state BETWEEN 0 AND 2),"
					 "accepted INTEGER NOT NULL CHECK(accepted IN (0, 1)),"
					 "excluded INTEGER NOT NULL CHECK(excluded IN (0, 1)),"
					 "PRIMARY KEY(scope_id, file_identity)) STRICT" },
				 "PRAGMA application_id = " + std::to_string(aApplicationId),
				 "INSERT INTO ledger_meta(singleton, schema_format) VALUES(1, "
					 + std::to_string(aSchemaVersion) + ")",
				 std::string{
					 "INSERT INTO review_scope"
					 "(host, account, repository, pull_request) "
					 "VALUES('github.com', 'octo', 'app', 7)" },
				 "INSERT INTO file_receipt(scope_id, file_identity, material_id, "
				 "read_state, accepted, excluded) VALUES(1, X'"
					 + hex_identity(aReceipt.File) + "', X'"
					 + hex_identity(aReceipt.Material) + "', "
					 + std::to_string(static_cast<std::uint8_t>(aReceipt.Read)) + ", "
					 + std::to_string(aReceipt.Accepted ? 1 : 0) + ", "
					 + std::to_string(aReceipt.Excluded ? 1 : 0) + ")",
			 }) {
			expect(
				execute(raw, sql, { }).has_value(),
				"the prior-version fixture is built");
		}
		if (aSchemaVersion == 2) {
			for (const auto& sql : {
					 std::string{
						 "CREATE TABLE conversation_receipt("
						 "scope_id INTEGER NOT NULL "
						 "REFERENCES review_scope(scope_id) ON DELETE CASCADE,"
						 "conversation INTEGER NOT NULL,"
						 "seen_comment INTEGER NOT NULL,"
						 "seen_edit INTEGER NOT NULL,"
						 "accepted_comment INTEGER NOT NULL,"
						 "accepted_edit INTEGER NOT NULL,"
						 "PRIMARY KEY(scope_id, conversation)) STRICT" },
					 std::string{
						 "INSERT INTO conversation_receipt(scope_id, conversation, "
						 "seen_comment, seen_edit, accepted_comment, accepted_edit) "
						 "VALUES(1, 5, 6, 7, 8, 9)" },
				 }) {
				expect(
					execute(raw, sql, { }).has_value(),
					"the version-two fixture is built");
			}
		}
		expect(
			execute(
				raw,
				"PRAGMA user_version = " + std::to_string(aSchemaVersion),
				{ })
				.has_value(),
			"the prior-version fixture is stamped");
		expect(::sqlite3_close(raw) == SQLITE_OK, "the prior-version fixture closes");
	}

	// The prohibition is checked against the schema the build creates, not
	// against the insert statements someone remembered to read. A presentation
	// coordinate added later fails here even if nothing ever writes to it.
	// A path that exists but cannot be read as a file is an obstruction the
	// store reports, not an exception it lets escape. The end-to-end
	// composition contract forces exactly this shape by leaving a directory
	// where the ledger belongs, and the preflight read is where it lands: the
	// open succeeds on POSIX and the first read is what fails. libstdc++
	// raises that failure out of `basic_filebuf::underflow` whatever the
	// stream's exception mask says, so the throw crossed this
	// `std::expected` boundary and unwound the composition above it
	// (BPR-0472).
	void test_a_directory_in_the_ledgers_place_is_a_typed_refusal()
	{
		const scratch_directory directory;
		const auto              path = directory.file("l.db");
		std::filesystem::create_directory(path);

		const auto refused = review_ledger_database::open(path, { });
		expect(
			!refused && refused.error() == catalog_database_failure::unavailable,
			"a directory left in the ledger's place is refused as unavailable");
		expect(
			std::filesystem::is_directory(path),
			"the refusal leaves the obstruction exactly as it found it");
	}

	void test_the_schema_stores_no_presentation_coordinate()
	{
		const scratch_directory directory;
		auto                    opened = review_ledger_database::open(directory.file("l.db"), { });
		expect(opened_ok(opened), "the ledger opens");

		const auto columns = review_ledger_columns(opened->handle());
		expect(columns.has_value() && !columns->empty(), "the schema reports its columns");

		// Substrings rather than exact names: `visual_row`, `row_index`, and
		// `scroll_offset` must all be caught, not just one spelling.
		constexpr auto forbidden = std::array{
			std::string_view{ "row" },
			std::string_view{ "index" },
			std::string_view{ "ribbon" },
			std::string_view{ "scroll" },
			std::string_view{ "viewport" },
			std::string_view{ "pixel" },
			std::string_view{ "coordinate" },
			std::string_view{ "widget" },
			std::string_view{ "token" },
			std::string_view{ "password" },
			std::string_view{ "secret" },
			std::string_view{ "credential" },
		};

		for (const auto& column : *columns) {
			for (const auto banned : forbidden) {
				const auto found = column.find(banned) != std::string::npos;
				if (found) {
					std::cerr << "  prohibited column: " << column
							  << " matches \"" << banned << "\"\n";
				}
				expect(
					!found,
					"no ledger column stores a presentation coordinate, a "
					"credential, or a duplicated source body");
			}
		}

		// The check would pass vacuously against an empty schema, so the
		// columns it did inspect are named.
		expect(
			std::ranges::any_of(
				*columns,
				[](const std::string& aColumn) {
					return aColumn == "file_receipt.material_id";
				}),
			"the enumeration reached the receipt table rather than nothing");
		for (const auto required : {
				 std::string_view{ "private_thread.comparison_id" },
				 std::string_view{ "last_reviewed_head.head_oid" },
				 std::string_view{ "local_defect.original_comparison" },
				 std::string_view{ "agent_session.head_oid" },
				 std::string_view{ "agent_session_sequence.last_session_id" },
			 }) {
			expect(
				std::ranges::find(*columns, required) != columns->end(),
				"the schema exposes every current private authority");
		}
	}

	void test_receipts_do_not_leak_across_identity()
	{
		const scratch_directory directory;
		auto                    opened = review_ledger_database::open(directory.file("l.db"), { });
		expect(opened_ok(opened), "the ledger opens");

		// The same pull request number and repository name on four different
		// identities. Anything shorter than the full scope collides here.
		const auto first  = opened->scope_id(scope_of("github.com", "octo", "app", 7));
		const auto second = opened->scope_id(scope_of("ghe.example.com", "octo", "app", 7));
		const auto third  = opened->scope_id(scope_of("github.com", "other", "app", 7));
		const auto fourth = opened->scope_id(scope_of("github.com", "octo", "other", 7));
		expect(
			first && second && third && fourth,
			"every identity resolves to a scope");
		expect(
			*first != *second && *first != *third && *first != *fourth
				&& *second != *third && *second != *fourth && *third != *fourth,
			"a reused pull request number never shares a scope across hosts, "
			"accounts, or repositories");

		const auto again = opened->scope_id(scope_of("github.com", "octo", "app", 7));
		expect(
			again && *again == *first,
			"the same identity resolves to the same scope rather than a new one");
	}

	// A migration mechanism whose only version is the initial one has never
	// migrated anything. Version 1 is built directly here so the 1-to-2 step
	// runs against a database that really was written by an older schema.
	void test_a_version_one_database_migrates_forward()
	{
		const scratch_directory directory;
		const auto              path    = directory.file("l.db");
		const auto              receipt = core::file_receipt{
			.File     = opaque<core::file_identity>(29U),
			.Material = opaque<core::file_material_id>(30U),
			.Read     = core::file_read_state::read,
			.Accepted = true,
		};
		create_prior_version_fixture(path, 1112560196, receipt, 1);

		auto opened = review_ledger_database::open(path, { });
		expect(opened_ok(opened), "a version one database opens and migrates");

		const auto version = scalar_integer(opened->handle(), "PRAGMA user_version");
		expect(
			version && *version == review_ledger_schema_version,
			"the migrated database reports the current schema version");

		const auto columns = review_ledger_columns(opened->handle());
		expect(
			columns.has_value()
				&& std::ranges::any_of(
					*columns,
					[](const std::string& aColumn) {
						return aColumn == "conversation_receipt.seen_edit";
					}),
			"the migration added the table version two introduced");

		// The complete receipt written under version one survives both migrations.
		const auto carried = opened->scope_id(scope_of("github.com", "octo", "app", 7));
		expect(carried && *carried == 1, "the pre-migration row is still there");
		const auto stored = opened->load_receipt(*carried, receipt.File);
		expect(
			stored && stored->has_value() && **stored == receipt,
			"the complete version-one file receipt survives one-to-two-to-three");
	}

	void test_a_wrong_application_prior_version_is_refused_without_mutation()
	{
		constexpr std::int32_t  wrongApplicationId = 0x12345678;
		const scratch_directory directory;
		for (const auto schemaVersion : { 1, 2 }) {
			const auto path = directory.file(
				"v" + std::to_string(schemaVersion) + ".db");
			const auto receipt = core::file_receipt{
				.File = opaque<core::file_identity>(
					static_cast<std::uint8_t>(32 + schemaVersion)),
				.Material = opaque<core::file_material_id>(
					static_cast<std::uint8_t>(34 + schemaVersion)),
				.Read     = core::file_read_state::seen,
				.Excluded = true,
			};
			create_prior_version_fixture(
				path, wrongApplicationId, receipt, schemaVersion);

			::sqlite3* beforeDatabase = nullptr;
			expect(
				::sqlite3_open_v2(
					path.string().c_str(),
					&beforeDatabase,
					SQLITE_OPEN_READONLY,
					nullptr)
						== SQLITE_OK
					&& beforeDatabase != nullptr,
				"the wrong-application prior version is readable before refusal");
			const auto schemaBefore = scalar_text(
				beforeDatabase,
				"SELECT group_concat(type || char(31) || name || char(31) || sql, "
				"char(30)) FROM (SELECT type, name, sql FROM sqlite_schema "
				"WHERE name NOT LIKE 'sqlite_%' ORDER BY name)");
			const auto receiptBefore = scalar_text(
				beforeDatabase,
				"SELECT hex(file_identity) || ':' || hex(material_id) || ':' || "
				"read_state || ':' || accepted || ':' || excluded "
				"FROM file_receipt WHERE scope_id = 1");
			const auto conversationBefore = schemaVersion == 2
				? scalar_text(
					  beforeDatabase,
					  "SELECT conversation || ':' || seen_comment || ':' || seen_edit "
					  "|| ':' || accepted_comment || ':' || accepted_edit "
					  "FROM conversation_receipt WHERE scope_id = 1")
				: std::expected<std::string, catalog_database_failure>{ std::string{ } };
			const auto applicationBefore
				= scalar_integer(beforeDatabase, "PRAGMA application_id");
			const auto versionBefore
				= scalar_integer(beforeDatabase, "PRAGMA user_version");
			const auto formatBefore = scalar_integer(
				beforeDatabase,
				"SELECT schema_format FROM ledger_meta WHERE singleton = 1");
			expect(
				::sqlite3_close(beforeDatabase) == SQLITE_OK,
				"the prior-version baseline closes");
			expect(
				schemaBefore && receiptBefore && applicationBefore && versionBefore
					&& formatBefore
					&& (schemaVersion == 1 || conversationBefore),
				"the complete prior-version baseline is captured");
			const auto bytesBefore = read_binary(path);

			const auto refused = review_ledger_database::open(path, { });
			expect(
				!refused
					&& refused.error()
						== catalog_database_failure::schema_mismatch,
				"each wrong-application prior version is refused");
			expect(
				read_binary(path) == bytesBefore,
				"wrong-application refusal preserves each prior-version file byte");

			::sqlite3* afterDatabase = nullptr;
			expect(
				::sqlite3_open_v2(
					path.string().c_str(),
					&afterDatabase,
					SQLITE_OPEN_READONLY,
					nullptr)
						== SQLITE_OK
					&& afterDatabase != nullptr,
				"the refused prior-version database remains readable");
			const auto schemaAfter = scalar_text(
				afterDatabase,
				"SELECT group_concat(type || char(31) || name || char(31) || sql, "
				"char(30)) FROM (SELECT type, name, sql FROM sqlite_schema "
				"WHERE name NOT LIKE 'sqlite_%' ORDER BY name)");
			const auto receiptAfter = scalar_text(
				afterDatabase,
				"SELECT hex(file_identity) || ':' || hex(material_id) || ':' || "
				"read_state || ':' || accepted || ':' || excluded "
				"FROM file_receipt WHERE scope_id = 1");
			const auto conversationAfter = schemaVersion == 2
				? scalar_text(
					  afterDatabase,
					  "SELECT conversation || ':' || seen_comment || ':' || seen_edit "
					  "|| ':' || accepted_comment || ':' || accepted_edit "
					  "FROM conversation_receipt WHERE scope_id = 1")
				: std::expected<std::string, catalog_database_failure>{ std::string{ } };
			const auto applicationAfter
				= scalar_integer(afterDatabase, "PRAGMA application_id");
			const auto versionAfter
				= scalar_integer(afterDatabase, "PRAGMA user_version");
			const auto formatAfter = scalar_integer(
				afterDatabase,
				"SELECT schema_format FROM ledger_meta WHERE singleton = 1");
			expect(
				::sqlite3_close(afterDatabase) == SQLITE_OK,
				"the refused prior version closes");
			expect(
				applicationAfter == applicationBefore && versionAfter == versionBefore
					&& formatAfter == formatBefore,
				"wrong-application refusal preserves each prior-version metadata fact");
			expect(
				schemaAfter == schemaBefore,
				"wrong-application refusal preserves each prior-version schema");
			expect(
				receiptAfter == receiptBefore
					&& conversationAfter == conversationBefore,
				"wrong-application refusal preserves every prior-version receipt");
		}
	}

	void test_a_version_two_database_migrates_without_dropping_receipts()
	{
		const scratch_directory directory;
		const auto              path    = directory.file("l.db");
		const auto              receipt = core::file_receipt{
			.File     = opaque<core::file_identity>(31U),
			.Material = opaque<core::file_material_id>(32U),
			.Read     = core::file_read_state::read,
			.Accepted = true,
		};
		create_prior_version_fixture(path, 1112560196, receipt, 2);
		constexpr std::int64_t scope = 1;

		auto migrated = review_ledger_database::open(path, { });
		expect(opened_ok(migrated), "the version-two ledger migrates to current");
		const auto stored = migrated->load_receipt(scope, receipt.File);
		expect(
			stored.has_value() && stored->has_value() && **stored == receipt,
			"the file receipt survives every migration after version two");
		const auto conversations = scalar_integer(
			migrated->handle(),
			"SELECT COUNT(*) FROM conversation_receipt WHERE conversation = 5 "
			"AND seen_comment = 6 AND seen_edit = 7 AND accepted_comment = 8 "
			"AND accepted_edit = 9");
		expect(
			conversations && *conversations == 1,
			"the conversation receipt survives the two-to-three migration");
		const auto columns = review_ledger_columns(migrated->handle());
		expect(
			columns
				&& std::ranges::find(
					   *columns, std::string{ "local_defect.lifecycle" })
					!= columns->end(),
			"the migration adds the version-three private tables");
	}

	void test_a_canonical_version_three_database_migrates_without_dropping_private_state()
	{
		const scratch_directory directory;
		const auto              path    = directory.file("l.db");
		const auto              receipt = core::file_receipt{
			.File     = opaque<core::file_identity>(41U),
			.Material = opaque<core::file_material_id>(42U),
			.Read     = core::file_read_state::seen,
			.Excluded = true,
		};
		const auto storedNote = note(43U, 44U, "version-three note");
		auto       headBytes  = std::array<std::uint8_t, 32>{ };
		headBytes.fill(45U);
		const auto storedHead = core::git_commit_oid{
			core::git_hash_algorithm::sha256, headBytes
		};
		std::int64_t scope = 0;
		{
			auto opened = review_ledger_database::open(path, { });
			expect(opened_ok(opened), "the current migration fixture opens");
			const auto id = opened->scope_id(
				scope_of("github.com", "octo", "app", 7));
			expect(id.has_value(), "the version-three migration scope resolves");
			scope = *id;
			expect(
				opened->store_receipt(scope, receipt).has_value()
					&& opened->store_note(scope, storedNote).has_value()
					&& opened->store_last_reviewed_head(scope, storedHead).has_value(),
				"version-three records are seeded before migration");
			bpr::tests::sqlite_contract::restore_v6_fixture(opened->handle());
			for (const auto sql : {
					 std::string_view{ "DROP TABLE agent_session" },
					 std::string_view{ "DROP TABLE agent_session_sequence" },
					 std::string_view{ "ALTER TABLE local_note DROP COLUMN revision" },
					 std::string_view{ "DROP TABLE local_note_sequence" },
					 std::string_view{ "UPDATE ledger_meta SET schema_format = 3" },
					 std::string_view{ "PRAGMA user_version = 3" },
				 }) {
				expect(
					execute(opened->handle(), sql, { }).has_value(),
					"the exact canonical version-three fixture is assembled");
			}
		}

		auto migrated = review_ledger_database::open(path, { });
		expect(opened_ok(migrated), "the canonical version-three ledger migrates");
		const auto restoredReceipt = migrated->load_receipt(scope, receipt.File);
		const auto restoredPrivate = migrated->load_private(scope);
		const auto sessions        = migrated->load_agent_session_catalog(scope);
		expect(
			restoredReceipt && restoredReceipt->has_value()
				&& **restoredReceipt == receipt && restoredPrivate
				&& restoredPrivate->Notes == std::vector{ storedNote }
				&& restoredPrivate->LastReviewedHead == storedHead,
			"every prior receipt, note, and reviewed head survives migration");
		expect(
			sessions && !sessions->Latest.has_value()
				&& sessions->Next == core::agent_session_id{ 1U },
			"migration creates an empty session catalog without allocating an id");
	}

	void test_version_four_conversation_defect_lines_migrate_to_zero_based_once()
	{
		const scratch_directory directory;
		const auto              path  = directory.file("l.db");
		std::int64_t            scope = 0;
		const auto              storedNote
			= note(54U, 55U, "non-line private state");

		auto currentLeft                        = defect(42U, 420U, 51U);
		currentLeft.History.Defect.Anchor.Side  = core::diff_side::left;
		currentLeft.History.OriginalAnchor.Side = core::diff_side::left;

		// In the legacy schema an outdated thread's original line was
		// indistinguishable from a current line after persistence.
		auto outdatedOriginal = defect(1U, 10U, 52U);

		auto       arbitraryComparison = defect(2U, 20U, 53U);
		const auto retainedComparison
			= opaque<core::file_comparison_digest>(90U);
		arbitraryComparison.History.Defect.Anchor.Comparison
			= retainedComparison;
		arbitraryComparison.History.OriginalAnchor.Comparison
			= retainedComparison;
		arbitraryComparison.History.Defect.Anchor.Line  = 73U;
		arbitraryComparison.History.OriginalAnchor.Line = 73U;

		auto zeroBoundary                        = defect(3U, 30U, 54U);
		zeroBoundary.History.Defect.Anchor.Line  = 0U;
		zeroBoundary.History.OriginalAnchor.Line = 0U;

		const auto migratedRecord
			= [](application::durable_defect_record aLegacy) {
				  if (aLegacy.History.Defect.Anchor.Line > 0U) {
					  --aLegacy.History.Defect.Anchor.Line;
				  }
				  if (aLegacy.History.OriginalAnchor.Line > 0U) {
					  --aLegacy.History.OriginalAnchor.Line;
				  }
				  aLegacy.History.Availability
					  = core::defect_anchor_availability::outdated;
				  return aLegacy;
			  };
		const auto expectedOutdatedOriginal
			= migratedRecord(outdatedOriginal);
		const auto expectedArbitraryComparison
			= migratedRecord(arbitraryComparison);
		const auto expectedZeroBoundary = migratedRecord(zeroBoundary);
		const auto expectedCurrentLeft  = migratedRecord(currentLeft);
		const auto expectedLegacy       = std::vector{
			expectedOutdatedOriginal,
			expectedArbitraryComparison,
			expectedZeroBoundary,
			expectedCurrentLeft,
		};
		{
			auto opened = review_ledger_database::open(path, { });
			expect(opened_ok(opened), "the version-four fixture ledger opens");
			const auto id = opened->scope_id(
				scope_of("github.com", "octo", "app", 7));
			expect(id.has_value(), "the version-four fixture scope resolves");
			scope = *id;

			expect(
				opened->store_defect(scope, currentLeft)
					&& opened->store_defect(scope, outdatedOriginal)
					&& opened->store_defect(scope, arbitraryComparison)
					&& opened->store_defect(scope, zeroBoundary)
					&& opened->store_note(scope, storedNote),
				"the unsafe legacy anchor shapes and unrelated private state are seeded");
			bpr::tests::sqlite_contract::restore_v6_fixture(opened->handle());
			for (const auto sql : {
					 std::string_view{ "ALTER TABLE local_note DROP COLUMN revision" },
					 std::string_view{ "DROP TABLE local_note_sequence" },
					 std::string_view{ "UPDATE ledger_meta SET schema_format = 4" },
					 std::string_view{ "PRAGMA user_version = 4" },
				 }) {
				expect(
					execute(opened->handle(), sql, { }).has_value(),
					"the exact canonical version-four fixture is stamped");
			}
		}

		{
			auto migrated = review_ledger_database::open(path, { });
			expect(opened_ok(migrated), "the canonical version-four ledger migrates");
			const auto version
				= scalar_integer(migrated->handle(), "PRAGMA user_version");
			const auto privateState = migrated->load_private(scope);
			expect(
				version && *version == review_ledger_schema_version && privateState,
				"the migrated ledger reports the current schema and loads private state");

			expect(
				privateState->Defects == expectedLegacy,
				"migration preserves every stored anchor identity while normalizing lines");
			expect(
				expectedCurrentLeft.History.Defect.Anchor.Line == 41U
					&& expectedCurrentLeft.History.Defect.Anchor.Side
						== core::diff_side::left
					&& expectedOutdatedOriginal.History.Defect.Anchor.Line == 0U
					&& expectedArbitraryComparison.History.Defect.Anchor.Comparison
						== retainedComparison
					&& expectedZeroBoundary.History.Defect.Anchor.Line == 0U,
				"left, outdated-original, arbitrary-comparison, and zero boundaries remain auditable");
			expect(
				std::ranges::all_of(
					privateState->Defects,
					[](const application::durable_defect_record& aDefect) {
						return aDefect.History.Availability
							== core::defect_anchor_availability::outdated
							&& !core::context_available(aDefect.History);
					}),
				"no pre-version-five defect remains placeable from SQLite alone");
			expect(
				privateState->Notes == std::vector{ storedNote },
				"the line migration leaves non-conversation private state unchanged");

			auto currentLine42                        = defect(84U, 840U, 56U);
			currentLine42.History.Defect.Anchor.Line  = 42U;
			currentLine42.History.OriginalAnchor.Line = 42U;
			currentLine42.History.Availability
				= core::defect_anchor_availability::available;
			expect(
				migrated->store_defect(scope, currentLine42).has_value(),
				"the new schema stores an explicitly available zero-based anchor");
		}

		auto reopened = review_ledger_database::open(path, { });
		expect(opened_ok(reopened), "the migrated ledger reopens without remigration");
		const auto reopenedPrivate                = reopened->load_private(scope);
		auto       expectedReopened               = expectedLegacy;
		auto       currentLine42                  = defect(84U, 840U, 56U);
		currentLine42.History.Defect.Anchor.Line  = 42U;
		currentLine42.History.OriginalAnchor.Line = 42U;
		currentLine42.History.Availability
			= core::defect_anchor_availability::available;
		expectedReopened.push_back(currentLine42);
		expect(
			reopenedPrivate && reopenedPrivate->Defects == expectedReopened
				&& reopenedPrivate->Notes == std::vector{ storedNote },
			"reopen is idempotent and preserves the new-schema availability choice");
	}

	void test_wrong_application_version_three_is_refused_byte_for_byte()
	{
		const scratch_directory directory;
		const auto              path = directory.file("l.db");
		{
			auto opened = review_ledger_database::open(path, { });
			expect(opened_ok(opened), "the wrong-application fixture opens");
			bpr::tests::sqlite_contract::restore_v6_fixture(opened->handle());
			for (const auto sql : {
					 std::string_view{ "DROP TABLE agent_session" },
					 std::string_view{ "DROP TABLE agent_session_sequence" },
					 std::string_view{ "ALTER TABLE local_note DROP COLUMN revision" },
					 std::string_view{ "DROP TABLE local_note_sequence" },
					 std::string_view{ "UPDATE ledger_meta SET schema_format = 3" },
					 std::string_view{ "PRAGMA user_version = 3" },
					 std::string_view{ "PRAGMA application_id = 305419896" },
				 }) {
				expect(
					execute(opened->handle(), sql, { }).has_value(),
					"the wrong-application canonical-v3 fixture is assembled");
			}
		}
		const auto before  = read_binary(path);
		const auto refused = review_ledger_database::open(path, { });
		expect(
			!refused
				&& refused.error() == catalog_database_failure::schema_mismatch,
			"a wrong-application canonical-v3 file is refused");
		expect(
			read_binary(path) == before,
			"wrong-application v3 refusal preserves every database byte");
	}

	void test_incompatible_retained_wal_is_refused_without_touching_any_file()
	{
		struct fixture_case
		{
			std::string_view Name;
			std::string_view Mutation;
			bool             RetainShm;
		};
		constexpr auto fixtures = std::array{
			fixture_case{
				"wrong-application", "PRAGMA application_id = 305419896", false },
			fixture_case{ "future", "PRAGMA user_version = 99", true },
			fixture_case{ "tampered", "DROP TABLE local_defect", true },
		};

		const scratch_directory directory;
		for (const auto& fixture : fixtures) {
			const auto path = directory.file(std::string{ fixture.Name } + ".db");
			create_retained_wal_fixture(
				path, fixture.Mutation, fixture.RetainShm);
			const auto before  = capture_ledger_files(path);
			const auto refused = review_ledger_database::open(path, { });
			expect(
				!refused
					&& refused.error()
						== catalog_database_failure::schema_mismatch,
				"each incompatible retained-WAL database is refused");
			expect(
				capture_ledger_files(path) == before,
				"refusal preserves main, WAL, and SHM existence and bytes");
		}
	}

	void test_preflight_cleanup_preserves_substitutes_and_nonempty_directories()
	{
		constexpr std::string_view privateCanary
			= "BPR_PREFLIGHT_PRIVATE_CLONE_0250";
		for (const auto mode : {
				 cleanup_hook_mode::substitute,
				 cleanup_hook_mode::nonempty,
			 }) {
			const scratch_directory directory;
			const auto              path = directory.file("l.db");
			{
				auto opened = review_ledger_database::open(path, { });
				expect(opened_ok(opened), "the cleanup fixture ledger opens");
				const auto scope = opened->scope_id(
					scope_of("github.com", "octo", "app", 7));
				expect(
					scope
						&& opened->store_note(
							*scope, note(250U, 251U, privateCanary)),
					"the private cleanup canary is stored");
			}
			const auto sourceImage = capture_ledger_files(path);
			expect(
				std::ranges::any_of(sourceImage, [privateCanary](const auto& aFile) {
					return aFile.Bytes.contains(privateCanary);
				}),
				"the source fixture physically contains the private clone canary");

			CleanupHookState = cleanup_hook_state{ .Mode = mode };
			set_review_ledger_preflight_cleanup_hook_for_testing(
				exercise_cleanup_hook);
			const auto refused = review_ledger_database::open(path, { });
			expect(
				!refused && refused.error() == catalog_database_failure::unavailable,
				"unsafe or incomplete preflight cleanup is an explicit refusal");
			expect(
				std::filesystem::exists(CleanupHookState->Sentinel)
					&& read_binary(CleanupHookState->Sentinel) == "must-survive",
				"cleanup preserves the substitute or unknown child exactly");

			const auto retained = mode == cleanup_hook_mode::substitute
				? CleanupHookState->Held
				: CleanupHookState->Clone;
			for (const auto suffix : { "ledger.db", "ledger.db-wal" }) {
				const auto image = capture_file(retained / suffix);
				expect(
					!image.Exists || !image.Bytes.contains(privateCanary),
					"a failed cleanup leaves no readable private-ledger clone");
			}
			remove_retained_preflight_root(CleanupHookState->Clone);
			CleanupHookState.reset();
		}
	}

	void test_preflight_revalidates_the_original_namespace_before_open()
	{
		struct fixture_case
		{
			std::string_view Name;
			std::string_view Mutation;
		};
		constexpr auto fixtures = std::array{
			fixture_case{ "wrong-application", "PRAGMA application_id = 305419896" },
			fixture_case{ "future", "PRAGMA user_version = 99" },
			fixture_case{ "tampered", "DROP TABLE local_defect" },
		};

		for (const auto& fixture : fixtures) {
			const scratch_directory directory;
			const auto              target     = directory.file("target.db");
			const auto              original   = directory.file("original.db");
			const auto              substitute = directory.file("substitute.db");
			create_retained_wal_fixture(
				target,
				"UPDATE ledger_meta SET schema_format = "
					+ std::to_string(review_ledger_schema_version),
				true);
			create_retained_wal_fixture(substitute, fixture.Mutation, true);
			const auto originalBefore   = capture_ledger_files(target);
			const auto substituteBefore = capture_ledger_files(substitute);
			expect(
				originalBefore != substituteBefore,
				"the accepted and incompatible namespace images are distinguishable");

			BoundaryHookState = boundary_hook_state{
				.Target     = target,
				.Original   = original,
				.Substitute = substitute,
			};
			set_review_ledger_preflight_boundary_hook_for_testing(
				replace_preflight_source);
			const auto refused = review_ledger_database::open(target, { });
			expect(
				!refused && refused.error() == catalog_database_failure::unavailable,
				"namespace replacement refuses before the original-path SQLite open");
			expect(
				capture_ledger_files(original) == originalBefore,
				"the displaced accepted ledger remains exact");
			expect(
				capture_ledger_files(target) == substituteBefore,
				"the wrong-app, future, or tampered substitute remains exact");
			BoundaryHookState.reset();
		}
	}

	void test_agent_session_ids_survive_restart_and_bounded_retention()
	{
		const scratch_directory directory;
		const auto              path      = directory.file("l.db");
		const auto              admission = agent_session_admission();
		std::int64_t            scope     = 0;
		{
			auto opened = review_ledger_database::open(path, { });
			expect(opened_ok(opened), "the session catalog opens");
			const auto id = opened->scope_id(
				scope_of("github.com", "octo", "app", 7));
			expect(id.has_value(), "the exact session scope resolves");
			scope            = *id;
			const auto empty = opened->load_agent_session_catalog(scope);
			expect(
				empty && !empty->Latest.has_value()
					&& empty->Next == core::agent_session_id{ 1U },
				"an empty catalog reports id one without a sequence write");
			for (auto expected = std::uint64_t{ 1U }; expected <= 130U; ++expected) {
				const auto admitted = opened->admit_agent_session(scope, admission);
				expect(
					admitted && admitted->Id == core::agent_session_id{ expected }
						&& admitted->Profile == admission.Profile
						&& admitted->Binding == admission.Binding,
					"each admission receives the exact next immutable identity");
			}
			const auto count = scalar_integer(
				opened->handle(),
				"SELECT COUNT(*) FROM agent_session WHERE scope_id = "
					+ std::to_string(scope));
			const auto oldest = scalar_integer(
				opened->handle(),
				"SELECT MIN(session_id) FROM agent_session WHERE scope_id = "
					+ std::to_string(scope));
			expect(
				count && *count == static_cast<std::int64_t>(application::maximum_retained_agent_sessions)
					&& oldest && *oldest == 3,
				"retention keeps exactly the newest 128 immutable metadata rows");
		}

		const auto filesBeforeReopen = capture_ledger_files(path);
		auto       reopened          = review_ledger_database::open(path, { });
		expect(opened_ok(reopened), "the bounded session catalog reopens");
		const auto same = reopened->scope_id(
			scope_of("github.com", "octo", "app", 7));
		const auto restored = same
			? reopened->load_agent_session_catalog(*same)
			: std::expected<application::agent_session_catalog_state, catalog_database_failure>{
				  std::unexpected(catalog_database_failure::unavailable)
			  };
		expect(
			same && *same == scope && restored
				&& restored->Latest.has_value()
				&& restored->Latest->Id == core::agent_session_id{ 130U }
				&& restored->Latest->Binding == admission.Binding
				&& restored->Next == core::agent_session_id{ 131U },
			"reopen restores the exact latest record and non-reused next id");
		reopened                  = std::unexpected(catalog_database_failure::unavailable);
		const auto filesAfterLoad = capture_ledger_files(path);
		if (filesAfterLoad != filesBeforeReopen) {
			for (auto index = std::size_t{ 0 }; index != filesAfterLoad.size(); ++index) {
				std::cerr << "  reopen file " << index << ": before="
						  << filesBeforeReopen[index].Exists << '/'
						  << filesBeforeReopen[index].Bytes.size() << " after="
						  << filesAfterLoad[index].Exists << '/'
						  << filesAfterLoad[index].Bytes.size() << '\n';
			}
		}
		expect(
			filesAfterLoad == filesBeforeReopen,
			"reopening and loading preserve physical main, WAL, and SHM files");
		reopened = review_ledger_database::open(path, { });
		expect(opened_ok(reopened), "the catalog reopens for later admissions");
		const auto next = reopened->admit_agent_session(scope, admission);
		expect(
			next && next->Id == core::agent_session_id{ 131U },
			"the first post-restart admission cannot reuse a pruned identity");

		const auto isolated = reopened->scope_id(
			scope_of("ghe.example.com", "octo", "app", 7));
		const auto isolatedCatalog = isolated
			? reopened->load_agent_session_catalog(*isolated)
			: std::expected<application::agent_session_catalog_state, catalog_database_failure>{
				  std::unexpected(catalog_database_failure::unavailable)
			  };
		expect(
			isolated && isolatedCatalog && !isolatedCatalog->Latest.has_value()
				&& isolatedCatalog->Next == core::agent_session_id{ 1U },
			"a different exact review scope cannot observe another session sequence");

		expect(
			execute(
				reopened->handle(),
				"UPDATE agent_session_sequence SET last_session_id = 9223372036854775807 "
				"WHERE scope_id = "
					+ std::to_string(scope),
				{ })
				.has_value(),
			"the exhaustion fixture advances only the high-water sequence");
		const auto exhausted = reopened->admit_agent_session(scope, admission);
		expect(
			!exhausted
				&& exhausted.error() == catalog_database_failure::quota_exceeded,
			"INT64_MAX refuses before inserting or reusing a session id");
	}

	void test_two_open_stores_allocate_unique_monotonic_session_ids()
	{
		const scratch_directory directory;
		const auto              path      = directory.file("l.db");
		const auto              admission = agent_session_admission();
		auto                    first     = review_ledger_database::open(path, { });
		expect(opened_ok(first), "the first session allocator opens");
		const auto scope = first->scope_id(
			scope_of("github.com", "octo", "app", 7));
		expect(scope.has_value(), "the shared allocation scope resolves");

		auto second = review_ledger_database::open(path, { });
		expect(opened_ok(second), "a second simultaneous session allocator opens");
		const auto sameScope = second->scope_id(
			scope_of("github.com", "octo", "app", 7));
		expect(
			sameScope && *sameScope == *scope,
			"both allocators resolve the same durable review scope");

		auto firstAdmission
			= std::expected<core::agent_session, catalog_database_failure>{
				  std::unexpected(catalog_database_failure::unavailable)
			  };
		auto secondAdmission
			= std::expected<core::agent_session, catalog_database_failure>{
				  std::unexpected(catalog_database_failure::unavailable)
			  };
		std::barrier start{ 3 };
		std::thread  firstThread{ [&] {
			start.arrive_and_wait();
			firstAdmission = first->admit_agent_session(*scope, admission);
		} };
		std::thread secondThread{ [&] {
			start.arrive_and_wait();
			secondAdmission = second->admit_agent_session(*scope, admission);
		} };
		start.arrive_and_wait();
		firstThread.join();
		secondThread.join();

		expect(
			firstAdmission && secondAdmission,
			"both simultaneous admissions commit through the lock path");
		auto allocated = std::array{
			firstAdmission->Id.value(), secondAdmission->Id.value()
		};
		std::ranges::sort(allocated);
		expect(
			allocated == std::array<std::uint64_t, 2U>{ 1U, 2U },
			"simultaneous allocators receive unique monotonic ids");
		const auto catalog = first->load_agent_session_catalog(*scope);
		expect(
			catalog && catalog->Latest.has_value()
				&& catalog->Latest->Id == core::agent_session_id{ 2U }
				&& catalog->Next == core::agent_session_id{ 3U },
			"the shared sequence advances once per simultaneous admission");
	}

	void test_a_newer_database_fails_closed()
	{
		const scratch_directory directory;
		const auto              path = directory.file("l.db");

		{
			auto opened = review_ledger_database::open(path, { });
			expect(opened.has_value(), "the ledger is created at the current version");
			expect(
				execute(opened->handle(), "PRAGMA user_version = 99", { })
					.has_value(),
				"the fixture is stamped with a newer version");
		}

		const auto reopened = review_ledger_database::open(path, { });
		expect(
			!reopened.has_value()
				&& reopened.error() == catalog_database_failure::schema_mismatch,
			"a database written by a newer build is refused rather than opened");

		// Refusing is not enough: the refusal must happen before anything is
		// written. Validating the version afterwards would report the same
		// failure while having already rewritten the metadata of a database
		// this build does not understand.
		::sqlite3* raw = nullptr;
		expect(
			::sqlite3_open_v2(
				path.string().c_str(), &raw, SQLITE_OPEN_READONLY, nullptr)
					== SQLITE_OK
				&& raw != nullptr,
			"the refused database can still be inspected");
		const auto format = scalar_integer(
			raw,
			"SELECT schema_format FROM ledger_meta WHERE singleton = 1");
		const auto stamped = scalar_integer(raw, "PRAGMA user_version");
		if (format) {
			std::cerr << "  schema_format after refusal: " << *format << '\n';
		}
		(void)::sqlite3_close(raw);
		expect(
			format && *format == review_ledger_schema_version,
			"a refused open leaves the recorded schema format untouched");
		expect(
			stamped && *stamped == 99,
			"a refused open leaves the newer version stamp exactly as it was");
	}

	void test_a_stamped_current_database_missing_a_required_table_fails_closed()
	{
		const scratch_directory directory;
		const auto              path = directory.file("l.db");

		{
			auto opened = review_ledger_database::open(path, { });
			expect(opened_ok(opened), "the complete current ledger opens");
			expect(
				execute(opened->handle(), "DROP TABLE local_defect", { }).has_value(),
				"the corruption fixture removes one required table without restamping");
		}

		const auto reopened = review_ledger_database::open(path, { });
		expect(
			!reopened
				&& reopened.error() == catalog_database_failure::schema_mismatch,
			"a stamped current ledger missing a required table is refused at open");
	}

	void test_a_stamped_current_database_with_a_malformed_column_fails_closed()
	{
		constexpr std::string_view noteTextColumn = "record BLOB NOT NULL";
		const scratch_directory    directory;
		const auto                 path = directory.file("l.db");

		{
			auto opened = review_ledger_database::open(path, { });
			expect(opened_ok(opened), "the canonical current ledger opens");
			auto definition = scalar_text(
				opened->handle(),
				"SELECT sql FROM sqlite_schema WHERE type = 'table' "
				"AND name = 'private_draft'");
			expect(definition.has_value(), "the canonical note DDL is readable");
			const auto noteText = definition->find(noteTextColumn);
			expect(
				noteText != std::string::npos,
				"the fixture found the exact note-text column");
			definition->insert(noteText, "injected INTEGER,");
			expect(
				execute(opened->handle(), "DROP TABLE private_draft", { }).has_value()
					&& execute(opened->handle(), *definition, { }).has_value(),
				"the malformed fixture recreates the same table name with an extra column");
		}

		const auto bytesBefore = read_binary(path);
		const auto reopened    = review_ledger_database::open(path, { });
		expect(
			!reopened
				&& reopened.error() == catalog_database_failure::schema_mismatch,
			"a same-name table with a noncanonical column is refused at open");
		expect(
			read_binary(path) == bytesBefore,
			"tampered current-schema refusal preserves every database byte");
	}

	void test_a_stamped_current_database_missing_the_defect_unique_fails_closed()
	{
		constexpr std::string_view uniqueClause
			= ",UNIQUE(scope_id, conversation)";
		const scratch_directory directory;
		const auto              path = directory.file("l.db");

		{
			auto opened = review_ledger_database::open(path, { });
			expect(opened_ok(opened), "the canonical defect schema opens");
			auto definition = scalar_text(
				opened->handle(),
				"SELECT sql FROM sqlite_schema WHERE type = 'table' "
				"AND name = 'local_defect'");
			expect(definition.has_value(), "the canonical defect DDL is readable");
			const auto unique = definition->find(uniqueClause);
			expect(
				unique != std::string::npos,
				"the fixture found the exact conversation uniqueness clause");
			definition->erase(unique, uniqueClause.size());
			expect(
				execute(opened->handle(), "DROP TABLE local_defect", { }).has_value()
					&& execute(opened->handle(), *definition, { }).has_value(),
				"the fixture recreates the same columns and name without UNIQUE");
		}

		const auto reopened = review_ledger_database::open(path, { });
		expect(
			!reopened
				&& reopened.error() == catalog_database_failure::schema_mismatch,
			"a same-name defect table missing UNIQUE is refused at open");
	}

	void test_a_stamped_current_database_without_strict_mode_fails_closed()
	{
		constexpr std::string_view strictSuffix = " STRICT";
		const scratch_directory    directory;
		const auto                 path = directory.file("l.db");

		{
			auto opened = review_ledger_database::open(path, { });
			expect(opened_ok(opened), "the canonical strict schema opens");
			auto definition = scalar_text(
				opened->handle(),
				"SELECT sql FROM sqlite_schema WHERE type = 'table' "
				"AND name = 'private_draft'");
			expect(definition.has_value(), "the canonical strict DDL is readable");
			const auto strict = definition->rfind(strictSuffix);
			expect(
				strict != std::string::npos
					&& strict + strictSuffix.size() == definition->size(),
				"the fixture found the exact strict-table suffix");
			definition->erase(strict, strictSuffix.size());
			expect(
				execute(opened->handle(), "DROP TABLE private_draft", { }).has_value()
					&& execute(opened->handle(), *definition, { }).has_value(),
				"the fixture recreates the same table name without strict mode");
		}

		const auto reopened = review_ledger_database::open(path, { });
		expect(
			!reopened
				&& reopened.error() == catalog_database_failure::schema_mismatch,
			"a same-name non-strict table is refused at open");
	}

	void test_a_stamped_current_database_with_an_altered_check_fails_closed()
	{
		constexpr std::string_view canonicalLimit = "length(body)<=65536";
		constexpr std::string_view alteredLimit   = "length(body)<=65535";
		const scratch_directory    directory;
		const auto                 path = directory.file("l.db");

		{
			auto opened = review_ledger_database::open(path, { });
			expect(opened_ok(opened), "the canonical checked schema opens");
			auto definition = scalar_text(
				opened->handle(),
				"SELECT sql FROM sqlite_schema WHERE type = 'table' "
				"AND name = 'private_draft'");
			expect(definition.has_value(), "the canonical checked DDL is readable");
			const auto limit = definition->find(canonicalLimit);
			expect(
				limit != std::string::npos,
				"the fixture found the exact note-size check constraint");
			definition->replace(limit, canonicalLimit.size(), alteredLimit);
			expect(
				execute(opened->handle(), "DROP TABLE private_draft", { }).has_value()
					&& execute(opened->handle(), *definition, { }).has_value(),
				"the fixture recreates the same table name with one altered check");
		}

		const auto reopened = review_ledger_database::open(path, { });
		expect(
			!reopened
				&& reopened.error() == catalog_database_failure::schema_mismatch,
			"a same-name table with an altered check is refused at open");
	}

	void test_reopening_is_idempotent()
	{
		const scratch_directory directory;
		const auto              path = directory.file("l.db");

		auto first = review_ledger_database::open(path, { });
		expect(opened_ok(first), "the ledger is created");
		const auto id = first->scope_id(scope_of("github.com", "octo", "app", 7));
		expect(id.has_value(), "a scope is written");
		// Close before reopening: the second open must read a committed file,
		// not share this connection's state.
		first = std::unexpected(catalog_database_failure::unavailable);

		auto second = review_ledger_database::open(path, { });
		expect(opened_ok(second), "the ledger reopens without migrating again");
		const auto same = second->scope_id(scope_of("github.com", "octo", "app", 7));
		expect(same && *same == *id, "the scope survives a clean reopen");
	}
	// The card's claim is that read, accepted, excluded, and seen all survive a
	// restart *identically*. Each state is written to its own file so a bug
	// that collapsed them onto one another cannot hide behind a shared row.
	void test_every_receipt_state_survives_a_restart()
	{
		const scratch_directory directory;
		const auto              path = directory.file("l.db");

		const auto states = std::array{
			core::file_receipt{
				.File     = opaque<core::file_identity>(40U),
				.Material = opaque<core::file_material_id>(50U),
				.Read     = core::file_read_state::seen },
			core::file_receipt{
				.File     = opaque<core::file_identity>(41U),
				.Material = opaque<core::file_material_id>(51U),
				.Read     = core::file_read_state::read },
			core::file_receipt{
				.File     = opaque<core::file_identity>(42U),
				.Material = opaque<core::file_material_id>(52U),
				.Read     = core::file_read_state::unread,
				.Accepted = true },
			core::file_receipt{
				.File     = opaque<core::file_identity>(43U),
				.Material = opaque<core::file_material_id>(53U),
				.Read     = core::file_read_state::read,
				.Excluded = true },
		};

		std::int64_t scope = 0;
		{
			auto opened = review_ledger_database::open(path, { });
			expect(opened_ok(opened), "the ledger opens");
			const auto id = opened->scope_id(
				scope_of("github.com", "octo", "app", 7));
			expect(id.has_value(), "the scope is written");
			scope = *id;
			for (const auto& receipt : states) {
				expect(
					opened->store_receipt(scope, receipt).has_value(),
					"each receipt commits");
			}
		}

		auto reopened = review_ledger_database::open(path, { });
		expect(opened_ok(reopened), "the ledger reopens");
		const auto sameScope = reopened->scope_id(
			scope_of("github.com", "octo", "app", 7));
		expect(
			sameScope && *sameScope == scope,
			"the scope resolves to the same row after a restart");

		for (const auto& receipt : states) {
			const auto loaded = reopened->load_receipt(scope, receipt.File);
			expect(
				loaded.has_value() && loaded->has_value(),
				"each receipt is found after the restart");
			expect(
				**loaded == receipt,
				"the receipt returns byte for byte, including its material");
		}

		// A file that was never written must come back absent rather than
		// defaulted, so "unread" is distinguishable from "no receipt".
		const auto missing
			= reopened->load_receipt(scope, opaque<core::file_identity>(44U));
		expect(
			missing.has_value() && !missing->has_value(),
			"a file with no receipt reads back as absent, not as unread");
	}

	void test_a_receipt_does_not_cross_scopes()
	{
		const scratch_directory directory;
		auto                    opened = review_ledger_database::open(directory.file("l.db"), { });
		expect(opened_ok(opened), "the ledger opens");

		const auto here = opened->scope_id(scope_of("github.com", "octo", "app", 7));
		const auto there
			= opened->scope_id(scope_of("ghe.example.com", "octo", "app", 7));
		expect(here && there, "both scopes resolve");

		const auto receipt = core::file_receipt{
			.File     = opaque<core::file_identity>(60U),
			.Material = opaque<core::file_material_id>(61U),
			.Read     = core::file_read_state::read,
			.Accepted = true,
		};
		expect(
			opened->store_receipt(*here, receipt).has_value(),
			"the receipt commits in the first scope");

		const auto leaked = opened->load_receipt(*there, receipt.File);
		expect(
			leaked.has_value() && !leaked->has_value(),
			"the same file identity carries no receipt in the other scope");

		const auto present = opened->load_receipt(*here, receipt.File);
		expect(
			present.has_value() && present->has_value() && **present == receipt,
			"the receipt is still readable in the scope that wrote it");
	}

	// [[BPR-0656]]. Conversation receipts: one row per conversation and
	// scope, replaced on a second store, invisible to another scope, and
	// carried through the application port.
	void test_conversation_receipts_round_trip_by_scope()
	{
		const scratch_directory directory;
		expect(directory.make_private(), "the scratch root becomes owner-private");
		auto opened = review_ledger_database::open(directory.file("c.db"), { });
		expect(opened_ok(opened), "the ledger opens");
		const auto here  = opened->scope_id(scope_of("github.com", "octo", "app", 7));
		const auto there = opened->scope_id(scope_of("github.com", "octo", "app", 8));
		expect(here && there, "both scopes resolve");

		const auto first = core::conversation_receipt{
			.Conversation    = core::review_conversation_id{ 501U },
			.SeenComment     = core::review_comment_id{ 7U },
			.SeenEdit        = core::comment_edit_version{ 70U },
			.AcceptedComment = core::review_comment_id{ 6U },
			.AcceptedEdit    = core::comment_edit_version{ 60U },
		};
		const auto second = core::conversation_receipt{
			.Conversation = core::review_conversation_id{ 502U },
			.SeenComment  = core::review_comment_id{ 9U },
			.SeenEdit     = core::comment_edit_version{ 90U },
		};
		expect(opened->store_conversation_receipt(*here, second).has_value()
				   && opened->store_conversation_receipt(*here, first).has_value()
				   && opened->store_conversation_receipt(*there, first).has_value(),
			   "receipts commit in both scopes");
		const auto loaded = opened->load_conversation_receipts(*here);
		expect(loaded.has_value() && *loaded == std::vector{ first, second },
			   "a scope loads its own receipts ordered by conversation");

		auto advanced        = first;
		advanced.SeenComment = core::review_comment_id{ 8U };
		advanced.SeenEdit    = core::comment_edit_version{ 80U };
		expect(opened->store_conversation_receipt(*here, advanced).has_value(),
			   "a second store of the same conversation is accepted");
		const auto replaced = opened->load_conversation_receipts(*here);
		expect(replaced.has_value() && *replaced == std::vector{ advanced, second },
			   "the second store replaces the cursor rather than adding a row");
		const auto other = opened->load_conversation_receipts(*there);
		expect(other.has_value() && *other == std::vector{ first },
			   "the other scope's receipt is untouched by the replacement");
		expect(!opened->store_conversation_receipt(*here, core::conversation_receipt{ }).has_value(),
			   "a nameless conversation is refused");
		// [[BPR-1601]]. A singleton adds an ordinary receipt beside existing
		// thread receipts. No schema change or reinterpretation is needed.
		const auto singleton = core::conversation_receipt{
			.Conversation = core::review_conversation_id{ 601U },
			.SeenComment  = core::review_comment_id{ 701U },
			.SeenEdit     = core::comment_edit_version{ 801U },
		};
		expect(opened->store_conversation_receipt(*here, singleton).has_value(), "singleton receipt persists beside old threads");
		opened = std::unexpected(catalog_database_failure::unavailable);
		opened = review_ledger_database::open(directory.file("c.db"), { });
		expect(opened_ok(opened), "ledger reopens after singleton read");
		expect(opened->load_conversation_receipts(*here) == std::vector{ advanced, second, singleton }, "restart retains singleton and exact old thread receipts");
		const auto posts  = std::array{ core::conversation_post_observation{ .Comment = singleton.SeenComment, .Edit = singleton.SeenEdit } };
		const auto unread = core::mark_conversation_unread(singleton, posts);
		expect(unread.has_value() && opened->store_conversation_receipt(*here, *unread).has_value(), "singleton unread commits");
		opened = std::unexpected(catalog_database_failure::unavailable);
		opened = review_ledger_database::open(directory.file("c.db"), { });
		expect(opened_ok(opened), "ledger reopens after singleton unread");
		const auto restarted = opened->load_conversation_receipts(*here);
		expect(restarted && *restarted == std::vector{ advanced, second, *unread } && core::unread_posts(restarted->back(), posts).size() == 1U,
			   "zero seen cursor survives restart as a present receipt, leaving old rows unchanged");
		// The port opens the ledger in the state directory by itself; the
		// direct handle is closed first so the two never share a file.
		opened = std::unexpected(catalog_database_failure::unavailable);

		auto port = bpr::adapters::sqlite::make_review_ledger_store({
			.StorageRoot = directory.path(),
			.Host        = "github.com",
			.Account     = "octo",
			.Repository  = "app",
			.PullRequest = 9,
		});
		expect(port.has_value(), "the application port opens on its own scope");
		const auto empty = (*port)->load_conversation_receipts();
		expect(empty.has_value() && empty->empty(), "a new scope has no receipts");
		expect((*port)->store_conversation_receipt(second).has_value(),
			   "the port stores a receipt");
		const auto viaPort = (*port)->load_conversation_receipts();
		expect(viaPort.has_value() && *viaPort == std::vector{ second },
			   "the port loads what it stored");
		expect(!(*port)->store_conversation_receipt(core::conversation_receipt{ }).has_value(),
			   "the port refuses a nameless conversation before the database");
	}

	void test_private_records_survive_restart_isolate_and_delete()
	{
		const scratch_directory directory;
		const auto              path        = directory.file("l.db");
		const auto              otherScopes = std::array{
			scope_of("ghe.example.com", "octo", "app", 7),
			scope_of("github.com", "other", "app", 7),
			scope_of("github.com", "octo", "other", 7),
			scope_of("github.com", "octo", "app", 8),
		};
		auto reviewNote = note(2U, 90U, "Review-level reminder");
		reviewNote.Attachment.reset();
		auto fileNote          = note(1U, 91U, "Exact comparison reminder");
		fileNote.Revision      = application::local_note_revision{ 2U };
		const auto firstDefect = defect(
			1U, 101U, 92U, core::defect_state::needs_verification);
		const auto secondDefect = defect(
			2U, 102U, 94U, core::defect_state::verified);
		const auto firstHead  = head('1');
		const auto secondHead = head('2', 64U);
		const auto expected   = application::private_ledger_observation{
			.Notes            = { fileNote, reviewNote },
			.LastReviewedHead = firstHead,
			.Defects          = { firstDefect, secondDefect },
		};

		std::int64_t                here = 0;
		std::array<std::int64_t, 4> isolatedScopes{ };
		{
			auto opened = review_ledger_database::open(path, { });
			expect(opened_ok(opened), "the private ledger opens");
			const auto first = opened->scope_id(
				scope_of("github.com", "octo", "app", 7));
			expect(first.has_value(), "the private authority scope resolves");
			here = *first;
			for (auto index = std::size_t{ 0 }; index < otherScopes.size(); ++index) {
				const auto isolated = opened->scope_id(otherScopes[index]);
				expect(isolated.has_value(), "each isolated private scope resolves");
				isolatedScopes[index] = *isolated;
			}
			// Reverse insertion order. The observation must still be stable by ID.
			expect(
				opened->store_note(here, reviewNote).has_value()
					&& opened->store_note(here, fileNote).has_value()
					&& opened->store_last_reviewed_head(here, firstHead).has_value()
					&& opened->store_defect(here, secondDefect).has_value()
					&& opened->store_defect(here, firstDefect).has_value(),
				"every private record commits under the first scope");
			for (const auto isolatedScope : isolatedScopes) {
				const auto isolated = opened->load_private(isolatedScope);
				expect(
					isolated
						&& *isolated == application::private_ledger_observation{ },
					"changing any review-scope identity field reveals no private state");
			}
		}

		auto reopened = review_ledger_database::open(path, { });
		expect(opened_ok(reopened), "the private ledger reopens");
		const auto restored = reopened->load_private(here);
		expect(
			restored && *restored == expected,
			"notes, head, defect metadata, thread linkage, and original anchors "
			"survive restart in deterministic order");

		expect(
			reopened->store_last_reviewed_head(here, secondHead).has_value(),
			"a changed review head replaces the prior exact head");
		const auto changed = reopened->load_private(here);
		expect(
			changed && changed->LastReviewedHead == secondHead
				&& changed->Notes == expected.Notes
				&& changed->Defects == expected.Defects,
			"changing the reviewed head does not disturb notes or defects");

		auto conflicting = defect(3U, 101U, 96U);
		expect(
			!reopened->store_defect(here, conflicting),
			"one shared conversation cannot silently acquire a second local defect");
		const auto afterConflict = reopened->load_private(here);
		expect(
			afterConflict && afterConflict->Defects == expected.Defects,
			"the refused thread collision leaves the original defect history intact");

		expect(
			reopened->erase_note(here, fileNote.Id, fileNote.Revision).has_value()
				&& reopened->erase_note(here, reviewNote.Id, reviewNote.Revision).has_value()
				&& reopened->erase_last_reviewed_head(here).has_value()
				&& reopened->erase_defect(
							   here, firstDefect.History.Defect.Id)
					   .has_value()
				&& reopened->erase_defect(
							   here, secondDefect.History.Defect.Id)
					   .has_value(),
			"every private record type deletes explicitly");
		const auto empty = reopened->load_private(here);
		expect(
			empty && *empty == application::private_ledger_observation{ },
			"deletion leaves a clean private observation without touching scope");
	}

	void test_private_record_bounds_and_anchor_honesty_are_enforced()
	{
		const scratch_directory directory;
		expect(directory.make_private(), "the scratch root becomes owner-private");
		auto state = open_state_directory(directory.path());
		expect(state.has_value(), "the bounded state directory opens");
		auto opened = review_ledger_database::open(
			state->path() / "review-ledger.sqlite3", { });
		expect(opened_ok(opened), "the bounded ledger opens");
		const auto scope = opened->scope_id(
			scope_of("github.com", "octo", "app", 7));
		expect(scope.has_value(), "the bounded scope resolves");

		auto emptyNote = note(1U, 110U, "x");
		emptyNote.Note.Text.clear();
		auto substitute = defect(1U, 120U, 112U);
		substitute.History.Defect.Anchor.Line += 1U;
		expect(
			!opened->store_note(*scope, emptyNote)
				&& !opened->store_last_reviewed_head(
					*scope, core::git_commit_oid{ })
				&& !opened->store_defect(*scope, substitute),
			"invalid text, head, and substitute anchor are refused whole");

		const auto maximumNote = note(
			2U,
			113U,
			std::string(core::maximum_note_bytes, 'x'));
		const auto oversizedNote = note(
			3U,
			114U,
			std::string(core::maximum_note_bytes + 1U, 'x'));
		expect(
			opened->store_note(*scope, maximumNote).has_value(),
			"an exact 16,384-byte private note is accepted");
		const auto oversized = opened->store_note(*scope, oversizedNote);
		expect(
			!oversized
				&& oversized.error().Reason
					== application::ledger_store_failure::invalid_record,
			"an exact 16,385-byte private note is refused");
		expect(
			opened->erase_note(*scope, maximumNote.Id, maximumNote.Revision).has_value(),
			"the boundary note is removed before filling the exact scope cap");

		for (std::uint64_t id = 1; id <= 4096; ++id) {
			expect(opened->store_note(*scope, note(id, 113U, "bounded")).has_value(), "the public facade reaches the exact scope cap");
		}

		const auto boundedNotes = opened->load_private(*scope);
		expect(
			boundedNotes
				&& boundedNotes->Notes.size()
					== application::maximum_private_notes
				&& boundedNotes->Notes.front().Id
					== application::local_note_id{ 1U }
				&& boundedNotes->Notes.back().Id
					== application::local_note_id{ 4'096U },
			"exactly 4,096 notes load in stable identity order");
		const auto extra = opened->store_note(
			*scope, note(4'097U, 114U, "one too many"));
		expect(
			!extra && extra.error().Reason == application::ledger_store_failure::quota_exceeded,
			"the store refuses a note beyond the declared observation cap");
		auto port = bpr::adapters::sqlite::make_review_ledger_store({
			.StorageRoot = directory.path(),
			.Host        = "github.com",
			.Account     = "octo",
			.Repository  = "app",
			.PullRequest = 7,
		});
		expect(port.has_value(), "the bounded application port opens");
		const auto portExtra = (*port)->store_note(
			note(4'097U, 115U, "still one too many"));
		if (!portExtra) {
			std::cerr << "  bounded port refusal: "
					  << application::name(portExtra.error().Reason) << '\n';
		}
		expect(
			!portExtra
				&& portExtra.error().Reason
					== application::ledger_store_failure::quota_exceeded,
			"the application port preserves the typed quota refusal");

		expect(
			execute(
				opened->handle(),
				"WITH RECURSIVE ids(value) AS (SELECT 1 UNION ALL SELECT "
				"value + 1 FROM ids WHERE value < 4096), records(value, file, "
				"anchor) AS MATERIALIZED (SELECT value, randomblob(32), "
				"randomblob(32) FROM ids) INSERT INTO local_defect(scope_id, "
				"defect_id, file_identity, conversation, anchor_comparison, "
				"anchor_side, anchor_line, original_comparison, original_side, "
				"original_line, severity, category, lifecycle, availability) "
				"SELECT "
					+ std::to_string(*scope)
					+ ", value, file, value, anchor, 0, value, anchor, 0, value, "
					  "1, 3, 0, 0 FROM records",
				{ })
				.has_value(),
			"the defect-cap fixture fills one exact scope in one transaction");
		const auto boundedRecords = opened->load_private(*scope);
		expect(
			boundedRecords
				&& boundedRecords->Defects.size()
					== application::maximum_private_defects
				&& boundedRecords->Defects.front().History.Defect.Id
					== core::defect_id{ 1U }
				&& boundedRecords->Defects.back().History.Defect.Id
					== core::defect_id{ 4'096U },
			"exactly 4,096 defects load in stable identity order");
		const auto extraDefect = opened->store_defect(
			*scope, defect(4'097U, 5'097U, 116U));
		expect(
			!extraDefect
				&& extraDefect.error()
					== catalog_database_failure::quota_exceeded,
			"the store refuses a defect beyond the declared observation cap");
	}
#if !defined(_WIN32)
	/// \brief Opens the ledger, leaves a write uncommitted, and waits to die.
	///
	/// \details Runs in a forked child. It announces readiness only after the
	/// uncommitted row is inside an open transaction, so the parent cannot kill
	/// it too early and pass for the wrong reason.
	[[noreturn]] void hold_an_uncommitted_write(
		const std::filesystem::path& aPath,
		const std::filesystem::path& aMarker,
		const core::file_receipt&    aReceipt)
	{
		auto opened = review_ledger_database::open(aPath, { });
		if (!opened) {
			::_exit(2);
		}
		const auto scope = opened->scope_id(scope_of("github.com", "octo", "app", 7));
		if (!scope) {
			::_exit(3);
		}
		if (!execute(opened->handle(), "BEGIN IMMEDIATE", { })) {
			::_exit(4);
		}
		if (!opened->store_receipt(*scope, aReceipt)) {
			::_exit(5);
		}

		{
			std::ofstream marker{ aMarker };
			marker << "ready";
		}

		// No commit, no close, no destructors. The parent kills this process.
		while (true) {
			::pause();
		}
	}

	// A recovery test that only restarts cleanly proves nothing. The process is
	// killed with an open transaction, and the assertions distinguish committed
	// work from uncommitted rather than checking that the file still opens.
	void test_a_killed_write_discards_only_the_uncommitted_work()
	{
		const scratch_directory directory;
		const auto              path   = directory.file("l.db");
		const auto              marker = directory.file("ready");

		const auto committed = core::file_receipt{
			.File     = opaque<core::file_identity>(70U),
			.Material = opaque<core::file_material_id>(71U),
			.Read     = core::file_read_state::read,
			.Accepted = true,
		};
		const auto abandoned = core::file_receipt{
			.File     = opaque<core::file_identity>(72U),
			.Material = opaque<core::file_material_id>(73U),
			.Read     = core::file_read_state::read,
		};

		std::int64_t scope = 0;
		{
			auto opened = review_ledger_database::open(path, { });
			expect(opened_ok(opened), "the ledger opens");
			const auto id = opened->scope_id(scope_of("github.com", "octo", "app", 7));
			expect(id.has_value(), "the scope is written");
			scope = *id;
			expect(
				opened->store_receipt(scope, committed).has_value(),
				"the first receipt commits normally");
		}

		const auto child = ::fork();
		expect(child >= 0, "the child process starts");
		if (child == 0) {
			// [[BPR-1121]]. The child never unwinds into this process's frames.
			bpr::tests::end_forked_child([&] { hold_an_uncommitted_write(path, marker, abandoned); });
		}

		// Wait for the child to reach the open transaction rather than racing it.
		auto waited = 0;
		while (!std::filesystem::exists(marker) && waited < 200) {
			std::this_thread::sleep_for(std::chrono::milliseconds{ 25 });
			++waited;
		}
		expect(
			std::filesystem::exists(marker),
			"the child reached its uncommitted write");

		expect(::kill(child, SIGKILL) == 0, "the child is killed outright");
		auto status = 0;
		expect(::waitpid(child, &status, 0) == child, "the child is reaped");
		expect(
			WIFSIGNALED(status) != 0 && WTERMSIG(status) == SIGKILL,
			"the child died from the signal rather than exiting gracefully");

		auto recovered = review_ledger_database::open(path, { });
		expect(opened_ok(recovered), "the ledger reopens after the kill");
		const auto sameScope
			= recovered->scope_id(scope_of("github.com", "octo", "app", 7));
		expect(sameScope && *sameScope == scope, "the scope survives");

		const auto survivor = recovered->load_receipt(scope, committed.File);
		expect(
			survivor.has_value() && survivor->has_value()
				&& **survivor == committed,
			"the receipt committed before the kill survives byte for byte");

		const auto discarded = recovered->load_receipt(scope, abandoned.File);
		expect(
			discarded.has_value() && !discarded->has_value(),
			"the write left inside the open transaction is discarded");
	}

	[[noreturn]] void hold_uncommitted_private_records(
		const std::filesystem::path& aPath,
		const std::filesystem::path& aMarker)
	{
		auto opened = review_ledger_database::open(aPath, { });
		if (!opened) {
			::_exit(2);
		}
		const auto scope = opened->scope_id(
			scope_of("github.com", "octo", "app", 7));
		if (!scope || !execute(opened->handle(), "BEGIN IMMEDIATE", { })) {
			::_exit(3);
		}
		if (!execute(opened->handle(), "UPDATE private_content SET body=CAST('abandoned' AS BLOB) WHERE scope_id=" + std::to_string(*scope), { })
			|| !opened->store_last_reviewed_head(*scope, head('2'))
			|| !opened->store_defect(*scope, defect(2U, 202U, 134U))) {
			::_exit(4);
		}
		{
			std::ofstream marker{ aMarker };
			marker << "ready";
		}
		while (true) {
			::pause();
		}
	}

	void test_a_killed_private_write_preserves_only_committed_records()
	{
		const scratch_directory directory;
		const auto              path      = directory.file("l.db");
		const auto              marker    = directory.file("private-ready");
		const auto              committed = application::private_ledger_observation{
			.Notes            = { note(1U, 130U, "committed") },
			.LastReviewedHead = head('1'),
			.Defects          = { defect(1U, 201U, 131U) },
		};
		std::int64_t scope = 0;
		{
			auto opened = review_ledger_database::open(path, { });
			expect(opened_ok(opened), "the private crash fixture opens");
			const auto id = opened->scope_id(
				scope_of("github.com", "octo", "app", 7));
			expect(id.has_value(), "the private crash scope resolves");
			scope = *id;
			expect(
				opened->store_note(scope, committed.Notes.front()).has_value()
					&& opened
						   ->store_last_reviewed_head(
							   scope, *committed.LastReviewedHead)
						   .has_value()
					&& opened->store_defect(scope, committed.Defects.front())
						   .has_value(),
				"all committed private records reach WAL before the crash");
		}

		const auto child = ::fork();
		expect(child >= 0, "the private crash child starts");
		if (child == 0) {
			// [[BPR-1121]]. The child never unwinds into this process's frames.
			bpr::tests::end_forked_child([&] { hold_uncommitted_private_records(path, marker); });
		}
		auto waited = 0;
		while (!std::filesystem::exists(marker) && waited < 200) {
			std::this_thread::sleep_for(std::chrono::milliseconds{ 25 });
			++waited;
		}
		expect(
			std::filesystem::exists(marker),
			"the child reached all three uncommitted private writes");
		expect(::kill(child, SIGKILL) == 0, "the private child is killed outright");
		auto status = 0;
		expect(::waitpid(child, &status, 0) == child, "the private child is reaped");
		expect(
			WIFSIGNALED(status) != 0 && WTERMSIG(status) == SIGKILL,
			"the private child dies from SIGKILL rather than cleanup");

		auto recovered = review_ledger_database::open(path, { });
		expect(opened_ok(recovered), "the private ledger recovers after SIGKILL");
		const auto restored = recovered->load_private(scope);
		expect(
			restored && *restored == committed,
			"committed note, head, and defect survive while every abandoned "
			"private write is discarded");
	}
#endif

	// Deleting the ledger must lose only private review state. The epoch
	// catalog is a separate file for exactly this reason, and a second database
	// alongside it stands in for that independence here.
	void test_deleting_the_ledger_leaves_the_neighbouring_store_intact()
	{
		const scratch_directory directory;
		const auto              ledgerPath = directory.file("ledger.db");
		const auto              otherPath  = directory.file("catalog.db");

		const auto receipt = core::file_receipt{
			.File     = opaque<core::file_identity>(80U),
			.Material = opaque<core::file_material_id>(81U),
			.Read     = core::file_read_state::read,
		};

		std::int64_t otherScope = 0;
		{
			auto ledger = review_ledger_database::open(ledgerPath, { });
			auto other  = review_ledger_database::open(otherPath, { });
			expect(opened_ok(ledger) && opened_ok(other), "both stores open");
			const auto scope = ledger->scope_id(scope_of("github.com", "octo", "app", 7));
			expect(scope.has_value(), "the ledger scope is written");
			expect(
				ledger->store_receipt(*scope, receipt).has_value(),
				"the receipt commits");
			const auto neighbour
				= other->scope_id(scope_of("github.com", "octo", "app", 7));
			expect(neighbour.has_value(), "the neighbour writes its own row");
			otherScope = *neighbour;
			expect(
				other->store_receipt(otherScope, receipt).has_value(),
				"the neighbour commits its own receipt");
		}

		for (const auto suffix : { "", "-wal", "-shm" }) {
			std::error_code ignored;
			std::filesystem::remove(
				ledgerPath.string() + suffix, ignored);
		}
		expect(
			!std::filesystem::exists(ledgerPath),
			"the ledger file is gone");

		auto recreated = review_ledger_database::open(ledgerPath, { });
		expect(opened_ok(recreated), "a clean ledger is recreated in its place");
		const auto scope
			= recreated->scope_id(scope_of("github.com", "octo", "app", 7));
		expect(scope.has_value(), "the recreated ledger accepts the scope again");
		const auto lost = recreated->load_receipt(*scope, receipt.File);
		expect(
			lost.has_value() && !lost->has_value(),
			"the private receipt is gone, which is what deletion means");

		auto neighbour = review_ledger_database::open(otherPath, { });
		expect(opened_ok(neighbour), "the neighbouring store still opens");
		const auto kept = neighbour->load_receipt(otherScope, receipt.File);
		expect(
			kept.has_value() && kept->has_value() && **kept == receipt,
			"deleting the ledger changed nothing in the store beside it");
	}

	void test_the_application_store_round_trips_private_records_by_scope()
	{
		const scratch_directory directory;
		expect(directory.make_private(), "the scratch root becomes owner-private");
		const auto configuration = bpr::adapters::sqlite::review_ledger_configuration{
			.StorageRoot = directory.path(),
			.Host        = "github.com",
			.Account     = "octo",
			.Repository  = "app",
			.PullRequest = 7,
		};
		const auto storedNote                 = note(9U, 140U, "port round trip");
		const auto storedHead                 = head('3');
		const auto storedDefect               = defect(9U, 209U, 142U);
		auto       updatedDefect              = storedDefect;
		updatedDefect.History.Defect.Severity = core::defect_severity::critical;
		updatedDefect.History.Defect.Category = core::defect_category::security;
		updatedDefect.History.Defect.State    = core::defect_state::verified;
		updatedDefect.History.Availability
			= core::defect_anchor_availability::thread_deleted;
		{
			auto store = bpr::adapters::sqlite::make_review_ledger_store(
				configuration);
			expect(store.has_value(), "the application-owned store opens");
			expect(
				(*store)->store_note(storedNote).has_value()
					&& (*store)->store_last_reviewed_head(storedHead).has_value()
					&& (*store)->store_defect(storedDefect).has_value(),
				"the application port commits every private record type");

			expect(
				(*store)->store_defect(updatedDefect).has_value()
					&& (*store)->store_defect(updatedDefect).has_value(),
				"classification, lifecycle, and availability remain mutable");

			auto changedFile       = updatedDefect;
			changedFile.File       = opaque<core::file_identity>(150U);
			const auto refusedFile = (*store)->store_defect(changedFile);
			expect(
				!refusedFile
					&& refusedFile.error()
						== application::ledger_store_failure::conflict,
				"changing only a defect file returns a typed conflict");
			const auto preservedAfterFile = (*store)->load_private();
			expect(
				preservedAfterFile
					&& preservedAfterFile->Defects == std::vector{ updatedDefect },
				"a refused file rewrite preserves the exact stored defect row");

			auto changedConversation         = updatedDefect;
			changedConversation.Conversation = core::review_conversation_id{ 210U };
			const auto refusedConversation
				= (*store)->store_defect(changedConversation);
			expect(
				!refusedConversation
					&& refusedConversation.error()
						== application::ledger_store_failure::conflict,
				"changing only a defect conversation returns a typed conflict");
			const auto preservedAfterConversation = (*store)->load_private();
			expect(
				preservedAfterConversation
					&& preservedAfterConversation->Defects
						== std::vector{ updatedDefect },
				"a refused conversation rewrite preserves the exact stored defect row");

			auto       changedAnchor = updatedDefect;
			const auto equalNewAnchor
				= defect(9U, 209U, 150U).History.OriginalAnchor;
			changedAnchor.History.Defect.Anchor  = equalNewAnchor;
			changedAnchor.History.OriginalAnchor = equalNewAnchor;
			const auto refusedAnchor             = (*store)->store_defect(changedAnchor);
			expect(
				!refusedAnchor
					&& refusedAnchor.error()
						== application::ledger_store_failure::conflict,
				"changing only equal current and original anchors returns a typed conflict");
			const auto preservedAfterAnchor = (*store)->load_private();
			expect(
				preservedAfterAnchor
					&& preservedAfterAnchor->Defects
						== std::vector{ updatedDefect },
				"a refused equal-anchor rewrite preserves the exact stored defect row");
		}
		{
			auto reopened = bpr::adapters::sqlite::make_review_ledger_store(
				configuration);
			expect(reopened.has_value(), "the application-owned store restarts");
			const auto restored = (*reopened)->load_private();
			expect(
				restored
					&& *restored
						== application::private_ledger_observation{
							.Notes            = { storedNote },
							.LastReviewedHead = storedHead,
							.Defects          = { updatedDefect },
						},
				"a second port instance restores exact private state");
		}
		auto otherConfiguration = configuration;
		otherConfiguration.Host = "ghe.example.com";
		auto other              = bpr::adapters::sqlite::make_review_ledger_store(
			otherConfiguration);
		expect(other.has_value(), "the second host scope opens");
		const auto isolated = (*other)->load_private();
		expect(
			isolated && *isolated == application::private_ledger_observation{ },
			"the application port cannot see another host's private records");
	}

#if !defined(_WIN32)
	[[nodiscard]] auto ledger_composes(const std::filesystem::path& aRoot) -> bool
	{
		return bpr::adapters::sqlite::make_review_ledger_store({
																   .StorageRoot = aRoot,
																   .Host        = "github.com",
																   .Account     = "octo",
																   .Repository  = "app",
																   .PullRequest = 7,
															   })
			.has_value();
	}

	[[nodiscard]] auto catalog_composes(const std::filesystem::path& aRoot) -> bool
	{
		return catalog_database::open(aRoot, { }).has_value();
	}

	[[nodiscard]] auto mode_of(const std::filesystem::path& aPath)
		-> std::optional<::mode_t>
	{
		struct stat status{ };
		if (::lstat(aPath.c_str(), &status) != 0) {
			return std::nullopt;
		}
		return status.st_mode;
	}

	// The ordering that used to produce the weaker directory. The ledger must
	// not depend on the catalog having been composed first, and before
	// BPR-0132 it discharged that by creating `state/v1` itself with
	// `create_directories` and a `chmod`. The directory it left was one the
	// catalog would accept but would never have made: no symlink refusal, no
	// ownership check, and nothing holding it open.
	//
	// The assertion that matters is the last one. Checking the mode alone
	// would have passed against the old code too.
	void test_the_ledger_composing_first_hardens_the_state_directory()
	{
		const scratch_directory directory;
		expect(directory.make_private(), "the scratch root becomes owner-private");

		expect(
			ledger_composes(directory.path()),
			"the ledger composes in a root no other store has touched");

		const auto state = directory.path() / "state" / "v1";
		const auto mode  = mode_of(state);
		expect(mode.has_value(), "the state directory exists");
		expect(
			S_ISDIR(*mode),
			"and is a directory rather than a link to one");
		expect(
			(*mode & static_cast<mode_t>(0777)) == 0700,
			"reachable only by its owner");

		struct stat status{ };
		expect(
			::lstat(state.c_str(), &status) == 0
				&& status.st_uid == ::geteuid(),
			"and owned by this user");

		expect(
			catalog_composes(directory.path()),
			"the catalog accepts the directory the ledger made");
	}

	// A symlinked `state/v1` is the case the ledger's own bootstrap could not
	// see: `create_directories` is content with a link, and `chmod` follows it
	// and narrows whatever is on the other end.
	void test_a_symlinked_state_directory_is_refused_by_either_store()
	{
		for (const auto ledgerFirst : { true, false }) {
			const scratch_directory directory;
			expect(directory.make_private(), "the scratch root becomes owner-private");

			const auto elsewhere = directory.path() / "elsewhere";
			std::filesystem::create_directories(elsewhere);
			expect(
				::chmod(elsewhere.c_str(), S_IRWXU) == 0,
				"the link target is itself private");

			const auto state = directory.path() / "state";
			std::filesystem::create_directories(state);
			expect(
				::chmod(state.c_str(), S_IRWXU) == 0,
				"the parent is private, so only the link is wrong");
			std::filesystem::create_directory_symlink(elsewhere, state / "v1");

			if (ledgerFirst) {
				expect(
					!ledger_composes(directory.path()),
					"the ledger refuses a symlinked state directory");
				expect(
					!catalog_composes(directory.path()),
					"and so does the catalog");
			} else {
				expect(
					!catalog_composes(directory.path()),
					"the catalog refuses a symlinked state directory");
				expect(
					!ledger_composes(directory.path()),
					"and so does the ledger");
			}
		}
	}

	// A directory anyone can read is not one private review state belongs in.
	// The old ledger would have narrowed it and carried on; narrowing is not
	// the same as refusing, because whoever set 0755 may already have looked.
	void test_a_group_readable_state_directory_is_refused_by_either_store()
	{
		for (const auto ledgerFirst : { true, false }) {
			const scratch_directory directory;
			expect(directory.make_private(), "the scratch root becomes owner-private");

			const auto state = directory.path() / "state" / "v1";
			std::filesystem::create_directories(state);
			expect(
				::chmod((directory.path() / "state").c_str(), S_IRWXU) == 0,
				"the parent is private");
			expect(
				::chmod(state.c_str(), S_IRWXU | S_IRGRP | S_IXGRP) == 0,
				"the state directory is not");

			if (ledgerFirst) {
				expect(
					!ledger_composes(directory.path()),
					"the ledger refuses a group-readable state directory");
				expect(
					!catalog_composes(directory.path()),
					"and so does the catalog");
			} else {
				expect(
					!catalog_composes(directory.path()),
					"the catalog refuses a group-readable state directory");
				expect(
					!ledger_composes(directory.path()),
					"and so does the ledger");
			}
		}
	}

	// One bootstrap means one refusal. A root the catalog would not open is
	// not one the ledger opens either.
	void test_a_public_storage_root_is_refused_by_either_store()
	{
		const scratch_directory directory;
		expect(
			::chmod(directory.path().c_str(), S_IRWXU | S_IRGRP | S_IXGRP) == 0,
			"the root is left readable by others");

		expect(
			!ledger_composes(directory.path()),
			"the ledger refuses a root it cannot call private");
		expect(
			!catalog_composes(directory.path()),
			"and so does the catalog, as it always did");
	}
#endif
	void test_simultaneous_note_edits_have_one_winner()
	{
		const scratch_directory directory;
		const auto              path   = directory.file("simultaneous-notes.db");
		auto                    first  = review_ledger_database::open(path, { });
		auto                    second = review_ledger_database::open(path, { });
		expect(first && second, "simultaneous note connections open");
		const auto scope = first->scope_id(scope_of("github.com", "octo", "app", 7));
		expect(scope.has_value(), "simultaneous note scope resolves");
		const auto initial = first->store_note(*scope, note(1U, 29U, "original"));
		expect(initial.has_value(), "simultaneous note base commits");
		auto left       = *initial;
		auto right      = *initial;
		left.Note.Text  = "left winner";
		right.Note.Text = "right winner";
		auto leftResult = std::expected<application::local_note_record, application::local_note_failure>{
			std::unexpected(application::local_note_failure{ .Reason = application::ledger_store_failure::unavailable })
		};
		auto         rightResult = leftResult;
		std::barrier start{ 3 };
		std::thread  leftThread{ [&] {start.arrive_and_wait(); leftResult=first->store_note(*scope,left,initial->Revision); } };
		std::thread  rightThread{ [&] {start.arrive_and_wait(); rightResult=second->store_note(*scope,right,initial->Revision); } };
		start.arrive_and_wait();
		leftThread.join();
		rightThread.join();
		expect(leftResult.has_value() != rightResult.has_value(), "exactly one simultaneous expected-revision edit wins");
		const auto& accepted = leftResult ? *leftResult : *rightResult;
		const auto& refused  = leftResult ? rightResult.error() : leftResult.error();
		expect(refused.Reason == application::ledger_store_failure::conflict && refused.Current == accepted
				   && accepted.Revision.value() == 2U,
			   "the losing connection observes the same committed competitor without advancing the sequence");
		auto reopened = review_ledger_database::open(path, { });
		expect(reopened.has_value(), "the simultaneous result reopens");
		const auto durable = reopened->load_private(*scope);
		expect(durable && durable->Notes == std::vector{ accepted }, "only the simultaneous winner survives restart");
		expect(execute(first->handle(), "UPDATE private_scope_state SET note_revision=0", { }).has_value(), "the sequence corruption fixture is designated");
		const auto corruptDelete = second->erase_note(*scope, accepted.Id, accepted.Revision);
		expect(!corruptDelete && corruptDelete.error().Reason == application::ledger_store_failure::integrity_failure,
			   "a missing allocator cannot erase the evidence needed to prevent revision reuse");
	}

	/// \brief [[BPR-1210]]. A file note keeps its path from schema 8; notes from
	/// schema 7 migrate intact with none, and say so rather than guess.
	void test_note_paths_migrate_and_round_trip()
	{
		const scratch_directory directory;
		const auto              path  = directory.file("note-paths.db");
		std::int64_t            scope = 0;
		auto                    old   = application::local_note_record{ };
		auto                    whole = application::local_note_record{ };
		{
			auto database = review_ledger_database::open(path, { });
			expect(database.has_value(), "the note-path fixture opens");
			const auto id = database->scope_id(scope_of("github.com", "octo", "app", 9));
			expect(id.has_value(), "the note-path scope resolves");
			scope    = *id;
			auto pre = database->store_note(scope, note(1U, 70U, "before paths"));
			expect(pre.has_value() && !pre->FilePath, "a schema 7 file note");
			old       = *pre;
			auto bare = note(2U, 72U, "about the whole review");
			bare.Attachment.reset();
			auto stored = database->store_note(scope, bare);
			expect(stored.has_value(), "a schema 7 whole-review note");
			whole = *stored;
			bpr::tests::sqlite_contract::restore_v7_fixture(database->handle());
		}
		{
			auto migrated = review_ledger_database::open(path, { });
			expect(migrated.has_value(), "a schema 7 ledger migrates");
			const auto version = scalar_integer(migrated->handle(), "PRAGMA user_version");
			const auto table   = scalar_integer(migrated->handle(), "SELECT COUNT(*) FROM private_note_path");
			expect(version && *version == review_ledger_schema_version && table && *table == 0, "migration preserves schema8 note paths under the current schema");
			const auto state = migrated->load_private(scope);
			expect(state && state->Notes == std::vector{ old, whole } && !state->Notes.front().FilePath,
				   "schema 7 notes keep text, revision and attachment, with no path");

			auto written     = note(3U, 74U, "written with its path");
			written.FilePath = "src/engine/renderer.cpp";
			const auto kept  = migrated->store_note(scope, written);
			expect(kept.has_value() && kept->FilePath == written.FilePath, "a file note is written with its path");
			auto edited      = *kept;
			edited.Note.Text = "edited later";
			edited.FilePath.reset();
			const auto edit = migrated->store_note(scope, edited, kept->Revision);
			expect(edit.has_value() && edit->FilePath == written.FilePath, "an edit keeps the path kept when the note was written");

			auto loose = note(4U, 76U, "path without a file");
			loose.Attachment.reset();
			loose.FilePath  = "src/a.cpp";
			auto longer     = note(5U, 78U, "path too long");
			longer.FilePath = std::string(application::maximum_note_path_bytes + 1U, 'a');
			expect(!migrated->store_note(scope, loose) && !migrated->store_note(scope, longer),
				   "a path without a file, or longer than its bound, is refused");
		}
		{
			auto reopened = review_ledger_database::open(path, { });
			expect(reopened.has_value(), "the schema 8 ledger reopens");
			const auto state = reopened->load_private(scope);
			const auto found = state ? std::ranges::find(state->Notes, application::local_note_id{ 3U }, &application::local_note_record::Id) : state->Notes.end();
			expect(state && found != state->Notes.end() && found->FilePath == std::optional<std::string>{ "src/engine/renderer.cpp" }
					   && found->Note.Text == "edited later",
				   "the path survives restart");
			expect(reopened->erase_note(scope, found->Id, found->Revision).has_value(), "the file note is deleted");
			const auto rows = scalar_integer(reopened->handle(), "SELECT COUNT(*) FROM private_note_path");
			expect(rows && *rows == 0, "deleting a note deletes its path");
		}
	}

	/// \brief [[BPR-1591]]. A note written on a line keeps its side, line
	/// and blob from schema 10; notes from schema 9 migrate unchanged with no
	/// line; an edit keeps the line; restart and a new head place it by
	/// `core::place_note`; deleting the note deletes its line.
	void test_note_lines_migrate_and_round_trip()
	{
		const scratch_directory directory;
		const auto              path  = directory.file("note-lines.db");
		std::int64_t            scope = 0;
		auto                    old   = application::local_note_record{ };
		const auto              blob  = [] {
			auto bytes = std::array<std::uint8_t, 32U>{ };
			for (auto index = std::size_t{ 0U }; index < 20U; ++index) {
				bytes[index] = static_cast<std::uint8_t>(0x40U + index);
			}
			return core::git_blob_oid{ core::git_hash_algorithm::sha1, bytes };
		}();
		{
			auto database = review_ledger_database::open(path, { });
			expect(database.has_value(), "the note-line fixture opens");
			const auto id = database->scope_id(scope_of("github.com", "octo", "app", 11));
			expect(id.has_value(), "the note-line scope resolves");
			scope        = *id;
			auto pre     = note(1U, 80U, "a whole-file note from schema 9");
			pre.FilePath = "src/engine/renderer.cpp";
			auto stored  = database->store_note(scope, pre);
			expect(stored.has_value(), "a schema 9 file note");
			old = *stored;
			bpr::tests::sqlite_contract::restore_v9_fixture(database->handle());
		}
		auto written = note(2U, 82U, "the budget resets here");
		{
			auto migrated = review_ledger_database::open(path, { });
			expect(migrated.has_value(), "a schema 9 ledger migrates");
			const auto version = scalar_integer(migrated->handle(), "PRAGMA user_version");
			const auto rows    = scalar_integer(migrated->handle(), "SELECT COUNT(*) FROM private_note_line");
			expect(version && *version == review_ledger_schema_version && *version == 10 && rows && *rows == 0, "schema 9 migrates to 10 with an empty note line table");
			const auto state = migrated->load_private(scope);
			expect(state && state->Notes == std::vector{ old } && !state->Notes.front().Attachment->Line, "a schema 9 note keeps text, revision, attachment and path, with no line");

			written.Attachment->Line = core::note_line_anchor{ .Side = core::diff_side::right, .Line = 41U, .Blob = blob };
			const auto kept          = migrated->store_note(scope, written);
			expect(kept.has_value() && kept->Attachment == written.Attachment, "a note is written with its line");
			auto base             = note(3U, 84U, "base side, no blob kept");
			base.Attachment->Line = core::note_line_anchor{ .Side = core::diff_side::left, .Line = 0U, .Blob = std::nullopt };
			expect(migrated->store_note(scope, base).has_value(), "a line without a blob is kept as it is");
			auto edited      = *kept;
			edited.Note.Text = "edited later";
			const auto edit  = migrated->store_note(scope, edited, kept->Revision);
			expect(edit.has_value() && edit->Attachment->Line == written.Attachment->Line, "an edit keeps the line");
			auto moved             = *edit;
			moved.Attachment->Line = core::note_line_anchor{ .Side = core::diff_side::right, .Line = 42U, .Blob = blob };
			const auto refused     = migrated->store_note(scope, moved, edit->Revision);
			expect(!refused && refused.error().Reason == application::ledger_store_failure::conflict, "a note never moves to another line");
			auto bad             = note(4U, 86U, "an invalid blob");
			bad.Attachment->Line = core::note_line_anchor{ .Side = core::diff_side::right, .Line = 1U, .Blob = core::git_blob_oid{ } };
			expect(!migrated->store_note(scope, bad), "an invalid blob is refused");
		}
		{
			auto reopened = review_ledger_database::open(path, { });
			expect(reopened.has_value(), "the schema 10 ledger reopens");
			const auto state = reopened->load_private(scope);
			const auto found = state ? std::ranges::find(state->Notes, application::local_note_id{ 2U }, &application::local_note_record::Id) : state->Notes.end();
			expect(state && found != state->Notes.end() && found->Attachment->Line == written.Attachment->Line && found->Note.Text == "edited later", "the line and its blob survive restart");
			// The comparison it was written on, and a new head whose head side is
			// the same blob, place it at its line; a changed head outdates it.
			auto shown           = core::file_alignment{ };
			shown.Key.File       = found->Attachment->File;
			shown.Key.RightBlob  = blob;
			shown.Digest         = found->Attachment->Comparison;
			shown.RightLineCount = 120U;
			const auto anchor    = application::note_anchor_of(*found);
			expect(core::place_note(anchor, &shown).Kind == core::note_placement_kind::placed, "after restart the note hangs at its line");
			shown.Digest    = opaque<core::file_comparison_digest>(99U);
			const auto same = core::place_note(anchor, &shown);
			expect(same.Kind == core::note_placement_kind::placed && same.At->Line == 41U, "a new head with the same head text keeps the line");
			shown.Key.RightBlob = core::git_blob_oid{ core::git_hash_algorithm::sha256, opaque<core::file_comparison_digest>(5U).bytes() };
			expect(core::place_note(anchor, &shown).Reason == core::note_outdated_reason::content_changed, "a new head that changed the file outdates the note");
			expect(reopened->erase_note(scope, found->Id, found->Revision).has_value(), "the line note is deleted");
			const auto rows = scalar_integer(reopened->handle(), "SELECT COUNT(*) FROM private_note_line");
			expect(rows && *rows == 1, "deleting a note deletes its line, and only its line");
		}
	}

	void test_version_five_note_migration_is_atomic_and_repeatable()
	{
		const scratch_directory directory;
		const auto              path  = directory.file("note-migrate.db");
		std::int64_t            scope = 0;
		{
			auto database = review_ledger_database::open(path, { });
			expect(database.has_value(), "the note migration fixture opens");
			const auto id = database->scope_id(scope_of("github.com", "octo", "app", 7));
			expect(id.has_value(), "the migration scope resolves");
			scope = *id;
			expect(database->store_note(scope, note(1U, 55U, "preserved")).has_value(), "the old note is seeded");
			bpr::tests::sqlite_contract::restore_v6_fixture(database->handle());
			for (const auto sql : { "ALTER TABLE local_note DROP COLUMN revision",
									"DROP TABLE local_note_sequence", "UPDATE ledger_meta SET schema_format=5",
									"PRAGMA user_version=5", "PRAGMA ignore_check_constraints=ON",
									"UPDATE local_note SET note_text=''" }) {
				expect(execute(database->handle(), sql, { }).has_value(), "the malformed legacy-row fixture is assembled");
			}
		}
		const auto before  = read_binary(path);
		const auto refused = review_ledger_database::open(path, { });
		expect(!refused, "migration refuses a legacy note that cannot satisfy current bounds");
		::sqlite3* raw = nullptr;
		expect(::sqlite3_open(path.string().c_str(), &raw) == SQLITE_OK, "the rollback fixture opens for inspection");
		const auto version = scalar_integer(raw, "PRAGMA user_version");
		const auto oldRows = scalar_integer(raw, "SELECT COUNT(*) FROM local_note WHERE note_text='' ");
		const auto partial = scalar_integer(raw, "SELECT COUNT(*) FROM sqlite_schema WHERE name IN ('local_note_v5','local_note_sequence')");
		expect(version && *version == 5 && oldRows && *oldRows == 1 && partial && *partial == 0,
			   "failed migration rolls back table replacement, sequence allocation and schema stamp together");
		expect(!before.empty(), "the legacy database held durable bytes before refusal");
		expect(execute(raw, "UPDATE local_note SET note_text='preserved'", { }).has_value(), "the designated fixture repairs only its invalid legacy text");
		(void)::sqlite3_close(raw);
		{
			auto migrated = review_ledger_database::open(path, { });
			expect(migrated.has_value(), "a retried valid migration succeeds");
			const auto state = migrated->load_private(scope);
			expect(state && state->Notes.size() == 1U && state->Notes.front().Revision.value() == 1U
					   && state->Notes.front().Note.Text == "preserved",
				   "legacy notes retain content and receive revision one");
			auto edit           = state->Notes.front();
			edit.Note.Text      = "after migration";
			const auto accepted = migrated->store_note(scope, edit, edit.Revision);
			expect(accepted && accepted->Revision.value() == 2U, "the migrated allocator exceeds every accepted legacy revision");
		}
		auto reopened = review_ledger_database::open(path, { });
		expect(reopened.has_value(), "schema-six reopen is idempotent");
		const auto state = reopened->load_private(scope);
		expect(state && state->Notes.front().Revision.value() == 2U
				   && state->Notes.front().Note.Text == "after migration",
			   "reopening never remigrates accepted revisions");
	}

	void test_two_connections_compare_note_revisions_without_aba()
	{
		const scratch_directory directory;
		const auto              path   = directory.file("note-cas.db");
		auto                    first  = review_ledger_database::open(path, { });
		auto                    second = review_ledger_database::open(path, { });
		expect(first && second, "two independent note connections open");
		const auto scope = first->scope_id(scope_of("github.com", "octo", "app", 7));
		expect(scope.has_value(), "the note conflict scope resolves");
		auto candidate      = note(1U, 22U, "original");
		candidate.Revision  = { };
		const auto original = first->store_note(*scope, candidate);
		expect(original && original->Revision.value() == 1U, "create allocates the first durable revision");
		const auto duplicate = second->store_note(*scope, *original);
		expect(!duplicate && duplicate.error().Reason == application::ledger_store_failure::conflict
				   && duplicate.error().Current == *original,
			   "implicit create never becomes an overwrite even with a candidate revision");
		auto winner         = *original;
		winner.Note.Text    = "winner";
		const auto accepted = first->store_note(*scope, winner, original->Revision);
		expect(accepted && accepted->Revision.value() == 2U, "the first expected-revision edit wins");
		auto loser             = *original;
		loser.Note.Text        = "loser";
		const auto stale       = second->store_note(*scope, loser, original->Revision);
		const auto staleDelete = second->erase_note(*scope, original->Id, original->Revision);
		expect(!stale && stale.error().Current == *accepted
				   && !staleDelete && staleDelete.error().Current == *accepted,
			   "the second connection rejects stale edits and deletes with the accepted record");
		for (const auto foreign : { scope_of("other.example", "octo", "app", 7),
									scope_of("github.com", "other", "app", 7), scope_of("github.com", "octo", "other", 7),
									scope_of("github.com", "octo", "app", 8) }) {
			const auto other = second->scope_id(foreign);
			expect(other.has_value(), "each foreign note scope resolves");
			const auto refused = second->store_note(*other, loser, accepted->Revision);
			expect(!refused && !refused.error().Current
					   && refused.error().Reason == application::ledger_store_failure::conflict,
				   "revision knowledge cannot create a note in a foreign review scope");
		}
		expect(first->erase_note(*scope, accepted->Id, accepted->Revision).has_value(), "the exact revision deletes");
		const auto missing = second->store_note(*scope, loser, accepted->Revision);
		expect(!missing && !missing.error().Current, "editing a deleted note conflicts with absence");
		const auto recreated = second->store_note(*scope, candidate);
		expect(recreated && recreated->Revision.value() > accepted->Revision.value(), "recreation never reuses an accepted revision");
		const auto aba = first->store_note(*scope, loser, original->Revision);
		expect(!aba && aba.error().Current == *recreated, "the original revision cannot overwrite the recreated identity");
		auto       reopened = review_ledger_database::open(path, { });
		const auto durable  = reopened->load_private(*scope);
		expect(durable && durable->Notes == std::vector{ *recreated }, "restart retains only the accepted note and revision");
		expect(execute(first->handle(), "UPDATE private_scope_state SET note_revision=9223372036854775807", { }).has_value(), "the exhaustion fixture advances its sequence");
		const auto exhausted = second->store_note(*scope, loser, recreated->Revision);
		expect(!exhausted && exhausted.error().Reason == application::ledger_store_failure::quota_exceeded,
			   "revision exhaustion refuses instead of wrapping");
	}

} // namespace

auto main() -> int
try {
	test_simultaneous_note_edits_have_one_winner();
	test_version_five_note_migration_is_atomic_and_repeatable();
	test_note_paths_migrate_and_round_trip();
	test_note_lines_migrate_and_round_trip();
	test_two_connections_compare_note_revisions_without_aba();
	test_a_directory_in_the_ledgers_place_is_a_typed_refusal();
	test_the_schema_stores_no_presentation_coordinate();
	test_receipts_do_not_leak_across_identity();
	test_a_version_one_database_migrates_forward();
	test_a_wrong_application_prior_version_is_refused_without_mutation();
	test_a_version_two_database_migrates_without_dropping_receipts();
	test_a_canonical_version_three_database_migrates_without_dropping_private_state();
	test_version_four_conversation_defect_lines_migrate_to_zero_based_once();
	test_wrong_application_version_three_is_refused_byte_for_byte();
	test_incompatible_retained_wal_is_refused_without_touching_any_file();
	test_preflight_revalidates_the_original_namespace_before_open();
	test_preflight_cleanup_preserves_substitutes_and_nonempty_directories();
	test_agent_session_ids_survive_restart_and_bounded_retention();
	test_two_open_stores_allocate_unique_monotonic_session_ids();
	test_a_newer_database_fails_closed();
	test_a_stamped_current_database_missing_a_required_table_fails_closed();
	test_a_stamped_current_database_with_a_malformed_column_fails_closed();
	test_a_stamped_current_database_missing_the_defect_unique_fails_closed();
	test_a_stamped_current_database_without_strict_mode_fails_closed();
	test_a_stamped_current_database_with_an_altered_check_fails_closed();
	test_reopening_is_idempotent();
	test_every_receipt_state_survives_a_restart();
	test_a_receipt_does_not_cross_scopes();
	test_conversation_receipts_round_trip_by_scope();
	test_private_records_survive_restart_isolate_and_delete();
	test_private_record_bounds_and_anchor_honesty_are_enforced();
#if !defined(_WIN32)
	test_a_killed_write_discards_only_the_uncommitted_work();
	test_a_killed_private_write_preserves_only_committed_records();
#endif
	test_deleting_the_ledger_leaves_the_neighbouring_store_intact();
	test_the_application_store_round_trips_private_records_by_scope();
#if !defined(_WIN32)
	test_the_ledger_composing_first_hardens_the_state_directory();
	test_a_symlinked_state_directory_is_refused_by_either_store();
	test_a_group_readable_state_directory_is_refused_by_either_store();
	test_a_public_storage_root_is_refused_by_either_store();
#endif

	std::cout << "review-ledger-storage-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
