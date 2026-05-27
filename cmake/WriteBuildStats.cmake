string(TIMESTAMP NOW "%Y-%m-%d %H:%M:%S")
file(SIZE "${TARGET_FILE}" FILE_SIZE)

if(FILE_SIZE GREATER_EQUAL 1048576)
    math(EXPR SIZE_MB "${FILE_SIZE} / 1048576")
    math(EXPR SIZE_REM "(${FILE_SIZE} % 1048576) * 100 / 1048576")
    set(PRETTY_SIZE "${SIZE_MB}.${SIZE_REM} MB")
elseif(FILE_SIZE GREATER_EQUAL 1024)
    math(EXPR SIZE_KB "${FILE_SIZE} / 1024")
    set(PRETTY_SIZE "${SIZE_KB} KB")
else()
    set(PRETTY_SIZE "${FILE_SIZE} B")
endif()

get_filename_component(EXE_NAME "${TARGET_FILE}" NAME)

set(ENTRY "[${NOW}] ${TARGET_NAME} (${BUILD_CONFIG}) | ${EXE_NAME} | ${PRETTY_SIZE} (${FILE_SIZE} bytes)\n")

file(APPEND "${STATS_FILE}" "${ENTRY}")