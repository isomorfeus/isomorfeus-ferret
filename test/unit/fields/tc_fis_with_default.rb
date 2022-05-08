require File.expand_path(File.join(File.dirname(__FILE__), "..", "..", "test_helper.rb"))
require_relative "tm_field_assertions"

class FisWithDefaultTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Index
  include FieldAssertions

  def test_fis_with_default
    fis = FieldInfos.new(store: :no, compression: :no, index: :no, term_vector: :no)
    field_prop_assertions(fis_get_or_add_field(fis, :name), :name, 1.0, F, F, F, F, F, F, F, F)
    field_prop_assertions(fis_get_or_add_field(fis, :dave), :dave, 1.0, F, F, F, F, F, F, F, F)
    field_prop_assertions(fis_get_or_add_field(fis, :wert), :wert, 1.0, F, F, F, F, F, F, F, F)
    field_prop_assertions(fis[0], :name, 1.0, F, F, F, F, F, F, F, F)
    field_prop_assertions(fis[1], :dave, 1.0, F, F, F, F, F, F, F, F)
    field_prop_assertions(fis[2], :wert, 1.0, F, F, F, F, F, F, F, F)
    assert_nil(fis[:random])

    fis = FieldInfos.new(store: :yes, compression: :no, index: :yes, term_vector: :yes)
    field_prop_assertions(fis_get_or_add_field(fis, :name), :name, 1.0, T, F, T, T, F, T, F, F)

    fis = FieldInfos.new(store: :yes, compression: :brotli, index: :untokenized, term_vector: :with_positions)
    field_prop_assertions(fis_get_or_add_field(fis, :name), :name, 1.0, T, T, T, F, F, T, T, F)

    fis = FieldInfos.new(store: :no, compression: :no, index: :omit_norms, term_vector: :with_offsets)
    field_prop_assertions(fis_get_or_add_field(fis, :name), :name, 1.0, F, F, T, T, T, T, F, T)

    fis = FieldInfos.new(store: :no, compression: :no, index: :untokenized_omit_norms, term_vector: :with_positions_offsets)
    field_prop_assertions(fis_get_or_add_field(fis, :name), :name, 1.0, F, F, T, F, T, T, T, T)
  end
end
