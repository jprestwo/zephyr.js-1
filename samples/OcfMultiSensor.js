// Copyright (c) 2017, Intel Corporation.

var ocf = require('ocf');
var server = ocf.server;

var aio = require('aio');
var gpio = require('gpio');
var pins = require("arduino101_pins");

var light = aio.open({ device: 0, pin: pins.A0 });
var temp = aio.open({ device: 0, pin: pins.A1 });
var fan = gpio.open({ pin: 8, mode: 'out', activeLow: false });
var PIR = gpio.open({ pin: 2, mode: 'in', edge: 'any' });

console.log("Started OCF server");

function logBase(base, y)
{
    return Math.log(y) / Math.log(base);
}

function convertRaw(raw)
{
    return (raw/4095) * 1023;
}

function calcTemp(raw)
{
    var B = 4275;
    var R0 = 100000;
    var R = 1023 / convertRaw(raw) - 1.0;
    R = R0 * R;
    var temp = 1.0 / (logBase(10, (R / R0)) / B + 1 / 298.15) - 273.15;
    if (tempProperties.units == 'C') {
        return temp;
    } else {
        return temp * (9/5) + 32;
    }
}

var lightProperties = {
    illuminance: null
};

var tempProperties = {
    temperature: null,
    units: 'C'
};

var fanProperties = {
    value: 0
};

var motionProperties = {
    value: 0
};

var motionResource = null;

PIR.onchange = function(event) {
    console.log("motion: " + event.value);
    motionProperties.value = event.value;
};

var lightInit = {
    resourcePath: "/a/illuminance",
    resourceTypes: ["oic.r.sensor.illuminance"],
    interfaces: ["/oic/if/r"],
    discoverable: true,
    observable: true,
    properties: lightProperties
};

var tempInit = {
    resourcePath: "/a/temperature",
    resourceTypes: ["oic.r.temperature"],
    interfaces: ["/oic/if/rw"],
    discoverable: true,
    observable: true,
    properties: tempProperties
};

var fanInit = {
    resourcePath: "/a/fan",
    resourceTypes: ["oic.r.fan"],
    interfaces: ["/oic/if/rw"],
    discoverable: true,
    observable: true,
    properties: fanProperties
};

var motionInit = {
    resourcePath: "/a/pir",
    resourceTypes: ["oic.r.sensor.motion"],
    interfaces: ["/oic/if/r"],
    discoverable: true,
    observable: true,
    properties: motionProperties
};

server.register(lightInit).then(function(resource) {
    console.log("Light resource registered");
});

server.register(tempInit).then(function(resource) {
    console.log("Temperature resource registered");
});

server.register(fanInit).then(function(resource) {
    console.log("Fan resource registered");
});

server.register(motionInit).then(function(resource) {
    console.log("Motion resource registered");
    motionResource = resource;
});

server.on('retrieve', function(request, observe) {
    if (request.target.resourcePath == "/a/illuminance") {
        lightProperties.light = light.read();
        request.respond(lightProperties);
    } else if (request.target.resourcePath == "/a/temperature") {
        tempProperties.temperature = calcTemp(temp.read());
        request.respond(tempProperties);
    } else if (request.target.resourcePath == "/a/fan") {
        request.respond(fanProperties);
    } else if (request.target.resourcePath == "/a/pir") {
        motionProperties.value = PIR.read();
        request.respond(motionProperties);
    } else {
        console.log("Resource requested does not exist");
    }
});

server.on('update', function(request) {
    console.log("updating: " + request.target.resourcePath);
    if (request.target.resourcePath == "/a/fan") {
        if (request.resource.properties) {
            fanProperties.value = request.resource.properties.value;
            fan.write(fanProperties.value);
            request.respond(fanProperties);
        }
    } else if (request.target.resourcePath == "/a/temperature") {
        console.log("in /a/temperature");
        if (request.resource.properties) {
            console.log("Updating temp");
            tempProperties.units = request.resource.properties.units;
            request.respond(tempProperties);
        }
    }
});

ocf.start();
