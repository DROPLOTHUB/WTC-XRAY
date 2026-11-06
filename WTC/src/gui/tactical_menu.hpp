#pragma once
// Included inside namespace GUI. Public ImGui API only; no game logic here.
// Display translation is separate from source IDs and favorite keys. Never saves settings.
// Localize presentation (including runtime diagnostics and clipboard reports), never data or IDs.
namespace TacticalUI {
inline std::string LocalText(const char* source) {
    return runtime_language::text(menu_preferences::label(source),menu_preferences::current.language);
}
template<class... A> inline std::string Format(const char* source,A... args) {
    return runtime_language::format(menu_preferences::label(source),menu_preferences::current.language,args...);
}
inline std::string WidgetLabel(const char* source) {
    const std::string original=menu_preferences::widget_label(source);
    const auto end=original.find("##");
    return LocalText(original.substr(0,end).c_str())+(end==std::string::npos?"":original.substr(end));
}
template<class... A> inline auto Button(const char* source,A... args) { return ImGui::Button(WidgetLabel(source).c_str(),args...); }
template<class... A> inline auto SmallButton(const char* source,A... args) { return ImGui::SmallButton(WidgetLabel(source).c_str(),args...); }
template<class... A> inline auto Checkbox(const char* source,A... args) { return ImGui::Checkbox(WidgetLabel(source).c_str(),args...); }
template<class... A> inline auto Selectable(const char* source,A... args) { return ImGui::Selectable(WidgetLabel(source).c_str(),args...); }
template<class... A> inline auto MenuItem(const char* source,A... args) { return ImGui::MenuItem(WidgetLabel(source).c_str(),args...); }
template<class... A> inline auto CollapsingHeader(const char* source,A... args) { return ImGui::CollapsingHeader(WidgetLabel(source).c_str(),args...); }
template<class... A> inline auto TreeNodeEx(const char* source,A... args) { return ImGui::TreeNodeEx(WidgetLabel(source).c_str(),args...); }
template<class... A> inline auto TreeNode(const char* source,A... args) { return ImGui::TreeNode(WidgetLabel(source).c_str(),args...); }
template<class... A> inline auto BeginPopupModal(const char* source,A... args) { return ImGui::BeginPopupModal(WidgetLabel(source).c_str(),args...); }
template<class... A> inline auto OpenPopup(const char* source,A... args) { return ImGui::OpenPopup(WidgetLabel(source).c_str(),args...); }
template<class... A> inline auto BeginCombo(const char* source,const char* preview,A... args) { return ImGui::BeginCombo(WidgetLabel(source).c_str(),LocalText(preview).c_str(),args...); }
template<class... A> inline void Text(const char* source,A... args) { ImGui::TextUnformatted(Format(source,args...).c_str()); }
template<class... A> inline void TextDisabled(const char* source,A... args) { ImGui::PushStyleColor(ImGuiCol_Text,ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)); Text(source,args...); ImGui::PopStyleColor(); }
template<class... A> inline void TextWrapped(const char* source,A... args) { ImGui::PushTextWrapPos(0.f); Text(source,args...); ImGui::PopTextWrapPos(); }
inline void TextUnformatted(const char* source) { ImGui::TextUnformatted(LocalText(source).c_str()); }
inline void SeparatorText(const char* source) { ImGui::SeparatorText(LocalText(source).c_str()); }
template<class... A> inline void SetTooltip(const char* source,A... args) { ImGui::SetTooltip("%s",Format(source,args...).c_str()); }
template<class... A> inline void BulletText(const char* source,A... args) { ImGui::Bullet(); ImGui::TextUnformatted(Format(source,args...).c_str()); }
template<class... A> inline void TextColored(const ImVec4& color,const char* source,A... args) { ImGui::PushStyleColor(ImGuiCol_Text,color); Text(source,args...); ImGui::PopStyleColor(); }
template<class... A> inline bool InputTextWithHint(const char* id,const char* hint,A... args) { return ImGui::InputTextWithHint(id,LocalText(hint).c_str(),args...); }
inline bool Combo(const char* source,int* value,const char* const items[],int count) {
    std::vector<std::string> storage; storage.reserve(count);
    for(int i=0;i<count;++i)storage.push_back(LocalText(items[i]));
    std::vector<const char*> translated; translated.reserve(count);
    for(const auto& item:storage)translated.push_back(item.c_str());
    return ImGui::Combo(WidgetLabel(source).c_str(),value,translated.data(),count);
}
}

inline char tactical_search[128] = {};
inline int tactical_page = 4; // Combat / Aimbot on first open
inline int tactical_tab = 0; // General / Other
inline bool tactical_sidebar_open = true;
inline bool tactical_columns_active = false;
inline ImFont* tactical_font = nullptr;
inline ImGuiID tactical_capture = 0;
inline std::string tactical_reveal_label, tactical_reveal_header, tactical_hotkey_notice;
inline std::array<int*,13> TacticalHotkeyPointers() { return {
    &menu_preferences::current.g2a_key,&sdk::xray_toggle_key,&sdk::key_esp_enable,&sdk::key_aimbot_enable,&sdk::key_air_lead_enable,
    &sdk::key_3d_box,&sdk::key_2d_box,&sdk::key_vehicle_name,&sdk::key_distance,
    &sdk::key_show_crew,&sdk::key_show_barrel,&sdk::key_show_ammorack,&sdk::key_show_fueltank};
}
inline bool tactical_favorites_only=false;
inline void TacticalSearchFocus(const char* label) {
    ImGui::PushID(label);
    if(ImGui::BeginPopupContextItem("##favorite")) {
        const bool selected=menu_preferences::current.favorites.contains(label);
        if(TacticalUI::MenuItem(selected?"Quitar de favoritos":"Anadir a favoritos"))
            menu_preferences::toggle_favorite(label);
        ImGui::EndPopup();
    }
    ImGui::PopID();
    if(tactical_reveal_header.empty() && tactical_reveal_label == label) {
        ImGui::SetScrollHereY(.35f); tactical_reveal_label.clear();
    }
}
inline bool TacticalAdvanced(const char* title) {
    if(tactical_reveal_header==title) { ImGui::SetNextItemOpen(true,ImGuiCond_Always); tactical_reveal_header.clear(); }
    if(!menu_preferences::current.advanced)return false;
    return tactical_search[0] || TacticalUI::CollapsingHeader(title);
}
inline void TacticalSection(const char* title)
{
    ImGui::Spacing();
    TacticalUI::TextColored(ImVec4(.91f,.32f,.36f,1.f), "%s", menu_preferences::label(title));
    ImGui::Spacing();
}
inline int tactical_column_count = 1;
inline void TacticalColumnsBegin(float scale, const char* id="##detail_columns")
{
    // Two columns only when each setting panel can remain comfortably wide.
    // Otherwise use separate full-width table rows instead of overlapping cells.
    tactical_column_count = ImGui::GetContentRegionAvail().x >= 1060.f * scale ? 2 : 1;
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(16.f*scale, 14.f*scale));
    tactical_columns_active = ImGui::BeginTable(id, tactical_column_count,
        ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_RowBg | ImGuiTableFlags_PadOuterX);
    if (tactical_columns_active) { ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); }
}
inline void TacticalNextColumn()
{
    if (!tactical_columns_active) { ImGui::Spacing(); ImGui::Separator(); return; }
    if (tactical_column_count == 1) { ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); }
    else ImGui::TableSetColumnIndex(1);
}
inline void TacticalColumnsEnd()
{
    if (tactical_columns_active) ImGui::EndTable();
    ImGui::PopStyleVar();
    tactical_columns_active = false;
    ImGui::Spacing();
}
inline bool TacticalSliderFloat(const char* label, float* value, float min, float max, const char* format = "%.3f")
{
    const std::string localized_format=TacticalUI::LocalText(format);
    format=localized_format.c_str();
    ImGui::PushID(label);
    TacticalUI::TextDisabled("%s",menu_preferences::label(label));
    const float width=ImGui::GetContentRegionAvail().x;
    const float number_width=std::clamp(width*.24f,75.f,105.f);
    ImGui::SetNextItemWidth(std::max(48.f,width-number_width-ImGui::GetStyle().ItemSpacing.x));
    ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0,0,0,0));
    bool changed=ImGui::SliderFloat("##range",value,min,max,format,ImGuiSliderFlags_NoInput);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(number_width);
    if (ImGui::InputFloat("##exact",value,0.f,0.f,format)) {
        *value=std::clamp(*value,min,max);
        changed=true;
    }
    TacticalSearchFocus(label);
    ImGui::PopID();
    return changed;
}
inline bool TacticalSliderInt(const char* label, int* value, int min, int max, const char* format = "%d")
{
    const std::string localized_format=TacticalUI::LocalText(format);
    format=localized_format.c_str();
    ImGui::PushID(label);
    TacticalUI::TextDisabled("%s",menu_preferences::label(label));
    const float width=ImGui::GetContentRegionAvail().x;
    const float number_width=std::clamp(width*.24f,75.f,105.f);
    ImGui::SetNextItemWidth(std::max(48.f,width-number_width-ImGui::GetStyle().ItemSpacing.x));
    ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0,0,0,0));
    bool changed=ImGui::SliderInt("##range",value,min,max,format,ImGuiSliderFlags_NoInput);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(number_width);
    if (ImGui::InputInt("##exact",value,0,0)) {
        *value=std::clamp(*value,min,max);
        changed=true;
    }
    TacticalSearchFocus(label);
    ImGui::PopID();
    return changed;
}
inline bool TacticalColorEdit(const char* label, float* rgba)
{
    // Stock ColorEdit's picker owns English prose/options. Use geometry only, with our bilingual chrome.
    ImGui::PushID(label);
    static std::array<float,4> original{};
    bool changed=false;
    constexpr auto swatch_flags=ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_AlphaPreviewHalf;
    ImGui::BeginGroup();
    if(ImGui::ColorButton("##color",ImVec4(rgba[0],rgba[1],rgba[2],rgba[3]),swatch_flags)) {
        std::copy_n(rgba,4,original.begin());
        ImGui::OpenPopup("##color_picker");
    }
    // Preserve color drag/drop without the stock English "Color" drag tooltip.
    if(ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_4F,rgba,4*sizeof(float));
        TacticalUI::TextUnformatted(label);
        ImGui::EndDragDropSource();
    }
    if(ImGui::BeginDragDropTarget()) {
        if(const auto* payload=ImGui::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_4F)) {
            if(payload->DataSize==4*sizeof(float)) { std::copy_n(static_cast<const float*>(payload->Data),4,rgba);changed=true; }
        }
        if(const auto* payload=ImGui::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_3F)) {
            if(payload->DataSize==3*sizeof(float)) { std::copy_n(static_cast<const float*>(payload->Data),3,rgba);changed=true; }
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::SameLine();TacticalUI::TextUnformatted(label);
    ImGui::EndGroup();
    TacticalSearchFocus(label);
    if(ImGui::IsItemHovered()) TacticalUI::SetTooltip("Editar color: %s",menu_preferences::label(label));
    if(ImGui::BeginPopup("##color_picker")) {
        TacticalUI::SeparatorText(label);
        ImGui::SetNextItemWidth(260.f*std::clamp(sdk::menu_ui_scale,.8f,1.5f));
        constexpr auto picker_flags=ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoOptions |
            ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoSidePreview |
            ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_AlphaBar |
            ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_PickerHueBar;
        changed=ImGui::ColorPicker4("##picker",rgba,picker_flags)||changed;
        const char* channels[]={"Rojo","Verde","Azul","Alfa"};
        for(int i=0;i<4;++i) {
            ImGui::PushID(i);
            ImGui::SetNextItemWidth(260.f*std::clamp(sdk::menu_ui_scale,.8f,1.5f));
            changed=ImGui::SliderFloat(TacticalUI::WidgetLabel(channels[i]).c_str(),&rgba[i],0.f,1.f,"%.3f",ImGuiSliderFlags_AlwaysClamp)||changed;
            ImGui::PopID();
        }
        TacticalUI::TextUnformatted("Actual");ImGui::SameLine();
        ImGui::ColorButton("##current",ImVec4(rgba[0],rgba[1],rgba[2],rgba[3]),swatch_flags);
        ImGui::SameLine();TacticalUI::TextUnformatted("Original");ImGui::SameLine();
        if(ImGui::ColorButton("##original",ImVec4(original[0],original[1],original[2],original[3]),swatch_flags)) {
            std::copy_n(original.begin(),4,rgba);changed=true;
        }
        if(TacticalUI::Button("Restaurar color")) {std::copy_n(original.begin(),4,rgba);changed=true;}
        // Keep all four numeric clipboard forms offered by stock ColorEdit (no translated data).
        char numeric[128];
        if(TacticalUI::Button("Copiar RGBA (float)")) {
            std::snprintf(numeric,sizeof(numeric),"(%.3ff, %.3ff, %.3ff, %.3ff)",rgba[0],rgba[1],rgba[2],rgba[3]);
            ImGui::SetClipboardText(numeric);
        }
        int bytes[4];for(int i=0;i<4;++i)bytes[i]=static_cast<int>(std::clamp(rgba[i],0.f,1.f)*255.f+.5f);
        if(TacticalUI::Button("Copiar RGBA (0..255)")) {
            std::snprintf(numeric,sizeof(numeric),"(%d,%d,%d,%d)",bytes[0],bytes[1],bytes[2],bytes[3]);
            ImGui::SetClipboardText(numeric);
        }
        if(TacticalUI::Button("Copiar RGB hexadecimal")) {
            std::snprintf(numeric,sizeof(numeric),"#%02X%02X%02X",bytes[0],bytes[1],bytes[2]);
            ImGui::SetClipboardText(numeric);
        }
        if(TacticalUI::Button("Copiar RGBA hexadecimal")) {
            std::snprintf(numeric,sizeof(numeric),"#%02X%02X%02X%02X",bytes[0],bytes[1],bytes[2],bytes[3]);
            ImGui::SetClipboardText(numeric);
        }
        if(TacticalUI::Button("Cerrar"))ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopID();
    return changed;
}
inline void TacticalNavItem(const char* label, int page, float scale)
{
    ImGui::PushID(page);
    const bool selected = tactical_page == page && !tactical_search[0];
    const ImVec2 p = ImGui::GetCursorScreenPos();
    if (TacticalUI::Selectable(label, selected, 0, ImVec2(0, 32.f*scale)))
    { tactical_page = page; tactical_tab = 0; tactical_search[0] = 0; }
    if (selected)
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(p.x,p.y+3.f),ImVec2(p.x+3.f,p.y+29.f*scale),ImGui::GetColorU32(ImGuiCol_CheckMark));
    ImGui::PopID();
}
inline void TacticalShield(float scale, bool expanded)
{
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const ImVec2 v[5] = { {p.x+4*scale,p.y+4*scale},{p.x+30*scale,p.y+4*scale},
        {p.x+30*scale,p.y+22*scale},{p.x+17*scale,p.y+36*scale},{p.x+4*scale,p.y+22*scale} };
    ImDrawList* d = ImGui::GetWindowDrawList();
    d->AddConvexPolyFilled(v,5,IM_COL32(91,15,26,255));
    d->AddPolyline(v,5,IM_COL32(244,44,63,255),ImDrawFlags_Closed,2.f*scale);
    d->AddText(ImVec2(p.x+10*scale,p.y+9*scale),IM_COL32(255,239,239,255),"W");
    if(expanded) {
        d->AddText(ImVec2(p.x+42*scale,p.y+6*scale),IM_COL32(249,248,250,255),"WTC");
        d->AddText(ImVec2(p.x+42*scale,p.y+23*scale),IM_COL32(142,143,151,255),app_version::sidebar_label());
    }
    ImGui::Dummy(ImVec2(0,44*scale));
}
inline bool TacticalToggle(const char* label, bool* value)
{
    ImGui::PushID(label);
    const float h=ImGui::GetFrameHeight()+2.f;
    const ImVec2 p=ImGui::GetCursorScreenPos();
    const float width=ImGui::GetContentRegionAvail().x;
    ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.095f,.10f,.11f,1.f));
    const bool clicked=TacticalUI::Button("##switch",ImVec2(width,h));
    ImGui::PopStyleColor();
    if(clicked) *value=!*value;
    ImDrawList* d=ImGui::GetWindowDrawList();
    const float switch_x=p.x+width-h*1.9f;
    const ImU32 track=ImGui::GetColorU32(*value ? ImGuiCol_CheckMark : ImGuiCol_Border);
    d->AddRectFilled(ImVec2(switch_x,p.y+h*.19f),ImVec2(switch_x+h*1.6f,p.y+h*.81f),track,h*.35f);
    d->AddCircleFilled(ImVec2(switch_x+h*(*value?1.23f:.37f),p.y+h*.5f),h*.21f,IM_COL32(245,245,247,255));
    if (*value) d->AddRectFilled(p,ImVec2(p.x+2.f,p.y+h),ImGui::GetColorU32(ImGuiCol_CheckMark));
    const ImVec2 text_min(p.x+10.f,p.y),text_max(std::max(p.x+11.f,switch_x-8.f),p.y+h);
    d->PushClipRect(text_min,text_max,true);
    d->AddText(ImVec2(p.x+10.f,p.y+(h-ImGui::GetFontSize())*.5f),ImGui::GetColorU32(ImGuiCol_Text),TacticalUI::LocalText(label).c_str());
    d->PopClipRect();
    if(ImGui::IsItemHovered() && ImGui::CalcTextSize(TacticalUI::LocalText(label).c_str()).x > text_max.x-text_min.x)
        TacticalUI::SetTooltip("%s",menu_preferences::label(label));
    TacticalSearchFocus(label);
    ImGui::PopID();
    return clicked;
}
inline void TacticalKeybind(const char* label, int& key)
{
    ImGuiID& capture=tactical_capture;
    static bool previous[256]={};
    ImGui::PushID(label);
    const ImGuiID id=ImGui::GetID("capture");
    char name[64]; strcpy_s(name,TacticalUI::LocalText("Sin asignar").c_str());
    if(key==VK_LBUTTON) strcpy_s(name,TacticalUI::LocalText("Mouse 1").c_str());
    else if(key==VK_RBUTTON) strcpy_s(name,TacticalUI::LocalText("Mouse 2").c_str());
    else if(key==VK_MBUTTON) strcpy_s(name,TacticalUI::LocalText("Mouse 3").c_str());
    else if(key>0) { const UINT scan=MapVirtualKeyA(key,MAPVK_VK_TO_VSC); if(!GetKeyNameTextA(static_cast<LONG>(scan<<16),name,64)) sprintf_s(name,"VK %d",key); }
    const float row_width=ImGui::GetContentRegionAvail().x;
    const float button_width=std::min(175.f,std::max(110.f,row_width*.42f));
    const float left=ImGui::GetCursorPosX();
    const bool fits=ImGui::CalcTextSize(TacticalUI::LocalText(label).c_str()).x+button_width+ImGui::GetStyle().ItemSpacing.x < row_width;
    if(fits) {
        TacticalUI::TextUnformatted(menu_preferences::label(label));
        ImGui::SameLine();
        ImGui::SetCursorPosX(left+row_width-button_width);
    } else TacticalUI::TextWrapped("%s",menu_preferences::label(label));
    if(ImGui::Button((std::string(capture==id?TacticalUI::LocalText("Pulsa una tecla...").c_str():name)+"###key_capture").c_str(),ImVec2(button_width,0))) {
        capture=id; for(int k=1;k<256;++k) previous[k]=(GetAsyncKeyState(k)&0x8000)!=0;
    }
    if(capture==id) for(int k=1;k<256;++k) {
        const bool down=(GetAsyncKeyState(k)&0x8000)!=0;
        if(down&&!previous[k]) {
            if(k==VK_BACK) { key=0; tactical_hotkey_notice.clear(); }
            else if(k!=VK_ESCAPE) {
                const auto pointers=TacticalHotkeyPointers();
                std::array<int,13> keys{};
                std::size_t owner=keys.size();
                for(std::size_t i=0;i<keys.size();++i) { keys[i]=*pointers[i]; if(pointers[i]==&key) owner=i; }
                if(owner<keys.size() && hotkey_validation::valid(k) && !hotkey_validation::taken(keys,owner,k)) {
                    key=k; tactical_hotkey_notice.clear();
                } else tactical_hotkey_notice=label; // Keep source until display so changing language updates existing notices.
            }
            capture=0; break;
        }
        previous[k]=down;
    }
    if(key && !hotkey_validation::valid(key)) TacticalUI::TextColored(ImVec4(1.f,.4f,.4f,1.f),"Atajo reservado o inválido");
    TacticalSearchFocus(label);
    ImGui::PopID();
}
inline void TacticalPreview()
{
    TacticalUI::SeparatorText("Vista previa ilustrativa");
    const ImVec2 p=ImGui::GetCursorScreenPos();
    const float w=ImGui::GetContentRegionAvail().x;
    const float h=150*std::clamp(sdk::menu_ui_scale,.8f,1.5f);
    auto* d=ImGui::GetWindowDrawList();
    d->AddRectFilled(p,ImVec2(p.x+w,p.y+h),IM_COL32(12,13,17,255),5);
    d->PushClipRect(p,ImVec2(p.x+w,p.y+h),true);
    for(float gx=p.x+18.f;gx<p.x+w;gx+=24.f) d->AddLine(ImVec2(gx,p.y),ImVec2(gx,p.y+h),IM_COL32(145,150,155,19));
    for(float gy=p.y+18.f;gy<p.y+h;gy+=24.f) d->AddLine(ImVec2(p.x,gy),ImVec2(p.x+w,gy),IM_COL32(145,150,155,19));
    d->AddText(ImVec2(p.x+12,p.y+9),IM_COL32(239,50,69,230),TacticalUI::LocalText("SIMULATION / PREVIEW").c_str());
    const float cx=p.x+w*.45f, cy=p.y+h*.55f;
    const ImU32 col=ImGui::ColorConvertFloat4ToU32(sdk::color_fighter);
    if(sdk::esp_show_boxes||sdk::esp_use_3d_boxes) d->AddRect(ImVec2(cx-35,cy-25),ImVec2(cx+35,cy+25),col,3);
    if(sdk::esp_show_vehicle_names) d->AddText(ImVec2(cx-30,cy-45),col,TacticalUI::LocalText("Fighter").c_str());
    if(sdk::esp_show_distance) d->AddText(ImVec2(cx-25,cy+32),col,"1200 m");
    ImVec4 lead=sdk::air_lead_sim_color; lead.w*=std::clamp(sdk::air_lead_opacity,0.f,1.f);
    const ImU32 lc=ImGui::ColorConvertFloat4ToU32(lead);
    if(sdk::air_lead_enabled) {
        const ImVec2 aim(cx+110,cy-18);
        if(sdk::air_lead_coverage_opacity) {
            ::lead_opacity::line(d,ImVec2(cx,cy),aim,lc,1.f,lead.w);
            ::lead_opacity::circle(d,aim,sdk::air_lead_marker_size,lc,lead.w);
            ::lead_opacity::line(d,ImVec2(aim.x-10,aim.y),ImVec2(aim.x+10,aim.y),lc,1.f,lead.w);
            ::lead_opacity::line(d,ImVec2(aim.x,aim.y-10),ImVec2(aim.x,aim.y+10),lc,1.f,lead.w);
        } else {
            d->AddLine(ImVec2(cx,cy),aim,lc); d->AddCircle(aim,sdk::air_lead_marker_size,lc);
            d->AddLine(ImVec2(aim.x-10,aim.y),ImVec2(aim.x+10,aim.y),lc);
            d->AddLine(ImVec2(aim.x,aim.y-10),ImVec2(aim.x,aim.y+10),lc);
        }
    }
    d->PopClipRect();
    ImGui::Dummy(ImVec2(w,h));
}
inline void TacticalStyle(float scale)
{
    ImGui::StyleColorsDark();
    auto& s=ImGui::GetStyle();
    s.WindowPadding=ImVec2(18,16); s.FramePadding=ImVec2(10,7); s.ItemSpacing=ImVec2(9,8);
    s.WindowRounding=7; s.ChildRounding=5; s.FrameRounding=3; s.GrabRounding=2;
    s.PopupRounding=4; s.ScrollbarRounding=4; s.WindowBorderSize=1; s.ChildBorderSize=1;
    // All available variations stay within the red tactical palette.
    const ImVec4 reds[]={{.96f,.15f,.22f,1},{.86f,.12f,.17f,1},{1.f,.22f,.26f,1},
        {.76f,.18f,.25f,1},{.96f,.27f,.22f,1},{.86f,.12f,.30f,1},{1.f,.19f,.17f,1}};
    const ImVec4 a=reds[std::clamp(sdk::menu_style,0,6)];
    s.Colors[ImGuiCol_WindowBg]=ImVec4(.025f,.028f,.032f,.98f);
    s.Colors[ImGuiCol_TitleBg]=ImVec4(.032f,.035f,.039f,1.f);
    s.Colors[ImGuiCol_TitleBgActive]=ImVec4(.085f,.027f,.032f,1.f);
    s.Colors[ImGuiCol_ChildBg]=ImVec4(.055f,.057f,.065f,.96f);
    s.Colors[ImGuiCol_PopupBg]=ImVec4(.08f,.082f,.088f,.99f);
    s.Colors[ImGuiCol_FrameBg]=ImVec4(.095f,.098f,.109f,1.f);
    s.Colors[ImGuiCol_FrameBgHovered]=ImVec4(.17f,.12f,.13f,1.f);
    s.Colors[ImGuiCol_FrameBgActive]=ImVec4(.24f,.10f,.12f,1.f);
    s.Colors[ImGuiCol_Button]=ImVec4(.11f,.11f,.12f,1.f);
    s.Colors[ImGuiCol_ButtonHovered]=ImVec4(.24f,.12f,.14f,1.f);
    s.Colors[ImGuiCol_ButtonActive]=ImVec4(.34f,.09f,.12f,1.f);
    s.Colors[ImGuiCol_Border]=ImVec4(.20f,.21f,.24f,1.f);
    s.Colors[ImGuiCol_Separator]=ImVec4(.20f,.21f,.23f,.62f);
    s.Colors[ImGuiCol_TableBorderLight]=ImVec4(.20f,.21f,.23f,.50f);
    s.Colors[ImGuiCol_TableRowBg]=ImVec4(.10f,.105f,.12f,.38f);
    s.Colors[ImGuiCol_TableRowBgAlt]=ImVec4(.08f,.085f,.10f,.38f);
    s.Colors[ImGuiCol_CheckMark]=a;
    s.Colors[ImGuiCol_SliderGrab]=a; s.Colors[ImGuiCol_SliderGrabActive]=ImVec4(1.f,.38f,.40f,1.f);
    s.Colors[ImGuiCol_Header]=ImVec4(a.x*.32f,a.y*.20f,a.z*.22f,1.f);
    s.Colors[ImGuiCol_HeaderHovered]=ImVec4(a.x*.48f,a.y*.25f,a.z*.27f,1.f);
    s.Colors[ImGuiCol_HeaderActive]=ImVec4(a.x*.65f,a.y*.30f,a.z*.32f,1.f);
    s.Colors[ImGuiCol_Text]=ImVec4(.95f,.95f,.97f,1.f);
    s.Colors[ImGuiCol_TextDisabled]=ImVec4(.58f,.59f,.64f,1.f);
    s.Colors[ImGuiCol_NavHighlight]=a;
    s.ScaleAllSizes(scale);
}
struct TacticalSearchGroup { int page; int tab; const char* header; const char* labels; };
inline constexpr TacticalSearchGroup tactical_index[] = {
    {0,0,"","Enable ESP|Show FPS|Radar|Air Lead|Misiles"},
    {0,1,"","Rendimiento|Edad snapshot|Fijar referencia (antes)"},
    {1,0,"","Only Show Planes|Filtrar aeronaves estaticas|Show Bomb Impact|Render Distance|Box Distance|2D Box|3D Box|Vehicle Names|Distance|Reload Progress|Seconds|Name Size|Distance Size|Reload Size|Module Name Size"},
    {1,1,"Mostrar tipos de vehículo","Helicopter|Bomber|Fighter|Attacker|Recon Drone|Light Tank|Heavy Tank|Medium Tank|SPAA|Gun Boat|Destroyer|Cruiser|Torpedo Boat|Death State Threshold"},
    {1,1,"Colores de vehiculos","Helicopter|Bomber|Fighter|Attacker|Recon Drone|Light Tank|Heavy Tank|Medium Tank|SPAA|Gun Boat|Destroyer|Cruiser|Torpedo Boat"},
    {2,0,"","Enable X-Ray|Module Names|Modo de representación|Wireframe Overlay|Gun Barrel|Cannon Breech|Crew|Fuel Tank|Ammo Rack|Engine|Transmission|Hide small modules >|Line Thickness|Module Alpha|X-Ray Distance|Names Distance"},
    {2,1,"Colores de modulos","Gun Barrel|Cannon Breech|Crew|Fuel Tank|Ammo Rack|Engine|Transmission"},
    {3,0,"","Live Projectile ESP (experimental ECS)|Seleccionar todos los misiles|Misiles Aire-Aire|Misiles Aire-Tierra|Misiles Tierra-Aire|AAM por defecto para nombres nuevos|Only Incoming Missiles|Use Confirmed Local Player|Incoming Distance|Incoming Angle|Alerta visual de aproximacion|Sonido de alerta"},
    {3,1,"","Projectile Debug|Límite por lista ECS|Presupuesto por lista"},
    {4,0,"","Enable Air Lead|Aim Tierra-Aire (G2A)|Atajo G2A|Marker Size|Line Thickness|Opacidad aim lead|Compatibilidad alfa (sin negro)|Marker|Prediccion terrestre G2G"},
    {4,0,"Seleccion de arma A2A","Indice de arma A2A (-1 auto)|Preferir mayor calibre"},
    {4,0,"Calidad de datos del lead","G2A calidad|G2A propio|G2A municion|Muestra de municion"},
    {3,0,"Alertas","Alerta visual de aproximacion|Sonido de alerta|Nombres futuros|Activar categoria actual y futura"},
    {0,1,"Rendimiento","Ventana 512|p99"},
    {4,1,"","Use Simulation|Turn Prediction (3D)|Adaptive Smoothing|Smooth Min Alpha|Smooth Max Alpha|Extra Lead Time (s)|Aimbot key|Air Lead key|Only Target (center)|Helicopters|Recon Drones|Fighters|Bombers|Attackers"},
    {5,0,"","Enable Radar|Enemy Marker"},
    {5,1,"","Radar / estado"},
    {6,0,"","Nombre de perfil|Cargar|Guardar"},
    {6,1,"","Export Config|Import Config"},
    {7,0,"","Idioma|Monitor|Apply Fullscreen to Monitor|Apply Fullscreen on Start|V-Sync|FPS objetivo|Escala de interfaz|Acento rojo"},
    {7,1,"Atajos de teclado","Toggle ESP|2D Box|3D Box|Vehicle Name|Distance|Toggle X-Ray|Show Crew|Show Barrel|Show AmmoRack|Show Fueltank|Toggle Aimbot|Toggle Air Lead"},
    {7,1,"Offsets avanzados","Guardar clave API local|Actualizar offsets ahora|Borrar clave API local|Close Menu"},
};
inline void OnRender()
{
    if(!menu_open) { tactical_capture=0; return; }
    if((tactical_page!=7 && tactical_page!=4) || tactical_search[0]) tactical_capture=0;
    const ImGuiStyle saved_style=ImGui::GetStyle();
    const float scale=std::clamp(sdk::menu_ui_scale,.8f,1.5f);
    TacticalStyle(scale);
    ImGui::PushFont(tactical_font);
    const ImVec2 display=ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize(ImVec2(std::min(1160.f*scale,display.x*.96f),std::min(800.f*scale,display.y*.94f)),ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(std::min(770.f*scale,display.x*.96f),std::min(550.f*scale,display.y*.94f)),ImVec2(FLT_MAX,FLT_MAX));
    if(ImGui::Begin("WTC / REDLINE###WTCMenu",&menu_open,ImGuiWindowFlags_NoCollapse)) {
        ImGui::SetWindowFontScale(scale);
        const ImVec2 window_pos=ImGui::GetWindowPos(), window_size=ImGui::GetWindowSize();
        ImGui::GetBackgroundDrawList(ImGui::GetWindowViewport())->AddRectFilled(ImVec2(window_pos.x-4*scale,window_pos.y-4*scale),
            ImVec2(window_pos.x+window_size.x+4*scale,window_pos.y+window_size.y+4*scale),
            IM_COL32(0,0,0,25),8*scale);
        auto* grid=ImGui::GetWindowDrawList();
        grid->PushClipRect(window_pos,ImVec2(window_pos.x+window_size.x,window_pos.y+window_size.y),true);
        for(float x=window_pos.x+48.f*scale;x<window_pos.x+window_size.x;x+=48.f*scale)
            grid->AddLine(ImVec2(x,window_pos.y+26.f),ImVec2(x,window_pos.y+window_size.y),IM_COL32(145,151,160,6));
        for(float y=window_pos.y+50.f*scale;y<window_pos.y+window_size.y;y+=48.f*scale)
            grid->AddLine(ImVec2(window_pos.x,y),ImVec2(window_pos.x+window_size.x,y),IM_COL32(145,151,160,6));
        grid->PopClipRect();
        TacticalUI::TextColored(ImVec4(.98f,.20f,.26f,1.f),"WTC  /  REDLINE");
        ImGui::SameLine(); TacticalUI::TextDisabled("  TACTICAL CONTROL  //  %s",runtime_language::raw(app_version::release_name));
        const int selected=std::clamp(active_preset,0,4);
        const bool profile_dirty=!same_preset(capture_current_settings(),presets[selected]);
        TacticalUI::Text("PERFIL  %s",runtime_language::raw(presets[selected].name.c_str())); ImGui::SameLine();
        if(profile_dirty) { TacticalUI::TextColored(ImVec4(1.f,.61f,.30f,1.f),"  / Perfil modificado"); ImGui::SameLine(); }
        const char* state=save_status==save_status_t::saved?"Guardado":save_status==save_status_t::pending?"Sin guardar":"Error al guardar";
        TacticalUI::TextColored(save_status==save_status_t::error?ImVec4(1,.4f,.4f,1):ImVec4(.6f,.63f,.66f,1),"/  %s",menu_preferences::label(state));
        // Dedicated action row: remains usable with long profile names or narrow windows.
        if(TacticalUI::SmallButton("Guardar configuración")) save_now();
        ImGui::SameLine();
        ImGui::BeginDisabled(sdk::request_manual_reload);
        if(TacticalUI::SmallButton("Recargar SDK")) sdk::request_manual_reload = true;
        if(ImGui::IsItemHovered()) TacticalUI::SetTooltip("Reinicia las lecturas del SDK. No guarda ni descarta tus opciones.");
        ImGui::EndDisabled();
        if(sdk::request_manual_reload) { ImGui::SameLine(); TacticalUI::TextDisabled("Recarga solicitada..."); }
        else if(!sdk::manual_reload_status.empty()) TacticalUI::TextWrapped("SDK: %s",sdk::manual_reload_status.c_str());
        if(save_status==save_status_t::error){ImGui::SameLine();if(TacticalUI::SmallButton("Reintentar"))save_now();}
        ImGui::SameLine();
        if(TacticalUI::SmallButton("Cambios / Revertir")) TacticalUI::OpenPopup("Cambios sin guardar");
        if(TacticalUI::BeginPopupModal("Cambios sin guardar",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
            const auto current=snapshot_settings();
            TacticalUI::TextWrapped("Restaura todos los perfiles y opciones al ultimo guardado correcto (o al estado inicial). No escribe archivos.");
            for(std::size_t i=0;i<presets.size();++i) {
                const auto& now=int(i)==active_preset?current.current:current.saved_presets[i];
                const auto& before=int(i)==last_saved.selected?last_saved.current:last_saved.saved_presets[i];
                if(!same_preset(now,before)) {
                    int switches=0,numbers=0,colors=0;
                    for(const auto& e:bool_bindings)if(now.*(e.preset)!=before.*(e.preset))++switches;
                    for(const auto& e:int_bindings)if(now.*(e.preset)!=before.*(e.preset))++numbers;
                    for(const auto& e:float_bindings)if(now.*(e.preset)!=before.*(e.preset))++numbers;
                    for(const auto& e:color_bindings) {const auto& a=now.*(e.preset);const auto& b=before.*(e.preset);
                        if(a.x!=b.x||a.y!=b.y||a.z!=b.z||a.w!=b.w)++colors;}
                    TacticalUI::Text("Perfil %zu: %d interruptores, %d valores/atajos, %d colores",i+1,switches,numbers,colors);
                    if(now.missile_overrides!=before.missile_overrides)TacticalUI::BulletText("Seleccion de misiles");
                    if(now.radar_kinds!=before.radar_kinds)TacticalUI::BulletText("Categorias de radar");
                    if(now.missile_categories!=before.missile_categories || now.missile_policy!=before.missile_policy)TacticalUI::BulletText("Categorias / reglas futuras de misiles");
                    if(!same_alerts(now.alerts,before.alerts))TacticalUI::BulletText("Alertas y sonido de misiles");
                    if(!(now.ui==before.ui))TacticalUI::BulletText("Vista, idioma, filtro aereo, favoritos o atajo G2A");
                    if(now.name!=before.name)TacticalUI::BulletText("Nombre del perfil");
                }
            }
            if(current.selected!=last_saved.selected)TacticalUI::BulletText("Perfil activo");
            if(same_settings(current,last_saved))TacticalUI::TextDisabled("Sin cambios pendientes.");
            if(TacticalUI::Button("Revertir todos los cambios")){revert_unsaved_settings();ImGui::CloseCurrentPopup();}
            ImGui::SameLine();if(TacticalUI::Button("Cancelar"))ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        TacticalUI::Checkbox("Vista avanzada",&menu_preferences::current.advanced);
        if(!menu_preferences::current.advanced)tactical_tab=0;
        ImGui::SameLine();TacticalUI::TextDisabled("Clic derecho en una opcion: favoritos");
        TacticalUI::Checkbox("Menu en otro monitor (experimental)",&sdk::menu_multimonitor_enabled);
        if (ImGui::IsItemHovered()) TacticalUI::SetTooltip("Desactivado por defecto tras el fallo reportado.\nAl activarlo, arrastra el titulo del menu a la otra pantalla.\nSi falla el dibujo, desactivalo aqui.");
        const auto optional_failures = TargetProcess->OptionalReadFailures();
        if(optional_failures) {
            TacticalUI::TextDisabled("Lecturas opcionales fallidas: %llu", static_cast<unsigned long long>(optional_failures));
            if(ImGui::IsItemHovered()) TacticalUI::SetTooltip(
                "Lecturas opcionales (nombres/tipos/recarga) fallaron o tenian un puntero incoherente.\n"
                "Ultima direccion: 0x%llX. Los errores de lecturas esenciales siguen en consola.",
                static_cast<unsigned long long>(TargetProcess->LastOptionalFailure()));
        }
        ImGui::Separator();
        const char* pages[]={"Panel","Visuales","X-Ray","Misiles","Aimbot","Radar","Perfiles","Ajustes"};
        const float side_width=(tactical_sidebar_open?220.f:78.f)*scale;
        ImGui::BeginChild("navigation",ImVec2(side_width,0),true,
            tactical_sidebar_open ? ImGuiWindowFlags_None : (ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse));
        TacticalShield(scale,tactical_sidebar_open);
        if(TacticalUI::SmallButton(tactical_sidebar_open?"<  Contraer":" > ")) tactical_sidebar_open=!tactical_sidebar_open;
        ImGui::Spacing(); ImGui::Separator();
        if(tactical_sidebar_open) {
            TacticalNavItem("Panel de control",0,scale);
            ImGui::Spacing();
            const auto flags=ImGuiTreeNodeFlags_DefaultOpen|ImGuiTreeNodeFlags_SpanAvailWidth;
            if (tactical_page==4||tactical_page==3) ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(.98f,.23f,.29f,1.f));
            const bool combat_open=TacticalUI::TreeNodeEx("COMBAT",flags);
            if (tactical_page==4||tactical_page==3) ImGui::PopStyleColor();
            if(combat_open) { TacticalNavItem("Aimbot / Lead",4,scale); TacticalNavItem("Misiles",3,scale); ImGui::TreePop(); }
            if(tactical_page==1) ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(.98f,.23f,.29f,1.f));
            const bool visuals_open=TacticalUI::TreeNodeEx("VISUALS",flags);
            if(tactical_page==1) ImGui::PopStyleColor();
            if(visuals_open) { TacticalNavItem("Visuales",1,scale); ImGui::TreePop(); }
            if(tactical_page==2||tactical_page==5) ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(.98f,.23f,.29f,1.f));
            const bool vehicle_open=TacticalUI::TreeNodeEx("VEHICLE ESP",flags);
            if(tactical_page==2||tactical_page==5) ImGui::PopStyleColor();
            if(vehicle_open) { TacticalNavItem("X-Ray",2,scale); TacticalNavItem("Radar",5,scale); ImGui::TreePop(); }
            if(tactical_page==6||tactical_page==7) ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(.98f,.23f,.29f,1.f));
            const bool settings_open=TacticalUI::TreeNodeEx("SETTINGS",flags);
            if(tactical_page==6||tactical_page==7) ImGui::PopStyleColor();
            if(settings_open) { TacticalNavItem("Perfiles",6,scale); TacticalNavItem("Ajustes",7,scale); ImGui::TreePop(); }
            ImGui::Spacing();ImGui::Separator();TacticalUI::TextDisabled("F1   Menu\nF2   Perfil");
        } else {
            const auto compact=[&](const char* letter,int page,bool active,const char* tooltip) {
                ImGui::PushID(page);
                if(active) ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.27f,.065f,.09f,1.f));
                if(TacticalUI::Button(letter,ImVec2(-1,30.f*scale))) { tactical_page=page;tactical_tab=0;tactical_search[0]=0; }
                if(active) ImGui::PopStyleColor();
                if(ImGui::IsItemHovered()) TacticalUI::SetTooltip("%s",menu_preferences::label(tooltip));
                ImGui::PopID();
            };
            compact("C",4,tactical_page==4||tactical_page==3,"Combat / Aimbot y misiles");
            compact("V",1,tactical_page==1,"Visuales");
            compact("E",2,tactical_page==2||tactical_page==5,"Vehicle ESP / X-Ray y radar");
            compact("S",7,tactical_page==6||tactical_page==7,"Ajustes y perfiles");
        }
        ImGui::EndChild(); ImGui::SameLine();
        static int last_page=-1,last_tab=-1;
        static bool last_search=false;
        const bool searching_before_input=tactical_search[0]!=0;
        if(last_page!=tactical_page || last_tab!=tactical_tab || last_search!=searching_before_input)
            ImGui::SetNextWindowScroll(ImVec2(-1.f,0.f));
        ImGui::BeginChild("content",ImVec2(0,0),false);
        ImGui::SetNextItemWidth(-1);TacticalUI::InputTextWithHint("##search","Buscar opcion o seccion...",tactical_search,IM_ARRAYSIZE(tactical_search));
        if(searching_before_input != (tactical_search[0]!=0)) ImGui::SetScrollY(0.f);
        last_page=tactical_page;last_tab=tactical_tab;last_search=tactical_search[0]!=0;
        ImGuiTextFilter filter(tactical_search);
        const char* section_desc[] = {
            "Estado principal y accesos rapidos", "Cajas, etiquetas, colores y filtros",
            "Modelos, modulos y apariencia", "Misiles, filtros y distancias",
            "Prediccion A2A, Tierra-Aire y G2G", "Radar y categorias",
            "Seleccion e intercambio de perfiles", "Pantalla, atajos y offsets"
        };
        ImGui::Spacing();
        TacticalUI::TextColored(ImVec4(.98f,.23f,.29f,1.f),"%s", menu_preferences::label(tactical_search[0] ? "RESULTADOS" : pages[std::clamp(tactical_page,0,7)]));
        if (!tactical_search[0]) TacticalUI::TextDisabled("%s",menu_preferences::label(section_desc[std::clamp(tactical_page,0,7)]));
        if(!tactical_search[0]) {
            ImGui::PushID(tactical_page);
            const auto tab=[&](const char* label,int index) {
                ImGui::PushID(index);
                const bool active=tactical_tab==index;
                ImGui::PushStyleColor(ImGuiCol_Button, active ? ImVec4(.23f,.07f,.09f,1.f) : ImVec4(.09f,.095f,.105f,1.f));
                if(TacticalUI::Button(label,ImVec2(122.f*scale,30.f*scale))) tactical_tab=index;
                ImGui::PopStyleColor();
                if(tactical_tab==index) {
                    const ImVec2 lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();
                    ImGui::GetWindowDrawList()->AddLine(ImVec2(lo.x,hi.y),ImVec2(hi.x,hi.y),ImGui::GetColorU32(ImGuiCol_CheckMark),2.f);
                }
                ImGui::PopID();
            };
            tab(tactical_page==3?"Misiles":tactical_page==4?"Prediccion":"General",0);
            if(menu_preferences::current.advanced) { ImGui::SameLine();tab(tactical_page==3?"Diagnostico":tactical_page==4?"Ajustes lead":"Avanzado",1); }
            ImGui::PopID();
        }
        ImGui::Spacing();
        TacticalUI::Checkbox("Solo favoritos",&tactical_favorites_only);
        const bool displaying_search=tactical_search[0]!=0 || tactical_favorites_only;
        if(displaying_search) {
            tactical_reveal_label.clear(); tactical_reveal_header.clear();
            ImGui::BeginChild("##search_results",ImVec2(0,0),false);
            int results=0;
            for(const auto& group:tactical_index) {
                const char* start=group.labels;
                while(*start) {
                    const char* end=std::strchr(start,'|');
                    const std::string label(start,end ? end-start : std::strlen(start));
                    if((filter.PassFilter(label.c_str()) || filter.PassFilter(TacticalUI::LocalText(label.c_str()).c_str())) &&
                        (!tactical_favorites_only || menu_preferences::current.favorites.contains(label))) {
                        ImGui::PushID(results++);
                        std::string display=TacticalUI::LocalText(label.c_str())+"   /   "+TacticalUI::LocalText(pages[group.page]);
                        if(group.header[0]) { display+=" - "; display+=TacticalUI::LocalText(group.header); }
                        if(TacticalUI::Selectable((display+"###search_result").c_str(),false,0,ImVec2(0,30.f*scale))) {
                            tactical_favorites_only=false;
                            if(group.tab==1 || group.header[0])menu_preferences::current.advanced=true;
                            tactical_page=group.page;
                            tactical_tab=group.tab;
                            tactical_reveal_label=label;
                            tactical_reveal_header=group.header;
                            tactical_search[0]=0;
                        }
                        if(ImGui::BeginPopupContextItem("##search_favorite")) {
                            if(TacticalUI::MenuItem(menu_preferences::current.favorites.contains(label)?"Quitar de favoritos":"Anadir a favoritos"))menu_preferences::toggle_favorite(label);
                            ImGui::EndPopup();
                        }
                        ImGui::PopID();
                    }
                    if(!end) break;
                    start=end+1;
                }
            }
            if(results==0) TacticalUI::TextDisabled("No se encontraron controles. Prueba otro término.");
            ImGui::EndChild();
        }
        if(!displaying_search) {
        if((!tactical_search[0] && tactical_page==0) || (tactical_search[0] && filter.PassFilter("General Enable ESP Show FPS Radar Air Lead Misiles"))) {
            ImGui::PushID(0);
            if (tactical_search[0]) TacticalUI::SeparatorText("Panel de control");
            if (tactical_tab==0 || tactical_search[0]) {
                TacticalColumnsBegin(scale);
                TacticalSection("SISTEMA / OVERLAY");
                TacticalToggle("Enable ESP", &sdk::esp_enabled);
                TacticalToggle("Show FPS", &sdk::esp_show_fps);
                TacticalNextColumn();
                TacticalSection("ACCESOS RAPIDOS");
                TacticalToggle("Radar", &sdk::radar_enabled);
                TacticalToggle("Air Lead", &sdk::air_lead_enabled);
                TacticalToggle("Misiles", &sdk::projectile_esp_enabled);
                TacticalColumnsEnd();
            }
            if (tactical_tab==1 && !tactical_search[0]) {
                TacticalColumnsBegin(scale);
                TacticalSection("RENDIMIENTO / MS");
                const double frame_ms=misc::prof_main_frame_us.load()/1000.0;
                TacticalUI::Text("Frame %.2f ms  |  %.0f FPS",frame_ms,frame_ms>0.0?1000.0/frame_ms:0.0);
                TacticalUI::Text("Frame p95 %.2f ms | max %.2f ms",misc::prof_frame_p95_us.load()/1000.0,misc::prof_frame_peak_us.load()/1000.0);
                TacticalUI::Text("Snapshot %.2f / ESP %.2f",misc::prof_update_frame_us.load()/1000.0,misc::prof_esp_us.load()/1000.0);
                TacticalUI::Text("ESP radar %.2f | unidades %.2f | X-Ray %.2f",
                    misc::prof_esp_radar_us.load()/1000.0,misc::prof_esp_units_us.load()/1000.0,
                    misc::prof_esp_xray_us.load()/1000.0);
                TacticalUI::Text("ESP recarga %.2f | INVUL %.2f",
                    misc::prof_esp_reload_us.load()/1000.0,misc::prof_esp_invul_us.load()/1000.0);
                TacticalUI::Text("Aimbot %.2f / Projectile %.2f",misc::prof_aimbot_us.load()/1000.0,misc::prof_projectile_us.load()/1000.0);
                const auto frame_quality=quality.frame();
                const auto esp_quality=quality.esp();
                TacticalUI::Text("Ventana %zu frames: frame p95 %.2f / p99 %.2f ms | ESP p95 %.2f / p99 %.2f ms",
                    frame_quality.samples,frame_quality.p95_ms,frame_quality.p99_ms,esp_quality.p95_ms,esp_quality.p99_ms);
                if(frame_quality.rejected || esp_quality.rejected)
                    TacticalUI::TextDisabled("Duraciones no validas descartadas: %llu",static_cast<unsigned long long>(frame_quality.rejected+esp_quality.rejected));
                TacticalUI::TextDisabled("p95 y max: ventana de 120 frames; percentiles p95/p99: ventana independiente de 512 muestras.");
                static uint64_t ref_frame=0,ref_peak=0,ref_age=0,ref_age_peak=0;
                static bool have_reference=false;
                ImGui::BeginDisabled(!misc::prof_frame_p95_us.load());
                if(TacticalUI::SmallButton("Fijar referencia (antes)")) {
                    ref_frame=misc::prof_frame_p95_us.load();ref_peak=misc::prof_frame_peak_us.load();
                    ref_age=misc::prof_snapshot_age_p95_us.load();ref_age_peak=misc::prof_snapshot_age_peak_us.load();
                    have_reference=true;
                }
                ImGui::EndDisabled();
                TacticalSearchFocus("Fijar referencia (antes)");
                if(have_reference) {
                    const double dframe=(static_cast<int64_t>(misc::prof_frame_p95_us.load())-static_cast<int64_t>(ref_frame))/1000.0;
                    const double dage=(static_cast<int64_t>(misc::prof_snapshot_age_p95_us.load())-static_cast<int64_t>(ref_age))/1000.0;
                    TacticalUI::Text("Después vs antes: frame %+0.2f ms, edad %+0.2f ms",dframe,dage);
                    TacticalUI::Text("Picos: frame %+0.2f ms, edad %+0.2f ms",
                        (static_cast<int64_t>(misc::prof_frame_peak_us.load())-static_cast<int64_t>(ref_peak))/1000.0,
                        (static_cast<int64_t>(misc::prof_snapshot_age_peak_us.load())-static_cast<int64_t>(ref_age_peak))/1000.0);
                    if(dframe < -1.0 && dage > 5.0)
                        TacticalUI::TextColored(ImVec4(1.f,.42f,.28f,1.f),"ALERTA: más FPS, pero datos más viejos.");
                }
                TacticalNextColumn();
                TacticalSection("DIAGNOSTICO");
                TacticalUI::Text("Edad snapshot %.2f ms",misc::prof_snapshot_age_us.load()/1000.0);
                TacticalUI::Text("Edad p95 %.2f ms | max %.2f ms",misc::prof_snapshot_age_p95_us.load()/1000.0,misc::prof_snapshot_age_peak_us.load()/1000.0);
                TacticalUI::Text("Update p95 %.2f ms",misc::prof_update_frame_p95_us.load()/1000.0);
                if(!misc::prof_snapshot_valid.load()) TacticalUI::TextColored(ImVec4(.95f,.36f,.28f,1.f),"Snapshot sin datos validos.");
                else if(misc::prof_snapshot_age_us.load()>200000) TacticalUI::TextColored(ImVec4(.95f,.36f,.28f,1.f),"Snapshot atrasado: revisar intervalos o conexion.");
                TacticalUI::Text("Proyecciones %llu / OBB %llu",static_cast<unsigned long long>(misc::prof_esp_w2s_calls.load()),static_cast<unsigned long long>(misc::prof_esp_obb_calls.load()));
                TacticalUI::Text("Recarga: candidatos %llu | barras %llu | sin dato %llu | valor fuera 0..16 %llu",
                    static_cast<unsigned long long>(misc::prof_reload_candidates.load()),
                    static_cast<unsigned long long>(misc::prof_reload_drawn.load()),
                    static_cast<unsigned long long>(misc::prof_reload_failed.load()),
                    static_cast<unsigned long long>(misc::prof_reload_unexpected.load()));
                if(misc::prof_reload_sample.load()>=0)
                    TacticalUI::Text("Muestra VisualReload (primer objetivo): %d",misc::prof_reload_sample.load());
                TacticalUI::Text("Invulnerabilidad: candidatos %llu | marcados %llu | sin lectura %llu | flag invalido %llu | timer %llu",
                    static_cast<unsigned long long>(misc::prof_invul_candidates.load()),
                    static_cast<unsigned long long>(misc::prof_invul_shown.load()),
                    static_cast<unsigned long long>(misc::prof_invul_failed.load()),
                    static_cast<unsigned long long>(misc::prof_invul_invalid.load()),
                    static_cast<unsigned long long>(misc::prof_invul_timer_shown.load()));
                TacticalUI::Text("Dummies del juego excluidos (ESP/radar): %llu",
                    static_cast<unsigned long long>(misc::known_synthetic_filtered.load()));
                TacticalUI::Text("ESP descartes: distancia %llu | tipo %llu",
                    static_cast<unsigned long long>(misc::prof_esp_cull_distance.load()),
                    static_cast<unsigned long long>(misc::prof_esp_cull_type.load()));
                TacticalUI::Text("Fuera de vista %llu | proyeccion %llu | OBB parcial %llu",
                    static_cast<unsigned long long>(misc::prof_esp_cull_frustum.load()),
                    static_cast<unsigned long long>(misc::prof_esp_cull_projection.load()),
                    static_cast<unsigned long long>(misc::prof_esp_partial_boxes.load()));
                if (ImGui::IsItemHovered()) TacticalUI::SetTooltip("OBB parcial: caja 3D incompleta, etiqueta conservada. No es un vischeck de obstaculos.");
                TacticalUI::Text("DMA camara: directo OK %llu / sin lectura %llu",
                    static_cast<unsigned long long>(scatter_read_recovery::camera_fallback_ok.load()),
                    static_cast<unsigned long long>(scatter_read_recovery::camera_unavailable.load()));
                TacticalUI::Text("DMA unidades: scatter fallo %llu / directo OK %llu / sin lectura %llu",
                    static_cast<unsigned long long>(scatter_read_recovery::units_scatter_fail.load()),
                    static_cast<unsigned long long>(scatter_read_recovery::units_fallback_ok.load()),
                    static_cast<unsigned long long>(scatter_read_recovery::units_unavailable.load()));
                TacticalUI::Text("Presupuesto misiles %d reads",sdk::projectile_scan_budget);
                TacticalColumnsEnd();
            }
            ImGui::PopID();
        }

        if((!tactical_search[0] && tactical_page==1) || (tactical_search[0] && filter.PassFilter("Visuales Only Show Planes Show Bomb Impact Render Distance Box Distance 2D Box 3D Box Vehicle Names Distance Reload Progress Invulnerable InvulTimer Invul Size Invul Distance Seconds Name Size Distance Size Reload Size Module Name Size Death State Threshold Helicopter Bomber Fighter Attacker Recon Drone Light Tank Heavy Tank Medium Tank SPAA Gun Boat Destroyer Cruiser Torpedo Boat"))) {
 ImGui::PushID(1); if (tactical_search[0]) TacticalUI::SeparatorText("Visuales");
 if (tactical_tab==0 || tactical_search[0]) { TacticalColumnsBegin(scale); TacticalSection("Visualización de vehículos");
                            TacticalToggle("Only Show Planes", &sdk::esp_only_show_planes);
                            if (ImGui::IsItemHovered())
                                TacticalUI::SetTooltip("Oculta todo lo que no sea aereo: terrestre, naval y estructuras.");
                            TacticalToggle("Filtrar aeronaves estaticas", &menu_preferences::current.filter_static_air);
                            if(ImGui::IsItemHovered())TacticalUI::SetTooltip("Oculta aeronaves tras 2 segundos dentro de 3 cm de posicion. Puede ocultar aeronaves estacionadas o en vuelo estacionario muy estable. Se muestran al moverse. Sin lecturas DMA adicionales; guardado manual por perfil.");
                            TacticalToggle("Show Bomb Impact", &sdk::esp_show_bomb_impact);
                            TacticalSliderFloat("Render Distance", &sdk::esp_render_distance, 1000.f, 55000.f, "%.0f m");
                            TacticalSliderFloat("Box Distance", &sdk::esp_box_distance, 500.f, 3500.f, "%.0f m");

                            TacticalUI::SeparatorText("Boxes");
                            TacticalToggle("2D Box", &sdk::esp_show_boxes);
                            
                            TacticalToggle("3D Box", &sdk::esp_use_3d_boxes);

                            TacticalNextColumn(); TacticalSection("Etiquetas y texto");
                            TacticalToggle("Vehicle Names", &sdk::esp_show_vehicle_names);
                            
                            TacticalToggle("Distance", &sdk::esp_show_distance);
                            TacticalUI::SeparatorText("Invulnerabilidad: vehiculos terrestres");
                            TacticalToggle("Mostrar INVUL", &sdk::esp_show_invulnerable);
                            if(ImGui::IsItemHovered()) TacticalUI::SetTooltip("Todos los vehiculos terrestres validos; no aviones, barcos, estructuras ni entidades dummy.");
                            if(sdk::esp_show_invulnerable && !offsets::api_invulnerable_ready)
                                TacticalUI::TextColored(ImVec4(.95f,.4f,.28f,1.f),"API sin Unit.m_Invulnerable: indicador desactivado.");
                            ImGui::BeginDisabled(!sdk::esp_show_invulnerable);
                            TacticalToggle("Mostrar valor InvulTimer", &sdk::esp_invul_show_timer);
                            if(ImGui::IsItemHovered()) TacticalUI::SetTooltip("Unit.m_InvulTimer: valor API bruto, no segundos confirmados.\nNunca determina por si solo si el vehiculo es invulnerable.");
                            TacticalSliderFloat("Distancia INVUL", &sdk::esp_invul_distance, 100.f, 3500.f, "%.0f m");
                            TacticalSliderFloat("Tamaño INVUL", &sdk::esp_font_size_invul, 8.f, 18.f, "%.0f");
                            TacticalColorEdit("Color INVUL", (float*)&sdk::color_invulnerable);
                            ImGui::EndDisabled();
                            TacticalToggle("Reload Progress", &sdk::esp_show_reload);
                            if(sdk::esp_show_reload && !offsets::api_reload_ready)
                                TacticalUI::TextColored(ImVec4(.95f,.4f,.28f,1.f),"API sin Unit.m_VisualReload: usando offset compilado anterior; revisar logs.");
                            ImGui::BeginDisabled(!sdk::esp_show_reload);
                            TacticalToggle("Seconds", &sdk::esp_show_reload_seconds);
                            if(ImGui::IsItemHovered()) TacticalUI::SetTooltip("Estimacion anterior: visual * 10/16 s.\nNo es una duracion confirmada por arma ni un nuevo offset.");
                            ImGui::EndDisabled();

                            TacticalUI::SeparatorText("Font Sizes");
                            TacticalSliderFloat("Name Size", &sdk::esp_font_size_name, 8.0f, 36.0f, "%.0f");
                            TacticalSliderFloat("Distance Size", &sdk::esp_font_size_distance, 8.0f, 36.0f, "%.0f");
                            TacticalSliderFloat("Reload Size", &sdk::esp_font_size_reload, 8.0f, 36.0f, "%.0f");
                            TacticalSliderFloat("Module Name Size", &sdk::esp_font_size_module, 8.0f, 36.0f, "%.0f");
TacticalPreview(); TacticalColumnsEnd(); }
 if (tactical_tab==1 || tactical_search[0]) { TacticalColumnsBegin(scale,"##vehicle_advanced"); TacticalSection("Visibilidad por categoría");
if(!tactical_search[0]) ImGui::SetNextItemOpen(true,ImGuiCond_Once);
if(TacticalAdvanced("Mostrar tipos de vehículo")){
                            const auto draw_vehicle_row = [](const char* label, bool* enabled)
                                {
                                    ImGui::PushID(label);
                                    TacticalToggle(label, enabled);
                                    ImGui::PopID();
                                };
                            TacticalUI::SeparatorText("Aéreos");
                            draw_vehicle_row("Helicopter", &sdk::show_helicopter);
                            draw_vehicle_row("Bomber", &sdk::show_bomber);
                            draw_vehicle_row("Fighter", &sdk::show_fighter);
                            draw_vehicle_row("Attacker", &sdk::show_attacker);
                            draw_vehicle_row("Recon Drone", &sdk::show_recon_drone);
                            TacticalUI::SeparatorText("Terrestres");
                            draw_vehicle_row("Light Tank", &sdk::show_light_tank);
                            draw_vehicle_row("Heavy Tank", &sdk::show_heavy_tank);
                            draw_vehicle_row("Medium Tank", &sdk::show_medium_tank);
                            draw_vehicle_row("SPAA", &sdk::show_spaa);
                            TacticalUI::SeparatorText("Navales");
                            draw_vehicle_row("Gun Boat", &sdk::show_gun_boat);
                            draw_vehicle_row("Destroyer", &sdk::show_destroyer);
                            draw_vehicle_row("Cruiser", &sdk::show_cruiser);
                            draw_vehicle_row("Torpedo Boat", &sdk::show_torpedo_boat);

                            TacticalUI::SeparatorText("Estado de vehículo");
                            TacticalSliderInt("Death State Threshold", &sdk::death_state_threshold, 1, 2);}
TacticalNextColumn(); TacticalSection("Colores por tipo");
if(!tactical_search[0]) ImGui::SetNextItemOpen(false,ImGuiCond_Once);
if(TacticalAdvanced("Colores de vehiculos")){                            TacticalColorEdit("Helicopter", (float*)&sdk::color_helicopter);
                            TacticalColorEdit("Bomber", (float*)&sdk::color_bomber);
                            TacticalColorEdit("Fighter", (float*)&sdk::color_fighter);
                            TacticalColorEdit("Attacker", (float*)&sdk::color_attacker);
                            TacticalColorEdit("Recon Drone", (float*)&sdk::color_recon_drone);                            TacticalColorEdit("Light Tank", (float*)&sdk::color_light_tank);
                            TacticalColorEdit("Heavy Tank", (float*)&sdk::color_heavy_tank);
                            TacticalColorEdit("Medium Tank", (float*)&sdk::color_medium_tank);
                            TacticalColorEdit("SPAA", (float*)&sdk::color_spaa);                            TacticalColorEdit("Gun Boat", (float*)&sdk::color_gun_boat);
                            TacticalColorEdit("Destroyer", (float*)&sdk::color_destroyer);
                            TacticalColorEdit("Cruiser", (float*)&sdk::color_cruiser);
                            TacticalColorEdit("Torpedo Boat", (float*)&sdk::color_torpedo_boat);}
TacticalColumnsEnd(); }
 ImGui::PopID();
 }

        if((!tactical_search[0] && tactical_page==2) || (tactical_search[0] && filter.PassFilter("X-Ray Enable X-Ray Module Names Wireframe Gun Barrel Cannon Breech Crew Fuel Tank Ammo Rack Engine Transmission Hide small modules > Line Thickness Module Alpha X-Ray Distance Names Distance Gun Barrel Cannon Breech Crew Fuel Tank Ammo Rack Engine Transmission"))) {
 ImGui::PushID(2); if (tactical_search[0]) TacticalUI::SeparatorText("X-Ray");
 if (tactical_tab==0 || tactical_search[0]) { TacticalColumnsBegin(scale); TacticalSection("Modelo de daño");
                            TacticalToggle("Enable X-Ray", &sdk::bShowDamageModel);
                            ImGui::BeginDisabled(!sdk::bShowDamageModel || !sdk::esp_enabled);
                            TacticalToggle("Module Names", &sdk::bShowModuleNames);
                            TacticalUI::TextDisabled("Modo: Wireframe (unico)");
                            TacticalUI::TextWrapped("Solo Ammo se simplifica a cajas de contorno; el resto conserva sus lineas originales.");

                            TacticalNextColumn(); TacticalSection("Módulos visibles");
                            TacticalToggle("Gun Barrel", &sdk::bShowModuleGunBarrel); 
                            TacticalToggle("Cannon Breech", &sdk::bShowModuleCannonBreech);
                            TacticalToggle("Crew", &sdk::bShowModuleCrew); 
                            TacticalToggle("Fuel Tank", &sdk::bShowModuleFuelTank);
                            TacticalToggle("Ammo Rack", &sdk::bShowModuleAmmoRack); 
                            TacticalToggle("Engine", &sdk::bShowModuleEngine);
                            TacticalToggle("Transmission", &sdk::bShowModuleTransmission);

                            TacticalSliderFloat("Hide small modules >", &sdk::xray_small_module_distance, 50.0f, 2000.0f, "%.0f m");
                            TacticalSliderFloat("Line Thickness", &sdk::xray_line_thickness, 0.2f, 3.0f, "%.1f");
                            TacticalSliderFloat("Module Alpha", &sdk::xray_module_alpha, 0.0f, 1.0f, "%.2f");
                            {
                                static int xd = (int)sdk::xray_render_distance, nd = (int)sdk::xray_names_distance;
                                xd = (int)sdk::xray_render_distance; nd = (int)sdk::xray_names_distance;
                                TacticalSliderInt("X-Ray Distance", &xd, 100, 5000, "%d m"); sdk::xray_render_distance = (float)xd;
                                TacticalSliderInt("Names Distance", &nd, 100, 5000, "%d m"); sdk::xray_names_distance = (float)nd;
                            }
                            ImGui::EndDisabled();
TacticalColumnsEnd(); }
if (tactical_tab==1 || tactical_search[0]) { TacticalColumnsBegin(scale,"##module_colors"); TacticalSection("Colores de módulos");
if(!tactical_search[0]) ImGui::SetNextItemOpen(true,ImGuiCond_Once);
if(TacticalAdvanced("Colores de modulos")){
                            ImGui::BeginDisabled(!sdk::bShowDamageModel || !sdk::esp_enabled);
                            TacticalColorEdit("Gun Barrel", (float*)&sdk::color_mod_gun_barrel);
                            TacticalColorEdit("Cannon Breech", (float*)&sdk::color_mod_cannon_breech);
                            TacticalColorEdit("Crew", (float*)&sdk::color_mod_crew);
                            TacticalNextColumn(); TacticalSection("ESTADO Y PROPULSION");
                            TacticalColorEdit("Fuel Tank", (float*)&sdk::color_mod_fuel_tank);
                            TacticalColorEdit("Ammo Rack", (float*)&sdk::color_mod_ammo_rack);
                            TacticalColorEdit("Engine", (float*)&sdk::color_mod_engine);
                            TacticalColorEdit("Transmission", (float*)&sdk::color_mod_transmission);
                            ImGui::EndDisabled();}
TacticalColumnsEnd(); } ImGui::PopID();
 }

        if((!tactical_search[0] && tactical_page==3) || (tactical_search[0] && filter.PassFilter("Misiles Live Projectile ESP (experimental ECS) Projectile Debug Missile Scan Budget Only Air-to-Air Missiles (AAM) Only Incoming Missiles Use Confirmed Local Player Incoming Distance Incoming Angle"))) {
 ImGui::PushID(3); if (tactical_search[0]) TacticalUI::SeparatorText("Misiles");
 if (tactical_tab==0 || tactical_search[0]) { TacticalColumnsBegin(scale); TacticalSection("Detección de misiles");
                            TacticalToggle("Live Projectile ESP (experimental ECS)", &sdk::projectile_esp_enabled);
                            ImGui::BeginDisabled(!sdk::projectile_esp_enabled);
                            TacticalToggle("AAM por defecto para nombres nuevos", &sdk::projectile_aam_only);
                            if (ImGui::IsItemHovered())
                                TacticalUI::SetTooltip("Marca automaticamente nombres AAM aun sin seleccion manual.\nTus casillas por nombre prevalecen; bombas y bengalas se excluyen.\nLos nombres nuevos aparecen en la lista al detectarlos.");
                            TacticalNextColumn(); TacticalSection("Avisos entrantes");
                            TacticalToggle("Only Incoming Missiles", &sdk::projectile_incoming_only);
                            if (ImGui::IsItemHovered())
                                TacticalUI::SetTooltip("Solo dibuja misiles que vienen hacia ti (cierre + cono).\nLas bengalas se descartan siempre por nombre.");
                            ImGui::BeginDisabled(!sdk::projectile_incoming_only);
                            {
                                TacticalToggle("Use Confirmed Local Player", &sdk::projectile_incoming_use_localplayer);
                                if (ImGui::IsItemHovered())
                                    TacticalUI::SetTooltip("Usa el jugador local confirmado del snapshot (el mismo del radar/aimbot).\nSi no esta disponible, cae a la lectura directa.");
                                TacticalSliderFloat("Incoming Distance", &sdk::projectile_incoming_distance, 500.0f, 8000.0f, "%.0f m");
                                TacticalSliderFloat("Incoming Angle", &sdk::projectile_incoming_angle, 5.0f, 60.0f, "%.0f deg");
                            }
                            ImGui::EndDisabled();
                            ImGui::EndDisabled();
TacticalColumnsEnd();
                            TacticalSection("ALERTAS DE APROXIMACION");
                            auto alert_cfg=missile_alerts::capture_settings();
                            const auto previous_alert_cfg=alert_cfg;
                            TacticalToggle("Alerta visual de aproximacion",&alert_cfg.enabled);
                            ImGui::BeginDisabled(!alert_cfg.enabled);
                            TacticalToggle("Sonido de alerta",&alert_cfg.audio_enabled);
                            if(menu_preferences::current.advanced) {
                                const auto slider=[&](const char* name,double& value,float lo,float hi,const char* unit){
                                    float v=static_cast<float>(value);if(TacticalSliderFloat(name,&v,lo,hi,unit))value=v;
                                };
                                slider("Radio de alerta",alert_cfg.max_range_m,100.f,8000.f,"%.0f m");
                                slider("Cierre minimo",alert_cfg.min_closure_mps,1.f,500.f,"%.0f m/s");
                                slider("Tiempo hasta paso mas cercano",alert_cfg.max_time_s,1.f,30.f,"%.1f s");
                                slider("Distancia de paso prevista",alert_cfg.max_miss_distance_m,10.f,1000.f,"%.0f m");
                                slider("Intervalo minimo de sonido",alert_cfg.audio_interval_s,1.f,30.f,"%.1f s");
                            }
                            ImGui::EndDisabled();
                            if(!same_alerts(previous_alert_cfg,alert_cfg))missile_alerts::apply_settings(alert_cfg);
                            TacticalUI::TextWrapped("Aproximacion geometrica: no confirma seguimiento ni bloqueo. Solo misiles incluidos por tus filtros. El sonido usa el aviso del sistema, limitado por intervalo.");
                            if(!sdk::projectile_esp_enabled)TacticalUI::TextDisabled("Alertas sin datos: activa Mostrar misiles.");
                            const auto threat=projectile_esp::approach_readout();
                            if(threat.approaching)TacticalUI::Text("Cierre %.0f m/s | paso en %.1f s | separacion %.0f m",threat.closure_mps,threat.time_to_closest_s,threat.miss_distance_m);
                            TacticalSection("CATALOGO DE MISILES");
                            if(TacticalUI::Button("Seleccionar todos los misiles")) {
                                sdk::projectile_aam_only=false;
                                projectile_visibility::apply_category_policy({});
                                projectile_visibility::set_all(true);
                            }
                            TacticalSearchFocus("Seleccionar todos los misiles");
                            if(ImGui::IsItemHovered()) TacticalUI::SetTooltip("Incluye nombres actuales y futuros. No cambia el filtro de entrantes ni activa el ESP. Guardado manual.");
                            TacticalUI::TextWrapped("Las selecciones se guardan manualmente. Bombas, bengalas y nombres con mm siguen excluidos.");
                            static ImGuiTextFilter missile_name_search;
                            missile_name_search.Draw(menu_preferences::widget_label("Buscar nombre##missile_filter").c_str(),260.f*scale);
                            static int missile_category=0, listed_category=-1;
                            const char* category_names[]={"Todos","Misiles Aire-Aire","Misiles Aire-Tierra","Misiles Tierra-Aire","Sin clasificar / multiuso"};
                            ImGui::SetNextItemWidth(std::min(320.f*scale,ImGui::GetContentRegionAvail().x));
                            for(int i=1;i<4;++i) if(tactical_reveal_label==category_names[i]) missile_category=i;
                            TacticalUI::Combo("Categoria",&missile_category,category_names,IM_ARRAYSIZE(category_names));
                            if(missile_category>0) TacticalSearchFocus(category_names[missile_category]);
                            TacticalUI::TextWrapped("Categoria orientativa. Clic derecho sobre un nombre para corregirla manualmente.");
                            if(missile_category>0) {
                                const auto category=static_cast<projectile_visibility::category>(missile_category-1);
                                auto policies=projectile_visibility::capture_category_policy();
                                int policy=static_cast<int>(policies[missile_category-1]);
                                const char* labels[]={"Valor AAM general","Mostrar nuevos","Ocultar nuevos"};
                                ImGui::SetNextItemWidth(230.f*scale);
                                if(TacticalUI::Combo("Nombres futuros",&policy,labels,3))projectile_visibility::set_category_policy(category,
                                    static_cast<projectile_visibility::category_policy>(policy));
                                if(TacticalUI::SmallButton("Activar categoria actual y futura")) {
                                    projectile_visibility::set_category_policy(category,projectile_visibility::category_policy::show);
                                    projectile_visibility::set_current_category_visibility(category,true);
                                }
                                if(TacticalUI::SmallButton("Ocultar categoria actual y futura")) {
                                    projectile_visibility::set_category_policy(category,projectile_visibility::category_policy::hide);
                                    projectile_visibility::set_current_category_visibility(category,false);
                                }
                                TacticalUI::TextWrapped("Las casillas por nombre prevalecen. Los botones de categoria no dependen de la busqueda.");
                            }
                            static std::vector<int> missile_categories;
                            static std::vector<std::pair<std::string,bool>> missile_names;
                            static std::vector<std::size_t> matching_names;
                            static std::string last_missile_search;
                            static std::uint64_t listed_revision=~std::uint64_t{0};
                            static bool listed_aam=false;
                            static std::uint64_t listed_category_revision=~std::uint64_t{0};
                            const auto category_revision=projectile_visibility::category_revision.load(std::memory_order_relaxed);
                            const auto revision=projectile_visibility::revision.load(std::memory_order_relaxed);
                            const bool rebuild=revision!=listed_revision || listed_aam!=sdk::projectile_aam_only || category_revision!=listed_category_revision;
                            if (rebuild) {
                                missile_names=projectile_visibility::entries(sdk::projectile_aam_only);
                                missile_categories.clear();
                                for(const auto& entry:missile_names)
                                    missile_categories.push_back(1+static_cast<int>(projectile_visibility::classify(entry.first)));
                                listed_revision=revision;
                                listed_aam=sdk::projectile_aam_only;
                                listed_category_revision=category_revision;
                            }
                            const std::string search_text=missile_name_search.InputBuf;
                            if (rebuild || search_text!=last_missile_search || listed_category!=missile_category) {
                                matching_names.clear();
                                for (std::size_t i=0;i<missile_names.size();++i)
                                    if ((missile_category==0 || missile_categories[i]==missile_category) &&
                                        missile_name_search.PassFilter(missile_names[i].first.c_str()))
                                        matching_names.push_back(i);
                                last_missile_search=search_text;
                                listed_category=missile_category;
                            }
                            ImGui::BeginDisabled(matching_names.empty());
                            const bool select_filtered=TacticalUI::SmallButton("Marcar resultados");
                            ImGui::SameLine();
                            const bool clear_filtered=TacticalUI::SmallButton("Desmarcar resultados");
                            if(select_filtered || clear_filtered) for(const auto i:matching_names) {
                                projectile_visibility::set(missile_names[i].first,select_filtered);
                                missile_names[i].second=select_filtered;
                            }
                            ImGui::EndDisabled();
                            std::size_t enabled_count=0;
                            for(const auto& entry:missile_names) if(entry.second) ++enabled_count;
                            TacticalUI::Text("Activos: %zu / %zu  |  Resultados: %zu",enabled_count,missile_names.size(),matching_names.size());
                            TacticalUI::TextWrapped("Marcar/desmarcar afecta solo a los resultados de categoria y busqueda. Entrantes se filtra despues.");
                            if(TacticalAdvanced("Registro de nombres")) {
                                TacticalUI::TextWrapped("Los nombres nuevos se registran automaticamente en missiles/registry.txt junto al ejecutable.");
                                TacticalUI::TextWrapped("%s",missile_registry_storage::status.c_str());
                            }
                            if (projectile_visibility::registry_capacity_reached())
                                TacticalUI::TextColored(ImVec4(1.f,.35f,.2f,1.f),"Registro lleno: no se anaden mas nombres.");
                            if (matching_names.empty()) TacticalUI::TextWrapped("Sin resultados. Los nombres aparecen automaticamente al detectarse en batalla.");
                            if (ImGui::BeginChild("##missile_names",ImVec2(0,240.f*scale),true)) {
                                ImGuiListClipper clipper;
                                clipper.Begin(static_cast<int>(matching_names.size()),ImGui::GetFrameHeightWithSpacing());
                                while (clipper.Step()) for (int row=clipper.DisplayStart;row<clipper.DisplayEnd;++row) {
                                    const auto& [name, selected]=missile_names[matching_names[row]];
                                    bool checked=selected;
                                    ImGui::PushID(name.c_str());
                                    if (TacticalUI::Checkbox("##show",&checked)) projectile_visibility::set(name,checked);
                                    ImGui::SameLine(); ImGui::TextUnformatted(name.c_str()); // Game missile name is data, never a catalog key.
                                    if(ImGui::IsItemHovered()) TacticalUI::SetTooltip("%s | Clic derecho: cambiar categoria",menu_preferences::label(category_names[missile_categories[matching_names[row]]]));
                                    if(ImGui::BeginPopupContextItem("##category")) {
                                        for(int i=0;i<4;++i)if(TacticalUI::MenuItem(category_names[i+1]))
                                            projectile_visibility::set_category_override(name,static_cast<projectile_visibility::category>(i));
                                        if(TacticalUI::MenuItem("Restaurar clasificacion automatica"))projectile_visibility::clear_category_override(name);
                                        ImGui::EndPopup();
                                    }
                                    ImGui::PopID();
                                }
                            }
                            ImGui::EndChild(); }
 if (tactical_tab==1 || tactical_search[0]) { TacticalColumnsBegin(scale,"##missile_diagnostics"); TacticalSection("Diagnóstico");
 ImGui::BeginDisabled(!sdk::projectile_esp_enabled);
 TacticalToggle("Projectile Debug", &sdk::projectile_debug);
 TacticalSliderInt("Límite por lista ECS", &sdk::projectile_scan_budget, 256, 4096, "%d");
 ImGui::EndDisabled();
 TacticalUI::Text("Presupuesto por lista %d | lecturas totales %llu",misc::prof_projectile_adaptive_budget.load(),static_cast<unsigned long long>(misc::prof_projectile_reads.load()));
 TacticalUI::Text("Escaneo %.2f ms",misc::prof_projectile_scan_us.load()/1000.0);
 TacticalNextColumn(); TacticalSection("ESTADO DEL FILTRO");
 TacticalUI::TextWrapped("AAM y misiles entrantes usan los filtros configurados en General.");
 TacticalUI::TextWrapped("Debug expone nombres descartados y datos de la lista ECS.");
 TacticalColumnsEnd(); } ImGui::PopID();
 }

        if((!tactical_search[0] && tactical_page==4) || (tactical_search[0] && filter.PassFilter("Aimbot Prediccion G2G Air Lead Use Simulation Turn Prediction Adaptive Smoothing Only Target Marker Size Line Thickness Opacidad Compatibilidad alfa aim lead Helicopters Drones Fighters Bombers Attackers"))) {
            ImGui::PushID(4);
            if (tactical_search[0]) TacticalUI::SeparatorText("Combat / Aimbot");
            if (tactical_tab==0 || tactical_search[0]) {
                TacticalColumnsBegin(scale);
                TacticalSection("Predicción aérea");
                TacticalToggle("Enable Air Lead", &sdk::air_lead_enabled);
                if(ImGui::IsItemHovered()) TacticalUI::SetTooltip("Aire-Aire: canon del avion contra objetivos aereos.");
                TacticalToggle("Aim Tierra-Aire (G2A)", &esp::g2a_lead::enabled);
                TacticalUI::TextWrapped("G2A calidad: %s",esp::g2a_lead::quality);
                TacticalUI::Text("G2A propio: %s | objetivo: %s | giro: %s",esp::g2a_lead::own_source,esp::g2a_lead::target_source,esp::g2a_lead::acceleration_source);
                TacticalUI::TextWrapped("G2A municion: %s",esp::g2a_lead::ammunition_status);
                if(esp::g2a_lead::sample_age_ms>=0)TacticalUI::Text("G2A muestra: %.0f ms",esp::g2a_lead::sample_age_ms);
                TacticalKeybind("Atajo G2A",menu_preferences::current.g2a_key);
                if(!sdk::esp_enabled)TacticalUI::TextWrapped("Predictores deshabilitados: activa ESP para dibujar las marcas.");
                if(sdk::air_lead_enabled && sdk::air_lead_use_turn_prediction)TacticalUI::TextDisabled("Prediccion A2A: giros 3D activos; residuo del modelo no es error de impacto.");
                TacticalUI::TextWrapped("G2A: %s",esp::g2a_lead::status);
                TacticalUI::TextWrapped("G2A: circulo, cruz y linea al objetivo como A2A, hasta 3500 m. Comparte color, tamano, grosor y opacidad con A2A; su activacion y calculo siguen siendo independientes de Air Lead y G2G.");
                if(esp::g2a_lead::flight_time>0) TacticalUI::Text("G2A vuelo: %.3f s | residual: %.3f m",esp::g2a_lead::flight_time,esp::g2a_lead::model_error);
                ImGui::Separator();
                TacticalUI::TextWrapped("A2A: %s",sdk::air_lead_status);
                TacticalUI::TextWrapped("Avion propio: %s | Objetivo: %s",sdk::air_lead_own_motion_status,sdk::air_lead_target_motion_status);
                TacticalUI::Text("Vuelo: %.3f s | Residual del modelo: %.3f m",sdk::air_lead_flight_time,sdk::air_lead_model_error);
                TacticalUI::TextDisabled("Residual numerico, no error de impacto medido en el juego.");
                TacticalUI::Text("Aceleracion API disponible: %s",offsets::api_air_acceleration_ready?"si":"no (derivada de muestras)");
                TacticalUI::TextWrapped("Municion A2A: %s",aimbot::aircraft_ballistics_status);
                const bool ammo_sampled=aimbot::aircraft_trace_sample.time_since_epoch().count()!=0;
                const double ammo_age=ammo_sampled?std::chrono::duration<double,std::milli>(
                    std::chrono::steady_clock::now()-aimbot::aircraft_trace_sample).count():-1.;
                TacticalUI::Text("Muestra de municion: %.0f ms | %s",ammo_age,ammunition_quality::freshness(ammo_age));
                TacticalUI::Text("Arma elegida: %d | slot: %d | ID cartucho: %d",aimbot::aircraft_weapon_trace.selected,
                    aimbot::aircraft_ammo_diag.slot,aimbot::aircraft_ammo_diag.props_id);
                if(aimbot::aircraft_ammo_diag.cartridge_read && aimbot::aircraft_ammo_diag.table_read &&
                    ammunition_quality::conflicting_speeds(aimbot::aircraft_ammo_diag.cartridge_speed,aimbot::aircraft_ammo_diag.table_speed))
                    TacticalUI::TextColored(ImVec4(1.f,.6f,.2f,1.f),"Aviso: velocidad del cartucho y tabla no coinciden (>10%% o 20 m/s).");
                if(TacticalAdvanced("Seleccion de arma A2A")) {
                    TacticalSliderInt("Indice de arma A2A (-1 auto)",&sdk::air_lead_weapon_selection,-1,20);
                    ImGui::BeginDisabled(sdk::air_lead_weapon_selection>=0);
                    TacticalToggle("Preferir mayor calibre",&sdk::air_lead_use_heaviest_weapon);
                    ImGui::EndDisabled();
                    TacticalUI::TextWrapped("Un indice explicito evita el respaldo global ambiguo. Automatico no demuestra que sea el arma disparada; verifica slot, cartucho y velocidad. No se inventan offsets de seleccion activa.");
                }
                if(aimbot::aircraft_global_fallback)
                    TacticalUI::TextDisabled("Fallback historico API: comprobar que la municion activa sea del canon A2A.");
                if(aimbot::ballisticsData.velocity>0.f)
                    TacticalUI::Text("Velocidad leida: %.1f m/s",aimbot::ballisticsData.velocity);
                if(TacticalAdvanced("Diagnostico municion A2A (sin claves ni punteros)")) {
                    const auto& d=aimbot::aircraft_ammo_diag;
                    TacticalUI::TextWrapped("Offsets aplicados en SDK: %s",runtime_language::raw(offset_auto_update::get_applied_version().c_str()));
                    TacticalUI::Text("Armory API: Unit +0x%llX | armas +0x%llX | total +0x%llX",
                        static_cast<unsigned long long>(offsets::unit_offsets::turret_offset),
                        static_cast<unsigned long long>(offsets::armory_offsets::weapons),
                        static_cast<unsigned long long>(offsets::armory_offsets::weapon_count));
                    TacticalUI::Text("Weapon local: info +0x%llX | slot +0x%llX | ammo +0x%llX",
                        static_cast<unsigned long long>(offsets::weapon_offsets::weapon_info),
                        static_cast<unsigned long long>(offsets::weapon_offsets::active_slot),
                        static_cast<unsigned long long>(offsets::weapon_info_offsets::ammo));
                    TacticalUI::Text("Tabla API: positivo +0x%llX | negativo +0x%llX",
                        static_cast<unsigned long long>(offsets::bullet_props_positive_offset),
                        static_cast<unsigned long long>(offsets::bullet_props_negative_offset));
                    TacticalUI::TextDisabled("Estructura no cubierta por API: stride 0xA0, count +0xA8, velocidad +0x20, max ID +0x60.");
                    TacticalUI::Text("Armas: %d | intentos: %d | slot: %d | cartuchos: %d",
                        d.weapon_count,d.attempts,d.slot,d.ammo_count);
                    TacticalUI::Text("ID proyectil: %d | tabla: %s | indice: %s | entrada: %s | max ID: %u",
                        d.props_id,d.table_pointer?"si":"no",d.table_id_in_range?"si":"no",
                        d.table_entry?"si":"no",d.table_max_id);
                    if(d.cartridge_read) TacticalUI::Text("Velocidad cartucho: %.1f m/s",d.cartridge_speed);
                    else TacticalUI::TextDisabled("Velocidad cartucho: lectura fallida / sin muestra");
                    if(d.table_read) TacticalUI::Text("Velocidad tabla: %.1f m/s",d.table_speed);
                    else TacticalUI::TextDisabled("Velocidad tabla: lectura fallida / no consultada");
                    TacticalUI::TextWrapped("Etapa: %s",aimbot::aircraft_ballistics_status);
                    const bool sampled=aimbot::aircraft_trace_sample.time_since_epoch().count()!=0;
                    const auto age_ms=sampled ? std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now()-aimbot::aircraft_trace_sample).count() : -1;
                    TacticalUI::Text("Edad de muestra: %lld ms (-1 = sin muestra)",static_cast<long long>(age_ms));
                    if(age_ms>1000) TacticalUI::TextWrapped("Muestra antigua: no representa una lectura actual.");
                    if(TacticalUI::Button("Copiar diagnostico de todas las armas")) {
                        std::ostringstream report;
                        // Localize only presentation; version strings and numerical diagnostics remain verbatim.
                        report<<app_version::release_name<<" | "<<offset_auto_update::get_applied_version()
                            <<TacticalUI::Format(" | edad %lld ms\n",static_cast<long long>(age_ms))
                            <<TacticalUI::Format("A2A: %s\nMunicion: %s\n",sdk::air_lead_status,aimbot::aircraft_ballistics_status)
                            <<TacticalUI::Format("Modo: %s | indice configurado %d | respaldo global usado %s\n",
                                sdk::air_lead_weapon_selection>=0 ? "indice explicito" :
                                sdk::air_lead_use_heaviest_weapon ? "mayor calibre" : "automatico",
                                sdk::air_lead_weapon_selection,aimbot::aircraft_global_fallback?"si":"no")
                            <<TacticalUI::Format("Ultimo estado contenedor global: %s\n",sdk::ground_ballistics_source);
                        const auto& trace=aimbot::aircraft_weapon_trace;
                        report<<TacticalUI::Format("Armas consultadas: %zu | seleccionada: %d (indices desde 0)\n",trace.count,trace.selected);
                        for(std::size_t i=0;i<trace.count;++i) {
                            const auto& row=trace.rows[i];const auto& sample=row.diag;
                            report<<TacticalUI::Format("Arma %d | slot %d | cartuchos %d | cadena %s | valida %s | %s\n",
                                row.index,sample.slot,sample.ammo_count,
                                sample.ammo_count<0 ? "sin muestra" : sample.ammo_count>=9 ? "indirecta" : "directa",
                                row.valid?"si":"no",row.status)
                                <<TacticalUI::Format("  array: %s | enlace: %s | cartucho: %s | ID %d\n",
                                    aimbot::ammo_pointer_label(sample.array_state),aimbot::ammo_pointer_label(sample.link_state),
                                    aimbot::ammo_pointer_label(sample.cartridge_state),sample.props_id);
                            // Stream floating point values exactly like the original technical report.
                            report<<TacticalUI::LocalText("  velocidad cartucho: ");
                            if(sample.cartridge_read)report<<sample.cartridge_speed;else report<<TacticalUI::LocalText("sin lectura");
                            report<<TacticalUI::LocalText(" | velocidad tabla: ");
                            if(sample.table_read)report<<sample.table_speed;else report<<TacticalUI::LocalText("sin lectura");
                            report<<TacticalUI::Format(" | tabla %s | ID en rango %s | entrada %s | max ID %u\n",
                                sample.table_pointer?"si":"no",sample.table_id_in_range?"si":"no",
                                sample.table_entry?"si":"no",sample.table_max_id);
                        }
                        ImGui::SetClipboardText(report.str().c_str());
                    }
                    if(TacticalUI::TreeNode("Resultados por arma (indices desde 0)")) {
                        const auto& trace=aimbot::aircraft_weapon_trace;
                        TacticalUI::Text("Consultadas: %u | elegida: %d",static_cast<unsigned>(trace.count),trace.selected);
                        for(size_t i=0;i<trace.count;++i) {
                            const auto& row=trace.rows[i];const auto& sample=row.diag;
                            ImGui::Separator();
                            TacticalUI::TextWrapped("Arma %d | slot %d | cartuchos %d | %s",row.index,sample.slot,sample.ammo_count,row.status);
                            TacticalUI::TextWrapped("Array: %s | enlace: %s | cartucho: %s",
                                aimbot::ammo_pointer_label(sample.array_state),aimbot::ammo_pointer_label(sample.link_state),
                                aimbot::ammo_pointer_label(sample.cartridge_state));
                            TacticalUI::Text("ID: %d | valida: %s",sample.props_id,row.valid?"si":"no");
                            if(sample.cartridge_read) TacticalUI::Text("Velocidad cartucho: %.1f m/s",sample.cartridge_speed);
                            if(sample.table_read) TacticalUI::Text("Velocidad tabla: %.1f m/s",sample.table_speed);
                            if(row.valid && row.index>=0 && row.index<=20) {
                                ImGui::PushID(row.index);
                                if(TacticalUI::SmallButton("Usar esta arma"))sdk::air_lead_weapon_selection=row.index;
                                ImGui::PopID();
                            }
                        }
                        ImGui::TreePop();
                    }
                    TacticalUI::TextDisabled("Los valores API validados no prueban que los campos internos del arma sigan vigentes.");
                }
                TacticalUI::TextDisabled("Estilo visual compartido A2A / G2A");
                ImGui::BeginDisabled((!sdk::air_lead_enabled && !esp::g2a_lead::enabled) || !sdk::esp_enabled);
                TacticalSliderFloat("Marker Size", &sdk::air_lead_marker_size, 2.0f, 15.0f, "%.1f px");
                TacticalSliderFloat("Line Thickness", &sdk::air_lead_line_thickness, 0.5f, 3.0f, "%.1f px");
                TacticalSliderFloat("Opacidad aim lead", &sdk::air_lead_opacity, 0.0f, 1.0f, "%.2f");
                TacticalToggle("Compatibilidad alfa (sin negro)",&sdk::air_lead_coverage_opacity);
                if(ImGui::IsItemHovered()) TacticalUI::SetTooltip("Trama puntos opacos con huecos transparentes; evita el negro en compositores que fallan con alfa parcial. Desactiva para alfa suave nativo.");
                TacticalColorEdit("Marker", (float*)&sdk::air_lead_sim_color);
                ImGui::EndDisabled();
                TacticalNextColumn();
                TacticalSection("Predicción terrestre");
                TacticalToggle("Prediccion terrestre G2G", &sdk::aimbot_enabled);
                TacticalUI::TextWrapped("G2G: %s", sdk::ground_prediction_status);
                if (sdk::ground_prediction_screen_known)
                    TacticalUI::Text("Marca: (%.0f, %.0f) px | monitor ESP %.0f x %.0f",
                        sdk::ground_prediction_screen_x,sdk::ground_prediction_screen_y,
                        sdk::ground_prediction_viewport_width,sdk::ground_prediction_viewport_height);
                TacticalUI::TextWrapped("Velocidad objetivo: %s", g2g_runtime::current.target_source);
                TacticalUI::TextWrapped("Velocidad propia: %s", g2g_runtime::current.own_source);
                if(g2g_runtime::current.valid) {
                    TacticalUI::Text("Muestra %.0f ms | solucion %.0f ms | vuelo %.2f s",
                        g2g_runtime::current.sample_age_ms,g2g_runtime::current.solve_age_ms,g2g_runtime::current.flight_time);
                    TacticalUI::Text("Residuo modelo %.3f m | %d pasos",g2g_runtime::current.error,g2g_runtime::current.steps);
                }
                TacticalUI::TextWrapped("Modelo G2G: origen de unidad (boca no verificada), herencia vectorial, arrastre geometrico (Cd no medido).");
                if(aimbot::ballisticsData.velocity>0.f && aimbot::ballisticsData.caliber>0.f)
                    TacticalUI::TextWrapped("Municion: %.0f m/s | calibre %.1f mm", aimbot::ballisticsData.velocity,
                        aimbot::ballisticsData.caliber * (aimbot::ballisticsData.caliber_in_mm ? 1.0f : 1000.0f));
                else TacticalUI::TextWrapped("Municion: sin datos fiables (%s)", sdk::ground_ballistics_source);
                TacticalPreview();
                TacticalColumnsEnd();
            }
            if (tactical_tab==1 || tactical_search[0]) {
                TacticalColumnsBegin(scale,"##aimbot_other");
                TacticalSection("Suavizado y controles");
                ImGui::BeginDisabled(!sdk::air_lead_enabled || !sdk::esp_enabled);
                TacticalToggle("Use Simulation", &sdk::air_lead_use_simulation);
                TacticalToggle("Turn Prediction (3D)", &sdk::air_lead_use_turn_prediction);
                if(ImGui::IsItemHovered())TacticalUI::SetTooltip("Giro 3D y aceleracion durante 0.85 s, luego tangente.\nVelocidad propia heredada solo al salir el proyectil; sin aceleracion del avion durante el vuelo.");
                if(!sdk::air_lead_use_turn_prediction)TacticalUI::TextDisabled("Este perfil usa aceleracion rectilinea; activa giro 3D para maniobras.");
                TacticalToggle("Adaptive Smoothing", &sdk::air_lead_smooth_adaptive);
                ImGui::BeginDisabled(!sdk::air_lead_smooth_adaptive);
                TacticalSliderFloat("Smooth Min Alpha", &sdk::air_lead_smooth_min_alpha, 0.05f, 0.95f, "%.2f");
                TacticalSliderFloat("Smooth Max Alpha", &sdk::air_lead_smooth_max_alpha, 0.05f, 0.95f, "%.2f");
                ImGui::EndDisabled();
                TacticalSliderFloat("Extra Lead Time (s)", &sdk::air_lead_extra_time, 0.0f, 0.5f, "%.2f s");
                ImGui::EndDisabled();
                TacticalKeybind("Aimbot key", sdk::key_aimbot_enable);
                ImGui::BeginDisabled(!sdk::air_lead_enabled || !sdk::esp_enabled);
                TacticalKeybind("Air Lead key", sdk::key_air_lead_enable);
                TacticalNextColumn();
                TacticalSection("Selección de objetivos");
                TacticalToggle("Only Target (center)", &sdk::air_lead_only_target);
                TacticalToggle("Helicopters", &sdk::air_lead_target_helicopter);
                TacticalToggle("Recon Drones", &sdk::air_lead_target_recon_drone);
                TacticalToggle("Fighters", &sdk::air_lead_target_fighter);
                TacticalToggle("Bombers", &sdk::air_lead_target_bomber);
                TacticalToggle("Attackers", &sdk::air_lead_target_attacker);
                ImGui::EndDisabled();
                TacticalColumnsEnd();
            }
            ImGui::PopID();
        }

        if((!tactical_search[0] && tactical_page==5) || (tactical_search[0] && filter.PassFilter("Radar Enable Radar Enemy Marker"))) {
            ImGui::PushID(5); if (tactical_search[0]) TacticalUI::SeparatorText("Radar");
            TacticalColumnsBegin(scale);
            if (tactical_tab==0 || tactical_search[0]) {
                TacticalSection("RADAR / PRINCIPAL");
                TacticalToggle("Enable Radar", &sdk::radar_enabled);
                TacticalUI::TextWrapped("Marcadores enemigos sobre minimapa y mapa tactico.");
                TacticalNextColumn(); TacticalSection("IDENTIFICACION");
                ImGui::BeginDisabled(!sdk::radar_enabled);
                TacticalColorEdit("Enemy Marker", (float*)&sdk::radar_enemy_color);
                ImGui::EndDisabled();
                TacticalUI::TextDisabled("La lista actual excluye aliados.");
            } else {
                TacticalSection("RADAR / ESTADO");
                TacticalUI::Text("Marcadores: %s", menu_preferences::label(sdk::radar_enabled ? "activos" : "inactivos"));
                const char* radar_states[]={"apagado/sin juego","mapa cerrado","rectangulo invalido","escala invalida","mapa en vivo","mapa reciente"};
                const int radar_state=std::clamp(esp::radar::map_status.load(),0,5);
                TacticalUI::Text("Mapa 1:1: %s",menu_preferences::label(radar_states[radar_state]));
                TacticalUI::Text("Contactos: %d/%d | sin tipo: %d",
                    esp::radar::rendered_contacts.load(),esp::radar::total_contacts.load(),
                    esp::radar::unidentified_contacts.load());
                TacticalNextColumn(); TacticalSection("COMPATIBILIDAD");
                TacticalUI::TextWrapped("Si el mapa 1:1 no aparece, revisa gate/rectangulo/escala y reporta contactos sin tipo. Cazas, ataque, bombarderos y helicopteros usan siluetas independientes.");
            }
            TacticalColumnsEnd();
            if (tactical_tab==0 || tactical_search[0]) {
                TacticalSection("TIPO DE UNIDAD O ESTRUCTURA / DIBUJAR");
                TacticalUI::TextWrapped("Iconos del radar: cada casilla controla los contactos de ese tipo.");
                if (ImGui::BeginChild("##radar_kinds",ImVec2(0,270.f*scale),true)) {
                    for (std::size_t i=0;i<esp::radar::icons::visible.size();++i) {
                        const auto kind=static_cast<esp::radar::icons::kind>(i);
                        ImGui::PushID(static_cast<int>(i));
                        bool shown=esp::radar::icons::visible[i];
                        if (TacticalUI::Checkbox("##visible",&shown)) esp::radar::icons::visible[i]=shown;
                        ImGui::SameLine();
                        const ImVec2 top=ImGui::GetCursorScreenPos();
                        ImGui::Dummy(ImVec2(24.f*scale,19.f*scale));
                        esp::radar::icons::draw(ImGui::GetWindowDrawList(),
                            ImVec2(top.x+12.f*scale,top.y+10.f*scale),kind,
                            ImGui::ColorConvertFloat4ToU32(sdk::radar_enemy_color));
                        ImGui::SameLine(); TacticalUI::TextUnformatted(esp::radar::icons::labels[i]);
                        ImGui::PopID();
                    }
                }
                ImGui::EndChild();
            }
            ImGui::PopID();
        }

        if((!tactical_search[0] && tactical_page==6) || (tactical_search[0] && filter.PassFilter("Perfiles ##name Load Save Export Config Import Config"))) {
 ImGui::PushID(6); if (tactical_search[0]) TacticalUI::SeparatorText("Perfiles");
 if(tactical_tab==0 || tactical_search[0]) { TacticalColumnsBegin(scale); TacticalSection("Perfiles guardados");
                    static char name_buf[5][64] = {};
                    static int pending_load=-1, pending_overwrite=-1;
                    bool open_load=false, open_overwrite=false;
                    static bool initialized = false;
                    if (!initialized) {
                        for (int i = 0; i < 5; i++) {
                            strncpy_s(name_buf[i], sizeof(name_buf[i]), presets[i].name.c_str(), sizeof(name_buf[i]) - 1);
                            name_buf[i][sizeof(name_buf[i]) - 1] = '\0';
                        }
                        initialized = true;
                    }
                    TacticalUI::TextDisabled("NOMBRE DE PERFIL");
                    for(int i=0;i<5;i++) {
                        ImGui::PushID(i);
                        const float available=ImGui::GetContentRegionAvail().x;
                        const float start_x=ImGui::GetCursorPosX();
                        const float gap=ImGui::GetStyle().ItemSpacing.x;
                        const float id_width=36.f*scale, action_width=76.f*scale;
                        const float name_width=std::max(90.f,available-id_width-2.f*action_width-3.f*gap);
                        if(active_preset==i) TacticalUI::TextColored(ImVec4(.98f,.25f,.30f,1.f),"%02d",i+1);
                        else TacticalUI::TextDisabled("%02d",i+1);
                        ImGui::SameLine();ImGui::SetCursorPosX(start_x+id_width);
                        ImGui::SetNextItemWidth(name_width);
                        if(ImGui::InputText("##name",name_buf[i],IM_ARRAYSIZE(name_buf[i])))
                            presets[i].name=name_buf[i];
                        if(i==0) TacticalSearchFocus("Nombre de perfil");
                        if(!ImGui::IsItemActive() && presets[i].name!=name_buf[i])
                            strncpy_s(name_buf[i],sizeof(name_buf[i]),presets[i].name.c_str(),_TRUNCATE);
                        ImGui::SameLine();
                        if(TacticalUI::Button("Cargar",ImVec2(action_width,0))) {
                            if(profile_dirty) { pending_load=i;open_load=true; }
                            else {
                                if(i!=active_preset) sync_active_preset_from_runtime();
                                presets[i].name=name_buf[i];active_preset=i;
                                apply_preset(presets[i]);
                            }
                        }
                        if(i==0) TacticalSearchFocus("Cargar");
                        ImGui::SameLine();
                        if(TacticalUI::Button("Guardar",ImVec2(action_width,0))) {
                            if(i!=active_preset) {pending_overwrite=i;open_overwrite=true;}
                            else { presets[i]=capture_current_settings();presets[i].name=name_buf[i];save_now(); }
                        }
                        if(i==0) TacticalSearchFocus("Guardar");
                        ImGui::PopID();
                        if(i<4) ImGui::Separator();
                    }
if(open_load) TacticalUI::OpenPopup("Confirmar cambio de perfil");
                    ImGui::SetNextWindowSize(ImVec2(420.f*scale,220.f*scale),ImGuiCond_Appearing);
                    ImGui::SetNextWindowPos(ImVec2(window_pos.x+window_size.x*.5f,window_pos.y+window_size.y*.5f),ImGuiCond_Appearing,ImVec2(.5f,.5f));
                    if(TacticalUI::BeginPopupModal("Confirmar cambio de perfil",nullptr,ImGuiWindowFlags_NoResize)) {
                        TacticalUI::TextWrapped("El perfil activo tiene cambios que aún no se aplicaron al perfil seleccionado.");
                        TacticalUI::TextDisabled("Solo se guarda cuando pulsas Guardar.");
                        if(TacticalUI::Button("Guardar actual y cargar",ImVec2(210.f*scale,0))) {
                            sync_active_preset_from_runtime();
                            active_preset=pending_load;apply_preset(presets[active_preset]);save_now();
                            pending_load=-1;ImGui::CloseCurrentPopup();
                        }
                        if(TacticalUI::Button("Descartar y cargar",ImVec2(210.f*scale,0))) {
                            active_preset=pending_load;apply_preset(presets[active_preset]);
                            pending_load=-1;ImGui::CloseCurrentPopup();
                        }
                        ImGui::SameLine();
                        if(TacticalUI::Button("Cancelar",ImVec2(95.f*scale,0))) {pending_load=-1;ImGui::CloseCurrentPopup();}
                        ImGui::EndPopup();
                    }
                    if(open_overwrite) TacticalUI::OpenPopup("Sobrescribir perfil");
                    ImGui::SetNextWindowSize(ImVec2(380.f*scale,155.f*scale),ImGuiCond_Appearing);
                    ImGui::SetNextWindowPos(ImVec2(window_pos.x+window_size.x*.5f,window_pos.y+window_size.y*.5f),ImGuiCond_Appearing,ImVec2(.5f,.5f));
                    if(TacticalUI::BeginPopupModal("Sobrescribir perfil",nullptr,ImGuiWindowFlags_NoResize)) {
                        TacticalUI::TextWrapped("Se reemplazará el contenido del perfil %d (%s).",pending_overwrite+1,
                            runtime_language::raw(pending_overwrite>=0?presets[pending_overwrite].name.c_str():""));
                        if(TacticalUI::Button("Sí, sobrescribir",ImVec2(165.f*scale,0))) {
                            const std::string name=presets[pending_overwrite].name;
                            presets[pending_overwrite]=capture_current_settings();
                            presets[pending_overwrite].name=name;
                            save_now();pending_overwrite=-1;ImGui::CloseCurrentPopup();
                        }
                        ImGui::SameLine();
                        if(TacticalUI::Button("Cancelar",ImVec2(95.f*scale,0))) {pending_overwrite=-1;ImGui::CloseCurrentPopup();}
                        ImGui::EndPopup();
                    }
TacticalNextColumn(); TacticalSection("SESION ACTUAL");
 TacticalUI::TextWrapped("Perfil %d: %s", active_preset+1,runtime_language::raw(presets[std::clamp(active_preset,0,4)].name.c_str()));
 TacticalUI::TextWrapped("Cargar avisa si descartaría cambios. Guardar sobre otro perfil requiere confirmación.");
 TacticalColumnsEnd(); }
 if(tactical_tab==1 || tactical_search[0]) { TacticalColumnsBegin(scale,"##profile_io"); TacticalSection("IMPORTAR / EXPORTAR");
                            static std::string pending_import;
                            if (TacticalUI::Button("Export Config")) {
                                OPENFILENAMEA ofn = { sizeof(OPENFILENAMEA) };
                                char file[260] = "cheat_config.cfg";
                                ofn.lpstrFile = file;
                                ofn.nMaxFile = 260;
                                ofn.lpstrFilter = menu_preferences::current.language==0
                                    ? "Archivos de configuracion (*.cfg)\0*.cfg\0Todos los archivos (*.*)\0*.*\0"
                                    : "Config Files (*.cfg)\0*.cfg\0All Files (*.*)\0*.*\0";
                                ofn.Flags = OFN_OVERWRITEPROMPT;
                                if (GetSaveFileNameA(&ofn) && !ExportConfig(file)) configuration_warning="Error al exportar el archivo.";
                            }
                            TacticalSearchFocus("Export Config");
                            
                            if (TacticalUI::Button("Import Config")) {
                                OPENFILENAMEA ofn = { sizeof(OPENFILENAMEA) };
                                char file[260] = "";
                                ofn.lpstrFile = file;
                                ofn.nMaxFile = 260;
                                ofn.lpstrFilter = menu_preferences::current.language==0
                                    ? "Archivos de configuracion (*.cfg)\0*.cfg\0Todos los archivos (*.*)\0*.*\0"
                                    : "Config Files (*.cfg)\0*.cfg\0All Files (*.*)\0*.*\0";
                                ofn.Flags = OFN_FILEMUSTEXIST;
                                if (GetOpenFileNameA(&ofn)) {pending_import=file;TacticalUI::OpenPopup("Confirmar importación");}
                            }
                            TacticalSearchFocus("Import Config");
                            ImGui::SetNextWindowSize(ImVec2(430.f*scale,205.f*scale),ImGuiCond_Appearing);
                    ImGui::SetNextWindowPos(ImVec2(window_pos.x+window_size.x*.5f,window_pos.y+window_size.y*.5f),ImGuiCond_Appearing,ImVec2(.5f,.5f));
                    if(TacticalUI::BeginPopupModal("Confirmar importación",nullptr,ImGuiWindowFlags_NoResize)) {
                                TacticalUI::TextWrapped("Importar reemplaza los cinco perfiles en esta sesión. Se respalda el archivo anterior; después pulsa Guardar configuración para conservar la importación.");
                                if(TacticalUI::Button("Respaldar e importar",ImVec2(195.f*scale,0))) {
                                    if(save_now()) {
                                        if(!ImportConfig(pending_import)) configuration_warning="Importación rechazada: archivo incompatible.";
                                    } else configuration_warning="No se pudo respaldar: importación cancelada.";
                                    pending_import.clear();ImGui::CloseCurrentPopup();
                                }
                                ImGui::SameLine();
                                if(TacticalUI::Button("Cancelar",ImVec2(95.f*scale,0))) {pending_import.clear();ImGui::CloseCurrentPopup();}
                                ImGui::EndPopup();
                            }
 TacticalNextColumn(); TacticalSection("ARCHIVOS");
 TacticalUI::TextWrapped("Sin autoguardado: pulsa Guardar configuración o Guardar en un perfil. Usa .cfg para transferirlos.");
 TacticalColumnsEnd(); } ImGui::PopID();
 }

        if((!tactical_search[0] && tactical_page==7) || (tactical_search[0] && filter.PassFilter("Ajustes Apply Fullscreen to Monitor Apply Fullscreen on Start V-Sync Target FPS Escala de interfaz Toggle ESP 2D Box 3D Box Vehicle Name Distance Toggle X-Ray Show Crew Show Barrel Show AmmoRack Show Fueltank Toggle Aimbot Toggle Air Lead Guardar clave API local Actualizar offsets ahora Recargar SDK Close Menu"))) {
 ImGui::PushID(7); if (tactical_search[0]) TacticalUI::SeparatorText("Ajustes");
 if(tactical_tab==0 || tactical_search[0]) { TacticalColumnsBegin(scale); TacticalSection("Pantalla");
                            TacticalUI::TextDisabled("Idioma");
                            int language=menu_preferences::valid_language(menu_preferences::current.language);
                            const char* languages[]={"Espanol","English"};
                            if(ImGui::Combo("##menu_language",&language,languages,2)) {
                                menu_preferences::current.language=language;
                                runtime_language::set_language(language);
                            }
                            if(ImGui::IsItemHovered())TacticalUI::SetTooltip("El idioma y el filtro se guardan solo al guardar el perfil o la configuracion.");
                            TacticalSearchFocus("Idioma");
                            int safe_monitor_index = sdk::monitor_index;
                            if (sdk::monitor_count <= 0) safe_monitor_index = 0;
                            else { if (safe_monitor_index < 0) safe_monitor_index = 0; if (safe_monitor_index > sdk::monitor_count - 1) safe_monitor_index = sdk::monitor_count - 1; }
                            sdk::monitor_index = safe_monitor_index;

                            const std::string unknown_monitor=TacticalUI::LocalText("Unknown monitor");
                            const char* preview = unknown_monitor.c_str();
                            if (!sdk::monitor_labels.empty() && safe_monitor_index < (int)sdk::monitor_labels.size())
                                preview = sdk::monitor_labels[safe_monitor_index].c_str();
                            TacticalUI::TextDisabled("Monitor");
                            ImGui::SetNextItemWidth(-1);
                            if (ImGui::BeginCombo("##monitor", preview))
                            {
                                for (int i = 0; i < (int)sdk::monitor_labels.size(); ++i)
                                {
                                    const bool is_selected = (sdk::monitor_index == i);
                                    if (ImGui::Selectable(sdk::monitor_labels[i].c_str(), is_selected))
                                    {
                                        sdk::monitor_index = i;
                                        sync_shared_settings_to_all_presets();
                                    }
                                    if (is_selected) ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }
                            TacticalSearchFocus("Monitor");
                            if (TacticalUI::Button("Apply Fullscreen to Monitor")) sdk::request_monitor_apply = true;
                            TacticalSearchFocus("Apply Fullscreen to Monitor");
                            if (TacticalToggle("Apply Fullscreen on Start", &sdk::auto_fullscreen_on_start))
                            {
                                sync_shared_settings_to_all_presets();
                            }
                            TacticalNextColumn(); TacticalSection("Interfaz");
                            TacticalToggle("V-Sync", &sdk::esp_vsync_enabled);
                            TacticalUI::TextDisabled("FPS objetivo");
                            ImGui::SetNextItemWidth(115.f*scale);
                            if (ImGui::InputInt("##target_fps", &sdk::esp_vsync_fps,0,0))
                                sdk::esp_vsync_fps = std::clamp(sdk::esp_vsync_fps, 30, 500);
                            TacticalSearchFocus("FPS objetivo");
TacticalSliderFloat("Escala de interfaz", &sdk::menu_ui_scale, 0.8f, 1.5f, "%.2fx");
TacticalUI::TextDisabled("Acento rojo");
ImGui::SetNextItemWidth(-1);
const char* accents[]={"Escarlata","Carmesi","Neon","Granate","Vermellon","Borgona","Brillante"};
TacticalUI::Combo("##accent",&sdk::menu_style,accents,7);
TacticalSearchFocus("Acento rojo");
TacticalColumnsEnd(); }
 if(tactical_tab==1 || tactical_search[0]) { TacticalColumnsBegin(scale,"##settings_advanced"); TacticalSection("Asignación de teclas");
if(!tactical_search[0]) ImGui::SetNextItemOpen(true,ImGuiCond_Once);
if(TacticalAdvanced("Atajos de teclado")){
                            if(!configuration_warning.empty()) TacticalUI::TextColored(ImVec4(.98f,.58f,.28f,1.f),"%s",menu_preferences::label(configuration_warning.c_str()));
                            if(!tactical_hotkey_notice.empty()) TacticalUI::TextWrapped("Atajo rechazado: %s (reservado o en uso).",menu_preferences::label(tactical_hotkey_notice.c_str()));
                            TacticalUI::SeparatorText("ESP");
                            TacticalKeybind("Toggle ESP", sdk::key_esp_enable);
                            TacticalKeybind("2D Box", sdk::key_2d_box);
                            TacticalKeybind("3D Box", sdk::key_3d_box);
                            TacticalKeybind("Vehicle Name", sdk::key_vehicle_name);
                            TacticalKeybind("Distance", sdk::key_distance);
                            TacticalUI::SeparatorText("X-Ray");
                            TacticalKeybind("Toggle X-Ray", sdk::xray_toggle_key);
                            TacticalKeybind("Show Crew", sdk::key_show_crew);
                            TacticalKeybind("Show Barrel", sdk::key_show_barrel);
                            TacticalKeybind("Show AmmoRack", sdk::key_show_ammorack);
                            TacticalKeybind("Show Fueltank", sdk::key_show_fueltank);
                            TacticalUI::SeparatorText("Aimbot");
                            TacticalKeybind("Toggle Aimbot", sdk::key_aimbot_enable);
                            TacticalKeybind("Toggle Air Lead", sdk::key_air_lead_enable);
                            TacticalUI::TextDisabled("F1 / F2 reservadas. Escape cancela; Backspace borra.");}
TacticalNextColumn(); TacticalSection("OFFSETS / SESION");
if(TacticalAdvanced("Offsets avanzados")){
                            TacticalUI::TextWrapped("Fuente: API validada o su caché cifrada para este usuario. Los archivos offsets.txt ya no se leen.");
                            TacticalUI::Text("Snapshot: %s | edad %.1f ms",menu_preferences::label(misc::prof_snapshot_valid.load()?"válido":"sin datos"),
                                misc::prof_snapshot_age_us.load()/1000.f);
                            TacticalUI::Text("Lecturas opcionales fallidas: %llu",static_cast<unsigned long long>(optional_failures));
                            if(optional_failures) TacticalUI::Text("Última dirección: 0x%llX",
                                static_cast<unsigned long long>(TargetProcess->LastOptionalFailure()));
                            if(!sdk::manual_reload_status.empty()) TacticalUI::TextWrapped("Recarga: %s",sdk::manual_reload_status.c_str());
                            if(sdk::request_manual_reload) TacticalUI::TextDisabled("Recarga solicitada...");
                            TacticalUI::TextDisabled("Sin API válida: última caché API o valores incorporados.");
                            TacticalUI::SeparatorText("Actualización automática privada");
                            TacticalUI::TextWrapped("HTTPS al iniciar: la clave persiste en Credenciales de Windows de este usuario, incluso al recompilar. Nunca se incluye en perfiles ni en la caché cifrada.");
                            static char local_api_key[513]{};
                            ImGui::SetNextItemWidth(-1);
                            TacticalUI::InputTextWithHint("##private_api_key","Introduce la clave aquí, no en el repositorio",
                                local_api_key,IM_ARRAYSIZE(local_api_key),ImGuiInputTextFlags_Password);
                            static bool has_private_key=offset_auto_update::vault.exists();
                            if(TacticalUI::Button("Guardar clave API local")) {
                                const bool stored=offset_auto_update::vault.save(local_api_key);
                                SecureZeroMemory(local_api_key,sizeof(local_api_key));
                                if(stored) {has_private_key=true; offset_auto_update::refresh(true);}
                                else offset_auto_update::set_status("No se pudo guardar la clave en Credenciales de Windows.");
                            }
                            TacticalSearchFocus("Guardar clave API local");
                            ImGui::BeginDisabled(!has_private_key);
                            if(TacticalUI::Button("Actualizar offsets ahora")) offset_auto_update::refresh(true);
                            TacticalSearchFocus("Actualizar offsets ahora");
                            ImGui::SameLine();
                            if(TacticalUI::Button("Borrar clave API local")) {
                                if(offset_auto_update::vault.erase()) {has_private_key=false; offset_auto_update::set_status("Clave borrada; la última caché API válida permanece disponible.");}
                            }
                            TacticalSearchFocus("Borrar clave API local");
                            ImGui::EndDisabled();
                            TacticalUI::TextWrapped("%s",offset_auto_update::get_status().c_str());
}
                            ImGui::Separator();
                            if (TacticalUI::Button("Close Menu")) menu_open = false;
                            TacticalSearchFocus("Close Menu");
 TacticalColumnsEnd(); } ImGui::PopID();
 }
        } // Fin de pagina: los resultados muestran opciones, no paneles enteros.
        ImGui::EndChild();
    }
    ImGui::End();
    ImGui::PopFont();
    ImGui::GetStyle()=saved_style;
}
