require File.expand_path(File.join(File.dirname(__FILE__), "..", "..", "test_helper.rb"))

class LazyDocTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Index
  include Isomorfeus::Ferret::Search
  include Isomorfeus::Ferret::Analysis
  include Isomorfeus::Ferret::Store

  def test_lazy_doc
    index = Isomorfeus::Ferret::I.new(:default_input_field => :xxx)
    hd = {:xxx => "two", :field2 => "three", :field3 => "four"}
    index << hd
    ld = index[0]
    assert_equal([:xxx, :field2, :field3], ld.keys)
    ld = index[0]
    assert_equal(hd[:xxx], ld[:xxx])
    ld = index[0]
    assert_equal(hd[:field2], ld[:field2])
    ld = index[0]
    assert_equal(hd[:field3], ld[:field3])
    ld = index[0]
    assert_equal(hd[:xxx], ld[:xxx])
    assert_equal(hd[:field2], ld[:field2])
    assert_equal(hd[:field3], ld[:field3])
    ld = index[0]
    assert_equal(hd.size, ld.size)
    ld = index[0]
    assert_equal(hd, ld)
    ld = index[0]
    assert_equal(hd, ld.load)
    ld = index[0]
    assert_equal(hd, ld.to_h)
    ld = index[0]
    assert_equal(hd, ld.to_hash)
    ld = index[0]
    assert_equal(LazyDoc, ld.class)
    ld = index[0]
    assert(ld.key?(:field2))
  end
end
