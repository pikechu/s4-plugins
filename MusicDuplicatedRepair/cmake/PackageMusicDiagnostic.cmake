foreach(required ASI_PATH INI_PATH PACKAGE_ROOT ZIP_PATH)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "Missing package argument: ${required}")
  endif()
endforeach()
if(NOT EXISTS "${ASI_PATH}" OR NOT EXISTS "${INI_PATH}")
  message(FATAL_ERROR "MusicLoopRepair binary or example INI is missing")
endif()
file(REMOVE_RECURSE "${PACKAGE_ROOT}")
file(MAKE_DIRECTORY "${PACKAGE_ROOT}/Plugins/MusicLoopRepair")
configure_file("${ASI_PATH}" "${PACKAGE_ROOT}/Plugins/MusicLoopRepair.asi"
               COPYONLY)
configure_file("${INI_PATH}"
               "${PACKAGE_ROOT}/Plugins/MusicLoopRepair/MusicLoopRepair.ini.example"
               COPYONLY)
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E tar cf "${ZIP_PATH}" --format=zip
    "Plugins/MusicLoopRepair.asi"
    "Plugins/MusicLoopRepair/MusicLoopRepair.ini.example"
  WORKING_DIRECTORY "${PACKAGE_ROOT}"
  RESULT_VARIABLE archive_result
  ERROR_VARIABLE archive_error
)
if(NOT archive_result EQUAL 0)
  message(FATAL_ERROR "Music diagnostic package failed: ${archive_error}")
endif()
file(SHA256 "${ZIP_PATH}" archive_hash)
file(SHA256 "${ASI_PATH}" asi_hash)
file(SHA256 "${INI_PATH}" ini_hash)
file(WRITE "${ZIP_PATH}.sha256"
  "${archive_hash}  MusicLoopRepair-diagnostic.zip\n"
  "${asi_hash}  Plugins/MusicLoopRepair.asi\n"
  "${ini_hash}  Plugins/MusicLoopRepair/MusicLoopRepair.ini.example\n"
)
message(STATUS "Music observation package: ${ZIP_PATH}")
