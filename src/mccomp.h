#pragma once

#include <cstdint>
#include <array>

// mccomp: A streaming compression algorithm optimized for microcontrollers and embedded systems.
// Uses RLE (Run-Length Encoding) and a dynamically built byte-pair lookup table.
namespace mccomp {

// Byte value ranges - the algorithm partitions the 256-byte space into regions.

// RLE (Run-Length Encoding) markers: bytes 0-8 encode runs of 3-11 identical bytes
static constexpr uint8_t kRLEStart = 0;
static constexpr uint8_t kRLEEnd = 8;

// Literal escape marker: signals that the next byte should be treated as-is
static constexpr uint8_t kLiteral = 127;

// Table lookup range: bytes 128-255 map to common byte pairs
static constexpr uint8_t kTableStart = 128;
static constexpr uint8_t kTableEnd = 254;   // 255 is reserved for End of File on Flash memory.

// RLE encoding parameters
static constexpr int kRLEMinLength = 3;
static constexpr int kRLEMaxLength = kRLEEnd - kRLEStart + kRLEMinLength - 1;
static constexpr int kTableSize = kTableEnd - kTableStart + 1;

// Check if a byte falls in the regular ASCII range.
// These bytes can be passed through without encoding/escaping.
inline bool isAscii(uint8_t byte) {
    return byte > kRLEEnd && byte < kLiteral;
}

// Adaptive byte-pair lookup table.
// Both compressor and decompressor build this table identically as they process the stream,
// allowing the decompressor to decode without needing the table transmitted.

class Table {
public:
	Table() = default;
    ~Table();

    // Add a byte to the stream, updating byte-pair statistics
    void push(uint8_t val);
    
    // Look up a byte pair in the table, returns index or -1 if not found
    int fetch(uint8_t a, uint8_t b) const;
    
    // Retrieve the byte pair stored at a given table index
    void get(int idx, uint8_t& a, uint8_t& b) const;

    // Get the frequency count for an entry at the given index
    int count(int idx) const;
    
    // Get table statistics: number of used entries and total hit count
    void utilization(int& nUsed, int& nTotal) const;

private:
    static constexpr int kHashA = 36;   // magic
    static constexpr int kHashB = 227;  // magic
    static constexpr int kNumTap = 1;   // multi-tap the table. initial test makes compression worse.

    int hash(uint8_t a, uint8_t b) const {
		// It's surprisingly sensitive to the choice of multipliers here.
        // These were found by rough testing, but worth revisiting on a
		// representative corpus.
        return (a * kHashA + b * kHashB) % kTableSize;
    }

    // Hash table entry storing a byte pair and its occurrence count
    struct Entry {
        uint8_t a = ' ';      // First byte of the pair
        uint8_t b = ' ';      // Second byte of the pair
        uint16_t count = 0;   // Frequency count (used for eviction decisions)

        bool match(uint8_t a_, uint8_t b_) const {
            return a == a_ && b == b_;
		}
    };
    
    uint8_t _prev = ' ';  // Previous byte seen (for tracking byte pairs)
    int _count = 0;
    std::array<Entry, kTableSize> _table;  // The hash table
};

// Result of a compression or decompression operation.
// Since operations are streaming, not all input may be consumed in one call.
struct Result {
    int nInput = 0;   // Number of input bytes consumed
    int nOutput = 0;  // Number of output bytes produced
    bool eofFF = false; // If detecting 0xff as EOF, true if the EOF byte was read
};

// Streaming compressor using RLE and adaptive byte-pair encoding.
// The same Compressor instance should be used for an entire stream to maintain table state.
class Compressor {
public:
	// Compress a chunk of data. Can be called multiple times for streaming compression.
    // 
    // Parameters:
    //   input      - Input buffer (can be full data or a chunk)
    //   inputSize  - Size of input buffer in bytes
    //   output     - Output buffer for compressed data
    //   outputSize - Size of output buffer (minimum 16 bytes, 40-10K recommended)
    // 
    // Returns:
    //   Result with nInput bytes consumed and nOutput bytes produced.
    //   Call again with remaining data if r.nInput < inputSize.
    Result compress(const uint8_t* input, size_t inputSize, uint8_t* output, size_t outputSize);

private:
    // Encode a run of repeated bytes using RLE markers
    int writeRLE(const uint8_t* input, const uint8_t* inputEnd, uint8_t* output, const uint8_t* outputEnd);

   Table _table;  // Adaptive byte-pair lookup table
};

// Streaming decompressor for data compressed with Compressor.
// The same Decompressor instance should be used for an entire stream to maintain table state.
class Decompressor {
public:
    //   eolFF   - if the input is known to be ASCII or UTF-8, then 0xff will never
    //                be written to the compressed stream, and can be used as EOF. 
    //                If true, will detect 0xff as EOF, and return eofFF = true in Result
    Decompressor(bool eofFF = false) : _detectEOF(eofFF) {}

	// Decompress a chunk of data. Can be called multiple times for streaming decompression.
    // 
    // Parameters:
    //   input      - Input buffer containing compressed data
    //   inputSize  - Size of input buffer in bytes
    //   output     - Output buffer for decompressed data
	//   outputSize - Size of output buffer.
    // 
    // Returns:
    //   Result with nInput bytes consumed and nOutput bytes produced.
	//   Repeat calls until all data is decompressed.
    Result decompress(const uint8_t* input, size_t inputSize, uint8_t* output, size_t outputSize);

    // Get statistics about table usage (for debugging and optimization)
    void utilization(int& nEntries, int& nTotal) const {
        _table.utilization(nEntries, nTotal);
    }

private:
    bool _detectEOF;
    int _carry = -1;    // It is possible that the last byte in input is part of an escape sequence.
	                    // In that case, we store it here to process on the next call.
    Table _table;       // Adaptive byte-pair lookup table
};

}