#pragma once

#include <cstdint>

namespace mccomp {

class Table {
    // todo
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
};

class Decompressor {
public:
    Result decompress(const uint8_t* input, int inputSize, uint8_t* output, int outputSize);
};

}