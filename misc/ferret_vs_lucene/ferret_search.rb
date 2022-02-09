require 'isomorfeus-ferret'
include Isomorfeus::Ferret

index = Index::Index.new(:path => File.expand_path('ferret_index'))

search_terms = [ 'english', 'gutenberg', 'yesterday', 'together', 'america', 'advanced', 'president', 'bunny' ]

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
final_time = Time.now - start_time
puts "Searching took: #{final_time}s for #{queries} queries\nthats #{queries / final_time} q/s"
puts "Total found: #{res}"
