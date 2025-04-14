import { serveTest } from '../test-server.js';
import { assert } from '../../assert.js';

export const handler = serveTest(({ test }) => {
  test('process.env exists', () => {
    assert(typeof process.env === 'object', 'process.env should be an object');
  });

  test('process.env has environment variables', () => {
    assert(typeof process.env.TEST_VAR === 'string', 'process.env.TEST_VAR should be a string');
    assert(process.env.TEST_VAR === 'test_value', 'process.env.TEST_VAR should have correct value');
    assert(typeof process.env.ANOTHER_VAR === 'string', 'process.env.ANOTHER_VAR should be a string');
    assert(process.env.ANOTHER_VAR === 'another_value', 'process.env.ANOTHER_VAR should have correct value');
  });

  test('process.env handles empty values', () => {
    assert(typeof process.env.EMPTY_VAR === 'string', 'process.env.EMPTY_VAR should be a string');
    assert(process.env.EMPTY_VAR === '', 'process.env.EMPTY_VAR should be empty string');
  });

  test('process.env is read-only', () => {
    const originalValue = process.env.TEST_VAR;
    try {
      process.env.TEST_VAR = 'new-value';
      assert(false, 'Should not be able to modify process.env');
    } catch (e) {
      assert(e instanceof TypeError, 'Should throw TypeError when modifying process.env');
    }
    assert(process.env.TEST_VAR === originalValue, 'process.env should not be modified');
  });
}); 