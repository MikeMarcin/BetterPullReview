#include "../../src/adapters/qt/src/insights_menu_mac.hpp"
#include "../support/test_expectation.hpp"

#import <AppKit/AppKit.h>

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QMenu>
#include <QMenuBar>

#include <iostream>
#include <memory>

namespace {
	using bpr::tests::expect;
	void settle()
	{
		for (int turn = 0; turn < 5; ++turn) {
			QCoreApplication::sendPostedEvents();
			QCoreApplication::processEvents(QEventLoop::AllEvents);
		}
	}
	auto select_bar(QMenuBar& aBar) -> NSMenuItem*
	{
		// This is only this prohibited-activation process's mainMenu property.
		// No window is shown, application activated or desktop pixels inspected.
		[NSApp setMainMenu:aBar.toNSMenu()];
		settle();
		NSMenu* application = [NSApp.mainMenu itemAtIndex:0].submenu;
		std::cout << "selected menus: ";
		for (NSMenuItem* top in NSApp.mainMenu.itemArray) {
			std::cout << top.title.UTF8String << "[";
			for (NSMenuItem* child in top.submenu.itemArray) { std::cout << child.title.UTF8String << ";"; }
			std::cout << "]";
		}
		std::cout << std::endl;
		[application update];
		NSMenuItem* found = nil;
		for (NSMenuItem* item in application.itemArray) {
			if ([item.title isEqualToString:@"Insights…"]) {
				expect(found == nil, "exactly one native Insights item");
				found = item;
			}
		}
		expect(found != nil, "Insights stays in the application menu");
		return found;
	}
	auto make_bar(int& aCalls, bool aEnabled = true) -> std::unique_ptr<QMenuBar>
	{
		auto  bar    = std::make_unique<QMenuBar>();
		auto* menu = bar->addMenu(QStringLiteral("Help"));
		menu->addAction(QStringLiteral("Diagnostics…"));
		auto* action = menu->addAction(QStringLiteral("Insights…"));
		action->setEnabled(aEnabled);
		action->setShortcut(QKeySequence{ QStringLiteral("Ctrl+Alt+Shift+I") });
		QObject::connect(action, &QAction::triggered, action, [&aCalls] { ++aCalls; });
		expect(bpr::qt::use_native_insights_menu(*action, *bar), "actual Cocoa registration");
		expect(action->isVisible() && action->menuRole() == QAction::NoRole
				   && !bar->findChild<QMenu*>()->actions().contains(action),
			   "the donor preserves its state without a competing Qt menu entry");
		return bar;
	}
}

int main(int aArgc, char** aArgv)
try {
	qputenv("QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM", "1");
	[NSApplication sharedApplication];
	static_cast<void>([NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited]);
	std::cout << "activation policy before Qt: " << NSApp.activationPolicy << '\n';
	expect(NSApp.activationPolicy == NSApplicationActivationPolicyProhibited, "the fixture cannot activate on the desktop");
	QApplication application{ aArgc, aArgv };
	expect(NSApp.activationPolicy == NSApplicationActivationPolicyProhibited, "Qt preserves prohibited activation");
	expect(!NSApp.active, "the fixture remains inactive");
	int   firstCalls = 0, secondCalls = 0, residentCalls = 0, reopenedCalls = 0;
	auto  first = make_bar(firstCalls);
	auto* item  = select_bar(*first);
	expect(item.enabled, "the current profile's command is enabled");
	expect([item.keyEquivalent isEqualToString:@"i"]
			   && item.keyEquivalentModifierMask == (NSEventModifierFlagCommand | NSEventModifierFlagOption | NSEventModifierFlagShift),
		   "the one native item owns the approved shortcut");
	[NSApp sendAction:item.action to:item.target from:item];
	expect(firstCalls == 1, "the first profile receives the command");
	auto second = make_bar(secondCalls);
	expect(select_bar(*second) == item, "a sibling reuses the same native item");
	[NSApp sendAction:item.action to:item.target from:item];
	expect(firstCalls == 1 && secondCalls == 1, "dispatch follows the actual current menu bar");
	first.reset();
	settle();
	expect(select_bar(*second) == item, "retiring the other donor preserves the current one");
	auto resident = make_bar(residentCalls, false);
	second.reset();
	expect(select_bar(*resident) == item && !item.enabled, "last-window close retains one disabled item");
	[NSApp sendAction:item.action to:item.target from:item];
	expect(residentCalls == 0 && firstCalls == 1 && secondCalls == 1, "a disabled or retired donor cannot receive a command");
	auto reopened = make_bar(reopenedCalls);
	resident.reset();
	expect(select_bar(*reopened) == item && item.enabled, "reopening restores one enabled command");
	[NSApp sendAction:item.action to:item.target from:item];
	expect(reopenedCalls == 1 && secondCalls == 1, "only the reopened profile receives it");
	reopened.reset();
	settle();
	[NSApp sendAction:item.action to:item.target from:item];
	expect(reopenedCalls == 1 && !NSApp.active, "destruction leaves no callable donor and never activates");
	std::cout << "insights-native-menu-tests:pass\n";
	return 0;
} catch (...) {
	return bpr::tests::failing_exit();
}
