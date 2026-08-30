cmake_minimum_required(VERSION 3.25)

set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "macOS architectures" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET 13.0 CACHE STRING "Minimum macOS version" FORCE)
set(MAX_SDK_PACKAGE_OUT_OF_TREE ON CACHE BOOL "Build an assembled Max package" FORCE)

set(MAX_SDK_BASE_PATH "" CACHE PATH "Cycling '74 max-sdk-base checkout")
set(MEDIAPIPE_SOURCE_PATH "" CACHE PATH "MediaPipe source checkout")
set(RUNTIME_PACKAGE_PATH "" CACHE PATH "Directory containing the unified MediaPipe runtime and models")

if(NOT EXISTS "${MAX_SDK_BASE_PATH}/CMakeLists.txt")
    message(FATAL_ERROR "Set MAX_SDK_BASE_PATH to max-sdk-base")
endif()
if(NOT EXISTS "${MEDIAPIPE_SOURCE_PATH}/mediapipe/tasks/c/vision/hand_landmarker/hand_landmarker.h")
    message(FATAL_ERROR "Set MEDIAPIPE_SOURCE_PATH to the MediaPipe checkout")
endif()
if(NOT EXISTS "${RUNTIME_PACKAGE_PATH}/support/libmediapipe.dylib")
    message(FATAL_ERROR "Set RUNTIME_PACKAGE_PATH to the unified tracking runtime")
endif()

add_subdirectory("${MAX_SDK_BASE_PATH}" "${CMAKE_BINARY_DIR}/max-sdk-base")
project(JitTrackers VERSION 1.1.0 LANGUAGES C CXX)

set(AUTHOR_DOMAIN "com.sebastiantomczak")
set(BUNDLE_IDENTIFIER "jit-trackers")
set(GIT_VERSION_TAG "1.1.0")
set(COPYRIGHT_STRING "2026 Sebastian Tomczak")
add_subdirectory(source)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/package/support")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/package/help")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/package/licenses")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/package/third_party_sources")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/package/patches")
foreach(item
        support/hand_landmarker.task
        support/pose_landmarker_lite.task
        support/face_landmarker.task
        support/libmediapipe.dylib
        support/libopencv_core.3.4.dylib
        support/libopencv_imgproc.3.4.dylib)
    configure_file("${RUNTIME_PACKAGE_PATH}/${item}"
                   "${CMAKE_BINARY_DIR}/package/${item}" COPYONLY)
endforeach()
file(COPY "${CMAKE_SOURCE_DIR}/licenses/"
     DESTINATION "${CMAKE_BINARY_DIR}/package/licenses")
file(COPY "${CMAKE_SOURCE_DIR}/third_party_sources/"
     DESTINATION "${CMAKE_BINARY_DIR}/package/third_party_sources")
file(COPY "${CMAKE_SOURCE_DIR}/patches/"
     DESTINATION "${CMAKE_BINARY_DIR}/package/patches")
foreach(document
        LICENSE
        THIRD_PARTY_NOTICES.md
        MODEL_PROVENANCE.md
        SOURCE_OFFER.md
        PRIVACY.md
        DISTRIBUTION.md)
    configure_file("${CMAKE_SOURCE_DIR}/${document}"
                   "${CMAKE_BINARY_DIR}/package/${document}" COPYONLY)
endforeach()
configure_file("${CMAKE_SOURCE_DIR}/help/jit.handpose.maxhelp"
               "${CMAKE_BINARY_DIR}/package/help/jit.handpose.maxhelp" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/help/jit.pose.maxhelp"
               "${CMAKE_BINARY_DIR}/package/help/jit.pose.maxhelp" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/help/jit.facemesh.maxhelp"
               "${CMAKE_BINARY_DIR}/package/help/jit.facemesh.maxhelp" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/package-info.json"
               "${CMAKE_BINARY_DIR}/package/package-info.json" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/readme.md"
               "${CMAKE_BINARY_DIR}/package/readme.md" COPYONLY)

add_custom_target(jit_trackers_finalize ALL
    # A configure-only change can regenerate these plists without relinking the
    # bundles. Always install the current metadata immediately before signing.
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_BINARY_DIR}/source/jit.handpose_Info.plist"
            "$<TARGET_BUNDLE_DIR:jit.handpose>/Contents/Info.plist"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_BINARY_DIR}/source/jit.pose/jit.pose_Info.plist"
            "$<TARGET_BUNDLE_DIR:jit.pose>/Contents/Info.plist"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_BINARY_DIR}/source/jit.facemesh/jit.facemesh_Info.plist"
            "$<TARGET_BUNDLE_DIR:jit.facemesh>/Contents/Info.plist"
    COMMAND /usr/bin/codesign --force --sign -
            "${CMAKE_BINARY_DIR}/package/support/libopencv_core.3.4.dylib"
    COMMAND /usr/bin/codesign --force --sign -
            "${CMAKE_BINARY_DIR}/package/support/libopencv_imgproc.3.4.dylib"
    COMMAND /usr/bin/codesign --force --sign -
            "${CMAKE_BINARY_DIR}/package/support/libmediapipe.dylib"
    COMMAND /usr/bin/codesign --force --deep --sign -
            "$<TARGET_BUNDLE_DIR:jit.handpose>"
    COMMAND /usr/bin/codesign --force --deep --sign -
            "$<TARGET_BUNDLE_DIR:jit.pose>"
    COMMAND /usr/bin/codesign --force --deep --sign -
            "$<TARGET_BUNDLE_DIR:jit.facemesh>"
    DEPENDS jit.handpose jit.pose jit.facemesh
    VERBATIM)
