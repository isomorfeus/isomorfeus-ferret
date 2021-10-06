# -*- encoding: utf-8 -*-
require_relative 'lib/isomorfeus/ferret/version.rb'

Gem::Specification.new do |s|
  s.name          = 'isomorfeus-ferret'
  s.version       = Isomorfeus::Ferret::VERSION

  s.authors       = ['Jan Biedermann']
  s.email         = ['jan@kursator.com']
  s.homepage      = 'http://isomorfeus.com'
  s.summary       = 'Search for Isomorfeus.'
  s.license       = 'MIT'
  s.description   = 'Search for Isomorfeus.'
  s.metadata      = { "github_repo" => "ssh://github.com/isomorfeus/gems" }
  s.files         = `git ls-files -- lib LICENSE README.md`.split("\n")
  s.require_paths = ['lib']

  s.add_development_dependency 'rake'
  s.add_development_dependency 'rake-compiler'
  s.add_development_dependency 'test-unit'
end
