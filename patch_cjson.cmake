file(READ "${CMAKE_BINARY_DIR}/_deps/cjson-src/CMakeLists.txt" CJSON_CMAKE)
string(REGEX REPLACE "cmake_minimum_required\\(VERSION [0-9]+\\.[0-9]+(\\.[0-9]+)?\\)" 
                     "cmake_minimum_required(VERSION 3.5)" 
                     CJSON_CMAKE "${CJSON_CMAKE}")
file(WRITE "${CMAKE_BINARY_DIR}/_deps/cjson-src/CMakeLists.txt" "${CJSON_CMAKE}")