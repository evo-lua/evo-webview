#pragma once

#include <CoreGraphics/CoreGraphics.h>
#include <objc/NSObjCRuntime.h>
#include <objc/objc-runtime.h>

#include <functional>
#include <string>

#include "webview.h"

namespace webview {
using dispatch_fn_t = std::function<void()>;

namespace objc {

// A convenient template function for unconditionally casting the specified
// C-like function into a function that can be called with the given return
// type and arguments. Caller takes full responsibility for ensuring that
// the function call is valid. It is assumed that the function will not
// throw exceptions.
template <typename Result, typename Callable, typename... Args>
Result invoke(Callable callable, Args... args) noexcept {
  return reinterpret_cast<Result (*)(Args...)>(callable)(args...);
}

// Calls objc_msgSend.
template <typename Result, typename... Args>
Result msg_send(Args... args) noexcept {
  return invoke<Result>(objc_msgSend, args...);
}

} // namespace objc

// enum NSBackingStoreType : NSUInteger { NSBackingStoreBuffered = 2 };

enum NSWindowStyleMask : NSUInteger {
  NSWindowStyleMaskTitled = 1,
  NSWindowStyleMaskClosable = 2,
  NSWindowStyleMaskMiniaturizable = 4,
  NSWindowStyleMaskResizable = 8
};

// enum NSApplicationActivationPolicy : NSInteger {
//   NSApplicationActivationPolicyRegular = 0
// };

enum WKUserScriptInjectionTime : NSInteger {
  WKUserScriptInjectionTimeAtDocumentStart = 0
};

enum NSModalResponse : NSInteger { NSModalResponseOK = 1 };

// Convenient conversion of string literals.
inline id operator"" _cls(const char *s, std::size_t) {
  return (id)objc_getClass(s);
}
inline SEL operator"" _sel(const char *s, std::size_t) {
  return sel_registerName(s);
}
inline id operator"" _str(const char *s, std::size_t) {
  return objc::msg_send<id>("NSString"_cls, "stringWithUTF8String:"_sel, s);
}

class cocoa_wkwebview_engine {
public:
  cocoa_wkwebview_engine(bool debug, void *window);

  virtual ~cocoa_wkwebview_engine() = default;
  void *window();
  void terminate();
  void run();
  void dispatch(std::function<void()> f);
  void set_title(const std::string &title);
  void set_size(int width, int height, int hints);
  void navigate(const std::string &url);
  void set_html(const std::string &html);
  void init(const std::string &js);
  void eval(const std::string &js);

private:
  virtual void on_message(const std::string &msg) = 0;
  id create_app_delegate();
  id create_script_message_handler();
  static id create_webkit_ui_delegate();
  static id get_shared_application();
  static cocoa_wkwebview_engine *get_associated_webview(id object);
  static id get_main_bundle() noexcept;
  static bool is_app_bundled() noexcept;
  void on_application_did_finish_launching(id delegate, id app);
  bool m_debug;

  void *m_parent_window;
  id m_window;
  id m_webview;
  id m_manager;
};

using browser_engine = cocoa_wkwebview_engine;

} // namespace webview
