# CMakeLists.txt
# Build configuration for web server component

idf_component_register(
    SRCS "web_server.cpp"
    INCLUDE_DIRS "include"
    EMBED_FILES
        "../../web_interface/index.html"
        "../../web_interface/style.css"
        "../../web_interface/app.js"
    REQUIRES esp_http_server fsm_controller gcode_parser
)
