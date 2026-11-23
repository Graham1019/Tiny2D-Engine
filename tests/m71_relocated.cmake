# Exercise the shipped directory layout, including whitespace, from a foreign cwd.
file(MAKE_DIRECTORY "${STAGE}/Lab Demo/assets" "${STAGE}/cwd")
file(COPY "${EXE}" DESTINATION "${STAGE}/Lab Demo")
get_filename_component(bin "${EXE}" DIRECTORY)
file(COPY "${bin}/assets/shaders" DESTINATION "${STAGE}/Lab Demo/assets")
set(shader "${STAGE}/Lab Demo/assets/shaders/display.frag")
# Explicit refresh even after the previous run deliberately corrupted this copy.
configure_file("${bin}/assets/shaders/display.frag" "${shader}" COPYONLY)
execute_process(COMMAND "${STAGE}/Lab Demo/tiny2d_lab" --scene neon --frames 9 --seed 0 --bloom off --view blur
    WORKING_DIRECTORY "${STAGE}/cwd" RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0 OR NOT out MATCHES "status=ok frames=9 ticks=9 seed=0.*view=blur bloom=off.*fullscreen=4")
    message(FATAL_ERROR "relocated run: ${rc} ${out} ${err}")
endif()
set(normal_shader "${STAGE}/Lab Demo/assets/shaders/normal.frag")
file(WRITE "${normal_shader}" "invalid normal shader")
execute_process(COMMAND "${STAGE}/Lab Demo/tiny2d_lab" --frames 1 --lighting on
    WORKING_DIRECTORY "${STAGE}/cwd" RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0 OR NOT out MATCHES "status=ok frames=1 ticks=1")
    message(FATAL_ERROR "radial path with unavailable normal shader: ${rc} ${out} ${err}")
endif()
execute_process(COMMAND "${STAGE}/Lab Demo/tiny2d_lab" --frames 1 --lighting on --normal-mapping on
    WORKING_DIRECTORY "${STAGE}/cwd" RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(rc EQUAL 0 OR NOT err MATCHES "normal.frag: compilation failed" OR NOT out MATCHES "status=error frames=0 ticks=0")
    message(FATAL_ERROR "normal shader diagnostic: ${rc} ${out} ${err}")
endif()
configure_file("${bin}/assets/shaders/normal.frag" "${normal_shader}" COPYONLY)
execute_process(COMMAND "${STAGE}/Lab Demo/tiny2d_lab" --frames 1 --lighting on --normal-mapping on --view normal
    WORKING_DIRECTORY "${STAGE}/cwd" RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0 OR NOT out MATCHES "status=ok frames=1 ticks=1.*view=normal")
    message(FATAL_ERROR "relocated normal view: ${rc} ${out} ${err}")
endif()
file(WRITE "${shader}" "invalid shader")
execute_process(COMMAND "${STAGE}/Lab Demo/tiny2d_lab" --frames 1 WORKING_DIRECTORY "${STAGE}/cwd"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(rc EQUAL 0 OR NOT err MATCHES "display.frag: compilation failed" OR NOT out MATCHES "status=error frames=0 ticks=0")
    message(FATAL_ERROR "shader diagnostic: ${rc} ${out} ${err}")
endif()
