vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO alandtse/CommonLibSSE-NG
    REF 39f9d07a6ffabea8fb559eee87ab7d27cd463e8a
    SHA512 712374255cf5da060715f24faabe07a581208f6ea5998ced00bbff340916d77f379441a1a0972388c4c7b0909bddbbf99cc5c2c99819e3d76fce8fe61270b579
)

set(OPENVR_REVISION 60eb187801956ad277f1cae6680e3a410ee0873b)
set(OPENVR_SOURCE_PATH "${SOURCE_PATH}/extern/openvr")
function(hcse_download_openvr_file path checksum)
    string(REPLACE "/" "-" filename "${path}")
    vcpkg_download_distfile(download
        URLS "https://raw.githubusercontent.com/ValveSoftware/openvr/${OPENVR_REVISION}/${path}"
        FILENAME "ValveSoftware-openvr-${OPENVR_REVISION}-${filename}"
        SHA512 "${checksum}")
    get_filename_component(directory "${path}" DIRECTORY)
    get_filename_component(name "${path}" NAME)
    file(INSTALL "${download}" DESTINATION "${OPENVR_SOURCE_PATH}/${directory}" RENAME "${name}")
endfunction()
hcse_download_openvr_file(headers/openvr.h
    d7a7644a7f532b920f59d831a982d0fce5868ef78a9f45a5c3f2a39000c48d1fafa7d8918b2bf8b44082dcc442e49bad15a8815687c255bb1e1252690d54b2b2)
hcse_download_openvr_file(lib/win64/openvr_api.lib
    7c4e5eab580d30a2459f6748a877aa893794346e54bf08cb9b5147ef1dbc02dcbd61c60259c6af549efd42d647742d95f79cd84a1a2001e8c0fdabeb749d8e49)
hcse_download_openvr_file(LICENSE
    72dfce8b257ebbc3def90a3bd886de638fb7af9706efc0141d2e61ef7ba8828a42b1ce1ddc68ecd982a78caf380d3796164e5d5eb68f36cfb4ac73f7aaf5acac)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_TESTS=OFF
        -DENABLE_SKYRIM_SE=ON
        -DENABLE_SKYRIM_AE=ON
        -DENABLE_SKYRIM_VR=ON
        -DSKSE_SUPPORT_XBYAK=OFF
        -DSKSE_SUPPORT_PATCH_SAFETY=OFF
        -DCMAKE_TRY_COMPILE_CONFIGURATION=Release
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/CommonLibSSE)

file(INSTALL "${SOURCE_PATH}/cmake/CommonLibSSE.cmake" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
file(APPEND "${CURRENT_PACKAGES_DIR}/share/${PORT}/CommonLibSSEConfig.cmake" "\nfind_dependency(directxtk CONFIG)\n")
file(INSTALL "${OPENVR_SOURCE_PATH}/headers/openvr.h" DESTINATION "${CURRENT_PACKAGES_DIR}/include")
file(INSTALL "${OPENVR_SOURCE_PATH}/lib/win64/openvr_api.lib" DESTINATION "${CURRENT_PACKAGES_DIR}/lib")
if(NOT VCPKG_BUILD_TYPE)
    file(INSTALL "${OPENVR_SOURCE_PATH}/lib/win64/openvr_api.lib" DESTINATION "${CURRENT_PACKAGES_DIR}/debug/lib")
endif()
vcpkg_replace_string(
    "${CURRENT_PACKAGES_DIR}/share/${PORT}/CommonLibSSE-targets.cmake"
    "${SOURCE_PATH}/extern/openvr/lib/win64/openvr_api.lib"
    "\${_IMPORT_PREFIX}/lib/openvr_api.lib"
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYING.txt" "${OPENVR_SOURCE_PATH}/LICENSE")
