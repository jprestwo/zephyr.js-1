var test = require('test_promise');

setInterval(function() {
    console.log("calling promise");
    var p = test.test_promise();
    p.then(function() {
        console.log("fulfilled");
    }).catch(function() {
        console.log("rejected");
    });
}, 100);
