#include "bpr/core/profile_data_reset.hpp"

#include <array>
#include <utility>

namespace bpr::core {
	namespace {
		using area = profile_data_area;

		constexpr std::array stores{
			std::pair{ std::string_view{ "epoch-catalog" }, area::caches },
			std::pair{ std::string_view{ "markdown-images" }, area::caches },
			std::pair{ std::string_view{ "review-overviews" }, area::caches },
			std::pair{ std::string_view{ "appearance-settings" }, area::settings },
			std::pair{ std::string_view{ "workbench-settings" }, area::settings },
			std::pair{ std::string_view{ "diagnostic-preferences" }, area::settings },
			std::pair{ std::string_view{ "window-restoration" }, area::settings },
			std::pair{ std::string_view{ "review-ledger" }, area::review_data },
			std::pair{ std::string_view{ "review-tours" }, area::review_data },
			std::pair{ std::string_view{ "review-cycles" }, area::review_data },
			std::pair{ std::string_view{ "review-mutation-recovery" }, area::review_data },
			std::pair{ std::string_view{ "review-queue" }, area::saved_sources },
			std::pair{ std::string_view{ "insights" }, area::statistics },
			// Named to make the boundary explicit; an unnamed store is kept too.
			std::pair{ std::string_view{ "device-bindings" }, area::kept },
			std::pair{ std::string_view{ "remembered-connections" }, area::kept },
			std::pair{ std::string_view{ "profile-registry" }, area::kept },
			std::pair{ std::string_view{ "release-update" }, area::kept },
			std::pair{ std::string_view{ "update-draft-recovery" }, area::kept },
		};

		constexpr std::array folders{
			std::pair{ std::string_view{ "repositories" }, area::caches },
			std::pair{ std::string_view{ "incoming" }, area::caches },
			std::pair{ std::string_view{ "locks" }, area::caches },
			std::pair{ std::string_view{ "quarantine" }, area::caches },
			std::pair{ std::string_view{ "mcp-snapshot" }, area::caches },
			std::pair{ std::string_view{ "publication-git-v1" }, area::review_data },
		};

		constexpr std::array names{
			std::pair{ std::string_view{ "caches" }, profile_data_reset::caches },
			std::pair{ std::string_view{ "settings" }, profile_data_reset::settings },
			std::pair{ std::string_view{ "review-data" }, profile_data_reset::review_data },
			std::pair{ std::string_view{ "everything" }, profile_data_reset::everything },
		};

		template <typename Table>
		[[nodiscard]] constexpr auto lookup(const Table& aTable, const std::string_view aName) noexcept -> area
		{
			for (const auto& [name, value] : aTable) {
				if (name == aName) {
					return value;
				}
			}
			return area::kept;
		}

		[[nodiscard]] constexpr auto known(const profile_data_reset aReset) noexcept -> bool
		{
			return aReset == profile_data_reset::caches || aReset == profile_data_reset::settings
				|| aReset == profile_data_reset::review_data || aReset == profile_data_reset::everything;
		}
	}

	auto profile_data_store_area(const std::string_view aStore) noexcept -> profile_data_area
	{
		return lookup(stores, aStore);
	}

	auto profile_data_folder_area(const std::string_view aFolder) noexcept -> profile_data_area
	{
		return lookup(folders, aFolder);
	}

	auto profile_data_reset_removes(const profile_data_reset aReset, const profile_data_area aArea) noexcept -> bool
	{
		switch (aArea) {
		case area::caches:
			return aReset == profile_data_reset::caches || aReset == profile_data_reset::everything;
		case area::settings:
			return aReset == profile_data_reset::settings || aReset == profile_data_reset::everything;
		case area::review_data:
			return aReset == profile_data_reset::review_data || aReset == profile_data_reset::everything;
		case area::saved_sources:
		case area::statistics:
			return aReset == profile_data_reset::everything;
		case area::kept:
			return false;
		}
		return false;
	}

	auto profile_data_reset_destructive(const profile_data_reset aReset) noexcept -> bool
	{
		return aReset == profile_data_reset::review_data || aReset == profile_data_reset::everything;
	}

	auto profile_data_reset_restores_queue_display(const profile_data_reset aReset) noexcept -> bool
	{
		return aReset == profile_data_reset::settings;
	}

	auto admit_profile_data_reset(const profile_data_reset_request& aRequest) noexcept -> release_maintenance_reason
	{
		if (!known(aRequest.Reset)) {
			return release_maintenance_reason::invalid_selection;
		}
		const auto destructive = profile_data_reset_destructive(aRequest.Reset);
		if ((destructive && !aRequest.Acknowledged) || (!destructive && aRequest.Copy)) {
			return release_maintenance_reason::invalid_selection;
		}
		return release_maintenance_reason::none;
	}

	auto profile_data_reset_name(const profile_data_reset aReset) noexcept -> std::string_view
	{
		for (const auto& [name, value] : names) {
			if (value == aReset) {
				return name;
			}
		}
		return { };
	}

	auto profile_data_reset_from_name(const std::string_view aName) noexcept -> std::optional<profile_data_reset>
	{
		for (const auto& [name, value] : names) {
			if (name == aName) {
				return value;
			}
		}
		return std::nullopt;
	}
}
