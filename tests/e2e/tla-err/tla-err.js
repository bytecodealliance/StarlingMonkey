let reject;
Promise.resolve().then(() => {
  reject(new Error('blah'));
});

await new Promise((_, _reject) => void (reject = _reject));
