cmake_minimum_required(VERSION 3.16)

project(ZZACWT VERSION 0.3)

# Add shared CMake modules
# Add shared CMake modules
# First look in sibling path for development, then in installed location for users
include(FetchContent)

# Try to find zzacmake as a sibling project first
set(ZZACMAKE_SIBLING_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../zzacmake")
if(EXISTS "${ZZACMAKE_SIBLING_DIR}/cmake")
    set(zzacmake_SOURCE_DIR "${ZZACMAKE_SIBLING_DIR}")
    message(STATUS "Using zzacmake from sibling project: ${zzacmake_SOURCE_DIR}")
else()
    # Fall back to fetching from GitHub
    FetchContent_Declare(
        zzacmake
        GIT_REPOSITORY https://github.com/pvrose/zzacmake.git
        GIT_TAG main
    )
    FetchContent_MakeAvailable(zzacmake)
    message(STATUS "Fetched zzacmake from GitHub: ${zzacmake_SOURCE_DIR}")
endif()

list(APPEND CMAKE_MODULE_PATH "${zzacmake_SOURCE_DIR}/cmake")

include(Gm3zzaProjectDefaults)
include(Gm3zzaVersioning)
include(Gm3zzaZzacommon)
include(Gm3zzaResources)
include(Gm3zzaWindowsRuntime)
include(Gm3zzaDocs)
include(deps/portaudio_helper)

## Set target executable and program metadata
set(TARGET zzacwt)
set(APP_VENDOR "GM3ZZA")
set(APP_NAME "ZZACWT")
set(APP_RC_VALUE "")
string(TIMESTAMP APP_TIMESTAMP UTC)
set(APP_SOURCE_DIR ${ZZACWT_SOURCE_DIR})

## Initialise defaults and settings
gm3zza_project_defaults()
gm3zza_set_default_install_dirs("${APP_NAME}")
gm3zza_split_version(${PROJECT_NAME})

## Set version string for app.cpp.in
set(APP_VERSION "${ZZACWT_VERSION}${APP_RC_VALUE}")

## Set source files (excluding app.cpp which is generated)
set(CPPFILES 
  ${ZZACWT_SOURCE_DIR}/src/codec.cpp
  ${ZZACWT_SOURCE_DIR}/src/cred_dialog.cpp
  ${ZZACWT_SOURCE_DIR}/src/main.cpp
  ${ZZACWT_SOURCE_DIR}/src/mod_mixer.cpp
  ${ZZACWT_SOURCE_DIR}/src/noise_gen.cpp
  ${ZZACWT_SOURCE_DIR}/src/oscillator.cpp
  ${ZZACWT_SOURCE_DIR}/src/review.cpp
  ${ZZACWT_SOURCE_DIR}/src/shaper.cpp
  ${ZZACWT_SOURCE_DIR}/src/text_gen.cpp
  ${ZZACWT_SOURCE_DIR}/src/user_if.cpp
)

## Data files
set(DATAFILES
  ${ZZACWT_SOURCE_DIR}/rose.png 
  ${ZZACWT_SOURCE_DIR}/text.txt
  ${ZZACWT_SOURCE_DIR}/word_list.txt
  ${ZZACWT_SOURCE_DIR}/qso_data.txt
)

## Generate Windows icon and resource file
if(MSVC)
  if(NOT DEFINED ZZACWT_ICON_WHITE_THRESHOLD)
    set(ZZACWT_ICON_WHITE_THRESHOLD 240)
  endif()
  
  gm3zza_enable_windows_icon(
    TARGET ${TARGET}
    PNG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/rose.png"
    RC_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/zzacwt.rc.in"
    WHITE_THRESHOLD ${ZZACWT_ICON_WHITE_THRESHOLD}
    OUTPUT_VAR APP_RC_FILE
  )
endif()

## Find PortAudio library
gm3zza_find_portaudio()
message(STATUS "PortAudio include dir: ${PORTAUDIO_INCLUDE_DIR}")
message(STATUS "PortAudio library: ${PORTAUDIOLIB}")

## Source file generation in binary tree
gm3zza_generate_app_source(
  INPUT "${ZZACWT_SOURCE_DIR}/src/app.cpp.in"
  OUTPUT_VAR GENERATED_APP_CPP
)

set(ALL_CPPFILES ${GENERATED_APP_CPP} ${CPPFILES})

## Use shared zzacommon (includes zzaf, zzam, zzad, zzafb)
## TODO investigate why zzab is needed to install the runtime DLLs even though zzafb is added.
gm3zza_use_zzacommon(COMPONENTS zzaf zzam zzad zzab zzafb zzap GIT_TAG master)


# Define the zzacwt executable
if(MSVC)
  add_executable(${TARGET} ${APP_RC_FILE})
else()
  add_executable(${TARGET})
endif()

if(MSVC)
  add_dependencies(${TARGET} 
    zzacommon::zzaf
    zzacommon::zzam
    zzacommon::zzad
    zzacommon::zzap
    zzacommon::zzafb
  )
endif()

target_include_directories(${TARGET} PRIVATE 
  ${ZZACWT_SOURCE_DIR}/include
  ${ZZACOMMON_INCLUDE_DIR}
  ${FLTK_INCLUDE_DIR}
  ${PORTAUDIO_INCLUDE_DIR}
)

if(MSVC)
  message(STATUS "Including other libraries for MSVC from ${USER_HOME_DIR}")
  target_include_directories(${TARGET} PRIVATE 
    "${USER_HOME_DIR}\\source\\repos\\json\\include"
  )
endif()

target_sources(${TARGET} PRIVATE ${ALL_CPPFILES})

target_link_libraries(${TARGET} PRIVATE
  zzacommon::zzaf
  zzacommon::zzam
  zzacommon::zzad
  zzacommon::zzafb
  zzacommon::zzap
  ${FLTK_LIBRARIES}
)

## Stage runtime assets for development
gm3zza_stage_runtime_files(TARGET ${TARGET} FILES ${DATAFILES})

## Copy runtime DLLs for development
gm3zza_copy_runtime_dlls(TARGET ${TARGET})


# Generate documents - userguide and API documentation.
if(ZZACWT_DOX)
  # Paths to the include directories used by api/Doxyfile.in
  set(ZZACWT_INCLUDE_DIR          "${ZZACWT_SOURCE_DIR}/include")

  if(ZZACWT_PDF)
    set(_docs_pdf_flag PDF)
  else()
    set(_docs_pdf_flag "")
  endif()

  gm3zza_enable_docs(
    TARGET        ${TARGET}
    APP_NAME      ${APP_NAME}
    USERGUIDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/userguide"
    API_DIR       "${CMAKE_CURRENT_SOURCE_DIR}/api"
    API_FILES     ${HPPFILES}
    ${_docs_pdf_flag}
  )

  # Copy zzacommon API docs into the api html output tree
  zzacommon_copy_api_docs(${GM3ZZA_API_HTML_DIR}/zzacommon)
endif()

## Install
install(TARGETS ${TARGET} RUNTIME COMPONENT applications)
gm3zza_install_runtime_files(FILES ${DATAFILES} COMPONENT data)

if(MSVC)
  zzacommon_install_runtime_dlls(DESTINATION bin COMPONENT application)
  gm3zza_install_registered_runtime_dlls()
endif()


# Install userguide and API documentation
if(ZZACWT_DOX)
    if(GM3ZZA_USERGUIDE_HTML_DIR)
      gm3zza_install_runtime_files(
        DIRECTORIES ${GM3ZZA_USERGUIDE_HTML_DIR}
        DEST_SUBDIR userguide
        COMPONENT documentation
      )
    endif()

    if(ZZACWT_PDF AND GM3ZZA_USERGUIDE_PDF_FILE)
      gm3zza_install_runtime_files(
        FILES ${GM3ZZA_USERGUIDE_PDF_FILE}
        DEST_SUBDIR userguide
        COMPONENT documentation
      )
    endif()

    if(GM3ZZA_API_HTML_DIR)
      gm3zza_install_runtime_files(
        DIRECTORIES ${GM3ZZA_API_HTML_DIR}
        DEST_SUBDIR api
        COMPONENT documentation
      )
    endif()
endif()
