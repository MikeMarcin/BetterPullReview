#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_FINDING_CARD_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_FINDING_CARD_HOST_HPP

#include "bpr/application/port.hpp"
#include "bpr/core/agent_finding.hpp"
#include "bpr/core/agent_finding_card_memory.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

/// \file
/// \brief Owner report 2026-11-10, after BPR-2052: the finding cards of a
/// stored review answer, made again when its thread is read back after a
/// restart, each as the reviewer left it.
namespace bpr::application {
	/// \brief One card of a stored answer: its finding, its place in the
	/// block, and what was done with it.
	struct restored_agent_finding_card
	{
		std::uint32_t                   Ordinal = 0U;
		core::agent_finding             Finding;
		core::agent_finding_card_record Kept;

		auto operator<=>(const restored_agent_finding_card&) const = default;
	};

	/// \brief The cards of one stored answer.
	struct restored_agent_findings
	{
		core::agent_findings_digest Findings{ };
		/// `core::restored_finding_session(Findings)`: what each card's
		/// requests and reported states carry.
		std::uint64_t                            Session = 0U;
		std::vector<restored_agent_finding_card> Cards;

		auto operator<=>(const restored_agent_findings&) const = default;
	};

	/// \brief The cards of a stored answer whose findings block is complete,
	/// read by the same decoder a live answer's block goes through, each with
	/// the disposition \p aMemory keeps for it.
	/// \details No snapshot is frozen for a stored answer, so no finding is
	/// anchored to a comparison; each keeps the side its block named, which
	/// is what a card's actions place a draft or a note on.
	/// \returns None when the answer holds no complete block or it names no
	/// finding. An agent integration: agent-free graphs do not compile it.
	[[nodiscard]] auto restore_agent_findings(std::string_view aAnswer, const core::agent_finding_card_memory& aMemory) -> std::optional<restored_agent_findings>;

	/// \brief Where a profile keeps its finding cards' dispositions, so they
	/// survive the application.
	class agent_finding_card_store_port
	{
	public:
		virtual ~agent_finding_card_store_port() = default;
		/// \brief Every kept record, least recently changed first; nothing when
		/// the store cannot be read.
		[[nodiscard]] virtual auto load() -> std::optional<std::vector<core::agent_finding_card_record>> = 0;
		/// \brief Keeps one record, or forgets the card when it is open.
		[[nodiscard]] virtual auto store(const core::agent_finding_card_record& aRecord) -> bool = 0;
	};

	struct agent_finding_card_dependencies
	{
		agent_finding_card_store_port* Store    = nullptr;
		effect_executor*               Effects  = nullptr;
		decision_executor*             Decision = nullptr;
		/// The kept records were read (or could not be): what is shown may
		/// take them.
		std::function<void()> Loaded;
	};

	/// \brief Keeps the dispositions of stored answers' finding cards.
	/// \details Lives on the decision lane; the store runs on the effect lane
	/// one write at a time, in the order the cards changed. What the core's
	/// memory says changed is written; nothing else is.
	class agent_finding_card_host final
	{
	public:
		explicit agent_finding_card_host(agent_finding_card_dependencies aDependencies);
		~agent_finding_card_host();
		agent_finding_card_host(const agent_finding_card_host&)                    = delete;
		auto operator=(const agent_finding_card_host&) -> agent_finding_card_host& = delete;

		/// \brief Reads the kept records once.
		void load();
		/// \brief One card's disposition changed.
		void               remember(core::agent_finding_card_record aRecord);
		[[nodiscard]] auto memory() const noexcept -> const core::agent_finding_card_memory&;
		[[nodiscard]] auto is_loaded() const noexcept -> bool;
		void               shutdown() noexcept;
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_FINDING_CARD_HOST_HPP
