async function main(event) {
    try {
        console.log(event.request);
        // let req = new Request();
        let p = fetch('https://fermyon.com/');
        console.log(p);
        let responsePromise = new Promise(async (resolve, reject) => {
            await p;
            resolve(new Response());
        });
        event.respondWith(responsePromise);
        let response = await p;
        console.log("received!");
        console.log(response.headers);
        for (let [key, value] of response.headers.entries()) {
            console.log([key, value]);
        }
        console.log("post headers!");
        // let textP = response.text();
        // let text = await textP;
        // console.log(text);
        // setTimeout(() => console.log(1), 1);
        // setTimeout(() => console.log(10), 10);
    } catch (e) {
        console.log(`Error: ${e}`);
    }
}

addEventListener('fetch', main);
console.log(1)
