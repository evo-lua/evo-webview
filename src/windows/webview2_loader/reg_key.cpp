#include "reg_key.hpp"

reg_key::reg_key(HKEY root_key, const wchar_t *sub_key, DWORD options,
                 REGSAM sam_desired) {
  HKEY handle;
  auto status = RegOpenKeyExW(root_key, sub_key, options, sam_desired, &handle);
  if (status == ERROR_SUCCESS) {
    m_handle = handle;
  }
}

reg_key::reg_key(HKEY root_key, const std::wstring &sub_key, DWORD options,
                 REGSAM sam_desired)
    : reg_key(root_key, sub_key.c_str(), options, sam_desired) {}

reg_key::~reg_key() {
  if (m_handle) {
    RegCloseKey(m_handle);
    m_handle = nullptr;
  }
}

bool reg_key::is_open() const { return !!m_handle; }
HKEY reg_key::get_handle() const { return m_handle; }

std::wstring reg_key::query_string(const wchar_t *name) const {
  DWORD buf_length = 0;
  auto status =
      RegQueryValueExW(m_handle, name, nullptr, nullptr, nullptr, &buf_length);
  if (status != ERROR_SUCCESS && status != ERROR_MORE_DATA) {
    return std::wstring();
  }
  std::wstring result(buf_length / sizeof(wchar_t), 0);
  auto buf = reinterpret_cast<LPBYTE>(&result[0]);
  status = RegQueryValueExW(m_handle, name, nullptr, nullptr, buf, &buf_length);
  if (status != ERROR_SUCCESS) {
    return std::wstring();
  }
  for (std::size_t length = result.size(); length > 0; --length) {
    if (result[length - 1] != 0) {
      result.resize(length);
      break;
    }
  }
  return result;
}
