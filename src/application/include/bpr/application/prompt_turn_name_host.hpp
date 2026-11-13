#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PROMPT_TURN_NAME_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PROMPT_TURN_NAME_HOST_HPP

#include "bpr/application/port.hpp"
#include "bpr/core/prompt_macro.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <vector>

/// \file
/// \brief RFC-BPR-0125 milestone 2 (AC07). The name a suggested prompt's turn
/// was sent under, kept so the thread read back after a restart shows it.
namespace bpr::application {
	/// \brief Where a profile keeps its turn names.
	class prompt_turn_name_store_port
	{
	public:
		virtual ~prompt_turn_name_store_port() = default;
		/// \brief Every kept name, least recently sent first; nothing when the
		/// store cannot be read.
		[[nodiscard]] virtual auto load() -> std::optional<std::vector<core::prompt_turn_name>> = 0;
		/// \brief Keeps one name, as the newest, within the bound.
		[[nodiscard]] virtual auto store(const core::prompt_turn_name& aName) -> bool = 0;
	};

	struct prompt_turn_name_dependencies
	{
		prompt_turn_name_store_port* Store    = nullptr;
		effect_executor*             Effects  = nullptr;
		decision_executor*           Decision = nullptr;
		/// The kept names were read (or could not be): a thread shown may
		/// take them.
		std::function<void()> Loaded;
	};

	/// \brief Keeps the names suggested prompts' turns were sent under.
	/// \details Lives on the decision lane; the store runs on the effect lane
	/// one write at a time, in the order the turns were sent. What the core's
	/// memory says changed is written; nothing else is. A name is the
	/// reviewer's own text and is never logged.
	class prompt_turn_name_host final
	{
	public:
		explicit prompt_turn_name_host(prompt_turn_name_dependencies aDependencies);
		~prompt_turn_name_host();
		prompt_turn_name_host(const prompt_turn_name_host&)                    = delete;
		auto operator=(const prompt_turn_name_host&) -> prompt_turn_name_host& = delete;

		/// \brief Reads the kept names once.
		void load();
		/// \brief One turn was sent under a name.
		void               remember(core::prompt_turn_name aName);
		[[nodiscard]] auto names() const noexcept -> const core::prompt_turn_names&;
		[[nodiscard]] auto is_loaded() const noexcept -> bool;
		void               shutdown() noexcept;
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PROMPT_TURN_NAME_HOST_HPP
