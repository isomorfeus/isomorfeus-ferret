require 'optparse'
require 'rubygems'
require 'isomorfeus-ferret'

FL = Dir["corpus/**/*.txt"]

include Isomorfeus::Ferret
include Isomorfeus::Ferret::Index

def init_writer(create)
  options = {
    :path => "ferret_index",
    :analyzer => Analysis::WhiteSpaceAnalyzer.new(),
    :merge_factor => 100,
    :use_compound_file => true,
    :max_buffer_memory => 0x10000000,
    :max_buffered_docs => 20_000
  }
  if create
    options[:create] = true
    field_infos = FieldInfos.new()
    field_infos.add_field(:body, :store => :no, :term_vector => :no) unless @store
    options[:field_infos] = field_infos
  end

  IndexWriter.new(options)
end

def build_index(file_list, max_to_index, increment, store)
  writer = init_writer(true)
  docs_so_far = 0

  file_list.each do |fn|
    File.open(fn) do |f|
      raise("Failed to read title") if (title = f.readline).nil?
      writer << {:title => title, :body => f.readlines}
    end

    docs_so_far += 1

    break if (docs_so_far >= max_to_index)

    if (docs_so_far % increment == 0)
      writer.close()
      writer = init_writer(false)
    end
  end

  # finish index
  num_indexed = writer.doc_count()
  writer.optimize()
  writer.close()

  return num_indexed
end

@docs = FL.size
@reps = 1
@inc = 0
@store = false
opts = OptionParser.new do |opts|
  opts.banner = "Usage: f.rb [options]"

  opts.separator ""
  opts.separator "Specific options:"


  opts.on("-d", "--docs VAL", Integer) {|v| @docs = v}
  opts.on("-r", "--reps VAL", Integer) {|v| @reps = v}
  opts.on("-i", "--inc VAL", Integer) {|v| @reps = v}
  opts.on("-s", "--store") {|v| @store = true}

end

opts.parse(ARGV)
@inc = @inc == 0 ? @docs + 1 : @inc

puts "-" * 60
times = []
@reps.times do |i|
  t = Time.now
  num_indexed = build_index(FL, @docs, @inc, @store)
  t = Time.new - t
  times << t
  puts "#{i}  Secs: %.2f  Docs: #{num_indexed}" % t
end
times.sort!
num_to_chop = @reps >> 2
num_kept = 0
mean_time = 0.0
trunc_mean_time = 0.0
@reps.times do |i|
  mean_time += times[i]
  next if (i < num_to_chop) || (i >= (@reps - num_to_chop))
  trunc_mean_time += times[i]
  num_kept += 1
end

mean_time /= @reps
trunc_mean_time /= num_kept
puts "-" * 60
puts "Mean %.2f secs" % mean_time
puts "Truncated Mean (#{num_kept} kept, #{@reps - num_kept} discarded): " +
     "%.2f secs" % trunc_mean_time
puts "-" * 60
