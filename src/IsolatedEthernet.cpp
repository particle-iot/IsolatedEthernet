#include "IsolatedEthernet.h"

// For POSIX filesystem used for the config file
#include <fcntl.h>
#include <sys/stat.h>

// LwIP defines these, undefine them to avoid warnings. The WIZnet code itself does not
// include Particle.h so these conflicts don't occur there.
#undef SOCK_STREAM
#undef SOCK_DGRAM

#include "wizchip_conf.h"
#include "dhcp.h"
#include "dns.h"
#include "socket.h"

IsolatedEthernet *IsolatedEthernet::_instance;

// [static]
IsolatedEthernet &IsolatedEthernet::instance()
{
    if (!_instance)
    {
        _instance = new IsolatedEthernet();
    }
    return *_instance;
}

IsolatedEthernet::IsolatedEthernet() : appLog("app.ether")
{
}

IsolatedEthernet::~IsolatedEthernet()
{
}


void IsolatedEthernet::setup()
{

    new Thread("IsolatedEthernet", threadFunctionStatic, this, OS_THREAD_PRIORITY_DEFAULT, OS_THREAD_STACK_SIZE_DEFAULT);

    if (pinCS != PIN_INVALID)
    {
        pinMode(pinCS, OUTPUT);
        digitalWrite(pinCS, HIGH);
    }

    if (pinINT != PIN_INVALID)
    {
        pinMode(pinINT, INPUT);
    }

    if (pinRESET != PIN_INVALID)
    {
        pinMode(pinRESET, OUTPUT);
        digitalWrite(pinRESET, HIGH);
    }

    // We manually set the CS pin, so don't do it in SPI.begin()
    spi->begin(PIN_INVALID);
    if (!hwReset())
    {
        // TODO: Implement software reset here if there is no reset pin defined
    }

    setMacAddress();

    // Set up bridge between WIZnet and this library
    reg_wizchip_cris_cbfunc(
        [](void)
        {
            instance().wizchip_cris_enter();
        },
        [](void)
        {
            instance().wizchip_cris_exit();
        });

    reg_wizchip_cs_cbfunc(
        [](void)
        {
            instance().wizchip_cs_select();
        },
        [](void)
        {
            instance().wizchip_cs_deselect();
        });


    reg_wizchip_spi_cbfunc(
        [](void)
        {
            return instance().wizchip_spi_readbyte();
        },
        [](uint8_t wb)
        {
            instance().wizchip_spi_writebyte(wb);
        });

    reg_wizchip_spiburst_cbfunc(
        [](uint8_t *pBuf, uint16_t len)
        {
            instance().wizchip_spi_readburst(pBuf, len);
        },
        [](uint8_t *pBuf, uint16_t len)
        {
            instance().wizchip_spi_writeburst(pBuf, len);
        });

    // This can only be done after setting callbacks
    wizchip_sw_reset();

    {
        // Initialize chip using default buffer sizes (2K per socket)
        int8_t res = wizchip_init(NULL, NULL);
        if (res != 0)
        {
            appLog.info("wizchip_init failed res=%d", (int)res);
        }
    }

    {
        wiz_PhyConf phyConfSet = {0};
        wiz_PhyConf phyConf = {0};

        phyConfSet.by = PHY_CONFBY_HW;
        phyConfSet.mode = PHY_MODE_AUTONEGO;

        wizphy_setphyconf(&phyConfSet);

        wizphy_getphyconf(&phyConf);
        appLog.trace("phyConf: by=%d mode=%d speed=%d duplex=%d", (int)phyConf.by, (int)phyConf.mode, (int)phyConf.speed, (int)phyConf.duplex);

        if (phyConfSet.by != phyConf.by || phyConfSet.mode != phyConf.mode) {
            appLog.error("phyConf did not set properly, connection to W5500 is probably not working");
        }

    }

    {
        wiz_NetInfo netInfo = {0};

        memcpy(netInfo.mac, macAddr, sizeof(macAddr));
        netInfo.dhcp = (dhcpState != DhcpState::NOT_USED) ? NETINFO_DHCP : NETINFO_STATIC;

        wizchip_setnetinfo(&netInfo);
    }

    if (dhcpState == DhcpState::ATTEMPT)
    {

        reg_dhcp_cbfunc(
            [](void)
            {
                instance().appLog.trace("ip_assign");
                instance().updateAddressSettingsFromDHCP();
                instance().dhcpState = DhcpState::GOT_ADDRESS;
            },
            [](void)
            {
                instance().appLog.trace("ip_update");
                instance().updateAddressSettingsFromDHCP();
            },
            [](void)
            {
                instance().appLog.trace("ip_conflict");
            });
    }

    setupDone = true;

    if (jsonConfigFile.length()) {
        loadConfigFile();
    }
    if (ipAddr[0] != 0 || ipAddr[1] != 0 || ipAddr[2] != 0 || ipAddr[3] != 0) {
        // Used for both JSON config file and setting it manually
        updateAddressSettings();
    }

}

void IsolatedEthernet::stateMachine()
{
    static unsigned long lastDhcpCheck = 0;
    static unsigned long lastDnsCheck = 0;

    bool curPhyLink = (wizphy_getphylink() == PHY_LINK_ON);
    if (curPhyLink != phyLink)
    {
        if (curPhyLink)
        {
            appLog.trace("PHY link up");
            callCallbacks(CallbackType::linkUp);

            switch(dhcpState) {
                case DhcpState::NOT_USED:
                    if (ipAddr[0] != 0 || ipAddr[1] != 0 || ipAddr[2] != 0 || ipAddr[3] != 0) {
                        isReady = true;
                        callCallbacks(CallbackType::gotIpAddress);
                    }
                    break;

                default:
                    break;
            }
        }
        else
        {
            appLog.trace("PHY link down");
            callCallbacks(CallbackType::linkDown);
            isReady = false;
        }
        phyLink = curPhyLink;
    }

    switch (dhcpState)
    {
    case DhcpState::ATTEMPT:
        if (phyLink)
        {
            appLog.trace("attempting to get DHCP");
            if (!dhcpBuffer)
            {
                dhcpBuffer = new uint8_t[548]; // RIP_MSG_SIZE, this isn't exported for some reason
            }
            int dhcpSocket = socketGetFree();
            if (dhcpSocket >= 0)
            {
                DHCP_init((uint8_t)dhcpSocket, dhcpBuffer);
                dhcpState = DhcpState::IN_PROGRESS;
            }
            else {
                appLog.error("No sockets for DHCP");
                dhcpState = DhcpState::DONE;
            }
        }
        break;

    case DhcpState::IN_PROGRESS:
        if (millis() - lastDhcpCheck >= 1000)
        {
            lastDhcpCheck = millis();

            // Call once per second
            DHCP_time_handler();
        }
        DHCP_run();
        if (!phyLink)
        {
            dhcpState = DhcpState::CLEANUP;
        }
        break;

    case DhcpState::GOT_ADDRESS:
        // Can't call DHCP_stop from the dhcp callback function!
        callCallbacks(CallbackType::gotIpAddress);
        dhcpState = DhcpState::CLEANUP;
        break;

    case DhcpState::CLEANUP_DISABLE:
    case DhcpState::CLEANUP:
        DHCP_stop();
        delete[] dhcpBuffer;
        dhcpBuffer = NULL;
        if (dhcpState == DhcpState::CLEANUP_DISABLE) {
            dhcpState = DhcpState::NOT_USED;
        }
        else {
            dhcpState = DhcpState::DONE;
        }
        break;

    case DhcpState::DONE:
        if (!phyLink)
        {
            // Lost link, get DHCP again
            appLog.trace("lost link, will attempt to get DHCP address again");
            dhcpState = DhcpState::ATTEMPT;
        }
        break;
    }

    if (dnsBuffer)
    {
        if (millis() - lastDnsCheck >= 1000)
        {
            lastDnsCheck = millis();
            DNS_time_handler();
        }
    }
}

bool IsolatedEthernet::hwReset()
{
    if (pinRESET != PIN_INVALID)
    {
        digitalWrite(pinRESET, LOW);
        delay(1);
        digitalWrite(pinRESET, HIGH);
        delay(1);
        return true;
    }
    else
    {
        return false;
    }
}

int IsolatedEthernet::inet_gethostbyname(const char *hostname, uint16_t hostnameLen, HAL_IPAddress *out_ip_addr, network_interface_t nif, void *reserved)
{
#if HAL_IPv6
    out_ip_addr->v = 4;
#endif
    int res = 0;
    if (!dnsBuffer)
    {
        dnsBuffer = new uint8_t[MAX_DNS_BUF_SIZE];
        if (!dnsBuffer)
        {
            return -1;
        }
    }

    int dnsSocket = socketGetFree();
    if (dnsSocket >= 0)
    {
        DNS_init((uint8_t)dnsSocket, dnsBuffer);

        uint8_t ipAddr[4];

        int8_t winRes = DNS_run(dnsAddr, const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(hostname)), ipAddr);
        if (winRes == 1)
        {
            IPAddress addr(ipAddr);
            *out_ip_addr = addr.raw();
            appLog.trace("dns success %s->%u.%u.%u.%u", hostname, ipAddr[0], ipAddr[1], ipAddr[2], ipAddr[3]);
            res = 0;
        }
        else
        {
            appLog.trace("dns error %s %d", hostname, res);
            res = -1;
        }
    }
    else
    {
        appLog.error("no available sockets for dns");
        res = -1;
    }

    delete[] dnsBuffer;

    return res;
}

IsolatedEthernet &IsolatedEthernet::withStaticIP()
{
    switch(dhcpState) {
        case DhcpState::IN_PROGRESS:
        case DhcpState::GOT_ADDRESS:
            dhcpState = DhcpState::CLEANUP_DISABLE;
            break;

        default:
            dhcpState = DhcpState::NOT_USED;
            break;
    }
    return *this;
}

IsolatedEthernet &IsolatedEthernet::withDHCP() 
{
    ipAddr[0] = ipAddr[1] = ipAddr[2] = ipAddr[3] = 0;
    dhcpState = DhcpState::ATTEMPT;
    return *this;
}

void IsolatedEthernet::loadJsonConfig(const char *str)
{
    JSONValue configObj = JSONValue::parseCopy(str);

    loadJsonConfig(configObj);
}

void IsolatedEthernet::loadJsonConfig(const JSONValue &configObj)
{
    JSONObjectIterator iter(configObj);
    while(iter.next()) {        
        // appLog.trace("key=%s value=%s", (const char *)iter.name(), (const char *)iter.value().toString());

        IPAddress addr;
        unsigned int a[4];
        bool isAddr = sscanf(iter.value().toString().data(), "%u.%u.%u.%u", &a[0], &a[1], &a[2], &a[3]) == 4;
        if (isAddr) {
            addr = IPAddress((uint8_t) a[0], (uint8_t) a[1], (uint8_t) a[2], (uint8_t) a[3]);
            appLog.trace("Addr: %s", addr.toString().c_str());
        }

        if (iter.name() == "ipAddr") {
            withIPAddress(addr);
        }
        else
        if (iter.name() == "subnetMask") {
            withSubnetMask(addr);
        }
        else
        if (iter.name() == "gatewayAddr") {
            withGatewayAddress(addr);
        }
        else
        if (iter.name() == "dnsAddr") {
            withDNSAddress(addr);
        }
        else
        if (iter.name() == "DHCP") {
            if (iter.value().toBool()) {
                withDHCP();
            }
            else {
                withStaticIP();
            }
        }
    }
}

void IsolatedEthernet::loadConfigFile() 
{
    char buf[256];

    int fd = open(jsonConfigFile.c_str(), O_RDONLY);
    if (fd != -1) {
        int res = read(fd, buf, sizeof(buf) - 1);
        
        close(fd);

        if (res > 0) {
            buf[res] = 0;
            if (res > 2 && buf[0] == '{') {
                appLog.trace("loading config: %s", buf);
                loadJsonConfig(buf);                    
            }
            else {
                appLog.trace("config file appears to be invalid, ignoring");
            }
        }
        else {
            appLog.trace("config file empty or error %s %d", jsonConfigFile.c_str(), res);
        }
    }
    else {
        appLog.trace("no config file present %s", jsonConfigFile.c_str());
    }


}

bool IsolatedEthernet::saveConfigFile() 
{
    char buf[256];

    memset(buf, 0, sizeof(buf));
    JSONBufferWriter writer(buf, sizeof(buf) - 1);
    writer.beginObject();
        writer.name("ipAddr").value(IsolatedEthernet::arrayToString(ipAddr).c_str());
        writer.name("subnetMask").value(IsolatedEthernet::arrayToString(subnetMaskArray).c_str());
        writer.name("gatewayAddr").value(IsolatedEthernet::arrayToString(gatewayAddr).c_str());
        writer.name("dnsAddr").value(IsolatedEthernet::arrayToString(dnsAddr).c_str());
    writer.endObject();


    int fd = open(jsonConfigFile.c_str(), O_RDWR | O_CREAT | O_TRUNC);
    if (fd != -1) {
        size_t len = strlen(buf);

        appLog.trace("saving config len=%d: %s", len, buf);
        int res = write(fd, buf, len);
        if (res != (int)len) {
            appLog.error("writing config failed %d", res);
        }
        fsync(fd);
        close(fd);


        return true;
    }
    else {
        appLog.error("could not open config file %s", jsonConfigFile.c_str());
    }

    return false;
}

void IsolatedEthernet::updateAddressSettingsFromDHCP()
{

    getIPfromDHCP(ipAddr);
    getSNfromDHCP(subnetMaskArray);
    getGWfromDHCP(gatewayAddr);
    getDNSfromDHCP(dnsAddr);

    updateAddressSettings();
}

void IsolatedEthernet::updateAddressSettings()
{
    appLog.trace("updateAddressSettings ipAddr=%u.%u.%u.%u subnetMaskArray=%u.%u.%u.%u gatewayAddr=%u.%u.%u.%u dnsAddr=%u.%u.%u.%u",
                ipAddr[0], ipAddr[1], ipAddr[2], ipAddr[3],
                subnetMaskArray[0], subnetMaskArray[1], subnetMaskArray[2], subnetMaskArray[3],
                gatewayAddr[0], gatewayAddr[1], gatewayAddr[2], gatewayAddr[3],
                dnsAddr[0], dnsAddr[1], dnsAddr[2], dnsAddr[3]);

    wiz_NetInfo netInfo = {0};
    memcpy(netInfo.mac, macAddr, sizeof(macAddr));
    memcpy(netInfo.ip, ipAddr, sizeof(ipAddr));
    memcpy(netInfo.sn, subnetMaskArray, sizeof(subnetMaskArray));
    memcpy(netInfo.gw, gatewayAddr, sizeof(gatewayAddr));
    memcpy(netInfo.dns, dnsAddr, sizeof(dnsAddr));
    netInfo.dhcp = (dhcpState != DhcpState::NOT_USED) ? NETINFO_DHCP : NETINFO_STATIC;

    wizchip_setnetinfo(&netInfo);

    bool curPhyLink = (wizphy_getphylink() == PHY_LINK_ON);
    if (curPhyLink) {
        isReady = true;
    }
}

// [static]
void IsolatedEthernet::ipAddressToArray(const IPAddress &addr, uint8_t *array) {
    array[0] = addr[0];
    array[1] = addr[1];
    array[2] = addr[2];
    array[3] = addr[3];
}

// [static]
String IsolatedEthernet::arrayToString(const uint8_t *array) {
    return String::format("%u.%u.%u.%u", (unsigned int) array[0], (unsigned int) array[1], (unsigned int) array[2], (unsigned int) array[3]);
}

void IsolatedEthernet::callCallbacks(CallbackType type, void *data)
{
    for (auto it = callbacks.begin(); it != callbacks.end(); it++)
    {
        (*it)(type, data);
    }
}

int IsolatedEthernet::socketGetFree()
{
    for (uint8_t ii = 0; ii < NUM_SOCKETS; ii++)
    {
        uint8_t status;
        wiznet::getsockopt(ii, wiznet::SO_STATUS, &status);
        if (status == SOCK_CLOSED)
        {
            return (int)ii;
        }
    }

    return -1; // No free sockets
}

void IsolatedEthernet::beginTransaction()
{
    spi->beginTransaction(spiSettings);
    if (pinCS != PIN_INVALID)
    {
        pinResetFast(pinCS);
    }
}

void IsolatedEthernet::endTransaction()
{
    if (pinCS != PIN_INVALID)
    {
        pinSetFast(pinCS);
    }
    spi->endTransaction();
}

void IsolatedEthernet::wizchip_cris_enter(void)
{
    spi->beginTransaction(spiSettings);
}

void IsolatedEthernet::wizchip_cris_exit(void)
{
    spi->endTransaction();
}

void IsolatedEthernet::wizchip_cs_select(void)
{
    if (pinCS != PIN_INVALID)
    {
        pinResetFast(pinCS);
    }
}

void IsolatedEthernet::wizchip_cs_deselect(void)
{
    if (pinCS != PIN_INVALID)
    {
        pinSetFast(pinCS);
    }
}

uint8_t IsolatedEthernet::wizchip_spi_readbyte(void)
{
    return spi->transfer(0xff);
}

void IsolatedEthernet::wizchip_spi_writebyte(uint8_t wb)
{
    spi->transfer(wb);
}

void IsolatedEthernet::wizchip_spi_readburst(uint8_t *pBuf, uint16_t len)
{
    spi->transfer(NULL, pBuf, len, NULL);
}

void IsolatedEthernet::wizchip_spi_writeburst(uint8_t *pBuf, uint16_t len)
{
    spi->transfer(pBuf, NULL, len, NULL);
}

os_thread_return_t IsolatedEthernet::threadFunction()
{
    while (true)
    {
        if (setupDone) {
            stateMachine();
        }
        delay(1);
    }
}

// [static]
os_thread_return_t IsolatedEthernet::threadFunctionStatic(void *param)
{
    ((IsolatedEthernet *)param)->threadFunction();
}


void IsolatedEthernet::setMacAddress() 
{
#if HAL_PLATFORM_NRF52840
    const uint32_t lsb = __builtin_bswap32(NRF_FICR->DEVICEADDR[0]);
    const uint32_t msb = NRF_FICR->DEVICEADDR[1] & 0xffff;
    memcpy(macAddr + 2, &lsb, sizeof(lsb));
    macAddr[0] = msb >> 8;
    macAddr[1] = msb;
    // Drop 'multicast' bit
    macAddr[0] &= 0b11111110;
    // Set 'locally administered' bit
    macAddr[0] |= 0b10;
#elif HAL_PLATFORM_RTL872X
    #define HAL_DEVICE_MAC_ETHERNET         3
    WiFi.macAddress(macAddr);
    macAddr[5] += HAL_DEVICE_MAC_ETHERNET;
#error "Unsupported platform" // temporary, as the P2 support is not currently working
#else
#error "Unsupported platform"
#endif

    appLog.trace("mac %02x:%02x:%02x:%02x:%02x:%02x", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);

}


extern "C" void wizchip_debug(const char *fmt, ...)
{
    va_list ap;
    char fmt2[100];
    char buf[100];

    const char *src = fmt;
    char *dst = fmt2;
    char *dstEnd = &fmt2[sizeof(fmt2) - 1];
    while (*src && dst < dstEnd)
    {
        if (*src != '\r' && *src != '\n')
        {
            *dst++ = *src++;
        }
        else
        {
            src++;
        }
    }
    *dst = 0;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt2, ap);
    va_end(ap);

    IsolatedEthernet::instance().appLog.trace("%s", buf);
}

extern "C" void wizchip_yield()
{
    delay(1);
}

//
// TCPClient
//

bool IsolatedEthernet::TCPClient::isOpen(sock_handle_t sd)
{
    uint8_t status;
    wiznet::getsockopt((uint8_t)sd, wiznet::SO_STATUS, &status);
    return status == SOCK_ESTABLISHED;
}

IsolatedEthernet::TCPClient::TCPClient() : TCPClient(-1)
{
}

IsolatedEthernet::TCPClient::TCPClient(sock_handle_t sock) :
        d_(std::make_shared<Data>(sock))
{
    flush_buffer();
}


// return 0 on error, 1 on success
int IsolatedEthernet::TCPClient::connect(const char *host, uint16_t port, network_interface_t nif)
{
    stop();
    if (IsolatedEthernet::instance().ready())
    {
        HAL_IPAddress halIpAddress;
        if (IsolatedEthernet::instance().inet_gethostbyname(host, strlen(host), &halIpAddress, 0, NULL) == 0)
        {
            IPAddress ip_addr(halIpAddress);
            return connect(ip_addr, port, nif);
        }
        else
        {
            IsolatedEthernet::instance().appLog.trace("unable to get IP for hostname");
        }
    }

    return 0; // error, could not connect
}

// return 0 on error, 1 on success
int IsolatedEthernet::TCPClient::connect(IPAddress ip, uint16_t port, network_interface_t nif)
{
    stop();

    IsolatedEthernet::instance().appLog.trace("TCPClient connect(%s %d)", ip.toString().c_str(), (int)port);                

    int connected = 0;
    if (IsolatedEthernet::instance().ready())
    {
        int sock = IsolatedEthernet::instance().socketGetFree();
        if (sock >= 0) {
            IsolatedEthernet::instance().appLog.trace("TCPClient using socket=%d", sock);

            int8_t res = wiznet::socket((uint8_t) sock, Sn_MR_TCP, port, 0);
            if (res >= 0) {
                d_->sock = sock;
                // IsolatedEthernet::instance().appLog.trace("TCPClient socket() success");
            }
            else {
                IsolatedEthernet::instance().appLog.trace("TCPClient socket error %d", (int) res);
            }
        }
        else {
            IsolatedEthernet::instance().appLog.error("TCPClient no available sockets");                    
        }

        if (socket_handle_valid(sock_handle()))
        {
            flush_buffer();

            uint8_t addr[4];
            IsolatedEthernet::ipAddressToArray(ip, addr);

            int8_t res = wiznet::connect(sock_handle(), addr, port);
            if (res == SOCK_OK) {
                // IsolatedEthernet::instance().appLog.trace("TCPClient connect() success");                
                connected = true;
            }
            else {
                IsolatedEthernet::instance().appLog.trace("TCPClient connect() res=%d", res);
                connected = false;
            }

            // Once connected, switch to non-blocking I/O mode
            uint8_t mode = SOCK_IO_NONBLOCK;
            wiznet::ctlsocket(sock_handle(), wiznet::CS_SET_IOMODE, &mode);

            d_->remoteIP = ip;
            nif_ = nif;
            if (!connected)
            {
                stop();
            }
        }
        else {
            IsolatedEthernet::instance().appLog.trace("TCPClient invalid socket handle %d", sock_handle());

        }
    }
    return connected;
}

size_t IsolatedEthernet::TCPClient::write(uint8_t b)
{
    return write(&b, 1, SPARK_WIRING_TCPCLIENT_DEFAULT_SEND_TIMEOUT);
}

size_t IsolatedEthernet::TCPClient::write(const uint8_t *buffer, size_t size)
{
    return write(buffer, size, SPARK_WIRING_TCPCLIENT_DEFAULT_SEND_TIMEOUT);
}

size_t IsolatedEthernet::TCPClient::write(uint8_t b, system_tick_t timeout)
{
    return write(&b, 1, timeout);
}

size_t IsolatedEthernet::TCPClient::write(const uint8_t *buffer, size_t size, system_tick_t timeout)
{
    clearWriteError();

    int ret = -1;
    unsigned long start = millis();
    size_t offset = 0;

    do {
        ret = wiznet::send(sock_handle(), const_cast<uint8_t *>(buffer + offset), size - offset);
        if (ret > 0) {
            offset += ret;
            if (offset == size) {
                ret = size;
                break;
            }
        }
        else
        if (ret != SOCK_BUSY) {
            setWriteError(ret);
         
            break;
        }
        delay(1);
    } while(timeout != 0 && millis() - start < timeout);

    /*
     * FIXME: We should not be returning negative numbers here
     */
    return ret;
}

int IsolatedEthernet::TCPClient::bufferCount()
{
    return d_->total - d_->offset;
}

int IsolatedEthernet::TCPClient::available()
{
    int avail = 0;

    // At EOB => Flush it
    if (d_->total && (d_->offset == d_->total))
    {
        flush_buffer();
    }

    if (IsolatedEthernet::instance().ready() && isOpen(sock_handle()))
    {
        // Have room
        if (d_->total < arraySize(d_->buffer))
        {
            // int ret = socket_receive(sock_handle(), d_->buffer + d_->total, arraySize(d_->buffer) - d_->total, 0);
            int ret = wiznet::recv(sock_handle(), d_->buffer + d_->total, arraySize(d_->buffer) - d_->total);
            if (ret > 0)
            {
                DEBUG("recv(=%d)", ret);
                if (d_->total == 0)
                    d_->offset = 0;
                d_->total += ret;
            }
        } // Have Space
    }     // WiFi.ready() && isOpen(sock_handle())
    avail = bufferCount();
    return avail;
}

int IsolatedEthernet::TCPClient::read()
{
    return (bufferCount() || available()) ? d_->buffer[d_->offset++] : -1;
}

int IsolatedEthernet::TCPClient::read(uint8_t *buffer, size_t size)
{
    int read = -1;
    if (bufferCount() || available())
    {
        read = (size > (size_t)bufferCount()) ? bufferCount() : size;
        memcpy(buffer, &d_->buffer[d_->offset], read);
        d_->offset += read;
    }
    return read;
}

int IsolatedEthernet::TCPClient::peek()
{
    return (bufferCount() || available()) ? d_->buffer[d_->offset] : -1;
}

void IsolatedEthernet::TCPClient::flush_buffer()
{
    d_->offset = 0;
    d_->total = 0;
}

void IsolatedEthernet::TCPClient::flush()
{
    uint16_t bufSize = getSn_TxMAX(sock_handle());
    uint16_t freeSize = getSn_TX_FSR(sock_handle());

    while(freeSize < bufSize) {
        delay(1);
        freeSize = getSn_TX_FSR(sock_handle());
    }
}

void IsolatedEthernet::TCPClient::stop()
{
    if (sock_handle() < 0) {
        return;
    }

    // This log line pollutes the log too much
    IsolatedEthernet::instance().appLog.trace("sock %d closesocket", sock_handle());

    // if (isOpen(sock_handle()))
    int8_t res = wiznet::disconnect(sock_handle());
    if (res != SOCK_OK) {
        IsolatedEthernet::instance().appLog.trace("sock %d disconnect failed %d", sock_handle(), (int) res);
    }
    d_->sock = -1;
    d_->remoteIP.clear();
    flush_buffer();
}

uint8_t IsolatedEthernet::TCPClient::connected()
{
    // Wlan up, open and not in CLOSE_WAIT or data still in the local buffer
    bool rv = (status() || bufferCount());
    // no data in the local buffer, Socket open but my be in CLOSE_WAIT yet the CC3000 may have data in its buffer
    if (!rv && isOpen(sock_handle()))
    {
        rv = available(); // Try CC3000
        if (!rv)
        { // No more Data and CLOSE_WAIT
            IsolatedEthernet::instance().appLog.trace("calling .stop(), no more data, in CLOSE_WAIT");
            stop(); // Close our side
        }
    }
    return rv;
}

uint8_t IsolatedEthernet::TCPClient::status()
{
    return (isOpen(sock_handle()) && IsolatedEthernet::instance().ready());
}

IsolatedEthernet::TCPClient::operator bool()
{
    return (status() != 0);
}

IPAddress IsolatedEthernet::TCPClient::remoteIP()
{
    return d_->remoteIP;
}


IsolatedEthernet::TCPClient::Data::Data(sock_handle_t sock)
    : sock(sock),
      offset(0),
      total(0)
{
}

IsolatedEthernet::TCPClient::Data::~Data()
{
    if (socket_handle_valid(sock))
    {
        wiznet::close(sock);
    }
}

//
// IsolatedEthernet::TCPServer
//
static IsolatedEthernet::TCPClient* s_invalid_client = nullptr;

#ifndef IN6_IS_ADDR_V4MAPPED
#define IN6_IS_ADDR_V4MAPPED(a) \
    ((((uint32_t*)(a))[0] == 0) && (((uint32_t*)(a))[1] == 0) && \
     (((uint32_t*)(a))[2] == htonl(0xffff)))
#endif // IN6_IS_ADDR_V4MAPPED

#define ntohl(x) inet_ntohl(x)
#define htonl(x) inet_htonl(x)
#define ntohs(x) inet_ntohs(x)
#define htons(x) inet_htons(x)

static void sockaddrToIpAddressPort(const struct sockaddr* saddr, IPAddress& addr, uint16_t* port) {
    if (saddr->sa_family == AF_INET) {
        const struct sockaddr_in* inaddr = (const struct sockaddr_in*)saddr;
        addr = (const uint8_t*)(&inaddr->sin_addr.s_addr);
        if (port) {
            *port = ntohs(inaddr->sin_port);
        }
    }
#if HAL_IPv6
    else if (saddr->sa_family == AF_INET6) {
        const struct sockaddr_in6* in6addr = (const struct sockaddr_in6*)saddr;
        HAL_IPAddress a = {};
        if (!IN6_IS_ADDR_V4MAPPED(&in6addr->sin6_addr)) {
            memcpy(a.ipv6, in6addr->sin6_addr.s6_addr, sizeof(a.ipv6));
            a.v = 6;
            addr = IPAddress(a);
        } else {
            auto ptr = (const uint32_t*)(in6addr->sin6_addr.s6_addr);
            addr = (const uint8_t*)(&ptr[3]);
        }
        if (port) {
            *port = ntohs(in6addr->sin6_port);
        }
    }
#endif // HAL_IPv6
}

class TCPServerClient: public IsolatedEthernet::TCPClient {
public:
    TCPServerClient(sock_handle_t sock) : IsolatedEthernet::TCPClient(sock) {
    }

    virtual IPAddress remoteIP() override {
        IPAddress addr;
        struct sockaddr_storage saddr = {};
        socklen_t len = sizeof(saddr);
        if (!sock_getpeername(sock_handle(), (struct sockaddr*)&saddr, &len)) {
            sockaddrToIpAddressPort((const struct sockaddr*)&saddr, addr, nullptr);
        }
        return addr;
    }
};

IsolatedEthernet::TCPServer::TCPServer(uint16_t port, network_interface_t nif)
        : _port(port),
          _nif(nif),
          _sock(-1),
          _client(-1) {
    SINGLE_THREADED_BLOCK() {
        if (!s_invalid_client) {
            s_invalid_client = new IsolatedEthernet::TCPClient(-1);
        }
    }
}

bool IsolatedEthernet::TCPServer::startListener() {
    bool result = false;

    int sock = IsolatedEthernet::instance().socketGetFree();
    if (sock >= 0) {
        IsolatedEthernet::instance().appLog.trace("TCPServer using socket=%d", sock);

        int8_t res = wiznet::socket((uint8_t) sock, Sn_MR_TCP, _port, 0);
        if (res >= 0) {
            _sock = sock;
            // IsolatedEthernet::instance().appLog.trace("TCPServer socket() success");

            int8_t res = wiznet::listen(_sock);
            if (res == SOCK_OK) {
                result = true;
            }
            else {
                IsolatedEthernet::instance().appLog.trace("TCPServer listen error=%d sock=%d", (int) res, (int)_sock);
            }
        }
        else {
            IsolatedEthernet::instance().appLog.trace("TCPServer socket error %d", (int) res);
        }
    }
    else {
        IsolatedEthernet::instance().appLog.error("TCPServer No available sockets");                    
    }
    return result;
}


bool IsolatedEthernet::TCPServer::begin() {
    stop();

    if (socket_handle_valid(_sock)) {
        return true;
    }

    if (IsolatedEthernet::instance().ready())
    {
        return startListener();
    }
    else {
        IsolatedEthernet::instance().appLog.trace("TCPServer Ethernet not ready");                    
        return false;
    }
}

void IsolatedEthernet::TCPServer::stop() {
    _client.stop();
    if (_sock >= 0) {
        int8_t res = wiznet::disconnect(_sock);
        if (res != SOCK_OK) {
            IsolatedEthernet::instance().appLog.trace("sock %d disconnect failed %d", _sock, (int) res);
        }
    }
    _sock = -1;
}



IsolatedEthernet::TCPClient IsolatedEthernet::TCPServer::available() {
    if (_sock < 0) {
        begin();
    }

    uint8_t status;
    wiznet::getsockopt(_sock, wiznet::SO_STATUS, &status);
    if (status != SOCK_ESTABLISHED) {
        _client = *s_invalid_client;
        return _client;
    }

    TCPServerClient client = TCPServerClient(_sock);
    client.d_->remoteIP = client.remoteIP(); // fetch the peer IP ready for the copy operator
    _client = client;

    // Start a new listener
    startListener();

    return _client;
}

size_t IsolatedEthernet::TCPServer::write(uint8_t b, system_tick_t timeout) {
    return write(&b, sizeof(b), timeout);
}

size_t IsolatedEthernet::TCPServer::write(const uint8_t *buf, size_t size, system_tick_t timeout) {
    _client.clearWriteError();
    size_t ret = _client.write(buf, size, timeout);
    setWriteError(_client.getWriteError());
    return ret;
}

size_t IsolatedEthernet::TCPServer::write(uint8_t b) {
    return write(&b, 1);
}

size_t IsolatedEthernet::TCPServer::write(const uint8_t *buffer, size_t size) {
    return write(buffer, size, SOCKET_WAIT_FOREVER);
}

//
// UDP
//

IsolatedEthernet::UDP::UDP()
        : _sock(-1),
          _offset(0),
          _total(0),
          _buffer(0),
          _buffer_size(512),
          _nif(NETWORK_INTERFACE_ALL),
          _buffer_allocated(false) {
}

bool IsolatedEthernet::UDP::setBuffer(size_t buf_size, uint8_t* buffer) {
    releaseBuffer();

    _buffer = buffer;
    _buffer_size = 0;
    if (!_buffer && buf_size) {         // requested allocation
        _buffer = new uint8_t[buf_size];
        _buffer_allocated = true;
    }
    if (_buffer) {
        _buffer_size = buf_size;
    }
    return _buffer_size;
}

void IsolatedEthernet::UDP::releaseBuffer() {
    if (_buffer_allocated && _buffer) {
        delete _buffer;
    }
    _buffer = NULL;
    _buffer_allocated = false;
    _buffer_size = 0;
    flush_buffer(); // clear buffer
}



uint8_t IsolatedEthernet::UDP::begin(uint16_t port, network_interface_t nif) {
    stop();

    bool result = false;
    int sock = IsolatedEthernet::instance().socketGetFree();
    if (sock >= 0) {
        IsolatedEthernet::instance().appLog.trace("UDP using socket=%d", (int)sock);

        int8_t res = wiznet::socket((uint8_t) sock, Sn_MR_UDP, port, 0x00);
        if (res >= 0) {
            _sock = sock;
            _port = port;
            // IsolatedEthernet::instance().appLog.trace("UDP socket() success");

            result = true;
        }
        else {
            IsolatedEthernet::instance().appLog.trace("UDP socket error %d", (int) res);
        }
    }
    else {
        IsolatedEthernet::instance().appLog.error("UDP No available sockets");                    
    }
    return result;
}

int IsolatedEthernet::UDP::available() {
    return _total - _offset;
}

void IsolatedEthernet::UDP::stop() {
    if (isOpen(_sock)) {
        int8_t res = wiznet::close(_sock);
        if (res != SOCK_OK) {
            IsolatedEthernet::instance().appLog.trace("sock %d disconnect failed %d", _sock, (int) res);
        }
    }

    _sock = -1;

    flush_buffer(); // clear buffer
}

int IsolatedEthernet::UDP::beginPacket(const char *host, uint16_t port) {
    if (IsolatedEthernet::instance().ready())
    {
        HAL_IPAddress ip_addr;

        if(IsolatedEthernet::instance().inet_gethostbyname((char*)host, strlen(host), &ip_addr, _nif, NULL) == 0)
        {
            IPAddress remote_addr(ip_addr);
            return beginPacket(remote_addr, port);
        }
    }
    return 0;
}

int IsolatedEthernet::UDP::beginPacket(IPAddress ip, uint16_t port) {
	LOG_DEBUG(TRACE, "begin packet %s#%d", ip.toString().c_str(), port);
    // default behavior previously was to use a 512 byte buffer, so instantiate that if not already done
    if (!_buffer && _buffer_size) {
        setBuffer(_buffer_size);
    }

    _remoteIP = ip;
    _remotePort = port;
    flush_buffer(); // clear buffer
    return _buffer_size;
}

int IsolatedEthernet::UDP::endPacket() {
    int result = sendPacket(_buffer, _offset, _remoteIP, _remotePort);
    flush(); // wait for send to complete
    return result;
}

int IsolatedEthernet::UDP::sendPacket(const uint8_t* buffer, size_t buffer_size, IPAddress remoteIP, uint16_t port) {
    LOG_DEBUG(TRACE, "sendPacket size %d, %s#%d", buffer_size, remoteIP.toString().c_str(), port);

    uint8_t addr[4];
    IsolatedEthernet::ipAddressToArray(remoteIP, addr);

    return wiznet::sendto(_sock, const_cast<uint8_t *>(buffer), buffer_size, addr, port);
}

size_t IsolatedEthernet::UDP::write(uint8_t byte) {
    return write(&byte, 1);
}

size_t IsolatedEthernet::UDP::write(const uint8_t *buffer, size_t size) {
    size_t available = _buffer ? _buffer_size - _offset : 0;
    if (size > available) {
        size = available;
    }
    memcpy(_buffer + _offset, buffer, size);
    _offset += size;
    return size;
}

int IsolatedEthernet::UDP::parsePacket(system_tick_t timeout) {
    if (!_buffer && _buffer_size) {
        setBuffer(_buffer_size);
    }

    flush_buffer();         // start a new read - discard the old data
    if (_buffer && _buffer_size) {
        int result = receivePacket(_buffer, _buffer_size, timeout);
        if (result > 0) {
            _total = result;
        }
    }
    return available();
}

int IsolatedEthernet::UDP::receivePacket(uint8_t* buffer, size_t size, system_tick_t timeout) {
    int ret = -1;
    if (isOpen(_sock) && buffer) {
        uint8_t addr[4];

        unsigned long start = millis();

        do {
            if (getSn_RX_RSR(_sock) > 0) {
                ret = wiznet::recvfrom(_sock, buffer, size, addr, &_remotePort);

                _remoteIP = IPAddress(addr);
                return ret;
                // LOG_DEBUG(TRACE, "received %d bytes from %s#%d", ret, _remoteIP.toString().c_str(), _remotePort);
            }    
            delay(1);
            ret = 0;
        } while((timeout != 0) && ((millis() - start) < timeout));
    }
    return ret;
}

int IsolatedEthernet::UDP::read() {
    return available() ? _buffer[_offset++] : -1;
}

int IsolatedEthernet::UDP::read(unsigned char* buffer, size_t len) {
    int read = -1;
    if (available()) {
        read = min(int(len), available());
        memcpy(buffer, &_buffer[_offset], read);
        _offset += read;
    }
    return read;
}

int IsolatedEthernet::UDP::peek() {
    return available() ? _buffer[_offset] : -1;
}

void IsolatedEthernet::UDP::flush() {
}

void IsolatedEthernet::UDP::flush_buffer() {
    _offset = 0;
    _total = 0;
}

size_t IsolatedEthernet::UDP::printTo(Print& p) const {
    // can't use available() since this is a `const` method, and available is part of the Stream interface, and is non-const.
    int size = _total - _offset;
    return p.write(_buffer + _offset, size);
}

int IsolatedEthernet::UDP::joinMulticast(const IPAddress& ip) {
    if (!isOpen(_sock)) {
        return -1;
    }
    stop();

    uint8_t addr[4];
    IsolatedEthernet::ipAddressToArray(ip, addr);

    bool result = false;
    int sock = IsolatedEthernet::instance().socketGetFree();
    if (sock >= 0) {
        IsolatedEthernet::instance().appLog.trace("UDP multicast using socket=%d", (int)sock);

        setSn_DIPR(sock, addr);
        setSn_DPORT(sock, _port);

        int8_t res = wiznet::socket((uint8_t) sock, Sn_MR_UDP, _port, Sn_MR_MULTI);
        if (res >= 0) {
            _sock = sock;
            // IsolatedEthernet::instance().appLog.trace("UDP multicast socket() success");

            result = true;
        }
        else {
            IsolatedEthernet::instance().appLog.trace("UDP multicast socket error %d", (int) res);
        }
    }
    else {
        IsolatedEthernet::instance().appLog.trace("UDP multicast no available sockets");                    
    }
    return result;
}

int IsolatedEthernet::UDP::leaveMulticast(const IPAddress& ip) {
    if (!isOpen(_sock)) {
        return -1;
    }
    stop();
    return begin(_port);
}

bool IsolatedEthernet::UDP::isOpen(sock_handle_t sn) {
    uint8_t status;
    wiznet::getsockopt((uint8_t)sn, wiznet::SO_STATUS, &status);
    return status == SOCK_UDP;
}

