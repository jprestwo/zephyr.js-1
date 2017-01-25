// Copyright (c) 2016-2017, Intel Corporation.

// Testing EVENT APIs

var event = require("events");

var total = 0;
var passed = 0;

function assert(actual, description) {
    total += 1;

    var label = "\033[1m\033[31mFAIL\033[0m";
    if (actual === true) {
        passed += 1;
        label = "\033[1m\033[32mPASS\033[0m";
    }

    console.log(label + " - " + description);
}

function expectThrow(description, func) {
    var threw = false;
    try {
        func();
    }
    catch (err) {
        threw = true;
    }
    assert(threw, description);
}

var eventEmitter = new event();

expectThrow("event: set max of listeners as 'value'", function () {
    eventEmitter.setMaxListeners("value");
});

expectThrow("event: set max of listeners as '-1024'", function () {
    eventEmitter.setMaxListeners(-1024);
});

expectThrow("event: get max of listeners on event named 'value'", function () {
    eventEmitter.getMaxListeners("value");
});

var defaultNum = eventEmitter.defaultMaxListeners;
var maxNum = eventEmitter.getMaxListeners();
assert(defaultNum === maxNum, "event: default number of listeners");

var SetlistenersNum = 20;
eventEmitter.setMaxListeners(SetlistenersNum);
var GetlistenersNum = eventEmitter.getMaxListeners();
assert(SetlistenersNum === GetlistenersNum,
       "event: set and get max of listeners");

var NoeventsName = eventEmitter.eventNames();
assert(NoeventsName !== null && typeof NoeventsName === "object",
       "event: get event name by no-event");

var EmitnoeventFlag, EmiteventFlag;
EmitnoeventFlag = eventEmitter.emit("value");

// set listener without arg
var onFlag = false;
function test_listener_noArg () {
    onFlag = true;
}

eventEmitter.on("event_noArg", test_listener_noArg);
EmiteventFlag = eventEmitter.emit("event_noArg");

// set listener with 20 args
var totalNum = 0;
var eventArg = [1, 2, 3, 4, 5, 6, 7, 8,
                9, 10, 11, 12, 13, 14,
                15, 16, 17, 18, 19, 20];

function test_listener_moreArg (arg1, arg2, arg3, arg4, arg5, arg6, arg7,
                                arg8, arg9, arg10, arg11, arg12, arg13, arg14,
                                arg15, arg16, arg17, arg18, arg19, arg20) {
    totalNum = arg1 + arg2 + arg3 + arg4 + arg5 + arg6 + arg7 +
               arg8 + arg9 + arg10 + arg11 + arg12 + arg13 + arg14 +
               arg15 + arg16 + arg17 + arg18 + arg19 + arg20;
}

eventEmitter.on("event_moreArg", test_listener_moreArg);

eventEmitter.emit("event_moreArg", eventArg[0], eventArg[1], eventArg[2],
                                   eventArg[3], eventArg[4], eventArg[5],
                                   eventArg[6], eventArg[7], eventArg[8],
                                   eventArg[9], eventArg[10], eventArg[11],
                                   eventArg[12], eventArg[13], eventArg[14],
                                   eventArg[15], eventArg[16], eventArg[17],
                                   eventArg[18], eventArg[19]);

// set 10 listener on one event
function event_listener_1 () {};
function event_listener_2 () {};
function event_listener_3 () {};
function event_listener_4 () {};
function event_listener_5 () {};
function event_listener_6 () {};
function event_listener_7 () {};
function event_listener_8 () {};
function event_listener_9 () {};
function event_listener_10 () {};

eventEmitter.on("event_listener", event_listener_1);
eventEmitter.on("event_listener", event_listener_2);
eventEmitter.on("event_listener", event_listener_3);
eventEmitter.on("event_listener", event_listener_4);
eventEmitter.on("event_listener", event_listener_5);
eventEmitter.addListener("event_listener", event_listener_6);
eventEmitter.addListener("event_listener", event_listener_7);
eventEmitter.addListener("event_listener", event_listener_8);
eventEmitter.addListener("event_listener", event_listener_9);
eventEmitter.addListener("event_listener", event_listener_10);

// test listeners name
var listenersName;
expectThrow("event: get listeners name without event name", function () {
    listenersName = eventEmitter.listeners();
});

expectThrow("event: get listeners name with invalid event", function () {
    listenersName = eventEmitter.listeners("value");
});

listenersName = eventEmitter.listeners("event_listener");
assert(listenersName.length === 10,
       "event: add listener and get listeners name");

// test events name
var eventsName;
expectThrow("event: get events name with invalid parameter", function () {
    eventsName = eventEmitter.eventNames("value");
});

eventsName = eventEmitter.eventNames();
assert((eventsName.length - 9) === 3, "event: get all events name");

// test remove all listeners
var oldAllListenersNum, newAllListenersNum;
expectThrow("event: remove all listeners without event name", function () {
    eventEmitter.removeAllListeners();
});

oldAllListenersNum = eventEmitter.listenerCount("event_listener");
eventEmitter.removeAllListeners("event_listener");
newAllListenersNum = eventEmitter.listenerCount("event_listener");
assert(oldAllListenersNum !== 0 && newAllListenersNum === 0,
       "event: remove all listeners on event");

// event response time is about 10 ms
var OldlistenerNum, NewlistenerNum;
setTimeout(function() {
    assert(onFlag, "event: listen and emit without args");

    assert(totalNum === 210, "event: set 20 args for listener");

    assert(EmitnoeventFlag === false && typeof EmitnoeventFlag === "boolean",
           "event: emit with invalid event name");

    assert(EmiteventFlag === true && typeof EmiteventFlag === "boolean",
           "event: emit with event name");

    // test remove one listener
    OldlistenerNum = eventEmitter.listenerCount("event_noArg");
    eventEmitter.removeListener("event_noArg", test_listener_noArg);
    NewlistenerNum = eventEmitter.listenerCount("event_noArg");

    assert(OldlistenerNum === 1 && NewlistenerNum === 0,
           "event: remove one listener");

    console.log("TOTAL: " + passed + " of " + total + " passed");
}, 1000);
