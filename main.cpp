#include "src/mccomp.h"

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <assert.h>
#include <stdio.h>


#define RUN_TEST(test) printf("Test: %s\n", #test); test

// Like assert(), but works in release mode too
#define TEST(x) \
	if (!(x)) {	\
        assert(false); \
		printf("ERROR: line %d in %s\n", __LINE__, __FILE__); \
		exit(-1); \
	} \
	assert(x);

void testTable()
{
    mccomp::Table t;
    {
        // ABAB
        t.push('A');
        t.push('B');

        int i = t.fetch('A', 'B');
        TEST(i >= 0 && i < mccomp::kTableSize);
        TEST(t.count(i) == 1);
        t.push('A');
        t.push('B');
        TEST(t.count(i) == 2);
    }
}

void testComp0()
{
    const char* in = "ABAB";
    uint8_t out[8];
    mccomp::Compressor c;
	mccomp::Result r = c.compress((const uint8_t*)in, 4, (uint8_t*)out, 8);
    TEST(r.nInput == 4);
    TEST(r.nOutput == 3);
	TEST(out[0] == 'A');
    TEST(out[1] == 'B');
	TEST(out[2] >= mccomp::kTableStart && out[2] <= mccomp::kTableEnd);
}

void testComp1()
{
    const char* in = "AAAA";
	uint8_t out[8];
    mccomp::Compressor c;
    mccomp::Result r = c.compress((const uint8_t*)in, 4, (uint8_t*)out, 8);
    TEST(r.nOutput == 2);
    TEST(out[0] == mccomp::kRLEStart + 1);
    TEST(out[1] == 'A');
}

int main(int argc, char* argv[]) {
    RUN_TEST(testTable());
	RUN_TEST(testComp0());
    RUN_TEST(testComp1());

    // Check if filename was provided as argument
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;
        return 1;
    }
    
    std::string filename = argv[1];
    std::ifstream file(filename);
    
    // Check if file opened successfully
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file '" << filename << "'" << std::endl;
        return 1;
    }
    
    // Read the entire file into a string
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string fileContent = buffer.str();
    
    size_t sz = fileContent.size();
    std::cout << "File size: " << sz << " bytes" << std::endl;
    file.close();
    
    std::vector<uint8_t> compressed;
    {
        static constexpr int bufferSize = 40;
        uint8_t working[bufferSize + 1];
        memset(working, 0, bufferSize + 1);

        mccomp::Compressor compressor;
        size_t pos = 0;
        while (pos < fileContent.size()) {
            mccomp::Result r = compressor.compress(
                (const uint8_t*)(fileContent.data() + pos),
                (int)(fileContent.size() - pos),
                working,
                bufferSize);

            assert(working[bufferSize] == 0);
            assert(r.nOutput <= bufferSize);

            for (size_t i = 0; i < r.nOutput; i++)
                compressed.push_back(working[i]);
            pos += r.nInput;
        }
    }    
    std::string uncompressed;
    {
        static constexpr int bufferSize = 19;
        uint8_t working[bufferSize + 1];
        memset(working, 0, bufferSize + 1);
        mccomp::Decompressor decompressor;

        size_t pos = 0;
        while (pos < compressed.size()) {
            mccomp::Result r = decompressor.decompress(
                compressed.data() + pos,
                (int)(compressed.size() - pos),
                working,
                bufferSize);

            assert(working[bufferSize == 0]);
            assert(r.nOutput <= bufferSize);

            for (size_t i = 0; i < r.nOutput; i++)
                uncompressed.push_back(working[i]);
            pos += r.nInput;

            if (pos == compressed.size()) {
                // Final bytes:
                assert(r.nInput > 0);
            }
        }

        std::cout << "Input: " << fileContent.size() << std::endl;
        std::cout << "Uncompressed: " << uncompressed.size() << " Compressed: " << compressed.size() << " bytes" << std::endl;
        std::cout << "Ratio%: " << 100.0 * compressed.size() / uncompressed.size() << std::endl;

        // Verify that decompressed data matches original data
        if (uncompressed != fileContent) {
            std::cout << "Error: Decompressed data does not match original data!" << std::endl;
            size_t mismatch = 0;
            size_t line = 0;
			while (mismatch < uncompressed.size() && mismatch < fileContent.size()) {
                if (uncompressed[mismatch] != fileContent[mismatch]) {
                    break;
                }
                if (fileContent[mismatch] == '\n')
                    line++;
                mismatch++;
            }
			std::cout << "First mismatch at byte " << mismatch << " line " << line << std::endl;
            std::cout << uncompressed << std::endl;
            return 1;
        } else {
            std::cout << "Decompression verified successfully." << std::endl;
        }
    }
    return 0;
}
