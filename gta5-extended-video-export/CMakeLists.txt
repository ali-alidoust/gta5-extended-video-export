set(PROJECT_NAME gta5-extended-video-export)

################################################################################
# Source groups
################################################################################
source_group("" FILES ${no_group_source_files})

set(Header_Files
        "config.h"
        "encoder.h"
        "game-detour-def.h"
        "hook-def.h"
        "logger.h"
        "MFUtility.h"
        "SafeQueue.h"
        "script.h"
        "stdafx.h"
        "targetver.h"
        "util.h"
        "yara-helper.h"
        "yara-patterns.h"
        "hooks.h")
source_group("Header Files" FILES ${Header_Files})

set(Source_Files
        "config.cpp"
        "dllmain.cpp"
        "encoder.cpp"
        "logger.cpp"
        "script.cpp"
        "yara-helper.cpp"
        )
source_group("Source Files" FILES ${Source_Files})

set(ALL_FILES
        ${no_group_source_files}
        ${Header_Files}
        ${Source_Files}
        )

################################################################################
# Target
################################################################################
add_library(${PROJECT_NAME} SHARED ${ALL_FILES})

use_props(${PROJECT_NAME} "${CMAKE_CONFIGURATION_TYPES}" "${DEFAULT_CXX_PROPS}")
set(ROOT_NAMESPACE gta5-extended-video-export)

set_target_properties(${PROJECT_NAME} PROPERTIES
        VS_GLOBAL_KEYWORD "Win32Proj"
        )
################################################################################
# Target name
################################################################################
set_target_properties(${PROJECT_NAME} PROPERTIES
        TARGET_NAME_DEBUG "ExtendedVideoExport"
        TARGET_NAME_MINSIZEREL "ExtendedVideoExport"
        TARGET_NAME_RELEASE "ExtendedVideoExport"
        TARGET_NAME_RELWITHDEBINFO "ExtendedVideoExport"
        )
################################################################################
# Compile definitions
################################################################################
target_compile_definitions(${PROJECT_NAME} PRIVATE
        "$<$<CONFIG:Debug>:"
        "_DEBUG;"
        "_SCL_SECURE_NO_WARNINGS"
        ">"
        "$<$<CONFIG:MinSizeRel>:"
        "NDEBUG;"
        "_CRT_NONSTDC_NO_WARNINGS"
        ">"
        "$<$<CONFIG:Release>:"
        "NDEBUG;"
        "_CRT_NONSTDC_NO_WARNINGS"
        ">"
        "$<$<CONFIG:RelWithDebInfo>:"
        "_DEBUG;"
        "_SCL_SECURE_NO_WARNINGS"
        ">"
        "_WINDOWS;"
        "_USRDLL;"
        "GTA5EXTENDEDVIDEOEXPORT_EXPORTS;"
        "TARGET_NAME=\"${TARGET_NAME}\";"
        "_CRT_SECURE_NO_WARNINGS;"
        "_WIN32_WINNT=0x0601"
        )

################################################################################
# Compile and link options
################################################################################
if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:MinSizeRel>:
            /Y-
            >
            $<$<CONFIG:Release>:
            /Y-
            >
            ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT};
            ${DEFAULT_CXX_EXCEPTION_HANDLING}
            )
    target_link_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Debug>:
            /DELAYLOAD:zlibd.dll;
            /DEBUG:FULL
            >
            $<$<CONFIG:MinSizeRel>:
            /DELAYLOAD:zlib.dll
            >
            $<$<CONFIG:Release>:
            /DELAYLOAD:zlib.dll
            >
            $<$<CONFIG:RelWithDebInfo>:
            /DELAYLOAD:zlibd.dll;
            /DEBUG:FULL
            >
            /DELAYLOAD:avcodec-57.dll;
            /DELAYLOAD:avdevice-57.dll;
            /DELAYLOAD:avfilter-6.dll;
            /DELAYLOAD:avformat-57.dll;
            /DELAYLOAD:avutil-55.dll;
            /DELAYLOAD:postproc-54.dll;
            /DELAYLOAD:swresample-2.dll;
            /DELAYLOAD:swscale-4.dll;
            /NODEFAULTLIB:libcmt.lib;
            /SUBSYSTEM:WINDOWS
            )
endif ()

################################################################################
# Pre build events
################################################################################
add_custom_command_if(
        TARGET ${PROJECT_NAME}
        PRE_BUILD
        COMMANDS
)
################################################################################
# Post build events
################################################################################
add_custom_command_if(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMANDS
        COMMAND $<CONFIG:Debug> xcopy /y /E "$<SHELL_PATH:${CMAKE_CURRENT_SOURCE_DIR}/>deploy\\*" "$<SHELL_PATH:${OUTPUT_DIRECTORY}>"
        COMMAND $<CONFIG:MinSizeRel> del /F /Q "$<SHELL_PATH:${OUTPUT_DIRECTORY}>EVE"
        COMMAND $<CONFIG:MinSizeRel> xcopy /y /E "$<SHELL_PATH:${CMAKE_CURRENT_SOURCE_DIR}/>deploy\\*" "$<SHELL_PATH:${OUTPUT_DIRECTORY}>"
        COMMAND $<CONFIG:Release> del /F /Q "$<SHELL_PATH:${OUTPUT_DIRECTORY}>EVE"
        COMMAND $<CONFIG:Release> xcopy /y /E "$<SHELL_PATH:${CMAKE_CURRENT_SOURCE_DIR}/>deploy\\*" "$<SHELL_PATH:${OUTPUT_DIRECTORY}>"
        COMMAND $<CONFIG:RelWithDebInfo> xcopy /y /E "$<SHELL_PATH:${CMAKE_CURRENT_SOURCE_DIR}/>deploy\\*" "$<SHELL_PATH:${OUTPUT_DIRECTORY}>"
)

################################################################################
# Dependencies
################################################################################
# ScriptHookV
find_library(SCRIPT_HOOK_V_LIBRARY ScriptHookV REQUIRED HINTS "${CMAKE_CURRENT_SOURCE_DIR}/../ScriptHookV/lib")
target_link_libraries(${PROJECT_NAME} PRIVATE ${SCRIPT_HOOK_V_LIBRARY})


# inih
find_path(INIH_INCLUDE_DIR ini.h REQUIRED)
find_library(INIH_LIBRARY inih REQUIRED)
target_include_directories(${PROJECT_NAME} PRIVATE ${INIH_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE ${INIH_LIBRARY})

# PolyHook2
find_package(PolyHook_2 CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE PolyHook_2::PolyHook_2)

# FFMPEG (libavcodec)
find_package(FFMPEG REQUIRED)
target_include_directories(${PROJECT_NAME} PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_directories(${PROJECT_NAME} PRIVATE ${FFMPEG_LIBRARY_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${FFMPEG_LIBRARIES})

# OpenEXR
find_package(IlmBase CONFIG REQUIRED)
find_package(OpenEXR CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE IlmBase::Iex IlmBase::Half IlmBase::Imath IlmBase::IexMath)
target_link_libraries(${PROJECT_NAME} PRIVATE OpenEXR::IlmImf OpenEXR::IlmImfUtil OpenEXR::IlmImfConfig)

# DirectX Headers
find_package(directx-headers CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Microsoft::DirectX-Headers)

# DirectX Math
find_package(directxmath CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Microsoft::DirectXMath)

# DirectXTex
find_package(directxtex CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Microsoft::DirectXTex)