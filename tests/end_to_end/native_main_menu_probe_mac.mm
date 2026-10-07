#include "native_main_menu_probe.hpp"

#import <AppKit/AppKit.h>
#include <objc/runtime.h>

extern "C" CFDictionaryRef CGSessionCopyCurrentDictionary(void);

#include <QElapsedTimer>

#include <mutex>
#include <string>
#include <vector>

namespace bpr::tests::native_menu {
	namespace {
		struct trace_state
		{
			std::mutex               Guard;
			QElapsedTimer            Clock;
			bool                     Installed = false;
			std::vector<std::string> Entries;
		};
		auto trace() -> trace_state&
		{
			static trace_state state;
			return state;
		}
		/// AppKit's private window class for a status item's button; matched by
		/// name because the class is not public.
		auto is_status_bar_window(NSWindow* aWindow) -> bool
		{
			return [NSStringFromClass([aWindow class]) isEqualToString:@"NSStatusBarWindow"];
		}
		auto text(NSString* aString) -> std::string
		{
			return aString == nil ? std::string{ } : std::string{ [aString UTF8String] };
		}
		auto pointer(NSMenu* aMenu) -> std::string
		{
			return text([NSString stringWithFormat:@"%p", (__bridge const void*)aMenu]);
		}
		auto describe(NSMenu* aMenu) -> std::string
		{
			if (aMenu == nil) {
				return { };
			}
			std::string line;
			for (NSMenuItem* item in [aMenu itemArray]) {
				if (!line.empty()) {
					line += '|';
				}
				if ([item isHidden]) {
					line += '~';
				}
				line += text([item title]);
				line += '[';
				NSMenu* submenu = [item submenu];
				if (submenu == nil) {
					line += '-';
				} else {
					auto first = true;
					for (NSMenuItem* entry in [submenu itemArray]) {
						if ([entry isSeparatorItem] || [entry isHidden]) {
							continue;
						}
						if (!first) {
							line += ';';
						}
						first = false;
						line += text([entry title]);
					}
				}
				line += ']';
			}
			return line;
		}
		/// An item title matches exactly or as its first word, because AppKit
		/// appends the application name to a merged Quit item.
		auto titled(NSMenuItem* aItem, std::string_view aTitle) -> bool
		{
			const auto title = text([aItem title]);
			return title == aTitle || (title.size() > aTitle.size() && title.compare(0, aTitle.size(), aTitle) == 0 && title[aTitle.size()] == ' ');
		}
		auto find_item(NSMenu* aMenu, std::string_view aItemTitle) -> NSInteger
		{
			if (aMenu == nil) {
				return -1;
			}
			const auto items = [aMenu itemArray];
			for (NSUInteger index = 0; index < [items count]; ++index) {
				NSMenuItem* item = [items objectAtIndex:index];
				if (![item isHidden] && titled(item, aItemTitle)) {
					return static_cast<NSInteger>(index);
				}
			}
			return -1;
		}
		/// The visible top-level menu with the title, or with an empty title the
		/// first top-level menu holding the item.
		auto find_menu(std::string_view aMenuTitle, std::string_view aItemTitle) -> NSMenu*
		{
			NSMenu* mainMenu = [NSApp mainMenu];
			if (mainMenu == nil) {
				return nil;
			}
			for (NSMenuItem* item in [mainMenu itemArray]) {
				if ([item isHidden]) {
					continue;
				}
				if (aMenuTitle.empty() ? find_item([item submenu], aItemTitle) >= 0 : titled(item, aMenuTitle)) {
					return [item submenu];
				}
			}
			return nil;
		}
		IMP original_set_main_menu = nullptr;
		void traced_set_main_menu(id aSelf, SEL aSelector, NSMenu* aMenu)
		{
			reinterpret_cast<void (*)(id, SEL, NSMenu*)>(original_set_main_menu)(aSelf, aSelector, aMenu);
			auto&                       state = trace();
			const std::lock_guard<std::mutex> lock{ state.Guard };
			state.Entries.push_back(std::to_string(state.Clock.elapsed()) + "ms setMainMenu: " + pointer(aMenu) + " -> " + describe(aMenu));
		}
	}

	auto describe_main_menu() -> std::string { return describe([NSApp mainMenu]); }

	auto main_menu_is(const void* aNsMenu) -> bool
	{
		return aNsMenu != nullptr && (__bridge const void*)[NSApp mainMenu] == aNsMenu;
	}

	auto main_menu_offers(const std::string_view aMenuTitle, const std::string_view aItemTitle) -> bool
	{
		return find_item(find_menu(aMenuTitle, aItemTitle), aItemTitle) >= 0;
	}

	auto main_menu_visible_titles() -> std::vector<std::string>
	{
		std::vector<std::string> titles;
		NSMenu*                  mainMenu = [NSApp mainMenu];
		if (mainMenu == nil) {
			return titles;
		}
		for (NSMenuItem* item in [mainMenu itemArray]) {
			if ([item isHidden]) {
				continue;
			}
			titles.push_back(text([item title]));
		}
		return titles;
	}

	auto main_menu_items(const std::string_view aMenuTitle) -> std::vector<std::string>
	{
		std::vector<std::string> items;
		if (aMenuTitle.empty()) {
			return items;
		}
		NSMenu* menu = find_menu(aMenuTitle, std::string_view{ });
		if (menu == nil) {
			return items;
		}
		for (NSMenuItem* entry in [menu itemArray]) {
			if ([entry isSeparatorItem] || [entry isHidden]) {
				continue;
			}
			items.push_back(text([entry title]));
		}
		return items;
	}

	auto main_menu_item_is_enabled(const std::string_view aMenuTitle, const std::string_view aItemTitle) -> bool
	{
		NSMenu*         menu  = find_menu(aMenuTitle, aItemTitle);
		const NSInteger index = find_item(menu, aItemTitle);
		return index >= 0 && [[menu itemAtIndex:index] isEnabled] == YES;
	}

	auto application_is_active() -> bool { return [NSApp isActive] == YES; }

	auto application_is_hidden() -> bool { return [NSApp isHidden] == YES; }

	auto session_screen_is_locked() -> bool
	{
		NSDictionary* session = CFBridgingRelease(CGSessionCopyCurrentDictionary());
		return session != nil && [[session objectForKey:@"CGSSessionScreenIsLocked"] boolValue] == YES;
	}

	auto frontmost_application_name() -> std::string
	{
		return text([[[NSWorkspace sharedWorkspace] frontmostApplication] localizedName]);
	}

	auto activation_policy_is_regular() -> bool
	{
		return [NSApp activationPolicy] == NSApplicationActivationPolicyRegular;
	}

	auto visible_window_count() -> std::size_t
	{
		std::size_t count = 0;
		for (NSWindow* window in [NSApp windows]) {
			if ([window isVisible] && !is_status_bar_window(window)) {
				++count;
			}
		}
		return count;
	}

	auto visible_status_item_count() -> std::size_t
	{
		std::size_t count = 0;
		for (NSWindow* window in [NSApp windows]) {
			if ([window isVisible] && is_status_bar_window(window)) {
				++count;
			}
		}
		return count;
	}

	void begin_main_menu_trace()
	{
		auto&                       state = trace();
		const std::lock_guard<std::mutex> lock{ state.Guard };
		state.Entries.clear();
		state.Clock.start();
		if (state.Installed) {
			return;
		}
		Method method = class_getInstanceMethod([NSApplication class], @selector(setMainMenu:));
		if (method == nullptr) {
			return;
		}
		original_set_main_menu = method_setImplementation(method, reinterpret_cast<IMP>(&traced_set_main_menu));
		state.Installed        = original_set_main_menu != nullptr;
	}

	auto main_menu_trace() -> std::vector<std::string>
	{
		auto&                       state = trace();
		const std::lock_guard<std::mutex> lock{ state.Guard };
		return state.Entries;
	}

	void activate_application() { [NSApp activateIgnoringOtherApps:YES]; }

	void hide_application() { [NSApp hide:nil]; }

	void unhide_application() { [NSApp unhide:nil]; }

	auto send_dock_reopen() -> bool
	{
		id<NSApplicationDelegate> delegate = [NSApp delegate];
		if (delegate == nil || ![delegate respondsToSelector:@selector(applicationShouldHandleReopen:hasVisibleWindows:)]) {
			return false;
		}
		return [delegate applicationShouldHandleReopen:NSApp hasVisibleWindows:NO] == YES;
	}

	auto send_command_key(const char aKey) -> bool
	{
		NSString* characters = [NSString stringWithFormat:@"%c", aKey];
		NSEvent*  event      = [NSEvent keyEventWithType:NSEventTypeKeyDown
											 location:NSZeroPoint
										modifierFlags:NSEventModifierFlagCommand
											timestamp:[[NSProcessInfo processInfo] systemUptime]
										 windowNumber:0
											  context:nil
										   characters:characters
						  charactersIgnoringModifiers:characters
											isARepeat:NO
											  keyCode:0];
		if ([NSApp mainMenu] == nil) {
			return false;
		}
		[NSApp sendEvent:event];
		return true;
	}

	auto send_command_key_to_key_window(const char aKey) -> bool
	{
		NSWindow* const window = [NSApp keyWindow];
		if (window == nil || [NSApp mainMenu] == nil) {
			return false;
		}
		NSString* characters = [NSString stringWithFormat:@"%c", aKey];
		NSEvent*  event      = [NSEvent keyEventWithType:NSEventTypeKeyDown
											 location:NSZeroPoint
										modifierFlags:NSEventModifierFlagCommand
											timestamp:[[NSProcessInfo processInfo] systemUptime]
										 windowNumber:[window windowNumber]
											  context:nil
										   characters:characters
						  charactersIgnoringModifiers:characters
											isARepeat:NO
											  keyCode:aKey == 'b' ? 11 : 0];
		[NSApp sendEvent:event];
		return true;
	}

	auto perform_main_menu_item(const std::string_view aMenuTitle, const std::string_view aItemTitle) -> bool
	{
		NSMenu*         menu  = find_menu(aMenuTitle, aItemTitle);
		const NSInteger index = find_item(menu, aItemTitle);
		if (index < 0 || ![[menu itemAtIndex:index] isEnabled]) {
			return false;
		}
		[menu performActionForItemAtIndex:index];
		return true;
	}
}
