find_package(Python COMPONENTS Interpreter)

set(BREEZE_STYLES "dark-blue;light-blue;dark-pink;light-pink")
set(BREEZE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/submodules/BreezeStyleSheets")
set(BREEZE_CONFIGURE "${BREEZE_SOURCE_DIR}/configure.py")

set(BREEZE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/breeze_resources")
set(BREEZE_QRC "${BREEZE_OUTPUT_DIR}/breeze.qrc")
set(BREEZE_STYLES_QRC "${BREEZE_OUTPUT_DIR}/breeze_styles.qrc")

set(STYLES_QRC_CONTENT "<!DOCTYPE RCC>\n")
set(STYLES_QRC_CONTENT "${STYLES_QRC_CONTENT}<RCC>\n")
set(STYLES_QRC_CONTENT "${STYLES_QRC_CONTENT}  <qresource>\n")
foreach(STYLE_NAME IN LISTS BREEZE_STYLES)
    set(STYLES_QRC_CONTENT "${STYLES_QRC_CONTENT}    <file alias=\"res/styles/Breeze ${STYLE_NAME}.qss\">${BREEZE_OUTPUT_DIR}/${STYLE_NAME}/stylesheet.qss</file>\n")
endforeach()
set(STYLES_QRC_CONTENT "${STYLES_QRC_CONTENT}  </qresource>\n")
set(STYLES_QRC_CONTENT "${STYLES_QRC_CONTENT}</RCC>\n")

set(TEMP_QRC_PATH "${CMAKE_CURRENT_BINARY_DIR}/breeze_styles.qrc.tmp")

file(WRITE "${TEMP_QRC_PATH}" "${STYLES_QRC_CONTENT}")
configure_file("${TEMP_QRC_PATH}" "${BREEZE_STYLES_QRC}" COPYONLY)

set(BREEZE_CONFIGURE_OUTPUTS "${BREEZE_QRC}")
foreach(STYLE_NAME IN LISTS BREEZE_STYLES)
    set(BREEZE_CONFIGURE_OUTPUTS "${BREEZE_CONFIGURE_OUTPUTS};${BREEZE_OUTPUT_DIR}/${STYLE_NAME}/stylesheet.qss")
endforeach()

string(REPLACE ";" "," BREEZE_STYLES "${BREEZE_STYLES}")
add_custom_command(
    OUTPUT ${BREEZE_CONFIGURE_OUTPUTS}
    COMMAND ${Python_EXECUTABLE} ${BREEZE_CONFIGURE}
            --styles=${BREEZE_STYLES} --resource=breeze.qrc
            --output-dir="${BREEZE_OUTPUT_DIR}"
    WORKING_DIRECTORY ${BREEZE_SOURCE_DIR}
    DEPENDS ${BREEZE_CONFIGURE} ${BREEZE_STYLES_QRC}
    COMMENT "Generating Breeze themes (if modified)")

add_library(breeze STATIC "${BREEZE_QRC}" "${BREEZE_STYLES_QRC}")

set_target_properties(breeze PROPERTIES
	FOLDER 3rdparty)
