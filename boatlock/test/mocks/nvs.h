#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using esp_err_t = int32_t;
using nvs_handle_t = uint32_t;

constexpr esp_err_t ESP_OK = 0;
constexpr esp_err_t ESP_ERR_NVS_NOT_FOUND = 0x1102;
constexpr esp_err_t ESP_ERR_NVS_INVALID_LENGTH = 0x110c;
constexpr esp_err_t ESP_ERR_NVS_INVALID_NAME = 0x1109;
constexpr esp_err_t ESP_ERR_NVS_INVALID_HANDLE = 0x1108;

enum nvs_open_mode_t {
  NVS_READONLY = 0,
  NVS_READWRITE = 1,
};

struct MockNvsNamespace {
  std::map<std::string, std::vector<uint8_t>> committed;
  std::map<std::string, std::vector<uint8_t>> pending;
  bool erasePending = false;
};

struct MockNvsState {
  std::map<std::string, MockNvsNamespace> namespaces;
  std::string activeNamespace;
  bool open = false;
  bool readOnly = false;
  int commitCount = 0;
  esp_err_t commitResult = ESP_OK;
  esp_err_t openResult = ESP_OK;
};

inline MockNvsState& nvs_mock_state() {
  static MockNvsState state;
  return state;
}

inline void nvs_mock_clear() {
  nvs_mock_state() = MockNvsState{};
}

inline int nvs_mock_commit_count() {
  return nvs_mock_state().commitCount;
}

inline void nvs_mock_set_commit_result(esp_err_t result) {
  nvs_mock_state().commitResult = result;
}

inline void nvs_mock_set_open_result(esp_err_t result) {
  nvs_mock_state().openResult = result;
}

inline void nvs_mock_put_blob(const char* ns, const char* key, const void* value, size_t len) {
  const uint8_t* bytes = static_cast<const uint8_t*>(value);
  nvs_mock_state().namespaces[ns].committed[key] = std::vector<uint8_t>(bytes, bytes + len);
}

inline void nvs_mock_put_u8(const char* ns, const char* key, uint8_t value) {
  nvs_mock_put_blob(ns, key, &value, sizeof(value));
}

inline bool nvs_mock_get_blob(const char* ns, const char* key, void* value, size_t len) {
  auto nsIt = nvs_mock_state().namespaces.find(ns);
  if (nsIt == nvs_mock_state().namespaces.end()) {
    return false;
  }
  auto keyIt = nsIt->second.committed.find(key);
  if (keyIt == nsIt->second.committed.end() || keyIt->second.size() != len) {
    return false;
  }
  std::memcpy(value, keyIt->second.data(), len);
  return true;
}

inline esp_err_t nvs_open(const char* name, nvs_open_mode_t mode, nvs_handle_t* out_handle) {
  if (!name || !out_handle || std::strlen(name) > 15) {
    return ESP_ERR_NVS_INVALID_NAME;
  }
  MockNvsState& state = nvs_mock_state();
  if (state.openResult != ESP_OK) {
    return state.openResult;
  }
  state.namespaces[name];
  state.activeNamespace = name;
  state.open = true;
  state.readOnly = mode == NVS_READONLY;
  *out_handle = 1;
  return ESP_OK;
}

inline void nvs_close(nvs_handle_t) {
  MockNvsState& state = nvs_mock_state();
  state.open = false;
  state.activeNamespace.clear();
  state.readOnly = false;
}

inline esp_err_t nvs_set_blob(nvs_handle_t, const char* key, const void* value, size_t len) {
  MockNvsState& state = nvs_mock_state();
  if (!state.open || state.readOnly || !key || !value || std::strlen(key) > 15) {
    return ESP_ERR_NVS_INVALID_HANDLE;
  }
  const uint8_t* bytes = static_cast<const uint8_t*>(value);
  state.namespaces[state.activeNamespace].pending[key] = std::vector<uint8_t>(bytes, bytes + len);
  return ESP_OK;
}

inline esp_err_t nvs_set_u8(nvs_handle_t handle, const char* key, uint8_t value) {
  return nvs_set_blob(handle, key, &value, sizeof(value));
}

inline esp_err_t nvs_get_blob(nvs_handle_t, const char* key, void* out_value, size_t* length) {
  MockNvsState& state = nvs_mock_state();
  if (!state.open || !key || !length) {
    return ESP_ERR_NVS_INVALID_HANDLE;
  }
  auto& ns = state.namespaces[state.activeNamespace];
  auto it = ns.committed.find(key);
  if (it == ns.committed.end()) {
    return ESP_ERR_NVS_NOT_FOUND;
  }
  if (!out_value) {
    *length = it->second.size();
    return ESP_OK;
  }
  if (*length < it->second.size()) {
    *length = it->second.size();
    return ESP_ERR_NVS_INVALID_LENGTH;
  }
  std::memcpy(out_value, it->second.data(), it->second.size());
  *length = it->second.size();
  return ESP_OK;
}

inline esp_err_t nvs_get_u8(nvs_handle_t handle, const char* key, uint8_t* out_value) {
  size_t len = sizeof(*out_value);
  return nvs_get_blob(handle, key, out_value, &len);
}

inline esp_err_t nvs_erase_all(nvs_handle_t) {
  MockNvsState& state = nvs_mock_state();
  if (!state.open || state.readOnly) {
    return ESP_ERR_NVS_INVALID_HANDLE;
  }
  MockNvsNamespace& ns = state.namespaces[state.activeNamespace];
  ns.pending.clear();
  ns.erasePending = true;
  return ESP_OK;
}

inline esp_err_t nvs_commit(nvs_handle_t) {
  MockNvsState& state = nvs_mock_state();
  if (!state.open) {
    return ESP_ERR_NVS_INVALID_HANDLE;
  }
  ++state.commitCount;
  if (state.commitResult != ESP_OK) {
    return state.commitResult;
  }
  MockNvsNamespace& ns = state.namespaces[state.activeNamespace];
  if (ns.erasePending) {
    ns.committed.clear();
    ns.erasePending = false;
  }
  for (const auto& item : ns.pending) {
    ns.committed[item.first] = item.second;
  }
  ns.pending.clear();
  return ESP_OK;
}
