if(NOT DEFINED BPR_APPLICATION OR NOT DEFINED BPR_ROOT)
    message(FATAL_ERROR "queue composition contract requires application and root")
endif()

file(REMOVE_RECURSE "${BPR_ROOT}")
file(MAKE_DIRECTORY "${BPR_ROOT}")

foreach(course IN ITEMS seed reopen)
    execute_process(
        COMMAND "${BPR_APPLICATION}" "${course}" "${BPR_ROOT}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
        TIMEOUT 25
    )
    string(TOUPPER "${course}" marker)
    if(NOT result EQUAL 0 OR NOT output MATCHES "QUEUE_COMPOSITION_${marker}_OK")
        message(FATAL_ERROR "queue ${course} failed: ${output}${error}")
    endif()
endforeach()

message("QUEUE_COMPOSITION_CONTRACT_OK")
