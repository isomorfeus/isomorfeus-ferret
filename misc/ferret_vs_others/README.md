Ferret versus Apache Lucene: performance benchmarks

Original author: Dave Balmain
with updates by others

*Disclaimer*: These benchmarks were written by myself, the developer of Ferret so they may be slightly biased. I have submitted them to the Lucene mailing list so that Lucene developers can check the fairness of these benchmarks. The numbers below are in no way an indication of the quality of either library. Lucene is currently a lot more stable than Ferret. The reason I have run these benchmarks against Lucene is that Ferret was originally ported from Lucene and is still very strongly influence by that library. I also believe Lucene is the gold standard for information retrieval libraries.

All suggestions/comments/critiques are most welcome and should be directed to myself at dbalmain.ml at gmail.com. Alternatively just add your comments to this page.

Apache Lucene can be downloaded here http://lucene.apache.org/core/downloads.html.
Corpus (Test Data)

The corpus I used for testing is Reuters-21578, Distribution 1.0. It is good because it has a reasonably large number of documents (19043) of only 150 words each on average making it easily downloadable. Different test data will give very different results. If benchmarking for your own application you should use test data similar to that which you will be working with.

To extract the data for indexing I have supplied an extraction script. To use it, download the Reuters collection, extract the archive and call:

bundle exec ruby reuters_extract.rb /path/to/expanded/archive/dir

This creates a 'corpus' dir, but thats not really necessary, the 'corpus' dir come supplied with the repo. Then run the indexer:

bundle exec ruby ferret_indexer.rb

This creates the 'ferret_index' dir for usage by the search and shows indexing performance.
To search:

bundle exec ruby ferret_search.rb

For Lucene, compiling the indexer (Use ':' instead of ';' within the classpath as separator on Platforms other than Windows):

javac -classpath lucene-analyzers-common-8.10.1.jar;lucene-core-8.10.1.jar;. LuceneIndexer.java

The indexer needs the extracted 'corpus' dir above. Running the indexer:

java -classpath lucene-analyzers-common-8.10.1.jar;lucene-core-8.10.1.jar;. LuceneIndexer

Compiling the searcher:

javac -classpath lucene-analyzers-common-8.10.1.jar;lucene-core-8.10.1.jar;lucene-queryparser-8.10.1.jar;. LuceneSearch.java

Running the searcher:

javac -classpath lucene-analyzers-common-8.10.1.jar;lucene-core-8.10.1.jar;lucene-queryparser-8.10.1.jar;. LuceneSearch



Indexing performance

(The following is fairly obsolete.)

For the indexing benchmark we need to look a few different situations. Most importantly, the following benchmarks look at performance when storing the field with term-vectors and not storing the field or term vectors. They also have options for reopening the IndexWriter at regular intervals. I use a WhiteSpaceAnalyzer so that analysis time will have little effect on the results. Here are the indexing benchmarking programs:

    LuceneIndexingBenchmarker
    FerretIndexingBenchmarker

I run each test 6 times and the top and bottom results are thrown away to the HotSpot warmup should have no effect on the Lucene results.
Unstored Without Term-Vectors

dbalmain@ubuntu:~/sandpit/benchmarks $ java -classpath lucene-core-2.0.0.jar:. -server -Xmx500M -XX:CompileThreshold=100 LuceneIndexer -reps 6
---------------------------------------------------
1   Secs: 37.96  Docs: 19043
2   Secs: 24.17  Docs: 19043
3   Secs: 23.19  Docs: 19043
4   Secs: 22.43  Docs: 19043
5   Secs: 21.23  Docs: 19043
6   Secs: 21.86  Docs: 19043
---------------------------------------------------
Lucene 2.0.0
JVM 1.5.0_06 (Sun Microsystems Inc.)
Linux 2.6.15-27-386 i386
Mean: 25.14 secs
Truncated mean (4 kept, 2 discarded): 22.91 secs
---------------------------------------------------
dbalmain@ubuntu:~/sandpit/benchmarks $ ruby ferret_indexer.rb --reps 6
------------------------------------------------------------
0  Secs: 6.18  Docs: 19043
1  Secs: 6.37  Docs: 19043
2  Secs: 7.25  Docs: 19043
3  Secs: 6.15  Docs: 19043
4  Secs: 6.15  Docs: 19043
5  Secs: 6.23  Docs: 19043
------------------------------------------------------------
Mean 6.39 secs
Truncated Mean (4 kept, 2 discarded): 6.23 secs
------------------------------------------------------------

Stored Without Term-Vectors

dbalmain@ubuntu:~/sandpit/benchmarks $ java -classpath lucene-core-2.0.0.jar:. -server -Xmx500M -XX:CompileThreshold=100 LuceneIndexer -reps 6 -store 1
---------------------------------------------------
1   Secs: 53.70  Docs: 19043
2   Secs: 37.56  Docs: 19043
3   Secs: 36.50  Docs: 19043
4   Secs: 34.90  Docs: 19043
5   Secs: 41.11  Docs: 19043
6   Secs: 34.32  Docs: 19043
---------------------------------------------------
Lucene 2.0.0
JVM 1.5.0_06 (Sun Microsystems Inc.)
Linux 2.6.15-27-386 i386
Mean: 39.68 secs
Truncated mean (4 kept, 2 discarded): 37.52 secs
---------------------------------------------------
dbalmain@ubuntu:~/sandpit/benchmarks $ ruby ferret_indexer.rb --reps 6 --store
------------------------------------------------------------
0  Secs: 12.47  Docs: 19043
1  Secs: 13.59  Docs: 19043
2  Secs: 12.50  Docs: 19043
3  Secs: 12.44  Docs: 19043
4  Secs: 12.60  Docs: 19043
5  Secs: 12.81  Docs: 19043
------------------------------------------------------------
Mean 12.74 secs
Truncated Mean (4 kept, 2 discarded): 12.60 secs
