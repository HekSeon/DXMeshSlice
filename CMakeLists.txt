cmake_minimum_required(VERSION 3.20)
set(CMAKE_SYSTEM_VERSION "10.0.26100.0" CACHE STRING "" FORCE)
project(DXMeshSlice CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS "src/*.h" "src/*.hpp")

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src FILES ${SOURCES} ${HEADERS})

add_executable(DXMeshSlice ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# DX11 sistem kütüphaneleri
target_link_libraries(${PROJECT_NAME} PRIVATE
    d3d11.lib
    dxgi.lib
    dxguid.lib
    d3dcompiler.lib
)

# DX12 sistem kütüphaneleri
target_link_libraries(${PROJECT_NAME} PRIVATE
    d3d12.lib
    dxgi.lib
    dxguid.lib
    d3dcompiler.lib
)

# Debug modda D3D12 debug layer için ek tanım (isteğe bağlı)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:_DEBUG>
)

# Çalışma dizini ayarı (Visual Studio için)
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

target_compile_definitions(DXMeshSlice PRIVATE UNICODE _UNICODE)