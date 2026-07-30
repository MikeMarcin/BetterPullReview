#include "bpr/application/github_credential_vault.hpp"

#include <algorithm>
#include <new>
#include <utility>

namespace bpr::application
{
namespace
{
void erase_bytes(std::byte* const bytes, std::size_t size) noexcept
{
    auto* cursor = reinterpret_cast<volatile unsigned char*>(bytes);
    while(size != 0U)
    {
        *cursor = 0U;
        ++cursor;
        --size;
    }
}

template<typename Completion>
[[nodiscard]] auto valid_completion(
    const Completion& completion) noexcept -> bool
{
    if(completion.valueless_by_exception())
    {
        return false;
    }

    return std::visit(
        [](const auto& terminal) noexcept {
            return valid(terminal);
        },
        completion);
}
}

github_credential_material::github_credential_material(
    std::unique_ptr<std::byte[]> bytes,
    const std::size_t size) noexcept
    : Bytes{std::move(bytes)}
    , Size{size}
{
}

github_credential_material::~github_credential_material() noexcept
{
    reset();
}

github_credential_material::github_credential_material(
    github_credential_material&& other) noexcept
    : Bytes{std::move(other.Bytes)}
    , Size{std::exchange(other.Size, 0U)}
{
}

auto github_credential_material::operator=(
    github_credential_material&& other) noexcept
    -> github_credential_material&
{
    if(this == &other)
    {
        return *this;
    }

    reset();
    Bytes = std::move(other.Bytes);
    Size = std::exchange(other.Size, 0U);
    return *this;
}

auto github_credential_material::create(
    const std::span<const std::byte> bytes) noexcept
    -> std::expected<
        github_credential_material,
        github_credential_material_error>
{
    if(bytes.empty())
    {
        return std::unexpected{
            github_credential_material_error::empty};
    }
    if(bytes.size() > maximum_github_credential_bytes)
    {
        return std::unexpected{
            github_credential_material_error::too_large};
    }

    auto owned = std::unique_ptr<std::byte[]>{
        new(std::nothrow) std::byte[bytes.size()]};
    if(owned == nullptr)
    {
        return std::unexpected{
            github_credential_material_error::allocation_failure};
    }

    std::ranges::copy(bytes, owned.get());
    return github_credential_material{
        std::move(owned),
        bytes.size()};
}

auto github_credential_material::bytes() const noexcept
    -> std::span<const std::byte>
{
    return {Bytes.get(), Size};
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

auto valid(const github_credential_material& material) noexcept -> bool
{
    return !material.empty()
        && material.size() <= maximum_github_credential_bytes
        && material.bytes().size() == material.size();
}

auto valid(const github_credential_enrollment& enrollment) noexcept -> bool
{
    return valid(enrollment.Context)
        && valid(enrollment.Credential);
}

auto valid(const github_credential_store_success& success) noexcept -> bool
{
    return valid(success.Context)
        && valid(success.Disposition);
}

auto valid(const github_credential_store_error& error) noexcept -> bool
{
    return valid(error.Context)
        && valid(error.Failure);
}

auto valid(
    const github_credential_store_cancelled& cancelled) noexcept -> bool
{
    return valid(cancelled.Context);
}

auto valid(
    const github_credential_store_completion& completion) noexcept -> bool
{
    return valid_completion(completion);
}

auto valid(
    const github_credential_retrieve_success& success) noexcept -> bool
{
    return valid(success.Context)
        && valid(success.Credential);
}

auto valid(
    const github_credential_retrieve_absent& absent) noexcept -> bool
{
    return valid(absent.Context);
}

auto valid(const github_credential_retrieve_error& error) noexcept -> bool
{
    return valid(error.Context)
        && valid(error.Failure);
}

auto valid(
    const github_credential_retrieve_cancelled& cancelled) noexcept -> bool
{
    return valid(cancelled.Context);
}

auto valid(
    const github_credential_retrieve_completion& completion) noexcept -> bool
{
    return valid_completion(completion);
}

auto valid(const github_credential_delete_success& success) noexcept -> bool
{
    return valid(success.Context)
        && valid(success.Disposition);
}

auto valid(const github_credential_delete_error& error) noexcept -> bool
{
    return valid(error.Context)
        && valid(error.Failure);
}

auto valid(
    const github_credential_delete_cancelled& cancelled) noexcept -> bool
{
    return valid(cancelled.Context);
}

auto valid(
    const github_credential_delete_completion& completion) noexcept -> bool
{
    return valid_completion(completion);
}
}
