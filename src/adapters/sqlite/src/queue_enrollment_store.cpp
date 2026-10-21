#include "bpr/adapters/sqlite/queue_enrollment_store.hpp"

#include "queue_enrollment_database.hpp"
#include "state_directory.hpp"

#include <optional>
#include <utility>

namespace bpr::adapters::sqlite {
	namespace {
		constexpr std::string_view QueueStoreName = "review-queue.sqlite3";

		[[nodiscard]] constexpr auto translate(
			const detail::catalog_database_failure aFailure) noexcept
			-> application::queue_store_failure
		{
			switch (aFailure) {
			case detail::catalog_database_failure::quota_exceeded:
				return application::queue_store_failure::collection_limit;
			case detail::catalog_database_failure::integrity_failure:
			case detail::catalog_database_failure::schema_mismatch:
				return application::queue_store_failure::integrity_failure;
			case detail::catalog_database_failure::cancelled:
				return application::queue_store_failure::cancelled;
			case detail::catalog_database_failure::invalid_path:
			case detail::catalog_database_failure::invalid_permissions:
			case detail::catalog_database_failure::unavailable:
			case detail::catalog_database_failure::platform_unsupported:
				return application::queue_store_failure::unavailable;
			}
			return application::queue_store_failure::unavailable;
		}

		class sqlite_queue_enrollment_store final
			: public application::queue_enrollment_store_port
		{
		public:
			explicit sqlite_queue_enrollment_store(
				std::filesystem::path aStorageRoot)
				: StorageRoot{ std::move(aStorageRoot) }
			{
			}

			auto load(std::stop_token aStop) -> std::expected<
				application::durable_queue_state,
				application::queue_store_failure> override
			{
				if (auto ready = ensure_open(aStop); !ready) {
					return std::unexpected(ready.error());
				}
				auto rules       = Database->load_rules();
				auto enrollments = Database->load_all();
				auto preferences = Database->load_preferences();
				auto saved       = Database->load_saved_repositories();
				if (!rules) {
					return std::unexpected(translate(rules.error()));
				}
				if (!enrollments) {
					return std::unexpected(translate(enrollments.error()));
				}
				if (!preferences) {
					return std::unexpected(translate(preferences.error()));
				}
				if (!saved) {
					return std::unexpected(translate(saved.error()));
				}
				auto servers = Database->load_saved_servers();
				if (!servers) {
					return std::unexpected(translate(servers.error()));
				}
				auto ignored = Database->load_home_ignores();
				if (!ignored) {
					return std::unexpected(translate(ignored.error()));
				}
				return application::durable_queue_state{
					.Rules             = std::move(*rules),
					.Enrollments       = std::move(*enrollments),
					.Preferences       = std::move(*preferences),
					.SavedRepositories = std::move(*saved),
					.SavedServers      = std::move(*servers),
					.Ignored           = std::move(*ignored),
				};
			}

			auto store_preferences(
				const application::queue_presentation_preferences& aPreferences,
				std::stop_token                                    aStop)
				-> std::expected<void, application::queue_store_failure> override
			{
				if (!application::valid(aPreferences)) {
					return std::unexpected(
						application::queue_store_failure::integrity_failure);
				}
				if (auto ready = ensure_open(aStop); !ready) {
					return ready;
				}
				auto stored = Database->store_preferences(aPreferences);
				return stored
					? std::expected<void, application::queue_store_failure>{ }
					: std::unexpected(translate(stored.error()));
			}

			auto apply_saved_repository_change(
				const application::saved_repository_intent& aIntent,
				std::stop_token                             aStop)
				-> std::expected<void, application::queue_store_failure> override
			{
				if (!application::valid(aIntent.Repository)) {
					return std::unexpected(
						application::queue_store_failure::integrity_failure);
				}
				if (auto ready = ensure_open(aStop); !ready) {
					return ready;
				}
				auto changed = aIntent.Action == application::saved_repository_action::add
					? Database->store_saved_repository(aIntent.Repository)
					: Database->erase_saved_repository(aIntent.Repository);
				return changed
					? std::expected<void, application::queue_store_failure>{ }
					: std::unexpected(translate(changed.error()));
			}

			auto apply_saved_server_change(
				const application::saved_server_intent& aIntent,
				std::stop_token                         aStop)
				-> std::expected<void, application::queue_store_failure> override
			{
				if (!application::valid(aIntent.Server)) {
					return std::unexpected(
						application::queue_store_failure::integrity_failure);
				}
				if (auto ready = ensure_open(aStop); !ready) {
					return ready;
				}
				auto changed = aIntent.Action == application::saved_server_action::add
					? Database->store_saved_server(aIntent.Server)
					: Database->erase_saved_server(aIntent.Server);
				return changed
					? std::expected<void, application::queue_store_failure>{ }
					: std::unexpected(translate(changed.error()));
			}

			auto apply_rule_change(
				const application::queue_rule_intent&                 aIntent,
				const std::span<const application::enrollment_record> aEnrollments,
				std::stop_token                                       aStop)
				-> std::expected<void, application::queue_store_failure> override
			{
				if (!core::valid(aIntent.Rule)) {
					return std::unexpected(
						application::queue_store_failure::invalid_rule);
				}
				if (auto ready = ensure_open(aStop); !ready) {
					return ready;
				}
				auto changed = Database->apply_rule_change(
					aIntent, aEnrollments, aStop);
				return changed
					? std::expected<void, application::queue_store_failure>{ }
					: std::unexpected(translate(changed.error()));
			}

			auto store_enrollment(
				const application::enrollment_record& aRecord,
				std::stop_token                       aStop)
				-> std::expected<void, application::queue_store_failure> override
			{
				if (auto ready = ensure_open(aStop); !ready) {
					return ready;
				}
				auto stored = Database->store(aRecord);
				return stored
					? std::expected<void, application::queue_store_failure>{ }
					: std::unexpected(translate(stored.error()));
			}

			auto store_enrollments(
				const std::span<const application::enrollment_record> aRecords,
				std::stop_token                                       aStop)
				-> std::expected<void, application::queue_store_failure> override
			{
				if (auto ready = ensure_open(aStop); !ready) {
					return ready;
				}
				auto stored = Database->store_all(aRecords, aStop);
				return stored
					? std::expected<void, application::queue_store_failure>{ }
					: std::unexpected(translate(stored.error()));
			}

			auto apply_home_ignores(
				const std::span<const core::home_ignore_delta> aDeltas,
				std::stop_token                                aStop)
				-> std::expected<void, application::queue_store_failure> override
			{
				if (auto ready = ensure_open(aStop); !ready) {
					return ready;
				}
				auto stored = Database->apply_home_ignores(aDeltas, aStop);
				return stored
					? std::expected<void, application::queue_store_failure>{ }
					: std::unexpected(translate(stored.error()));
			}

		private:
			[[nodiscard]] auto ensure_open(std::stop_token aStop)
				-> std::expected<void, application::queue_store_failure>
			{
				if (aStop.stop_requested()) {
					return std::unexpected(
						application::queue_store_failure::cancelled);
				}
				if (Database.has_value()) {
					return { };
				}
				auto directory = detail::open_state_directory(StorageRoot);
				if (!directory) {
					return std::unexpected(
						application::queue_store_failure::unavailable);
				}
				auto opened = detail::queue_enrollment_database::open(
					*directory, QueueStoreName, aStop);
				if (!opened) {
					return std::unexpected(translate(opened.error()));
				}
				Directory.emplace(std::move(*directory));
				Database.emplace(std::move(*opened));
				return { };
			}

			std::filesystem::path StorageRoot;
			// [[BPR-0883]]. Held for as long as the database is open, as
			// `state_directory.hpp` promises. Declared first, released last.
			std::optional<detail::state_directory>           Directory;
			std::optional<detail::queue_enrollment_database> Database;
		};
	} // namespace

	auto make_queue_enrollment_store(
		queue_enrollment_configuration aConfiguration)
		-> std::unique_ptr<application::queue_enrollment_store_port>
	{
		return std::make_unique<sqlite_queue_enrollment_store>(
			std::move(aConfiguration.StorageRoot));
	}
} // namespace bpr::adapters::sqlite
