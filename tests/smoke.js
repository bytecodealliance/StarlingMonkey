async function main(event) {
    try {
        let url = new URL(event.request.url);
        url.host = "fermyon.com";
        url.protocol = "https";
        url.port = "";
        // console.log(event.request);
        // let req = new Request();
        let p = fetch(url);
        let resolve, reject;
        let responsePromise = new Promise(async (res, rej) => {
            resolve = res;
            reject = rej;
        });
        event.respondWith(responsePromise);
        let response = await p;
        // console.log("received!");
        let buffer = await response.arrayBuffer();
        let body = new ReadableStream()

        resolve(new Response(body, {headers: response.headers}));
        // for (let [key, value] of response.headers.entries()) {
        //     console.log([key, value]);
        // }
        // let textP = response.text();
        // event.waitUntil(textP);
        // let text = await textP;
        // console.log(text.substring(0, 50));
        // resolve(new Response(text));
        // setTimeout(() => console.log(1), 1);
        // setTimeout(() => console.log(10), 10);
    } catch (e) {
        console.log(`Error: ${e}`);
    }
}

addEventListener('fetch', main);
console.log(1)
