cmake_minimum_required(VERSION 3.25)

# --------------------------------------------------------------------------
# Project
# --------------------------------------------------------------------------
project(KEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 单配置生成器(如 Ninja)时，默认给个可调试配置
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

# --------------------------------------------------------------------------
# 3rd: vcpkg packages
#   通过 CMakePresets.json 里的 CMAKE_TOOLCHAIN_FILE 使用 vcpkg
# --------------------------------------------------------------------------
find_package(SDL2 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

# --------------------------------------------------------------------------
# 3rd: bgfx (子模块) - 只开 tools(为了 shaderc)，关闭示例/自定义目标
#   你需要在仓库里有 extern/bgfx.cmake（以及其子模块 bx/bimg）
# --------------------------------------------------------------------------
set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_TOOLS    ON  CACHE BOOL "" FORCE)  # 需要 shaderc
set(BGFX_CUSTOM_TARGETS OFF CACHE BOOL "" FORCE)
add_subdirectory(extern/bgfx.cmake)

# --------------------------------------------------------------------------
# Sources
# --------------------------------------------------------------------------
file(GLOB KE_SOURCES
  src/*.cpp
  src/gfx/*.cpp
)
add_executable(KEngine ${KE_SOURCES})
target_include_directories(KEngine PRIVATE src)

target_link_libraries(KEngine PRIVATE
  SDL2::SDL2main SDL2::SDL2
  spdlog::spdlog fmt::fmt
  bgfx bimg bx
)

if(MSVC)
  target_compile_definitions(KEngine PRIVATE _CRT_SECURE_NO_WARNINGS)
  # 源码用 UTF-8（避免中文路径/注释产生告警）
  target_compile_options(KEngine PRIVATE /utf-8)
endif()

# --------------------------------------------------------------------------
# Shaders: 统一从 ${PROJECT_SOURCE_DIR}/shaders 编译到
#          ${CMAKE_BINARY_DIR}/shaders/{dx11|glsl|spirv}
#   需要在源码目录下放置:
#     shaders/varying.def.sc
#     shaders/vs_simple.sc
#     shaders/fs_simple.sc
# --------------------------------------------------------------------------
# ---- shader 输出根目录/包含路径（如你已在上文定义，可略） ----
set(SHADER_DIR ${CMAKE_SOURCE_DIR}/shaders)
set(SHADER_OUT ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_OUT})
set(BGFX_DIR ${CMAKE_SOURCE_DIR}/extern/bgfx.cmake/bgfx)
set(BGFX_SHADER_INCLUDE ${BGFX_DIR}/src)

function(bgfx_shader_multi OUT NAME TYPE)
  # 统一使用 D3D11 的 s_5_0（阶段由 --type 决定）
  set(D3D_PROFILE s_5_0)

  # D3D11
  add_custom_command(
    OUTPUT ${SHADER_OUT}/dx11/${NAME}.bin
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUT}/dx11
    COMMAND $<TARGET_FILE:shaderc>
        -f ${SHADER_DIR}/${NAME}.sc
        -o ${SHADER_OUT}/dx11/${NAME}.bin
        --type ${TYPE} --platform windows --profile s_5_0
        --entry main
        --varyingdef ${SHADER_DIR}/varying.def.sc
        -i ${BGFX_SHADER_INCLUDE}

    DEPENDS ${SHADER_DIR}/${NAME}.sc ${SHADER_DIR}/varying.def.sc shaderc
    COMMENT "Compiling ${NAME}.sc -> dx11/${NAME}.bin"
  )

        # Vulkan (SPIR-V)
        add_custom_command(
        OUTPUT ${SHADER_OUT}/spirv/${NAME}.bin
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUT}/spirv
        COMMAND $<TARGET_FILE:shaderc>
                -f ${SHADER_DIR}/${NAME}.sc
                -o ${SHADER_OUT}/spirv/${NAME}.bin
                --type ${TYPE} --platform linux --profile spirv
                --entry main
                --varyingdef ${SHADER_DIR}/varying.def.sc
                -i ${BGFX_SHADER_INCLUDE}
        DEPENDS ${SHADER_DIR}/${NAME}.sc ${SHADER_DIR}/varying.def.sc shaderc
        COMMENT "Generating shaders/spirv/${NAME}.bin"
        )

        # OpenGL (GLSL 1.50)
        add_custom_command(
        OUTPUT ${SHADER_OUT}/glsl/${NAME}.bin
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUT}/glsl
        COMMAND $<TARGET_FILE:shaderc>
                -f ${SHADER_DIR}/${NAME}.sc
                -o ${SHADER_OUT}/glsl/${NAME}.bin
                --type ${TYPE} --platform linux --profile 150
                --entry main
                --varyingdef ${SHADER_DIR}/varying.def.sc
                -i ${BGFX_SHADER_INCLUDE}
        DEPENDS ${SHADER_DIR}/${NAME}.sc ${SHADER_DIR}/varying.def.sc shaderc
        COMMENT "Generating shaders/glsl/${NAME}.bin"
        )

  list(APPEND _outs
    ${SHADER_OUT}/dx11/${NAME}.bin
    ${SHADER_OUT}/spirv/${NAME}.bin
    ${SHADER_OUT}/glsl/${NAME}.bin
  )
  set(${OUT} "${_outs}" PARENT_SCOPE)
endfunction()



bgfx_shader_multi(VS_BINS vs_simple v)
bgfx_shader_multi(FS_BINS fs_simple f)
set(SHADER_BINARIES ${VS_BINS} ${FS_BINS})

add_custom_target(build_shaders ALL DEPENDS ${SHADER_BINARIES})
add_dependencies(KEngine build_shaders)

# 运行时让程序知道 shader 输出目录
target_compile_definitions(KEngine PRIVATE KE_SHADER_DIR=\"${SHADER_OUT}\")
add_dependencies(KEngine build_shaders)
# === 把编译好的着色器复制到可执行文件旁边 ===
add_custom_command(TARGET KEngine POST_BUILD
  # 目标目录：<exe同级>/shaders/dx11
  COMMAND ${CMAKE_COMMAND} -E make_directory
          "$<TARGET_FILE_DIR:KEngine>/shaders/dx11"
  # 整个目录拷贝（包含你后续新增的 .bin）
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${SHADER_OUT}/dx11" "$<TARGET_FILE_DIR:KEngine>/shaders/dx11"
)

# --------------------------------------------------------------------------
# （可选）把源码分组到 IDE 过滤器
# --------------------------------------------------------------------------
source_group(TREE ${CMAKE_SOURCE_DIR}/src FILES ${KE_SOURCES})
