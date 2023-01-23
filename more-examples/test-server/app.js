const os = require('os');
const http = require('http');

const net = require('net')
const dgram = require('dgram');


const serverPort = process.env.SERVER_PORT || 4550;

const multicastAddr = '239.1.1.123';
const multicastPort = serverPort + 1; // 4551

const largeReceivePort = serverPort + 2; // 4552

const largeSendPort = serverPort + 3; // 4553

const showDebug = false;

{
    const ifaces = os.networkInterfaces();
    
    // http://stackoverflow.com/questions/3653065/get-local-ip-address-in-node-js
    Object.keys(ifaces).forEach(function (ifname) {
        ifaces[ifname].forEach(function (iface) {
            if ('IPv4' !== iface.family || iface.internal !== false) {
                // skip over internal (i.e. 127.0.0.1) and non-ipv4 addresses
                return;
            }            
            console.log("found address " + ifname + ": " + iface.address);
        });
    });    
}

async function testServer(options) {
    if (showDebug) console.log('connection ' + options.connNum + ' TCP client connection to server ' + options.remoteAddr + ':' + serverPort);
    
    let success = false;

    const client = net.createConnection(serverPort, options.remoteAddr, function() {
        client.write('testing!\n');      
        success = true;  
    });
    client.on('data', function(data) {
        data = data.replace('\r', '').trim(); // Remove CRs
        for(const line of data.split('\n')) {
            if (line.length > 0) {
                if (showDebug) console.log('testServer ' + options.connNum + ' received data', line);             
            }
        }
        client.end();
    });
    client.on('close', function(data) {
        if (showDebug) console.log('connection ' + options.connNum + ' received close');

        if (!success) {
            console.log('testServer test failed');
        }
    
    });
    client.on('error', function(err) {
        console.log('connection ' + options.connNum  + ' error', err);
    });

}

const server = net.createServer();

let lastConnNum = 0;

server.on('connection', function(socket) {
    const connNum = ++lastConnNum;

    socket.setEncoding('utf8');
    let success = false;

    socket.on('data', function(data) {
        data = data.replace('\r', '').trim(); // Remove CRs
        for(const line of data.split('\n')) {
            if (line.length > 0) {
                if (showDebug) console.log('server received data', line);             
                if (line.startsWith('testing! testCounter=')) {
                    success = true;
                }
            }
        }
    });
    socket.on('close', function(data) {
        if (showDebug) console.log('connection ' + connNum + ' received close');
        socket.end();

        if (!success) {            
            console.log('server did not receive expected data');
        }
    
    });
    socket.on('error', function(err) {
        console.log('connection ' + connNum  + ' error', err);
    });
    
    
    if (showDebug) console.log('connection ' + connNum + ' received from ' + socket.remoteAddress);
    testServer({remoteAddr:socket.remoteAddress, connNum});
    

    socket.write('Test server!\n');
});

server.listen(serverPort, function() {
    if (showDebug) console.log('listening on port ' + serverPort);
});


const multicastServer = dgram.createSocket({ type: "udp4", reuseAddr: true });

multicastServer.bind(multicastPort);

multicastServer.on("listening", function () {
    multicastServer.addMembership(multicastAddr);

    if (showDebug) console.log('multicast listening on ' + multicastServer.address().address + ' multicastAddr=' + multicastAddr + ' port=' + multicastPort);
});
multicastServer.on('message', function(msg, info) {
    // info.address, info.port
    console.log('received multicast UDP from ' + info.address + ': ' + info.port);

});

const udpServer = dgram.createSocket('udp4');
udpServer.on('message', function(msg, info) {
    // info.address, info.port
    console.log('received UDP from ' + info.address + ': ' + info.port);

    udpServer.send("udp response", info.port, info.address, function(err) {
        if (err) {
            console.log('udp send response failed', err);
        }
    });

    udpServer.send('multicast test from udp receiver', multicastPort, multicastAddr, function(err) {
        if (err) {
            console.log('udp multicast send response failed', err);
        }
    });

});
udpServer.bind(serverPort);

const largeReceiveServer = net.createServer();

let lastLargeReceiveConnNum = 0;

largeReceiveServer.on('connection', function(socket) {
    const connNum = ++lastLargeReceiveConnNum;
    let dataOffset = 0;
    let errorsReported = 0;

    const startMs = new Date().getTime();

    socket.on('data', function(data) {

        for(let ii = 0; ii < data.length; ii++, dataOffset++) {
            const receivedData = data.readUint8(ii);
            const expectedData = dataOffset % 256;

            if (receivedData != expectedData && errorsReported < 10) {
                errorsReported++;
                console.log('largeReceiveServer connection ' + connNum + ' invalid data at dataOffset=' + dataOffset + ' got=' + receivedData + ' expected=' + expectedData);
            }
        }
    });
    socket.on('close', function(data) {
        if (showDebug) console.log('largeReceiveServer connection ' + connNum + ' received close');
        socket.end();

        const endMs = new Date().getTime();
        const elapsedMs = endMs - startMs;

        const kbytesPerSec = Math.floor((dataOffset / 1024) / (elapsedMs / 1000) * 10) / 10;

        if (dataOffset == 1024 * 1024 && errorsReported == 0) {
            console.log('largeReceiveServer success: received ' + dataOffset + ' bytes in ' + elapsedMs + ' ms, ' + kbytesPerSec + ' kbytes/sec');
        }
        else {
            console.log('largeReceiveServer error: received ' + dataOffset + ' bytes, ' + errorsReported + ' errors in ' + elapsedMs + ' ms ');
        }
    });
    socket.on('error', function(err) {
        console.log('largeReceiveServer connection ' + connNum  + ' error', err);
    });
});

largeReceiveServer.listen(largeReceivePort, function() {
    if (showDebug) console.log('largeReceiveServer listening on port ' + largeReceivePort);
});


const largeSendServer = net.createServer();

let lastLargeSendConnNum = 0;

largeSendServer.on('connection', function(socket) {
    const connNum = ++lastLargeSendConnNum;
    let dataOffset = 0;

    const startMs = new Date().getTime();

    const buf = Buffer.alloc(1024 * 1024);
    for(let ii = 0; ii < buf.length; ii++, dataOffset++) {
        buf.writeUInt8(dataOffset % 256, ii);
    }
    socket.write(buf);

    if (showDebug) console.log('largeSendServer write completed');

    socket.on('data', function(data) {
    });
    socket.on('close', function(data) {
        // TODO: Add data speed stats here
        console.log('largeSendServer connection ' + connNum + ' received close');
        socket.end();

        const endMs = new Date().getTime();
        const elapsedMs = endMs - startMs;
        const kbytesPerSec = Math.floor((dataOffset / 1024) / (elapsedMs / 1000) * 10) / 10;

        console.log('largeSendServer sent ' + dataOffset + ' bytes, in ' + elapsedMs + ' ms, ' + kbytesPerSec + ' Kbytes/sec');
    });
    socket.on('error', function(err) {
        console.log('largeSendServer connection ' + connNum  + ' error', err);
    });
});

largeSendServer.listen(largeSendPort, function() {
    if (showDebug) console.log('largeSendServer listening on port ' + largeSendPort);
});
