#include "src/mccomp.h"

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <assert.h>

int main(int argc, char* argv[]) {
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
        }

        std::cout << "Original: " << uncompressed.size() << " Compressed: " << compressed.size() << " bytes" << std::endl;
        std::cout << "Ratio%: " << 100.0 * compressed.size() / uncompressed.size() << std::endl;

        // Verify that decompressed data matches original data
        if (uncompressed != fileContent) {
            std::cout << "Error: Decompressed data does not match original data!" << std::endl;
            std::cout << uncompressed << std::endl;
            return 1;
        } else {
            std::cout << "Decompression verified successfully." << std::endl;
        }
    }




    
    return 0;
}
