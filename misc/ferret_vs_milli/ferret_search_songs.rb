require 'optparse'
require 'isomorfeus-ferret'

include Isomorfeus::Ferret

index = Index::Index.new(:path => File.expand_path('ferret_songs_index'))

@reps = 1

opts = OptionParser.new do |opts|
  opts.banner = "Usage: ferret_search.rb [options]"

  opts.separator ""
  opts.separator "Specific options:"

  opts.on("-r", "--reps VAL", Integer) {|v| @reps = v}
end

opts.parse(ARGV)
executed_queries = 0
res = 0
puts "Ferret Search songs"
puts "-" * 63
times = []

queries = {
  base: {
    terms: [
        "john ",             # 9097
        "david ",            # 4794
        "charles ",          # 1957
        "david bowie ",      # 1200
        "michael jackson ",  # 600
        "thelonious monk ",  # 303
        "charles mingus ",   # 142
        "marcus miller ",    # 60
        "tamo ",             # 13
        "Notstandskomitee "  # 4
    ],
    optional_words: true,
    filter: nil,
    sort_fields: nil,
    sort_direction: nil
  },
  proximity: {
    terms: [
      "black saint sinner lady ",
      "les dangeureuses 1960 ",
      "The Disneyland Sing-Along Chorus ",
      "Under Great Northern Lights ",
      "7000 Danses Un Jour Dans Notre Vie "
    ],
    optional_words: false
  },
  typo: {
    terms: [
      "mongus ",
      "thelonius monk ",
      "Disnaylande ",
      "the white striper ",
      "indochie ",
      "indochien ",
      "klub des loopers ",
      "fear of the duck ",
      "michel depech ",
      "stromal ",
      "dire straights ",
      "Arethla Franklin "
    ],
    optional_words: false
  },
  words: {
    terms: [
      "the black saint and the sinner lady and the good doggo ", # four words to pop
      "les liaisons dangeureuses 1793 ",                         # one word to pop
      "The Disneyland Children's Sing-Alone song ",              # two words to pop
      "seven nation mummy ",                                     # one word to pop
      "7000 Danses / Le Baiser / je me trompe de mots ",         # four words to pop
      "Bring Your Daughter To The Slaughter but now this is not part of the title ", # nine words to pop
      "whathavenotnsuchforth and a good amount of words to pop to match the first one " # 13
    ]
  },
  asc: {
    sort_fields: %i[released_timestamp],
    sort_direction: :asc
  },
  desc: {
    sort_fields: %i[released_timestamp],
    sort_direction: :desc
  },
  filter_le: {
      filter: "released-timestamp <= 946728000" # year 2000
  },
  filter_range: {
      filter: "released-timestamp 946728000 TO 1262347200", # year 2000 to 2010
  },
  big_filter: {
      filter: "released-timestamp != 1262347200 AND (NOT (released-timestamp = 946728000)) AND (duration-float = 1 OR (duration-float 1.1 TO 1.5 AND released-timestamp > 315576000))",
  },
  prefix: {
      terms: [
          "s", # 500k+ results
          "a", #
          "b", #
          "i", #
          "x", # only 7k results
      ],
  }
}

@reps.times do |i|
  start_time = Time.now
  executed_queries = 0
  res = 0
  queries.each do |name, query|
    query = queries[:base].merge(query)
    1000.times do
      query[:terms].each do |st|
        executed_queries += 1
        index.search_each("#{st}", limit: 10) do |doc, score|
          res += 1
        end
      end
    end
  end
  t = Time.now - start_time
  times << t
  puts "#{i+1}  Secs: %.2f  Queries: #{executed_queries}, #{(executed_queries/t).to_i} queries/s" % t
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
     "%.2f secs, #{(executed_queries/trunc_mean_time).to_i} queries/s" % trunc_mean_time
puts "#{res} docs found"
puts "-" * 63
