param([string]$StartAt = '')
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (!(Test-Path $vswhere)) { throw 'Visual Studio Installer / vswhere.exe not found.' }
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Install Visual Studio C++ x64 build tools.' }
$vcvars = Join-Path $vs 'VC/Auxiliary/Build/vcvars64.bat'
$project = Split-Path $PSScriptRoot -Parent
$output = Join-Path $project 'build'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$names = @('ground_prediction_tests','g2g_runtime_tests','coverage_opacity_tests','lead_marker_visual_tests','hotkey_validation_tests','config_validation_tests','offsets_supplied_tests','offset_credentials_tests','offset_api_payload_tests','offset_api_mapping_tests','offset_auto_install_tests','radar_geometry_tests','radar_symbols_tests','radar_runtime_tests','air_ghost_filter_tests','xray_boxes_tests','manual_save_tests','ballistics_read_tests','startup_pointer_tests','projectile_ecs_selector_tests','selection_filters_tests','esp_frame_policy_tests','overlay_profile_position_tests','missile_registry_storage_tests','menu_multimonitor_tests','scatter_read_recovery_tests','unit_type_filter_tests','esp_reload_display_tests','xray_wireframe_only_tests','invulnerability_display_tests','camera_batch_tests','adjacent_pointer_fields_tests','app_version_tests','a2a_lead_prediction_tests','aircraft_ballistics_policy_tests','a2a_ammo_diagnostics_tests','legacy_a2a_only_tests','preset_bindings_startup_tests','air_lead_audit_tests','aircraft_ammo_trace_tests','render_work_policy_tests','adjacent_value_fields_tests','render_demand_tests','reusable_snapshot_tests','air_motion_tracking_tests','air_turn_integration_tests','a2a_vector_intercept_tests','g2a_lead_tests','air_cache_tests','missile_categories_tests','menu_organization_tests','quality_diagnostics_tests','menu_preferences_tests','menu_language_tests','runtime_language_catalog_tests','runtime_language_tests','console_language_tests','missile_and_preferences_tests')
$startIndex = 0
if ($StartAt) {
    $startIndex = [Array]::IndexOf($names,$StartAt)
    if ($startIndex -lt 0) { throw 'Unknown StartAt suite name.' }
}
for ($index=$startIndex; $index -lt $names.Count; ++$index) {
    $name = $names[$index]
    $source = Join-Path $PSScriptRoot "$name.cpp"
    $exe = Join-Path $output "$name.exe"
    $obj = Join-Path $output "$name.obj"
    $command = 'call "{0}" >nul && cl /nologo /std:c++20 /EHsc /O2 /W4 /utf-8 "{1}" /Fe:"{2}" /Fo:"{3}"' -f $vcvars,$source,$exe,$obj
    & $env:COMSPEC /d /c $command
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
exit 0
