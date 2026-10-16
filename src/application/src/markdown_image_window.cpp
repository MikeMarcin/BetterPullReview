#include "bpr/application/markdown_image_window.hpp"

#include <algorithm>
#include <list>
#include <map>
#include <utility>

namespace bpr::application {
	struct markdown_image_window::state : std::enable_shared_from_this<state>
	{
		struct entry
		{
			std::uint64_t                Id = 0U;
			std::string                  Body;
			std::uint32_t                Ordinal = 0U;
			core::markdown_image_source  Source;
			std::string                  Node;
			std::optional<std::uint64_t> Ticket;
		};

		state(decision_executor& aDecision, markdown_image_service* aService, markdown_image_resolution_port* aResolution,
			  facts_source aFacts, node_source aNodes, publication aPublish)
			: Decision{ aDecision }
			, Service{ aService }
			, Resolution{ aResolution }
			, Facts{ std::move(aFacts) }
			, Nodes{ std::move(aNodes) }
			, Publish{ std::move(aPublish) }
		{
		}

		decision_executor&                                         Decision;
		markdown_image_service*                                    Service;
		markdown_image_resolution_port*                            Resolution;
		facts_source                                               Facts;
		node_source                                                Nodes;
		publication                                                Publish;
		std::list<entry>                                           Entries;
		std::vector<std::string>                                   Unresolved;
		std::map<std::uint64_t, markdown_image_resolution_request> InFlight;
		std::string                                                Scope;
		std::uint64_t                                              NextId       = 1U;
		std::uint64_t                                              NextSequence = 1U;
		std::size_t                                                Outstanding  = 0U;
		bool                                                       FlushPosted  = false;
		bool                                                       Closed       = false;

		auto current_facts() const -> markdown_image_window_facts
		{
			try {
				return Facts ? Facts() : markdown_image_window_facts{ };
			} catch (...) {
				return { };
			}
		}

		auto projection(const markdown_image_window_facts& aFacts, std::string aBody, const std::uint32_t aOrdinal, std::string aSource,
						const core::markdown_image_state aState, std::shared_ptr<const markdown_decoded_image> aImage = nullptr) const
			-> markdown_image_projection
		{
			return { .Context = aFacts.Context.value_or(core::markdown_context{ }), .Body = std::move(aBody), .Ordinal = aOrdinal, .Source = std::move(aSource), .State = aState, .Image = std::move(aImage) };
		}

		void publish(std::vector<markdown_image_projection> aImages) const
		{
			if (aImages.empty() || !Publish || Closed) {
				return;
			}
			try {
				Publish(std::move(aImages));
			} catch (...) {
				// Presentation failures belong to the projection owner; an image
				// result is never retried through this lane.
			}
		}

		void cancel_entry(const entry& aEntry) const
		{
			if (aEntry.Ticket && Service != nullptr) {
				Service->cancel(*aEntry.Ticket);
			}
		}

		void reset()
		{
			for (const auto& item : Entries) {
				cancel_entry(item);
			}
			Entries.clear();
			Unresolved.clear();
			for (const auto& [sequence, request] : InFlight) {
				if (Resolution != nullptr) {
					try {
						Resolution->cancel_resolution(request);
					} catch (...) {
					}
				}
			}
			InFlight.clear();
		}

		/// A body's new request keeps what it still asks for in flight and
		/// cancels the rest; it returns the references already waiting.
		auto supersede_body(const core::markdown_image_intent& aIntent) -> std::vector<std::uint32_t>
		{
			std::vector<std::uint32_t> kept;
			for (auto it = Entries.begin(); it != Entries.end();) {
				const auto still = it->Body == aIntent.Body
					&& std::ranges::any_of(aIntent.References, [&it](const core::markdown_image_reference& aReference) {
						   return aReference.Ordinal == it->Ordinal && aReference.Source == it->Source.Written;
					   });
				if (it->Body == aIntent.Body && !still) {
					cancel_entry(*it);
					it = Entries.erase(it);
					continue;
				}
				if (still) {
					kept.push_back(it->Ordinal);
				}
				++it;
			}
			return kept;
		}

		void request(const core::markdown_image_intent& aIntent)
		{
			if (Closed) {
				return;
			}
			const auto facts = current_facts();
			if (facts.Scope != Scope) {
				reset();
				Scope = facts.Scope;
			}
			const auto                             node     = Nodes ? Nodes(aIntent.Body) : std::nullopt;
			const auto                             capable  = facts.Available && Service != nullptr && Service->available() && Resolution != nullptr && facts.Resolution.has_value();
			const auto                             admitted = core::admit_markdown_images(aIntent, { .Context = facts.Context, .BodyAvailable = node.has_value(), .Available = capable, .Hostname = facts.Policy.Hostname });
			std::vector<markdown_image_projection> immediate;
			if (!admitted) {
				if (admitted.error() == core::markdown_image_refusal::unavailable) {
					for (const auto& reference : aIntent.References) {
						if (immediate.size() >= core::markdown_image_references_per_body) {
							break;
						}
						immediate.push_back(projection(facts, aIntent.Body, reference.Ordinal, reference.Source, core::markdown_image_state::blocked));
					}
				}
				publish(std::move(immediate));
				return;
			}
			const auto waiting = supersede_body(aIntent);
			for (const auto& admission : *admitted) {
				if (std::ranges::find(waiting, admission.Ordinal) != waiting.end()) {
					continue;
				}
				const auto written = std::ranges::find(aIntent.References, admission.Ordinal, &core::markdown_image_reference::Ordinal)->Source;
				if (!admission.Source) {
					immediate.push_back(projection(facts, aIntent.Body, admission.Ordinal, written, admission.Source.error()));
					continue;
				}
				if (admission.Source->Kind != core::markdown_image_source_kind::repository_relative) {
					if (auto image = Service->cached(core::markdown_image_cache_key(facts.Policy.Hostname, facts.Account, *admission.Source))) {
						immediate.push_back(projection(facts, aIntent.Body, admission.Ordinal, written, core::markdown_image_state::loaded, std::move(image)));
						continue;
					}
				}
				if (node->empty()) {
					immediate.push_back(projection(facts, aIntent.Body, admission.Ordinal, written, core::markdown_image_state::blocked));
					continue;
				}
				if (Entries.size() >= core::markdown_image_window_queue) {
					immediate.push_back(projection(facts, aIntent.Body, admission.Ordinal, written, core::markdown_image_state::over_limit));
					continue;
				}
				Entries.push_back({ .Id = NextId++, .Body = aIntent.Body, .Ordinal = admission.Ordinal, .Source = *admission.Source, .Node = *node, .Ticket = std::nullopt });
				if (std::ranges::find(Unresolved, *node) == Unresolved.end()) {
					Unresolved.push_back(*node);
				}
			}
			publish(std::move(immediate));
			if (!Unresolved.empty() && !FlushPosted) {
				FlushPosted = true;
				auto weak   = weak_from_this();
				Decision.post([weak] {
					if (const auto self = weak.lock()) {
						self->flush();
					}
				});
			}
		}

		/// Resolves every body asked for in this turn, in batches.
		void flush()
		{
			FlushPosted = false;
			if (Closed || Unresolved.empty()) {
				return;
			}
			const auto facts = current_facts();
			if (facts.Scope != Scope || !facts.Resolution || Resolution == nullptr) {
				reset();
				return;
			}
			while (!Unresolved.empty()) {
				const auto count   = std::min(Unresolved.size(), core::markdown_image_bodies_per_resolution);
				auto       request = *facts.Resolution;
				request.Nodes.assign(Unresolved.begin(), Unresolved.begin() + static_cast<std::ptrdiff_t>(count));
				Unresolved.erase(Unresolved.begin(), Unresolved.begin() + static_cast<std::ptrdiff_t>(count));
				request.Sequence = NextSequence++;
				// Each resolution names its own request within the accepted read.
				request.Context.Request = core::request_id{ request.Sequence };
				InFlight.emplace(request.Sequence, request);
				++Outstanding;
				auto  weak     = weak_from_this();
				auto* lane     = &Decision;
				auto  sequence = request.Sequence;
				auto  asked    = request.Nodes;
				try {
					Resolution->resolve(std::move(request), [weak, lane, sequence](markdown_image_resolution_completion aCompletion) {
						lane->post([weak, sequence, completion = std::move(aCompletion)]() mutable {
							if (const auto self = weak.lock()) {
								self->resolved(sequence, std::move(completion));
							}
						});
					});
				} catch (...) {
					--Outstanding;
					InFlight.erase(sequence);
					fail_nodes(facts, asked, core::markdown_image_state::failed);
				}
			}
		}

		void fail_nodes(const markdown_image_window_facts& aFacts, const std::vector<std::string>& aNodes, const core::markdown_image_state aState)
		{
			std::vector<markdown_image_projection> results;
			for (auto it = Entries.begin(); it != Entries.end();) {
				if (!it->Ticket && std::ranges::find(aNodes, it->Node) != aNodes.end()) {
					results.push_back(projection(aFacts, it->Body, it->Ordinal, it->Source.Written, aState));
					it = Entries.erase(it);
				} else {
					++it;
				}
			}
			publish(std::move(results));
		}

		void resolved(const std::uint64_t aSequence, markdown_image_resolution_completion aCompletion)
		{
			--Outstanding;
			const auto found = InFlight.find(aSequence);
			if (found == InFlight.end() || Closed) {
				return;
			}
			const auto nodes = std::move(found->second.Nodes);
			InFlight.erase(found);
			const auto facts = current_facts();
			if (facts.Scope != Scope) {
				return;
			}
			const auto* success = std::get_if<markdown_image_resolution_success>(&aCompletion);
			if (success == nullptr) {
				fail_nodes(facts, nodes, core::markdown_image_state::failed);
				return;
			}
			std::vector<markdown_image_projection> results;
			for (auto it = Entries.begin(); it != Entries.end();) {
				if (it->Ticket || std::ranges::find(nodes, it->Node) == nodes.end()) {
					++it;
					continue;
				}
				const auto body   = std::ranges::find(success->Bodies, it->Node, &markdown_image_resolved_body::Node);
				const auto served = body == success->Bodies.end() ? std::nullopt : core::match_served_markdown_image(it->Source, body->Served);
				if (!served) {
					results.push_back(projection(facts, it->Body, it->Ordinal, it->Source.Written, core::markdown_image_state::blocked));
					it = Entries.erase(it);
					continue;
				}
				const auto key = core::markdown_image_cache_key(facts.Policy.Hostname, facts.Account, core::resolved_markdown_image_source(it->Source, *served));
				if (auto image = Service->cached(key)) {
					results.push_back(projection(facts, it->Body, it->Ordinal, it->Source.Written, core::markdown_image_state::loaded, std::move(image)));
					it = Entries.erase(it);
					continue;
				}
				const auto hop = core::admit_markdown_image_fetch(*served, facts.Policy);
				if (!hop) {
					results.push_back(projection(facts, it->Body, it->Ordinal, it->Source.Written, hop.error()));
					it = Entries.erase(it);
					continue;
				}
				auto       weak  = weak_from_this();
				const auto id    = it->Id;
				auto       fetch = facts.Fetch;
				fetch.First      = *hop;
				fetch.Policy     = facts.Policy;
				it->Ticket       = Service->load({ .Key = key, .Fetch = std::move(fetch) },
												 [weak, id](const core::markdown_image_state aState, std::shared_ptr<const markdown_decoded_image> aImage) {
											   if (const auto self = weak.lock()) {
												   self->loaded(id, aState, std::move(aImage));
											   }
												 });
				++it;
			}
			publish(std::move(results));
		}

		void loaded(const std::uint64_t aId, const core::markdown_image_state aState, std::shared_ptr<const markdown_decoded_image> aImage)
		{
			const auto found = std::ranges::find(Entries, aId, &entry::Id);
			if (found == Entries.end() || Closed) {
				return;
			}
			auto item = std::move(*found);
			Entries.erase(found);
			const auto facts = current_facts();
			if (facts.Scope != Scope) {
				return;
			}
			std::vector<markdown_image_projection> results;
			results.push_back(projection(facts, std::move(item.Body), item.Ordinal, std::move(item.Source.Written), aState, aState == core::markdown_image_state::loaded ? std::move(aImage) : nullptr));
			publish(std::move(results));
		}
	};

	markdown_image_window::markdown_image_window(decision_executor& aDecision, markdown_image_service* aService,
												 markdown_image_resolution_port* aResolution, facts_source aFacts, node_source aNodes, publication aPublish)
		: State{ std::make_shared<state>(aDecision, aService, aResolution, std::move(aFacts), std::move(aNodes), std::move(aPublish)) }
	{
	}

	markdown_image_window::~markdown_image_window()
	{
		shutdown();
	}

	void markdown_image_window::request(const core::markdown_image_intent& aIntent)
	{
		State->request(aIntent);
	}

	void markdown_image_window::reset()
	{
		State->reset();
	}

	void markdown_image_window::shutdown()
	{
		State->reset();
		State->Closed = true;
	}

	auto markdown_image_window::quiescent() const noexcept -> bool
	{
		return State->Outstanding == 0U;
	}

	auto markdown_image_window::pending() const noexcept -> std::size_t
	{
		return State->Entries.size();
	}
} // namespace bpr::application
