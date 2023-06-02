#pragma once

#include "shared/library_symbol.hpp"
#include "loader.hpp"
#include "shared/native_library.hpp"
#include "reg_key.hpp"

#include <array>
#include <string>
#include <vector>
#include <WebView2.h>
#include <windows.h>

// Enable built-in WebView2Loader implementation by default.
#ifndef WEBVIEW_MSWEBVIEW2_BUILTIN_IMPL
#define WEBVIEW_MSWEBVIEW2_BUILTIN_IMPL 1
#endif

// Link WebView2Loader.dll explicitly by default only if the built-in
// implementation is enabled.
#ifndef WEBVIEW_MSWEBVIEW2_EXPLICIT_LINK
#define WEBVIEW_MSWEBVIEW2_EXPLICIT_LINK WEBVIEW_MSWEBVIEW2_BUILTIN_IMPL
#endif

// Explicit linking of WebView2Loader.dll should be used along with
// the built-in implementation.
#if WEBVIEW_MSWEBVIEW2_BUILTIN_IMPL == 1 &&                                    \
    WEBVIEW_MSWEBVIEW2_EXPLICIT_LINK != 1
#undef WEBVIEW_MSWEBVIEW2_EXPLICIT_LINK
#error Please set WEBVIEW_MSWEBVIEW2_EXPLICIT_LINK=1.
#endif

#if WEBVIEW_MSWEBVIEW2_BUILTIN_IMPL == 1
enum class webview2_runtime_type { installed = 0, embedded = 1 };

namespace webview2_symbols {
template <typename T> using library_symbol = webview::library_symbol<T>;

using CreateWebViewEnvironmentWithOptionsInternal_t =
    HRESULT(STDMETHODCALLTYPE *)(
        bool, webview2_runtime_type, PCWSTR, IUnknown *,
        ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *);
using DllCanUnloadNow_t = HRESULT(STDMETHODCALLTYPE *)();

static constexpr auto CreateWebViewEnvironmentWithOptionsInternal =
    library_symbol<CreateWebViewEnvironmentWithOptionsInternal_t>(
        "CreateWebViewEnvironmentWithOptionsInternal");
static constexpr auto DllCanUnloadNow =
    library_symbol<DllCanUnloadNow_t>("DllCanUnloadNow");
} // namespace webview2_symbols
#endif /* WEBVIEW_MSWEBVIEW2_BUILTIN_IMPL */

#if WEBVIEW_MSWEBVIEW2_EXPLICIT_LINK == 1
namespace webview2_symbols {
using CreateCoreWebView2EnvironmentWithOptions_t = HRESULT(STDMETHODCALLTYPE *)(
    PCWSTR, PCWSTR, ICoreWebView2EnvironmentOptions *,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *);
using GetAvailableCoreWebView2BrowserVersionString_t =
    HRESULT(STDMETHODCALLTYPE *)(PCWSTR, LPWSTR *);

static constexpr auto CreateCoreWebView2EnvironmentWithOptions =
    library_symbol<CreateCoreWebView2EnvironmentWithOptions_t>(
        "CreateCoreWebView2EnvironmentWithOptions");
static constexpr auto GetAvailableCoreWebView2BrowserVersionString =
    library_symbol<GetAvailableCoreWebView2BrowserVersionString_t>(
        "GetAvailableCoreWebView2BrowserVersionString");
} // namespace webview2_symbols
#endif /* WEBVIEW_MSWEBVIEW2_EXPLICIT_LINK */

#if WEBVIEW_MSWEBVIEW2_BUILTIN_IMPL == 1
// Gets the last component of a Windows native file path.
// For example, if the path is "C:\a\b" then the result is "b".
template <typename T>
std::basic_string<T>
get_last_native_path_component(const std::basic_string<T> &path) {
  if (auto pos = path.find_last_of(static_cast<T>('\\'));
      pos != std::basic_string<T>::npos) {
    return path.substr(pos + 1);
  }
  return std::basic_string<T>();
}
#endif /* WEBVIEW_MSWEBVIEW2_BUILTIN_IMPL */

namespace webview {
namespace webview2_loader {

// Parses a version string with 1-4 integral components, e.g. "1.2.3.4".
// Missing or invalid components default to 0, and excess components are ignored.
template <typename T>
std::array<unsigned int, 4>
parse_version(const std::basic_string<T> &version) noexcept {
  auto parse_component = [](auto sb, auto se) -> unsigned int {
    try {
      auto n = std::stol(std::basic_string<T>(sb, se));
      return n < 0 ? 0 : n;
    } catch (std::exception &) {
      return 0;
    }
  };
  auto end = version.end();
  auto sb = version.begin(); // subrange begin
  auto se = sb;              // subrange end
  unsigned int ci = 0;       // component index
  std::array<unsigned int, 4> components{};
  while (sb != end && se != end && ci < components.size()) {
    if (*se == static_cast<T>('.')) {
      components[ci++] = parse_component(sb, se);
      sb = ++se;
      continue;
    }
    ++se;
  }
  if (sb < se && ci < components.size()) {
    components[ci] = parse_component(sb, se);
  }
  return components;
}

template <typename T, std::size_t Length>
auto parse_version(const T (&version)[Length]) noexcept {
  return parse_version(std::basic_string<T>(version, Length));
}

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

class loader {
public:
  HRESULT create_environment_with_options(
      PCWSTR browser_dir, PCWSTR user_data_dir,
      ICoreWebView2EnvironmentOptions *env_options,
      ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler
          *created_handler) const {
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
  get_available_browser_version_string(PCWSTR browser_dir,
                                       LPWSTR *version) const {
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

private:
#if WEBVIEW_MSWEBVIEW2_BUILTIN_IMPL == 1
  struct client_info_t {
    bool found = false;
    std::wstring dll_path;
    std::wstring version;
    webview2_runtime_type runtime_type;
  };

  HRESULT create_environment_with_options_impl(
      PCWSTR browser_dir, PCWSTR user_data_dir,
      ICoreWebView2EnvironmentOptions *env_options,
      ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler
          *created_handler) const {
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
  get_available_browser_version_string_impl(PCWSTR browser_dir,
                                            LPWSTR *version) const {
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

  client_info_t find_available_client(PCWSTR browser_dir) const {
    if (browser_dir) {
      return find_embedded_client(api_version, browser_dir);
    }
    auto found_client =
        find_installed_client(api_version, true, default_release_channel_guid);
    if (!found_client.found) {
      found_client = find_installed_client(api_version, false,
                                           default_release_channel_guid);
    }
    return found_client;
  }

  std::wstring make_client_dll_path(const std::wstring &dir) const {
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

  client_info_t
  find_installed_client(unsigned int min_api_version, bool system,
                        const std::wstring &release_channel) const {
    std::wstring sub_key = client_state_reg_sub_key;
    sub_key += release_channel;
    auto root_key = system ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    reg_key key(root_key, sub_key, 0, KEY_READ | KEY_WOW64_32KEY);
    if (!key.is_open()) {
      return {};
    }
    auto ebwebview_value = key.query_string(L"EBWebView");

    auto client_version_string =
        get_last_native_path_component(ebwebview_value);
    auto client_version = parse_version(client_version_string);
    if (client_version[2] < min_api_version) {
      // Our API version is greater than the runtime API version.
      return {};
    }

    auto client_dll_path = make_client_dll_path(ebwebview_value);
    return {true, client_dll_path, client_version_string,
            webview2_runtime_type::installed};
  }

  client_info_t find_embedded_client(unsigned int min_api_version,
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

  // The minimum WebView2 API version we need regardless of the SDK release
  // actually used. The number comes from the SDK release version,
  // e.g. 1.0.1150.38. To be safe the SDK should have a number that is greater
  // than or equal to this number. The Edge browser webview client must
  // have a number greater than or equal to this number.
  static constexpr unsigned int api_version = 1150;

  static constexpr auto client_state_reg_sub_key =
      L"SOFTWARE\\Microsoft\\EdgeUpdate\\ClientState\\";

  // GUID for the stable release channel.
  static constexpr auto stable_release_guid =
      L"{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}";

  static constexpr auto default_release_channel_guid = stable_release_guid;
#endif /* WEBVIEW_MSWEBVIEW2_BUILTIN_IMPL */

#if WEBVIEW_MSWEBVIEW2_EXPLICIT_LINK == 1
  native_library m_lib{L"WebView2Loader.dll"};
#endif
};

} // namespace webview2_loader
} // namespace webview