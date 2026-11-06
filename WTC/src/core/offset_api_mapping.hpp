#pragma once
#include "offset_api_payload.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace offset_api_mapping {
struct binding {const char* remote;const char* local;};
inline constexpr binding bindings[] = {
    {"Global.g_RocketListIndexPtr","g_RocketListIndexPtr"},
    {"Global.g_RocketListIndexPtr","rocket_index"},
    {"Global.g_BombListIndexPtr","g_BombListIndexPtr"},
    {"Global.g_BombListIndexPtr","bomb_index"},
    {"Global.g_EntityManager","g_EntityManager"},
    {"Global.g_EntityManager","alllistdata_offset"},
    {"Global.g_GameContext","cgame_offset"},
    {"Global.g_HudInfo","hud_offset"},
    {"Global.g_LocalPlayer","localplayer_offset"},
    {"Global.g_MyUnit","my_unit_offset"},
    {"Global.g_IsScoping","is_scoping_offset"},
    {"Global.g_ScreenWidth","screen_width_offset"},
    {"Global.g_ViewMatrix","view_matrix_offset"},
    {"Global.g_ViewAngles","view_angles_offset"},
    {"Global.g_AirPredictionBool","air_prediction_bool_offset"},
    {"Global.g_GameOptics","game_optics_offset"},
    {"Global.g_GameFontList","game_font_list_offset"},
    {"Global.g_LocalizationInfo","localization_info_offset"},
    {"Global.g_MapDrawnThisFrame","map_drawn_this_frame_offset"},
    {"Global.g_MapFullWorldCenter","map_full_world_center_offset"},
    {"Global.g_MapFullWorldSize","map_full_world_size_offset"},
    {"Global.g_MapMinimapPos","map_minimap_pos_offset"},
    {"Global.g_MapMinimapSize","map_minimap_size_offset"},
    {"Global.g_MapViewSize","map_view_size_offset"},
    {"Global.g_MapWorldCenter","map_world_center_offset"},
    {"Global.g_TacticalMapObj","tactical_map_obj_offset"},
    {"Global.g_NameMap","name_map_offset"},
    {"Global.g_UIPosArr","ui_pos_arr_offset"},
    {"Global.g_BaseGameVersionStr","base_game_version_str"},
    {"Global.g_ClockObj","clock_obj"},
    {"Global.g_GameDifficulty","game_difficulty"},
    {"Global.g_IsInHangar","is_in_hangar_offset"},
    {"Global.g_ProjBallisticsPropsPositive","proj_ballistics_props_positive_offset"},
    {"Global.g_ProjBallisticsPropsNegative","proj_ballistics_props_negative_offset"},
    {"Global.g_BulletPropsPositive","bullet_props_positive_offset"},
    {"Global.g_BulletPropsNegative","bullet_props_negative_offset"},
    {"Global.g_ReloadOffset1","reload_offset1"},
    {"Global.g_ReloadOffset2","reload_offset2"},
    {"Global.g_ReloadOffset3","reload_offset3"},
    {"Global.g_ReloadOffset4","reload_offset4"},
    {"Global.g_ReloadOffset5","reload_offset5"},
    {"Global.g_ReloadOffset6","reload_offset6"},
    {"Global.g_ReloadOffset7","reload_offset7"},
    {"Game.m_BallisticsCont","cgame_offsets.ballistics_offset"},
    {"Game.m_BasesCont","cgame_offsets.bases_cont"},
    {"Game.m_CameraCont","cgame_offsets.camera_offset"},
    {"Game.m_CurrentMap","cgame_offsets.current_map"},
    {"Game.m_LevelOwner","cgame_offsets.level_owner"},
    {"Game.m_ReportPtr","cgame_offsets.report_ptr"},
    {"Game.m_Time","cgame_offsets.time_offset"},
    {"Game.m_TimeManager","cgame_offsets.time_manager"},
    {"Game.m_UnitCount1","cgame_offsets.unit_count1"},
    {"Game.m_UnitCount2","cgame_offsets.unit_count2"},
    {"Game.m_UnitCount3","cgame_offsets.unit_count3"},
    {"Game.m_UnitList1","cgame_offsets.unit_list1"},
    {"Game.m_UnitList2","cgame_offsets.unit_list2"},
    {"Game.m_UnitList3","cgame_offsets.unit_list3"},
    {"Camera.m_CameraBase","cgame_offsets.camera_offsets.camera_base"},
    {"Camera.m_ViewMatrix","cgame_offsets.camera_offsets.camera_matrix_offset"},
    {"Camera.m_CameraPos","cgame_offsets.camera_offsets.camera_position_offset"},
    {"Ballistics.m_BombPredPos","cgame_offsets.ballistic_offsets.bomb_prediction_offset"},
    {"Ballistics.m_Caliber","cgame_offsets.ballistic_offsets.round_caliber_offset"},
    {"Ballistics.m_Length","cgame_offsets.ballistic_offsets.round_length_offset"},
    {"Ballistics.m_Mass","cgame_offsets.ballistic_offsets.round_mass_offset"},
    {"Ballistics.m_MuzzleSpeed","cgame_offsets.ballistic_offsets.round_velocity_offset"},
    {"Ballistics.m_MaxDistance","cgame_offsets.ballistic_offsets.max_dist"},
    {"Player.m_GuiState","localplayer.guiState_offset"},
    {"Player.m_OwnedUnit","localplayer.localunit_offset"},
    {"Player.m_OwnedUnit","localplayer.ownedUnitRef_offset"},
    {"Player.m_ControlledUnit","localplayer.controlledUnitRef_offset"},
    {"Player.m_SpectatedModelIndex","localplayer.spectatedModel_offset"},
    {"Player.m_BotName","localplayer.botName_offset"},
    {"Player.m_Name","localplayer.nik_offset"},
    {"Player.m_TeamNum","localplayer.teamNum_offset"},
    {"Player.m_Score","localplayer.score_offset"},
    {"Unit.m_AirContainer","unit_offsets.airmovement_offset"},
    {"Unit.m_Armory","unit_offsets.armory_offset"},
    {"Unit.m_Armory","unit_offsets.turret_offset"},
    {"Unit.m_BaseOverloadInfo","unit_offsets.baseOverloadInfo_offset"},
    {"Unit.m_BBMax","unit_offsets.bbmax_offset"},
    {"Unit.m_BBMin","unit_offsets.bbmin_offset"},
    {"Unit.m_BodyBBMax","unit_offsets.bodyBBMax_offset"},
    {"Unit.m_BodyBBMin","unit_offsets.bodyBBMin_offset"},
    {"Unit.m_BriefMalfunctionState","unit_offsets.briefMalfunctionState_offset"},
    {"Unit.m_DamageModelCont","unit_offsets.damageModelCont_offset"},
    {"Unit.m_EntityId","unit_offsets.entityId_offset"},
    {"Unit.m_UnitInfo","unit_offsets.info_offset"},
    {"Unit.m_InvulTimer","unit_offsets.invulTimer_offset"},
    {"Unit.m_Invulnerable","unit_offsets.invulnerable_offset"},
    {"Unit.m_IsBurning","unit_offsets.isBurning_offset"},
    {"Unit.m_ModelTransform","unit_offsets.modelTransform_offset"},
    {"Unit.m_PlayerInfo","unit_offsets.playerInfo_offset"},
    {"Unit.m_Position","unit_offsets.position_offset"},
    {"Unit.m_RotationMatrix","unit_offsets.rotation_matrix_offset"},
    {"Unit.m_TeamNum","unit_offsets.unitArmyNo_offset"},
    {"Unit.m_UnitUid","unit_offsets.unitIndex_offset"},
    {"Unit.m_UnitState","unit_offsets.unitState_offset"},
    {"Unit.m_UnitType","unit_offsets.unitType_offset"},
    {"Unit.m_VisualReload","unit_offsets.visualReloadProgress_offset"},
    {"Unit.m_XrayUnitGate","unit_offsets.xrayUnitGate_offset"},
    {"Unit.m_UnitFlags1","unit_offsets.unitFlags1_offset"},
    {"Unit.m_UnitFlags2","unit_offsets.unitFlags2_offset"},
    {"Unit.m_UnitFlags3","unit_offsets.unitFlags3_offset"},
    {"Unit.m_UnitFlags4","unit_offsets.unitFlags4_offset"},
    {"Unit.m_Velocity","unit_offsets.ground_velocity_offset"},
    {"GroundMovement.m_Velocity","unit_offsets.groundmovement_velocity_offset"},
    {"AirContainer.m_Velocity","unit_offsets.airmovement_offsets.velocity_offset"},
    {"AirContainer.m_Acceleration","unit_offsets.airmovement_offsets.acceleration_offset"},
    {"Unit::Info.m_HaveCCIPForBombs","unit_info_offsets.have_ccip_for_bombs"},
    {"Unit::Info.m_HaveCCIPForGun","unit_info_offsets.have_ccip_for_gun"},
    {"Unit::Info.m_HaveCCIPForRocket","unit_info_offsets.have_ccip_for_rocket"},
    {"Unit::Info.m_HaveCCIPForTurret","unit_info_offsets.have_ccip_for_turret"},
    {"Unit::Info.m_BomberView","unit_info_offsets.bomber_view"},
    {"Unit::Info.m_IsNuclearBomber","unit_info_offsets.is_nuclear_bomber"},
    {"Armory.m_ControllableWeapons","armory_offsets.controllable_weapons"},
    {"Armory.m_ControllableWeaponsCount","armory_offsets.controllable_weapons_count"},
    {"Armory.m_WeaponCount","armory_offsets.weapon_count"},
    {"Armory.m_WeaponPositionInfo","armory_offsets.weapon_position_info"},
    {"Armory.m_WeaponPositionInfoCount","armory_offsets.weapon_position_info_count"},
    {"Armory.m_Weapons","armory_offsets.weapons"},
    {"Projectile.m_MissileNameCont","projectile_offsets.missile_name_container"},
    {"Projectile.m_BombNameCont","projectile_offsets.bomb_name_container"},
    {"Projectile.m_Position","projectile_offsets.position"},
    {"Projectile.m_Velocity","projectile_offsets.velocity"},
    {"Projectile.m_OwnerUnit","projectile_offsets.owner"},
    {"MissileNameCont.m_Type","projectile_offsets.missile_name_type"},
    {"BombNameCont.m_Name","projectile_offsets.bomb_name_text"},
    {"TacticalMap.m_AspectScale","tactical_map_offsets.aspect"},
    {"TacticalMap.m_ModeCache","tactical_map_offsets.mode"},
    {"TacticalMap.m_BoundsMin","tactical_map_offsets.bounds_min"},
    {"TacticalMap.m_BoundsMax","tactical_map_offsets.bounds_max"},
    {"Function.das_get_usefull_total_time","function_das_get_usefull_total_time"},
    {"Function.rayhit_normalized","function_rayhit_normalized"},
    {"DamageModel.m_ModelContainer","damage_model.m_ModelContainer"},
    {"DamageModel.m_Header","damage_model.m_Header"},
    {"DamageModel.m_StateArray","damage_model.m_StateArray"},
    {"DamageModel.m_StateCount","damage_model.m_StateCount"},
    {"DamageModel.m_StaticProps","damage_model.m_StaticProps"},
    {"ModelContainer.m_PackedHeader","damage_model.m_PackedHeader"},
    {"ModelContainer.m_TransformArray","damage_model.m_TransformArray"},
    {"BoneMatrix.m_BoneMatrixLocalRow0","bone_matrix.m_BoneMatrixLocalRow0"},
    {"BoneMatrix.m_BoneMatrixLocalRow1","bone_matrix.m_BoneMatrixLocalRow1"},
    {"BoneMatrix.m_BoneMatrixLocalRow2","bone_matrix.m_BoneMatrixLocalRow2"},
    {"BoneMatrix.m_BoneMatrixLocalTrans","bone_matrix.m_BoneMatrixLocalTrans"},
    {"BoneMatrix.m_BoneMatrixFwd","bone_matrix.m_BoneMatrixFwd"},
    {"BoneMatrix.m_BoneMatrixUp","bone_matrix.m_BoneMatrixUp"},
    {"BoneMatrix.m_BoneMatrixRight","bone_matrix.m_BoneMatrixRight"},
    {"BoneMatrix.m_BoneMatrixPos","bone_matrix.m_BoneMatrixPos"},
    {"WeaponInfo.m_Ammo","weapon_info_offsets.ammo"},
    {"WeaponInfo.m_Clip_size","weapon_info_offsets.clip_size"},
    {"WeaponInfo.m_Cur_ammo","weapon_info_offsets.cur_ammo"},
    {"WeaponInfo.m_Max_ammo","weapon_info_offsets.max_ammo"},
    {"WeaponInfo.m_Wasted_ammo","weapon_info_offsets.wasted_ammo"},
};
inline bool air_velocity_available(const offset_api_payload::manifest& remote) {
    const auto container=remote.entries.find("Unit.m_AirContainer");
    const auto velocity=remote.entries.find("AirContainer.m_Velocity");
    return container!=remote.entries.end() && container->second!=0 &&
        velocity!=remote.entries.end() && velocity->second!=0;
}
inline bool air_acceleration_available(const offset_api_payload::manifest& remote) {
    const auto field=remote.entries.find("AirContainer.m_Acceleration");
    return air_velocity_available(remote) && field!=remote.entries.end() && field->second!=0;
}
inline bool invulnerable_available(const offset_api_payload::manifest& remote) {
    const auto field=remote.entries.find("Unit.m_Invulnerable");
    return field!=remote.entries.end() && field->second!=0;
}
inline bool invul_timer_available(const offset_api_payload::manifest& remote) {
    const auto field=remote.entries.find("Unit.m_InvulTimer");
    return field!=remote.entries.end() && field->second!=0;
}
inline bool reload_available(const offset_api_payload::manifest& remote) {
    const auto value=remote.entries.find("Unit.m_VisualReload");
    return value!=remote.entries.end() && value->second!=0;
}
inline bool ground_velocity_available(const offset_api_payload::manifest& remote) {
    const auto pointer=remote.entries.find("Unit.m_Velocity");
    const auto field=remote.entries.find("GroundMovement.m_Velocity");
    return pointer!=remote.entries.end() && field!=remote.entries.end() &&
        pointer->second!=0 && field->second!=0;
}
inline bool round_ballistics_available(const offset_api_payload::manifest& remote) {
    for(const auto* field:{"Game.m_BallisticsCont","Ballistics.m_MuzzleSpeed",
                           "Ballistics.m_Caliber","Ballistics.m_Mass","Ballistics.m_Length"}) {
        const auto found=remote.entries.find(field);
        if(found==remote.entries.end() || found->second==0)return false;
    }
    return true;
}
inline bool apply(const offset_api_payload::manifest& remote,
    const std::unordered_map<std::string,std::uintptr_t>& existing,
    std::unordered_map<std::string,std::uintptr_t>& output,
    std::string& error,std::size_t& mapped) {
    static constexpr std::string_view required[]={
        "Global.g_EntityManager","Global.g_GameContext","Global.g_LocalPlayer",
        "Global.g_RocketListIndexPtr","Global.g_BombListIndexPtr",
        "Global.g_ViewMatrix","Game.m_UnitList1","Unit.m_Position",
        "Projectile.m_MissileNameCont","MissileNameCont.m_Type"
    };
    for(const auto key:required) {
        const auto it=remote.entries.find(std::string(key));
        if(it==remote.entries.end() || it->second==0) {
            error="API incompleta: falta offset crítico; conservando copia anterior";
            return false;
        }
    }
    auto next=existing;
    std::size_t updated=0;
    for(const auto& binding:bindings) {
        const auto it=remote.entries.find(binding.remote);
        if(it==remote.entries.end()) continue;
        const auto dest=next.find(binding.local);
        if(dest==next.end()) {error="Registro interno no encontrado";return false;}
        const auto value=it->second;
        const std::string_view group(binding.remote);
        if((group.starts_with("Global.") || group.starts_with("Function.")) ?
           (value<0x10000 || value>0x40000000) : (value>0x100000)) {
            error="Offset remoto fuera de rango; conservando copia anterior";
            return false;
        }
        next[binding.local]=value;
        ++updated;
    }
    if(updated<45) {error="API sin suficientes campos conocidos";return false;}
    mapped=updated;
    output=std::move(next);error.clear();return true;
}
}
