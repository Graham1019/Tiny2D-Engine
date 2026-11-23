# Compare the actual display output (capture runs before swap) on the same driver.
file(MAKE_DIRECTORY "${STAGE}")
foreach(take IN ITEMS first second other_seed)
    set(seed 71)
    if(take STREQUAL "other_seed")
        set(seed 72)
    endif()
    execute_process(COMMAND "${EXE}" --scene particles --seed "${seed}" --frames 60 --size 320x200
        --particle-script demo --capture "${STAGE}/${take}.png"
        RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "Particle capture failed ${rc} ${out} ${err}")
    endif()
    file(SHA256 "${STAGE}/${take}.png" "${take}")
endforeach()
if(NOT first STREQUAL second OR first STREQUAL other_seed)
    message(FATAL_ERROR "Finite capture determinism: ${first} ${second} ${other_seed}")
endif()
