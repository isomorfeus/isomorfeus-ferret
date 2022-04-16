<h1 align="center">
  <img src="https://github.com/isomorfeus/isomorfeus-ferret/blob/master/Logo.png?raw=true" align="center" width="216" height="234" />
  <br/>
&nbsp;&nbsp;&nbsp;Isomorfeus Ferret<br/>
</h1>

Convenient and well performing document store, indexing and search.

### Community and Support
At the [Isomorfeus Framework Project](https://isomorfeus.com)

## About this project

Isomorfeus-Ferret is a revived version of the original ferret gem created by Dave Balmain.
During revival many things havbe been fixed, now all tests pass, no crashes and it
successfully compiles and runs with rubys >3. Its no longer a goal to have
a c library available, but instead the usage is meant as ruby gem with a c extension only.

It should work on *nixes, *nuxes, *BSDs and also works on Windows.

## Improvements and Changes in Version 0.13

### Breaking

- For version 0.13 die index file format has changed and is no longer compatible with previous versions. Indexes of older versions must be recreated with 0.13 (export all data from and with previous version, import alls data with 0.13)
- The :store option no longer accepts :compress, compression must now be specified by the separate :compress options (see below).
- The ASCII-specific Tokenizers and Analyzers have been removed

### Sring Encoding support

#### Input strings and stored fields

In versions prior 0.13 the string encoding had to match the locale string encoding.
In 0.13 the dependency on the locale setting has been resolved, input strings are now correctly tokenized
according to their source encoding, with positions correctly matching the input string.
All Ruby string encodings are supported.
When fields are stored, they are now stored with the encoding, so that when they are retrieved again, they
retain the original encoding with positions matching the string in its original encoding.

#### Tokens and Filters

Tokens are internally converted to UTF-8, which may change their length compared to their original encoding,
yet they retain position information according to the source in its original encoding.
The benefit is, that Filters, Stemmers or anything else working with Tokens only needs to support UTF-8 encoding,
greatly simplifying things and ensuring consistent query results.

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

The encoding support demands its toll, indexing performance dropped a bit in comparision to 0.12, but still thousands of docs per second, depending on machine/docs.
On Windows the indexing performance is still terrible, but that may be resolved in a future project.

Search performance is still excellent and multiple times faster than Lucene.

Lucene achieves roughly double the indexing performance. This seems to be because of the different way strings and
encodings are handled in Java. For example, the Java WhitespaceTokenizer code requires only one method call per character (check for whitespace), but for Ruby, to support all the different encodings, several method calls are required per character (retrieve character according to encoding, check character for whitespace).
Ferret is internally using the standard Ruby string encoding methods.

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

- clone repo
- bundle install
- rake ferret_vs_lucene

A recent Java JDK must be installed to compile and run lucene benchmarks.

Results on Linux:
```
Ferret:
Indexing Secs: 7.36  Docs: 19043, 2587 docs/s
Searching took: 0.3366296s for 8000 queries
thats 23765 q/s

Lucene:
Indexing Secs: 4.22  Docs: 19043, 4516 docs/s
Searching took: 1.48s for 8000 queries
thats 5420 q/s
---------------------------------------------------
Lucene 9.0.0 0b18b3b965cedaf5eb129aa41243a44c83ca826d - jpountz - 2021-12-01 14:23:49
JVM 17.0.1 (Private Build)
```

## Future

Lots of things to do:
- Improve indexing performance on Windows (WriteFile is terribly slow, maybe use mapping, see libuv)
- Bring documentation in order in a docs directory
- Review code (especially for memory/stack issues, typical c issues)
- Take care of ruby GVL and threading
- See todo directory: https://github.com/isomorfeus/isomorfeus-ferret/tree/master/misc/todo

Any help, support much appreciated!
