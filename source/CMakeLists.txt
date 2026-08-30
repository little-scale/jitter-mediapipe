set_property(DIRECTORY PROPERTY max::external YES)
set(BUNDLE_IDENTIFIER "jit-handpose")
project(jit.handpose)

add_library(mediapipe_tasks SHARED IMPORTED GLOBAL)
set_target_properties(mediapipe_tasks PROPERTIES
    IMPORTED_LOCATION "${RUNTIME_PACKAGE_PATH}/support/libmediapipe.dylib")

target_sources(jit.handpose PRIVATE jit.handpose.cpp)
target_include_directories(jit.handpose PRIVATE "${MEDIAPIPE_SOURCE_PATH}")
target_link_libraries(jit.handpose PRIVATE mediapipe_tasks)
set_target_properties(jit.handpose PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    BUILD_WITH_INSTALL_RPATH ON
    BUILD_RPATH "@loader_path/../../../../support"
    INSTALL_RPATH "@loader_path/../../../../support")

add_custom_command(TARGET jit.handpose POST_BUILD
    COMMAND /usr/bin/codesign --force --deep --sign -
            "$<TARGET_BUNDLE_DIR:jit.handpose>"
    VERBATIM)

add_subdirectory(jit.pose)
add_subdirectory(jit.facemesh)
