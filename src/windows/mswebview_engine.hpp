#pragma once

#ifdef _MSC_VER
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "version.lib")
#endif

#include <shlobj.h>
#include <shlwapi.h>
#include <stdlib.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "WebView2.h"

#include <functional>

#include "shared/library_symbol.hpp"
#include "shared/native_library.hpp"

#include "webview2_loader/msedge_runtime_loader.hpp"
#include "webview2_loader/com_event_handler.hpp"

#include "webview.h"
#include "com_init_wrapper.hpp"
#include "mswebview_engine.hpp"
#include "wstring_utils.hpp"

namespace webview {

using dispatch_fn_t = std::function<void()>;
using msg_cb_t = std::function<void(const std::string)>;

using com_event_handler = webview::webview2_loader::com_event_handler;

struct user32_symbols {
  using DPI_AWARENESS_CONTEXT = HANDLE;
  using SetProcessDpiAwarenessContext_t = BOOL(WINAPI *)(DPI_AWARENESS_CONTEXT);
  using SetProcessDPIAware_t = BOOL(WINAPI *)();

  static constexpr auto SetProcessDpiAwarenessContext =
      library_symbol<SetProcessDpiAwarenessContext_t>(
          "SetProcessDpiAwarenessContext");
  static constexpr auto SetProcessDPIAware =
      library_symbol<SetProcessDPIAware_t>("SetProcessDPIAware");
};

struct shcore_symbols {
  typedef enum { PROCESS_PER_MONITOR_DPI_AWARE = 2 } PROCESS_DPI_AWARENESS;
  using SetProcessDpiAwareness_t = HRESULT(WINAPI *)(PROCESS_DPI_AWARENESS);

  static constexpr auto SetProcessDpiAwareness =
      library_symbol<SetProcessDpiAwareness_t>("SetProcessDpiAwareness");
};

inline bool enable_dpi_awareness() {
  auto user32 = native_library(L"user32.dll");
  if (auto fn = user32.get(user32_symbols::SetProcessDpiAwarenessContext)) {
    if (fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE)) {
      return true;
    }
    return GetLastError() == ERROR_ACCESS_DENIED;
  }
  if (auto shcore = native_library(L"shcore.dll")) {
    if (auto fn = shcore.get(shcore_symbols::SetProcessDpiAwareness)) {
      auto result = fn(shcore_symbols::PROCESS_PER_MONITOR_DPI_AWARE);
      return result == S_OK || result == E_ACCESSDENIED;
    }
  }
  if (auto fn = user32.get(user32_symbols::SetProcessDPIAware)) {
    return !!fn();
  }
  return true;
}

class win32_edge_engine {
public:
  win32_edge_engine(bool debug, void *window);

  virtual ~win32_edge_engine();

  win32_edge_engine(const win32_edge_engine &other) = delete;
  win32_edge_engine &operator=(const win32_edge_engine &other) = delete;
  win32_edge_engine(win32_edge_engine &&other) = delete;
  win32_edge_engine &operator=(win32_edge_engine &&other) = delete;

  void run();
  void *window();
  void terminate();
  void dispatch(dispatch_fn_t f);

  void set_title(const std::string &title);

  void set_size(int width, int height, int hints);

  void navigate(const std::string &url);

  void init(const std::string &js);

  void eval(const std::string &js);

  void set_html(const std::string &html);

private:
  bool embed(HWND wnd, bool debug, msg_cb_t cb);

  void resize(HWND wnd);

  bool is_webview2_available() const noexcept;

  virtual void on_message(const std::string &msg) = 0;

  // The app is expected to call CoInitializeEx before
  // CreateCoreWebView2EnvironmentWithOptions.
  // Source: https://docs.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/webview2-idl#createcorewebview2environmentwithoptions
  com_init_wrapper m_com_init{COINIT_APARTMENTTHREADED};
  HWND m_window = nullptr;
  POINT m_minsz = POINT{0, 0};
  POINT m_maxsz = POINT{0, 0};
  DWORD m_main_thread = GetCurrentThreadId();
  ICoreWebView2 *m_webview = nullptr;
  ICoreWebView2Controller *m_controller = nullptr;
  com_event_handler *m_com_handler = nullptr;
  webview::webview2_loader::msedge_runtime_loader m_webview2_loader;
};

using browser_engine = win32_edge_engine;

} // namespace webview