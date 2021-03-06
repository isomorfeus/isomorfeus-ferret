#!/usr/bin/env ruby
require 'rubygems'
require 'isomorfeus-ferret'

unless File.directory?(ARGV[0])
  puts "Usage: print_diagnostics <index-directory>"
  exit
end

reader = Ismorfeus::Ferret::Index::IndexReader.new(Isomorfeus::Ferret::Store::FSDirectory.new(ARGV[0]))
puts ""
puts "Document count = #{reader.max_doc}"
puts ""
reader.fields.each do |field|
  count = 0
  reader.terms(field).each {|t| count += 1}
  puts "#{field}: #{count}"
end
puts ""
puts reader.field_infos
puts ""
puts "Index directory listing"
puts "      size   file name"
Dir[File.join(ARGV[0], '*')].each do |f|
  puts "%10d   %s" % [File.stat(f).size, f]
end
