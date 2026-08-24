#ifndef BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_TESTING_HPP
#define BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_TESTING_HPP

#if defined(BPR_SQLITE_TESTING)
	#include "bpr/application/epoch_catalog_port.hpp"

	#include <cstdint>
	#include <filesystem>

namespace bpr::adapters::sqlite::detail::testing {
	// Arms one test-only interruption after a complete, fsynced bootstrap
	// database exists and before its no-replace canonical publication.
	/// \brief Performs the interrupt next bootstrap before publication operation.
	void interrupt_next_bootstrap_before_publication() noexcept;

	// Classifies a SQLite result code without requiring a live connection.
	/// \brief Computes the classify sqlite result code result.
	[[nodiscard]] auto classify_sqlite_result_code(int aResultCode) noexcept
		-> application::epoch_catalog_failure;

	#if defined(_WIN32)
	// What one owner-private catalog object is actually worth to the process
	// that created it. `AceCount` is the measured size of the object's own
	// DACL: one entry outside an AppContainer, and two inside, where the
	// restricted access check also requires the container's package SID.
	/// \brief Records one owner-private catalog object probe.
	struct private_descriptor_probe final
	{
		bool          Contained         = false;
		bool          Created           = false;
		bool          Usable            = false;
		bool          DirectoryValid    = false;
		bool          FileValid         = false;
		std::uint32_t DirectoryAceCount = 0U;
		std::uint32_t FileAceCount      = 0U;
		std::uint32_t LastError         = 0U;
	};

	// Creates a directory and a file below `aDirectory` with the product's own
	// private security attributes, writes to the file, and runs the product's
	// own validator over both.
	/// \brief Computes the probe private descriptor result.
	[[nodiscard]] auto probe_private_descriptor(
		const std::filesystem::path& aDirectory) -> private_descriptor_probe;
	#endif
} // namespace bpr::adapters::sqlite::detail::testing
#endif

#endif // BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_TESTING_HPP
