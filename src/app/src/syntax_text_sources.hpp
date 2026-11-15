#ifndef BPR_SRC_APP_SRC_SYNTAX_TEXT_SOURCES_HPP
#define BPR_SRC_APP_SRC_SYNTAX_TEXT_SOURCES_HPP

#include "bpr/adapters/syntax/packaged_syntax_classifier.hpp"
#include "bpr/application/appearance_runtime_ports.hpp"
#include "bpr/core/epoch_model.hpp"

#include <algorithm>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

/// \file
/// \brief In-memory texts handed to a packaged classifier, and a relay for a
/// classification host's answers. Shared by the local Review Cycle and local
/// draft compositions, whose texts exist only in memory.
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

}

#endif
