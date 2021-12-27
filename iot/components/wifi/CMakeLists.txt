set(wifi "src/wifi.c")
set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/protocol_examples_common)

idf_component_register(SRCS "${wifi}"
                    INCLUDE_DIRS "include")