#ifndef STREAM_ERRORS_H
#define STREAM_ERRORS_H

namespace StreamErrors {
DEF_ERR(CompressingChunkFailed, JSEXN_TYPEERR, "CompressionStream transform: error compressing chunk", 0)
DEF_ERR(DecompressingChunkFailed, JSEXN_TYPEERR, "DecompressionStream transform: error decompressing chunk", 0)
DEF_ERR(StreamInitializationFailed, JSEXN_ERR, "Error initializing {0} stream", 1)
DEF_ERR(StreamAlreadyLocked, JSEXN_TYPEERR, "Can't lock an already locked ReadableStream", 0)
DEF_ERR(PipeThroughFromLockedStream, JSEXN_TYPEERR, "pipeThrough called on a ReadableStream that's already locked", 0)
DEF_ERR(PipeThroughToLockedStream, JSEXN_TYPEERR, "The writable end of the transform object passed to pipeThrough "
                                                  "is already locked", 0)
DEF_ERR(PipeThroughWrongArg, JSEXN_TYPEERR, "pipeThrough called on a ReadableStream that's already locked", 0)
DEF_ERR(TransformStreamTerminated, JSEXN_TYPEERR, "The TransformStream has been terminated", 0)
DEF_ERR(InvalidCompressionFormat, JSEXN_TYPEERR, "'format' has to be \"deflate\",  \"deflate-raw\", or \"gzip\", "
                                                 "but got \"{0}\"", 1)
};     // namespace StreamErrors

#endif // STREAM_ERRORS_H
