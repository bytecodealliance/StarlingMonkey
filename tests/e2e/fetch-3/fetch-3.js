async function main(event) {
    let resolve, reject;
    
    try {
        let responsePromise = new Promise(async (res, rej) => {
            resolve = res;
            reject = rej;
        });
        event.respondWith(responsePromise);

        let response = await fetch("https://echo.free.beeceptor.com", {
            method: "POST",
            headers: {
                "Content-Type": "application/json"
            },
            body: JSON.stringify({hello: "world"})
        })

        let responseJson = await response.json();

        let result = {
            method: responseJson.method,
            parsedBody: responseJson.parsedBody,
        };

        console.log("Successfully received response json body");
        resolve(new Response(JSON.stringify(result)));
    } catch (e) {
        console.log(`Error: ${e}. Stack: ${e.stack}`);
    }
}

addEventListener('fetch', main);
