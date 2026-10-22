#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_SKILL_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_SKILL_HOST_HPP

#include "bpr/application/port.hpp"
#include "bpr/core/agent_context_supplement.hpp"
#include "bpr/core/review_skill.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace bpr::application {
	/// \brief [[BPR-PENDING-AQ5]], RFC-BPR-0091 owner decision D1. The text
	/// of the `bpr-pr-review` skill built into this application, from
	/// `packaging/agent-skills/bpr-pr-review/SKILL.md`.
	[[nodiscard]] auto built_in_review_skill() noexcept -> std::string_view;

	/// \brief The profile's edited copy of the review skill and its record.
	struct stored_review_skill
	{
		std::string                    Text;
		core::review_skill_copy_record Record;

		auto operator<=>(const stored_review_skill&) const = default;
	};

	/// \brief Where the profile keeps its edited copy.
	class review_skill_store_port
	{
	public:
		virtual ~review_skill_store_port() = default;
		/// The stored copy, none, or nothing at all when the store could not
		/// be read.
		[[nodiscard]] virtual auto load_review_skill() -> std::optional<std::optional<stored_review_skill>> = 0;
		[[nodiscard]] virtual auto store_review_skill(const stored_review_skill& aSkill) -> bool            = 0;
		[[nodiscard]] virtual auto forget_review_skill() -> bool                                            = 0;
	};

	/// \brief How one Save, Restore default, Use version N or Keep mine ended.
	enum class review_skill_answer : std::uint8_t {
		/// Done.
		done,
		/// Save refused the text; the editor keeps it.
		refused,
		/// The copy for Downloads could not be written, so nothing was removed.
		copy_failed,
		/// The store did not take the change.
		not_saved,
	};

	/// \brief What Settings › Agents & context › Review skill shows.
	struct review_skill_projection
	{
		/// The skill requests carry now.
		core::review_skill_source Source = core::review_skill_source::built_in;
		std::string               Text;
		std::uint32_t             BuiltInVersion = 0U;
		/// The built-in version an edited copy came from.
		std::optional<std::uint32_t> CopyBaseVersion;
		/// "Version N is available": a newer built-in for an edited copy.
		std::optional<std::uint32_t> Offer;
		/// The stored copy no longer reads as a review skill (edited outside
		/// Settings); requests carry the built-in text until it is fixed.
		std::optional<core::review_skill_refusal> StoredProblem;
		/// Why the last Save was refused.
		std::optional<core::review_skill_refusal> Refusal;
		/// Where the last copy for Downloads was written.
		std::optional<std::string> SavedCopy;
		bool                       Loaded     = false;
		std::uint64_t              Generation = 0U;

		auto operator<=>(const review_skill_projection&) const = default;
	};

	struct review_skill_dependencies
	{
		review_skill_store_port* Store = nullptr;
		/// The built-in text; `built_in_review_skill()` in the application.
		std::string BuiltIn;
		/// Writes one text to a new file in Downloads and names it, or nothing.
		std::function<std::optional<std::string>(const std::string& aText)> SaveCopy;
		effect_executor*                                                    Effects  = nullptr;
		decision_executor*                                                  Decision = nullptr;
		std::function<void(const review_skill_projection&)>                 Publish;
	};

	/// \brief [[BPR-PENDING-AQ5]], RFC-BPR-0091 owner decision D1. The
	/// profile's review skill: the built-in default until an edit, then the
	/// edited copy, which no update changes by itself.
	///
	/// \details Lives on the decision lane; the store and the copy for
	/// Downloads run on the effect lane. The first Save of a text other than
	/// the built-in one makes the copy and records the built-in version and
	/// digest it came from. Restore default and Use version N write the copy
	/// to Downloads first unless told not to, and remove nothing when that
	/// fails. Keep mine is remembered for the offered version.
	class review_skill_host final
	{
	public:
		using answer = std::function<void(review_skill_answer)>;

		explicit review_skill_host(review_skill_dependencies aDependencies);
		~review_skill_host();
		review_skill_host(const review_skill_host&)                    = delete;
		auto operator=(const review_skill_host&) -> review_skill_host& = delete;

		void load();
		void save(std::string aText, answer aAnswered = { });
		/// Restore default and Use version N: back to the built-in text.
		void use_built_in(bool aSaveCopy, answer aAnswered = { });
		void keep_mine(answer aAnswered = { });

		/// The skill the next review request's brief carries.
		[[nodiscard]] auto brief_skill() const -> std::optional<core::brief_review_skill>;
		[[nodiscard]] auto projection() const -> review_skill_projection;
		void               shutdown() noexcept;
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_SKILL_HOST_HPP
