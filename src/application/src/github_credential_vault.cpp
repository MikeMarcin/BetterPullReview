#include "bpr/application/github_credential_vault.hpp"

#include <algorithm>
#include <new>
#include <utility>

namespace bpr::application {
	namespace {
		void erase_bytes(std::byte* const aBytes, std::size_t aSize) noexcept
		{
			auto* cursor = reinterpret_cast<volatile unsigned char*>(aBytes);
			while (aSize != 0U) {
				*cursor = 0U;
				++cursor;
				--aSize;
			}
		}

		template <typename Completion>
		[[nodiscard]] auto valid_completion(
			const Completion& aCompletion) noexcept -> bool
		{
			if (aCompletion.valueless_by_exception()) {
				return false;
			}

			return std::visit(
				[](const auto& aTerminal) noexcept {
					return valid(aTerminal);
				},
				aCompletion);
		}
	}

	github_credential_material::github_credential_material(
		std::unique_ptr<std::byte[]> aBytes,
		const std::size_t            aSize) noexcept
		: Bytes{ std::move(aBytes) }
		, Size{ aSize }
	{
	}

	github_credential_material::~github_credential_material() noexcept
	{
		reset();
	}

	github_credential_material::github_credential_material(
		github_credential_material&& aOther) noexcept
		: Bytes{ std::move(aOther.Bytes) }
		, Size{ std::exchange(aOther.Size, 0U) }
	{
	}

	auto github_credential_material::operator=(
		github_credential_material&& aOther) noexcept
		-> github_credential_material&
	{
		if (this == &aOther) {
			return *this;
		}

		reset();
		Bytes = std::move(aOther.Bytes);
		Size  = std::exchange(aOther.Size, 0U);
		return *this;
	}

	auto github_credential_material::create(
		const std::span<const std::byte> aBytes) noexcept
		-> std::expected<
			github_credential_material,
			github_credential_material_error>
	{
		if (aBytes.empty()) {
			return std::unexpected{
				github_credential_material_error::empty
			};
		}
		if (aBytes.size() > maximum_github_credential_bytes) {
			return std::unexpected{
				github_credential_material_error::too_large
			};
		}

		auto owned = std::unique_ptr<std::byte[]>{
			new (std::nothrow) std::byte[aBytes.size()]
		};
		if (owned == nullptr) {
			return std::unexpected{
				github_credential_material_error::allocation_failure
			};
		}

		std::ranges::copy(aBytes, owned.get());
		return github_credential_material{
			std::move(owned),
			aBytes.size()
		};
	}

	auto github_credential_material::bytes() const noexcept
		-> std::span<const std::byte>
	{
		return { Bytes.get(), Size };
	}

	auto github_credential_material::size() const noexcept -> std::size_t
	{
		return Size;
	}

	auto github_credential_material::empty() const noexcept -> bool
	{
		return Size == 0U;
	}

	void github_credential_material::reset() noexcept
	{
		erase_bytes(Bytes.get(), Size);
		Bytes.reset();
		Size = 0U;
	}

	auto valid(const github_credential_material& aMaterial) noexcept -> bool
	{
		return !aMaterial.empty()
			&& aMaterial.size() <= maximum_github_credential_bytes
			&& aMaterial.bytes().size() == aMaterial.size();
	}

	auto valid(const github_credential_enrollment& aEnrollment) noexcept -> bool
	{
		return valid(aEnrollment.Context)
			&& valid(aEnrollment.Credential);
	}

	auto valid(const github_credential_store_success& aSuccess) noexcept -> bool
	{
		return valid(aSuccess.Context)
			&& valid(aSuccess.Disposition);
	}

	auto valid(const github_credential_store_error& aError) noexcept -> bool
	{
		return valid(aError.Context)
			&& valid(aError.Failure);
	}

	auto valid(
		const github_credential_store_cancelled& aCancelled) noexcept -> bool
	{
		return valid(aCancelled.Context);
	}

	auto valid(
		const github_credential_store_completion& aCompletion) noexcept -> bool
	{
		return valid_completion(aCompletion);
	}

	auto valid(
		const github_credential_retrieve_success& aSuccess) noexcept -> bool
	{
		return valid(aSuccess.Context)
			&& valid(aSuccess.Credential);
	}

	auto valid(
		const github_credential_retrieve_absent& aAbsent) noexcept -> bool
	{
		return valid(aAbsent.Context);
	}

	auto valid(const github_credential_retrieve_error& aError) noexcept -> bool
	{
		return valid(aError.Context)
			&& valid(aError.Failure);
	}

	auto valid(
		const github_credential_retrieve_cancelled& aCancelled) noexcept -> bool
	{
		return valid(aCancelled.Context);
	}

	auto valid(
		const github_credential_retrieve_completion& aCompletion) noexcept -> bool
	{
		return valid_completion(aCompletion);
	}

	auto valid(const github_credential_delete_success& aSuccess) noexcept -> bool
	{
		return valid(aSuccess.Context)
			&& valid(aSuccess.Disposition);
	}

	auto valid(const github_credential_delete_error& aError) noexcept -> bool
	{
		return valid(aError.Context)
			&& valid(aError.Failure);
	}

	auto valid(
		const github_credential_delete_cancelled& aCancelled) noexcept -> bool
	{
		return valid(aCancelled.Context);
	}

	auto valid(
		const github_credential_delete_completion& aCompletion) noexcept -> bool
	{
		return valid_completion(aCompletion);
	}
}
