cmake_minimum_required(VERSION 3.20)
project(vm_executable_cohort LANGUAGES CXX)
add_library(cohort_calc SHARED "${WIO_ROOT}/tests/native/vm_deployment_calc.cpp")
target_compile_definitions(cohort_calc PRIVATE WIO_CALC_BUILD WIO_DEPLOY_BIAS=100)
add_library(cohort_registry SHARED "${WIO_ROOT}/tests/native/vm_deployment_producer.cpp")
target_link_libraries(cohort_registry PRIVATE cohort_calc)
target_include_directories(cohort_registry PRIVATE "${WIO_ROOT}/sdk/include")
target_compile_features(cohort_registry PRIVATE cxx_std_20)
foreach(target IN ITEMS cohort_calc cohort_registry)
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/native/$<CONFIG>"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/native/$<CONFIG>"
        BUILD_RPATH_USE_ORIGIN TRUE)
endforeach()
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/paths-$<CONFIG>.cmake" CONTENT
"set(producer [==[$<TARGET_FILE:cohort_registry>]==])
set(helper [==[$<TARGET_FILE:cohort_calc>]==])
")
