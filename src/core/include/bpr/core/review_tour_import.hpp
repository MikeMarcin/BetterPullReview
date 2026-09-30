#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_TOUR_IMPORT_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_TOUR_IMPORT_HPP
#include "bpr/core/review_tour_markdown.hpp"
namespace bpr::core {
	/// \brief Bounded raw objects admitted by the host for the exact PR. Links are inert equality keys.
	struct tour_remote_object
	{
		tour_remote_binding                  Binding;
		std::string                          Raw;
		std::string                          Version;
		std::optional<tour_code_destination> Source;
		bool                                 Available        = true;
		bool                                 ValidDestination = true;
		bool                                 Root             = false;
	};
	struct tour_import_candidate
	{
		std::string Label;
		review_tour Draft;
		bool        Preferred = false;
	};
	/// \brief Enumerate every selected-list candidate; never choose newest or acquire write authority.
	[[nodiscard]] auto discover_tours(const review_tour& aSeed, std::string_view aDescription,
									  std::string_view aDescriptionEntity, std::span<const tour_remote_object> aObjects, bool aComplete) -> std::vector<tour_import_candidate>;
}
#endif
