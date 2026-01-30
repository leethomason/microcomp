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
        // Emit RLE
        if (out + 2 >= outputEnd) {
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
		int bytes = writeRLE(in, inEnd, out, outEnd);

        if (bytes > 0) {
            if (out + 2 >= outEnd)
                break;
            in += bytes;
            out += 2;
            continue;
		}
        // Literal.
        if (*in >= 128) {
            if (out + 2 >= outEnd) {
                break; // Not enough space
            }
            *out++ = kLiteral;
            *out++ = *in++;
        }
        else {
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
        uint8_t byte = *in++;
        if (byte >= kRLEStart && byte <= kRLEEnd) {
			if (in + 1 >= inEnd) {
                in--;
                break; // Not enough input
            }
            int runLength = int(byte - kRLEStart + kRLEMinLength);
            uint8_t value = *in++;
            if (out + runLength > outEnd) {
                in -= 2;
                break; // Not enough output space
            }
            std::fill(out, out + runLength, value);
            out += runLength;
        }
		else if (byte == kLiteral) {
			if (in + 1 >= inEnd) {
                in--;
                break; // Not enough input
            }
			*out++ = *in++;
        }
        else {
            *out++ = byte;
		}
    }
	return Result{ int(in - input), int(out - output) };
}

} // namespace mccomp