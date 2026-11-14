#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PROMPT_MACRO_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PROMPT_MACRO_HOST_HPP

#include "bpr/application/port.hpp"
#include "bpr/core/prompt_macro.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

/// \file
/// \brief RFC-BPR-0125 milestone 3 ([[BPR-2146]]), with RFC-BPR-0128's Quick
/// ask and scopes ([[BPR-2163]]). A profile's suggested prompts: only the
/// differences from the shipped set, under a revision so two windows cannot
/// overwrite each other.
namespace bpr::application {
	/// \brief What a profile's store holds.
	struct prompt_macro_stored
	{
		core::prompt_macro_differences Differences;
		/// Bumped by every write; a write names the revision it was made from.
		std::uint64_t Revision = 0U;
		/// Rows this build cannot read (a place, About or fact a newer build
		/// wrote): kept, never offered and never rewritten.
		std::vector<std::string> Newer;

		auto operator<=>(const prompt_macro_stored&) const = default;
	};

	enum class prompt_macro_write : std::uint8_t {
		stored,
		/// Written from an older revision: nothing changed.
		stale,
		/// The store refused the write: nothing changed.
		failed,
	};

	/// \brief Where a profile keeps its suggested prompts.
	class prompt_macro_store_port
	{
	public:
		virtual ~prompt_macro_store_port() = default;
		/// \brief The stored differences; nothing when the store cannot be
		/// read. A store without the tables is the shipped set, revision 0.
		[[nodiscard]] virtual auto load() -> std::optional<prompt_macro_stored> = 0;
		/// \brief Replaces the differences when the store is at `aFrom`.
		[[nodiscard]] virtual auto store(const core::prompt_macro_differences& aDifferences, std::uint64_t aFrom) -> prompt_macro_write = 0;
	};

	/// \brief What the window shows: the merged set, and whether edits can be
	/// made at all.
	struct prompt_macro_projection
	{
		core::prompt_macro_set Set;
		std::uint64_t          Revision = 0U;
		/// The store could be read; false shows the shipped set and says the
		/// reviewer's edits are unavailable.
		bool                     Readable = false;
		bool                     Loaded   = false;
		std::vector<std::string> Newer;
		/// The last write's outcome, for the sheet's message.
		std::optional<prompt_macro_write> LastWrite;

		auto operator<=>(const prompt_macro_projection&) const = default;
	};

	struct prompt_macro_dependencies
	{
		prompt_macro_store_port* Store    = nullptr;
		effect_executor*         Effects  = nullptr;
		decision_executor*       Decision = nullptr;
		/// The projection changed: a load, a write, or a refusal.
		std::function<void(const prompt_macro_projection&)> Changed;
	};

	/// \brief Keeps a window's suggested prompts. Lives on the decision lane;
	/// the store runs on the effect lane, one operation at a time. A
	/// template is the reviewer's own text and is never logged.
	class prompt_macro_host final
	{
	public:
		explicit prompt_macro_host(prompt_macro_dependencies aDependencies);
		~prompt_macro_host();
		prompt_macro_host(const prompt_macro_host&)                    = delete;
		auto operator=(const prompt_macro_host&) -> prompt_macro_host& = delete;

		/// \brief Reads the store again (at start, and when the window comes
		/// back to the front, so another window's edit reaches it).
		void load();
		/// \brief Writes `aDifferences` from the revision shown.
		void               save(core::prompt_macro_differences aDifferences);
		[[nodiscard]] auto projection() const noexcept -> const prompt_macro_projection&;
		/// \brief The differences behind the projection.
		[[nodiscard]] auto differences() const noexcept -> const core::prompt_macro_differences&;
		void               shutdown() noexcept;
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};

	/// \brief The differences that make `aEdited` from the shipped set: each
	/// built-in whose body or On differs, every own macro, and each place's
	/// order when it is not the shipped order.
	[[nodiscard]] auto prompt_macro_differences_of(const std::vector<core::prompt_builtin>& aShipped, const std::vector<core::prompt_macro>& aEdited) -> core::prompt_macro_differences;
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PROMPT_MACRO_HOST_HPP
