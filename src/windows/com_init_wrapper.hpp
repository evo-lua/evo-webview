#pragma once

#include <windows.h>

class com_init_wrapper {
public:
  com_init_wrapper(DWORD dwCoInit);
  ~com_init_wrapper();

  com_init_wrapper(const com_init_wrapper &other) = delete;
  com_init_wrapper &operator=(const com_init_wrapper &other) = delete;
  com_init_wrapper(com_init_wrapper &&other) = delete;
  com_init_wrapper &operator=(com_init_wrapper &&other) = delete;

  bool is_initialized() const;

private:
  bool m_initialized = false;
};
