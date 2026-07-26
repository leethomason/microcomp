# MicroComp Compression Library

A simple compression library in C++. Designed for compressing log
files on resource-constrained systems such as microcontrollers.

MicroComp works on any data (UTF-8, binary, etc.) but is optimized for low-ASCII
text data. It provides efficient compression without external dependencies or
memory allocation.

It was made to compress log files on a microcontroller before being written
to flash storage, and then decompressed on the same microcontroller
when reading it back. It's a narrow use case, and perhaps not widely useful, but
it was fun to make and I hope others find it useful.

## Features

* Simple and small codebase
* No memory allocation
* Fast compression and decompression
* Optimized for low-ASCII text data, but works on any data
* Incremental processing of data in chunks
* ASCII strings never expand
* Compressor and decompressor use less than 600 bytes each
* No table or dictionary is stored in the compressed data

## Performance

It reduces file size to roughly 55-70% of original size on typical
text files. I was shocked at how well it worked given the simplicity
of the algorithm.

It was inspired by https://github.com/antirez/smaz and
https://github.com/Ed-von-Schleck/shoco, which are both very nifty
libraries, but they have the drawback of requiring a dictionary
(possibly purpose-built for a given corpus) which I wanted to avoid.
MicroComp uses similar ideas but builds its dictionary on the fly. Also
thanks to https://qoiformat.org/ for exploring simple compression schemes.

Thanks to https://github.com/logpai/loghub for test files.

## Algorithm

MicroComp uses a byte-based approach without bit manipulation:

* RLE is used for runs of 3 to 11 identical bytes. A byte in
  the range of [kRLEStart, kRLEEnd] indicates a run and the
  run length. The next byte is the repeated value.
* A table of common byte pairs is built on the fly. The table
  is a fast hash into common byte pairs seen. Byte pairs not
  used are incrementally evicted. A byte in the range of
  [kTableStart, kTableEnd] indicates a byte pair from the table,
  at index (value - kTableStart).
* Values used as RLE or Table tags are escaped by writing
  kLiteral then the value. A degenerate input will have all values
  that need to be escaped, and the compressed size will be double
  the original size.
* Remaining values are written as is.

## End of File on Flash Memory

Flash memory is erased to 0xff, not 0, because flash memory
writes only write 0 bits. MicroComp is designed with flash-memory storage
in mind, so there's a little extra accommodation for this case.

For text, the value 255 can be used as EOF:
* 0xff isn't ASCII
* (luckily) 0xff isn't UTF-8 either

Therefore, if the input is text (ASCII or UTF-8), a 255-byte value will never be 
written to the compressed stream, and you can use 255/0xff as EOF on the
compressed data. `testEOF()` shows this in action.

## Usage

If you have the full size of the data to compress/decompress in memory,
it's as simple as:

```cpp
    mccomp::Compressor c;
    c.compress(in.data(), int(in.size()), compressed.data(), int(compressed.size()));

    ...

    mccomp::Decompressor d;
    d.decompress(compressed.data(), int(compressed.size()), out.data(), int(out.size()));
```

But incremental processing is how it is intended to be used.
For an example of this, check out `canonTest()` in `main.cpp`.

The steps are:

1. Create a `Compressor` or `Decompressor` object.
2. Create input and output buffers. These can be re-used across calls. Anything from 16 bytes
   or larger should work fine. Around 40 bytes to 10K is a good size, depending on how much
   stack space you have.

The inner loop for compression:

```cpp

    mccomp::Compressor comp;
    while (true) {
        inFile.read(readBuffer, kBufferSize); // input stream
        size_t nRead = inFile.gcount();
        if (nRead == 0)
            break;

        // This is a little tricky! The readbuffer may not be consumed by one
        // call to compress(), so it needs to be iterated through.
        size_t pos = 0;
        while (pos < nRead) {
            mccomp::Result r = comp.compress(
                (const uint8_t*)(readBuffer + pos),
                int(nRead - pos),
                (uint8_t*)writeBuffer,
                kBufferSize);

            compFile.write(writeBuffer, r.nOutput); // output (compressed) stream
            pos += r.nInput;
        }
    }

```

Likewise for decompression:

```cpp
    mccomp::Decompressor dec;
    while (true) {
        compFileIn.read(readBuffer, kBufferSize); // input (compressed) stream
        size_t nRead = compFileIn.gcount();
        if (nRead == 0)
            break;

        // This is a little tricky! The readbuffer may not be consumed by one
        // call to compress(), so it needs to be iterated through.
        size_t pos = 0;
        while (pos < nRead) {
            mccomp::Result r = dec.decompress(
                (const uint8_t*)(readBuffer + pos), 
                int(nRead - pos), 
                (uint8_t*)writeBuffer, 
                kBufferSize);

            outFile.write(writeBuffer, r.nOutput); // output (uncompressed) stream
            pos += r.nInput;
        }
    }
```


## License

MIT License. See `LICENSE` file.
