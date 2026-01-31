# MCComp Compression Library

A simple byte-pair compression library in C++, designed for microcontrollers and compressed log file processing.

MCComp works on any data (UTF-8, binary, etc.) but is optimized for low-ASCII text data. It provides efficient compression without external dependencies or memory allocation.

## Features

* Simple and small codebase (single C++ source file)
* No memory allocation
* Fast compression and decompression
* Incremental processing of data in chunks

## Performance

It reduces file size to about 70-75% of original size on typical
text files. (Shockingly good for such a simple algorithm and
teeny codebase.)

It was inspired by https://github.com/antirez/smaz and 
https://github.com/Ed-von-Schleck/shoco, which both are very nifty
libraries. BUT they have the drawback of requiring a dictionary
(possibly purpose built for a given corpus) which I wanted to avoid.
MCComp uses similar ideas but builds its dictionary on the fly.

## Algorithm

MCComp uses a byte-based approach without bit manipulation:

* RLE is used for runs of 3 or more identical bytes. A byte in
  the range of [kRLEStart, kRLEEnd] indicates a run and the
  run length. The next byte is the repeated value.
* A table of common byte pairs is built on the fly. The table
  is a fast index into common byte pairs seen. Byte pairs not
  used are incrementally evicted. A byte in the range of
  [kTableStart, kTableEnd] indicates a byte pair from the table,
  at index (value - kTableStart).
* Values used as RLE or Table tags are escaped by writing
  kLiteral then the value. A degenerate input will have all values
  that need to be escaped, and the compressed size will be double
  the original size.
* Remaining values are written as is.

