// The player's settings: what the F11 menu shows and changes, kept in
// CoD3Recomp.ini beside the executable. Read from any thread through a copy;
// written by the menu.
#pragma once

#include <cstdint>
#include "blood.h"
#include <filesystem>
#include <string>
#include <vector>

namespace Settings
{
    // The keys the menu binds, each standing in for one of the pad's
    // controls. The order is the order the menu lists them in.
    enum Action
    {
        Fire, Aim, Jump, Crouch, Use, Reload, Weapon, Melee, Sprint, Grenade, Smoke,
        Objectives, Pause, Forward, Backward, Left, Right,
        BinocularsOut, BinocularsAway,
        ActionCount
    };

    const char* ActionName(Action action);   // "Fire", "Aim", ...

    // A key, as a Windows virtual key code, or 0 for none.
    const char* KeyName(int virtualKey);

    struct Values
    {
        int renderer = 0;             // 0 Direct3D 11; 1 and 2 are names only
        int resolutionWidth = 0;      // the resolution the frame is drawn at; 0 by 0 is the desktop's
        int resolutionHeight = 0;
        int resolutionScale = 100;    // per cent of it the frame is really drawn at, 25 .. 200
        int textureFilter = 3;        // 0 the title's own, 1 bilinear, 2 trilinear, 3 anisotropic
        int anisotropy = 16;          // 2..16, with textureFilter 3
        bool vsync = true;
        int frameCap = 0;             // a level's frames held to this many a second, evenly paced (0: none; render_d3d11.cpp)
        bool unlockFrameRate = true;  // a level played at what the machine can do; the menus and films keep the console's 60
        int antialiasing = 3;         // 0 none, 1 FXAA, 2 MSAA 2x, 3 MSAA 4x, 4 MSAA 8x, 5 MSAA 4x and FXAA
        bool textureDump = false;     // every texture the game uploads saved once as textures\\dump\\<key>.png (texture_pack.cpp)
        bool textureReplace = true;   // textures\\replace\\<key>.png or .dds used in the game's place
        int textureQuality = 2;       // 0 low (two mip levels dropped), 1 medium (one), 2 high (all)
        int brightness = 100;         // 0.5.249: the picture's brightness, 50 .. 150 % (100 the game's own), over the game's display colour table
        int fov = 65;                 // the title's cg_fov, 65 (its own) .. 100
        int modelDetail = 1;          // 0 the title's own level of detail (r_lodscale 1), 1 the most detailed models at every distance (r_lodscale 0)
        bool fog = true;              // the title's fog (r_fog)
        int drawDistance = 0;         // 0 the title's own, 1 far, 2 medium, 3 near (r_zfar: what lies beyond is not drawn)
        int effects = 0;              // 0 all the title's effects, 1 fewer far away (fx_cullscale), 2 none (fx_enable 0)
        bool dynamicLights = true;    // lights from muzzle flashes and explosions (r_dynamiclight)
        bool aimBlur = false;         // the title's depth of field while aiming down the sights: off, since at a PC's resolution its quarter size blur leaves a ghost outline round the weapon
        bool consoleDepth = true;     // depth cut to the console's 24 bit float precision (no flickering surfaces; costs with MSAA)
        bool sharpShadows = true;     // the shadows' four tap filtering on the shadow map's own texels, point sampled (render_commands.cpp)
        int effectsResolution = 0;    // the half size surfaces (smoke, soft particles, shadow cascades): 0 the console's half size, 1 three quarters (1.5x), 2 full size (2x) (render_resources.cpp)
        bool foliageCoverage = true;  // with MSAA, alpha tested edges (foliage, fences) spread over the samples (xenos_hlsl.cpp)
        bool fastShadowMaps = true;   // the shadow maps' casters drawn with no pixel program, the GPU's depth only path (render_commands.cpp)
        bool steadyDecals = true;     // the decals and other layers over a surface put a little nearer, so they do not flicker (render_pipeline.cpp)
        bool filmVideoLevels = true;  // the films shown as a video player shows them, without the display's TV gamma ramp (render_d3d11.cpp)
        bool nativeCommands = false;  // the draws' state taken from the game's own D3D calls, the command stream run only for what the GPU still does (docs/native-d3d11-plan.md, N3; experimental, after a restart)
        bool watchWrites = false;     // still textures and vertex buffers known unchanged by their pages, not hashed every frame (write_watch.h; experimental, after a restart)
        // The optimizations, each of which can be turned off to find one
        // behind something that looks wrong (tuning.h).
        bool depthThread = false;       // depth copies for the title converted on a thread of their own (0.5.341: off by default: on, it crashed 0.5.340 multiplayer, the late copy writing over memory the game had reused)
        bool fastConstants = true;      // the constant packets stored as runs
        bool shaderLoadCache = true;    // programs loaded again matched without hashing them byte by byte
        bool strongChangeCheck = true;  // the eight lane fingerprint for telling whether memory changed
        bool rangeLooks = true;         // with "Watch memory writes" off, the vertices each draw reads of a changing big buffer looked at again
        bool opaqueShaders = true;      // pixel programs without the alpha test for the draws that do not test
        bool stagingUploads = true;     // textures sent to the GPU through staging copies
        bool texturePool = true;        // textures of a shape kept and used again
        // The HDR mode (F11's HDR tab; render_d3d11.cpp, hdr/cod3_hdr.hlsl on
        // RenoDX): an scRGB swap chain, the frame's targets 16 bit float, and
        // RenoDX's tone mappers and sliders. The sliders are RenoDX's own
        // scales: 50 is the game's look.
        bool hdr = false;
        int hdrToneMapper = 3;          // 0 vanilla (clipped as SDR), 1 none, 2 ACES, 3 RenoDRT, 4 PsychoV 17, 5 PsychoV 22, 6 PsychoV 30, 7 Prism
        float hdrPeakNits = 0.0f;       // the display's peak; 0 is what Windows reports for it (1000 when it reports nothing)
        float hdrGameNits = 203.0f;     // the game's white
        float hdrUiNits = 203.0f;       // the HUD's and the menus' white
        int hdrGammaCorrection = 1;     // the SDR picture decoded as 0 sRGB, 1 gamma 2.2, 2 gamma 2.4
        float hdrExposure = 1.0f;       // 0 .. 4
        float hdrHighlights = 50.0f;    // 0 .. 100
        float hdrShadows = 50.0f;
        float hdrContrast = 50.0f;
        float hdrSaturation = 50.0f;
        float hdrHighlightSaturation = 50.0f;   // RenoDRT
        float hdrBlowout = 0.0f;                // RenoDRT
        float hdrFlare = 0.0f;                  // RenoDRT
        float hdrHueCorrection = 100.0f;
        float hdrConeResponse = 50.0f;          // PsychoV
        float hdrBloom = 50.0f;         // the bloom's strength (in SDR too): 50 the game's
        float hdrTvRamp = 100.0f;       // how much of the title's TV gamma ramp, 0 .. 100: 100 the table's full shape, held to Game brightness (mid grey and white kept: cod3_hdr.hlsl's TvRampAnchored), lower lightens the shadows
        bool hdrHudMatch = true;        // the HUD's white (the title draws it at about 86%) brought to the UI brightness
        bool hdrFilmUpgrade = false;    // films expanded to HDR (RenoDX's video upscale)
        int windowMode = 0;           // 0 windowed, 1 borderless full screen
        bool fpsOverlay = false;
        bool statsOverlay = false;    // the renderer's counters over the picture
        bool aimAssist = true;        // the title's own, through its config
        bool quickLanding = true;
        bool quickSprint = true;
        int customPrimary = 0;        // (older single choice, no longer used)
        static constexpr int CustomClasses = 5;
        int activeClass = 0;          // 0 none: the class's own kit; 1..5 the custom class used at one's spawns
        char className[CustomClasses][16] = {};
        int classPrimary[CustomClasses] = {};   // 0 the class's own; else mp_fixes.cpp's list
        int classSidearm[CustomClasses] = {};   // 0 the side's own; 1 Colt; 2 P38; 3 to 19 a main weapon (mp_fixes.cpp's list, from the M1 Garand) in the pistol's place
        int classBase[CustomClasses] = {};      // 0 the class picked; 1..7 heavy assault .. scout (its ability and kit)
        int classGrenadeType[CustomClasses] = {}; // 0 the side's own; 1 American frag; 2 German stick; 3 Mills bomb; 4 sticky bomb
        int classGrenades[CustomClasses] = {};  // 0 the usual; 1..4 grenades at a spawn
        bool fasterAds = true;        // walking with the sights up a third quicker (bots.cpp's move scale hook)
        bool twoGrenades = true;
        int campaignTest = 0;          // a campaign gun put together in part, for finding what keeps its view model from showing: 1 the copy's animations kept, 2 its view model kept
        bool campaignSounds = true;    // the campaign levels' sound banks loaded, the campaign guns' own sounds (mp_fixes.cpp)
        bool campaignIcons = true;     // the campaign guns' own HUD icons, looked for in the campaign archives too (0.5.45; off: the copy's) (mp_fixes.cpp)
        bool campaignShotSounds = true; // the campaign guns' own gunshot sounds (STEN_FIRE_1ST, ...) from the campaign banks, their muzzle flashes kept (mp_fixes.cpp)
        int campaignShotEvents = 0;      // shot events keeping the campaign gun's own name (1, 2, 4, 8: events 1, 3, 7, 53) (mp_fixes.cpp)
        bool adsInAir = true;          // the sights can come up in the air (the game: on the ground only) (mp_fixes.cpp)
        float stenReload = 0.0f;       // the campaign guns' reload times in seconds (0: mp_fixes.cpp's own), for tuning without a build
        float fg42Reload = 0.0f;
        float brenReload = 0.0f;
        float g43Reload = 0.0f;
        bool mg34NoDeploy = true;   // the MG34 aims from the shoulder (no bipod, no forced prone)
        float fg42AdsFov = 0.0f;       // the FG42's field of view with the sights up, degrees (0: its copy's, the BAR's)
        float stenAdsFov = 0.0f;
        float brenAdsFov = 0.0f;
        float enfieldAdsFov = 0.0f;
        bool campaignGunshots = false; // the campaign guns' shots by their own names too (off: the shots then lose their muzzle flash and play the wrong sounds, 0.5.31) (mp_fixes.cpp)
        bool bigMemory = true;         // the game's map block in graphics memory past 512 MB (gpu_address.cpp, mp_fixes.cpp)
        int extraMapMemory = 0;        // MB added to the game's own memory block for maps when campaignGuns is on (mp_fixes.cpp), 0..18
        bool campaignGuns = false;     // (off: the extra archive leaves the map short of memory) the campaign's Lee-Enfield (and more) set up in multiplayer (mp_fixes.cpp)
        bool launcher = true;          // everyone's own spawn: the side's rocket launcher besides, out on d-pad left (mp_fixes.cpp)
        bool killstreakArtillery = true;   // multiplayer: the killstreaks, kills in a row without dying (host's side; mp_fixes.cpp)
        int killstreakUav = 3;             // kills in a row for the UAV (every enemy on the compass)
        int killstreakKills = 5;           // for the artillery barrage, called with the binoculars
        int killstreakTyphoon = 7;         // for the Typhoon's strafing run
        int uavSeconds = 30;
        bool uavPlane = true;              // 0.5.253: the UAV's plane over the map (its P-47/Stuka archives loaded with every map)
        bool binoculars = true;        // everyone's own spawn: binoculars besides, out on d-pad up, away on d-pad down (mp_fixes.cpp)
        bool fourSecondary = true;
        int aimWalk = 80;             // walking with the sights up, per cent of walking from the hip (the game: 40)
        int sniperWalk = 30;          // 0.5.197: walking with a scope up, per cent of the hip walk (slower than other guns' 40)
        int sniperSway = 50;          // 0.5.197: scoped rifles' sway while aiming, per cent of the game's own
        bool botVehicles = true;      // 0.5.197: bots crew vehicles (drive, ride, man the gun) and use mounted guns
        int abilityRecharge = 50;     // per cent of the usual wait for the artillery, health packs and ammo (100: the game's own)    // four of the second grenade slot (smoke) and of the special (mines)      // everyone's own spawn: at least two grenades (the title gives one, two and three by rank)        // 0 the class's own main weapon; else one of mp_fixes.cpp's list, whatever the class      // stamina back soon after a sprint, not after a second's wait (mp_fixes.cpp)     // a heavy landing slows for a moment, not up to two seconds (mp_fixes.cpp)
        bool bounces = true;          // jumps onto slopes bounce, as in Call of Duty 4 (movement_fixes.cpp)
        bool controller = true;       // read a pad when one is there
        char playerName[16] = {};
        char lanAddress[16] = {};
        char systemLinkPeers[128] = {}; // addresses System Link searches also go to, one by one (for Tailscale, or past a router)     // the IPv4 address System Link gives out; empty chooses (a Hamachi or other virtual LAN's first)     // the gamertag others see in multiplayer; empty is the Windows user name
        int masterVolume = 70;        // per cent, the whole game's sound, 0 .. 100
        int nativeVolume = 50;        // native game Options, remembered independently of F11
        int controllerHorizontal = 100; // native percentage: 100 = level10, 150 = level15
        int controllerVertical = 100;
        bool rumble = true;           // the title's vibration sent to the pad
        int stickDeadzone = 15;       // per cent of a stick's travel ignored around its centre, 0 .. 50
        int audioChannels = 0;        // the speakers: 0 automatic (5.1 when Windows' speakers have six or more channels), 1 stereo, 2 5.1 (audio_out.cpp)
        int stickVerticalScale = 200; // the pad's up and down look, per cent of the title's own (which halves it): 200 matches the turn, 50 .. 300 (mouse_look.cpp)
        float mouseSensitivity = 1.0f;   // both axes, 0.1 .. 3
        bool rawMouse = true;            // the mouse's own counts, not the pointer's travel
        bool directMouse = true;         // the mouse turns the view itself, not through the right stick (mouse_look.cpp)
        int maxPlayers = 24;             // 0.5.247: players in a match, 24 (the game's) or 32 (test; read at start; both PCs the same)
        bool meleeLunge = true;          // a melee carries the player at the enemy it locks on to (mp_fixes.cpp)
        bool shellShockBlur = false;     // the blurred, dark screen of a nearby explosion's shell shock (mp_fixes.cpp)
        bool toggleSprint = true;        // a press of sprint starts a run that lasts, not one held (input.cpp)
        Blood::Options blood;
        bool hitmarkers = true;
        bool hitmarkerSound = true;
        bool hitmarkerRedKill = true;
        int hitmarkerSize = 56;        // full image width at 1080p
        int hitmarkerOpacity = 100;    // percent
        int hitmarkerDuration = 200;   // milliseconds
        int hitmarkerVolume = 70;      // percent, also follows master volume
        int keys[ActionCount] = {};
    };

    Values Defaults();

    // The resolution chosen, resolved: the desktop's when the setting is
    // 0 by 0. In a window the window is made this size; over the whole
    // screen the picture is scaled to it.
    void Resolution(const Values& values, int& width, int& height);

    // The size the frame is really drawn at: the resolution times the scale.
    void DrawnSize(const Values& values, int& width, int& height);

    // The resolutions the display offers, largest last, for the menu.
    struct Mode { int width, height; };
    const std::vector<Mode>& DisplayModes();

    // The current values, and setting them (which also saves).
    Values Get();
    void Set(const Values& values);
    // Setting them without saving, for a slider dragged in the menu (the
    // HDR tabs apply as they move); Save() when the drag is over.
    void Preview(const Values& values);

    // Reads the file beside the executable, or starts from the defaults.
    void Load(const std::filesystem::path& exeDirectory);
    void Save();

    // The console commands the settings ask of the title (the field of
    // view, the model detail, the aim assist), for its command buffer: at
    // the first ask whatever differs from the title's own values, then
    // what changed since the last ask. Empty when there is nothing to say.
    std::string TitleCommandsChanged();
}
