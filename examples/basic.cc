#include "webview.hpp"

#ifdef _WIN32

#include <windows.h>

int WINAPI WinMain(HINSTANCE hInt, HINSTANCE hPrevInst, LPSTR lpCmdLine,
                   int nCmdShow) {
#else
int main() {
#endif
  webview::webview w(false, nullptr);
  w.set_title("Basic Example");
  w.set_size(480, 320, WEBVIEW_HINT_NONE);
  w.set_html("Thanks for using webview!");
  w.run();
  return 0;
}
