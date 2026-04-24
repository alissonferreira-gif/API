#pragma once

#include <cstdio>
#include <format>
#include <string>
#include <utility>

namespace print_compat {

inline void println() {
    std::fputc('\n', stdout);
}

template <class... Args>
inline void print(std::format_string<Args...> fmt, Args&&... args) {
    std::string out = std::format(fmt, std::forward<Args>(args)...);
    std::fwrite(out.data(), 1, out.size(), stdout);
}

template <class... Args>
inline void println(std::format_string<Args...> fmt, Args&&... args) {
    std::string out = std::format(fmt, std::forward<Args>(args)...);
    out.push_back('\n');
    std::fwrite(out.data(), 1, out.size(), stdout);
}

template <class... Args>
inline void println(std::FILE* stream, std::format_string<Args...> fmt, Args&&... args) {
    std::string out = std::format(fmt, std::forward<Args>(args)...);
    out.push_back('\n');
    std::fwrite(out.data(), 1, out.size(), stream);
}

} // namespace print_compat

using print_compat::print;
using print_compat::println;
