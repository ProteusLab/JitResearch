set(COMPONENT_NAME memory)

if(BUILD_TESTS)
    find_package(GTest REQUIRED)

    prot_add_component_utest(${COMPONENT_NAME})

endif()
