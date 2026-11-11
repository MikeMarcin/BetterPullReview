#include "bpr/application/agent_finding_card_host.hpp"

#include "bpr/application/agent_launch.hpp"
#include "bpr/core/agent_answer_findings.hpp"
#include "bpr/core/agent_session.hpp"

#include <algorithm>
#include <atomic>
#include <deque>
#include <string>
#include <utility>

namespace bpr::application {
	auto restore_agent_findings(const std::string_view aAnswer, const core::agent_finding_card_memory& aMemory) -> std::optional<restored_agent_findings>
	{
		const auto split = core::split_agent_answer(aAnswer);
		if (split.Block != core::agent_findings_block::complete) {
			return std::nullopt;
		}
		// The live answer's decoder, with a snapshot naming each file the block
		// does and freezing nothing: a finding keeps the side its block named.
		const auto session = core::agent_session{ };
		const auto first   = decode_agent_review_findings(aAnswer, session, { });
		if (!first || first->Findings.empty()) {
			return std::nullopt;
		}
		auto named = core::agent_finding_snapshot{ };
		for (const auto& finding : first->Findings) {
			if (std::ranges::find(named.Files, finding.Path, &core::agent_finding_snapshot_file::Path) == named.Files.end()) {
				named.Files.push_back({ .Path = finding.Path });
			}
		}
		const auto decoded = decode_agent_review_findings(aAnswer, session, named);
		if (!decoded || decoded->Findings.empty()) {
			return std::nullopt;
		}
		auto restored    = restored_agent_findings{ .Findings = core::digest_agent_findings(split.Payload) };
		restored.Session = core::restored_finding_session(restored.Findings);
		auto ordinal     = std::uint32_t{ 0U };
		for (const auto& finding : decoded->Findings) {
			auto card = restored_agent_finding_card{ .Ordinal = ordinal, .Finding = finding, .Kept = aMemory.recall(restored.Findings, ordinal) };
			// Nothing was frozen, so there is no comparison to name.
			if (card.Finding.Anchor.Line) {
				card.Finding.Anchor.Line->Comparison = { };
			}
			restored.Cards.push_back(std::move(card));
			++ordinal;
		}
		return restored;
	}

	struct agent_finding_card_host::implementation final : std::enable_shared_from_this<implementation>
	{
		explicit implementation(agent_finding_card_dependencies aDependencies)
			: Dependencies{ std::move(aDependencies) }
		{ }

		template <typename Task, typename Settle>
		void off_lane(Task aTask, Settle aSettle)
		{
			if (Closing || Dependencies.Effects == nullptr || Dependencies.Decision == nullptr) {
				return;
			}
			++Outstanding;
			try {
				Dependencies.Effects->post([self = shared_from_this(), task = std::move(aTask), settle = std::move(aSettle)]() mutable {
					if (self->Closing) {
						--self->Outstanding;
						return;
					}
					auto result = std::optional<decltype(task())>{ };
					try {
						result.emplace(task());
					} catch (...) { }
					try {
						self->Dependencies.Decision->post([owner = self, reply = std::move(settle), outcome = std::move(result)]() mutable {
							--owner->Outstanding;
							if (!owner->Closing) {
								reply(std::move(outcome));
							}
						});
					} catch (...) {
						--self->Outstanding;
					}
				});
			} catch (...) {
				--Outstanding;
			}
		}

		/// Writes the oldest change waiting, one at a time and in order.
		void write_next()
		{
			if (Writing || Waiting.empty()) {
				return;
			}
			Writing   = true;
			auto next = std::move(Waiting.front());
			Waiting.pop_front();
			off_lane([store = Dependencies.Store, record = std::move(next)] { return store != nullptr && store->store(record); },
					 [self = shared_from_this()](std::optional<bool> /*aStored*/) {
						 self->Writing = false;
						 self->write_next();
					 });
		}

		agent_finding_card_dependencies             Dependencies;
		core::agent_finding_card_memory             Memory;
		std::deque<core::agent_finding_card_record> Waiting;
		bool                                        Writing = false;
		bool                                        Asked   = false;
		bool                                        Loaded  = false;
		std::atomic_bool                            Closing{ false };
		std::atomic_size_t                          Outstanding{ 0U };
	};

	agent_finding_card_host::agent_finding_card_host(agent_finding_card_dependencies aDependencies)
		: Implementation{ std::make_shared<implementation>(std::move(aDependencies)) }
	{ }

	agent_finding_card_host::~agent_finding_card_host() { shutdown(); }

	void agent_finding_card_host::load()
	{
		auto& host = *Implementation;
		if (host.Asked) {
			return;
		}
		host.Asked = true;
		host.off_lane([store = host.Dependencies.Store] { return store != nullptr ? store->load() : std::nullopt; },
					  [self = Implementation](std::optional<std::optional<std::vector<core::agent_finding_card_record>>> aRead) {
						  if (aRead && *aRead) {
							  self->Memory.adopt(**aRead);
						  }
						  self->Loaded = true;
						  if (self->Dependencies.Loaded) {
							  try {
								  self->Dependencies.Loaded();
							  } catch (...) { }
						  }
					  });
	}

	void agent_finding_card_host::remember(core::agent_finding_card_record aRecord)
	{
		auto& host = *Implementation;
		if (host.Closing) {
			return;
		}
		const auto kept = core::normalized(std::move(aRecord));
		if (!host.Memory.remember(kept)) {
			return;
		}
		host.Waiting.push_back(kept);
		host.write_next();
	}

	auto agent_finding_card_host::memory() const noexcept -> const core::agent_finding_card_memory& { return Implementation->Memory; }

	auto agent_finding_card_host::is_loaded() const noexcept -> bool { return Implementation->Loaded; }

	void agent_finding_card_host::shutdown() noexcept { Implementation->Closing = true; }

	auto agent_finding_card_host::is_quiescent() const noexcept -> bool { return Implementation->Outstanding.load() == 0U; }
} // namespace bpr::application
