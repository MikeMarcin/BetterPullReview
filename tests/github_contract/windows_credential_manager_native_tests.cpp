#include "windows_credential_manager.hpp"
#include "windows_credential_manager_test_support.hpp"

#include "bpr/application/manual_executor.hpp"

#if !defined(NOMINMAX)
	#define NOMINMAX
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
	#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cwchar>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {
	using namespace bpr;
	using namespace bpr::adapters::github;
	using namespace bpr::tests::windows_credential_manager;

	inline constexpr std::size_t   native_canary_a_bytes      = 73U;
	inline constexpr std::size_t   native_canary_b_bytes      = application::maximum_github_credential_bytes;
	inline constexpr std::size_t   native_leak_probe_bytes    = 32U;
	inline constexpr std::uint64_t native_session             = 0x4250522300000001ULL;
	inline constexpr std::uint64_t native_generation          = 0x4250522300000002ULL;
	inline constexpr std::uint64_t maximum_audited_file_bytes = 128U * 1024U * 1024U;

	using leak_probe  = std::span<const std::byte>;
	using leak_probes = std::span<const leak_probe>;

	static_assert(native_leak_probe_bytes <= native_canary_a_bytes);
	static_assert(native_leak_probe_bytes <= native_canary_b_bytes);

	class secure_bytes final
	{
	public:
		explicit secure_bytes(const std::size_t size)
			: Bytes(size)
		{
		}

		~secure_bytes() noexcept
		{
			reset();
		}

		secure_bytes(const secure_bytes&)                    = delete;
		auto operator=(const secure_bytes&) -> secure_bytes& = delete;
		secure_bytes(secure_bytes&&) noexcept                = default;
		auto operator=(secure_bytes&& other) noexcept -> secure_bytes&
		{
			if (this != &other) {
				reset();
				Bytes = std::move(other.Bytes);
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
			if (!Bytes.empty()) {
				SecureZeroMemory(Bytes.data(), Bytes.size());
				Bytes.clear();
			}
		}

		std::vector<std::byte> Bytes;
	};

	class unique_handle final
	{
	public:
		unique_handle() = default;
		explicit unique_handle(const HANDLE handle) noexcept
			: Handle{ handle }
		{
		}

		~unique_handle() noexcept
		{
			reset();
		}

		unique_handle(const unique_handle&)                    = delete;
		auto operator=(const unique_handle&) -> unique_handle& = delete;

		unique_handle(unique_handle&& other) noexcept
			: Handle{ std::exchange(other.Handle, nullptr) }
		{
		}

		auto operator=(unique_handle&& other) noexcept -> unique_handle&
		{
			if (this != &other) {
				reset();
				Handle = std::exchange(other.Handle, nullptr);
			}
			return *this;
		}

		[[nodiscard]] auto get() const noexcept -> HANDLE
		{
			return Handle;
		}

		[[nodiscard]] auto valid() const noexcept -> bool
		{
			return Handle != nullptr && Handle != INVALID_HANDLE_VALUE;
		}

		[[nodiscard]] auto release() noexcept -> HANDLE
		{
			return std::exchange(Handle, nullptr);
		}

		void reset(const HANDLE handle = nullptr) noexcept
		{
			if (valid()) {
				static_cast<void>(CloseHandle(Handle));
			}
			Handle = handle;
		}

	private:
		HANDLE Handle = nullptr;
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

		audit_directory(audit_directory&& other) noexcept
			: Path{ std::move(other.Path) }
		{
			other.Path.clear();
		}

		auto operator=(audit_directory&& other) noexcept
			-> audit_directory&
		{
			if (this != &other) {
				reset();
				Path = std::move(other.Path);
				other.Path.clear();
			}
			return *this;
		}

		[[nodiscard]] static auto create()
			-> std::optional<audit_directory>
		{
			std::error_code temporary_error;
			const auto      temporary_root = std::filesystem::temp_directory_path(temporary_error);
			if (temporary_error) {
				return std::nullopt;
			}
			for (auto attempt = 0U; attempt < 16U; ++attempt) {
				std::array<std::byte, 16U> random{ };
				if (
					BCryptGenRandom(
						nullptr,
						reinterpret_cast<PUCHAR>(random.data()),
						static_cast<ULONG>(random.size()),
						BCRYPT_USE_SYSTEM_PREFERRED_RNG)
					!= 0) {
					SecureZeroMemory(random.data(), random.size());
					return std::nullopt;
				}

				constexpr std::array<wchar_t, 16U> digits{
					L'0',
					L'1',
					L'2',
					L'3',
					L'4',
					L'5',
					L'6',
					L'7',
					L'8',
					L'9',
					L'a',
					L'b',
					L'c',
					L'd',
					L'e',
					L'f',
				};
				std::wstring suffix;
				suffix.reserve(random.size() * 2U);
				for (const auto byte : random) {
					const auto value = std::to_integer<std::uint8_t>(byte);
					suffix.push_back(digits[value >> 4U]);
					suffix.push_back(digits[value & 0x0FU]);
				}
				SecureZeroMemory(random.data(), random.size());

				auto path = temporary_root
					/ (L"betterpullreview-credential-manager-audit-"
					   + suffix);
				std::error_code error;
				if (!std::filesystem::create_directory(path, error)) {
					continue;
				}

				audit_directory directory;
				directory.Path = std::move(path);
				for (const auto* child : {
						 L"home",
						 L"tmp",
						 L"roaming",
						 L"local",
					 }) {
					std::filesystem::create_directory(
						directory.Path / child,
						error);
					if (error) {
						return std::nullopt;
					}
				}
				return directory;
			}
			return std::nullopt;
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
			// directory. Retain it for deliberate cleanup instead of deleting
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

		void record(Completion completion)
		{
			++Calls;
			Terminal.emplace(std::move(completion));
		}
	};

	using store_recorder    = terminal_recorder<application::github_credential_store_completion>;
	using retrieve_recorder = terminal_recorder<application::github_credential_retrieve_completion>;
	using delete_recorder   = terminal_recorder<application::github_credential_delete_completion>;

	template <typename Terminal, typename Completion>
	[[nodiscard]] auto terminal_as(const std::optional<Completion>& completion)
		-> const Terminal*
	{
		return completion.has_value()
			? std::get_if<Terminal>(&*completion)
			: nullptr;
	}

	[[nodiscard]] auto native_authority()
		-> application::github_credential_authority_key
	{
		return authority(
			0x4250522300000011ULL,
			0x4250522300000012ULL,
			0x4250522300000013ULL,
			0x4250522300000014ULL);
	}

	[[nodiscard]] auto native_context(const std::uint64_t request)
		-> application::github_credential_operation_context
	{
		return context(
			native_authority(),
			request,
			native_session,
			native_generation);
	}

	[[nodiscard]] auto run_store(
		application::github_credential_vault_port& vault,
		application::manual_executor&              lane,
		const std::uint64_t                        request,
		const std::span<const std::byte>           bytes) -> bool
	{
		auto material = application::github_credential_material::create(bytes);
		if (!material.has_value()) {
			return false;
		}

		const auto     operation = native_context(request);
		store_recorder recorder;
		vault.store(
			application::github_credential_enrollment{
				.Context    = operation,
				.Credential = std::move(*material),
			},
			[&recorder](
				application::github_credential_store_completion terminal) {
				recorder.record(std::move(terminal));
			});
		if (lane.pending() != 1U) {
			vault.cancel(operation);
			lane.drain();
			return false;
		}
		lane.run_next();

		const auto* success = terminal_as<
			application::github_credential_store_success>(recorder.Terminal);
		return recorder.Calls == 1U && success != nullptr
			&& success->Context == operation
			&& success->Disposition
			== application::github_credential_store_disposition::stored;
	}

	[[nodiscard]] auto run_retrieve(
		application::github_credential_vault_port& vault,
		application::manual_executor&              lane,
		const std::uint64_t                        request,
		const std::span<const std::byte>           expected) -> bool
	{
		const auto        operation = native_context(request);
		retrieve_recorder recorder;
		vault.retrieve(
			operation,
			[&recorder](
				application::github_credential_retrieve_completion terminal) {
				recorder.record(std::move(terminal));
			});
		if (lane.pending() != 1U) {
			vault.cancel(operation);
			lane.drain();
			return false;
		}
		lane.run_next();

		const auto* success = terminal_as<
			application::github_credential_retrieve_success>(
			recorder.Terminal);
		return recorder.Calls == 1U && success != nullptr
			&& success->Context == operation
			&& same_bytes(success->Credential.bytes(), expected);
	}

	[[nodiscard]] auto run_retrieve_absent(
		application::github_credential_vault_port& vault,
		application::manual_executor&              lane,
		const std::uint64_t                        request) -> bool
	{
		const auto        operation = native_context(request);
		retrieve_recorder recorder;
		vault.retrieve(
			operation,
			[&recorder](
				application::github_credential_retrieve_completion terminal) {
				recorder.record(std::move(terminal));
			});
		if (lane.pending() != 1U) {
			vault.cancel(operation);
			lane.drain();
			return false;
		}
		lane.run_next();

		const auto* absent = terminal_as<
			application::github_credential_retrieve_absent>(
			recorder.Terminal);
		return recorder.Calls == 1U && absent != nullptr
			&& absent->Context == operation;
	}

	[[nodiscard]] auto run_delete(
		application::github_credential_vault_port& vault,
		application::manual_executor&              lane,
		const std::uint64_t                        request,
		const std::optional<
			application::github_credential_delete_disposition>
			expected) -> bool
	{
		const auto      operation = native_context(request);
		delete_recorder recorder;
		vault.delete_credential(
			operation,
			[&recorder](
				application::github_credential_delete_completion terminal) {
				recorder.record(std::move(terminal));
			});
		if (lane.pending() != 1U) {
			vault.cancel(operation);
			lane.drain();
			return false;
		}
		lane.run_next();

		const auto* success = terminal_as<
			application::github_credential_delete_success>(
			recorder.Terminal);
		return recorder.Calls == 1U && success != nullptr
			&& success->Context == operation
			&& (!expected.has_value()
				|| success->Disposition == *expected);
	}

	// BPR-0543. The refresh-slot equivalents of the three helpers above. They
	// drive the same vault against its second Credential Manager target, so a
	// slot that answered from the base target would be caught by the base
	// retrieve between them rather than by inspection.
	[[nodiscard]] auto run_store_refresh(
		application::github_credential_vault_port& aVault,
		application::manual_executor&              aLane,
		const std::uint64_t                        aRequest,
		const std::span<const std::byte>           aBytes) -> bool
	{
		auto material = application::github_credential_material::create(aBytes);
		if (!material.has_value()) {
			return false;
		}

		const auto     operation = native_context(aRequest);
		store_recorder recorder;
		aVault.store_refresh(
			application::github_credential_enrollment{
				.Context    = operation,
				.Credential = std::move(*material),
			},
			[&recorder](
				application::github_credential_store_completion terminal) {
				recorder.record(std::move(terminal));
			});
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

	[[nodiscard]] auto run_retrieve_refresh(
		application::github_credential_vault_port& aVault,
		application::manual_executor&              aLane,
		const std::uint64_t                        aRequest,
		const std::span<const std::byte>           aExpected) -> bool
	{
		const auto        operation = native_context(aRequest);
		retrieve_recorder recorder;
		aVault.retrieve_refresh(
			operation,
			[&recorder](
				application::github_credential_retrieve_completion terminal) {
				recorder.record(std::move(terminal));
			});
		if (aLane.pending() != 1U) {
			aVault.cancel(operation);
			aLane.drain();
			return false;
		}
		aLane.run_next();

		const auto* success = terminal_as<
			application::github_credential_retrieve_success>(
			recorder.Terminal);
		return recorder.Calls == 1U && success != nullptr
			&& success->Context == operation
			&& same_bytes(success->Credential.bytes(), aExpected);
	}

	[[nodiscard]] auto run_retrieve_refresh_absent(
		application::github_credential_vault_port& aVault,
		application::manual_executor&              aLane,
		const std::uint64_t                        aRequest) -> bool
	{
		const auto        operation = native_context(aRequest);
		retrieve_recorder recorder;
		aVault.retrieve_refresh(
			operation,
			[&recorder](
				application::github_credential_retrieve_completion terminal) {
				recorder.record(std::move(terminal));
			});
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

	[[nodiscard]] auto run_delete_refresh(
		application::github_credential_vault_port& aVault,
		application::manual_executor&              aLane,
		const std::uint64_t                        aRequest,
		const std::optional<
			application::github_credential_delete_disposition> aExpected) -> bool
	{
		const auto      operation = native_context(aRequest);
		delete_recorder recorder;
		aVault.delete_refresh(
			operation,
			[&recorder](
				application::github_credential_delete_completion terminal) {
				recorder.record(std::move(terminal));
			});
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
		application::manual_executor& lane)
		-> std::unique_ptr<application::github_credential_vault_port>
	{
		auto vault = make_windows_credential_manager_integration_test_vault(lane);
		return vault.has_value() ? std::move(*vault) : nullptr;
	}

	[[nodiscard]] auto ensure_native_item_absent(
		const std::uint64_t request_base) -> bool
	{
		application::manual_executor lane;
		auto                         vault = make_native_vault(lane);
		if (vault == nullptr) {
			return false;
		}

		const auto deletion = run_delete(*vault, lane, request_base, std::nullopt);
		const auto absence  = run_retrieve_absent(*vault, lane, request_base + 1U);
		// BPR-0543. The refresh target is a second entry in the same store, so
		// setup and cleanup have to clear it too or a failed run leaks refresh
		// material into the next one.
		const auto refresh_deletion = run_delete_refresh(
			*vault,
			lane,
			request_base + 2U,
			std::nullopt);
		return deletion && absence && refresh_deletion;
	}

	[[nodiscard]] auto run_native_lifecycle(
		const std::span<const std::byte> canary_a,
		const std::span<const std::byte> canary_b) -> bool
	{
		application::manual_executor lane;
		auto                         vault = make_native_vault(lane);
		if (vault == nullptr) {
			return false;
		}

		auto ok = true;
		ok      = run_delete(*vault, lane, 100U, std::nullopt) && ok;
		ok      = run_retrieve_absent(*vault, lane, 101U) && ok;
		ok      = run_store(*vault, lane, 102U, canary_a) && ok;
		ok      = run_retrieve(*vault, lane, 103U, canary_a) && ok;
		ok      = run_store(*vault, lane, 104U, canary_b) && ok;
		ok      = run_retrieve(*vault, lane, 105U, canary_b) && ok;
		ok      = run_delete(
					  *vault,
					  lane,
					  106U,
					  application::github_credential_delete_disposition::deleted)
			&& ok;
		ok = run_retrieve_absent(*vault, lane, 107U) && ok;
		ok = run_delete(
				 *vault,
				 lane,
				 108U,
				 application::github_credential_delete_disposition::absent)
			&& ok;

		const auto cleanup              = run_delete(*vault, lane, 109U, std::nullopt);
		const auto post_cleanup_absence = run_retrieve_absent(*vault, lane, 110U);
		return ok && cleanup && post_cleanup_absence;
	}

	// BPR-0543. The refresh slot against the real Credential Manager: a stored
	// entry is retrievable under its authority, is absent from the base slot,
	// and is gone after delete_refresh. The last delete proves an absent entry
	// answers success rather than unavailable, which is what BPR-0540's
	// orderly-quit erasure relies on.
	// [[BPR-0545]]. The erasure occasion against the real Credential Manager:
	// the credential delete first, then the refresh erasure issued from its
	// terminal under the same context, which is what the host does now. Both
	// complete on their own targets and both targets are absent afterwards.
	// The pair submitted together under one context is refused before it is
	// queued -- the contract suite pins that -- so this row is the other half:
	// that the fixed ordering actually reaches both targets on a real vault.
	[[nodiscard]] auto run_native_erasure_occasion(
		const std::span<const std::byte> aCredential,
		const std::span<const std::byte> aRefresh) -> bool
	{
		application::manual_executor lane;
		auto                         vault = make_native_vault(lane);
		if (vault == nullptr) {
			return false;
		}

		auto ok = true;
		ok      = run_delete(*vault, lane, 300U, std::nullopt) && ok;
		ok      = run_delete_refresh(*vault, lane, 301U, std::nullopt) && ok;

		ok = run_store(*vault, lane, 302U, aCredential) && ok;
		ok = run_store_refresh(*vault, lane, 303U, aRefresh) && ok;
		ok = run_retrieve(*vault, lane, 304U, aCredential) && ok;
		ok = run_retrieve_refresh(*vault, lane, 305U, aRefresh) && ok;

		// The occasion. One context number for both halves, the second issued
		// only after the first has terminated, exactly as the host sequences
		// it; a vault that queued them together would refuse the second.
		const auto occasion = 306U;
		ok                  = run_delete(
								  *vault,
								  lane,
								  occasion,
								  application::github_credential_delete_disposition::deleted)
			&& ok;
		ok = run_delete_refresh(
				 *vault,
				 lane,
				 occasion,
				 application::github_credential_delete_disposition::deleted)
			&& ok;

		ok = run_retrieve_absent(*vault, lane, 307U) && ok;
		ok = run_retrieve_refresh_absent(*vault, lane, 308U) && ok;

		const auto cleanup = run_delete(*vault, lane, 309U, std::nullopt)
			&& run_delete_refresh(*vault, lane, 310U, std::nullopt);
		return ok && cleanup;
	}

	[[nodiscard]] auto run_native_refresh_lifecycle(
		const std::span<const std::byte> aCanaryA,
		const std::span<const std::byte> aCanaryB) -> bool
	{
		application::manual_executor lane;
		auto                         vault = make_native_vault(lane);
		if (vault == nullptr) {
			return false;
		}

		auto ok = true;
		ok      = run_delete_refresh(*vault, lane, 200U, std::nullopt) && ok;
		ok      = run_delete(*vault, lane, 201U, std::nullopt) && ok;

		ok = run_store_refresh(*vault, lane, 202U, aCanaryA) && ok;
		ok = run_retrieve_refresh(*vault, lane, 203U, aCanaryA) && ok;
		// The base slot must not answer for it: the same authority, the other
		// target, and nothing stored there.
		ok = run_retrieve_absent(*vault, lane, 204U) && ok;

		// A base credential beside it stays its own, both ways round.
		ok = run_store(*vault, lane, 205U, aCanaryB) && ok;
		ok = run_retrieve(*vault, lane, 206U, aCanaryB) && ok;
		ok = run_retrieve_refresh(*vault, lane, 207U, aCanaryA) && ok;

		ok = run_delete_refresh(
				 *vault,
				 lane,
				 208U,
				 application::github_credential_delete_disposition::deleted)
			&& ok;
		// Deleting the refresh entry leaves the base credential untouched.
		ok = run_retrieve(*vault, lane, 209U, aCanaryB) && ok;
		ok = run_delete_refresh(
				 *vault,
				 lane,
				 210U,
				 application::github_credential_delete_disposition::absent)
			&& ok;

		const auto cleanup = run_delete(*vault, lane, 211U, std::nullopt)
			&& run_delete_refresh(*vault, lane, 212U, std::nullopt);
		const auto absence = run_retrieve_absent(*vault, lane, 213U);
		return ok && cleanup && absence;
	}

	[[nodiscard]] auto read_exact(
		const HANDLE               handle,
		const std::span<std::byte> output) -> bool
	{
		auto offset = std::size_t{ 0U };
		while (offset < output.size()) {
			DWORD      count     = 0U;
			const auto remaining = output.size() - offset;
			if (
				ReadFile(
					handle,
					output.data() + offset,
					static_cast<DWORD>(remaining),
					&count,
					nullptr)
					== FALSE
				|| count == 0U) {
				return false;
			}
			offset += static_cast<std::size_t>(count);
		}
		return true;
	}

	[[nodiscard]] auto write_exact(
		const HANDLE                     handle,
		const std::span<const std::byte> input) -> bool
	{
		auto offset = std::size_t{ 0U };
		while (offset < input.size()) {
			DWORD      count     = 0U;
			const auto remaining = input.size() - offset;
			if (
				WriteFile(
					handle,
					input.data() + offset,
					static_cast<DWORD>(remaining),
					&count,
					nullptr)
					== FALSE
				|| count == 0U) {
				return false;
			}
			offset += static_cast<std::size_t>(count);
		}
		return true;
	}

	[[nodiscard]] auto read_frame(const HANDLE handle)
		-> std::optional<secure_bytes>
	{
		std::array<std::byte, 4U> header{ };
		if (!read_exact(handle, header)) {
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
		if (!read_exact(handle, bytes.mutable_bytes())) {
			return std::nullopt;
		}
		return bytes;
	}

	[[nodiscard]] auto write_frame(
		const HANDLE                     handle,
		const std::span<const std::byte> bytes) -> bool
	{
		if (
			bytes.empty()
			|| bytes.size() > application::maximum_github_credential_bytes) {
			return false;
		}
		const auto       length = static_cast<std::uint32_t>(bytes.size());
		const std::array header{
			static_cast<std::byte>((length >> 24U) & 0xFFU),
			static_cast<std::byte>((length >> 16U) & 0xFFU),
			static_cast<std::byte>((length >> 8U) & 0xFFU),
			static_cast<std::byte>(length & 0xFFU),
		};
		return write_exact(handle, header) && write_exact(handle, bytes);
	}

	[[nodiscard]] auto printable(
		const std::span<const std::byte> bytes) -> bool
	{
		return std::ranges::all_of(bytes, [](const std::byte byte) {
			const auto value = std::to_integer<unsigned int>(byte);
			return value >= 0x21U && value <= 0x7EU;
		});
	}

	[[nodiscard]] auto random_printable_bytes(const std::size_t size)
		-> std::optional<secure_bytes>
	{
		secure_bytes bytes{ size };
		if (
			BCryptGenRandom(
				nullptr,
				reinterpret_cast<PUCHAR>(bytes.mutable_bytes().data()),
				static_cast<ULONG>(bytes.mutable_bytes().size()),
				BCRYPT_USE_SYSTEM_PREFERRED_RNG)
			!= 0) {
			return std::nullopt;
		}
		for (auto& byte : bytes.mutable_bytes()) {
			const auto random = std::to_integer<unsigned int>(byte);
			byte              = static_cast<std::byte>(0x21U + (random % 94U));
		}
		return bytes;
	}

	[[nodiscard]] auto write_category(
		const std::string_view category) -> bool
	{
		const auto output = GetStdHandle(STD_OUTPUT_HANDLE);
		return output != nullptr && output != INVALID_HANDLE_VALUE
			&& write_exact(
				   output,
				   std::as_bytes(std::span{ category }));
	}

	[[nodiscard]] auto native_child() -> int
	{
		const auto input    = GetStdHandle(STD_INPUT_HANDLE);
		auto       canary_a = read_frame(input);
		auto       canary_b = read_frame(input);
		std::byte  extra{ };
		DWORD      trailing_count = 0U;
		const auto trailing_read  = ReadFile(input, &extra, 1U, &trailing_count, nullptr);
		const auto trailing_error = trailing_read == FALSE ? GetLastError() : ERROR_SUCCESS;
		const auto at_end         = (trailing_read != FALSE && trailing_count == 0U)
			|| (trailing_read == FALSE
				&& trailing_error == ERROR_BROKEN_PIPE);
		const auto input_valid = canary_a.has_value() && canary_b.has_value()
			&& canary_a->bytes().size() == native_canary_a_bytes
			&& canary_b->bytes().size() == native_canary_b_bytes
			&& !same_bytes(canary_a->bytes(), canary_b->bytes())
			&& printable(canary_a->bytes())
			&& printable(canary_b->bytes()) && at_end;
		const auto passed = input_valid
			&& run_native_lifecycle(
								canary_a->bytes(),
								canary_b->bytes())
			&& run_native_refresh_lifecycle(
								canary_a->bytes(),
								canary_b->bytes())
			&& run_native_erasure_occasion(
								canary_a->bytes(),
								canary_b->bytes());
		static_cast<void>(write_category(
			passed
				? "windows-credential-manager-native:pass\n"
				: "windows-credential-manager-native:fail\n"));
		return passed ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	[[nodiscard]] auto wide_string_contains(
		const std::wstring_view          text,
		const std::span<const std::byte> bytes) -> bool
	{
		if (bytes.empty() || text.size() < bytes.size()) {
			return false;
		}
		return std::search(
				   text.begin(),
				   text.end(),
				   bytes.begin(),
				   bytes.end(),
				   [](const wchar_t character, const std::byte byte) {
					   return character
						   == static_cast<wchar_t>(
								  std::to_integer<unsigned char>(byte));
				   })
			!= text.end();
	}

	[[nodiscard]] auto wide_string_contains_any(
		const std::wstring_view text,
		const leak_probes       probes) -> bool
	{
		return std::ranges::any_of(probes, [text](const leak_probe probe) {
			return wide_string_contains(text, probe);
		});
	}

	[[nodiscard]] auto read_file(
		const std::filesystem::path& path)
		-> std::optional<secure_bytes>
	{
		const auto    native_path = path.wstring();
		unique_handle input{ CreateFileW(
			native_path.c_str(),
			GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			nullptr) };
		if (!input.valid()) {
			return std::nullopt;
		}

		LARGE_INTEGER size{ };
		if (
			GetFileSizeEx(input.get(), &size) == FALSE || size.QuadPart < 0
			|| static_cast<std::uint64_t>(size.QuadPart)
				> maximum_audited_file_bytes) {
			return std::nullopt;
		}
		secure_bytes bytes{ static_cast<std::size_t>(size.QuadPart) };
		if (!read_exact(input.get(), bytes.mutable_bytes())) {
			return std::nullopt;
		}
		return bytes;
	}

	[[nodiscard]] auto probes_absent_from_file(
		const std::filesystem::path& path,
		const leak_probes            probes) -> bool
	{
		auto bytes = read_file(path);
		if (!bytes.has_value()) {
			return false;
		}
		return std::ranges::none_of(
			probes,
			[&bytes](const leak_probe probe) {
				return std::ranges::search(bytes->bytes(), probe).begin()
					!= bytes->bytes().end();
			});
	}

	[[nodiscard]] auto exact_file_text(
		const std::filesystem::path& path,
		const std::string_view       expected) -> bool
	{
		auto bytes = read_file(path);
		return bytes.has_value()
			&& bytes->bytes().size() == expected.size()
			&& std::ranges::equal(
				   bytes->bytes(),
				   std::as_bytes(std::span{ expected }));
	}

	[[nodiscard]] auto probes_absent_from_tree(
		const std::filesystem::path& root,
		const leak_probes            probes) -> bool
	{
		std::error_code error;
		auto            iterator = std::filesystem::recursive_directory_iterator{
			root,
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
					|| !probes_absent_from_file(
						entry.path(),
						probes)) {
					return false;
				}
			} else if (error) {
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] auto windows_directory()
		-> std::optional<std::filesystem::path>
	{
		std::array<wchar_t, MAX_PATH + 1U> buffer{ };
		const auto                         size = GetWindowsDirectoryW(buffer.data(), static_cast<UINT>(buffer.size()));
		if (
			size == 0U
			|| static_cast<std::size_t>(size) >= buffer.size()) {
			return std::nullopt;
		}
		return std::filesystem::path{
			std::wstring_view{ buffer.data(), size }
		};
	}

	[[nodiscard]] auto make_child_environment(
		const std::filesystem::path& root)
		-> std::optional<std::vector<wchar_t>>
	{
		const auto system_root = windows_directory();
		if (!system_root.has_value()) {
			return std::nullopt;
		}

		std::vector<std::wstring> variables{
			L"APPDATA=" + (root / L"roaming").wstring(),
			L"LOCALAPPDATA=" + (root / L"local").wstring(),
			L"PATH=" + (*system_root / L"System32").wstring(),
			L"SystemRoot=" + system_root->wstring(),
			L"TEMP=" + (root / L"tmp").wstring(),
			L"TMP=" + (root / L"tmp").wstring(),
			L"USERPROFILE=" + (root / L"home").wstring(),
		};
		std::ranges::sort(
			variables,
			[](const std::wstring& left, const std::wstring& right) {
				return _wcsicmp(left.c_str(), right.c_str()) < 0;
			});

		auto size = std::size_t{ 1U };
		for (const auto& variable : variables) {
			size += variable.size() + 1U;
		}
		if (size > 32U * 1024U) {
			return std::nullopt;
		}

		std::vector<wchar_t> block;
		block.reserve(size);
		for (const auto& variable : variables) {
			block.insert(block.end(), variable.begin(), variable.end());
			block.push_back(L'\0');
		}
		block.push_back(L'\0');
		return block;
	}

	[[nodiscard]] auto environment_strings(
		const std::vector<wchar_t>& block)
		-> std::vector<std::wstring_view>
	{
		std::vector<std::wstring_view> variables;
		auto                           offset = std::size_t{ 0U };
		while (offset < block.size() && block[offset] != L'\0') {
			const auto* begin  = block.data() + offset;
			const auto  length = std::wcslen(begin);
			variables.emplace_back(begin, length);
			offset += length + 1U;
		}
		return variables;
	}

	[[nodiscard]] auto canonical_executable()
		-> std::optional<std::filesystem::path>
	{
		auto capacity = std::size_t{ MAX_PATH + 1U };
		while (capacity <= 32U * 1024U) {
			std::vector<wchar_t> buffer(capacity);
			const auto           size = GetModuleFileNameW(
				nullptr,
				buffer.data(),
				static_cast<DWORD>(buffer.size()));
			if (size == 0U) {
				return std::nullopt;
			}
			if (
				static_cast<std::size_t>(size)
				< buffer.size() - 1U) {
				std::error_code error;
				auto            path = std::filesystem::canonical(
					std::filesystem::path{
						std::wstring_view{ buffer.data(), size } },
					error);
				return error ? std::nullopt
							 : std::optional<std::filesystem::path>{
								   std::move(path)
							   };
			}
			capacity *= 2U;
		}
		return std::nullopt;
	}

	[[nodiscard]] auto quote_windows_argument(
		const std::wstring_view argument) -> std::wstring
	{
		std::wstring quoted;
		quoted.push_back(L'"');
		auto backslashes = std::size_t{ 0U };
		for (const auto character : argument) {
			if (character == L'\\') {
				++backslashes;
				continue;
			}
			if (character == L'"') {
				quoted.append(backslashes * 2U + 1U, L'\\');
				quoted.push_back(L'"');
				backslashes = 0U;
				continue;
			}
			quoted.append(backslashes, L'\\');
			backslashes = 0U;
			quoted.push_back(character);
		}
		quoted.append(backslashes * 2U, L'\\');
		quoted.push_back(L'"');
		return quoted;
	}

	class process_attribute_list final
	{
	public:
		process_attribute_list() = default;
		~process_attribute_list() noexcept
		{
			if (List != nullptr) {
				DeleteProcThreadAttributeList(List);
			}
		}

		process_attribute_list(const process_attribute_list&) = delete;
		auto operator=(const process_attribute_list&)
			-> process_attribute_list& = delete;

		[[nodiscard]] auto initialize(
			const std::span<const HANDLE> handles) -> bool
		{
			SIZE_T bytes = 0U;
			static_cast<void>(
				InitializeProcThreadAttributeList(nullptr, 1U, 0U, &bytes));
			if (bytes == 0U) {
				return false;
			}

			try {
				Storage.resize(bytes);
			} catch (...) {
				return false;
			}
			List = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
				Storage.data());
			if (
				InitializeProcThreadAttributeList(
					List,
					1U,
					0U,
					&bytes)
				== FALSE) {
				List = nullptr;
				return false;
			}
			if (
				UpdateProcThreadAttribute(
					List,
					0U,
					PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
					const_cast<HANDLE*>(handles.data()),
					handles.size_bytes(),
					nullptr,
					nullptr)
				== FALSE) {
				DeleteProcThreadAttributeList(List);
				List = nullptr;
				return false;
			}
			return true;
		}

		[[nodiscard]] auto get() const noexcept
			-> LPPROC_THREAD_ATTRIBUTE_LIST
		{
			return List;
		}

	private:
		std::vector<std::byte>       Storage;
		LPPROC_THREAD_ATTRIBUTE_LIST List = nullptr;
	};

	[[nodiscard]] auto create_inheritable_file(
		const std::filesystem::path& path) -> unique_handle
	{
		SECURITY_ATTRIBUTES security{
			.nLength              = static_cast<DWORD>(sizeof(SECURITY_ATTRIBUTES)),
			.lpSecurityDescriptor = nullptr,
			.bInheritHandle       = TRUE,
		};
		const auto native_path = path.wstring();
		return unique_handle{ CreateFileW(
			native_path.c_str(),
			GENERIC_WRITE,
			FILE_SHARE_READ,
			&security,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			nullptr) };
	}

	[[nodiscard]] auto audit_native_child(
		const int      parent_argument_count,
		wchar_t* const parent_arguments[]) -> bool
	{
		auto canary_a = random_printable_bytes(native_canary_a_bytes);
		auto canary_b = random_printable_bytes(native_canary_b_bytes);
		if (!canary_a.has_value() || !canary_b.has_value()) {
			return false;
		}
		while (same_bytes(canary_a->bytes(), canary_b->bytes())) {
			canary_b = random_printable_bytes(native_canary_b_bytes);
			if (!canary_b.has_value()) {
				return false;
			}
		}
		const std::array probes{
			canary_a->bytes().first(native_leak_probe_bytes),
			canary_a->bytes().last(native_leak_probe_bytes),
			canary_b->bytes().first(native_leak_probe_bytes),
			canary_b->bytes().last(native_leak_probe_bytes),
		};

		auto       directory  = audit_directory::create();
		const auto executable = canonical_executable();
		if (!directory.has_value() || !executable.has_value()) {
			return false;
		}

		const auto stdout_path = directory->path() / L"stdout.audit";
		const auto stderr_path = directory->path() / L"stderr.audit";
		auto       stdout_file = create_inheritable_file(stdout_path);
		auto       stderr_file = create_inheritable_file(stderr_path);

		SECURITY_ATTRIBUTES pipe_security{
			.nLength              = static_cast<DWORD>(sizeof(SECURITY_ATTRIBUTES)),
			.lpSecurityDescriptor = nullptr,
			.bInheritHandle       = TRUE,
		};
		HANDLE child_input_raw  = nullptr;
		HANDLE parent_input_raw = nullptr;
		if (
			!stdout_file.valid() || !stderr_file.valid()
			|| CreatePipe(
				   &child_input_raw,
				   &parent_input_raw,
				   &pipe_security,
				   0U)
				== FALSE) {
			if (child_input_raw != nullptr) {
				static_cast<void>(CloseHandle(child_input_raw));
			}
			if (parent_input_raw != nullptr) {
				static_cast<void>(CloseHandle(parent_input_raw));
			}
			return false;
		}
		unique_handle child_input{ child_input_raw };
		unique_handle parent_input{ parent_input_raw };
		if (
			SetHandleInformation(
				parent_input.get(),
				HANDLE_FLAG_INHERIT,
				0U)
			== FALSE) {
			return false;
		}

		const std::array child_handles{
			child_input.get(),
			stdout_file.get(),
			stderr_file.get(),
		};
		process_attribute_list attributes;
		auto                   environment = make_child_environment(directory->path());
		if (
			!attributes.initialize(child_handles)
			|| !environment.has_value()) {
			return false;
		}

		constexpr std::wstring_view child_mode      = L"--native-child";
		const auto                  executable_text = executable->wstring();
		auto                        command_line    = quote_windows_argument(executable_text) + L" "
			+ quote_windows_argument(child_mode);
		std::vector<wchar_t> mutable_command{
			command_line.begin(),
			command_line.end()
		};
		mutable_command.push_back(L'\0');

		auto        audit_ok           = true;
		const auto* raw_parent_command = GetCommandLineW();
		if (raw_parent_command == nullptr) {
			audit_ok = false;
		} else {
			const auto raw = std::wstring_view{ raw_parent_command };
			audit_ok       = !wide_string_contains_any(raw, probes)
				&& audit_ok;
		}
		for (auto index = 0; index < parent_argument_count; ++index) {
			const auto argument = std::wstring_view{ parent_arguments[index] };
			audit_ok            = !wide_string_contains_any(argument, probes)
				&& audit_ok;
		}
		audit_ok = !wide_string_contains_any(command_line, probes)
			&& audit_ok;
		for (const auto variable :
			 environment_strings(*environment)) {
			audit_ok = !wide_string_contains_any(variable, probes)
				&& audit_ok;
		}

		STARTUPINFOEXW startup{ };
		startup.StartupInfo.cb         = static_cast<DWORD>(sizeof(startup));
		startup.StartupInfo.dwFlags    = STARTF_USESTDHANDLES;
		startup.StartupInfo.hStdInput  = child_input.get();
		startup.StartupInfo.hStdOutput = stdout_file.get();
		startup.StartupInfo.hStdError  = stderr_file.get();
		startup.lpAttributeList        = attributes.get();
		PROCESS_INFORMATION process_info{ };
		const auto          working_directory = directory->path().wstring();
		const auto          created           = CreateProcessW(
													executable_text.c_str(),
													mutable_command.data(),
													nullptr,
													nullptr,
													TRUE,
													EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT
														| CREATE_NO_WINDOW,
													environment->data(),
													working_directory.c_str(),
													&startup.StartupInfo,
													&process_info)
			!= FALSE;
		unique_handle process{
			created ? process_info.hProcess : nullptr
		};
		unique_handle thread{
			created ? process_info.hThread : nullptr
		};
		child_input.reset();
		stdout_file.reset();
		stderr_file.reset();

		auto  wrote           = false;
		auto  waited          = false;
		auto  process_stopped = !created;
		DWORD child_exit      = STILL_ACTIVE;
		if (created) {
			wrote = write_frame(parent_input.get(), canary_a->bytes())
				&& write_frame(parent_input.get(), canary_b->bytes());
			parent_input.reset();

			const auto wait_result = WaitForSingleObject(process.get(), 15'000U);
			if (wait_result == WAIT_OBJECT_0) {
				waited          = true;
				process_stopped = true;
			} else {
				static_cast<void>(
					TerminateProcess(
						process.get(),
						static_cast<UINT>(EXIT_FAILURE)));
				process_stopped = WaitForSingleObject(process.get(), 5'000U)
					== WAIT_OBJECT_0;
			}
			if (process_stopped) {
				static_cast<void>(
					GetExitCodeProcess(process.get(), &child_exit));
			}
		} else {
			parent_input.reset();
		}

		if (!process_stopped) {
			directory->abandon();
			return false;
		}

		const auto parent_cleanup = ensure_native_item_absent(3'000U);
		audit_ok                  = created && waited && wrote
			&& child_exit == static_cast<DWORD>(EXIT_SUCCESS) && audit_ok;
		audit_ok = exact_file_text(
					   stdout_path,
					   "windows-credential-manager-native:pass\n")
			&& exact_file_text(stderr_path, "") && audit_ok;

		std::error_code source_error;
		const auto      source = std::filesystem::canonical(__FILE__, source_error);
		audit_ok               = !source_error
			&& probes_absent_from_file(
				*executable,
				probes)
			&& probes_absent_from_file(
				source,
				probes)
			&& probes_absent_from_tree(
				directory->path(),
				probes)
			&& parent_cleanup && audit_ok;
		return directory->remove() && audit_ok;
	}

	[[nodiscard]] auto fixture_mode(const bool setup) -> int
	{
		const auto request_base = setup ? 1'000U : 2'000U;
		const auto passed       = ensure_native_item_absent(request_base);
		static_cast<void>(write_category(
			setup
				? (
					  passed
						  ? "windows-credential-manager-setup:pass\n"
						  : "windows-credential-manager-setup:fail\n")
				: (
					  passed
						  ? "windows-credential-manager-cleanup:pass\n"
						  : "windows-credential-manager-cleanup:fail\n")));
		return passed ? EXIT_SUCCESS : EXIT_FAILURE;
	}
}

auto wmain(
	const int argument_count,
	wchar_t*  arguments[]) -> int
{
	if (
		argument_count == 2
		&& std::wstring_view{ arguments[1] } == L"--native-child") {
		return native_child();
	}
	if (
		argument_count == 2
		&& std::wstring_view{ arguments[1] } == L"--fixture-setup") {
		return fixture_mode(true);
	}
	if (
		argument_count == 2
		&& std::wstring_view{ arguments[1] } == L"--fixture-cleanup") {
		return fixture_mode(false);
	}
	if (argument_count != 1) {
		static_cast<void>(write_category(
			"windows-credential-manager-audit:fail\n"));
		return EXIT_FAILURE;
	}

	const auto passed = audit_native_child(argument_count, arguments);
	static_cast<void>(write_category(
		passed
			? "windows-credential-manager-audit:pass\n"
			: "windows-credential-manager-audit:fail\n"));
	return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
