require 'bundler'
require 'bundler/cli'
require 'bundler/cli/exec'
require 'fileutils'
require_relative 'lib/isomorfeus/ferret/version'

require 'bundler/setup'
require 'rake/extensiontask'
require 'rake/testtask'

Rake::ExtensionTask.new :isomorfeus_ferret_ext

task :ferret_vs_lucene do
  Rake::Task[:ferret_bench].invoke
  Rake::Task[:lucene_bench].invoke
end

task :lucene_bench do
  puts "\n\n\tLucene:\n\n"
  pwd = Dir.pwd
  Dir.chdir('misc/ferret_vs_lucene')
  puts "title stored:"
  FileUtils.rm_rf('lucene_index')
  FileUtils.rm_f('LuceneIndexer.class')
  FileUtils.rm_f('LuceneSearch.class')
  sep = Gem.win_platform? ? ';' : ':'
  lucene_version = '9.1.0'
  system("javac -classpath lucene-analysis-common-#{lucene_version}.jar#{sep}lucene-core-#{lucene_version}.jar#{sep}. LuceneIndexer.java")
  system("javac -classpath lucene-analysis-common-#{lucene_version}.jar#{sep}lucene-core-#{lucene_version}.jar#{sep}lucene-queryparser-#{lucene_version}.jar#{sep}. LuceneSearch.java")
  system("java -classpath lucene-analysis-common-#{lucene_version}.jar#{sep}lucene-core-#{lucene_version}.jar#{sep}. LuceneIndexer -reps 6")
  system("java -classpath lucene-analysis-common-#{lucene_version}.jar#{sep}lucene-core-#{lucene_version}.jar#{sep}lucene-queryparser-#{lucene_version}.jar#{sep}. LuceneSearch")
  puts "index size: #{Dir['lucene_index/*'].select { |f| File.file?(f) }.sum { |f| File.size(f) } / 1_048_576}Mb"
  Dir.chdir(pwd)
end

task :ferret_bench => :compile do
  puts "\n\n\tFerret:\n\n"
  pwd = Dir.pwd
  Dir.chdir('misc/ferret_vs_lucene')

  FileUtils.rm_rf('ferret_index')
  system('bundle exec ruby ferret_indexer.rb -r 6 -a l')
  system('bundle exec ruby ferret_search.rb')
  puts "Index size: #{Dir['ferret_index/*'].select { |f| File.file?(f) }.sum { |f| File.size(f) } / 1_048_576}Mb"

  puts
  FileUtils.rm_rf('ferret_index')
  system('bundle exec ruby ferret_indexer.rb -r 6 -a s')
  system('bundle exec ruby ferret_search.rb')
  puts "Index size: #{Dir['ferret_index/*'].select { |f| File.file?(f) }.sum { |f| File.size(f) } / 1_048_576}Mb"

  puts
  FileUtils.rm_rf('ferret_index')
  system('bundle exec ruby ferret_indexer.rb -r 6 -a w')
  system('bundle exec ruby ferret_search.rb')
  puts "Index size: #{Dir['ferret_index/*'].select { |f| File.file?(f) }.sum { |f| File.size(f) } / 1_048_576}Mb"

  Dir.chdir(pwd)
end

task :ferret_compression_bench => :compile do
  puts "\n\n\tFerret:\n\n"
  pwd = Dir.pwd
  Dir.chdir('misc/ferret_vs_lucene')

  puts "\ntitle and content stored:"
  FileUtils.rm_rf('ferret_index')
  system('bundle exec ruby ferret_indexer.rb -r 2 --store')
  system('bundle exec ruby ferret_search.rb')
  puts "Stored content index size: #{Dir['ferret_index/*'].select { |f| File.file?(f) }.sum { |f| File.size(f) } / 1_048_576}Mb"

  puts "\ntitle and content stored and compressed with brotli:"
  FileUtils.rm_rf('ferret_index')
  system('bundle exec ruby ferret_indexer.rb -r 2 -c b --store')
  system('bundle exec ruby ferret_search.rb')
  puts "Compressed stored content index size: #{Dir['ferret_index/*'].select { |f| File.file?(f) }.sum { |f| File.size(f) } / 1_048_576}Mb"

  puts "\ntitle and content stored and compressed with bzip:"
  FileUtils.rm_rf('ferret_index')
  system('bundle exec ruby ferret_indexer.rb -r 2 -c z --store')
  system('bundle exec ruby ferret_search.rb')
  puts "Compressed stored content index size: #{Dir['ferret_index/*'].select { |f| File.file?(f) }.sum { |f| File.size(f) } / 1_048_576}Mb"

  puts "\ntitle and content stored and compressed with lz4:"
  FileUtils.rm_rf('ferret_index')
  system('bundle exec ruby ferret_indexer.rb -r 2 -c l --store')
  system('bundle exec ruby ferret_search.rb')
  puts "Compressed stored content index size: #{Dir['ferret_index/*'].select { |f| File.file?(f) }.sum { |f| File.size(f) } / 1_048_576}Mb"

  Dir.chdir(pwd)
end

task :parser do
  pwd = Dir.pwd
  Dir.chdir('parser')
  system("bison -o frt_q_parser.c frt_q_parser.y")
  Dir.chdir(pwd)
  FileUtils.cp('parser/frt_q_parser.c', 'ext/isomorfeus_ferret_ext/frt_q_parser.c', preserve: false, verbose: true)
end

task :specs do
  Rake::Task['units'].invoke
  Rake::Task['thread_safety'].invoke
end

task :push_packages do
  Rake::Task['push_packages_to_rubygems'].invoke
  Rake::Task['push_packages_to_github'].invoke
end

task :push_packages_to_rubygems do
  system("gem push isomorfeus-ferret-#{Isomorfeus::Ferret::VERSION}.gem")
end

task :push_packages_to_github do
  system("gem push --key github --host https://rubygems.pkg.github.com/isomorfeus isomorfeus-ferret-#{Isomorfeus::Ferret::VERSION}.gem")
end

task :push do
  system("git push github")
  system("git push gitlab")
  system("git push bitbucket")
  system("git push gitprep")
end

task :thread_safety do
  system('bundle exec ruby test/threading/thread_safety_index_test.rb')
  system('bundle exec ruby test/threading/thread_safety_read_write_test.rb')
  system('bundle exec ruby test/threading/thread_safety_test.rb')
end

Rake::TestTask.new("units" => :compile) do |t|
  t.libs << "test/unit"
  t.pattern = 'test/unit/t[csz]_*.rb'
  t.verbose = true
end
task :unit => :units

task :default => [:compile, :units]