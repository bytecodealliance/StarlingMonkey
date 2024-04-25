import { Foo } from './smoke-dependency.js';
import "./nested-smoke-dependency.js";


function readAll(response) {
    return response.text();
}

function passBody(response) {
    return response.body;
}

function pipeBody(response) {
    let ts = new TransformStream();
    response.body.pipeTo(ts.writable);
    return ts.readable;
}

function pumpBody(response) {
    const reader = response.body.getReader();

    return new ReadableStream({
        start(controller) {
            return pump();

            function pump() {
                return reader.read().then(({ done, value }) => {
                    // When no more data needs to be consumed, close the stream
                    if (done) {
                        controller.close();
                        return;
                    }

                    console.log(`piping ${value.byteLength} bytes`);

                    // Enqueue the next data chunk into our target stream
                    controller.enqueue(value);
                    return pump();
                });
            }
        },
    });
}

function embeddedBody() {
    return `<!doctype html>
<html>
<head>
    <title>Example Domain</title>

    <meta charset="utf-8" />
    <meta http-equiv="Content-type" content="text/html; charset=utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <style type="text/css">
    body {
        background-color: #f0f0f2;
        margin: 0;
        padding: 0;
        font-family: -apple-system, system-ui, BlinkMacSystemFont, "Segoe UI", "Open Sans", "Helvetica Neue", Helvetica, Arial, sans-serif;
        
    }
    div {
        width: 600px;
        margin: 5em auto;
        padding: 2em;
        background-color: #fdfdff;
        border-radius: 0.5em;
        box-shadow: 2px 3px 7px 2px rgba(0,0,0,0.02);
    }
    a:link, a:visited {
        color: #38488f;
        text-decoration: none;
    }
    @media (max-width: 700px) {
        div {
            margin: 0 auto;
            width: auto;
        }
    }
    </style>    
</head>

<body>
<div>
    <h1>Example Domain</h1>
    <p>This domain is for use in illustrative examples in documents. You may use this
    domain in literature without prior coordination or asking for permission.</p>
    <p><a href="https://www.iana.org/domains/example">More information...</a></p>
</div>
</body>
</html>
`;
}

async function main(event) {
    let resolve, reject;

    try {
        // let now = Date.now();
        // let diff = () => Date.now() - now;
        // // // setTimeout(() => console.log(`1`), 1);
        // // // let id = setTimeout(() => console.log(`5000`), 5000);
        // // // clearTimeout(id);
        // // setTimeout(() => console.log(diff()), 10);
        // // // clearTimeout(id);
        // // setTimeout(() => console.log(diff()), 100);
        // // setTimeout(() => console.log(diff()), 1000);
        // setTimeout(() => console.log(diff()), 2000);
        // // setTimeout(() => console.log(diff()), 3000);
        // // setTimeout(() => console.log(diff()), 3000);
        // // setTimeout(() => console.log(diff()), 4000);
        // // setTimeout(() => console.log(diff()), 5000);

        let responsePromise = new Promise(async (res, rej) => {
            resolve = res;
            reject = rej;
        });
        event.respondWith(responsePromise);

        let url = new URL(event.request.url);
        if (url.pathname === "/") {
            console.log(`chaining to /chained`);
            let response = await fetch("/chained");
            let body = await response.text();
            resolve(new Response(body));
            return;
        }

        url.host = "example.com";
        url.protocol = "https";
        url.port = "";
        // let p2 = fetch(url);
        // let resolve, reject;

        // let p = fetch("https://example.com");
        // let response = await p;
        // console.log(response);
        // let body = await readAll(response);
        // let body = passBody(response);
        // let body = pipeBody(response);
        // let body = pumpBody(response);
        let body = embeddedBody();

        let resp = new Response(body, {headers: typeof response != "undefined" ? response.headers : undefined});
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
        console.log(`Error: ${e}. Stack: ${e.stack}`);
    }
}

addEventListener('fetch', main);
