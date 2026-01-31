#include "mccomp.h"
#include <cassert>
#include <cstring>
#include <algorithm>
#include <stdio.h>

namespace mccomp {

Table::~Table()
{
#if false
    printf("--- Table ---\n");
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
    assert(isAscii(a));

    // Age down the count with every push, rolling through the table
    // Anything with count == 0 will get re-used
    _count++;
    const int ageIndex = _count % kTableSize;
    if (_table[ageIndex].count > 0) {
        _table[ageIndex].count--;
    }

    const int idx = hash(_prev, a);
    if (_table[idx].count == 0) {
        _table[idx] = {_prev, a, 1};
    }
    else if (_table[idx].match(_prev, a)) {
        // Prevent overflow by capping at max value
        if (_table[idx].count < UINT16_MAX) {
            _table[idx].count++;
        }
    }
    _prev = a;
}

int Table::fetch(uint8_t a, uint8_t b) const
{
    const int idx = hash(a, b);
    const Entry& entry = _table[idx];
    if (entry.match(a, b)) {
        assert(isAscii(entry.a));
        assert(isAscii(entry.b));
        return idx;
    }
    return -1;
}

void Table::get(int idx, uint8_t& a, uint8_t& b) const
{
    assert(idx >= 0 && idx < kTableSize);
    const Entry& entry = _table[idx];
    a = entry.a;
    b = entry.b;
    assert(isAscii(entry.a));
    assert(isAscii(entry.b));
}

int Table::count(int idx) const
{
    assert(idx >= 0 && idx < kTableSize);
    return _table[idx].count;
}

void Table::utilization(int& nUsed, int& nTotal) const
{
    nUsed = 0;
    nTotal = 0;
    for (const auto& entry : _table) {
        if (entry.count > 0) {
            nUsed++;
        }
        nTotal += entry.count;
    }
}

int Compressor::writeRLE(const uint8_t* input, const uint8_t* inputEnd, uint8_t* out, const uint8_t* outputEnd)
{
    // Check if we have space for RLE marker + value (2 bytes minimum)
    if (out + 2 > outputEnd || input + kRLEMinLength > inputEnd) {
        return 0;
    }

    const uint8_t value = *input;
    const uint8_t* p = input + 1;
    
    // Count consecutive identical bytes up to maximum RLE length
    while (p < inputEnd && *p == value && (p - input) < kRLEMaxLength) {
        p++;
    }
    
    const int runLength = static_cast<int>(p - input);
    if (runLength >= kRLEMinLength) {
        *out++ = static_cast<uint8_t>(kRLEStart + (runLength - kRLEMinLength));
        *out++ = value;
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
        const int rleBytes = writeRLE(in, inEnd, out, outEnd);
        if (rleBytes > 0) {
            // RLE succeeded and already wrote 2 bytes
            in += rleBytes;
            out += 2;
            continue;
        }
        
        const uint8_t byte = *in;
        const uint8_t nextByte = (in + 1 < inEnd) ? *(in + 1) : 0;

        // If both ASCII, check if we can use byte-pair compression
        // Query table before pushing to match decompressor behavior
        if (isAscii(byte) && isAscii(nextByte)) {
            const int idx = _table.fetch(byte, nextByte);
            if (idx >= 0) {
                if (out + 1 > outEnd) {
                    break;
                }
                *out++ = static_cast<uint8_t>(idx + kTableStart);
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
            // Low ASCII values can be written directly
            if (out + 1 > outEnd) {
                break;
            }
            _table.push(byte);
            *out++ = *in++;
        }
    }
    Result result;
    result.nInput = static_cast<int>(in - input);
    result.nOutput = static_cast<int>(out - output);
    result.readDone = in == inEnd;
    result.writeDone = true; // the compressor won't write to out unless there is space, so it's always done
    result.done = result.readDone && result.writeDone;
    return result;
}

Result Decompressor::decompress(const uint8_t* input, int inputSize, uint8_t* output, int outputSize)
{
    const uint8_t* in = input;
    const uint8_t* inEnd = input + inputSize;
    uint8_t* out = output;
    const uint8_t* outEnd = output + outputSize;

    while (out < outEnd) {
        // Flush RLE
        if (_nRLE) {
            if (_rleValue < 0) {
                _rleValue = *in++;
                continue;
            }
            int n = std::min(_nRLE, int(outEnd - out));
            _nRLE -= n;
            while (n--)
                *out++ = uint8_t(_rleValue);
            if (_nRLE == 0)
                _rleValue = -1;
            continue;
        }
        // Flush literal
        if (_nLiteral) {
            if (_literalValue < 0) {
                assert(_literalValue < 0);
                _literalValue = *in++;
                continue;
            }
            int n = std::min(_nLiteral, int(outEnd - out));
            _nLiteral -= n;
            while (n--)
                *out++ = uint8_t(_literalValue);
            if (_nLiteral == 0)
                _literalValue = -1;
            continue;
        }

        // Now that the RLE and literal is flushed, we need input!
        if (in >= inEnd) {
            break;
        }
        
        const uint8_t byte = *in;
        if (byte >= kRLEStart && byte <= kRLEEnd) {
            _nRLE = static_cast<int>(byte - kRLEStart + kRLEMinLength);
            ++in; // consume marker
            _rleValue = -1;
            continue;
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
            // Literal escape sequence: marker + value (2 bytes total)
            _nLiteral = 1;
            ++in;   // consume marker
            _literalValue = -1;
            continue;
        }
        else {
            _table.push(byte);
            *out++ = *in++;
        }
    }
    Result result;
    result.nInput = static_cast<int>(in - input);
    result.nOutput = static_cast<int>(out - output);
    result.readDone = in == inEnd;
    result.writeDone = (_literalValue < 0) && (_rleValue < 0);
    result.done = result.readDone && result.writeDone;
    return result;
}

} // namespace mccomp