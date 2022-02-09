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
  Rake::Task[:run_ferret_bench].invoke
  Rake::Task[:run_lucene_bench].invoke
end

task :run_lucene_bench do
  puts "\n\n\tLucene:\n\n"
  pwd = Dir.pwd
  Dir.chdir('misc/ferret_vs_lucene')
  FileUtils.rm_rf('lucene_index')
  FileUtils.rm_f('LuceneIndexer.class')
  FileUtils.rm_f('LuceneSearch.class')
  sep = Gem.win_platform? ? ';' : ':'
  lucene_version = '9.0.0'
  system("javac -classpath lucene-analysis-common-#{lucene_version}.jar#{sep}lucene-core-#{lucene_version}.jar#{sep}. LuceneIndexer.java")
  system("javac -classpath lucene-analysis-common-#{lucene_version}.jar#{sep}lucene-core-#{lucene_version}.jar#{sep}lucene-queryparser-#{lucene_version}.jar#{sep}. LuceneSearch.java")
  system("java -classpath lucene-analysis-common-#{lucene_version}.jar#{sep}lucene-core-#{lucene_version}.jar#{sep}. LuceneIndexer -reps 3")
  system("java -classpath lucene-analysis-common-#{lucene_version}.jar#{sep}lucene-core-#{lucene_version}.jar#{sep}lucene-queryparser-#{lucene_version}.jar#{sep}. LuceneSearch")
  Dir.chdir(pwd)
end

task :run_ferret_bench => :compile do
  puts "\n\n\tFerret:\n\n"
  pwd = Dir.pwd
  Dir.chdir('misc/ferret_vs_lucene')
  FileUtils.rm_rf('ferret_index')
  system('bundle exec ruby ferret_indexer.rb -r 3')
  system('bundle exec ruby ferret_search.rb')
  Dir.chdir(pwd)
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

desc "Run tests with Valgrind"
task :valgrind do
  sh "valgrind --suppressions=ferret_valgrind.supp " +
      "--leak-check=yes --show-reachable=yes " +
      "-v ruby test/unit/index/tc_index_reader.rb"
end

desc "run thread safety tests"
task :thread_safety do
  system('bundle exec ruby test/threading/thread_safety_index_test.rb')
  system('bundle exec ruby test/threading/thread_safety_read_write_test.rb')
  system('bundle exec ruby test/threading/thread_safety_test.rb')
end

desc "run unit tests in test/unit"
Rake::TestTask.new("units" => :compile) do |t|
  t.libs << "test/unit"
  t.pattern = 'test/unit/t[csz]_*.rb'
  t.verbose = true
end
task :unit => :units

task :default => [:compile, :units]