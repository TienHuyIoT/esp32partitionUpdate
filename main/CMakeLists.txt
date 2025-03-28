idf_component_register(SRCS "main.cpp"
                    "growPartition/growPartition.cpp"
                    INCLUDE_DIRS ".")

# Register Kconfig file for this component
set(COMPONENT_KCONFIG ${CMAKE_CURRENT_LIST_DIR}/Kconfig)
