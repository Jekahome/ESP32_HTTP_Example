
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

FILE(GLOB src_files ${PROJECT_DIR}/main/*.cpp)

idf_component_register(SRCS ${src_files}
INCLUDE_DIRS "."
EMBED_FILES "winter.png"
EMBED_TXTFILES "certs/cacert.pem" "certs/prvtkey.pem"
#REQUIRES tcp_transport nvs_flash esp_http_server esp_https_server sdmmc json esp_http_client fatfs driver esp_netif esp_wifi esp_eth
)

 
# https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#controlling-component-compilation
# https://cmake.org/cmake/help/v3.5/command/target_compile_options.html
target_compile_options(${COMPONENT_LIB} PUBLIC  -Wno-unused-variable
                                                -Wno-unused-parameter
                                                -Wno-unused-but-set-variable
                                                -Wno-error=unused-label
                                                -Wno-error=address 
                                                -Wno-address
                                                -Wno-missing-field-initializers
                                                -Wno-unused-function)

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
target_compile_options(${COMPONENT_LIB} PUBLIC -D CONFIG_BLINK_GPIO=2)


 