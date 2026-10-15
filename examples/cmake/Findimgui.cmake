include(FetchContent)

if (NOT TARGET imgui)
    FetchContent_Declare(
        imgui
        # Dear ImGui + stack layout + the two patches of misc/imgui_patches (see docs/fork_imgui_bundle.md),
        # at a fixed tag: the branches "layouts" and "imgui_bundle" are rebased from time to time.
        # Upstream fetches thedmd/imgui b085dd8feac1595915d85243b5515af34a10a394 (feature/layout-external).
        GIT_REPOSITORY      https://github.com/pthom/imgui.git
        GIT_TAG             bundle_20261012b
    )

    FetchContent_MakeAvailable(imgui)

    file(GLOB SOURCES CONFIGURE_DEPENDS "${imgui_SOURCE_DIR}/*.cpp" "${imgui_SOURCE_DIR}/*.h")

    source_group(TREE ${imgui_SOURCE_DIR} FILES ${SOURCES})

    set(imgui_natvis_path "${imgui_SOURCE_DIR}/misc/debuggers/imgui.natvis")

    add_library(imgui STATIC ${SOURCES} ${imgui_natvis_path})

    target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
    )

    # They change the layout of ImGui's types, so everything built with ImGui sees them.
    set(IMGUI_NODE_EDITOR_EXAMPLES_DEFINITIONS "" CACHE STRING "Definitions for Dear ImGui and the examples, for example IMGUI_DISABLE_OBSOLETE_FUNCTIONS")
    target_compile_definitions(imgui PUBLIC ${IMGUI_NODE_EDITOR_EXAMPLES_DEFINITIONS})

    set_target_properties(
        imgui
        PROPERTIES
            FOLDER "external"
    )
endif()

FetchContent_GetProperties(imgui)

foreach(component ${imgui_FIND_COMPONENTS})

    if (TARGET imgui_${component})
        set(imgui_${component}_FOUND TRUE)
        continue()
    endif()

    file(GLOB COMPONENT_SOURCES CONFIGURE_DEPENDS "${imgui_SOURCE_DIR}/backends/imgui_impl_${component}*")
    if (NOT COMPONENT_SOURCES)
        set(imgui_${component}_FOUND FALSE)
        continue()
    endif()

    source_group(TREE ${imgui_SOURCE_DIR}/backends FILES ${COMPONENT_SOURCES})

    add_library(imgui_${component} STATIC ${COMPONENT_SOURCES})

    target_include_directories(imgui_${component} PUBLIC
        ${imgui_SOURCE_DIR}/backends
    )

    target_link_libraries(imgui_${component} PUBLIC imgui)

    if (component MATCHES "dx11")
        target_link_libraries(imgui_${component} PUBLIC d3d11)
    elseif (component MATCHES "opengl*")
        find_package(OpenGL REQUIRED COMPONENTS OpenGL)
        target_link_libraries(imgui_${component} PUBLIC OpenGL::GL)
    elseif(component STREQUAL "glfw")
        find_package(glfw REQUIRED)
        target_link_libraries(imgui_${component} PUBLIC glfw)
    endif()

    add_library(imgui::${component} ALIAS imgui_${component})

    set_target_properties(
        imgui_${component}
        PROPERTIES
            FOLDER "external"
    )

    set(imgui_${component}_FOUND TRUE)

endforeach()

if (TARGET imgui)
    set(imgui_LIBRARY imgui)
endif()

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
    imgui
    REQUIRED_VARS imgui_LIBRARY
    HANDLE_COMPONENTS
)

