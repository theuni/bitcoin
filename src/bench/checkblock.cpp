// Copyright (c) 2016-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <bench/data.h>

#include <chainparams.h>
#include <common/args.h>
#include <consensus/validation.h>
#include <streams.h>
#include <util/chaintype.h>
#include <validation.h>

#include <memory_resource>
#include <cassert>

// These are the two major time-sinks which happen after we have fully received
// a block off the wire, but before we can relay the block on to peers using
// compact block relay.

class null_resource : public std::pmr::memory_resource
{
    virtual void* do_allocate(size_t, size_t) override
    {
        assert(false);
        return nullptr;
    }
    virtual void do_deallocate(void*, size_t, size_t) override
    {
    }
    virtual bool do_is_equal(memory_resource const&) const noexcept override
    {
        return false;
    }
};

static void DeserializeBlockTest(benchmark::Bench& bench)
{
    DataStream stream(benchmark::data::block413567);
    std::byte a{0};
    stream.write({&a, 1}); // Prevent compaction

    bench.unit("block").run([&] {
        CBlock block;
        stream >> TX_WITH_WITNESS(block);
        bool rewound = stream.Rewind(benchmark::data::block413567.size());
        assert(rewound);
    });
}

static void DeserializeBlockWithAllocTest(benchmark::Bench& bench)
{
    DataStream stream(benchmark::data::block413567);
    std::byte a{0};
    stream.write({&a, 1}); // Prevent compaction

    std::vector<std::byte> block_buffer{1'000'000};
    null_resource empty_resource;

    bench.unit("block").run([&] {
        std::pmr::monotonic_buffer_resource monotonic_resource{block_buffer.data(), block_buffer.size(), &empty_resource};
        std::pmr::polymorphic_allocator<> allocator{&monotonic_resource};
        CBlock block(allocator);
        stream >> TX_WITH_WITNESS(block);
        bool rewound = stream.Rewind(benchmark::data::block413567.size());
        assert(rewound);
    });
}

static void DeserializeAndCheckBlockTest(benchmark::Bench& bench)
{
    DataStream stream(benchmark::data::block413567);
    std::byte a{0};
    stream.write({&a, 1}); // Prevent compaction

    ArgsManager bench_args;
    const auto chainParams = CreateChainParams(bench_args, ChainType::MAIN);

    bench.unit("block").run([&] {
        CBlock block; // Note that CBlock caches its checked state, so we need to recreate it here
        stream >> TX_WITH_WITNESS(block);
        bool rewound = stream.Rewind(benchmark::data::block413567.size());
        assert(rewound);

        BlockValidationState validationState;
        bool checked = CheckBlock(block, validationState, chainParams->GetConsensus());
        assert(checked);
    });
}

BENCHMARK(DeserializeBlockTest, benchmark::PriorityLevel::HIGH);
BENCHMARK(DeserializeBlockWithAllocTest, benchmark::PriorityLevel::HIGH);
BENCHMARK(DeserializeAndCheckBlockTest, benchmark::PriorityLevel::HIGH);
