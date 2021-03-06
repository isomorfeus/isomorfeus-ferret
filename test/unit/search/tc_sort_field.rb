require File.expand_path(File.join(File.dirname(__FILE__), "..", "..", "test_helper.rb"))

class SortFieldTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Search

  def test_field_score
    fs = SortField::SCORE
    assert_equal(:score, fs.type)
    assert_nil(fs.name)
    assert(!fs.reverse?, "SCORE_ID should not be reverse")
    assert_nil(fs.comparator)
  end

  def test_field_doc
    fs = SortField::DOC_ID
    assert_equal(:doc_id, fs.type)
    assert_nil(fs.name)
    assert(!fs.reverse?, "DOC_ID should be reverse")
    assert_nil(fs.comparator)
  end

  def test_error_raised
    assert_raise(ArgumentError) {
      SortField.new(nil, :type => :integer)
    }
  end
end
