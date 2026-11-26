cmake_minimum_required(VERSION 3.20)
include("${CMAKE_CURRENT_LIST_DIR}/WioVmNativePaths.cmake")
if(NOT WIO_VM_INSTALL_MANIFEST OR NOT WIO_VM_INSTALL_STAGE OR NOT WIO_VM_INSTALL_DESTINATION OR
   NOT WIO_VM_INSTALL_DEPLOY OR NOT DEFINED CMAKE_INSTALL_PREFIX)
    message(FATAL_ERROR "Invalid VM native install arguments")
endif()
# The prefix is chosen by cmake --install --prefix at install time. DESTDIR is
# only a staging prefix; it is never recorded in module code or bundle metadata.
set(root "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}")
file(MAKE_DIRECTORY "${root}")
set(destination "${root}/${WIO_VM_INSTALL_DESTINATION}")
wio_vm_native_inside("${root}" "${destination}")
file(MAKE_DIRECTORY "${destination}")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DMANIFEST=${WIO_VM_INSTALL_MANIFEST}"
    "-DSTAGE=${WIO_VM_INSTALL_STAGE}" "-DDESTINATION=${destination}" -P "${WIO_VM_INSTALL_DEPLOY}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "VM native install failed before image installation: ${output}\n${error}")
endif()
# Record the data files as well as the image in CMake's install manifest. This
# list is metadata only; this helper never uninstalls or collects generations.
file(READ "${WIO_VM_INSTALL_MANIFEST}" document)
string(JSON bundle GET "${document}" bundle)
string(JSON count LENGTH "${document}" files)
if(count GREATER 0)
    math(EXPR last "${count} - 1")
    foreach(index RANGE 0 ${last})
        string(JSON name GET "${document}" files ${index} name)
        list(APPEND CMAKE_INSTALL_MANIFEST_FILES "${CMAKE_INSTALL_PREFIX}/${WIO_VM_INSTALL_DESTINATION}/${bundle}/${name}")
    endforeach()
endif()
list(APPEND CMAKE_INSTALL_MANIFEST_FILES
    "${CMAKE_INSTALL_PREFIX}/${WIO_VM_INSTALL_DESTINATION}/${bundle}/wio-native-manifest.json")
