#pragma once
#include <cstddef>
#include <cstring>
#include <cctype>

namespace RemoteHttp {
constexpr size_t bodyLimit = 20000;
inline bool status(const char *line, int &code) {
  if (!line || std::strlen(line) < 12 ||
      (std::strncmp(line, "HTTP/1.0 ", 9) && std::strncmp(line, "HTTP/1.1 ", 9)) ||
      (line[12] && line[12] != ' ')) return false;
  code = 0;
  for (int i = 9; i < 12; ++i) {
    if (line[i] < '0' || line[i] > '9') return false;
    code = code * 10 + line[i] - '0';
  }
  return code >= 100 && code <= 599;
}
inline bool headerName(const char *line, const char *name) {
  while (*name) {
    if (!*line || std::tolower(static_cast<unsigned char>(*line++)) != *name++) return false;
  }
  return *line == ':';
}
inline bool header(const char *line, int &remaining, bool &lengthSeen) {
  if (!line || !*line || !std::strchr(line, ':') || *line == ' ' || *line == '\t') return false;
  for (const char *c = line; *c; ++c) if (static_cast<unsigned char>(*c) < 32 && *c != '\t') return false;
  if (headerName(line, "transfer-encoding") || headerName(line, "content-encoding")) return false;
  if (!headerName(line, "content-length")) return true;
  if (lengthSeen) return false;
  const char *at = std::strchr(line, ':') + 1;
  while (*at == ' ' || *at == '\t') ++at;
  int size = 0; unsigned digits = 0;
  while (*at >= '0' && *at <= '9') {
    if (++digits > 5) return false;
    size = size * 10 + *at++ - '0';
  }
  while (*at == ' ' || *at == '\t') ++at;
  if (!digits || *at || size > static_cast<int>(bodyLimit)) return false;
  lengthSeen = true; remaining = size; return true;
}
inline bool complete(int remaining, bool connected, size_t received) {
  return received <= bodyLimit && (remaining == 0 || (remaining < 0 && !connected));
}
}
