#include <functional>
#include <string>

#include "webview.hpp"
#include "json_utils.hpp"

namespace webview {

webview::webview(bool debug, void *wnd) : browser_engine(debug, wnd) {}

void webview::navigate(const std::string &url) {
  if (url.empty()) {
    browser_engine::navigate("about:blank");
    return;
  }
  browser_engine::navigate(url);
}

webview::binding_ctx_t::binding_ctx_t(binding_t callback, void *arg)
    : callback(callback), arg(arg) {}

// Synchronous bind
void webview::bind(const std::string &name, sync_binding_t fn) {
  auto wrapper = [this, fn](const std::string &seq, const std::string &req,
                            void * /*arg*/) { resolve(seq, 0, fn(req)); };
  bind(name, wrapper, nullptr);
}

// Asynchronous bind
void webview::bind(const std::string &name, binding_t fn, void *arg) {
  if (bindings.count(name) > 0) {
    return;
  }
  bindings.emplace(name, binding_ctx_t(fn, arg));
  auto js = "(function() { var name = '" + name + "';" + R""(
      var RPC = window._rpc = (window._rpc || {nextSeq: 1});
      window[name] = function() {
        var seq = RPC.nextSeq++;
        var promise = new Promise(function(resolve, reject) {
          RPC[seq] = {
            resolve: resolve,
            reject: reject,
          };
        });
        window.external.invoke(JSON.stringify({
          id: seq,
          method: name,
          params: Array.prototype.slice.call(arguments),
        }));
        return promise;
      }
    })())"";
  init(js);
  eval(js);
}

void webview::unbind(const std::string &name) {
  auto found = bindings.find(name);
  if (found != bindings.end()) {
    auto js = "delete window['" + name + "'];";
    init(js);
    eval(js);
    bindings.erase(found);
  }
}

void webview::resolve(const std::string &seq, int status,
                      const std::string &result) {
  dispatch([seq, status, result, this]() {
    if (status == 0) {
      eval("window._rpc[" + seq + "].resolve(" + result +
           "); delete window._rpc[" + seq + "]");
    } else {
      eval("window._rpc[" + seq + "].reject(" + result +
           "); delete window._rpc[" + seq + "]");
    }
  });
}

void webview::on_message(const std::string &msg) {
  auto seq = json::json_parse(msg, "id", 0);
  auto name = json::json_parse(msg, "method", 0);
  auto args = json::json_parse(msg, "params", 0);
  auto found = bindings.find(name);
  if (found == bindings.end()) {
    return;
  }
  const auto &context = found->second;
  context.callback(seq, args, context.arg);
}
} // namespace webview