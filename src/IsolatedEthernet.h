#ifndef __ISOLATEDETHERNET_H
#define __ISOLATEDETHERNET_H

#include "Particle.h"
#include <vector>

/**
 * Particle library for WIZnet W5500 for accessing devices on isolated Ethernet LANs
 * 
 * Features:
 * 
 * - Only implements TCPClient, TCPServer, UDP, and UDP multicast to Ethernet. Cloud connection uses native networking (cellular or Wi-Fi).
 * - Static IP addressing or DHCP.
 * - Can run from an isolated Ethernet network.
 * - Works with any avaiable SPI interface and any available GPIO for SPI CS. INT and RESET are optional.
 * - Uses separate TCPClient, TCPServer, and UDP classes over Ethernet so you can still use those classes with native networking at the same time.
 * - Works with any WIZnet W5500 board, including Particle M.2 evaluation board, Ethernet Featherwing, Mikroe ETH click, and Adafruit Ethernet Featherwing.
 * 
 * This class is a singleton; you do not create one as a global, on the stack, or with new.
 * 
 * From global application setup you must call:
 * 
 *   IsolatedEthernet::instance().setup();
 */
class IsolatedEthernet {
public:
    class TCPServer; // Forward declaration

    /**
     * @brief TCPClient class used to access the isolated Ethernet
     * 
     * Replace `TCPClient` with `IsolatedEthernet::TCPClient` to use Ethernet instead of the Particle device's native networking (cellular or Wi-Fi).
     */
    class TCPClient : public Client {
    public:
        /**
         * @brief Construct a new TCPClient object. You will do this for each TCP client connection.
         * 
         * There is a limit of 8 connections (TCP, UDP, DNS, DHCP) at a time on the W5500.
         */
        TCPClient();

        /**
         * @brief Construct a new TCPClient object. You will not generally use this overload, it's used internally.
         */
        TCPClient(sock_handle_t sock);

        /**
         * @brief Destroy the TCPClient object. This will close the connection if necessary and release its resources.
         */
        virtual ~TCPClient() {};

        /**
         * @brief Returns true if the network socket is open and the underlying network is ready. 
         * 
         * @return uint8_t 1 if the network socket is open or 0 if not, essentially a boolean.
         * 
         * This is different than connected() which returns true if the socket is closed but there is still unread buffered data, available() is non-zero.
         */
        uint8_t status();

        /**
         * @brief Connect to a host by IP address
         * 
         * @param ip The IP address to connect to 
         * @param port The IP port number to connect to
         * @return int true (1) if the connection was made or false (0) if not.
         * 
         * This call will block until the connection is made or times out.
         */
        virtual int connect(IPAddress ip, uint16_t port, network_interface_t=0);

        /**
         * @brief Connect to a host by its DNS hostname
         * 
         * @param host the hostname to connect to
         * @param port The IP port number to connect to
         * @return int true (1) if the connection was made or false (0) if not.
         * 
         * This call will block until the connection is made or times out.
         * 
         * Note that every connection will cause a DNS lookup when connecting by hostname; there is no DNS cache.
         */
        virtual int connect(const char *host, uint16_t port, network_interface_t=0);

        /**
         * @brief Writes a single byte to the remote host
         * 
         * @param b The byte to write (can be ASCII or binary).
         * 
         * @return size_t The number of bytes written, typically 1.
         * 
         * This overload does not take a timeout and uses the default timeout of 30 seconds.
         * 
         * It's best to use the overload that takes a buffer and a size. Each byte write is
         * communicated to the W5500 using SPI calls. The W5500 may further buffer the data
         * before assembling it in packets, but using the buffer-based approach is still
         * much faster.
         * 
         * There's a bug in Device OS where the write function can also return a negative error code
         * from write(), however size_t is unsigned, so it tends to return as a very large positive
         * integer instead. Because of this, it's best to ignore the result or test for != 1.
         */
        virtual size_t write(uint8_t b);

        /**
         * @brief Writes a buffer of data to the remote host
         * 
         * @param buffer Pointer to a buffer of bytes to send (can be binary or ASCII)
         * @param size Number of bytes to send. 
         * 
         * @return size_t The number of bytes written, typically 1.
         * 
         * Internally, the W5500 can't buffer more than 2048 bytes of data, however this library
         * will break up your send into chunks to fit in the available buffer space.
         * 
         * This overload does not take a timeout and uses the default timeout of 30 seconds.
         * The timeout is for the whole send, not individual chunks. If the timeout is 
         * exceeded there is no guarantee of how many bytes were actually sent.
         * 
         * There's a bug in Device OS where the write function can also return a negative error code
         * from write(), however size_t is unsigned, so it tends to return as a very large positive
         * integer instead. Because of this, it's best to ignore the result or test for != 1.
         */        
        virtual size_t write(const uint8_t *buffer, size_t size);


        /**
         * @brief Writes a single byte to the remote host
         * 
         * @param b The byte to write (can be ASCII or binary).
         * @param timeout Timeout in milliseconds, or 0 to wait forever
         * 
         * @return size_t The number of bytes written, typically 1.
         * 
         * It's best to use the overload that takes a buffer and a size. Each byte write is
         * communicated to the W5500 using SPI calls. The W5500 may further buffer the data
         * before assembling it in packets, but using the buffer-based approach is still
         * much faster.
         * 
         * There's a bug in Device OS where the write function can also return a negative error code
         * from write(), however size_t is unsigned, so it tends to return as a very large positive
         * integer instead. Because of this, it's best to ignore the result or test for != 1.
         */
        virtual size_t write(uint8_t b, system_tick_t timeout);

        /**
         * @brief Writes a buffer of data to the remote host
         * 
         * @param buffer Pointer to a buffer of bytes to send (can be binary or ASCII)
         * @param size Number of bytes to send. 
         * @param timeout Timeout in milliseconds, or 0 to wait forever
         * 
         * @return size_t The number of bytes written, typically 1.
         * 
         * Internally, the W5500 can't buffer more than 2048 bytes of data, however this library
         * will break up your send into chunks to fit in the available buffer space.
         * 
         * The timeout is for the whole send, not individual chunks. If the timeout is 
         * exceeded there is no guarantee of how many bytes were actually sent.
         * 
         * There's a bug in Device OS where the write function can also return a negative error code
         * from write(), however size_t is unsigned, so it tends to return as a very large positive
         * integer instead. Because of this, it's best to ignore the result or test for != 1.
         */                
        virtual size_t write(const uint8_t *buffer, size_t size, system_tick_t timeout);

        /**
         * @brief Returns the number of bytes available to read
         * 
         * @return int number of bytes
         * 
         * Note that the receive buffer is only 2048 bytes on the W5500. Thus the other side of
         * the connection may be holding more data that has not been received by the W5500
         * yet. 
         * 
         * If you are expecting a fixed-size structure, you should always read out all available
         * bytes on each loop into a separate buffer and process when it has been read completely.
         * Do not wait for available() to be the size of the structure then read.
         */
        virtual int available();

        /**
         * @brief Read a single byte from the receive buffer
         * 
         * @return int The returned byte (0-255), or -1 if there is no data to be read
         * 
         * It's much more efficient to use the overload that takes a buffer and size than
         * reading bytes one at a time.
         */
        virtual int read();

        /**
         * @brief Read a buffer of data from the receive buffer
         * 
         * @param buffer A pointer to a buffer to read data into
         * @param size Number of bytes to read, must be > 0.
         * 
         * @return int the number of bytes read, or a negative value on error
         * 
         * This is much more efficient to read data into a buffer using this call instead
         * of reading one byte at a time. The number of bytes read is not guaranteed to be
         * size requested; if any bytes are available they will be copied to buffer and the result
         * will be the number of bytes actually read.
         * 
         * The optimize request size is 2048 bytes, which is the size of the incoming data
         * buffer. Making it larger will have no effect since there will never be more bytes
         * in the buffer.
         */
        virtual int read(uint8_t *buffer, size_t size);

        /**
         * @brief Look at the next byte will be read, without removing it from the input buffer.
         * 
         * @return int The returned byte (0-255), or -1 if there is no data to be read
         */
        virtual int peek();

        /**
         * @brief Blocks until all data waiting to be sent in the W5500 send buffer has been sent
         */
        virtual void flush();

        /**
         * @brief Discards data waiting to be read from the internal buffer
         * 
         * There may still be data in the W5500 buffers.
         */
        void flush_buffer();

        /**
         * @brief End this connection, release its resources
         * 
         * The Wiring API for streams is not particularly well-suited for TCP as it doesn't have
         * a clear concept of half-closed connections.
         */
        virtual void stop();

        /**
         * @brief Returns true if there is data waiting to be read or if currently connected to the remote host
         * 
         * @return uint8_t If data available or connected
         * 
         * This is different than status() which only checks whether the connection is open, which seems
         * kind of backwards, but that's how the Wiring API works.
         */
        virtual uint8_t connected();

        /**
         * @brief Equivalent to status() != 0, but as the bool() operator, making it easy to test if there is a connection.
         * 
         * @return true 
         * @return false 
         * 
         * This is particularly useful with TCPServer available().
         */
        virtual operator bool();

        /**
         * @brief Return the IP address of the other side of the connection
         * 
         * @return IPAddress 
         */
        virtual IPAddress remoteIP();


        friend class IsolatedEthernet::TCPServer;

        using Print::write;
    protected:
        /**
         * @brief Return true of the socket is currently open. Used internally.
         * 
         * @return true 
         * @return false 
         */
        bool isOpen();

        /**
         * @brief Used internally to access the socket handle for this connection
         * 
         * @return sock_handle_t A W5500 socket handle (0 - 7) or -1 if there isn't an open socket.
         */
        inline sock_handle_t sock_handle() { return d_->sock; }

    private:
        struct Data {
            sock_handle_t sock;
            uint8_t buffer[TCPCLIENT_BUF_MAX_SIZE];
            uint16_t offset;
            uint16_t total;
            IPAddress remoteIP;

            explicit Data(sock_handle_t sock);
            ~Data();
        };

        std::shared_ptr<Data> d_;

        inline int bufferCount();
        bool isOpen(sock_handle_t sd);
    };

    /**
     * @brief Replacement for TCPServer class to use Ethernet
     * 
     * Replace `TCPServer` with `IsolatedEthernet::TCPServer` to use Ethernet instead of the Particle device's native networking (cellular or Wi-Fi).
     * 
     * Note that `IsolatedEthernet::TCPServer` can be used on cellular devices that normally do not support server mode (Boron, B Series SoM, etc.).
     */
    class TCPServer : public Print {
    public:
        /**
         * @brief Construct a new TCPServer object. This is safe as a globally constructed object.
         * 
         * @param port The port number to bind to, or 0 for any available port.
         * @param nif Ignored
         */
        TCPServer(uint16_t port, network_interface_t nif=0);

        /**
         * @brief Destroy the TCPServer object, free the underlying listening socket.
         * 
         * This method only closes the most recent client connection to the server. If you
         * have multiple connections open, only the last is closed. This makes no sense,
         * but that's how the Device OS TCPServer works.
         */
        ~TCPServer() { stop(); }

        /**
         * @brief If a connection has been made to this server, returns it
         * 
         * @return IsolatedEthernet::TCPClient The TCPClient that will be used to handle the new connection
         * 
         * The pattern typically used is:
         *   TCPClient client = server.available();
         *   if (client) {
         *       // Handle client stuff here
         *       client.stop();
         *   }
         * 
         * When a connection arrives, a new listener is created to handle another connection. There is no
         * setting for the maximum number of clients for a particular server, though you can manage that by
         * immediately closing clients using stop() when there are too many connections. You're still 
         * limited to the maximum of 8 sockets on the W5500.
         */
        IsolatedEthernet::TCPClient available();

        /**
         * @brief Starts a server listening for a connection
         * 
         * @return true 
         * @return false 
         * 
         * This should only be done after IsolatedEthernet().instance().ready() is true (the PHY link 
         * is up and the device has an IP address.)
         */
        virtual bool begin();

        /**
         * @brief Writes a single byte to most recently connected remote host. Do not use this method!
         * 
         * @param b The byte to write (can be ASCII or binary).
         * 
         * @return size_t The number of bytes written, typically 1.
         * 
         * Do not use this method. Instead store the TCPClient object that is returned from
         * server.available() and use that.
         * 
         * If you use this method, and a new connection comes in on the same port, you will
         * start communicating with that client and no longer be able to communicate with
         * the original one, which makes no sense, but this is how the Device OS TCPServer
         * Wiring API works.
         */
        virtual size_t write(uint8_t b);

        /**
         * @brief Writes a buffer of data to the remote host. Do not use this method!
         * 
         * @param buf Pointer to a buffer of bytes to send (can be binary or ASCII)
         * @param size Number of bytes to send. 
         * 
         * @return size_t The number of bytes written, typically 1.
         * 
         * Do not use this method. Instead store the TCPClient object that is returned from
         * server.available() and use that.
         * 
         * If you use this method, and a new connection comes in on the same port, you will
         * start communicating with that client and no longer be able to communicate with
         * the original one, which makes no sense, but this is how the Device OS TCPServer
         * Wiring API works.
         */                
        virtual size_t write(const uint8_t *buf, size_t size);

        /**
         * @brief Writes a single byte to the remote host. Do not use this method!
         * 
         * @param b The byte to write (can be ASCII or binary).
         * @param timeout Timeout in milliseconds, or 0 to wait forever
         * 
         * @return size_t The number of bytes written, typically 1.
         * 
         * Do not use this method. Instead store the TCPClient object that is returned from
         * server.available() and use that.
         * 
         * If you use this method, and a new connection comes in on the same port, you will
         * start communicating with that client and no longer be able to communicate with
         * the original one, which makes no sense, but this is how the Device OS TCPServer
         * Wiring API works.
         */
        virtual size_t write(uint8_t b, system_tick_t timeout);
        
        /**
         * @brief Writes a buffer of data to the remote host. Do not use this method!
         * 
         * @param buf Pointer to a buffer of bytes to send (can be binary or ASCII)
         * @param size Number of bytes to send. 
         * @param timeout Timeout in milliseconds, or 0 to wait forever
         * 
         * @return size_t The number of bytes written, typically 1.
         * 
         * Do not use this method. Instead store the TCPClient object that is returned from
         * server.available() and use that.
         * 
         * If you use this method, and a new connection comes in on the same port, you will
         * start communicating with that client and no longer be able to communicate with
         * the original one, which makes no sense, but this is how the Device OS TCPServer
         * Wiring API works.
         */
        virtual size_t write(const uint8_t *buf, size_t size, system_tick_t timeout);

        /**
         * @brief Stop accepting connections 
         * 
         * This method only closes the most recent client connection to the server. If you
         * have multiple connections open, only the last is closed. This makes no sense,
         * but that's how the Device OS TCPServer works.
         */
        void stop();

    private:
        /**
         * @brief Used internally to start a new listener
         * 
         * @return true 
         * @return false 
         * 
         * This method is necessary because the WIZnet implements server mode a little oddly.
         * Instead of binding the server socket once, and accepting onto another socket, the 
         * listener socket becomes the client socket. 
         */
        bool startListener();
        uint16_t _port;
        network_interface_t _nif;
        sock_handle_t _sock;
        IsolatedEthernet::TCPClient _client;

        using Print::write;
    };

    /**
     * @brief Replacement for UDP class to use Ethernet
     * 
     * Replace `UDP` with `IsolatedEthernet::UDP` to use Ethernet instead of the Particle device's native networking (cellular or Wi-Fi).
     */
    class UDP : public Stream, public Printable {
    private:

        /**
         * The underlying socket handle from the HAL.
         */
        sock_handle_t _sock;

        /**
         * The local port this UDP socket is bound to.
         */
        uint16_t _port;

        /**
         * The IP address of the peer that sent the received packet.
         * Available after parsePacket().
         */
        IPAddress _remoteIP;

        /**
         * The port of the peer that sent the received packet.
         * Available after parsePacket().
         */
        uint16_t _remotePort;

        /**
         * The current read/write offset in the buffer. Set to 0 after
         * parsePacket(), incremented during write()
         */
        uint16_t _offset;

        /**
         * The number of bytes in the buffer. Available after parsePacket()
         */
        uint16_t _total;

        /**
         * The dynamically allocated buffer to store the packet that has been read or
         * the packet that is being written.
         */
        uint8_t* _buffer;

        /**
         * The size of the buffer.
         */
        size_t _buffer_size;

        /**
         * The network interface this UDP socket should bind to.
         */
        network_interface_t _nif;

        /**
         * Set to non-zero if the buffer was dynamically allocated by this class.
         */
        bool _buffer_allocated;



    public:
        /**
         * @brief Construct a new UDP object. This is safe as a globally constructed object.
         * 
         */
        UDP();

        /**
         * @brief Destroy the UDP object. This releases the socket (it allocated) and any buffers.
         */
        virtual ~UDP() { stop(); releaseBuffer(); }

        /**
         * @brief Allocates a buffer for parsePacket(). Default is 512 if not specified.
         * 
         * @param buffer_size The size of the read/write buffer. Can be 0 if
         * only readPacket() and sendPacket() are used, as these methods
         * use client-provided buffers.
         *
         * @param buffer    A pre-allocated buffer. This is optional, and if not specified
         *  the UDP class will allocate the buffer on the heap.
         * 
         * @return true if successful or false if the memory allocation failed.
         */
        bool setBuffer(size_t buffer_size, uint8_t* buffer=NULL);

        /**
         * @brief Releases the current buffer, discarding any previously allocated memory.
         * 
         * After this call only sendPacket() and receivePacket() may be used,
         * until a new buffer is set via setBuffer().
         */
        void releaseBuffer();

        /**
         * @brief Initializes a UDP socket
         * 
         * @param port  The local port to connect to.
         * @param nif   The network interface to connect to (not used, pass 0)
         * @return non-zero on success
         */
        virtual uint8_t begin(uint16_t port, network_interface_t nif=0);

        /**
         * @brief Disconnects this UDP socket.
         */
        virtual void stop();

        /**
         * @brief Sends an packet directly. You should use this method instead of beginPacket(), write(), and endPacket().
         *
         * @param buffer Pointer to a buffer to send (const uint8_t *), can be binary data.
         * @param buffer_size Size of buffer in bytes, must be > 0.
         * @param destination The IP address to send to.
         * @param port The port to send to.
         * @return The number of bytes sent in the packet, or a negative error code.
         * 
         * This does not require the UDP instance to have an allocated buffer.
         */
        virtual int sendPacket(const uint8_t* buffer, size_t buffer_size, IPAddress destination, uint16_t port);


        /**
         * @brief Sends an packet directly. You should use this method instead of beginPacket(), write(), and endPacket().
         *
         * @param buffer Pointer to a buffer to send (const char *), can be binary data.
         * @param buffer_size Size of buffer in bytes, must be > 0.
         * @param destination The IP address to send to.
         * @param port The port to send to.
         * @return The number of bytes sent in the packet, or a negative error code.
         * 
         * Even though the parameter is a const char *, buffer is not treated as a c-string; pass stren(buffer)
         * as the buffer_size if desired.
         * 
         * This does not require the UDP instance to have an allocated buffer. 
         */
        virtual int sendPacket(const char* buffer, size_t buffer_size, IPAddress destination, uint16_t port) {
            return sendPacket((uint8_t*)buffer, buffer_size, destination, port);
        }

        /**
         * @brief Retrieves a packet directly. You should use this method instead of parsePacket().
         * 
         * @param buffer        The buffer to read data to (uint8_t *). It may contain binary data.
         * @param buf_size      The buffer size
         * @param timeout In milliseconds to wait for a packet, or 0 = do not wait and return 0 if there is no packet available.
         * @return The number of bytes written to the buffer, a negative value on error, or 0 if no packet is available and timeout == 0.
         * 
         * This does not require the UDP instance to have an allocated buffer. 
         * 
         * If the buffer is not large enough for the packet, the remainder that doesn't fit is discarded.
         * 
         * This method is preferable to parsePacket() for several reasons:
         * - parsePacket() uses the return value of -1 for both packet not currently available, and listener is invalid
         *   and you need to call begin() again.
         * - Using many calls to read() is often less efficient.
         * - parsePacket() and beginPacket() share a single buffer.
         * - parsePacket() is not multi-thread safe.
         */
        virtual int receivePacket(uint8_t* buffer, size_t buf_size, system_tick_t timeout = 0);


        /**
         * @brief Retrieves a packet directly. You should use this method instead of parsePacket().
         * 
         * @param buffer        The buffer to read data to (char *). It may contain binary data.
         * @param buf_size      The buffer size
         * @param timeout In milliseconds to wait for a packet, or 0 = do not wait and return 0 if there is no packet available.
         * @return The number of bytes written to the buffer, a negative value on error, or 0 if no packet is available and timeout == 0.
         * 
         * Even though the buffer is a char *, there is no guarantee the packet will contain a c-string. It could contain embedded
         * nulls, and may not be null-terminated.
         * 
         * This does not require the UDP instance to have an allocated buffer. 
         * 
         * If the buffer is not large enough for the packet, the remainder that doesn't fit is discarded.
         */
        virtual int receivePacket(char* buffer, size_t buf_size, system_tick_t timeout = 0) {
            return receivePacket((uint8_t*)buffer, buf_size, timeout);
        }

        /**
         * @brief Begin writing a packet to the given destination. It is often better to use sendPacket() instead.
         * 
         * @param ip        The IP address of the destination peer.
         * @param port      The destination port of the peer
         * @return non-zero on success.
         * 
         * The beginPacket() and parsePacket() functions share a buffer so you should never use both
         * at the same time.
         */
        virtual int beginPacket(IPAddress ip, uint16_t port);

        /**
         * @brief Begin writing a packet to the given destination. It is often better to use sendPacket() instead.
         * 
         * @param host      The DNS hostname to send to.
         * @param port      The destination port of the peer
         * @return non-zero on success.
         * 
         * DNS will be done for every packet when using a hostname, the result is not cached. If
         * you will be sending numerous packets you should resolve the hostname once and save the
         * IPAddress instead.
         * 
         * The beginPacket() and parsePacket() functions share a buffer so you should never use both
         * at the same time.
         */

        virtual int beginPacket(const char *host, uint16_t port);

        /**
         * @brief Writes to the currently open packet after a call to beginPacket().
         * @return 1 if the data was written, 0 otherwise.
         * Note that the data is buffered and not sent over the network.
         */
        virtual size_t write(uint8_t);

        /**
         * @brief Writes to the currently open packet after a call to beginPacket().
         * 
         * @return a positive number if the data was written, 0 otherwise.
         * Note that the data is buffered and not sent over the network.
         */
        virtual size_t write(const uint8_t *buffer, size_t size);

        /**
         * @brief Sends the current buffered packet over the network and clears the buffer.
         * @return
         */
        virtual int endPacket();

        /**
         * @brief Reads a UDP packet into the packet buffer. You should use receivePacket instead.
         * 
         * @param timeout Timeout in milliseconds, or 0 to not wait 
         * 
         * @return int number of bytes in the packet or -1 for error or no packet available.
         * 
         * Aside from parsePacket() / read() being much less efficient, another problem is that parsePacket()
         * does not differentiate between no data available and an error getting a packet. When listening
         * for packets, if the underlying network goes away, all listeners are closed and must be reopened
         * using begin(). The parsePacket() method does not differentiate between the case where no packet
         * has arrived yet and packets will never arrive again (because you need to call begin() again).
         * This is very annoying and does not occur with receivePacket() which returns all three states 
         * separately.
         * 
         * The beginPacket() and parsePacket() functions share a buffer so you should never use both
         * at the same time.
         */
        virtual int parsePacket(system_tick_t timeout = 0);

        /**
         * @brief Get the number of bytes that can be read after parsePacket
         * 
         * @return number of bytes left to read in the packet
         * 
         * The number of bytes will decrease for each read() call made.
         */
        virtual int available();

        /**
         * @brief Read a single byte from the read buffer. Available after parsePacket().
         * @return The byte read (0-255) or -1 if there are no bytes available.
         * 
         * You should use receivePacket() instead of parsePacket() / read().
         */
        virtual int read();

        /**
         * @brief Reads multiple bytes. Available after parsePacket().
         * @return The number of bytes read. Could be < len.
         * 
         * This is not the same as receivePacket() even though it has the same prototype.
         * This method reads the bytes left in the packet buffer filled in by parsePacket().
         * The various read() methods consume the data in the buffer until empty.
         * 
         * You should use receivePacket() instead of parsePacket() / read().
         */
        virtual int read(unsigned char* buffer, size_t len);

        /**
         * @brief Reads multiple bytes. Available after parsePacket().
         * @return The number of bytes read. Could be < len.
         * 
         * This is not the same as receivePacket() even though it has the same prototype.
         * This method reads the bytes left in the packet buffer filled in by parsePacket().
         * The various read() methods consume the data in the buffer until empty.
         *
         * Even though this method takes a char * buffer, it does not create a c-string.
         * The data could be binary, may contain embedded nulls, and is not null-terminated.
         *  
         * You should use receivePacket() instead of parsePacket() / read().
         */
        virtual int read(char* buffer, size_t len) { return read((unsigned char*)buffer, len); };

        /**
         * @brief Returns the next character that read() will return without consuming it
         * 
         * @return The byte (0-255) or -1 if there are no bytes available.
         */
        virtual int peek();

        /**
         * @brief Blocks until all data has been sent out
         */
        virtual void flush();

        /**
         * @brief Discards the currently read packet from parsePacket().
         */
        void flush_buffer();

        /**
         * @brief Returns the buffer used by parsePacket() and beginPacket()
         * 
         * @return const uint8_t* Pointer to the buffer
         */
        const uint8_t* buffer() const { return _buffer; }

        /**
         * @brief Return the IP address of the other side of the connection
         * 
         * @return IPAddress IP address
         */
        virtual IPAddress remoteIP() { return _remoteIP; };

        /**
         * @brief Return the port number of the other side of the connection
         * 
         * @return uint16_t Port number 
         */
        virtual uint16_t remotePort() { return _remotePort; };

        /**
         * Prints the current read parsed packet to the given output.
         * @param p
         * @return
         */
        virtual size_t printTo(Print& p) const;

        /**
         * @brief Join a multicast network
         * 
         * @param ip The multicast IP address. This must be a multicast IP address, not a LAN address.
         * 
         * @return true if successful, false if not.
         * 
         * You must call begin() before joinMulticast() because the port is specified as a parameter
         * to begin().
         * 
         * You can only bind to a single multicast address per UDP object.
         */
        int joinMulticast(const IPAddress& ip);

        /**
         * @brief Leave a multicast network
         * 
         * @param ip The multicast IP address to leave. This parameter is not used 
         * @return true if successful, false if not.
         * 
         * When you leave a multicast network it turns the socket back into a normal unicast
         * socket, as when you call begin(). If you want to free all resources, you 
         * must also call stop().
         */
        int leaveMulticast(const IPAddress& ip);

        /**
         * @brief Returns true if the socket is currently open. Used internally.
         * 
         * @param sock 
         * @return true 
         * @return false 
         * 
         * If you want to see if a connection is open, use connected() or status().
         */
        bool isOpen(sock_handle_t sock);

        /**
         * @brief Returns the internal socket handle for this connection
         * 
         * @return sock_handle_t 
         */
        sock_handle_t socket() {
            return _sock;
        }

        using Print::write;
    };

public:
    /**
     * @brief Gets the singleton instance of this class, allocating it if necessary
     * 
     * Use IsolatedEthernet::instance() to instantiate the singleton.
     */
    static IsolatedEthernet &instance();

    /**
     * @brief You must call this from global setup(). Set options first using the withXXX() methods.
     */
    void setup();

    /**
     * @brief State machine. This is called from the worker thread loop.
     * 
     * While the library does not currently support it, you could disable the thread and
     * run the state machine from loop.
     */
    void stateMachine();


#if (defined(D3) && defined(D4) && defined(D5)) || defined(DOXYGEN)
    /**
     * @brief Configure for Particle Ethernet FeatherWing
     * 
     * @return IsolatedEthernet& Reference to this object so you can chain options, fluent-style.
     * 
     * For custom boards, you can set settings individually using withSPI, withPinCS, withPinINT, withPinRESET.
     */
    IsolatedEthernet &withEthernetFeatherWing() 
    {
        return withSPI(&SPI).withPinCS(D5).withPinINT(D4).withPinRESET(D3);
    }
#endif

#if (defined(D8) && defined(D22) && defined(A7)) || defined(DOXYGEN)
    /**
     * @brief Configure for Particle M.2 evaluation board
     * 
     * @return IsolatedEthernet& Reference to this object so you can chain options, fluent-style.
     * 
     */
    IsolatedEthernet &withEthernetM2EvalBoard()
    {
        return withSPI(&SPI).withPinCS(D8).withPinINT(D22).withPinRESET(A7);
    }
#endif

#if (defined(D8) && defined(D22) && defined(D7) && defined(D4) && defined(D23) && defined(A0)) || defined(DOXYGEN)
    /**
     * @brief Configure for Mikroe Gen 3 SoM Shield with ETH wiz click
     * 
     * @param bus The bus. Must be 1 (left) or 2 (right).
     * 
     * @return IsolatedEthernet& Reference to this object so you can chain options, fluent-style.
     * 
     * https://www.mikroe.com/click-shield-for-particle-gen-3
     * https://www.mikroe.com/eth-wiz-click
     * 
     * https://docs.particle.io/hardware/expansion/mikroe/
     * 
     */
    IsolatedEthernet &withEthernetMikroeGen3SomShield(int bus) {
        if (bus == 1) {
            return withSPI(&SPI).withPinCS(D8).withPinINT(D22).withPinRESET(D7);
        }
        else {
            return withSPI(&SPI).withPinCS(D4).withPinINT(D23).withPinRESET(A0);
        }
    }
#endif


#if (defined(D8) && defined(A2) && defined(A4) && defined(A5) && defined(D5) && defined(D6) &&defined(D7)) || defined(DOXYGEN)
    /**
     * @brief Configure for Mikroe Feather Shield with ETH wiz click
     * 
     * @param bus The bus. Must be 1 (left) or 2 (right).
     * 
     * @return IsolatedEthernet& Reference to this object so you can chain options, fluent-style.
     * 
     * https://www.mikroe.com/feather-click-shield
     * https://www.mikroe.com/eth-wiz-click
     * 
     * https://docs.particle.io/hardware/expansion/mikroe/
     */
    IsolatedEthernet &withEthernetMikroeFeatherShield(int bus) {
        if (bus == 1) {
            return withSPI(&SPI).withPinCS(A5).withPinINT(A4).withPinRESET(A2);
        }
        else {
            return withSPI(&SPI).withPinCS(D5).withPinINT(D6).withPinRESET(D7);
        }
    }
#endif
    /**
     * @brief Sets the SPI interface to use. Default is SPI.
     * 
     * @param spi SPI interface, default is SPI, but you can also use SPI1.
     * 
     * @return IsolatedEthernet& Reference to this object so you can chain options, fluent-style.
     */
    IsolatedEthernet &withSPI(SPIClass *spi) { this->spi = spi; return *this; };


    /**
     * @brief Sets the CS pin. Default is D5. 
     * 
     * @param pinCS Any available GPIO to use the CS pin. Default is D5 for Feather and D8 for B Series eval board.
     * 
     * @return IsolatedEthernet& Reference to this object so you can chain options, fluent-style.
     * 
     * Must be called before setup()! Changing it later will not work properly.
     * 
     * In the unusual case of using fixed-length data mode (FDM) with the W5500 CS pin grounded, pass PIN_INVALID
     * to this method. Note: FDM is not currently supported, so you must use a CS pin.
     */
    IsolatedEthernet &withPinCS(pin_t pinCS) { this->pinCS = pinCS; return *this; };

    /**
     * @brief Sets the INT pin. Default is PIN_INVALID (not used). 
     * 
     * @param pinINT Any available GPIO to use the INT pin. Default is D4 for Feather and D22 for B Series eval board.
     * 
     * @return IsolatedEthernet& Reference to this object so you can chain options, fluent-style.
     * 
     * This setting is not actually used. This library currently always runs in polling mode
     * and does not use the hardware interrupt support, but it could be used in the future.
     * 
     * Must be called before setup()! Changing it later will not work properly.
     */
    IsolatedEthernet &withPinINT(pin_t pinINT) { this->pinINT = pinINT; return *this; };

    /**
     * @brief Sets the INT pin. Default is PIN_INVALID (not used). 
     * 
     * @param pinRESET Any available GPIO to use the INT pin. Default is D3 for Feather and A7 for B Series eval board.
     * 
     * @return IsolatedEthernet& Reference to this object so you can chain options, fluent-style.
     * 
     * This setting is optional. If not used, then software reset is used instead.
     * 
     * Must be called before setup()! Changing it later will not work properly.
     */
    IsolatedEthernet &withPinRESET(pin_t pinRESET) { this->pinRESET = pinRESET; return *this; };

    /**
     * @brief Sets custom settings for the SPI transactions with the WIZnet W5500. Not normally needed.
     * 
     * @param spiSettings A SPISettings object (speed, bit order, mode)
     * 
     * @return IsolatedEthernet& Reference to this object so you can chain options, fluent-style.
     * 
     * The W5500 only supports mode 0 and 3, MSB first.
     */
    IsolatedEthernet &withSpiSettings(const SPISettings &spiSettings) { this->spiSettings = spiSettings; return *this; };

    /**
     * @brief Sets the IP address when using static IP addressing (instead of DHCP)
     * 
     * @param ip The IP Address to use. This is often an internal address like 192.168.1.100 or 10.1.2.50. 
     * @return IsolatedEthernet& Reference to this object so you can chain options, fluent-style.
     * 
     * This method should be called before setup(). If you want to change the IP address later at runtime,
     * set the IP address, subnet mask, gateway address, then call updateAddressSettings.
     * 
     * This internally calls withStaticIP() and will stop any DHCP that is currently in progress.
     * 
     * If you use withIPAddress you must also call withSubnetMask, it does not configure a default automatically
     * based on your address class!
     */
    IsolatedEthernet &withIPAddress(const IPAddress &ip) { withStaticIP(); ipAddressToArray(ip, ipAddr); return *this; };

    /**
     * @brief Sets the subnet mask when using static IP addressing (instead of DHCP). This is required when using static IP addressing!
     * 
     * @param ip The subnet mask to use, often something like 255.255.255.0 
     * @return IsolatedEthernet& Reference to this object so you can chain options, fluent-style.
     * 
     * This method should be called before setup(). If you want to change the subnet mask later at runtime,
     * set the IP address, subnet mask, gateway address, then call updateAddressSettings.
     * 
     * This internally calls withStaticIP() and will stop any DHCP that is currently in progress.
     */
    IsolatedEthernet &withSubnetMask(const IPAddress &ip) { withStaticIP(); ipAddressToArray(ip, subnetMaskArray); return *this; };

    /**
     * @brief Sets the gateway address static IP addressing (instead of DHCP).
     * 
     * @param ip The gateway address to use, often something like 192.168.1.1. This must be in the same subnet as the IP address.
     * @return IsolatedEthernet& Reference to this object so you can chain options, fluent-style.
     * 
     * This method should be called before setup(). If you want to change the gateway address later at runtime,
     * set the IP address, subnet mask, gateway address, then call updateAddressSettings.
     * 
     * This internally calls withStaticIP() and will stop any DHCP that is currently in progress.
     */
    IsolatedEthernet &withGatewayAddress(const IPAddress &ip) { withStaticIP(); ipAddressToArray(ip, gatewayAddr); return *this; };

    /**
     * @brief Sets the DNS server address static IP addressing (instead of DHCP).
     * 
     * @param ip The DNS server to use. Often the gateway address, it could also be a site DNS or public DNS like 8.8.8.8.
     * @return IsolatedEthernet& Reference to this object so you can chain options, fluent-style.
     * 
     * This method should be called before setup(). If you want to change the DNS address later at runtime,
     * set the IP address, subnet mask, gateway address, then call updateAddressSettings.
     * 
     * This internally calls withStaticIP() and will stop any DHCP that is currently in progress.
     * 
     * Only one DNS address is supported.
     */
    IsolatedEthernet &withDNSAddress(const IPAddress &ip) { withStaticIP(); ipAddressToArray(ip, dnsAddr); return *this; };

    /**
     * @brief Enables static IP mode
     * 
     * @return IsolatedEthernet& Reference to this object so you can chain options, fluent-style.
     * 
     * You normally don't need to call this if you are using withIPAddress(), etc.. However, in the case where
     * you do not want to attempt DHCP but also do not know your IP address yet, you can call this to disable
     * DHCP.
     */
    IsolatedEthernet &withStaticIP();

    /**
     * @brief Enables DHCP mode
     * 
     * @return IsolatedEthernet& Reference to this object so you can chain options, fluent-style.
     * 
     * Since the default is DHCP mode you normally don't need to call this, but you can use this to switch
     * from static to DHCP again.
     */
    IsolatedEthernet &withDHCP();


    /**
     * @brief Specifies that the static IP settings will be stored in a file on the flash file system.
     * 
     * @param path Path to the file in the POSIX file system. If in a subdirectory, the subdirectory must exist.
     * @return IsolatedEthernet& Reference to this object so you can chain options, fluent-style.
     * 
     * You must call this before setup(). It's OK to call this before the settings file exists.
     * 
     */
    IsolatedEthernet &withJsonConfigFile(const char *path) { this->jsonConfigFile = path; return *this; };

    /**
     * @brief Loads the configuration file. You normally do not need to do this; it's done automatically if necessary.
     * 
     * The configuration file name is specified using withJsonConfigFile(). 
     */
    void loadConfigFile();

    /**
     * @brief Save the configuration file
     * 
     * @return true 
     * @return false 
     * 
     * Saves the current settings for ipAddr, subnetMaskArray, gatewayAddr, dnsAddr to the settings file specified
     * by withJsonConfigFile. You use this after updating the settings so they will be used the next restart.
     */
    bool saveConfigFile();

    /**
     * @brief Loads a JSON configuration from a string 
     * 
     * @param str A string containing a JSON settings object.
     * 
     * You typically do this if you receive a new configuration, such as from a Particle function.
     */
    void loadJsonConfig(const char *str);

    /**
     * @brief Loads a JSON configuration option that has been parsed by the Device OS JSON parser.
     * 
     * @param configObj 
     * 
     * See also the overload that takes a c-string, which really just calls this function after 
     * parsing the data.
     */
    void loadJsonConfig(const JSONValue &configObj);

    /**
     * @brief Does a hardware reset of the W5500
     * 
     * @return true 
     * @return false 
     * 
     * This is only done if pinRESET is defined for the hardware reset pin. If there is no hardware reset
     * pin connected, a software reset is done over SPI.
     */
    bool hwReset();

    /**
     * @brief Get the local IP address as an IPAddress object. For compatibility with the WiFi class.
     * 
     * @return IPAddress 
     */
    IPAddress localIP() {
        return IPAddress(ipAddr);
    }

    /**
     * @brief Get the subnet mask as an IPAddress object. For compatibility with the WiFi class.
     * 
     * @return IPAddress 
     */
    IPAddress subnetMask() {
        return IPAddress(subnetMaskArray);
    }

    /**
     * @brief Get the gateway IP address as an IPAddress object. For compatibility with the WiFi class.
     * 
     * @return IPAddress 
     */
    IPAddress gatewayIP() {
        return IPAddress(gatewayAddr);
    }

    /**
     * @brief Get the DNS server IP address as an IPAddress object. For compatibility with the WiFi class.
     * 
     * @return IPAddress 
     */
    IPAddress dnsServerIP() {
        return IPAddress(dnsAddr);
    }

    /*
    // This is not currently exposed by the DHCP module, but it could be added
    IPAddress dhcpServerIP() {
    	return IPAddress(wifi_config()->nw.aucDHCPServer);
    }
    */

    /**
     * @brief Perform a DNS lookup
     * 
     * @param name Host name to look up
     * @return IPAddress 
     * 
     * This API is compatible with the WiFi class method.
     * 
     * IPAddress supports operator bool() so you can test that IPAddress is true to see if an address was returned. It will be 0 (false) if the hostname could
     * not be looked up because DNS wasn't configured, not available, or the host name does not exist.
     */
    IPAddress resolve(const char* name)
    {
        HAL_IPAddress ip = {};
        return (inet_gethostbyname(name, strlen(name), &ip, 0, NULL) != 0) ?
                IPAddress(uint32_t(0)) : IPAddress(ip);
    }

    /**
     * @brief Perform a DNS lookup.
     * 
     * @param hostname The hostname to look up
     * @param hostnameLen The length of hostname
     * @param out_ip_addr Filled in with the 
     * @param nif Ignored. Pass 0.
     * @param reserved Ignored, pass NULL or 0.
     * @return int 0 on success, or a non-zero error code
     * 
     * This API is provided to match the Device OS HAL API of the same name. The resolve() method is easier to use.
     */
    int inet_gethostbyname(const char* hostname, uint16_t hostnameLen, HAL_IPAddress* out_ip_addr, network_interface_t nif, void* reserved);

    /**
     * @brief Call this after updating the ipAddr, subnetMaskArray, gatewayAddr, or dnsAddr
     * 
     * This takes the fields in this class and updates the registers in the W5500.
     * 
     * If you set the fields before calling setup() you do not need to call this method as it will be handled
     * automatically. Same for using the configuration file or JSON configuration options. This is called 
     * internally after parsing JSON configuration.
     */
    void updateAddressSettings();

    /**
     * @brief Returns true if there is a PHY link and an IP address set
     * 
     * @return true 
     * @return false 
     * 
     * Does not validate that the ipAddress or gateway address is valid or whether there's any connectivity above the PHY layer.
     * 
     * It's normally used in place of WiFi.ready(), Ethernet.ready(), etc..
     */
    bool ready() const { return isReady; };

    /**
     * @brief Callback messages if you register a callback using withCallback()
     */
    enum class CallbackType {
        linkUp,         //!< PHY link is up
        linkDown,       //!< PHY link is down
        gotIpAddress    //!< An IP address has been assigned
    };

    /**
     * @brief Add a callback so you can code can be notified when things occur
     * 
     * @param cb The callback function or lambds
     * @return IsolatedEthernet& Reference to this object so you can chain options, fluent-style.
     * 
     * The prototype for the callback is:
     * 
     *   void callback(CallbackType type, void* data)
     */
    IsolatedEthernet &withCallback(std::function<void(CallbackType,void*)> cb) { callbacks.push_back(cb); return *this; };



    /**
     * @brief Logger instance used by IsolatedEthernet
     * 
     * All logging messages use the category app.ether so you can control the level in 
     * your log handler instances. 
     * 
     * Within this library, always use appLog.info() instead of Log.info(), for example.
     */
    Logger appLog;

    /**
     * @brief Utility function to convert a Device OS IPAddress class to an array of 4 uint8_t for an IPv4 address
     * 
     * @param addr The address to read from
     * @param array Pointer to an array of 4 uint8_t to fill in
     * 
     * To convert an array to IPAddress, there's a constructor to IPAddress that takes a const uint8_t *.
     */
    static void ipAddressToArray(const IPAddress &addr, uint8_t *array);

    /**
     * @brief Utility function to convert an array of 4 uint8_t to a dotted octet String
     * 
     * @param array Pointer to an array of 4 uint8_t
     * @return String A string containing dotted octets ("192.168.1.1")
     */
    static String arrayToString(const uint8_t *array);

private:
    /**
     * @brief The constructor is protected because the class is a singleton
     * 
     * Use IsolatedEthernet::instance() to instantiate the singleton.
     */
    IsolatedEthernet();

    /**
     * @brief The destructor is protected because the class is a singleton and cannot be deleted
     */
    virtual ~IsolatedEthernet();

    /**
     * This class is a singleton and cannot be copied
     */
    IsolatedEthernet(const IsolatedEthernet&) = delete;

    /**
     * This class is a singleton and cannot be copied
     */
    IsolatedEthernet& operator=(const IsolatedEthernet&) = delete;

    /**
     * @brief Copies the settings from the ioLibrary DHCP module. Used internally.
     * 
     * The data is copied into fields in this class (ipAddr, subnetMaskArray, gatewayAddr, dnsAddr) add
     * then those settings are copied into the W5500 registers.
     */
    void updateAddressSettingsFromDHCP();

    /**
     * @brief Calls all registered callback functions with the specified message and data
     * 
     * @param type A CallbackType enumeration (linkUp, linkDown, gotIPAddress, etc.)
     * @param data Optional data, depending on the type.
     */
    void callCallbacks(CallbackType type, void *data = nullptr);

    /**
     * @brief Called from setup to set the Ethernet MAC address field macAddr. Used internally.
     */
    void setMacAddress();

	/**
	 * @brief Begins an SPI transaction
     * 
     * Calls spi->beginTransaction() then sets the CS pin low.
	 */
	void beginTransaction();

	/**
	 * @brief Ends an SPI transaction
     * 
     * Sets the CS pin high then calls spi->endTransaction.
	 */
	void endTransaction();

    /**
     * @brief Begins an SPI transaction. Hooks into WIZnet ioDriver library.
     */
    void wizchip_cris_enter(void);

    /**
     * @brief Ends an SPI transaction. Hooks into WIZnet ioDriver library.
     */
    void wizchip_cris_exit(void);

    /**
     * @brief Asserts (low) the CS pin. Hooks into WIZnet ioDriver library.
     * 
     * Only done within a wizchip_cris_enter/wizchip_cris_exit block.
     * 
     * Theoretically the W5500 supports operation using fixed-length data mode (FDM) and tying
     * CS low, but that is not supported by this library as it means the SPI bus cannot be 
     * shared with other devices. Also, FDM is very inefficient and a pain to use.
     */
    void wizchip_cs_select(void);

    /**
     * @brief Deasserts (high) the CS pin. Hooks into WIZnet ioDriver library.
     * 
     * Only done within a wizchip_cris_enter/wizchip_cris_exit block.
     */
    void wizchip_cs_deselect(void);

    /**
     * @brief Read a byte via SPI. Hooks into WIZnet ioDriver library.
     * 
     * If the wizchip_spi_readburst callback is defined, then this is not used.
     * 
     * This technically also writes out a 0xff byte at the same time, because of how SPI
     * works.
     */
    uint8_t wizchip_spi_readbyte(void);

    /**
     * @brief Writes a byte via SPI. Hooks into WIZnet ioDriver library.
     * 
     * If the wizchip_spi_writeburst callback is defined, then this is not used.
     */
    void wizchip_spi_writebyte(uint8_t wb);

    /**
     * @brief Reads data using DMA. This is much more efficient than single byte. Hooks into WIZnet ioDriver library.
     * 
     * @param pBuf 
     * @param len 
     * 
     * Even though it uses DMA, it still blocks until the transmission is complete.
     */
    void wizchip_spi_readburst(uint8_t *pBuf, uint16_t len);

    /**
     * @brief Writes data using DMA. This is much more efficient than single byte. Hooks into WIZnet ioDriver library.
     * 
     * @param pBuf 
     * @param len 
     * 
     * Even though it uses DMA, it still blocks until the transmission is complete.
     */
    void wizchip_spi_writeburst(uint8_t *pBuf, uint16_t len);

    /**
     * @brief Thread function. This runs continuously after setup. 
     * 
     * @return os_thread_return_t 
     */
    os_thread_return_t threadFunction();

    /**
     * @brief Thread function. This is called from the Device OS thread API, as it does not take a std::function.
     * 
     * @param param 
     * @return os_thread_return_t
     * 
     * It just calls threadFunction in the IsolatedEthernet class. 
     */
    static os_thread_return_t threadFunctionStatic(void* param);


    /**
     * @brief SPI interface to use, default is SPI, could be SPI1 (or SPI2).
     */
	SPIClass *spi = &SPI;

    /**
     * @brief Chip select pin. Default is D5
     */
	pin_t pinCS = D5; 

    /**
     * @brief Interrupt pin. Default is PIN_INVALID (not used). 
     */
	pin_t pinINT = PIN_INVALID;

    /**
     * @brief Reset pin. Default is PIN_INVALID (not used).
     * 
     */
    pin_t pinRESET = PIN_INVALID;


    /**
     * @brief Default SPI settings
     * 
     * - Speeds up to 80 MHz are theoretically possible on the W5500, but there may be distorted signals because of
     * crosstalk. Maximum guaranteed speed is 33.3 MHz
     * - MSBFIRST is required by the W5500
     * - Mode 0 and Mode 3 are supported by the W5500.
     * 
     * Gen 3 devices (Argon, Boron, B Series SoM, and Tracker SoM) support SPI speeds up to 32 MHz on SPI and 8 MHz on SPI1.
     * P2 & Photon 2: SPI uses the RTL872x SPI1 peripheral (25 MHz maximum speed), SPI1 uses the RTL872x SPI0 peripheral (50 MHz maximum speed).
     */
    SPISettings spiSettings = SPISettings(32*MHZ, MSBFIRST, SPI_MODE0);

    /**
     * @brief Ethernet MAC address setting
     * 
     * The WIZnet W5500 does not have a MAC address set from the factory. We set one in the private
     * MAC address space based on a unique value from the MCU. It's the same algorithm the Particle
     * Ethernet Featherwing uses. It's set in setMacAddress which is called from setup().
     */
    uint8_t macAddr[6] = {0};

    /**
     * @brief IP Address setting
     * 
     * Data is stored in these fields when reading the JSON configuration, setting the value using
     * direct methods, or from DHCP. The updateAddressSettings() method updates all four settings
     * in the W5500 from these fields.
     */
    uint8_t ipAddr[4] = {0};

    /**
     * @brief Subnet mask setting
     * 
     * Data is stored in these fields when reading the JSON configuration, setting the value using
     * direct methods, or from DHCP. The updateAddressSettings() method updates all four settings
     * in the W5500 from these fields.
     */
    uint8_t subnetMaskArray[4] = {0};

    /**
     * @brief Gateway Address setting
     * 
     * Data is stored in these fields when reading the JSON configuration, setting the value using
     * direct methods, or from DHCP. The updateAddressSettings() method updates all four settings
     * in the W5500 from these fields.
     */
    uint8_t gatewayAddr[4] = {0};

    /**
     * @brief DNS server address setting
     * 
     * Only one DNS server can be specified. It is optional, if you don't need to resolve hostnames
     * into IP addresses. For example, if you connect or send packets to IP addresses, or only
     * listen for connections, DNS is not required.
     * 
     * Data is stored in these fields when reading the JSON configuration, setting the value using
     * direct methods, or from DHCP. The updateAddressSettings() method updates all four settings
     * in the W5500 from these fields.
     */
    uint8_t dnsAddr[4] = {0};

    /**
     * @brief Flag is set when the setup() method has been completed
     */
    bool setupDone = false;    

    /**
     * @brief Flag that is updated when the PHY state changes
     * 
     * This is updates at once once per second, so there may be some delay between the time
     * that the link is disconnected and the flag is updated. When the flag is updated, 
     * the callback is called, so you may want to register a callback instead of polling 
     * this flag.
     */
    bool phyLink = false;

    /**
     * @brief Flag that indicates ready. Use the ready() method to read this flag.
     * 
     * This is true when the PHY link is up and an IP address is set. The IP address, subnet
     * mask, and gateway address are not validated, so the network may not function 
     * properly, but this flag will still return true.
     */
    bool isReady = false;

    /**
     * @brief State machine states for DHCP processing
     */
    enum class DhcpState {
        NOT_USED,           //!< Using static IP addressing, no DCHP.
        ATTEMPT,            //!< Set to this state to enable DCHP (default).
        IN_PROGRESS,        //!< After PHY comes up, goes into this state to process DHCP.
        GOT_ADDRESS,        //!< Got an IP address, pending calling callback and updating W5500.
        CLEANUP,            //!< Release memory and socket used by DHCP.
        CLEANUP_DISABLE,    //!< Release memory and socket used by DHCP, then go into NOT_USED.
        DONE                //!< DHCP was completed successfully. If PHY is lost, will go back into ATTEMPT.
    };

    /**
     * @brief Current state machine state for DHCP
     */
    DhcpState dhcpState = DhcpState::ATTEMPT;

    /**
     * @brief Buffer used for DHCP processing. It's 548 bytes, allocated on the heap, only during DHCP processing.
     */
    uint8_t *dhcpBuffer = NULL;

    /**
     * @brief True if DNS is enabled (default)
     */
    bool dnsEnable = true;

    /**
     * @brief Buffer used for DNS processing. It's 512 bytes, allocated on the heap, only during DNS processing.
     */
    uint8_t *dnsBuffer = NULL;

    /**
     * @brief Number of sockets available in hardware. This is 8 on the W5500.
     */
    static const uint8_t NUM_SOCKETS = 8;

    /**
     * @brief Get a socket that is not currently in use for a new connection or listener
     * 
     * @return int -1 if there are no sockets available, otherwise 0 <= sock < NUM_SOCKETS.
     */
    int socketGetFree();

    /**
     * @brief Callbacks registered using withCallback
     * 
     * Number of callbacks limited only by RAM, stored in a std::vector.
     */
    std::vector<std::function<void(CallbackType,void*)>> callbacks;

    /**
     * @brief Path to JSON config file on the POSIX flash file system, if using
     * 
     * Will be an empty string if not using the config file
     */
    String jsonConfigFile;

    /**
     * @brief Singleton instance of this class
     * 
     * The object pointer to this class is stored here. It's NULL at system boot.
     */
    static IsolatedEthernet *_instance;  

    friend class WizInterface;

};


#endif /* __ISOLATEDETHERNET_H */