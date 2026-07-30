#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <span>

class QWidget;

namespace bpr::qt
{
class sensitive_bytes final
{
public:
    sensitive_bytes() = delete;
    ~sensitive_bytes() noexcept;

    sensitive_bytes(const sensitive_bytes&) = delete;
    auto operator=(const sensitive_bytes&) -> sensitive_bytes& = delete;
    sensitive_bytes(sensitive_bytes&& other) noexcept;
    auto operator=(sensitive_bytes&& other) noexcept -> sensitive_bytes&;

    [[nodiscard]] auto bytes() const noexcept -> std::span<const std::byte>;
    [[nodiscard]] auto size() const noexcept -> std::size_t;
    [[nodiscard]] auto empty() const noexcept -> bool;
    void reset() noexcept;

private:
    friend auto prompt_for_sensitive_text(
        std::size_t maximum_bytes,
        QWidget* parent) -> std::optional<sensitive_bytes>;

    sensitive_bytes(
        std::unique_ptr<std::byte[]> bytes,
        std::size_t size) noexcept;

    std::unique_ptr<std::byte[]> Bytes;
    std::size_t Size = 0U;
};

[[nodiscard]] auto prompt_for_sensitive_text(
    std::size_t maximum_bytes,
    QWidget* parent = nullptr) -> std::optional<sensitive_bytes>;
}
