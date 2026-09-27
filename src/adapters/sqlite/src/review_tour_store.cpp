#include "bpr/adapters/sqlite/review_tour_store.hpp"
#include "bpr/core/sha256.hpp"
#include "review_tour_codec.hpp"
#include "sqlite_connection.hpp"
#include "state_directory.hpp"
#include "storage_internal.hpp"
#include <algorithm>
#include <sqlite3.h>

namespace bpr::adapters::sqlite {
	namespace {
		using application::tour_store_failure;
		using application::tour_store_result;
		constexpr std::int64_t     application_id = 0x42505254;
		constexpr std::string_view target_schema  = "CREATE TABLE targets(key BLOB PRIMARY KEY,payload BLOB NOT NULL,digest BLOB NOT NULL) STRICT";
		constexpr std::string_view schema         = "CREATE TABLE tours(key BLOB PRIMARY KEY,payload BLOB NOT NULL,digest BLOB NOT NULL) STRICT";
		auto                       fail(tour_store_failure aFailure) { return std::unexpected(application::tour_store_error{ aFailure, { } }); }
		struct connection
		{
			sqlite3* Database = nullptr;
			~connection()
			{
				if (Database) {
					sqlite3_close_v2(Database);
				}
			}
		};
		struct rollback
		{
			sqlite3* Database;
			~rollback() { static_cast<void>(detail::execute(Database, "ROLLBACK")); }
		};
		struct budget
		{
			std::stop_token Stop;
			unsigned        Remaining = 10000;
			static auto     progress(void* aValue) -> int
			{
				auto& self = *static_cast<budget*>(aValue);
				return self.Stop.stop_requested() || self.Remaining-- == 0;
			}
		};
		auto commit(sqlite3* aDatabase, std::stop_token aStop) -> bool
		{
			if (aStop.stop_requested()) {
				return false;
			}
			const auto progress  = detail::progress_scope{ aDatabase, aStop };
			auto       statement = detail::prepare(aDatabase, "COMMIT");
			return statement && sqlite3_step(statement->Value) == SQLITE_DONE;
		}
		auto schema_valid(sqlite3* aDatabase, bool aLegacy = false) -> bool
		{
			const auto count      = detail::scalar_integer(aDatabase, "SELECT count(*) FROM (SELECT 1 FROM sqlite_schema WHERE name NOT LIKE 'sqlite_%' LIMIT 4)");
			const auto definition = detail::scalar_text(aDatabase, "SELECT sql FROM sqlite_schema WHERE name='tours' AND type='table'");
			const auto targets    = detail::scalar_text(aDatabase, "SELECT sql FROM sqlite_schema WHERE name='targets' AND type='table'");
			return detail::scalar_integer(aDatabase, "PRAGMA application_id") == application_id
				&& detail::scalar_integer(aDatabase, "PRAGMA user_version") == (aLegacy ? 1 : 2)
				&& count == (aLegacy ? 1 : 2) && definition && detail::canonical_sql(*definition) == detail::canonical_sql(schema)
				&& (aLegacy || (targets && detail::canonical_sql(*targets) == detail::canonical_sql(target_schema)));
		}
		auto establish(sqlite3* aDatabase, std::stop_token aStop) -> bool
		{
			sqlite3_limit(aDatabase, SQLITE_LIMIT_LENGTH, static_cast<int>(core::maximum_tour_bytes + 4096U));
			sqlite3_limit(aDatabase, SQLITE_LIMIT_SQL_LENGTH, 4096);
			const auto empty  = detail::scalar_integer(aDatabase, "PRAGMA user_version") == 0 && detail::scalar_integer(aDatabase, "PRAGMA application_id") == 0 && detail::scalar_integer(aDatabase, "SELECT count(*) FROM sqlite_schema") == 0;
			const auto legacy = schema_valid(aDatabase, true);
			if ((!empty && !legacy && !schema_valid(aDatabase)) || !detail::configure_connection(aDatabase) || aStop.stop_requested()) {
				return false;
			}
			if (!empty && !legacy) {
				return true;
			}
			if (!detail::execute(aDatabase, "BEGIN IMMEDIATE", aStop)) {
				return false;
			}
			const auto transaction = rollback{ aDatabase };
			return (!empty || (detail::execute(aDatabase, schema, aStop) && detail::execute(aDatabase, "PRAGMA application_id=1112560212", aStop)))
				&& detail::execute(aDatabase, target_schema, aStop) && detail::execute(aDatabase, "PRAGMA user_version=2", aStop)
				&& schema_valid(aDatabase) && commit(aDatabase, aStop);
		}
		void target_codec(detail::tour_codec& aCodec, application::tour_saved_target& aTarget)
		{
			aCodec.owner(aTarget.Owner);
			aCodec.id(aTarget.Draft);
			aCodec.id(aTarget.HostProfile);
			aCodec.text(aTarget.Hostname, 253);
			aCodec.text(aTarget.Repository.Owner, 100);
			aCodec.text(aTarget.Repository.Name, 100);
			aCodec.id(aTarget.Number);
			aCodec.oid(aTarget.Base);
			aCodec.oid(aTarget.Head);
		}
		auto valid_target(const application::tour_saved_target& aTarget) -> bool
		{
			return core::valid(aTarget.Owner) && aTarget.Draft.value() && aTarget.HostProfile.value()
				&& !aTarget.Hostname.empty() && aTarget.Hostname.size() <= 253 && core::valid(aTarget.Repository)
				&& aTarget.Number.value() && aTarget.Base.valid() && aTarget.Head.valid();
		}

		auto key(core::tour_owner aOwner, core::tour_draft_id aDraft) -> std::vector<std::uint8_t>
		{
			detail::tour_codec codec;
			codec.owner(aOwner);
			codec.id(aDraft);
			return std::move(codec.Bytes);
		}
		auto bind(sqlite3_stmt* aStatement, int aColumn, std::span<const std::uint8_t> aBytes) -> bool
		{
			return sqlite3_bind_blob(aStatement, aColumn, aBytes.data(), static_cast<int>(aBytes.size()), SQLITE_TRANSIENT) == SQLITE_OK;
		}
		auto blob(sqlite3_stmt* aStatement, int aColumn, std::size_t aLimit) -> std::span<const std::uint8_t>
		{
			const auto size = sqlite3_column_bytes(aStatement, aColumn);
			if (sqlite3_column_type(aStatement, aColumn) != SQLITE_BLOB || size <= 0 || static_cast<std::size_t>(size) > aLimit) {
				return { };
			}
			return { static_cast<const std::uint8_t*>(sqlite3_column_blob(aStatement, aColumn)), static_cast<std::size_t>(size) };
		}
		auto read(sqlite3* aDatabase, const core::tour_owner& aOwner, core::tour_draft_id aDraft) -> tour_store_result<std::optional<core::review_tour>>
		{
			auto       row        = detail::prepare(aDatabase, "SELECT payload,digest FROM tours WHERE key=?1 LIMIT 1");
			const auto encodedKey = key(aOwner, aDraft);
			if (!row || !bind(row->Value, 1, encodedKey)) {
				return fail(tour_store_failure::unavailable);
			}
			const auto result = sqlite3_step(row->Value);
			if (result == SQLITE_DONE) {
				return std::nullopt;
			}
			if (result != SQLITE_ROW) {
				return fail(tour_store_failure::unavailable);
			}
			const auto payload = blob(row->Value, 0, core::maximum_tour_bytes);
			const auto digest  = blob(row->Value, 1, 32);
			if (payload.empty() || digest.size() != 32 || !std::ranges::equal(core::sha256(payload), digest)) {
				return fail(tour_store_failure::corrupt);
			}
			detail::tour_codec codec{ { }, payload, true };
			core::review_tour  tour;
			codec.tour(tour);
			if (!codec.Good || !codec.Input.empty() || core::validate(tour) != core::tour_refusal::none || tour.Owner != aOwner || tour.Id != aDraft) {
				return fail(tour_store_failure::corrupt);
			}
			return tour;
		}
		class tour_store final : public application::review_tour_store
		{
		public:
			tour_store(std::filesystem::path aRoot, core::bpr_profile_id aProfile)
				: Root{ std::move(aRoot) }
				, Profile{ aProfile }
			{ }
			auto load(const core::tour_owner& aOwner, core::tour_draft_id aDraft, std::stop_token aStop) -> tour_store_result<std::optional<core::review_tour>> override
			{
				if (aOwner.Profile != Profile || !core::valid(aOwner) || !aDraft.value()) {
					return fail(tour_store_failure::wrong_profile);
				}
				auto opened = open(aStop);
				if (!opened) {
					return std::unexpected(opened.error());
				}
				const auto database = connection{ *opened };
				budget     work{ aStop };
				const auto bounded = detail::progress_handler_scope{ database.Database, 1000, &budget::progress, &work };
				auto       result  = read(database.Database, aOwner, aDraft);
				if (aStop.stop_requested()) {
					return fail(tour_store_failure::cancelled);
				}
				return result;
			}
			auto save(const core::review_tour& aTour, std::uint64_t aExpectedRevision, std::stop_token aStop) -> tour_store_result<core::review_tour> override
			{
				if (aTour.Owner.Profile != Profile) {
					return fail(tour_store_failure::wrong_profile);
				}
				if (core::validate(aTour) != core::tour_refusal::none || aTour.Revision <= aExpectedRevision) {
					return fail(tour_store_failure::bounds);
				}
				auto               candidate = aTour;
				detail::tour_codec codec;
				codec.tour(candidate);
				if (!codec.Good || codec.Bytes.size() > core::maximum_tour_bytes) {
					return fail(tour_store_failure::bounds);
				}
				auto opened = open(aStop);
				if (!opened) {
					return std::unexpected(opened.error());
				}
				const auto database = connection{ *opened };
				budget     work{ aStop };
				const auto bounded = detail::progress_handler_scope{ database.Database, 1000, &budget::progress, &work };
				if (!detail::execute(database.Database, "BEGIN IMMEDIATE", aStop)) {
					return fail(tour_store_failure::unavailable);
				}
				const auto transaction = rollback{ database.Database };
				const auto current     = read(database.Database, aTour.Owner, aTour.Id);
				if (!current) {
					return std::unexpected(current.error());
				}
				if ((current->has_value() ? current->value().Revision : 0U) != aExpectedRevision) {
					return std::unexpected(application::tour_store_error{ tour_store_failure::conflict, *current });
				}
				const auto count = detail::scalar_integer(database.Database, "SELECT count(*) FROM (SELECT 1 FROM tours LIMIT 129)");
				if (!count || *count > 128 || (!*current && *count >= 128)) {
					return fail(tour_store_failure::bounds);
				}
				auto row = detail::prepare(database.Database, "INSERT INTO tours(key,payload,digest) VALUES(?1,?2,?3) ON CONFLICT(key) DO UPDATE SET payload=excluded.payload,digest=excluded.digest");
				if (!row || !bind(row->Value, 1, key(aTour.Owner, aTour.Id)) || !bind(row->Value, 2, codec.Bytes) || !bind(row->Value, 3, core::sha256(codec.Bytes)) || sqlite3_step(row->Value) != SQLITE_DONE || !commit(database.Database, aStop)) {
					return fail(aStop.stop_requested() ? tour_store_failure::cancelled : tour_store_failure::unavailable);
				}
				return aTour;
			}

			auto targets(std::stop_token aStop) -> tour_store_result<std::vector<application::tour_saved_target>> override
			{
				auto opened = open(aStop);
				if (!opened) {
					return std::unexpected(opened.error());
				}
				const auto database = connection{ *opened };
				auto       rows     = detail::prepare(database.Database, "SELECT payload,digest FROM targets LIMIT 129");
				if (!rows) {
					return fail(tour_store_failure::unavailable);
				}
				std::vector<application::tour_saved_target> result;
				int                                         status = SQLITE_OK;
				while ((status = sqlite3_step(rows->Value)) == SQLITE_ROW) {
					if (aStop.stop_requested()) {
						return fail(tour_store_failure::cancelled);
					}
					const auto payload = blob(rows->Value, 0, 4096), digest = blob(rows->Value, 1, 32);
					if (payload.empty() || digest.size() != 32 || !std::ranges::equal(core::sha256(payload), digest) || result.size() == 128) {
						return fail(tour_store_failure::corrupt);
					}
					detail::tour_codec             codec{ { }, payload, true };
					application::tour_saved_target target;
					target_codec(codec, target);
					if (!codec.Good || !codec.Input.empty() || !valid_target(target) || target.Owner.Profile != Profile) {
						return fail(tour_store_failure::corrupt);
					}
					result.push_back(std::move(target));
				}
				if (status != SQLITE_DONE) {
					return fail(tour_store_failure::unavailable);
				}
				return result;
			}
			auto remember_target(const application::tour_saved_target& aTarget, std::stop_token aStop) -> tour_store_result<void> override
			{
				if (!valid_target(aTarget) || aTarget.Owner.Profile != Profile) {
					return fail(tour_store_failure::wrong_profile);
				}
				auto opened = open(aStop);
				if (!opened) {
					return std::unexpected(opened.error());
				}
				const auto         database = connection{ *opened };
				auto               target   = aTarget;
				detail::tour_codec codec;
				target_codec(codec, target);
				if (!codec.Good || !detail::execute(database.Database, "BEGIN IMMEDIATE", aStop)) {
					return fail(tour_store_failure::unavailable);
				}
				const auto transaction = rollback{ database.Database };
				const auto count       = detail::scalar_integer(database.Database, "SELECT count(*) FROM (SELECT 1 FROM targets LIMIT 129)");
				auto       exists      = detail::prepare(database.Database, "SELECT 1 FROM targets WHERE key=?1");
				const auto encodedKey  = key(target.Owner, target.Draft);
				if (!count || *count > 128 || !exists || !bind(exists->Value, 1, encodedKey)) {
					return fail(tour_store_failure::unavailable);
				}
				const auto present = sqlite3_step(exists->Value);
				if (present != SQLITE_ROW && present != SQLITE_DONE) {
					return fail(tour_store_failure::unavailable);
				}
				if (present == SQLITE_DONE && *count >= 128) {
					return fail(tour_store_failure::bounds);
				}
				auto row = detail::prepare(database.Database, "INSERT INTO targets(key,payload,digest) VALUES(?1,?2,?3) ON CONFLICT(key) DO UPDATE SET payload=excluded.payload,digest=excluded.digest");
				if (!row || !bind(row->Value, 1, encodedKey) || !bind(row->Value, 2, codec.Bytes) || !bind(row->Value, 3, core::sha256(codec.Bytes)) || sqlite3_step(row->Value) != SQLITE_DONE || !commit(database.Database, aStop)) {
					return fail(tour_store_failure::unavailable);
				}
				return { };
			}

		private:
			auto open(std::stop_token aStop) -> tour_store_result<sqlite3*>
			{
				if (aStop.stop_requested()) {
					return fail(tour_store_failure::cancelled);
				}
				auto directory = detail::open_state_directory(Root);
				if (!directory || !detail::sqlite_runtime_ready()) {
					return fail(tour_store_failure::unavailable);
				}
				const auto opened = detail::open_private_store_database(*directory, "review-tours.sqlite3", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_NOFOLLOW);
				if (!opened) {
					return fail(tour_store_failure::unavailable);
				}
				if (!establish(*opened, aStop)) {
					sqlite3_close_v2(*opened);
					return fail(tour_store_failure::schema_mismatch);
				}
				return *opened;
			}
			std::filesystem::path Root;
			core::bpr_profile_id  Profile;
		};
	}
	auto make_review_tour_store(std::filesystem::path aRoot, core::bpr_profile_id aProfile) -> std::unique_ptr<application::review_tour_store>
	{
		return std::make_unique<tour_store>(std::move(aRoot), aProfile);
	}
}
