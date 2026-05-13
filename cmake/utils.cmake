include(GoogleTest)

# Helpful macro to add unittest Accepts src_file (with utest) as mandatory
# argument and list of linked libraries after it
macro(prot_add_utest src_file)
  set(TEST_NAME "${src_file}_test")
  add_executable(${TEST_NAME} ${src_file})

  target_link_libraries(${TEST_NAME} PRIVATE PROT::defaults GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)
  foreach(arg IN LISTS ARGN)
    target_link_libraries(${TEST_NAME} PRIVATE ${arg})
  endforeach()
  gtest_discover_tests(
    ${TEST_NAME}
    EXTRA_ARGS --gtest_color=yes
    PROPERTIES LABELS unit)
endmacro()

function(prot_add_component_utest COMPONENT_NAME)
  file(GLOB TESTLIST RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "*.cc")

  foreach(TEST_SRC ${TESTLIST})
    set(TEST_NAME "${src_file}_test")
    prot_add_utest(${TEST_SRC} ARGN)

    set_target_properties(${TEST_NAME}
            PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/test/${COMPONENT_NAME}"
    )

  endforeach()
endfunction()
