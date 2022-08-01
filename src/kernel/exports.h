// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_KERNEL_EXPORTS_H
#define BITCOIN_KERNEL_EXPORTS_H

#if defined(BITCOIN_DLL_EXPORT)
#define BITCOIN_EXPORT __declspec(dllexport)
#else
#define BITCOIN_EXPORT
#endif

#endif // BITCOIN_KERNEL_EXPORTS_H
