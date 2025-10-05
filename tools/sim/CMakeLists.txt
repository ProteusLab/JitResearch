add_executable(prot_sim_app sim_app.cpp)
target_link_libraries(prot_sim_app PRIVATE CLI11::CLI11 PROT::defaults)
set_target_properties(prot_sim_app PROPERTIES OUTPUT_NAME psim)
install(TARGETS prot_sim_app)
