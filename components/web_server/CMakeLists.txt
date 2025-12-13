# CMakeLists.txt
# Build configuration for web server component

idf_component_register(
    SRCS "web_server.cpp"
    INCLUDE_DIRS "include"
    EMBED_FILES
        "../../web_interface/index.html"
        "../../web_interface/manual.html"
        "../../web_interface/exec.html"
        "../../web_interface/style.css"
        "../../web_interface/app.js"
        "../../web_interface/manual.js"
        "../../web_interface/exec.js"
        "../../web_interface/board_visualization.js"
        "../../web_interface/board_canvas.js"
        "../../web_interface/machine_canvas.js"
    REQUIRES esp_http_server fsm_controller gcode_parser execution_fsm
)
