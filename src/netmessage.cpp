// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <netmessage.h>
#include <memusage.h>

size_t CSerializedNetMsg::GetMemoryUsage() const noexcept
{
    return sizeof(*this) + memusage::DynamicUsage(m_type) + memusage::DynamicUsage(data);
}

size_t CNetMessage::GetMemoryUsage() const noexcept
{
    return sizeof(*this) + memusage::DynamicUsage(m_type) + m_recv.GetMemoryUsage();
}
