add_executable(pg2-rx pg2-rx.c)
target_compile_options(pg2-rx PRIVATE -O3 -Wall -Wmissing-declarations -Werror)
target_link_libraries(pg2-rx pg2sdr m)

include(GNUInstallDirs)
install(
  TARGETS pg2-rx
    COMPONENT Binaries
)
