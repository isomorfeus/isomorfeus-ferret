require File.expand_path(File.join(File.dirname(__FILE__), "..", "..", "test_helper.rb"))

class MultipleSearchRequestsTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Search
  include Isomorfeus::Ferret::Store
  include Isomorfeus::Ferret::Analysis
  include Isomorfeus::Ferret::Index

  def setup
    dpath = File.expand_path(File.join(File.dirname(__FILE__), '../../temp/multisearch'))
    fs_dir = Isomorfeus::Ferret::Store::FSDirectory.new(dpath, true)

    iw = IndexWriter.new(:dir => fs_dir, :create => true, :key => [:id])
    1000.times do |x|
      doc = {:id => x}
      iw << doc
    end
    iw.close
    fs_dir.close

    @ix = Index.new(:path => dpath, :create => true, :key => [:id])
  end

  def tear_down
    @ix.close
  end

  def test_repeated_queries_segmentation_fault
    1000.times do |x|
      bq = BooleanQuery.new
      tq1 = TermQuery.new(:id, 1)
      tq2 = TermQuery.new(:another_id, 1)
      bq.add_query(tq1, :must)
      bq.add_query(tq2, :must)
      @ix.search(bq)
    end
  end

  def test_repeated_queries_bus_error
    1000.times do |x|
      bq = BooleanQuery.new
      tq1 = TermQuery.new(:id, '1')
      tq2 = TermQuery.new(:another_id, '1')
      tq3 = TermQuery.new(:yet_another_id, '1')
      tq4 = TermQuery.new(:still_another_id, '1')
      tq5 = TermQuery.new(:one_more_id, '1')
      tq6 = TermQuery.new(:and_another_id, '1')
      bq.add_query(tq1, :must)
      bq.add_query(tq2, :must)
      bq.add_query(tq3, :must)
      bq.add_query(tq4, :must)
      bq.add_query(tq5, :must)
      bq.add_query(tq6, :must)
      @ix.search(bq)
    end
  end
end
