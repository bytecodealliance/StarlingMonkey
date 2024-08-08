import { serveTest } from "../test-server.js";
import { deepStrictEqual, strictEqual, throws, rejects } from "../../assert.js";

// From https://www.rfc-editor.org/rfc/rfc7517#appendix-A.1
const createPublicRsaJsonWebKeyData = () => ({
  alg: "RS256",
  e: "AQAB",
  ext: true,
  key_ops: ["verify"],
  kty: "RSA",
  n: "0vx7agoebGcQSuuPiLJXZptN9nndrQmbXEps2aiAFbWhM78LhWx4cbbfAAtVT86zwu1RK7aPFFxuhDR1L6tSoc_BJECPebWKRXjBZCiFV4n3oknjhMstn64tZ_2W-5JsGY4Hc5n9yBXArwl93lqt7_RN5w6Cf0h4QyQ5v-65YGjQR0_FDW2QvzqY368QQMicAtaSqzs8KJZgnYb9c7d0zgdAZHzu6qMQvRL5hajrn1n91CbOpbISD08qNLyrdkt-bFTWhAI4vMQFh6WeZu0fM4lFd2NcRwr3XPksINHaQ-G_xBniIqbw0Ls1jF44-csFCur-kEgU8awapJzKnqDKgw",
});

// From https://www.rfc-editor.org/rfc/rfc7517#appendix-A.1
const createPublicEcdsaJsonWebKeyData = () => ({
  kty: "EC",
  crv: "P-256",
  x: "MKBCTNIcKUSDii11ySs3526iDZ8AiTo7Tu6KPAqv7D4",
  y: "4Etl6SRW2YiLUrN5vfvVHuhp7x8PxltmWWlbbM4IFyM",
  kid: "1",
  ext: true,
  key_ops: ["verify"],
});

// From https://www.rfc-editor.org/rfc/rfc7517#appendix-A.2
const createPrivateEcdsaJsonWebKeyData = () => ({
  kty: "EC",
  crv: "P-256",
  x: "MKBCTNIcKUSDii11ySs3526iDZ8AiTo7Tu6KPAqv7D4",
  y: "4Etl6SRW2YiLUrN5vfvVHuhp7x8PxltmWWlbbM4IFyM",
  d: "870MB6gfuTJ4HtUnUvYMyJpr5eUZNP4Bk43bVdj3eAE",
  use: "sig",
  kid: "1",
  ext: true,
  key_ops: ["sign"],
});

// From https://www.rfc-editor.org/rfc/rfc7517#appendix-A.2
const createPrivateRsaJsonWebKeyData = () => ({
  alg: "RS256",
  d: "X4cTteJY_gn4FYPsXB8rdXix5vwsg1FLN5E3EaG6RJoVH-HLLKD9M7dx5oo7GURknchnrRweUkC7hT5fJLM0WbFAKNLWY2vv7B6NqXSzUvxT0_YSfqijwp3RTzlBaCxWp4doFk5N2o8Gy_nHNKroADIkJ46pRUohsXywbReAdYaMwFs9tv8d_cPVY3i07a3t8MN6TNwm0dSawm9v47UiCl3Sk5ZiG7xojPLu4sbg1U2jx4IBTNBznbJSzFHK66jT8bgkuqsk0GjskDJk19Z4qwjwbsnn4j2WBii3RL-Us2lGVkY8fkFzme1z0HbIkfz0Y6mqnOYtqc0X4jfcKoAC8Q",
  dp: "G4sPXkc6Ya9y8oJW9_ILj4xuppu0lzi_H7VTkS8xj5SdX3coE0oimYwxIi2emTAue0UOa5dpgFGyBJ4c8tQ2VF402XRugKDTP8akYhFo5tAA77Qe_NmtuYZc3C3m3I24G2GvR5sSDxUyAN2zq8Lfn9EUms6rY3Ob8YeiKkTiBj0",
  dq: "s9lAH9fggBsoFR8Oac2R_E2gw282rT2kGOAhvIllETE1efrA6huUUvMfBcMpn8lqeW6vzznYY5SSQF7pMdC_agI3nG8Ibp1BUb0JUiraRNqUfLhcQb_d9GF4Dh7e74WbRsobRonujTYN1xCaP6TO61jvWrX-L18txXw494Q_cgk",
  e: "AQAB",
  ext: true,
  key_ops: ["sign"],
  kty: "RSA",
  n: "0vx7agoebGcQSuuPiLJXZptN9nndrQmbXEps2aiAFbWhM78LhWx4cbbfAAtVT86zwu1RK7aPFFxuhDR1L6tSoc_BJECPebWKRXjBZCiFV4n3oknjhMstn64tZ_2W-5JsGY4Hc5n9yBXArwl93lqt7_RN5w6Cf0h4QyQ5v-65YGjQR0_FDW2QvzqY368QQMicAtaSqzs8KJZgnYb9c7d0zgdAZHzu6qMQvRL5hajrn1n91CbOpbISD08qNLyrdkt-bFTWhAI4vMQFh6WeZu0fM4lFd2NcRwr3XPksINHaQ-G_xBniIqbw0Ls1jF44-csFCur-kEgU8awapJzKnqDKgw",
  p: "83i-7IvMGXoMXCskv73TKr8637FiO7Z27zv8oj6pbWUQyLPQBQxtPVnwD20R-60eTDmD2ujnMt5PoqMrm8RfmNhVWDtjjMmCMjOpSXicFHj7XOuVIYQyqVWlWEh6dN36GVZYk93N8Bc9vY41xy8B9RzzOGVQzXvNEvn7O0nVbfs",
  q: "3dfOR9cuYq-0S-mkFLzgItgMEfFzB2q3hWehMuG0oCuqnb3vobLyumqjVZQO1dIrdwgTnCdpYzBcOfW5r370AFXjiWft_NGEiovonizhKpo9VVS78TzFgxkIdrecRezsZ-1kYd_s1qDbxtkDEgfAITAG9LUnADun4vIcb6yelxk",
  qi: "GyM_p6JrXySiz1toFgKbWV-JdI3jQ4ypu9rbMWx3rQJBfmt0FoYzgUIZEVFEcOqwemRN81zoDAaa-Bk0KWNGDjJHZDdDmFhW3AN7lI-puxk_mHZGJ11rxyR8O55XLSe3SPmRfKwZI6yU24ZxvQKFYItdldUKGzO6Ia6zTKhAVRU",
});

const createRsaJsonWebKeyAlgorithm = () => ({
  name: "RSASSA-PKCS1-v1_5",
  hash: { name: "SHA-256" },
});

const ecdsaJsonWebKeyAlgorithm = Object.freeze({
  name: "ECDSA",
  namedCurve: "P-256",
  hash: Object.freeze({ name: "SHA-256" }),
});

export const handler = serveTest(async (t) => {
  t.test("crypto", () => {
    strictEqual(typeof crypto, "object", `typeof crypto`);
    strictEqual(crypto instanceof Crypto, true, `crypto instanceof Crypto`);
  });

  await t.test("subtle", async () => {
    strictEqual(typeof crypto.subtle, "object", `typeof crypto.subtle`);
    strictEqual(
      crypto.subtle instanceof SubtleCrypto,
      true,
      `crypto.subtle instanceof SubtleCrypto`
    );
  });

  // importKey
  {
    await t.test("subtle.importKey", async () => {
      strictEqual(
        typeof crypto.subtle.importKey,
        "function",
        `typeof crypto.subtle.importKey`
      );
      strictEqual(
        crypto.subtle.importKey,
        SubtleCrypto.prototype.importKey,
        `crypto.subtle.importKey === SubtleCrypto.prototype.importKey`
      );
    });
    await t.test("subtle.importKey.length", async () => {
      strictEqual(
        crypto.subtle.importKey.length,
        5,
        `crypto.subtle.importKey.length === 5`
      );
    });
    await t.test("subtle.importKey.called-as-constructor", async () => {
      throws(
        () => {
          new crypto.subtle.importKey();
        },
        TypeError
      );
    });
    await t.test("subtle.importKey.called-with-wrong-this", async () => {
      const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
      await rejects(
        async () => {
          await crypto.subtle.importKey.call(
            undefined,
            "jwk",
            publicRsaJsonWebKeyData,
            createRsaJsonWebKeyAlgorithm(),
            publicRsaJsonWebKeyData.ext,
            publicRsaJsonWebKeyData.key_ops
          );
        },
        TypeError
      );
    });
    await t.test("subtle.importKey.called-with-no-arguments", async () => {
      await rejects(
        async () => {
          await crypto.subtle.importKey();
        },
        TypeError
      );
    });

    // first-parameter
    {
      await t.test(
        "subtle.importKey.first-parameter-calls-7.1.17-ToString",
        async () => {
          const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
          const sentinel = Symbol("sentinel");
          const test = async () => {
            const format = {
              toString() {
                throw sentinel;
              },
            };
            await crypto.subtle.importKey(
              format,
              publicRsaJsonWebKeyData,
              createRsaJsonWebKeyAlgorithm(),
              publicRsaJsonWebKeyData.ext,
              publicRsaJsonWebKeyData.key_ops
            );
          };
          await rejects(test);
          try {
            await test();
          } catch (thrownError) {
            strictEqual(thrownError, sentinel, "thrownError === sentinel");
          }
        }
      );
      await t.test(
        "subtle.importKey.first-parameter-non-existent-format",
        async () => {
          const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
          await rejects(
            async () => {
              await crypto.subtle.importKey(
                "jake",
                publicRsaJsonWebKeyData,
                createRsaJsonWebKeyAlgorithm(),
                publicRsaJsonWebKeyData.ext,
                publicRsaJsonWebKeyData.key_ops
              );
            },
            DOMException, null, "NotSupportedError"
          );
        }
      );
    }

    // second-parameter
    {
      await t.test(
        "subtle.importKey.second-parameter-invalid-format",
        async () => {
          const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
          await rejects(
            async () => {
              await crypto.subtle.importKey(
                "jwk",
                Symbol(),
                createRsaJsonWebKeyAlgorithm(),
                publicRsaJsonWebKeyData.ext,
                publicRsaJsonWebKeyData.key_ops
              );
            },
            TypeError
          );
        }
      );
      // jwk public key
      {
        await t.test(
          "subtle.importKey.rsa-jwk-public.second-parameter-missing-e-field",
          async () => {
            const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
            await rejects(
              async () => {
                delete publicRsaJsonWebKeyData.e;
                await crypto.subtle.importKey(
                  "jwk",
                  publicRsaJsonWebKeyData,
                  createRsaJsonWebKeyAlgorithm(),
                  publicRsaJsonWebKeyData.ext,
                  publicRsaJsonWebKeyData.key_ops
                );
              },
              DOMException, null, "DataError"
            );
          }
        );
        await t.test(
          "subtle.importKey.rsa-jwk-public.second-parameter-e-field-calls-7.1.17-ToString",
          async () => {
            const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
            let sentinel = Symbol("sentinel");
            const test = async () => {
              sentinel = Symbol();
              publicRsaJsonWebKeyData.e = {
                toString() {
                  throw sentinel;
                },
              };
              await crypto.subtle.importKey(
                "jwk",
                publicRsaJsonWebKeyData,
                createRsaJsonWebKeyAlgorithm(),
                publicRsaJsonWebKeyData.ext,
                publicRsaJsonWebKeyData.key_ops
              );
            };
            await rejects(test);
            try {
              await test();
            } catch (thrownError) {
              strictEqual(thrownError, sentinel, "thrownError === sentinel");
            }
          }
        );
        await t.test(
          "subtle.importKey.rsa-jwk-public.second-parameter-invalid-e-field",
          async () => {
            const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
            await rejects(
              async () => {
                publicRsaJsonWebKeyData.e = "`~!@#@#$Q%^%&^*";
                await crypto.subtle.importKey(
                  "jwk",
                  publicRsaJsonWebKeyData,
                  createRsaJsonWebKeyAlgorithm(),
                  publicRsaJsonWebKeyData.ext,
                  publicRsaJsonWebKeyData.key_ops
                );
              },
              DOMException, null, "DataError"
            );
          }
        );
        await t.test(
          "subtle.importKey.rsa-jwk-public.second-parameter-missing-kty-field",
          async () => {
            const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
            await rejects(
              async () => {
                delete publicRsaJsonWebKeyData.kty;
                await crypto.subtle.importKey(
                  "jwk",
                  publicRsaJsonWebKeyData,
                  createRsaJsonWebKeyAlgorithm(),
                  publicRsaJsonWebKeyData.ext,
                  publicRsaJsonWebKeyData.key_ops
                );
              },
              TypeError
            );
          }
        );
        await t.test(
          "subtle.importKey.rsa-jwk-public.second-parameter-invalid-kty-field",
          async () => {
            const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
            await rejects(
              async () => {
                publicRsaJsonWebKeyData.kty = "jake";
                await crypto.subtle.importKey(
                  "jwk",
                  publicRsaJsonWebKeyData,
                  createRsaJsonWebKeyAlgorithm(),
                  publicRsaJsonWebKeyData.ext,
                  publicRsaJsonWebKeyData.key_ops
                );
              },
              DOMException, null, "DataError"
            );
          }
        );
        await t.test(
          "subtle.importKey.rsa-jwk-public.second-parameter-missing-key_ops-field",
          async () => {
            const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
            const key_ops = Array.from(publicRsaJsonWebKeyData.key_ops);
            delete publicRsaJsonWebKeyData.key_ops;
            await crypto.subtle.importKey(
              "jwk",
              publicRsaJsonWebKeyData,
              createRsaJsonWebKeyAlgorithm(),
              publicRsaJsonWebKeyData.ext,
              key_ops
            );
          }
        );
        await t.test(
          "subtle.importKey.rsa-jwk-public.second-parameter-non-sequence-key_ops-field",
          async () => {
            const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
            await rejects(
              async () => {
                const key_ops = Array.from(publicRsaJsonWebKeyData.key_ops);
                publicRsaJsonWebKeyData.key_ops = "jake";
                await crypto.subtle.importKey(
                  "jwk",
                  publicRsaJsonWebKeyData,
                  createRsaJsonWebKeyAlgorithm(),
                  publicRsaJsonWebKeyData.ext,
                  key_ops
                );
              },
              DOMException, null, "DataError"
            );
          }
        );

        await t.test(
          "subtle.importKey.rsa-jwk-public.second-parameter-empty-key_ops-field",
          async () => {
            const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
            const key_ops = Array.from(publicRsaJsonWebKeyData.key_ops);
            publicRsaJsonWebKeyData.key_ops = [];
            await crypto.subtle.importKey(
              "jwk",
              publicRsaJsonWebKeyData,
              createRsaJsonWebKeyAlgorithm(),
              publicRsaJsonWebKeyData.ext,
              key_ops
            );
          }
        );
        await t.test(
          "subtle.importKey.rsa-jwk-public.second-parameter-duplicated-key_ops-field",
          async () => {
            const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
            await rejects(
              async () => {
                const key_ops = Array.from(publicRsaJsonWebKeyData.key_ops);
                publicRsaJsonWebKeyData.key_ops = ["sign", "sign"];
                await crypto.subtle.importKey(
                  "jwk",
                  publicRsaJsonWebKeyData,
                  createRsaJsonWebKeyAlgorithm(),
                  publicRsaJsonWebKeyData.ext,
                  key_ops
                );
              },
                DOMException, null, "DataError"
            );
          }
        );
        await t.test(
          "subtle.importKey.rsa-jwk-public.second-parameter-invalid-key_ops-field",
          async () => {
            const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
            await rejects(
              async () => {
                const key_ops = Array.from(publicRsaJsonWebKeyData.key_ops);
                publicRsaJsonWebKeyData.key_ops = ["sign", "jake"];
                await crypto.subtle.importKey(
                  "jwk",
                  publicRsaJsonWebKeyData,
                  createRsaJsonWebKeyAlgorithm(),
                  publicRsaJsonWebKeyData.ext,
                  key_ops
                );
              },
                DOMException, null, "DataError"
            );
          }
        );

        await t.test(
          "subtle.importKey.rsa-jwk-public.second-parameter-key_ops-field-calls-7.1.17-ToString",
          async () => {
            const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
            let sentinel = Symbol("sentinel");
            const key_ops = Array.from(publicRsaJsonWebKeyData.key_ops);
            const test = async () => {
              sentinel = Symbol();
              const op = {
                toString() {
                  throw sentinel;
                },
              };
              publicRsaJsonWebKeyData.key_ops = ["sign", op];
              await crypto.subtle.importKey(
                "jwk",
                publicRsaJsonWebKeyData,
                createRsaJsonWebKeyAlgorithm(),
                publicRsaJsonWebKeyData.ext,
                key_ops
              );
            };
            await rejects(test);
            try {
              await test();
            } catch (thrownError) {
              strictEqual(thrownError, sentinel, "thrownError === sentinel");
            }
          }
        );

        await t.test(
          "subtle.importKey.rsa-jwk-public.second-parameter-missing-n-field",
          async () => {
            const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
            await rejects(
              async () => {
                delete publicRsaJsonWebKeyData.n;
                await crypto.subtle.importKey(
                  "jwk",
                  publicRsaJsonWebKeyData,
                  createRsaJsonWebKeyAlgorithm(),
                  publicRsaJsonWebKeyData.ext,
                  publicRsaJsonWebKeyData.key_ops
                );
              },
              DOMException, null, "DataError"
            );
          }
        );
        await t.test(
          "subtle.importKey.rsa-jwk-public.second-parameter-n-field-calls-7.1.17-ToString",
          async () => {
            const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
            let sentinel = Symbol("sentinel");
            const test = async () => {
              sentinel = Symbol();
              publicRsaJsonWebKeyData.n = {
                toString() {
                  throw sentinel;
                },
              };
              await crypto.subtle.importKey(
                "jwk",
                publicRsaJsonWebKeyData,
                createRsaJsonWebKeyAlgorithm(),
                publicRsaJsonWebKeyData.ext,
                publicRsaJsonWebKeyData.key_ops
              );
            };
            await rejects(test);
            try {
              await test();
            } catch (thrownError) {
              strictEqual(thrownError, sentinel, "thrownError === sentinel");
            }
          }
        );
        await t.test(
          "subtle.importKey.rsa-jwk-public.second-parameter-invalid-n-field",
          async () => {
            const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
            await rejects(
              async () => {
                publicRsaJsonWebKeyData.n = "`~!@#@#$Q%^%&^*";
                await crypto.subtle.importKey(
                  "jwk",
                  publicRsaJsonWebKeyData,
                  createRsaJsonWebKeyAlgorithm(),
                  publicRsaJsonWebKeyData.ext,
                  publicRsaJsonWebKeyData.key_ops
                );
              },
              DOMException, null, "DataError"
            );
          }
        );
      }
      // jwk private key
      // TODO
      // raw HMAC secret keys
      // TODO
      // raw Elliptic Curve public keys
      {
        await t.test(
          "subtle.importKey.ecdsa-jwk-public.second-parameter-missing-x-field",
          async () => {
            const publicEcdsaJsonWebKeyData = createPublicEcdsaJsonWebKeyData();
            await rejects(
              async () => {
                delete publicEcdsaJsonWebKeyData.x;
                await crypto.subtle.importKey(
                  "jwk",
                  publicEcdsaJsonWebKeyData,
                  ecdsaJsonWebKeyAlgorithm,
                  publicEcdsaJsonWebKeyData.ext,
                  publicEcdsaJsonWebKeyData.key_ops
                );
              },
              DOMException, null, "DataError"
            );
          }
        );
        await t.test(
          "subtle.importKey.ecdsa-jwk-public.second-parameter-x-field-calls-7.1.17-ToString",
          async () => {
            const publicEcdsaJsonWebKeyData = createPublicEcdsaJsonWebKeyData();
            let sentinel = Symbol("sentinel");
            const test = async () => {
              sentinel = Symbol();
              publicEcdsaJsonWebKeyData.x = {
                toString() {
                  throw sentinel;
                },
              };
              await crypto.subtle.importKey(
                "jwk",
                publicEcdsaJsonWebKeyData,
                ecdsaJsonWebKeyAlgorithm,
                publicEcdsaJsonWebKeyData.ext,
                publicEcdsaJsonWebKeyData.key_ops
              );
            };
            await rejects(test);
            try {
              await test();
            } catch (thrownError) {
              strictEqual(thrownError, sentinel, "thrownError === sentinel");
            }
          }
        );
        await t.test(
          "subtle.importKey.ecdsa-jwk-public.second-parameter-invalid-x-field",
          async () => {
            const publicEcdsaJsonWebKeyData = createPublicEcdsaJsonWebKeyData();
            await rejects(
              async () => {
                publicEcdsaJsonWebKeyData.x = "`~!@#@#$Q%^%&^*";
                await crypto.subtle.importKey(
                  "jwk",
                  publicEcdsaJsonWebKeyData,
                  ecdsaJsonWebKeyAlgorithm,
                  publicEcdsaJsonWebKeyData.ext,
                  publicEcdsaJsonWebKeyData.key_ops
                );
              },
              DOMException, null, "DataError"
            );
          }
        );
        await t.test(
          "subtle.importKey.ecdsa-jwk-public.second-parameter-missing-y-field",
          async () => {
            const publicEcdsaJsonWebKeyData = createPublicEcdsaJsonWebKeyData();
            await rejects(
              async () => {
                delete publicEcdsaJsonWebKeyData.y;
                await crypto.subtle.importKey(
                  "jwk",
                  publicEcdsaJsonWebKeyData,
                  ecdsaJsonWebKeyAlgorithm,
                  publicEcdsaJsonWebKeyData.ext,
                  publicEcdsaJsonWebKeyData.key_ops
                );
              },
              DOMException, null, "DataError"
            );
          }
        );
        await t.test(
          "subtle.importKey.ecdsa-jwk-public.second-parameter-y-field-calls-7.1.17-ToString",
          async () => {
            const publicEcdsaJsonWebKeyData = createPublicEcdsaJsonWebKeyData();
            let sentinel = Symbol("sentinel");
            const test = async () => {
              sentinel = Symbol();
              publicEcdsaJsonWebKeyData.y = {
                toString() {
                  throw sentinel;
                },
              };
              await crypto.subtle.importKey(
                "jwk",
                publicEcdsaJsonWebKeyData,
                ecdsaJsonWebKeyAlgorithm,
                publicEcdsaJsonWebKeyData.ext,
                publicEcdsaJsonWebKeyData.key_ops
              );
            };
            await rejects(test);
            try {
              await test();
            } catch (thrownError) {
              strictEqual(thrownError, sentinel, "thrownError === sentinel");
            }
          }
        );
        await t.test(
          "subtle.importKey.ecdsa-jwk-public.second-parameter-invalid-y-field",
          async () => {
            const publicEcdsaJsonWebKeyData = createPublicEcdsaJsonWebKeyData();
            await rejects(
              async () => {
                publicEcdsaJsonWebKeyData.y = "`~!@#@#$Q%^%&^*";
                await crypto.subtle.importKey(
                  "jwk",
                  publicEcdsaJsonWebKeyData,
                  ecdsaJsonWebKeyAlgorithm,
                  publicEcdsaJsonWebKeyData.ext,
                  publicEcdsaJsonWebKeyData.key_ops
                );
              },
              DOMException, null, "DataError"
            );
          }
        );
        await t.test(
          "subtle.importKey.ecdsa-jwk-public.second-parameter-missing-kty-field",
          async () => {
            const publicEcdsaJsonWebKeyData = createPublicEcdsaJsonWebKeyData();
            await rejects(
              async () => {
                delete publicEcdsaJsonWebKeyData.kty;
                await crypto.subtle.importKey(
                  "jwk",
                  publicEcdsaJsonWebKeyData,
                  ecdsaJsonWebKeyAlgorithm,
                  publicEcdsaJsonWebKeyData.ext,
                  publicEcdsaJsonWebKeyData.key_ops
                );
              },
              TypeError
            );
          }
        );
        await t.test(
          "subtle.importKey.ecdsa-jwk-public.second-parameter-invalid-kty-field",
          async () => {
            const publicEcdsaJsonWebKeyData = createPublicEcdsaJsonWebKeyData();
            await rejects(
              async () => {
                publicEcdsaJsonWebKeyData.kty = "jake";
                await crypto.subtle.importKey(
                  "jwk",
                  publicEcdsaJsonWebKeyData,
                  ecdsaJsonWebKeyAlgorithm,
                  publicEcdsaJsonWebKeyData.ext,
                  publicEcdsaJsonWebKeyData.key_ops
                );
              },
              DOMException, null, "DataError"
            );
          }
        );
        await t.test(
          "subtle.importKey.ecdsa-jwk-public.second-parameter-missing-key_ops-field",
          async () => {
            const publicEcdsaJsonWebKeyData = createPublicEcdsaJsonWebKeyData();
            const key_ops = Array.from(publicEcdsaJsonWebKeyData.key_ops);
            delete publicEcdsaJsonWebKeyData.key_ops;
            await crypto.subtle.importKey(
              "jwk",
              publicEcdsaJsonWebKeyData,
              ecdsaJsonWebKeyAlgorithm,
              publicEcdsaJsonWebKeyData.ext,
              key_ops
            );
          }
        );
        await t.test(
          "subtle.importKey.ecdsa-jwk-public.second-parameter-non-sequence-key_ops-field",
          async () => {
            const publicEcdsaJsonWebKeyData = createPublicEcdsaJsonWebKeyData();
            await rejects(
              async () => {
                const key_ops = Array.from(publicEcdsaJsonWebKeyData.key_ops);
                publicEcdsaJsonWebKeyData.key_ops = "jake";
                await crypto.subtle.importKey(
                  "jwk",
                  publicEcdsaJsonWebKeyData,
                  ecdsaJsonWebKeyAlgorithm,
                  publicEcdsaJsonWebKeyData.ext,
                  key_ops
                );
              },
              DOMException, null, "DataError"
            );
          }
        );

        await t.test(
          "subtle.importKey.ecdsa-jwk-public.second-parameter-empty-key_ops-field",
          async () => {
            const publicEcdsaJsonWebKeyData = createPublicEcdsaJsonWebKeyData();
            const key_ops = Array.from(publicEcdsaJsonWebKeyData.key_ops);
            publicEcdsaJsonWebKeyData.key_ops = [];
            await crypto.subtle.importKey(
              "jwk",
              publicEcdsaJsonWebKeyData,
              ecdsaJsonWebKeyAlgorithm,
              publicEcdsaJsonWebKeyData.ext,
              key_ops
            );
          }
        );
        await t.test(
          "subtle.importKey.ecdsa-jwk-public.second-parameter-duplicated-key_ops-field",
          async () => {
            const publicEcdsaJsonWebKeyData = createPublicEcdsaJsonWebKeyData();
            await rejects(
              async () => {
                const key_ops = Array.from(publicEcdsaJsonWebKeyData.key_ops);
                publicEcdsaJsonWebKeyData.key_ops = ["sign", "sign"];
                await crypto.subtle.importKey(
                  "jwk",
                  publicEcdsaJsonWebKeyData,
                  ecdsaJsonWebKeyAlgorithm,
                  publicEcdsaJsonWebKeyData.ext,
                  key_ops
                );
              },
              DOMException, null, "DataError"
            );
          }
        );
        await t.test(
          "subtle.importKey.ecdsa-jwk-public.second-parameter-invalid-key_ops-field",
          async () => {
            const publicEcdsaJsonWebKeyData = createPublicEcdsaJsonWebKeyData();
            await rejects(
              async () => {
                const key_ops = Array.from(publicEcdsaJsonWebKeyData.key_ops);
                publicEcdsaJsonWebKeyData.key_ops = ["sign", "jake"];
                await crypto.subtle.importKey(
                  "jwk",
                  publicEcdsaJsonWebKeyData,
                  ecdsaJsonWebKeyAlgorithm,
                  publicEcdsaJsonWebKeyData.ext,
                  key_ops
                );
              },
              DOMException, null, "DataError"
            );
          }
        );

        await t.test(
          "subtle.importKey.ecdsa-jwk-public.second-parameter-key_ops-field-calls-7.1.17-ToString",
          async () => {
            const publicEcdsaJsonWebKeyData = createPublicEcdsaJsonWebKeyData();
            let sentinel = Symbol("sentinel");
            const key_ops = Array.from(publicEcdsaJsonWebKeyData.key_ops);
            const test = async () => {
              sentinel = Symbol();
              const op = {
                toString() {
                  throw sentinel;
                },
              };
              publicEcdsaJsonWebKeyData.key_ops = ["sign", op];
              await crypto.subtle.importKey(
                "jwk",
                publicEcdsaJsonWebKeyData,
                ecdsaJsonWebKeyAlgorithm,
                publicEcdsaJsonWebKeyData.ext,
                key_ops
              );
            };
            await rejects(test);
            try {
              await test();
            } catch (thrownError) {
              strictEqual(thrownError, sentinel, "thrownError === sentinel");
            }
          }
        );

        await t.test(
          "subtle.importKey.ecdsa-jwk-private.second-parameter-d-field-calls-7.1.17-ToString",
          async () => {
            const privateEcdsaJsonWebKeyData =
              createPrivateEcdsaJsonWebKeyData();
            let sentinel = Symbol("sentinel");
            const test = async () => {
              sentinel = Symbol();
              privateEcdsaJsonWebKeyData.d = {
                toString() {
                  throw sentinel;
                },
              };
              await crypto.subtle.importKey(
                "jwk",
                privateEcdsaJsonWebKeyData,
                ecdsaJsonWebKeyAlgorithm,
                privateEcdsaJsonWebKeyData.ext,
                privateEcdsaJsonWebKeyData.key_ops
              );
            };
            await rejects(test);
            try {
              await test();
            } catch (thrownError) {
              strictEqual(thrownError, sentinel, "thrownError === sentinel");
            }
          }
        );
        await t.test(
          "subtle.importKey.ecdsa-jwk-private.second-parameter-invalid-d-field",
          async () => {
            const privateEcdsaJsonWebKeyData =
              createPrivateEcdsaJsonWebKeyData();
            await rejects(
              async () => {
                privateEcdsaJsonWebKeyData.d = "`~!@#@#$Q%^%&^*";
                await crypto.subtle.importKey(
                  "jwk",
                  privateEcdsaJsonWebKeyData,
                  ecdsaJsonWebKeyAlgorithm,
                  privateEcdsaJsonWebKeyData.ext,
                  privateEcdsaJsonWebKeyData.key_ops
                );
              },
              DOMException, null, "DataError"
            );
          }
        );
      }
      // pkcs8 Elliptic Curve private keys
      // TODO
      // pkcs8 RSA private keys
      // raw AES
    }
    // third-parameter
    {
      await t.test("subtle.importKey.third-parameter-undefined", async () => {
        const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
        await rejects(
          async () => {
            await crypto.subtle.importKey(
              "jwk",
              publicRsaJsonWebKeyData,
              undefined,
              publicRsaJsonWebKeyData.ext,
              publicRsaJsonWebKeyData.key_ops
            );
          },
          DOMException, null, "NotSupportedError"
        );
      });
      await t.test(
        "subtle.importKey.third-parameter-name-field-calls-7.1.17-ToString",
        async () => {
          const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
          const rsaJsonWebKeyAlgorithm = createRsaJsonWebKeyAlgorithm();
          const sentinel = Symbol("sentinel");
          const test = async () => {
            rsaJsonWebKeyAlgorithm.name = {
              toString() {
                throw sentinel;
              },
            };
            await crypto.subtle.importKey(
              "jwk",
              publicRsaJsonWebKeyData,
              rsaJsonWebKeyAlgorithm,
              publicRsaJsonWebKeyData.ext,
              publicRsaJsonWebKeyData.key_ops
            );
          };
          await rejects(test);
          try {
            await test();
          } catch (thrownError) {
            strictEqual(thrownError, sentinel, "thrownError === sentinel");
          }
        }
      );
      await t.test(
        "subtle.importKey.third-parameter-invalid-name-field",
        async () => {
          const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
          const rsaJsonWebKeyAlgorithm = createRsaJsonWebKeyAlgorithm();
          await rejects(
            async () => {
              rsaJsonWebKeyAlgorithm.name = "`~!@#@#$Q%^%&^*";
              await crypto.subtle.importKey(
                "jwk",
                publicRsaJsonWebKeyData,
                rsaJsonWebKeyAlgorithm,
                publicRsaJsonWebKeyData.ext,
                publicRsaJsonWebKeyData.key_ops
              );
            },
            DOMException, null, "NotSupportedError"
          );
        }
      );
      await t.test(
        "subtle.importKey.third-parameter-hash-name-field-calls-7.1.17-ToString",
        async () => {
          const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
          const rsaJsonWebKeyAlgorithm = createRsaJsonWebKeyAlgorithm();
          const sentinel = Symbol("sentinel");
          const test = async () => {
            rsaJsonWebKeyAlgorithm.hash.name = {
              toString() {
                throw sentinel;
              },
            };
            await crypto.subtle.importKey(
              "jwk",
              publicRsaJsonWebKeyData,
              rsaJsonWebKeyAlgorithm,
              publicRsaJsonWebKeyData.ext,
              publicRsaJsonWebKeyData.key_ops
            );
          };
          await rejects(test);
          try {
            await test();
          } catch (thrownError) {
            strictEqual(thrownError, sentinel, "thrownError === sentinel");
          }
        }
      );
      await t.test(
        "subtle.importKey.third-parameter-hash-algorithm-does-not-match-json-web-key-hash-algorithm",
        async () => {
          const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
          const rsaJsonWebKeyAlgorithm = createRsaJsonWebKeyAlgorithm();
          await rejects(
            async () => {
              rsaJsonWebKeyAlgorithm.hash.name = "SHA-1";
              await crypto.subtle.importKey(
                "jwk",
                publicRsaJsonWebKeyData,
                rsaJsonWebKeyAlgorithm,
                publicRsaJsonWebKeyData.ext,
                publicRsaJsonWebKeyData.key_ops
              );
            },
            DOMException, null, "DataError"
          );
        }
      );
    }

    // fifth-parameter
    {
      await t.test("subtle.importKey.fifth-parameter-undefined", async () => {
        const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
        await rejects(
          async () => {
            await crypto.subtle.importKey(
              "jwk",
              publicRsaJsonWebKeyData,
              createRsaJsonWebKeyAlgorithm(),
              publicRsaJsonWebKeyData.ext,
              undefined
            );
          },
          TypeError
        );
      });
      await t.test("subtle.importKey.fifth-parameter-invalid", async () => {
        const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
        await rejects(
          async () => {
            await crypto.subtle.importKey(
              "jwk",
              publicRsaJsonWebKeyData,
              createRsaJsonWebKeyAlgorithm(),
              publicRsaJsonWebKeyData.ext,
              ["jake"]
            );
          },
          TypeError
        );
      });
      await t.test(
        "subtle.importKey.fifth-parameter-duplicate-operations",
        async () => {
          const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
          const key_ops = publicRsaJsonWebKeyData.key_ops.concat(
            publicRsaJsonWebKeyData.key_ops
          );
          await crypto.subtle.importKey(
            "jwk",
            publicRsaJsonWebKeyData,
            createRsaJsonWebKeyAlgorithm(),
            publicRsaJsonWebKeyData.ext,
            key_ops
          );
        }
      );

      await t.test(
        "subtle.importKey.fifth-parameter-operations-do-not-match-json-web-key-operations",
        async () => {
          const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
          await rejects(
            async () => {
              await crypto.subtle.importKey(
                "jwk",
                publicRsaJsonWebKeyData,
                createRsaJsonWebKeyAlgorithm(),
                publicRsaJsonWebKeyData.ext,
                ["sign"]
              );
            },
            DOMException, null, "DataError"
          );
        }
      );

      await t.test(
        "subtle.importKey.fifth-parameter-operation-fields-calls-7.1.17-ToString",
        async () => {
          const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
          let sentinel = Symbol("sentinel");
          const test = async () => {
            sentinel = Symbol();
            const op = {
              toString() {
                throw sentinel;
              },
            };
            await crypto.subtle.importKey(
              "jwk",
              publicRsaJsonWebKeyData,
              createRsaJsonWebKeyAlgorithm(),
              publicRsaJsonWebKeyData.ext,
              ["sign", op]
            );
          };
          await rejects(test);
          try {
            await test();
          } catch (thrownError) {
            strictEqual(thrownError, sentinel, "thrownError === sentinel");
          }
        }
      );
    }

    // happy paths
    {
      await t.test("subtle.importKey.JWK-RS256-Public", async () => {
        const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
        const key = await crypto.subtle.importKey(
          "jwk",
          publicRsaJsonWebKeyData,
          createRsaJsonWebKeyAlgorithm(),
          publicRsaJsonWebKeyData.ext,
          publicRsaJsonWebKeyData.key_ops
        );
        strictEqual(key instanceof CryptoKey, true, `key instanceof CryptoKey`);
        deepStrictEqual(
          key.algorithm,
          {
            name: "RSASSA-PKCS1-v1_5",
            hash: {
              name: "SHA-256",
            },
            modulusLength: 2048,
            publicExponent: new Uint8Array([1, 0, 1]),
          },
          `key.algorithm`
        );
        strictEqual(key.extractable, true, `key.extractable === true`);
        strictEqual(key.type, "public", `key.type === "public"`);
        deepStrictEqual(
          key.usages,
          ["verify"],
          `key.usages deep equals ["verify"]`
        );
      });

      await t.test("subtle.importKey.JWK-EC256-Public", async () => {
        const publicEcdsaJsonWebKeyData = createPublicEcdsaJsonWebKeyData();
        const key = await crypto.subtle.importKey(
          "jwk",
          publicEcdsaJsonWebKeyData,
          ecdsaJsonWebKeyAlgorithm,
          publicEcdsaJsonWebKeyData.ext,
          publicEcdsaJsonWebKeyData.key_ops
        );
        strictEqual(key instanceof CryptoKey, true, `key instanceof CryptoKey`);
        deepStrictEqual(
          key.algorithm,
          {
            name: "ECDSA",
            namedCurve: "P-256",
          },
          `key.algorithm`
        );
        strictEqual(key.extractable, true, `key.extractable === true`);
        strictEqual(key.type, "public", `key.type === "public"`);
        deepStrictEqual(
          key.usages,
          ["verify"],
          `key.usages deep equals ["verify"]`
        );
      });

      await t.test("subtle.importKey.HMAC", async () => {
        const keyUint8Array = new Uint8Array([1, 0, 1]);

        for (const algorithm of ["SHA-1", "SHA-256", "SHA-384", "SHA-512"]) {
          const key = await globalThis.crypto.subtle.importKey(
            "raw",
            keyUint8Array,
            { name: "HMAC", hash: algorithm },
            false,
            ["sign", "verify"]
          );
          strictEqual(
            key instanceof CryptoKey,
            true,
            `key instanceof CryptoKey`
          );
          deepStrictEqual(
            key.algorithm,
            {
              name: "HMAC",
              hash: { name: algorithm },
              length: 24,
            },
            `key.algorithm`
          );
          strictEqual(key.extractable, false, `key.extractable`);
          strictEqual(key.type, "secret", `key.type`);
          deepStrictEqual(key.usages, ["sign", "verify"], `key.usages`);
        }
      });
      await t.test("subtle.importKey.JWK-HS256-Public", async () => {
        const key = await crypto.subtle.importKey(
          "jwk",
          {
            kty: "oct",
            k: "Y0zt37HgOx-BY7SQjYVmrqhPkO44Ii2Jcb9yydUDPfE",
            alg: "HS256",
            ext: true,
          },
          {
            name: "HMAC",
            hash: { name: "SHA-256" },
          },
          false,
          ["sign", "verify"]
        );
        strictEqual(key instanceof CryptoKey, true, `key instanceof CryptoKey`);
        deepStrictEqual(
          key.algorithm,
          {
            name: "HMAC",
            hash: { name: "SHA-256" },
            length: 256,
          },
          `key.algorithm`
        );
        strictEqual(key.extractable, false, `key.extractable`);
        strictEqual(key.type, "secret", `key.type`);
        deepStrictEqual(key.usages, ["sign", "verify"], `key.usages`);
      });
    }
  }

  // digest
  {
    const enc = new TextEncoder();
    const data = enc.encode("hello world");
    await t.test("subtle.digest", async () => {
      strictEqual(
        typeof crypto.subtle.digest,
        "function",
        `typeof crypto.subtle.digest`
      );
      strictEqual(
        crypto.subtle.digest,
        SubtleCrypto.prototype.digest,
        `crypto.subtle.digest === SubtleCrypto.prototype.digest`
      );
    });
    await t.test("subtle.digest.length", async () => {
      strictEqual(
        crypto.subtle.digest.length,
        2,
        `crypto.subtle.digest.length === 2`
      );
    });
    await t.test("subtle.digest.called-as-constructor", async () => {
      throws(
        () => {
          new crypto.subtle.digest();
        },
        TypeError
      );
    });
    await t.test("subtle.digest.called-with-wrong-this", async () => {
      await rejects(
        async () => {
          await crypto.subtle.digest.call(undefined);
        },
        TypeError,
      );
    });
    await t.test("subtle.digest.called-with-no-arguments", async () => {
      await rejects(
        async () => {
          await crypto.subtle.digest();
        },
        TypeError,
      );
    });

    // first-parameter
    {
      await t.test(
        "subtle.digest.first-parameter-calls-7.1.17-ToString",
        async () => {
          const sentinel = Symbol("sentinel");
          const test = async () => {
            await crypto.subtle.digest(
              {
                name: {
                  toString() {
                    throw sentinel;
                  },
                },
              },
              data
            );
          };
          await rejects(test);
          try {
            await test();
          } catch (thrownError) {
            strictEqual(thrownError, sentinel, "thrownError === sentinel");
          }
        }
      );
      await t.test(
        "subtle.digest.first-parameter-non-existent-format",
        async () => {
          await rejects(
            async () => {
              await crypto.subtle.digest("jake", data);
            },
            DOMException, null, "NotSupportedError"
          );
        }
      );
    }
    // second-parameter
    {
      await t.test("subtle.digest.second-parameter-undefined", async () => {
        await rejects(
          async () => {
            await crypto.subtle.digest("sha-1", undefined);
          },
          TypeError,
        );
      });
    }
    // happy paths
    {
      // "MD5"
      await t.test("subtle.digest.md5", async () => {
        const result = new Uint8Array(
          await crypto.subtle.digest("md5", new Uint8Array())
        );
        const expected = new Uint8Array([
          212, 29, 140, 217, 143, 0, 178, 4, 233, 128, 9, 152, 236, 248, 66,
          126,
        ]);
        deepStrictEqual(result, expected, "result deep equals expected");
      });
      // "SHA-1"
      await t.test("subtle.digest.sha-1", async () => {
        const result = new Uint8Array(
          await crypto.subtle.digest("sha-1", new Uint8Array())
        );
        const expected = new Uint8Array([
          218, 57, 163, 238, 94, 107, 75, 13, 50, 85, 191, 239, 149, 96, 24,
          144, 175, 216, 7, 9,
        ]);
        deepStrictEqual(result, expected, "result deep equals expected");
      });
      // "SHA-256"
      await t.test("subtle.digest.sha-256", async () => {
        const result = new Uint8Array(
          await crypto.subtle.digest("sha-256", new Uint8Array())
        );
        const expected = new Uint8Array([
          227, 176, 196, 66, 152, 252, 28, 20, 154, 251, 244, 200, 153, 111,
          185, 36, 39, 174, 65, 228, 100, 155, 147, 76, 164, 149, 153, 27, 120,
          82, 184, 85,
        ]);
        deepStrictEqual(result, expected, "result deep equals expected");
      });
      // "SHA-384"
      await t.test("subtle.digest.sha-384", async () => {
        const result = new Uint8Array(
          await crypto.subtle.digest("sha-384", new Uint8Array())
        );
        const expected = new Uint8Array([
          56, 176, 96, 167, 81, 172, 150, 56, 76, 217, 50, 126, 177, 177, 227,
          106, 33, 253, 183, 17, 20, 190, 7, 67, 76, 12, 199, 191, 99, 246, 225,
          218, 39, 78, 222, 191, 231, 111, 101, 251, 213, 26, 210, 241, 72, 152,
          185, 91,
        ]);
        deepStrictEqual(result, expected, "result deep equals expected");
      });
      // "SHA-512"
      await t.test("subtle.digest.sha-512", async () => {
        const result = new Uint8Array(
          await crypto.subtle.digest("sha-512", new Uint8Array())
        );
        const expected = new Uint8Array([
          207, 131, 225, 53, 126, 239, 184, 189, 241, 84, 40, 80, 214, 109, 128,
          7, 214, 32, 228, 5, 11, 87, 21, 220, 131, 244, 169, 33, 211, 108, 233,
          206, 71, 208, 209, 60, 93, 133, 242, 176, 255, 131, 24, 210, 135, 126,
          236, 47, 99, 185, 49, 189, 71, 65, 122, 129, 165, 56, 50, 122, 249,
          39, 218, 62,
        ]);
        deepStrictEqual(result, expected, "result deep equals expected");
      });
    }
  }

  // sign
  {
    const enc = new TextEncoder();
    const data = enc.encode("hello world");
    await t.test("subtle.sign", async () => {
      strictEqual(
        typeof crypto.subtle.sign,
        "function",
        `typeof crypto.subtle.sign`
      );
      strictEqual(
        crypto.subtle.sign,
        SubtleCrypto.prototype.sign,
        `crypto.subtle.sign === SubtleCrypto.prototype.sign`
      );
    });
    await t.test("subtle.sign.length", async () => {
      strictEqual(
        crypto.subtle.sign.length,
        3,
        `crypto.subtle.sign.length === 3`
      );
    });
    await t.test("subtle.sign.called-as-constructor", async () => {
      throws(
        () => {
          new crypto.subtle.sign();
        },
        TypeError
      );
    });
    await t.test("subtle.sign.called-with-wrong-this", async () => {
      const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
      await rejects(
        async () => {
          await crypto.subtle.sign.call(
            undefined,
            createRsaJsonWebKeyAlgorithm(),
            publicRsaJsonWebKeyData,
            data
          );
        },
        TypeError,
      );
    });
    await t.test("subtle.sign.called-with-no-arguments", async () => {
      await rejects(
        async () => {
          await crypto.subtle.sign();
        },
        TypeError,
      );
    });
    // first-parameter
    {
      await t.test(
        "subtle.sign.first-parameter-calls-7.1.17-ToString",
        async () => {
          const privateRsaJsonWebKeyData = createPrivateRsaJsonWebKeyData();
          const sentinel = Symbol("sentinel");
          const key = await crypto.subtle.importKey(
            "jwk",
            privateRsaJsonWebKeyData,
            createRsaJsonWebKeyAlgorithm(),
            privateRsaJsonWebKeyData.ext,
            privateRsaJsonWebKeyData.key_ops
          );
          const test = async () => {
            await crypto.subtle.sign(
              {
                name: {
                  toString() {
                    throw sentinel;
                  },
                },
              },
              key,
              data
            );
          };
          await rejects(test);
          try {
            await test();
          } catch (thrownError) {
            strictEqual(thrownError, sentinel, "thrownError === sentinel");
          }
        }
      );
      await t.test(
        "subtle.sign.first-parameter-non-existent-algorithm",
        async () => {
          const privateRsaJsonWebKeyData = createPrivateRsaJsonWebKeyData();
          await rejects(
            async () => {
              const key = await crypto.subtle.importKey(
                "jwk",
                privateRsaJsonWebKeyData,
                createRsaJsonWebKeyAlgorithm(),
                privateRsaJsonWebKeyData.ext,
                privateRsaJsonWebKeyData.key_ops
              );
              await crypto.subtle.sign("jake", key, data);
            },
            DOMException, null, "NotSupportedError"
          );
        }
      );
    }
    // second-parameter
    {
      await t.test("subtle.sign.second-parameter-invalid-format", async () => {
        await rejects(
          async () => {
            await crypto.subtle.sign(
              createRsaJsonWebKeyAlgorithm(),
              "jake",
              data
            );
          },
          TypeError
        );
      });
      await t.test("subtle.sign.second-parameter-invalid-usages", async () => {
        const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
        await rejects(
          async () => {
            const key = await crypto.subtle.importKey(
              "jwk",
              publicRsaJsonWebKeyData,
              createRsaJsonWebKeyAlgorithm(),
              publicRsaJsonWebKeyData.ext,
              publicRsaJsonWebKeyData.key_ops
            );
            await crypto.subtle.sign(createRsaJsonWebKeyAlgorithm(), key, data);
          },
          DOMException, null, "InvalidAccessError"
        );
      });
    }
    // third-parameter
    {
      await t.test("subtle.sign.third-parameter-invalid-format", async () => {
        const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
        await rejects(
          async () => {
            const key = await crypto.subtle.importKey(
              "jwk",
              publicRsaJsonWebKeyData,
              createRsaJsonWebKeyAlgorithm(),
              publicRsaJsonWebKeyData.ext,
              publicRsaJsonWebKeyData.key_ops
            );
            await crypto.subtle.sign(
              createRsaJsonWebKeyAlgorithm(),
              key,
              undefined
            );
          },
          TypeError
        );
      });
    }
    // happy-path
    {
      await t.test("subtle.sign.happy-path-jwk", async () => {
        const privateRsaJsonWebKeyData = createPrivateRsaJsonWebKeyData();
        const key = await crypto.subtle.importKey(
          "jwk",
          privateRsaJsonWebKeyData,
          createRsaJsonWebKeyAlgorithm(),
          privateRsaJsonWebKeyData.ext,
          privateRsaJsonWebKeyData.key_ops
        );
        const signature = new Uint8Array(
          await crypto.subtle.sign(createRsaJsonWebKeyAlgorithm(), key, data)
        );
        const expected = new Uint8Array([
          70, 96, 33, 185, 93, 42, 67, 49, 243, 70, 88, 68, 194, 148, 53, 249,
          255, 192, 232, 132, 161, 194, 41, 244, 174, 211, 218, 203, 7, 238, 71,
          182, 101, 49, 139, 222, 165, 70, 222, 105, 82, 156, 184, 44, 100, 108,
          121, 237, 250, 119, 66, 228, 156, 243, 71, 105, 62, 246, 22, 2, 160,
          116, 71, 147, 202, 168, 24, 92, 224, 41, 148, 161, 124, 80, 212, 169,
          212, 64, 29, 189, 2, 171, 174, 188, 159, 89, 93, 122, 219, 166, 105,
          92, 107, 173, 103, 238, 145, 226, 94, 139, 71, 124, 17, 233, 49, 138,
          89, 246, 3, 82, 238, 154, 169, 188, 66, 198, 32, 23, 230, 90, 164,
          140, 51, 47, 221, 149, 161, 14, 254, 169, 224, 223, 119, 94, 27, 63,
          199, 93, 65, 53, 24, 151, 146, 242, 239, 41, 108, 136, 31, 99, 42,
          213, 128, 244, 140, 238, 157, 107, 117, 241, 219, 137, 97, 39, 109,
          185, 176, 97, 193, 60, 117, 244, 106, 62, 193, 188, 87, 199, 37, 70,
          137, 37, 231, 110, 228, 228, 139, 53, 240, 56, 92, 102, 220, 176, 127,
          248, 24, 217, 208, 29, 209, 216, 29, 251, 100, 252, 243, 183, 195, 96,
          126, 102, 136, 48, 39, 186, 45, 202, 10, 187, 22, 52, 183, 190, 149,
          153, 32, 12, 90, 66, 49, 122, 190, 154, 167, 9, 12, 32, 77, 177, 222,
          54, 211, 233, 219, 205, 133, 0, 113, 77, 158, 1, 125, 5, 15, 195,
        ]);
        deepStrictEqual(signature, expected, "signature deep equals expected");
      });
      await t.test("subtle.sign.happy-path-hmac", async () => {
        const encoder = new TextEncoder();
        const messageUint8Array = encoder.encode("aki");
        const keyUint8Array = new Uint8Array([1, 0, 1]);
        const results = {
          "SHA-1": new Uint8Array([
            222, 61, 81, 133, 232, 89, 130, 225, 248, 25, 220, 34, 245, 103, 89,
            127, 136, 77, 146, 166,
          ]),
          "SHA-256": new Uint8Array([
            92, 237, 16, 210, 91, 89, 194, 36, 95, 98, 27, 175, 64, 25, 15, 160,
            152, 178, 145, 235, 62, 92, 23, 202, 125, 228, 8, 25, 148, 26, 215,
            242,
          ]),
          "SHA-384": new Uint8Array([
            238, 20, 74, 173, 238, 236, 161, 229, 250, 167, 72, 210, 188, 239,
            233, 39, 233, 166, 114, 241, 140, 229, 201, 129, 243, 173, 74, 198,
            223, 145, 228, 96, 253, 91, 166, 111, 244, 23, 141, 62, 112, 156,
            90, 166, 214, 69, 185, 48,
          ]),
          "SHA-512": new Uint8Array([
            211, 127, 139, 149, 23, 225, 84, 230, 82, 249, 109, 254, 168, 236,
            217, 112, 174, 52, 231, 62, 167, 197, 33, 11, 181, 21, 162, 236,
            214, 132, 43, 161, 92, 112, 230, 182, 140, 69, 169, 229, 87, 98, 57,
            81, 140, 134, 219, 253, 139, 169, 85, 181, 195, 195, 166, 241, 219,
            33, 9, 56, 67, 213, 51, 224,
          ]),
        };

        for (const algorithm of ["SHA-1", "SHA-256", "SHA-384", "SHA-512"]) {
          const key = await globalThis.crypto.subtle.importKey(
            "raw",
            keyUint8Array,
            { name: "HMAC", hash: algorithm },
            false,
            ["sign", "verify"]
          );
          // Sign the message with HMAC and the CryptoKey
          const signature = new Uint8Array(
            await globalThis.crypto.subtle.sign("HMAC", key, messageUint8Array)
          );
          const expected = results[algorithm];
          deepStrictEqual(
            signature,
            expected,
            `${algorithm} signature deep equals expected`
          );
        }
      });
    }
  }

  // verify
  {
    await t.test("subtle.verify", async () => {
      strictEqual(
        typeof crypto.subtle.verify,
        "function",
        `typeof crypto.subtle.verify`
      );
      strictEqual(
        crypto.subtle.verify,
        SubtleCrypto.prototype.verify,
        `crypto.subtle.verify === SubtleCrypto.prototype.verify`
      );
    });
    await t.test("subtle.verify.length", async () => {
      strictEqual(
        crypto.subtle.verify.length,
        4,
        `crypto.subtle.verify.length === 4`
      );
    });
    await t.test("subtle.verify.called-as-constructor", async () => {
      throws(
        () => {
          new crypto.subtle.verify();
        },
        TypeError
      );
    });
    await t.test("subtle.verify.called-with-wrong-this", async () => {
      const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
      await rejects(
        async () => {
          const key = await crypto.subtle.importKey(
            "jwk",
            publicRsaJsonWebKeyData,
            createRsaJsonWebKeyAlgorithm(),
            publicRsaJsonWebKeyData.ext,
            publicRsaJsonWebKeyData.key_ops
          );
          await crypto.subtle.verify.call(
            undefined,
            createRsaJsonWebKeyAlgorithm(),
            key,
            new Uint8Array(),
            new Uint8Array()
          );
        },
        TypeError,
      );
    });
    await t.test("subtle.verify.called-with-no-arguments", async () => {
      await rejects(
        async () => {
          await crypto.subtle.verify();
        },
        TypeError,
      );
    });
    // first-parameter
    {
      await t.test(
        "subtle.verify.first-parameter-calls-7.1.17-ToString",
        async () => {
          const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
          const sentinel = Symbol("sentinel");
          const test = async () => {
            const key = await crypto.subtle.importKey(
              "jwk",
              publicRsaJsonWebKeyData,
              createRsaJsonWebKeyAlgorithm(),
              publicRsaJsonWebKeyData.ext,
              publicRsaJsonWebKeyData.key_ops
            );
            await crypto.subtle.verify(
              {
                name: {
                  toString() {
                    throw sentinel;
                  },
                },
              },
              key,
              new Uint8Array(),
              new Uint8Array()
            );
          };
          await rejects(test);
          try {
            await test();
          } catch (thrownError) {
            strictEqual(thrownError, sentinel, "thrownError === sentinel");
          }
        }
      );
      await t.test(
        "subtle.verify.first-parameter-non-existent-algorithm",
        async () => {
          const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
          await rejects(
            async () => {
              const key = await crypto.subtle.importKey(
                "jwk",
                publicRsaJsonWebKeyData,
                createRsaJsonWebKeyAlgorithm(),
                publicRsaJsonWebKeyData.ext,
                publicRsaJsonWebKeyData.key_ops
              );
              await crypto.subtle.verify(
                "jake",
                key,
                new Uint8Array(),
                new Uint8Array()
              );
            },
            DOMException, null, "NotSupportedError"
          );
        }
      );
    }
    // second-parameter
    {
      await t.test(
        "subtle.verify.second-parameter-invalid-format",
        async () => {
          await rejects(
            async () => {
              await crypto.subtle.verify(
                createRsaJsonWebKeyAlgorithm(),
                "jake",
                new Uint8Array(),
                new Uint8Array()
              );
            },
            TypeError
          );
        }
      );
      await t.test(
        "subtle.verify.second-parameter-invalid-usages",
        async () => {
          const privateRsaJsonWebKeyData = createPrivateRsaJsonWebKeyData();
          await rejects(
            async () => {
              const key = await crypto.subtle.importKey(
                "jwk",
                privateRsaJsonWebKeyData,
                createRsaJsonWebKeyAlgorithm(),
                privateRsaJsonWebKeyData.ext,
                privateRsaJsonWebKeyData.key_ops
              );
              await crypto.subtle.verify(
                createRsaJsonWebKeyAlgorithm(),
                key,
                new Uint8Array(),
                new Uint8Array()
              );
            },
            DOMException, null, "InvalidAccessError"
          );
        }
      );
    }
    // third-parameter
    {
      await t.test("subtle.verify.third-parameter-invalid-format", async () => {
        const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
        await rejects(
          async () => {
            const key = await crypto.subtle.importKey(
              "jwk",
              publicRsaJsonWebKeyData,
              createRsaJsonWebKeyAlgorithm(),
              publicRsaJsonWebKeyData.ext,
              publicRsaJsonWebKeyData.key_ops
            );
            await crypto.subtle.verify(
              createRsaJsonWebKeyAlgorithm(),
              key,
              undefined,
              new Uint8Array()
            );
          },
          TypeError
        );
      });
    }
    // fourth-parameter
    {
      await t.test(
        "subtle.verify.fourth-parameter-invalid-format",
        async () => {
          const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
          await rejects(
            async () => {
              const key = await crypto.subtle.importKey(
                "jwk",
                publicRsaJsonWebKeyData,
                createRsaJsonWebKeyAlgorithm(),
                publicRsaJsonWebKeyData.ext,
                publicRsaJsonWebKeyData.key_ops
              );
              await crypto.subtle.verify(
                createRsaJsonWebKeyAlgorithm(),
                key,
                new Uint8Array(),
                undefined
              );
            },
            TypeError
          );
        }
      );
    }
    // incorrect-signature
    {
      await t.test("subtle.verify.incorrect-signature-jwk", async () => {
        const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
        const key = await crypto.subtle.importKey(
          "jwk",
          publicRsaJsonWebKeyData,
          createRsaJsonWebKeyAlgorithm(),
          publicRsaJsonWebKeyData.ext,
          publicRsaJsonWebKeyData.key_ops
        );
        const signature = new Uint8Array();
        const enc = new TextEncoder();
        const data = enc.encode("hello world");
        const result = await crypto.subtle.verify(
          createRsaJsonWebKeyAlgorithm(),
          key,
          signature,
          data
        );
        strictEqual(result, false, "result === false");
      });
      await t.test("subtle.verify.incorrect-signature-hmac", async () => {
        const keyUint8Array = new Uint8Array([1, 0, 1]);
        const signature = new Uint8Array();
        const enc = new TextEncoder();
        const data = enc.encode("hello world");

        for (const algorithm of ["SHA-1", "SHA-256", "SHA-384", "SHA-512"]) {
          const key = await globalThis.crypto.subtle.importKey(
            "raw",
            keyUint8Array,
            { name: "HMAC", hash: algorithm },
            false,
            ["sign", "verify"]
          );
          const result = await crypto.subtle.verify(
            "HMAC",
            key,
            signature,
            data
          );
          strictEqual(result, false, "result");
        }
      });
    }
    // correct-signature
    {
      await t.test("subtle.verify.correct-signature-jwk-rsa", async () => {
        const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
        const privateRsaJsonWebKeyData = createPrivateRsaJsonWebKeyData();
        const pkey = await crypto.subtle.importKey(
          "jwk",
          privateRsaJsonWebKeyData,
          createRsaJsonWebKeyAlgorithm(),
          privateRsaJsonWebKeyData.ext,
          privateRsaJsonWebKeyData.key_ops
        );
        const key = await crypto.subtle.importKey(
          "jwk",
          publicRsaJsonWebKeyData,
          createRsaJsonWebKeyAlgorithm(),
          publicRsaJsonWebKeyData.ext,
          publicRsaJsonWebKeyData.key_ops
        );
        const enc = new TextEncoder();
        const data = enc.encode("hello world");
        const signature = await crypto.subtle.sign(
          createRsaJsonWebKeyAlgorithm(),
          pkey,
          data
        );
        const result = await crypto.subtle.verify(
          createRsaJsonWebKeyAlgorithm(),
          key,
          signature,
          data
        );
        strictEqual(result, true, "result === true");
      });
      await t.test("subtle.verify.correct-signature-jwk-ecdsa", async () => {
        const publicEcdsaJsonWebKeyData = createPublicEcdsaJsonWebKeyData();
        const privateEcdsaJsonWebKeyData = createPrivateEcdsaJsonWebKeyData();
        const pkey = await crypto.subtle.importKey(
          "jwk",
          privateEcdsaJsonWebKeyData,
          ecdsaJsonWebKeyAlgorithm,
          privateEcdsaJsonWebKeyData.ext,
          privateEcdsaJsonWebKeyData.key_ops
        );
        const key = await crypto.subtle.importKey(
          "jwk",
          publicEcdsaJsonWebKeyData,
          ecdsaJsonWebKeyAlgorithm,
          publicEcdsaJsonWebKeyData.ext,
          publicEcdsaJsonWebKeyData.key_ops
        );
        const enc = new TextEncoder();
        const data = enc.encode("hello world");
        const signature = await crypto.subtle.sign(
          ecdsaJsonWebKeyAlgorithm,
          pkey,
          data
        );
        const result = await crypto.subtle.verify(
          ecdsaJsonWebKeyAlgorithm,
          key,
          signature,
          data
        );
        strictEqual(result, true, "result === true");
      });
      await t.test("subtle.verify.correct-signature-hmac", async () => {
        const results = {
          "SHA-1": new Uint8Array([
            222, 61, 81, 133, 232, 89, 130, 225, 248, 25, 220, 34, 245, 103, 89,
            127, 136, 77, 146, 166,
          ]),
          "SHA-256": new Uint8Array([
            92, 237, 16, 210, 91, 89, 194, 36, 95, 98, 27, 175, 64, 25, 15, 160,
            152, 178, 145, 235, 62, 92, 23, 202, 125, 228, 8, 25, 148, 26, 215,
            242,
          ]),
          "SHA-384": new Uint8Array([
            238, 20, 74, 173, 238, 236, 161, 229, 250, 167, 72, 210, 188, 239,
            233, 39, 233, 166, 114, 241, 140, 229, 201, 129, 243, 173, 74, 198,
            223, 145, 228, 96, 253, 91, 166, 111, 244, 23, 141, 62, 112, 156,
            90, 166, 214, 69, 185, 48,
          ]),
          "SHA-512": new Uint8Array([
            211, 127, 139, 149, 23, 225, 84, 230, 82, 249, 109, 254, 168, 236,
            217, 112, 174, 52, 231, 62, 167, 197, 33, 11, 181, 21, 162, 236,
            214, 132, 43, 161, 92, 112, 230, 182, 140, 69, 169, 229, 87, 98, 57,
            81, 140, 134, 219, 253, 139, 169, 85, 181, 195, 195, 166, 241, 219,
            33, 9, 56, 67, 213, 51, 224,
          ]),
        };
        const encoder = new TextEncoder();
        const messageUint8Array = encoder.encode("aki");
        const keyUint8Array = new Uint8Array([1, 0, 1]);

        for (const algorithm of ["SHA-1", "SHA-256", "SHA-384", "SHA-512"]) {
          const key = await globalThis.crypto.subtle.importKey(
            "raw",
            keyUint8Array,
            { name: "HMAC", hash: algorithm },
            false,
            ["sign", "verify"]
          );
          // Sign the message with HMAC and the CryptoKey
          // const signature = new Uint8Array(await globalThis.crypto.subtle.sign("HMAC", key, messageUint8Array));
          const signature = results[algorithm];
          const result = await crypto.subtle.verify(
            "HMAC",
            key,
            signature,
            messageUint8Array
          );
          strictEqual(result, true, "result");
        }
      });
    }
  }
});
