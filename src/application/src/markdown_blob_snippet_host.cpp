#include "bpr/application/markdown_blob_snippet_host.hpp"

#include <algorithm>
#include <deque>
#include <list>
#include <set>
#include <utility>

namespace bpr::application {
	struct markdown_blob_snippet_host::implementation final : std::enable_shared_from_this<implementation>
	{
		/// One file, and every range of it a reader asked for.
		struct wanted
		{
			std::string                                Key;
			core::markdown_blob_permalink              Source;
			std::vector<core::markdown_blob_permalink> Ranges;
		};

		struct reading
		{
			wanted           File;
			std::string      Identity;
			std::stop_source Stop;
			std::uint64_t    Generation = 0U;
		};

		struct held
		{
			std::string                        Key;
			std::shared_ptr<const std::string> Bytes;
		};

		implementation(markdown_blob_source_port& aSource, decision_executor& aDecisions, scope_source aScope, publisher aPublish, const markdown_blob_snippet_limits aLimits)
			: Source{ aSource }
			, Decisions{ aDecisions }
			, Scope{ std::move(aScope) }
			, Publish{ std::move(aPublish) }
			, Limits{ aLimits }
		{
		}

		void request(const std::vector<markdown_blob_snippet_request>& aRequests)
		{
			if (Closing) {
				return;
			}
			const auto scope = Scope ? Scope() : std::nullopt;
			if (!scope) {
				Refused += aRequests.size();
				return;
			}
			if (scope->Identity != Identity) {
				// Lines read for one pull request never answer another.
				clear();
				Identity = scope->Identity;
			}
			// Reads start in document order; past the bound the oldest waiting gives way.
			for (const auto& request : aRequests) {
				want(*scope, request.Permalink);
				pump();
			}
			pump();
		}

		void want(const markdown_blob_snippet_scope& aScope, const core::markdown_blob_permalink& aPermalink)
		{
			if (!core::markdown_blob_permalink_in_repository(aPermalink, aScope.Host, aScope.Owner, aScope.Repository) || !core::markdown_blob_snippet_range_shown(aPermalink)) {
				++Refused;
				return;
			}
			auto key = core::markdown_blob_permalink_source_key(aPermalink);
			if (Failed.contains(key)) {
				++Refused;
				return;
			}
			if (const auto found = std::ranges::find(Held, key, &held::Key); found != Held.end()) {
				// Most recently used last.
				Held.splice(Held.end(), Held, found);
				++CacheHits;
				post_answer(Held.back().Bytes, aPermalink);
				return;
			}
			const auto addRange = [&aPermalink](wanted& aWanted) {
				if (std::ranges::none_of(aWanted.Ranges, [&](const auto& aRange) { return core::markdown_blob_permalink_same_lines(aRange, aPermalink); })) {
					aWanted.Ranges.push_back(aPermalink);
				}
			};
			for (auto& active : Reading) {
				if (active.File.Key == key && active.Generation == Generation) {
					addRange(active.File);
					return;
				}
			}
			if (const auto found = std::ranges::find(Waiting, key, &wanted::Key); found != Waiting.end()) {
				addRange(*found);
				return;
			}
			if (Waiting.size() >= std::max<std::size_t>(Limits.Waiting, 1U)) {
				Waiting.pop_front();
			}
			Waiting.push_back({ .Key = std::move(key), .Source = aPermalink, .Ranges = { aPermalink } });
		}

		void pump()
		{
			while (!Closing && !Waiting.empty() && current_reads() < std::max<std::size_t>(Limits.Concurrent, 1U)) {
				auto file = std::move(Waiting.front());
				Waiting.pop_front();
				Reading.push_back({ .File = std::move(file), .Identity = Identity, .Stop = { }, .Generation = Generation });
				auto&      active  = Reading.back();
				const auto started = Source.read({ .Permalink = active.File.Source, .Identity = Identity }, active.Stop.get_token(),
												 [weak = weak_from_this(), key = active.File.Key, generation = Generation](markdown_blob_source_result aResult) {
													 if (const auto self = weak.lock()) {
														 self->done(key, generation, std::move(aResult));
													 }
												 });
				if (started) {
					++Reads;
				} else {
					// Not on hand yet: nothing is remembered, so a later render asks again.
					Reading.pop_back();
				}
			}
		}

		[[nodiscard]] auto current_reads() const -> std::size_t
		{
			return static_cast<std::size_t>(std::ranges::count(Reading, Generation, &reading::Generation));
		}

		void done(const std::string& aKey, const std::uint64_t aGeneration, markdown_blob_source_result aResult)
		{
			const auto found = std::ranges::find_if(Reading, [&](const reading& aReading) { return aReading.File.Key == aKey && aReading.Generation == aGeneration; });
			if (found == Reading.end()) {
				return;
			}
			auto finished = std::move(*found);
			Reading.erase(found);
			const auto current = !Closing && aGeneration == Generation && finished.Identity == Identity;
			if (current) {
				switch (aResult.Status) {
				case markdown_blob_source_status::read:
					if (aResult.Bytes.size() <= core::markdown_blob_snippet_file_bytes) {
						auto bytes = std::make_shared<const std::string>(std::move(aResult.Bytes));
						hold(aKey, bytes);
						for (const auto& range : finished.File.Ranges) {
							answer(*bytes, range);
						}
					} else {
						Failed.insert(aKey);
						Refused += finished.File.Ranges.size();
					}
					break;
				case markdown_blob_source_status::absent:
				case markdown_blob_source_status::refused:
					Failed.insert(aKey);
					Refused += finished.File.Ranges.size();
					break;
				case markdown_blob_source_status::unavailable:
				case markdown_blob_source_status::cancelled:
					break;
				}
			}
			pump();
		}

		void hold(const std::string& aKey, std::shared_ptr<const std::string> aBytes)
		{
			HeldBytes += aBytes->size();
			Held.push_back({ .Key = aKey, .Bytes = std::move(aBytes) });
			// The newest file stays even when it alone is over the bound.
			while (Held.size() > 1U && HeldBytes > Limits.CachedBytes) {
				HeldBytes -= Held.front().Bytes->size();
				Held.pop_front();
			}
		}

		void answer(const std::string& aBytes, const core::markdown_blob_permalink& aPermalink)
		{
			auto lines = core::markdown_blob_snippet_lines(aBytes, aPermalink);
			if (!lines) {
				++Refused;
				return;
			}
			if (Publish) {
				Publish({ .Permalink = aPermalink, .Lines = std::make_shared<const std::vector<std::string>>(std::move(*lines)) });
			}
		}

		void post_answer(std::shared_ptr<const std::string> aBytes, const core::markdown_blob_permalink& aPermalink)
		{
			Decisions.post([weak = weak_from_this(), bytes = std::move(aBytes), permalink = aPermalink, identity = Identity] {
				const auto self = weak.lock();
				if (self && !self->Closing && self->Identity == identity) {
					self->answer(*bytes, permalink);
				}
			});
		}

		void clear()
		{
			++Generation;
			for (auto& active : Reading) {
				active.Stop.request_stop();
			}
			Waiting.clear();
			Held.clear();
			HeldBytes = 0U;
			Failed.clear();
		}

		markdown_blob_source_port&   Source;
		decision_executor&           Decisions;
		scope_source                 Scope;
		publisher                    Publish;
		markdown_blob_snippet_limits Limits;
		std::string                  Identity;
		std::deque<wanted>           Waiting;
		/// Reads of earlier generations stay until their one result arrives.
		std::list<reading>    Reading;
		std::list<held>       Held;
		std::size_t           HeldBytes = 0U;
		std::set<std::string> Failed;
		std::uint64_t         Generation = 0U;
		std::size_t           Reads      = 0U;
		std::size_t           CacheHits  = 0U;
		std::size_t           Refused    = 0U;
		bool                  Closing    = false;
	};

	markdown_blob_snippet_host::markdown_blob_snippet_host(markdown_blob_source_port& aSource, decision_executor& aDecisions, scope_source aScope, publisher aPublish, const markdown_blob_snippet_limits aLimits)
		: Implementation{ std::make_shared<implementation>(aSource, aDecisions, std::move(aScope), std::move(aPublish), aLimits) }
	{
	}

	markdown_blob_snippet_host::~markdown_blob_snippet_host()
	{
		shutdown();
	}

	void markdown_blob_snippet_host::request(const std::vector<markdown_blob_snippet_request>& aRequests) { Implementation->request(aRequests); }

	void markdown_blob_snippet_host::reset()
	{
		Implementation->clear();
		Implementation->Identity.clear();
	}

	void markdown_blob_snippet_host::shutdown()
	{
		if (Implementation->Closing) {
			return;
		}
		Implementation->clear();
		Implementation->Closing = true;
		// A read still in flight answers into nothing: its completion holds
		// only a weak reference, and finds no reading of its generation.
		Implementation->Reading.clear();
	}

	auto markdown_blob_snippet_host::is_quiescent() const noexcept -> bool { return Implementation->Reading.empty(); }
	auto markdown_blob_snippet_host::reads() const noexcept -> std::size_t { return Implementation->Reads; }
	auto markdown_blob_snippet_host::cache_hits() const noexcept -> std::size_t { return Implementation->CacheHits; }
	auto markdown_blob_snippet_host::refused() const noexcept -> std::size_t { return Implementation->Refused; }
	auto markdown_blob_snippet_host::waiting() const noexcept -> std::size_t { return Implementation->Waiting.size(); }
	auto markdown_blob_snippet_host::cached_bytes() const noexcept -> std::size_t { return Implementation->HeldBytes; }
} // namespace bpr::application
