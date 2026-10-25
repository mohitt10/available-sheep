cmake_minimum_required(VERSION 3.16)
project(available_sheep LANGUAGES CXX)

add_library(expression_dag src/expression_dag.cpp)
target_include_directories(expression_dag PUBLIC include)
target_compile_features(expression_dag PUBLIC cxx_std_17)

add_library(avl_tree src/Avltree.cpp)
target_include_directories(avl_tree PUBLIC include)
target_compile_features(avl_tree PUBLIC cxx_std_17)

add_library(identifier_trie src/identifier_trie.cpp)
target_include_directories(identifier_trie PUBLIC include)
target_compile_features(identifier_trie PUBLIC cxx_std_17)

add_library(ir src/Ir.cpp src/Interpreter.cpp)
target_include_directories(ir PUBLIC include)
target_compile_features(ir PUBLIC cxx_std_17)

include(CTest)
if(BUILD_TESTING)
    add_executable(expression_dag_test tests/expression_dag_test.cpp)
    target_link_libraries(expression_dag_test PRIVATE expression_dag)
    add_test(NAME expression_dag_test COMMAND expression_dag_test)

    add_executable(avl_test tests/avl_test.cpp)
    target_link_libraries(avl_test PRIVATE avl_tree)
    add_test(NAME avl_test COMMAND avl_test)

    add_executable(identifier_trie_test tests/identifier_trie_test.cpp)
    target_link_libraries(identifier_trie_test PRIVATE identifier_trie)
    add_test(NAME identifier_trie_test COMMAND identifier_trie_test)

    add_executable(ir_test tests/Irtests.cpp)
    target_link_libraries(ir_test PRIVATE ir avl_tree)
    add_test(NAME ir_test COMMAND ir_test)
endif()