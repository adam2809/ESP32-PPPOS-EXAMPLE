# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/main/*.*)

idf_build_get_property(project_dir PROJECT_DIR)
idf_component_register(
    SRCS ${app_sources}
    EMBED_TXTFILES server_root_cert.pem
)
