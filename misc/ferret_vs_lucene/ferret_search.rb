require 'optparse'
require 'isomorfeus-ferret'

include Isomorfeus::Ferret

index = Index::Index.new(:path => File.expand_path('ferret_index'))

@reps = 1

opts = OptionParser.new do |opts|
  opts.banner = "Usage: ferret_reader.rb [options]"

  opts.separator ""
  opts.separator "Specific options:"

  opts.on("-r", "--reps VAL", Integer) {|v| @reps = v}
end

opts.parse(ARGV)
queries = 0
res = 0
puts "Ferret Search"
puts "-" * 63
times = []

search_terms = [ 'english', 'gutenberg', 'yesterday', 'together', 'america', 'advanced', 'president', 'bunny' ]

@reps.times do |i|
  start_time = Time.now
  queries = 0
  res = 0
  1000.times do
    search_terms.each do |st|
      queries += 1
      index.search_each("body:\"#{st}\"", limit: 10) do |doc, score|
        res += 1
      end
    end
  end
  t = Time.now - start_time
  times << t
  puts "#{i+1}  Secs: %.2f  Queries: #{queries}, #{(queries/t).to_i} queries/s" % t
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
puts "-" * 63
puts "Mean %.2f secs" % mean_time
puts "Truncated Mean (#{num_kept} kept, #{@reps - num_kept} discarded): " +
     "%.2f secs, #{(queries/trunc_mean_time).to_i} queries/s" % trunc_mean_time
puts "#{res} docs found"
puts "-" * 63
