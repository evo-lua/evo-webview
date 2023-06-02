//
// ====================================================================
//
// This implementation uses Win32 API to create a native window. It
// uses Edge/Chromium webview2 backend as a browser engine.
//
// ====================================================================
//

#include "mswebview_engine.hpp"

namespace webview {

win32_edge_engine::win32_edge_engine(bool debug, void *window) {
  if (!is_webview2_available()) {
    return;
  }
  if (!m_com_init.is_initialized()) {
    return;
  }
  enable_dpi_awareness();
  if (window == nullptr) {
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    HICON icon = (HICON)LoadImage(hInstance, IDI_APPLICATION, IMAGE_ICON,
                                  GetSystemMetrics(SM_CXICON),
                                  GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(WNDCLASSEX));
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.hInstance = hInstance;
    wc.lpszClassName = L"webview";
    wc.hIcon = icon;
    wc.lpfnWndProc =
        (WNDPROC)(+[](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
          auto w = (win32_edge_engine *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
          switch (msg) {
          case WM_SIZE:
            w->resize(hwnd);
            break;
          case WM_CLOSE:
            DestroyWindow(hwnd);
            break;
          case WM_DESTROY:
            w->terminate();
            break;
          case WM_GETMINMAXINFO: {
            auto lpmmi = (LPMINMAXINFO)lp;
            if (w == nullptr) {
              return 0;
            }
            if (w->m_maxsz.x > 0 && w->m_maxsz.y > 0) {
              lpmmi->ptMaxSize = w->m_maxsz;
              lpmmi->ptMaxTrackSize = w->m_maxsz;
            }
            if (w->m_minsz.x > 0 && w->m_minsz.y > 0) {
              lpmmi->ptMinTrackSize = w->m_minsz;
            }
          } break;
          default:
            return DefWindowProcW(hwnd, msg, wp, lp);
          }
          return 0;
        });
    RegisterClassExW(&wc);
    m_window = CreateWindowW(L"webview", L"", WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, nullptr,
                             nullptr, hInstance, nullptr);
    if (m_window == nullptr) {
      return;
    }
    SetWindowLongPtr(m_window, GWLP_USERDATA, (LONG_PTR)this);
  } else {
    m_window = *(static_cast<HWND *>(window));
  }

  ShowWindow(m_window, SW_SHOW);
  UpdateWindow(m_window);
  SetFocus(m_window);

  auto cb =
      std::bind(&win32_edge_engine::on_message, this, std::placeholders::_1);

  embed(m_window, debug, cb);
  resize(m_window);
  m_controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
}

win32_edge_engine::~win32_edge_engine() {
  if (m_com_handler) {
    m_com_handler->Release();
    m_com_handler = nullptr;
  }
  if (m_webview) {
    m_webview->Release();
    m_webview = nullptr;
  }
  if (m_controller) {
    m_controller->Release();
    m_controller = nullptr;
  }
}

void win32_edge_engine::run() {
  MSG msg;
  BOOL res;
  while ((res = GetMessage(&msg, nullptr, 0, 0)) != -1) {
    if (msg.hwnd) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      continue;
    }
    if (msg.message == WM_APP) {
      auto f = (dispatch_fn_t *)(msg.lParam);
      (*f)();
      delete f;
    } else if (msg.message == WM_QUIT) {
      return;
    }
  }
}
void *win32_edge_engine::window() { return (void *)m_window; }
void win32_edge_engine::terminate() { PostQuitMessage(0); }
void win32_edge_engine::dispatch(dispatch_fn_t f) {
  PostThreadMessage(m_main_thread, WM_APP, 0, (LPARAM) new dispatch_fn_t(f));
}

void win32_edge_engine::set_title(const std::string &title) {
  SetWindowTextW(m_window, webview::wstring::widen_string(title).c_str());
}

void win32_edge_engine::set_size(int width, int height, int hints) {
  auto style = GetWindowLong(m_window, GWL_STYLE);
  if (hints == WEBVIEW_HINT_FIXED) {
    style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
  } else {
    style |= (WS_THICKFRAME | WS_MAXIMIZEBOX);
  }
  SetWindowLong(m_window, GWL_STYLE, style);

  if (hints == WEBVIEW_HINT_MAX) {
    m_maxsz.x = width;
    m_maxsz.y = height;
  } else if (hints == WEBVIEW_HINT_MIN) {
    m_minsz.x = width;
    m_minsz.y = height;
  } else {
    RECT r;
    r.left = r.top = 0;
    r.right = width;
    r.bottom = height;
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, 0);
    SetWindowPos(m_window, nullptr, r.left, r.top, r.right - r.left,
                 r.bottom - r.top,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE | SWP_FRAMECHANGED);
    resize(m_window);
  }
}

void win32_edge_engine::navigate(const std::string &url) {
  auto wurl = webview::wstring::widen_string(url);
  m_webview->Navigate(wurl.c_str());
}

void win32_edge_engine::init(const std::string &js) {
  auto wjs = webview::wstring::widen_string(js);
  m_webview->AddScriptToExecuteOnDocumentCreated(wjs.c_str(), nullptr);
}

void win32_edge_engine::eval(const std::string &js) {
  auto wjs = webview::wstring::widen_string(js);
  m_webview->ExecuteScript(wjs.c_str(), nullptr);
}

void win32_edge_engine::set_html(const std::string &html) {
  m_webview->NavigateToString(webview::wstring::widen_string(html).c_str());
}

bool win32_edge_engine::embed(HWND wnd, bool debug, msg_cb_t cb) {
  std::atomic_flag flag = ATOMIC_FLAG_INIT;
  flag.test_and_set();

  wchar_t currentExePath[MAX_PATH];
  GetModuleFileNameW(nullptr, currentExePath, MAX_PATH);
  wchar_t *currentExeName = PathFindFileNameW(currentExePath);

  wchar_t dataPath[MAX_PATH];
  if (!SUCCEEDED(
          SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, dataPath))) {
    return false;
  }
  wchar_t userDataFolder[MAX_PATH];
  PathCombineW(userDataFolder, dataPath, currentExeName);

  m_com_handler = new com_event_handler(
      wnd, cb,
      [&](ICoreWebView2Controller *controller, ICoreWebView2 *webview) {
        if (!controller || !webview) {
          flag.clear();
          return;
        }
        controller->AddRef();
        webview->AddRef();
        m_controller = controller;
        m_webview = webview;
        flag.clear();
      });

  m_com_handler->set_attempt_handler([&] {
    return m_webview2_loader.create_environment_with_options(
        nullptr, userDataFolder, nullptr, m_com_handler);
  });
  m_com_handler->try_create_environment();

  MSG msg = {};
  while (flag.test_and_set() && GetMessage(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  if (!m_controller || !m_webview) {
    return false;
  }
  ICoreWebView2Settings *settings = nullptr;
  auto res = m_webview->get_Settings(&settings);
  if (res != S_OK) {
    return false;
  }
  res = settings->put_AreDevToolsEnabled(debug ? TRUE : FALSE);
  if (res != S_OK) {
    return false;
  }
  init("window.external={invoke:s=>window.chrome.webview.postMessage(s)}");
  return true;
}

void win32_edge_engine::resize(HWND wnd) {
  if (m_controller == nullptr) {
    return;
  }
  RECT bounds;
  GetClientRect(wnd, &bounds);
  m_controller->put_Bounds(bounds);
}

bool win32_edge_engine::is_webview2_available() const noexcept {
  LPWSTR version_info = nullptr;
  auto res = m_webview2_loader.get_available_browser_version_string(
      nullptr, &version_info);
  // The result will be equal to HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)
  // if the WebView2 runtime is not installed.
  auto ok = SUCCEEDED(res) && version_info;
  if (version_info) {
    CoTaskMemFree(version_info);
  }
  return ok;
}

} // namespace webview