#include "webview2_loader/msedge_runtime_loader.hpp"
#include "shared/library_symbol.hpp"
#include "shared/native_library.hpp"
#include "reg_key.hpp"

#include <array>
#include <string>
#include <vector>
#include <WebView2.h>
#include <windows.h>

namespace webview {
namespace webview2_loader {

std::wstring get_file_version_string(const std::wstring &file_path) noexcept {
  DWORD dummy_handle; // Unused
  DWORD info_buffer_length =
      GetFileVersionInfoSizeW(file_path.c_str(), &dummy_handle);
  if (info_buffer_length == 0) {
    return std::wstring();
  }
  std::vector<char> info_buffer;
  info_buffer.reserve(info_buffer_length);
  if (!GetFileVersionInfoW(file_path.c_str(), 0, info_buffer_length,
                           info_buffer.data())) {
    return std::wstring();
  }
  auto sub_block = L"\\StringFileInfo\\040904B0\\ProductVersion";
  LPWSTR version = nullptr;
  unsigned int version_length = 0;
  if (!VerQueryValueW(info_buffer.data(), sub_block,
                      reinterpret_cast<LPVOID *>(&version), &version_length)) {
    return std::wstring();
  }
  if (!version || version_length == 0) {
    return std::wstring();
  }
  return std::wstring(version, version_length);
}

HRESULT msedge_runtime_loader::create_environment_with_options(
    PCWSTR browser_dir, PCWSTR user_data_dir,
    ICoreWebView2EnvironmentOptions *env_options,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *created_handler)
    const {
#if WEBVIEW_MSWEBVIEW2_EXPLICIT_LINK == 1
  if (m_lib.is_loaded()) {
    if (auto fn = m_lib.get(
            webview2_symbols::CreateCoreWebView2EnvironmentWithOptions)) {
      return fn(browser_dir, user_data_dir, env_options, created_handler);
    }
  }
#if WEBVIEW_MSWEBVIEW2_BUILTIN_IMPL == 1
  return create_environment_with_options_impl(browser_dir, user_data_dir,
                                              env_options, created_handler);
#else
  return S_FALSE;
#endif
#else
  return ::CreateCoreWebView2EnvironmentWithOptions(
      browser_dir, user_data_dir, env_options, created_handler);
#endif /* WEBVIEW_MSWEBVIEW2_EXPLICIT_LINK */
}

HRESULT
msedge_runtime_loader::get_available_browser_version_string(
    PCWSTR browser_dir, LPWSTR *version) const {
#if WEBVIEW_MSWEBVIEW2_EXPLICIT_LINK == 1
  if (m_lib.is_loaded()) {
    if (auto fn = m_lib.get(
            webview2_symbols::GetAvailableCoreWebView2BrowserVersionString)) {
      return fn(browser_dir, version);
    }
  }
#if WEBVIEW_MSWEBVIEW2_BUILTIN_IMPL == 1
  return get_available_browser_version_string_impl(browser_dir, version);
#else
  return S_FALSE;
#endif
#else
  return ::GetAvailableCoreWebView2BrowserVersionString(browser_dir, version);
#endif /* WEBVIEW_MSWEBVIEW2_EXPLICIT_LINK */
}

#if WEBVIEW_MSWEBVIEW2_BUILTIN_IMPL == 1
HRESULT msedge_runtime_loader::create_environment_with_options_impl(
    PCWSTR browser_dir, PCWSTR user_data_dir,
    ICoreWebView2EnvironmentOptions *env_options,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *created_handler)
    const {
  auto found_client = find_available_client(browser_dir);
  if (!found_client.found) {
    return -1;
  }
  auto client_dll = native_library(found_client.dll_path.c_str());
  if (auto fn = client_dll.get(
          webview2_symbols::CreateWebViewEnvironmentWithOptionsInternal)) {
    return fn(true, found_client.runtime_type, user_data_dir, env_options,
              created_handler);
  }
  if (auto fn = client_dll.get(webview2_symbols::DllCanUnloadNow)) {
    if (!fn()) {
      client_dll.detach();
    }
  }
  return ERROR_SUCCESS;
}

HRESULT
msedge_runtime_loader::get_available_browser_version_string_impl(
    PCWSTR browser_dir, LPWSTR *version) const {
  if (!version) {
    return -1;
  }
  auto found_client = find_available_client(browser_dir);
  if (!found_client.found) {
    return -1;
  }
  auto info_length_bytes =
      found_client.version.size() * sizeof(found_client.version[0]);
  auto info = static_cast<LPWSTR>(CoTaskMemAlloc(info_length_bytes));
  if (!info) {
    return -1;
  }
  CopyMemory(info, found_client.version.c_str(), info_length_bytes);
  *version = info;
  return 0;
}

webview::webview2_loader::msedge_runtime_loader::client_info_t
msedge_runtime_loader::find_available_client(PCWSTR browser_dir) const {
  if (browser_dir) {
    return find_embedded_client(api_version, browser_dir);
  }
  auto found_client =
      find_installed_client(api_version, true, default_release_channel_guid);
  if (!found_client.found) {
    found_client =
        find_installed_client(api_version, false, default_release_channel_guid);
  }
  return found_client;
}

std::wstring
msedge_runtime_loader::make_client_dll_path(const std::wstring &dir) const {
  auto dll_path = dir;
  if (!dll_path.empty()) {
    auto last_char = dir[dir.size() - 1];
    if (last_char != L'\\' && last_char != L'/') {
      dll_path += L'\\';
    }
  }
  dll_path += L"EBWebView\\";
#if defined(_M_X64) || defined(__x86_64__)
  dll_path += L"x64";
#elif defined(_M_IX86) || defined(__i386__)
  dll_path += L"x86";
#elif defined(_M_ARM64) || defined(__aarch64__)
  dll_path += L"arm64";
#else
#error WebView2 integration for this platform is not yet supported.
#endif
  dll_path += L"\\EmbeddedBrowserWebView.dll";
  return dll_path;
}

webview::webview2_loader::msedge_runtime_loader::client_info_t
msedge_runtime_loader::find_installed_client(
    unsigned int min_api_version, bool system,
    const std::wstring &release_channel) const {
  std::wstring sub_key = client_state_reg_sub_key;
  sub_key += release_channel;
  auto root_key = system ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  reg_key key(root_key, sub_key, 0, KEY_READ | KEY_WOW64_32KEY);
  if (!key.is_open()) {
    return {};
  }
  auto ebwebview_value = key.query_string(L"EBWebView");

  auto client_version_string = get_last_native_path_component(ebwebview_value);
  auto client_version = parse_version(client_version_string);
  if (client_version[2] < min_api_version) {
    // Our API version is greater than the runtime API version.
    return {};
  }

  auto client_dll_path = make_client_dll_path(ebwebview_value);
  return {true, client_dll_path, client_version_string,
          webview2_runtime_type::installed};
}

webview::webview2_loader::msedge_runtime_loader::client_info_t
msedge_runtime_loader::find_embedded_client(unsigned int min_api_version,
                                            const std::wstring &dir) const {
  auto client_dll_path = make_client_dll_path(dir);

  auto client_version_string = get_file_version_string(client_dll_path);
  auto client_version = parse_version(client_version_string);
  if (client_version[2] < min_api_version) {
    // Our API version is greater than the runtime API version.
    return {};
  }

  return {true, client_dll_path, client_version_string,
          webview2_runtime_type::embedded};
}

#endif /* WEBVIEW_MSWEBVIEW2_BUILTIN_IMPL */

} // namespace webview2_loader
} // namespace webview