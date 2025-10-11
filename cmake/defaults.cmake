add_library(prot_defaults INTERFACE)
set_target_properties(prot_defaults PROPERTIES CXX_EXTENSIONS OFF)
target_compile_features(prot_defaults INTERFACE cxx_std_20)

# Compile stuff
target_compile_options(prot_defaults INTERFACE -Wall -Wextra -Wpedantic
                                               -Wold-style-cast -Wvla)
if(PROT_ENABLE_WERROR)
  message(STATUS "-Werror enabled, have fun :)")
  target_compile_options(prot_defaults INTERFACE -Werror)
endif()

add_library(PROT::defaults ALIAS prot_defaults)
