async function main(event) {
    let resolve, reject;
    
    try {
        let responsePromise = new Promise(async (res, rej) => {
            resolve = res;
            reject = rej;
        });
        event.respondWith(responsePromise);

        const headerKey = "X-Test-Header";
        const headerValue = "test-header-value";
        
        let response = await fetch("https://echo.free.beeceptor.com", {
            method: "POST",
            // headers: {
            //     [headerKey] :headerValue 
            // },
            body: "hello world"
        })

        let responseJson = await response.json();
        console.log("Successfully received response json body");
        resolve(new Response(JSON.stringify(responseJson)));
    } catch (e) {
        console.log(`Error: ${e}. Stack: ${e.stack}`);
    }
}

addEventListener('fetch', main);
