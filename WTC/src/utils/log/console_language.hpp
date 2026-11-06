#pragma once

#include <cstdio>
#include "../../gui/runtime_language.hpp"

namespace console_language
{
    // Localize the format, never OS/process/module/path arguments. Keeping the
    // printf call intact preserves its conversions, return value and data bytes.
    template <typename... Args>
    inline int print_to(std::FILE* stream, const char* format, Args... args)
    {
        const auto localized = runtime_language::text(format);
        return std::fprintf(stream, localized.c_str(), args...);
    }

    template <typename... Args>
    inline int print(const char* format, Args... args)
    {
        return print_to(stdout, format, args...);
    }
}
