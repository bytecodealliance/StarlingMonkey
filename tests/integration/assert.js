export class AssertionError extends Error {
  constructor (msg) {
    super(msg);
  }
}

function fail (check, message, actual, expected) {
  let errText = check;
  if (message)
    errText += ` - ${message}`;
  if (actual || expected)
    errText += `\n\nGot:\n\n${JSON.stringify(actual)}\n\nExpected:\n\n${JSON.stringify(expected)}`
  throw new AssertionError(errText);
}

export function strictEqual (actual, expected, message) {
  if (actual !== expected) {
    fail('not strict equal', message, actual, expected);
  }
}

export function throws(func, errorClass, errorMessage) {
  try {
    func();
  } catch (err) {
    if (errorClass) {
      if (!(err instanceof errorClass)) {
        return fail(`not expected error instance calling \`${func.toString()}\``, errorMessage, err.name, errorClass.name);
      }
    }
    if (errorMessage) {
      if (err.message !== errorMessage) {
        return fail(`not expected error message calling \`${func.toString()}\``, errorMessage, err.message, errorMessage);
      }
    }
    return;
  }
  fail(`Expected \`${func.toString()}\` to throw, but it didn't`);
}
