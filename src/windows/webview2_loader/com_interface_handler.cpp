#include "com_interface_handler.hpp"
#include "wstring_utils.hpp"

namespace webview {
namespace webview2_loader {

com_interface_handler::com_interface_handler(HWND hwnd, msg_cb_t msgCb,
                                             com_interface_handler_cb_t cb)
    : m_window(hwnd), m_msgCb(msgCb), m_cb(cb) {}

ULONG STDMETHODCALLTYPE com_interface_handler::AddRef() {
  return ++m_ref_count;
}

ULONG STDMETHODCALLTYPE com_interface_handler::Release() {
  if (m_ref_count > 1) {
    return --m_ref_count;
  }
  delete this;
  return 0;
}

HRESULT STDMETHODCALLTYPE com_interface_handler::QueryInterface(REFIID riid,
                                                                LPVOID *ppv) {
  using namespace webview2_loader::cast_info; // TBD: remove this line

  if (!ppv) {
    return E_POINTER;
  }

  // All of the COM interfaces we implement should be added here regardless
  // of whether they are required.
  // This is just to be on the safe side in case the WebView2 Runtime ever
  // requests a pointer to an interface we implement.
  // The WebView2 Runtime must at the very least be able to get a pointer to
  // ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler when we use
  // our custom WebView2 loader implementation, and observations have shown
  // that it is the only interface requested in this case. None have been
  // observed to be requested when using the official WebView2 loader.

  if (cast_if_equal_iid(riid, controller_completed, ppv) ||
      cast_if_equal_iid(riid, environment_completed, ppv) ||
      cast_if_equal_iid(riid, message_received, ppv) ||
      cast_if_equal_iid(riid, permission_requested, ppv)) {
    return S_OK;
  }

  return E_NOINTERFACE;
}

HRESULT STDMETHODCALLTYPE
com_interface_handler::Invoke(HRESULT res, ICoreWebView2Environment *env) {
  if (SUCCEEDED(res)) {
    res = env->CreateCoreWebView2Controller(m_window, this);
    if (SUCCEEDED(res)) {
      return S_OK;
    }
  }
  try_create_environment();
  return S_OK;
}

HRESULT STDMETHODCALLTYPE com_interface_handler::Invoke(
    HRESULT res, ICoreWebView2Controller *controller) {
  if (FAILED(res)) {
    // See try_create_environment() regarding
    // HRESULT_FROM_WIN32(ERROR_INVALID_STATE).
    // The result is E_ABORT if the parent window has been destroyed already.
    switch (res) {
    case HRESULT_FROM_WIN32(ERROR_INVALID_STATE):
    case E_ABORT:
      return S_OK;
    }
    try_create_environment();
    return S_OK;
  }

  ICoreWebView2 *webview;
  ::EventRegistrationToken token;
  controller->get_CoreWebView2(&webview);
  webview->add_WebMessageReceived(this, &token);
  webview->add_PermissionRequested(this, &token);

  m_cb(controller, webview);
  return S_OK;
}

HRESULT STDMETHODCALLTYPE com_interface_handler::Invoke(
    ICoreWebView2 *sender, ICoreWebView2WebMessageReceivedEventArgs *args) {
  LPWSTR message;
  args->TryGetWebMessageAsString(&message);
  m_msgCb(webview::wstring::narrow_string(message));
  sender->PostWebMessageAsString(message);

  CoTaskMemFree(message);
  return S_OK;
}

HRESULT STDMETHODCALLTYPE com_interface_handler::Invoke(
    ICoreWebView2 *sender, ICoreWebView2PermissionRequestedEventArgs *args) {
  COREWEBVIEW2_PERMISSION_KIND kind;
  args->get_PermissionKind(&kind);
  if (kind == COREWEBVIEW2_PERMISSION_KIND_CLIPBOARD_READ) {
    args->put_State(COREWEBVIEW2_PERMISSION_STATE_ALLOW);
  }
  return S_OK;
}

void com_interface_handler::set_attempt_handler(
    std::function<HRESULT()> attempt_handler) noexcept {
  m_attempt_handler = attempt_handler;
}

void com_interface_handler::try_create_environment() noexcept {
  // WebView creation fails with HRESULT_FROM_WIN32(ERROR_INVALID_STATE) if
  // a running instance using the same user data folder exists, and the
  // Environment objects have different EnvironmentOptions.
  // Source: https://docs.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2environment?view=webview2-1.0.1150.38
  if (m_attempts < m_max_attempts) {
    ++m_attempts;
    auto res = m_attempt_handler();
    if (SUCCEEDED(res)) {
      return;
    }
    // Not entirely sure if this error code only applies to
    // CreateCoreWebView2Controller so we check here as well.
    if (res == HRESULT_FROM_WIN32(ERROR_INVALID_STATE)) {
      return;
    }
    try_create_environment();
    return;
  }
  // Give up.
  m_cb(nullptr, nullptr);
}

} // namespace webview2_loader
} // namespace webview
