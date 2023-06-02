#pragma once

#include <string>
#include <windows.h>

class reg_key {
public:
  explicit reg_key(HKEY root_key, const wchar_t *sub_key, DWORD options,
                   REGSAM sam_desired);

  explicit reg_key(HKEY root_key, const std::wstring &sub_key, DWORD options,
                   REGSAM sam_desired);

  virtual ~reg_key();

  reg_key(const reg_key &other) = delete;
  reg_key &operator=(const reg_key &other) = delete;
  reg_key(reg_key &&other) = delete;
  reg_key &operator=(reg_key &&other) = delete;

  bool is_open() const;
  HKEY get_handle() const;

  std::wstring query_string(const wchar_t *name) const;

private:
  HKEY m_handle = nullptr;
};
