#pragma once

#include <cstdint>
#include <array>

namespace mccomp {

// Use the low end of the byte range for RLE markers
static constexpr uint8_t kRLEStart = 0;
static constexpr uint8_t kRLEEnd = 8; // backspace

static constexpr uint8_t kLiteral = 127;
static constexpr uint8_t kTableStart = 128;
static constexpr uint8_t kTableEnd = 255;

static constexpr int kRLEMinLength = 3;
static constexpr int kRLEMaxLength = kRLEEnd - kRLEStart + kRLEMinLength - 1;
static constexpr int kTableSize = kTableEnd - kTableStart + 1;

// "normal" ascii byte
inline bool isAscii(uint8_t byte) {
    return byte > kRLEEnd && byte < kLiteral;
}

// The Table is created on the fly by the compressor and decompressor.
// They use the data stream itself to populate the table in the same way.

class Table {
public:
	Table() = default;
    ~Table();

    void push(uint8_t val);
    int fetch(uint8_t a, uint8_t b) const;
    void get(int idx, uint8_t& a, uint8_t& b) const;

    int count(int idx) const;
    void utilization(int& nUsed, int& nTotal) const;

private:
    int hash(uint8_t a, uint8_t b) const {
		// It's surprisingly sensitive to the choice of multipliers here.
        // These were found by rough testing, but worth revisiting on a
		// representative corpus.
        return (a * 37 + b * 227) % kTableSize;       // 71.9
    }

    struct Entry {
        uint8_t a = ' ';
        uint8_t b = ' ';
        uint16_t count = 0;

        bool match(uint8_t a_, uint8_t b_) const {
            return a == a_ && b == b_;
		}
    };
    uint8_t _prev = ' ';
    int _count = 0;
    std::array<Entry, kTableSize> _table;
};

struct Result {
    int nInput = 0;
    int nOutput = 0;
};

class Compressor {
public:
	// Compress data. This can (and should) be called multiple times to process.
    // 'input' the input buffer, can be the full data or a chunk
	// 'inputSize' the size of the input buffer
	// 'output' the output buffer to write compressed data into
	// 'outputSize' the size of the output buffer - 16 bytes is the minimum, but something 40b - 10k 
	//              probably better depending on memory constraints.
	// 'Result' contains the number of input bytes consumed and output bytes produced. Note that
	//          you may need to call multiple times to process all data.
    Result compress(const uint8_t* input, int inputSize, uint8_t* output, int outputSize);

private:
	int writeRLE(const uint8_t* input, const uint8_t* inputEnd, uint8_t* output, const uint8_t* outputEnd);

    Table _table;
};

class Decompressor {
public:
	// Decompress data. This can (and should) be called multiple times to process.
    // Buffer sizes can be modest; on a microcontroller, 40-80 bytes is sufficient.
    Result decompress(const uint8_t* input, int inputSize, uint8_t* output, int outputSize);

    // debugging: clarify the table usage
    void utilization(int& nEntries, int& nTotal) const {
        _table.utilization(nEntries, nTotal);
    }

private:
    Table _table;
};

}