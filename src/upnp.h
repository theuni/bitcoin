// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_UPNP_H
#define BITCOIN_UPNP_H

#include <string>
#include <memory>

struct IGDdatas;
struct UPNPDev;
struct UPNPUrls;
class CUPnP
{
    struct Deleter
    {
        void operator()(UPNPDev* ) const;
        void operator()(UPNPUrls*) const;
        void operator()(IGDdatas*) const;
    };
    static void FreeDevlist(UPNPDev* ptr);
    static void FreeUrls(UPNPUrls* ptr);
    typedef std::unique_ptr<UPNPDev , Deleter> UPNPDevPtr;
    typedef std::unique_ptr<UPNPUrls, Deleter> UPNPUrlsPtr;
    typedef std::unique_ptr<IGDdatas, Deleter> IGDdatasPtr;
public:
    CUPnP() = default;
    bool Discover(int timeout, std::string& error);
    std::string GetExternalIPAddress(std::string& error);
    bool AddPortMap(uint16_t port, std::string strDesc, std::string& error);
private:
    std::string GetExternalIPAddressInt(const char * controlURL, const char * servicetype, std::string& error);
    bool AddPortMapInt(const char* port, const char* strDesc, const char * controlURL, const char * servicetype, const char* lanaddr, std::string& error);

    std::string m_lanaddr;

    UPNPUrlsPtr m_urls;
    UPNPDevPtr m_devlist;
    IGDdatasPtr m_data;
};

#endif // BITCOIN_UPNP_H
