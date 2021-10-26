require 'isomorfeus-ferret'
include Isomorfeus::Ferret

index = Index::Index.new(:path => File.expand_path('ferret_index'))

search_terms = [ 'english', 'gutenberg', 'yesterday', 'together', 'america', 'advanced', 'president', 'bunny' ]

1000.times do
  search_terms.each do |st|
    index.search_each(st) do |doc, score|
      puts "Found document: #{doc} with score: #{score}"
    end
  end
end
