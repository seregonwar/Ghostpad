#import <TargetConditionals.h>

#if TARGET_OS_IOS
#import <UIKit/UIKit.h>
#ifdef __cplusplus
extern "C" {
#endif

// Installs the hidden UIKeyInput view into the provided host view (e.g., the root view or MTKView's superview)
void ImGuiInstallIOSKeyboardBridge(UIView *hostView);

// Synchronizes keyboard visibility with ImGui's io.WantTextInput. Call once per frame
void ImGuiUpdateIOSKeyboardBridge(void);

#ifdef __cplusplus
}
#endif

#else // !TARGET_OS_IOS

#ifdef __cplusplus
extern "C" {
#endif
// No-op stubs for non-iOS builds so including this header doesn't break other targets
static inline void ImGuiInstallIOSKeyboardBridge(void* /*hostView*/) {}
static inline void ImGuiUpdateIOSKeyboardBridge(void) {}
#ifdef __cplusplus
}
#endif

#endif // TARGET_OS_IOS
