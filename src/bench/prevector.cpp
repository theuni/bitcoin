// Copyright (c) 2016 the Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bench.h"

#include "prevector.h"
#include <vector>

static void VectorCopy(benchmark::State& state)
{
    int i(0);
    std::vector<unsigned char> vec1;
    while (state.KeepRunning()) {
        std::vector<unsigned char> vec2;
        vec1.push_back(i++);
        vec2 = vec1;
        assert((int)vec2.size() == i);
        vec1 = vec2;
        assert((int)vec1.size() == i);
        if(i % 5000 == 0) {
            vec1.clear();
            vec1.shrink_to_fit();
            i = 0;
        }
    }
}

static void PrevectorCopy(benchmark::State& state)
{
    int i(0);
    prevector<28, unsigned char> vec1;
    while (state.KeepRunning()) {
        prevector<28, unsigned char> vec2;
        vec1.push_back(i++);
        vec2 = vec1;
        assert((int)vec2.size() == i);
        vec1 = vec2;
        assert((int)vec1.size() == i);
        if(i % 5000 == 0) {
            vec1.clear();
            vec1.shrink_to_fit();
            i = 0;
        }
    }
}

static void VectorMove(benchmark::State& state)
{
    int i(0);
    std::vector<unsigned char> vec1;
    while (state.KeepRunning()) {
        std::vector<unsigned char> vec2;
        vec1.push_back(i++);
        vec2 = std::move(vec1);
        assert((int)vec2.size() == i);
        vec1 = std::move(vec2);
        assert((int)vec1.size() == i);
        if(i % 5000 == 0) {
            vec1.clear();
            vec1.shrink_to_fit();
            i = 0;
        }
    }
}

static void PrevectorMove(benchmark::State& state)
{
    int i(0);
    prevector<28, unsigned char> vec1;
    while (state.KeepRunning()) {
        prevector<28, unsigned char> vec2;
        vec1.push_back(i++);
        vec2 = std::move(vec1);
        assert((int)vec2.size() == i);
        vec1 = std::move(vec2);
        assert((int)vec1.size() == i);
        if(i % 5000 == 0) {
            vec1.clear();
            vec1.shrink_to_fit();
            i = 0;
        }
    }
}

BENCHMARK(VectorCopy);
BENCHMARK(VectorMove);
BENCHMARK(PrevectorCopy);
BENCHMARK(PrevectorMove);
