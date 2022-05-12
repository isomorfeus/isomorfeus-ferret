require File.expand_path(File.join(File.dirname(__FILE__), "..", "..", "test_helper.rb"))

class MultiMapperTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Utils

  def test_multimapper
    text = "abc cabc abd cabcd"
    mapper = MultiMapper.new

    mapper.add_mapping("abc", "hello")

    mapper.compile
    assert_equal(24, mapper.map_length(text, 1000))
    assert_equal("hello chello abd chellod", mapper.map(text, 1000))
    assert_equal("hello chello abd chel", mapper.map(text, 22))
    assert_equal(21, mapper.map_length(text, 22))
    assert_equal("hello chello a", mapper.map(text, 15))

    mapper.add_mapping("abcd", "hello")
    mapper.compile
    assert_equal("hello chello abd chellod", mapper.map(text, 1000))

    mapper.add_mapping("cab", "taxi")
    mapper.compile
    assert_equal("hello taxic abd taxicd", mapper.map(text, 1000))
  end

  def test_multimapper_utf8
    text = "zàáâãäåāăz";
    mapper = MultiMapper.new

    mapper.add_mapping("à", "a")
    mapper.add_mapping("á", "a")
    mapper.add_mapping("â", "a")
    mapper.add_mapping("ã", "a")
    mapper.add_mapping("ä", "a")
    mapper.add_mapping("å", "a")
    mapper.add_mapping("ā", "a")
    mapper.add_mapping("ă", "a")
    mapper.compile;
    assert_equal("zaaaaaaaaz", mapper.map(text))
  end
end
