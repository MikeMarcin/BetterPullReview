#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_RELEASE_MAINTENANCE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_RELEASE_MAINTENANCE_HPP

#include "bpr/core/profile_window.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace bpr::core {

	/// \brief [[BPR-1722]]. An export's output bounds: a heavy day's
	/// records with room to spare.
	/// \details A record is 706 to 867 bytes (783 to 795 on average in the
	/// fixture courses), and heavy use writes about 1,000 an hour: a pull
	/// request opened is about 30 to 45 records, a Home refresh about 5 to 10,
	/// a session start about 15. 16,384 records is about 16 hours of heavy
	/// use, and at 1 KiB each, wider than any measured, they fit 16 MiB.
	/// Records with long classified details stop at the byte limit first.
	inline constexpr std::uint32_t release_diagnostic_record_limit = 16384U;
	inline constexpr std::uint64_t release_diagnostic_byte_limit   = 16U * 1024U * 1024U;
	inline constexpr std::uint32_t release_maintenance_entry_limit = 4096U;

	struct release_maintenance_root_id_tag;
	struct release_maintenance_revision_tag;
	struct release_maintenance_generation_tag;
	using release_maintenance_root_id    = strong_id<release_maintenance_root_id_tag>;
	using release_maintenance_revision   = strong_id<release_maintenance_revision_tag>;
	using release_maintenance_generation = strong_id<release_maintenance_generation_tag>;

	/// \brief Legacy raw classes remain forbidden; rich formats admit only rebuilt, classified values.
	enum class release_diagnostic_field : std::uint32_t {
		operation         = 1U << 0U,
		outcome           = 1U << 1U,
		failure_code      = 1U << 2U,
		counts            = 1U << 3U,
		summary           = 1U << 4U,
		identity_or_path  = 1U << 5U,
		source_body       = 1U << 6U,
		comment_body      = 1U << 7U,
		private_text      = 1U << 8U,
		credential        = 1U << 9U,
		crash_memory      = 1U << 10U,
		classified_detail = 1U << 11U,
		correlation       = 1U << 12U,
		captured_identity = 1U << 13U,
	};
	inline constexpr std::uint32_t release_diagnostic_default_fields    = 0x0fU;
	inline constexpr std::uint32_t release_diagnostic_classified_fields = static_cast<std::uint32_t>(release_diagnostic_field::classified_detail) | static_cast<std::uint32_t>(release_diagnostic_field::correlation);

	/// \brief Permits only classified fixed vocabulary and numeric metadata, never raw text.
	/// \details The adapter must parse into those types before applying this rule. A label
	/// or a nonzero digest cannot establish that arbitrary log bytes are safe to export.
	[[nodiscard]] auto exportable(release_diagnostic_field aField) noexcept -> bool;

	/// \brief Artifact ownership classes, not paths or native deletion capabilities.
	enum class release_maintenance_artifact : std::uint32_t {
		diagnostics         = 1U << 0U,
		state_stores        = 1U << 1U,
		retained_history    = 1U << 2U,
		agent_workspaces    = 1U << 3U,
		mcp_snapshots       = 1U << 4U,
		credential_bindings = 1U << 5U,
		profile_registry    = 1U << 6U,
		updater_state       = 1U << 7U,
		sibling_profiles    = 1U << 8U,
		developer_worktrees = 1U << 9U,
		exported_files      = 1U << 10U,
		shared_review_state = 1U << 11U,
	};
	inline constexpr std::uint32_t release_profile_purge_categories = 0x3fU;

	/// \brief Exact local profile runtime; Root identifies a host-admitted native root.
	/// \details Root is opaque and must change if admission/root identity changes. It
	/// grants no filesystem authority by itself and is unrelated to GitHub account IDs.
	struct release_maintenance_authority
	{
		bpr_profile_id              Profile;
		application_session_id      Session;
		release_maintenance_root_id Root;
		auto                        operator==(const release_maintenance_authority&) const -> bool = default;
	};

	enum class diagnostic_export_policy : std::uint8_t { metadata,
														 anonymized,
														 identifying };
	enum class diagnostic_export_scope : std::uint8_t { retained_sessions,
														current_session };
	/// \brief Exact requested capture scope, carried through preparation and confirmation.
	struct diagnostic_export_options
	{
		diagnostic_export_policy Policy                                                     = diagnostic_export_policy::metadata;
		diagnostic_export_scope  Scope                                                      = diagnostic_export_scope::retained_sessions;
		bool                     IncludeRecent                                              = false;
		auto                     operator==(const diagnostic_export_options&) const -> bool = default;
	};
	[[nodiscard]] auto valid_diagnostic_export_options(diagnostic_export_options aOptions) noexcept -> bool;

	/// \brief [[BPR-1722]]. A local date and time, as the presenter's or
	/// host's clock reads it; a name uses it to the minute.
	struct diagnostic_export_local_time
	{
		int      Year   = 0;
		unsigned Month  = 0U;
		unsigned Day    = 0U;
		unsigned Hour   = 0U;
		unsigned Minute = 0U;
		unsigned Second = 0U;
	};
	/// \brief [[BPR-1722]]. The name a save suggests for an export:
	/// `BetterPullReview-diagnostics-2026-10-21-1432-0.3.1.zip`.
	/// \details The local date and time come first, so saved exports sort by
	/// when they were taken, and the name holds no colon or other character a
	/// file system refuses. The version is added only when it is dotted
	/// decimal numbers, so no host, account or repository can reach the name.
	/// An impossible date or time is left out. [[BPR-1725]]: the file is
	/// a `.zip` holding the text (`diagnostic_export_archive`).
	[[nodiscard]] auto diagnostic_export_file_name(const diagnostic_export_local_time& aTime, std::string_view aVersion) -> std::string;
	/// \brief [[BPR-1725]]. The name of the text inside a saved archive:
	/// the file's name with a final `.zip` replaced by `.txt`, so
	/// `BetterPullReview-diagnostics-2026-10-21-1432-0.3.1.txt`.
	/// \details Any case of `.zip` is replaced; a name that then does not end
	/// in `.txt` gains it. A name that is empty, holds a path separator, a
	/// NUL or a control character, is not UTF-8 or is over 255 bytes gives
	/// `BetterPullReview-diagnostics.txt`.
	[[nodiscard]] auto diagnostic_export_entry_name(std::string_view aFileName) -> std::string;
	/// \brief [[BPR-1725]]. The bytes of a saved export: a standard ZIP
	/// archive with one regular-file entry, `aText` deflated under
	/// `diagnostic_export_entry_name(aFileName)`, with its CRC-32, sizes, a
	/// UTF-8 name, `aTime` as its modification time and owner-only (0600)
	/// permissions, as the saved file has.
	/// \details The text is stored instead when deflate would not shrink it.
	/// A time the format cannot hold (before 1980 or after 2107, or
	/// impossible) gives 1980-01-01 00:00. Equal inputs give equal bytes.
	[[nodiscard]] auto diagnostic_export_archive(std::string_view aFileName, std::string_view aText, const diagnostic_export_local_time& aTime) -> std::string;

	/// \brief Immutable classified bytes retained by the host for preview and export.
	struct release_diagnostic_snapshot
	{
		release_maintenance_revision Revision;
		std::array<std::uint8_t, 32> Digest{ };
		std::uint32_t                Fields         = release_diagnostic_default_fields;
		std::uint32_t                Records        = 0U;
		std::uint64_t                Bytes          = 0U;
		std::uint32_t                OmittedRecords = 0U;
		diagnostic_export_options    Options{ };
		std::uint64_t                FirstUtcMs             = 0U;
		std::uint64_t                LastUtcMs              = 0U;
		std::uint32_t                Sessions               = 0U;
		std::uint32_t                Categories             = 0U;
		std::uint32_t                RecentRecords          = 0U;
		std::uint32_t                CaptureRedactedRecords = 0U;
		/// [[BPR-1717]]. Of `OmittedRecords`, the classified records left
		/// out to stay within the record and byte limits. They are always the
		/// oldest: an export keeps the newest records and drops from the start.
		std::uint32_t LimitOmittedRecords = 0U;
		/// [[BPR-1717]]. Whole log files left unread, oldest first, to stay
		/// within the input limit; their records are not counted anywhere else.
		std::uint32_t UnreadFiles                                                  = 0U;
		auto          operator==(const release_diagnostic_snapshot&) const -> bool = default;
	};

	/// \brief Digest-bound complete inventory retained and revalidated by the host.
	/// \details Entries count exact native targets, including vault bindings and sidecars.
	/// Unknown/changed objects require a fresh inventory; recursive root deletion is not
	/// authorized. Purge includes all profile categories so binding metadata cannot be
	/// selected for deletion independently of its exact credential-owner cleanup.
	struct release_maintenance_inventory
	{
		release_maintenance_revision Revision;
		std::array<std::uint8_t, 32> Digest{ };
		std::uint32_t                Categories                                                     = 0U;
		std::uint32_t                Entries                                                        = 0U;
		std::uint32_t                UnknownEntries                                                 = 0U;
		bool                         Complete                                                       = false;
		auto                         operator==(const release_maintenance_inventory&) const -> bool = default;
	};

	using release_maintenance_selection = std::variant<std::monostate, release_diagnostic_snapshot, release_maintenance_inventory>;

	enum class release_maintenance_command : std::uint8_t {
		preview_diagnostics,
		export_diagnostics,
		prepare_clear,
		clear_diagnostics,
		prepare_purge,
		purge_profile,
		cancel,
	};
	enum class release_maintenance_reason : std::uint8_t {
		none,
		unavailable,
		invalid_authority,
		busy,
		no_selection,
		selection_changed,
		invalid_selection,
		privacy_refusal,
		limit_exceeded,
		incomplete_inventory,
		stale_completion,
		invalid_completion,
		cancelled,
		point_of_no_return,
		io_failure,
		recovery_required,
		identity_exhausted,
		shutdown,
		invalid_command,
		/// \brief A file or directory in the profile is not private to its
		/// owner, so the operation touched nothing ([[BPR-1038]]). Distinct from
		/// `selection_changed`: nothing changed, and preparing again cannot
		/// help.
		storage_not_private,
	};
	enum class release_maintenance_phase : std::uint8_t {
		idle,
		preparing,
		ready,
		running,
		cancelling,
		completed,
		failed,
		cancelled,
		stopped,
	};

	/// \brief Complete request identity; prepared selections also retain their origin.
	struct release_maintenance_context
	{
		release_maintenance_authority  Authority;
		request_id                     Request;
		release_maintenance_generation Generation;
		release_maintenance_command    Operation = release_maintenance_command::preview_diagnostics;
		release_maintenance_selection  Selection;
		diagnostic_export_options      ExportOptions{ };
		auto                           operator==(const release_maintenance_context&) const -> bool = default;
	};

	/// \brief Host composition facts; absent capabilities fail closed, including signed out.
	struct release_maintenance_capabilities
	{
		bool Diagnostics                                                       = false;
		bool Export                                                            = false;
		bool Clear                                                             = false;
		bool Purge                                                             = false;
		auto operator==(const release_maintenance_capabilities&) const -> bool = default;
	};

	/// \brief A profile-bound gesture; execute echoes the preview, cancel its exact target.
	struct release_maintenance_intent
	{
		release_maintenance_authority              Authority;
		release_maintenance_command                Command = release_maintenance_command::preview_diagnostics;
		std::optional<release_maintenance_context> Target;
		diagnostic_export_options                  ExportOptions{ };
		bool                                       IdentifyingConfirmed = false;
	};
	/// \brief Rebinds after root admission or recovery, retiring all previous selections/work.
	struct release_maintenance_authority_changed
	{
		release_maintenance_authority    Authority;
		release_maintenance_capabilities Capabilities;
	};
	/// \brief Exact host notification that a source/selection changed; cancels old work.
	struct release_maintenance_invalidated
	{
		release_maintenance_authority Authority;
	};
	/// \brief Stops admission; host must drain retired work before releasing its capabilities.
	struct release_maintenance_shutdown
	{ };
	/// \brief A bounded prepared observation; never permission to export or delete by itself.
	struct release_maintenance_prepared
	{
		release_maintenance_context   Context;
		release_maintenance_selection Selection;
	};
	/// \brief Host fact: atomic export or irreversible cleanup has started for this request.
	struct release_maintenance_commit_started
	{
		release_maintenance_context Context;
	};
	/// \brief Exact durable success, including a success that raced advisory cancellation.
	struct release_maintenance_completed
	{
		release_maintenance_context Context;
	};
	/// \brief Typed operational failure; unknown reasons cannot retire an operation.
	struct release_maintenance_failed
	{
		release_maintenance_context Context;
		release_maintenance_reason  Reason = release_maintenance_reason::unavailable;
	};
	/// \brief Cancellation is a terminal only before irreversible work started.
	struct release_maintenance_cancelled
	{
		release_maintenance_context Context;
	};
	using release_maintenance_event = std::variant<release_maintenance_intent, release_maintenance_authority_changed,
												   release_maintenance_invalidated, release_maintenance_shutdown, release_maintenance_prepared,
												   release_maintenance_commit_started, release_maintenance_completed, release_maintenance_failed, release_maintenance_cancelled>;

	/// \brief Executes one semantic operation or requests cancellation of that exact context.
	/// \details Export requires an independently admitted local output capability. Purge
	/// requires ingress closure, generation invalidation, producer drain and store closure
	/// under the retained root lease. Core requests never supply a path, token or vault key.
	struct release_maintenance_request
	{
		release_maintenance_context Context;
		bool                        Cancel                                                       = false;
		auto                        operator==(const release_maintenance_request&) const -> bool = default;
	};
	/// \brief Fixed-size state; snapshot bytes and inventories stay in the host.
	struct release_maintenance_state
	{
		release_maintenance_authority              Authority;
		release_maintenance_capabilities           Capabilities;
		request_id                                 NextRequest{ 1U };
		release_maintenance_generation             Generation{ 1U };
		std::optional<release_maintenance_context> Prepared;
		std::optional<release_maintenance_context> Pending;
		release_maintenance_phase                  Phase                                                      = release_maintenance_phase::idle;
		release_maintenance_reason                 Reason                                                     = release_maintenance_reason::none;
		bool                                       CancellationRequested                                      = false;
		bool                                       PointOfNoReturn                                            = false;
		bool                                       Stopped                                                    = false;
		bool                                       Exhausted                                                  = false;
		bool                                       RecoveryRequired                                           = false;
		auto                                       operator==(const release_maintenance_state&) const -> bool = default;
	};
	/// \brief Bounded deltas and at most one effect; rejection never rewrites state.
	struct release_maintenance_output
	{
		bool                                       Accepted         = false;
		bool                                       StatusChanged    = false;
		bool                                       SelectionChanged = false;
		bool                                       AuthorityChanged = false;
		release_maintenance_reason                 Diagnostic       = release_maintenance_reason::none;
		std::optional<release_maintenance_request> Request;
		auto                                       operator==(const release_maintenance_output&) const -> bool = default;
	};
	/// \brief Command legality, shared by presentation and actual gesture admission.
	struct release_maintenance_command_projection
	{
		release_maintenance_command Command;
		release_maintenance_reason  DisabledReason = release_maintenance_reason::none;
		[[nodiscard]] auto          enabled() const noexcept -> bool { return DisabledReason == release_maintenance_reason::none; }
	};

	/// \brief Constructs a fail-closed runtime state without effects or nondeterminism.
	[[nodiscard]] auto make_release_maintenance_state(release_maintenance_authority    aAuthority,
													  release_maintenance_capabilities aCapabilities) noexcept -> release_maintenance_state;
	/// \brief Projects all seven commands; execute still requires the exact prepared identity.
	[[nodiscard]] auto release_maintenance_commands(const release_maintenance_state& aState) noexcept
		-> std::array<release_maintenance_command_projection, 7>;
	/// \brief Applies deterministic profile maintenance policy without I/O or content bytes.
	[[nodiscard]] auto transition(release_maintenance_state& aState, const release_maintenance_event& aEvent) noexcept
		-> release_maintenance_output;
}

#endif
