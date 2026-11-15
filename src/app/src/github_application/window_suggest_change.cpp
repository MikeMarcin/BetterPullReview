/// \file
/// \brief RFC-BPR-0118 ([[BPR-2078]]). Suggest the change in the window: the
/// drawer draft's press bounds its material (the draft's lines at the head,
/// its words and the lines around them) and asks it as an Agent tab turn of
/// its own, one fresh tool-free call briefed by `review.suggest-change`; the
/// turn's answer goes to the window, whose core decides whether it opens in
/// the code. Nothing launches without the press, and nothing reaches GitHub.

#include "github_application/window_composition.hpp"

#include "bpr/core/suggest_change.hpp"

namespace bpr::app {
#if defined(BPR_AGENT_INTEGRATIONS)
	void github_application::implementation::bind_suggest_change()
	{
		if (Window == nullptr || Window->agent_tab() == nullptr) {
			return;
		}
		auto* const tab = Window->agent_tab();
		Window->set_suggest_change_dispatch([this](const qt::workbench_window::suggest_change_press& aPress) { ask_suggest_change(aPress); },
											[this] {
												if (!Closing && AgentHost != nullptr) {
													AgentHost->cancel();
													if (auto* const runtime = agent_runtime(); runtime != nullptr) {
														runtime->cancel();
													}
												}
											});
		tab->set_suggest_change_observer([this](const qt::agent_tab_view::suggest_change_turn& aTurn) {
			if (Closing || Window == nullptr || !aTurn.Ended) {
				return;
			}
			SuggestChangeMaterial.reset();
			if (!aTurn.Answered) {
				Window->refuse_suggest_change(aTurn.Ending.isEmpty() ? QStringLiteral("The agent did not answer. Your comment is unchanged.") : aTurn.Ending);
				return;
			}
			static_cast<void>(Window->offer_suggested_change(aTurn.AnswerText.toStdString()));
		});
		Window->set_suggest_change_ready(tab->setup_readiness() == qt::agent_setup_readiness::ready);
	}

	void github_application::implementation::ask_suggest_change(const qt::workbench_window::suggest_change_press& aPress)
	{
		if (Closing || Window == nullptr || Window->agent_tab() == nullptr || EpochHost == nullptr) {
			return;
		}
		const auto& state = EpochHost->state();
		if (!state.AcceptedRevision || !SelectedDiffPublication) {
			Window->refuse_suggest_change(QStringLiteral("Not sent. The pull request has not been read yet."));
			return;
		}
		const auto path       = SelectedDiffPublication->Request.File.PresentationPath;
		SuggestChangeMaterial = core::bound_suggest_change_material(core::suggest_change_material_input{
			.Path       = path,
			.HeadCommit = std::string{ core::canonical_hex(state.AcceptedRevision->Head).view() },
			.Comment    = aPress.Prose,
			.Head       = aPress.Head,
			.First      = aPress.First,
			.Last       = aPress.Last,
		});
		if (!SuggestChangeMaterial) {
			Window->refuse_suggest_change(QStringLiteral("Not sent. These lines are too long to send."));
			return;
		}
		if (const auto refused = Window->agent_tab()->ask_suggest_change(QString::fromStdString(path), aPress.First, aPress.Last); !refused.isEmpty()) {
			SuggestChangeMaterial.reset();
			Window->refuse_suggest_change(refused);
		}
	}

	auto github_application::implementation::compose_suggest_change(application::agent_context_observation& aObservation) -> bool
	{
		if (!SuggestChangeMaterial) {
			Window->agent_panel()->refuse("Suggest the change: the draft's lines were not read. Nothing was sent.");
			return false;
		}
		aObservation.SuggestChange = SuggestChangeMaterial;
		// One call: its context is the request, with no earlier conversation.
		aObservation.Brief.reset();
		return true;
	}
#endif
} // namespace bpr::app
