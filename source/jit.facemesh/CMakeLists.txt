set_property(DIRECTORY PROPERTY max::external YES)
set(BUNDLE_IDENTIFIER "jit-facemesh")
project(jit.facemesh)

target_sources(jit.facemesh PRIVATE jit.facemesh.cpp)
target_include_directories(jit.facemesh PRIVATE "${MEDIAPIPE_SOURCE_PATH}")
target_link_libraries(jit.facemesh PRIVATE mediapipe_tasks)
set_target_properties(jit.facemesh PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    BUILD_WITH_INSTALL_RPATH ON
    BUILD_RPATH "@loader_path/../../../../support"
    INSTALL_RPATH "@loader_path/../../../../support")

add_custom_command(TARGET jit.facemesh POST_BUILD
    COMMAND /usr/bin/codesign --force --deep --sign -
            "$<TARGET_BUNDLE_DIR:jit.facemesh>"
    VERBATIM)
