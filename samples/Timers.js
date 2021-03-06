// Copyright (c) 2016, Intel Corporation.

// Example using setInterval and setTimeout, passing arguments
// The timeout should stop the setInterval timer after five times

// Hardware Requirements:
//   - None

print("Starting Timers example...");

var count = 1;

var i = setInterval(function(a, b) {
    print("Interval #" + count + ' arg1: ' + a + ' arg2: ' + b);
    count++;
}, 1000, 1, 2);

setTimeout(function(a) {
    print("Timeout, clearing interval");
    clearInterval(a);
}, 5000, i);
