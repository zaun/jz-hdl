file(READ "${INPUT_FILE}" JSON_HEX HEX)

get_filename_component(GUARD_NAME "${OUTPUT_FILE}" NAME_WE)
string(TOUPPER "${GUARD_NAME}" GUARD_NAME)
string(REGEX REPLACE "[^A-Za-z0-9_]" "_" GUARD_NAME "${GUARD_NAME}")
set(GUARD_NAME "GENERATED_${GUARD_NAME}_H")

set(VAR_NAME_SANITIZED "${VAR_NAME}")
string(REGEX REPLACE "[^A-Za-z0-9_]" "_" VAR_NAME_SANITIZED "${VAR_NAME_SANITIZED}")

string(REGEX REPLACE "([0-9A-Fa-f][0-9A-Fa-f])" "0x\\1," JSON_BYTES "${JSON_HEX}")
string(REGEX REPLACE "(.{1,80})" "\\1\n" JSON_BYTES "${JSON_BYTES}")

file(WRITE "${OUTPUT_FILE}" 
"#ifndef ${GUARD_NAME}
#define ${GUARD_NAME}

const unsigned char ${VAR_NAME_SANITIZED}[] = {
${JSON_BYTES}0x00
};

#endif
")
