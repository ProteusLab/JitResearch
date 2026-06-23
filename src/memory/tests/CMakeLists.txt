set(COMPONENT_NAME memory)

if(BUILD_TESTS)
    prot_add_component_utest(${COMPONENT_NAME} PROT::memory)
endif()
