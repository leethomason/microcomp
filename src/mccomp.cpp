#include "mccomp.h"

#include <cassert>
#include <cstring>
#include <algorithm>
#include <stdio.h>

namespace mccomp {

Table::~Table()
{
}


void Table::push(uint8_t a)
{
    assert(isAscii(a));

    const int start = hash(_prev, a);
    const int end = std::min(start + kNumTap, kTableSize);

    int weakest = -1;
    for (int idx = start; idx < end; idx++) {
        if (_table[idx].count == 0) {
            _table[idx].a = _prev;
            _table[idx].b = a;
            _table[idx].count = 1;
            _prev = a;
            return;
        }
        if (_table[idx].match(_prev, a)) {
            if (_table[idx].count < UINT16_MAX) {
                _table[idx].count++;
            }
            _prev = a;
            return;
        }
        if (weakest < 0 || _table[idx].count < _table[weakest].count) {
            weakest = idx;
        }
    }

    // Every slot in this bucket is occupied by a different pair: age down
    // the least-valuable occupant (contention-based eviction) instead of
    // decaying an unrelated slot elsewhere in the table. Once it runs out
    // of credit, hand its slot straight to the pair actively contending
    // for it.
    Entry& loser = _table[weakest];
    if (--loser.count == 0) {
        loser.a = _prev;
        loser.b = a;
        loser.count = 1;
    }
    _prev = a;
}

int Table::fetch(uint8_t a, uint8_t b) const
{
    const int start = hash(a, b);
    const int end = std::min(start + kNumTap, kTableSize);
    for (int idx = start; idx < end; idx++) {
        const Entry& entry = _table[idx];
        if (entry.count > 0 && entry.match(a, b)) {
            assert(isAscii(entry.a));
            assert(isAscii(entry.b));
            return idx;
        }
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


Result Compressor::compress(const uint8_t* input, size_t inputSize, uint8_t* output, size_t outputSize)
{
    const uint8_t* in = input;
    const uint8_t* inEnd = input + inputSize;
    uint8_t* out = output;
    const uint8_t* outEnd = output + outputSize;

    while (in < inEnd && out < outEnd) {
        // Try RLE encoding first. There are some log files with a 
        // lot of space runs, dashes, 0 leads on numbers, where
		// this is a significant win.
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
    result.eofFF = false;
    return result;
}

Decompressor::Payload Decompressor::fetchPayload(const uint8_t* in, const uint8_t* inEnd,
                               uint8_t* out, const uint8_t* outEnd,
                               uint8_t marker, bool fromCarry, int outReq)
{
    int advance = 0;
    const int inReq = fromCarry ? 1 : 2;   // payload only, or marker + payload
    if (in + inReq > inEnd || out + outReq > outEnd) {
        if (!fromCarry && in + 1 == inEnd) {
            // The marker is the last byte in hand; consume it into the
            // carry so the payload can arrive next call.
            _carry = marker;
            advance = 1;
        }
        // Otherwise leave state untouched (still pending on output space,
        // or the marker will simply be reprocessed next call) and consume
        // nothing.
		Payload payload;
        payload.payload = -1;
        payload.advance = advance;
        return payload;
    }
    if (!fromCarry) {
        ++in;   // consume marker
        advance = 1;
    }
    _carry = -1;
    advance++;
    Payload payload;
    payload.payload = *in;
    payload.advance = advance;
    return payload;           // consume and return payload
}

Result Decompressor::decompress(const uint8_t* input, size_t inputSize, uint8_t* output, size_t outputSize)
{
    const uint8_t* in = input;
    const uint8_t* inEnd = input + inputSize;
    uint8_t* out = output;
    const uint8_t* outEnd = output + outputSize;
    bool eofFF = false;

    while(in < inEnd && out < outEnd) {
        // If the previous call left a marker byte pending (it had already been
        // consumed from that call's input, but its payload hadn't arrived yet),
        // resume from it. Only the payload comes from this call's buffer.
        const bool fromCarry = (_carry >= 0);
        const uint8_t byte = fromCarry ? uint8_t(_carry) : *in;

        if (_detectEOF && (byte == 0xff)) {
            eofFF = true;
            break;
        }

        if (byte >= kRLEStart && byte <= kRLEEnd) {
            const int nRLE = static_cast<int>(byte - kRLEStart + kRLEMinLength);
            // Conservative: require one extra byte of output headroom.
            const Payload payload = fetchPayload(in, inEnd, out, outEnd, byte, fromCarry, 1 + nRLE);
			in += payload.advance;
            if (payload.payload < 0) 
                break;

            // RLEs are not pushed to the Table
            for (int i = 0; i < nRLE; i++) {
                *out++ = static_cast<uint8_t>(payload.payload);
            }
            continue;
        }
        else if (byte >= kTableStart && byte <= kTableEnd) {
            static constexpr int kInReq = 1;
            static constexpr int kOutReq = 2;
            if (in + kInReq > inEnd || out + kOutReq > outEnd) {
                break; // Not enough input or output space
            }

            uint8_t a, b;
            _table.get(byte - kTableStart, a, b);
            _table.push(a);
            _table.push(b);
            in++;
            *out++ = a;
            *out++ = b;
            continue;
        }
        else if (byte == kLiteral) {
            // Literal escape sequence: marker + value (2 bytes total)
            const Payload payload = fetchPayload(in, inEnd, out, outEnd, byte, fromCarry, 1);
            in += payload.advance;
            if (payload.payload < 0) 
                break;

            *out++ = static_cast<uint8_t>(payload.payload);
            continue;
        }
        else {
            _table.push(byte);
            *out++ = byte;
            in++;
        }
    }
    Result result;
    result.nInput = static_cast<int>(in - input);
    result.nOutput = static_cast<int>(out - output);
    result.eofFF = eofFF;
    return result;
}

} // namespace mccomp