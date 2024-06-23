#ifndef TEXT_CODEC_ERRORS_H
#define TEXT_CODEC_ERRORS_H

namespace TextCodecErrors {
DEF_ERR(FetchNetworkError, JSEXN_TYPEERR, "NetworkError when attempting to fetch resource", 0)
DEF_ERR(InvalidEncoding, JSEXN_RANGEERR, "TextDecoder constructor: The given encoding is not supported.", 0)
DEF_ERR(DecodingFailed, JSEXN_TYPEERR, "TextDecoder.decode: Decoding failed.", 0)
};

#endif // TEXT_CODEC_ERRORS_H
