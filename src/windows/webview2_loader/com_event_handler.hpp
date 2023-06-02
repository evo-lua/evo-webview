#pragma once

#include <atomic>
#include <functional>
#include <string>

#include <WebView2.h>
#include <windows.h>

namespace webview {
namespace webview2_loader {

using msg_cb_t = std::function<void(const std::string)>;

template <typename T> struct cast_info_t {
  using type = T;
  IID iid;
};

namespace cast_info {

// These are exported by WebView2.h but may not be available as constants (depends on the SDK version?)
static constexpr IID
    IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler{
        0x6C4819F3, 0xC9B7, 0x4260, 0x81, 0x27, 0xC9,
        0xF5,       0xBD,   0xE7,   0xF6, 0x8C};
static constexpr IID
    IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler{
        0x4E8A3389, 0xC9D8, 0x4BD2, 0xB6, 0xB5, 0x12,
        0x4F,       0xEE,   0x6C,   0xC1, 0x4D};
static constexpr IID IID_ICoreWebView2PermissionRequestedEventHandler{
    0x15E1C6A3, 0xC72A, 0x4DF3, 0x91, 0xD7, 0xD0, 0x97, 0xFB, 0xEC, 0x6B, 0xFD};
static constexpr IID IID_ICoreWebView2WebMessageReceivedEventHandler{
    0x57213F19, 0x00E6, 0x49FA, 0x8E, 0x07, 0x89, 0x8E, 0xA0, 0x1E, 0xCB, 0xD2};

static constexpr auto controller_completed =
    cast_info_t<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>{
        IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler};

static constexpr auto environment_completed =
    cast_info_t<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>{
        IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler};

static constexpr auto message_received =
    cast_info_t<ICoreWebView2WebMessageReceivedEventHandler>{
        IID_ICoreWebView2WebMessageReceivedEventHandler};

static constexpr auto permission_requested =
    cast_info_t<ICoreWebView2PermissionRequestedEventHandler>{
        IID_ICoreWebView2PermissionRequestedEventHandler};
} // namespace cast_info

class com_event_handler
    : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
      public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,
      public ICoreWebView2WebMessageReceivedEventHandler,
      public ICoreWebView2PermissionRequestedEventHandler {
  using com_interface_handler_cb_t =
      std::function<void(ICoreWebView2Controller *, ICoreWebView2 *webview)>;

public:
  com_event_handler(HWND hwnd, msg_cb_t msgCb, com_interface_handler_cb_t cb);
  virtual ~com_event_handler() = default;
  com_event_handler(const com_event_handler &other) = delete;
  com_event_handler &operator=(const com_event_handler &other) = delete;
  com_event_handler(com_event_handler &&other) = delete;
  com_event_handler &operator=(com_event_handler &&other) = delete;

  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, LPVOID *ppv);
  HRESULT STDMETHODCALLTYPE Invoke(HRESULT res, ICoreWebView2Environment *env);
  HRESULT STDMETHODCALLTYPE Invoke(HRESULT res,
                                   ICoreWebView2Controller *controller);
  HRESULT STDMETHODCALLTYPE
  Invoke(ICoreWebView2 *sender, ICoreWebView2WebMessageReceivedEventArgs *args);
  HRESULT STDMETHODCALLTYPE Invoke(
      ICoreWebView2 *sender, ICoreWebView2PermissionRequestedEventArgs *args);

  // Checks whether the specified IID equals the IID of the specified type and
  // if so casts the "this" pointer to T and returns it. Returns nullptr on
  // mismatching IIDs.
  // If ppv is specified then the pointer will also be assigned to *ppv.
  template <typename T>
  T *cast_if_equal_iid(REFIID riid, const cast_info_t<T> &info,
                       LPVOID *ppv = nullptr) noexcept {
    T *ptr = nullptr;
    if (IsEqualIID(riid, info.iid)) {
      ptr = static_cast<T *>(this);
      ptr->AddRef();
    }
    if (ppv) {
      *ppv = ptr;
    }
    return ptr;
  }

  // Set the function that will perform the initiating logic for creating
  // the WebView2 environment.
  void set_attempt_handler(std::function<HRESULT()> attempt_handler) noexcept;
  // Retry creating a WebView2 environment.
  // The initiating logic for creating the environment is defined by the
  // caller of set_attempt_handler().
  void try_create_environment() noexcept;

private:
  HWND m_window;
  msg_cb_t m_msgCb;
  com_interface_handler_cb_t m_cb;
  std::atomic<ULONG> m_ref_count{1};
  std::function<HRESULT()> m_attempt_handler;
  unsigned int m_max_attempts = 5;
  unsigned int m_attempts = 0;
};

} // namespace webview2_loader
} // namespace webview
