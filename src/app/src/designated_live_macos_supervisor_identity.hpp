#ifndef BPR_SRC_APP_SRC_DESIGNATED_LIVE_MACOS_SUPERVISOR_IDENTITY_HPP
#define BPR_SRC_APP_SRC_DESIGNATED_LIVE_MACOS_SUPERVISOR_IDENTITY_HPP

/// \file
/// \brief Retained capsule and inventory authority for the macOS supervisor.

#if defined(__APPLE__)

	#include "designated_live_macos_capsule_identity.hpp"

	#include <spawn.h>
	#include <sys/stat.h>

	#include <cstdint>
	#include <filesystem>
	#include <optional>
	#include <string>
	#include <string_view>
	#include <vector>

namespace bpr::app::designated_live_supervisor_identity {
	/// \brief Anonymous, single-use source and inventory channels for one verifier.
	class verifier_transport final
	{
	public:
		verifier_transport(const verifier_transport&)                    = delete;
		auto operator=(const verifier_transport&) -> verifier_transport& = delete;
		verifier_transport(verifier_transport&& aOther) noexcept;
		auto operator=(verifier_transport&& aOther) noexcept
			-> verifier_transport&;
		~verifier_transport();

	private:
		friend class session;
		verifier_transport(
			int aParentVerifierDescriptor,
			int aChildVerifierDescriptor,
			int aParentInventoryDescriptor,
			int aChildInventoryDescriptor) noexcept;
		void close_all() noexcept;

		int ParentVerifierDescriptor  = -1;
		int ChildVerifierDescriptor   = -1;
		int ParentInventoryDescriptor = -1;
		int ChildInventoryDescriptor  = -1;
	};

	/// \brief One retained prelaunch identity session.
	///
	/// \details The root and inventory are opened once before verification. The
	/// verifier runs relative to the retained root from exact digest-bound bytes
	/// delivered through an anonymous descriptor and reads the inventory through
	/// a second anonymous descriptor. The application spawn receives the same
	/// retained root as its working-directory
	/// authority. Path bindings are rechecked only to detect substitution; they
	/// are never used as the authority for verifier or application resolution.
	class session final
	{
	public:
		/// \brief Opens and validates the exact root and inventory objects.
		[[nodiscard]] static auto open(
			const std::filesystem::path& aRoot,
			const std::filesystem::path& aInventory,
			std::string_view             aTrustedVerifierSha256) -> std::optional<session>;

		session(const session&)                    = delete;
		auto operator=(const session&) -> session& = delete;
		session(session&& aOther) noexcept;
		auto operator=(session&& aOther) noexcept -> session&;
		~session();

		/// \brief Original normalized root text, for diagnostics and static APIs.
		[[nodiscard]] auto root_path() const noexcept
			-> const std::filesystem::path&;

		/// \brief Retained capsule-root descriptor.
		[[nodiscard]] auto root_descriptor() const noexcept -> int;

		/// \brief SHA-256 of exactly the retained inventory bytes sent to the
		/// verifier.
		[[nodiscard]] auto inventory_sha256() const noexcept -> std::string_view;

		/// \brief Revalidates path bindings and the captured retained objects.
		[[nodiscard]] auto revalidate() const -> bool;

		/// \brief Makes the verifier consume new anonymous source and inventory
		/// streams while resolving relative to the retained root.
		[[nodiscard]] auto configure_verifier_spawn(
			posix_spawn_file_actions_t& aActions) const
			-> std::optional<verifier_transport>;

		/// \brief Sends exact retained verifier and inventory bytes, each
		/// followed by an exact EOF on its own anonymous stream.
		[[nodiscard]] auto transmit_verifier_and_inventory(
			verifier_transport& aTransport) const -> bool;

		/// Fixed inherited descriptor containing the digest-bound verifier source.
		[[nodiscard]] static constexpr auto verifier_child_descriptor() noexcept
			-> int
		{
			return 64;
		}

		/// Fixed inherited anonymous descriptor containing the inventory stream.
		[[nodiscard]] static constexpr auto inventory_child_descriptor() noexcept
			-> int
		{
			return 65;
		}

		/// \brief Makes a relative application spawn resolve from this root.
		[[nodiscard]] auto configure_application_spawn(
			posix_spawn_file_actions_t& aActions) const -> bool;

	private:
		session() = default;
		void close_all() noexcept;

		std::filesystem::path                                RootPath;
		std::string                                          RootName;
		std::string                                          InventoryName;
		std::string                                          InventorySha256;
		std::string                                          VerifierSha256;
		int                                                  RootParentDescriptor      = -1;
		int                                                  RootDescriptor            = -1;
		int                                                  InventoryParentDescriptor = -1;
		int                                                  InventoryDescriptor       = -1;
		int                                                  VerifierDescriptor        = -1;
		struct stat                                          RootIdentity{ };
		struct stat                                          InventoryIdentity{ };
		struct stat                                          VerifierIdentity{ };
		std::vector<std::uint8_t>                            InventoryBytes;
		std::vector<std::uint8_t>                            VerifierBytes;
		std::vector<designated_live_capsule_identity::entry> CapsuleIdentity;
	};
}

#endif // defined(__APPLE__)

#endif // BPR_SRC_APP_SRC_DESIGNATED_LIVE_MACOS_SUPERVISOR_IDENTITY_HPP
