vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO google/filament
    REF v1.68.5
    SHA512 10821eaff12710df9f008d48436365067f7abb8b34dd614cbfa82dea2802f1e0a5e2bbed2a5e29d22da1e083c50f0e60b19853cac019898591c4f150f7ea10b3
)

vcpkg_replace_string(
    "${SOURCE_PATH}/CMakeLists.txt"
    "option(FILAMENT_SKIP_SDL2 \"Skip dependencies of SDL2, and SDL2\" OFF)"
    "option(FILAMENT_SKIP_SDL2 \"Skip dependencies of SDL2, and SDL2\" OFF)\noption(FILAMENT_SKIP_TOOLS \"Don't build tools\" OFF)"
)

vcpkg_replace_string(
    "${SOURCE_PATH}/CMakeLists.txt"
    "    add_subdirectory(${TOOLS}/cmgen)"
    "    if (NOT FILAMENT_SKIP_TOOLS)\n    add_subdirectory(${TOOLS}/cmgen)"
)

vcpkg_replace_string(
    "${SOURCE_PATH}/CMakeLists.txt"
    "    add_subdirectory(${TOOLS}/specgen)"
    "    add_subdirectory(${TOOLS}/specgen)\n    endif()"
)

vcpkg_replace_string(
    "${SOURCE_PATH}/CMakeLists.txt"
    "    export(TARGETS matc cmgen filamesh mipgen resgen uberz glslminifier FILE ${IMPORT_EXECUTABLES})"
    "    if (NOT FILAMENT_SKIP_TOOLS)\n        export(TARGETS matc cmgen filamesh mipgen resgen uberz glslminifier FILE ${IMPORT_EXECUTABLES})\n    endif()"
)

vcpkg_configure_cmake(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DFILAMENT_SKIP_SAMPLES=ON
        -DFILAMENT_SKIP_TOOLS=ON
        -DFILAMENT_SUPPORTS_OPENGL=ON
        -DFILAMENT_SUPPORTS_VULKAN=OFF
        -DFILAMENT_SUPPORTS_METAL=OFF
        -DFILAMENT_SKIP_SDL2=ON
        -DFILAMENT_BUILD_FILAMAT=OFF
        -DFILAMENT_ENABLE_MATDBG=OFF
        -DFILAMENT_ENABLE_LTO=OFF
        -DFILAMENT_SKIP_TOOLS=ON
        -DFILAMENT_ENABLE_EXPERIMENTAL_GCC_SUPPORT=ON
        -DDIST_DIR=.
)

vcpkg_install_cmake()
vcpkg_copy_pdbs()

vcpkg_install_copyright(
    FILE_LIST
        "${SOURCE_PATH}/LICENSE"
)
