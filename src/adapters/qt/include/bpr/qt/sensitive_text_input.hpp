#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SENSITIVE_TEXT_INPUT_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SENSITIVE_TEXT_INPUT_HPP

#include <cstddef>
#include <memory>
#include <optional>
#include <span>

class QLineEdit;
class QWidget;

namespace bpr::qt {
	/// \brief Provides the sensitive bytes contract.
	class sensitive_bytes final
	{
	public:
		sensitive_bytes() = delete;
		~sensitive_bytes() noexcept;

		sensitive_bytes(const sensitive_bytes&)                    = delete;
		auto operator=(const sensitive_bytes&) -> sensitive_bytes& = delete;
		sensitive_bytes(sensitive_bytes&& aOther) noexcept;
		auto operator=(sensitive_bytes&& aOther) noexcept -> sensitive_bytes&;

		[[nodiscard]] auto bytes() const noexcept -> std::span<const std::byte>;
		[[nodiscard]] auto size() const noexcept -> std::size_t;
		[[nodiscard]] auto empty() const noexcept -> bool;
		void               reset() noexcept;

	private:
		friend auto prompt_for_sensitive_text(
			std::size_t aMaximumBytes,
			QWidget*    aParent) -> std::optional<sensitive_bytes>;
		friend auto take_sensitive_text(QLineEdit& aEditor, std::size_t aMaximumBytes) -> std::optional<sensitive_bytes>;

		sensitive_bytes(
			std::unique_ptr<std::byte[]> aBytes,
			std::size_t                  aSize) noexcept;

		std::unique_ptr<std::byte[]> Bytes;
		std::size_t                  Size = 0U;
	};

	/// \brief RFC-BPR-0032 amendment r1. Takes the text of a password field
	/// in place, with no dialog: the field and every copy are erased, and the
	/// bytes are returned only when nonempty and within `aMaximumBytes`.
	[[nodiscard]] auto take_sensitive_text(QLineEdit& aEditor, std::size_t aMaximumBytes) -> std::optional<sensitive_bytes>;

	/// \brief Computes the prompt for sensitive text result.
	[[nodiscard]] auto prompt_for_sensitive_text(
		std::size_t aMaximumBytes,
		QWidget*    aParent = nullptr) -> std::optional<sensitive_bytes>;
}

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SENSITIVE_TEXT_INPUT_HPP
