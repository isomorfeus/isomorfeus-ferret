require File.expand_path(File.join(File.dirname(__FILE__), "..", "..", "test_helper.rb"))
require_relative "tm_field_assertions"

class FisRwTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Index
  include FieldAssertions

  def test_fis_rw
    # store = Isomorfeus::Ferret::Store::RAMDirectory.new
    fis = FieldInfos.new(store: :yes, compression: :no, index: :untokenized_omit_norms, term_vector: :with_positions_offsets)
    fis.add(FieldInfo.new(:FFFFFFFF, store: :no, compression: :no, index: :no, term_vector: :no))
    fis.add(FieldInfo.new(:TFTTFTFF, store: :yes, compression: :no, index: :yes, term_vector: :yes))
    fis.add(FieldInfo.new(:TTTFFTTF, store: :yes, compression: :brotli, index: :untokenized, term_vector: :with_positions))
    fis.add(FieldInfo.new(:FFTTTTFT, store: :no, compression: :no, index: :omit_norms, term_vector: :with_offsets))
    fis.add(FieldInfo.new(:FFTFTTTT, store: :no, compression: :no, index: :untokenized_omit_norms, term_vector: :with_positions_offsets))
    fis[1].boost = 2.0
    fis[2].boost = 3.0
    fis[3].boost = 4.0
    fis[4].boost = 5.0

    # TODO
    # os = store->new_output(store, "fields")
    # fis_write(fis, os)
    # frt_os_close(os)

    # these fields won't be saved be will added again later
    assert_equal(5, fis.size)
    field_prop_assertions(fis_get_or_add_field(fis, :new_field), :new_field, 1.0, T, F, T, F, T, T, T, T)
    assert_equal(6, fis.size)
    field_prop_assertions(fis_get_or_add_field(fis, :another), :another, 1.0, T, F, T, F, T, T, T, T)
    assert_equal(7, fis.size)

    # TODO
    # is = store->open_input(store, "fields")
    # fis = fis_read(is)
    fis = FieldInfos.new(store: :yes, compression: :no, index: :untokenized_omit_norms, term_vector: :with_positions_offsets)
    fis.add(FieldInfo.new(:FFFFFFFF, store: :no, compression: :no, index: :no, term_vector: :no))
    fis.add(FieldInfo.new(:TFTTFTFF, store: :yes, compression: :no, index: :yes, term_vector: :yes))
    fis.add(FieldInfo.new(:TTTFFTTF, store: :yes, compression: :brotli, index: :untokenized, term_vector: :with_positions))
    fis.add(FieldInfo.new(:FFTTTTFT, store: :no, compression: :no, index: :omit_norms, term_vector: :with_offsets))
    fis.add(FieldInfo.new(:FFTFTTTT, store: :no, compression: :no, index: :untokenized_omit_norms, term_vector: :with_positions_offsets))
    fis[1].boost = 2.0
    fis[2].boost = 3.0
    fis[3].boost = 4.0
    fis[4].boost = 5.0

    # frt_is_close(is)
    # assert_equal(store: :yes, fis.store_val)
    # assert_equal({ index: :omit_norms }, fis.index)
    # assert_equal({ term_vector: :with_positions_offsets }, fis.term_vector)

    field_prop_assertions(fis[0], :FFFFFFFF, 1.0, F, F, F, F, F, F, F, F)
    field_prop_assertions(fis[1], :TFTTFTFF, 2.0, T, F, T, T, F, T, F, F)
    field_prop_assertions(fis[2], :TTTFFTTF, 3.0, T, T, T, F, F, T, T, F)
    field_prop_assertions(fis[3], :FFTTTTFT, 4.0, F, F, T, T, T, T, F, T)
    field_prop_assertions(fis[4], :FFTFTTTT, 5.0, F, F, T, F, T, T, T, T)
    assert_equal(5, fis.size)
    field_prop_assertions(fis_get_or_add_field(fis, :new_field), :new_field, 1.0, T, F, T, F, T, T, T, T)
    assert_equal(6, fis.size)
    field_prop_assertions(fis_get_or_add_field(fis, :another), :another, 1.0, T, F, T, F, T, T, T, T)
    assert_equal(7, fis.size)
    str = fis.to_s
    fisstr = <<~FISSTR
    default:
      store: :yes
      index: :untokenized_omit_norms
      term_vector: :with_positions_offsets
    fields:
      FFFFFFFF:
        boost: 1.000000
        store: :no
        index: :no
        term_vector: :no
      TFTTFTFF:
        boost: 2.000000
        store: :yes
        index: :yes
        term_vector: :yes
      TTTFFTTF:
        boost: 3.000000
        store: :compressed
        index: :untokenized
        term_vector: :with_positions
      FFTTTTFT:
        boost: 4.000000
        store: :no
        index: :omit_norms
        term_vector: :with_offsets
      FFTFTTTT:
        boost: 5.000000
        store: :no
        index: :untokenized_omit_norms
        term_vector: :with_positions_offsets
      new_field:
        boost: 1.000000
        store: :yes
        index: :untokenized_omit_norms
        term_vector: :with_positions_offsets
      another:
        boost: 1.000000
        store: :yes
        index: :untokenized_omit_norms
        term_vector: :with_positions_offsets
    FISSTR
    assert_equal(fisstr, str)
    # frt_store_close(store)
  end
end
