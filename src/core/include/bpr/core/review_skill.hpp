#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_SKILL_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_SKILL_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace bpr::core {
	/// \brief [[BPR-PENDING-AQ5]], RFC-BPR-0091 owner decision D1. The review
	/// skill's name, which a saved copy must keep.
	inline constexpr std::string_view review_skill_name = "bpr-pr-review";
	/// \brief The most a review skill's `SKILL.md` may hold.
	inline constexpr std::size_t maximum_review_skill_bytes = 64U * 1024U;

	/// \brief What a review skill's front matter says about it.
	struct review_skill_facts
	{
		std::string Description;
		/// `metadata.bpr-version`; absent in a copy that removed it.
		std::optional<std::uint32_t> Version;

		auto operator<=>(const review_skill_facts&) const = default;
	};

	/// \brief Why a review skill cannot be saved, for its words in Qt.
	enum class review_skill_refusal : std::uint8_t {
		/// Over `maximum_review_skill_bytes`.
		too_large,
		/// Not UTF-8.
		not_text,
		/// No `---` front matter at the top.
		no_front_matter,
		/// The front matter names no `name`, or another one.
		name_changed,
		/// The front matter has no non-empty `description`.
		no_description,
	};
	[[nodiscard]] auto stable_name(review_skill_refusal aRefusal) noexcept -> std::string_view;

	/// \brief Reads one `SKILL.md`'s front matter, or says why it cannot be a
	/// review skill.
	///
	/// \details The Agent Skills front matter subset the review skill needs:
	/// the `---` block at the top, its top-level `name` and `description`
	/// keys (bare, single- or double-quoted on one line), and `bpr-version`
	/// as a decimal integer under a top-level `metadata` key. Every other key
	/// and line is skipped. It is not a YAML reader and claims none of YAML's
	/// other rules; a value it cannot read is treated as absent.
	[[nodiscard]] auto inspect_review_skill(std::string_view aText) -> std::expected<review_skill_facts, review_skill_refusal>;

	/// \brief Where the skill a request carries comes from.
	enum class review_skill_source : std::uint8_t {
		/// The default built into this application.
		built_in,
		/// The profile's edited copy.
		profile_copy,
	};
	[[nodiscard]] auto stable_name(review_skill_source aSource) noexcept -> std::string_view;

	/// \brief What the profile's edited copy records beside its text.
	struct review_skill_copy_record
	{
		/// The built-in `bpr-version` the copy was made from.
		std::uint32_t BaseVersion = 0U;
		/// SHA-256 of the built-in text the copy was made from.
		std::array<std::uint8_t, 32> BaseDigest{ };
		/// The built-in version the reviewer chose **Keep mine** for, if any.
		std::optional<std::uint32_t> KeptVersion;

		auto operator<=>(const review_skill_copy_record&) const = default;
	};

	/// \brief The newer built-in version to offer an edited copy, if any.
	///
	/// \details Offered when the built-in version is above the one the copy
	/// came from and the reviewer has not kept their copy for this version or
	/// a later one. An unedited default is never offered anything: it simply
	/// is the built-in text.
	[[nodiscard]] auto newer_review_skill(std::uint32_t aBuiltInVersion, const std::optional<review_skill_copy_record>& aCopy) noexcept
		-> std::optional<std::uint32_t>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_SKILL_HPP
