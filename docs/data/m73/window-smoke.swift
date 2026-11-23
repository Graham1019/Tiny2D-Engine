// Launch the standalone Survivor and verify its own on-screen window on macOS.
import Foundation
import CoreGraphics

let executable = URL(fileURLWithPath: CommandLine.arguments[1]).standardizedFileURL
let app = Process()
app.executableURL = executable
try app.run()
var found = false
for _ in 0..<100 {
    let windows = CGWindowListCopyWindowInfo([.optionOnScreenOnly, .excludeDesktopElements], kCGNullWindowID) as? [[String: Any]] ?? []
    if let window = windows.first(where: {
        ($0[kCGWindowOwnerPID as String] as? Int32) == app.processIdentifier && ($0[kCGWindowLayer as String] as? Int) == 0
    }) {
        print("Visible Survivor window for PID", app.processIdentifier, window[kCGWindowBounds as String] ?? "")
        found = true
        break
    }
    if !app.isRunning { break }
    Thread.sleep(forTimeInterval: 0.05)
}
if app.isRunning { app.terminate() }
app.waitUntilExit()
if !found { exit(1) }
