set(COMPONENT_NAME isa)

if(PROT_BUILD_TESTS)
    prot_add_component_utest(${COMPONENT_NAME} PROT::isa)
endif()
