#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_MARKDOWN_BLOB_SNIPPET_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_MARKDOWN_BLOB_SNIPPET_HOST_HPP

#include "bpr/application/port.hpp"
#include "bpr/core/markdown_blob_snippet.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

/// \file
/// \brief [[BPR-1802]]. The lines a blob line permalink names, read for the
/// readers that show it as GitHub's snippet box.
/// \details A reader asks for the permalinks its body holds alone on a line;
/// this host admits only those in the accepted pull request's repository on
/// its host, reads each file once through `markdown_blob_source_port` (the
/// review's Git cache, never an arbitrary fetch), keeps the bytes in a
/// bounded cache, and publishes the lines with the permalink, so every
/// reader showing it can tell the answer is for a box it holds. It holds no
/// reader, no body and no colour.
namespace bpr::application {
	/// \brief A permalink a reader shows alone on a line.
	struct markdown_blob_snippet_request
	{
		core::markdown_blob_permalink Permalink;
	};

	/// \brief The lines of one permalink, for every reader showing it.
	struct markdown_blob_snippet_publication
	{
		core::markdown_blob_permalink                   Permalink;
		std::shared_ptr<const std::vector<std::string>> Lines;
	};

	/// \brief The accepted pull request permalinks are judged by.
	struct markdown_blob_snippet_scope
	{
		std::string Host;
		std::string Owner;
		std::string Repository;
		/// Changes whenever lines read under one scope must not reach another:
		/// window, session, account, pull request and head.
		std::string Identity;

		auto operator==(const markdown_blob_snippet_scope&) const -> bool = default;
	};

	enum class markdown_blob_source_status : std::uint8_t {
		read,
		/// The commit holds no file at the path.
		absent,
		/// Larger than `core::markdown_blob_snippet_file_bytes`, or not text.
		refused,
		/// The commit or its objects are not on hand; asked again later.
		unavailable,
		cancelled,
	};

	/// \brief One file to read: the permalink's commit and path, under a scope.
	struct markdown_blob_source_read
	{
		core::markdown_blob_permalink Permalink;
		std::string                   Identity;
	};

	struct markdown_blob_source_result
	{
		markdown_blob_source_status Status = markdown_blob_source_status::unavailable;
		std::string                 Bytes;
	};

	/// \brief Reads one file at one commit of the accepted repository.
	class markdown_blob_source_port
	{
	public:
		virtual ~markdown_blob_source_port() = default;
		/// \brief Starts one bounded read; false when it cannot start now.
		/// \details A started read delivers exactly one result, on the
		/// decision lane and never inside this call, also after `aStop`.
		[[nodiscard]] virtual auto read(const markdown_blob_source_read& aRead, std::stop_token aStop, std::function<void(markdown_blob_source_result)> aCompletion) -> bool = 0;
	};

	struct markdown_blob_snippet_limits
	{
		/// Reads in flight at once.
		std::size_t Concurrent = 2U;
		/// Files waiting to be read; the oldest gives way.
		std::size_t Waiting = 32U;
		/// Bytes of files kept, least recently used given up first.
		std::size_t CachedBytes = 4U * 1024U * 1024U;
	};

	/// \brief Admits, reads, caches and publishes permalink snippets, on the
	/// decision lane.
	class markdown_blob_snippet_host final
	{
	public:
		using scope_source = std::function<std::optional<markdown_blob_snippet_scope>()>;
		using publisher    = std::function<void(const markdown_blob_snippet_publication&)>;

		markdown_blob_snippet_host(markdown_blob_source_port& aSource, decision_executor& aDecisions, scope_source aScope, publisher aPublish, markdown_blob_snippet_limits aLimits = { });
		~markdown_blob_snippet_host();
		markdown_blob_snippet_host(const markdown_blob_snippet_host&)                    = delete;
		auto operator=(const markdown_blob_snippet_host&) -> markdown_blob_snippet_host& = delete;

		/// \brief Asks for each permalink's lines. A held file answers through
		/// a posted task, never inside this call; an out-of-scope or
		/// over-bound permalink is refused and nothing is read for it.
		void request(const std::vector<markdown_blob_snippet_request>& aRequests);
		/// \brief Stops every read, forgets every file and refusal.
		void               reset();
		void               shutdown();
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;
		/// \brief Files the source was asked for.
		[[nodiscard]] auto reads() const noexcept -> std::size_t;
		/// \brief Answers served from held files.
		[[nodiscard]] auto cache_hits() const noexcept -> std::size_t;
		/// \brief Permalinks refused: out of scope, over bound, or a file that
		/// gave no lines.
		[[nodiscard]] auto refused() const noexcept -> std::size_t;
		[[nodiscard]] auto waiting() const noexcept -> std::size_t;
		[[nodiscard]] auto cached_bytes() const noexcept -> std::size_t;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
} // namespace bpr::application

#endif
