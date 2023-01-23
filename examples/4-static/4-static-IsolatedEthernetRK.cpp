#include "IsolatedEthernet.h"

SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);

SerialLogHandler logHandler(LOG_LEVEL_INFO, { // Logging level default
    { "app.ether", LOG_LEVEL_TRACE } // Logging level for IsolatedEthernet messages
});

// When running this example, it's expected to get a bad request error like this:
// 0000086755 [app] INFO: data: HTTP/1.1 400 Bad Request


void setup() {
    // Ethernet must be disabled in Device OS
    System.disableFeature(FEATURE_ETHERNET_DETECTION);

    waitFor(Serial.isConnected, 10000);
    delay(2000);

    IsolatedEthernet::instance()
        .withEthernetFeatherWing()
        .withIPAddress(IPAddress(192, 168, 2, 26))
        .withSubnetMask(IPAddress(255, 255, 255, 0))
        .withGatewayAddress(IPAddress(192, 168, 2, 1))
        .withDNSAddress(IPAddress(8, 8, 8, 8))
        .setup();    

    // Particle.connect();
}

void loop() {
    
    static bool testRun = false;
    if (!testRun && IsolatedEthernet::instance().ready()) {
        testRun = true;
        IsolatedEthernet::TCPClient client;

        if (client.connect("particle.io", 80)) {
            IPAddress remoteAddr = client.remoteIP();
            Log.info("connected to %s", remoteAddr.toString().c_str());

            client.write("HEAD / HTTP/1.0\r\n\r\n");

            String s = client.readStringUntil('\n');
            client.stop();

            Log.info("data: %s", s.c_str());
        }
    }
}
