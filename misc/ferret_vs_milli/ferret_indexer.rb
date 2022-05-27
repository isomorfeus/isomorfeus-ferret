require 'optparse'
require 'oj'
require 'isomorfeus-ferret'
require 'net/http'
require 'zlib'

include Isomorfeus::Ferret
include Isomorfeus::Ferret::Index

BASE_URL = "https://milli-benchmarks.fra1.digitaloceanspaces.com/datasets/"
DATASETS = ["smol-songs.csv.gz", "smol-wiki-articles.csv.gz", "movies.json"]

DATASETS.each do |set|
  s = set.end_with?(".gz") ? set[0..-4] : set
  if !File.exist?(s) && !File.exist?(set)
    puts "downloading #{set}"
    uri = URI.parse(BASE_URL + set)
    Net::HTTP.start(uri.host, uri.port, use_ssl: true) do |http|
      resp = http.get(uri.request_uri)
      open(set, "wb") { |file| file.write(resp.body) }
    end
  end
  if !File.exist?(s) && set.end_with?(".gz")
    puts "inflating #{set}"
    Zlib::GzipReader.open(set) do |gz|
      File.write(s, gz.read)
    end
  end
end

def movies_field_infos(field_infos, c)
  # stored
  # displayed_fields = ["title", "poster", "overview", "release_date", "genres"]
  # stored + indexed
  # searchable_fields = ["title", "overview"]
  # we just index those too
  # faceted_fields = ["released_date", "genres"]

  %i[poster].each do |field|
    field_infos.add_field(field, store: :yes, compression: c, index: :no)
  end
  %i[title overview release_date genres].each do |field|
    field_infos.add_field(field, store: :yes, compression: c, index: :yes, term_vector: :no)
  end
end

def songs_field_infos(field_infos, c)
  # stored
  # displayed_fields = ["title", "album", "artist", "genre", "country", "released", "duration"]
  # stored + indexed
  # searchable_fields = ["title", "album", "artist"]
  # we just index those too
  # faceted_fields = ["released-timestamp", "duration-float", "genre", "country", "artist"]

  %i[genre country released duration].each do |field|
    field_infos.add_field(field, store: :yes, compression: c, index: :no)
  end
  %i[title album artist].each do |field|
    field_infos.add_field(field, store: :yes, compression: c, index: :yes, term_vector: :no)
  end
  %i[released_timestamp duration_float].each do |field|
    field_infos.add_field(field, store: :no, index: :yes, term_vector: :no)
  end
end

def wiki_field_infos(field_infos, c)
  # stored
  # displayed_fields = ["title", "body", "url"]
  # stored + indexed
  # searchable_fields = ["title", "body"]

  %i[url].each do |field|
    field_infos.add_field(field, store: :yes, compression: c, index: :no)
  end
  %i[title body].each do |field|
    field_infos.add_field(field, store: :yes, compression: c, index: :yes, term_vector: :yes)
  end
end

def init_writer(create)
  a = case @analyzer
      when 'l' then Analysis::LetterAnalyzer.new()
      when 's' then Analysis::StandardAnalyzer.new()
      when 'w' then Analysis::WhiteSpaceAnalyzer.new()
      else
        Analysis::WhiteSpaceAnalyzer.new()
      end
  print "#{a.class.name.split('::').last}\t"
  options = {
    :path => "ferret_#{@data}_index",
    :analyzer => a,
    :merge_factor => 100,
    :use_compound_file => true,
    :max_buffer_memory => 0x10000000,
    :max_buffered_docs => 20_000
  }
  if create
    ca = :brotli
    ca = :lz4 if @comp == 'l'
    ca = :bz2 if @comp == 'z'
    c = @comp ? ca : :no
    options[:create] = true
    field_infos = FieldInfos.new()
    case @data
    when 'movies' then movies_field_infos(field_infos, c)
    when 'songs' then songs_field_infos(field_infos, c)
    when 'wiki' then wiki_field_infos(field_infos, c)
    else
      movies_field_infos(field_infos)
    end
    options[:field_infos] = field_infos
  end

  IndexWriter.new(options)
end

def build_index_movies
  writer = init_writer(true)
  movies = Oj.load(File.read("movies.json"), symbol_keys: true)
  docs_so_far = 0

  movies.each do |movie|
    movie[:key] = movie.delete(:id)
    writer << movie
    docs_so_far += 1
  end

  # finish index
  num_indexed = writer.doc_count()
  writer.optimize()
  writer.close()

  num_indexed
end

def build_index_songs
  writer = init_writer(true)
  docs_so_far = 0

  File.open("smol-songs.csv") do |f|
    f.each_line do |line|
      unless line.start_with?('id,')
        a = line.split(',')
        # id,title,album,artist,genre,country,released,duration,released-timestamp,duration-float
        writer << { key: a[0], title: a[1], album: a[2], artist: a[3], genre: a[4], country: a[5],
                    released: a[6], duration: a[7], released_timestamp: a[8], duration_float: a[9] }
        docs_so_far += 1
      end
    end
  end

  # finish index
  num_indexed = writer.doc_count()
  writer.optimize()
  writer.close()

  num_indexed
end

def build_index_wiki
  writer = init_writer(true)
  docs_so_far = 0

  File.open("smol-wiki-articles.csv") do |f|
    f.each_line do |line|
      unless line.start_with?('body,')
        a = line.split(',')
        # body,title,url
        writer << { body: a[0], title: a[1], url: a[2] }
        docs_so_far += 1
      end
    end
  end

  # finish index
  num_indexed = writer.doc_count()
  writer.optimize()
  writer.close()

  num_indexed
end

@data = 'movies'
@reps = 1
@comp = false
@analyzer = 'w'

opts = OptionParser.new do |opts|
  opts.banner = "Usage: ferret_indexer.rb [options]"

  opts.separator ""
  opts.separator "Specific options:"

  opts.on("-r", "--reps VAL", Integer) {|v| @reps = v}
  opts.on("-c", "--comp VAL") {|v| @comp = v }
  opts.on("-a", "--analyzer VAL", String) {|v| @analyzer = v.downcase[0] }
  opts.on("-d", "--data VAL") {|v| @data = v }
end

opts.parse(ARGV)

num_indexed = 0

puts "Ferret Indexer #{@data}"
puts "-" * 63
times = []

@reps.times do |i|
  t = Time.now
  num_indexed = case @data
                when 'movies' then build_index_movies
                when 'songs' then build_index_songs
                when 'wiki' then build_index_wiki
                else
                  build_index_movies
                end
  t = Time.new - t
  times << t
  puts "#{i+1}  Secs: %.2f  Docs: #{num_indexed}, #{(num_indexed/t).to_i} docs/s" % t
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
     "%.2f secs, #{(num_indexed/trunc_mean_time).to_i} docs/s" % trunc_mean_time
puts "-" * 63
