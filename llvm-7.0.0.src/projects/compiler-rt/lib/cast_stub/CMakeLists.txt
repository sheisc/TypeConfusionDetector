set(CAST_STUB_SOURCES
  cast_stub.cc
  )

include_directories(..)

#set(HEXTYPE_CFLAGS ${SANITIZER_COMMON_CFLAGS})
# Append "-O0" at the end to overwrite previously-set optimization level
set(CAST_STUB_CFLAGS "${SANITIZER_COMMON_CFLAGS} -O0")

add_custom_target(cast_stub)
# "Debug-build/lib/clang/3.9.0/lib/linux/libclang_rt.hextype-x86_64.a"
add_compiler_rt_runtime(clang_rt.cast_stub
        STATIC
        ARCHS x86_64
        SOURCES ${CAST_STUB_SOURCES}
        CFLAGS ${CAST_STUB_CFLAGS}
        PARENT_TARGET cast_stub)
# "Debug-build/lib/clang/3.9.0/lib/linux/libclang_rt.hextype-x86_64.so"
add_compiler_rt_runtime(clang_rt.cast_stub
        SHARED
        ARCHS x86_64
        SOURCES ${CAST_STUB_SOURCES}
        CFLAGS ${CAST_STUB_CFLAGS}
        PARENT_TARGET cast_stub)

add_sanitizer_rt_symbols(clang_rt.cast_stub)

add_dependencies(compiler-rt cast_stub)
