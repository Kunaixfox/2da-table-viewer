cmake_minimum_required(VERSION 3.20)
project(da-table-viewer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# Source files
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/FamilyPanel.cpp
    src/TablePanel.cpp
    src/DetailsPanel.cpp
    src/FfiWrapper.cpp
)

set(HEADERS
    src/MainWindow.h
    src/FamilyPanel.h
    src/TablePanel.h
    src/DetailsPanel.h
    src/FfiWrapper.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Link Qt
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets)

# Link Rust FFI library
# On Windows, link to da_ffi.dll.lib (import library)
# The DLL must be in PATH or next to the exe at runtime
set(RUST_FFI_DIR "${CMAKE_SOURCE_DIR}/../target/release")
target_link_directories(${PROJECT_NAME} PRIVATE ${RUST_FFI_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE da_ffi.dll)

# Include path for FFI header
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Copy DLL to output directory after build
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RUST_FFI_DIR}/da_ffi.dll"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMENT "Copying da_ffi.dll to output directory"
)

# Install rules
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# Use windeployqt for Windows deployment
if(WIN32)
    find_program(WINDEPLOYQT windeployqt HINTS "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${WINDEPLOYQT} --no-translations --no-system-d3d-compiler
                    --no-network --no-opengl-sw
                    "$<TARGET_FILE:${PROJECT_NAME}>"
            COMMENT "Running windeployqt..."
        )
    endif()
endif()
