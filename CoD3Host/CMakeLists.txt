cmake_minimum_required(VERSION 3.20)
project("CoD3Host" CXX)

# The host executable: first time setup, guest address space, and the kernel
# import stubs the recompiled code calls into. This is the part that will grow
# into an actual runtime.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT WIN32)
    message(FATAL_ERROR "The host is Windows only for now: it uses Win32 for the "
                        "address space reservation and the file pickers.")
endif()

# The icon and version information. RC_DEPENDS makes a change to the icon
# rebuild the executable, which it otherwise would not.
enable_language(RC)
set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} /I\"${CMAKE_CURRENT_SOURCE_DIR}/resources\"")

set(COD3_HOST_SOURCES
    "log.cpp"
    "main.cpp"
    "installer.cpp"
    "xdvdfs.cpp"
    "kernel.cpp"
    "kernel_stubs.cpp"
    "kernel_memory.cpp"
    "kernel_system.cpp"
    "kernel_objects.cpp"
    "kernel_file.cpp"
    "kernel_xam.cpp"
    "kernel_thread.cpp"
    "kernel_pcr.cpp"
    "kernel_apc.cpp"
    "kernel_video.cpp"
    "kernel_audio.cpp"
    "kernel_misc.cpp"
    "title_trace.cpp"
    "heap_trace.cpp"
    "pool_trace.cpp"
    "table_trace.cpp"
    "anim_heap_trace.cpp"
    "native_crt.cpp"
    "probe.cpp"
    "timeline.cpp"
    "render_d3d11.cpp"
    "render_state.cpp"
    "render_commands.cpp"
    "render_pipeline.cpp"
    "render_shaders.cpp"
    "render_resources.cpp"
    "texture_pack.cpp"
    "render_stats.cpp"
    "xenos_hlsl.cpp"
    "gpu.cpp"
    "gpu_address.cpp"
    "sampler.cpp"
    "watchpoint.cpp"
    "scheduler.cpp"
    "window.cpp"
    "input.cpp"
    "button_prompts.cpp"
    "hitmarkers.cpp"
    "blood.cpp"
    "blood_render.cpp"
    "blood_native.cpp"
    "settings.cpp"
    "overlay.cpp"
    "../thirdparty/imgui/imgui.cpp"
    "../thirdparty/imgui/imgui_draw.cpp"
    "../thirdparty/imgui/imgui_tables.cpp"
    "../thirdparty/imgui/imgui_widgets.cpp"
    "../thirdparty/imgui/backends/imgui_impl_dx11.cpp"
    "../thirdparty/imgui/backends/imgui_impl_win32.cpp"
    "audio_out.cpp"
    "xma.cpp"
    "parallel.cpp"
    "title_fixes.cpp"
    "d3d_hooks.cpp"
    "native_state.cpp"
    "write_watch.cpp"
    "watchdog.cpp"
    "modules.cpp"
    "coroutines.cpp"
    "kernel_net.cpp"
    "debug_probe.cpp"
    "movement_fixes.cpp"
    "lobby_rules.cpp"
    "mp_fixes.cpp"
    "bots.cpp"
    "dom.cpp"
    "class_editor.cpp"
    "mouse_look.cpp"
    "native_options.cpp"
    "coop.cpp")   # 0.5.223: co-op campaign experiment (COD3_COOP=host|join; empty in multiplayer)
add_executable(CoD3 WIN32 ${COD3_HOST_SOURCES} "resources/CoD3.rc")

# The HDR mode's shaders (hdr/cod3_hdr.hlsl on RenoDX's shader library and
# the Prism tone mapper, thirdparty/renodx and thirdparty/prism, both MIT),
# built into the executable: hdr/embed_shaders.cmake.
set(COD3_HDR_EMBED_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/hdr_shader_files.cpp")
include("${CMAKE_CURRENT_SOURCE_DIR}/hdr/embed_shaders.cmake")
target_sources(CoD3 PRIVATE "${COD3_HDR_EMBED_OUTPUT}")

# A console application: setup is interactive and the import trap has to be
# readable. WIN32 above is overridden here deliberately.
set_target_properties(CoD3 PROPERTIES WIN32_EXECUTABLE OFF)

target_link_libraries(CoD3 PRIVATE
    CoD3RecompLib   # the recompiled game code
    ${COD3_LEVEL_LIBRARIES}   # the levels' own code, one library each
    XenonUtils      # XEX parsing, reused so the loader matches the recompiler
    ole32           # IFileOpenDialog
    winmm           # sound out
    dbghelp         # the watchdog's stack walks
    d3d11           # the host GPU
    dxgi
    d3dcompiler     # the menu's own shaders (Dear ImGui)
    windowscodecs   # screenshots as PNG
    shell32
    ws2_32          # the network (kernel_net.cpp)
    iphlpapi)       # its adapters

# Decompiled functions, from a CoD3Decomp checkout beside this repository (or
# wherever COD3_DECOMP_DIR says): each takes the place of the recompiled
# function of its name, the recompiled one staying callable as __imp__sub_X.
# That code is derived from the game's and is kept in a private repository;
# nothing of it is here. Without the checkout the build is as before.
set(COD3_DECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../CoD3Decomp" CACHE PATH "A CoD3Decomp checkout, if there is one")
if (EXISTS "${COD3_DECOMP_DIR}/src/decomp.h")
    file(GLOB_RECURSE COD3_DECOMP_SOURCES CONFIGURE_DEPENDS "${COD3_DECOMP_DIR}/src/*.cpp")
    list(LENGTH COD3_DECOMP_SOURCES decompCount)
    message(STATUS "CoD3: ${decompCount} decompiled source files from ${COD3_DECOMP_DIR}")
    target_sources(CoD3 PRIVATE ${COD3_DECOMP_SOURCES})
    target_compile_definitions(CoD3 PRIVATE COD3_HAVE_DECOMP=1)
endif()

# FFmpeg's headers, for XMA decoding through its avcodec, loaded at run time
# (xma.cpp). Without them the build decodes nothing and says so.
find_path(COD3_FFMPEG_INCLUDE libavcodec/avcodec.h
    PATHS "$ENV{FFMPEG_DIR}/include" "C:/ffmpeg/include" "${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/ffmpeg/include")
if (COD3_FFMPEG_INCLUDE)
    message(STATUS "CoD3: XMA decoding through FFmpeg, headers at ${COD3_FFMPEG_INCLUDE}")
    # The DLLs are loaded by the major version the headers name
    # (avcodec-NN.dll, xma.cpp), and a release ships FFmpeg 8.0's: avcodec-62
    # and avutil-60. Headers of another FFmpeg (a nightly build is 63 and 61)
    # build fine and then find no DLL at all, and every XMA sound is silent.
    file(STRINGS "${COD3_FFMPEG_INCLUDE}/libavcodec/version_major.h" COD3_AVCODEC_MAJOR_LINE REGEX "^#define LIBAVCODEC_VERSION_MAJOR")
    string(REGEX MATCH "[0-9]+" COD3_AVCODEC_MAJOR "${COD3_AVCODEC_MAJOR_LINE}")
    if (NOT COD3_AVCODEC_MAJOR STREQUAL "62")
        message(WARNING "CoD3: the FFmpeg headers at ${COD3_FFMPEG_INCLUDE} are avcodec ${COD3_AVCODEC_MAJOR}, "
                        "so the game will look for avcodec-${COD3_AVCODEC_MAJOR}.dll. The port ships FFmpeg 8.0 "
                        "(avcodec-62.dll): use FFmpeg 8.0's headers, or put that version's DLLs beside CoD3.exe, "
                        "or the in-game sound will be silent.")
    endif()
    target_include_directories(CoD3 PRIVATE "${COD3_FFMPEG_INCLUDE}")
    target_compile_definitions(CoD3 PRIVATE COD3_HAVE_FFMPEG_HEADERS=1)
else()
    message(STATUS "CoD3: no FFmpeg headers, XMA sound will be silent")
endif()

# Dear ImGui, for the settings menu, from thirdparty/imgui.
target_include_directories(CoD3 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/imgui")

# The levels whose code was recompiled and linked in, for modules.cpp: one
# COD3_LEVEL(name) line each, written at configure time from what the
# levels directory found.
set(COD3_LEVELS_INC "${CMAKE_CURRENT_BINARY_DIR}/cod3_levels.inc")
set(levelsText "// Generated by CMake: the level libraries linked into this build.
")
foreach(library ${COD3_LEVEL_LIBRARIES})
    string(REPLACE "CoD3Level_" "" levelName "${library}")
    string(APPEND levelsText "COD3_LEVEL(${levelName})
")
endforeach()
file(WRITE "${COD3_LEVELS_INC}" "${levelsText}")
target_include_directories(CoD3 PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

target_compile_definitions(CoD3 PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS)

# kernel_stubs.cpp is one line per import and will shrink as imports get
# implemented; the unused parameter warnings there are not interesting.
target_compile_options(CoD3 PRIVATE -Wno-unused-parameter)

# Debug information for the host alone, so the watchdog can name the
# functions on a stuck thread's stack. The recompiled library stays as it
# is: its symbols would be gigabytes and say nothing a guest address does not.
target_compile_options(CoD3 PRIVATE -g -gcodeview)

# The main thread runs guest code too, at whatever depth the title's own
# calls reach; see ExCreateThread for why that wants a large host stack.
target_link_options(CoD3 PRIVATE /STACK:16777216)

target_link_options(CoD3 PRIVATE /DEBUG)

# --- CoD3MP.exe: the multiplayer executable --------------------------------
#
# The same host around the recompiled multiplayer executable,
# codmp_xenonf.xex (CoD3RecompLibMP, from scripts/recompile_mp.py). Its
# sources are this folder's with every single player address put to its
# multiplayer twin (mp_addresses.txt), written into the build by
# scripts/make_mp_host.py whenever a host file changes.
if (TARGET CoD3RecompLibMP)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(mpSource "${CMAKE_CURRENT_BINARY_DIR}/mp_src")
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/make_mp_host.py" "${mpSource}"
        RESULT_VARIABLE mpResult OUTPUT_VARIABLE mpOutput ERROR_VARIABLE mpError)
    if (NOT mpResult EQUAL 0)
        message(FATAL_ERROR "make_mp_host.py failed:\n${mpOutput}${mpError}")
    endif()
    string(STRIP "${mpOutput}" mpOutput)
    message(STATUS "${mpOutput}")
    file(GLOB hostFiles CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/*.txt")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        ${hostFiles} "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/make_mp_host.py" "${CMAKE_CURRENT_SOURCE_DIR}/../docs/kernel-imports-mp.txt")

    set(mpLeftOut title_trace.cpp heap_trace.cpp pool_trace.cpp table_trace.cpp anim_heap_trace.cpp probe.cpp)
    set(mpSources "${mpSource}/mp_trace_standins.cpp")
    foreach(source ${COD3_HOST_SOURCES})
        get_filename_component(name "${source}" NAME)
        if (source MATCHES "^\\.\\./")
            list(APPEND mpSources "${source}")             # Dear ImGui, as it is
        elseif (NOT name IN_LIST mpLeftOut)
            list(APPEND mpSources "${mpSource}/${name}")
        endif()
    endforeach()

    add_executable(CoD3MP WIN32 ${mpSources} "resources/CoD3.rc" "${COD3_HDR_EMBED_OUTPUT}")
    # 0.5.215: no console window beside the game (Justin: "a separate cmd
    # window opens"). A windows program, still started through main();
    # everything printed still goes to CoD3MP.log (log.cpp handles a start
    # with no console). --console or COD3_CONSOLE=1 opens one (main.cpp).
    set_target_properties(CoD3MP PROPERTIES WIN32_EXECUTABLE ON)
    target_link_libraries(CoD3MP PRIVATE
        CoD3RecompLibMP XenonUtils ole32 winmm dbghelp d3d11 dxgi d3dcompiler windowscodecs shell32 ws2_32 iphlpapi)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/mp_levels/cod3_levels.inc"
        "// Generated by CMake: the multiplayer executable loads no level DLLs.\n")
    target_include_directories(CoD3MP PRIVATE
        "${mpSource}" "${CMAKE_CURRENT_BINARY_DIR}/mp_levels" "${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/imgui")
    if (COD3_FFMPEG_INCLUDE)
        target_include_directories(CoD3MP PRIVATE "${COD3_FFMPEG_INCLUDE}")
        target_compile_definitions(CoD3MP PRIVATE COD3_HAVE_FFMPEG_HEADERS=1)
    endif()
    target_compile_definitions(CoD3MP PRIVATE COD3_MP=1 NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
    target_compile_options(CoD3MP PRIVATE -Wno-unused-parameter -g -gcodeview)
    target_link_options(CoD3MP PRIVATE /STACK:16777216 /DEBUG /ENTRY:mainCRTStartup)
endif()
