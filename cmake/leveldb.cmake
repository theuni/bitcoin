# Copyright (c) 2023 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# This file is part of the transition from Autotools to CMake. Once CMake
# support has been merged we should switch to using the upstream CMake
# buildsystem.

include(CheckCXXSymbolExists)
check_cxx_symbol_exists(F_FULLFSYNC "fcntl.h" HAVE_FULLFSYNC)

add_library(leveldb STATIC EXCLUDE_FROM_ALL
  ${PROJECT_SOURCE_DIR}/src/leveldb/db/builder.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/db/c.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/db/db_impl.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/db/db_iter.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/db/dbformat.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/db/dumpfile.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/db/filename.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/db/log_reader.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/db/log_writer.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/db/memtable.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/db/repair.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/db/table_cache.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/db/version_edit.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/db/version_set.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/db/write_batch.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/table/block.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/table/block_builder.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/table/filter_block.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/table/format.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/table/iterator.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/table/merger.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/table/table.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/table/table_builder.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/table/two_level_iterator.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/util/arena.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/util/bloom.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/util/cache.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/util/coding.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/util/comparator.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/util/crc32c.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/util/env.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/util/filter_policy.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/util/hash.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/util/histogram.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/util/logging.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/util/options.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/util/status.cc
  ${PROJECT_SOURCE_DIR}/src/leveldb/helpers/memenv/memenv.cc
)
if(WIN32)
  target_sources(leveldb PRIVATE ${PROJECT_SOURCE_DIR}/src/leveldb/util/env_windows.cc)
  set_property(SOURCE ${PROJECT_SOURCE_DIR}/src/leveldb/util/env_windows.cc
    APPEND PROPERTY COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/wd4722>
  )
else()
  target_sources(leveldb PRIVATE ${PROJECT_SOURCE_DIR}/src/leveldb/util/env_posix.cc)
endif()

target_precompile_headers(leveldb
  PRIVATE
  ${PROJECT_SOURCE_DIR}/src/leveldb/db/builder.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/db/db_impl.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/db/db_iter.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/db/dbformat.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/db/filename.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/db/log_format.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/db/log_reader.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/db/log_writer.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/db/memtable.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/db/skiplist.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/db/snapshot.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/db/table_cache.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/db/version_edit.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/db/version_set.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/db/write_batch_internal.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/helpers/memenv/memenv.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/include/leveldb/c.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/include/leveldb/cache.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/include/leveldb/comparator.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/include/leveldb/db.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/include/leveldb/dumpfile.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/include/leveldb/env.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/include/leveldb/export.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/include/leveldb/filter_policy.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/include/leveldb/iterator.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/include/leveldb/options.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/include/leveldb/slice.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/include/leveldb/status.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/include/leveldb/table.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/include/leveldb/table_builder.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/include/leveldb/write_batch.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/port/port.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/port/port_stdcxx.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/port/thread_annotations.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/table/block.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/table/block_builder.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/table/filter_block.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/table/format.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/table/iterator_wrapper.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/table/merger.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/table/two_level_iterator.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/util/arena.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/util/coding.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/util/crc32c.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/util/hash.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/util/histogram.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/util/logging.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/util/mutexlock.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/util/no_destructor.h
  ${PROJECT_SOURCE_DIR}/src/leveldb/util/random.h
)

if (WIN32)
  target_precompile_headers(leveldb PRIVATE ${PROJECT_SOURCE_DIR}/src/leveldb/util/windows_logger.h)
else()
  target_precompile_headers(leveldb PRIVATE ${PROJECT_SOURCE_DIR}/src/leveldb/util/posix_logger.h)
endif()

target_compile_definitions(leveldb
  PRIVATE
    HAVE_SNAPPY=0
    HAVE_CRC32C=1
    HAVE_FDATASYNC=$<BOOL:${HAVE_FDATASYNC}>
    HAVE_FULLFSYNC=$<BOOL:${HAVE_FULLFSYNC}>
    HAVE_O_CLOEXEC=$<BOOL:${HAVE_O_CLOEXEC}>
    FALLTHROUGH_INTENDED=[[fallthrough]]
    LEVELDB_IS_BIG_ENDIAN=${WORDS_BIGENDIAN}
)

if(WIN32)
  target_compile_definitions(leveldb
    PRIVATE
      LEVELDB_PLATFORM_WINDOWS
      _UNICODE
      UNICODE
      __USE_MINGW_ANSI_STDIO=1
  )
else()
  target_compile_definitions(leveldb PRIVATE LEVELDB_PLATFORM_POSIX)
endif()

target_include_directories(leveldb
  PRIVATE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/leveldb>
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/leveldb/include>
)

#TODO: figure out how to filter out:
# -Wconditional-uninitialized -Werror=conditional-uninitialized -Wsuggest-override -Werror=suggest-override

target_link_libraries(leveldb PRIVATE crc32c)
