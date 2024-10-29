addEventListener('fetch', async (event) => {
  try {
    if (!event.request.url.includes('/nested')) {
      event.respondWith(main(event));
      return;
    }

    let encoder = new TextEncoder();
    let body = new TransformStream({
      start(controller) {
      },
      transform(chunk, controller) {
        controller.enqueue(encoder.encode(chunk));
      },
      flush(controller) {
      }
    });
    let writer = body.writable.getWriter();
    event.respondWith(new Response(body.readable));
    let word = new URL(event.request.url).searchParams.get('word');
    console.log(`streaming word: ${word}`);
    for (let letter of word) {
      console.log(`Writing letter ${letter}`);
      await writer.write(letter);
    }
    if (word.endsWith(".")) {
      await writer.write("\n");
    }
    await writer.close();
  } catch (e) {
    console.error(e);
  }
});
async function main(event) {
  let fullBody = "This sentence will be streamed in chunks.";
  let responses = [];
  for (let word of fullBody.split(" ").join("+ ").split(" ")) {
    responses.push((await fetch(`${event.request.url}/nested?word=${word}`)).body);
  }
  return new Response(concatStreams(responses));
}

function concatStreams(streams) {
  let { readable, writable } = new TransformStream();
  async function iter() {
    for (let stream of streams) {
      try {
        await stream.pipeTo(writable, {preventClose: true});
      } catch (e) {
        console.error(`error during pipeline execution: ${e}`);
      }
    }
    console.log("closing writable");
    await writable.close();
  }
  iter();
  return readable;
}
