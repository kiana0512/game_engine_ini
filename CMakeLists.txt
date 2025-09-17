cmake_minimum_required(VERSION 3.25)
project(KEngine LANGUAGES CXX)

# --------------------------------------------------------------------------
# C++ 标准
# --------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

# --------------------------------------------------------------------------
# 第三方库（vcpkg）
# --------------------------------------------------------------------------
find_package(SDL2 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
# glm：优先 config 包，找不到则头文件兜底
find_package(glm CONFIG QUIET)

# --------------------------------------------------------------------------
# bgfx 子模块（只开 tools 用于 shaderc）
# --------------------------------------------------------------------------
set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_TOOLS    ON  CACHE BOOL "" FORCE)
set(BGFX_CUSTOM_TARGETS OFF CACHE BOOL "" FORCE)
add_subdirectory(extern/bgfx.cmake EXCLUDE_FROM_ALL)

# --------------------------------------------------------------------------
# 源码
# --------------------------------------------------------------------------
file(GLOB_RECURSE KE_SOURCES CONFIGURE_DEPENDS
  src/*.cpp
)
add_executable(KEngine ${KE_SOURCES})

target_include_directories(KEngine PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/extern/stb
  ${CMAKE_SOURCE_DIR}/extern/tinygltf
)

# --------------------------------------------------------------------------
# 连接三方库
# --------------------------------------------------------------------------
if(TARGET bgfx::bgfx)
  set(_bgfx bgfx::bgfx)
else()
  set(_bgfx bgfx)
endif()
if(TARGET bimg::bimg)
  set(_bimg bimg::bimg)
else()
  set(_bimg bimg)
endif()
if(TARGET bx::bx)
  set(_bx bx::bx)
else()
  set(_bx bx)
endif()

target_link_libraries(KEngine PRIVATE
  SDL2::SDL2main SDL2::SDL2
  spdlog::spdlog fmt::fmt
  ${_bgfx} ${_bimg} ${_bx}
  nlohmann_json::nlohmann_json
)

# glm 处理：优先 target，其次仅 include 目录
if(TARGET glm::glm)
  target_link_libraries(KEngine PRIVATE glm::glm)
elseif(TARGET glm::glm-header-only)
  target_link_libraries(KEngine PRIVATE glm::glm-header-only)
else()
  find_path(GLM_INCLUDE_DIR NAMES glm/glm.hpp
            PATHS $ENV{VCPKG_ROOT}/installed/$ENV{VCPKG_DEFAULT_TRIPLET}
            PATH_SUFFIXES include
            NO_DEFAULT_PATH)
  if(NOT GLM_INCLUDE_DIR)
    find_path(GLM_INCLUDE_DIR NAMES glm/glm.hpp) # 全局路径兜底
  endif()
  if(GLM_INCLUDE_DIR)
    message(STATUS "GLM headers: ${GLM_INCLUDE_DIR}")
    target_include_directories(KEngine PRIVATE ${GLM_INCLUDE_DIR})
  else()
    message(FATAL_ERROR "GLM not found. Install via: vcpkg install glm:x64-windows, or set GLM_INCLUDE_DIR.")
  endif()
endif()

# 兜底：极少数 bgfx 端口不传 include 目录
get_target_property(_bgfx_incs ${_bgfx} INTERFACE_INCLUDE_DIRECTORIES)
if(NOT _bgfx_incs)
  target_include_directories(KEngine PRIVATE
    ${CMAKE_SOURCE_DIR}/extern/bgfx.cmake/bgfx/include
    ${CMAKE_SOURCE_DIR}/extern/bgfx.cmake/bx/include
    ${CMAKE_SOURCE_DIR}/extern/bgfx.cmake/bimg/include
  )
endif()

# --------------------------------------------------------------------------
# MSVC 设置
# --------------------------------------------------------------------------
if(MSVC)
  target_compile_definitions(KEngine PRIVATE UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)
  target_compile_options(KEngine PRIVATE /utf-8)

  # VS 的 C 编译器 TryCompile 可能叠加 /utf-8 与 /source-charset:utf-8，做个温和规避
  string(REPLACE "/utf-8" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
endif()

# --------------------------------------------------------------------------
# Shaders（输出到 ${CMAKE_BINARY_DIR}/shaders/{dx11,spirv,glsl}）
# --------------------------------------------------------------------------
set(SHADER_DIR ${CMAKE_SOURCE_DIR}/shaders)
set(SHADER_OUT ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_OUT})
set(BGFX_DIR ${CMAKE_SOURCE_DIR}/extern/bgfx.cmake/bgfx)
set(BGFX_SHADER_INCLUDE ${BGFX_DIR}/src)

function(bgfx_shader_multi_with_varying OUT NAME TYPE VARYING_FILE)
  # DX11
  add_custom_command(
    OUTPUT ${SHADER_OUT}/dx11/${NAME}.bin
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUT}/dx11
    COMMAND $<TARGET_FILE:shaderc>
            -f ${SHADER_DIR}/${NAME}.sc
            -o ${SHADER_OUT}/dx11/${NAME}.bin
            --type ${TYPE} --platform windows --profile s_5_0
            --entry main
            --varyingdef ${VARYING_FILE}
            -i ${BGFX_SHADER_INCLUDE}
            -i ${SHADER_DIR}
    DEPENDS ${SHADER_DIR}/${NAME}.sc ${VARYING_FILE} shaderc
    COMMENT "Compiling ${NAME}.sc -> dx11/${NAME}.bin"
    VERBATIM
  )
  # SPIR-V
  add_custom_command(
    OUTPUT ${SHADER_OUT}/spirv/${NAME}.bin
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUT}/spirv
    COMMAND $<TARGET_FILE:shaderc>
            -f ${SHADER_DIR}/${NAME}.sc
            -o ${SHADER_OUT}/spirv/${NAME}.bin
            --type ${TYPE} --platform linux --profile spirv
            --entry main
            --varyingdef ${VARYING_FILE}
            -i ${BGFX_SHADER_INCLUDE}
            -i ${SHADER_DIR}
    DEPENDS ${SHADER_DIR}/${NAME}.sc ${VARYING_FILE} shaderc
    COMMENT "Compiling ${NAME}.sc -> spirv/${NAME}.bin"
    VERBATIM
  )
  # OpenGL 150
  add_custom_command(
    OUTPUT ${SHADER_OUT}/glsl/${NAME}.bin
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUT}/glsl
    COMMAND $<TARGET_FILE:shaderc>
            -f ${SHADER_DIR}/${NAME}.sc
            -o ${SHADER_OUT}/glsl/${NAME}.bin
            --type ${TYPE} --platform linux --profile 150
            --entry main
            --varyingdef ${VARYING_FILE}
            -i ${BGFX_SHADER_INCLUDE}
            -i ${SHADER_DIR}
    DEPENDS ${SHADER_DIR}/${NAME}.sc ${VARYING_FILE} shaderc
    COMMENT "Compiling ${NAME}.sc -> glsl/${NAME}.bin"
    VERBATIM
  )
  list(APPEND _outs
    ${SHADER_OUT}/dx11/${NAME}.bin
    ${SHADER_OUT}/spirv/${NAME}.bin
    ${SHADER_OUT}/glsl/${NAME}.bin
  )
  set(${OUT} "${_outs}" PARENT_SCOPE)
endfunction()

set(VARYING_FILE ${SHADER_DIR}/varying.def.sc)

# 旧演示路径
bgfx_shader_multi_with_varying(VS_SIMPLE_BINS vs_simple v ${VARYING_FILE})
bgfx_shader_multi_with_varying(FS_SIMPLE_BINS fs_simple f ${VARYING_FILE})
bgfx_shader_multi_with_varying(VS_TEX_BINS    vs_tex    v ${VARYING_FILE})
bgfx_shader_multi_with_varying(FS_TEX_BINS    fs_tex    f ${VARYING_FILE})
bgfx_shader_multi_with_varying(VS_MESH_BINS   vs_mesh   v ${VARYING_FILE})
bgfx_shader_multi_with_varying(FS_MESH_BINS   fs_mesh   f ${VARYING_FILE})

# Day6：PBR 前向管线
bgfx_shader_multi_with_varying(VS_PBR_BINS    vs_pbr    v ${VARYING_FILE})
bgfx_shader_multi_with_varying(FS_PBRMR_BINS  fs_pbr_mr f ${VARYING_FILE})

set(SHADER_BINARIES
  ${VS_SIMPLE_BINS} ${FS_SIMPLE_BINS}
  ${VS_TEX_BINS}    ${FS_TEX_BINS}
  ${VS_MESH_BINS}   ${FS_MESH_BINS}
  ${VS_PBR_BINS}    ${FS_PBRMR_BINS}
)

add_custom_target(build_shaders ALL DEPENDS ${SHADER_BINARIES})
add_dependencies(KEngine build_shaders)

# 运行时宏
target_compile_definitions(KEngine PRIVATE
  KE_SHADER_DIR=\"${SHADER_OUT}\"
  KE_ASSET_DIR=\"${CMAKE_SOURCE_DIR}/docs\"
)

# 拷贝 DX11 shader 到可执行目录
add_custom_command(TARGET KEngine POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:KEngine>/shaders/dx11"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${SHADER_OUT}/dx11" "$<TARGET_FILE_DIR:KEngine>/shaders/dx11"
  COMMENT "Copy dx11 shaders to output dir"
)

# --------------------------------------------------------------------------
# IDE 过滤器
# --------------------------------------------------------------------------
source_group(TREE ${CMAKE_SOURCE_DIR}/src FILES ${KE_SOURCES})
