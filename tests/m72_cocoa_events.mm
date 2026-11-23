#import <AppKit/AppKit.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>

// Post only to this test's AppKit queue. glfwPollEvents dispatches the NSEvent
// through the Cocoa window delegate, GLFW's key cache and the installed chain.
void postCocoaResetKey(GLFWwindow* window, bool down, bool repeat) {
    NSWindow* native = glfwGetCocoaWindow(window);
    NSEvent* event = [NSEvent keyEventWithType:(down ? NSEventTypeKeyDown : NSEventTypeKeyUp)
        location:NSZeroPoint modifierFlags:0 timestamp:NSProcessInfo.processInfo.systemUptime
        windowNumber:native.windowNumber context:nil characters:@"r" charactersIgnoringModifiers:@"r"
        isARepeat:repeat keyCode:static_cast<unsigned short>(glfwGetKeyScancode(GLFW_KEY_R))];
    [NSApp postEvent:event atStart:YES];
    glfwPollEvents();
}
