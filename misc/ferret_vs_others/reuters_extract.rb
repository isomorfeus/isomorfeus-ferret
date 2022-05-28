#!/usr/bin/env ruby
#
# Extract the corpus to a number of text files for indexing by Lucene and
# Ferret benchmarks.
#
require 'fileutils'

source_dir = ARGV[0]
unless source_dir and File.directory?(source_dir)
  raise "Usage: ./extract_reuters.rb /path/to/expanded/archive/dir"
end

main_out_dir = 'corpus'
FileUtils.mkdir_p(main_out_dir) unless File.directory?(main_out_dir)

num_files = 0
# get a list of the sgm files
Dir["#{source_dir}/**/*.sgm"].each do |file_name|
  puts "Processing :" + file_name
  path = File.join(main_out_dir, File.basename(file_name))
  FileUtils.mkdir_p(File.join(main_out_dir, File.basename(file_name)))
  in_body = in_title = false
  body = nil
  title = nil
  File.readlines(file_name, mode: 'rb').each do |line|
    case line
    when /<REUTERS/
      title = nil
      body  = []
    when %r{<TITLE>([^<]*)</TITLE>}
      title = $1
    when %r{<TITLE>([^<]*)}
      in_title = true
      title = $1
    when %r{([^<]*)</TITLE>}
      in_title = false
      title << $1
    when /<BODY>(.*)/m
      body << $1
      in_body = true
    when %r{(.*)</BODY>}
      in_body = false
      body << $1
      File.open(File.join(path, "article%05d.txt" % num_files), "w") do |f|
        f.puts title
        f.puts ""
        f.puts body.join('')
      end
      num_files += 1
    else
      body << line if in_body
      title << line if in_title
    end
  end
end

puts "Total articles extracted: #{num_files}"