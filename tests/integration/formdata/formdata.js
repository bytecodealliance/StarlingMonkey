import { serveTest } from "../test-server.js";
import { assert, strictEqual, deepStrictEqual } from "../../assert.js";

// Adopted from wpt tests

const kTestChars = "ABC~‾¥≈¤･・•∙·☼★星🌟星★☼·∙•・･¤≈¥‾~XYZ";

async function formDataPostFileUploadTest(fileBaseName) {
  const formData = new FormData();
  let file = new Blob([kTestChars], { type: "text/plain" });
  try {
    // Switch to File in browsers that allow this
    file = new File([file], fileBaseName, { type: file.type });
  } catch (ignoredException) {
  }

  formData.append("filename", fileBaseName);
  formData.append(fileBaseName, "filename");
  formData.append("file", file, fileBaseName);

  const req = new Request('about:blank', {
    method: 'POST',
    body: formData,
  });

  const formDataText = await req.text();
  const formDataLines = formDataText.split("\r\n");
  if (formDataLines.length && !formDataLines[formDataLines.length - 1]) {
    --formDataLines.length;
  }

  assert(
    formDataLines.length > 2,
    `${fileBaseName}: multipart form data must have at least 3 lines: ${
      JSON.stringify(formDataText)
    }`,
  );

  const boundary = formDataLines[0];
  assert(
    formDataLines[formDataLines.length - 1] === boundary + "--",
    `${fileBaseName}: multipart form data must end with ${boundary}--: ${
      JSON.stringify(formDataText)
    }`,
  );

  const asValue = fileBaseName.replace(/\r\n?|\n/g, "\r\n");
  const asName = asValue.replace(/[\r\n"]/g, encodeURIComponent);
  const asFilename = fileBaseName.replace(/[\r\n"]/g, encodeURIComponent);
  const expectedText = [
    boundary,
    'Content-Disposition: form-data; name="filename"',
    "",
    asValue,
    boundary,
    `Content-Disposition: form-data; name="${asName}"`,
    "",
    "filename",
    boundary,
    `Content-Disposition: form-data; name="file"; ` +
    `filename="${asFilename}"`,
    "Content-Type: text/plain",
    "",
    kTestChars,
    boundary + "--",
  ].join("\r\n");

  strictEqual(
    formDataText, expectedText,
    `Unexpected multipart-shaped form data received:\n${formDataText}\nExpected:\n${expectedText}`,
  );
}

export const handler = serveTest(async (t) => {
  await t.test("ASCII", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form.txt")
  );

  await t.test("x-user-defined", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-\uF7F0\uF793\uF783\uF7A0.txt")
  );

  await t.test("windows-1252", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-☺😂.txt")
  );

  await t.test("JIS X 0201 and JIS X 0208", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-???.txt")
  );

  await t.test("Unicode-1", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-???.txt")
  );

  await t.test("Unicode-2", async () =>
    formDataPostFileUploadTest(`file-for-upload-in-form-${kTestChars}.txt`)
  );

  await t.test("ASCII-with-NUL", async () =>
  formDataPostFileUploadTest("file-for-upload-in-form-NUL-[\0].txt")
  );

  await t.test("ASCII-with-BS", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-BS-[\b].txt")
  );

  await t.test("ASCII-with-VT", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-VT-[\v].txt")
  );

  await t.test("ASCII-with-LF", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-LF-[\n].txt")
  );

  await t.test("ASCII-with-LFCR", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-LF-CR-[\n\r].txt")
  );

  await t.test("ASCII-with-CR", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-CR-[\r].txt")
  );

  await t.test("ASCII-with-CRLF", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-CR-LF-[\r\n].txt")
  );

  await t.test("ASCII-with-HT", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-HT-[\t].txt")
  );

  await t.test("ASCII-with-FF", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-FF-[\f].txt")
  );

  await t.test("ASCII-with-DEL", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-DEL-[\x7F].txt")
  );

  await t.test("ASCII-with-ESC", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-ESC-[\x1B].txt")
  );

  await t.test("ASCII-with-SPACE", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-SPACE-[ ].txt")
  );
  await t.test("ASCII-with-QUOTATION-MARK", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-QUOTATION-MARK-[\x22].txt")
  );

  await t.test("ASCII-with-double-quoted", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-double-quoted.txt")
  );

  await t.test("ASCII-with-REVERSE-SOLIDUS", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-REVERSE-SOLIDUS-[\\].txt")
  );

  await t.test("ASCII-with-EXCLAMATION-MARK", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-EXCLAMATION-MARK-[!].txt")
  );

  await t.test("ASCII-with-DOLLAR-SIGN", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-DOLLAR-SIGN-[$].txt")
  );

  await t.test("ASCII-with-PERCENT-SIGN", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-PERCENT-SIGN-[%].txt")
  );

  await t.test("ASCII-with-AMPERSAND", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-AMPERSAND-[&].txt")
  );

  await t.test("ASCII-with-APOSTROPHE", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-APOSTROPHE-['].txt")
  );

  await t.test("ASCII-with-LEFT-PARENTHESIS", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-LEFT-PARENTHESIS-[(].txt")
  );

  await t.test("ASCII-with-RIGHT-PARENTHESIS", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-RIGHT-PARENTHESIS-[)].txt")
  );

  await t.test("ASCII-with-ASTERISK", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-ASTERISK-[*].txt")
  );

  await t.test("ASCII-with-PLUS-SIGN", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-PLUS-SIGN-[+].txt")
  );

  await t.test("ASCII-with-COMMA", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-COMMA-[,].txt")
  );

  await t.test("ASCII-with-FULL-STOP", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-FULL-STOP-[.].txt")
  );

  await t.test("ASCII-with-SOLIDUS", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-SOLIDUS-[/].txt")
  );

  await t.test("ASCII-with-COLON", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-COLON-[:].txt")
  );

  await t.test("ASCII-with-SEMICOLON", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-SEMICOLON-[;].txt")
  );

  await t.test("ASCII-with-EQUALS-SIGN", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-EQUALS-SIGN-[=].txt")
  );

  await t.test("ASCII-with-QUESTION-MARK", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-QUESTION-MARK-[?].txt")
  );

  await t.test("ASCII-with-CIRCUMFLEX-ACCENT", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-CIRCUMFLEX-ACCENT-[^].txt")
  );

  await t.test("ASCII-with-LEFT-SQUARE-BRACKET", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-LEFT-SQUARE-BRACKET-[[].txt")
  );

  await t.test("ASCII-with-RIGHT-SQUARE-BRACKET", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-RIGHT-SQUARE-BRACKET-[]].txt")
  );

  await t.test("ASCII-with-LEFT-CURLY-BRACKET", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-LEFT-CURLY-BRACKET-[{].txt")
  );

  await t.test("ASCII-with-VERTICAL-LINE", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-VERTICAL-LINE-[|].txt")
  );

  await t.test("ASCII-with-RIGHT-CURLY-BRACKET", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-RIGHT-CURLY-BRACKET-[}].txt")
  );

  await t.test("ASCII-with-TILDE", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-TILDE-[~].txt")
  );

  await t.test("ASCII-with-single-quoted", async () =>
    formDataPostFileUploadTest("file-for-upload-in-form-single-quoted.txt")
  );
});
