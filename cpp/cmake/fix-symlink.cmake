# On Windows hosts, takes a symlink at SYMLINK_PATH and rewrites its target path
# using native path separators.
# Has no effect on other hosts.
if(CMAKE_HOST_WIN32)
    file(READ_SYMLINK "${SYMLINK_PATH}" SYMLINK_TARGET_PATH)

    cmake_path(NATIVE_PATH SYMLINK_TARGET_PATH SYMLINK_TARGET_NATIVE_PATH)

    file(CREATE_LINK "${SYMLINK_TARGET_NATIVE_PATH}" "${SYMLINK_PATH}" SYMBOLIC)
endif()
