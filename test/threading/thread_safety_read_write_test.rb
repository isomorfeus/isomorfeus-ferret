require File.expand_path(File.join(File.dirname(__FILE__), "..", "test_helper.rb"))
require File.expand_path(File.join(File.dirname(__FILE__), "number_to_spoken.rb"))
require 'thread'

class IndexThreadSafetyReadWriteTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Index

  INDEX_DIR = File.expand_path(File.join(File.dirname(__FILE__), ".." , "temp", "threading"))
  ITERATIONS = 1000
  NUM_THREADS = 3
  ANALYZER = Isomorfeus::Ferret::Analysis::Analyzer.new

  def setup
    @index = Index.new(:path => INDEX_DIR, :create => true, :analyzer => ANALYZER, :default_field => :content)
    @verbose = false
  end

  def search_thread
    ITERATIONS.times do
      do_search
      sleep(rand(1))
    end
  rescue => e
    puts e
    puts e.backtrace
    @index = nil
    raise e
  end

  def index_thread
    ITERATIONS.times do
      do_add_doc
      sleep(rand(1))
    end
  rescue => e
    puts e
    puts e.backtrace
    @index = nil
    raise e
  end

  def do_add_doc
    n = rand(0xFFFFFFFF)
    d = {:id => n.to_s, :content => n.to_spoken}
    puts("Adding #{n}") if @verbose
    begin
      @index << d
    rescue => e
      puts e
      puts e.backtrace
      @index = nil
      raise e
    end
  end

  def do_search
    n = rand(0xFFFFFFFF)
    puts("Searching for #{n}") if @verbose
    hits = @index.search_each(n.to_spoken, :num_docs => 3) do |d, s|
      puts "Hit for #{n}: #{@index[d]["id"]} - #{s}" if @verbose
    end
    puts("Searched for #{n}: total = #{hits}") if @verbose
  end

  def test_threading
    threads = []
    NUM_THREADS.times do
      threads << Thread.new { search_thread }
    end
    NUM_THREADS.times do
      threads << Thread.new { index_thread }
    end
    threads.each { |t| t.join }
  end
end
