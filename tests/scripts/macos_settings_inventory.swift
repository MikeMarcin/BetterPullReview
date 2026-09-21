// Native external-process evidence for RFC-BPR-0018 AC-01 and AC-11.
// Run only against a disposable signed-out application launched on a private
// CFFIXED_USER_HOME, with network egress and the personal app-data root denied.
// Arguments: PID EXPECTED_EXECUTABLE BUILD PRODUCT_REVISION OUTPUT_JSON.
// This drives only Settings, New Window and Quit in that exact process. It
// never presses a setting, signs in, reads editable values or walks Apple/Services.
import AppKit
import ApplicationServices
import CryptoKit
import Foundation

struct CourseFailure: Error, CustomStringConvertible {
    let description: String
}

func require(_ condition: Bool, _ message: String) throws {
    if !condition { throw CourseFailure(description: message) }
}

func attribute(_ element: AXUIElement, _ key: String) -> CFTypeRef? {
    var result: CFTypeRef?
    return AXUIElementCopyAttributeValue(element, key as CFString, &result) == .success ? result : nil
}

func text(_ element: AXUIElement, _ key: String = kAXTitleAttribute) -> String {
    attribute(element, key) as? String ?? ""
}

func children(_ element: AXUIElement) -> [AXUIElement] {
    attribute(element, kAXChildrenAttribute) as? [AXUIElement] ?? []
}

func elementAttribute(_ element: AXUIElement, _ key: String) -> AXUIElement? {
    guard let value = attribute(element, key), CFGetTypeID(value) == AXUIElementGetTypeID() else { return nil }
    return (value as! AXUIElement)
}

func waitFor(_ message: String, _ predicate: () -> Bool) throws {
    let deadline = Date().addingTimeInterval(8)
    while !predicate() && Date() < deadline { RunLoop.current.run(until: Date().addingTimeInterval(0.05)) }
    try require(predicate(), message)
}

func press(_ element: AXUIElement) throws {
    try require(AXUIElementPerformAction(element, kAXPressAction as CFString) == .success,
                "AXPress refused for \(text(element))")
}

struct Node: Encodable {
    let depth: Int
    let type: String
    let name: String
    let enabled: Bool
    let state: String
}

func nodes(_ element: AXUIElement, depth: Int = 0) throws -> [Node] {
    try require(depth < 32, "unbounded accessibility depth")
    let role = text(element, kAXRoleAttribute)
    // Qt's native bridge gives static text its name as AXValue. Editable values
    // are deliberately not read. Some native combo titles are current choices;
    // the cross-platform comparison must account for that, not rename them away.
    let name = role == kAXStaticTextRole ? text(element, kAXValueAttribute) : text(element)
    let toggle = role == kAXCheckBoxRole ? (attribute(element, kAXValueAttribute) as? NSNumber)?.intValue : nil
    var result = [Node(depth: depth, type: role, name: name,
                       enabled: (attribute(element, kAXEnabledAttribute) as? Bool) ?? true,
                       state: toggle.map { $0 == 0 ? "Off" : "On" } ?? "")]
    for child in children(element) { result += try nodes(child, depth: depth + 1) }
    try require(result.count < 2000, "unbounded accessibility page")
    return result
}

struct Page: Encodable {
    let index: Int
    let category: String
    let nodes: [Node]
}

struct Capture: Encodable {
    let schema = "bpr.settings-inventory.ax.v1"
    let build: String
    let revision: String
    let executable_sha256: String
    let platform: String
    let source: String
    let categories: [String]
    let native_checks: [String]
    let pages: [Page]
}

func course() throws {
    let args = CommandLine.arguments
    try require(args.count == 6, "usage: swift macos_settings_inventory.swift PID EXPECTED_EXECUTABLE BUILD PRODUCT_REVISION OUTPUT_JSON")
    guard let pid = pid_t(args[1]), let running = NSRunningApplication(processIdentifier: pid),
          let executable = running.executableURL else { throw CourseFailure(description: "application PID absent") }
    try require(executable.resolvingSymlinksInPath().path == URL(fileURLWithPath: args[2]).resolvingSymlinksInPath().path,
                "PID does not name the expected executable")
    try require(AXIsProcessTrusted(), "native accessibility permission unavailable")
    let digest = SHA256.hash(data: try Data(contentsOf: executable)).map { String(format: "%02x", $0) }.joined()
    let app = AXUIElementCreateApplication(pid)
    let categories = ["General", "Accounts & hosts", "Reviews & queue", "Appearance",
                      "Notifications", "Agents & context", "Storage & privacy"]
    func windows() -> [AXUIElement] { attribute(app, kAXWindowsAttribute) as? [AXUIElement] ?? [] }
    func settings() -> [AXUIElement] { windows().filter { text($0).hasPrefix("Settings — ") } }
    func workbenches() -> [AXUIElement] { windows().filter { text($0) == "BetterPullReview — Default profile" } }
    func focus(_ window: AXUIElement) throws {
        _ = running.activate(options: [])
        try require(AXUIElementPerformAction(window, kAXRaiseAction as CFString) == .success, "AXRaise refused")
        _ = AXUIElementSetAttributeValue(window, kAXMainAttribute as CFString, kCFBooleanTrue)
        try waitFor("target window did not become active") {
            guard let focused = elementAttribute(app, kAXFocusedWindowAttribute) else { return false }
            return running.isActive && CFEqual(focused, window)
        }
    }
    func menuItem(_ title: String) throws -> AXUIElement {
        guard let bar = elementAttribute(app, kAXMenuBarAttribute) else { throw CourseFailure(description: "no native menu bar") }
        // Direct children only: never enumerate the system Apple menu or Services.
        for menu in children(bar) where text(menu) != "Apple" {
            for submenu in children(menu) {
                if let item = children(submenu).first(where: { text($0) == title }) { return item }
            }
        }
        throw CourseFailure(description: "native menu item absent: \(title)")
    }
    func key(_ code: CGKeyCode, command: Bool = false) throws {
        for down in [true, false] {
            guard let event = CGEvent(keyboardEventSource: nil, virtualKey: code, keyDown: down) else {
                throw CourseFailure(description: "native key event unavailable")
            }
            event.flags = command ? .maskCommand : []
            event.postToPid(pid)
        }
        Thread.sleep(forTimeInterval: 0.15)
    }
    func close(_ window: AXUIElement) throws {
        guard let button = elementAttribute(window, kAXCloseButtonAttribute) else { throw CourseFailure(description: "close button absent") }
        try press(button)
    }
    for window in settings() { try close(window) }
    try waitFor("one initial signed-out workbench required") { workbenches().count == 1 && settings().isEmpty }
    let first = workbenches()[0]
    try focus(first)
    let preferences = try menuItem("Preferences...")
    try require(text(preferences, kAXMenuItemCmdCharAttribute) == ",", "Preferences lacks the native comma equivalent")
    try require((attribute(preferences, kAXMenuItemCmdModifiersAttribute) as? NSNumber)?.intValue == 0,
                "Preferences equivalent is not Command-comma")
    try press(preferences)
    try waitFor("native Preferences did not open one labelled Settings window") {
        settings().count == 1 && text(settings()[0]) == "Settings — Default profile"
    }
    // Qt recreates its AX and WindowServer identities on hide/show. Compare
    // native identity across visible invocations, and count/name/focus after
    // reopening. qt_settings_window separately checks the retained Qt object.
    var shared = settings()[0]
    func oneSettings() -> Bool { settings().count == 1 && text(settings()[0]) == "Settings — Default profile" }
    func sameSettings() -> Bool { oneSettings() && CFEqual(settings()[0], shared) }
    func settingsFocused() -> Bool {
        guard oneSettings(), let focused = elementAttribute(app, kAXFocusedWindowAttribute) else { return false }
        return CFEqual(focused, settings()[0])
    }
    try focus(first)
    try require(sameSettings(), "Settings is modal or lost when the workbench is focused")
    try key(43, command: true) // macOS virtual key: comma.
    try waitFor("Command-comma did not reuse and focus Settings") { sameSettings() && settingsFocused() }
    try focus(shared)
    try key(53) // Escape, with no child popup open.
    try waitFor("Escape did not hide Settings") { settings().isEmpty }
    try focus(first)
    try key(43, command: true)
    try waitFor("Command-comma did not reopen one profile-labelled Settings") { oneSettings() && settingsFocused() }
    shared = settings()[0]
    try focus(first)
    try press(menuItem("New Window"))
    try waitFor("native New Window did not open a same-profile sibling") { workbenches().count == 2 }
    guard let second = workbenches().first(where: { !CFEqual($0, first) }) else { throw CourseFailure(description: "sibling identity missing") }
    try focus(second)
    try press(menuItem("Preferences..."))
    try waitFor("sibling Preferences did not reuse and focus the profile's Settings") { sameSettings() && settingsFocused() }
    try close(shared)
    try waitFor("native close did not hide Settings") { settings().isEmpty }
    try focus(second)
    try key(43, command: true)
    try waitFor("sibling Command-comma did not reopen one Settings") { oneSettings() && settingsFocused() }
    shared = settings()[0]
    // The page-owning window is the second one. Retiring its sibling must not
    // disturb the mounted page. Retiring the owner itself deliberately hides
    // Settings before unmounting its controls (the profile course tests reopen).
    try close(first)
    try waitFor("closing the non-owning workbench lost the shared Settings") { workbenches().count == 1 && sameSettings() }
    try focus(shared)
    guard let sidebar = children(shared).first(where: { text($0) == "Settings pages" }),
          let page = children(shared).first(where: { text($0) == "Settings page" }) else {
        throw CourseFailure(description: "Settings sidebar/page absent")
    }
    let rows = children(sidebar).filter { text($0, kAXRoleAttribute) == kAXRowRole }
    try require(rows.map { children($0).map { text($0) }.joined() } == categories, "category inventory/order differs")
    try require(AXUIElementSetAttributeValue(sidebar, kAXFocusedAttribute as CFString, kCFBooleanTrue) == .success,
                "sidebar cannot receive keyboard focus")
    try key(115) // Home selects the first sidebar row.
    var pages: [Page] = []
    for (index, category) in categories.enumerated() {
        if index != 0 { try key(125) } // Down follows the sidebar's natural order.
        try waitFor("sidebar failed to present \(category)") {
            children(page).first.map { text($0, kAXValueAttribute) == category } ?? false
        }
        pages.append(Page(index: index, category: category, nodes: try nodes(page)))
    }
    try close(shared)
    try waitFor("Settings close did not return to the surviving workbench") { settings().isEmpty && workbenches().count == 1 }
    try focus(second)
    try press(menuItem("Preferences..."))
    try waitFor("Preferences did not reopen Settings after all page captures") { oneSettings() && settingsFocused() }
    try focus(second)
    let quitTitle = "Quit " + (running.localizedName ?? "BetterPullReview")
    try press(menuItem(quitTitle))
    try waitFor("native Quit did not terminate the process") { running.isTerminated }
    let capture = Capture(build: args[3], revision: args[4], executable_sha256: digest,
                          platform: ProcessInfo.processInfo.operatingSystemVersionString,
                          source: "Native macOS AX tree below Settings page; keyboard sidebar navigation; editable values and descriptions omitted. AX combo titles can be current choices. No setting or sign-in action invoked.",
                          categories: categories,
                          native_checks: ["Preferences menu has Command-comma equivalent", "Preferences opens Settings — Default profile",
                                          "modeless workbench focus with Settings visible", "Command-comma reuses the Settings identity",
                                          "Escape hides; Command-comma reopens one profile-labelled Settings", "native same-profile New Window",
                                          "sibling menu reuses Settings; sibling shortcut reopens one Settings", "closing sibling preserves Settings",
                                          "seven categories reached by keyboard in order", "native close and Preferences reopen one Settings", "native Quit terminates"],
                          pages: pages)
    let encoder = JSONEncoder()
    encoder.outputFormatting = [.prettyPrinted, .sortedKeys, .withoutEscapingSlashes]
    try encoder.encode(capture).write(to: URL(fileURLWithPath: args[5]), options: .withoutOverwriting)
    print("MACOS_SETTINGS_INVENTORY_OK \(args[3]) \(pages.map { $0.nodes.count }) \(digest)")
}

do { try course() }
catch { fputs("MACOS_SETTINGS_INVENTORY_FAILED: \(error)\n", stderr); exit(1) }
