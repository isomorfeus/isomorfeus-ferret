require File.expand_path(File.join(File.dirname(__FILE__), "..", "test_helper.rb"))

class PoshTest < Test::Unit::TestCase
  def test_all_internal
    puts "\n\n POSH:"
    Isomorfeus::Ferret::Test.posh

    assert_equal(0, 0)
  end
end
