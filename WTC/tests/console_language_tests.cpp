#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include "../src/utils/log/console_language.hpp"

static std::string source(const char* name)
{
    std::ifstream in(std::filesystem::path(__FILE__).parent_path().parent_path() / name);
    assert(in);
    return {std::istreambuf_iterator<char>(in), {}};
}

template <typename... Args>
static std::string capture(const char* format, Args... args)
{
    std::FILE* file = std::tmpfile();
    assert(file);
    const int count = console_language::print_to(file, format, args...);
    assert(count >= 0);
    std::rewind(file);
    std::string result;
    for (int c; (c = std::fgetc(file)) != EOF;) result += static_cast<char>(c);
    std::fclose(file);
    assert(result.size() == static_cast<std::size_t>(count));
    return result;
}

int main()
{
    // Before profiles load, startup is English. The same sink follows each
    // subsequent language/profile switch without changing persistence or data.
    assert(capture("loading libraries...\n") == "loading libraries...\n");
    runtime_language::set_language(0);
    const auto spanish = capture("loading libraries...\n");
    assert(spanish != "loading libraries...\n" && spanish.back() == '\n');
    runtime_language::set_language(1);
    assert(capture("loading libraries...\n") == "loading libraries...\n");
    runtime_language::set_language(0);
    // Deliberately use catalog-like data and percent signs: data is not text
    // to translate, nor a second format string. Numeric printf contracts stay.
    assert(capture("%s | %08X | %zu | %.1f | %%\n", "Connected 100%", 42u,
        std::size_t{4096}, 1.25) == "Connected 100% | 0000002A | 4096 | 1.2 | %\n");
    const std::string process = "Connected 100%";
    const auto info = capture("Process information of %s\n", process.c_str());
    assert(info.find(process) != std::string::npos && info != "Process information of Connected 100%\n");
    const std::string path = "C:\\Connected\\100%\\aces.exe";
    assert(capture("%s", path.c_str()) == path);
    assert(capture("Base Address: 0x%zx\n", std::size_t{0x1234}).find("0x1234") != std::string::npos);
    const auto includes = source("src/includes.hpp");
    assert(includes.find("console_language::print") != std::string::npos);
    const auto main = source("src/main.cpp");
    assert(main.find("menu_preferences::current = preset.ui;\n        runtime_language::set_language(menu_preferences::current.language);") != std::string::npos);
    const auto memory = source("src/memory/memory.cpp");
    assert(memory.find("printf(\"[!] Failed to get physical memory map") == std::string::npos);
    runtime_language::set_language(1);
    std::puts("PASS console language switches, raw data, printf contracts and profile wiring");
}
