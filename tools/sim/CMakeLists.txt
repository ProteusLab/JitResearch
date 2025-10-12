add_executable(prot_sim_app sim_app.cpp)
target_link_libraries(
  prot_sim_app PRIVATE CLI11::CLI11 PROT::defaults PROT::interpreter
                       PROT::elf_loader PROT::hart fmt::fmt)
set_target_properties(prot_sim_app PROPERTIES OUTPUT_NAME psim)
install(TARGETS prot_sim_app)
