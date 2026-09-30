#include "review_cycle_desktop.hpp"

#include "bpr/adapters/sqlite/review_cycle_store.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/qt/review_cycle_view.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAction>
#include <QFileDialog>
#include <QRandomGenerator>

#include <array>

namespace bpr::app {
	namespace {
		/// Operation identities must not repeat across runs of the same profile; the
		/// global generator is securely seeded.
		auto random_seed() -> std::array<std::uint8_t, 32>
		{
			auto seed = std::array<std::uint8_t, 32>{ };
			for (auto& byte : seed) {
				byte = static_cast<std::uint8_t>(QRandomGenerator::global()->bounded(256U));
			}
			return seed;
		}
	}

	review_cycle_desktop::review_cycle_desktop(qt::workbench_window& aWindow, const std::filesystem::path& aProfileRoot, const core::bpr_profile_id aProfile,
											   application::effect_executor& aEffects, application::decision_executor& aDecisions)
		: Window{ aWindow }
		, Source{ adapters::workspace::make_local_workspace_source() }
	{
		Host = std::make_unique<application::review_cycle_workbench>(adapters::sqlite::make_review_cycle_store(aProfileRoot, aProfile), *Source, aEffects, aDecisions, *this, random_seed());
		View = new qt::review_cycle_view{ };
		Window.adopt_local_cycle(View);
		View->set_dispatch({
			.OpenFile    = [this](const std::string& aPath, const bool aNewTab) { Host->open_file(aPath, aNewTab); },
			.Tab         = [this](const core::document_tab_intent& aIntent) { Host->tab(aIntent); },
			.ObserveLine = [this](const core::document_tab_id aTab, const std::uint64_t aLine) { Host->observe_line(aTab, aLine); },
			.Comment     = [this](core::begin_cycle_feedback aNote) { Host->comment(std::move(aNote)); },
			.Suggest     = [this](application::review_cycle_suggestion_request aRequest) { Host->suggest(std::move(aRequest)); },
			.Relate      = [this](const core::cycle_feedback_item_ref& aItem, const std::uint64_t aVersion, core::cycle_related_reference aReference) { Host->relate(aItem, aVersion, std::move(aReference)); },
		});
		Window.set_local_cycle_entry_dispatch(
			[this] {
				const auto chosen = QFileDialog::getExistingDirectory(&Window, QStringLiteral("Open Project"));
				if (!chosen.isEmpty()) {
					open_project(chosen.toStdString());
				}
			},
			[this](const core::local_cycle_subject& aSubject) { Host->resume(aSubject); });
		Host->refresh_recents();
	}

	review_cycle_desktop::~review_cycle_desktop()
	{
		shutdown();
	}

	void review_cycle_desktop::publish(const application::review_cycle_workbench_projection& aProjection)
	{
		if (Closing) {
			return;
		}
		View->present(aProjection);
		if (aProjection.Cycle) {
			Window.show_local_cycle(QString::fromUtf8(aProjection.Cycle->Title.data(), static_cast<qsizetype>(aProjection.Cycle->Title.size())));
		} else if (!aProjection.Message.empty()) {
			// A refusal before any cycle opened is said on the page it was asked from.
			Window.show_local_cycle(QStringLiteral("Open Project"));
		}
		// Recents change when a cycle is created or first opened, not on every reading step.
		if (aProjection.Cycle && aProjection.Activity == application::review_cycle_activity::idle && aProjection.Cycle->Subject != LastSubject) {
			LastSubject = aProjection.Cycle->Subject;
			Host->refresh_recents();
		}
	}

	void review_cycle_desktop::publish(const std::vector<application::review_cycle_recent>& aRecents)
	{
		if (!Closing) {
			Window.present_local_cycle_recents(aRecents);
		}
	}

	void review_cycle_desktop::open_project(const std::string& aLocation)
	{
		Host->open_project(aLocation);
	}

	void review_cycle_desktop::leave()
	{
		Window.leave_local_cycle();
	}

	auto review_cycle_desktop::host() noexcept -> application::review_cycle_workbench&
	{
		return *Host;
	}

	void review_cycle_desktop::shutdown()
	{
		Closing = true;
		if (Host) {
			Host->shutdown();
		}
	}

	auto review_cycle_desktop::is_quiescent() const noexcept -> bool
	{
		return !Host || Host->is_quiescent();
	}
}
