
function(group_files sources)
	foreach(FILE ${sources})
		# Get the directory of the source file
		get_filename_component(PARENT_DIR "${FILE}" DIRECTORY)

		# Remove common directory prefix to make the group
		string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" GROUP "${PARENT_DIR}")

		# Make sure we are using windows slashes
		string(REPLACE "/" "\\" GROUP "${GROUP}")

		# Strip the root parts for each possible component
		if("${FILE}" MATCHES "source/app/.*")
			string(LENGTH "source/app/." STR_LEN)
			string(SUBSTRING ${GROUP} ${STR_LEN} -1 GROUP)
		elseif("${FILE}" MATCHES "source/version/.*")
			string(LENGTH "source/version/." STR_LEN)
			string(SUBSTRING ${GROUP} ${STR_LEN} -1 GROUP)
		elseif("${FILE}" MATCHES "source/lib/include/.*")
			string(LENGTH "source/lib/include/." STR_LEN)
			string(SUBSTRING ${GROUP} ${STR_LEN} -1 GROUP)
		elseif("${FILE}" MATCHES "source/shlib_types/.*")
			string(LENGTH "source/shlib_types/." STR_LEN)
			string(SUBSTRING ${GROUP} ${STR_LEN} -1 GROUP)
		elseif("${FILE}" MATCHES "source/lib/source/.*")
			string(LENGTH "source/lib/source/." STR_LEN)
			string(SUBSTRING ${GROUP} ${STR_LEN} -1 GROUP)
		endif()

		# Do the grouping
		source_group("${GROUP}" FILES "${FILE}")
	endforeach()
endfunction()
