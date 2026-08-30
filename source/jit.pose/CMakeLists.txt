set_property(DIRECTORY PROPERTY max::external YES)
set(BUNDLE_IDENTIFIER "jit-pose")
project(jit.pose)

target_sources(jit.pose PRIVATE jit.pose.cpp)
target_include_directories(jit.pose PRIVATE "${MEDIAPIPE_SOURCE_PATH}")
target_link_libraries(jit.pose PRIVATE mediapipe_tasks)
set_target_properties(jit.pose PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    BUILD_WITH_INSTALL_RPATH ON
    BUILD_RPATH "@loader_path/../../../../support"
    INSTALL_RPATH "@loader_path/../../../../support")

add_custom_command(TARGET jit.pose POST_BUILD
    COMMAND /usr/bin/codesign --force --deep --sign -
            "$<TARGET_BUNDLE_DIR:jit.pose>"
    VERBATIM)
