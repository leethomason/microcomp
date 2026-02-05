#include "src/mccomp.h"

#include <cstdio>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <assert.h>
#include <stdio.h>
#include <array>


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

void testSmallBinary()
{
    std::array<uint8_t, 4> in = { 0, 255, 1, 254 };
    std::array<uint8_t, 8> compressed;
    std::array<uint8_t, 4> out;

    mccomp::Compressor c;
    mccomp::Result r = c.compress(in.data(), int(in.size()), compressed.data(), int(compressed.size()));
    TEST(r.nInput == in.size());
    TEST(r.nOutput <= compressed.size());
    TEST(compressed[0] == mccomp::kLiteral);
    TEST(compressed[1] == 0);
    TEST(compressed[2] == mccomp::kLiteral);
    TEST(compressed[3] == 255);
    TEST(compressed[4] == mccomp::kLiteral);
    TEST(compressed[5] == 1);
    TEST(compressed[6] == mccomp::kLiteral);
    TEST(compressed[7] == 254);

    int compressedSize = r.nOutput;

    mccomp::Decompressor d;
    r = d.decompress(compressed.data(), int(compressed.size()), out.data(), int(out.size()));
    TEST(r.nInput == compressedSize);
    TEST(r.nOutput == 4);

    TEST(out == in);
}

void testBinary()
{
    std::array<uint8_t, 512> in;
    for (int i = 0; i < 512; i++)
       in[i] = uint8_t(i);

    std::array<uint8_t, 1024> compressed;
    std::array<uint8_t, 512> out;

    mccomp::Compressor c;
    mccomp::Result r = c.compress(in.data(), int(in.size()), compressed.data(), int(compressed.size()));
    TEST(r.nInput == in.size());
    TEST(r.nOutput <= compressed.size());
    int compressedSize = r.nOutput;

    mccomp::Decompressor d;
    r = d.decompress(compressed.data(), int(compressed.size()), out.data(), int(out.size()));
    TEST(r.nInput == compressedSize);
    TEST(r.nOutput == 512);

    TEST(out == in);
}

void testEOF()
{
    std::array<uint8_t, 64> in;
    for (int i = 0; i < 64; i++)
        in[i] = uint8_t('0' + (i % 10));
    
    std::array<uint8_t, 64> compressed;
    compressed.fill(0xff);
    {
        size_t pos = 0;
        static constexpr size_t kBufSize = 16;
        uint8_t buf[kBufSize];
    }

}

bool compareFiles(const std::string& filename1, const std::string& filename2) {
    std::ifstream file1(filename1, std::ifstream::ate | std::ifstream::binary);
    std::ifstream file2(filename2, std::ifstream::ate | std::ifstream::binary);

    if (!file1.is_open() || !file2.is_open()) {
        std::cerr << "Error opening one or both files." << std::endl;
        return false;
    }

    if (file1.tellg() != file2.tellg()) {
        return false;
    }

    file1.seekg(0);
    file2.seekg(0);

    return std::equal(std::istreambuf_iterator<char>(file1),
        std::istreambuf_iterator<char>(),
        std::istreambuf_iterator<char>(file2));
}

void canonTest()
{
    std::ifstream inFile("test.log");
    TEST(inFile.is_open());
    std::ofstream compFile("test-comp.dat", std::ios::binary);
    TEST(compFile.is_open());
    std::ofstream outFile("test-out.log");
    TEST(outFile.is_open());

    static constexpr size_t kBufferSize = 100;
    char readBuffer[kBufferSize];     // uncompressed
    char writeBuffer[kBufferSize];    // compressed
    size_t inSize = 0;
    size_t compSize = 0;

    {
        mccomp::Compressor comp;
        while (true) {
            inFile.read(readBuffer, kBufferSize);
            size_t nRead = inFile.gcount();
            if (nRead == 0)
                break;
            inSize += nRead;

            // This is a little tricky! The readbuffer may not be consumed by one
            // call to compress(), so it needs to be iterated through.
            size_t pos = 0;
            while (pos < nRead) {
                mccomp::Result r = comp.compress(
                    (const uint8_t*)(readBuffer + pos),
                    int(nRead - pos),
                    (uint8_t*)writeBuffer,
                    kBufferSize);

                compFile.write(writeBuffer, r.nOutput);
                pos += r.nInput;
            }
        }
    }
    inFile.close();
    compFile.close();
    
    std::ifstream compFileIn("test-comp.dat", std::ios::binary);
    TEST(compFileIn.is_open());
    {
        mccomp::Decompressor dec;
        while (true) {
            compFileIn.read(readBuffer, kBufferSize);
            size_t nRead = compFileIn.gcount();
            if (nRead == 0)
                break;
            compSize += nRead;

            // This is a little tricky! The readbuffer may not be consumed by one
            // call to compress(), so it needs to be iterated through.
            size_t pos = 0;
            while (pos < nRead) {
                mccomp::Result r = dec.decompress(
                    (const uint8_t*)(readBuffer + pos), 
                    int(nRead - pos), 
                    (uint8_t*)writeBuffer, 
                    kBufferSize);

                outFile.write(writeBuffer, r.nOutput);
                pos += r.nInput;
            }
        }
    }
    compFileIn.close();
    outFile.close();

    TEST(compareFiles("test.log", "test-out.log"));
    std::cout << "Canon test compression: " << 1.0 * compSize / inSize << "\n";
}

int cycle(const std::string& fileContent, bool log, int buffer0 = 40, int buffer1 = 40) 
{
    static constexpr int kBufferAlloc = 40;
    assert(buffer0 <= kBufferAlloc);
    assert(buffer1 <= kBufferAlloc);

    std::vector<uint8_t> compressed;
    {
        uint8_t workingIn[kBufferAlloc + 1];
        uint8_t workingOut[kBufferAlloc + 1];
        memset(workingIn, 0, kBufferAlloc + 1);
        memset(workingOut, 0, kBufferAlloc + 1);

        mccomp::Compressor compressor;

        // Iterate over the input stream.
        size_t pos = 0;
        while (pos < fileContent.size()) {
            size_t nBytes = std::min(size_t(buffer0), (fileContent.size() - pos));

            // Want to test that there aren't overruns, etc,
            // so copy the input to workingIn.
            memcpy(workingIn, (const uint8_t*)(fileContent.data() + pos), nBytes);

            mccomp::Result r = compressor.compress(
                workingIn, int(nBytes),
                workingOut, buffer1);

            assert(workingIn[buffer0] == 0);
            assert(workingOut[buffer1] == 0);
            assert(r.nInput <= buffer0);
            assert(r.nOutput <= buffer1);

            for (size_t i = 0; i < r.nOutput; i++) {
                TEST(workingOut[i] != 255);
                compressed.push_back(workingOut[i]);
            }
            pos += r.nInput;
        }
    }
    std::string uncompressed;
    int nEntries = 0;
    int nTotal = 0;

    {
        uint8_t workingIn[kBufferAlloc + 1];
        uint8_t workingOut[kBufferAlloc + 1];
        memset(workingIn, 0, kBufferAlloc + 1);
        memset(workingOut, 0, kBufferAlloc + 1);

        mccomp::Decompressor decompressor;

        // Iterate over the output stream size.
        // If known, could alternatively use the input stream size.
        size_t pos = 0;
        while (pos < compressed.size()) {
            size_t nBytes = std::min(size_t(buffer0), compressed.size() - pos);
            memcpy(workingIn, compressed.data() + pos, nBytes);

            mccomp::Result r = decompressor.decompress(
                workingIn, int(nBytes),
                workingOut, buffer1);

            assert(workingIn[buffer0] == 0);
            assert(workingOut[buffer1] == 0);
            assert(r.nInput <= buffer0);
            assert(r.nOutput <= buffer1);

            for (size_t i = 0; i < r.nOutput; i++)
                uncompressed.push_back(workingOut[i]);
            pos += r.nInput;
        }

        if (log) {
            std::cout << "Input: " << fileContent.size() << std::endl;
            std::cout << "Uncompressed: " << uncompressed.size() << " Compressed: " << compressed.size() << " bytes" << std::endl;
            std::cout << "Ratio%: " << 100.0 * compressed.size() / uncompressed.size() << std::endl;
            std::cout << "Utilization: nEntries = " << nEntries << " / " << mccomp::kTableSize << " nTotal = " << nTotal << "\n";
        }
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
        }
        else {
            if (log) 
                std::cout << "Decompression verified successfully." << std::endl;
        }
    }
    return 0;
}

int main(int argc, char* argv[]) {
    RUN_TEST(testTable());
	RUN_TEST(testComp0());
    RUN_TEST(testComp1());
	RUN_TEST(testSmallBinary());
    RUN_TEST(testBinary());
    RUN_TEST(canonTest());

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
    
    int rc = cycle(fileContent, true);

    // Check different buffer sizes.
    for (int i = 16; i < 40; i += 3) {
        for (int j = 16; j < 40; j += 4) {
            rc = cycle(fileContent, false, i, j);
            if (rc)
                break;
        }
    }

    return rc;
}
