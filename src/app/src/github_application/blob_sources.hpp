#ifndef BPR_APP_GITHUB_APPLICATION_BLOB_SOURCES_HPP
#define BPR_APP_GITHUB_APPLICATION_BLOB_SOURCES_HPP
#include "bpr/adapters/syntax/packaged_syntax_classifier.hpp"
#include "bpr/application/diff_runtime_ports.hpp"
#include "bpr/application/markdown_blob_snippet_host.hpp"
#include "bpr/application/markdown_code_syntax.hpp"
#include "bpr/core/diff_transition.hpp"
#include "bpr/core/epoch_model.hpp"

#include <cstddef>
#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

#if defined(BPR_BUILD_TESTING)
	#include <condition_variable>
	#include <map>
#endif

// Blob bytes for syntax classification, Markdown code and permalink
// snippets, and the counted diff blob port.
namespace bpr::app {
	/// \brief Supplies blob bytes to the classifier through the epoch host.
	///
	/// \details The classifier reads no repository. It asks for one blob's
	/// bytes, and the only component allowed to produce them is the epoch
	/// host, whose cache session is lifetime-exclusive - a second reader
	/// opening its own Git cache writer is the failure this indirection
	/// exists to prevent.
	///
	/// `materialize` needs the request context that names the authority its
	/// cache session was opened for, so each submitted blob is bound to the
	/// request that reads it. Registrations are written on the decision lane and
	/// read on the appearance effect executor, which is why they are guarded.
	///
	/// One binding per blob rather than one mutable comparison. The single
	/// retained context was latest-write-wins, so a classification that began
	/// late read whichever comparison had since become current instead of the one
	/// it was submitted for. Binding is insert-if-absent: a blob is
	/// content-addressed, so two comparisons naming it want identical bytes, and
	/// keeping the first binding means no later display can move a read that is
	/// already in flight. BPR-0390.
	///
	/// The retained values are comparisons, never classifications: this class
	/// holds no span, decides no role, and would be useless to anything that
	/// wanted to skip the classifier.
	/// \brief [[BPR-1562]]. Serves the Markdown readers' code texts, held
	/// by the reader's classification host while classified, to its own
	/// packaged classifier. No repository, no file: the texts are in memory.
	class markdown_code_blob_source final : public adapters::syntax::blob_source
	{
	public:
		explicit markdown_code_blob_source(const application::markdown_code_texts& aTexts);

		[[nodiscard]] auto blob_bytes(const core::git_blob_oid& aBlob) -> std::optional<std::string> override;

	private:
		const application::markdown_code_texts& Texts;
	};

	/// \brief [[BPR-1802]]. A permalink's file, read through the review's
	/// own objects by the composition's epoch host.
	class permalink_snippet_source final : public application::markdown_blob_source_port
	{
	public:
		using reader = std::function<bool(const application::markdown_blob_source_read&, std::function<void(application::markdown_blob_source_result)>)>;

		explicit permalink_snippet_source(reader aRead);

		[[nodiscard]] auto read(const application::markdown_blob_source_read&                 aRead, std::stop_token /*aStop*/,
								std::function<void(application::markdown_blob_source_result)> aCompletion) -> bool override;

	private:
		reader Read;
	};

	class composition_blob_source final : public adapters::syntax::blob_source
	{
	public:
		explicit composition_blob_source(application::diff_blob_port& aBlobs);

		/// \brief Binds each of a comparison's blobs to the request that reads it.
		void register_comparison(const core::diff_request_context& aContext);

		/// \brief Releases one blob's binding once its classification is over.
		void release_blob(const core::git_blob_oid& aBlob);

		void clear_comparison();

		[[nodiscard]] auto blob_bytes(const core::git_blob_oid& aBlob)
			-> std::optional<std::string> override;

#if defined(BPR_BUILD_TESTING)
		/// \brief [[BPR-1558]]. Holds every classifier blob read at its start,
		/// as a slow read would, until released or its comparison is left.
		void hold_reads(const bool aHold);

		[[nodiscard]] auto held_reads() const -> std::size_t;
#endif

	private:
		// Two blobs per comparison, so eight comparisons' worth of concurrent
		// classification. Past that a read is refused rather than mis-bound.
		static constexpr auto MaximumRequests = std::size_t{ 16U };

#if defined(BPR_BUILD_TESTING)
		/// \brief Waits out a test hold; false when the comparison was left.
		///
		/// \details The departure stops the wait as it stops a real read. The
		/// callback is registered before the lock is taken and destroyed after
		/// it is released, so it can neither run nor be waited for under it.
		[[nodiscard]] auto wait_while_held() -> bool;

		/// \brief Wakes held reads to look again. The lock orders the wakeup
		/// after a waiter's predicate check, so none is lost.
		void wake_held_reads();

		mutable std::mutex      HoldMutex;
		std::condition_variable HoldChanged;
		bool                    Held    = false;
		std::size_t             Waiting = 0U;
#endif

		[[nodiscard]] auto find_request(const core::git_blob_oid& aBlob)
			-> std::vector<application::diff_blob_request>::iterator;

		[[nodiscard]] auto departure_token() -> std::stop_token;

		application::diff_blob_port&                Blobs;
		std::mutex                                  Mutex;
		std::vector<application::diff_blob_request> Requests;
		// Stopped when the reviewer leaves the comparison. One source per
		// selection rather than one per read: every outstanding read for a
		// departed comparison should end, and they end together.
		std::stop_source Departure;
	};
#if defined(BPR_BUILD_TESTING)
	/// \brief [[BPR-1369]]. Counts the blobs the diff host reads, by blob.
	///
	/// \details Tests-on only: the diff host's blob port forwards every call
	/// to the epoch host unchanged and counts each side it answers, so a
	/// course can say which opens read a comparison from Git or the network
	/// and which did not. A tests-off build hands the epoch host over
	/// directly.
	class counting_diff_blob_port final : public application::diff_blob_port
	{
	public:
		explicit counting_diff_blob_port(application::diff_blob_port& aTarget) noexcept;

		[[nodiscard]] auto materialize(const application::diff_blob_request& aRequest, std::stop_token aStop)
			-> application::diff_blob_observation override;

		[[nodiscard]] auto materialize_both(const application::diff_blob_request& aLeft, const application::diff_blob_request& aRight, std::stop_token aStop)
			-> std::optional<std::pair<application::diff_blob_observation, application::diff_blob_observation>> override;

		[[nodiscard]] auto reads(const core::git_blob_oid& aBlob) const -> std::uint64_t;

	private:
		void count(const core::git_blob_oid& aBlob);

		application::diff_blob_port*                Target = nullptr;
		mutable std::mutex                          Mutex;
		std::map<core::git_blob_oid, std::uint64_t> Reads;
	};
#endif
}
#endif
