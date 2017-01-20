var udp = require('dgram');

console.log('required dgram');

var server = udp.createSocket({ type: 'udp4' });

server.on('listening', function() {
   console.log('listening');
});

server.on('message', function(message, remote) {
    console.log("Message from " + remote.address + ':' + remote.port +' - ' + message.toString('ascii'));
});

server.bind(4242, '192.0.2.1');
