require 'isomorfeus-ferret'
include Isomorfeus::Ferret

index = Index::Index.new(:path => File.expand_path('ferret_index'))

start_time = Time.now
docs = 0
index.each do |doc|
  docs += 1
  doc
end
final_time = Time.now - start_time
puts "Loading took: #{final_time}s for #{docs} docs\nthats #{docs / final_time} docs/s"

