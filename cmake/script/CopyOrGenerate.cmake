cmake_path(GET CAPNP_FILE PARENT_PATH parent_path)
set(filenames
  .c++
  .h
  .proxy-client.c++
  .proxy-types.h
  .proxy-server.c++
  .proxy-types.c++
  .proxy.h
)
file(SHA256 ${capnp_path} file_hash)
set(do_copy TRUE)
foreach(generated_file ${filenames})
  set(cached_hash "cached_hash")
  if (EXISTS ${capnp_path}${generated_file} AND EXISTS ${capnp_path}${generated_file}.hash)
    file(READ ${capnp_path}${generated_file}.hash cached_hash LIMIT 64)
    string(STRIP ${cached_hash} cached_hash)
  endif()
  if(NOT "${file_hash}" MATCHES "${cached_hash}")
    set(do_copy FALSE)
    break()
  endif()
endforeach()

if(do_copy)
  message("Copying cached files for ${capnp_path}")
  foreach(generated_file ${filenames})
    file(COPY
       ${capnp_path}${generated_file}
       DESTINATION ${parent_path}
    )
  endforeach()
else()
  message("Generating files for ${capnp_path}")
  execute_process(COMMAND ${MPGEN} ${CAPNP_SRCDIR} ${INCLUDE_PREFIX} ${capnp_path} ${TCS_IMPORT_PATHS} ${MP_INCLUDE_DIR}
                  COMMAND_ECHO STDOUT)
endif()
