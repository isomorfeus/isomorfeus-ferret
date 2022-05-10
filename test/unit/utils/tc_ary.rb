require File.expand_path(File.join(File.dirname(__FILE__), "..", "..", "test_helper.rb"))

class AryTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Utils

  FRT_ARY_INIT_CAPA = 8
  PQ_STRESS_SIZE = 1000

  def test_ary
    ary = Ary.new
    raised = 0
    assert_equal(0, ary.size)
    assert_equal(FRT_ARY_INIT_CAPA, ary.capa)

    ary = Ary.new
    ary.push("one")
    assert_equal(1, ary.size)
    ary.unshift("zero")
    assert_equal(2, ary.size)
    assert_equal("zero", ary[0])
    assert_equal("one", ary[1])
    assert_nil(ary.remove(2))

    begin
        ary.set(-3, "minusone")
    rescue
        raised = 1
    end
    assert_equal(1, raised)

    ary = Ary.new(10)
    assert_equal(0, ary.size)
    assert_equal(10, ary.capa)
    ary.set(1, "one")
    assert_equal(2, ary.size)
    assert_equal("one", ary[1])
    assert_nil(ary[0])
    assert_nil(ary.get(0))
    assert_nil(ary[2])
    assert_nil(ary.get(2))

    # cannot use the simple reference outside of the allocated range
    assert_equal("one", ary.get(-1))
    assert_nil(ary.get(22))
    assert_nil(ary.get(-22))

    ary.set(2, "two")
    assert_equal(3, ary.size)
    assert_equal("one", ary[1])
    assert_equal("two", ary[2])
    assert_nil(ary[0])
    assert_nil(ary[3])
    assert_equal("one", ary.get(-2))
    assert_equal("two", ary.get(-1))
    ary.set(-1, "two")
    ary.set(-3, "zero")

    assert_equal("zero", ary[0])
    assert_equal("one", ary[1])
    assert_equal("two", ary[2])
    assert_equal(3, ary.size)

    ary.set(19, "nineteen")
    assert_equal("nineteen", ary.get(19))
    assert_equal(20, ary.size)
    (4..18).each { |i| assert_nil(ary[i]) }

    ary.push("twenty")
    assert_equal(21, ary.size)
    assert_equal("twenty", ary.pop)
    assert_equal(20, ary.size)

    assert_equal("nineteen", ary.pop)
    assert_equal(19, ary.size)

    assert_nil(ary.pop)
    assert_equal(18, ary.size)

    ary.push("eighteen")
    assert_equal(19, ary.size)
    assert_equal("eighteen", ary[18])
    assert_equal("eighteen", ary.get(-1))
    assert_equal("zero", ary.get(-19))
    assert_equal("one", ary.get(-18))
    assert_equal("two", ary.get(-17))
    assert_nil(ary.get(-16))
    assert_nil(ary.get(-20))

    assert_equal("zero", ary.shift)
    assert_equal(18, ary.size)
    assert_equal("eighteen", ary[17])
    assert_nil(ary[18])
    assert_equal("one", ary.get(-18))
    assert_equal("two", ary.get(-17))
    assert_nil(ary.get(-16))
    assert_nil(ary.get(-19))

    assert_equal("one", ary.shift)
    assert_equal(17, ary.size)
    assert_equal("eighteen", ary[16])
    assert_nil(ary[18])
    assert_nil(ary[17])
    assert_equal("two", ary.get(-17))
    assert_nil(ary.get(-16))
    assert_nil(ary.get(-18))

    ary[5] = "five"
    ary[6] = "six"
    ary[7] = "seven"

    assert_equal("five", ary.get(5))
    assert_equal("six", ary.get(6))
    assert_equal("seven", ary.get(7))

    ary.remove(6)
    assert_equal(16, ary.size)

    assert_equal("five", ary.get(5))
    assert_equal("seven", ary.get(6))
    assert_nil(ary.get(4))
    assert_nil(ary.get(7))
    assert_equal("eighteen", ary[15])
    assert_equal("two", ary.get(-16))
    assert_nil(ary.get(-15))
    assert_nil(ary.get(-17))
    assert_equal("five", ary.get(5))
    assert_equal("seven", ary.get(6))

    tmp = "sixsix"
    ary[6] = tmp
    ary[7] = "seven"
    assert_equal("sixsix", ary.get(6))
    assert_equal("seven", ary.get(7))

    ary.remove(6)
    assert_equal("five", ary.get(5))
    assert_equal("seven", ary.get(6))
    assert_nil(ary.get(4))
    assert_nil(ary.get(7))
  end
end
