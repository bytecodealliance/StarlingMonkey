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

function base64ToArrayBuffer(b64) {
  const bin = atob(b64.replace(/\s+/g, ""));
  const arr = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) {
    arr[i] = bin.charCodeAt(i);
  }
  return arr.buffer;
}

// generated using openssl cli
const ecdsaP256Pkcs8 = base64ToArrayBuffer(
  `MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQg9+kCK7f6pqJhDk71sqDp+c0R6Y8h5Zq1Yzk8l6aULSehRANCAATmqlo5vDVhxTExquFDjVFNDdjiODbfvZmruJQhJO9JIl/VYdSOlQtLcgEzShv+MJ88HqvxPNlkPrKLDMejB1BE`,
);
const ecdsaP256Spki = base64ToArrayBuffer(
  `MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE4P7+DJBk6R8R6HRSDLaakl0k83D+MYGqLUEO2DcyB8cAZvGxJRYDakQZuQc/BpYgPTs/C4u4nGr2sQ08cV87xQ==`,
);
const ecdsaP384Pkcs8 = base64ToArrayBuffer(
  `MIG2AgEAMBAGByqGSM49AgEGBSuBBAAiBIGeMIGbAgEBBDBUiVVDYE7TA1iib5lEUReaejNpqSuv1hmYYRRsm9zHIslS+z9YjIjMe9UP9pwuOBOhZANiAATv3ew0tmfIjO4rcKB4CirOjvTO+cZBOQpizR+oNnMhhcaFGpewkUzpEQ/du893jA5kANEtzKsplaIAql16zDwr15e8YA73ChHv1RLzapqFHnmD3jbYTzYpEaWlkUDTTWI=`,
);
const ecdsaP384Spki = base64ToArrayBuffer(
  `MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAEsKinmo/3Ti72/i4papPRvi1DKCe+Cd1N+4osN3OeVYe7KmzfTBX7+deLI1bu9TzXm9VeIa7+omKPXBrLVBB3BN/U3eQW097jG9BX4+91rwW4i/vESlxCGSbb9FhLfxxw`,
);
const ecdsaP521Pkcs8 = base64ToArrayBuffer(
  `MIHuAgEAMBAGByqGSM49AgEGBSuBBAAjBIHWMIHTAgEBBEIAGBugOxmjib54TudV/c8teIoAKTC0lYPqgphDS1EEqpxp6IPKQFRBISB6GcxyXHBfqgDv4uVYy2BECzdgRmaEzKihgYkDgYYABAABos1qChbTBstc+4EBpivvNkVy9I1eVQAJhoFEail50sxob/1RMDBYO744Xa2MZz/gssUnVwvJpPmXhDa86omB5gEBLsJ77j3MaY7cZ3ZGL1X2fCWL5VjqxrMj9IQ7N+qrCD86WWnBHa49TnuLHkF+Dw0qe2S3QsmbM8zJ5+o7t1pWBA==`,
);
const ecdsaP521Spki = base64ToArrayBuffer(
  `MIGbMBAGByqGSM49AgEGBSuBBAAjA4GGAAQBy8hKSiiCilqsLQc9/a2uzv9jVruRvz4T/+ftCXqTXPPtVzuVMOupiV92RUAP/1cbordlIbmRHcHESZ3PKENoSxQAajLUC+7f08O4kCc/hrwT5DYGuSJYEhXsFIVzszEOAOxgNo3KquhpQZetawTeiGBPoKAMe7DFvMQ7MbzvNIHEZhg=`,
);
const rsa2048Pkcs8 = base64ToArrayBuffer(
  `MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQCRE7mySLjyeokRW/J5qFKc1HSNg/1sbk482ETmiiGUa+537x9H2WqBJ4/ZjQq05LwevKWYuwMW/pAImnwnnIetQ+VN5iAMmo3A1ksIKx+32gQV9DXmK9QI5mYzIH8CJOTEZUlljhrZ4j1yteFtRXD0D8Tr7z+54FO6qISaVGgrWLfGqcBehXmKrf0Iu4oQ3qa4QTDagQU+CVseD5dau5WG16UEJu9tbzQWjnKhuMPVo3vq0/c9pqsJjkGL2AhOBLgpnuWgsXEqhm7ztd5ueuHfbwQCeqjYOwk5j9Bcu6uvTQ8b3fjZhAv2mSnciyCjsl+oZ75fpTZ3ymsHDYXKKyszAgMBAAECggEAEMYoluIMrFCZr42jjAh3wNVZwpELyLE6T5Or58H53vjZwjk+MycCxvsliUBjCAZYb+9c8DtTQKHfUndWOphBIbnzdd3RP9PQ5wZ/92pRCQdmqIkrgDj6E6tcDrZHqR5N6x1Q7rKPDTk45J39+g3o4Pc/zA3GHv4+gBsC9fsjbjtvq5DCO0tqmzKWAe6CtX0JnHiumqv9WD2h5ApXqDfrNdwo+nqYzuA3spAk+yAsEssqGW+2+hK0wOhl/75Yo762sAkR+9W8KWRh2TDWyUSIokcKypjokAYdFkT4MCP7o3FZg/YShLgK6EsulrPBKtFkU28gP25w5OqcK/krsDTf/QKBgQDJxKVw5iE1u7EBAYHOCC/EgKL522HaDZEQa1g8mWYDvv0QohyPdR/om19Xcdyo4RpSXxJSZvO//hBwJ6h8gbF1fb0JducMCJKR8qxYgSdCs52EoQwtJkOnsCQ1L69TEDHW31hHTAvwtTPPyaKzoD47yh03Gv2Qtqw7O4dLnqr5LQKBgQC4EkCn6mQ8BHZQSRZXPRuhuQ1pvtPDAgRw8/IZPttkJUnaewV7vdQQfAReHwwFgN4Qz/1tkyh1lxUw+qHTP6tn4FcogH2NjRkI4yYSVKlrjiQy9LlcS4bO5noigwpK9VL3Dl65YXaQpPAoiH/M7KoJuadcftMvMYSVbpsrAUSx3wKBgAfYMWZ2TAw+w976JAXSo6jMJ4n3UZKcvGsbAU8515GFt2kSJHIfZ6IviEFqF94pAlD5iUjS398zDYiOwio1EKU0wki/6rO5EZnDCZhXTSN2wEULzeFjf3Xhj0bSF8ru8kEcZd4/wqcVJHKLsFuzezfv37rovbsGnyOeaOAzxwnxAoGANdkBGR40nGohwnLfGj5CKxlblyfAAzg/3FtA46nDvJQ7+rIqdHyf4QKmtHIYrjN4wypVVQzLtTqxdFadqJLjrcuvM6YDFLnGLRdmN86UkWZPqKh24U0m7rf12srC5BLIZoXJqCme6cmWiAUGfght5dJt510iope11ZE5y2bflbkCgYEApcgQs8TvOWxRGSPdgd7pz2TXtgs+J17T0X+h/RqP7S7CxFAJ01loFCswA1Gl/Kdw0ShdXEMXOHLXTyVOy2q1suBfT5bb5nQh9GnYr96Cs4qPqbK/MwniREeJLpGLW3COHPhNQIZG2L4mSpkA+07UOMcQF+BwuoVnDOhUs8cNEuo=`,
);
const rsa2048Spki = base64ToArrayBuffer(
  `MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAvuj+4WNu+G/s+90tjtLBEXpaVQHFOvVBzZ/vMplfYMF1hI5X0iFJlyV06qNCCTaEtNzbzueJfKWzoar6cxzGCumvquXXLIZ/Gg9y9hOwotakJBoE5imhZ12Z37SgRUUC8TkCKyh1C+PCwcf8W9cOFT0RUg82ed5ByRM0VEV4TvUngZH61nu02zUg/fr7HUDIj7e6N/0wjtqIiQuc9dzAk1lt7qo47w35w0njGwhE/VuVrSTILmdRvroMt2nZ1O+c054DbiBlqSsXeaW99YGewT0d2RKdLnf0myvDBuQcFqZUnt26nD2Q2DDI17Mo8MaOmQzgrZ5iVufZ8lC86t18iwIDAQAB`,
);
const rsa3072Pkcs8 = base64ToArrayBuffer(
  `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`,
);
const rsa4096Pkcs8 = base64ToArrayBuffer(
  `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`,
);

const ecdsaP256Algorithm = {
  name: "ECDSA",
  namedCurve: "P-256",
};

const ecdsaP384Algorithm = {
  name: "ECDSA",
  namedCurve: "P-384",
};

const ecdsaP521Algorithm = {
  name: "ECDSA",
  namedCurve: "P-521",
};

const rsaPkcs1Sha1Algorithm = {
  name: "RSASSA-PKCS1-v1_5",
  hash: { name: "SHA-1" },
};

const rsaPkcs1Sha256Algorithm = {
  name: "RSASSA-PKCS1-v1_5",
  hash: { name: "SHA-256" },
};

const rsaPkcs1Sha384Algorithm = {
  name: "RSASSA-PKCS1-v1_5",
  hash: { name: "SHA-384" },
};

const rsaPkcs1Sha512Algorithm = {
  name: "RSASSA-PKCS1-v1_5",
  hash: { name: "SHA-512" },
};

// From https://www.rfc-editor.org/rfc/rfc8032#section-7.1 TEST 1
const ed25519Test1PublicJsonWebKeyData = () => ({
  kty: "OKP",
  crv: "Ed25519",
  x: "11qYAYKxCrfVS_7TyWQHOg7hcvPapiMlrwIaaPcHURo",
  ext: true,
  key_ops: ["verify"],
});
const ed25519Test1PrivateJsonWebKeyData = () => ({
  kty: "OKP",
  crv: "Ed25519",
  x: "11qYAYKxCrfVS_7TyWQHOg7hcvPapiMlrwIaaPcHURo",
  d: "nWGxne_9WmC6hEr0kuwsxERJxWl7MmkZcDusAxyuf2A",
  ext: true,
  key_ops: ["sign"],
});
const ed25519Test1Raw = new Uint8Array([
  215, 90, 152, 1, 130, 177, 10, 183, 213, 75, 254, 211, 201, 100, 7, 58,
  14, 225, 114, 243, 218, 166, 35, 37, 175, 2, 26, 104, 247, 7, 81, 26,
]);
// The message for TEST 1 is empty.
const ed25519Test1Signature = new Uint8Array([
  229, 86, 67, 0, 195, 96, 172, 114, 144, 134, 226, 204, 128, 110, 130, 138,
  132, 135, 127, 30, 184, 229, 217, 116, 216, 115, 224, 101, 34, 73, 1, 85,
  95, 184, 130, 21, 144, 163, 59, 172, 198, 30, 57, 112, 28, 249, 180, 107,
  210, 91, 245, 240, 89, 91, 190, 36, 101, 81, 65, 67, 142, 122, 16, 11,
]);

// From https://www.rfc-editor.org/rfc/rfc8032#section-7.1 TEST 2
const ed25519Test2Spki = new Uint8Array([
  48, 42, 48, 5, 6, 3, 43, 101, 112, 3, 33, 0, 61, 64, 23, 195,
  232, 67, 137, 90, 146, 183, 10, 167, 77, 27, 126, 188, 156, 152, 44, 207,
  46, 196, 150, 140, 192, 205, 85, 241, 42, 244, 102, 12,
]);
const ed25519Test2Pkcs8 = new Uint8Array([
  48, 46, 2, 1, 0, 48, 5, 6, 3, 43, 101, 112, 4, 34, 4, 32,
  76, 205, 8, 155, 40, 255, 150, 218, 157, 182, 195, 70, 236, 17, 78, 15,
  91, 138, 49, 159, 53, 171, 166, 36, 218, 140, 246, 237, 79, 184, 166, 251,
]);
const ed25519Test2Message = new Uint8Array([0x72]);
const ed25519Test2Signature = new Uint8Array([
  146, 160, 9, 169, 240, 212, 202, 184, 114, 14, 130, 11, 95, 100, 37, 64,
  162, 178, 123, 84, 22, 80, 63, 143, 179, 118, 34, 35, 235, 219, 105, 218,
  8, 90, 193, 228, 62, 21, 153, 110, 69, 143, 54, 19, 208, 241, 29, 140,
  56, 123, 46, 174, 180, 48, 42, 238, 176, 13, 41, 22, 18, 187, 12, 0,
]);


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
      `crypto.subtle instanceof SubtleCrypto`,
    );
  });

  // importKey
  {
    await t.test("subtle.importKey", async () => {
      strictEqual(
        typeof crypto.subtle.importKey,
        "function",
        `typeof crypto.subtle.importKey`,
      );
      strictEqual(
        crypto.subtle.importKey,
        SubtleCrypto.prototype.importKey,
        `crypto.subtle.importKey === SubtleCrypto.prototype.importKey`,
      );
    });
    await t.test("subtle.importKey.length", async () => {
      strictEqual(
        crypto.subtle.importKey.length,
        5,
        `crypto.subtle.importKey.length === 5`,
      );
    });
    await t.test("subtle.importKey.called-as-constructor", async () => {
      throws(() => {
        new crypto.subtle.importKey();
      }, TypeError);
    });
    await t.test("subtle.importKey.called-with-wrong-this", async () => {
      const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
      await rejects(async () => {
        await crypto.subtle.importKey.call(
          undefined,
          "jwk",
          publicRsaJsonWebKeyData,
          createRsaJsonWebKeyAlgorithm(),
          publicRsaJsonWebKeyData.ext,
          publicRsaJsonWebKeyData.key_ops,
        );
      }, TypeError);
    });
    await t.test("subtle.importKey.called-with-no-arguments", async () => {
      await rejects(async () => {
        await crypto.subtle.importKey();
      }, TypeError);
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
              publicRsaJsonWebKeyData.key_ops,
            );
          };
          await rejects(test);
          try {
            await test();
          } catch (thrownError) {
            strictEqual(thrownError, sentinel, "thrownError === sentinel");
          }
        },
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
                publicRsaJsonWebKeyData.key_ops,
              );
            },
            DOMException,
            null,
            "NotSupportedError",
          );
        },
      );
    }

    // second-parameter
    {
      await t.test(
        "subtle.importKey.second-parameter-invalid-format",
        async () => {
          const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
          await rejects(async () => {
            await crypto.subtle.importKey(
              "jwk",
              Symbol(),
              createRsaJsonWebKeyAlgorithm(),
              publicRsaJsonWebKeyData.ext,
              publicRsaJsonWebKeyData.key_ops,
            );
          }, TypeError);
        },
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
                  publicRsaJsonWebKeyData.key_ops,
                );
              },
              DOMException,
              null,
              "DataError",
            );
          },
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
                publicRsaJsonWebKeyData.key_ops,
              );
            };
            await rejects(test);
            try {
              await test();
            } catch (thrownError) {
              strictEqual(thrownError, sentinel, "thrownError === sentinel");
            }
          },
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
                  publicRsaJsonWebKeyData.key_ops,
                );
              },
              DOMException,
              null,
              "DataError",
            );
          },
        );
        await t.test(
          "subtle.importKey.rsa-jwk-public.second-parameter-missing-kty-field",
          async () => {
            const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
            await rejects(async () => {
              delete publicRsaJsonWebKeyData.kty;
              await crypto.subtle.importKey(
                "jwk",
                publicRsaJsonWebKeyData,
                createRsaJsonWebKeyAlgorithm(),
                publicRsaJsonWebKeyData.ext,
                publicRsaJsonWebKeyData.key_ops,
              );
            }, DOMException, null, "DataError");
          },
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
                  publicRsaJsonWebKeyData.key_ops,
                );
              },
              DOMException,
              null,
              "DataError",
            );
          },
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
              key_ops,
            );
          },
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
                  key_ops,
                );
              },
              DOMException,
              null,
              "DataError",
            );
          },
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
              key_ops,
            );
          },
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
                  key_ops,
                );
              },
              DOMException,
              null,
              "DataError",
            );
          },
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
                  key_ops,
                );
              },
              DOMException,
              null,
              "DataError",
            );
          },
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
                key_ops,
              );
            };
            await rejects(test);
            try {
              await test();
            } catch (thrownError) {
              strictEqual(thrownError, sentinel, "thrownError === sentinel");
            }
          },
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
                  publicRsaJsonWebKeyData.key_ops,
                );
              },
              DOMException,
              null,
              "DataError",
            );
          },
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
                publicRsaJsonWebKeyData.key_ops,
              );
            };
            await rejects(test);
            try {
              await test();
            } catch (thrownError) {
              strictEqual(thrownError, sentinel, "thrownError === sentinel");
            }
          },
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
                  publicRsaJsonWebKeyData.key_ops,
                );
              },
              DOMException,
              null,
              "DataError",
            );
          },
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
                  publicEcdsaJsonWebKeyData.key_ops,
                );
              },
              DOMException,
              null,
              "DataError",
            );
          },
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
                publicEcdsaJsonWebKeyData.key_ops,
              );
            };
            await rejects(test);
            try {
              await test();
            } catch (thrownError) {
              strictEqual(thrownError, sentinel, "thrownError === sentinel");
            }
          },
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
                  publicEcdsaJsonWebKeyData.key_ops,
                );
              },
              DOMException,
              null,
              "DataError",
            );
          },
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
                  publicEcdsaJsonWebKeyData.key_ops,
                );
              },
              DOMException,
              null,
              "DataError",
            );
          },
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
                publicEcdsaJsonWebKeyData.key_ops,
              );
            };
            await rejects(test);
            try {
              await test();
            } catch (thrownError) {
              strictEqual(thrownError, sentinel, "thrownError === sentinel");
            }
          },
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
                  publicEcdsaJsonWebKeyData.key_ops,
                );
              },
              DOMException,
              null,
              "DataError",
            );
          },
        );
        await t.test(
          "subtle.importKey.ecdsa-jwk-public.second-parameter-missing-kty-field",
          async () => {
            const publicEcdsaJsonWebKeyData = createPublicEcdsaJsonWebKeyData();
            await rejects(async () => {
              delete publicEcdsaJsonWebKeyData.kty;
              await crypto.subtle.importKey(
                "jwk",
                publicEcdsaJsonWebKeyData,
                ecdsaJsonWebKeyAlgorithm,
                publicEcdsaJsonWebKeyData.ext,
                publicEcdsaJsonWebKeyData.key_ops,
              );
            }, DOMException, null, "DataError");
          },
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
                  publicEcdsaJsonWebKeyData.key_ops,
                );
              },
              DOMException,
              null,
              "DataError",
            );
          },
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
              key_ops,
            );
          },
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
                  key_ops,
                );
              },
              DOMException,
              null,
              "DataError",
            );
          },
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
              key_ops,
            );
          },
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
                  key_ops,
                );
              },
              DOMException,
              null,
              "DataError",
            );
          },
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
                  key_ops,
                );
              },
              DOMException,
              null,
              "DataError",
            );
          },
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
                key_ops,
              );
            };
            await rejects(test);
            try {
              await test();
            } catch (thrownError) {
              strictEqual(thrownError, sentinel, "thrownError === sentinel");
            }
          },
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
                privateEcdsaJsonWebKeyData.key_ops,
              );
            };
            await rejects(test);
            try {
              await test();
            } catch (thrownError) {
              strictEqual(thrownError, sentinel, "thrownError === sentinel");
            }
          },
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
                  privateEcdsaJsonWebKeyData.key_ops,
                );
              },
              DOMException,
              null,
              "DataError",
            );
          },
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
              publicRsaJsonWebKeyData.key_ops,
            );
          },
          DOMException,
          null,
          "NotSupportedError",
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
              publicRsaJsonWebKeyData.key_ops,
            );
          };
          await rejects(test);
          try {
            await test();
          } catch (thrownError) {
            strictEqual(thrownError, sentinel, "thrownError === sentinel");
          }
        },
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
                publicRsaJsonWebKeyData.key_ops,
              );
            },
            DOMException,
            null,
            "NotSupportedError",
          );
        },
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
              publicRsaJsonWebKeyData.key_ops,
            );
          };
          await rejects(test);
          try {
            await test();
          } catch (thrownError) {
            strictEqual(thrownError, sentinel, "thrownError === sentinel");
          }
        },
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
                publicRsaJsonWebKeyData.key_ops,
              );
            },
            DOMException,
            null,
            "DataError",
          );
        },
      );
    }

    // fifth-parameter
    {
      await t.test("subtle.importKey.fifth-parameter-undefined", async () => {
        const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
        await rejects(async () => {
          await crypto.subtle.importKey(
            "jwk",
            publicRsaJsonWebKeyData,
            createRsaJsonWebKeyAlgorithm(),
            publicRsaJsonWebKeyData.ext,
            undefined,
          );
        }, TypeError);
      });
      await t.test("subtle.importKey.fifth-parameter-invalid", async () => {
        const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
        await rejects(async () => {
          await crypto.subtle.importKey(
            "jwk",
            publicRsaJsonWebKeyData,
            createRsaJsonWebKeyAlgorithm(),
            publicRsaJsonWebKeyData.ext,
            ["jake"],
          );
        }, TypeError);
      });
      // https://wicg.github.io/webcrypto-modern-algos/ adds these values to the KeyUsage enum, so
      // they must reach the algorithm's usage check (SyntaxError) instead of failing enum
      // conversion (TypeError).
      const modernKeyUsages = ["encapsulateKey", "encapsulateBits", "decapsulateKey", "decapsulateBits"];
      await t.test("subtle.importKey.fifth-parameter-modern-algorithms-usages", async () => {
        for (const usage of modernKeyUsages) {
          const publicEcdsaJsonWebKeyData = createPublicEcdsaJsonWebKeyData();
          await rejects(async () => {
            await crypto.subtle.importKey(
              "jwk",
              publicEcdsaJsonWebKeyData,
              ecdsaJsonWebKeyAlgorithm,
              publicEcdsaJsonWebKeyData.ext,
              ["verify", usage],
            );
          }, DOMException, undefined, "SyntaxError");
        }
      });
      await t.test("subtle.importKey.jwk-key_ops-modern-algorithms-usages", async () => {
        for (const usage of modernKeyUsages) {
          const jwk = { ...createPublicEcdsaJsonWebKeyData(), key_ops: ["verify", usage] };
          const key = await crypto.subtle.importKey("jwk", jwk, ecdsaJsonWebKeyAlgorithm, true, ["verify"]);
          deepStrictEqual(key.usages, ["verify"], `key.usages with key_ops ["verify", "${usage}"]`);
        }
      });
      await t.test(
        "subtle.importKey.fifth-parameter-duplicate-operations",
        async () => {
          const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
          const key_ops = publicRsaJsonWebKeyData.key_ops.concat(
            publicRsaJsonWebKeyData.key_ops,
          );
          await crypto.subtle.importKey(
            "jwk",
            publicRsaJsonWebKeyData,
            createRsaJsonWebKeyAlgorithm(),
            publicRsaJsonWebKeyData.ext,
            key_ops,
          );
        },
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
                ["sign"],
              );
            },
            DOMException,
            null,
            "DataError",
          );
        },
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
              ["sign", op],
            );
          };
          await rejects(test);
          try {
            await test();
          } catch (thrownError) {
            strictEqual(thrownError, sentinel, "thrownError === sentinel");
          }
        },
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
          publicRsaJsonWebKeyData.key_ops,
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
          `key.algorithm`,
        );
        strictEqual(key.extractable, true, `key.extractable === true`);
        strictEqual(key.type, "public", `key.type === "public"`);
        deepStrictEqual(
          key.usages,
          ["verify"],
          `key.usages deep equals ["verify"]`,
        );
      });

      await t.test("subtle.importKey.JWK-EC256-Public", async () => {
        const publicEcdsaJsonWebKeyData = createPublicEcdsaJsonWebKeyData();
        const key = await crypto.subtle.importKey(
          "jwk",
          publicEcdsaJsonWebKeyData,
          ecdsaJsonWebKeyAlgorithm,
          publicEcdsaJsonWebKeyData.ext,
          publicEcdsaJsonWebKeyData.key_ops,
        );
        strictEqual(key instanceof CryptoKey, true, `key instanceof CryptoKey`);
        deepStrictEqual(
          key.algorithm,
          {
            name: "ECDSA",
            namedCurve: "P-256",
          },
          `key.algorithm`,
        );
        strictEqual(key.extractable, true, `key.extractable === true`);
        strictEqual(key.type, "public", `key.type === "public"`);
        deepStrictEqual(
          key.usages,
          ["verify"],
          `key.usages deep equals ["verify"]`,
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
            ["sign", "verify"],
          );
          strictEqual(
            key instanceof CryptoKey,
            true,
            `key instanceof CryptoKey`,
          );
          deepStrictEqual(
            key.algorithm,
            {
              name: "HMAC",
              hash: { name: algorithm },
              length: 24,
            },
            `key.algorithm`,
          );
          strictEqual(key.extractable, false, `key.extractable`);
          strictEqual(key.type, "secret", `key.type`);
          deepStrictEqual(key.usages, ["sign", "verify"], `key.usages`);
        }
      });
      await t.test("subtle.importKey.HMAC-non-sign-or-verify-usage", async () => {
        for (const usages of [[], ["encrypt"], ["sign", "encrypt"], ["verify", "deriveBits"]]) {
          await rejects(async () => {
            await crypto.subtle.importKey(
              "raw",
              new Uint8Array([1, 0, 1]),
              { name: "HMAC", hash: "SHA-256" },
              false,
              usages,
            );
          }, DOMException, undefined, "SyntaxError");
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
          ["sign", "verify"],
        );
        strictEqual(key instanceof CryptoKey, true, `key instanceof CryptoKey`);
        deepStrictEqual(
          key.algorithm,
          {
            name: "HMAC",
            hash: { name: "SHA-256" },
            length: 256,
          },
          `key.algorithm`,
        );
        strictEqual(key.extractable, false, `key.extractable`);
        strictEqual(key.type, "secret", `key.type`);
        deepStrictEqual(key.usages, ["sign", "verify"], `key.usages`);
      });

      // ECDSA P-256 Tests
      await t.test("pkcs8-ecdsa-p256-extractable-true", async () => {
        const key = await crypto.subtle.importKey(
          "pkcs8",
          ecdsaP256Pkcs8,
          ecdsaP256Algorithm,
          true,
          ["sign"],
        );

        strictEqual(key instanceof CryptoKey, true, "key instanceof CryptoKey");
        deepStrictEqual(
          key.algorithm,
          {
            name: "ECDSA",
            namedCurve: "P-256",
          },
          "key.algorithm",
        );
        strictEqual(key.extractable, true, "key.extractable");
        strictEqual(key.type, "private", "key.type");
        deepStrictEqual(key.usages, ["sign"], "key.usages");
      });

      await t.test("pkcs8-ecdsa-p256-extractable-false", async () => {
        const key = await crypto.subtle.importKey(
          "pkcs8",
          ecdsaP256Pkcs8,
          ecdsaP256Algorithm,
          false,
          ["sign"],
        );

        strictEqual(key instanceof CryptoKey, true, "key instanceof CryptoKey");
        deepStrictEqual(
          key.algorithm,
          {
            name: "ECDSA",
            namedCurve: "P-256",
          },
          "key.algorithm",
        );
        strictEqual(key.extractable, false, "key.extractable");
        strictEqual(key.type, "private", "key.type");
        deepStrictEqual(key.usages, ["sign"], "key.usages");
      });

      // ECDSA P-384 Tests
      await t.test("pkcs8-ecdsa-p384-extractable-true", async () => {
        const key = await crypto.subtle.importKey(
          "pkcs8",
          ecdsaP384Pkcs8,
          ecdsaP384Algorithm,
          true,
          ["sign"],
        );

        strictEqual(key instanceof CryptoKey, true, "key instanceof CryptoKey");
        deepStrictEqual(
          key.algorithm,
          {
            name: "ECDSA",
            namedCurve: "P-384",
          },
          "key.algorithm",
        );
        strictEqual(key.extractable, true, "key.extractable");
        strictEqual(key.type, "private", "key.type");
        deepStrictEqual(key.usages, ["sign"], "key.usages");
      });

      await t.test("pkcs8-ecdsa-p384-extractable-false", async () => {
        const key = await crypto.subtle.importKey(
          "pkcs8",
          ecdsaP384Pkcs8,
          ecdsaP384Algorithm,
          false,
          ["sign"],
        );

        strictEqual(key instanceof CryptoKey, true, "key instanceof CryptoKey");
        strictEqual(key.extractable, false, "key.extractable");
        strictEqual(key.algorithm.name, "ECDSA", "algorithm name");
        strictEqual(key.algorithm.namedCurve, "P-384", "named curve");
        strictEqual(key.type, "private", "key.type");
        deepStrictEqual(key.usages, ["sign"], "key.usages");
      });

      // ECDSA P-521 Tests
      await t.test("pkcs8-ecdsa-p521-extractable-true", async () => {
        const key = await crypto.subtle.importKey(
          "pkcs8",
          ecdsaP521Pkcs8,
          ecdsaP521Algorithm,
          true,
          ["sign"],
        );

        strictEqual(key instanceof CryptoKey, true, "key instanceof CryptoKey");
        deepStrictEqual(
          key.algorithm,
          {
            name: "ECDSA",
            namedCurve: "P-521",
          },
          "key.algorithm",
        );
        strictEqual(key.extractable, true, "key.extractable");
        strictEqual(key.type, "private", "key.type");
        deepStrictEqual(key.usages, ["sign"], "key.usages");
      });

      await t.test("pkcs8-ecdsa-p521-extractable-false", async () => {
        const key = await crypto.subtle.importKey(
          "pkcs8",
          ecdsaP521Pkcs8,
          ecdsaP521Algorithm,
          false,
          ["sign"],
        );

        strictEqual(key instanceof CryptoKey, true, "key instanceof CryptoKey");
        strictEqual(key.extractable, false, "key.extractable");
        strictEqual(key.algorithm.name, "ECDSA", "algorithm name");
        strictEqual(key.algorithm.namedCurve, "P-521", "named curve");
        strictEqual(key.type, "private", "key.type");
        deepStrictEqual(key.usages, ["sign"], "key.usages");
      });

      // RSA 2048-bit with different hash algorithms
      await t.test("pkcs8-rsa-2048-pkcs1-sha1", async () => {
        const key = await crypto.subtle.importKey(
          "pkcs8",
          rsa2048Pkcs8,
          rsaPkcs1Sha1Algorithm,
          true,
          ["sign"],
        );

        strictEqual(key instanceof CryptoKey, true, "key instanceof CryptoKey");
        deepStrictEqual(
          key.algorithm,
          {
            name: "RSASSA-PKCS1-v1_5",
            hash: { name: "SHA-1" },
            modulusLength: 2048,
            publicExponent: new Uint8Array([1, 0, 1]),
          },
          "key.algorithm",
        );
        strictEqual(key.extractable, true, "key.extractable");
        strictEqual(key.type, "private", "key.type");
        deepStrictEqual(key.usages, ["sign"], "key.usages");
      });

      await t.test("pkcs8-rsa-2048-pkcs1-sha256", async () => {
        const key = await crypto.subtle.importKey(
          "pkcs8",
          rsa2048Pkcs8,
          rsaPkcs1Sha256Algorithm,
          true,
          ["sign"],
        );

        strictEqual(key instanceof CryptoKey, true, "key instanceof CryptoKey");
        deepStrictEqual(
          key.algorithm,
          {
            name: "RSASSA-PKCS1-v1_5",
            hash: { name: "SHA-256" },
            modulusLength: 2048,
            publicExponent: new Uint8Array([1, 0, 1]),
          },
          "key.algorithm",
        );
        strictEqual(key.extractable, true, "key.extractable");
        strictEqual(key.type, "private", "key.type");
        deepStrictEqual(key.usages, ["sign"], "key.usages");
      });

      await t.test("pkcs8-rsa-2048-pkcs1-sha384", async () => {
        const key = await crypto.subtle.importKey(
          "pkcs8",
          rsa2048Pkcs8,
          rsaPkcs1Sha384Algorithm,
          false,
          ["sign"],
        );

        strictEqual(key instanceof CryptoKey, true, "key instanceof CryptoKey");
        deepStrictEqual(
          key.algorithm,
          {
            name: "RSASSA-PKCS1-v1_5",
            hash: { name: "SHA-384" },
            modulusLength: 2048,
            publicExponent: new Uint8Array([1, 0, 1]),
          },
          "key.algorithm",
        );
        strictEqual(key.extractable, false, "key.extractable");
        strictEqual(key.type, "private", "key.type");
        deepStrictEqual(key.usages, ["sign"], "key.usages");
      });

      await t.test("pkcs8-rsa-2048-pkcs1-sha512", async () => {
        const key = await crypto.subtle.importKey(
          "pkcs8",
          rsa2048Pkcs8,
          rsaPkcs1Sha512Algorithm,
          true,
          ["sign"],
        );

        strictEqual(key instanceof CryptoKey, true, "key instanceof CryptoKey");
        deepStrictEqual(
          key.algorithm,
          {
            name: "RSASSA-PKCS1-v1_5",
            hash: { name: "SHA-512" },
            modulusLength: 2048,
            publicExponent: new Uint8Array([1, 0, 1]),
          },
          "key.algorithm",
        );
        strictEqual(key.extractable, true, "key.extractable");
        strictEqual(key.type, "private", "key.type");
        deepStrictEqual(key.usages, ["sign"], "key.usages");
      });

      // Test with different input data types
      await t.test("pkcs8-uint8array-input", async () => {
        const uint8ArrayData = new Uint8Array(ecdsaP256Pkcs8);
        const key = await crypto.subtle.importKey(
          "pkcs8",
          uint8ArrayData,
          ecdsaP256Algorithm,
          true,
          ["sign"],
        );

        strictEqual(key instanceof CryptoKey, true, "key instanceof CryptoKey");
        strictEqual(key.algorithm.name, "ECDSA", "algorithm name");
        strictEqual(key.algorithm.namedCurve, "P-256", "named curve");
        strictEqual(key.extractable, true, "key.extractable");
        strictEqual(key.type, "private", "key.type");
        deepStrictEqual(key.usages, ["sign"], "key.usages");
      });

      await t.test("pkcs8-int8array-input", async () => {
        const int8ArrayData = new Int8Array(ecdsaP256Pkcs8);
        const key = await crypto.subtle.importKey(
          "pkcs8",
          int8ArrayData,
          ecdsaP256Algorithm,
          false,
          ["sign"],
        );

        strictEqual(key instanceof CryptoKey, true, "key instanceof CryptoKey");
        strictEqual(key.algorithm.name, "ECDSA", "algorithm name");
        strictEqual(key.algorithm.namedCurve, "P-256", "named curve");
        strictEqual(key.extractable, false, "key.extractable");
        strictEqual(key.type, "private", "key.type");
        deepStrictEqual(key.usages, ["sign"], "key.usages");
      });

      // Test extractable parameter coercion
      await t.test("pkcs8-extractable-coercion", async () => {
        // Test with number (truthy)
        const key1 = await crypto.subtle.importKey(
          "pkcs8",
          ecdsaP256Pkcs8,
          ecdsaP256Algorithm,
          1,
          ["sign"],
        );
        strictEqual(key1.extractable, true, "number 1 coerced to true");

        // Test with number (falsy)
        const key2 = await crypto.subtle.importKey(
          "pkcs8",
          ecdsaP256Pkcs8,
          ecdsaP256Algorithm,
          0,
          ["sign"],
        );
        strictEqual(key2.extractable, false, "number 0 coerced to false");

        // Test with string (truthy)
        const key3 = await crypto.subtle.importKey(
          "pkcs8",
          ecdsaP256Pkcs8,
          ecdsaP256Algorithm,
          "true",
          ["sign"],
        );
        strictEqual(key3.extractable, true, "string 'true' coerced to true");

        // Test with empty string (falsy)
        const key4 = await crypto.subtle.importKey(
          "pkcs8",
          ecdsaP256Pkcs8,
          ecdsaP256Algorithm,
          "",
          ["sign"],
        );
        strictEqual(key4.extractable, false, "empty string coerced to false");
      });

      // Test RSA key size validation
      await t.test("pkcs8-rsa-key-size-validation", async () => {
        // Test that different RSA key sizes are correctly detected
        const key2048 = await crypto.subtle.importKey(
          "pkcs8",
          rsa2048Pkcs8,
          rsaPkcs1Sha256Algorithm,
          true,
          ["sign"],
        );

        const key3072 = await crypto.subtle.importKey(
          "pkcs8",
          rsa3072Pkcs8,
          rsaPkcs1Sha256Algorithm,
          true,
          ["sign"],
        );

        strictEqual(
          key2048.algorithm.modulusLength,
          2048,
          "2048-bit key detected",
        );
        strictEqual(
          key3072.algorithm.modulusLength,
          3072,
          "3072-bit key detected",
        );

        // All should have same public exponent
        deepStrictEqual(
          key2048.algorithm.publicExponent,
          new Uint8Array([1, 0, 1]),
          "2048-bit public exponent",
        );
        deepStrictEqual(
          key3072.algorithm.publicExponent,
          new Uint8Array([1, 0, 1]),
          "3072-bit public exponent",
        );
      });

      // Test ECDSA curve validation
      await t.test("pkcs8-ecdsa-curve-validation", async () => {
        // Test that different ECDSA curves are correctly detected
        const keyP256 = await crypto.subtle.importKey(
          "pkcs8",
          ecdsaP256Pkcs8,
          ecdsaP256Algorithm,
          true,
          ["sign"],
        );

        const keyP384 = await crypto.subtle.importKey(
          "pkcs8",
          ecdsaP384Pkcs8,
          ecdsaP384Algorithm,
          true,
          ["sign"],
        );

        const keyP521 = await crypto.subtle.importKey(
          "pkcs8",
          ecdsaP521Pkcs8,
          ecdsaP521Algorithm,
          true,
          ["sign"],
        );

        strictEqual(
          keyP256.algorithm.namedCurve,
          "P-256",
          "P-256 curve detected",
        );
        strictEqual(
          keyP384.algorithm.namedCurve,
          "P-384",
          "P-384 curve detected",
        );
        strictEqual(
          keyP521.algorithm.namedCurve,
          "P-521",
          "P-521 curve detected",
        );

        // All should be ECDSA
        strictEqual(keyP256.algorithm.name, "ECDSA", "P-256 algorithm");
        strictEqual(keyP384.algorithm.name, "ECDSA", "P-384 algorithm");
        strictEqual(keyP521.algorithm.name, "ECDSA", "P-521 algorithm");
      });

      // SPKI ECDSA Tests
      await t.test("spki-ecdsa-p256", async () => {
        const key = await crypto.subtle.importKey(
          "spki",
          ecdsaP256Spki,
          ecdsaP256Algorithm,
          true,
          ["verify"],
        );

        strictEqual(key instanceof CryptoKey, true, "key instanceof CryptoKey");
        deepStrictEqual(
          key.algorithm,
          {
            name: "ECDSA",
            namedCurve: "P-256",
          },
          "key.algorithm",
        );
        strictEqual(key.extractable, true, "key.extractable");
        strictEqual(key.type, "public", "key.type");
        deepStrictEqual(key.usages, ["verify"], "key.usages");
      });

      await t.test("spki-ecdsa-p384", async () => {
        const key = await crypto.subtle.importKey(
          "spki",
          ecdsaP384Spki,
          ecdsaP384Algorithm,
          true,
          ["verify"],
        );

        strictEqual(key instanceof CryptoKey, true, "key instanceof CryptoKey");
        deepStrictEqual(
          key.algorithm,
          {
            name: "ECDSA",
            namedCurve: "P-384",
          },
          "key.algorithm",
        );
        strictEqual(key.extractable, true, "key.extractable");
        strictEqual(key.type, "public", "key.type");
        deepStrictEqual(key.usages, ["verify"], "key.usages");
      });

      await t.test("spki-ecdsa-p521", async () => {
        const key = await crypto.subtle.importKey(
          "spki",
          ecdsaP521Spki,
          ecdsaP521Algorithm,
          true,
          ["verify"],
        );

        strictEqual(key instanceof CryptoKey, true, "key instanceof CryptoKey");
        deepStrictEqual(
          key.algorithm,
          {
            name: "ECDSA",
            namedCurve: "P-521",
          },
          "key.algorithm",
        );
        strictEqual(key.extractable, true, "key.extractable");
        strictEqual(key.type, "public", "key.type");
        deepStrictEqual(key.usages, ["verify"], "key.usages");
      });

      // SPKI RSA Test
      await t.test("spki-rsa-2048-pkcs1-sha256", async () => {
        const key = await crypto.subtle.importKey(
          "spki",
          rsa2048Spki,
          rsaPkcs1Sha256Algorithm,
          true,
          ["verify"],
        );

        strictEqual(key instanceof CryptoKey, true, "key instanceof CryptoKey");
        deepStrictEqual(
          key.algorithm,
          {
            name: "RSASSA-PKCS1-v1_5",
            hash: { name: "SHA-256" },
            modulusLength: 2048,
            publicExponent: new Uint8Array([1, 0, 1]),
          },
          "key.algorithm",
        );
        strictEqual(key.extractable, true, "key.extractable");
        strictEqual(key.type, "public", "key.type");
        deepStrictEqual(key.usages, ["verify"], "key.usages");
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
        `typeof crypto.subtle.digest`,
      );
      strictEqual(
        crypto.subtle.digest,
        SubtleCrypto.prototype.digest,
        `crypto.subtle.digest === SubtleCrypto.prototype.digest`,
      );
    });
    await t.test("subtle.digest.length", async () => {
      strictEqual(
        crypto.subtle.digest.length,
        2,
        `crypto.subtle.digest.length === 2`,
      );
    });
    await t.test("subtle.digest.called-as-constructor", async () => {
      throws(() => {
        new crypto.subtle.digest();
      }, TypeError);
    });
    await t.test("subtle.digest.called-with-wrong-this", async () => {
      await rejects(async () => {
        await crypto.subtle.digest.call(undefined);
      }, TypeError);
    });
    await t.test("subtle.digest.called-with-no-arguments", async () => {
      await rejects(async () => {
        await crypto.subtle.digest();
      }, TypeError);
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
              data,
            );
          };
          await rejects(test);
          try {
            await test();
          } catch (thrownError) {
            strictEqual(thrownError, sentinel, "thrownError === sentinel");
          }
        },
      );
      await t.test(
        "subtle.digest.first-parameter-non-existent-format",
        async () => {
          await rejects(
            async () => {
              await crypto.subtle.digest("jake", data);
            },
            DOMException,
            null,
            "NotSupportedError",
          );
        },
      );
    }
    // second-parameter
    {
      await t.test("subtle.digest.second-parameter-undefined", async () => {
        await rejects(async () => {
          await crypto.subtle.digest("sha-1", undefined);
        }, TypeError);
      });
    }
    // happy paths
    {
      // "MD5"
      await t.test("subtle.digest.md5", async () => {
        const result = new Uint8Array(
          await crypto.subtle.digest("md5", new Uint8Array()),
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
          await crypto.subtle.digest("sha-1", new Uint8Array()),
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
          await crypto.subtle.digest("sha-256", new Uint8Array()),
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
          await crypto.subtle.digest("sha-384", new Uint8Array()),
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
          await crypto.subtle.digest("sha-512", new Uint8Array()),
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
        `typeof crypto.subtle.sign`,
      );
      strictEqual(
        crypto.subtle.sign,
        SubtleCrypto.prototype.sign,
        `crypto.subtle.sign === SubtleCrypto.prototype.sign`,
      );
    });
    await t.test("subtle.sign.length", async () => {
      strictEqual(
        crypto.subtle.sign.length,
        3,
        `crypto.subtle.sign.length === 3`,
      );
    });
    await t.test("subtle.sign.called-as-constructor", async () => {
      throws(() => {
        new crypto.subtle.sign();
      }, TypeError);
    });
    await t.test("subtle.sign.called-with-wrong-this", async () => {
      const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
      await rejects(async () => {
        await crypto.subtle.sign.call(
          undefined,
          createRsaJsonWebKeyAlgorithm(),
          publicRsaJsonWebKeyData,
          data,
        );
      }, TypeError);
    });
    await t.test("subtle.sign.called-with-no-arguments", async () => {
      await rejects(async () => {
        await crypto.subtle.sign();
      }, TypeError);
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
            privateRsaJsonWebKeyData.key_ops,
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
              data,
            );
          };
          await rejects(test);
          try {
            await test();
          } catch (thrownError) {
            strictEqual(thrownError, sentinel, "thrownError === sentinel");
          }
        },
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
                privateRsaJsonWebKeyData.key_ops,
              );
              await crypto.subtle.sign("jake", key, data);
            },
            DOMException,
            null,
            "NotSupportedError",
          );
        },
      );
    }
    // second-parameter
    {
      await t.test("subtle.sign.second-parameter-invalid-format", async () => {
        await rejects(async () => {
          await crypto.subtle.sign(
            createRsaJsonWebKeyAlgorithm(),
            "jake",
            data,
          );
        }, TypeError);
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
              publicRsaJsonWebKeyData.key_ops,
            );
            await crypto.subtle.sign(createRsaJsonWebKeyAlgorithm(), key, data);
          },
          DOMException,
          null,
          "InvalidAccessError",
        );
      });
    }
    // third-parameter
    {
      await t.test("subtle.sign.third-parameter-invalid-format", async () => {
        const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
        await rejects(async () => {
          const key = await crypto.subtle.importKey(
            "jwk",
            publicRsaJsonWebKeyData,
            createRsaJsonWebKeyAlgorithm(),
            publicRsaJsonWebKeyData.ext,
            publicRsaJsonWebKeyData.key_ops,
          );
          await crypto.subtle.sign(
            createRsaJsonWebKeyAlgorithm(),
            key,
            undefined,
          );
        }, TypeError);
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
          privateRsaJsonWebKeyData.key_ops,
        );
        const signature = new Uint8Array(
          await crypto.subtle.sign(createRsaJsonWebKeyAlgorithm(), key, data),
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
            ["sign", "verify"],
          );
          // Sign the message with HMAC and the CryptoKey
          const signature = new Uint8Array(
            await globalThis.crypto.subtle.sign("HMAC", key, messageUint8Array),
          );
          const expected = results[algorithm];
          deepStrictEqual(
            signature,
            expected,
            `${algorithm} signature deep equals expected`,
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
        `typeof crypto.subtle.verify`,
      );
      strictEqual(
        crypto.subtle.verify,
        SubtleCrypto.prototype.verify,
        `crypto.subtle.verify === SubtleCrypto.prototype.verify`,
      );
    });
    await t.test("subtle.verify.length", async () => {
      strictEqual(
        crypto.subtle.verify.length,
        4,
        `crypto.subtle.verify.length === 4`,
      );
    });
    await t.test("subtle.verify.called-as-constructor", async () => {
      throws(() => {
        new crypto.subtle.verify();
      }, TypeError);
    });
    await t.test("subtle.verify.called-with-wrong-this", async () => {
      const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
      await rejects(async () => {
        const key = await crypto.subtle.importKey(
          "jwk",
          publicRsaJsonWebKeyData,
          createRsaJsonWebKeyAlgorithm(),
          publicRsaJsonWebKeyData.ext,
          publicRsaJsonWebKeyData.key_ops,
        );
        await crypto.subtle.verify.call(
          undefined,
          createRsaJsonWebKeyAlgorithm(),
          key,
          new Uint8Array(),
          new Uint8Array(),
        );
      }, TypeError);
    });
    await t.test("subtle.verify.called-with-no-arguments", async () => {
      await rejects(async () => {
        await crypto.subtle.verify();
      }, TypeError);
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
              publicRsaJsonWebKeyData.key_ops,
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
              new Uint8Array(),
            );
          };
          await rejects(test);
          try {
            await test();
          } catch (thrownError) {
            strictEqual(thrownError, sentinel, "thrownError === sentinel");
          }
        },
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
                publicRsaJsonWebKeyData.key_ops,
              );
              await crypto.subtle.verify(
                "jake",
                key,
                new Uint8Array(),
                new Uint8Array(),
              );
            },
            DOMException,
            null,
            "NotSupportedError",
          );
        },
      );
    }
    // second-parameter
    {
      await t.test(
        "subtle.verify.second-parameter-invalid-format",
        async () => {
          await rejects(async () => {
            await crypto.subtle.verify(
              createRsaJsonWebKeyAlgorithm(),
              "jake",
              new Uint8Array(),
              new Uint8Array(),
            );
          }, TypeError);
        },
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
                privateRsaJsonWebKeyData.key_ops,
              );
              await crypto.subtle.verify(
                createRsaJsonWebKeyAlgorithm(),
                key,
                new Uint8Array(),
                new Uint8Array(),
              );
            },
            DOMException,
            null,
            "InvalidAccessError",
          );
        },
      );
    }
    // third-parameter
    {
      await t.test("subtle.verify.third-parameter-invalid-format", async () => {
        const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
        await rejects(async () => {
          const key = await crypto.subtle.importKey(
            "jwk",
            publicRsaJsonWebKeyData,
            createRsaJsonWebKeyAlgorithm(),
            publicRsaJsonWebKeyData.ext,
            publicRsaJsonWebKeyData.key_ops,
          );
          await crypto.subtle.verify(
            createRsaJsonWebKeyAlgorithm(),
            key,
            undefined,
            new Uint8Array(),
          );
        }, TypeError);
      });
    }
    // fourth-parameter
    {
      await t.test(
        "subtle.verify.fourth-parameter-invalid-format",
        async () => {
          const publicRsaJsonWebKeyData = createPublicRsaJsonWebKeyData();
          await rejects(async () => {
            const key = await crypto.subtle.importKey(
              "jwk",
              publicRsaJsonWebKeyData,
              createRsaJsonWebKeyAlgorithm(),
              publicRsaJsonWebKeyData.ext,
              publicRsaJsonWebKeyData.key_ops,
            );
            await crypto.subtle.verify(
              createRsaJsonWebKeyAlgorithm(),
              key,
              new Uint8Array(),
              undefined,
            );
          }, TypeError);
        },
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
          publicRsaJsonWebKeyData.key_ops,
        );
        const signature = new Uint8Array();
        const enc = new TextEncoder();
        const data = enc.encode("hello world");
        const result = await crypto.subtle.verify(
          createRsaJsonWebKeyAlgorithm(),
          key,
          signature,
          data,
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
            ["sign", "verify"],
          );
          const result = await crypto.subtle.verify(
            "HMAC",
            key,
            signature,
            data,
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
          privateRsaJsonWebKeyData.key_ops,
        );
        const key = await crypto.subtle.importKey(
          "jwk",
          publicRsaJsonWebKeyData,
          createRsaJsonWebKeyAlgorithm(),
          publicRsaJsonWebKeyData.ext,
          publicRsaJsonWebKeyData.key_ops,
        );
        const enc = new TextEncoder();
        const data = enc.encode("hello world");
        const signature = await crypto.subtle.sign(
          createRsaJsonWebKeyAlgorithm(),
          pkey,
          data,
        );
        const result = await crypto.subtle.verify(
          createRsaJsonWebKeyAlgorithm(),
          key,
          signature,
          data,
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
          privateEcdsaJsonWebKeyData.key_ops,
        );
        const key = await crypto.subtle.importKey(
          "jwk",
          publicEcdsaJsonWebKeyData,
          ecdsaJsonWebKeyAlgorithm,
          publicEcdsaJsonWebKeyData.ext,
          publicEcdsaJsonWebKeyData.key_ops,
        );
        const enc = new TextEncoder();
        const data = enc.encode("hello world");
        const signature = await crypto.subtle.sign(
          ecdsaJsonWebKeyAlgorithm,
          pkey,
          data,
        );
        const result = await crypto.subtle.verify(
          ecdsaJsonWebKeyAlgorithm,
          key,
          signature,
          data,
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
            ["sign", "verify"],
          );
          // Sign the message with HMAC and the CryptoKey
          // const signature = new Uint8Array(await globalThis.crypto.subtle.sign("HMAC", key, messageUint8Array));
          const signature = results[algorithm];
          const result = await crypto.subtle.verify(
            "HMAC",
            key,
            signature,
            messageUint8Array,
          );
          strictEqual(result, true, "result");
        }
      });
    }
  }

  // Ed25519
  {
    const ed25519Algorithm = { name: "Ed25519" };
    const assertEd25519Key = (key, type, extractable, usages) => {
      strictEqual(key instanceof CryptoKey, true, "key instanceof CryptoKey");
      deepStrictEqual(key.algorithm, { name: "Ed25519" }, "key.algorithm");
      strictEqual(key.extractable, extractable, "key.extractable");
      strictEqual(key.type, type, "key.type");
      deepStrictEqual(key.usages, usages, "key.usages");
    };

    // importKey happy paths
    await t.test("ed25519-import-raw", async () => {
      const key = await crypto.subtle.importKey("raw", ed25519Test1Raw, ed25519Algorithm, true, ["verify"]);
      assertEd25519Key(key, "public", true, ["verify"]);
    });
    await t.test("ed25519-import-raw-empty-usages", async () => {
      const key = await crypto.subtle.importKey("raw", ed25519Test1Raw, ed25519Algorithm, false, []);
      assertEd25519Key(key, "public", false, []);
    });
    await t.test("ed25519-import-spki", async () => {
      const key = await crypto.subtle.importKey("spki", ed25519Test2Spki, ed25519Algorithm, true, ["verify"]);
      assertEd25519Key(key, "public", true, ["verify"]);
    });
    await t.test("ed25519-import-pkcs8", async () => {
      const key = await crypto.subtle.importKey("pkcs8", ed25519Test2Pkcs8, ed25519Algorithm, false, ["sign"]);
      assertEd25519Key(key, "private", false, ["sign"]);
    });
    await t.test("ed25519-import-jwk-public", async () => {
      const jwk = ed25519Test1PublicJsonWebKeyData();
      const key = await crypto.subtle.importKey("jwk", jwk, ed25519Algorithm, jwk.ext, jwk.key_ops);
      assertEd25519Key(key, "public", true, ["verify"]);
    });
    await t.test("ed25519-import-jwk-private", async () => {
      const jwk = ed25519Test1PrivateJsonWebKeyData();
      const key = await crypto.subtle.importKey("jwk", jwk, ed25519Algorithm, jwk.ext, jwk.key_ops);
      assertEd25519Key(key, "private", true, ["sign"]);
    });
    await t.test("ed25519-import-jwk-alg", async () => {
      for (const alg of ["Ed25519", "EdDSA"]) {
        const jwk = { ...ed25519Test1PublicJsonWebKeyData(), alg };
        const key = await crypto.subtle.importKey("jwk", jwk, ed25519Algorithm, true, ["verify"]);
        assertEd25519Key(key, "public", true, ["verify"]);
      }
    });
    await t.test("ed25519-import-algorithm-name-is-case-insensitive", async () => {
      const key = await crypto.subtle.importKey("raw", ed25519Test1Raw, "ed25519", true, ["verify"]);
      assertEd25519Key(key, "public", true, ["verify"]);
    });

    // importKey failures
    await t.test("ed25519-import-raw-sign-usage", async () => {
      await rejects(async () => {
        await crypto.subtle.importKey("raw", ed25519Test1Raw, ed25519Algorithm, true, ["sign"]);
      }, DOMException, undefined, "SyntaxError");
    });
    await t.test("ed25519-import-raw-wrong-length", async () => {
      await rejects(async () => {
        await crypto.subtle.importKey("raw", ed25519Test1Raw.slice(1), ed25519Algorithm, true, ["verify"]);
      }, DOMException, undefined, "DataError");
    });
    await t.test("ed25519-import-spki-sign-usage", async () => {
      await rejects(async () => {
        await crypto.subtle.importKey("spki", ed25519Test2Spki, ed25519Algorithm, true, ["sign"]);
      }, DOMException, undefined, "SyntaxError");
    });
    await t.test("ed25519-import-spki-trailing-data", async () => {
      const spki = new Uint8Array([...ed25519Test2Spki, 0]);
      await rejects(async () => {
        await crypto.subtle.importKey("spki", spki, ed25519Algorithm, true, ["verify"]);
      }, DOMException, undefined, "DataError");
    });
    await t.test("ed25519-import-spki-ecdsa-key", async () => {
      await rejects(async () => {
        await crypto.subtle.importKey("spki", ecdsaP256Spki, ed25519Algorithm, true, ["verify"]);
      }, DOMException, undefined, "DataError");
    });
    await t.test("ed25519-import-pkcs8-verify-usage", async () => {
      await rejects(async () => {
        await crypto.subtle.importKey("pkcs8", ed25519Test2Pkcs8, ed25519Algorithm, true, ["verify"]);
      }, DOMException, undefined, "SyntaxError");
    });
    await t.test("ed25519-import-pkcs8-empty-usages", async () => {
      await rejects(async () => {
        await crypto.subtle.importKey("pkcs8", ed25519Test2Pkcs8, ed25519Algorithm, true, []);
      }, DOMException, undefined, "SyntaxError");
    });
    await t.test("ed25519-import-pkcs8-ecdsa-key", async () => {
      await rejects(async () => {
        await crypto.subtle.importKey("pkcs8", ecdsaP256Pkcs8, ed25519Algorithm, true, ["sign"]);
      }, DOMException, undefined, "DataError");
    });
    await t.test("ed25519-import-jwk-private-empty-usages", async () => {
      await rejects(async () => {
        await crypto.subtle.importKey("jwk", ed25519Test1PrivateJsonWebKeyData(), ed25519Algorithm, true, []);
      }, DOMException, undefined, "SyntaxError");
    });
    await t.test("ed25519-import-jwk-wrong-kty", async () => {
      const jwk = { ...ed25519Test1PublicJsonWebKeyData(), kty: "EC" };
      await rejects(async () => {
        await crypto.subtle.importKey("jwk", jwk, ed25519Algorithm, true, ["verify"]);
      }, DOMException, undefined, "DataError");
    });
    await t.test("ed25519-import-jwk-wrong-crv", async () => {
      const jwk = { ...ed25519Test1PublicJsonWebKeyData(), crv: "X25519" };
      await rejects(async () => {
        await crypto.subtle.importKey("jwk", jwk, ed25519Algorithm, true, ["verify"]);
      }, DOMException, undefined, "DataError");
    });
    await t.test("ed25519-import-jwk-wrong-alg", async () => {
      const jwk = { ...ed25519Test1PublicJsonWebKeyData(), alg: "ES256" };
      await rejects(async () => {
        await crypto.subtle.importKey("jwk", jwk, ed25519Algorithm, true, ["verify"]);
      }, DOMException, undefined, "DataError");
    });
    await t.test("ed25519-import-jwk-missing-x", async () => {
      const { x, ...jwk } = ed25519Test1PrivateJsonWebKeyData();
      await rejects(async () => {
        await crypto.subtle.importKey("jwk", jwk, ed25519Algorithm, true, ["sign"]);
      }, DOMException, undefined, "DataError");
    });
    await t.test("ed25519-import-jwk-mismatched-x-and-d", async () => {
      const jwk = { ...ed25519Test1PrivateJsonWebKeyData(), x: "PUAXw-hDiVqStwqnTRt-vJyYLM8uxJaMwM1V8Sr0Zgw" };
      await rejects(async () => {
        await crypto.subtle.importKey("jwk", jwk, ed25519Algorithm, true, ["sign"]);
      }, DOMException, undefined, "DataError");
    });
    await t.test("ed25519-import-jwk-key-ops-not-superset", async () => {
      const jwk = { ...ed25519Test1PublicJsonWebKeyData(), key_ops: ["sign"] };
      await rejects(async () => {
        await crypto.subtle.importKey("jwk", jwk, ed25519Algorithm, true, ["verify"]);
      }, DOMException, undefined, "DataError");
    });

    // sign
    await t.test("ed25519-sign-pkcs8", async () => {
      const key = await crypto.subtle.importKey("pkcs8", ed25519Test2Pkcs8, ed25519Algorithm, false, ["sign"]);
      const signature = new Uint8Array(await crypto.subtle.sign(ed25519Algorithm, key, ed25519Test2Message));
      deepStrictEqual(signature, ed25519Test2Signature, "signature deep equals expected");
    });
    await t.test("ed25519-sign-jwk-empty-message", async () => {
      const jwk = ed25519Test1PrivateJsonWebKeyData();
      const key = await crypto.subtle.importKey("jwk", jwk, ed25519Algorithm, false, ["sign"]);
      const signature = new Uint8Array(await crypto.subtle.sign("Ed25519", key, new Uint8Array()));
      deepStrictEqual(signature, ed25519Test1Signature, "signature deep equals expected");
    });
    await t.test("ed25519-sign-with-public-key", async () => {
      const key = await crypto.subtle.importKey("raw", ed25519Test1Raw, ed25519Algorithm, false, ["verify"]);
      await rejects(async () => {
        await crypto.subtle.sign(ed25519Algorithm, key, new Uint8Array());
      }, DOMException, undefined, "InvalidAccessError");
    });
    await t.test("ed25519-sign-with-ecdsa-algorithm", async () => {
      const key = await crypto.subtle.importKey("pkcs8", ed25519Test2Pkcs8, ed25519Algorithm, false, ["sign"]);
      await rejects(async () => {
        await crypto.subtle.sign({ name: "ECDSA", hash: "SHA-256" }, key, ed25519Test2Message);
      }, DOMException, undefined, "InvalidAccessError");
    });

    // verify
    await t.test("ed25519-verify-spki", async () => {
      const key = await crypto.subtle.importKey("spki", ed25519Test2Spki, ed25519Algorithm, false, ["verify"]);
      const result = await crypto.subtle.verify(ed25519Algorithm, key, ed25519Test2Signature, ed25519Test2Message);
      strictEqual(result, true, "result");
    });
    await t.test("ed25519-verify-raw-empty-message", async () => {
      const key = await crypto.subtle.importKey("raw", ed25519Test1Raw, ed25519Algorithm, false, ["verify"]);
      const result = await crypto.subtle.verify(ed25519Algorithm, key, ed25519Test1Signature, new Uint8Array());
      strictEqual(result, true, "result");
    });
    await t.test("ed25519-verify-jwk", async () => {
      const jwk = ed25519Test1PublicJsonWebKeyData();
      const key = await crypto.subtle.importKey("jwk", jwk, ed25519Algorithm, false, ["verify"]);
      const result = await crypto.subtle.verify(ed25519Algorithm, key, ed25519Test1Signature, new Uint8Array());
      strictEqual(result, true, "result");
    });
    await t.test("ed25519-verify-sign-round-trip", async () => {
      const privateKey = await crypto.subtle.importKey("pkcs8", ed25519Test2Pkcs8, ed25519Algorithm, false, ["sign"]);
      const publicKey = await crypto.subtle.importKey("spki", ed25519Test2Spki, ed25519Algorithm, false, ["verify"]);
      const message = new TextEncoder().encode("StarlingMonkey");
      const signature = await crypto.subtle.sign(ed25519Algorithm, privateKey, message);
      strictEqual(await crypto.subtle.verify(ed25519Algorithm, publicKey, signature, message), true, "result");
    });
    await t.test("ed25519-verify-altered-signature", async () => {
      const key = await crypto.subtle.importKey("spki", ed25519Test2Spki, ed25519Algorithm, false, ["verify"]);
      const signature = ed25519Test2Signature.slice();
      signature[0] ^= 1;
      const result = await crypto.subtle.verify(ed25519Algorithm, key, signature, ed25519Test2Message);
      strictEqual(result, false, "result");
    });
    await t.test("ed25519-verify-altered-message", async () => {
      const key = await crypto.subtle.importKey("spki", ed25519Test2Spki, ed25519Algorithm, false, ["verify"]);
      const result = await crypto.subtle.verify(ed25519Algorithm, key, ed25519Test2Signature, new Uint8Array([0x73]));
      strictEqual(result, false, "result");
    });
    await t.test("ed25519-verify-wrong-signature-length", async () => {
      const key = await crypto.subtle.importKey("spki", ed25519Test2Spki, ed25519Algorithm, false, ["verify"]);
      for (const signature of [ed25519Test2Signature.slice(1), new Uint8Array([...ed25519Test2Signature, 0])]) {
        const result = await crypto.subtle.verify(ed25519Algorithm, key, signature, ed25519Test2Message);
        strictEqual(result, false, "result");
      }
    });
    await t.test("ed25519-verify-with-private-key", async () => {
      const key = await crypto.subtle.importKey("pkcs8", ed25519Test2Pkcs8, ed25519Algorithm, false, ["sign"]);
      await rejects(async () => {
        await crypto.subtle.verify(ed25519Algorithm, key, ed25519Test2Signature, ed25519Test2Message);
      }, DOMException, undefined, "InvalidAccessError");
    });

    // PBKDF2
    // Expected outputs generated with Node.js crypto.subtle (OpenSSL), cross-checked against the
    // RFC 6070 / RFC 7914 PBKDF2 test vectors.
    const hexToBytes = (hex) => new Uint8Array(hex.match(/../g).map((b) => parseInt(b, 16)));
    const pbkdf2Password = new TextEncoder().encode("password");
    const pbkdf2Salt = new TextEncoder().encode("salt");
    const pbkdf2Params = (hash, iterations = 1) => ({ name: "PBKDF2", salt: pbkdf2Salt, iterations, hash });

    // importKey
    await t.test("pbkdf2-importKey-raw", async () => {
      const key = await crypto.subtle.importKey("raw", pbkdf2Password, "PBKDF2", false, ["deriveBits"]);
      strictEqual(key.type, "secret", "type");
      deepStrictEqual(key.algorithm, { name: "PBKDF2" }, "algorithm");
      deepStrictEqual(key.usages, ["deriveBits"], "usages");
      strictEqual(key.extractable, false, "extractable");
    });
    await t.test("pbkdf2-importKey-raw-extractable", async () => {
      const key = await crypto.subtle.importKey("raw", pbkdf2Password, { name: "PBKDF2" }, true, ["deriveKey"]);
      strictEqual(key.extractable, true, "extractable");
      deepStrictEqual(key.usages, ["deriveKey"], "usages");
    });
    await t.test("pbkdf2-importKey-jwk-not-supported", async () => {
      await rejects(async () => {
        await crypto.subtle.importKey("jwk", { kty: "oct", k: "cGFzc3dvcmQ" }, "PBKDF2", false, ["deriveBits"]);
      }, DOMException, undefined, "NotSupportedError");
    });
    await t.test("pbkdf2-importKey-wrong-usages", async () => {
      for (const usages of [["sign"], ["verify"], ["encrypt"], ["deriveBits", "sign"], ["unwrapKey"]]) {
        await rejects(async () => {
          await crypto.subtle.importKey("raw", pbkdf2Password, "PBKDF2", false, usages);
        }, DOMException, undefined, "SyntaxError");
      }
    });
    await t.test("pbkdf2-importKey-empty-key-data", async () => {
      await rejects(async () => {
        await crypto.subtle.importKey("raw", new Uint8Array(), "PBKDF2", false, ["deriveBits"]);
      }, DOMException, undefined, "DataError");
    });
    await t.test("pbkdf2-importKey-case-insensitive-name", async () => {
      const key = await crypto.subtle.importKey("raw", pbkdf2Password, "pbkdf2", false, ["deriveBits"]);
      strictEqual(key.algorithm.name, "PBKDF2", "algorithm name");
    });

    // deriveBits
    const pbkdf2Vectors = [
      // password "password", salt "salt"
      { hash: "SHA-1", iterations: 1, length: 160, expected: "0c60c80f961f0e71f3a9b524af6012062fe037a6" },
      { hash: "SHA-1", iterations: 2, length: 160, expected: "ea6c014dc72d6f8ccd1ed92ace1d41f0d8de8957" },
      { hash: "SHA-256", iterations: 1, length: 256, expected: "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b" },
      { hash: "SHA-256", iterations: 2, length: 256, expected: "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43" },
      { hash: "SHA-384", iterations: 1, length: 384, expected: "c0e14f06e49e32d73f9f52ddf1d0c5c7191609233631dadd76a567db42b78676b38fc800cc53ddb642f5c74442e62be4" },
      { hash: "SHA-512", iterations: 1, length: 512, expected: "867f70cf1ade02cff3752599a3a53dc4af34c7a669815ae5d513554e1c8cf252c02d470a285a0501bad999bfe943c08f050235d7d68b1da55e63f73b60a57fce" },
      { hash: "SHA-256", iterations: 1, length: 384, expected: "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b4dbf3a2f3dad3377264bb7b8e8330d4e" },
      { hash: "SHA-256", iterations: 1, length: 8, expected: "12" },
    ];
    for (const { hash, iterations, length, expected } of pbkdf2Vectors) {
      await t.test(`pbkdf2-deriveBits-${hash}-${iterations}-${length}`, async () => {
        const key = await crypto.subtle.importKey("raw", pbkdf2Password, "PBKDF2", false, ["deriveBits"]);
        const bits = new Uint8Array(await crypto.subtle.deriveBits(pbkdf2Params(hash, iterations), key, length));
        deepStrictEqual(bits, hexToBytes(expected), "derived bits deep equal expected");
      });
    }
    await t.test("pbkdf2-deriveBits-rfc7914", async () => {
      // RFC 7914 vectors: P = "passwordPASSWORDpassword", S = "saltSALTsaltSALTsaltSALTsaltSALTsalt", c = 4096
      const key = await crypto.subtle.importKey("raw", new TextEncoder().encode("passwordPASSWORDpassword"), "PBKDF2", false, ["deriveBits"]);
      const salt = new TextEncoder().encode("saltSALTsaltSALTsaltSALTsaltSALTsalt");
      const bits256 = new Uint8Array(await crypto.subtle.deriveBits({ name: "PBKDF2", salt, iterations: 4096, hash: "SHA-256" }, key, 256));
      deepStrictEqual(bits256, hexToBytes("348c89dbcbd32b2f32d814b8116e84cf2b17347ebc1800181c4e2a1fb8dd53e1"), "SHA-256 derived bits");
      const bits512 = new Uint8Array(await crypto.subtle.deriveBits({ name: "PBKDF2", salt, iterations: 4096, hash: "SHA-512" }, key, 512));
      deepStrictEqual(bits512, hexToBytes("8c0511f4c6e597c6ac6315d8f0362e225f3c501495ba23b868c005174dc4ee71115b59f9e60cd9532fa33e0f75aefe30225c583a186cd82bd4daea9724a3d3b8"), "SHA-512 derived bits");
    });
    await t.test("pbkdf2-deriveBits-zero-iterations", async () => {
      const key = await crypto.subtle.importKey("raw", pbkdf2Password, "PBKDF2", false, ["deriveBits"]);
      await rejects(async () => {
        await crypto.subtle.deriveBits(pbkdf2Params("SHA-256", 0), key, 256);
      }, DOMException, undefined, "OperationError");
    });
    await t.test("pbkdf2-deriveBits-zero-length", async () => {
      const key = await crypto.subtle.importKey("raw", pbkdf2Password, "PBKDF2", false, ["deriveBits"]);
      const bits = await crypto.subtle.deriveBits(pbkdf2Params("SHA-256"), key, 0);
      strictEqual(bits.byteLength, 0, "derived bits length");
    });
    await t.test("pbkdf2-deriveBits-length-not-multiple-of-8", async () => {
      const key = await crypto.subtle.importKey("raw", pbkdf2Password, "PBKDF2", false, ["deriveBits"]);
      for (const length of [1, 7, 9, 230, 255]) {
        await rejects(async () => {
          await crypto.subtle.deriveBits(pbkdf2Params("SHA-256"), key, length);
        }, DOMException, undefined, "OperationError");
      }
    });
    await t.test("pbkdf2-deriveBits-length-null-or-missing", async () => {
      const key = await crypto.subtle.importKey("raw", pbkdf2Password, "PBKDF2", false, ["deriveBits"]);
      for (const length of [null, undefined]) {
        await rejects(async () => {
          await crypto.subtle.deriveBits(pbkdf2Params("SHA-256"), key, length);
        }, DOMException, undefined, "OperationError");
      }
      await rejects(async () => {
        await crypto.subtle.deriveBits(pbkdf2Params("SHA-256"), key);
      }, DOMException, undefined, "OperationError");
    });
    await t.test("pbkdf2-deriveBits-length-out-of-range", async () => {
      const key = await crypto.subtle.importKey("raw", pbkdf2Password, "PBKDF2", false, ["deriveBits"]);
      // [EnforceRange] unsigned long: non-integral or out-of-range lengths are TypeErrors
      for (const length of [NaN, Infinity, -8, 1.5, 2 ** 32 + 8]) {
        await rejects(async () => {
          await crypto.subtle.deriveBits(pbkdf2Params("SHA-256"), key, length);
        }, TypeError);
      }
    });
    await t.test("pbkdf2-deriveBits-iterations-out-of-range", async () => {
      const key = await crypto.subtle.importKey("raw", pbkdf2Password, "PBKDF2", false, ["deriveBits"]);
      // [EnforceRange] unsigned long: non-integral or out-of-range iterations are TypeErrors
      for (const iterations of [NaN, Infinity, -1, 1.5, 2 ** 32]) {
        await rejects(async () => {
          await crypto.subtle.deriveBits(pbkdf2Params("SHA-256", iterations), key, 256);
        }, TypeError);
      }
    });
    await t.test("pbkdf2-deriveBits-missing-params", async () => {
      const key = await crypto.subtle.importKey("raw", pbkdf2Password, "PBKDF2", false, ["deriveBits"]);
      // salt, iterations and hash are all required members of Pbkdf2Params
      for (const algorithm of [
        { name: "PBKDF2", iterations: 1, hash: "SHA-256" },
        { name: "PBKDF2", salt: pbkdf2Salt, hash: "SHA-256" },
        { name: "PBKDF2", salt: pbkdf2Salt, iterations: 1 },
        "PBKDF2",
      ]) {
        await rejects(async () => {
          await crypto.subtle.deriveBits(algorithm, key, 256);
        }, TypeError);
      }
    });
    await t.test("pbkdf2-deriveBits-wrong-key-algorithm", async () => {
      const pbkdf2Key = await crypto.subtle.importKey("raw", pbkdf2Password, "PBKDF2", false, ["deriveBits"]);
      const hmacKey = await crypto.subtle.importKey("raw", pbkdf2Password, { name: "HMAC", hash: "SHA-256" }, false, ["sign"]);
      await rejects(async () => {
        await crypto.subtle.deriveBits(pbkdf2Params("SHA-256"), hmacKey, 256);
      }, DOMException, undefined, "InvalidAccessError");
      await rejects(async () => {
        await crypto.subtle.sign("HMAC", pbkdf2Key, new Uint8Array(1));
      }, DOMException, undefined, "InvalidAccessError");
    });
    await t.test("pbkdf2-deriveBits-key-without-usage", async () => {
      const key = await crypto.subtle.importKey("raw", pbkdf2Password, "PBKDF2", false, ["deriveKey"]);
      await rejects(async () => {
        await crypto.subtle.deriveBits(pbkdf2Params("SHA-256"), key, 256);
      }, DOMException, undefined, "InvalidAccessError");
    });
    await t.test("pbkdf2-deriveBits-non-cryptokey", async () => {
      await rejects(async () => {
        await crypto.subtle.deriveBits(pbkdf2Params("SHA-256"), {}, 256);
      }, TypeError);
    });
    await t.test("pbkdf2-deriveBits-unsupported-algorithms", async () => {
      const key = await crypto.subtle.importKey("raw", pbkdf2Password, "PBKDF2", false, ["deriveBits"]);
      // Algorithms which are recognized but don't support the deriveBits operation
      for (const name of ["AES-CBC", "HMAC", "SHA-256", "Ed25519"]) {
        await rejects(async () => {
          await crypto.subtle.deriveBits({ name }, key, 256);
        }, DOMException, undefined, "NotSupportedError");
      }
      // Key derivation algorithms which are not implemented yet
      for (const name of ["HKDF", "ECDH"]) {
        await rejects(async () => {
          await crypto.subtle.deriveBits({ name }, key, 256);
        }, DOMException, undefined, "NotSupportedError");
      }
      await rejects(async () => {
        await crypto.subtle.deriveBits({ name: "NOPE" }, key, 256);
      }, DOMException, undefined, "NotSupportedError");
    });
  }
});
