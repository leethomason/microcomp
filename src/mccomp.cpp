#include "mccomp.h"
#include <assert.h>
#include <memory.h>
#include <algorithm>
#include <stdio.h>

namespace mccomp {

Table::~Table()
{
#if 0
    for (int i = 0; i < kTableSize; i++) {
		const Entry& e = _table[i];
        printf("%c%c:%3d ", e.a >= 32 && e.a < 127 ? e.a : ' ', e.b >= 32 && e.b < 127 ? e.b : ' ', e.count);
		if (i % 10 == 9) {
            printf("\n");
        }
    }
	printf("\n");
#endif
}


void Table::push(uint8_t a)
{
    assert(a != 0);
    assert(a < 128);

	_count++;
    if (_table[_count % kTableSize].count > 0) {
        _table[_count % kTableSize].count--;
    }

    if (_prev == 0) {
        _prev = a;
        return;
    }

    int idx = hash(_prev, a);
    if (_table[idx].count == 0) {
        _table[idx].count = 1;
        _table[idx].a = _prev;
        _table[idx].b = a;
    }
    else if (_table[idx].match(_prev, a)) {
        _table[idx].count++;
    }
    _prev = a;
}

int Table::fetch(uint8_t a, uint8_t b) const
{
    int idx = hash(a, b);
    if (_table[idx].count > 0 && _table[idx].match(a, b)) {
        assert(_table[idx].a != 0 && _table[idx].b != 0);
        assert(_table[idx].a < 128 && _table[idx].b < 128);
        return idx;
    }
    return -1;
}

void Table::get(int idx, uint8_t& a, uint8_t& b) const
{
    a = _table[idx].a;
    b = _table[idx].b;
    assert(a != 0 && b != 0);
    assert(a < 128 && b < 128);
}

int Table::count(int idx) const
{
    return _table[idx].count;
}

void Table::utilization(int& nUsed, int& nTotal) const
{
    nUsed = 0;
    nTotal = 0;
    for (int i = 0; i < kTableSize; i++) {
        if (_table[i].count > 0) {
            nUsed++;
        }
        nTotal += _table[i].count;
	}
}


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

// Take ABCD
// BC = 1 already in table
// compress:
//   *in = A. next = B. AB not in table, push(A)
//   *in = B. next = C. BC in table at idx 1, push(B), push(C), skip
//   *in = D  next = ? done
// decompress:
//   *in = A. push(A)
//   *in = idx 1. get(1) = BC, push(B), push(C), skip
//   *in = D. next= ? done


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
		// If both ascii, add to table
        uint8_t byte = *in;
		uint8_t nextByte = (in + 1 < inEnd) ? *(in + 1) : 0;

		// To match decompressor, we need to query the table before pushing
		if (isAscii(byte) && isAscii(nextByte)) {
            int idx = _table.fetch(byte, nextByte);
            if (idx >= 0) {
                *out++ = uint8_t(idx + kTableStart);
                in += 2;
				_table.push(byte);
				_table.push(nextByte);
                continue;
            }
        }
        // Emit as literal
        if (!isAscii(byte)) {
            // High-bit values need escape sequence: kLiteral marker + value
            if (out + 2 > outEnd) {
                break;
            }
            *out++ = kLiteral;
            *out++ = *in++;
        }
        else {
            // Low values can be written directly
			_table.push(byte);
            *out++ = byte;
            in++;
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
        else if (byte >= kTableStart && byte <= kTableEnd) {
			if (out + 2 > outEnd) {
                break; // Not enough output space
            }

            uint8_t a, b;
            _table.get(byte - kTableStart, a, b);
            _table.push(a);
            _table.push(b);
            in++;
			*out++ = a;
			*out++ = b;
        }
		else if (byte == kLiteral) {
            in++; // Consume marker
			*out++ = *in++; // Consume and write value
        }
        else {
            // Direct literal value
            _table.push(byte);
            in++;
            *out++ = byte;
		}
    }
	return Result{ int(in - input), int(out - output) };
}

} // namespace mccomp