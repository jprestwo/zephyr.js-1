var result = true;

var p1 = new Promise(function(fulfill, reject) {
    console.log("Running promise function");
    if (result) {
        console.log("fulfilling promise");
        fulfill("fulfilled");
    } else {
        reject("rejected");
    } 
});

console.log("setting then/catch");

p1.then(function(arg) {
    console.log("then:" + arg);
}).catch(function(arg) {
    console.log("catch:" + arg);
});
