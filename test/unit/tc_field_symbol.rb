require File.expand_path(File.join(File.dirname(__FILE__), "..", "test_helper.rb"))

class FieldSymbolTest < Test::Unit::TestCase
  def test_field_symbol
    Isomorfeus::Ferret::FIELD_TYPES.each do |field_type|
      assert(:sym.respond_to?(field_type),
             "Symbol doesn't respond to #{field_type}")
    end

    %w(desc desc? type).each do  |method|
      assert(:sym.respond_to?(method),
             "Symbol doesn't respond to #{method}")
    end

    assert_nil(:sym.type)
    assert(!:sym.desc?)
    assert(:sym.desc.desc?)
    assert(!:sym.desc.desc.desc?)

    Isomorfeus::Ferret::FIELD_TYPES.each do |field_type|
      assert_equal(field_type, :sym.__send__(field_type).type)
    end

    assert(:string, :sym.integer.byte.float.string.type.to_s)
  end
end
