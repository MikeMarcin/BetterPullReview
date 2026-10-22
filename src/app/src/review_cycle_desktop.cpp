#include "review_cycle_desktop.hpp"

#include "bpr/adapters/git/installed_git_publication.hpp"
#include "bpr/adapters/sqlite/review_cycle_store.hpp"
#include "bpr/adapters/sqlite/review_tour_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/adapters/sqlite/tour_owner_channel.hpp"
#include "bpr/adapters/syntax/packaged_syntax_classifier.hpp"
#include "bpr/adapters/workspace/local_export_destination.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/core/diff_line_matcher.hpp"
#include "bpr/core/git_objects.hpp"
#include "bpr/qt/review_cycle_promote_sheet.hpp"
#include "bpr/qt/review_cycle_publish_sheet.hpp"
#include "bpr/qt/review_cycle_share_sheet.hpp"
#include "bpr/qt/review_cycle_view.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "tour_owner_service.hpp"
// [[BPR-1180]]. The Send to Agent adapter, only with the agent integrations.
#if defined(BPR_AGENT_INTEGRATIONS)
	#include "bpr/adapters/agent/agent_process_launcher.hpp"
	#include "bpr/application/review_cycle_agent_sender.hpp"
#endif

#include <QAction>
#include <QClipboard>
#include <QDateTime>
#include <QFileDialog>
#include <QGuiApplication>
#include <QLineEdit>
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

	/// \brief The code host's projections, handed to the desktop that owns it
	/// (BPR-1161): a finished classification, a failure or a refusal each
	/// end the one text it was classifying.
	class code_syntax_relay final : public application::appearance_projection_port
	{
	public:
		explicit code_syntax_relay(std::function<void(const application::token_span_publication*)> aDone)
			: Done{ std::move(aDone) }
		{
		}

		void publish(const application::appearance_projection& aProjection) override
		{
			if (aProjection.Spans) {
				Done(&*aProjection.Spans);
			}
		}

		void fail(const application::appearance_failure_publication& /*aFailure*/) override
		{
			Done(nullptr);
		}

		void rejected(const application::appearance_terminal_rejection /*aRejection*/) override
		{
			Done(nullptr);
		}

	private:
		std::function<void(const application::token_span_publication*)> Done;
	};

	namespace {
		/// \brief The platform clipboard as Copy to Clipboard sees it
		/// (BPR-1161): plain text, confirmed only when the platform hands the
		/// same text back. Nothing here can tell whether anyone pasted it.
		class platform_clipboard final : public application::review_cycle_clipboard
		{
		public:
			auto write(const std::string_view aText) -> outcome override
			{
				auto* const clipboard = QGuiApplication::clipboard();
				if (clipboard == nullptr) {
					return outcome::unavailable;
				}
				const auto text = QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size()));
				clipboard->setText(text, QClipboard::Clipboard);
				return clipboard->text(QClipboard::Clipboard) == text ? outcome::confirmed : outcome::unconfirmed;
			}
		};

		auto utf8_path(const std::string& aPath) -> std::filesystem::path
		{
			// The path is UTF-8 on every platform, so it is not read in a code page.
			return std::filesystem::path{ std::u8string{ aPath.begin(), aPath.end() } };
		}

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

	/// \brief [[BPR-1273]]. A pull request's Review Tour, as Add Notes to
	/// Review Tour reaches it: the profile's tour list read from its store,
	/// and every read and edit of a draft sent to the profile's tour owner,
	/// which runs it on the window that holds that draft or on the stored
	/// draft. Nothing here reaches GitHub.
	class desktop_tour_port final : public application::review_cycle_tour_port
	{
	public:
		desktop_tour_port(std::filesystem::path aRoot, const core::bpr_profile_id aProfile, application::effect_executor& aEffects)
			: Root{ std::move(aRoot) }
			, Profile{ aProfile }
			, Effects{ aEffects }
		{
		}

		review_cycle_desktop::tour_route Route;

		void targets(targets_reply aReply) override
		{
			try {
				Effects.post([root = Root, profile = Profile, reply = aReply] {
					auto listed = std::expected<std::vector<application::tour_saved_target>, application::review_cycle_tour_failure>{ std::unexpected(application::review_cycle_tour_failure::unanswered) };
					try {
						if (auto read = adapters::sqlite::make_review_tour_store(root, profile)->targets({ }); read) {
							listed = std::move(*read);
						}
					} catch (...) {
					}
					reply(std::move(listed));
				});
			} catch (...) {
				aReply(std::unexpected(application::review_cycle_tour_failure::unanswered));
			}
		}

		void load(const application::tour_saved_target& aTarget, std::string aRequest, reply aReply) override
		{
			auto request           = application::tour_owner_request{ .Operation = aRequest.empty() ? application::tour_owner_operation::load : application::tour_owner_operation::request_status, .Draft = tour_draft_token(aTarget) };
			request.Edit.RequestId = std::move(aRequest);
			send(std::move(request), std::move(aReply));
		}

		void execute(const application::tour_saved_target& aTarget, application::tour_service_request aRequest, reply aReply) override
		{
			send({ .Operation = application::tour_owner_operation::mutate, .Draft = tour_draft_token(aTarget), .Edit = std::move(aRequest) }, std::move(aReply));
		}

	private:
		void send(application::tour_owner_request aRequest, reply aReply) const
		{
			auto bytes = adapters::sqlite::encode_tour_request(std::move(aRequest));
			if (!Route || bytes.empty()) {
				aReply(std::unexpected(application::review_cycle_tour_failure::unanswered));
				return;
			}
			Route(std::move(bytes), [reply = std::move(aReply)](const application::tour_channel_bytes& aAnswer) {
				const auto decoded = adapters::sqlite::decode_tour_response(aAnswer);
				if (!decoded || !decoded->Error.empty() || !decoded->Draft) {
					// A refusal is final; anything else leaves the request unanswered, to be read again.
					const auto error = decoded ? decoded->Error : std::string{ };
					reply(std::unexpected(error == "revision_conflict"     ? application::review_cycle_tour_failure::conflict
											  : error == "command_refused" ? application::review_cycle_tour_failure::refused
																		   : application::review_cycle_tour_failure::unanswered));
					return;
				}
				reply(application::review_cycle_tour_answer{ .Draft = *decoded->Draft, .Receipt = decoded->Receipt });
			});
		}

		std::filesystem::path         Root;
		core::bpr_profile_id          Profile;
		application::effect_executor& Effects;
	};

	review_cycle_desktop::review_cycle_desktop(qt::workbench_window& aWindow, const std::filesystem::path& aProfileRoot, const core::bpr_profile_id aProfile,
											   application::effect_executor& aEffects, application::decision_executor& aDecisions, const review_cycle_syntax aSyntax,
											   application::effect_executor* aCaptures, [[maybe_unused]] std::shared_ptr<const application::claude_process_wrapper_source> aClaudeWrapper,
											   [[maybe_unused]] application::claude_locator                              aLocateClaude,
											   [[maybe_unused]] std::shared_ptr<const application::agent_program_source> aPrograms,
											   [[maybe_unused]] application::agent_program_locator                       aLocatePrograms)
		: Window{ aWindow }
		, Source{ adapters::workspace::make_local_workspace_source() }
		, ProfileRoot{ aProfileRoot }
	{
		Host = std::make_unique<application::review_cycle_workbench>(adapters::sqlite::make_review_cycle_store(aProfileRoot, aProfile), *Source, aEffects, aDecisions, *this, random_seed(),
																	 aCaptures != nullptr ? aCaptures : (OwnCaptures = std::make_unique<worker_executor>()).get(),
																	 core::make_xdiff_line_matcher());
		View = new qt::review_cycle_view{ };
		Window.adopt_local_cycle(View);
		Tours    = std::make_unique<desktop_tour_port>(aProfileRoot, aProfile, aEffects);
		Promoter = std::make_unique<application::review_cycle_promoter>(adapters::sqlite::make_review_cycle_store(aProfileRoot, aProfile), *Tours, aEffects, aDecisions,
																		[this](const std::optional<application::review_cycle_promotion_view>& aView) {
																			if (!Closing) {
																				View->present_promotion(aView);
																			}
																		});
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
			SyntaxHost->observe_work(aSyntax.Insights);
			View->composer()->SyntaxWanted = [this](const qt::suggestion_syntax_request& aRequest) { want_syntax(aRequest); };
			View->SourceSyntaxWanted       = [this](const qt::source_syntax_request& aRequest) { want_source_syntax(aRequest); };
			// [[BPR-1161]]. Share notes' code in the same colours, by its own host.
			CodeTexts      = std::make_unique<suggestion_text_source>();
			CodeClassifier = std::make_unique<adapters::syntax::packaged_syntax_classifier>(*SyntaxDefinitions, *CodeTexts);
			CodeRelay      = std::make_unique<code_syntax_relay>([this](const application::token_span_publication* aSpans) { code_syntax_done(aSpans); });
			CodeHost       = std::make_unique<application::syntax_classification_application_host>(*CodeClassifier, *aSyntax.Effects, aDecisions, *CodeRelay, core::application_session_id{ 2U },
																								   application::appearance_host_limits{ .OutstandingRequests = 1U, .TokenSpanCacheEntries = 4U });
			CodeHost->observe_work(aSyntax.Insights);
			View->share_page()->SyntaxWanted = [this](const qt::source_syntax_request& aRequest) { want_code_syntax(aRequest); };
			// [[BPR-1281]]. The comparison's two sides through the same host.
			View->ComparisonSyntaxWanted = [this](const qt::source_syntax_request& aRequest) { want_code_syntax(aRequest); };
		}
		View->set_dispatch({
			.OpenFile      = [this](const std::string& aPath, const bool aNewTab) { Host->open_file(aPath, aNewTab); },
			.Tab           = [this](const core::document_tab_intent& aIntent) { Host->tab(aIntent); },
			.ObserveLine   = [this](const core::document_tab_id aTab, const std::uint64_t aLine) { Host->observe_line(aTab, aLine); },
			.Comment       = [this](core::begin_cycle_feedback aNote) { Host->comment(std::move(aNote)); },
			.Suggest       = [this](application::review_cycle_suggestion_request aRequest) { Host->suggest(std::move(aRequest)); },
			.Relate        = [this](const core::cycle_feedback_item_ref& aItem, const std::uint64_t aVersion, core::cycle_related_reference aReference) { Host->relate(aItem, aVersion, std::move(aReference)); },
			.Capture       = [this](const bool aDeliberate) { Host->capture_revision(aDeliberate); },
			.CancelCapture = [this] { Host->cancel_capture(); },
			.ChooseRebind =
				[this] {
					// [[BPR-1279]]. A folder for a folder cycle, a file for a file cycle; the host checks it before anything is asked.
					const auto& shown  = Host->projection();
					const auto  file   = shown.Cycle && shown.Cycle->WorkspaceKind == core::workspace_root_kind::file;
					const auto  title  = QStringLiteral("Rebind Folder");
					const auto  chosen = file ? QFileDialog::getOpenFileName(&Window, title) : QFileDialog::getExistingDirectory(&Window, title);
					if (!chosen.isEmpty()) {
						const auto bytes = chosen.toUtf8();
						Host->check_rebind(std::string{ bytes.constData(), static_cast<std::size_t>(bytes.size()) });
					}
				},
			.ConfirmRebind = [this] { Host->confirm_rebind(); },
			.CancelRebind  = [this] { Host->cancel_rebind(); },
			.ListFiles     = [this](const int aDirection, const bool aOpenEdge) { Host->list_files(aDirection, aOpenEdge); },
			.Compare       = [this](const core::cycle_view_request& aRequest) { Host->compare(aRequest); },
			.FullSource    = [this] { Host->full_source(); },
			.Refresh       = [this] { Host->refresh_view(); },
			.MarkReviewed  = [this] { Host->mark_reviewed(); },
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
			.Compose          = [this] { Host->compose(); },
			.Share            = {
				.Back      = [this] { Host->close_compose(); },
				.Edit      = [this](core::cycle_compose_edit aEdit) { Host->edit_composition(std::move(aEdit)); },
				.Share     = [this](const core::cycle_share_route aRoute) { share(aRoute); },
				.Replace   = [this](const bool aReplace) { Host->resolve_replace(aReplace); },
				.ShowCode  = [this](const core::cycle_feedback_item_id aItem) { Host->show_note_code(aItem); },
				.CloseCode = [this] { Host->close_note_code(); },
				.Send      = [this](const core::cycle_send_request& aRequest) { Host->send(aRequest); },
				.StopSend  = [this] { Host->cancel_send(); },
				.AddReply  = [this] { Host->add_send_reply(); },
			},
			.OpenPublication = [this] { Host->open_publication(); },
			.Publish         = {
				.Back              = [this] { Host->close_publication(); },
				.Edit              = [this](application::review_cycle_publication_form aForm) { Host->edit_publication(std::move(aForm)); },
				.Prepare           = [this] { Host->prepare_publication(); },
				.Revise            = [this] { Host->revise_publication(); },
				.Publish           = [this] { Host->publish_publication(); },
				.Recover           = [this] { Host->recover_publication(); },
				.CancelPublication = [this] { Host->cancel_publication(); },
				.SavePatch         = [this] { save_patch(); },
				.Link              = [this](const std::string& aAddress) { Host->link_pull_request(aAddress); },
				.OpenPullRequest   = [this] { open_pull_request(); },
			},
			.OpenPromotion = [this] { open_promotion(); },
			.Promote       = {
				.Back            = [this] { Promoter->close(); },
				.Choose          = [this](const core::cycle_feedback_item_id aItem, const bool aChosen) { Promoter->choose(aItem, aChosen); },
				.Add             = [this] { Promoter->add(); },
				.OpenPullRequest = [this] { open_promotion_pull_request(); },
			},
		});
		// BPR-1161: Copy writes the platform clipboard; Save writes only the
		// file chosen, on the effect lane. No other route is reachable here.
		Clipboard = std::make_unique<platform_clipboard>();
		Host->set_clipboard(Clipboard.get());
#if defined(BPR_AGENT_INTEGRATIONS)
		// [[BPR-1180]]. Send to Agent: the installed profiles through the same
		// supervised launch every other use of them has, one send at a time on
		// its own worker. The child gets only each profile's allowed names.
		// [[BPR-1301]]. A Claude send runs the profile's wrapper when one is
		// saved, exactly as every other Claude launch of the window does, and
		// with none saved the claude found on PATH (owner, 2026-10-07).
		SendLauncher = std::make_unique<adapters::agent::agent_process_launcher>();
		SendClaude   = std::make_unique<application::wrapped_claude_process>(*SendLauncher, std::move(aClaudeWrapper), &adapters::agent::inspect_claude_process_wrapper,
																			 std::move(aLocateClaude));
		// [[BPR-PENDING-AQ1]], RFC-BPR-0092 Q5. A Codex or plain-text harness
		// send runs the program the window's launches run.
		SendProcesses = std::make_unique<application::located_agent_process>(*SendClaude, std::move(aPrograms), &adapters::agent::inspect_claude_process_wrapper,
																			 std::move(aLocatePrograms));
		SendLane      = std::make_unique<worker_executor>();
		Sender        = std::make_shared<application::agent_round_sender>(*SendProcesses, [](const std::string& aName) -> std::optional<std::string> {
			if (!qEnvironmentVariableIsSet(aName.c_str())) {
				return std::nullopt;
			}
			return qEnvironmentVariable(aName.c_str()).toStdString();
		});
		set_send_profiles(core::built_in_agent_profiles());
#endif
		Host->set_destination_factory([](const std::string& aPath) { return adapters::workspace::make_local_export_destination(utf8_path(aPath)); });
		ChooseSave = [this](const std::string& aSuggested, const bool aBundle) {
			// Over the sheet that asked, which is window-modal, when one is open.
			auto* const over   = View->publish_sheet()->isVisible() ? static_cast<QWidget*>(View->publish_sheet())
				: View->share_page()->isVisible()                   ? static_cast<QWidget*>(View->share_page())
																	: static_cast<QWidget*>(&Window);
			const auto  patch  = aSuggested.ends_with(".patch");
			const auto  chosen = QFileDialog::getSaveFileName(over, aBundle ? QStringLiteral("Save as Bundle") : patch ? QStringLiteral("Save Patch")
																													   : QStringLiteral("Save as File"),
															  QString::fromStdString(aSuggested),
															  aBundle     ? QStringLiteral("Review bundles (*.bpr-review.zip)")
																  : patch ? QStringLiteral("Patches (*.patch);;All files (*)")
																		  : QStringLiteral("Markdown (*.md);;All files (*)"));
			const auto  bytes  = chosen.toUtf8();
			return std::string{ bytes.constData(), static_cast<std::size_t>(bytes.size()) };
		};
		// A chosen response file is read on the effect lane, never through a link.
		Host->set_file_reader([](const std::string& aPath, const std::uint64_t aMaximum, std::stop_token aStop) {
			return adapters::workspace::read_chosen_file(utf8_path(aPath), aMaximum, std::move(aStop));
		});
		Window.set_local_cycle_entry_dispatch(
			[this] {
				const auto chosen = QFileDialog::getExistingDirectory(&Window, QStringLiteral("Open Project"));
				if (!chosen.isEmpty()) {
					open_project(chosen.toStdString());
				}
			},
			[this](const core::local_cycle_subject& aSubject) {
				Asked        = true;
				BringForward = true;
				Host->resume(aSubject);
			});
		Host->refresh_recents();
	}

	review_cycle_desktop::~review_cycle_desktop()
	{
		shutdown();
	}

	auto review_cycle_desktop::draft_editor(const core::review_cycle_id aCycle) const -> core::cycle_draft_editor
	{
		const auto selected = Host->selected();
		if (Closing || !selected || selected->Cycle != aCycle || !Host->projection().Share) {
			return core::cycle_draft_editor::settled;
		}
		if (const auto shown = View->share_page()->draft_editor(); shown != core::cycle_draft_editor::settled) {
			return shown;
		}
		return Host->saving_draft() ? core::cycle_draft_editor::unsaved : core::cycle_draft_editor::settled;
	}

	void review_cycle_desktop::save_draft_editor(const core::review_cycle_id aCycle)
	{
		const auto selected = Host->selected();
		if (!Closing && selected && selected->Cycle == aCycle && Host->projection().Share) {
			View->share_page()->save_draft_editor();
		}
	}

	void review_cycle_desktop::external_change(const core::review_cycle_id aCycle)
	{
		if (Closing) {
			return;
		}
		Host->observe_external_change(aCycle);
		Host->refresh_recents();
	}

	void review_cycle_desktop::publish(const application::review_cycle_workbench_projection& aProjection)
	{
		if (Closing) {
			return;
		}
		View->present(aProjection);
		if (aProjection.Cycle) {
			const auto title = QString::fromUtf8(aProjection.Cycle->Title.data(), static_cast<qsizetype>(aProjection.Cycle->Title.size()));
			if (std::exchange(BringForward, false)) {
				Window.show_local_cycle(title);
			} else {
				Window.retitle_local_cycle(title);
			}
		} else if (!aProjection.Message.empty() && Asked) {
			// A refusal before any cycle opened is said on the page it was
			// asked from; one at start, from listing recents, leaves Home in place.
			BringForward = false;
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
		Asked        = true;
		BringForward = true;
		Host->open_project(aLocation);
	}

	void review_cycle_desktop::adopt_appearance(const qt::appearance_profile& aProfile)
	{
		View->adopt_appearance(aProfile);
	}

	void review_cycle_desktop::set_save_chooser(save_chooser aChooser)
	{
		ChooseSave = std::move(aChooser);
	}

	void review_cycle_desktop::prefer_send_profile(const std::optional<core::agent_profile_id> aProfile)
	{
		if (PreferredSendProfile == aProfile) {
			return;
		}
		PreferredSendProfile = aProfile;
		set_send_profiles(SendProfiles);
	}

	void review_cycle_desktop::set_send_profiles(std::vector<core::agent_profile> aProfiles)
	{
		if (!Sender || !SendLane) {
			return;
		}
		SendProfiles = aProfiles;
		if (PreferredSendProfile) {
			std::ranges::stable_partition(aProfiles, [preferred = *PreferredSendProfile](const core::agent_profile& aProfile) { return aProfile.Id == preferred; });
		}
		Host->set_send_environment({
			.Profiles    = std::move(aProfiles),
			.Sender      = Sender,
			.Lane        = SendLane.get(),
			.AccountHost = [this]() -> std::optional<std::string> {
				const auto account = Publishing.Account ? Publishing.Account() : std::nullopt;
				return account ? std::optional{ account->Host } : std::nullopt;
			},
		});
	}

	void review_cycle_desktop::set_publishing(publishing aPublishing)
	{
		Publishing       = std::move(aPublishing);
		auto environment = application::review_cycle_publication_environment{ };
		if (!Publishing.GitExecutable.empty()) {
			// The catalog's isolation root is the profile's own, owner-private,
			// made on the effect lane at first use; it shares nothing with the cache.
			environment.Candidates = [git = Publishing.GitExecutable, system = Publishing.WindowsSystemRoot,
									  isolation = ProfileRoot / "publication-git-v1"](std::stop_token aStop)
				-> application::publication_git_result<std::unique_ptr<application::publication_candidate_port>> {
				if (!adapters::sqlite::create_storage_root_owner_private(isolation)) {
					return std::unexpected(application::publication_git_error{ .Failure = application::publication_git_failure::unavailable, .Detail = "the catalog's private folder could not be made" });
				}
				auto made = adapters::git::make_installed_git_publication({ .GitExecutable = git, .IsolationRoot = isolation, .WindowsSystemRoot = system }, std::move(aStop));
				if (!made) {
					return std::unexpected(application::publication_git_error{ .Failure = application::publication_git_failure::unavailable, .Detail = "installed Git could not be prepared" });
				}
				return std::move(*made);
			};
		}
		environment.Account = [this]() -> std::optional<application::github_publication_account> {
			return !Closing && Publishing.Account ? Publishing.Account() : std::nullopt;
		};
		environment.Now = [] {
			// The commit's times: now, in this computer's offset from UTC.
			const auto now = QDateTime::currentDateTime();
			return core::git_signature{ .Seconds = now.toSecsSinceEpoch(), .OffsetMinutes = now.offsetFromUtc() / 60 };
		};
		Host->set_publication_environment(std::move(environment));
	}

	void review_cycle_desktop::set_publication_account(std::function<std::optional<application::github_publication_account>()> aAccount)
	{
		Publishing.Account = std::move(aAccount);
	}

	void review_cycle_desktop::save_patch()
	{
		const auto& shown = Host->projection().Publication;
		if (!shown) {
			return;
		}
		auto path = ChooseSave ? ChooseSave(shown->PatchName, false) : std::string{ };
		if (!path.empty()) {
			Host->save_publication_patch(std::move(path));
		}
	}

	void review_cycle_desktop::open_pull_request()
	{
		const auto& shown = Host->projection().Publication;
		if (Closing || !shown || !shown->Current || !shown->Current->Journal.Link || Window.home_open_bar() == nullptr || Window.home_open_action() == nullptr) {
			return;
		}
		const auto address = shown->Current->Journal.Link->Url;
		Host->close_publication();
		open_address(address);
	}

	void review_cycle_desktop::open_address(const std::string& aAddress)
	{
		if (Closing || aAddress.empty() || Window.home_open_bar() == nullptr || Window.home_open_action() == nullptr) {
			return;
		}
		// The existing pull request workflow takes it from here: the Home bar
		// opens the address as a pasted link does, on Overview.
		Window.home_open_bar()->setText(QString::fromUtf8(aAddress.data(), static_cast<qsizetype>(aAddress.size())));
		Window.home_open_action()->trigger();
	}

	void review_cycle_desktop::set_tour_route(tour_route aRoute)
	{
		Tours->Route = std::move(aRoute);
	}

	void review_cycle_desktop::open_promotion()
	{
		const auto& shown    = Host->projection();
		const auto  selected = Host->selected();
		if (Closing || !selected || shown.Links.empty()) {
			return;
		}
		Promoter->open(*selected, shown.Links, shown.Notes);
	}

	void review_cycle_desktop::open_promotion_pull_request()
	{
		const auto& shown = Promoter->view();
		if (Closing || !shown) {
			return;
		}
		const auto address = shown->Link.Url;
		Promoter->close();
		open_address(address);
	}

	void review_cycle_desktop::share(const core::cycle_share_route aRoute)
	{
		const auto& shown = Host->projection().Share;
		if (!shown) {
			return;
		}
		if (aRoute != core::cycle_share_route::save_document && aRoute != core::cycle_share_route::save_bundle) {
			Host->share(aRoute);
			return;
		}
		// The reviewer names the file; nothing is written without one.
		const auto bundle = aRoute == core::cycle_share_route::save_bundle;
		auto       path   = ChooseSave ? ChooseSave(bundle ? shown->BundleName : shown->DocumentName, bundle) : std::string{ };
		if (!path.empty()) {
			Host->share(aRoute, std::move(path));
		}
	}

	void review_cycle_desktop::want_syntax(const qt::suggestion_syntax_request& aRequest)
	{
		want(static_cast<std::size_t>(aRequest.Side), aRequest.Path, aRequest.Document);
	}

	void review_cycle_desktop::want_source_syntax(const qt::source_syntax_request& aRequest)
	{
		want(source_slot, aRequest.Path, aRequest.Document);
	}

	void review_cycle_desktop::want(const std::size_t aSlot, const std::string& aPath, const std::string& aDocument)
	{
		if (Closing || !SyntaxHost) {
			return;
		}
		auto       document = std::make_shared<const std::string>(aDocument);
		const auto bytes    = std::span{ reinterpret_cast<const std::uint8_t*>(document->data()), document->size() };
		const auto blob     = core::git_blob_identity(bytes, core::git_hash_algorithm::sha1);
		if (!blob) {
			// [[BPR-1450]]. A text collision detection refuses has no cache
			// key: it stays plain, as without a syntax host.
			Wanted[aSlot].reset();
			return;
		}
		Wanted[aSlot] = wanted_syntax{
			.Key      = { .Blob = *blob, .Definition = SyntaxDefinitions->definition_for_path(aPath) },
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

	void review_cycle_desktop::want_code_syntax(const qt::source_syntax_request& aRequest)
	{
		if (Closing || !CodeHost) {
			return;
		}
		auto       document = std::make_shared<const std::string>(aRequest.Document);
		const auto bytes    = std::span{ reinterpret_cast<const std::uint8_t*>(document->data()), document->size() };
		const auto blob     = core::git_blob_identity(bytes, core::git_hash_algorithm::sha1);
		if (!blob) {
			// [[BPR-1450]]. No cache key for a refused text: it stays plain.
			return;
		}
		auto wanted = wanted_syntax{
			.Key      = { .Blob = *blob, .Definition = SyntaxDefinitions->definition_for_path(aRequest.Path) },
			.Document = std::move(document),
		};
		if ((CodeClassifying && CodeClassifying->Key == wanted.Key) || std::ranges::find(CodeWanted, wanted.Key, &wanted_syntax::Key) != CodeWanted.end()) {
			return;
		}
		// The newest texts are the ones on screen; the oldest waiting gives way.
		constexpr auto waiting = std::size_t{ 16U };
		if (CodeWanted.size() >= waiting) {
			CodeWanted.pop_front();
		}
		CodeWanted.push_back(std::move(wanted));
		submit_code_syntax();
	}

	void review_cycle_desktop::submit_code_syntax()
	{
		if (Closing || !CodeHost || CodeClassifying || CodeWanted.empty()) {
			return;
		}
		CodeClassifying = std::move(CodeWanted.front());
		CodeWanted.pop_front();
		CodeTexts->hold(CodeClassifying->Key.Blob, CodeClassifying->Document);
		CodeHost->classify(CodeClassifying->Key);
	}

	void review_cycle_desktop::code_syntax_done(const application::token_span_publication* aSpans)
	{
		if (CodeClassifying) {
			CodeTexts->release(CodeClassifying->Key.Blob);
			if (!Closing && aSpans != nullptr && aSpans->Key == CodeClassifying->Key) {
				View->share_page()->adopt_code_syntax(*CodeClassifying->Document, aSpans->Spans);
				View->adopt_comparison_syntax(*CodeClassifying->Document, aSpans->Spans);
			}
		}
		CodeClassifying.reset();
		submit_code_syntax();
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
			const auto& source = Wanted[source_slot];
			if (!Closing && source && source->Key == spans.Key) {
				View->adopt_source_syntax(*source->Document, spans.Spans);
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
		Promoter->close();
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
		if (Promoter) {
			Promoter->shutdown();
		}
		// Once: a destructor's repeat may run after the window has gone.
		if (SyntaxHost && !SyntaxHost->is_closing()) {
			View->composer()->SyntaxWanted = { };
			View->SourceSyntaxWanted       = { };
			SyntaxHost->shutdown();
		}
		if (CodeHost && !CodeHost->is_closing()) {
			View->share_page()->SyntaxWanted = { };
			CodeWanted.clear();
			CodeHost->shutdown();
		}
	}

	auto review_cycle_desktop::is_quiescent() const noexcept -> bool
	{
		return (!Host || Host->is_quiescent()) && (!Promoter || Promoter->is_quiescent()) && (!SyntaxHost || SyntaxHost->is_quiescent()) && (!CodeHost || CodeHost->is_quiescent());
	}
}
