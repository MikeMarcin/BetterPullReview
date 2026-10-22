#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_CLONE_LINK_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_CLONE_LINK_HOST_HPP

#include "bpr/application/port.hpp"
#include "bpr/core/clone_discovery.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::application {
	/// \brief [[BPR-PENDING-AQ3]], RFC-BPR-0091 owner decision D2. One
	/// repository's remembered choice: its linked clone, or the private copy.
	///
	/// \details Keyed by the Git host's name and the repository's identity, so
	/// a renamed repository keeps its link; the owner and name are as last
	/// seen and match a link whose identity is unknown. The path is a folder
	/// the reviewer chose or confirmed; nothing here holds a credential or a
	/// URL.
	struct repository_link
	{
		std::string   Host;
		std::uint64_t Repository = 0U;
		std::string   Owner;
		std::string   Name;
		/// The linked clone; empty when `PrivateCopy`.
		std::string Path;
		/// The remote that names the repository.
		std::string Remote;
		bool        PrivateCopy = false;

		auto operator<=>(const repository_link&) const = default;
	};

	/// \brief Whether a link may be stored: bounded text, a clone path unless
	/// it is the private copy (and none when it is), no control character.
	[[nodiscard]] inline auto valid(const repository_link& aLink) noexcept -> bool
	{
		const auto text = [](const std::string& aText, const bool aEmptyAllowed) {
			return (aEmptyAllowed || !aText.empty()) && aText.size() <= core::maximum_remote_text_bytes
				&& std::ranges::none_of(aText, [](const char aCharacter) { return static_cast<unsigned char>(aCharacter) < 0x20U || aCharacter == 0x7F; });
		};
		return text(aLink.Host, false) && text(aLink.Owner, false) && text(aLink.Name, false) && text(aLink.Remote, true)
			&& (aLink.PrivateCopy ? aLink.Path.empty() : text(aLink.Path, false));
	}

	/// \brief Where a profile's clone folders and links live.
	class clone_link_store_port
	{
	public:
		virtual ~clone_link_store_port() = default;
		/// \brief The listed folders, in order, or nothing when unreadable.
		[[nodiscard]] virtual auto load_clone_folders() -> std::optional<std::vector<std::string>>    = 0;
		[[nodiscard]] virtual auto store_clone_folders(std::span<const std::string> aFolders) -> bool = 0;
		/// \brief Every link, or nothing when unreadable.
		[[nodiscard]] virtual auto load_links() -> std::optional<std::vector<repository_link>> = 0;
		/// \brief Adds or replaces the link for its host and repository.
		[[nodiscard]] virtual auto store_link(const repository_link& aLink) -> bool                                                                              = 0;
		[[nodiscard]] virtual auto forget_link(const std::string& aHost, std::uint64_t aRepository, const std::string& aOwner, const std::string& aName) -> bool = 0;
	};

	/// \brief The repository of the review setup is for.
	struct clone_repository
	{
		std::string   Host;
		std::uint64_t Repository = 0U;
		std::string   Owner;
		std::string   Name;

		auto operator<=>(const clone_repository&) const = default;
	};

	/// \brief Why a remembered clone no longer serves.
	enum class clone_link_problem : std::uint8_t {
		none,
		/// The folder is gone or is no longer a clone.
		not_a_clone,
		/// The folder is a clone, but none of its remotes names the repository.
		no_matching_remote,
	};

	[[nodiscard]] inline auto stable_name(const clone_link_problem aProblem) noexcept -> std::string_view
	{
		switch (aProblem) {
		case clone_link_problem::none:               return "none";
		case clone_link_problem::not_a_clone:        return "not_a_clone";
		case clone_link_problem::no_matching_remote: return "no_matching_remote";
		}
		return "none";
	}

	/// \brief What became of one folder the reviewer chose for a repository.
	enum class clone_choice_answer : std::uint8_t {
		linked,
		not_a_clone,
		no_matching_remote,
		not_saved,
	};

	/// \brief What the Repository step and Settings are given, as values.
	struct clone_link_projection
	{
		std::optional<clone_repository> Repository;
		/// The link for this repository, after it was checked again.
		std::optional<repository_link> Linked;
		clone_link_problem             Problem = clone_link_problem::none;
		std::vector<core::clone_match> Matches;
		std::vector<std::string>       Folders;
		/// Usual folders that exist, not yet listed.
		std::vector<std::string>     Suggestions;
		std::vector<repository_link> Links;
		bool                         Discovering = false;
		bool                         Discovered  = false;
		std::uint64_t                Generation  = 0U;

		auto operator<=>(const clone_link_projection&) const = default;
	};

	/// \brief The metadata reads the host asks of the workspace adapter.
	struct clone_reads
	{
		std::function<std::vector<core::clone_candidate>(std::span<const std::string>, std::stop_token)> Scan;
		std::function<std::optional<core::clone_candidate>(const std::string&)>                          Read;
		std::function<std::vector<std::string>(std::span<const std::string>)>                            Existing;
	};

	struct clone_link_dependencies
	{
		clone_link_store_port* Store = nullptr;
		clone_reads            Reads;
		/// The reviewer's home folder, for suggestions only.
		std::string                                       Home;
		bool                                              Windows  = false;
		effect_executor*                                  Effects  = nullptr;
		decision_executor*                                Decision = nullptr;
		std::function<void(const clone_link_projection&)> Publish;
	};

	/// \brief [[BPR-PENDING-AQ3]], RFC-BPR-0091 M3. Finds the reviewer's clone
	/// of a review's repository in the listed clone folders and remembers it.
	///
	/// \details State on the decision lane; every read on the effect lane,
	/// through `clone_reads`, which reads metadata only and never runs Git.
	/// The core matches remotes and orders the matches. A remembered link is
	/// checked again whenever its repository is observed: a moved folder or a
	/// clone that no longer names the repository is reported and stops serving
	/// until the reviewer chooses again. Shut down, then wait for quiescence,
	/// before the store or a lane ends.
	class clone_link_host final
	{
	public:
		explicit clone_link_host(clone_link_dependencies aDependencies);
		~clone_link_host();
		clone_link_host(const clone_link_host&)                    = delete;
		auto operator=(const clone_link_host&) -> clone_link_host& = delete;

		/// Reads the listed folders and the links.
		void load();
		/// The repository of the review now open, or none.
		void observe_repository(std::optional<clone_repository> aRepository);
		/// Looks in the listed folders for this repository's clones.
		void discover();
		void set_clone_folders(std::vector<std::string> aFolders);
		void add_clone_folder(std::string aFolder);
		void remove_clone_folder(const std::string& aFolder);
		/// Links the chosen folder to the observed repository if it is a
		/// clone that names it.
		void choose_clone(std::string aPath, std::function<void(clone_choice_answer)> aAnswered = { });
		/// Records the private copy as this repository's choice.
		void use_private_copy();
		void forget(const repository_link& aLink);

		/// The observed repository has a link that serves or the private copy.
		[[nodiscard]] auto repository_chosen() const noexcept -> bool;
		[[nodiscard]] auto projection() const -> clone_link_projection;
		void               shutdown() noexcept;
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_CLONE_LINK_HOST_HPP
