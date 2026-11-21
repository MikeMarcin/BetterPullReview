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

	namespace {
		/// \brief [[BPR-2351]]. Whether a block of `aBlock`'s language may
		/// borrow from a file of `aFile`'s: the same definition, or C and C++,
		/// whose identifiers and roles are one family (a `.h` beside a
		/// `cpp` block).
		[[nodiscard]] auto shares_names(const syntax_definition_version& aBlock, const syntax_definition_version& aFile) noexcept -> bool
		{
			const auto family = [](const std::string_view aName) { return aName == "C" || aName == cpp_syntax_definition_name; };
			return aBlock.Name == aFile.Name || (family(aBlock.Name) && family(aFile.Name));
		}
	} // namespace

	struct markdown_code_syntax_host::implementation final : appearance_projection_port
	{
		/// \brief One reader's ask: the file name that chose the definition,
		/// and the reviewed file the block is anchored to, if any.
		struct asker
		{
			std::string Selector;
			std::string Anchor;

			auto operator==(const asker&) const -> bool = default;
		};

		struct wanted
		{
			token_span_cache_key               Key;
			std::shared_ptr<const std::string> Text;
			/// Every reader's ask: each is answered.
			std::vector<asker> Askers;
		};

		/// \brief [[BPR-2351]]. A block's spans with a file's roles borrowed,
		/// by the block's key and the file's: the same block in two files can
		/// differ, and a file classified again is a new entry.
		struct borrowing
		{
			token_span_cache_key Block;
			token_span_cache_key File;
			shared_token_spans   Spans;
		};

		/// \brief [[BPR-2351]]. An anchored ask, kept so a file's context
		/// arriving later can answer it again.
		struct anchored
		{
			std::string                        Anchor;
			std::string                        Selector;
			std::shared_ptr<const std::string> Text;
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
				remember(request);
				want(request);
			}
			submit();
		}

		/// \brief Keeps an anchored ask, newest last, at most `Limits.Waiting`.
		void remember(const markdown_code_syntax_request& aRequest)
		{
			if (Closing || aRequest.Anchor.empty() || !aRequest.Text || !ContextFor) {
				return;
			}
			std::erase_if(Anchored, [&aRequest](const anchored& aKept) { return aKept.Anchor == aRequest.Anchor && aKept.Selector == aRequest.Selector && *aKept.Text == *aRequest.Text; });
			if (Anchored.size() >= std::max<std::size_t>(Limits.Waiting, 1U)) {
				Anchored.pop_front();
			}
			Anchored.push_back({ .Anchor = aRequest.Anchor, .Selector = aRequest.Selector, .Text = aRequest.Text });
		}

		void context_changed(const std::string_view aAnchor)
		{
			auto again = std::vector<markdown_code_syntax_request>{ };
			for (const auto& kept : Anchored) {
				if (kept.Anchor == aAnchor) {
					again.push_back({ .Selector = kept.Selector, .Text = kept.Text, .Anchor = kept.Anchor });
				}
			}
			if (!again.empty()) {
				request(again);
			}
		}

		/// \brief The spans one asker is answered with: the block's own, or
		/// with its file's roles borrowed when that file's context is held.
		[[nodiscard]] auto answer_for(const wanted& aFinished, const asker& aAsker, const shared_token_spans& aOwn) -> shared_token_spans
		{
			if (aAsker.Anchor.empty() || !ContextFor || !aOwn) {
				return aOwn;
			}
			const auto context = ContextFor(aAsker.Anchor);
			if (!context || !context->Roles || !shares_names(aFinished.Key.Definition, context->File.Definition)) {
				return aOwn;
			}
			const auto same = [&](const borrowing& aEntry) { return aEntry.Block == aFinished.Key && aEntry.File == context->File; };
			if (const auto found = std::ranges::find_if(Borrowed, same); found != Borrowed.end()) {
				++BorrowedHits;
				return found->Spans;
			}
			auto spans = std::make_shared<const token_span_list>(core::borrow_code_context_roles(*aFinished.Text, *aOwn, *context->Roles, core::markdown_code_classified_bytes));
			++Borrowings;
			if (Borrowed.size() >= std::max<std::size_t>(Limits.Cached, 1U)) {
				Borrowed.pop_front();
			}
			Borrowed.push_back({ .Block = aFinished.Key, .File = context->File, .Spans = spans });
			return spans;
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
			const auto key      = token_span_cache_key{ .Blob = *blob, .Definition = *definition };
			const auto who      = asker{ .Selector = aRequest.Selector, .Anchor = aRequest.Anchor };
			const auto addAsker = [&who](wanted& aWanted) {
				if (std::ranges::find(aWanted.Askers, who) == aWanted.Askers.end()) {
					aWanted.Askers.push_back(who);
				}
			};
			if (Classifying && Classifying->Key == key) {
				addAsker(*Classifying);
				return;
			}
			if (const auto found = std::ranges::find(Waiting, key, &wanted::Key); found != Waiting.end()) {
				addAsker(*found);
				return;
			}
			if (Waiting.size() >= Limits.Waiting) {
				// The newest texts are the ones on screen; the oldest gives way.
				Waiting.pop_front();
			}
			Waiting.push_back({ .Key = key, .Text = aRequest.Text, .Askers = { who } });
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
				for (const auto& who : finished.Askers) {
					Publish({ .Selector = who.Selector, .Text = finished.Text, .Spans = answer_for(finished, who, aSpans->Spans), .Anchor = who.Anchor });
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
		context_resolver                       ContextFor;
		std::optional<wanted>                  Classifying;
		std::deque<wanted>                     Waiting;
		std::deque<borrowing>                  Borrowed;
		std::deque<anchored>                   Anchored;
		std::size_t                            Refused      = 0U;
		std::size_t                            Borrowings   = 0U;
		std::size_t                            BorrowedHits = 0U;
		bool                                   Closing      = false;
		bool                                   Submitting   = false;
	};

	void markdown_code_syntax_host::observe_work(std::shared_ptr<insights_observer> aObserver) { Implementation->Host.observe_work(std::move(aObserver)); }

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

	void markdown_code_syntax_host::set_context_source(context_resolver aContextFor) { Implementation->ContextFor = std::move(aContextFor); }

	void markdown_code_syntax_host::context_changed(const std::string_view aAnchor)
	{
		if (!Implementation->Closing && !aAnchor.empty()) {
			Implementation->context_changed(aAnchor);
		}
	}

	void markdown_code_syntax_host::shutdown()
	{
		if (Implementation->Closing) {
			return;
		}
		Implementation->Closing = true;
		Implementation->Waiting.clear();
		Implementation->Anchored.clear();
		Implementation->Borrowed.clear();
		Implementation->Host.shutdown();
	}

	auto markdown_code_syntax_host::is_quiescent() const noexcept -> bool { return Implementation->Host.is_quiescent(); }
	auto markdown_code_syntax_host::classifications() const noexcept -> std::size_t { return Implementation->Host.classification_submissions(); }
	auto markdown_code_syntax_host::cache_hits() const noexcept -> std::size_t { return Implementation->Host.cache_hits(); }
	auto markdown_code_syntax_host::refused() const noexcept -> std::size_t { return Implementation->Refused; }
	auto markdown_code_syntax_host::borrowed() const noexcept -> std::size_t { return Implementation->Borrowings; }
	auto markdown_code_syntax_host::borrowed_hits() const noexcept -> std::size_t { return Implementation->BorrowedHits; }
	auto markdown_code_syntax_host::waiting() const noexcept -> std::size_t { return Implementation->Waiting.size(); }
} // namespace bpr::application
