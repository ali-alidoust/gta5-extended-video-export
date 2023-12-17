set(PROJECT_NAME gta5-extended-video-export-config)

################################################################################
# Source groups
################################################################################
# set(Header_Files
# )

# source_group("Header Files" FILES ${Header_Files})

set(Source_Files
    "gta5-extended-video-export-config.cpp"
)

set(VK_Source_Files
        "../voukoder/VoukoderTypeLib_i.c")

source_group("Source Files" FILES ${Source_Files} ${VK_Source_Files})

set(ALL_FILES
#    ${Header_Files}
    ${Source_Files}
    ${VK_Source_Files}
)

################################################################################
# Target
################################################################################
add_executable(${PROJECT_NAME} WIN32 ${ALL_FILES})

use_props(${PROJECT_NAME} "${CMAKE_CONFIGURATION_TYPES}" "${DEFAULT_CXX_PROPS}")
set(ROOT_NAMESPACE gta5extendedvideoexportconfig)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_GLOBAL_KEYWORD "Win32Proj"
)
set_target_properties(${PROJECT_NAME} PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL     "TRUE"
    INTERPROCEDURAL_OPTIMIZATION_RELEASE        "TRUE"
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
    "_CONSOLE;"
    "TARGET_NAME=\"${TARGET_NAME}\";"
    "_CRT_SECURE_NO_WARNINGS"
)

################################################################################
# Compile and link options
################################################################################
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT};
        ${DEFAULT_CXX_EXCEPTION_HANDLING};
        /Y-
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:
            /DEBUG:FULL
        >
        $<$<CONFIG:MinSizeRel>:
            /DEBUG;
            /OPT:REF;
            /OPT:ICF;
            /INCREMENTAL:NO
        >
        $<$<CONFIG:Release>:
            /DEBUG;
            /OPT:REF;
            /OPT:ICF;
            /INCREMENTAL:NO
        >
        $<$<CONFIG:RelWithDebInfo>:
            /DEBUG:FULL
        >
        /SUBSYSTEM:WINDOWS
    )
endif()

add_custom_command_if(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMANDS
        COMMAND $<CONFIG:Debug> move /Y "$<SHELL_PATH:${OUTPUT_DIRECTORY}>\\gta5-extended-video-export-config.exe" "$<SHELL_PATH:${OUTPUT_DIRECTORY}/>EVE\\EncoderConfig.exe"
        COMMAND $<CONFIG:Debug> move /Y "$<SHELL_PATH:${OUTPUT_DIRECTORY}>\\gta5-extended-video-export-config.pdb" "$<SHELL_PATH:${OUTPUT_DIRECTORY}/>EVE\\EncoderConfig.pdb"
        COMMAND $<CONFIG:Release> move /Y "$<SHELL_PATH:${OUTPUT_DIRECTORY}>\\gta5-extended-video-export-config.exe" "$<SHELL_PATH:${OUTPUT_DIRECTORY}/>EVE\\EncoderConfig.exe"
        COMMAND $<CONFIG:Release> move /Y "$<SHELL_PATH:${OUTPUT_DIRECTORY}>\\gta5-extended-video-export-config.pdb" "$<SHELL_PATH:${OUTPUT_DIRECTORY}/>EVE\\EncoderConfig.pdb"
)

### Imports 

# Voukoder
find_path(VOUKODER_INCLUDE_DIR VoukoderTypeLib_h.h REQUIRED HINTS "${CMAKE_CURRENT_SOURCE_DIR}/../voukoder")
target_include_directories(${PROJECT_NAME} PRIVATE ${VOUKODER_INCLUDE_DIR})

find_package(nlohmann_json REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)

# wxwidgets
#find_path(WXWIDGETS_INCLUDE_DIR wx/wx.h REQUIRED)
#target_include_directories(${PROJECT_NAME} PRIVATE ${WXWIDGETS_INCLUDE_DIR})
#find_library(WXWIDGETS_LIBRARY wxbase31u REQUIRED)
#target_link_libraries(${PROJECT_NAME} PRIVATE ${WXWIDGETS_LIBRARY})
#find_library(WXWIDGETS_CORE_LIBRARY wxmsw31u_core REQUIRED)
#target_link_libraries(${PROJECT_NAME} PRIVATE ${WXWIDGETS_CORE_LIBRARY})

# 
#include(${CMAKE_SOURCE_DIR}/winmsvc.cmake)
# find_package(wx CONFIG REQUIRED COMPONENTS core base)
# target_link_libraries(${PROJECT_NAME} PRIVATE wxwidgets)