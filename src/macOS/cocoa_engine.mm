//
// ====================================================================
//
// This implementation uses Cocoa WKWebView backend on macOS. It is
// written using ObjC runtime and uses WKWebView class as a browser runtime.
// You should pass "-framework Webkit" flag to the compiler.
//
// ====================================================================
//

#include <AppKit/AppKit.h>
#include <CoreGraphics/CoreGraphics.h> // tbd can remove?
#include <WebKit/WebKit.h>
#include <objc/NSObjCRuntime.h>
#include <objc/objc-runtime.h>

#include <functional>
#include <string>

#include "webview.h"
#include "cocoa_engine.hpp"

namespace webview {
using dispatch_fn_t = std::function<void()>;

cocoa_wkwebview_engine::cocoa_wkwebview_engine(bool debug, void *window)
    : m_debug{debug}, m_parent_window{window} {
  id app = get_shared_application();
  id delegate = create_app_delegate();
  objc_setAssociatedObject(delegate, "webview", (id)this,
                           OBJC_ASSOCIATION_ASSIGN);
  objc::msg_send<void>(app, "setDelegate:"_sel, delegate);

  // See comments related to application lifecycle in create_app_delegate().
  if (window) {
    on_application_did_finish_launching(delegate, app);
  } else {
    // Start the main run loop so that the app delegate gets the
    // NSApplicationDidFinishLaunchingNotification notification after the run
    // loop has started in order to perform further initialization.
    // We need to return from this constructor so this run loop is only
    // temporary.
    objc::msg_send<void>(app, "run"_sel);
  }
}
void *cocoa_wkwebview_engine::window() { return (void *)m_window; }
void cocoa_wkwebview_engine::terminate() {
  id app = get_shared_application();
  objc::msg_send<void>(app, "terminate:"_sel, nullptr);
}
void cocoa_wkwebview_engine::run() {
  id app = get_shared_application();
  objc::msg_send<void>(app, "run"_sel);
}
void cocoa_wkwebview_engine::dispatch(std::function<void()> f) {
  dispatch_async_f(dispatch_get_main_queue(), new dispatch_fn_t(f),
                   (dispatch_function_t)([](void *arg) {
                     auto f = static_cast<dispatch_fn_t *>(arg);
                     (*f)();
                     delete f;
                   }));
}
void cocoa_wkwebview_engine::set_title(const std::string &title) {
  objc::msg_send<void>(m_window, "setTitle:"_sel,
                       objc::msg_send<id>("NSString"_cls,
                                          "stringWithUTF8String:"_sel,
                                          title.c_str()));
}
void cocoa_wkwebview_engine::set_size(int width, int height, int hints) {
  auto style = static_cast<NSWindowStyleMask>(NSWindowStyleMaskTitled |
                                              NSWindowStyleMaskClosable |
                                              NSWindowStyleMaskMiniaturizable);
  if (hints != WEBVIEW_HINT_FIXED) {
    style = static_cast<NSWindowStyleMask>(style | NSWindowStyleMaskResizable);
  }
  objc::msg_send<void>(m_window, "setStyleMask:"_sel, style);

  if (hints == WEBVIEW_HINT_MIN) {
    objc::msg_send<void>(m_window, "setContentMinSize:"_sel,
                         CGSizeMake(width, height));
  } else if (hints == WEBVIEW_HINT_MAX) {
    objc::msg_send<void>(m_window, "setContentMaxSize:"_sel,
                         CGSizeMake(width, height));
  } else {
    objc::msg_send<void>(m_window, "setFrame:display:animate:"_sel,
                         CGRectMake(0, 0, width, height), YES, NO);
  }
  objc::msg_send<void>(m_window, "center"_sel);
}
void cocoa_wkwebview_engine::navigate(const std::string &urlString) {
  id url = objc::msg_send<id>("NSURL"_cls, "URLWithString:"_sel,
                              objc::msg_send<id>("NSString"_cls,
                                                 "stringWithUTF8String:"_sel,
                                                 urlString.c_str()));

  NSURLRequest *request = [NSURLRequest requestWithURL:url];
  [(WKWebView *)m_webview loadRequest:request];
}
void cocoa_wkwebview_engine::set_html(const std::string &html) {
  NSString *htmlString = [NSString stringWithUTF8String:html.c_str()];
  [(WKWebView *)m_webview loadHTMLString:htmlString baseURL:nil];
}
void cocoa_wkwebview_engine::init(const std::string &js) {
  NSString *jsString = [NSString stringWithUTF8String:js.c_str()];

  WKUserScript *userScript = [[WKUserScript alloc]
        initWithSource:jsString
         injectionTime:WKUserScriptInjectionTimeAtDocumentStart
      forMainFrameOnly:YES];

  [m_manager addUserScript:userScript];
}
void cocoa_wkwebview_engine::eval(const std::string &js) {
  NSString *jsString = [NSString stringWithUTF8String:js.c_str()];

  [m_webview evaluateJavaScript:jsString
              completionHandler:^(id result, NSError *error){
                  // Handle result or error here, if needed?
                  // if (error) {
                  //   NSLog(@"JavaScript evaluation error: %@", error);
                  // } else {
                  //   NSLog(@"JavaScript evaluation result: %@", result);
                  // }
              }];
}

id cocoa_wkwebview_engine::create_app_delegate() {
  // Note: Avoid registering the class name "AppDelegate" as it is the
  // default name in projects created with Xcode, and using the same name
  // causes objc_registerClassPair to crash.
  auto cls = objc_allocateClassPair((Class) "NSResponder"_cls,
                                    "WebviewAppDelegate", 0);
  class_addProtocol(cls, objc_getProtocol("NSTouchBarProvider"));
  class_addMethod(cls, "applicationShouldTerminateAfterLastWindowClosed:"_sel,
                  (IMP)(+[](id, SEL, id) -> BOOL { return 1; }), "c@:@");
  // If the library was not initialized with an existing window then the user
  // is likely managing the application lifecycle and we would not get the
  // "applicationDidFinishLaunching:" message and therefore do not need to
  // add this method.
  if (!m_parent_window) {
    class_addMethod(cls, "applicationDidFinishLaunching:"_sel,
                    (IMP)(+[](id self, SEL, id notification) {
                      id app = objc::msg_send<id>(notification, "object"_sel);
                      auto w = get_associated_webview(self);
                      w->on_application_did_finish_launching(self, app);
                    }),
                    "v@:@");
  }
  objc_registerClassPair(cls);
  return objc::msg_send<id>((id)cls, "new"_sel);
}
id cocoa_wkwebview_engine::create_script_message_handler() {
  auto cls = objc_allocateClassPair((Class) "NSResponder"_cls,
                                    "WebkitScriptMessageHandler", 0);
  class_addProtocol(cls, objc_getProtocol("WKScriptMessageHandler"));
  class_addMethod(cls, "userContentController:didReceiveScriptMessage:"_sel,
                  (IMP)(+[](id self, SEL, id, id msg) {
                    auto w = get_associated_webview(self);
                    w->on_message(objc::msg_send<const char *>(
                        objc::msg_send<id>(msg, "body"_sel), "UTF8String"_sel));
                  }),
                  "v@:@@");
  objc_registerClassPair(cls);
  id instance = objc::msg_send<id>((id)cls, "new"_sel);
  objc_setAssociatedObject(instance, "webview", (id)this,
                           OBJC_ASSOCIATION_ASSIGN);
  return instance;
}
id cocoa_wkwebview_engine::create_webkit_ui_delegate() {
  auto cls =
      objc_allocateClassPair((Class) "NSObject"_cls, "WebkitUIDelegate", 0);
  class_addProtocol(cls, objc_getProtocol("WKUIDelegate"));
  class_addMethod(
      cls,
      "webView:runOpenPanelWithParameters:initiatedByFrame:completionHandler:"_sel,
      (IMP)(+[](id, SEL, id, id parameters, id, id completion_handler) {
        auto allows_multiple_selection =
            objc::msg_send<BOOL>(parameters, "allowsMultipleSelection"_sel);
        auto allows_directories =
            objc::msg_send<BOOL>(parameters, "allowsDirectories"_sel);

        // Show a panel for selecting files.
        id panel = objc::msg_send<id>("NSOpenPanel"_cls, "openPanel"_sel);
        objc::msg_send<void>(panel, "setCanChooseFiles:"_sel, YES);
        objc::msg_send<void>(panel, "setCanChooseDirectories:"_sel,
                             allows_directories);
        objc::msg_send<void>(panel, "setAllowsMultipleSelection:"_sel,
                             allows_multiple_selection);
        auto modal_response =
            objc::msg_send<NSModalResponse>(panel, "runModal"_sel);

        // Get the URLs for the selected files. If the modal was canceled
        // then we pass null to the completion handler to signify
        // cancellation.
        id urls = modal_response == NSModalResponseOK
                      ? objc::msg_send<id>(panel, "URLs"_sel)
                      : nullptr;

        // Invoke the completion handler block.
        id sig = objc::msg_send<id>("NSMethodSignature"_cls,
                                    "signatureWithObjCTypes:"_sel, "v@?@");
        id invocation = objc::msg_send<id>(
            "NSInvocation"_cls, "invocationWithMethodSignature:"_sel, sig);
        objc::msg_send<void>(invocation, "setTarget:"_sel, completion_handler);
        objc::msg_send<void>(invocation, "setArgument:atIndex:"_sel, &urls, 1);
        objc::msg_send<void>(invocation, "invoke"_sel);
      }),
      "v@:@@@@");
  objc_registerClassPair(cls);
  return objc::msg_send<id>((id)cls, "new"_sel);
}
id cocoa_wkwebview_engine::get_shared_application() {
  return [NSApplication sharedApplication];
}
cocoa_wkwebview_engine *
cocoa_wkwebview_engine::get_associated_webview(id object) {
  auto w =
      (cocoa_wkwebview_engine *)objc_getAssociatedObject(object, "webview");
  assert(w);
  return w;
}
id cocoa_wkwebview_engine::get_main_bundle() noexcept {
  return [NSBundle mainBundle];
}
bool cocoa_wkwebview_engine::is_app_bundled() noexcept {
  NSBundle *bundle = get_main_bundle();
  if (!bundle) {
    return false;
  }
  NSString *bundlePath = [bundle bundlePath];
  BOOL bundled = [bundlePath hasSuffix:@".app"];
  return bundled;
}
void cocoa_wkwebview_engine::on_application_did_finish_launching(
    id /*delegate*/, id app) {
  // See comments related to application lifecycle in create_app_delegate().
  if (!m_parent_window) {
    // Stop the main run loop so that we can return
    // from the constructor.
    [app stop:nil];
  }

  // Activate the app if it is not bundled.
  // Bundled apps launched from Finder are activated automatically but
  // otherwise not. Activating the app even when it has been launched from
  // Finder does not seem to be harmful but calling this function is rarely
  // needed as proper activation is normally taken care of for us.
  // Bundled apps have a default activation policy of
  // NSApplicationActivationPolicyRegular while non-bundled apps have a
  // default activation policy of NSApplicationActivationPolicyProhibited.
  if (!is_app_bundled()) {
    // "setActivationPolicy:" must be invoked before
    // "activateIgnoringOtherApps:" for activation to work.
    [app setActivationPolicy:NSApplicationActivationPolicyRegular];
    // Activate the app regardless of other active apps.
    // This can be obtrusive so we only do it when necessary.
    [app activateIgnoringOtherApps:YES];
  }

  // Main window
  if (!m_parent_window) {
    NSRect frame = NSMakeRect(0, 0, 0, 0);
    auto style = NSWindowStyleMaskTitled;
    m_window = [[[NSWindow alloc] initWithContentRect:frame
                                            styleMask:style
                                              backing:NSBackingStoreBuffered
                                                defer:NO]
        autorelease]; // TBD autorelease useful or not?
  } else
    m_window = (id)m_parent_window;

  // Webview
  id config = [WKWebViewConfiguration new];
  m_manager = [config userContentController];
  m_webview = [WKWebView alloc];

  if (m_debug)
    [[config preferences] setValue:@YES forKey:@"developerExtrasEnabled"];
  [[config preferences] setValue:@YES forKey:@"fullScreenEnabled"];
  [[config preferences] setValue:@YES forKey:@"javaScriptCanAccessClipboard"];
  [[config preferences] setValue:@YES forKey:@"DOMPasteAllowed"];

  id ui_delegate = create_webkit_ui_delegate();
  NSRect frame = NSMakeRect(0, 0, 0, 0);

  [m_webview initWithFrame:frame configuration:config];
  [m_webview setUIDelegate:ui_delegate];
  id script_message_handler = create_script_message_handler();
  [m_manager addScriptMessageHandler:script_message_handler name:@"external"];

  init(R""(
      window.external = {
        invoke: function(s) {
          window.webkit.messageHandlers.external.postMessage(s);
        },
      };
      )"");
  [m_window setContentView:m_webview];
  [m_window makeKeyAndOrderFront:nil];
}

} // namespace webview
