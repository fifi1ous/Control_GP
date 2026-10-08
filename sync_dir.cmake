# Incremental directory sync using robocopy.
# Only copies files that have changed (newer timestamp or different size).
# robocopy exit codes 0-7 = success, 8+ = error.
execute_process(
    COMMAND robocopy "${SRC}" "${DST}" /MIR /NJH /NJS /NP /NFL /NDL
    RESULT_VARIABLE rc
)
if(rc GREATER 7)
    message(FATAL_ERROR "robocopy failed syncing ${SRC} -> ${DST} (exit code ${rc})")
endif()
