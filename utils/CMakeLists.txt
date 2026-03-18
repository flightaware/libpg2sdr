add_executable(pg2-util
  log.c device.c io.c image.c hotplug.c dfu_load.c mem_load.c nanojson.c meta.c
  load.c write.c device-info.c image-info.c reset.c standby.c blink.c
  pg2-util.c)
target_compile_options(pg2-util PRIVATE -O3 -Wall -Wmissing-declarations -Werror)
target_link_libraries(pg2-util pg2sdr_core)
