import org.apache.lucene.store.NIOFSDirectory;
import org.apache.lucene.index.IndexWriter;
import org.apache.lucene.index.IndexWriterConfig;
import org.apache.lucene.analysis.core.WhitespaceAnalyzer;
import org.apache.lucene.document.Document;
import org.apache.lucene.document.Field;
import org.apache.lucene.document.FieldType;

import java.io.File;
import java.io.BufferedReader;
import java.io.FileReader;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.text.DecimalFormat;
import java.util.Date;
import java.util.List;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Arrays;

/**
 * This code is taken from Marvin Humphrey's KinoSearch benchmarks but any
 * errors have probably been added by myself, Dave Balmain.
 *
 * LuceneIndexer - benchmarking app
 * usage: java LuceneIndexer [-docs MAX_TO_INDEX] [-reps NUM_REPETITIONS]
 *                           [-increment NUM_REPETITIONS]
 *
 * -docs        specifies maximum number of documents to index. There are a
 *              little under 20,000 in the Reuters corpus.
 * -reps        the number of time to run the test
 * -increment   The number of documents to add before closing and reopening
 *              the IndexWriter
 *
 * Recommended options: -server -Xmx500M -XX:CompileThreshold=100
 */
public class LuceneIndexer {
  static File corpusDir = new File("corpus");
  static Path indexPath = Paths.get("lucene_index");
  static String[] fileList;

  public static void main (String[] args) throws Exception {
    // assemble the sorted list of article files
    fileList = buildFileList();

    // parse command line args
    int maxToIndex = fileList.length; // default: index all docs
    int numReps    = 1;               // default: run once
    int increment  = 0;
    String arg;
    int i = 0;
    while (i < (args.length - 1) && args[i].startsWith("-")) {
      arg = args[i++];
      if (arg.equals("-docs"))
         maxToIndex = Integer.parseInt(args[i++]);
      else if (arg.equals("-reps"))
        numReps = Integer.parseInt(args[i++]);
      else if (arg.equals("-increment"))
        increment = Integer.parseInt(args[i++]);
      }
      else
        throw new Exception("Unknown argument: " + arg);
    }
    increment = increment == 0 ? maxToIndex + 1 : increment;

    // start the output
    System.out.println("---------------------------------------------------");

    // build the index numReps times, then print a final report
    float[] times = new float[numReps];
    for (int rep = 1; rep <= numReps; rep++) {
      // start the clock and build the index
      long start = new Date().getTime();
      int numIndexed = buildIndex(fileList, maxToIndex, increment);

      // stop the clock and print a report
      long end = new Date().getTime();
      float secs = (float)(end - start) / 1000;
      times[rep - 1] = secs;
      printInterimReport(rep, secs, numIndexed);
    }
    printFinalReport(times);
  }

  // Return a lexically sorted list of all article files from all subdirs.
  static String[] buildFileList () throws Exception {
    File[] articleDirs = corpusDir.listFiles();
    List<String> filePaths = new ArrayList<String>();
    for (int i = 0; i < articleDirs.length; i++) {
      File[] articles = articleDirs[i].listFiles();
      for (int j = 0; j < articles.length; j++) {
        String path = articles[j].getPath();
        if (path.indexOf("article") == -1)
          continue;
        filePaths.add(path);
      }
    }
    Collections.sort(filePaths);
    return (String[])filePaths.toArray(new String[filePaths.size()]);
  }

  // Initialize an IndexWriter
  static IndexWriter initWriter (int count) throws java.io.IOException {
    boolean create = count > 0 ? false : true;
    NIOFSDirectory indexDir = new NIOFSDirectory(indexPath);
    IndexWriterConfig iwc = new IndexWriterConfig(new WhitespaceAnalyzer());
    iwc.setOpenMode(IndexWriterConfig.OpenMode.CREATE);
    IndexWriter writer = new IndexWriter(indexDir, iwc);

    return writer;
  }

  // Build an index, stopping at maxToIndex docs if maxToIndex > 0.
  static int buildIndex (String[] fileList, int maxToIndex, int increment) throws Exception {
    IndexWriter writer = initWriter(0);
    int docsSoFar = 0;

    while (docsSoFar < maxToIndex) {
      for (int i = 0; i < fileList.length; i++) {
        // add content to index
        File f = new File(fileList[i]);
        Document doc = new Document();
        BufferedReader br = new BufferedReader(new FileReader(f));

        try {
          // the title is the first line
          String title;
          if ( (title = br.readLine()) == null)
            throw new Exception("Failed to read title");
          FieldType fieldTypeTitle = new FieldType();
          fieldTypeTitle.setStored(true);
          fieldTypeTitle.setStoreTermVectors(false);
          fieldTypeTitle.setTokenized(true);
          Field titleField = new Field("title", title, fieldTypeTitle);
          doc.add(titleField);

          // the body is the rest
          StringBuffer buf = new StringBuffer();
          String str;
          while ( (str = br.readLine()) != null )
            buf.append( str );
          String body = buf.toString();
          FieldType fieldTypeBody = new FieldType();
          fieldTypeBody.setIndexOptions(org.apache.lucene.index.IndexOptions.DOCS_AND_FREQS_AND_POSITIONS_AND_OFFSETS);
          fieldTypeBody.setStored(true);
          fieldTypeBody.setStoreTermVectors(true);
          fieldTypeBody.setStoreTermVectorPositions(true);
          fieldTypeBody.setStoreTermVectorOffsets(true);
          fieldTypeBody.setTokenized(true);
          Field bodyField = new Field("body", body, fieldTypeBody);
          doc.add(bodyField);

          writer.addDocument(doc);
        } finally {
          br.close();
        }

        docsSoFar++;
        if (docsSoFar >= maxToIndex)
          break;
        if (docsSoFar % increment == 0) {
          writer.close();
          writer = initWriter(docsSoFar);
        }
      }
    }

    // finish index
    int numIndexed = writer.getDocStats().numDocs;
    // writer.optimize();
    writer.close();

    return numIndexed;
  }

  // Print out stats for one run.
  private static void printInterimReport(int rep, float secs,
                                         int numIndexed) {
    DecimalFormat secsFormat = new DecimalFormat("#,##0.00");
    String secString = secsFormat.format(secs);
    System.out.println(rep + "   Secs: " + secString +
                       "  Docs: " + numIndexed + ", " +
                       String.valueOf((int)(numIndexed/secs)) + " docs/s");
  }

  // Print out aggregate stats
  private static void printFinalReport(float[] times) {
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

    System.out.println("---------------------------------------------------");
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
                        truncatedMeanString + " secs");
    System.out.println("---------------------------------------------------");
  }
}