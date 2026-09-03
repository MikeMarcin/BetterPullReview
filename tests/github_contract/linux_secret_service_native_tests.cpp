#if !defined(_GNU_SOURCE)
	#define _GNU_SOURCE
#endif

#include "linux_secret_service.hpp"
#include "linux_secret_service_test_support.hpp"

#include "bpr/application/manual_executor.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <optional>
#include <poll.h>
#include <span>
#include <spawn.h>
#include <string>
#include <string_view>
#include <sys/random.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <variant>
#include <vector>

namespace {
	using namespace bpr;
	using namespace bpr::adapters::github;
	using namespace bpr::tests::linux_secret_service;

	inline constexpr std::size_t   native_canary_a_bytes      = 73U;
	inline constexpr std::size_t   native_canary_b_bytes      = application::maximum_github_credential_bytes;
	inline constexpr std::size_t   native_leak_probe_bytes    = 32U;
	inline constexpr std::uint64_t native_session             = 0x4250522400000001ULL;
	inline constexpr std::uint64_t native_generation          = 0x4250522400000002ULL;
	inline constexpr std::uint64_t maximum_audited_file_bytes = 128U * 1024U * 1024U;
	inline constexpr int           child_ready_descriptor     = 100;
	inline constexpr int           child_control_descriptor   = 101;

	using leak_probe  = std::span<const std::byte>;
	using leak_probes = std::span<const leak_probe>;

	static_assert(native_leak_probe_bytes <= native_canary_a_bytes);
	static_assert(native_leak_probe_bytes <= native_canary_b_bytes);

	class secure_bytes final
	{
	public:
		explicit secure_bytes(const std::size_t aSize)
			: Bytes(aSize)
		{
		}

		~secure_bytes() noexcept
		{
			reset();
		}

		secure_bytes(const secure_bytes&)                    = delete;
		auto operator=(const secure_bytes&) -> secure_bytes& = delete;
		secure_bytes(secure_bytes&&) noexcept                = default;
		auto operator=(secure_bytes&& aOther) noexcept -> secure_bytes&
		{
			if (this != &aOther) {
				reset();
				Bytes = std::move(aOther.Bytes);
			}
			return *this;
		}

		[[nodiscard]] auto mutable_bytes() noexcept -> std::span<std::byte>
		{
			return Bytes;
		}

		[[nodiscard]] auto bytes() const noexcept
			-> std::span<const std::byte>
		{
			return Bytes;
		}

	private:
		void reset() noexcept
		{
			erase(Bytes);
			Bytes.clear();
		}

		std::vector<std::byte> Bytes;
	};

	class file_descriptor final
	{
	public:
		file_descriptor() = default;
		explicit file_descriptor(const int aDescriptor) noexcept
			: Descriptor{ aDescriptor }
		{
		}

		~file_descriptor() noexcept
		{
			reset();
		}

		file_descriptor(const file_descriptor&)                    = delete;
		auto operator=(const file_descriptor&) -> file_descriptor& = delete;

		file_descriptor(file_descriptor&& aOther) noexcept
			: Descriptor{ std::exchange(aOther.Descriptor, -1) }
		{
		}

		auto operator=(file_descriptor&& aOther) noexcept -> file_descriptor&
		{
			if (this != &aOther) {
				reset();
				Descriptor = std::exchange(aOther.Descriptor, -1);
			}
			return *this;
		}

		[[nodiscard]] auto get() const noexcept -> int
		{
			return Descriptor;
		}

		[[nodiscard]] auto valid() const noexcept -> bool
		{
			return Descriptor >= 0;
		}

		void reset(const int aDescriptor = -1) noexcept
		{
			if (valid()) {
				static_cast<void>(::close(Descriptor));
			}
			Descriptor = aDescriptor;
		}

	private:
		int Descriptor = -1;
	};

	class audit_directory final
	{
	public:
		audit_directory() = default;
		~audit_directory() noexcept
		{
			reset();
		}

		audit_directory(const audit_directory&)                    = delete;
		auto operator=(const audit_directory&) -> audit_directory& = delete;

		audit_directory(audit_directory&& aOther) noexcept
			: Path{ std::move(aOther.Path) }
		{
			aOther.Path.clear();
		}

		auto operator=(audit_directory&& aOther) noexcept
			-> audit_directory&
		{
			if (this != &aOther) {
				reset();
				Path = std::move(aOther.Path);
				aOther.Path.clear();
			}
			return *this;
		}

		[[nodiscard]] static auto create()
			-> std::optional<audit_directory>
		{
			std::error_code temporaryError;
			const auto      temporaryRoot = std::filesystem::temp_directory_path(temporaryError);
			if (temporaryError) {
				return std::nullopt;
			}

			auto pattern        = (temporaryRoot
								   / "betterpullreview-secret-service-audit-XXXXXX")
									  .string();
			auto mutablePattern = std::vector<char>{ pattern.begin(), pattern.end() };
			mutablePattern.push_back('\0');
			const auto* created = ::mkdtemp(mutablePattern.data());
			if (created == nullptr) {
				return std::nullopt;
			}

			audit_directory directory;
			directory.Path = created;
			for (const auto* child : {
					 "home",
					 "tmp",
					 "config",
					 "data",
					 "cache",
				 }) {
				std::error_code error;
				if (
					!std::filesystem::create_directory(
						directory.Path / child,
						error)
					|| error) {
					return std::nullopt;
				}
			}
			return directory;
		}

		[[nodiscard]] auto path() const noexcept
			-> const std::filesystem::path&
		{
			return Path;
		}

		[[nodiscard]] auto remove() noexcept -> bool
		{
			if (Path.empty()) {
				return true;
			}

			std::error_code error;
			static_cast<void>(std::filesystem::remove_all(Path, error));
			if (error) {
				return false;
			}
			const auto remains = std::filesystem::exists(Path, error);
			if (error || remains) {
				return false;
			}
			Path.clear();
			return true;
		}

		void abandon() noexcept
		{
			// A child that is not confirmed stopped may still use this exact
			// directory. Preserve it for deliberate cleanup instead of removing
			// storage beneath a live process.
			Path.clear();
		}

	private:
		void reset() noexcept
		{
			if (!Path.empty()) {
				std::error_code ignored;
				std::filesystem::remove_all(Path, ignored);
				Path.clear();
			}
		}

		std::filesystem::path Path;
	};

	template <typename Completion>
	struct terminal_recorder
	{
		std::size_t               Calls = 0U;
		std::optional<Completion> Terminal;

		void record(Completion aCompletion)
		{
			++Calls;
			Terminal.emplace(std::move(aCompletion));
		}
	};

	using store_recorder    = terminal_recorder<application::github_credential_store_completion>;
	using retrieve_recorder = terminal_recorder<application::github_credential_retrieve_completion>;
	using delete_recorder   = terminal_recorder<application::github_credential_delete_completion>;

	template <typename Terminal, typename Completion>
	[[nodiscard]] auto terminal_as(const std::optional<Completion>& aCompletion)
		-> const Terminal*
	{
		return aCompletion.has_value()
			? std::get_if<Terminal>(&*aCompletion)
			: nullptr;
	}

	[[nodiscard]] auto native_authority()
		-> application::github_credential_authority_key
	{
		return authority(
			0x4250522400000011ULL,
			0x4250522400000012ULL,
			0x4250522400000013ULL,
			0x4250522400000014ULL);
	}

	[[nodiscard]] auto native_context(const std::uint64_t aRequest)
		-> application::github_credential_operation_context
	{
		return context(
			native_authority(),
			aRequest,
			native_session,
			native_generation);
	}

	void write_fixed_diagnostic(
		const std::string_view aCategory) noexcept
	{
		auto remaining = aCategory;
		while (!remaining.empty()) {
			const auto written = ::write(STDERR_FILENO, remaining.data(), remaining.size());
			if (written < 0 && errno == EINTR) {
				continue;
			}
			if (written <= 0) {
				return;
			}
			remaining.remove_prefix(static_cast<std::size_t>(written));
		}
	}

	/// @brief Which of the vault's two slots a native helper drives.
	///
	/// The refresh slot is a second store behind the same port, so every
	/// lifecycle question the credential slot answers -- stored, read back,
	/// absent, deleted -- has to be asked of it separately. Parameterizing the
	/// helpers rather than copying them keeps the two slots answering the same
	/// questions, so a slot that quietly stopped answering one of them shows up
	/// as a failure here instead of as a case nobody wrote for it.
	enum class vault_slot : std::uint8_t {
		credential,
		refresh,
	};

	[[nodiscard]] auto run_store(
		application::github_credential_vault_port& aVault,
		application::manual_executor&              aLane,
		const std::uint64_t                        aRequest,
		const std::span<const std::byte>           aBytes,
		const vault_slot                           aSlot = vault_slot::credential)
		-> bool
	{
		auto material = application::github_credential_material::create(aBytes);
		if (!material.has_value()) {
			return false;
		}

		const auto     operation = native_context(aRequest);
		store_recorder recorder;
		auto           enrollment = application::github_credential_enrollment{
			.Context    = operation,
			.Credential = std::move(*material),
		};
		auto handler = [&recorder](
						   application::github_credential_store_completion aTerminal) {
			recorder.record(std::move(aTerminal));
		};
		if (aSlot == vault_slot::refresh) {
			aVault.store_refresh(std::move(enrollment), handler);
		} else {
			aVault.store(std::move(enrollment), handler);
		}
		if (aLane.pending() != 1U) {
			aVault.cancel(operation);
			aLane.drain();
			return false;
		}
		aLane.run_next();

		const auto* success = terminal_as<
			application::github_credential_store_success>(recorder.Terminal);
		return recorder.Calls == 1U && success != nullptr
			&& success->Context == operation
			&& success->Disposition
			== application::github_credential_store_disposition::stored;
	}

	[[nodiscard]] auto run_retrieve(
		application::github_credential_vault_port& aVault,
		application::manual_executor&              aLane,
		const std::uint64_t                        aRequest,
		const std::span<const std::byte>           aExpected,
		const vault_slot                           aSlot = vault_slot::credential)
		-> bool
	{
		const auto        operation = native_context(aRequest);
		retrieve_recorder recorder;
		auto              handler = [&recorder](
										application::github_credential_retrieve_completion aTerminal) {
			recorder.record(std::move(aTerminal));
		};
		if (aSlot == vault_slot::refresh) {
			aVault.retrieve_refresh(operation, handler);
		} else {
			aVault.retrieve(operation, handler);
		}
		if (aLane.pending() != 1U) {
			aVault.cancel(operation);
			aLane.drain();
			return false;
		}
		aLane.run_next();

		const auto* success = terminal_as<
			application::github_credential_retrieve_success>(
			recorder.Terminal);
		if (recorder.Calls != 1U) {
			write_fixed_diagnostic(
				"linux-secret-service-retrieve:terminal-count\n");
			return false;
		}
		if (success == nullptr) {
			const auto* error = terminal_as<
				application::github_credential_retrieve_error>(
				recorder.Terminal);
			if (error == nullptr) {
				write_fixed_diagnostic(
					"linux-secret-service-retrieve:unexpected-terminal\n");
				return false;
			}
			switch (error->Failure) {
			case application::github_credential_vault_failure::none:
				write_fixed_diagnostic(
					"linux-secret-service-retrieve:failure-none\n");
				break;
			case application::github_credential_vault_failure::locked:
				write_fixed_diagnostic(
					"linux-secret-service-retrieve:locked\n");
				break;
			case application::github_credential_vault_failure::unavailable:
				write_fixed_diagnostic(
					"linux-secret-service-retrieve:unavailable\n");
				break;
			case application::github_credential_vault_failure::access_denied:
				write_fixed_diagnostic(
					"linux-secret-service-retrieve:access-denied\n");
				break;
			case application::github_credential_vault_failure::corrupt:
				write_fixed_diagnostic(
					"linux-secret-service-retrieve:corrupt\n");
				break;
			case application::github_credential_vault_failure::
				allocation_failure:
				write_fixed_diagnostic(
					"linux-secret-service-retrieve:allocation-failure\n");
				break;
			case application::github_credential_vault_failure::invalid_request:
				write_fixed_diagnostic(
					"linux-secret-service-retrieve:invalid-request\n");
				break;
			case application::github_credential_vault_failure::operation_failed:
				write_fixed_diagnostic(
					"linux-secret-service-retrieve:operation-failed\n");
				break;
			}
			return false;
		}
		if (success->Context != operation) {
			write_fixed_diagnostic(
				"linux-secret-service-retrieve:context\n");
			return false;
		}
		if (!same_bytes(success->Credential.bytes(), aExpected)) {
			write_fixed_diagnostic(
				"linux-secret-service-retrieve:bytes\n");
			return false;
		}
		return true;
	}

	[[nodiscard]] auto run_retrieve_absent(
		application::github_credential_vault_port& aVault,
		application::manual_executor&              aLane,
		const std::uint64_t                        aRequest,
		const vault_slot                           aSlot = vault_slot::credential)
		-> bool
	{
		const auto        operation = native_context(aRequest);
		retrieve_recorder recorder;
		auto              handler = [&recorder](
										application::github_credential_retrieve_completion aTerminal) {
			recorder.record(std::move(aTerminal));
		};
		if (aSlot == vault_slot::refresh) {
			aVault.retrieve_refresh(operation, handler);
		} else {
			aVault.retrieve(operation, handler);
		}
		if (aLane.pending() != 1U) {
			aVault.cancel(operation);
			aLane.drain();
			return false;
		}
		aLane.run_next();

		const auto* absent = terminal_as<
			application::github_credential_retrieve_absent>(
			recorder.Terminal);
		return recorder.Calls == 1U && absent != nullptr
			&& absent->Context == operation;
	}

	[[nodiscard]] auto run_delete(
		application::github_credential_vault_port& aVault,
		application::manual_executor&              aLane,
		const std::uint64_t                        aRequest,
		const std::optional<
			application::github_credential_delete_disposition>
						 aExpected,
		const vault_slot aSlot = vault_slot::credential) -> bool
	{
		const auto      operation = native_context(aRequest);
		delete_recorder recorder;
		auto            handler = [&recorder](
									  application::github_credential_delete_completion aTerminal) {
			recorder.record(std::move(aTerminal));
		};
		if (aSlot == vault_slot::refresh) {
			aVault.delete_refresh(operation, handler);
		} else {
			aVault.delete_credential(operation, handler);
		}
		if (aLane.pending() != 1U) {
			aVault.cancel(operation);
			aLane.drain();
			return false;
		}
		aLane.run_next();

		const auto* success = terminal_as<
			application::github_credential_delete_success>(
			recorder.Terminal);
		return recorder.Calls == 1U && success != nullptr
			&& success->Context == operation
			&& (!aExpected.has_value()
				|| success->Disposition == *aExpected);
	}

	[[nodiscard]] auto make_native_vault(
		application::manual_executor& aLane)
		-> std::unique_ptr<application::github_credential_vault_port>
	{
		auto vault = make_linux_secret_service_integration_test_vault(aLane);
		return vault.has_value() ? std::move(*vault) : nullptr;
	}

	[[nodiscard]] auto ensure_native_item_absent(
		const std::uint64_t aRequestBase) -> bool
	{
		application::manual_executor lane;
		auto                         vault = make_native_vault(lane);
		if (vault == nullptr) {
			return false;
		}

		const auto deletion = run_delete(*vault, lane, aRequestBase, std::nullopt);
		const auto absence  = run_retrieve_absent(*vault, lane, aRequestBase + 1U);
		return deletion && absence;
	}

	void record_lifecycle_result(
		const bool             aResult,
		const std::string_view aCategory,
		bool&                  aAggregate) noexcept
	{
		if (!aResult) {
			write_fixed_diagnostic(aCategory);
		}
		aAggregate = aResult && aAggregate;
	}

	[[nodiscard]] auto run_native_lifecycle(
		const std::span<const std::byte> aCanaryA,
		const std::span<const std::byte> aCanaryB) -> bool
	{
		application::manual_executor lane;
		auto                         vault = make_native_vault(lane);
		if (vault == nullptr) {
			return false;
		}

		auto ok = true;
		record_lifecycle_result(
			run_delete(*vault, lane, 100U, std::nullopt),
			"linux-secret-service-lifecycle:initial-delete\n",
			ok);
		record_lifecycle_result(
			run_retrieve_absent(*vault, lane, 101U),
			"linux-secret-service-lifecycle:initial-absence\n",
			ok);
		record_lifecycle_result(
			run_store(*vault, lane, 102U, aCanaryA),
			"linux-secret-service-lifecycle:first-store\n",
			ok);
		record_lifecycle_result(
			run_retrieve(*vault, lane, 103U, aCanaryA),
			"linux-secret-service-lifecycle:first-retrieve\n",
			ok);
		record_lifecycle_result(
			run_store(*vault, lane, 104U, aCanaryB),
			"linux-secret-service-lifecycle:replacement-store\n",
			ok);
		record_lifecycle_result(
			run_retrieve(*vault, lane, 105U, aCanaryB),
			"linux-secret-service-lifecycle:replacement-retrieve\n",
			ok);
		record_lifecycle_result(
			run_delete(
				*vault,
				lane,
				106U,
				application::github_credential_delete_disposition::deleted),
			"linux-secret-service-lifecycle:delete\n",
			ok);
		record_lifecycle_result(
			run_retrieve_absent(*vault, lane, 107U),
			"linux-secret-service-lifecycle:post-delete-absence\n",
			ok);
		record_lifecycle_result(
			run_delete(
				*vault,
				lane,
				108U,
				application::github_credential_delete_disposition::absent),
			"linux-secret-service-lifecycle:repeat-delete\n",
			ok);

		// [[BPR-0542]] The refresh slot, against the same real collection. The
		// order matters: the base credential is stored again first, so the two
		// slots hold different bytes under the *same* authority at the same
		// moment. That is the only arrangement in which "the refresh read
		// answered from the refresh slot" is a claim rather than a coincidence
		// -- with one slot empty, a single shared slot would pass every check
		// below.
		record_lifecycle_result(
			run_store(*vault, lane, 120U, aCanaryA),
			"linux-secret-service-refresh:base-store\n",
			ok);
		record_lifecycle_result(
			run_retrieve_absent(*vault, lane, 121U, vault_slot::refresh),
			"linux-secret-service-refresh:initial-absence\n",
			ok);
		record_lifecycle_result(
			run_store(*vault, lane, 122U, aCanaryB, vault_slot::refresh),
			"linux-secret-service-refresh:store\n",
			ok);
		record_lifecycle_result(
			run_retrieve(*vault, lane, 123U, aCanaryB, vault_slot::refresh),
			"linux-secret-service-refresh:retrieve\n",
			ok);
		// The cut that makes the two slots two: the base slot still answers
		// its own bytes, not the refresh bytes just written beside it.
		record_lifecycle_result(
			run_retrieve(*vault, lane, 124U, aCanaryA),
			"linux-secret-service-refresh:base-unshadowed\n",
			ok);
		record_lifecycle_result(
			run_delete(
				*vault,
				lane,
				125U,
				application::github_credential_delete_disposition::deleted,
				vault_slot::refresh),
			"linux-secret-service-refresh:delete\n",
			ok);
		record_lifecycle_result(
			run_retrieve_absent(*vault, lane, 126U, vault_slot::refresh),
			"linux-secret-service-refresh:post-delete-absence\n",
			ok);
		// Erasure honesty, the row [[BPR-0540]] depends on: deleting a refresh
		// entry that is not there is a success reporting `absent`, never
		// `unavailable`. An `unavailable` here would let an orderly shutdown
		// treat "I could not reach the store" as "there was nothing to erase".
		record_lifecycle_result(
			run_delete(
				*vault,
				lane,
				127U,
				application::github_credential_delete_disposition::absent,
				vault_slot::refresh),
			"linux-secret-service-refresh:repeat-delete\n",
			ok);
		// And the base credential survived the refresh slot's whole lifecycle.
		record_lifecycle_result(
			run_retrieve(*vault, lane, 128U, aCanaryA),
			"linux-secret-service-refresh:base-survives\n",
			ok);
		record_lifecycle_result(
			run_delete(*vault, lane, 129U, std::nullopt),
			"linux-secret-service-refresh:base-cleanup\n",
			ok);

		const auto cleanup            = run_delete(*vault, lane, 109U, std::nullopt);
		const auto postCleanupAbsence = run_retrieve_absent(*vault, lane, 110U);
		const auto refreshCleanup     = run_delete(
			*vault,
			lane,
			130U,
			std::nullopt,
			vault_slot::refresh);
		record_lifecycle_result(
			refreshCleanup,
			"linux-secret-service-refresh:cleanup\n",
			ok);
		record_lifecycle_result(
			cleanup,
			"linux-secret-service-lifecycle:cleanup\n",
			ok);
		record_lifecycle_result(
			postCleanupAbsence,
			"linux-secret-service-lifecycle:cleanup-absence\n",
			ok);
		return ok && cleanup && postCleanupAbsence && refreshCleanup;
	}

	[[nodiscard]] auto read_exact(
		const int                  aDescriptor,
		const std::span<std::byte> aOutput) -> bool
	{
		auto offset = std::size_t{ 0U };
		while (offset < aOutput.size()) {
			const auto count = ::read(
				aDescriptor,
				aOutput.data() + offset,
				aOutput.size() - offset);
			if (count > 0) {
				offset += static_cast<std::size_t>(count);
				continue;
			}
			if (count < 0 && errno == EINTR) {
				continue;
			}
			return false;
		}
		return true;
	}

	[[nodiscard]] auto write_exact(
		const int                        aDescriptor,
		const std::span<const std::byte> aInput) -> bool
	{
		auto offset = std::size_t{ 0U };
		while (offset < aInput.size()) {
			const auto count = ::write(
				aDescriptor,
				aInput.data() + offset,
				aInput.size() - offset);
			if (count > 0) {
				offset += static_cast<std::size_t>(count);
				continue;
			}
			if (count < 0 && errno == EINTR) {
				continue;
			}
			return false;
		}
		return true;
	}

	[[nodiscard]] auto read_frame(const int aDescriptor)
		-> std::optional<secure_bytes>
	{
		std::array<std::byte, 4U> header{ };
		if (!read_exact(aDescriptor, header)) {
			return std::nullopt;
		}
		const auto length = (static_cast<std::uint32_t>(
								 std::to_integer<std::uint8_t>(header[0]))
							 << 24U)
			| (static_cast<std::uint32_t>(
				   std::to_integer<std::uint8_t>(header[1]))
			   << 16U)
			| (static_cast<std::uint32_t>(
				   std::to_integer<std::uint8_t>(header[2]))
			   << 8U)
			| static_cast<std::uint32_t>(
								std::to_integer<std::uint8_t>(header[3]));
		if (
			length == 0U
			|| length > application::maximum_github_credential_bytes) {
			return std::nullopt;
		}

		secure_bytes bytes{ length };
		if (!read_exact(aDescriptor, bytes.mutable_bytes())) {
			return std::nullopt;
		}
		return bytes;
	}

	[[nodiscard]] auto write_frame(
		const int                        aDescriptor,
		const std::span<const std::byte> aBytes) -> bool
	{
		if (
			aBytes.empty()
			|| aBytes.size() > application::maximum_github_credential_bytes) {
			return false;
		}
		const auto       length = static_cast<std::uint32_t>(aBytes.size());
		const std::array header{
			static_cast<std::byte>((length >> 24U) & 0xFFU),
			static_cast<std::byte>((length >> 16U) & 0xFFU),
			static_cast<std::byte>((length >> 8U) & 0xFFU),
			static_cast<std::byte>(length & 0xFFU),
		};
		return write_exact(aDescriptor, header)
			&& write_exact(aDescriptor, aBytes);
	}

	[[nodiscard]] auto printable(
		const std::span<const std::byte> aBytes) -> bool
	{
		return std::ranges::all_of(aBytes, [](const std::byte aByte) {
			const auto value = std::to_integer<unsigned int>(aByte);
			return value >= 0x21U && value <= 0x7EU;
		});
	}

	[[nodiscard]] auto fill_random(
		const std::span<std::byte> aBytes) -> bool
	{
		auto offset = std::size_t{ 0U };
		while (offset < aBytes.size()) {
			const auto count = ::getrandom(
				aBytes.data() + offset,
				aBytes.size() - offset,
				0U);
			if (count > 0) {
				offset += static_cast<std::size_t>(count);
				continue;
			}
			if (count < 0 && errno == EINTR) {
				continue;
			}
			return false;
		}
		return true;
	}

	[[nodiscard]] auto random_printable_bytes(const std::size_t aSize)
		-> std::optional<secure_bytes>
	{
		secure_bytes bytes{ aSize };
		if (!fill_random(bytes.mutable_bytes())) {
			return std::nullopt;
		}
		for (auto& byte : bytes.mutable_bytes()) {
			const auto random = std::to_integer<unsigned int>(byte);
			byte              = static_cast<std::byte>(0x21U + (random % 94U));
		}
		return bytes;
	}

	[[nodiscard]] auto opaque_bytes(
		const std::size_t aSize,
		const std::size_t aSalt) -> secure_bytes
	{
		secure_bytes bytes{ aSize };
		auto         output = bytes.mutable_bytes();
		for (std::size_t index = 0U; index < output.size(); ++index) {
			output[index] = static_cast<std::byte>(
				static_cast<std::uint8_t>(
					((index * 197U) + aSalt) & 0xFFU));
		}

		constexpr std::array requiredPrefix{
			std::byte{ 0x00U },
			std::byte{ 0xFFU },
			std::byte{ 0xC0U },
			std::byte{ 0xAFU },
			std::byte{ 0x80U },
			std::byte{ 0xFEU },
		};
		std::ranges::copy(requiredPrefix, output.begin());
		return bytes;
	}

	[[nodiscard]] auto run_expected_store_failure(
		const application::github_credential_vault_failure aExpected,
		const std::uint64_t                                aRequest) -> bool
	{
		auto canary = random_printable_bytes(native_canary_a_bytes);
		if (!canary.has_value()) {
			return false;
		}

		application::manual_executor lane;
		auto                         factory = make_linux_secret_service_integration_test_vault(lane);
		if (!factory.has_value()) {
			return factory.error() == aExpected;
		}
		auto       vault     = std::move(*factory);
		const auto operation = native_context(aRequest);
		auto       material  = application::github_credential_material::create(
			canary->bytes());
		if (!material.has_value()) {
			return false;
		}

		store_recorder recorder;
		vault->store(
			application::github_credential_enrollment{
				.Context    = operation,
				.Credential = std::move(*material),
			},
			[&recorder](
				application::github_credential_store_completion aTerminal) {
				recorder.record(std::move(aTerminal));
			});
		if (lane.pending() != 1U) {
			vault->cancel(operation);
			lane.drain();
			return false;
		}
		lane.run_next();

		const auto* error = terminal_as<
			application::github_credential_store_error>(recorder.Terminal);
		if (
			recorder.Calls == 1U && error != nullptr
			&& error->Context == operation && error->Failure == aExpected) {
			return true;
		}

		// An unexpected success must not leave the exact integration item.
		static_cast<void>(
			run_delete(*vault, lane, aRequest + 1U, std::nullopt));
		return false;
	}

	[[nodiscard]] auto run_bus_redirect_rejection() -> bool
	{
		constexpr std::string_view hostileBusName = "org.betterpullreview.Hostile.Secret";
		const auto*                configured     = std::getenv("SECRET_SERVICE_BUS_NAME");
		if (
			configured == nullptr
			|| std::string_view{ configured } != hostileBusName) {
			return false;
		}

		return run_expected_store_failure(
			application::github_credential_vault_failure::unavailable,
			5'000U);
	}

	[[nodiscard]] auto write_category(
		const std::string_view aCategory) -> bool
	{
		return write_exact(
			STDOUT_FILENO,
			std::as_bytes(std::span{ aCategory }));
	}

	void report_audit_failure(
		const bool             aCondition,
		const std::string_view aCategory) noexcept
	{
		if (!aCondition) {
			static_cast<void>(write_exact(
				STDERR_FILENO,
				std::as_bytes(std::span{ aCategory })));
		}
	}

	[[nodiscard]] auto native_child() -> int
	{
		auto       canaryA = read_frame(STDIN_FILENO);
		auto       canaryB = read_frame(STDIN_FILENO);
		std::byte  extra{ };
		const auto trailing   = ::read(STDIN_FILENO, &extra, 1U);
		const auto inputValid = canaryA.has_value() && canaryB.has_value()
			&& canaryA->bytes().size() == native_canary_a_bytes
			&& canaryB->bytes().size() == native_canary_b_bytes
			&& !same_bytes(canaryA->bytes(), canaryB->bytes())
			&& printable(canaryA->bytes())
			&& printable(canaryB->bytes()) && trailing == 0;

		constexpr std::array      ready{ std::byte{ 0x52U } };
		std::array<std::byte, 1U> control{ };
		const auto                synchronized = inputValid
			&& write_exact(child_ready_descriptor, ready)
			&& read_exact(child_control_descriptor, control)
			&& control[0] == std::byte{ 0x47U };
		auto       opaqueA     = opaque_bytes(native_canary_a_bytes, 17U);
		auto       opaqueB     = opaque_bytes(native_canary_b_bytes, 41U);
		const auto opaqueShape = !printable(opaqueA.bytes()) && !printable(opaqueB.bytes())
			&& opaqueA.bytes().front() == std::byte{ 0x00U }
			&& opaqueA.bytes()[1U] == std::byte{ 0xFFU }
			&& opaqueA.bytes()[2U] == std::byte{ 0xC0U }
			&& opaqueA.bytes()[3U] == std::byte{ 0xAFU }
			&& opaqueA.bytes()[4U] == std::byte{ 0x80U }
			&& opaqueB.bytes().front() == std::byte{ 0x00U }
			&& opaqueB.bytes()[1U] == std::byte{ 0xFFU };
		const auto passed = synchronized
			&& run_native_lifecycle(
								canaryA->bytes(),
								canaryB->bytes())
			&& opaqueShape
			&& run_native_lifecycle(opaqueA.bytes(), opaqueB.bytes());
		static_cast<void>(write_category(
			passed ? "linux-secret-service-native:pass\n"
				   : "linux-secret-service-native:fail\n"));
		return passed ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	[[nodiscard]] auto string_contains(
		const std::string_view aText,
		const leak_probes      aProbes) -> bool
	{
		return std::ranges::any_of(aProbes, [aText](const leak_probe aProbe) {
			if (aProbe.empty() || aText.size() < aProbe.size()) {
				return false;
			}
			return std::search(
					   aText.begin(),
					   aText.end(),
					   aProbe.begin(),
					   aProbe.end(),
					   [](const char aCharacter, const std::byte aByte) {
						   return static_cast<unsigned char>(aCharacter)
							   == std::to_integer<unsigned char>(aByte);
					   })
				!= aText.end();
		});
	}

	[[nodiscard]] auto read_regular_file(
		const std::filesystem::path& aPath)
		-> std::optional<secure_bytes>
	{
		file_descriptor input{
			::open(aPath.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW)
		};
		if (!input.valid()) {
			return std::nullopt;
		}

		struct stat information{ };
		if (
			::fstat(input.get(), &information) != 0
			|| !S_ISREG(information.st_mode) || information.st_size < 0
			|| static_cast<std::uint64_t>(information.st_size)
				> maximum_audited_file_bytes) {
			return std::nullopt;
		}

		secure_bytes bytes{
			static_cast<std::size_t>(information.st_size)
		};
		if (!read_exact(input.get(), bytes.mutable_bytes())) {
			return std::nullopt;
		}
		return bytes;
	}

	[[nodiscard]] auto read_bounded_stream(
		const std::filesystem::path& aPath,
		const std::size_t            aMaximumBytes)
		-> std::optional<secure_bytes>
	{
		file_descriptor input{
			::open(aPath.c_str(), O_RDONLY | O_CLOEXEC)
		};
		if (!input.valid()) {
			return std::nullopt;
		}

		std::vector<std::byte> accumulated;
		try {
			accumulated.reserve(std::min(aMaximumBytes, std::size_t{ 4'096U }));
			std::array<std::byte, 4'096U> block{ };
			while (true) {
				const auto count = ::read(input.get(), block.data(), block.size());
				if (count > 0) {
					const auto received = static_cast<std::size_t>(count);
					if (received > aMaximumBytes - accumulated.size()) {
						erase(block);
						erase(accumulated);
						return std::nullopt;
					}
					accumulated.insert(
						accumulated.end(),
						block.begin(),
						block.begin()
							+ static_cast<std::ptrdiff_t>(received));
					erase(block);
					continue;
				}
				erase(block);
				if (count == 0) {
					break;
				}
				if (errno == EINTR) {
					continue;
				}
				erase(accumulated);
				return std::nullopt;
			}
		} catch (...) {
			erase(accumulated);
			return std::nullopt;
		}

		secure_bytes bytes{ accumulated.size() };
		std::ranges::copy(accumulated, bytes.mutable_bytes().begin());
		erase(accumulated);
		return bytes;
	}

	[[nodiscard]] auto probes_absent(
		const std::span<const std::byte> aBytes,
		const leak_probes                aProbes) -> bool
	{
		return std::ranges::none_of(aProbes, [aBytes](const leak_probe aProbe) {
			return std::ranges::search(aBytes, aProbe).begin() != aBytes.end();
		});
	}

	[[nodiscard]] auto probes_absent_from_file(
		const std::filesystem::path& aPath,
		const leak_probes            aProbes) -> bool
	{
		auto bytes = read_regular_file(aPath);
		return bytes.has_value()
			&& probes_absent(bytes->bytes(), aProbes);
	}

	[[nodiscard]] auto probes_absent_from_stream(
		const std::filesystem::path& aPath,
		const leak_probes            aProbes) -> bool
	{
		auto bytes = read_bounded_stream(aPath, 1024U * 1024U);
		return bytes.has_value()
			&& probes_absent(bytes->bytes(), aProbes);
	}

	[[nodiscard]] auto exact_file_text(
		const std::filesystem::path& aPath,
		const std::string_view       aExpected) -> bool
	{
		auto bytes = read_regular_file(aPath);
		return bytes.has_value()
			&& bytes->bytes().size() == aExpected.size()
			&& std::ranges::equal(
				   bytes->bytes(),
				   std::as_bytes(std::span{ aExpected }));
	}

	[[nodiscard]] auto file_contains_text(
		const std::filesystem::path& aPath,
		const std::string_view       aExpected) -> bool
	{
		auto bytes = read_regular_file(aPath);
		if (!bytes.has_value()) {
			return false;
		}
		const auto expectedBytes = std::as_bytes(std::span{ aExpected });
		return std::ranges::search(
				   bytes->bytes(), expectedBytes)
				   .begin()
			!= bytes->bytes().end();
	}

	[[nodiscard]] auto probes_absent_from_tree(
		const std::filesystem::path& aRoot,
		const leak_probes            aProbes) -> bool
	{
		std::error_code error;
		auto            iterator = std::filesystem::recursive_directory_iterator{
			aRoot,
			std::filesystem::directory_options::none,
			error
		};
		const auto end = std::filesystem::recursive_directory_iterator{ };
		if (error) {
			return false;
		}
		while (iterator != end) {
			const auto entry = *iterator;
			iterator.increment(error);
			if (error) {
				return false;
			}
			if (entry.is_regular_file(error)) {
				if (
					error
					|| !probes_absent_from_file(entry.path(), aProbes)) {
					return false;
				}
			} else if (error) {
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] auto environment_value(
		const std::string_view aName) -> std::optional<std::string>
	{
		const auto* value = std::getenv(std::string{ aName }.c_str());
		if (value == nullptr || *value == '\0') {
			return std::nullopt;
		}
		return std::string{ value };
	}

	[[nodiscard]] auto make_child_environment(
		const std::filesystem::path& aRoot)
		-> std::optional<std::vector<std::string>>
	{
		const auto sessionBus = environment_value("DBUS_SESSION_BUS_ADDRESS");
		if (!sessionBus.has_value()) {
			return std::nullopt;
		}

		std::vector<std::string> environment{
			"DBUS_SESSION_BUS_ADDRESS=" + *sessionBus,
			"HOME=" + (aRoot / "home").string(),
			"LANG=C",
			"PATH=/usr/bin:/bin",
			"TMPDIR=" + (aRoot / "tmp").string(),
			"XDG_CACHE_HOME=" + (aRoot / "cache").string(),
			"XDG_CONFIG_HOME=" + (aRoot / "config").string(),
			"XDG_DATA_HOME=" + (aRoot / "data").string(),
		};
		if (const auto runtime = environment_value("XDG_RUNTIME_DIR");
			runtime.has_value()) {
			environment.push_back("XDG_RUNTIME_DIR=" + *runtime);
		}
		std::ranges::sort(environment);
		return environment;
	}

	[[nodiscard]] auto make_pipe()
		-> std::optional<std::array<file_descriptor, 2U>>
	{
		std::array<int, 2U> raw{ -1, -1 };
		if (::pipe2(raw.data(), O_CLOEXEC) != 0) {
			return std::nullopt;
		}
		return std::array{
			file_descriptor{ raw[0] },
			file_descriptor{ raw[1] },
		};
	}

	[[nodiscard]] auto add_dup(
		posix_spawn_file_actions_t& aActions,
		const int                   aSource,
		const int                   aDestination) -> bool
	{
		return posix_spawn_file_actions_adddup2(
				   &aActions,
				   aSource,
				   aDestination)
			== 0;
	}

	[[nodiscard]] auto add_close(
		posix_spawn_file_actions_t& aActions,
		const int                   aDescriptor) -> bool
	{
		return posix_spawn_file_actions_addclose(&aActions, aDescriptor) == 0;
	}

	[[nodiscard]] auto wait_until_readable(
		const int                       aDescriptor,
		const std::chrono::milliseconds aTimeout) -> bool
	{
		pollfd requested{
			.fd      = aDescriptor,
			.events  = POLLIN,
			.revents = 0,
		};
		while (true) {
			const auto result = ::poll(&requested, 1U, static_cast<int>(aTimeout.count()));
			if (result > 0) {
				return (requested.revents & POLLIN) != 0;
			}
			if (result < 0 && errno == EINTR) {
				continue;
			}
			return false;
		}
	}

	[[nodiscard]] auto wait_for_child(
		const pid_t                                 aChild,
		const std::chrono::steady_clock::time_point aDeadline,
		int&                                        aStatus) -> bool
	{
		while (std::chrono::steady_clock::now() < aDeadline) {
			const auto result = ::waitpid(aChild, &aStatus, WNOHANG);
			if (result == aChild) {
				return true;
			}
			if (result < 0 && errno == EINTR) {
				continue;
			}
			if (result < 0) {
				return false;
			}
			timespec delay{
				.tv_sec  = 0,
				.tv_nsec = 10'000'000L,
			};
			while (::nanosleep(&delay, &delay) != 0 && errno == EINTR) {
			}
		}
		return false;
	}

	[[nodiscard]] auto stop_and_reap_child(
		const pid_t aChild,
		int&        aStatus) -> bool
	{
		if (::kill(aChild, SIGKILL) != 0 && errno != ESRCH) {
			return false;
		}
		while (true) {
			const auto result = ::waitpid(aChild, &aStatus, 0);
			if (result == aChild) {
				return true;
			}
			if (result < 0 && errno == EINTR) {
				continue;
			}
			return result < 0 && errno == ECHILD;
		}
	}

	[[nodiscard]] auto audit_native_child(
		const int   aParentArgumentCount,
		char* const aParentArguments[]) -> bool
	{
		auto canaryA = random_printable_bytes(native_canary_a_bytes);
		auto canaryB = random_printable_bytes(native_canary_b_bytes);
		if (!canaryA.has_value() || !canaryB.has_value()) {
			return false;
		}
		while (same_bytes(canaryA->bytes(), canaryB->bytes())) {
			canaryB = random_printable_bytes(native_canary_b_bytes);
			if (!canaryB.has_value()) {
				return false;
			}
		}
		const std::array probes{
			canaryA->bytes().first(native_leak_probe_bytes),
			canaryA->bytes().last(native_leak_probe_bytes),
			canaryB->bytes().first(native_leak_probe_bytes),
			canaryB->bytes().last(native_leak_probe_bytes),
		};

		auto            directory = audit_directory::create();
		std::error_code executableError;
		const auto      executable = std::filesystem::canonical("/proc/self/exe", executableError);
		if (!directory.has_value() || executableError) {
			return false;
		}

		const auto      stdoutPath = directory->path() / "stdout.audit";
		const auto      stderrPath = directory->path() / "stderr.audit";
		file_descriptor stdoutFile{ ::open(
			stdoutPath.c_str(),
			O_CREAT | O_TRUNC | O_RDWR | O_CLOEXEC,
			S_IRUSR | S_IWUSR) };
		file_descriptor stderrFile{ ::open(
			stderrPath.c_str(),
			O_CREAT | O_TRUNC | O_RDWR | O_CLOEXEC,
			S_IRUSR | S_IWUSR) };
		auto            input       = make_pipe();
		auto            ready       = make_pipe();
		auto            control     = make_pipe();
		auto            environment = make_child_environment(directory->path());
		if (
			!stdoutFile.valid() || !stderrFile.valid()
			|| !input.has_value() || !ready.has_value()
			|| !control.has_value() || !environment.has_value()) {
			return false;
		}

		const std::array ownedDescriptors{
			stdoutFile.get(),
			stderrFile.get(),
			(*input)[0].get(),
			(*input)[1].get(),
			(*ready)[0].get(),
			(*ready)[1].get(),
			(*control)[0].get(),
			(*control)[1].get(),
		};
		if (std::ranges::any_of(ownedDescriptors, [](const int aDescriptor) {
				return aDescriptor <= STDERR_FILENO
					|| aDescriptor == child_ready_descriptor
					|| aDescriptor == child_control_descriptor;
			})) {
			return false;
		}

		posix_spawn_file_actions_t actions;
		if (posix_spawn_file_actions_init(&actions) != 0) {
			return false;
		}
		auto actionsValid = add_dup(actions, stdoutFile.get(), STDOUT_FILENO)
			&& add_dup(actions, stderrFile.get(), STDERR_FILENO)
			&& add_dup(actions, (*input)[0].get(), STDIN_FILENO)
			&& add_dup(
								actions,
								(*ready)[1].get(),
								child_ready_descriptor)
			&& add_dup(
								actions,
								(*control)[0].get(),
								child_control_descriptor);
		for (const auto descriptor : ownedDescriptors) {
			actionsValid = add_close(actions, descriptor) && actionsValid;
		}
		actionsValid = posix_spawn_file_actions_addchdir_np(
						   &actions,
						   directory->path().c_str())
				== 0
			&& actionsValid;

		std::vector<char*> environmentPointers;
		environmentPointers.reserve(environment->size() + 1U);
		for (auto& value : *environment) {
			environmentPointers.push_back(value.data());
		}
		environmentPointers.push_back(nullptr);

		auto                  executableText = executable.string();
		auto                  childMode      = std::string{ "--native-child" };
		std::array<char*, 3U> childArguments{
			executableText.data(),
			childMode.data(),
			nullptr,
		};

		auto auditOk = actionsValid;
		for (auto index = 0; index < aParentArgumentCount; ++index) {
			auditOk = !string_contains(aParentArguments[index], probes) && auditOk;
		}
		for (const auto* argument : childArguments) {
			if (argument != nullptr) {
				auditOk = !string_contains(argument, probes) && auditOk;
			}
		}
		for (const auto& value : *environment) {
			auditOk = !string_contains(value, probes) && auditOk;
		}

		pid_t      child       = -1;
		const auto spawnStatus = actionsValid
			? posix_spawn(
				  &child,
				  executableText.c_str(),
				  &actions,
				  nullptr,
				  childArguments.data(),
				  environmentPointers.data())
			: EINVAL;
		static_cast<void>(posix_spawn_file_actions_destroy(&actions));

		(*input)[0].reset();
		(*ready)[1].reset();
		(*control)[0].reset();
		stdoutFile.reset();
		stderrFile.reset();
		if (spawnStatus != 0) {
			return false;
		}

		const auto wrote = write_frame((*input)[1].get(), canaryA->bytes())
			&& write_frame((*input)[1].get(), canaryB->bytes());
		(*input)[1].reset();

		std::array<std::byte, 1U> readySignal{ };
		const auto                childReady = wait_until_readable(
												   (*ready)[0].get(),
												   std::chrono::seconds{ 5 })
			&& read_exact((*ready)[0].get(), readySignal)
			&& readySignal[0] == std::byte{ 0x52U };
		(*ready)[0].reset();

		const auto childProcessRoot = std::filesystem::path{ "/proc" }
			/ std::to_string(static_cast<long long>(child));
		const auto actualInputsClean = childReady
			&& probes_absent_from_stream(
										   childProcessRoot / "cmdline",
										   probes)
			&& probes_absent_from_stream(
										   childProcessRoot / "environ",
										   probes);
		constexpr std::array proceed{ std::byte{ 0x47U } };
		const auto           released = write_exact((*control)[1].get(), proceed);
		(*control)[1].reset();

		int        childStatus = 0;
		const auto waited      = wait_for_child(
			child,
			std::chrono::steady_clock::now()
				+ std::chrono::seconds{ 15 },
			childStatus);
		const auto processStopped = waited || stop_and_reap_child(child, childStatus);
		if (!processStopped) {
			directory->abandon();
			return false;
		}

		const auto parentCleanup  = ensure_native_item_absent(3'000U);
		const auto childExited    = waited && WIFEXITED(childStatus);
		const auto childSucceeded = childExited && WEXITSTATUS(childStatus) == EXIT_SUCCESS;
		const auto stdoutIsExact  = exact_file_text(
			stdoutPath,
			"linux-secret-service-native:pass\n");
		const auto stderrIsEmpty = exact_file_text(stderrPath, "");
		report_audit_failure(
			actionsValid,
			"linux-secret-service-audit:spawn-actions\n");
		report_audit_failure(
			wrote,
			"linux-secret-service-audit:frame-write\n");
		report_audit_failure(
			childReady,
			"linux-secret-service-audit:child-ready\n");
		report_audit_failure(
			actualInputsClean,
			"linux-secret-service-audit:process-input-scan\n");
		report_audit_failure(
			released,
			"linux-secret-service-audit:child-release\n");
		report_audit_failure(
			waited,
			"linux-secret-service-audit:child-timeout\n");
		report_audit_failure(
			childExited,
			"linux-secret-service-audit:child-exit\n");
		report_audit_failure(
			childSucceeded,
			"linux-secret-service-audit:child-lifecycle\n");
		report_audit_failure(
			stdoutIsExact,
			"linux-secret-service-audit:stdout\n");
		report_audit_failure(
			stderrIsEmpty,
			"linux-secret-service-audit:stderr\n");
		for (const auto category : {
				 std::string_view{
					 "linux-secret-service-lifecycle:initial-delete\n" },
				 std::string_view{
					 "linux-secret-service-lifecycle:initial-absence\n" },
				 std::string_view{
					 "linux-secret-service-lifecycle:first-store\n" },
				 std::string_view{
					 "linux-secret-service-lifecycle:first-retrieve\n" },
				 std::string_view{
					 "linux-secret-service-lifecycle:replacement-store\n" },
				 std::string_view{
					 "linux-secret-service-lifecycle:replacement-retrieve\n" },
				 std::string_view{
					 "linux-secret-service-lifecycle:delete\n" },
				 std::string_view{
					 "linux-secret-service-lifecycle:post-delete-absence\n" },
				 std::string_view{
					 "linux-secret-service-lifecycle:repeat-delete\n" },
				 std::string_view{
					 "linux-secret-service-lifecycle:cleanup\n" },
				 std::string_view{
					 "linux-secret-service-lifecycle:cleanup-absence\n" },
				 std::string_view{
					 "linux-secret-service-refresh:base-store\n" },
				 std::string_view{
					 "linux-secret-service-refresh:initial-absence\n" },
				 std::string_view{ "linux-secret-service-refresh:store\n" },
				 std::string_view{ "linux-secret-service-refresh:retrieve\n" },
				 std::string_view{
					 "linux-secret-service-refresh:base-unshadowed\n" },
				 std::string_view{ "linux-secret-service-refresh:delete\n" },
				 std::string_view{
					 "linux-secret-service-refresh:post-delete-absence\n" },
				 std::string_view{
					 "linux-secret-service-refresh:repeat-delete\n" },
				 std::string_view{
					 "linux-secret-service-refresh:base-survives\n" },
				 std::string_view{
					 "linux-secret-service-refresh:base-cleanup\n" },
				 std::string_view{ "linux-secret-service-refresh:cleanup\n" },
				 std::string_view{
					 "linux-secret-service-retrieve:terminal-count\n" },
				 std::string_view{
					 "linux-secret-service-retrieve:unexpected-terminal\n" },
				 std::string_view{
					 "linux-secret-service-retrieve:failure-none\n" },
				 std::string_view{
					 "linux-secret-service-retrieve:locked\n" },
				 std::string_view{
					 "linux-secret-service-retrieve:unavailable\n" },
				 std::string_view{
					 "linux-secret-service-retrieve:access-denied\n" },
				 std::string_view{
					 "linux-secret-service-retrieve:corrupt\n" },
				 std::string_view{
					 "linux-secret-service-retrieve:allocation-failure\n" },
				 std::string_view{
					 "linux-secret-service-retrieve:invalid-request\n" },
				 std::string_view{
					 "linux-secret-service-retrieve:operation-failed\n" },
				 std::string_view{
					 "linux-secret-service-retrieve:context\n" },
				 std::string_view{
					 "linux-secret-service-retrieve:bytes\n" },
			 }) {
			report_audit_failure(
				!file_contains_text(stderrPath, category),
				category);
		}
		report_audit_failure(
			parentCleanup,
			"linux-secret-service-audit:parent-cleanup\n");
		auditOk = wrote && childReady && actualInputsClean && released
			&& childSucceeded && stdoutIsExact && stderrIsEmpty
			&& parentCleanup && auditOk;

		std::error_code sourceError;
		const auto      source            = std::filesystem::canonical(__FILE__, sourceError);
		const auto      sourceResolved    = !sourceError;
		const auto      executableIsClean = probes_absent_from_file(executable, probes);
		const auto      sourceIsClean     = sourceResolved && probes_absent_from_file(source, probes);
		const auto      scratchIsClean    = probes_absent_from_tree(directory->path(), probes);
		report_audit_failure(
			sourceResolved,
			"linux-secret-service-audit:source-resolution\n");
		report_audit_failure(
			executableIsClean,
			"linux-secret-service-audit:executable-scan\n");
		report_audit_failure(
			sourceIsClean,
			"linux-secret-service-audit:source-scan\n");
		report_audit_failure(
			scratchIsClean,
			"linux-secret-service-audit:scratch-scan\n");
		auditOk = sourceResolved && executableIsClean && sourceIsClean
			&& scratchIsClean && auditOk;
		const auto directoryRemoved = directory->remove();
		report_audit_failure(
			directoryRemoved,
			"linux-secret-service-audit:scratch-cleanup\n");
		return directoryRemoved && auditOk;
	}

	[[nodiscard]] auto fixture_mode(const bool aSetup) -> int
	{
		const auto requestBase     = aSetup ? 1'000U : 2'000U;
		const auto passed          = ensure_native_item_absent(requestBase);
		const auto successCategory = aSetup ? "linux-secret-service-setup:pass\n"
											: "linux-secret-service-cleanup:pass\n";
		const auto failureCategory = aSetup ? "linux-secret-service-setup:fail\n"
											: "linux-secret-service-cleanup:fail\n";
		static_cast<void>(
			write_category(passed ? successCategory : failureCategory));
		return passed ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	[[nodiscard]] auto expected_failure_mode(
		const application::github_credential_vault_failure aExpected,
		const std::string_view                             aPassCategory,
		const std::string_view                             aFailCategory) -> int
	{
		const auto passed = run_expected_store_failure(aExpected, 4'000U);
		static_cast<void>(
			write_category(passed ? aPassCategory : aFailCategory));
		return passed ? EXIT_SUCCESS : EXIT_FAILURE;
	}
}

auto main(const int argument_count, char* arguments[]) -> int
{
	static_cast<void>(std::signal(SIGPIPE, SIG_IGN));
	if (
		argument_count == 2
		&& std::string_view{ arguments[1] } == "--native-child") {
		return native_child();
	}
	if (
		argument_count == 2
		&& std::string_view{ arguments[1] } == "--fixture-setup") {
		return fixture_mode(true);
	}
	if (
		argument_count == 2
		&& std::string_view{ arguments[1] } == "--fixture-cleanup") {
		return fixture_mode(false);
	}
	if (
		argument_count == 2
		&& std::string_view{ arguments[1] } == "--expect-unavailable") {
		const auto noExplicitSessionBus = std::getenv("DBUS_SESSION_BUS_ADDRESS") == nullptr;
		if (!noExplicitSessionBus) {
			static_cast<void>(write_category(
				"linux-secret-service-unavailable:fail\n"));
			return EXIT_FAILURE;
		}
		return expected_failure_mode(
			application::github_credential_vault_failure::unavailable,
			"linux-secret-service-unavailable:pass\n",
			"linux-secret-service-unavailable:fail\n");
	}
	if (
		argument_count == 2
		&& std::string_view{ arguments[1] } == "--expect-locked") {
		return expected_failure_mode(
			application::github_credential_vault_failure::locked,
			"linux-secret-service-locked:pass\n",
			"linux-secret-service-locked:fail\n");
	}
	if (
		argument_count == 2
		&& std::string_view{ arguments[1] }
			== "--expect-bus-redirect-rejected") {
		const auto passed = run_bus_redirect_rejection();
		static_cast<void>(write_category(
			passed
				? "linux-secret-service-bus-redirect-rejected:pass\n"
				: "linux-secret-service-bus-redirect-rejected:fail\n"));
		return passed ? EXIT_SUCCESS : EXIT_FAILURE;
	}
	if (argument_count != 1) {
		static_cast<void>(
			write_category("linux-secret-service-audit:fail\n"));
		return EXIT_FAILURE;
	}

	const auto passed = audit_native_child(argument_count, arguments);
	static_cast<void>(write_category(
		passed ? "linux-secret-service-audit:pass\n"
			   : "linux-secret-service-audit:fail\n"));
	return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
