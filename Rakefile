require 'bundler'
require 'bundler/cli'
require 'bundler/cli/exec'

require_relative 'lib/isomorfeus/ferret/version'

require 'bundler/setup'
require 'rake/extensiontask'
require 'rake/testtask'

Rake::ExtensionTask.new :isomorfeus_ferret_ext

task :specs do
  system('bundle exec rspec')
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

desc "run unit tests in test/unit"
Rake::TestTask.new("units" => :compile) do |t|
  t.libs << "test/unit"
  t.pattern = 'test/unit/t[csz]_*.rb'
  t.verbose = true
end
task :unit => :units

task :default => [:compile, :unit]