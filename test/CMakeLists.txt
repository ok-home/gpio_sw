idf_component_register( 
    SRCS "test_gpio_sw.c"
    INCLUDE_DIRS "." 
    REQUIRES unity gpio_sw
    )
target_link_libraries ( ${COMPONENT_LIB}  INTERFACE  "-Wl,--wrap=gpio_config" )
target_link_libraries ( ${COMPONENT_LIB}  INTERFACE  "-Wl,--wrap=gpio_get_level" )
target_link_libraries ( ${COMPONENT_LIB}  INTERFACE  "-Wl,--wrap=gpio_set_level" )
target_link_libraries ( ${COMPONENT_LIB}  INTERFACE  "-Wl,--wrap=gpio_isr_handler_add" )



