require File.expand_path(File.join(File.dirname(__FILE__), "..", "..", "test_helper.rb"))
require_relative "tm_field_assertions"

class FieldNewTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Index
  include FieldAssertions

  def test_field_new
    fi = FieldInfo.new(:name, store: :no, compression: :no, index: :no, term_vector: :no)
    field_prop_assertions(fi, :name, 1.0, F, F, F, F, F, F, F, F)
    fi = FieldInfo.new(:name, store: :yes, compression: :no, index: :yes, term_vector: :yes)
    field_prop_assertions(fi, :name, 1.0, T, F, T, T, F, T, F, F)
    fi = FieldInfo.new(:name, store: :yes, compression: :brotli, index: :untokenized, term_vector: :with_positions)
    field_prop_assertions(fi, :name, 1.0, T, T, T, F, F, T, T, F)
    fi = FieldInfo.new(:name, store: :no, compression: :no, index: :omit_norms, term_vector: :with_offsets)
    field_prop_assertions(fi, :name, 1.0, F, F, T, T, T, T, F, T)
    fi = FieldInfo.new(:name, boost: 1000.0, store: :no, compression: :no, index: :untokenized_omit_norms, term_vector: :with_positions_offsets)
    field_prop_assertions(fi, :name, 1000.0, F, F, T, F, T, T, T, T)
  end
end
