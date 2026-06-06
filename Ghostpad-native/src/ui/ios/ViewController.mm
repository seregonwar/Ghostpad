#import "ViewController.h"
#include "imgui.h"
#include "imgui_impl_metal.h"
#include "ui/app.h"
#include <string>
#import <QuartzCore/QuartzCore.h>
#import "../../../ImGuiIOSKeyboardBridge.h"

/*
 *  +-------------------------------------------------------------+
 *  |                METAL DEVICE / TEXTURE LOADER                |
 *  +-------------------------------------------------------------+
 */

static id<MTLDevice> g_metal_device = nil;

namespace ghostpad {
ImTextureID createControllerTexture(const unsigned char* pixels, int width, int height) {
    if (g_metal_device == nil) {
        NSLog(@"[Ghostpad] Error: Metal device is nil, cannot create controller texture");
        return 0;
    }
    
    MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                                  width:width
                                                                                                 height:height
                                                                                              mipmapped:NO];
    textureDescriptor.usage = MTLTextureUsageShaderRead;
    
    id<MTLTexture> texture = [g_metal_device newTextureWithDescriptor:textureDescriptor];
    if (!texture) {
        NSLog(@"[Ghostpad] Error: Failed to create Metal texture");
        return 0;
    }
    
    MTLRegion region = MTLRegionMake2D(0, 0, width, height);
    NSUInteger bytesPerRow = width * 4;
    [texture replaceRegion:region mipmapLevel:0 withBytes:pixels bytesPerRow:bytesPerRow];
    
    return (ImTextureID)(__bridge void*)texture;
}
}

/*
 *  +-------------------------------------------------------------+
 *  |                  VIEW CONTROLLER INTERFACE                  |
 *  +-------------------------------------------------------------+
 */

@interface ViewController () {
    MTKView *_mtkView;
    id<MTLCommandQueue> _commandQueue;
    ghostpad::App *_app;
}
@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    self.view.backgroundColor = [UIColor blackColor];
    
    g_metal_device = MTLCreateSystemDefaultDevice();
    if (!g_metal_device) {
        NSLog(@"[Ghostpad] Metal is not supported on this device");
        return;
    }
    
    _commandQueue = [g_metal_device newCommandQueue];
    
    _mtkView = [[MTKView alloc] initWithFrame:self.view.bounds device:g_metal_device];
    _mtkView.delegate = self;
    _mtkView.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
    _mtkView.depthStencilPixelFormat = MTLPixelFormatInvalid;
    _mtkView.framebufferOnly = YES;
    _mtkView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    _mtkView.contentScaleFactor = [UIScreen mainScreen].nativeScale;
    [self.view addSubview:_mtkView];
    
    // Install iOS keyboard bridge so InputText fields can summon the software keyboard
    ImGuiInstallIOSKeyboardBridge(self.view);
    
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentsDirectory = [paths firstObject];
    std::string dataDir = [documentsDirectory UTF8String];
    
    _app = new ghostpad::App(dataDir);
    _app->init();
    
    ImGui_ImplMetal_Init(g_metal_device);
}

- (void)dealloc {
    if (_app) {
        _app->shutdown();
        delete _app;
        _app = nullptr;
    }
    ImGui_ImplMetal_Shutdown();
    [super dealloc];
}

/*
 *  +-------------------------------------------------------------+
 *  |                     TOUCH EVENT HANDLING                    |
 *  +-------------------------------------------------------------+
 */

- (void)handleTouch:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event state:(BOOL)pressed {
    ImGuiIO& io = ImGui::GetIO();
    for (UITouch *touch in touches) {
        CGPoint location = [touch locationInView:_mtkView];
        io.AddMousePosEvent(location.x, location.y);
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, pressed);
    }
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [self handleTouch:touches withEvent:event state:YES];
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    ImGuiIO& io = ImGui::GetIO();
    UITouch *touch = [touches anyObject];
    CGPoint location = [touch locationInView:_mtkView];
    io.AddMousePosEvent(location.x, location.y);
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [self handleTouch:touches withEvent:event state:NO];
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [self handleTouch:touches withEvent:event state:NO];
}

/*
 *  +-------------------------------------------------------------+
 *  |                     MTKVIEWDELEGATE LOOPS                   |
 *  +-------------------------------------------------------------+
 */

- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size {
    // No-op, dimensions updated during render frame
}

- (void)drawInMTKView:(nonnull MTKView *)view {
    static CFTimeInterval lastTime = 0.0;
    CFTimeInterval currentTime = CACurrentMediaTime();
    if (lastTime == 0.0) {
        lastTime = currentTime;
    }
    double dt = currentTime - lastTime;
    lastTime = currentTime;
    
    _app->update(dt);
    
    CGFloat scale = view.contentScaleFactor;
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(view.bounds.size.width, view.bounds.size.height);
    io.DisplayFramebufferScale = ImVec2(scale, scale);
    
    MTLRenderPassDescriptor *renderPassDescriptor = view.currentRenderPassDescriptor;
    if (renderPassDescriptor == nil) {
        return;
    }
    
    renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.07, 0.07, 0.08, 1.0);
    renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    
    id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
    id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
    
    ImGui_ImplMetal_NewFrame(renderPassDescriptor);
    ImGui::NewFrame();
    
    // Sync iOS software keyboard with ImGui's WantTextInput flag
    ImGuiUpdateIOSKeyboardBridge();
    
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::Begin("Ghostpad Native", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoBringToFrontOnFocus |
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    
    _app->drawAppChrome();
    
    ImGui::End();
    
    ImGui::Render();
    ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), commandBuffer, renderEncoder);
    
    [renderEncoder endEncoding];
    [commandBuffer presentDrawable:view.currentDrawable];
    [commandBuffer commit];
}

@end

/*
 *  +-------------------------------------------------------------+
 *  |                    PORTABLE HELPERS                         |
 *  +-------------------------------------------------------------+
 */

extern "C" void openBrowserURL(const char* url) {
    NSString *urlString = [NSString stringWithUTF8String:url];
    NSURL *nsUrl = [NSURL URLWithString:urlString];
    dispatch_async(dispatch_get_main_queue(), ^{
        [[UIApplication sharedApplication] openURL:nsUrl options:@{} completionHandler:nil];
    });
}


