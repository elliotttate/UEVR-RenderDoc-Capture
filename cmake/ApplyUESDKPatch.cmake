# ApplyUESDKPatch.cmake
#
# UESDK (praydog's Unreal SDK) is a PRIVATE submodule, so it cannot be forked
# publicly to carry our AFW/embedded-RenderDoc fixes. Instead we apply the fixes
# to the builder's OWN UESDK checkout at configure time. Anyone building UEVR
# already has legitimate UESDK access (it is required to build at all), so this
# republishes nothing -- it is just a local working-tree patch.
#
# The patches:
# - teach FRHITexture::get_native_resource()'s vtable probe to accept RenderDoc-
#   wrapped D3D12 resources (whose vtable lives in renderdoc.dll).
# - make UE5 FRenderTargetPool::FindFreeElement scanning robust enough to find
#   the named SceneDepthZ/SceneVelocity resources AFW needs without DLSS.
#
# Must run BEFORE add_subdirectory(dependencies/submodules/UESDK) so the patched
# file is on disk before it is compiled. Idempotent: skips if already applied,
# warns (never fails) on upstream drift.

set(_uesdk_dir   "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/submodules/UESDK")
set(_uesdk_patches
    "embedded-RenderDoc native resource wrapper|${CMAKE_CURRENT_SOURCE_DIR}/patches/UESDK-StereoStuff-renderdoc.patch"
    "UE5 FRenderTargetPool AFW resource scan|${CMAKE_CURRENT_SOURCE_DIR}/patches/UESDK-FRenderTargetPool-ue5-afw.patch")

if(NOT EXISTS "${_uesdk_dir}/src/sdk")
    message(STATUS "UESDK patches: ${_uesdk_dir}/src/sdk not found (submodule not checked out?) -- skipping")
else()
    find_package(Git QUIET)
    if(NOT GIT_EXECUTABLE)
        set(GIT_EXECUTABLE git)
    endif()

    foreach(_uesdk_patch_spec IN LISTS _uesdk_patches)
        string(REPLACE "|" ";" _uesdk_patch_parts "${_uesdk_patch_spec}")
        list(GET _uesdk_patch_parts 0 _uesdk_patch_label)
        list(GET _uesdk_patch_parts 1 _uesdk_patch)

        if(NOT EXISTS "${_uesdk_patch}")
            message(WARNING "UESDK ${_uesdk_patch_label} patch: ${_uesdk_patch} missing")
            continue()
        endif()

        # Already applied? A clean reverse-apply check means the fix is present.
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${_uesdk_dir}" apply --reverse --check "${_uesdk_patch}"
            RESULT_VARIABLE _uesdk_rev_rc OUTPUT_QUIET ERROR_QUIET)

        if(_uesdk_rev_rc EQUAL 0)
            message(STATUS "UESDK ${_uesdk_patch_label} patch: already applied")
            continue()
        endif()

        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${_uesdk_dir}" apply --check "${_uesdk_patch}"
            RESULT_VARIABLE _uesdk_fwd_rc OUTPUT_QUIET ERROR_QUIET)
        if(_uesdk_fwd_rc EQUAL 0)
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" -C "${_uesdk_dir}" apply "${_uesdk_patch}"
                RESULT_VARIABLE _uesdk_ap_rc)
            if(_uesdk_ap_rc EQUAL 0)
                message(STATUS "UESDK ${_uesdk_patch_label} patch: APPLIED")
            else()
                message(WARNING "UESDK ${_uesdk_patch_label} patch: failed to apply")
            endif()
        else()
            message(WARNING "UESDK ${_uesdk_patch_label} patch: does not apply cleanly to this UESDK "
                            "revision (upstream drift?). See ${_uesdk_patch}")
        endif()
    endforeach()
endif()
