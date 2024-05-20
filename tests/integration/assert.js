export class AssertionError extends Error {
  constructor (message, detail, actual, expected) {
    let errText = message;
    if (detail)
      errText += ` - ${detail}`;
    if (actual || expected)
      errText += `\n\nGot:\n\n${JSON.stringify(actual)}\n\nExpected:\n\n${JSON.stringify(expected)}`
    super(errText);
  }
}

export function assert (assertion, message) {
  if (!assertion)
    throw new AssertionError('assertion failed', message);
}

export function strictEqual (actual, expected, message) {
  if (actual !== expected)
    throw new AssertionError('not strict equal', message, actual, expected);
}

function innerDeepStrictEqual(actual, expected) {
  if (Array.isArray(expected)) {
    if (!Array.isArray(actual) || expected.length !== actual.length)
      return false;
    for (let i = 0; i < expected.length; i++) {
      if (!innerDeepStrictEqual(actual[i], expected[i]))
        return false;
    }
    return true;
  }
  else if (typeof expected === 'object' && expected !== null) {
    if (typeof actual !== 'object' || actual === null)
      return false;
    const keys = Object.keys(expected).sort();
    if (Object.keys(actual).length !== keys.length)
      return false;
    for (const key of keys) {
      if (!innerDeepStrictEqual(actual[key], expected[key]))
        return false;
    }
    return true;
  }
  else {
    return actual === expected;
  }
}

export function deepStrictEqual (actual, expected, message) {
  if (!innerDeepStrictEqual(actual, expected))
    throw new AssertionError('not deep strict equal', message, actual, expected);
}

export function throws(func, errorClass, errorMessage) {
  try {
    func();
  } catch (err) {
    if (errorClass) {
      if (!(err instanceof errorClass)) {
        throw new AssertionError(`not expected error instance calling \`${func.toString()}\``, errorMessage, err.name, errorClass.name);
      }
    }
    if (errorMessage) {
      if (err.message !== errorMessage) {
        throw new AssertionError(`not expected error message calling \`${func.toString()}\``, errorMessage, err.message, errorMessage);
      }
    }
    return;
  }
  throw new AssertionError(`Expected \`${func.toString()}\` to throw, but it didn't`);
}
