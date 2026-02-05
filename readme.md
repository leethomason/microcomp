# MCComp Compression Library

A simple byte-pair compression library in C++, designed for microcontrollers
and compressed log file processing.

MCComp works on any data (UTF-8, binary, etc.) but is optimized for low-ASCII
text data. It provides efficient compression without external dependencies or
memory allocation.

It was made to compress log files on a microcontroller before being written
to slow flash storage, and then decompressing on the same microcontroller
when reading back. It's a narrow use case, and perhaps not widely useful, but
it was fun to make and I hope others find it useful.

## Features

* Simple and small codebase
* No memory allocation
* Fast compression and decompression
* Incremental processing of data in chunks
* ASCII strings never expand

## Performance

It reduces file size to about 60-75% of original size on typical
text files. I was shocked at how well it worked given the simplicity
of the algorithm.

It was inspired by https://github.com/antirez/smaz and 
https://github.com/Ed-von-Schleck/shoco, which both are very nifty
libraries. but they have the drawback of requiring a dictionary
(possibly purpose built for a given corpus) which I wanted to avoid.
MCComp uses similar ideas but builds its dictionary on the fly. Also
https://qoiformat.org/ for exploring simple compression schemes.

Thanks to https://github.com/logpai/loghub for test files.

## Algorithm

MCComp uses a byte-based approach without bit manipulation:

* RLE is used for runs of 3 or more identical bytes. A byte in
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
writes only write 0 bits. If you are reading a file from flash,
how do you know if you reached the end?

For text, the value 255 can be used as EOF
* it isn't ASCII
* it isn't UTF-8 either

(Binary will require looking for a long run of 0xff or maintaining a length.)

That's handy. MCComp maintains this. If the input is text (ASCII
or UTF-8) a 255 byte value will never be written to the compressed stream.
