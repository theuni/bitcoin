// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_FS_H
#define BITCOIN_FS_H

#include <stdio.h>
#include <string>

/** Filesystem operations and types */
#if defined(USE_EXPERIMENTAL_TS_FS)
    #include <experimental/filesystem>
    namespace fs = std::experimental::filesystem;
#elif defined(USE_INTERNAL_FS)
    #error not implemented
#else
    #include <boost/filesystem.hpp>
    #include <boost/filesystem/fstream.hpp>
    #include <boost/filesystem/detail/utf8_codecvt_facet.hpp>
    namespace fs = boost::filesystem;
#endif


/** Bridge operations to C stdio */
namespace fsbridge {
    FILE *fopen(const fs::path& p, const char *mode);
    FILE *freopen(const fs::path& p, const char *mode, FILE *stream);
};

#endif
