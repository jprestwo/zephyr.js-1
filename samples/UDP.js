var udp = require('dgram');

console.log('required dgram');

var server = udp.createSocket({ type: 'udp4' });

server.on('listening', function() {
   console.log('listening');
});

server.on('message', function(message, remote) {
    console.log(remote.address + ':' + remote.port +' - ' + message);
});

server.bind(4242, '192.0.2.1');