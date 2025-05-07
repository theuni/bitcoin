// Copyright (c) 2015-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <sync.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <stdexcept>

BOOST_AUTO_TEST_SUITE(reverselock_tests)

BOOST_AUTO_TEST_CASE(reverselock_basics)
{
    Mutex mutex;
    WAIT_LOCK(mutex, lock);

    BOOST_CHECK(lock.owns_lock());
    AssertLockHeld(mutex);
    {
        REVERSE_LOCK(lock, mutex);
        AssertLockNotHeld(mutex);
        BOOST_CHECK(!lock.owns_lock());
    }
    BOOST_CHECK(lock.owns_lock());
}

BOOST_AUTO_TEST_CASE(reverselock_multiple)
{
    Mutex mutex2;
    Mutex mutex;
    WAIT_LOCK(mutex2, lock2);
    WAIT_LOCK(mutex, lock);

    // Make sure undoing two locks succeeds
    {
        REVERSE_LOCK(lock, mutex);
        BOOST_CHECK(!lock.owns_lock());
        REVERSE_LOCK(lock2, mutex2);
        BOOST_CHECK(!lock2.owns_lock());
    }
    BOOST_CHECK(lock.owns_lock());
    BOOST_CHECK(lock2.owns_lock());
}

BOOST_AUTO_TEST_CASE(reverselock_errors)
{
    Mutex mutex2;
    Mutex mutex;
    WAIT_LOCK(mutex2, lock2);
    WAIT_LOCK(mutex, lock);

#ifdef DEBUG_LOCKORDER
    bool prev = g_debug_lockorder_abort;
    g_debug_lockorder_abort = false;

    // Make sure trying to reverse lock a previous lock fails
    BOOST_CHECK_EXCEPTION(REVERSE_LOCK(lock2, mutex2), std::logic_error, HasReason("lock2 was not most recent critical section locked"));
    BOOST_CHECK(lock2.owns_lock());

    g_debug_lockorder_abort = prev;
#endif
}

BOOST_AUTO_TEST_SUITE_END()
