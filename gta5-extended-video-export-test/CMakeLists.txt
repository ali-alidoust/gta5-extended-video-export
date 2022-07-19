set(PROJECT_NAME gta5-extended-video-export-test)

################################################################################
# Source groups
################################################################################
set(no_group_source_files
    "ReadMe.txt"
)
source_group("" FILES ${no_group_source_files})

set(Header_Files
    "../gta5-extended-video-export/encoder.h"
    "../gta5-extended-video-export/logger.h"
    "stdafx.h"
    "targetver.h"
)
source_group("Header Files" FILES ${Header_Files})

set(Source_Files
    "../gta5-extended-video-export/encoder.cpp"
    "../gta5-extended-video-export/logger.cpp"
    "gta5-extended-video-export-test.cpp"
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
add_executable(${PROJECT_NAME} ${ALL_FILES})

use_props(${PROJECT_NAME} "${CMAKE_CONFIGURATION_TYPES}" "${DEFAULT_CXX_PROPS}")
set(ROOT_NAMESPACE gta5extendedvideoexporttest)

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
        /SUBSYSTEM:CONSOLE
    )
endif()


# FFMPEG (libavcodec)
# find_package(FFMPEG REQUIRED)
# target_include_directories(${PROJECT_NAME} PRIVATE ${FFMPEG_INCLUDE_DIRS})
# target_link_directories(${PROJECT_NAME} PRIVATE ${FFMPEG_LIBRARY_DIRS})
# target_link_libraries(${PROJECT_NAME} PRIVATE ${FFMPEG_LIBRARIES})

# OpenEXR
find_package(IlmBase CONFIG REQUIRED)
find_package(OpenEXR CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE IlmBase::Iex IlmBase::Half IlmBase::Imath IlmBase::IexMath)
target_link_libraries(${PROJECT_NAME} PRIVATE OpenEXR::IlmImf OpenEXR::IlmImfUtil OpenEXR::IlmImfConfig)