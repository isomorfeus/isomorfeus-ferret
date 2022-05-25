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
docs = 0
puts "Ferret Lazy Reader, just the doc"
puts "-" * 63
times = []

@reps.times do |i|
  start_time = Time.now
  docs = 0
  10.times do
    index.each do |doc|
      docs += 1
      doc
    end
  end
  t = Time.now - start_time
  times << t
  puts "#{i+1}  Secs: %.2f  Docs: #{docs}, #{(docs/t).to_i} docs/s" % t
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
     "%.2f secs, #{(docs/trunc_mean_time).to_i} docs/s" % trunc_mean_time
puts "-" * 63


docs = 0
puts "Ferret Lazy Reader, doc[:title]"
puts "-" * 63
times = []

@reps.times do |i|
  start_time = Time.now
  docs = 0
  10.times do
    index.each do |doc|
      docs += 1
      doc[:title]
    end
  end
  t = Time.now - start_time
  times << t
  puts "#{i+1}  Secs: %.2f  Docs: #{docs}, #{(docs/t).to_i} docs/s" % t
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
     "%.2f secs, #{(docs/trunc_mean_time).to_i} docs/s" % trunc_mean_time
puts "-" * 63

docs = 0
puts "Ferret Lazy Reader, doc[:body]"
puts "-" * 63
times = []

@reps.times do |i|
  start_time = Time.now
  docs = 0
  10.times do
    index.each do |doc|
      docs += 1
      doc[:body]
    end
  end
  t = Time.now - start_time
  times << t
  puts "#{i+1}  Secs: %.2f  Docs: #{docs}, #{(docs/t).to_i} docs/s" % t
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
     "%.2f secs, #{(docs/trunc_mean_time).to_i} docs/s" % trunc_mean_time
puts "-" * 63

docs = 0
puts "Ferret Lazy Reader, doc[:title] and doc[:body]"
puts "-" * 63
times = []

@reps.times do |i|
  start_time = Time.now
  docs = 0
  10.times do
    index.each do |doc|
      docs += 1
      doc[:title]
      doc[:body]
    end
  end
  t = Time.now - start_time
  times << t
  puts "#{i+1}  Secs: %.2f  Docs: #{docs}, #{(docs/t).to_i} docs/s" % t
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
     "%.2f secs, #{(docs/trunc_mean_time).to_i} docs/s" % trunc_mean_time
puts "-" * 63

docs = 0
puts "Ferret Lazy Reader, doc[:body] and doc[:title]"
puts "-" * 63
times = []

@reps.times do |i|
  start_time = Time.now
  docs = 0
  10.times do
    index.each do |doc|
      docs += 1
      doc[:body]
      doc[:title]
    end
  end
  t = Time.now - start_time
  times << t
  puts "#{i+1}  Secs: %.2f  Docs: #{docs}, #{(docs/t).to_i} docs/s" % t
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
     "%.2f secs, #{(docs/trunc_mean_time).to_i} docs/s" % trunc_mean_time
puts "-" * 63

docs = 0
puts "Ferret Lazy Reader,doc.load"
puts "-" * 63
times = []

@reps.times do |i|
  start_time = Time.now
  docs = 0
  10.times do
    index.each do |doc|
      docs += 1
      doc.load
    end
  end
  t = Time.now - start_time
  times << t
  puts "#{i+1}  Secs: %.2f  Docs: #{docs}, #{(docs/t).to_i} docs/s" % t
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
     "%.2f secs, #{(docs/trunc_mean_time).to_i} docs/s" % trunc_mean_time
puts "-" * 63