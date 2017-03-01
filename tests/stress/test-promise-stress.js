var test = require('test_promise');

var sync_toggle = 0;
var async_toggle = 0;

setInterval(function() {
    console.log("calling promise");
    var p = test.test_promise();
    p.then(function() {
        console.log("fulfilled sync");
    }).catch(function() {
        console.log("rejected sync");
    });
}, 100);


setInterval(function() {
    var p = test.test_async_promise();
    p.then(function() {
        console.log("fulfilled async");
    }).catch(function() {
        console.log("rejected async");
    });
    setTimeout(function() {
        if (async_toggle) {
            test.fulfill(p);
        } else {
            test.reject(p);
        }
        async_toggle = (async_toggle) ? 0 : 1;
    }, 100);
}, 200);
