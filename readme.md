# MCComp Compression Library

A very simple byte-pair compression library in C++. It is intended
for running on a micro-controller and for reading and writing
compressed log files.

It will work on any data - including UTF-8 and binary - but it
is tuned for low-ascii text data.

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

Simple byte based approach - no bit fiddling.

* RLE is used for runs of 3 or more identical bytes. A RLE tag -
  which includes the size of the run - is written followed by
  the byte value.
* A table of common byte pairs is built on the fly. Any high ascii
  value is interpreted as an index into the table.
* High ascii values which are written as a high ascii marker followed
  by the value itself.
* Low ascii values are written as-is.