#import <AppKit/AppKit.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>

namespace lab {
// During GLFW's Cocoa key callback, currentEvent is the NSEvent being dispatched.
bool cocoaKeyRepeats(GLFWwindow* window, int scancode) {
    NSEvent* event = NSApp.currentEvent;
    return event.type == NSEventTypeKeyDown && event.window == glfwGetCocoaWindow(window)
        && event.keyCode == scancode && event.isARepeat;
}
}
