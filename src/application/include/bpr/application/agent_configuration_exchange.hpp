#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_CONFIGURATION_EXCHANGE_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_CONFIGURATION_EXCHANGE_HPP

#include "bpr/application/agent_setup_preferences.hpp"
#include "bpr/application/appearance_runtime_ports.hpp"
#include "bpr/application/port.hpp"
#include "bpr/application/prompt_macro_host.hpp"
#include "bpr/application/review_skill_host.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
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
		/// [[BPR-2032]], RFC-BPR-0112. The profile's optional author workflows
		/// with this computer's bindings. Selecting or importing one runs nothing.
		std::vector<core::local_workflow> Workflows;
		/// RFC-BPR-0125 AC24 ([[BPR-2149]]). The profile's suggested prompts
		/// and their revision: an export records its prompt pack in them and
		/// an import merges a pack into them.
		prompt_macro_stored PromptMacros;

		auto operator<=>(const agent_configuration_snapshot&) const = default;
	};

	/// \brief Every prompt this version recognizes, as the snapshot has it:
	/// the eight writing hooks in `core::writing_hook` order, then the review
	/// skill.
	[[nodiscard]] auto local_prompts(const agent_configuration_snapshot& aSnapshot) -> std::vector<core::local_prompt>;
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
	/// in the profile's order), prompts and workflows (by ID), Identity when
	/// given and the given repositories, or none when they exceed the file's
	/// bound or a workflow's agent is not in the profile.
	/// \details A workflow's agent actions carry the agent they are bound to
	/// here, which the export then includes; no program path is exported.
	/// [[BPR-2198]]: Identity and Repositories are names the composition
	/// read (`core::portable_identity_of`, `core::portable_repositories_of`);
	/// no credential is ever an input.
	/// RFC-BPR-0125 AC24: `aPack`, when given and holding a prompt, is the
	/// file's prompt pack. [[BPR-2283]]: `aTheme`, when given, its Theme.
	/// [[BPR-2301]]: `aReview`, when given, its Review.
	[[nodiscard]] auto export_agent_configuration(const agent_configuration_snapshot& aSnapshot, std::span<const std::uint32_t> aAgents, std::span<const exported_prompt> aPrompts,
												  std::string aLabel, std::string aExportedBy, std::span<const std::string> aWorkflows = { }, const core::portable_identity* aIdentity = nullptr,
												  std::span<const core::portable_repository> aRepositories = { }, const core::prompt_pack_export* aPack = nullptr,
												  const core::portable_theme* aTheme = nullptr, const core::portable_review* aReview = nullptr) -> std::optional<std::string>;

	/// \brief [[BPR-2301]]. The Review section an export of `aSettings`
	/// carries: every Settings › Review preference, and nothing a load
	/// observed (`LabelColumnsRecovery`).
	[[nodiscard]] auto portable_review_of(const review_behaviour_settings& aSettings) -> core::portable_review;
	/// \brief [[BPR-2301]]. `aHere` with every setting `aFile` holds in its
	/// place; the center ribbon held to its bounds. Always `valid`.
	[[nodiscard]] auto with_review(review_behaviour_settings aHere, const core::portable_review& aFile) -> review_behaviour_settings;
	/// \brief [[BPR-2301]]. The settings that differ between two records, in
	/// `core::review_setting` order.
	[[nodiscard]] auto review_differences(const review_behaviour_settings& aBefore, const review_behaviour_settings& aAfter) -> std::vector<core::review_setting>;

	/// \brief RFC-BPR-0125 AC24. The snapshot an export of `aPack` records:
	/// the prompts with their links (and new UUIDs) at one revision on.
	[[nodiscard]] auto snapshot_after_export(const agent_configuration_snapshot& aBefore, const core::prompt_pack_export& aPack) -> agent_configuration_snapshot;

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
		/// [[BPR-2198]]. The profile's name, sign-ins, servers and
		/// repositories, which the composition reads for Identity and
		/// Repositories.
		core::profile_facts Here;
		/// [[BPR-2283]]. The file's Theme as an import would keep it, against
		/// the theme on screen it would replace; the composition reads both from
		/// the appearance host. None when the file has no Theme.
		struct theme_against_here
		{
			core::appearance_variant OnScreen = core::appearance_variant::light;
			core::appearance_palette Here{ };
			imported_theme           Imported;

			auto operator<=>(const theme_against_here&) const = default;
		};
		std::optional<theme_against_here> Theme;
		/// [[BPR-2301]]. The profile's Review settings, and what importing the
		/// file's Review would make them (`with_review`); the composition
		/// reads them from the workbench preferences. None when the file has
		/// no Review or the preferences are not loaded.
		struct review_against_here
		{
			review_behaviour_settings         Here;
			review_behaviour_settings         Imported;
			std::vector<core::review_setting> Changes;

			auto operator<=>(const review_against_here&) const = default;
		};
		std::optional<review_against_here> Review;

		auto operator<=>(const agent_configuration_preview&) const = default;
	};

	/// \brief [[BPR-2032]]. The profile agent each agent action of imported
	/// workflow `aItem` would run as under the agent choices: the entry an
	/// added, kept or replaced agent becomes, or the profile's agent of the
	/// same name. An action whose agent is neither is left unbound.
	[[nodiscard]] auto default_workflow_agents(const agent_configuration_preview& aPreview, std::span<const core::agent_import_choice> aAgents, std::size_t aItem)
		-> std::vector<core::workflow_agent_binding>;

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
		/// [[BPR-2032]]: `aWorkflows`, each applied with its program bindings
		/// and, where none is given, `default_workflow_agents`.
		/// RFC-BPR-0125 AC24: `aPack`, the prompt pack's choices, merged into
		/// the profile's suggested prompts in the same write.
		void apply(const agent_configuration_preview& aPreview, std::vector<core::agent_import_choice> aAgents, std::vector<std::size_t> aPrompts, std::function<void(applied)> aAnswered,
				   std::vector<core::workflow_import_choice> aWorkflows = { }, core::prompt_pack_choices aPack = { });
		/// \brief RFC-BPR-0125 AC24. Records an export's prompt pack (`aAfter`
		/// from `snapshot_after_export`) if the stored settings still equal
		/// `aBefore`.
		void record_export(agent_configuration_snapshot aBefore, agent_configuration_snapshot aAfter, std::function<void(agent_configuration_outcome)> aAnswered);
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
