cmake_minimum_required(VERSION 3.21)
project(SgsQtSpine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick OpenGL)
find_package(OpenGL REQUIRED)

qt_standard_project_setup()

set(SPINE_DIR ${CMAKE_SOURCE_DIR}/third_party/spine-c)

file(GLOB_RECURSE SPINE_SOURCES
    ${SPINE_DIR}/src/*.c
)

qt_add_executable(${PROJECT_NAME}
    main.cpp
    spineviewport.cpp
    spinerenderer.cpp
    spine_qt_bridge.cpp
    ${SPINE_SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${SPINE_DIR}/include
    ${SPINE_DIR}/src
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    NOMINMAX
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Quick
    Qt6::OpenGL
    OpenGL::GL
)

qt_add_resources(${PROJECT_NAME} qml_res
    PREFIX "/"
    FILES
        qml/main.qml
)
