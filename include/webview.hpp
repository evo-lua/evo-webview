#pragma once

#if !defined(WEBVIEW_GTK) && !defined(WEBVIEW_COCOA) && !defined(WEBVIEW_EDGE)
#if defined(__APPLE__)
#define WEBVIEW_COCOA
#elif defined(__unix__)
#define WEBVIEW_GTK
#elif defined(_WIN32)
#define WEBVIEW_EDGE
#else
#error "please, specify webview backend"
#endif
#endif

#include <functional>
#include <map>
#include <string>

#include "json_utils.hpp" // Very sketchy since this isn't part of the public API, but it is what it is (for now)

#if defined(WEBVIEW_GTK)
#include "webkit2gtk_engine.hpp"
#elif defined(WEBVIEW_COCOA)
#include "cocoa_engine.hpp"
#elif defined(WEBVIEW_EDGE)
#include "mswebview_engine.hpp"
#endif /* WEBVIEW_GTK, WEBVIEW_COCOA, WEBVIEW_EDGE */

namespace webview {

class webview : public browser_engine {
public:
  webview(bool debug = false, void *wnd = nullptr);

  void navigate(const std::string &url);

  using binding_t = std::function<void(std::string, std::string, void *)>;
  class binding_ctx_t {
  public:
    binding_ctx_t(binding_t callback, void *arg);
    // This function is called upon execution of the bound JS function
    binding_t callback;
    // This user-supplied argument is passed to the callback
    void *arg;
  };

  using sync_binding_t = std::function<std::string(std::string)>;
  // Synchronous bind
  void bind(const std::string &name, sync_binding_t fn);

  // Asynchronous bind
  void bind(const std::string &name, binding_t fn, void *arg);

  void unbind(const std::string &name);

  void resolve(const std::string &seq, int status, const std::string &result);

private:
  void on_message(const std::string &msg);

  std::map<std::string, binding_ctx_t> bindings;
};
} // namespace webview