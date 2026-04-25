#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string>

namespace hilsim {

inline void appendSimJsonString(std::string& out, const std::string& value) {
  out += '"';
  static const char kHex[] = "0123456789abcdef";
  for (unsigned char ch : value) {
    switch (ch) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (ch < 0x20) {
          out += "\\u00";
          out += kHex[(ch >> 4) & 0x0F];
          out += kHex[ch & 0x0F];
        } else {
          out += (char)ch;
        }
        break;
    }
  }
  out += '"';
}

inline std::string simJsonString(const std::string& value) {
  std::string out;
  out.reserve(value.size() + 2);
  appendSimJsonString(out, value);
  return out;
}

}  // namespace hilsim
