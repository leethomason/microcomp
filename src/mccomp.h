#pragma once

#include <cstdint>
#include <array>

namespace mccomp {

static constexpr uint8_t kLiteral = 128;
static constexpr uint8_t kRLEStart = 129;
static constexpr uint8_t kRLEEnd = 136;
static constexpr uint8_t kTableStart = kRLEEnd + 1;
static constexpr uint8_t kTableEnd = 255;

static constexpr int kRLEMinLength = 3;
static constexpr int kRLEMaxLength = kRLEEnd - kRLEStart + kRLEMinLength - 1;
static constexpr int kTableSize = kTableEnd - kTableStart + 1;

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
        return (a * 37 + b * 101) % kTableSize;
    }

    struct Entry {
        uint8_t a = 0;
        uint8_t b = 0;
        uint16_t count = 0;

        bool match(uint8_t a_, uint8_t b_) const {
            return a == a_ && b == b_;
		}
    };
    uint8_t _prev;
    int _count = 0;
    std::array<Entry, kTableSize> _table;
};

inline bool isAscii(uint8_t byte) {
    return byte < kLiteral;
}

struct Result {
    int nInput = 0;
    int nOutput = 0;
};

class Compressor {
public:
	// Compress data. This can (and should) be called multiple times to process, where
	// inputSize and outputSize specify the size of the input and output buffers.
    Result compress(const uint8_t* input, int inputSize, uint8_t* output, int outputSize);

private:
	int writeRLE(const uint8_t* input, const uint8_t* inputEnd, uint8_t* output, const uint8_t* outputEnd);

    Table _table;
};

class Decompressor {
public:
	// Decompress data. This can (and should) be called multiple times to process.
    Result decompress(const uint8_t* input, int inputSize, uint8_t* output, int outputSize);

    void utilization(int& nEntries, int& nTotal) const {
        _table.utilization(nEntries, nTotal);
    }

private:
    Table _table;
};

}