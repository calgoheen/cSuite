cmake_minimum_required(VERSION 3.22.1)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum OSX deployment target")

file(STRINGS "VERSION" CurrentVersion)

# Remove prerelease suffix from VERSION string for CMake project version
string(REGEX MATCH "^[0-9]+(\\.[0-9]+)*" ProjectVersion "${CurrentVersion}")

project("cSuite" VERSION ${ProjectVersion})

include(FetchContent)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.8)
FetchContent_MakeAvailable(JUCE)

FetchContent_Declare(chowdsp_utils
    GIT_REPOSITORY https://github.com/Chowdhury-DSP/chowdsp_utils
    GIT_TAG ffc70ba399f9afaeefb996eb14e55a1d487270b8)
FetchContent_MakeAvailable(chowdsp_utils)

# Ignore warnings from chowdsp
file(GLOB chowdspModules "${chowdsp_utils_SOURCE_DIR}/modules/*/chowdsp_*")
foreach(module IN LISTS chowdspModules)
    get_filename_component(target "${module}" NAME)
    if(TARGET ${target})
        get_target_property(includes ${target} INTERFACE_INCLUDE_DIRECTORIES)
        set_target_properties(${target} PROPERTIES
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${includes}")
    endif()
endforeach()

FetchContent_Declare(clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG ea834dfd366ab2103b98e92eaacb581c69fc2e0b)
FetchContent_MakeAvailable(clap-juce-extensions)

add_subdirectory(cgo_modules)

juce_add_plugin(${PROJECT_NAME}
    COMPANY_NAME CalGoheen
    COMPANY_WEBSITE "https://calgoheen.com"
    COMPANY_EMAIL "calgoheen@gmail.com"
    BUNDLE_ID com.CalGoheen.${PROJECT_NAME}
    PLUGIN_MANUFACTURER_CODE Calg
    PLUGIN_CODE Suit
    IS_SYNTH FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    FORMATS AU VST3 Standalone)

clap_juce_extensions_plugin(TARGET ${PROJECT_NAME}
    CLAP_ID "com.CalGoheen.${PROJECT_NAME}"
    CLAP_FEATURES "audio effect" "mono" "stereo")

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

file(GLOB_RECURSE SourceFiles CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h")

target_sources(${PROJECT_NAME} PRIVATE ${SourceFiles})

juce_add_binary_data(${PROJECT_NAME}_Assets
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets/font.ttf"
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets/font_bold.ttf")

target_compile_definitions(${PROJECT_NAME}
    PUBLIC
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0)

if(MSVC)
    target_compile_definitions(${PROJECT_NAME}
        PUBLIC
            WIN32_LEAN_AND_MEAN
            NOMINMAX)
endif()

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ${PROJECT_NAME}_Assets
        juce::juce_audio_utils
        juce::juce_dsp
        chowdsp::chowdsp_dsp_utils
        chowdsp::chowdsp_filters
        chowdsp::chowdsp_waveshapers
        cgo::cgo_dsp
        cgo::cgo_gui
        cgo::cgo_plugin
        cgo::cgo_graph

    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

juce_generate_juce_header(${PROJECT_NAME})
