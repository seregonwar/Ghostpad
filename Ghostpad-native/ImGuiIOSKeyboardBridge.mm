// ImGuiIOSKeyboardBridge.mm
#import "ImGuiIOSKeyboardBridge.h"
#import <TargetConditionals.h>

#if TARGET_OS_IOS
#import <UIKit/UIKit.h>
#import <CoreGraphics/CoreGraphics.h>
#include "imgui.h"

@interface ImGuiTextInputView : UIView <UIKeyInput>
@end

static ImGuiTextInputView *gInputView = nil;
static BOOL gKeyboardVisible = NO;

@implementation ImGuiTextInputView

- (BOOL)canBecomeFirstResponder { return YES; }

// UIKeyInput protocol
- (BOOL)hasText { return NO; }

- (void)insertText:(NSString *)text {
    ImGuiIO& io = ImGui::GetIO();
    const char *utf8 = [text UTF8String];
    if (utf8) io.AddInputCharactersUTF8(utf8);

    // Special-case Return -> Enter key
    if ([text isEqualToString:@"\n"]) {
        io.AddKeyEvent(ImGuiKey_Enter, true);
        io.AddKeyEvent(ImGuiKey_Enter, false);
    }
}

- (void)deleteBackward {
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiKey_Backspace, true);
    io.AddKeyEvent(ImGuiKey_Backspace, false);
}

// Basic paste support (optional)
- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
    return action == @selector(paste:);
}
- (void)paste:(id)sender {
    NSString *s = UIPasteboard.generalPasteboard.string;
    if (s.length > 0) {
        ImGui::GetIO().AddInputCharactersUTF8(s.UTF8String);
    }
}

@end

void ImGuiInstallIOSKeyboardBridge(UIView *hostView) {
    if (!hostView) return;
    if (!gInputView) {
        gInputView = [[ImGuiTextInputView alloc] initWithFrame:CGRectMake(0, 0, 0, 0)];
        gInputView.userInteractionEnabled = NO; // invisible, not intercepting touches
        gInputView.backgroundColor = UIColor.clearColor;
        // Place it as a subview so becomeFirstResponder works
        [hostView addSubview:gInputView];
    }
}

void ImGuiUpdateIOSKeyboardBridge(void) {
    if (!gInputView) return;
    ImGuiIO& io = ImGui::GetIO();
    BOOL want = io.WantTextInput ? YES : NO;

    if (want && !gKeyboardVisible) {
        gKeyboardVisible = YES;
        dispatch_async(dispatch_get_main_queue(), ^{
            [gInputView becomeFirstResponder];
        });
    } else if (!want && gKeyboardVisible) {
        gKeyboardVisible = NO;
        dispatch_async(dispatch_get_main_queue(), ^{
            [gInputView resignFirstResponder];
        });
    }
}

#else
// Non-iOS: nothing
#endif // TARGET_OS_IOS


