import org.apache.lucene.store.*;
import org.apache.lucene.analysis.standard.*;
import org.apache.lucene.index.*;
import org.apache.lucene.search.*;
import org.apache.lucene.queryparser.classic.*;

import java.nio.file.Path;
import java.nio.file.Paths;
import java.text.DecimalFormat;
import java.util.Date;

class LuceneSearch {
  public static void main(String[] args) throws java.io.IOException, ParseException {
    String [] searchTerms = { "english", "gutenberg", "yesterday", "together", "america", "advanced", "president", "bunny" };
    Path indexPath = Paths.get("lucene_index");
    NIOFSDirectory indexDir = new NIOFSDirectory(indexPath);
    DirectoryReader dirReader = StandardDirectoryReader.open(indexDir);
    IndexSearcher searcher = new IndexSearcher(dirReader);
    TopDocs hits = null;
    QueryParser parser = new QueryParser("body", new StandardAnalyzer());
    int res = 0;
    int q = 0;
    long start = new Date().getTime();
    for (int i = 0; i < 1000; i++) {
      for (String st : searchTerms) {
        q++;
        Query query = parser.parse(st);
        hits = searcher.search(query, 10);
        for (int c = 0 ; c < hits.scoreDocs.length; c++) {
          res++;
        }
      }
    }
    long end = new Date().getTime();
    float secs = (float)(end - start) / 1000;
    DecimalFormat secsFormat = new DecimalFormat("#,##0.00");
    String secString = secsFormat.format(secs);
    System.out.println("Searching took: " + secString + "s for " + q + " queries\nthats " + (int)(q/secs) + " q/s");
    System.out.println("Total found: " + res);
  }
}
