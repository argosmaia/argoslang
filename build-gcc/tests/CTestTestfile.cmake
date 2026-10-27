# CMake generated Testfile for 
# Source directory: /home/dgti/Projetos/argoslang/tests
# Build directory: /home/dgti/Projetos/argoslang/build-gcc/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
include("/home/dgti/Projetos/argoslang/build-gcc/tests/argos_tests-b12d07c_include.cmake")
add_test([=[example_minimal_literal_check]=] "/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "check" "/home/dgti/Projetos/argoslang/examples/minimal_literal.argos")
set_tests_properties([=[example_minimal_literal_check]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;58;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_arithmetic_check]=] "/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "check" "/home/dgti/Projetos/argoslang/examples/minimal_arithmetic.argos")
set_tests_properties([=[example_minimal_arithmetic_check]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;58;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_branch_check]=] "/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "check" "/home/dgti/Projetos/argoslang/examples/minimal_branch.argos")
set_tests_properties([=[example_minimal_branch_check]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;58;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_loop_check]=] "/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "check" "/home/dgti/Projetos/argoslang/examples/minimal_loop.argos")
set_tests_properties([=[example_minimal_loop_check]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;58;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_optional_check]=] "/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "check" "/home/dgti/Projetos/argoslang/examples/minimal_optional.argos")
set_tests_properties([=[example_minimal_optional_check]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;58;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_call_check]=] "/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "check" "/home/dgti/Projetos/argoslang/examples/minimal_call.argos")
set_tests_properties([=[example_minimal_call_check]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;58;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_nested_check]=] "/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "check" "/home/dgti/Projetos/argoslang/examples/minimal_nested.argos")
set_tests_properties([=[example_minimal_nested_check]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;58;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_signed_check]=] "/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "check" "/home/dgti/Projetos/argoslang/examples/minimal_signed.argos")
set_tests_properties([=[example_minimal_signed_check]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;58;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_parameters_check]=] "/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "check" "/home/dgti/Projetos/argoslang/examples/minimal_parameters.argos")
set_tests_properties([=[example_minimal_parameters_check]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;58;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_print_int_check]=] "/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "check" "/home/dgti/Projetos/argoslang/examples/minimal_print_int.argos")
set_tests_properties([=[example_minimal_print_int_check]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;58;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_print_bool_check]=] "/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "check" "/home/dgti/Projetos/argoslang/examples/minimal_print_bool.argos")
set_tests_properties([=[example_minimal_print_bool_check]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;58;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_print_string_check]=] "/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "check" "/home/dgti/Projetos/argoslang/examples/minimal_print_string.argos")
set_tests_properties([=[example_minimal_print_string_check]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;58;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_boolean_call_check]=] "/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "check" "/home/dgti/Projetos/argoslang/examples/minimal_boolean_call.argos")
set_tests_properties([=[example_minimal_boolean_call_check]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;58;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_control_flow_check]=] "/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "check" "/home/dgti/Projetos/argoslang/examples/minimal_control_flow.argos")
set_tests_properties([=[example_minimal_control_flow_check]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;58;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_real_division_check]=] "/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "check" "/home/dgti/Projetos/argoslang/examples/minimal_real_division.argos")
set_tests_properties([=[example_minimal_real_division_check]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;58;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_string_values_check]=] "/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "check" "/home/dgti/Projetos/argoslang/examples/minimal_string_values.argos")
set_tests_properties([=[example_minimal_string_values_check]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;58;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_short_circuit_check]=] "/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "check" "/home/dgti/Projetos/argoslang/examples/minimal_short_circuit.argos")
set_tests_properties([=[example_minimal_short_circuit_check]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;58;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_closures_check]=] "/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "check" "/home/dgti/Projetos/argoslang/examples/minimal_closures.argos")
set_tests_properties([=[example_minimal_closures_check]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;58;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_objects_check]=] "/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "check" "/home/dgti/Projetos/argoslang/examples/minimal_objects.argos")
set_tests_properties([=[example_minimal_objects_check]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;58;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_properties_check]=] "/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "check" "/home/dgti/Projetos/argoslang/examples/minimal_properties.argos")
set_tests_properties([=[example_minimal_properties_check]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;58;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_object_closure_check]=] "/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "check" "/home/dgti/Projetos/argoslang/examples/minimal_object_closure.argos")
set_tests_properties([=[example_minimal_object_closure_check]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;58;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_frontend_inheritance_check]=] "/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "check" "/home/dgti/Projetos/argoslang/examples/minimal_frontend_inheritance.argos")
set_tests_properties([=[example_minimal_frontend_inheritance_check]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;58;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_frontend_bound_method_check]=] "/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "check" "/home/dgti/Projetos/argoslang/examples/minimal_frontend_bound_method.argos")
set_tests_properties([=[example_minimal_frontend_bound_method_check]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;58;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_modulo_by_zero_vm]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_modulo_by_zero.argos" "-P" "/home/dgti/Projetos/argoslang/tests/runtime_error.cmake")
set_tests_properties([=[example_minimal_modulo_by_zero_vm]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;64;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_modulo_overflow_vm]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_modulo_overflow.argos" "-P" "/home/dgti/Projetos/argoslang/tests/runtime_error.cmake")
set_tests_properties([=[example_minimal_modulo_overflow_vm]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;71;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_literal_vm]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_literal.argos" "-DEXPECTED=42" "-DEXPECTED_STDOUT=" "-P" "/home/dgti/Projetos/argoslang/tests/run_example.cmake")
set_tests_properties([=[example_minimal_literal_vm]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;129;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_arithmetic_vm]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_arithmetic.argos" "-DEXPECTED=14" "-DEXPECTED_STDOUT=" "-P" "/home/dgti/Projetos/argoslang/tests/run_example.cmake")
set_tests_properties([=[example_minimal_arithmetic_vm]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;129;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_branch_vm]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_branch.argos" "-DEXPECTED=7" "-DEXPECTED_STDOUT=" "-P" "/home/dgti/Projetos/argoslang/tests/run_example.cmake")
set_tests_properties([=[example_minimal_branch_vm]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;129;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_loop_vm]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_loop.argos" "-DEXPECTED=6" "-DEXPECTED_STDOUT=" "-P" "/home/dgti/Projetos/argoslang/tests/run_example.cmake")
set_tests_properties([=[example_minimal_loop_vm]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;129;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_call_vm]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_call.argos" "-DEXPECTED=42" "-DEXPECTED_STDOUT=" "-P" "/home/dgti/Projetos/argoslang/tests/run_example.cmake")
set_tests_properties([=[example_minimal_call_vm]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;129;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_nested_vm]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_nested.argos" "-DEXPECTED=11" "-DEXPECTED_STDOUT=" "-P" "/home/dgti/Projetos/argoslang/tests/run_example.cmake")
set_tests_properties([=[example_minimal_nested_vm]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;129;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_signed_vm]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_signed.argos" "-DEXPECTED=2" "-DEXPECTED_STDOUT=" "-P" "/home/dgti/Projetos/argoslang/tests/run_example.cmake")
set_tests_properties([=[example_minimal_signed_vm]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;129;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_parameters_vm]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_parameters.argos" "-DEXPECTED=42" "-DEXPECTED_STDOUT=" "-P" "/home/dgti/Projetos/argoslang/tests/run_example.cmake")
set_tests_properties([=[example_minimal_parameters_vm]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;129;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_print_int_vm]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_print_int.argos" "-DEXPECTED=0" "-DEXPECTED_STDOUT=42" "-P" "/home/dgti/Projetos/argoslang/tests/run_example.cmake")
set_tests_properties([=[example_minimal_print_int_vm]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;129;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_print_bool_vm]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_print_bool.argos" "-DEXPECTED=0" "-DEXPECTED_STDOUT=true" "-P" "/home/dgti/Projetos/argoslang/tests/run_example.cmake")
set_tests_properties([=[example_minimal_print_bool_vm]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;129;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_print_string_vm]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_print_string.argos" "-DEXPECTED=0" "-DEXPECTED_STDOUT=hello" "-P" "/home/dgti/Projetos/argoslang/tests/run_example.cmake")
set_tests_properties([=[example_minimal_print_string_vm]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;129;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_boolean_call_vm]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_boolean_call.argos" "-DEXPECTED=0" "-DEXPECTED_STDOUT=true" "-P" "/home/dgti/Projetos/argoslang/tests/run_example.cmake")
set_tests_properties([=[example_minimal_boolean_call_vm]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;129;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_control_flow_vm]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_control_flow.argos" "-DEXPECTED=8" "-DEXPECTED_STDOUT=" "-P" "/home/dgti/Projetos/argoslang/tests/run_example.cmake")
set_tests_properties([=[example_minimal_control_flow_vm]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;129;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_break_vm]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_break.argos" "-DEXPECTED=7" "-DEXPECTED_STDOUT=" "-P" "/home/dgti/Projetos/argoslang/tests/run_example.cmake")
set_tests_properties([=[example_minimal_break_vm]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;129;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_real_division_vm]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_real_division.argos" "-DEXPECTED=0" "-DEXPECTED_STDOUT=0.5
0.5
0.5" "-P" "/home/dgti/Projetos/argoslang/tests/run_example.cmake")
set_tests_properties([=[example_minimal_real_division_vm]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;129;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_string_values_vm]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_string_values.argos" "-DEXPECTED=0" "-DEXPECTED_STDOUT=olá
same
nul-safe
line one
line two" "-P" "/home/dgti/Projetos/argoslang/tests/run_example.cmake")
set_tests_properties([=[example_minimal_string_values_vm]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;129;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_short_circuit_vm]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_short_circuit.argos" "-DEXPECTED=0" "-DEXPECTED_STDOUT=nested short-circuit
short-circuit" "-P" "/home/dgti/Projetos/argoslang/tests/run_example.cmake")
set_tests_properties([=[example_minimal_short_circuit_vm]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;129;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_closures_vm]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_closures.argos" "-DEXPECTED=0" "-DEXPECTED_STDOUT=13
7" "-P" "/home/dgti/Projetos/argoslang/tests/run_example.cmake")
set_tests_properties([=[example_minimal_closures_vm]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;129;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_objects_vm]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_objects.argos" "-DEXPECTED=0" "-DEXPECTED_STDOUT=15
15
a
true" "-P" "/home/dgti/Projetos/argoslang/tests/run_example.cmake")
set_tests_properties([=[example_minimal_objects_vm]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;129;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_properties_vm]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_properties.argos" "-DEXPECTED=0" "-DEXPECTED_STDOUT=target
get
rhs
set
get
5
get
set
get
2.5
updated
true
5
6
4" "-P" "/home/dgti/Projetos/argoslang/tests/run_example.cmake")
set_tests_properties([=[example_minimal_properties_vm]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;129;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_object_closure_vm]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_object_closure.argos" "-DEXPECTED=0" "-DEXPECTED_STDOUT=2
5
captured
12
8
10
1
2.5
retained
done" "-P" "/home/dgti/Projetos/argoslang/tests/run_example.cmake")
set_tests_properties([=[example_minimal_object_closure_vm]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;129;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_literal_native]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_literal.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_literal-test" "-DEXPECTED=42" "-DEXPECTED_STDOUT=" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_literal_native]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;193;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_literal_assembly]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_literal.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_literal-assembly-test" "-DEXPECTED=42" "-DEXPECTED_STDOUT=" "-DASSEMBLY=ON" "-DCC=/usr/bin/cc" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_literal_assembly]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;203;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_arithmetic_native]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_arithmetic.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_arithmetic-test" "-DEXPECTED=14" "-DEXPECTED_STDOUT=" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_arithmetic_native]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;193;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_arithmetic_assembly]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_arithmetic.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_arithmetic-assembly-test" "-DEXPECTED=14" "-DEXPECTED_STDOUT=" "-DASSEMBLY=ON" "-DCC=/usr/bin/cc" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_arithmetic_assembly]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;203;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_branch_native]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_branch.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_branch-test" "-DEXPECTED=7" "-DEXPECTED_STDOUT=" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_branch_native]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;193;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_branch_assembly]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_branch.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_branch-assembly-test" "-DEXPECTED=7" "-DEXPECTED_STDOUT=" "-DASSEMBLY=ON" "-DCC=/usr/bin/cc" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_branch_assembly]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;203;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_loop_native]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_loop.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_loop-test" "-DEXPECTED=6" "-DEXPECTED_STDOUT=" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_loop_native]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;193;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_loop_assembly]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_loop.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_loop-assembly-test" "-DEXPECTED=6" "-DEXPECTED_STDOUT=" "-DASSEMBLY=ON" "-DCC=/usr/bin/cc" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_loop_assembly]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;203;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_call_native]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_call.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_call-test" "-DEXPECTED=42" "-DEXPECTED_STDOUT=" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_call_native]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;193;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_call_assembly]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_call.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_call-assembly-test" "-DEXPECTED=42" "-DEXPECTED_STDOUT=" "-DASSEMBLY=ON" "-DCC=/usr/bin/cc" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_call_assembly]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;203;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_nested_native]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_nested.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_nested-test" "-DEXPECTED=11" "-DEXPECTED_STDOUT=" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_nested_native]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;193;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_nested_assembly]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_nested.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_nested-assembly-test" "-DEXPECTED=11" "-DEXPECTED_STDOUT=" "-DASSEMBLY=ON" "-DCC=/usr/bin/cc" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_nested_assembly]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;203;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_signed_native]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_signed.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_signed-test" "-DEXPECTED=2" "-DEXPECTED_STDOUT=" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_signed_native]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;193;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_signed_assembly]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_signed.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_signed-assembly-test" "-DEXPECTED=2" "-DEXPECTED_STDOUT=" "-DASSEMBLY=ON" "-DCC=/usr/bin/cc" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_signed_assembly]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;203;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_parameters_native]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_parameters.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_parameters-test" "-DEXPECTED=42" "-DEXPECTED_STDOUT=" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_parameters_native]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;193;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_parameters_assembly]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_parameters.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_parameters-assembly-test" "-DEXPECTED=42" "-DEXPECTED_STDOUT=" "-DASSEMBLY=ON" "-DCC=/usr/bin/cc" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_parameters_assembly]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;203;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_print_int_native]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_print_int.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_print_int-test" "-DEXPECTED=0" "-DEXPECTED_STDOUT=42" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_print_int_native]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;193;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_print_int_assembly]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_print_int.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_print_int-assembly-test" "-DEXPECTED=0" "-DEXPECTED_STDOUT=42" "-DASSEMBLY=ON" "-DCC=/usr/bin/cc" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_print_int_assembly]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;203;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_print_bool_native]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_print_bool.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_print_bool-test" "-DEXPECTED=0" "-DEXPECTED_STDOUT=true" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_print_bool_native]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;193;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_print_bool_assembly]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_print_bool.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_print_bool-assembly-test" "-DEXPECTED=0" "-DEXPECTED_STDOUT=true" "-DASSEMBLY=ON" "-DCC=/usr/bin/cc" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_print_bool_assembly]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;203;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_print_string_native]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_print_string.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_print_string-test" "-DEXPECTED=0" "-DEXPECTED_STDOUT=hello" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_print_string_native]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;193;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_print_string_assembly]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_print_string.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_print_string-assembly-test" "-DEXPECTED=0" "-DEXPECTED_STDOUT=hello" "-DASSEMBLY=ON" "-DCC=/usr/bin/cc" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_print_string_assembly]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;203;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_boolean_call_native]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_boolean_call.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_boolean_call-test" "-DEXPECTED=0" "-DEXPECTED_STDOUT=true" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_boolean_call_native]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;193;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_boolean_call_assembly]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_boolean_call.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_boolean_call-assembly-test" "-DEXPECTED=0" "-DEXPECTED_STDOUT=true" "-DASSEMBLY=ON" "-DCC=/usr/bin/cc" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_boolean_call_assembly]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;203;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_control_flow_native]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_control_flow.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_control_flow-test" "-DEXPECTED=8" "-DEXPECTED_STDOUT=" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_control_flow_native]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;193;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_control_flow_assembly]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_control_flow.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_control_flow-assembly-test" "-DEXPECTED=8" "-DEXPECTED_STDOUT=" "-DASSEMBLY=ON" "-DCC=/usr/bin/cc" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_control_flow_assembly]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;203;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_break_native]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_break.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_break-test" "-DEXPECTED=7" "-DEXPECTED_STDOUT=" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_break_native]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;193;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_break_assembly]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_break.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_break-assembly-test" "-DEXPECTED=7" "-DEXPECTED_STDOUT=" "-DASSEMBLY=ON" "-DCC=/usr/bin/cc" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_break_assembly]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;203;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_real_division_native]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_real_division.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_real_division-test" "-DEXPECTED=0" "-DEXPECTED_STDOUT=0.5
0.5
0.5" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_real_division_native]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;193;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_real_division_assembly]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_real_division.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_real_division-assembly-test" "-DEXPECTED=0" "-DEXPECTED_STDOUT=0.5
0.5
0.5" "-DASSEMBLY=ON" "-DCC=/usr/bin/cc" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_real_division_assembly]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;203;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_string_values_native]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_string_values.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_string_values-test" "-DEXPECTED=0" "-DEXPECTED_STDOUT=olá
same
nul-safe
line one
line two" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_string_values_native]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;193;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_string_values_assembly]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_string_values.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_string_values-assembly-test" "-DEXPECTED=0" "-DEXPECTED_STDOUT=olá
same
nul-safe
line one
line two" "-DASSEMBLY=ON" "-DCC=/usr/bin/cc" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_string_values_assembly]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;203;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_short_circuit_native]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_short_circuit.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_short_circuit-test" "-DEXPECTED=0" "-DEXPECTED_STDOUT=nested short-circuit
short-circuit" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_short_circuit_native]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;193;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_short_circuit_assembly]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_short_circuit.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_short_circuit-assembly-test" "-DEXPECTED=0" "-DEXPECTED_STDOUT=nested short-circuit
short-circuit" "-DASSEMBLY=ON" "-DCC=/usr/bin/cc" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_short_circuit_assembly]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;203;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_closures_native]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_closures.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_closures-test" "-DEXPECTED=0" "-DEXPECTED_STDOUT=13
7" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_closures_native]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;193;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_closures_assembly]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_closures.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_closures-assembly-test" "-DEXPECTED=0" "-DEXPECTED_STDOUT=13
7" "-DASSEMBLY=ON" "-DCC=/usr/bin/cc" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_closures_assembly]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;203;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_objects_native]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_objects.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_objects-test" "-DEXPECTED=0" "-DEXPECTED_STDOUT=15
15
a
true" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_objects_native]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;193;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_objects_assembly]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_objects.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_objects-assembly-test" "-DEXPECTED=0" "-DEXPECTED_STDOUT=15
15
a
true" "-DASSEMBLY=ON" "-DCC=/usr/bin/cc" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_objects_assembly]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;203;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_properties_native]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_properties.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_properties-test" "-DEXPECTED=0" "-DEXPECTED_STDOUT=target
get
rhs
set
get
5
get
set
get
2.5
updated
true
5
6
4" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_properties_native]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;193;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_properties_assembly]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_properties.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_properties-assembly-test" "-DEXPECTED=0" "-DEXPECTED_STDOUT=target
get
rhs
set
get
5
get
set
get
2.5
updated
true
5
6
4" "-DASSEMBLY=ON" "-DCC=/usr/bin/cc" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_properties_assembly]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;203;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_object_closure_native]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_object_closure.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_object_closure-test" "-DEXPECTED=0" "-DEXPECTED_STDOUT=2
5
captured
12
8
10
1
2.5
retained
done" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_object_closure_native]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;193;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_object_closure_assembly]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_object_closure.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_object_closure-assembly-test" "-DEXPECTED=0" "-DEXPECTED_STDOUT=2
5
captured
12
8
10
1
2.5
retained
done" "-DASSEMBLY=ON" "-DCC=/usr/bin/cc" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_object_closure_assembly]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;203;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_literal_short_form]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_literal.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal-literal-short-test" "-DEXPECTED=42" "-DSHORT_FORM=ON" "-P" "/home/dgti/Projetos/argoslang/tests/compile_example.cmake")
set_tests_properties([=[example_minimal_literal_short_form]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;216;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_optional_native_rejected]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_optional.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_optional-unsupported-test" "-P" "/home/dgti/Projetos/argoslang/tests/compile_rejection.cmake")
set_tests_properties([=[example_minimal_optional_native_rejected]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;227;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_null_safety_native_rejected]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/null_safety.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/null_safety-unsupported-test" "-P" "/home/dgti/Projetos/argoslang/tests/compile_rejection.cmake")
set_tests_properties([=[example_null_safety_native_rejected]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;227;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_frontend_inheritance_native_rejected]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_frontend_inheritance.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_frontend_inheritance-unsupported-test" "-P" "/home/dgti/Projetos/argoslang/tests/compile_rejection.cmake")
set_tests_properties([=[example_minimal_frontend_inheritance_native_rejected]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;227;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_frontend_bound_method_native_rejected]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_frontend_bound_method.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal_frontend_bound_method-unsupported-test" "-P" "/home/dgti/Projetos/argoslang/tests/compile_rejection.cmake")
set_tests_properties([=[example_minimal_frontend_bound_method_native_rejected]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;227;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_modulo_by_zero_native]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_modulo_by_zero.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal-modulo-by-zero-test" "-P" "/home/dgti/Projetos/argoslang/tests/runtime_error.cmake")
set_tests_properties([=[example_minimal_modulo_by_zero_native]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;236;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[example_minimal_modulo_overflow_native]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-DSOURCE=/home/dgti/Projetos/argoslang/examples/minimal_modulo_overflow.argos" "-DBINARY=/home/dgti/Projetos/argoslang/build-gcc/bin/minimal-modulo-overflow-test" "-P" "/home/dgti/Projetos/argoslang/tests/runtime_error.cmake")
set_tests_properties([=[example_minimal_modulo_overflow_native]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;244;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
add_test([=[legacy_constructor_rejected_by_all_commands]=] "/usr/bin/cmake" "-DARGOSC=/home/dgti/Projetos/argoslang/build-gcc/bin/argosc" "-P" "/home/dgti/Projetos/argoslang/tests/legacy_constructor_rejection.cmake")
set_tests_properties([=[legacy_constructor_rejected_by_all_commands]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;254;add_test;/home/dgti/Projetos/argoslang/tests/CMakeLists.txt;0;")
subdirs("../_deps/catch2-build")
