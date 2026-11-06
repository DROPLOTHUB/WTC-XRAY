#pragma once

#include <Windows.h>

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>

// ============================================================================
//   OFFSETS — tabla mas reciente suministrada por el usuario; build exacta no indicada.
//   Integracion comprobada por compilacion, no validada en memoria del juego.
//   Estructura: [SECCIÓN N] para navegación rápida
// ============================================================================

namespace offsets
{
    // ========================================================================
    // [SECCIÓN 1] GLOBALES
    // ========================================================================
    namespace Global
    {
        // Selectores y manager de la tabla global suministrada mas recientemente.
        inline uintptr_t g_RocketListIndexPtr = 0x7A27860;
        inline uintptr_t g_BombListIndexPtr = 0x7A26320;
        inline uintptr_t g_EntityManager = 0x7E8CBE8;
    }

    inline uintptr_t alllistdata_offset = 0x7E8CBE8;
    inline uintptr_t bomb_index = 0x7A26320;
    inline uintptr_t cgame_offset = 0x7A4B848;
    inline uintptr_t hud_offset = 0x7A4AE70;
    inline uintptr_t localplayer_offset = 0x7A22CB0;
    inline uintptr_t my_unit_offset = 0x7A4D7F8;
    inline uintptr_t rocket_index = 0x7A27860;
    inline uintptr_t is_scoping_offset = 0x7A94A6C;
    inline uintptr_t screen_width_offset = 0x7E91950;
    inline uintptr_t view_matrix_offset = 0x7AA58B8;
    inline uintptr_t view_angles_offset = 0x7A50368;

    inline uintptr_t proj_ballistics_props_positive_offset = 0x7A303E0;
    inline uintptr_t proj_ballistics_props_negative_offset = 0x7A30448;
    inline uintptr_t bullet_props_positive_offset = 0x7A39890;
    inline uintptr_t bullet_props_negative_offset = 0x7A398F8;

    inline uintptr_t air_prediction_bool_offset = 0x7EB4120;
    inline uintptr_t game_font_list_offset = 0x7F669A8;
    inline uintptr_t game_optics_offset = 0x7A4D6C8;
    inline uintptr_t localization_info_offset = 0x7E8E520;
    inline uintptr_t map_drawn_this_frame_offset = 0x7A98CAC;
    inline uintptr_t map_full_world_center_offset = 0x7A98CC0;
    inline uintptr_t map_full_world_size_offset = 0x794E7B0;
    inline uintptr_t map_minimap_pos_offset = 0x794E7A0;
    inline uintptr_t map_minimap_size_offset = 0x794E7A8;
    inline uintptr_t map_view_size_offset = 0x794E798;
    inline uintptr_t map_world_center_offset = 0x7A98CB0;
    inline uintptr_t tactical_map_obj_offset = 0x7A76918;

    namespace tactical_map_offsets
    {
        inline uintptr_t aspect = 0x114;
        inline uintptr_t mode = 0x8414;
        inline uintptr_t bounds_min = 0x5174;
        inline uintptr_t bounds_max = 0x517C;
        inline uintptr_t mode_stride = 72;
    }
    inline uintptr_t name_map_offset = 0x7A4A668;
    inline uintptr_t reload_offset1 = 0x7E8CC70;
    inline uintptr_t reload_offset2 = 0x7E8CC68;
    inline uintptr_t reload_offset3 = 0x7A198EC;
    inline uintptr_t reload_offset4 = 0x7E8CD70;
    inline uintptr_t reload_offset5 = 0x7E8CD60;
    inline uintptr_t reload_offset6 = 0x7E8CDF8;
    inline uintptr_t reload_offset7 = 0x7E8CD68;
    inline uintptr_t ui_pos_arr_offset = 0x7A97C68;
    inline uintptr_t function_das_get_usefull_total_time = 0xACA250;

    inline uintptr_t base_game_version_str = 0x7A50F98;
    inline uintptr_t clock_obj = 0x7A251B0;
    inline uintptr_t game_difficulty = 0x79B5D28;
    inline uintptr_t is_in_hangar_offset = 0x7A3704B;
    inline uintptr_t function_rayhit_normalized = 0x3462BC0;

    // ========================================================================
    // [SECCIÓN 2] CGAME
    // ========================================================================
    namespace cgame_offsets
    {
        inline uintptr_t ballistics_offset = 0x3F0;
        inline uintptr_t bases_cont = 0x3D0;
        inline uintptr_t camera_offset = 0x660;
        inline uintptr_t current_map = 0x1F0;
        inline uintptr_t level_owner = 0x3C8;
        inline uintptr_t report_ptr = 0x3E0;
        inline uintptr_t time_offset = 0x2D0;
        inline uintptr_t time_manager = 0x400;
        inline uintptr_t unit_count1 = 0x320;
        inline uintptr_t unit_count2 = 0x338;
        inline uintptr_t unit_count3 = 0x350;
        inline uintptr_t unit_list1 = 0x310;
        inline uintptr_t unit_list2 = 0x328;
        inline uintptr_t unit_list3 = 0x340;

        namespace camera_offsets
        {
            inline uintptr_t camera_base = 0xA0;
            inline uintptr_t camera_matrix_offset = 0x1D8;
            inline uintptr_t camera_position_offset = 0x60;
        }

        namespace ballistic_offsets
        {
            inline uintptr_t bomb_prediction_offset = 0x1CCC;
            inline uintptr_t round_caliber_offset = 0x2150;
            inline uintptr_t round_length_offset = 0x2154;
            inline uintptr_t round_mass_offset = 0x214C;
            inline uintptr_t round_velocity_offset = 0x2140;
            inline uintptr_t max_dist = 0x2158;
            inline uintptr_t telecontrol_offset = 0xCD0;

            inline uintptr_t& velocity = round_velocity_offset;
            inline uintptr_t& mass = round_mass_offset;
            inline uintptr_t& caliber = round_caliber_offset;
            inline uintptr_t& length = round_length_offset;
            inline uintptr_t& bullet_impact_point = bomb_prediction_offset;
            inline uintptr_t& bomb_impact_point = bomb_prediction_offset;

            namespace telecontrol_offsets
            {
                inline uintptr_t gameui_offset = 0x9E0;
            }
        }
    }

    // ========================================================================
    // [SECCIÓN 3] LOCALPLAYER
    // ========================================================================
    namespace localplayer
    {
        inline uintptr_t guiState_offset = 0x640;
        inline uintptr_t localunit_offset = 0x848;
        inline uintptr_t controlledUnitRef_offset = 0x878;
        inline uintptr_t spectatedModel_offset = 0x670;
        inline uintptr_t botName_offset = 0x160;
        inline uintptr_t nik_offset = 0x60;
        inline uintptr_t teamNum_offset = 0x228;
        inline uintptr_t score_offset = 0x8F8;

        inline uintptr_t brokenSlots_offset = 0x570;
        inline uintptr_t cachedIsAutoSquad_offset = 0xB90;
        inline uintptr_t clanTag_offset = 0x188;
        inline uintptr_t classinessMark_offset = 0x610;
        inline uintptr_t countryId_offset = 0x250;
        inline uintptr_t customState_offset = 0x2B0;
        inline uintptr_t decals_offset = 0x200;
        inline uintptr_t disabledByMatchingSlots_offset = 0x540;
        inline uintptr_t dummyForCountUsedSlots_offset = 0x750;
        inline uintptr_t dummyForCrewUnitsList_offset = 0x510;
        inline uintptr_t dummyForFootballStat_offset = 0xAF0;
        inline uintptr_t dummyForKillStreaksProgress_offset = 0x860;
        inline uintptr_t dummyForPlayerStat_offset = 0xAC0;
        inline uintptr_t dummyForRoundScore_offset = 0xA60;
        inline uintptr_t dummyForSpawnCosts_offset = 0x790;
        inline uintptr_t dummyForSpawnDelayTimes_offset = 0x7D0;
        inline uintptr_t dummyForSupportPlanes_offset = 0x440;
        inline uintptr_t forceLockTarget_offset = 0xB68;
        inline uintptr_t forcedRespawn_offset = 0x690;
        inline uintptr_t invitedNickName_offset = 0x138;
        inline uintptr_t memberId_offset = 0x278;
        inline uintptr_t missionSupportUnitEnabled_offset = 0xC10;
        inline uintptr_t missionSupportUnitRef_offset = 0xBE0;
        inline uintptr_t nickFrame_offset = 0xBB8;
        inline uintptr_t nickLocKey_offset = 0x160;
        inline uintptr_t numFreeSpareUsed_offset = 0xC60;
        inline uintptr_t ownedSlots_offset = 0x5E8;
        inline uintptr_t ownedUnitRef_offset = 0x848;
        inline uintptr_t publicFlags_offset = 0x1D8;
        inline uintptr_t rageTokens_offset = 0xC38;
        inline uintptr_t realNick_offset = 0xB18;
        inline uintptr_t soldier_offset = 0x3228;
        inline uintptr_t spareAircraftInSlots_offset = 0x5C0;
        inline uintptr_t squadScore_offset = 0x890;
        inline uintptr_t squadronId_offset = 0xB40;
        inline uintptr_t stateRefl_offset = 0x818;
        inline uintptr_t supportUnitRef_offset = 0x958;
        inline uintptr_t timeToKick_offset = 0x6B8;
        inline uintptr_t timeToRespawnInCoop_offset = 0x668;
        inline uintptr_t timeToRespawn_offset = 0x640;
        inline uintptr_t wasReadySlots_offset = 0x598;
        inline uintptr_t wreckedPartShipUnitRef_offset = 0x988;
        inline uintptr_t nikename = 0xB8;
    }

    // ========================================================================
    // [SECCIÓN 4] UNIT
    // ========================================================================
    namespace unit_offsets
    {
        inline uintptr_t airmovement_offset = 0xD50;
        inline uintptr_t armory_offset = 0x10D0;
        inline uintptr_t turret_offset = 0x10D0;
        inline uintptr_t baseOverloadInfo_offset = 0x10B8;
        inline uintptr_t bbmax_offset = 0x274;
        inline uintptr_t bbmin_offset = 0x268;
        inline uintptr_t bodyBBMax_offset = 0x218C;
        inline uintptr_t bodyBBMin_offset = 0x2180;
        inline uintptr_t briefMalfunctionState_offset = 0xB18;
        inline uintptr_t damageModelCont_offset = 0x10A8;
        inline uintptr_t entityId_offset = 0x88;
        inline uintptr_t info_offset = 0x1028;
        inline uintptr_t invulTimer_offset = 0xE74;
        inline uintptr_t invulnerable_offset = 0xE98;
        inline uintptr_t isBurning_offset = 0x1BF8;
        inline uintptr_t modelTransform_offset = 0x258;   // m_ModelTransform → GeomNodeTree
        inline uintptr_t playerInfo_offset = 0xFA0;
        inline uintptr_t position_offset = 0xD40;
        inline uintptr_t rotation_matrix_offset = 0xD1C;
        inline uintptr_t unitArmyNo_offset = 0x1018;
        inline uintptr_t unitIndex_offset = 0x8;
        inline uintptr_t unit_uid_offset = 0x8;
        inline uintptr_t unitState_offset = 0xF98;
        inline uintptr_t unitType_offset = 0x8C;
        inline uintptr_t visualReloadProgress_offset = 0xAF0;
        inline uintptr_t xrayUnitGate_offset = 0x1467;

        inline uintptr_t unitFlags1_offset = 0x90;
        inline uintptr_t unitFlags2_offset = 0x91;
        inline uintptr_t unitFlags3_offset = 0x92;
        inline uintptr_t unitFlags4_offset = 0x93;

        inline uintptr_t a_7k_bomb_mk81_snakeye_offset = 0x800;
        inline uintptr_t allowBailout_offset = 0xCC0;
        inline uintptr_t armada_01_0_offset = 0x1040;
        inline uintptr_t bombDelayExplosion_offset = 0xB28;
        inline uintptr_t brokenPropFlags_offset = 0x3418;
        inline uintptr_t brokenTurretDriveJammedTime_offset = 0x840;
        inline uintptr_t brokenTurretDriveMult_offset = 0x890;
        inline uintptr_t brokenTurretDriveSpeed_offset = 0x868;
        inline uintptr_t crewStatMult_offset = 0x1C18;
        inline uintptr_t curNightVisionMode_offset = 0x950;
        inline uintptr_t default_offset = 0x20E0;
        inline uintptr_t delayWithFlightTime_offset = 0xB50;
        inline uintptr_t dummyForDeathInfo_offset = 0xBD8;
        inline uintptr_t dummyForUnitFlags_offset = 0xF30;
        inline uintptr_t dummyVarForCrewLayout_offset = 0x980;
        inline uintptr_t dummyVarForMissionAddText_offset = 0x9A8;
        inline uintptr_t enableCiwsGunners_offset = 0xED0;
        inline uintptr_t enableGunners_offset = 0xEA8;
        inline uintptr_t extinguishAssistant_offset = 0xE08;
        inline uintptr_t extinguishAssistee_offset = 0xDD8;
        inline uintptr_t farthestExitZoneId_offset = 0xFC0;
        inline uintptr_t fpvRocketCameraControlEnabled_offset = 0x1110;
        inline uintptr_t fuseModeIndex_offset = 0x1B50;
        inline uintptr_t ground_velocity_offset = 0x2100; // Unit.m_Velocity: puntero, NO vec3_t
        inline uintptr_t& groundmovement_offset = ground_velocity_offset; // getter antiguo: mismo puntero API
        inline uintptr_t groundmovement_velocity_offset = 0x5C; // GroundMovement.m_Velocity
        inline uintptr_t hasModuleEffectsToRepair_offset = 0x8E0;
        inline uintptr_t isAlternativeShotFreq_offset = 0x1F0;
        inline uintptr_t isBreechDamaged_offset = 0x8B8;
        inline uintptr_t isNeedExtinguishHelp_offset = 0x1A70;
        inline uintptr_t isNeedRepairHelp_offset = 0x1A40;
        inline uintptr_t isOnAirfieldAndCanRearmedVal_offset = 0x3AE8;
        inline uintptr_t killer_offset = 0x18F8;
        inline uintptr_t lastBuildingTime_offset = 0xAB0;
        inline uintptr_t lastContactFrom_offset = 0xC10;
        inline uintptr_t lastContactTimeRel_offset = 0xC98;
        inline uintptr_t lastContactTime_offset = 0xC70;
        inline uintptr_t lastContactTo_offset = 0xC40;
        inline uintptr_t lowRateUnitFlags_offset = 0xF58;
        inline uintptr_t maskingFactor_offset = 0x1AF0;
        inline uintptr_t moduleEffectsRepairAtTime_offset = 0x908;
        inline uintptr_t nextUseArtilleryTime_offset = 0x17D8;
        inline uintptr_t persistentAerobaticsSmokeId_offset = 0x33F0;
        inline uintptr_t persistentAerobaticsSmoke_offset = 0x33C8;
        inline uintptr_t pilot_armada_01_0_offset = 0x1EE8;
        inline uintptr_t prepareExtinguishAssistantTime_offset = 0x1768;
        inline uintptr_t prepareRepairAssistantTime_offset = 0x1740;
        inline uintptr_t prepareRepairAssisteeTime_offset = 0x1718;
        inline uintptr_t prepareRepairCooldownsTime_offset = 0x1898;
        inline uintptr_t repairAssistant_offset = 0xDA8;
        inline uintptr_t repairAssistee_offset = 0xD78;
        inline uintptr_t repairCooldowns_offset = 0x1AA0;
        inline uintptr_t repairTimer_offset = 0x1FD0;
        inline uintptr_t rocketFuseDist_offset = 0xB78;
        inline uintptr_t scoutCooldown_offset = 0x13A0;
        inline uintptr_t scoutStartTime_offset = 0x1378;
        inline uintptr_t smokeScreenActived_offset = 0x1BF0;
        inline uintptr_t smokeScreenCount_offset = 0x1BC8;
        inline uintptr_t stealthArmyMask_offset = 0x1088;
        inline uintptr_t stealthRadiusSq_offset = 0x1060;
        inline uintptr_t supportPlaneCatapultsFuseMask_offset = 0xA78;
        inline uintptr_t supportPlanesCount_offset = 0xA50;
        inline uintptr_t timeRepair_offset = 0x1FA8;
        inline uintptr_t timeToNextSmokeScreen_offset = 0x1BA0;
        inline uintptr_t timeToRearm_offset = 0x1FF8;
        inline uintptr_t torpedoDiveDepth_offset = 0xBA0;
        inline uintptr_t vulnerableByUnitUId_offset = 0xF00;
        inline uintptr_t wingBreakRnd_offset = 0x3440;

        namespace airmovement_offsets
        {
            inline uintptr_t velocity_offset = 0x15B0;
            inline uintptr_t acceleration_offset = 0x15C8;
        }
    }

    // ========================================================================
    // [SECCIÓN 5] UNIT_INFO
    // ========================================================================
    namespace unit_info_offsets
    {
        inline uintptr_t have_ccip_for_bombs = 0x459;
        inline uintptr_t have_ccip_for_gun = 0x457;
        inline uintptr_t have_ccip_for_rocket = 0x456;
        inline uintptr_t have_ccip_for_turret = 0x458;
        inline uintptr_t bomber_view = 0x454;
        inline uintptr_t is_nuclear_bomber = 0x5EA;

        inline uintptr_t vehicle_name_offset = 0x28;
        inline uintptr_t unit_type_offset = 0x38;
    }

    // ========================================================================
    // [SECCIÓN 6] ARMORY
    // ========================================================================
    namespace armory_offsets
    {
        inline uintptr_t controllable_weapons = 0x1C8;
        inline uintptr_t controllable_weapons_count = 0x1D8;
        inline uintptr_t weapon_count = 0x330;
        inline uintptr_t weapon_position_info = 0xB8;
        inline uintptr_t weapon_position_info_count = 0xC8;
        inline uintptr_t weapons = 0x320;
    }

    // ========================================================================
    // [SECCIÓN 7] WEAPON
    // ========================================================================
    namespace weapon_offsets
    {
        inline uintptr_t active_slot = 0x5F4;
        inline uintptr_t blocked = 0x2D4;
        inline uintptr_t control_slot = 0x24;
        inline uintptr_t reload_finish_time = 0x610;
        inline uintptr_t reload_start_time = 0x618;
        inline uintptr_t selected_slot = 0x5F5;
        inline uintptr_t weapon_info = 0x3B8;
    }

    // ========================================================================
    // [SECCIÓN 8] WEAPON_INFO
    // ========================================================================
    namespace weapon_info_offsets
    {
        inline uintptr_t ammo = 0xB0;
        inline uintptr_t clip_size = 0x13C;
        inline uintptr_t cur_ammo = 0x12C;
        inline uintptr_t max_ammo = 0x128;
        inline uintptr_t wasted_ammo = 0x130;
    }

    // ========================================================================
    // [SECCIÓN 9] WEAPON_POSITION_INFO
    // ========================================================================
    namespace weapon_position_info_offsets
    {
        inline uintptr_t bone_matrix = 0x0;
        inline uintptr_t position = 0x2A8;
        inline uintptr_t slot_id = 0x2F0;
    }

    // ========================================================================
    // [SECCIÓN 9b] BONE MATRIX
    // ========================================================================
    namespace bone_matrix
    {
        inline uintptr_t m_BoneMatrixLocalRow0 = 0x78;
        inline uintptr_t m_BoneMatrixLocalRow1 = 0x84;
        inline uintptr_t m_BoneMatrixLocalRow2 = 0x90;
        inline uintptr_t m_BoneMatrixLocalTrans = 0x9C;

        inline uintptr_t m_BoneMatrixFwd = 0x110;
        inline uintptr_t m_BoneMatrixUp = 0x11C;
        inline uintptr_t m_BoneMatrixRight = 0x128;
        inline uintptr_t m_BoneMatrixPos = 0x134;
    }

    // ========================================================================
    // [SECCIÓN 10] DAMAGE MODEL — PACKED BVH + SKELETON TREE
    // ========================================================================
    namespace damage_model
    {
        inline uintptr_t m_ValidFlag = 0x78;
        inline uintptr_t m_ModelContainer = 0x60;
        inline uintptr_t m_Header = 0x370;
        inline uintptr_t m_StateArray = 0x400;
        inline uintptr_t m_StateCount = 0x408;
        inline uintptr_t m_StaticProps = 0x3C0;

        inline uintptr_t m_PackedHeader = 0x78;
        inline uintptr_t m_TransformArray = 0xA0;

        inline uintptr_t records_rel = 0x04;
        inline uintptr_t node_count = 0x08;
        inline uintptr_t geometry_rel = 0x24;
        inline uintptr_t names_rel = 0x4C;
        inline uintptr_t names_valid = 0x50;

        inline uintptr_t record_stride = 0x40;
        inline uintptr_t rec_tree_index = 0x04;
        inline uintptr_t rec_bbmin = 0x08;
        inline uintptr_t rec_bbmax = 0x14;
        inline uintptr_t rec_transform_index = 0x24;
        inline uintptr_t rec_vert_count = 0x30;
        inline uintptr_t rec_geometry_rel = 0x34;
        inline uintptr_t rec_geometry_flags = 0x38;
        inline uintptr_t rec_name_offset = 0x3C;

        inline uintptr_t geom_quant_scale = 0x0C;
        inline uintptr_t geom_quant_offset = 0x18;
        inline uintptr_t geom_tree_bytes = 0x24;
        inline uintptr_t geom_root = 0x28;
        inline uintptr_t geom_data = 0x30;

        inline int max_parts = 512;
        inline int max_vertices = 200000;
        inline int max_tris = 200000;
        inline int max_bvh_depth = 64;
        inline int node_budget = 50000;
        inline int cache_ttl_seconds = 60;

        inline uintptr_t geom_node_tree_count = 0x10;
        inline uintptr_t geom_node_tree_base = 0x20;
    }

    // ========================================================================
    // [SECCIÓN 11] PROYECTILES / NOMENCLATURA
    // ========================================================================
    namespace projectile_offsets
    {
        inline uintptr_t missile_name_type = 0x870; // MissileNameCont::m_Type
        inline uintptr_t missile_name_container = 0x720;
        inline uintptr_t bomb_name_container = 0x718;
        inline uintptr_t bomb_name_text = 0x10;
        inline uintptr_t position = 0x2C0;
        inline uintptr_t velocity = 0x2DC;
        inline uintptr_t owner = 0x58;
    }

    // ========================================================================
    // [SECCIÓN 12] REGISTRY
    // ========================================================================
    // Disponibilidad determinada sólo por el manifiesto API validado.
    inline bool api_ground_velocity_ready = false;
    inline bool api_ballistics_ready = false;
    inline bool api_reload_ready = false; // Unit.m_VisualReload del manifiesto privado, no fallback
    inline bool api_invulnerable_ready = false; // Unit.m_Invulnerable presente en API validada
    inline bool api_invul_timer_ready = false; // Unit.m_InvulTimer presente en API validada
    inline bool api_air_velocity_ready = false; // Unit.m_AirContainer + AirContainer.m_Velocity API
    inline bool api_air_acceleration_ready = false; // optional AirContainer.m_Acceleration supplied by API

    struct runtime_offset_entry
    {
        const char* key;
        uintptr_t* value;
    };

    inline auto runtime_offsets_registry() -> const auto&
    {
        static constexpr std::array registry = {
            // [1] GLOBALES
            runtime_offset_entry{"g_RocketListIndexPtr", &offsets::Global::g_RocketListIndexPtr},
            runtime_offset_entry{"g_BombListIndexPtr", &offsets::Global::g_BombListIndexPtr},
            runtime_offset_entry{"g_EntityManager", &offsets::Global::g_EntityManager},
            runtime_offset_entry{"projectile_offsets.missile_name_type", &offsets::projectile_offsets::missile_name_type},
            runtime_offset_entry{"projectile_offsets.missile_name_container", &offsets::projectile_offsets::missile_name_container},
            runtime_offset_entry{"projectile_offsets.bomb_name_container", &offsets::projectile_offsets::bomb_name_container},
            runtime_offset_entry{"projectile_offsets.bomb_name_text", &offsets::projectile_offsets::bomb_name_text},
            runtime_offset_entry{"projectile_offsets.position", &offsets::projectile_offsets::position},
            runtime_offset_entry{"projectile_offsets.velocity", &offsets::projectile_offsets::velocity},
            runtime_offset_entry{"projectile_offsets.owner", &offsets::projectile_offsets::owner},
            runtime_offset_entry{"alllistdata_offset", &offsets::alllistdata_offset},
            runtime_offset_entry{"bomb_index", &offsets::bomb_index},
            runtime_offset_entry{"cgame_offset", &offsets::cgame_offset},
            runtime_offset_entry{"hud_offset", &offsets::hud_offset},
            runtime_offset_entry{"localplayer_offset", &offsets::localplayer_offset},
            runtime_offset_entry{"my_unit_offset", &offsets::my_unit_offset},
            runtime_offset_entry{"rocket_index", &offsets::rocket_index},
            runtime_offset_entry{"is_scoping_offset", &offsets::is_scoping_offset},
            runtime_offset_entry{"screen_width_offset", &offsets::screen_width_offset},
            runtime_offset_entry{"view_matrix_offset", &offsets::view_matrix_offset},
            runtime_offset_entry{"view_angles_offset", &offsets::view_angles_offset},
            runtime_offset_entry{"proj_ballistics_props_positive_offset", &offsets::proj_ballistics_props_positive_offset},
            runtime_offset_entry{"proj_ballistics_props_negative_offset", &offsets::proj_ballistics_props_negative_offset},
            runtime_offset_entry{"bullet_props_positive_offset", &offsets::bullet_props_positive_offset},
            runtime_offset_entry{"bullet_props_negative_offset", &offsets::bullet_props_negative_offset},
            runtime_offset_entry{"air_prediction_bool_offset", &offsets::air_prediction_bool_offset},
            runtime_offset_entry{"game_font_list_offset", &offsets::game_font_list_offset},
            runtime_offset_entry{"game_optics_offset", &offsets::game_optics_offset},
            runtime_offset_entry{"localization_info_offset", &offsets::localization_info_offset},
            runtime_offset_entry{"map_drawn_this_frame_offset", &offsets::map_drawn_this_frame_offset},
            runtime_offset_entry{"map_full_world_center_offset", &offsets::map_full_world_center_offset},
            runtime_offset_entry{"map_full_world_size_offset", &offsets::map_full_world_size_offset},
            runtime_offset_entry{"map_minimap_pos_offset", &offsets::map_minimap_pos_offset},
            runtime_offset_entry{"map_minimap_size_offset", &offsets::map_minimap_size_offset},
            runtime_offset_entry{"map_view_size_offset", &offsets::map_view_size_offset},
            runtime_offset_entry{"map_world_center_offset", &offsets::map_world_center_offset},
            runtime_offset_entry{"tactical_map_obj_offset", &offsets::tactical_map_obj_offset},
            runtime_offset_entry{"tactical_map_offsets.aspect", &offsets::tactical_map_offsets::aspect},
            runtime_offset_entry{"tactical_map_offsets.mode", &offsets::tactical_map_offsets::mode},
            runtime_offset_entry{"tactical_map_offsets.bounds_min", &offsets::tactical_map_offsets::bounds_min},
            runtime_offset_entry{"tactical_map_offsets.bounds_max", &offsets::tactical_map_offsets::bounds_max},
            runtime_offset_entry{"tactical_map_offsets.mode_stride", &offsets::tactical_map_offsets::mode_stride},
            runtime_offset_entry{"name_map_offset", &offsets::name_map_offset},
            runtime_offset_entry{"reload_offset1", &offsets::reload_offset1},
            runtime_offset_entry{"reload_offset2", &offsets::reload_offset2},
            runtime_offset_entry{"reload_offset3", &offsets::reload_offset3},
            runtime_offset_entry{"reload_offset4", &offsets::reload_offset4},
            runtime_offset_entry{"reload_offset5", &offsets::reload_offset5},
            runtime_offset_entry{"reload_offset6", &offsets::reload_offset6},
            runtime_offset_entry{"reload_offset7", &offsets::reload_offset7},
            runtime_offset_entry{"ui_pos_arr_offset", &offsets::ui_pos_arr_offset},
            runtime_offset_entry{"base_game_version_str", &offsets::base_game_version_str},
            runtime_offset_entry{"clock_obj", &offsets::clock_obj},
            runtime_offset_entry{"game_difficulty", &offsets::game_difficulty},
            runtime_offset_entry{"is_in_hangar_offset", &offsets::is_in_hangar_offset},
            runtime_offset_entry{"function_rayhit_normalized", &offsets::function_rayhit_normalized},
            runtime_offset_entry{"function_das_get_usefull_total_time", &offsets::function_das_get_usefull_total_time},

            // [2] CGAME
            runtime_offset_entry{"cgame_offsets.ballistics_offset", &offsets::cgame_offsets::ballistics_offset},
            runtime_offset_entry{"cgame_offsets.bases_cont", &offsets::cgame_offsets::bases_cont},
            runtime_offset_entry{"cgame_offsets.camera_offset", &offsets::cgame_offsets::camera_offset},
            runtime_offset_entry{"cgame_offsets.current_map", &offsets::cgame_offsets::current_map},
            runtime_offset_entry{"cgame_offsets.level_owner", &offsets::cgame_offsets::level_owner},
            runtime_offset_entry{"cgame_offsets.report_ptr", &offsets::cgame_offsets::report_ptr},
            runtime_offset_entry{"cgame_offsets.time_offset", &offsets::cgame_offsets::time_offset},
            runtime_offset_entry{"cgame_offsets.time_manager", &offsets::cgame_offsets::time_manager},
            runtime_offset_entry{"cgame_offsets.unit_count1", &offsets::cgame_offsets::unit_count1},
            runtime_offset_entry{"cgame_offsets.unit_count2", &offsets::cgame_offsets::unit_count2},
            runtime_offset_entry{"cgame_offsets.unit_count3", &offsets::cgame_offsets::unit_count3},
            runtime_offset_entry{"cgame_offsets.unit_list1", &offsets::cgame_offsets::unit_list1},
            runtime_offset_entry{"cgame_offsets.unit_list2", &offsets::cgame_offsets::unit_list2},
            runtime_offset_entry{"cgame_offsets.unit_list3", &offsets::cgame_offsets::unit_list3},
            runtime_offset_entry{"cgame_offsets.camera_offsets.camera_base", &offsets::cgame_offsets::camera_offsets::camera_base},
            runtime_offset_entry{"cgame_offsets.camera_offsets.camera_matrix_offset", &offsets::cgame_offsets::camera_offsets::camera_matrix_offset},
            runtime_offset_entry{"cgame_offsets.camera_offsets.camera_position_offset", &offsets::cgame_offsets::camera_offsets::camera_position_offset},
            runtime_offset_entry{"cgame_offsets.ballistic_offsets.bomb_prediction_offset", &offsets::cgame_offsets::ballistic_offsets::bomb_prediction_offset},
            runtime_offset_entry{"cgame_offsets.ballistic_offsets.round_caliber_offset", &offsets::cgame_offsets::ballistic_offsets::round_caliber_offset},
            runtime_offset_entry{"cgame_offsets.ballistic_offsets.round_length_offset", &offsets::cgame_offsets::ballistic_offsets::round_length_offset},
            runtime_offset_entry{"cgame_offsets.ballistic_offsets.round_mass_offset", &offsets::cgame_offsets::ballistic_offsets::round_mass_offset},
            runtime_offset_entry{"cgame_offsets.ballistic_offsets.round_velocity_offset", &offsets::cgame_offsets::ballistic_offsets::round_velocity_offset},
            runtime_offset_entry{"cgame_offsets.ballistic_offsets.max_dist", &offsets::cgame_offsets::ballistic_offsets::max_dist},
            runtime_offset_entry{"cgame_offsets.ballistic_offsets.telecontrol_offset", &offsets::cgame_offsets::ballistic_offsets::telecontrol_offset},

            // [3] LOCALPLAYER
            runtime_offset_entry{"localplayer.guiState_offset", &offsets::localplayer::guiState_offset},
            runtime_offset_entry{"localplayer.localunit_offset", &offsets::localplayer::localunit_offset},
            runtime_offset_entry{"localplayer.ownedUnitRef_offset", &offsets::localplayer::ownedUnitRef_offset},
            runtime_offset_entry{"localplayer.controlledUnitRef_offset", &offsets::localplayer::controlledUnitRef_offset},
            runtime_offset_entry{"localplayer.spectatedModel_offset", &offsets::localplayer::spectatedModel_offset},
            runtime_offset_entry{"localplayer.botName_offset", &offsets::localplayer::botName_offset},
            runtime_offset_entry{"localplayer.nik_offset", &offsets::localplayer::nik_offset},
            runtime_offset_entry{"localplayer.teamNum_offset", &offsets::localplayer::teamNum_offset},
            runtime_offset_entry{"localplayer.score_offset", &offsets::localplayer::score_offset},

            // [4] UNIT
            runtime_offset_entry{"unit_offsets.airmovement_offset", &offsets::unit_offsets::airmovement_offset},
            runtime_offset_entry{"unit_offsets.armory_offset", &offsets::unit_offsets::armory_offset},
            runtime_offset_entry{"unit_offsets.turret_offset", &offsets::unit_offsets::turret_offset},
            runtime_offset_entry{"unit_offsets.baseOverloadInfo_offset", &offsets::unit_offsets::baseOverloadInfo_offset},
            runtime_offset_entry{"unit_offsets.bbmax_offset", &offsets::unit_offsets::bbmax_offset},
            runtime_offset_entry{"unit_offsets.bbmin_offset", &offsets::unit_offsets::bbmin_offset},
            runtime_offset_entry{"unit_offsets.bodyBBMax_offset", &offsets::unit_offsets::bodyBBMax_offset},
            runtime_offset_entry{"unit_offsets.bodyBBMin_offset", &offsets::unit_offsets::bodyBBMin_offset},
            runtime_offset_entry{"unit_offsets.briefMalfunctionState_offset", &offsets::unit_offsets::briefMalfunctionState_offset},
            runtime_offset_entry{"unit_offsets.damageModelCont_offset", &offsets::unit_offsets::damageModelCont_offset},
            runtime_offset_entry{"unit_offsets.entityId_offset", &offsets::unit_offsets::entityId_offset},
            runtime_offset_entry{"unit_offsets.info_offset", &offsets::unit_offsets::info_offset},
            runtime_offset_entry{"unit_offsets.invulTimer_offset", &offsets::unit_offsets::invulTimer_offset},
            runtime_offset_entry{"unit_offsets.invulnerable_offset", &offsets::unit_offsets::invulnerable_offset},
            runtime_offset_entry{"unit_offsets.isBurning_offset", &offsets::unit_offsets::isBurning_offset},
            runtime_offset_entry{"unit_offsets.modelTransform_offset", &offsets::unit_offsets::modelTransform_offset},
            runtime_offset_entry{"unit_offsets.playerInfo_offset", &offsets::unit_offsets::playerInfo_offset},
            runtime_offset_entry{"unit_offsets.position_offset", &offsets::unit_offsets::position_offset},
            runtime_offset_entry{"unit_offsets.rotation_matrix_offset", &offsets::unit_offsets::rotation_matrix_offset},
            runtime_offset_entry{"unit_offsets.unitArmyNo_offset", &offsets::unit_offsets::unitArmyNo_offset},
            runtime_offset_entry{"unit_offsets.unitIndex_offset", &offsets::unit_offsets::unitIndex_offset},
            runtime_offset_entry{"unit_offsets.unitState_offset", &offsets::unit_offsets::unitState_offset},
            runtime_offset_entry{"unit_offsets.unitType_offset", &offsets::unit_offsets::unitType_offset},
            runtime_offset_entry{"unit_offsets.visualReloadProgress_offset", &offsets::unit_offsets::visualReloadProgress_offset},
            runtime_offset_entry{"unit_offsets.xrayUnitGate_offset", &offsets::unit_offsets::xrayUnitGate_offset},
            runtime_offset_entry{"unit_offsets.unitFlags1_offset", &offsets::unit_offsets::unitFlags1_offset},
            runtime_offset_entry{"unit_offsets.unitFlags2_offset", &offsets::unit_offsets::unitFlags2_offset},
            runtime_offset_entry{"unit_offsets.unitFlags3_offset", &offsets::unit_offsets::unitFlags3_offset},
            runtime_offset_entry{"unit_offsets.unitFlags4_offset", &offsets::unit_offsets::unitFlags4_offset},
            runtime_offset_entry{"unit_offsets.ground_velocity_offset", &offsets::unit_offsets::ground_velocity_offset},
            runtime_offset_entry{"unit_offsets.groundmovement_velocity_offset", &offsets::unit_offsets::groundmovement_velocity_offset},
            runtime_offset_entry{"unit_offsets.airmovement_offsets.velocity_offset", &offsets::unit_offsets::airmovement_offsets::velocity_offset},
            runtime_offset_entry{"unit_offsets.airmovement_offsets.acceleration_offset", &offsets::unit_offsets::airmovement_offsets::acceleration_offset},

            // [5] UNIT_INFO
            runtime_offset_entry{"unit_info_offsets.have_ccip_for_bombs", &offsets::unit_info_offsets::have_ccip_for_bombs},
            runtime_offset_entry{"unit_info_offsets.have_ccip_for_gun", &offsets::unit_info_offsets::have_ccip_for_gun},
            runtime_offset_entry{"unit_info_offsets.have_ccip_for_rocket", &offsets::unit_info_offsets::have_ccip_for_rocket},
            runtime_offset_entry{"unit_info_offsets.have_ccip_for_turret", &offsets::unit_info_offsets::have_ccip_for_turret},
            runtime_offset_entry{"unit_info_offsets.bomber_view", &offsets::unit_info_offsets::bomber_view},
            runtime_offset_entry{"unit_info_offsets.is_nuclear_bomber", &offsets::unit_info_offsets::is_nuclear_bomber},
            runtime_offset_entry{"unit_info_offsets.vehicle_name_offset", &offsets::unit_info_offsets::vehicle_name_offset},
            runtime_offset_entry{"unit_info_offsets.unit_type_offset", &offsets::unit_info_offsets::unit_type_offset},

            // [6] ARMORY
            runtime_offset_entry{"armory_offsets.controllable_weapons", &offsets::armory_offsets::controllable_weapons},
            runtime_offset_entry{"armory_offsets.controllable_weapons_count", &offsets::armory_offsets::controllable_weapons_count},
            runtime_offset_entry{"armory_offsets.weapon_count", &offsets::armory_offsets::weapon_count},
            runtime_offset_entry{"armory_offsets.weapon_position_info", &offsets::armory_offsets::weapon_position_info},
            runtime_offset_entry{"armory_offsets.weapon_position_info_count", &offsets::armory_offsets::weapon_position_info_count},
            runtime_offset_entry{"armory_offsets.weapons", &offsets::armory_offsets::weapons},

            // [7] WEAPON
            runtime_offset_entry{"weapon_offsets.active_slot", &offsets::weapon_offsets::active_slot},
            runtime_offset_entry{"weapon_offsets.blocked", &offsets::weapon_offsets::blocked},
            runtime_offset_entry{"weapon_offsets.control_slot", &offsets::weapon_offsets::control_slot},
            runtime_offset_entry{"weapon_offsets.reload_finish_time", &offsets::weapon_offsets::reload_finish_time},
            runtime_offset_entry{"weapon_offsets.reload_start_time", &offsets::weapon_offsets::reload_start_time},
            runtime_offset_entry{"weapon_offsets.selected_slot", &offsets::weapon_offsets::selected_slot},
            runtime_offset_entry{"weapon_offsets.weapon_info", &offsets::weapon_offsets::weapon_info},

            // [8] WEAPON_INFO
            runtime_offset_entry{"weapon_info_offsets.ammo", &offsets::weapon_info_offsets::ammo},
            runtime_offset_entry{"weapon_info_offsets.clip_size", &offsets::weapon_info_offsets::clip_size},
            runtime_offset_entry{"weapon_info_offsets.cur_ammo", &offsets::weapon_info_offsets::cur_ammo},
            runtime_offset_entry{"weapon_info_offsets.max_ammo", &offsets::weapon_info_offsets::max_ammo},
            runtime_offset_entry{"weapon_info_offsets.wasted_ammo", &offsets::weapon_info_offsets::wasted_ammo},

            // [9] WEAPON_POSITION_INFO
            runtime_offset_entry{"weapon_position_info_offsets.bone_matrix", &offsets::weapon_position_info_offsets::bone_matrix},
            runtime_offset_entry{"weapon_position_info_offsets.position", &offsets::weapon_position_info_offsets::position},
            runtime_offset_entry{"weapon_position_info_offsets.slot_id", &offsets::weapon_position_info_offsets::slot_id},

            // [9b] BONE MATRIX
            runtime_offset_entry{"bone_matrix.m_BoneMatrixLocalRow0", &offsets::bone_matrix::m_BoneMatrixLocalRow0},
            runtime_offset_entry{"bone_matrix.m_BoneMatrixLocalRow1", &offsets::bone_matrix::m_BoneMatrixLocalRow1},
            runtime_offset_entry{"bone_matrix.m_BoneMatrixLocalRow2", &offsets::bone_matrix::m_BoneMatrixLocalRow2},
            runtime_offset_entry{"bone_matrix.m_BoneMatrixLocalTrans", &offsets::bone_matrix::m_BoneMatrixLocalTrans},
            runtime_offset_entry{"bone_matrix.m_BoneMatrixFwd", &offsets::bone_matrix::m_BoneMatrixFwd},
            runtime_offset_entry{"bone_matrix.m_BoneMatrixUp", &offsets::bone_matrix::m_BoneMatrixUp},
            runtime_offset_entry{"bone_matrix.m_BoneMatrixRight", &offsets::bone_matrix::m_BoneMatrixRight},
            runtime_offset_entry{"bone_matrix.m_BoneMatrixPos", &offsets::bone_matrix::m_BoneMatrixPos},

            // [10] DAMAGE MODEL
            runtime_offset_entry{"damage_model.m_ValidFlag", &offsets::damage_model::m_ValidFlag},
            runtime_offset_entry{"damage_model.m_ModelContainer", &offsets::damage_model::m_ModelContainer},
            runtime_offset_entry{"damage_model.m_Header", &offsets::damage_model::m_Header},
            runtime_offset_entry{"damage_model.m_StateArray", &offsets::damage_model::m_StateArray},
            runtime_offset_entry{"damage_model.m_StateCount", &offsets::damage_model::m_StateCount},
            runtime_offset_entry{"damage_model.m_StaticProps", &offsets::damage_model::m_StaticProps},
            runtime_offset_entry{"damage_model.m_PackedHeader", &offsets::damage_model::m_PackedHeader},
            runtime_offset_entry{"damage_model.m_TransformArray", &offsets::damage_model::m_TransformArray},
            runtime_offset_entry{"damage_model.records_rel", &offsets::damage_model::records_rel},
            runtime_offset_entry{"damage_model.node_count", &offsets::damage_model::node_count},
            runtime_offset_entry{"damage_model.geometry_rel", &offsets::damage_model::geometry_rel},
            runtime_offset_entry{"damage_model.names_rel", &offsets::damage_model::names_rel},
            runtime_offset_entry{"damage_model.names_valid", &offsets::damage_model::names_valid},
            runtime_offset_entry{"damage_model.record_stride", &offsets::damage_model::record_stride},
            runtime_offset_entry{"damage_model.rec_tree_index", &offsets::damage_model::rec_tree_index},
            runtime_offset_entry{"damage_model.rec_bbmin", &offsets::damage_model::rec_bbmin},
            runtime_offset_entry{"damage_model.rec_bbmax", &offsets::damage_model::rec_bbmax},
            runtime_offset_entry{"damage_model.rec_transform_index", &offsets::damage_model::rec_transform_index},
            runtime_offset_entry{"damage_model.rec_vert_count", &offsets::damage_model::rec_vert_count},
            runtime_offset_entry{"damage_model.rec_geometry_rel", &offsets::damage_model::rec_geometry_rel},
            runtime_offset_entry{"damage_model.rec_geometry_flags", &offsets::damage_model::rec_geometry_flags},
            runtime_offset_entry{"damage_model.rec_name_offset", &offsets::damage_model::rec_name_offset},
            runtime_offset_entry{"damage_model.geom_quant_scale", &offsets::damage_model::geom_quant_scale},
            runtime_offset_entry{"damage_model.geom_quant_offset", &offsets::damage_model::geom_quant_offset},
            runtime_offset_entry{"damage_model.geom_tree_bytes", &offsets::damage_model::geom_tree_bytes},
            runtime_offset_entry{"damage_model.geom_root", &offsets::damage_model::geom_root},
            runtime_offset_entry{"damage_model.geom_data", &offsets::damage_model::geom_data},
            runtime_offset_entry{"damage_model.geom_node_tree_count", &offsets::damage_model::geom_node_tree_count},
            runtime_offset_entry{"damage_model.geom_node_tree_base", &offsets::damage_model::geom_node_tree_base},
        };

        return registry;
    }

    // ========================================================================
    // [SECCIÓN 12] HELPERS
    // ========================================================================
    inline auto restore_default_offsets() -> void
    {
        const auto& defaults = []() -> const std::unordered_map<std::string, uintptr_t>&
            {
                static const auto snapshot = []()
                    {
                        std::unordered_map<std::string, uintptr_t> values;
                        values.reserve(runtime_offsets_registry().size());
                        for (const auto& entry : runtime_offsets_registry())
                            values.emplace(entry.key, *entry.value);
                        return values;
                    }();
                return snapshot;
            }();

        for (const auto& entry : runtime_offsets_registry())
        {
            const auto it = defaults.find(entry.key);
            if (it != defaults.end())
                *entry.value = it->second;
        }
    }


}
