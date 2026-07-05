set(_GLOBAL_INTERNAL_INSTALL_MANIFEST "")

set(_GLOBAL_INTERNAL_INSTALL_MANIFEST "")

function(install)
    _install(${ARGV})

    list(GET ARGV 0 INSTALL_TYPE)

    if(INSTALL_TYPE STREQUAL "TARGETS")
        set(TRACKED_TARGETS "")
        set(DEFAULT_DEST "bin")

        set(KEYWORDS "DESTINATION" "EXPORT" "PERMISSIONS" "CONFIGURATIONS" "COMPONENT" "NAMELINK_COMPONENT"
                     "RUNTIME" "LIBRARY" "ARCHIVE" "INCLUDES" "PRIVATE_HEADER" "PUBLIC_HEADER" "RESOURCE")

        math(EXPR LAST_IDX "${ARGC} - 1")
        foreach(i RANGE 1 ${LAST_IDX})
            list(GET ARGV ${i} ARG)
            if(ARG IN_LIST KEYWORDS)
                break()
            else()
                list(APPEND TRACKED_TARGETS "${ARG}")
            endif()
        endforeach()

        set(TARGET_DEST "")

        foreach(BLOCK RUNTIME LIBRARY ARCHIVE)
            list(FIND ARGV "${BLOCK}" BLOCK_IDX)
            if(BLOCK_IDX GREATER -1)
                math(EXPR DEST_KW_IDX "${BLOCK_IDX} + 1")
                if(${DEST_KW_IDX} LESS ${ARGC})
                    list(GET ARGV ${DEST_KW_IDX} NEXT_ARG)
                    if(NEXT_ARG STREQUAL "DESTINATION")
                        math(EXPR DEST_VAL_IDX "${DEST_KW_IDX} + 1")
                        list(GET ARGV ${DEST_VAL_IDX} TARGET_DEST)
                    endif()
                endif()
            endif()
        endforeach()

        if(NOT TARGET_DEST)
            list(FIND ARGV "DESTINATION" GLOBAL_DEST_IDX)
            if(GLOBAL_DEST_IDX GREATER -1)
                math(EXPR GLOBAL_DEST_VAL_IDX "${GLOBAL_DEST_IDX} + 1")
                list(GET ARGV ${GLOBAL_DEST_VAL_IDX} TARGET_DEST)
            endif()
        endif()

        if(NOT TARGET_DEST)
            set(TARGET_DEST "${DEFAULT_DEST}")
        endif()

        foreach(tgt IN LISTS TRACKED_TARGETS)
            if(TARGET ${tgt})
                list(APPEND _GLOBAL_INTERNAL_INSTALL_MANIFEST "${TARGET_DEST}/$<TARGET_FILE_NAME:${tgt}>")
            endif()
        endforeach()
    else()
        set(options EXPORT)
        set(oneValueArgs DESTINATION RENAME COMPONENT)
        set(multiValueArgs FILES DIRECTORY PROGRAMS)
        cmake_parse_arguments(PARSE_ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGV})

        if(PARSE_ARG_DESTINATION)
            set(DEST "${PARSE_ARG_DESTINATION}")
        else()
            set(DEST "share")
        endif()

        if(PARSE_ARG_FILES OR PARSE_ARG_PROGRAMS)
            foreach(fl IN LISTS PARSE_ARG_FILES PARSE_ARG_PROGRAMS)
                get_filename_component(fl_name "${fl}" NAME)
                list(APPEND _GLOBAL_INTERNAL_INSTALL_MANIFEST "${DEST}/${fl_name}")
            endforeach()
        endif()

        if(PARSE_ARG_DIRECTORY)
            foreach(dir IN LISTS PARSE_ARG_DIRECTORY)
                get_filename_component(dir_name "${dir}" NAME)
                list(APPEND _GLOBAL_INTERNAL_INSTALL_MANIFEST "${DEST}/${dir_name}/")
            endforeach()
        endif()
    endif()

    set(_GLOBAL_INTERNAL_INSTALL_MANIFEST "${_GLOBAL_INTERNAL_INSTALL_MANIFEST}" PARENT_SCOPE)
endfunction()

function(qt_target_add_install_manifest TARGET_NAME ALIAS_PATH)
    set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated_manifest_${TARGET_NAME}")
    set(MANIFEST_FILE "${GEN_DIR}/install_manifest.txt")
    set(QRC_FILE "${GEN_DIR}/resources_${TARGET_NAME}.qrc")

    file(MAKE_DIRECTORY "${GEN_DIR}")

    string(REPLACE ";" "\n" MANIFEST_CONTENT "${_GLOBAL_INTERNAL_INSTALL_MANIFEST}")

    file(GENERATE
        OUTPUT "${MANIFEST_FILE}"
        CONTENT "${MANIFEST_CONTENT}\n"
    )

    file(GENERATE OUTPUT "${QRC_FILE}" CONTENT
"<!DOCTYPE RCC><RCC version=\"1.0\">
<qresource prefix=\"/\">
    <file alias=\"${ALIAS_PATH}\">${MANIFEST_FILE}</file>
</qresource>
</RCC>\n"
    )

    set(MANIFEST_TARGET_NAME "${TARGET_NAME}_generate_install_manifest")
    add_custom_target(${MANIFEST_TARGET_NAME} ALL
        DEPENDS "${MANIFEST_FILE}" "${QRC_FILE}"
        COMMENT "Generating install manifest for ${TARGET_NAME}..."
    )

    get_target_property(USER_TARGET_FOLDER ${TARGET_NAME} AUTOGEN_TARGETS_FOLDER)
    if(NOT USER_TARGET_FOLDER)
        get_property(USER_TARGET_FOLDER GLOBAL PROPERTY AUTOGEN_TARGETS_FOLDER)
    endif()
    if(NOT USER_TARGET_FOLDER AND DEFINED AUTOGEN_TARGETS_FOLDER)
        set(USER_TARGET_FOLDER "${AUTOGEN_TARGETS_FOLDER}")
    endif()
    if(USER_TARGET_FOLDER)
        set_target_properties(${MANIFEST_TARGET_NAME} PROPERTIES
            FOLDER "${USER_TARGET_FOLDER}"
        )
    endif()

    get_target_property(USER_SOURCE_GROUP ${TARGET_NAME} AUTOGEN_SOURCE_GROUP)
    if(NOT USER_TARGET_FOLDER)
        get_property(USER_SOURCE_GROUP GLOBAL PROPERTY AUTOGEN_SOURCE_GROUP)
    endif()
    if(NOT USER_SOURCE_GROUP AND DEFINED AUTOGEN_SOURCE_GROUP)
        set(USER_SOURCE_GROUP "${AUTOGEN_SOURCE_GROUP}")
    endif()
    if(USER_SOURCE_GROUP)
        source_group("${USER_SOURCE_GROUP}" FILES "${QRC_FILE}")
    endif()

    set_source_files_properties("${QRC_FILE}" PROPERTIES
        QT_RESOURCE_ALIAS "install_manifest_resources.qrc"
    )
    target_sources(${TARGET_NAME} PRIVATE "${QRC_FILE}")
    qt_add_resources(${TARGET_NAME} "install_manifest_resources"
        FILES "${QRC_FILE}"
    )
    add_dependencies(${TARGET_NAME} ${MANIFEST_TARGET_NAME})
endfunction()
