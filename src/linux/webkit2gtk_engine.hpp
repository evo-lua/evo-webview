#pragma once

#include <JavaScriptCore/JavaScript.h>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

#include <functional>
#include <string>

#include "webview.h"

namespace webview {

using dispatch_fn_t = std::function<void()>;

class gtk_webkit_engine {
public:
  gtk_webkit_engine(bool debug, void *window);
  virtual ~gtk_webkit_engine() = default;
  void *window();
  void run();
  void terminate();
  void dispatch(std::function<void()> f);

  void set_title(const std::string &title);

  void set_size(int width, int height, int hints);

  void navigate(const std::string &url);

  void set_html(const std::string &html);

  void init(const std::string &js);

  void eval(const std::string &js);

private:
  virtual void on_message(const std::string &msg) = 0;

  static char *get_string_from_js_result(WebKitJavascriptResult *r);

  GtkWidget *m_window;
  GtkWidget *m_webview;
};

using browser_engine = gtk_webkit_engine;

} // namespace webview
