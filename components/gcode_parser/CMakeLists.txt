# CMakeLists.txt
# Build configuration for G-Code parser component

idf_component_register(
    SRCS "gcode_parser.c"
    INCLUDE_DIRS "include"
    REQUIRES stepper_motor soldering_iron
)
