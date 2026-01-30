#include "mccomp.h"
#include <assert.h>
#include <memory.h>
#include <algorithm>
#include <stdio.h>

// todo: End-of-file marker
// todo: RLE
// todo: compression table
// todo: low v high ascii

namespace mccomp {

static constexpr uint8_t kLiteral = 128;
static constexpr uint8_t kRLEStart = 129;
static constexpr uint8_t kRLEEnd = 136;

static constexpr int kRLEMinLength = 3;
static constexpr int kRLEMaxLength = kRLEEnd - kRLEStart + kRLEMinLength - 1;

int Compressor::writeRLE(const uint8_t* input, const uint8_t* inputEnd, uint8_t* out, const uint8_t* outputEnd)
{
    const uint8_t* p = input + 1;
    while (p < inputEnd && *p == *input && (p - input) < kRLEMaxLength) {
        p++;
    }
    int runLength = int(p - input);
    if (runLength >= kRLEMinLength) {
        // Check if we have space for RLE marker + value (2 bytes)
        if (out + 2 > outputEnd) {
            return 0;
        }
        *out++ = uint8_t(kRLEStart + (runLength - kRLEMinLength));
        *out++ = *input;
        return runLength;
    }
    return 0;
}

Result Compressor::compress(const uint8_t* input, int inputSize, uint8_t* output, int outputSize)
{
    const uint8_t* in = input;
    const uint8_t* inEnd = input + inputSize;
    uint8_t* out = output;
	const uint8_t* outEnd = output + outputSize;

	while (in < inEnd && out < outEnd) {
        // Try RLE encoding first
		int rleBytes = writeRLE(in, inEnd, out, outEnd);
        if (rleBytes > 0) {
            // RLE succeeded and already wrote 2 bytes
            in += rleBytes;
            out += 2;
            continue;
		}

        // Emit as literal
        if (*in >= 128) {
            // High-bit values need escape sequence: kLiteral marker + value
            if (out + 2 > outEnd) {
                break;
            }
            *out++ = kLiteral;
            *out++ = *in++;
        }
        else {
            // Low values can be written directly
            *out++ = *in++;
        }
    }
    return { int(in - input), int(out - output) };
}

Result Decompressor::decompress(const uint8_t* input, int inputSize, uint8_t* output, int outputSize)
{
    const uint8_t* in = input;
    const uint8_t* inEnd = input + inputSize;
    uint8_t* out = output;
    const uint8_t* outEnd = output + outputSize;

    while (in < inEnd && out < outEnd) {
        uint8_t byte = *in;
        
        if (byte >= kRLEStart && byte <= kRLEEnd) {
            // RLE sequence: marker + value (2 bytes total)
			if (in + 2 > inEnd) {
                break; // Not enough input
            }
            int runLength = int(byte - kRLEStart + kRLEMinLength);
            if (out + runLength > outEnd) {
                break; // Not enough output space
            }
            in++; // Consume marker
            uint8_t value = *in++; // Consume value
            std::fill(out, out + runLength, value);
            out += runLength;
        }
		else if (byte == kLiteral) {
            // Escaped literal: kLiteral marker + actual value (2 bytes total)
			if (in + 2 > inEnd) {
                break; // Not enough input
            }
            in++; // Consume marker
			*out++ = *in++; // Consume and write value
        }
        else {
            // Direct literal value
            in++;
            *out++ = byte;
		}
    }
	return Result{ int(in - input), int(out - output) };
}

} // namespace mccomp