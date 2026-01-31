#pragma once

#include <cstdint>
#include <array>

namespace mccomp {

static constexpr uint8_t kLiteral = 128;
static constexpr uint8_t kRLEStart = 129;
static constexpr uint8_t kRLEEnd = 136;
static constexpr uint8_t kTableStart = 137;
static constexpr uint8_t kTableEnd = 255;

static constexpr int kRLEMinLength = 3;
static constexpr int kRLEMaxLength = kRLEEnd - kRLEStart + kRLEMinLength - 1;
static constexpr int kTableSize = kTableEnd - kTableStart + 1;

class Table {
public:
    void push(uint8_t val);
    int fetch(uint8_t a, uint8_t b) const;
    void get(int idx, uint8_t& a, uint8_t& b) const;

    int count(int idx) const;

private:
    int hash(uint8_t a, uint8_t b) const {
        return (a * kTableSize + b) % kTableSize;
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
    std::array<Entry, kTableSize> _table;
};

struct Result {
    int nInput = 0;
    int nOutput = 0;
};

class Compressor {
public:
    Result compress(const uint8_t* input, int inputSize, uint8_t* output, int outputSize);

private:
	int writeRLE(const uint8_t* input, const uint8_t* inputEnd, uint8_t* output, const uint8_t* outputEnd);

    Table _table;
};

class Decompressor {
public:
    Result decompress(const uint8_t* input, int inputSize, uint8_t* output, int outputSize);

private:
    Table _table;
};

}