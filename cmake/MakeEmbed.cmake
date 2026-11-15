file(READ ${INPUT_FILE} EMBED_CONTENTS)
file(WRITE ${OUTPUT_FILE}
    "const char* k${VAR_NAME} = R\"(${EMBED_CONTENTS})\";\n"
)