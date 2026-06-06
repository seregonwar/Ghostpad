// Ghostpad Native - iOS File Picker
// Uses UIDocumentPickerViewController with a proper delegate

#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <objc/runtime.h>
#include <string>
#include <dispatch/dispatch.h>

// ── Delegate class that captures the picked URL ──────────────────────────
@interface GhostpadFilePickerDelegate : NSObject <UIDocumentPickerDelegate>
@property (nonatomic, copy) void (^completionHandler)(NSURL* _Nullable url);
@end

@implementation GhostpadFilePickerDelegate

- (void)documentPicker:(UIDocumentPickerViewController *)controller
didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
    NSURL* picked = urls.firstObject;
    // Start accessing security-scoped resource
    [picked startAccessingSecurityScopedResource];
    if (self.completionHandler) {
        self.completionHandler(picked);
    }
    [controller dismissViewControllerAnimated:YES completion:nil];
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController *)controller {
    if (self.completionHandler) {
        self.completionHandler(nil);
    }
    [controller dismissViewControllerAnimated:YES completion:nil];
}

@end

namespace ghostpad::ui {

std::string pickFileIOS(const std::string& title, const std::string& filter_ext) {
    __block std::string result;
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);

    dispatch_async(dispatch_get_main_queue(), ^{
        // Determine UTI types from extension
        NSArray<UTType*>* types = nil;
        if (!filter_ext.empty() && filter_ext != "*.*") {
            NSString* ext = [NSString stringWithUTF8String:filter_ext.c_str()];
            if ([ext hasPrefix:@"*."]) ext = [ext substringFromIndex:2];
            else if ([ext hasPrefix:@"."]) ext = [ext substringFromIndex:1];
            UTType* utType = [UTType typeWithFilenameExtension:ext];
            if (utType) types = @[utType];
        }
        if (!types || types.count == 0) {
            types = @[UTTypeData, UTTypeContent];
        }

        UIDocumentPickerViewController* picker =
            [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:types];

        // Delegate
        GhostpadFilePickerDelegate* delegate = [[GhostpadFilePickerDelegate alloc] init];
        delegate.completionHandler = ^(NSURL* url) {
            if (url) {
                result = [[url path] UTF8String];
                [url stopAccessingSecurityScopedResource];
            }
            dispatch_semaphore_signal(sem);
        };
        picker.delegate = delegate;

        // Hold a strong reference to delegate so it outlives the block
        objc_setAssociatedObject(picker, "delegateKeeper", delegate, OBJC_ASSOCIATION_RETAIN);

        // Find topmost VC
        UIViewController* rootVC = nil;
        for (UIScene* scene in [[UIApplication sharedApplication] connectedScenes]) {
            if ([scene isKindOfClass:[UIWindowScene class]]) {
                UIWindowScene* windowScene = (UIWindowScene*)scene;
                rootVC = windowScene.windows.firstObject.rootViewController;
                if (rootVC) break;
            }
        }
        if (!rootVC) {
            // Fallback for older iOS versions
            #pragma clang diagnostic push
            #pragma clang diagnostic ignored "-Wdeprecated-declarations"
            rootVC = [[[UIApplication sharedApplication] keyWindow] rootViewController];
            #pragma clang diagnostic pop
        }
        while (rootVC.presentedViewController) {
            rootVC = rootVC.presentedViewController;
        }

        @try {
            [rootVC presentViewController:picker animated:YES completion:nil];
        } @catch (NSException* e) {
            NSLog(@"[Ghostpad] Failed to present file picker: %@", e);
            dispatch_semaphore_signal(sem);
        }
    });

    // Wait max 120s for user to pick a file
    dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 120 * NSEC_PER_SEC));
    return result;
}

} // namespace ghostpad::ui
