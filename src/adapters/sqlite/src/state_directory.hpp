#ifndef BPR_SRC_ADAPTERS_SQLITE_SRC_STATE_DIRECTORY_HPP
#define BPR_SRC_ADAPTERS_SQLITE_SRC_STATE_DIRECTORY_HPP

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>

namespace bpr::adapters::sqlite::detail {
	/// \brief Why `<root>/state/v1` could not be opened.
	enum class state_directory_failure : std::uint8_t {
		invalid_path,
		invalid_permissions,
		platform_unsupported,
		unavailable,
	};

	/// \brief An opened `<root>/state/v1`, shown to be private to this user.
	///
	/// \details Every durable store in this adapter lives in one directory, so
	/// there is one policy for creating it: an explicit owner-only ACL on
	/// Windows, verified ownership and mode on POSIX, no symbolic link on the
	/// way in, and a refusal to accept a directory that was replaced between
	/// the check and the open.
	///
	/// Before BPR-0132 the epoch catalog applied that policy and the review
	/// ledger did not — it called `create_directories` and narrowed the mode
	/// afterwards, because it must not require the catalog to have been
	/// composed first. Whichever store opened first decided what the directory
	/// was, so the weaker policy won whenever the ledger got there first.
	///
	/// The handles are held, not merely checked. A directory validated and then
	/// released can be replaced before it is used.
	class state_directory final
	{
	public:
		/// \brief The platform handles retained for the directory's lifetime.
		///
		/// \details Completed by the implementation, which is the only place
		/// the platform types are named. A caller that needs somewhere to put a
		/// database needs `path()` and nothing else.
		struct opened;

		explicit state_directory(std::unique_ptr<opened> aOpened) noexcept;
		state_directory(state_directory&& aOther) noexcept;
		auto operator=(state_directory&& aOther) noexcept -> state_directory&;
		state_directory(const state_directory&)                    = delete;
		auto operator=(const state_directory&) -> state_directory& = delete;
		~state_directory();

		/// \brief The directory itself.
		[[nodiscard]] auto path() const noexcept -> const std::filesystem::path&;

		/// \brief The retained platform handles.
		[[nodiscard]] auto handles() noexcept -> opened&;

	private:
		std::unique_ptr<opened> Opened;
	};

	/// \brief Creates and opens `<root>/state/v1`, owner-private.
	///
	/// \details The one bootstrap. Both durable stores call it, so neither can
	/// establish the directory on terms the other would have refused.
	[[nodiscard]] auto open_state_directory(const std::filesystem::path& aRoot)
		-> std::expected<state_directory, state_directory_failure>;
} // namespace bpr::adapters::sqlite::detail

#endif // BPR_SRC_ADAPTERS_SQLITE_SRC_STATE_DIRECTORY_HPP
