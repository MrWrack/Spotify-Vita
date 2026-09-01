cmake_minimum_required(VERSION 3.10)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VITASDK})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake"
            CACHE PATH "VitaSDK toolchain file")
    else()
        message(FATAL_ERROR "Please define VITASDK or CMAKE_TOOLCHAIN_FILE")
    endif()
endif()

project(spotify-vita C)

include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2")

include_directories(include)

add_executable(${PROJECT_NAME}
    src/main.c
    src/app_cover.c
    src/cover_pipeline.c
    src/test_cover.c
    src/cover_vita2d.c
    src/vita2d_renderer.c
    src/app_ui.c
    src/app_input.c
    src/app_controller.c
    src/cover_texture.c
    src/spotify_auth_pkce.c
    src/spotify_http.c
    src/spotify_json.c
    src/spotify_playback.c
    src/spotify_state_worker.c
    src/vita_browser.c
    src/vita_network.c
    src/spotify_callback_server.c
    src/now_playing.c
    src/spotify_token_store.c
    src/spotify_login.c
)

target_link_libraries(${PROJECT_NAME}
    vita2d

    SceDisplay_stub
    SceGxm_stub
    SceSysmodule_stub
    SceCtrl_stub
    SceTouch_stub
    ScePgf_stub
    ScePvf_stub
    SceCommonDialog_stub
    SceAppMgr_stub
    SceSysmem_stub
    SceKernelThreadMgr_stub
    SceLibKernel_stub
    SceHttp_stub
    SceNet_stub
    SceNetCtl_stub
    SceSsl_stub

    freetype
    png
    jpeg
    z
    m
    c
)

vita_create_self(eboot.bin ${PROJECT_NAME})

vita_create_vpk(${PROJECT_NAME}.vpk SPVT00001 eboot.bin
    VERSION 01.00
    NAME "Spotify Vita"
    FILE sce_sys/icon0.png sce_sys/icon0.png
    FILE sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
    FILE sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
    FILE sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
)
