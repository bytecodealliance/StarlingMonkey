var v = "bar";
console.log("hello.");
addEventListener('fetch', (event) => {
    foo(event);
});

function foo(ev) {
    let arr = [1, 2, 3];
    let obj = {
        a: 1,
        b: 2,
        c: 3
    };
    let re = /a/;
    bar(ev, obj);
    console.log("hello from foo");
}

function bar(evt, o) {
    let body = `Hello from content! ${o.b}`;
    let resp = new Response(body);
    evt.respondWith(resp);
}
