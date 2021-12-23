set(sensors_rs485 "src/sensors_rs485.c")
set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/protocol_examples_common)

idf_component_register(SRCS "${sensors_rs485}"
                    INCLUDE_DIRS "include")