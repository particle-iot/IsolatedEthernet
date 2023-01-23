#include "IsolatedEthernet.h"

SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);

SerialLogHandler logHandler(LOG_LEVEL_INFO, { // Logging level default
    { "app.ether", LOG_LEVEL_INFO } // Logging level for IsolatedEthernet messages
});

const IPAddress serverAddr(192,168,2,6);
const uint16_t serverPort = 4550;
int testCounter = 0;
int errorCount = 0;

const IPAddress multicastAddr(239, 1, 1, 123);
const uint16_t multicastPort = serverPort + 1; // 4551

IsolatedEthernet::TCPServer server(serverPort);

void runTests();

void setup() {
    // Ethernet must be disabled in Device OS
    System.disableFeature(FEATURE_ETHERNET_DETECTION);

    waitFor(Serial.isConnected, 10000);
    delay(2000);

    /*
        .withEthernetFeatherWing()
        .withEthernetMikroeGen3SomShield(1)
        .withEthernetMikroeFeatherShield(1)
        .withEthernetM2EvalBoard()
        .withPinCS(D8)

        .withIPAddress(IPAddress(192, 168, 2, 26))
        .withSubnetMask(IPAddress(255, 255, 255, 0))
        .withGatewayAddress(IPAddress(192, 168, 1, 1))

    */

    IsolatedEthernet::instance()
        .withEthernetFeatherWing()
        .setup();

    // Particle.connect();
}

void loop() {
    static bool lastReady = false;
    bool curReady = IsolatedEthernet::instance().ready();
    if (lastReady != curReady) {
        if (curReady) {
            Log.trace("Ethernet ready");
            server.begin();
        }
        else {
            Log.trace("Ethernet not ready");
        }
        lastReady = curReady;
    }


    {
        static unsigned long lastCheck = 0;
        if (lastReady && millis() - lastCheck >= 30000) {
            lastCheck = millis();

            runTests();

            lastCheck = millis();
        }
    }

}


void runTests() {
    Log.trace("running tests...");
    testCounter++;
    errorCount = 0;

    {
        IsolatedEthernet::TCPClient client;

        if (client.connect(serverAddr, serverPort)) {
            Log.trace("connected by TCP");

            String s = client.readStringUntil('\n');
            Log.trace("read '%s'", s.c_str());
            if (s != "Test server!") {
                Log.error("TCPClient test invalid data: %s", s.c_str());
                errorCount++;
            }


            client.printlnf("testing! testCounter=%d", testCounter);
                      
            //Log.trace("read '%s'", s.c_str());

            client.stop();

        }
        else {
            Log.error("TCPClient test failed to connect");
            errorCount++;
        }
    }

    {
        IsolatedEthernet::TCPClient client;

        bool gotResponse = false;

        unsigned long start = millis();
        while(millis() - start < 2000) {
            client = server.available();
            if (client) {
                Log.trace("received server connection!");

                String s = client.readStringUntil('\n');
                Log.trace("server read '%s'", s.c_str());

                if (s != "testing!") {
                    Log.error("TCPServer test invalid data: %s", s.c_str());
                }
                else {
                    gotResponse = true;
                }

                client.stop();
            }
        }

        if (!gotResponse) {
            Log.error("TCPServer test failed");
            errorCount++;
        }
    }

    {
        IsolatedEthernet::UDP multicastReceiver;
        multicastReceiver.begin(multicastPort);
        multicastReceiver.joinMulticast(multicastAddr);

        IsolatedEthernet::UDP udp;

        bool gotUDP = false;
        bool gotUDPMulitcast = false;

        udp.begin(serverPort);

        char buf[128];
        snprintf(buf, sizeof(buf), "Test UDP client testCounter=%d", testCounter);
        udp.sendPacket(buf, strlen(buf), serverAddr, serverPort);

        snprintf(buf, sizeof(buf), "Test UDP multicast testCounter=%d", testCounter);
        multicastReceiver.sendPacket(buf, strlen(buf), multicastAddr, multicastPort);

        unsigned long start = millis();
        while(millis() - start < 2000) {
            int size = udp.receivePacket(buf, sizeof(buf));
            if (size > 0) {
                buf[size] = 0;
                Log.trace("UDP packet received: %s", buf);
                gotUDP = true;
            }

            size = multicastReceiver.receivePacket(buf, sizeof(buf));
            if (size > 0) {
                buf[size] = 0;
                Log.trace("UDP multicast packet received: %s", buf);
                gotUDPMulitcast = true;
            }
        }
        
        udp.stop();
        multicastReceiver.stop();

        if (!gotUDP) {
            Log.error("UDP receive test failed");
            errorCount++;
        } 
        if (!gotUDPMulitcast) {
            Log.error("UDP multicast receive test failed");
            errorCount++;
        }

    }

    {
        IsolatedEthernet::TCPClient client;

        if (client.connect(serverAddr, serverPort + 2)) {
            Log.trace("connected by TCP to largeReceiveServer");

            size_t sendSize = 4096;
            uint8_t *buf = new uint8_t[sendSize];

            for(size_t dataOffset = 0; dataOffset < 1024 * 1024; dataOffset += sendSize) {
                for(size_t ii = 0; ii < sendSize; ii++) {
                    buf[ii] = (uint8_t) (dataOffset + ii);            
                }
                int res = client.write(buf, sendSize);
                if (res != sendSize) {
                    Log.error("Large receive test error writing %d at offset %lu", res, dataOffset);
                    errorCount++;
                    break;
                }
            }
            // Make sure send buffer is empty
            client.flush();

            delete[] buf;

            client.stop();

        }
        else {
            Log.error("Large receive test failed to connect");
            errorCount++;
        }        
    }
    {
        IsolatedEthernet::TCPClient client;

        if (client.connect(serverAddr, serverPort + 3)) {
            Log.trace("connected by TCP to largeSendServer (this test takes a while)");

            size_t bufSize = 2048;
            uint8_t *buf = new uint8_t[bufSize];
            size_t dataOffset;
            size_t numErrors = 0;

            unsigned long start = millis();

            for(dataOffset = 0; dataOffset < 1024 * 1024; ) {
                int count = client.readBytes((char *)buf, bufSize);
                if (count < 0) {
                    Log.error("error reading %d", count);
                    break;
                }   

                for(size_t ii = 0; ii < count; ii++, dataOffset++) {
                    if (buf[ii] != (uint8_t) dataOffset) {
                        if (++numErrors < 19) {
                            Log.error("data mismatch offset=%d got=%02x expected=%02x", dataOffset, buf[ii], (uint8_t) dataOffset);
                        }
                    }
                }
                dataOffset += count;
                // Log.trace("largeSendServer dataOffset=%d", dataOffset);

                if (millis() - start >= 120000) {
                    Log.error("test timed out with %lu bytes received", dataOffset);
                    numErrors = 1024 * 1024 - dataOffset;
                    break;
                }
            }

            delete[] buf;

            unsigned long elapsed = millis() - start;
            double rate = ((double)dataOffset / 1024) / ((double)elapsed / 1000);


            if (dataOffset != 1024 * 1024 || numErrors > 0) {
                Log.error("largeDataSend test error received %lu bytes in %lu ms, numErrors=%lu, %.1lf kbytes/sec", dataOffset, elapsed, numErrors, rate);
                errorCount++;
            }
            else {
                Log.info("largeDataSend test received %lu bytes in %lu ms, numErrors=%lu, %.1lf kbytes/sec", dataOffset, elapsed, numErrors, rate);
            }            

            client.stop();

        }
        else {
            Log.error("largeSendServer failed to connect");
            errorCount++;
        }                
    }

    if (errorCount == 0) {
        Log.info("tests completed successfully!");
    }
    else {
        Log.error("tests completed with %d errors", errorCount);
    }

}