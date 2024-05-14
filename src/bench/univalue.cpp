// Copyright (c) 2024 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <bench/data.h>

#include <univalue.h>
#include <string>

static UniValue buildUniValue()
{
    UniValue ret{UniValue::VOBJ};
    ret.pushKV("string", std::string("a string"));
    ret.pushKV("int", 123);
    ret.pushKV("bool", true);
    return ret;
}

static void UniValuePushKVCopy(benchmark::Bench& bench)
{
    UniValue val = buildUniValue();
    std::vector<UniValue> vec(100000, val);
    size_t index{0};
    UniValue dest{UniValue::VOBJ};
    bench.unit("op").run([&] {
        for (const auto& val : vec) {
            dest.pushKV(std::to_string(index), val);
        }
    });
}

static void UniValuePushKVMove(benchmark::Bench& bench)
{
    UniValue val = buildUniValue();
    std::vector<UniValue> vec(100000, val);
    size_t index{0};
    UniValue dest{UniValue::VOBJ};
    bench.unit("op").run([&] {
        for (auto&& val : vec) {
            dest.pushKV(std::to_string(index), std::move(val));
        }
    });
}

static void UniValueAssignCopy(benchmark::Bench& bench)
{
    UniValue val = buildUniValue();
    std::vector<UniValue> vec(100000, val);
    UniValue dest;
    bench.unit("op").run([&] {
        for (const auto& val : vec) {
            dest = val;
        }
    });
}

static void UniValueAssignMove(benchmark::Bench& bench)
{
    UniValue val = buildUniValue();
    std::vector<UniValue> vec(100000, val);
    UniValue dest;
    bench.unit("op").run([&] {
        for (auto&& val : vec) {
            dest = std::move(val);
        }
    });
}

static void UniValueConstructCopy(benchmark::Bench& bench)
{
    UniValue val = buildUniValue();
    std::vector<UniValue> vec(100000, val);
    bench.unit("op").run([&] {
        for (const auto& val : vec) {
            UniValue dest(val);
        }
    });
}

static void UniValueConstructMove(benchmark::Bench& bench)
{
    UniValue val = buildUniValue();
    std::vector<UniValue> vec(100000, val);
    bench.unit("op").run([&] {
        for (auto&& val : vec) {
            UniValue dest(std::move(val));
        }
    });
}

BENCHMARK(UniValuePushKVCopy, benchmark::PriorityLevel::HIGH);
BENCHMARK(UniValuePushKVMove, benchmark::PriorityLevel::HIGH);
BENCHMARK(UniValueAssignCopy, benchmark::PriorityLevel::HIGH);
BENCHMARK(UniValueAssignMove, benchmark::PriorityLevel::HIGH);
BENCHMARK(UniValueConstructCopy, benchmark::PriorityLevel::HIGH);
BENCHMARK(UniValueConstructMove, benchmark::PriorityLevel::HIGH);
