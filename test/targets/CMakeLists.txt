# add cpp target
function(add_test_cpp_target name)
  add_executable(${name} "${name}.cpp")
  # -g = generate debug info, -O0 = disable optimizations, -pie = generate PIE
  target_compile_options(${name} PRIVATE -g -O0 -pie)
  add_dependencies(tests ${name})
endfunction()

# add assembly target
function(add_test_asm_target name)
  add_executable(${name} "${name}.s")
  target_compile_options(${name} PRIVATE -pie)
  add_dependencies(tests ${name})
endfunction()

add_test_cpp_target(run_endlessly)
add_test_cpp_target(end_immediately)
add_test_cpp_target(hello_sdb)

add_test_asm_target(reg_write)
add_test_asm_target(reg_read)
