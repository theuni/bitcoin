# Copyright (c) 2023 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# This file is part of the transition from autotools to CMake. Once CMake
# support has been merged we should switch to using the upstream CMake
# buildsystem.

add_library(leveldb STATIC EXCLUDE_FROM_ALL "")

target_compile_definitions(leveldb PRIVATE HAVE_SNAPPY=0)
target_compile_definitions(leveldb PRIVATE HAVE_CRC32C=1)

if (HAVE_FDATASYNC)
  target_compile_definitions(leveldb PRIVATE HAVE_FDATASYNC=1)
else()
  target_compile_definitions(leveldb PRIVATE HAVE_FDATASYNC=0)
endif()

if (HAVE_FULLFSYNC)
  target_compile_definitions(leveldb PRIVATE HAVE_FULLFSYNC=1)
else()
  target_compile_definitions(leveldb PRIVATE HAVE_FULLFSYNC=0)
endif()

if (HAVE_O_CLOEXEC)
  target_compile_definitions(leveldb PRIVATE HAVE_O_CLOEXEC=1)
else()
  target_compile_definitions(leveldb PRIVATE HAVE_O_CLOEXEC=0)
endif()

target_compile_definitions(leveldb PRIVATE LEVELDB_IS_BIG_ENDIAN=${WORDS_BIGENDIAN})

if(WIN32)
  target_compile_definitions(leveldb PRIVATE LEVELDB_PLATFORM_WINDOWS=1)
  target_compile_definitions(leveldb PRIVATE UNICODE=1)
  target_compile_definitions(leveldb PRIVATE __USE_MINGW_ANSI_STDIO=1)
else()
  target_compile_definitions(leveldb PRIVATE LEVELDB_PLATFORM_POSIX)
endif()

target_compile_definitions(leveldb PRIVATE FALLTHROUGH_INTENDED=[[fallthrough]])
target_include_directories(leveldb
  PRIVATE
    ${PROJECT_SOURCE_DIR}/src/leveldb
    ${PROJECT_SOURCE_DIR}/src/leveldb/include
    ${PROJECT_SOURCE_DIR}/src/crc32c/include
)

#TODO: figure out how to filter out:
# -Wconditional-uninitialized -Werror=conditional-uninitialized -Wsuggest-override -Werror=suggest-override

target_sources(leveldb
  PRIVATE
    "leveldb/db/builder.cc"
    "leveldb/db/builder.h"
    "leveldb/db/c.cc"
    "leveldb/db/db_impl.cc"
    "leveldb/db/db_impl.h"
    "leveldb/db/db_iter.cc"
    "leveldb/db/db_iter.h"
    "leveldb/db/dbformat.cc"
    "leveldb/db/dbformat.h"
    "leveldb/db/dumpfile.cc"
    "leveldb/db/filename.cc"
    "leveldb/db/filename.h"
    "leveldb/db/log_format.h"
    "leveldb/db/log_reader.cc"
    "leveldb/db/log_reader.h"
    "leveldb/db/log_writer.cc"
    "leveldb/db/log_writer.h"
    "leveldb/db/memtable.cc"
    "leveldb/db/memtable.h"
    "leveldb/db/repair.cc"
    "leveldb/db/skiplist.h"
    "leveldb/db/snapshot.h"
    "leveldb/db/table_cache.cc"
    "leveldb/db/table_cache.h"
    "leveldb/db/version_edit.cc"
    "leveldb/db/version_edit.h"
    "leveldb/db/version_set.cc"
    "leveldb/db/version_set.h"
    "leveldb/db/write_batch_internal.h"
    "leveldb/db/write_batch.cc"
    "leveldb/port/port_stdcxx.h"
    "leveldb/port/port.h"
    "leveldb/port/thread_annotations.h"
    "leveldb/table/block_builder.cc"
    "leveldb/table/block_builder.h"
    "leveldb/table/block.cc"
    "leveldb/table/block.h"
    "leveldb/table/filter_block.cc"
    "leveldb/table/filter_block.h"
    "leveldb/table/format.cc"
    "leveldb/table/format.h"
    "leveldb/table/iterator_wrapper.h"
    "leveldb/table/iterator.cc"
    "leveldb/table/merger.cc"
    "leveldb/table/merger.h"
    "leveldb/table/table_builder.cc"
    "leveldb/table/table.cc"
    "leveldb/table/two_level_iterator.cc"
    "leveldb/table/two_level_iterator.h"
    "leveldb/util/arena.cc"
    "leveldb/util/arena.h"
    "leveldb/util/bloom.cc"
    "leveldb/util/cache.cc"
    "leveldb/util/coding.cc"
    "leveldb/util/coding.h"
    "leveldb/util/comparator.cc"
    "leveldb/util/crc32c.cc"
    "leveldb/util/crc32c.h"
    "leveldb/util/env.cc"
    "leveldb/util/filter_policy.cc"
    "leveldb/util/hash.cc"
    "leveldb/util/hash.h"
    "leveldb/util/logging.cc"
    "leveldb/util/logging.h"
    "leveldb/util/mutexlock.h"
    "leveldb/util/no_destructor.h"
    "leveldb/util/options.cc"
    "leveldb/util/random.h"
    "leveldb/util/status.cc"
    "leveldb/helpers/memenv/memenv.cc"
    "leveldb/helpers/memenv/memenv.h"
  )
if (WIN32)
target_sources(leveldb PRIVATE leveldb/util/env_windows.cc)
else()
target_sources(leveldb PRIVATE leveldb/util/env_posix.cc)
endif()
