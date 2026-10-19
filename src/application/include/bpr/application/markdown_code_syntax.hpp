#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_MARKDOWN_CODE_SYNTAX_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_MARKDOWN_CODE_SYNTAX_HPP

#include "bpr/application/appearance_runtime_ports.hpp"
#include "bpr/application/insights_observer.hpp"
#include "bpr/application/port.hpp"
#include "bpr/core/epoch_model.hpp"

#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-1562]]. The fenced code of rendered Markdown bodies, coloured
/// by the review panes' classifier (RFC-BPR-0036 owner amendment 2).
/// \details A reader asks for a block's text with the file name that chooses
/// its language (`core::markdown_code_language_of`); this host classifies it
/// on the effect lane through a `syntax_classification_application_host` of
/// its own, over the same classifier and definitions as the panes, and
/// publishes the spans with the same text and file name, so a reader can tell
/// they are for a block it shows. It holds no reader, no body and no colour.
namespace bpr::application {
	/// \brief A block's text, and the file name that chooses its definition.
	struct markdown_code_syntax_request
	{
		std::string                        Selector;
		std::shared_ptr<const std::string> Text;
	};

	/// \brief The spans of one block's text, for every reader showing it.
	struct markdown_code_syntax_publication
	{
		std::string                        Selector;
		std::shared_ptr<const std::string> Text;
		shared_token_spans                 Spans;
	};

	/// \brief The texts being classified, read by identity on the effect lane.
	/// \details The classifier reads bytes by blob identity, and a block's text
	/// exists only in memory, so it is held here while it is classified.
	/// Written on the decision lane and read on the effect lane, hence the lock.
	class markdown_code_texts final
	{
	public:
		void               hold(const core::git_blob_oid& aBlob, std::shared_ptr<const std::string> aText);
		void               release(const core::git_blob_oid& aBlob);
		[[nodiscard]] auto bytes(const core::git_blob_oid& aBlob) const -> std::optional<std::string>;

	private:
		struct held
		{
			core::git_blob_oid                 Blob;
			std::shared_ptr<const std::string> Text;
		};
		mutable std::mutex Mutex;
		std::vector<held>  Texts;
	};

	/// \brief The bounds of the reader's code classification.
	struct markdown_code_syntax_limits
	{
		/// Texts waiting to be classified; the oldest gives way.
		std::size_t Waiting = 256U;
		/// Classified texts retained, by text and definition.
		std::size_t Cached = 128U;
	};

	/// \brief Classifies the code blocks readers ask about, one at a time,
	/// newest first after the one in flight, each text and definition once.
	class markdown_code_syntax_host final
	{
	public:
		using definition_resolver = std::function<std::optional<syntax_definition_version>(std::string_view)>;
		using publisher           = std::function<void(const markdown_code_syntax_publication&)>;

		/// \param aDefinitionFor The definition a file name selects; none for
		/// plain text, which is never classified.
		markdown_code_syntax_host(syntax_classification_port& aClassifier, markdown_code_texts& aTexts, effect_executor& aEffects, decision_executor& aDecisions,
								  definition_resolver aDefinitionFor, publisher aPublish, markdown_code_syntax_limits aLimits = { });
		~markdown_code_syntax_host();
		markdown_code_syntax_host(const markdown_code_syntax_host&)                    = delete;
		auto operator=(const markdown_code_syntax_host&) -> markdown_code_syntax_host& = delete;

		/// \brief Asks for each block's spans; a cached text answers at once.
		void               request(const std::vector<markdown_code_syntax_request>& aRequests);
		void               shutdown();
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;
		/// \brief Texts the classifier was asked about.
		[[nodiscard]] auto classifications() const noexcept -> std::size_t;
		/// \brief Answers served from the cache.
		[[nodiscard]] auto cache_hits() const noexcept -> std::size_t;
		/// \brief Requests refused: plain, too large, or no identity.
		[[nodiscard]] auto refused() const noexcept -> std::size_t;
		[[nodiscard]] auto waiting() const noexcept -> std::size_t;

		void observe_work(std::shared_ptr<insights_observer> aObserver);

	private:
		struct implementation;
		std::unique_ptr<implementation> Implementation;
	};
} // namespace bpr::application

#endif
