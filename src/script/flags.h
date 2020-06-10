// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_FLAGS_H
#define BITCOIN_SCRIPT_FLAGS_H

#include <stdint.h>
#include <utility>

/*
 *
 *  All flags are intended to be soft forks: the set of acceptable scripts under
 *  flags (A | B) is a subset of the acceptable scripts under flag (A).
 */
enum
{
    SCRIPT_VERIFY_NONE      = 0,

    // Evaluate P2SH subscripts (BIP16).
    SCRIPT_VERIFY_P2SH      = (1U << 0),

    // Passing a non-strict-DER signature or one with undefined hashtype to a checksig operation causes script failure.
    // Evaluating a pubkey that is not (0x04 + 64 bytes) or (0x02 or 0x03 + 32 bytes) by checksig causes script failure.
    // (not used or intended as a consensus rule).
    SCRIPT_VERIFY_STRICTENC = (1U << 1),

    // Passing a non-strict-DER signature to a checksig operation causes script failure (BIP62 rule 1)
    SCRIPT_VERIFY_DERSIG    = (1U << 2),

    // Passing a non-strict-DER signature or one with S > order/2 to a checksig operation causes script failure
    // (BIP62 rule 5).
    SCRIPT_VERIFY_LOW_S     = (1U << 3),

    // verify dummy stack item consumed by CHECKMULTISIG is of zero-length (BIP62 rule 7).
    SCRIPT_VERIFY_NULLDUMMY = (1U << 4),

    // Using a non-push operator in the scriptSig causes script failure (BIP62 rule 2).
    SCRIPT_VERIFY_SIGPUSHONLY = (1U << 5),

    // Require minimal encodings for all push operations (OP_0... OP_16, OP_1NEGATE where possible, direct
    // pushes up to 75 bytes, OP_PUSHDATA up to 255 bytes, OP_PUSHDATA2 for anything larger). Evaluating
    // any other push causes the script to fail (BIP62 rule 3).
    // In addition, whenever a stack element is interpreted as a number, it must be of minimal length (BIP62 rule 4).
    SCRIPT_VERIFY_MINIMALDATA = (1U << 6),

    // Discourage use of NOPs reserved for upgrades (NOP1-10)
    //
    // Provided so that nodes can avoid accepting or mining transactions
    // containing executed NOP's whose meaning may change after a soft-fork,
    // thus rendering the script invalid; with this flag set executing
    // discouraged NOPs fails the script. This verification flag will never be
    // a mandatory flag applied to scripts in a block. NOPs that are not
    // executed, e.g.  within an unexecuted IF ENDIF block, are *not* rejected.
    // NOPs that have associated forks to give them new meaning (CLTV, CSV)
    // are not subject to this rule.
    SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS  = (1U << 7),

    // Require that only a single stack element remains after evaluation. This changes the success criterion from
    // "At least one stack element must remain, and when interpreted as a boolean, it must be true" to
    // "Exactly one stack element must remain, and when interpreted as a boolean, it must be true".
    // (BIP62 rule 6)
    // Note: CLEANSTACK should never be used without P2SH or WITNESS.
    SCRIPT_VERIFY_CLEANSTACK = (1U << 8),

    // Verify CHECKLOCKTIMEVERIFY
    //
    // See BIP65 for details.
    SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY = (1U << 9),

    // support CHECKSEQUENCEVERIFY opcode
    //
    // See BIP112 for details
    SCRIPT_VERIFY_CHECKSEQUENCEVERIFY = (1U << 10),

    // Support segregated witness
    //
    SCRIPT_VERIFY_WITNESS = (1U << 11),

    // Making v1-v16 witness program non-standard
    //
    SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM = (1U << 12),

    // Segwit script only: Require the argument of OP_IF/NOTIF to be exactly 0x01 or empty vector
    //
    SCRIPT_VERIFY_MINIMALIF = (1U << 13),

    // Signature(s) must be empty vector if a CHECK(MULTI)SIG operation failed
    //
    SCRIPT_VERIFY_NULLFAIL = (1U << 14),

    // Public keys in segregated witness scripts must be compressed
    //
    SCRIPT_VERIFY_WITNESS_PUBKEYTYPE = (1U << 15),

    // Making OP_CODESEPARATOR and FindAndDelete fail any non-segwit scripts
    //
    SCRIPT_VERIFY_CONST_SCRIPTCODE = (1U << 16),
};

namespace consensus {
    struct dummy {
        using basetype = uint32_t;
        enum class flags : basetype {
            SCRIPT_VERIFY_NONE      = 0,
            // Evaluate P2SH subscripts (BIP16).
            //
            SCRIPT_VERIFY_P2SH      = (1U << 0),
            // Passing a non-strict-DER signature to a checksig operation causes script failure (BIP62 rule 1)
            //
            SCRIPT_VERIFY_DERSIG    = (1U << 2),
            // verify dummy stack item consumed by CHECKMULTISIG is of zero-length (BIP62 rule 7).
            //
            SCRIPT_VERIFY_NULLDUMMY = (1U << 4),
            // See BIP65 for details.
            //
            SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY = (1U << 9),
            // support CHECKSEQUENCEVERIFY opcode
            // See BIP112 for details
            //
            SCRIPT_VERIFY_CHECKSEQUENCEVERIFY = (1U << 10),
            // Support segregated witness
            //
            SCRIPT_VERIFY_WITNESS = (1U << 11),
        };
        constexpr friend flags operator&(const flags& lhs, const flags& rhs){
            return static_cast<flags>(static_cast<basetype>(lhs) & static_cast<basetype>(rhs));
        }
        constexpr friend flags operator|(const flags& lhs, const flags& rhs){
            return static_cast<flags>(static_cast<basetype>(lhs) | static_cast<basetype>(rhs));
        }
        friend flags& operator|=(flags& lhs, const flags& rhs){
            return lhs = lhs | rhs;
        }
        constexpr friend bool is_set(const flags& lhs, const flags& rhs) {
            return (lhs & rhs) != flags::SCRIPT_VERIFY_NONE;
        }
        constexpr friend flags without_flag(const flags& lhs, const flags& rhs) {
            return static_cast<flags>(static_cast<basetype>(lhs) & ~static_cast<basetype>(rhs));
        }
    };
}

namespace policy {
    struct dummy {
        using basetype = uint32_t;
        enum class flags : basetype {
            SCRIPT_VERIFY_NONE      = 0,

            // Passing a non-strict-DER signature or one with undefined hashtype to a checksig operation causes script failure.
            // Evaluating a pubkey that is not (0x04 + 64 bytes) or (0x02 or 0x03 + 32 bytes) by checksig causes script failure.
            // (not used or intended as a consensus rule).
            SCRIPT_VERIFY_STRICTENC = (1U << 1),

            // Passing a non-strict-DER signature or one with S > order/2 to a checksig operation causes script failure
            // (BIP62 rule 5).
            SCRIPT_VERIFY_LOW_S     = (1U << 3),

            // Using a non-push operator in the scriptSig causes script failure (BIP62 rule 2).
            SCRIPT_VERIFY_SIGPUSHONLY = (1U << 5),

            // Require minimal encodings for all push operations (OP_0... OP_16, OP_1NEGATE where possible, direct
            // pushes up to 75 bytes, OP_PUSHDATA up to 255 bytes, OP_PUSHDATA2 for anything larger). Evaluating
            // any other push causes the script to fail (BIP62 rule 3).
            // In addition, whenever a stack element is interpreted as a number, it must be of minimal length (BIP62 rule 4).
            SCRIPT_VERIFY_MINIMALDATA = (1U << 6),

            // Discourage use of NOPs reserved for upgrades (NOP1-10)
            //
            // Provided so that nodes can avoid accepting or mining transactions
            // containing executed NOP's whose meaning may change after a soft-fork,
            // thus rendering the script invalid; with this flag set executing
            // discouraged NOPs fails the script. This verification flag will never be
            // a mandatory flag applied to scripts in a block. NOPs that are not
            // executed, e.g.  within an unexecuted IF ENDIF block, are *not* rejected.
            // NOPs that have associated forks to give them new meaning (CLTV, CSV)
            // are not subject to this rule.
            SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS  = (1U << 7),

            // Require that only a single stack element remains after evaluation. This changes the success criterion from
            // "At least one stack element must remain, and when interpreted as a boolean, it must be true" to
            // "Exactly one stack element must remain, and when interpreted as a boolean, it must be true".
            // (BIP62 rule 6)
            // Note: CLEANSTACK should never be used without P2SH or WITNESS.
            SCRIPT_VERIFY_CLEANSTACK = (1U << 8),

            // Making v1-v16 witness program non-standard
            //
            SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM = (1U << 12),

            // Segwit script only: Require the argument of OP_IF/NOTIF to be exactly 0x01 or empty vector
            //
            SCRIPT_VERIFY_MINIMALIF = (1U << 13),

            // Signature(s) must be empty vector if a CHECK(MULTI)SIG operation failed
            //
            SCRIPT_VERIFY_NULLFAIL = (1U << 14),

            // Public keys in segregated witness scripts must be compressed
            //
            SCRIPT_VERIFY_WITNESS_PUBKEYTYPE = (1U << 15),

            // Making OP_CODESEPARATOR and FindAndDelete fail any non-segwit scripts
            //
            SCRIPT_VERIFY_CONST_SCRIPTCODE = (1U << 16),
        };
        constexpr friend flags operator&(const flags& lhs, const flags& rhs){
            return static_cast<flags>(static_cast<basetype>(lhs) & static_cast<basetype>(rhs));
        }
        constexpr friend flags operator|(const flags& lhs, const flags& rhs){
            return static_cast<flags>(static_cast<basetype>(lhs) | static_cast<basetype>(rhs));
        }
        friend flags& operator|=(flags& lhs, const flags& rhs){
            return lhs = lhs | rhs;
        }
        constexpr friend bool is_set(const flags& lhs, const flags& rhs) {
            return (lhs & rhs) != flags::SCRIPT_VERIFY_NONE;
        }
        constexpr friend flags without_flag(const flags& lhs, const flags& rhs) {
            return static_cast<flags>(static_cast<basetype>(lhs) & ~static_cast<basetype>(rhs));
        }
    };
}
using ConsensusFlags = consensus::dummy::flags;
using PolicyFlags = policy::dummy::flags;

/*
    Temporary ugly convenience function only necessary for refactoring
*/

std::pair<ConsensusFlags, PolicyFlags> inline SplitConsensusAndPolicyFlags(int flags) {

    ConsensusFlags consensus_flags = ConsensusFlags::SCRIPT_VERIFY_NONE;
    if (flags & SCRIPT_VERIFY_P2SH) consensus_flags |= ConsensusFlags::SCRIPT_VERIFY_P2SH;
    if (flags & SCRIPT_VERIFY_DERSIG) consensus_flags |= ConsensusFlags::SCRIPT_VERIFY_DERSIG;
    if (flags & SCRIPT_VERIFY_NULLDUMMY) consensus_flags |= ConsensusFlags::SCRIPT_VERIFY_NULLDUMMY;
    if (flags & SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY) consensus_flags |= ConsensusFlags::SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY;
    if (flags & SCRIPT_VERIFY_CHECKSEQUENCEVERIFY) consensus_flags |= ConsensusFlags::SCRIPT_VERIFY_CHECKSEQUENCEVERIFY;
    if (flags & SCRIPT_VERIFY_WITNESS) consensus_flags |= ConsensusFlags::SCRIPT_VERIFY_WITNESS;

    PolicyFlags policy_flags = PolicyFlags::SCRIPT_VERIFY_NONE;
    if (flags & SCRIPT_VERIFY_STRICTENC) policy_flags |= PolicyFlags::SCRIPT_VERIFY_STRICTENC;
    if (flags & SCRIPT_VERIFY_LOW_S) policy_flags |= PolicyFlags::SCRIPT_VERIFY_LOW_S;
    if (flags & SCRIPT_VERIFY_SIGPUSHONLY) policy_flags |= PolicyFlags::SCRIPT_VERIFY_SIGPUSHONLY;
    if (flags & SCRIPT_VERIFY_MINIMALDATA) policy_flags |= PolicyFlags::SCRIPT_VERIFY_MINIMALDATA;
    if (flags & SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS) policy_flags |= PolicyFlags::SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS;
    if (flags & SCRIPT_VERIFY_CLEANSTACK) policy_flags |= PolicyFlags::SCRIPT_VERIFY_CLEANSTACK;
    if (flags & SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM) policy_flags |= PolicyFlags::SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM;
    if (flags & SCRIPT_VERIFY_MINIMALIF) policy_flags |= PolicyFlags::SCRIPT_VERIFY_MINIMALIF;
    if (flags & SCRIPT_VERIFY_NULLFAIL) policy_flags |= PolicyFlags::SCRIPT_VERIFY_NULLFAIL;
    if (flags & SCRIPT_VERIFY_WITNESS_PUBKEYTYPE) policy_flags |= PolicyFlags::SCRIPT_VERIFY_WITNESS_PUBKEYTYPE;
    if (flags & SCRIPT_VERIFY_CONST_SCRIPTCODE) policy_flags |= PolicyFlags::SCRIPT_VERIFY_CONST_SCRIPTCODE;

    return {consensus_flags, policy_flags};
}

#endif // BITCOIN_SCRIPT_FLAGS_H
