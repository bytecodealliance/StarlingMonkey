async function main(event) {
    let resolve, reject;

    // Function to create an async stream
    function createAsyncStream() {
        const encoder = new TextEncoder();
        const readableStream = new ReadableStream({
            start(controller) {
                const chunk = { hello: "world" };
                const encodedChunk = encoder.encode(JSON.stringify(chunk));
                
                // Simulate an async operation
                setTimeout(() => {
                    controller.enqueue(encodedChunk);
                    controller.close();
                }, 1000); // 1 second delay
            }
        });
        return readableStream;
    }

    try {
        let responsePromise = new Promise(async (res, rej) => {
            resolve = res;
            reject = rej;
        });
        event.respondWith(responsePromise);

        console.log("Sending fetch request with async stream");

        // Create the async stream
        const asyncStream = createAsyncStream();

        // Perform the fetch request with the async stream as the body
        let response = await fetch("https://echo.free.beeceptor.com", {
            method: "POST",
            headers: {
                "Content-Type": "application/json"
            },
            body: asyncStream
        });

        console.log("Fetch request success");

        let responseJson = await response.json();

        console.log("Response JSON:", responseJson);

        let result = {
            method: responseJson.method,
            parsedBody: responseJson.parsedBody,
        };

        console.log("Successfully received response json body:", result);

        resolve(new Response(JSON.stringify(result)));
    } catch (e) {
        console.log(`Error: ${e}. Stack: ${e.stack}`);
    } 
}

addEventListener('fetch', main);
