async function main(event) {
    let resolve, reject;
    
    try {
        let responsePromise = new Promise(async (res, rej) => {
            resolve = res;
            reject = rej;
        });
        event.respondWith(responsePromise);

        console.log("fetching")
        
        let response = await fetch("https://echo.free.beeceptor.com", {
            method: "POST",
            headers: {
                "Content-Type": "application/json"
            },
            body: JSON.stringify({hello: "world"})
        })

        console.log("fetch executed")

        let responseJson = await response.json();

        console.log("Successfully received response json body");
        resolve(new Response(JSON.stringify(responseJson)));
    } catch (e) {
        console.log(`Error: ${e}. Stack: ${e.stack}`);
    }
}

addEventListener('fetch', main);
