#include "review_cycle_desktop.hpp"

#include "bpr/adapters/sqlite/review_cycle_store.hpp"
#include "bpr/adapters/syntax/packaged_syntax_classifier.hpp"
#include "bpr/adapters/workspace/local_export_destination.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/core/git_objects.hpp"
#include "bpr/qt/review_cycle_view.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAction>
#include <QFileDialog>
#include <QRandomGenerator>

#include <algorithm>
#include <array>
#include <filesystem>
#include <mutex>
#include <span>
#include <string>
#include <utility>

namespace bpr::app {
	/// \brief Serves the texts Suggest change asks about to the classifier
	/// (BPR-1156).
	/// \details The classifier reads bytes by blob identity and the texts
	/// being composed exist only in memory, so each one is held here, by the
	/// identity of its exact bytes, while it is classified. Held on the
	/// decision lane and read on the classifier's effect lane, hence the
	/// lock. Bounded: a text is released when its classification is
	/// published and the oldest goes beyond the bound, which only makes a
	/// late read unavailable.
	class suggestion_text_source final : public adapters::syntax::blob_source
	{
	public:
		void hold(const core::git_blob_oid& aBlob, std::shared_ptr<const std::string> aText)
		{
			const auto guard = std::lock_guard{ Mutex };
			if (std::ranges::find(Texts, aBlob, &held_text::Blob) != Texts.end()) {
				return;
			}
			if (Texts.size() >= MaximumTexts) {
				Texts.erase(Texts.begin());
			}
			Texts.push_back({ .Blob = aBlob, .Text = std::move(aText) });
		}

		void release(const core::git_blob_oid& aBlob)
		{
			const auto guard = std::lock_guard{ Mutex };
			std::erase_if(Texts, [&aBlob](const held_text& aText) { return aText.Blob == aBlob; });
		}

		[[nodiscard]] auto blob_bytes(const core::git_blob_oid& aBlob) -> std::optional<std::string> override
		{
			const auto guard = std::lock_guard{ Mutex };
			const auto held  = std::ranges::find(Texts, aBlob, &held_text::Blob);
			return held == Texts.end() ? std::nullopt : std::optional{ *held->Text };
		}

	private:
		struct held_text
		{
			core::git_blob_oid                 Blob;
			std::shared_ptr<const std::string> Text;
		};

		static constexpr std::size_t MaximumTexts = 8U;
		std::mutex                   Mutex;
		std::vector<held_text>       Texts;
	};

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
											   application::effect_executor& aEffects, application::decision_executor& aDecisions, const review_cycle_syntax aSyntax)
		: Window{ aWindow }
		, Source{ adapters::workspace::make_local_workspace_source() }
	{
		Host = std::make_unique<application::review_cycle_workbench>(adapters::sqlite::make_review_cycle_store(aProfileRoot, aProfile), *Source, aEffects, aDecisions, *this, random_seed());
		View = new qt::review_cycle_view{ };
		Window.adopt_local_cycle(View);
		if (aSyntax.Definitions != nullptr && aSyntax.Effects != nullptr) {
			// The review panes' classifier over the same packaged definitions,
			// with its own small cache: a proposal is a few whole-file texts,
			// and keystrokes must not evict what the panes retain.
			SyntaxDefinitions = aSyntax.Definitions;
			SyntaxTexts       = std::make_unique<suggestion_text_source>();
			SyntaxClassifier  = std::make_unique<adapters::syntax::packaged_syntax_classifier>(*SyntaxDefinitions, *SyntaxTexts);
			SyntaxHost        = std::make_unique<application::syntax_classification_application_host>(
				*SyntaxClassifier, *aSyntax.Effects, aDecisions, static_cast<application::appearance_projection_port&>(*this), core::application_session_id{ 1U },
				application::appearance_host_limits{ .OutstandingRequests = 4U, .TokenSpanCacheEntries = 4U });
			View->composer()->SyntaxWanted = [this](const qt::suggestion_syntax_request& aRequest) { want_syntax(aRequest); };
		}
		View->set_dispatch({
			.OpenFile     = [this](const std::string& aPath, const bool aNewTab) { Host->open_file(aPath, aNewTab); },
			.Tab          = [this](const core::document_tab_intent& aIntent) { Host->tab(aIntent); },
			.ObserveLine  = [this](const core::document_tab_id aTab, const std::uint64_t aLine) { Host->observe_line(aTab, aLine); },
			.Comment      = [this](core::begin_cycle_feedback aNote) { Host->comment(std::move(aNote)); },
			.Suggest      = [this](application::review_cycle_suggestion_request aRequest) { Host->suggest(std::move(aRequest)); },
			.Relate       = [this](const core::cycle_feedback_item_ref& aItem, const std::uint64_t aVersion, core::cycle_related_reference aReference) { Host->relate(aItem, aVersion, std::move(aReference)); },
			.Capture      = [this](const bool aDeliberate) { Host->capture_revision(aDeliberate); },
			.Compare      = [this](const core::cycle_view_request& aRequest) { Host->compare(aRequest); },
			.FullSource   = [this] { Host->full_source(); },
			.Refresh      = [this] { Host->refresh_view(); },
			.MarkReviewed = [this] { Host->mark_reviewed(); },
			.ChooseResponse =
				[this](const core::cycle_round_id aRound) {
					const auto chosen = QFileDialog::getOpenFileName(&Window, QStringLiteral("Open a response file"), { }, QStringLiteral("Review responses (*.json);;All files (*)"));
					if (!chosen.isEmpty()) {
						Host->import_response(aRound, chosen.toStdString());
					}
				},
			.Reply            = [this](const core::cycle_round_id aRound, core::cycle_manual_reply aReply) { Host->attach_reply(aRound, std::move(aReply)); },
			.Decide           = [this](const core::cycle_feedback_item_ref& aItem, const std::optional<core::cycle_reviewer_decision> aDecision) { Host->decide(aItem, aDecision); },
			.PrepareNextRound = [this] { Host->prepare_next_round(); },
		});
		// A chosen response file is read on the effect lane, never through a link.
		Host->set_file_reader([](const std::string& aPath, const std::uint64_t aMaximum, std::stop_token aStop) {
			// The path is UTF-8 on every platform, so it is not read in a code page.
			return adapters::workspace::read_chosen_file(std::filesystem::path{ std::u8string{ aPath.begin(), aPath.end() } }, aMaximum, std::move(aStop));
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

	void review_cycle_desktop::adopt_appearance(const qt::appearance_profile& aProfile)
	{
		View->composer()->adopt_appearance(aProfile);
	}

	void review_cycle_desktop::want_syntax(const qt::suggestion_syntax_request& aRequest)
	{
		if (Closing || !SyntaxHost) {
			return;
		}
		auto       document                             = std::make_shared<const std::string>(aRequest.Document);
		const auto bytes                                = std::span{ reinterpret_cast<const std::uint8_t*>(document->data()), document->size() };
		Wanted[static_cast<std::size_t>(aRequest.Side)] = wanted_syntax{
			.Key      = { .Blob = core::git_blob_identity(bytes, core::git_hash_algorithm::sha1), .Definition = SyntaxDefinitions->definition_for_path(aRequest.Path) },
			.Document = std::move(document),
		};
		submit_syntax();
	}

	void review_cycle_desktop::submit_syntax()
	{
		// Latest wins: one text per side waits and at most two are classified
		// at once, so typing never queues a classification per keystroke.
		constexpr auto running = std::size_t{ 2U };
		for (auto& wanted : Wanted) {
			if (Closing || !wanted || wanted->Submitted || SyntaxHost->outstanding_requests() >= running) {
				continue;
			}
			wanted->Submitted = true;
			SyntaxTexts->hold(wanted->Key.Blob, wanted->Document);
			SyntaxHost->classify(wanted->Key);
		}
	}

	void review_cycle_desktop::publish(const application::appearance_projection& aProjection)
	{
		if (aProjection.Spans) {
			const auto& spans = *aProjection.Spans;
			SyntaxTexts->release(spans.Key.Blob);
			for (const auto side : { qt::suggestion_syntax_side::original, qt::suggestion_syntax_side::proposed }) {
				const auto& wanted = Wanted[static_cast<std::size_t>(side)];
				if (!Closing && wanted && wanted->Key == spans.Key) {
					View->composer()->adopt_syntax(side, *wanted->Document, spans.Spans);
				}
			}
		}
		submit_syntax();
	}

	void review_cycle_desktop::fail(const application::appearance_failure_publication& /*aFailure*/)
	{
		// The text stays plain; whatever waits behind it may go now.
		submit_syntax();
	}

	void review_cycle_desktop::rejected(const application::appearance_terminal_rejection /*aRejection*/)
	{
		submit_syntax();
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
		// Once: a destructor's repeat may run after the window has gone.
		if (SyntaxHost && !SyntaxHost->is_closing()) {
			View->composer()->SyntaxWanted = { };
			SyntaxHost->shutdown();
		}
	}

	auto review_cycle_desktop::is_quiescent() const noexcept -> bool
	{
		return (!Host || Host->is_quiescent()) && (!SyntaxHost || SyntaxHost->is_quiescent());
	}
}
