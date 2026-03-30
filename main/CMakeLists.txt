set(SYNTH_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../synth)

idf_component_register(
    SRCS
        "main.cpp"
        "${SYNTH_DIR}/adsr.cpp"
        "${SYNTH_DIR}/oscillator.cpp"
        "${SYNTH_DIR}/voice.cpp"
        "${SYNTH_DIR}/synth.cpp"
    INCLUDE_DIRS
        "."
        "${SYNTH_DIR}"
    PRIV_REQUIRES
        espressif__tinyusb
        driver
        usb
        esp_timer
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE SYNTH_N_VOICES=9)

# Expose main/'s directory to TinyUSB so it can find tusb_config.h during its own compilation
idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)
target_include_directories(${tusb_lib} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
