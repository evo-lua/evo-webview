#include "com_init_wrapper.hpp"

com_init_wrapper::com_init_wrapper(DWORD dwCoInit) : m_initialized(false) {
  // We can safely continue as long as COM was either successfully
  // initialized or already initialized.
  // RPC_E_CHANGED_MODE means that CoInitializeEx was already called with
  // a different concurrency model.
  switch (CoInitializeEx(nullptr, dwCoInit)) {
  case S_OK:
  case S_FALSE:
    m_initialized = true;
    break;
  }
}

com_init_wrapper::~com_init_wrapper() {
  if (m_initialized) {
    CoUninitialize();
    m_initialized = false;
  }
}

bool com_init_wrapper::is_initialized() const { return m_initialized; }
