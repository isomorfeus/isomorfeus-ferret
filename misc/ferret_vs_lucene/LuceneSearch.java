import org.apache.lucene.store.*;
import org.apache.lucene.document.*;
import org.apache.lucene.analysis.*;
import org.apache.lucene.analysis.standard.*;
import org.apache.lucene.index.*;
import org.apache.lucene.search.*;
import org.apache.lucene.queryparser.classic.*;

import java.nio.file.Path;
import java.nio.file.Paths;

class LuceneSearch {
  public static void main(String[] args) throws java.io.IOException, ParseException {
    String [] searchTerms = { "english", "gutenberg", "yesterday", "together", "america", "advanced", "president", "bunny" };
    Path indexPath = Paths.get("lucene_index");
    NIOFSDirectory indexDir = new NIOFSDirectory(indexPath);
    DirectoryReader dirReader = StandardDirectoryReader.open(indexDir);
    IndexSearcher searcher = new IndexSearcher(dirReader);
    TopDocs hits = null;
    QueryParser parser = new QueryParser("contents", new StandardAnalyzer());

    for (int i = 0; i < 1000; i++) {
      for (String st : searchTerms) {
        Query query = parser.parse(st);
        hits = searcher.search(query, 20000);
        for (int c = 0 ; c < hits.totalHits.value && c < 20; c++) {
          ScoreDoc d = hits.scoreDocs[c];
          System.out.println("Found document: " + c + " with score: " + d.score);
        }
      }
    }
  }
}
