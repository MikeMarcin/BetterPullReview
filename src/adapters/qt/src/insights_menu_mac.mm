#include "insights_menu_mac.hpp"

#import <AppKit/AppKit.h>

#include <QAction>
#include <QGuiApplication>
#include <QMenu>
#include <QMenuBar>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include <algorithm>
#include <functional>
#include <vector>

@interface BPRInsightsMenuTarget : NSObject {
@public
	std::function<void()> Activate;
	std::function<bool()> Enabled;
}
- (void)openInsights:(id)aSender;
- (BOOL)validateMenuItem:(NSMenuItem*)aItem;
@end

@implementation BPRInsightsMenuTarget
- (void)openInsights:(id)aSender
{
	static_cast<void>(aSender);
	if (Activate) {
		Activate();
	}
}
- (BOOL)validateMenuItem:(NSMenuItem*)aItem
{
	static_cast<void>(aItem);
	return Enabled && Enabled();
}
@end

namespace bpr::qt {
	namespace {
		class native_insights_menu final : public QObject
		{
			struct donor
			{
				QPointer<QAction>  Action;
				QPointer<QMenuBar> Bar;
			};

		public:
			native_insights_menu()
				: QObject{ qGuiApp }
			{
				Target           = [BPRInsightsMenuTarget new];
				Target->Enabled  = [this] { const auto action = current_action(); return action && action->isEnabled(); };
				Target->Activate = [this] {
					if (const auto action = current_action(); action && action->isEnabled()) {
						action->trigger();
					}
				};
				Item                           = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Insights…", @"Application-menu command for the current profile's Insights") action:@selector(openInsights:) keyEquivalent:@"i"];
				Item.target                    = Target;
				Item.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagOption | NSEventModifierFlagShift;
				Item.enabled                   = false;
				connect(qGuiApp, &QGuiApplication::focusWindowChanged, this, [this] { schedule(); });
				connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this] { schedule(); });
			}
			~native_insights_menu() override
			{
				Target->Activate = { };
				Target->Enabled  = { };
				[Item.menu removeItem:Item];
			}
			void attach(QAction& aAction, QMenuBar& aBar)
			{
				Donors.push_back({ &aAction, &aBar });
				// ApplicationSpecificRole creates an item for every QMenu. AppKit
				// retains that application menu while Qt switches window bars.
				// Only the native item owns the Cocoa menu entry. Removing the
				// donor from its QMenu preserves its enabled state and the window's
				// existing QAction shortcut; hiding a QAction disables it in Qt.
				aAction.setMenuRole(QAction::NoRole);
				for (auto* object : aAction.associatedObjects()) {
					if (auto* menu = qobject_cast<QMenu*>(object)) {
						menu->removeAction(&aAction);
					}
				}
				connect(&aAction, &QAction::changed, this, [this] { schedule(); });
				connect(&aAction, &QObject::destroyed, this, [this] { schedule(); });
				connect(&aBar, &QObject::destroyed, this, [this] { schedule(); });
				schedule();
			}

		private:
			auto current_action() const -> QPointer<QAction>
			{
				for (const auto& donor : Donors) {
					if (donor.Action && donor.Bar && donor.Bar->toNSMenu() == NSApp.mainMenu) {
						return donor.Action;
					}
				}
				return { };
			}
			void schedule()
			{
				QTimer::singleShot(0, this, [this] {
					std::erase_if(Donors, [](const auto& aDonor) { return !aDonor.Action || !aDonor.Bar; });
					NSMenu* main = NSApp.mainMenu;
					if (main.numberOfItems == 0) {
						return;
					}
					NSMenu* application = [main itemAtIndex:0].submenu;
					if (application != nil && Item.menu != application) {
						[Item.menu removeItem:Item];
						auto insertion = application.numberOfItems;
						for (NSInteger index = 0; index < application.numberOfItems; ++index) {
							if ([application itemAtIndex:index].submenu == NSApp.servicesMenu) {
								insertion = index;
								if (index > 0 && [application itemAtIndex:index - 1].separatorItem) {
									--insertion;
								}
								break;
							}
						}
						[application insertItem:Item atIndex:insertion];
					}
					const auto action = current_action();
					Item.enabled      = action && action->isEnabled();
				});
			}
			std::vector<donor>     Donors;
			BPRInsightsMenuTarget* Target = nil;
			NSMenuItem*            Item   = nil;
		};
	}
	auto use_native_insights_menu(QAction& aAction, QMenuBar& aBar) -> bool
	{
		if (QGuiApplication::platformName() != QStringLiteral("cocoa")) {
			return false;
		}
		static QPointer<native_insights_menu> native;
		if (!native) {
			native = new native_insights_menu;
		}
		native->attach(aAction, aBar);
		return true;
	}
}
