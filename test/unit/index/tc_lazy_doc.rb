require File.expand_path(File.join(File.dirname(__FILE__), "..", "..", "test_helper.rb"))

class LazyDocTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Index
  include Isomorfeus::Ferret::Search
  include Isomorfeus::Ferret::Analysis
  include Isomorfeus::Ferret::Store

  def test_lazy_doc
    index = Isomorfeus::Ferret::I.new(:default_input_field => :xxx)
    hd = {:xxx => "two", :field2 => "three", :field3 => "four"}
    hd_lt = {:xxx => "two", :field2 => "three"}
    hd_ltx = {:xxx => "two", :field2 => "three", :field3 => "aaa"}
    hd_gt = {:xxx => "two", :field2 => "three", :field3 => "four", :field4 => "five"}
    hd_gtx = {:xxx => "two", :field2 => "three", :field3 => "xxx"}
    index << hd
    ld = index[0]
    assert_equal([:field2, :field3, :xxx], ld.keys.sort)
    assert_equal([:field2, :field3, :xxx], ld.fields.sort)
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
    ld = index[0]
    assert_equal(Enumerator, ld.each.class)
    assert_equal([[:field2, "three"], [:field3, "four"], [:xxx, "two"]], ld.each.to_a.sort)
    assert_equal({"xxx" => "two", "field2" => "three", "field3" => "four"},  ld.transform_keys {|k|k.to_s})
    ld = index[0]
    assert(hd_lt < ld)
    assert(ld > hd_lt)
    assert(!(hd_ltx < ld))
    assert(!(ld > hd_ltx))
    assert(hd_gt > ld)
    assert(ld < hd_gt)
    assert(!(hd_gtx > ld))
    assert(!(ld < hd_gtx))
  end
end
