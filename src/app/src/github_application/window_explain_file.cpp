/// \file
/// \brief RFC-BPR-0117 ([[BPR-2076]]). Explain this file in the window: the
/// press reads the file's change and its text at the head from the review's own
/// objects, then asks the Agent tab's question about that file; the answer's
/// card at the file's header keeps it as a note, continues it in the Agent tab
/// or is dismissed. Nothing launches without the press, and nothing reaches
/// GitHub.

#include "github_application/window_composition.hpp"

#include "bpr/application/diff_line_matcher.hpp"
#include "bpr/core/diff_compute.hpp"
#include "bpr/core/explain_file.hpp"
#include "bpr/qt/markdown_body_view.hpp"

#include <limits>
#include <memory>

namespace bpr::app {
#if defined(BPR_AGENT_INTEGRATIONS)
	namespace {
		[[nodiscard]] auto hex(const core::git_commit_oid& aOid) -> std::string { return std::string{ core::canonical_hex(aOid).view() }; }

		/// The two sides of the file as they arrive.
		struct explain_read final
		{
			std::string                Path;
			std::string                Key;
			std::optional<std::string> Base;
			std::optional<std::string> Head;
			bool                       TooLarge = false;
			bool                       Failed   = false;
			int                        Waiting  = 2;
		};

		/// Not a turn of the Agent tab: the card's Continue only opens the tab.
		constexpr auto no_turn = std::numeric_limits<std::size_t>::max();
	} // namespace

	void github_application::implementation::bind_explain_file()
	{
		if (Window == nullptr || Window->agent_tab() == nullptr || Window->explain_card() == nullptr) {
			return;
		}
		auto* const tab  = Window->agent_tab();
		auto* const card = Window->explain_card();
		Window->set_explain_file_dispatch([this](const QString& aPath) { ask_explain_file(aPath.toStdString()); });
		tab->set_explain_observer([this](const qt::explain_file_progress& aProgress) {
			if (Closing || Window == nullptr) {
				return;
			}
			if (aProgress.Running) {
				ExplainFile.Agent = aProgress.Running->Agent;
			}
			Window->present_explain_file(aProgress);
		});
		card->set_dresser([tab](qt::markdown_body_view& aView) { tab->dress_answer_view(aView); });
		card->set_link_dispatch([tab](const QString& aLink) { tab->open_reference(aLink); });
		card->set_keep_dispatch([this](const QString& aPath, const QString& aAnswer) { keep_explanation(aPath.toStdString(), aAnswer.toStdString()); });
		card->set_continue_dispatch([this](const std::size_t aTurn) { continue_explanation(aTurn); });
		card->set_dismiss_dispatch([this] {
			if (Window != nullptr) {
				Window->dismiss_explain_file();
			}
		});
		card->set_go_to_note([this](const std::uint64_t aNote) {
			if (Window != nullptr) {
				Window->go_to_private_note(aNote);
			}
		});
		Window->set_explain_file_offered(tab->setup_readiness() == qt::agent_setup_readiness::ready);
	}

	void github_application::implementation::refuse_explain_file(const std::string& aPath, const QString& aWords)
	{
		if (Window != nullptr) {
			Window->present_explain_file(qt::explain_file_progress{ .Path = QString::fromStdString(aPath), .Turn = no_turn, .Ending = aWords, .Answered = false });
		}
	}

	/// The press: the file's change in this pull request and its text at the
	/// head, read from the review's own objects; the question follows once
	/// both have arrived. Only the press reaches here.
	void github_application::implementation::ask_explain_file(const std::string& aPath)
	{
		if (Closing || Window == nullptr || EpochHost == nullptr || aPath.empty()) {
			return;
		}
		const auto& snapshot = WindowRead->accepted_snapshot();
		const auto& state    = EpochHost->state();
		if (!snapshot || !state.AcceptedRevision || !LastAcceptedHandoff) {
			refuse_explain_file(aPath, QStringLiteral("Not sent. The pull request has not been read yet."));
			return;
		}
		auto basePath = aPath;
		auto listed   = false;
		for (const auto& file : snapshot->Files) {
			if (file.Path == aPath) {
				listed = true;
				if (file.PreviousPath) {
					basePath = *file.PreviousPath;
				}
			}
		}
		if (!listed) {
			refuse_explain_file(aPath, QStringLiteral("Not sent. %1 is not one of this pull request's files.").arg(QString::fromStdString(aPath)));
			return;
		}
		const auto base       = state.AcceptedRevision->Base;
		const auto head       = state.AcceptedRevision->Head;
		const auto key        = aPath + "@" + hex(base) + ".." + hex(head);
		ExplainFile.Path      = aPath;
		ExplainFile.Key       = key;
		ExplainFile.Material.reset();
		auto       read       = std::make_shared<explain_read>();
		read->Path            = aPath;
		read->Key             = key;
		const auto authority  = workbench_authority(*LastAcceptedHandoff);
		const auto done       = [this, read, base, head] {
			if (read->Waiting != 0 || Closing || ExplainFile.Key != read->Key) {
				return;
			}
			if (read->Failed) {
				refuse_explain_file(read->Path, QStringLiteral("Not sent. This file's code can't be read here."));
				return;
			}
			const auto as_span = [](const std::optional<std::string>& aSide) -> std::optional<std::span<const std::uint8_t>> {
				return aSide ? std::optional{ std::span<const std::uint8_t>{ reinterpret_cast<const std::uint8_t*>(aSide->data()), aSide->size() } } : std::nullopt;
			};
			auto change = std::string{ };
			if (read->TooLarge) {
				change = "File " + read->Path + " is too large to compare here\n";
			} else {
				const auto options = core::diff_options{ };
				const auto left    = as_span(read->Base);
				const auto right   = as_span(read->Head);
				if (const auto mkey = core::returning_comparison(read->Path, left, right, options); mkey && LineMatcher) {
					auto       matching = application::diff_line_matching{ *LineMatcher };
					const auto outcome  = core::compute_material_diff({ .Key = *mkey, .Options = options, .LeftBytes = left, .RightBytes = right, .Limits = core::diff_compute_limits{ } }, matching);
					change              = outcome.Result ? core::render_returning_change(read->Path, *outcome.Result) : "File " + read->Path + " changed and could not be compared\n";
				}
			}
			ExplainFile.Material = core::bound_explain_material(core::explain_material_input{
				.Path       = read->Path,
				.BaseCommit = hex(base),
				.HeadCommit = hex(head),
				.Change     = std::move(change),
				.Head       = read->TooLarge ? std::nullopt : read->Head,
				.TooLarge   = read->TooLarge,
			});
			if (!ExplainFile.Material) {
				refuse_explain_file(read->Path, QStringLiteral("Not sent. This file's path can't be sent."));
				return;
			}
			if (Window == nullptr || Window->agent_tab() == nullptr) {
				return;
			}
			if (const auto refused = Window->agent_tab()->ask_explain_file(QString::fromStdString(read->Path)); !refused.isEmpty()) {
				refuse_explain_file(read->Path, refused);
			}
		};
		for (const auto baseSide : { true, false }) {
			auto       request  = application::permalink_source_request{ .WorkbenchAuthority = authority, .Head = head, .Commit = baseSide ? base : head, .Path = baseSide ? basePath : aPath };
			const auto admitted = EpochHost->resolve_permalink_source(std::move(request), [read, baseSide, done](const application::permalink_source_observation& aRead) {
				using status = application::permalink_source_status;
				if (aRead.Status == status::resolved) {
					(baseSide ? read->Base : read->Head) = aRead.Bytes;
				} else if (aRead.Status == status::too_large) {
					read->TooLarge = true;
				} else {
					read->Failed = true;
				}
				--read->Waiting;
				done();
			});
			if (!admitted) {
				read->Failed = true;
				--read->Waiting;
			}
		}
		done();
	}

	auto github_application::implementation::compose_explain(application::agent_context_observation& aObservation) -> bool
	{
		if (!ExplainFile.Material || !ChatQuestion || ChatQuestion->File != ExplainFile.Material->Path) {
			Window->agent_panel()->refuse("Explain this file: the file's code was not read. Nothing was sent.");
			return false;
		}
		aObservation.Explain = ExplainFile.Material;
		// One call: its context is the request, with no earlier conversation.
		if (aObservation.Brief) {
			aObservation.Brief->Conversation.clear();
			aObservation.Brief->ConversationOmitted = 0U;
		}
		return true;
	}

	/// Keep as note: a private note on the file, not a line, whose first line
	/// says it is the agent's explanation.
	void github_application::implementation::keep_explanation(const std::string& aPath, const std::string& aAnswer)
	{
		auto* const card   = Window != nullptr ? Window->explain_card() : nullptr;
		const auto  answer = [card](const bool aKept, const QString& aWhy, const std::uint64_t aNote = 0U) {
            if (card != nullptr) {
                card->present_note(aKept, aWhy, aNote);
            }
		};
		if (Closing || Window == nullptr || !ReviewLedgerHost || !ReviewLedgerAvailable) {
			answer(false, QStringLiteral("no review is open"));
			return;
		}
		const auto text = core::explain_note_text(qt::agent_short_name(ExplainFile.Agent).toStdString(), aAnswer, core::maximum_note_bytes);
		if (text.empty()) {
			answer(false, QStringLiteral("there is no answer to keep"));
			return;
		}
		const auto attachment = current_note_attachment();
		if (!attachment || !SelectedDiffPublication || SelectedDiffPublication->Request.File.PresentationPath != aPath) {
			answer(false, QStringLiteral("the file is not open"));
			return;
		}
		auto note       = application::local_note_record{ };
		note.Id         = next_private_note_id();
		note.Note.Text  = text;
		note.Attachment = *attachment;
		if (aPath.size() <= application::maximum_note_path_bytes) {
			note.FilePath = aPath;
		}
		ReviewLedgerHost->persist(note, std::nullopt, ledger_answer([this, answer](const std::expected<application::local_note_record, application::local_note_failure>& aStored) {
									  if (!aStored) {
										  answer(false, QString::fromUtf8(application::name(aStored.error().Reason).data(), static_cast<qsizetype>(application::name(aStored.error().Reason).size())));
										  return;
									  }
									  refresh_agent_private_context_choices();
									  answer(true, { }, aStored->Id.value());
								  }));
	}

	/// Continue in Agent tab: the tab opens at the turn, and the next question
	/// there opens with this one and its answer (Ask more, RFC-BPR-0101 as
	/// amended 2026-11-12), so that turn resumes the tab's own session with it.
	void github_application::implementation::continue_explanation(const std::size_t aTurn)
	{
		if (Closing || Window == nullptr || Window->agent_tab() == nullptr) {
			return;
		}
		auto* const tab = Window->agent_tab();
		Window->open_agent_question();
		if (aTurn == no_turn || tab->turn_explain(aTurn).isEmpty()) {
			return;
		}
		tab->reveal_turn(aTurn);
		if (const auto answer = tab->turn_answer(aTurn); !answer.isEmpty()) {
			AskMoreSeed = std::pair{ tab->turn_question(aTurn).toStdString(), answer.toStdString() };
		}
	}
#endif
} // namespace bpr::app
