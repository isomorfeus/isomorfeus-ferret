import org.apache.lucene.store.*;
import org.apache.lucene.analysis.standard.*;
import org.apache.lucene.index.*;
import org.apache.lucene.search.*;
import org.apache.lucene.queryparser.classic.*;

import java.nio.file.Path;
import java.nio.file.Paths;
import java.text.DecimalFormat;
import java.util.Date;
import java.util.Arrays;

class LuceneSearch {
  public static void main(String[] args) throws Exception, java.io.IOException, ParseException {
    int numReps = 1; // default: run once
    String arg;
    int i = 0;
    while (i < (args.length - 1) && args[i].startsWith("-")) {
      arg = args[i++];
      if (arg.equals("-reps"))
        numReps = Integer.parseInt(args[i++]);
      else
        throw new Exception("Unknown argument: " + arg);
    }

    String [] searchTerms = { "english", "gutenberg", "yesterday", "together", "america", "advanced", "president", "bunny" };
    Path indexPath = Paths.get("lucene_index");
    NIOFSDirectory indexDir = new NIOFSDirectory(indexPath);
    DirectoryReader dirReader = StandardDirectoryReader.open(indexDir);
    IndexSearcher searcher = new IndexSearcher(dirReader);
    TopDocs hits = null;
    QueryParser parser = new QueryParser("body", new StandardAnalyzer());

    // start the output
    System.out.println("Lucene Search");
    System.out.println("---------------------------------------------------------------");

    float[] times = new float[numReps];
    int q = 0;
    for (int rep = 1; rep <= numReps; rep++) {
      q = 0;
      int res = 0;
      long start = new Date().getTime();
      for (i = 0; i < 1000; i++) {
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
      times[rep - 1] = secs;
      printInterimReport(rep, secs, q);
    }
    printFinalReport(times, q);
  }

  // Print out stats for one run.
  private static void printInterimReport(int rep, float secs, int q) {
    DecimalFormat secsFormat = new DecimalFormat("#,##0.00");
    String secString = secsFormat.format(secs);
    System.out.println(rep + "  Secs: " + secString +
                       "  Queries: " + q + ", " +
                       String.valueOf((int)(q/secs)) + " queries/s");
  }

    // Print out aggregate stats
  private static void printFinalReport(float[] times, int q) {
    // produce mean and truncated mean
    Arrays.sort(times);
    float meanTime = 0.0f;
    float truncatedMeanTime = 0.0f;
    int numToChop = times.length >> 2;
    int numKept = 0;
    for (int i = 0; i < times.length; i++) {
        meanTime += times[i];
        // discard fastest 25% and slowest 25% of reps
        if (i < numToChop || i >= (times.length - numToChop))
            continue;
        truncatedMeanTime += times[i];
        numKept++;
    }
    meanTime /= times.length;
    truncatedMeanTime /= numKept;
    int numDiscarded = times.length - numKept;
    DecimalFormat format = new DecimalFormat("#,##0.00");
    String meanString = format.format(meanTime);
    String truncatedMeanString = format.format(truncatedMeanTime);

    // get the Lucene version
    String luceneVersion = org.apache.lucene.util.Version.getPackageImplementationVersion();

    System.out.println("---------------------------------------------------------------");
    System.out.println("Lucene " +  luceneVersion);
    System.out.println("JVM " + System.getProperty("java.version") +
                       " (" + System.getProperty("java.vendor") + ")");
    System.out.println(System.getProperty("os.name") + " " +
                       System.getProperty("os.version") + " " +
                       System.getProperty("os.arch"));
    System.out.println("Mean: " + meanString + " secs");
    System.out.println("Truncated mean (" +
                        numKept + " kept, " +
                        numDiscarded + " discarded): " +
                        truncatedMeanString + " secs, "+
                        String.valueOf((int)(q/truncatedMeanTime)) + " queries/s");
    System.out.println("---------------------------------------------------------------");
  }
}
