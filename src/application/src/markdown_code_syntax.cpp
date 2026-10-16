#include "bpr/application/markdown_code_syntax.hpp"

#include "bpr/application/syntax_classification_application_host.hpp"
#include "bpr/core/git_objects.hpp"
#include "bpr/core/markdown_code.hpp"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <span>
#include <utility>

namespace bpr::application {
	void markdown_code_texts::hold(const core::git_blob_oid& aBlob, std::shared_ptr<const std::string> aText)
	{
		const auto guard = std::lock_guard{ Mutex };
		if (std::ranges::find(Texts, aBlob, &held::Blob) == Texts.end()) {
			Texts.push_back({ .Blob = aBlob, .Text = std::move(aText) });
		}
	}

	void markdown_code_texts::release(const core::git_blob_oid& aBlob)
	{
		const auto guard = std::lock_guard{ Mutex };
		std::erase_if(Texts, [&aBlob](const held& aHeld) { return aHeld.Blob == aBlob; });
	}

	auto markdown_code_texts::bytes(const core::git_blob_oid& aBlob) const -> std::optional<std::string>
	{
		const auto guard = std::lock_guard{ Mutex };
		const auto found = std::ranges::find(Texts, aBlob, &held::Blob);
		return found == Texts.end() ? std::nullopt : std::optional{ *found->Text };
	}

	struct markdown_code_syntax_host::implementation final : appearance_projection_port
	{
		struct wanted
		{
			token_span_cache_key               Key;
			std::shared_ptr<const std::string> Text;
			/// Every file name a reader asked with: each is answered.
			std::vector<std::string> Selectors;
		};

		implementation(syntax_classification_port& aClassifier, markdown_code_texts& aTexts, effect_executor& aEffects, decision_executor& aDecisions,
					   definition_resolver aDefinitionFor, publisher aPublish, const markdown_code_syntax_limits aLimits)
			: Texts{ aTexts }
			, DefinitionFor{ std::move(aDefinitionFor) }
			, Publish{ std::move(aPublish) }
			, Limits{ aLimits }
			// One classification at a time, so a failure, which names no key,
			// is always the one in flight; the cache is this host's own, so
			// the review panes' entries are never evicted by a comment.
			, Host{ aClassifier, aEffects, aDecisions, *this, core::application_session_id{ 3U },
					appearance_host_limits{ .OutstandingRequests = 1U, .TokenSpanCacheEntries = std::clamp<std::size_t>(aLimits.Cached, 1U, maximum_token_span_cache_entries) } }
		{
		}

		implementation(const implementation&)                    = delete;
		auto operator=(const implementation&) -> implementation& = delete;

		void request(const std::vector<markdown_code_syntax_request>& aRequests)
		{
			for (const auto& request : aRequests) {
				want(request);
			}
			submit();
		}

		void want(const markdown_code_syntax_request& aRequest)
		{
			if (Closing || !aRequest.Text || aRequest.Text->size() > core::markdown_code_classified_bytes) {
				++Refused;
				return;
			}
			const auto definition = DefinitionFor ? DefinitionFor(aRequest.Selector) : std::nullopt;
			const auto bytes      = std::span{ reinterpret_cast<const std::uint8_t*>(aRequest.Text->data()), aRequest.Text->size() };
			const auto blob       = core::git_blob_identity(bytes, core::git_hash_algorithm::sha1);
			if (!definition || !blob) {
				// Plain text is never classified; a text collision detection
				// refuses has no key and stays plain too ([[BPR-1450]]).
				++Refused;
				return;
			}
			const auto key       = token_span_cache_key{ .Blob = *blob, .Definition = *definition };
			const auto add_to    = [&aRequest](wanted& aWanted) {
                if (std::ranges::find(aWanted.Selectors, aRequest.Selector) == aWanted.Selectors.end()) {
					aWanted.Selectors.push_back(aRequest.Selector);
				}
			};
			if (Classifying && Classifying->Key == key) {
				add_to(*Classifying);
				return;
			}
			if (const auto found = std::ranges::find(Waiting, key, &wanted::Key); found != Waiting.end()) {
				add_to(*found);
				return;
			}
			if (Waiting.size() >= Limits.Waiting) {
				// The newest texts are the ones on screen; the oldest gives way.
				Waiting.pop_front();
			}
			Waiting.push_back({ .Key = key, .Text = aRequest.Text, .Selectors = { aRequest.Selector } });
		}

		void submit()
		{
			while (!Closing && !Classifying && !Waiting.empty() && !Submitting) {
				Classifying = std::move(Waiting.back());
				Waiting.pop_back();
				Texts.hold(Classifying->Key.Blob, Classifying->Text);
				// A cached text answers inside this call; its answer must not
				// start the next one from within it.
				Submitting = true;
				Host.classify(Classifying->Key);
				Submitting = false;
			}
		}

		void done(const token_span_publication* aSpans)
		{
			if (!Classifying) {
				return;
			}
			auto finished = std::move(*Classifying);
			Classifying.reset();
			Texts.release(finished.Key.Blob);
			if (!Closing && aSpans != nullptr && aSpans->Key == finished.Key && Publish) {
				for (auto& selector : finished.Selectors) {
					Publish({ .Selector = std::move(selector), .Text = finished.Text, .Spans = aSpans->Spans });
				}
			}
			if (!Submitting) {
				submit();
			}
		}

		void publish(const appearance_projection& aProjection) override
		{
			if (aProjection.Spans) {
				done(&*aProjection.Spans);
			}
		}

		void fail(const appearance_failure_publication& /*aFailure*/) override { done(nullptr); }

		void rejected(const appearance_terminal_rejection /*aRejection*/) override { done(nullptr); }

		markdown_code_texts&                   Texts;
		definition_resolver                    DefinitionFor;
		publisher                              Publish;
		markdown_code_syntax_limits            Limits;
		syntax_classification_application_host Host;
		std::optional<wanted>                  Classifying;
		std::deque<wanted>                     Waiting;
		std::size_t                            Refused         = 0U;
		bool                                   Closing         = false;
		bool                                   Submitting      = false;
	};

	markdown_code_syntax_host::markdown_code_syntax_host(syntax_classification_port& aClassifier, markdown_code_texts& aTexts, effect_executor& aEffects, decision_executor& aDecisions,
														 definition_resolver aDefinitionFor, publisher aPublish, const markdown_code_syntax_limits aLimits)
		: Implementation{ std::make_unique<implementation>(aClassifier, aTexts, aEffects, aDecisions, std::move(aDefinitionFor), std::move(aPublish), aLimits) }
	{
	}

	markdown_code_syntax_host::~markdown_code_syntax_host()
	{
		shutdown();
	}

	void markdown_code_syntax_host::request(const std::vector<markdown_code_syntax_request>& aRequests)
	{
		Implementation->request(aRequests);
	}

	void markdown_code_syntax_host::shutdown()
	{
		if (Implementation->Closing) {
			return;
		}
		Implementation->Closing = true;
		Implementation->Waiting.clear();
		Implementation->Host.shutdown();
	}

	auto markdown_code_syntax_host::is_quiescent() const noexcept -> bool { return Implementation->Host.is_quiescent(); }
	auto markdown_code_syntax_host::classifications() const noexcept -> std::size_t { return Implementation->Host.classification_submissions(); }
	auto markdown_code_syntax_host::cache_hits() const noexcept -> std::size_t { return Implementation->Host.cache_hits(); }
	auto markdown_code_syntax_host::refused() const noexcept -> std::size_t { return Implementation->Refused; }
	auto markdown_code_syntax_host::waiting() const noexcept -> std::size_t { return Implementation->Waiting.size(); }
} // namespace bpr::application
