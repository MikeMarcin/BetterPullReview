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
			// [[BPR-NNNN]]. The agent setup; the conversations are apart.
			std::pair{ std::string_view{ "agent-setup" }, area::settings },
			std::pair{ std::string_view{ "agent-sessions" }, area::agent_sessions },
			std::pair{ std::string_view{ "window-restoration" }, area::settings },
			std::pair{ std::string_view{ "view-filters" }, area::settings },
			std::pair{ std::string_view{ "discuss" }, area::settings },
			std::pair{ std::string_view{ "pull-request-activity" }, area::settings },
			std::pair{ std::string_view{ "review-ledger" }, area::review_data },
			std::pair{ std::string_view{ "review-tours" }, area::review_data },
			std::pair{ std::string_view{ "review-cycles" }, area::review_data },
			std::pair{ std::string_view{ "local-drafts" }, area::review_data },
			std::pair{ std::string_view{ "review-mutation-recovery" }, area::review_data },
			std::pair{ std::string_view{ "review-queue" }, area::saved_sources },
			std::pair{ std::string_view{ "insights" }, area::statistics },
			std::pair{ std::string_view{ "device-bindings" }, area::connections },
			// Named to make the boundary explicit; an unnamed store is kept too.
			// Remembered connections are edited by the resets that sign out,
			// never removed, so their identity high-water marks survive.
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
			std::pair{ std::string_view{ "local-draft-git-v1" }, area::review_data },
		};

		constexpr std::array names{
			std::pair{ std::string_view{ "caches" }, profile_data_reset::caches },
			std::pair{ std::string_view{ "settings" }, profile_data_reset::settings },
			std::pair{ std::string_view{ "review-data" }, profile_data_reset::review_data },
			std::pair{ std::string_view{ "connections" }, profile_data_reset::connections },
			std::pair{ std::string_view{ "everything" }, profile_data_reset::everything },
			std::pair{ std::string_view{ "agent-sessions" }, profile_data_reset::agent_sessions },
			std::pair{ std::string_view{ "saved-sources" }, profile_data_reset::saved_sources },
			std::pair{ std::string_view{ "statistics" }, profile_data_reset::statistics },
			std::pair{ std::string_view{ "agent-setup" }, profile_data_reset::agent_setup },
			std::pair{ std::string_view{ "agent-workspaces" }, profile_data_reset::agent_workspaces },
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

		/// The categories whose areas are stores or folders of the profile.
		constexpr auto file_categories = profile_data_reset_selection{ profile_data_reset::caches }
											 .with(profile_data_reset::settings)
											 .with(profile_data_reset::review_data)
											 .with(profile_data_reset::saved_sources)
											 .with(profile_data_reset::statistics)
											 .with(profile_data_reset::connections)
											 .with(profile_data_reset::agent_sessions);
		/// The categories whose data exists only on this computer.
		constexpr auto destructive_categories = profile_data_reset_selection{ profile_data_reset::review_data }
													.with(profile_data_reset::agent_sessions)
													.with(profile_data_reset::agent_workspaces)
													.with(profile_data_reset::saved_sources)
													.with(profile_data_reset::statistics);
		/// The categories whose removed stores a copy can hold.
		constexpr auto copied_categories = file_categories.without(profile_data_reset::connections);

		/// The categories both hold.
		[[nodiscard]] constexpr auto common(const profile_data_reset_selection aLeft, const profile_data_reset_selection aRight) noexcept -> profile_data_reset_selection
		{
			return aLeft.without(aLeft.without(aRight));
		}
	}

	auto profile_data_reset_selection::single() const noexcept -> std::optional<profile_data_reset>
	{
		if (*this == all()) {
			return profile_data_reset::everything;
		}
		for (const auto each : profile_data_reset_categories) {
			if (*this == profile_data_reset_selection{ each }) {
				return each;
			}
		}
		return std::nullopt;
	}

	auto profile_data_store_area(const std::string_view aStore) noexcept -> profile_data_area
	{
		return lookup(stores, aStore);
	}

	auto profile_data_folder_area(const std::string_view aFolder) noexcept -> profile_data_area
	{
		return lookup(folders, aFolder);
	}

	auto profile_data_reset_removes(const profile_data_reset_selection aReset, const profile_data_area aArea) noexcept -> bool
	{
		switch (aArea) {
		case area::caches:
			return aReset.contains(profile_data_reset::caches);
		case area::settings:
			return aReset.contains(profile_data_reset::settings);
		case area::review_data:
			return aReset.contains(profile_data_reset::review_data);
		case area::saved_sources:
			return aReset.contains(profile_data_reset::saved_sources);
		case area::statistics:
			return aReset.contains(profile_data_reset::statistics);
		case area::connections:
			return profile_data_reset_signs_out(aReset);
		case area::agent_sessions:
			return aReset.contains(profile_data_reset::agent_sessions);
		case area::kept:
			return false;
		}
		return false;
	}

	auto profile_data_reset_destructive(const profile_data_reset_selection aReset) noexcept -> bool
	{
		const auto closed = aReset.with(profile_data_reset_implied(aReset));
		return !common(closed, destructive_categories).empty();
	}

	auto profile_data_reset_restores_queue_display(const profile_data_reset_selection aReset) noexcept -> bool
	{
		return aReset.contains(profile_data_reset::settings) && !aReset.contains(profile_data_reset::saved_sources);
	}

	auto profile_data_reset_signs_out(const profile_data_reset_selection aReset) noexcept -> bool
	{
		return aReset.contains(profile_data_reset::connections);
	}

	auto profile_data_reset_removes_files(const profile_data_reset_selection aReset) noexcept -> bool
	{
		return !common(aReset, file_categories).empty();
	}

	auto profile_data_reset_copyable(const profile_data_reset_selection aReset) noexcept -> bool
	{
		return profile_data_reset_destructive(aReset) && !common(aReset, copied_categories).empty();
	}

	auto profile_data_reset_implied(const profile_data_reset_selection aChosen) noexcept -> profile_data_reset_selection
	{
		auto implied = profile_data_reset_selection{ };
		if (aChosen.contains(profile_data_reset::agent_sessions) && !aChosen.contains(profile_data_reset::agent_workspaces)) {
			implied = implied.with(profile_data_reset::agent_workspaces);
		}
		if (aChosen.contains(profile_data_reset::settings) && !aChosen.contains(profile_data_reset::agent_setup)) {
			implied = implied.with(profile_data_reset::agent_setup);
		}
		return implied;
	}

	auto plan_profile_data_reset(const profile_data_reset_selection aChosen) -> profile_data_reset_plan
	{
		auto plan      = profile_data_reset_plan{ };
		plan.Implied   = profile_data_reset_implied(aChosen);
		plan.Selection = aChosen.with(plan.Implied);
		// Before anything that removes or reads either store.
		if (plan.Selection.contains(profile_data_reset::settings) || plan.Selection.contains(profile_data_reset::agent_setup)
			|| plan.Selection.contains(profile_data_reset::agent_sessions) || plan.Selection.contains(profile_data_reset::agent_workspaces)) {
			plan.Steps.push_back(profile_data_reset_step::move_sessions);
		}
		if (plan.Selection.contains(profile_data_reset::agent_workspaces)) {
			plan.Steps.push_back(profile_data_reset_step::agent_workspaces);
		}
		if (plan.Selection.contains(profile_data_reset::agent_setup)) {
			plan.Steps.push_back(profile_data_reset_step::agent_setup);
		}
		if (profile_data_reset_removes_files(plan.Selection)) {
			plan.Steps.push_back(profile_data_reset_step::files);
		}
		if (profile_data_reset_restores_queue_display(plan.Selection)) {
			plan.Steps.push_back(profile_data_reset_step::queue_display);
		}
		if (profile_data_reset_signs_out(plan.Selection)) {
			plan.Steps.push_back(profile_data_reset_step::forget_connections);
		}
		return plan;
	}

	auto profile_data_reset_held_back(const std::uint32_t aWorkspacesLeft) noexcept -> profile_data_reset_selection
	{
		return aWorkspacesLeft != 0U ? profile_data_reset_selection{ profile_data_reset::agent_sessions }.with(profile_data_reset::agent_workspaces) : profile_data_reset_selection{ };
	}

	auto admit_profile_data_reset(const profile_data_reset_request& aRequest) noexcept -> release_maintenance_reason
	{
		const auto selection = aRequest.Reset.with(profile_data_reset_implied(aRequest.Reset));
		if (selection.empty()) {
			return release_maintenance_reason::invalid_selection;
		}
		const auto destructive = profile_data_reset_destructive(selection);
		if ((destructive && !aRequest.Acknowledged) || (aRequest.Copy && !profile_data_reset_copyable(selection))) {
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

	auto profile_data_reset_selection_from_names(const std::string_view aNames) noexcept -> std::optional<profile_data_reset_selection>
	{
		auto selection = profile_data_reset_selection{ };
		auto rest      = aNames;
		while (true) {
			const auto comma = rest.find(',');
			const auto name  = rest.substr(0U, comma);
			const auto named = profile_data_reset_from_name(name);
			if (!named) {
				return std::nullopt;
			}
			selection = selection.with(*named);
			if (comma == std::string_view::npos) {
				return selection;
			}
			rest.remove_prefix(comma + 1U);
		}
	}
}
