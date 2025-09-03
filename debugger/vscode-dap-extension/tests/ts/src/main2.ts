const v = "bar";
console.log("hello.");

//@ts-ignore
addEventListener('fetch', (event: FetchEvent) => {
    foo(event);
});

function foo(ev: FetchEvent) {
    let arrrr = [1, 2, 3];
    let obj = {
        a: 1,
        b: 2,
        c: 3
    };
    let re = /a/;
    barrrrrr(ev, obj);
    console.log("hello from foo");
}

function barrrrrr(evt: FetchEvent, o: { b: number }) {
    let body = `Hello from +++EVEN MORE SSSSAUCIER+++ TYPESCRIPTIER content! ${o.b}`;
    let resp = new Response(body);
    evt.respondWith(resp);
}
