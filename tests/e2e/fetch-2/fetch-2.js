
async function main(event) {
    let resolve, reject;
    
    try {
        const responsePromise = new Promise(async (res, rej) => {
            resolve = res;
            reject = rej;
        });
        event.respondWith(responsePromise);
        
        const response = await fetch("https://jsonplaceholder.typicode.com/comments");
        const reader = response.body.getReader();

        let bodyChunks = [] ;
        let chunk;
        while (!(chunk = await reader.read()).done) {
            for (let i = 0; i < chunk.value.length; i++) {
                bodyChunks.push(chunk.value[i]);
            }
        }
        const bodyText = new TextDecoder().decode(new Uint8Array(bodyChunks));
        console.log("Successfully received response body");

        resolve(new Response(bodyText, { headers: response.headers }));
    } catch (e) {
        console.log(`Error: ${e}. Stack: ${e.stack}`);
    }
}

addEventListener('fetch', main);
