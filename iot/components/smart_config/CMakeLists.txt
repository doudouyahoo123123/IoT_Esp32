set(smart_config "src/smart_config.c")
set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/protocol_examples_common)

idf_component_register(SRCS "${smart_config}"
                    INCLUDE_DIRS "include")