#ifndef BPR_SRC_ADAPTERS_MCP_INCLUDE_BPR_ADAPTERS_MCP_MCP_CONTEXT_HELPER_HPP
#define BPR_SRC_ADAPTERS_MCP_INCLUDE_BPR_ADAPTERS_MCP_MCP_CONTEXT_HELPER_HPP

#include "bpr/core/agent_finding.hpp"
#include "bpr/core/agent_session.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::adapters::mcp {
	/// \brief The one MCP revision this helper speaks.
	///
	/// \details One declared revision, not a negotiated range. A helper that
	/// accepted several would have to keep several answer shapes, and the
	/// closed tool enumeration below would stop being one reviewable list.
	inline constexpr std::string_view mcp_protocol_version = "2025-06-18";

	/// \brief The server identity reported by `initialize`.
	inline constexpr std::string_view mcp_server_name
		= "better-pull-review-context";
	inline constexpr std::string_view mcp_server_version = "1";

	/// \brief Every tool this helper will ever advertise.
	///
	/// \details A closed source-level enumeration. There is deliberately no
	/// dynamic, configured, or plugin registration anywhere in this adapter, so
	/// adding a tool is a diff a reviewer reads here — and every enumerator is
	/// a read of the frozen projection. There is no write, no draft, no
	/// publish, and no note store, not even a disabled one: a mutation is not
	/// a tool that is turned off here, it is a tool that does not exist.
	enum class mcp_tool : std::uint8_t {
		/// The `agent_revision_binding` this helper is pinned to.
		snapshot_binding,
		/// Every path the frozen projection contains.
		list_snapshot_files,
		/// One frozen file's bytes, bounded.
		read_snapshot_file,
		/// One frozen file's alignment facts, as `agent_finding_snapshot_file`.
		snapshot_file_alignment,
	};

	/// \brief The number of declared tools.
	///
	/// \details Exists so a sweep over the tools can be exhaustive rather than
	/// a sample: an undeclared tool fails the count.
	inline constexpr std::size_t mcp_tool_count = 4U;

	static_assert(
		static_cast<std::size_t>(mcp_tool::snapshot_file_alignment) + 1U
			== mcp_tool_count,
		"mcp_tool_count must cover every declared tool");

	/// \brief What one advertised tool is called and what it answers.
	struct mcp_tool_descriptor
	{
		mcp_tool         Tool = mcp_tool::snapshot_binding;
		std::string_view Name;
		std::string_view Summary;
		/// Whether the tool takes the one argument this helper accepts. There
		/// is no host, port, endpoint, URL, or credential argument anywhere in
		/// this table, and no type below has a field one could occupy.
		bool TakesPath = false;
	};

	/// \brief The advertised list, in enumerator order.
	inline constexpr std::array<mcp_tool_descriptor, mcp_tool_count> mcp_tools{ {
		{
			.Tool    = mcp_tool::snapshot_binding,
			.Name    = "snapshot_binding",
			.Summary = "The frozen revision binding this snapshot is pinned to.",
		},
		{
			.Tool    = mcp_tool::list_snapshot_files,
			.Name    = "list_snapshot_files",
			.Summary = "Every path the frozen review projection contains.",
		},
		{
			.Tool      = mcp_tool::read_snapshot_file,
			.Name      = "read_snapshot_file",
			.Summary   = "One frozen file's bytes, bounded, from inside the snapshot.",
			.TakesPath = true,
		},
		{
			.Tool      = mcp_tool::snapshot_file_alignment,
			.Name      = "snapshot_file_alignment",
			.Summary   = "One frozen file's comparison, line counts, and alignment identities.",
			.TakesPath = true,
		},
	} };

	/// \brief Every reason this helper refuses, as one closed enumeration.
	///
	/// \details Each value is a distinct cause a caller can act on. A single
	/// "denied" would make the refusal sweep unable to tell a path escape from
	/// a stale revision, and a test that cannot tell them apart cannot hold
	/// either to account.
	enum class mcp_refusal : std::uint8_t {
		/// The line was not a well-formed JSON-RPC 2.0 request.
		invalid_request,
		/// The method is not one of the three this helper implements:
		/// `initialize`, `tools/list`, and `tools/call`. Named rather than
		/// counted, because a count beside a list nobody wrote down is the
		/// kind of fact that stops being true without anyone noticing.
		unknown_method,
		/// The name is not in `mcp_tools`. Every write, draft, publish, and
		/// note name arrives here, because none of them is a tool.
		unknown_tool,
		/// The arguments are missing, mistyped, or oversize.
		invalid_arguments,
		/// The path names a network location by URI scheme or authority.
		network_location_refused,
		/// The path resolves outside the snapshot root, symlinks included.
		path_outside_snapshot,
		/// The snapshot does not contain the named entry.
		snapshot_entry_missing,
		/// The entry is larger than one answer may carry.
		snapshot_entry_too_large,
		/// The head moved: the observed binding is not the pinned one. The
		/// projection is deliberately not re-read.
		revision_binding_stale,
		/// The snapshot root is unreadable or malformed.
		snapshot_unavailable,
	};

	/// \brief The number of declared refusals.
	inline constexpr std::size_t mcp_refusal_count = 10U;

	static_assert(
		static_cast<std::size_t>(mcp_refusal::snapshot_unavailable) + 1U
			== mcp_refusal_count,
		"mcp_refusal_count must cover every declared refusal");

	/// \brief The stable wire name of one refusal.
	[[nodiscard]] auto name(mcp_refusal aRefusal) noexcept -> std::string_view;

	/// \brief Environment variable names this helper may hold.
	///
	/// \details Names, never values — the same shape the launch path's profile
	/// allowlist has, and for the same reason: a map from name to value would
	/// give every name a place a token could sit. Both entries are first-party
	/// and non-secret: a directory path and a revision binding. The helper
	/// reduces its own environment to exactly this list before it reads a
	/// single request, so a parent that leaks a credential into the child
	/// leaks it into a process that has already dropped it.
	inline constexpr std::array<std::string_view, 2U> mcp_environment_allowlist{
		std::string_view{ "BPR_MCP_SNAPSHOT_ROOT" },
		std::string_view{ "BPR_MCP_PINNED_BINDING" },
	};

	/// \brief The longest request line this helper will decode.
	inline constexpr std::size_t maximum_mcp_request_bytes = 64U * 1024U;
	/// \brief The most bytes one `read_snapshot_file` answer may carry.
	inline constexpr std::size_t maximum_mcp_snapshot_entry_bytes
		= 1U * 1024U * 1024U;
	/// \brief The most paths one frozen projection may contain.
	inline constexpr std::size_t maximum_mcp_snapshot_files = 4096U;
	/// \brief The deepest JSON nesting one request may carry.
	inline constexpr std::size_t maximum_mcp_json_depth = 16U;

	/// \brief Whether a requested path names a network location by URI scheme.
	///
	/// \details `https://api.github.com/...` is a location this helper must
	/// never treat as a snapshot-relative path. It refuses rather than
	/// canonicalising it into a directory that happens to exist.
	[[nodiscard]] auto names_uri_scheme(std::string_view aPath) noexcept -> bool;

	/// \brief Whether a requested path names a network or UNC authority.
	///
	/// \details A leading `//` or `\\` names a host, which is the shape a
	/// listener or share would be reached through.
	[[nodiscard]] auto names_network_authority(std::string_view aPath) noexcept
		-> bool;

	/// \brief The canonical one-line spelling of one revision binding.
	[[nodiscard]] auto encode_revision_binding(
		const core::agent_revision_binding& aBinding) -> std::string;

	/// \brief Reads one revision binding from its canonical one-line spelling.
	[[nodiscard]] auto decode_revision_binding(std::string_view aText)
		-> std::optional<core::agent_revision_binding>;

	/// \brief What one environment reduction found and what it kept.
	struct mcp_environment_report
	{
		/// Every name present before the reduction. Names only.
		std::vector<std::string> Received;
		/// Every name still present after it. Compared against the allowlist.
		std::vector<std::string> Retained;
	};

	/// \brief Reduces this process's environment to `mcp_environment_allowlist`.
	///
	/// \details Reads the complete environment, removes every name outside the
	/// allowlist, then re-reads the environment so that `Retained` is what the
	/// process actually holds rather than what the reduction intended.
	[[nodiscard]] auto reduce_environment_to_allowlist() -> mcp_environment_report;

	/// \brief One helper bound to one frozen snapshot and one pinned binding.
	///
	/// \details The pinned binding is a value supplied by the host that froze
	/// the projection. Every `tools/call` re-reads the snapshot's own binding
	/// record and compares it against that pin; when they differ the call is
	/// refused as `revision_binding_stale` and **no projection read happens**,
	/// which is the difference between reporting staleness and silently
	/// serving whichever revision exists now.
	///
	/// Nothing here opens a socket, starts a process, or reads a credential.
	/// The type has no field a token, endpoint, or port could occupy.
	class mcp_context_helper final
	{
	public:
		mcp_context_helper(
			std::filesystem::path        aSnapshotRoot,
			core::agent_revision_binding aPinnedBinding);

		/// \brief Answers one newline-delimited JSON-RPC 2.0 message.
		///
		/// \returns The response line, or nothing for a notification.
		[[nodiscard]] auto handle_line(std::string_view aLine)
			-> std::optional<std::string>;

		/// \brief The binding this helper is pinned to.
		[[nodiscard]] auto pinned_binding() const noexcept
			-> const core::agent_revision_binding&;

	private:
		std::filesystem::path        SnapshotRoot;
		core::agent_revision_binding Pinned;
	};

	/// \brief The frozen per-path alignment facts a snapshot root records.
	[[nodiscard]] auto read_snapshot_alignment(
		const std::filesystem::path& aSnapshotRoot)
		-> std::optional<std::vector<core::agent_finding_snapshot_file>>;
} // namespace bpr::adapters::mcp

#endif // BPR_SRC_ADAPTERS_MCP_INCLUDE_BPR_ADAPTERS_MCP_MCP_CONTEXT_HELPER_HPP
