require File.expand_path(File.join(File.dirname(__FILE__), "..", "test_helper.rb"))

class InternalTest < Test::Unit::TestCase
  def test_all_internal
    puts "\n\n Internal Tests:"
    count = Isomorfeus::Ferret::Test.run_all

    assert_equal(count, 35)
  end
end
