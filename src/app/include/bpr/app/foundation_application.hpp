#ifndef BPR_SRC_APP_INCLUDE_BPR_APP_FOUNDATION_APPLICATION_HPP
#define BPR_SRC_APP_INCLUDE_BPR_APP_FOUNDATION_APPLICATION_HPP

#include "bpr/qt/foundation_window.hpp"

#include <memory>

namespace bpr::app {
	/// \brief Provides the foundation application contract.
	class foundation_application final
	{
	public:
		explicit foundation_application(qt::foundation_view_config aConfig);
		~foundation_application();

		foundation_application(const foundation_application&) = delete;
		auto operator=(const foundation_application&)
			-> foundation_application&                                      = delete;
		foundation_application(foundation_application&&)                    = delete;
		auto operator=(foundation_application&&) -> foundation_application& = delete;

		void show();

		[[nodiscard]] auto window() noexcept -> qt::foundation_window&;

	private:
		class implementation;
		std::unique_ptr<implementation> Implementation;
	};
}

#endif // BPR_SRC_APP_INCLUDE_BPR_APP_FOUNDATION_APPLICATION_HPP
