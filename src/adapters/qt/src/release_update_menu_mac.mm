#include "release_update_menu_mac.hpp"

#import <AppKit/AppKit.h>

#include <QGuiApplication>
#include <QObject>
#include <QTimer>

@interface BPRReleaseUpdateMenuTarget : NSObject {
@public
	std::function<void()> Check;
}
- (void)checkForUpdates:(id)aSender;
@end

@implementation BPRReleaseUpdateMenuTarget
- (void)checkForUpdates:(id)aSender
{
	static_cast<void>(aSender);
	if (Check) {
		Check();
	}
}
@end

namespace bpr::qt {
	namespace {
		class native_update_menu final : public QObject
		{
		public:
			explicit native_update_menu(std::function<void()> aCheck)
			{
				Target        = [BPRReleaseUpdateMenuTarget new];
				Target->Check = std::move(aCheck);
				Item          = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Check for Updates…", @"Application-menu command for checking software updates") action:@selector(checkForUpdates:) keyEquivalent:@""];
				Item.target   = Target;
				// Qt keeps the AppKit application menu while changing window bars.
				// A native item avoids per-QMenu ApplicationSpecificRole duplicates.
				connect(qGuiApp, &QGuiApplication::focusWindowChanged, this, [this] { schedule(); });
				connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this] { schedule(); });
				schedule();
			}
			~native_update_menu() override
			{
				Target->Check = { };
				[Item.menu removeItem:Item];
			}

		private:
			void schedule()
			{
				QTimer::singleShot(0, this, [this] {
					NSMenu* main = NSApp.mainMenu;
					if (main.numberOfItems == 0) {
						return;
					}
					NSMenu* application = [main itemAtIndex:0].submenu;
					if (application != nil && Item.menu != application) {
						[Item.menu removeItem:Item];
						// Under Qt's About slot, its menu loader's first item, so About
						// BetterPullReview ([[BPR-1720]]) leads the application
						// menu and Check for Updates… follows it, as on macOS generally.
						[application insertItem:Item atIndex:(application.numberOfItems > 0 ? 1 : 0)];
					}
				});
			}
			BPRReleaseUpdateMenuTarget* Target = nil;
			NSMenuItem*                 Item   = nil;
		};
	}
	auto make_release_update_native_menu(std::function<void()> aCheck) -> std::unique_ptr<QObject>
	{
		if (QGuiApplication::platformName() != QStringLiteral("cocoa")) {
			return { };
		}
		return std::make_unique<native_update_menu>(std::move(aCheck));
	}
}
