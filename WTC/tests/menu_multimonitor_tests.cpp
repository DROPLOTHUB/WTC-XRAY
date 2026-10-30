#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <cstdio>
static std::string contents(const std::filesystem::path& path) {
    std::ifstream input(path); assert(input);
    return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
}
int main() {
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
    const auto main=contents(root/"src"/"main.cpp");
    const auto menu=contents(root/"src"/"gui"/"tactical_menu.hpp");
    const auto backend=contents(root/"lib"/"imgui"/"backends"/"imgui_impl_dx11.cpp");
    assert(main.find("if (sdk::menu_multimonitor_enabled) io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable")!=std::string::npos);
    assert(main.find("else io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable")!=std::string::npos);
    assert(menu.find("Menu en otro monitor (experimental)")!=std::string::npos);
    assert(main.find("ConfigViewportsNoAutoMerge = true")!=std::string::npos);
    assert(main.find("ImGui::UpdatePlatformWindows();")!=std::string::npos);
    assert(main.find("ImGui::RenderPlatformWindowsDefault();")!=std::string::npos);
    assert(main.find("ImGui::DestroyPlatformWindows();")!=std::string::npos);
    assert(main.find("io.IniFilename = nullptr")!=std::string::npos);
    assert(main.find("WS_EX_NOREDIRECTIONBITMAP")!=std::string::npos);
    assert(menu.find("GetBackgroundDrawList(ImGui::GetWindowViewport())")!=std::string::npos);
    assert(backend.find("Renderer_CreateWindow = ImGui_ImplDX11_CreateWindow")!=std::string::npos);
    std::puts("PASS movable menu viewport separate from transparent overlay; needs dual-monitor live test");
}
