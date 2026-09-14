#include "bpr/qt/motion_observer.hpp"

#import <AppKit/AppKit.h>

#include <QGuiApplication>
#include <QLatin1StringView>

#include <functional>
#include <memory>
#include <utility>

namespace bpr::qt {
	namespace {
		struct notification_state
		{
			std::function<void()> Handler;
		};

		class mac_motion_source final : public motion_observation_source
		{
		public:
			mac_motion_source()
			{
				if (QGuiApplication::instance() == nullptr
					|| QGuiApplication::platformName() != QLatin1String{ "cocoa" }) {
					return;
				}
				Workspace = [NSWorkspace sharedWorkspace];
				if (Workspace == nil) {
					return;
				}
				const auto state = State;
				Token = [[Workspace notificationCenter]
					addObserverForName:NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification
					object:Workspace
					queue:[NSOperationQueue mainQueue]
					usingBlock:^(NSNotification*) {
						if (state->Handler) {
							state->Handler();
						}
					}];
			}

			~mac_motion_source() override
			{
				State->Handler = { };
				if (Token != nil) {
					[[Workspace notificationCenter] removeObserver:Token];
				}
			}

			[[nodiscard]] auto read() noexcept -> core::motion_observation override
			{
				if (QGuiApplication::instance() == nullptr
					|| QGuiApplication::platformName() != QLatin1String{ "cocoa" }) {
					return core::motion_observation::unsupported;
				}
				if (Workspace == nil || Token == nil) {
					return core::motion_observation::failed;
				}
				return [Workspace accessibilityDisplayShouldReduceMotion]
					? core::motion_observation::reduced
					: core::motion_observation::full;
			}

			void set_change_handler(std::function<void()> aHandler) override
			{
				State->Handler = std::move(aHandler);
			}

		private:
			NSWorkspace*                         Workspace = nil;
			id                                   Token     = nil;
			std::shared_ptr<notification_state> State = std::make_shared<notification_state>();
		};
	} // namespace

	auto make_native_motion_source() -> std::unique_ptr<motion_observation_source>
	{
		return std::make_unique<mac_motion_source>();
	}
} // namespace bpr::qt
