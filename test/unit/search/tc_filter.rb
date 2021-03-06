require File.expand_path(File.join(File.dirname(__FILE__), "..", "..", "test_helper.rb"))
require 'date'

class FilterTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Search
  include Isomorfeus::Ferret::Analysis
  include Isomorfeus::Ferret::Index

  def setup
    @dir = Isomorfeus::Ferret::Store::RAMDirectory.new
    iw = IndexWriter.new(:dir => @dir,
                         :analyzer => WhiteSpaceAnalyzer.new,
                         :create => true)
    [
      {:int => "0", :date => "20040601", :switch => "on"},
      {:int => "1", :date => "20041001", :switch => "off"},
      {:int => "2", :date => "20051101", :switch => "on"},
      {:int => "3", :date => "20041201", :switch => "off"},
      {:int => "4", :date => "20051101", :switch => "on"},
      {:int => "5", :date => "20041201", :switch => "off"},
      {:int => "6", :date => "20050101", :switch => "on"},
      {:int => "7", :date => "20040701", :switch => "off"},
      {:int => "8", :date => "20050301", :switch => "on"},
      {:int => "9", :date => "20050401", :switch => "off"}
    ].each {|doc| iw << doc}
    iw.close
  end

  def teardown
    @dir.close
  end

  def do_test_top_docs(searcher, query, expected, filter)
    top_docs = searcher.search(query, {:filter => filter})
    #puts top_docs
    assert_equal(expected.size, top_docs.hits.size)
    top_docs.total_hits.times do |i|
      assert_equal(expected[i], top_docs.hits[i].doc)
    end
  end

  def test_range_filter
    searcher = Searcher.new(@dir)
    q = MatchAllQuery.new
    rf = RangeFilter.new(:int, :>= => "2", :<= => "6")
    do_test_top_docs(searcher, q, [2,3,4,5,6], rf)
    rf = RangeFilter.new(:int, :>= => "2", :< => "6")
    do_test_top_docs(searcher, q, [2,3,4,5], rf)
    rf = RangeFilter.new(:int, :> => "2", :<= => "6")
    do_test_top_docs(searcher, q, [3,4,5,6], rf)
    rf = RangeFilter.new(:int, :> => "2", :< => "6")
    do_test_top_docs(searcher, q, [3,4,5], rf)
    rf = RangeFilter.new(:int, :>= => "6")
    do_test_top_docs(searcher, q, [6,7,8,9], rf)
    rf = RangeFilter.new(:int, :> => "6")
    do_test_top_docs(searcher, q, [7,8,9], rf)
    rf = RangeFilter.new(:int, :<= => "2")
    do_test_top_docs(searcher, q, [0,1,2], rf)
    rf = RangeFilter.new(:int, :< => "2")
    do_test_top_docs(searcher, q, [0,1], rf)

    bits = rf.bits(searcher.reader)
    assert(bits[0])
    assert(bits[1])
    assert(!bits[2])
    assert(!bits[3])
    assert(!bits[4])
  end

  def test_range_filter_errors
    assert_raise(ArgumentError) {RangeFilter.new(:f, :> => "b", :< => "a")}
    assert_raise(ArgumentError) {RangeFilter.new(:f, :include_lower => true)}
    assert_raise(ArgumentError) {RangeFilter.new(:f, :include_upper => true)}
  end

  def test_query_filter
    searcher = Searcher.new(@dir)
    q = MatchAllQuery.new
    qf = QueryFilter.new(TermQuery.new(:switch, "on"))
    do_test_top_docs(searcher, q, [0,2,4,6,8], qf)
    # test again to test caching doesn't break it
    do_test_top_docs(searcher, q, [0,2,4,6,8], qf)
    qf = QueryFilter.new(TermQuery.new(:switch, "off"))
    do_test_top_docs(searcher, q, [1,3,5,7,9], qf)

    bits = qf.bits(searcher.reader)
    assert(bits[1])
    assert(bits[3])
    assert(bits[5])
    assert(bits[7])
    assert(bits[9])
    assert(!bits[0])
    assert(!bits[2])
    assert(!bits[4])
    assert(!bits[6])
    assert(!bits[8])
  end

  def test_filtered_query
    searcher = Searcher.new(@dir)
    q = MatchAllQuery.new
    rf = RangeFilter.new(:int, :>= => "2", :<= => "6")
    rq = FilteredQuery.new(q, rf)
    qf = QueryFilter.new(TermQuery.new(:switch, "on"))
    do_test_top_docs(searcher, rq, [2,4,6], qf)
    query = FilteredQuery.new(rq, qf)
    rf2 = RangeFilter.new(:int, :>= => "3")
    do_test_top_docs(searcher, query, [4,6], rf2)
  end

  class CustomFilter
    def bits(ir)
      bv = Isomorfeus::Ferret::Utils::BitVector.new
      bv[0] = bv[2] = bv[4] = true
      bv
    end
  end

  def test_custom_filter
    searcher = Searcher.new(@dir)
    q = MatchAllQuery.new
    filt = CustomFilter.new
    do_test_top_docs(searcher, q, [0, 2, 4], filt)
  end

  def test_filter_proc
    searcher = Searcher.new(@dir)
    q = MatchAllQuery.new
    filter_proc = lambda {|doc, score, s| (s[doc][:int].to_i % 2) == 0}
    top_docs = searcher.search(q, :filter_proc => filter_proc)
    top_docs.hits.each do |hit|
      assert_equal(0, searcher[hit.doc][:int].to_i % 2)
    end
  end

  def test_score_modifying_filter_proc
    searcher = Searcher.new(@dir)
    q = MatchAllQuery.new
    start_date = Date.parse('2008-02-08')
    date_half_life_50 = lambda do |doc, score, s|
      days = (start_date - Date.parse(s[doc][:date], '%Y%m%d')).to_i
      1.0 / (2.0 ** (days.to_f / 50.0))
    end
    top_docs = searcher.search(q, :filter_proc => date_half_life_50)
    docs = top_docs.hits.collect {|hit| hit.doc}
    assert_equal(docs, [2,4,9,8,6,3,5,1,7,0])
    rev_date_half_life_50 = lambda do |doc, score, s|
      days = (start_date - Date.parse(s[doc][:date], '%Y%m%d')).to_i
      1.0 - 1.0 / (2.0 ** (days.to_f / 50.0))
    end
    top_docs = searcher.search(q, :filter_proc => rev_date_half_life_50)
    docs = top_docs.hits.collect {|hit| hit.doc}
    assert_equal(docs, [0,7,1,3,5,6,8,9,2,4])
  end
end
