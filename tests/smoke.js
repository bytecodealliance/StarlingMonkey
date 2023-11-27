async function main(event) {
    try {
        let now = Date.now();
        // let diff = () => Date.now() - now;
        // // setTimeout(() => console.log(`1`), 1);
        // // let id = setTimeout(() => console.log(`5000`), 5000);
        // // clearTimeout(id);
        // setTimeout(() => console.log(diff()), 10);
        // // clearTimeout(id);
        // setTimeout(() => console.log(diff()), 100);
        // setTimeout(() => console.log(diff()), 1000);
        // setTimeout(() => console.log(diff()), 2000);
        // setTimeout(() => console.log(diff()), 3000);
        // setTimeout(() => console.log(diff()), 3000);
        // setTimeout(() => console.log(diff()), 4000);
        // setTimeout(() => console.log(diff()), 5000);
        let url = new URL(event.request.url);
        url.host = "fermyon.com";
        url.protocol = "https";
        url.port = "";
        let p = fetch(url);
        let p2 = fetch(url);
        let resolve, reject;
        let responsePromise = new Promise(async (res, rej) => {
            resolve = res;
            reject = rej;
        });
        event.respondWith(responsePromise);
        let response = await p;
        console.log(response);
        let incomingBody = response.body;
        let body = null;
        if (incomingBody) {
            let ts = new TransformStream();
            incomingBody.pipeTo(ts.writable);
            body = ts.readable;
            // const reader = incomingBody.getReader();
            //
            // body = new ReadableStream({
            //     start(controller) {
            //         return pump();
            //
            //         function pump() {
            //             return reader.read().then(({ done, value }) => {
            //                 // When no more data needs to be consumed, close the stream
            //                 if (done) {
            //                     controller.close();
            //                     return;
            //                 }
            //
            //                 console.log(`piping ${value.byteLength} bytes`);
            //
            //                 // Enqueue the next data chunk into our target stream
            //                 controller.enqueue(value);
            //                 return pump();
            //             });
            //         }
            //     },
            // });
        }

        let resp = new Response(body, {headers: response.headers});
        console.log(`post resp ${resp}`);
        resolve(resp);
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
