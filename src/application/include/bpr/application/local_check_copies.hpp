#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_LOCAL_CHECK_COPIES_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_LOCAL_CHECK_COPIES_HPP

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief RFC-BPR-0120 ([[BPR-2189]]). A local draft's check copies: one
/// disposable folder per candidate, written from the candidate's own bytes
/// (its committed head's tree and the included working files the draft
/// captured), never from the author's worktree and never into it.
/// \details Each copy is an RFC-BPR-0104 scratch folder (`check-DRAFT-N`
/// under the agents' scratch root), so Settings counts it, Clear scratch
/// folders clears it and an unused one ages out with the others. Inside it,
/// `copy` is the folder the agent runs in, `scratch` its scratch folder, and
/// `manifest` the digest of every file written, from which the files the run
/// changed are found. Everything here runs on the host's effect lane.
namespace bpr::application {
	struct local_check_copy
	{
		std::string           Key;
		std::filesystem::path Folder;
		/// Where the agent runs: the candidate's files.
		std::filesystem::path Copy;
		std::filesystem::path Scratch;
		auto                  operator<=>(const local_check_copy&) const = default;
	};

	/// \brief The copies as the draft host uses them, on its effect lane.
	class local_check_copy_port
	{
	public:
		virtual ~local_check_copy_port() = default;
		/// \brief Where the copy is; computing it touches nothing.
		[[nodiscard]] virtual auto locate(std::string_view aDraft, std::uint64_t aCandidate) const -> local_check_copy = 0;
		/// \brief Whether the copy was made whole before: it is reused (Q4).
		[[nodiscard]] virtual auto ready(const local_check_copy& aCopy) const -> bool = 0;
		/// \brief Starts a copy afresh: anything partial goes; false when the
		/// folder cannot be made.
		[[nodiscard]] virtual auto begin(const local_check_copy& aCopy) -> bool = 0;
		/// \brief Writes one file of the candidate. A path that is absolute,
		/// climbs out of the copy or passes through a link is refused.
		[[nodiscard]] virtual auto put(const local_check_copy& aCopy, std::string_view aPath, std::span<const std::uint8_t> aBytes, bool aExecutable) -> bool = 0;
		/// \brief Records the digests of everything written: the copy is whole.
		[[nodiscard]] virtual auto seal(const local_check_copy& aCopy) -> bool = 0;
		/// \brief The files written that the run removed or changed, in path order.
		[[nodiscard]] virtual auto changed(const local_check_copy& aCopy) const -> std::vector<std::string> = 0;
		/// \brief Removes the draft's copies, all of them or all but `aKeep`'s.
		virtual void prune(std::string_view aDraft, std::optional<std::uint64_t> aKeep) = 0;
	};

	class local_check_copies final : public local_check_copy_port
	{
	public:
		/// \param aRoot The agents' scratch root; absolute.
		explicit local_check_copies(std::filesystem::path aRoot);

		/// \brief The scratch key of a draft's candidate's copy.
		[[nodiscard]] static auto key(std::string_view aDraft, std::uint64_t aCandidate) -> std::string;
		[[nodiscard]] auto        locate(std::string_view aDraft, std::uint64_t aCandidate) const -> local_check_copy override;
		[[nodiscard]] auto        ready(const local_check_copy& aCopy) const -> bool override;
		[[nodiscard]] auto        begin(const local_check_copy& aCopy) -> bool override;
		[[nodiscard]] auto        put(const local_check_copy& aCopy, std::string_view aPath, std::span<const std::uint8_t> aBytes, bool aExecutable) -> bool override;
		[[nodiscard]] auto        seal(const local_check_copy& aCopy) -> bool override;
		[[nodiscard]] auto        changed(const local_check_copy& aCopy) const -> std::vector<std::string> override;
		void                      prune(std::string_view aDraft, std::optional<std::uint64_t> aKeep) override;
		[[nodiscard]] auto        root() const noexcept -> const std::filesystem::path&;

	private:
		std::filesystem::path Root;
		/// Digests of a copy being written, by key, until it is sealed.
		std::map<std::string, std::map<std::string, std::array<std::uint8_t, 32>>> Writing;
	};
}

#endif
