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
			case detail::catalog_database_failure::integrity_failure:
			case detail::catalog_database_failure::schema_mismatch:
				return application::queue_store_failure::integrity_failure;
			case detail::catalog_database_failure::cancelled:
				return application::queue_store_failure::cancelled;
			case detail::catalog_database_failure::invalid_path:
			case detail::catalog_database_failure::invalid_permissions:
			case detail::catalog_database_failure::quota_exceeded:
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
				if (!rules) {
					return std::unexpected(translate(rules.error()));
				}
				if (!enrollments) {
					return std::unexpected(translate(enrollments.error()));
				}
				return application::durable_queue_state{
					.Rules       = std::move(*rules),
					.Enrollments = std::move(*enrollments),
				};
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
					directory->path() / QueueStoreName,
					aStop);
				if (!opened) {
					return std::unexpected(translate(opened.error()));
				}
				Database.emplace(std::move(*opened));
				return { };
			}

			std::filesystem::path                            StorageRoot;
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
