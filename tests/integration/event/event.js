import { serveTest } from '../test-server.js';
import { strictEqual, deepStrictEqual, throws } from '../../assert.js';

export const handler = serveTest(async (t) => {
  await t.test('event-listener-exception-handling', async () => {
    const calledListeners = [];
    const thrownErrors = [];

    const target = new EventTarget();

    function addListener(name, shouldThrow = false) {
      target.addEventListener('test', function() {
        calledListeners.push(name);
        console.log(`Listener '${name}' executed`);

        if (shouldThrow) {
          const error = new Error(`Error from ${name}`);
          thrownErrors.push(error);
          throw error;
        }
      });
    }

    addListener('first');
    addListener('second', true);  // throws
    addListener('third');
    addListener('fourth', true);  // throws
    addListener('fifth');

    target.dispatchEvent(new Event('test'));

    strictEqual(calledListeners.length, 5, 'All 5 listeners should be called');
    strictEqual(thrownErrors.length, 2, 'Exactly 2 errors should be thrown');

    deepStrictEqual(
      calledListeners,
      ['first', 'second', 'third', 'fourth', 'fifth'],
      'Listeners should be called in correct order despite exceptions'
    );

    strictEqual(
      thrownErrors[0].message,
      'Error from second',
      'First error should be from second listener'
    );

    strictEqual(
      thrownErrors[1].message,
      'Error from fourth',
      'Second error should be from fourth listener'
    );
  });

  await t.test('listener-removal-during-dispatch', async () => {
    const calledListeners = [];
    const target = new EventTarget();

    function secondListener() {
      calledListeners.push('second');
    }

    function fourthListener() {
      calledListeners.push('fourth');
    }

    // First listener removes two others
    target.addEventListener('test', function() {
      calledListeners.push('first');

      // Remove both second and fourth listeners
      target.removeEventListener('test', secondListener);
      target.removeEventListener('test', fourthListener);
    });

    target.addEventListener('test', secondListener);

    target.addEventListener('test', function() {
      calledListeners.push('third');
    });

    target.addEventListener('test', fourthListener);

    target.addEventListener('test', function() {
      calledListeners.push('fifth');
    });

    target.dispatchEvent(new Event('test'));

    strictEqual(calledListeners.length, 3, 'Only 3 out of 5 listeners should be called');

    deepStrictEqual(
      calledListeners,
      ['first', 'third', 'fifth'],
      'Second and fourth listeners should be skipped due to removal'
    );
  });
});
