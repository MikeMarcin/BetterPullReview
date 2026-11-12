#include "local_draft_desktop.hpp"

#include "bpr/adapters/git/installed_git_publication.hpp"
#include "bpr/adapters/sqlite/local_draft_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/core/diff_line_matcher.hpp"
#include "bpr/qt/local_draft_evidence_area.hpp"
#include "bpr/qt/local_draft_view.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "system_browser_opener.hpp"

#include <QDateTime>
#include <QFileDialog>
#include <QRandomGenerator>

#include <array>
#include <utility>

namespace bpr::app {
	namespace {
		auto random_seed() -> std::array<std::uint8_t, 32>
		{
			auto seed = std::array<std::uint8_t, 32>{ };
			for (auto& byte : seed) {
				byte = static_cast<std::uint8_t>(QRandomGenerator::global()->bounded(256U));
			}
			return seed;
		}
	}

	local_draft_desktop::local_draft_desktop(qt::workbench_window& aWindow, const std::filesystem::path& aProfileRoot, std::filesystem::path aGitExecutable,
											 std::optional<std::filesystem::path> aWindowsSystemRoot, application::effect_executor& aEffects, application::decision_executor& aDecisions)
		: Window{ aWindow }
		, Source{ adapters::workspace::make_local_workspace_source() }
	{
		auto git = [root = aProfileRoot, executable = std::move(aGitExecutable), windows = std::move(aWindowsSystemRoot)](
					   const std::stop_token aStop) -> application::publication_git_result<std::unique_ptr<application::local_draft_git_port>> {
			const auto isolation = root / "local-draft-git-v1";
			if (executable.empty() || !adapters::sqlite::create_storage_root_owner_private(isolation)) {
				return std::unexpected{ application::publication_git_error{ .Failure = application::publication_git_failure::unavailable } };
			}
			auto made = adapters::git::make_installed_git_local_draft_reader({ .GitExecutable = executable, .IsolationRoot = isolation, .WindowsSystemRoot = windows }, aStop);
			if (!made) {
				return std::unexpected{ application::publication_git_error{ .Failure = application::publication_git_failure::unavailable, .Detail = made.error().Detail } };
			}
			return std::move(*made);
		};
		Host = std::make_unique<application::local_draft_workbench>(
			[root = aProfileRoot] { return adapters::sqlite::open_local_draft_store(root); }, *Source, std::move(git), aEffects, aDecisions, *this, random_seed(),
			[] { return static_cast<std::int64_t>(QDateTime::currentSecsSinceEpoch()); }, core::make_xdiff_line_matcher());
		View = new qt::local_draft_view{ };
		Window.adopt_local_draft(View);
		View->set_dispatch({
			.OpenFile      = [this](const std::string& aPath) { Host->open_file(aPath); },
			.Include       = [this](const std::string& aPath, const bool aIncluded) { Host->include(aPath, aIncluded); },
			.Refresh       = [this] { Host->refresh(); },
			.ShowCandidate = [this](const std::uint64_t aOrdinal) { Host->show_candidate(aOrdinal); },
			.AddNote       = [this](const std::string& aText, const std::string& aPath) { Host->add_note(aText, aPath); },
			.RemoveNote    = [this](const std::uint64_t aNote) { Host->remove_note(aNote); },
			.Rename        = [this](const std::string& aTitle) { Host->rename(aTitle); },
			.ChooseTarget  = [this](core::local_draft_target aTarget) { Host->choose_target(std::move(aTarget)); },
			.Rebind        = [this] { Host->rebind(); },
			.Discard       = [this] { Host->discard(); },
			.Close =
				[this] {
					Host->close();
					Window.leave_local_draft();
				},
		});
		// RFC-BPR-0109: the Evidence area. Nothing here runs a check; a link
		// opens in the system browser only as a validated web address.
		View->evidence()->set_dispatch({
			.AddClaim        = [this](const core::claim_kind aKind, const std::string& aText) { Host->add_claim(aKind, aText); },
			.ReviseClaim     = [this](const std::uint64_t aClaim, const std::string& aText) { Host->revise_claim(aClaim, aText); },
			.AddCheck        = [this](const core::check_method aMethod, const std::string& aTitle, const std::string& aCommand, std::vector<std::uint64_t> aClaims,
									  core::formal_scope aFormal) { Host->add_check(aMethod, aTitle, aCommand, std::move(aClaims), std::move(aFormal)); },
			.RecordResult    = [this](core::manual_result aResult) { Host->record_result(std::move(aResult)); },
			.AddQuestion     = [this](const std::string& aText, const std::uint64_t aClaim) { Host->add_question(aText, aClaim); },
			.ResolveQuestion = [this](const std::uint64_t aQuestion, const bool aResolved) { Host->resolve_question(aQuestion, aResolved); },
			.Decide          = [this](const std::uint64_t aRecord, const core::evidence_disposition aDisposition) { Host->decide_evidence(aRecord, aDisposition); },
			.Select          = [this](const std::uint64_t aRecord, const bool aSelected) { Host->select_evidence(aRecord, aSelected); },
			.Import =
				[this] {
					const auto chosen = EvidenceChooser ? EvidenceChooser()
														: QFileDialog::getOpenFileName(&Window, QStringLiteral("Import Evidence"), QString{ }, QStringLiteral("Evidence documents (*.json)"));
					if (!chosen.isEmpty()) {
						Host->import_evidence(chosen.toStdString());
					}
				},
			.OpenArtifact = [this](const std::uint64_t aResult, const std::size_t aIndex) { Host->open_artifact(aResult, aIndex); },
			.OpenLink =
				[this](const std::uint64_t aResult, const std::size_t aIndex) {
					const auto& draft = Host->projection().Draft;
					if (!draft) {
						return;
					}
					const auto& results = draft->Evidence.Results;
					const auto  found   = std::ranges::find(results, aResult, &core::evidence_result::Id);
					if (found == results.end() || aIndex >= found->Artifacts.size() || found->Artifacts[aIndex].State != core::artifact_state::link) {
						return;
					}
					const auto& address = found->Artifacts[aIndex].Reference;
					if (LinkOpener) {
						static_cast<void>(LinkOpener(address));
					} else {
						static_cast<void>(system_browser_opener{ }.open(address));
					}
				},
			.CloseArtifact = [this] { Host->close_artifact(); },
		});
		Window.set_local_draft_entry_dispatch(
			[this] {
				const auto chosen = Chooser ? Chooser() : QFileDialog::getExistingDirectory(&Window, QStringLiteral("Prepare Pull Request"));
				if (!chosen.isEmpty()) {
					open_location(chosen.toStdString());
				}
			},
			[this](const core::local_draft_id& aId) {
				BringForward = true;
				Host->resume(aId);
			});
		Host->refresh_home();
	}

	local_draft_desktop::~local_draft_desktop()
	{
		shutdown();
	}

	void local_draft_desktop::publish(const application::local_draft_workbench_projection& aProjection)
	{
		if (Closing) {
			return;
		}
		View->present(aProjection);
		if (aProjection.Stage == application::local_draft_stage::closed) {
			Window.leave_local_draft();
			return;
		}
		if (std::exchange(BringForward, false) || Window.showing_local_draft()) {
			Window.show_local_draft(View->title_text());
		}
	}

	void local_draft_desktop::publish(const application::local_draft_home_projection& aHome)
	{
		if (!Closing) {
			Window.present_local_drafts(aHome);
		}
	}

	void local_draft_desktop::open_location(const std::string& aLocation, std::optional<std::string> aContext)
	{
		BringForward = true;
		Host->open_location(aLocation, std::move(aContext));
	}

	void local_draft_desktop::set_folder_chooser(folder_chooser aChooser)
	{
		Chooser = std::move(aChooser);
	}

	void local_draft_desktop::show_draft(const core::local_draft_id& aId)
	{
		if (Closing) {
			return;
		}
		BringForward = true;
		Host->resume(aId);
		Window.raise();
		Window.activateWindow();
		Host->refresh_home();
	}

	void local_draft_desktop::set_evidence_chooser(folder_chooser aChooser)
	{
		EvidenceChooser = std::move(aChooser);
	}

	void local_draft_desktop::set_link_opener(link_opener aOpener)
	{
		LinkOpener = std::move(aOpener);
	}

	void local_draft_desktop::set_code_font(const QFont& aFont)
	{
		View->set_code_font(aFont);
	}

	auto local_draft_desktop::host() noexcept -> application::local_draft_workbench&
	{
		return *Host;
	}

	auto local_draft_desktop::view() noexcept -> qt::local_draft_view*
	{
		return View;
	}

	void local_draft_desktop::shutdown()
	{
		Closing = true;
		// The page outlives this desktop in the window; a gesture it sends
		// while it is torn down (a title losing focus) reaches nothing.
		// A window already gone took the page with it: nothing to detach.
		if (View) {
			View->set_dispatch({ });
			View->evidence()->set_dispatch({ });
			Window.set_local_draft_entry_dispatch({ }, { });
		}
		if (Host) {
			Host->shutdown();
		}
	}

	auto local_draft_desktop::is_quiescent() const noexcept -> bool
	{
		return !Host || Host->is_quiescent();
	}
}
