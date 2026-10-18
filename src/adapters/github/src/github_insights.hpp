#ifndef BPR_GITHUB_INSIGHTS_HPP
#define BPR_GITHUB_INSIGHTS_HPP
#include "bpr/application/insights_observer.hpp"
#include "github_read_contract.hpp"
#include <mutex>
namespace bpr::adapters::github::detail {
	/// \brief Per-transport passive channel. Bootstrap facts wait for the verified account binding.
	class github_insights_channel final
	{
	public:
		github_insights_channel(std::shared_ptr<application::insights_observer> aObserver, std::uint64_t aHost);
		void bind(std::uint64_t aAccount) noexcept;
		void observe(core::insights_event aEvent) noexcept;
		void request(core::insights_resource aResource, core::insights_category aCategory, const wire_response* aResponse = nullptr, std::uint64_t aCount = 1) noexcept;

	private:
		std::shared_ptr<application::insights_observer> Observer;
		std::mutex                                      Mutex;
		core::insights_scope                            Scope;
		bool                                            Bound = false;
		std::vector<core::insights_event>               Pending;
	};
	[[nodiscard]] auto insights_category_for(read_operation aOperation) noexcept -> core::insights_category;
	[[nodiscard]] auto insights_resource_for(read_operation aOperation) noexcept -> core::insights_resource;
}
#endif
