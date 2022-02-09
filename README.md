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

It should work on *nixes and *nuxes and also works on Windows.

However, the revival is still fresh and although it appears to be working, issues have to be expected.

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
- rake units
- rake thread_safety

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
- Bring documentation in order in a docs directory
- Review code (especially for memory/stack issues, typical c issues)
- Take care of ruby GVL and threading
- See todo directory: https://github.com/isomorfeus/isomorfeus-ferret/tree/master/misc/todo

Any help, support much appreciated!
