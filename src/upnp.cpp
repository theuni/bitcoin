#include "upnp.h"

#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/miniwget.h>
#include <miniupnpc/upnpcommands.h>
#include <miniupnpc/upnperrors.h>
#include <assert.h>
#include <future>

void CUPnP::Deleter::operator()(UPNPDev* ptr) const
{
    freeUPNPDevlist(ptr);
}

void CUPnP::Deleter::operator()(UPNPUrls* ptr) const
{
    FreeUPNPUrls(ptr);
    delete ptr;
}
void CUPnP::Deleter::operator()(IGDdatas* ptr) const
{
    delete ptr;
}

bool CUPnP:: Discover(int timeout, std::string& error)
{
    constexpr const char* multicastif = nullptr;
    constexpr const char* minissdpdpath = nullptr;
#ifdef UPNP_LOCAL_PORT_ANY
    constexpr int localport = UPNP_LOCAL_PORT_ANY;
#else
    constexpr int localport = 0;
#endif
    constexpr int searchalltypes = 0;
    constexpr int ttl = 2;
    int errcode = ttl;

    UPNPDevPtr devlist;
#ifndef UPNPDISCOVER_SUCCESS
    /* miniupnpc 1.5 */
    (void)searchalltypes;
    (void)ttl;
    (void)errcode;
    devlist.reset(upnpDiscover(timeout, multicastif, minissdpdpath, localport));
#elif MINIUPNPC_API_VERSION < 14
    /* miniupnpc 1.6 */
    (void)ttl;
    devlist.reset(upnpDiscover(timeout, multicastif, minissdpdpath, localport, searchalltypes, &errcode));
#else
    /* miniupnpc 1.9.20150730 */
    devlist.reset(upnpDiscover(timeout, multicastif, minissdpdpath, localport, searchalltypes, ttl, &errcode));
#endif
    printf("discover done\n");
#ifdef UPNPDISCOVER_SUCCESS
    if(errcode != UPNPCOMMAND_SUCCESS) {
        error.assign("Failed to discover");
        return false;
    }
#else
    if (!devlist) {
        error.assign("failed to discover");
        return false;
    }
#endif

    UPNPUrlsPtr urls(new UPNPUrls{});
    IGDdatasPtr data(new IGDdatas{});
    char lanaddr[64] = {0};

    int igderr = UPNP_GetValidIGD(devlist.get(), urls.get(), data.get(), lanaddr, sizeof(lanaddr));
    if(igderr != 1) {
        error.assign("No valid UPnP IGDs found\n");
        return false;
    }

    m_devlist = std::move(devlist);
    m_urls = std::move(urls);
    m_data = std::move(data);
    m_lanaddr.assign(lanaddr);
    return true;
}

std::string CUPnP::GetExternalIPAddressInt(const char * controlURL, const char * servicetype, std::string& error)
{
    int errcode;
    assert(controlURL);
    assert(servicetype);
    std::string result;
    char externalIPAddress[40] = {0};
    errcode = UPNP_GetExternalIPAddress(controlURL, servicetype, externalIPAddress);
    if (errcode == UPNPCOMMAND_SUCCESS)
        result.assign(externalIPAddress);
    else
        error.assign(strupnperror(errcode));
    return result;
}

std::string CUPnP::GetExternalIPAddress(std::string& error)
{
    std::string ret;
    std::string controlURL;
    std::string servicetype;
    {
        controlURL.assign(m_urls->controlURL);
        servicetype.assign(m_data->first.servicetype);
    }
    if(controlURL.empty() || servicetype.empty())
        error = "need to discover";
    else
        ret = GetExternalIPAddressInt(controlURL.c_str(), servicetype.c_str(), error);
    return ret;
}

bool CUPnP::AddPortMapInt(const char* port, const char* strDesc, const char * controlURL, const char * servicetype, const char* lanaddr, std::string& error)
{
    assert(port);
    assert(strDesc);
    assert(controlURL);
    assert(servicetype);
    assert(lanaddr);

    int errcode;
#ifndef UPNPDISCOVER_SUCCESS
    /* miniupnpc 1.5 */
    errcode = UPNP_AddPortMapping(controlURL, servicetype,
                                port, port, lanaddr, strDesc, "TCP", 0);
#else
    /* miniupnpc 1.6 */
    errcode = UPNP_AddPortMapping(controlURL, servicetype,
                                port, port, lanaddr, strDesc, "TCP", 0, "0");
#endif
    if (errcode != UPNPCOMMAND_SUCCESS)
        error.assign(strupnperror(errcode));

    return errcode == UPNPCOMMAND_SUCCESS;
}

bool CUPnP::AddPortMap(uint16_t port, std::string strDesc, std::string& error)
{
    bool ret = false;
    std::string controlURL;
    std::string servicetype;
    std::string lanaddr;
    {
        controlURL.assign(m_urls->controlURL);
        servicetype.assign(m_data->first.servicetype);
        lanaddr.assign(m_lanaddr);
    }

    if(controlURL.empty() || servicetype.empty() || lanaddr.empty())
        error = "need to discover";
    else
        ret = AddPortMapInt(std::to_string(port).c_str(), strDesc.c_str(), controlURL.c_str(), servicetype.c_str(), lanaddr.c_str(), error);
    return ret;
}

int main()
{
    CUPnP test;
    std::string errstr;
    test.Discover(2000, errstr);
    printf("errstr: %s\n", errstr.c_str());
    auto getip([&test, &errstr](std::string& iperrstr) -> std::string {
        return test.GetExternalIPAddress(iperrstr);
    });
    std::packaged_task<std::string(std::string&)> iptask;
    auto fut = iptask.get_future();
    //std::thread(iptask(errstr));
    fut.get();
}
