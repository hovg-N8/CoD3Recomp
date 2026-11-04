#include "settings.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

#include <Windows.h>

namespace
{
    std::mutex g_mutex;
    Settings::Values g_values;
    std::filesystem::path g_file;

    const char* const ActionNames[Settings::ActionCount] = {
        "Fire", "Aim", "Jump", "Crouch", "Use", "Reload", "Weapon", "Melee", "Sprint",
        "Grenade", "Smoke", "Objectives", "Pause", "Forward", "Backward", "Left", "Right",
        "Binoculars out", "Binoculars away",
    };
    const char* const ActionKeys[Settings::ActionCount] = {
        "fire", "aim", "jump", "crouch", "use", "reload", "weapon", "melee", "sprint",
        "grenade", "smoke", "objectives", "pause", "forward", "backward", "left", "right",
        "binoculars_out", "binoculars_away",
    };

    std::string Trim(const std::string& text)
    {
        size_t a = 0, b = text.size();
        while (a < b && isspace((unsigned char)text[a])) a++;
        while (b > a && isspace((unsigned char)text[b - 1])) b--;
        return text.substr(a, b - a);
    }
}

const char* Settings::ActionName(Action action)
{
    return action >= 0 && action < ActionCount ? ActionNames[action] : "?";
}

const char* Settings::KeyName(int key)
{
    static char name[64];
    switch (key)
    {
    case 0: return "-";
    case VK_LBUTTON: return "Mouse 1";
    case VK_RBUTTON: return "Mouse 2";
    case VK_MBUTTON: return "Mouse 3";
    case VK_XBUTTON1: return "Mouse 4";
    case VK_XBUTTON2: return "Mouse 5";
    case VK_SPACE: return "Space";
    case VK_RETURN: return "Enter";
    case VK_ESCAPE: return "Escape";
    case VK_TAB: return "Tab";
    case VK_SHIFT: return "Shift";
    case VK_LSHIFT: return "Left Shift";
    case VK_RSHIFT: return "Right Shift";
    case VK_CONTROL: return "Ctrl";
    case VK_LCONTROL: return "Left Ctrl";
    case VK_RCONTROL: return "Right Ctrl";
    case VK_MENU: return "Alt";
    case VK_UP: return "Up";
    case VK_DOWN: return "Down";
    case VK_LEFT: return "Left";
    case VK_RIGHT: return "Right";
    case VK_BACK: return "Backspace";
    case VK_CAPITAL: return "Caps Lock";
    default: break;
    }
    if ((key >= '0' && key <= '9') || (key >= 'A' && key <= 'Z')) { name[0] = char(key); name[1] = 0; return name; }
    if (key >= VK_F1 && key <= VK_F12) { snprintf(name, sizeof(name), "F%d", key - VK_F1 + 1); return name; }
    if (key >= VK_NUMPAD0 && key <= VK_NUMPAD9) { snprintf(name, sizeof(name), "Num %d", key - VK_NUMPAD0); return name; }
    // Anything else by what the keyboard layout calls it.
    const UINT scan = MapVirtualKeyW(UINT(key), MAPVK_VK_TO_VSC);
    wchar_t wide[64] = {};
    if (scan != 0 && GetKeyNameTextW(LONG(scan << 16), wide, 63) > 0)
    {
        WideCharToMultiByte(CP_UTF8, 0, wide, -1, name, sizeof(name), nullptr, nullptr);
        return name;
    }
    snprintf(name, sizeof(name), "Key %d", key);
    return name;
}

Settings::Values Settings::Defaults()
{
    Values v;
    v.keys[Fire] = VK_LBUTTON;
    v.keys[Aim] = VK_RBUTTON;
    v.keys[Jump] = VK_SPACE;
    v.keys[Crouch] = 'C';
    v.keys[Use] = 'E';
    v.keys[Reload] = 'R';
    v.keys[Weapon] = 'F';
    v.keys[Melee] = 'V';
    v.keys[Sprint] = VK_SHIFT;
    for (int i = 0; i < Values::CustomClasses; i++) snprintf(v.className[i], sizeof(v.className[i]), "CUSTOM %d", i + 1);
    v.keys[Grenade] = 'G';
    v.keys[Smoke] = '4';
    v.keys[Objectives] = VK_TAB;
    v.keys[Pause] = VK_ESCAPE;
    v.keys[Forward] = 'W';
    v.keys[Backward] = 'S';
    v.keys[Left] = 'A';
    v.keys[Right] = 'D';
    v.keys[BinocularsOut] = 'B';
    v.keys[BinocularsAway] = 'N';
    return v;
}

Settings::Values Settings::Get()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_values;
}

void Settings::Set(const Values& values)
{
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_values = values;
    }
    Blood::Configure(values.blood);
    Save();
}

void Settings::Preview(const Values& values)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_values = values;
    Blood::Configure(values.blood);
}
void Settings::Load(const std::filesystem::path& exeDirectory)
{
    g_file = exeDirectory / "CoD3Recomp.ini";
    Values v = Defaults();
    if (FILE* file = _wfopen(g_file.c_str(), L"rb"))
    {
        char line[512];
        while (fgets(line, sizeof(line), file))
        {
            const std::string text = Trim(line);
            if (text.empty() || text[0] == '#' || text[0] == ';' || text[0] == '[') continue;
            const size_t eq = text.find('=');
            if (eq == std::string::npos) continue;
            const std::string key = Trim(text.substr(0, eq)), value = Trim(text.substr(eq + 1));
            if (key == "renderer") v.renderer = atoi(value.c_str());
            else if (key == "resolution")
            {
                // "1920x1080", or "desktop".
                int w = 0, h = 0;
                if (sscanf(value.c_str(), "%dx%d", &w, &h) == 2 && w > 0 && h > 0) { v.resolutionWidth = w; v.resolutionHeight = h; }
            }
            else if (key == "resolution_scale") v.resolutionScale = atoi(value.c_str());
            else if (key == "texture_filter") v.textureFilter = atoi(value.c_str());
            else if (key == "anisotropy") v.anisotropy = atoi(value.c_str());
            else if (key == "vsync") v.vsync = atoi(value.c_str()) != 0;
            else if (key == "unlock_frame_rate") v.unlockFrameRate = atoi(value.c_str()) != 0;
            else if (key == "frame_cap") v.frameCap = std::clamp(atoi(value.c_str()), 0, 1000);
            else if (key == "antialiasing") v.antialiasing = atoi(value.c_str());
            else if (key == "texture_quality") v.textureQuality = atoi(value.c_str());
            else if (key == "fov") v.fov = atoi(value.c_str());
            else if (key == "brightness") v.brightness = std::clamp(atoi(value.c_str()), 50, 150);
            else if (key == "model_detail") v.modelDetail = atoi(value.c_str());
            else if (key == "fog") v.fog = atoi(value.c_str()) != 0;
            else if (key == "draw_distance") v.drawDistance = atoi(value.c_str());
            else if (key == "effects") v.effects = atoi(value.c_str());
            else if (key == "dynamic_lights") v.dynamicLights = atoi(value.c_str()) != 0;
            // aim_blur, the key before this one, is left unread: it was on by
            // default, and a file saved then would keep the ghost outline.
            else if (key == "aim_depth_of_field") v.aimBlur = atoi(value.c_str()) != 0;
            else if (key == "console_depth") v.consoleDepth = atoi(value.c_str()) != 0;
            else if (key == "sharp_shadows") v.sharpShadows = atoi(value.c_str()) != 0;
            else if (key == "full_res_effects") v.effectsResolution = atoi(value.c_str()) != 0 ? 2 : 0;   // the older on/off key
            else if (key == "effects_resolution") v.effectsResolution = atoi(value.c_str());
            else if (key == "foliage_coverage") v.foliageCoverage = atoi(value.c_str()) != 0;
            else if (key == "fast_shadow_maps") v.fastShadowMaps = atoi(value.c_str()) != 0;
            else if (key == "steady_decals") v.steadyDecals = atoi(value.c_str()) != 0;
            else if (key == "native_commands") v.nativeCommands = atoi(value.c_str()) != 0;
            else if (key == "watch_writes") v.watchWrites = atoi(value.c_str()) != 0;
            else if (key == "film_video_levels") v.filmVideoLevels = atoi(value.c_str()) != 0;
            else if (key == "opt_depth_thread2") v.depthThread = atoi(value.c_str()) != 0;   // 0.5.341: new name, so a 0.5.340 file's 1 is not kept
            else if (key == "opt_fast_constants") v.fastConstants = atoi(value.c_str()) != 0;
            else if (key == "opt_shader_load_cache") v.shaderLoadCache = atoi(value.c_str()) != 0;
            else if (key == "opt_strong_change_check") v.strongChangeCheck = atoi(value.c_str()) != 0;
            else if (key == "opt_range_looks") v.rangeLooks = atoi(value.c_str()) != 0;
            else if (key == "opt_opaque_shaders") v.opaqueShaders = atoi(value.c_str()) != 0;
            else if (key == "opt_staging_uploads") v.stagingUploads = atoi(value.c_str()) != 0;
            else if (key == "opt_texture_pool") v.texturePool = atoi(value.c_str()) != 0;
            else if (key == "hdr") v.hdr = atoi(value.c_str()) != 0;
            else if (key == "hdr_tone_mapper") v.hdrToneMapper = std::clamp(atoi(value.c_str()), 0, 7);
            else if (key == "hdr_peak_nits") v.hdrPeakNits = std::clamp(float(atof(value.c_str())), 0.0f, 10000.0f);
            else if (key == "hdr_game_nits") v.hdrGameNits = std::clamp(float(atof(value.c_str())), 48.0f, 1000.0f);
            else if (key == "hdr_ui_nits") v.hdrUiNits = std::clamp(float(atof(value.c_str())), 48.0f, 1000.0f);
            else if (key == "hdr_gamma_correction") v.hdrGammaCorrection = std::clamp(atoi(value.c_str()), 0, 2);
            else if (key == "hdr_exposure") v.hdrExposure = std::clamp(float(atof(value.c_str())), 0.0f, 4.0f);
            else if (key == "hdr_highlights") v.hdrHighlights = std::clamp(float(atof(value.c_str())), 0.0f, 100.0f);
            else if (key == "hdr_shadows") v.hdrShadows = std::clamp(float(atof(value.c_str())), 0.0f, 100.0f);
            else if (key == "hdr_contrast") v.hdrContrast = std::clamp(float(atof(value.c_str())), 0.0f, 100.0f);
            else if (key == "hdr_saturation") v.hdrSaturation = std::clamp(float(atof(value.c_str())), 0.0f, 100.0f);
            else if (key == "hdr_highlight_saturation") v.hdrHighlightSaturation = std::clamp(float(atof(value.c_str())), 0.0f, 100.0f);
            else if (key == "hdr_blowout") v.hdrBlowout = std::clamp(float(atof(value.c_str())), 0.0f, 100.0f);
            else if (key == "hdr_flare") v.hdrFlare = std::clamp(float(atof(value.c_str())), 0.0f, 100.0f);
            else if (key == "hdr_hue_correction") v.hdrHueCorrection = std::clamp(float(atof(value.c_str())), 0.0f, 100.0f);
            else if (key == "hdr_cone_response") v.hdrConeResponse = std::clamp(float(atof(value.c_str())), 0.0f, 100.0f);
            else if (key == "hdr_bloom") v.hdrBloom = std::clamp(float(atof(value.c_str())), 0.0f, 100.0f);
            else if (key == "hdr_tv_ramp")
            {
                // Was on or off (1 or 0); now how much of it, 0 to 100.
                const float ramp = float(atof(value.c_str()));
                v.hdrTvRamp = ramp == 1.0f ? 100.0f : std::clamp(ramp, 0.0f, 100.0f);
            }
            else if (key == "hdr_hud_match") v.hdrHudMatch = atoi(value.c_str()) != 0;
            else if (key == "hdr_film_upgrade") v.hdrFilmUpgrade = atoi(value.c_str()) != 0;
            else if (key == "window_mode") v.windowMode = atoi(value.c_str());
            else if (key == "fps_overlay") v.fpsOverlay = atoi(value.c_str()) != 0;
            else if (key == "stats_overlay") v.statsOverlay = atoi(value.c_str()) != 0;
            else if (key == "aim_assist") v.aimAssist = atoi(value.c_str()) != 0;
            else if (key == "bounces") v.bounces = atoi(value.c_str()) != 0;
            else if (key == "quick_landing") v.quickLanding = atoi(value.c_str()) != 0;
            else if (key == "quick_sprint") v.quickSprint = atoi(value.c_str()) != 0;
            else if (key == "custom_primary") v.customPrimary = atoi(value.c_str());
            else if (key == "active_class") v.activeClass = atoi(value.c_str());
            else if (key == "two_grenades") v.twoGrenades = atoi(value.c_str()) != 0;
            else if (key == "faster_ads") v.fasterAds = atoi(value.c_str()) != 0;
            else if (key == "campaign_test") v.campaignTest = atoi(value.c_str());
            else if (key == "campaign_guns") v.campaignGuns = atoi(value.c_str()) != 0;
            else if (key == "big_memory") v.bigMemory = atoi(value.c_str()) != 0;
            else if (key == "campaign_sounds") v.campaignSounds = atoi(value.c_str()) != 0;
            else if (key == "campaign_gunshots") v.campaignGunshots = atoi(value.c_str()) != 0;
            else if (key == "campaign_icons") v.campaignIcons = atoi(value.c_str()) != 0;
            else if (key == "campaign_shot_sounds") v.campaignShotSounds = atoi(value.c_str()) != 0;
            else if (key == "campaign_shot_events") v.campaignShotEvents = std::clamp(atoi(value.c_str()), 0, 15);
            else if (key == "ads_in_air") v.adsInAir = atoi(value.c_str()) != 0;
            else if (key == "sten_reload") v.stenReload = std::clamp(float(atof(value.c_str())), 0.0f, 10.0f);
            else if (key == "fg42_reload") v.fg42Reload = std::clamp(float(atof(value.c_str())), 0.0f, 10.0f);
            else if (key == "fg42_ads_fov") v.fg42AdsFov = std::clamp(float(atof(value.c_str())), 0.0f, 90.0f);
            else if (key == "sten_ads_fov") v.stenAdsFov = std::clamp(float(atof(value.c_str())), 0.0f, 90.0f);
            else if (key == "bren_ads_fov") v.brenAdsFov = std::clamp(float(atof(value.c_str())), 0.0f, 90.0f);
            else if (key == "enfield_ads_fov") v.enfieldAdsFov = std::clamp(float(atof(value.c_str())), 0.0f, 90.0f);
            else if (key == "bren_reload") v.brenReload = std::clamp(float(atof(value.c_str())), 0.0f, 10.0f);
            else if (key == "g43_reload") v.g43Reload = std::clamp(float(atof(value.c_str())), 0.0f, 10.0f);
            else if (key == "mg34_no_deploy") v.mg34NoDeploy = atoi(value.c_str()) != 0;
            else if (key == "extra_map_memory") v.extraMapMemory = std::clamp(atoi(value.c_str()), 0, 18);
            else if (key == "launcher") v.launcher = atoi(value.c_str()) != 0;
            else if (key == "texture_dump") v.textureDump = atoi(value.c_str()) != 0;
            else if (key == "texture_replace") v.textureReplace = atoi(value.c_str()) != 0;
            else if (key == "max_players") v.maxPlayers = atoi(value.c_str()) >= 32 ? 32 : 24;
            else if (key == "binoculars") v.binoculars = atoi(value.c_str()) != 0;
            else if (key == "killstreak_artillery") v.killstreakArtillery = atoi(value.c_str()) != 0;
            else if (key == "killstreak_kills" || key == "killstreak_artillery_kills") v.killstreakKills = std::clamp(atoi(value.c_str()), 1, 30);
            else if (key == "killstreak_uav_kills") v.killstreakUav = std::clamp(atoi(value.c_str()), 1, 30);
            else if (key == "killstreak_typhoon_kills") v.killstreakTyphoon = std::clamp(atoi(value.c_str()), 1, 30);
            else if (key == "uav_seconds") v.uavSeconds = std::clamp(atoi(value.c_str()), 5, 120);
            else if (key == "uav_plane") v.uavPlane = atoi(value.c_str()) != 0;
            else if (key == "four_secondary") v.fourSecondary = atoi(value.c_str()) != 0;
            else if (key == "aim_walk") v.aimWalk = atoi(value.c_str());
            else if (key == "sniper_walk") v.sniperWalk = atoi(value.c_str());
            else if (key == "sniper_sway") v.sniperSway = atoi(value.c_str());
            else if (key == "bot_vehicles") v.botVehicles = atoi(value.c_str()) != 0;
            else if (key == "ability_recharge") v.abilityRecharge = atoi(value.c_str());
            else if (key.compare(0, 6, "class_") == 0 && key.size() > 7)
            {
                // class_<n>_name, class_<n>_primary, class_<n>_sidearm
                const int n = key[6] - '1';
                if (n >= 0 && n < Values::CustomClasses)
                {
                    const std::string field = key.substr(8);
                    if (field == "name") { strncpy(v.className[n], value.c_str(), sizeof(v.className[n]) - 1); v.className[n][sizeof(v.className[n]) - 1] = 0; }
                    else if (field == "primary") v.classPrimary[n] = atoi(value.c_str());
                    else if (field == "sidearm") v.classSidearm[n] = atoi(value.c_str());
                    else if (field == "base") v.classBase[n] = atoi(value.c_str());
                    else if (field == "grenade_type") v.classGrenadeType[n] = atoi(value.c_str());
                    else if (field == "grenades") v.classGrenades[n] = atoi(value.c_str());
                }
            }
            else if (key == "direct_mouse") v.directMouse = atoi(value.c_str()) != 0;
            else if (key == "blood_enabled") v.blood.enabled = atoi(value.c_str()) != 0;
            else if (key == "blood_amount") v.blood.amount = atoi(value.c_str());
            else if (key == "blood_density") v.blood.density = atoi(value.c_str());
            else if (key == "blood_cloud_size") v.blood.cloudSize = float(atof(value.c_str()));
            else if (key == "blood_cloud_opacity") v.blood.cloudOpacity = float(atof(value.c_str()));
            else if (key == "blood_size") v.blood.size = float(atof(value.c_str()));
            else if (key == "blood_decals") v.blood.decals = atoi(value.c_str()) != 0;
            else if (key == "blood_physics") v.blood.physics = atoi(value.c_str()) != 0;
            else if (key == "blood_persistence") v.blood.persistence = atoi(value.c_str());
            else if (key == "blood_max_decals") v.blood.maxDecals = atoi(value.c_str());
            else if (key == "blood_red") v.blood.color[0] = float(atof(value.c_str()));
            else if (key == "blood_green") v.blood.color[1] = float(atof(value.c_str()));
            else if (key == "blood_blue") v.blood.color[2] = float(atof(value.c_str()));
            else if (key == "hitmarkers") v.hitmarkers = atoi(value.c_str()) != 0;
            else if (key == "hitmarker_sound") v.hitmarkerSound = atoi(value.c_str()) != 0;
            else if (key == "hitmarker_red_kill") v.hitmarkerRedKill = atoi(value.c_str()) != 0;
            else if (key == "hitmarker_size") v.hitmarkerSize = atoi(value.c_str());
            else if (key == "hitmarker_opacity") v.hitmarkerOpacity = atoi(value.c_str());
            else if (key == "hitmarker_duration") v.hitmarkerDuration = atoi(value.c_str());
            else if (key == "hitmarker_volume") v.hitmarkerVolume = atoi(value.c_str());
            else if (key == "toggle_sprint") v.toggleSprint = atoi(value.c_str()) != 0;
            else if (key == "shell_shock_blur") v.shellShockBlur = atoi(value.c_str()) != 0;
            else if (key == "melee_lunge") v.meleeLunge = atoi(value.c_str()) != 0;
            else if (key == "controller") v.controller = atoi(value.c_str()) != 0;
            else if (key == "player_name") { strncpy(v.playerName, value.c_str(), sizeof(v.playerName) - 1); v.playerName[sizeof(v.playerName) - 1] = 0; }
            else if (key == "lan_address") { strncpy(v.lanAddress, value.c_str(), sizeof(v.lanAddress) - 1); v.lanAddress[sizeof(v.lanAddress) - 1] = 0; }
            else if (key == "system_link_peers") { strncpy(v.systemLinkPeers, value.c_str(), sizeof(v.systemLinkPeers) - 1); v.systemLinkPeers[sizeof(v.systemLinkPeers) - 1] = 0; }
            else if (key == "master_volume") v.masterVolume = atoi(value.c_str());
            else if (key == "native_volume") v.nativeVolume = atoi(value.c_str());
            else if (key == "controller_horizontal") v.controllerHorizontal = atoi(value.c_str());
            else if (key == "controller_vertical") v.controllerVertical = atoi(value.c_str());
            else if (key == "rumble") v.rumble = atoi(value.c_str()) != 0;
            else if (key == "stick_deadzone") v.stickDeadzone = atoi(value.c_str());
            else if (key == "stick_vertical_scale") v.stickVerticalScale = atoi(value.c_str());
            else if (key == "audio_channels") v.audioChannels = atoi(value.c_str());
            else if (key == "mouse_sensitivity") v.mouseSensitivity = float(atof(value.c_str()));
            else if (key == "raw_mouse") v.rawMouse = atoi(value.c_str()) != 0;
            else if (key.compare(0, 4, "key_") == 0)
            {
                for (int i = 0; i < ActionCount; i++)
                    if (key.compare(4, std::string::npos, ActionKeys[i]) == 0) v.keys[i] = atoi(value.c_str());
            }
        }
        fclose(file);
    }
    Blood::Clamp(v.blood);
    v.hitmarkerSize = std::clamp(v.hitmarkerSize, 16, 160);
    v.hitmarkerOpacity = std::clamp(v.hitmarkerOpacity, 0, 100);
    v.hitmarkerDuration = std::clamp(v.hitmarkerDuration, 50, 1000);
    v.hitmarkerVolume = std::clamp(v.hitmarkerVolume, 0, 100);
    if (v.renderer < 0 || v.renderer > 2) v.renderer = 0;
    if (v.resolutionWidth < 320 || v.resolutionHeight < 240 || v.resolutionWidth > 16384 || v.resolutionHeight > 16384) { v.resolutionWidth = 0; v.resolutionHeight = 0; }
    if (v.resolutionScale < 25 || v.resolutionScale > 200) v.resolutionScale = 100;
    if (v.textureFilter < 0 || v.textureFilter > 3) v.textureFilter = 3;
    if (v.anisotropy < 2 || v.anisotropy > 16) v.anisotropy = 16;
    if (v.antialiasing < 0 || v.antialiasing > 5) v.antialiasing = 3;
    if (v.textureQuality < 0 || v.textureQuality > 2) v.textureQuality = 2;
    if (v.fov < 65 || v.fov > 100) v.fov = 65;
    if (v.modelDetail < 0 || v.modelDetail > 1) v.modelDetail = 1;
    if (v.drawDistance < 0 || v.drawDistance > 3) v.drawDistance = 0;
    if (v.effects < 0 || v.effects > 2) v.effects = 0;
    if (v.customPrimary < 0 || v.customPrimary > 13) v.customPrimary = 0;
    if (v.activeClass < 0 || v.activeClass > Values::CustomClasses) v.activeClass = 0;
    if (v.aimWalk < 40 || v.aimWalk > 100) v.aimWalk = 80;
    if (v.sniperWalk < 15 || v.sniperWalk > 100) v.sniperWalk = 30;
    if (v.sniperSway < 0 || v.sniperSway > 100) v.sniperSway = 50;
    if (v.abilityRecharge < 10 || v.abilityRecharge > 100) v.abilityRecharge = 50;
    for (int i = 0; i < Values::CustomClasses; i++)
    {
        if (v.classPrimary[i] < 0 || v.classPrimary[i] > 21) v.classPrimary[i] = 0;
        if (v.classSidearm[i] < 0 || v.classSidearm[i] > 23) v.classSidearm[i] = 0;
        if (v.classBase[i] < 0 || v.classBase[i] > 7) v.classBase[i] = 0;
        if (v.classGrenadeType[i] < 0 || v.classGrenadeType[i] > 4) v.classGrenadeType[i] = 0;
        if (v.classGrenades[i] < 0 || v.classGrenades[i] > 4) v.classGrenades[i] = 0;
        if (!v.className[i][0]) snprintf(v.className[i], sizeof(v.className[i]), "CUSTOM %d", i + 1);
    }
    if (v.windowMode < 0 || v.windowMode > 1) v.windowMode = 0;
    if (v.masterVolume < 0 || v.masterVolume > 100) v.masterVolume = 70;
    if (v.nativeVolume < 0 || v.nativeVolume > 100) v.nativeVolume = 50;
    if (v.controllerHorizontal < 10 || v.controllerHorizontal > 200) v.controllerHorizontal = 100;
    if (v.controllerVertical < 10 || v.controllerVertical > 200) v.controllerVertical = 100;
    if (v.stickDeadzone < 0 || v.stickDeadzone > 50) v.stickDeadzone = 15;
    if (v.stickVerticalScale < 50 || v.stickVerticalScale > 300) v.stickVerticalScale = 200;
    if (v.audioChannels < 0 || v.audioChannels > 2) v.audioChannels = 0;
    if (v.effectsResolution < 0 || v.effectsResolution > 2) v.effectsResolution = 0;
    if (!(v.mouseSensitivity >= 0.1f && v.mouseSensitivity <= 3.0f)) v.mouseSensitivity = 1.0f;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_values = v;
    Blood::Configure(v.blood);
}

void Settings::Save()
{
    Values v = Get();
    if (g_file.empty()) return;
    if (FILE* file = _wfopen(g_file.c_str(), L"wb"))
    {
        fprintf(file, "# Call of Duty 3 recompiled: the settings menu (F11) keeps its values here.\n");
        char resolution[32];
        if (v.resolutionWidth > 0 && v.resolutionHeight > 0) snprintf(resolution, sizeof(resolution), "%dx%d", v.resolutionWidth, v.resolutionHeight);
        else snprintf(resolution, sizeof(resolution), "desktop");
        fprintf(file, "[graphics]\nrenderer = %d\nresolution = %s\nresolution_scale = %d\ntexture_filter = %d\nanisotropy = %d\nvsync = %d\nunlock_frame_rate = %d\nframe_cap = %d\nantialiasing = %d\ntexture_quality = %d\nfov = %d\nmodel_detail = %d\naim_depth_of_field = %d\nconsole_depth = %d\nnative_commands = %d\nwatch_writes = %d\nwindow_mode = %d\nfps_overlay = %d\nstats_overlay = %d\n",
            v.renderer, resolution, v.resolutionScale, v.textureFilter, v.anisotropy, v.vsync ? 1 : 0, v.unlockFrameRate ? 1 : 0, v.frameCap, v.antialiasing, v.textureQuality, v.fov, v.modelDetail, v.aimBlur ? 1 : 0, v.consoleDepth ? 1 : 0, v.nativeCommands ? 1 : 0, v.watchWrites ? 1 : 0, v.windowMode, v.fpsOverlay ? 1 : 0, v.statsOverlay ? 1 : 0);
        fprintf(file, "brightness = %d\n", v.brightness);
        fprintf(file, "fog = %d\ndraw_distance = %d\neffects = %d\ndynamic_lights = %d\n", v.fog ? 1 : 0, v.drawDistance, v.effects,
                v.dynamicLights ? 1 : 0);
        fprintf(file, "sharp_shadows = %d\neffects_resolution = %d\nfoliage_coverage = %d\nfast_shadow_maps = %d\n", v.sharpShadows ? 1 : 0,
                v.effectsResolution, v.foliageCoverage ? 1 : 0, v.fastShadowMaps ? 1 : 0);
        fprintf(file, "film_video_levels = %d\nsteady_decals = %d\n", v.filmVideoLevels ? 1 : 0, v.steadyDecals ? 1 : 0);
        fprintf(file, "opt_depth_thread2 = %d\nopt_fast_constants = %d\nopt_shader_load_cache = %d\nopt_strong_change_check = %d\nopt_range_looks = %d\nopt_opaque_shaders = %d\nopt_staging_uploads = %d\nopt_texture_pool = %d\n",
                v.depthThread ? 1 : 0, v.fastConstants ? 1 : 0, v.shaderLoadCache ? 1 : 0, v.strongChangeCheck ? 1 : 0,
                v.rangeLooks ? 1 : 0, v.opaqueShaders ? 1 : 0, v.stagingUploads ? 1 : 0, v.texturePool ? 1 : 0);
        fprintf(file, "hdr = %d\nhdr_tone_mapper = %d\nhdr_peak_nits = %.0f\nhdr_game_nits = %.0f\nhdr_ui_nits = %.0f\nhdr_gamma_correction = %d\n",
                v.hdr ? 1 : 0, v.hdrToneMapper, v.hdrPeakNits, v.hdrGameNits, v.hdrUiNits, v.hdrGammaCorrection);
        fprintf(file, "hdr_exposure = %.2f\nhdr_highlights = %.0f\nhdr_shadows = %.0f\nhdr_contrast = %.0f\nhdr_saturation = %.0f\nhdr_highlight_saturation = %.0f\nhdr_blowout = %.0f\nhdr_flare = %.0f\nhdr_hue_correction = %.0f\nhdr_cone_response = %.0f\nhdr_bloom = %.0f\nhdr_tv_ramp = %.0f\nhdr_film_upgrade = %d\nhdr_hud_match = %d\n",
                v.hdrExposure, v.hdrHighlights, v.hdrShadows, v.hdrContrast, v.hdrSaturation, v.hdrHighlightSaturation, v.hdrBlowout,
                v.hdrFlare, v.hdrHueCorrection, v.hdrConeResponse, v.hdrBloom, v.hdrTvRamp, v.hdrFilmUpgrade ? 1 : 0, v.hdrHudMatch ? 1 : 0);
        fprintf(file, "[game]\nplayer_name = %s\nlan_address = %s\nsystem_link_peers = %s\nmaster_volume = %d\naudio_channels = %d\naim_assist = %d\nbounces = %d\ncontroller = %d\nrumble = %d\nstick_deadzone = %d\nmouse_sensitivity = %.2f\nstick_vertical_scale = %d\nraw_mouse = %d\ndirect_mouse = %d\ntoggle_sprint = %d\nshell_shock_blur = %d\nmelee_lunge = %d\n", v.playerName, v.lanAddress, v.systemLinkPeers, v.masterVolume, v.audioChannels, v.aimAssist ? 1 : 0, v.bounces ? 1 : 0, v.controller ? 1 : 0, v.rumble ? 1 : 0, v.stickDeadzone, v.mouseSensitivity, v.stickVerticalScale, v.rawMouse ? 1 : 0, v.directMouse ? 1 : 0, v.toggleSprint ? 1 : 0, v.shellShockBlur ? 1 : 0, v.meleeLunge ? 1 : 0);
        fprintf(file, "max_players = %d\n", v.maxPlayers >= 32 ? 32 : 24);
        fprintf(file, "blood_enabled = %d\nblood_amount = %d\nblood_density = %d\nblood_size = %.3f\nblood_decals = %d\nblood_physics = %d\nblood_persistence = %d\nblood_max_decals = %d\nblood_red = %.4f\nblood_green = %.4f\nblood_blue = %.4f\n",
            v.blood.enabled,v.blood.amount,v.blood.density,v.blood.size,v.blood.decals,v.blood.physics,v.blood.persistence,v.blood.maxDecals,v.blood.color[0],v.blood.color[1],v.blood.color[2]);
        fprintf(file, "blood_cloud_size = %.3f\nblood_cloud_opacity = %.3f\n", v.blood.cloudSize,v.blood.cloudOpacity);
        fprintf(file, "hitmarkers = %d\nhitmarker_sound = %d\nhitmarker_red_kill = %d\nhitmarker_size = %d\nhitmarker_opacity = %d\nhitmarker_duration = %d\nhitmarker_volume = %d\n",
            v.hitmarkers, v.hitmarkerSound, v.hitmarkerRedKill, v.hitmarkerSize, v.hitmarkerOpacity, v.hitmarkerDuration, v.hitmarkerVolume);
        fprintf(file, "quick_landing = %d\nquick_sprint = %d\ncustom_primary = %d\n", v.quickLanding ? 1 : 0, v.quickSprint ? 1 : 0,
                v.customPrimary);
        fprintf(file, "active_class = %d\ntwo_grenades = %d\nfaster_ads = %d\nfour_secondary = %d\n", v.activeClass, v.twoGrenades ? 1 : 0,
                v.fasterAds ? 1 : 0, v.fourSecondary ? 1 : 0);
        fprintf(file, "texture_dump = %d\ntexture_replace = %d\n", v.textureDump ? 1 : 0, v.textureReplace ? 1 : 0);
        fprintf(file, "binoculars = %d\nkillstreak_artillery = %d\nkillstreak_uav_kills = %d\nkillstreak_artillery_kills = %d\nkillstreak_typhoon_kills = %d\nuav_seconds = %d\n",
                v.binoculars ? 1 : 0, v.killstreakArtillery ? 1 : 0, v.killstreakUav, v.killstreakKills, v.killstreakTyphoon, v.uavSeconds);
        fprintf(file, "uav_plane = %d\n", v.uavPlane ? 1 : 0);
        fprintf(file, "ability_recharge = %d\naim_walk = %d\nlauncher = %d\ncampaign_guns = %d\ncampaign_test = %d\nextra_map_memory = %d\nbig_memory = %d\ncampaign_sounds = %d\ncampaign_gunshots = %d\ncampaign_icons = %d\n", v.abilityRecharge, v.aimWalk, v.launcher ? 1 : 0, v.campaignGuns ? 1 : 0, v.campaignTest, v.extraMapMemory, v.bigMemory ? 1 : 0, v.campaignSounds ? 1 : 0, v.campaignGunshots ? 1 : 0, v.campaignIcons ? 1 : 0);
        fprintf(file, "sniper_walk = %d\nsniper_sway = %d\nbot_vehicles = %d\n", v.sniperWalk, v.sniperSway, v.botVehicles ? 1 : 0);
        fprintf(file, "campaign_shot_sounds = %d\ncampaign_shot_events = %d\nads_in_air = %d\nsten_reload = %.2f\nfg42_reload = %.2f\nbren_reload = %.2f\ng43_reload = %.2f\nfg42_ads_fov = %.1f\nsten_ads_fov = %.1f\nbren_ads_fov = %.1f\nenfield_ads_fov = %.1f\n", v.campaignShotSounds ? 1 : 0, v.campaignShotEvents,
                v.adsInAir ? 1 : 0, v.stenReload, v.fg42Reload, v.brenReload, v.g43Reload, v.fg42AdsFov, v.stenAdsFov, v.brenAdsFov, v.enfieldAdsFov);
        fprintf(file, "mg34_no_deploy = %d\n", v.mg34NoDeploy ? 1 : 0);
        for (int i = 0; i < Values::CustomClasses; i++)
            fprintf(file, "class_%d_name = %s\nclass_%d_primary = %d\nclass_%d_sidearm = %d\nclass_%d_base = %d\nclass_%d_grenade_type = %d\nclass_%d_grenades = %d\n",
                    i + 1, v.className[i], i + 1, v.classPrimary[i], i + 1, v.classSidearm[i], i + 1, v.classBase[i], i + 1,
                    v.classGrenadeType[i], i + 1, v.classGrenades[i]);
        fprintf(file, "[native_options]\n");
        fprintf(file, "native_volume = %d\ncontroller_horizontal = %d\ncontroller_vertical = %d\n",
                v.nativeVolume, v.controllerHorizontal, v.controllerVertical);
        fprintf(file, "[keys]\n");
        for (int i = 0; i < ActionCount; i++) fprintf(file, "key_%s = %d\n", ActionKeys[i], v.keys[i]);
        fclose(file);
    }
}

void Settings::Resolution(const Values& values, int& width, int& height)
{
    if (values.resolutionWidth > 0 && values.resolutionHeight > 0)
    {
        width = values.resolutionWidth;
        height = values.resolutionHeight;
        return;
    }
    width = GetSystemMetrics(SM_CXSCREEN);
    height = GetSystemMetrics(SM_CYSCREEN);
    if (width <= 0 || height <= 0) { width = 1280; height = 720; }
}

void Settings::DrawnSize(const Values& values, int& width, int& height)
{
    Resolution(values, width, height);
    const int percent = values.resolutionScale < 25 ? 25 : values.resolutionScale > 200 ? 200 : values.resolutionScale;
    width = std::max(320, width * percent / 100);
    height = std::max(240, height * percent / 100);
}

const std::vector<Settings::Mode>& Settings::DisplayModes()
{
    // Asked of the display once: every size it offers at 720p or better,
    // each once, smallest first, the desktop's among them.
    static const std::vector<Mode> modes = []() {
        std::vector<Mode> found;
        DEVMODEW mode{};
        mode.dmSize = sizeof(mode);
        for (DWORD i = 0; EnumDisplaySettingsW(nullptr, i, &mode); i++)
        {
            if (mode.dmPelsWidth < 1280 || mode.dmPelsHeight < 720) continue;
            bool known = false;
            for (const Mode& m : found) if (m.width == int(mode.dmPelsWidth) && m.height == int(mode.dmPelsHeight)) known = true;
            if (!known) found.push_back({ int(mode.dmPelsWidth), int(mode.dmPelsHeight) });
        }
        const Mode desktop{ GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
        bool known = false;
        for (const Mode& m : found) if (m.width == desktop.width && m.height == desktop.height) known = true;
        if (!known && desktop.width > 0) found.push_back(desktop);
        std::sort(found.begin(), found.end(), [](const Mode& a, const Mode& b) { return a.width * a.height < b.width * b.height || (a.width * a.height == b.width * b.height && a.width < b.width); });
        return found;
    }();
    return modes;
}

// The model detail: COD3_MODELDETAIL=0|1 over the settings.
static int ModelDetail(const Settings::Values& v)
{
    static const int detailOverride = []() { const char* t = getenv("COD3_MODELDETAIL"); return t ? (t[0] == '0' ? 0 : 1) : -1; }();
    return detailOverride >= 0 ? detailOverride : v.modelDetail;
}

std::string Settings::TitleCommandsChanged()
{
    // What the title has, to begin with, is its own: the field of view 65,
    // the aim assist on, r_lodscale 1. So the first ask brings whatever the
    // settings change from that, and later ones what changed since. "set"
    // rather than the variable's name alone, because the first ask comes
    // before the title has registered most of them (the renderer's and the
    // game's come later): set makes the variable, and the title's own
    // registration then keeps the value it finds. These are commands on
    // the title's buffer, not lines in a config: the title never runs
    // default.cfg (it only checks the file is there), so lines added to
    // it did nothing.
    static int appliedFov = 65, appliedDetail = 0;
    static bool appliedAimAssist = true;
    const Values v = Get();
    std::string commands;
    if (v.fov != appliedFov)
    {
        appliedFov = v.fov;
        commands += "set cg_fov " + std::to_string(v.fov) + "\n";
    }
    // The model detail is the title's r_lodscale, which scales the distance
    // at which a model steps down to a coarser version: 1 is its own, 0
    // keeps the finest one at every distance.
    if (ModelDetail(v) != appliedDetail)
    {
        appliedDetail = ModelDetail(v);
        commands += std::string("set r_lodscale ") + (appliedDetail ? "0" : "1") + "\n";
    }
    // The title's own aim assist is three switches of its console.
    if (v.aimAssist != appliedAimAssist)
    {
        appliedAimAssist = v.aimAssist;
        const char* on = v.aimAssist ? "1" : "0";
        commands += std::string("set aim_slowdown_enabled ") + on + "\nset aim_lockon_enabled " + on + "\nset aim_autoaim_enabled " + on + "\n";
    }
    // Fog, draw distance, effects and dynamic lights: the title's own
    // switches, from the menu's Graphics page. The first ask sends only what
    // differs from the title's own.
    static bool appliedFog = true, appliedLights = true;
    static int appliedDistance = 0, appliedEffects = 0;
    if (v.fog != appliedFog)
    {
        appliedFog = v.fog;
        commands += std::string("set r_fog ") + (v.fog ? "1" : "0") + "\n";
    }
    if (v.drawDistance != appliedDistance)
    {
        appliedDistance = v.drawDistance;
        static const char* const distances[] = { "0", "9000", "6000", "3500" };
        commands += std::string("set r_zfar ") + distances[v.drawDistance] + "\n";
    }
    if (v.effects != appliedEffects)
    {
        appliedEffects = v.effects;
        commands += std::string("set fx_enable ") + (v.effects == 2 ? "0" : "1") + "\nset fx_cullscale " +
                    (v.effects == 1 ? "0.5" : "1") + "\n";
    }
    if (v.dynamicLights != appliedLights)
    {
        appliedLights = v.dynamicLights;
        commands += std::string("set r_dynamiclight ") + (v.dynamicLights ? "1" : "0") + "\n";
    }
    if (!commands.empty()) commands.pop_back();   // the last line's end: the queue adds its own
    return commands;
}
