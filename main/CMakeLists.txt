# CMakeLists.txt
# Build configuration for main application

idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        system_config
)
