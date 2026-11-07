#include "github_application/blob_sources.hpp"

#include "bpr/core/diff_model.hpp"

#include <algorithm>

namespace bpr::app {
	markdown_code_blob_source::markdown_code_blob_source(const application::markdown_code_texts& aTexts)
		: Texts{ aTexts }
	{
	}

	auto markdown_code_blob_source::blob_bytes(const core::git_blob_oid& aBlob) -> std::optional<std::string>
	{
		return Texts.bytes(aBlob);
	}

	permalink_snippet_source::permalink_snippet_source(reader aRead)
		: Read{ std::move(aRead) }
	{
	}

	auto permalink_snippet_source::read(const application::markdown_blob_source_read&                 aRead, std::stop_token /*aStop*/,
										std::function<void(application::markdown_blob_source_result)> aCompletion) -> bool
	{
		// The epoch job is bounded and takes no outside stop; the host
		// drops a stopped read's answer by its generation.
		return Read && Read(aRead, std::move(aCompletion));
	}

	composition_blob_source::composition_blob_source(application::diff_blob_port& aBlobs)
		: Blobs{ aBlobs }
	{
	}

	void composition_blob_source::register_comparison(const core::diff_request_context& aContext)
	{
		const auto guard = std::lock_guard{ Mutex };
		for (const auto side :
			 { core::diff_side::left, core::diff_side::right }) {
			const auto& blob = side == core::diff_side::left
				? aContext.Key.LeftBlob
				: aContext.Key.RightBlob;
			if (!blob.has_value() || find_request(*blob) != Requests.end()) {
				continue;
			}
			if (Requests.size() >= MaximumRequests) {
				// Refused rather than evicted. Evicting would move a read
				// that is already in flight, which is the defect this
				// replaced; refusing makes the read report unavailable, the
				// classification fail, and a later display resubmit.
				continue;
			}
			Requests.push_back(application::diff_blob_request{
				.Context      = aContext,
				.Side         = side,
				.Blob         = *blob,
				.MaximumBytes = adapters::syntax::maximum_classified_blob_bytes,
			});
		}
	}

	void composition_blob_source::release_blob(const core::git_blob_oid& aBlob)
	{
		const auto guard = std::lock_guard{ Mutex };
		if (const auto known = find_request(aBlob); known != Requests.end()) {
			Requests.erase(known);
		}
	}

	void composition_blob_source::clear_comparison()
	{
		auto departed = std::stop_source{ };
		{
			const auto guard = std::lock_guard{ Mutex };
			Requests.clear();
			// Swapped out under the lock and stopped outside it, so a read
			// waiting on the old token is released without holding the mutex
			// its own completion path may want.
			departed  = std::move(Departure);
			Departure = std::stop_source{ };
		}
		departed.request_stop();
	}

	auto composition_blob_source::blob_bytes(const core::git_blob_oid& aBlob)
		-> std::optional<std::string>
	{
		const auto request = [this, &aBlob] {
			const auto guard = std::lock_guard{ Mutex };
			const auto known = find_request(aBlob);
			return known == Requests.end()
				? std::optional<application::diff_blob_request>{ }
				: std::optional{ *known };
		}();
		if (!request.has_value()) {
			// A blob nothing bound belongs to a comparison the reviewer has
			// left, or to one whose binding was refused at the bound. Side
			// is carried by the binding rather than derived here, so it can
			// no longer be answered from a comparison that arrived later.
			return std::nullopt;
		}
#if defined(BPR_BUILD_TESTING)
		if (!wait_while_held()) {
			// Left while held: the read a departure stops answers nothing.
			return std::nullopt;
		}
#endif
		// A real token, where this used to pass a default-constructed one that
		// could never signal. `clear_comparison` stops it, so a read for a
		// comparison the reviewer has left is abandoned promptly rather than
		// only when its bound elapses. BPR-0390.
		const auto observation = Blobs.materialize(*request, departure_token());
		if (
			observation.Status != application::diff_blob_status::available) {
			// Absent rather than empty. An empty string would classify as
			// a file with no tokens, which is a different and wrong
			// answer from "these bytes were not available".
			return std::nullopt;
		}
		return std::string{
			observation.Bytes.begin(), observation.Bytes.end()
		};
	}
#if defined(BPR_BUILD_TESTING)
	void composition_blob_source::hold_reads(const bool aHold)
	{
		{
			const auto guard = std::lock_guard{ HoldMutex };
			Held             = aHold;
		}
		HoldChanged.notify_all();
	}

	auto composition_blob_source::held_reads() const -> std::size_t
	{
		const auto guard = std::lock_guard{ HoldMutex };
		return Waiting;
	}

	auto composition_blob_source::wait_while_held() -> bool
	{
		const auto departure = departure_token();
		const auto wake      = std::stop_callback{ departure, [this] { wake_held_reads(); } };
		{
			auto lock = std::unique_lock{ HoldMutex };
			++Waiting;
			HoldChanged.wait(lock, [this, &departure] { return !Held || departure.stop_requested(); });
			--Waiting;
		}
		return !departure.stop_requested();
	}

	void composition_blob_source::wake_held_reads()
	{
		{
			const auto guard = std::lock_guard{ HoldMutex };
		}
		HoldChanged.notify_all();
	}
#endif
	auto composition_blob_source::find_request(const core::git_blob_oid& aBlob)
		-> std::vector<application::diff_blob_request>::iterator
	{
		return std::ranges::find_if(
			Requests,
			[&aBlob](const application::diff_blob_request& aRequest) {
				return aRequest.Blob == aBlob;
			});
	}

	auto composition_blob_source::departure_token() -> std::stop_token
	{
		const auto guard = std::lock_guard{ Mutex };
		return Departure.get_token();
	}
#if defined(BPR_BUILD_TESTING)
	counting_diff_blob_port::counting_diff_blob_port(application::diff_blob_port& aTarget) noexcept
		: Target{ &aTarget }
	{
	}

	auto counting_diff_blob_port::materialize(const application::diff_blob_request& aRequest, std::stop_token aStop)
		-> application::diff_blob_observation
	{
		auto observation = Target->materialize(aRequest, std::move(aStop));
		count(aRequest.Blob);
		return observation;
	}

	auto counting_diff_blob_port::materialize_both(const application::diff_blob_request& aLeft, const application::diff_blob_request& aRight, std::stop_token aStop)
		-> std::optional<std::pair<application::diff_blob_observation, application::diff_blob_observation>>
	{
		auto observations = Target->materialize_both(aLeft, aRight, std::move(aStop));
		if (observations.has_value()) {
			count(aLeft.Blob);
			count(aRight.Blob);
		}
		return observations;
	}

	auto counting_diff_blob_port::reads(const core::git_blob_oid& aBlob) const -> std::uint64_t
	{
		const std::scoped_lock lock{ Mutex };
		const auto             found = Reads.find(aBlob);
		return found == Reads.end() ? 0U : found->second;
	}

	void counting_diff_blob_port::count(const core::git_blob_oid& aBlob)
	{
		const std::scoped_lock lock{ Mutex };
		++Reads[aBlob];
	}
#endif
}
