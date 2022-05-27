<h1 align="center">
  <img src="https://github.com/isomorfeus/isomorfeus-ferret/blob/master/Logo.png?raw=true" align="center" width="216" height="234" />
  <br/>
&nbsp;&nbsp;&nbsp;Isomorfeus Ferret<br/>
</h1>

Convenient and well performing document store, indexing and search.

### Community and Support
At the [Isomorfeus Framework Project](https://isomorfeus.com)

## About this project

Isomorfeus-Ferret is a revived version of the original ferret gem created by Dave Balmain,
[https://github.com/dbalmain/ferret](https://github.com/dbalmain/ferret).
During revival many things havbe been fixed, now all tests pass, no crashes and it
successfully compiles and runs with rubys >3. Its no longer a goal to have
a c library available, but instead the usage is meant as ruby gem with a c extension only.

It works on *nixes, *nuxes, *BSDs and also works on Windows and RaspberryPi.

## Improvements and Changes in Version 0.14

### Breaking

- The API for LazyDocs has changed, they are read only now. LazyDoc#to_h may be used to create a hash, that may be changed and reindexed as doc.

### Performance

- LazyDoc is now truly lazy, fields are automatically retrieved. LazyDoc#load is no longer required, but may be used to preload all fields.
- Index#each is now multiple times faster, depending on use case.

### Other

- The Index class now includes Enumerable

## Improvements and Changes in Version 0.13

### Breaking

- For version 0.13 die index file format has changed and is no longer compatible with previous versions. Indexes of older versions must be recreated with 0.13 (export all data from and with previous version, import alls data with 0.13)
- The :store option no longer accepts :compress, compression must now be specified by the separate :compress options (see below).
- The ASCII-specific Tokenizers and Analyzers have been removed

### String Encoding support

#### Input strings and stored fields

In versions prior 0.13 the string encoding had to match the locale string encoding.
In 0.13 the dependency on the locale setting has been resolved, input strings are now correctly tokenized
according to their source encoding, with positions correctly matching the input string.
All Ruby string encodings are supported.
When fields are stored, they are now stored with the encoding, so that when they are retrieved again, they
retain the original encoding with positions matching the string in its original encoding.

#### Tokens, Terms, Filters and Queries

Tokens are internally converted to UTF-8, which may change their length compared to their original encoding,
yet they retain position information according to the source in its original encoding. Terms are likewise stored in UTF-8 encoding.
Queries are converted to UTF-8 encoding too.
The benefit is, that Filters, Stemmers or anything else working with Tokens and Terms only needs to support UTF-8 encoding,
greatly simplifying things and ensuring consistent query results, independent of source encoding.

### Compression

Compression semantics have changed, now Brotli, BZip2 and LZ4 compression codecs are supported.
- BZip2: slow compression, slow decompression, high compression ratio
- Brotli: slow compression, fast decrompression, high compression ratio, recommended for general purpose.
- LZ4: fast compression, fast decrompression, low compression ratio

To see performance and compression ratios `rake ferret_compression_bench` can be run from the cloned repo.
It uses data and code within the misc/ferret_vs_lucene directory.

To compress a stored field the :compression option can be used with one of: :no, :brotli, :bz2 or :lz4.
Example:
```ruby
fis.add_field(:compressed_field, :store => :yes, :compression => :brotli, :term_vector => :yes)
```

### Performance

For version 0.13.7 the performance bottle neck has been identified and removed, ferret now delivers excellent indexing perfomance on all platforms, see numbers below.
On Windows performance is still not as good as on Linux, but that is equally true for Lucene and because of how the Windows filesystem works.

## Documentation

The documentations is currently scattered throughout the repo.

For a quick start its best to read:
https://github.com/isomorfeus/isomorfeus-ferret/blob/master/TUTORIAL.md

Further:
https://github.com/isomorfeus/isomorfeus-ferret/blob/master/lib/isomorfeus/ferret/index/index.rb
https://github.com/isomorfeus/isomorfeus-ferret/blob/master/lib/isomorfeus/ferret/document.rb

The query language and parser are documented here:
https://github.com/isomorfeus/isomorfeus-ferret/blob/master/ext/isomorfeus_ferret_ext/frb_qparser.c

Examples can be found in the 'test' directory or in 'misc/ferret_vs_lucene'.

## Running Specs

- clone repo
- bundle install
- rake

Ensure your locale is set to C.UTF-8, because the internal c tests don't know how to handle localized output.

## Benchmarks

### Indexing and Searching
- clone repo
- bundle install
- rake ferret_vs_lucene

A recent Java JDK must be installed to compile and run lucene benchmarks.

Results, Ferret 0.14.0 vs. Lucene 9.1.0, WhitespaceAnalyzer,
Linux Ubuntu 20.04, FreeBSD 13.1 and Windows 10 on old Intel Core i5 from 2015,
LinuxPi on RaspberryPi 400:

| OS      | Task       | Ferret          | Lucene*        |
|---------|------------|-----------------|----------------|
| Linux   | Indexing   |     5125 docs/s |    4671 docs/s |
| FreeBSD | Indexing   |     4537 docs/s |    3831 docs/s |
| Windows | Indexing   |     2488 docs/s |    2588 docs/s |
| LinuxPi | Indexing   |     1200 docs/s |     551 docs/s |
| Linux   | Searching  | 26610 queries/s | 7165 queries/s |
| FreeBSD | Searching  | 24167 queries/s | 4288 queries/s |
| Windows | Searching  |  3901 queries/s | 1033 queries/s |
| LinuxPi | Searching  |  6194 queries/s |  769 queries/s |
|         | Index Size |           28 MB |          35 MB |

* JVM Versions:
OpenJDK Runtime Environment (build 18-ea+36-Ubuntu-1) (Linux)
OpenJDK Runtime Environment (build 17.0.3+7-Raspbian-1deb11u1rpt1) (LinuxPi)
OpenJDK Runtime Environment Temurin-18.0.1+10 (build 18.0.1+10) (Windows)
OpenJDK Runtime Environment (build 17.0.2+8-1) (FreeBSD)

### Storing Fields with Compression, Indexing and Retrieval

- clone repo
- bundle install
- rake ferret_compression_benchmark

Results on Linux, 0.14.0, on old Intel Core i5 from 2015:

| Compression | Index & Store | Retrieve Title | Index size |
|-------------|---------------|----------------|------------|
| none        |   4862 docs/s |  278827 docs/s |      43 MB |
| brotli      |   3559 docs/s |  178170 docs/s |      36 MB |
| bzip2       |   2628 docs/s |   81877 docs/s |      38 MB |
| lz4         |   4648 docs/s |  232236 docs/s |      41 MB |

## Future

Lots of things to do:
- Bring documentation in order in a docs directory
- Review code (especially for memory/stack issues, typical c issues)
- Take care of ruby GVL and threading
- See todo directory: https://github.com/isomorfeus/isomorfeus-ferret/tree/master/misc/todo

Any help, support much appreciated!
