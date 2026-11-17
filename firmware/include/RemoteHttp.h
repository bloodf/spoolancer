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
inline bool valueIs(const char *line, const char *expected) {
  const char *at = std::strchr(line, ':') + 1;
  while (*at == ' ' || *at == '\t') ++at;
  while (*expected) if (std::tolower(static_cast<unsigned char>(*at++)) != *expected++) return false;
  while (*at == ' ' || *at == '\t') ++at;
  return !*at;
}
inline bool header(const char *line, int &remaining, bool &lengthSeen, bool &chunked) {
  if (!line || !*line || !std::strchr(line, ':') || *line == ' ' || *line == '\t') return false;
  for (const char *c = line; *c; ++c) if (static_cast<unsigned char>(*c) < 32 && *c != '\t') return false;
  if (headerName(line, "transfer-encoding")) {
    if (lengthSeen || chunked || !valueIs(line, "chunked")) return false;
    chunked = true; return true;
  }
  if (headerName(line, "content-encoding")) return valueIs(line, "identity");
  if (!headerName(line, "content-length")) return true;
  if (lengthSeen || chunked) return false;
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
// A bounded decoder shared by the real network path and native response fixtures.
class Body {
 public:
  Body(int length, bool chunks) : remaining(length), chunked(chunks) {}
  template<class Text> bool feed(char byte, Text &output) {
    if (!chunked) {
      if (remaining == 0 || received >= bodyLimit) return false;
      output += byte; ++received;
      if (remaining > 0) --remaining;
      return true;
    }
    if (phase == Data) {
      if (received >= bodyLimit) return false;
      output += byte; ++received;
      if (--chunk == 0) phase = DataCR;
      return true;
    }
    if (++framing > 4096) return false;
    switch (phase) {
      case Size:
        if (byte == '\r') { if (!digits) return false; phase = SizeLF; return true; }
        if (extension) return byte >= 32 && byte != 127;
        if (byte == ';' && digits) { extension = true; return true; }
        if (!std::isxdigit(static_cast<unsigned char>(byte))) return false;
        {
          unsigned digit = byte <= '9' ? byte - '0' : std::tolower(static_cast<unsigned char>(byte)) - 'a' + 10;
          if (chunk > (bodyLimit - digit) / 16) return false;
          chunk = chunk * 16 + digit; ++digits;
        }
        return true;
      case SizeLF:
        if (byte != '\n' || chunk > bodyLimit - received) return false;
        phase = chunk ? Data : Trailer; return true;
      case DataCR:
        if (byte != '\r') return false;
        phase = DataLF; return true;
      case DataLF:
        if (byte != '\n') return false;
        phase = Size; chunk = digits = 0; extension = false; return true;
      case Trailer:
        if (byte == '\r') { phase = TrailerLF; return true; }
        if (byte < 32 || byte == 127 || ++trailerLength > 512) return false;
        return true;
      case TrailerLF:
        if (byte != '\n') return false;
        phase = trailerLength ? Trailer : Done; trailerLength = 0; return true;
      default: return false;
    }
  }
  bool done(bool connected) const {
    return chunked ? phase == Done : remaining == 0 || (remaining < 0 && !connected);
  }
 private:
  enum Phase { Size, SizeLF, Data, DataCR, DataLF, Trailer, TrailerLF, Done } phase = Size;
  int remaining;
  bool chunked, extension = false;
  size_t chunk = 0, digits = 0, received = 0, framing = 0, trailerLength = 0;
};
}
