#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_CONFIGURATION_EXCHANGE_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_CONFIGURATION_EXCHANGE_HPP

#include "bpr/application/agent_setup_preferences.hpp"
#include "bpr/application/port.hpp"
#include "bpr/application/review_skill_host.hpp"
#include "bpr/core/agent_configuration.hpp"
#include "bpr/core/review_preparation.hpp"

#include <array>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-2016]], RFC-BPR-0107. The host half of sharing agents and
/// prompts: what the profile holds now, the file read and written on the
/// effect lane, and one all-or-nothing application of an import.
namespace bpr::application {
	/// \brief The review skill's purpose ID (RFC-BPR-0107 assigns it).
	inline constexpr std::string_view review_skill_purpose = "review.skill";

	/// \brief Everything an import may change, as stored: the agent setup
	/// (its list), the edited review skill and the writing hooks, with the
	/// built-in version each hook override was written against when it was
	/// recorded for the override's current text.
	struct agent_configuration_snapshot
	{
		agent_setup_preferences                                          Agents;
		std::optional<stored_review_skill>                               ReviewSkill;
		core::writing_hook_settings                                      Hooks;
		std::array<std::optional<std::string>, core::writing_hook_count> WrittenAgainst;

		auto operator<=>(const agent_configuration_snapshot&) const = default;
	};

	/// \brief Every prompt this version recognizes, as the snapshot has it:
	/// the eight writing hooks in `core::writing_hook` order, then the review
	/// skill.
	[[nodiscard]] auto local_prompts(const agent_configuration_snapshot& aSnapshot) -> std::vector<core::local_prompt>;
	/// \brief Earlier purpose IDs this version still reads.
	[[nodiscard]] auto prompt_aliases() noexcept -> std::span<const core::prompt_alias>;
	/// \brief The writing hook a purpose names, or none (the review skill, or unknown).
	[[nodiscard]] auto prompt_hook(std::string_view aPurpose) noexcept -> std::optional<core::writing_hook>;

	/// \brief One prompt chosen for export: its purpose and whether the
	/// shipped default is sent rather than the profile's edit.
	struct exported_prompt
	{
		std::string Purpose;
		bool        BuiltIn = false;
	};
	/// \brief The exact bytes of an export of the chosen agents (by entry id,
	/// in the profile's order) and prompts, or none when they exceed the
	/// file's bound.
	[[nodiscard]] auto export_agent_configuration(const agent_configuration_snapshot& aSnapshot, std::span<const std::uint32_t> aAgents, std::span<const exported_prompt> aPrompts,
												  std::string aLabel, std::string aExportedBy) -> std::optional<std::string>;

	/// \brief The snapshot an import's result makes from `aBefore`: new and
	/// re-identified entries get an identity from `aMint`, a replaced entry's
	/// kept check is dropped, an imported review skill records the built-in
	/// version it was written against, and each hook keeps its provenance.
	[[nodiscard]] auto snapshot_after_import(const agent_configuration_snapshot& aBefore, const core::agent_configuration_result& aResult, std::span<const std::uint32_t> aReplaced,
											 const std::function<std::string()>& aMint) -> agent_configuration_snapshot;

	/// \brief How one application ended.
	enum class agent_configuration_outcome : std::uint8_t {
		applied,
		/// The stored settings are not what the preview was made from (an edit
		/// in another window or through `bpr`); nothing changed.
		changed_since,
		/// A store refused a write; nothing changed.
		storage_failed,
	};
	[[nodiscard]] auto stable_name(agent_configuration_outcome aOutcome) noexcept -> std::string_view;

	/// \brief Where the profile keeps what an import changes.
	class agent_configuration_store_port
	{
	public:
		virtual ~agent_configuration_store_port() = default;
		/// The stored snapshot, or none when a store cannot be read.
		[[nodiscard]] virtual auto load() -> std::optional<agent_configuration_snapshot> = 0;
		/// Writes `aAfter` only if the stored settings still equal `aBefore`;
		/// all of it or none, across a crash too (`recover`).
		[[nodiscard]] virtual auto apply(const agent_configuration_snapshot& aBefore, const agent_configuration_snapshot& aAfter) -> agent_configuration_outcome = 0;
		/// Records that a hook's override `aText` was written against `aVersion`.
		[[nodiscard]] virtual auto record_written_against(core::writing_hook aHook, std::string_view aText, std::string_view aVersion) -> bool = 0;
		/// Finishes or undoes an application a crash interrupted.
		[[nodiscard]] virtual auto recover() -> bool = 0;
	};

	/// \brief Why a file could not be offered for import.
	struct agent_configuration_read_error
	{
		/// None: the file itself could not be read.
		std::optional<core::configuration_file_refusal> Refusal;

		auto operator<=>(const agent_configuration_read_error&) const = default;
	};

	/// \brief A file ready for the preview, against the snapshot it was read with.
	struct agent_configuration_preview
	{
		agent_configuration_snapshot    Snapshot;
		std::vector<core::local_prompt> Prompts;
		core::decoded_configuration     File;

		auto operator<=>(const agent_configuration_preview&) const = default;
	};

	struct agent_configuration_dependencies
	{
		agent_configuration_store_port* Store = nullptr;
		/// Reads at most `aLimit` + 1 bytes of a file; none when it cannot.
		std::function<std::optional<std::string>(const std::string& aPath, std::size_t aLimit)> ReadFile;
		/// Writes a whole file, replacing it; false when it cannot.
		std::function<bool(const std::string& aPath, const std::string& aBytes)> WriteFile;
		/// A new agent identity: 32 lowercase hex digits.
		std::function<std::string()> MintIdentity;
		effect_executor*             Effects  = nullptr;
		decision_executor*           Decision = nullptr;
	};

	/// \brief [[BPR-2016]], RFC-BPR-0107. Export, preview and application.
	///
	/// \details Lives on the decision lane; the stores and the file run on the
	/// effect lane. Nothing it does launches a program, reaches the network,
	/// reads a credential or edits a harness's own configuration. Answers
	/// arrive on the decision lane and never after shutdown.
	class agent_configuration_host final
	{
	public:
		explicit agent_configuration_host(agent_configuration_dependencies aDependencies);
		~agent_configuration_host();
		agent_configuration_host(const agent_configuration_host&)                    = delete;
		auto operator=(const agent_configuration_host&) -> agent_configuration_host& = delete;

		/// The stored snapshot, or none.
		void snapshot(std::function<void(std::optional<agent_configuration_snapshot>)> aAnswered);
		/// Writes an export's bytes to `aPath`.
		void write(std::string aPath, std::string aBytes, std::function<void(bool)> aAnswered);
		/// Reads `aPath` against the stored snapshot.
		void read(std::string aPath, std::function<void(std::expected<agent_configuration_preview, agent_configuration_read_error>)> aAnswered);
		/// Applies the chosen items of a preview. A refusal of the choices
		/// answers with that refusal and changes nothing.
		using applied = std::expected<std::pair<agent_configuration_outcome, agent_configuration_snapshot>, core::configuration_apply_error>;
		void apply(const agent_configuration_preview& aPreview, std::vector<core::agent_import_choice> aAgents, std::vector<std::size_t> aPrompts, std::function<void(applied)> aAnswered);
		/// Records a Settings save's provenance, best effort.
		void record_written_against(core::writing_hook aHook, std::string aText);

		void               shutdown() noexcept;
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_CONFIGURATION_EXCHANGE_HPP
