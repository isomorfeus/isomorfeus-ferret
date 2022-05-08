require File.expand_path(File.join(File.dirname(__FILE__), "..", "..", "test_helper.rb"))
require_relative "tm_field_assertions"

class FisBasicTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Index
  include FieldAssertions

  def test_fis_basic
    fis = FieldInfos.new(store: :no, compression: :no, index: :no, term_vector: :no);
    fis.add(FieldInfo.new(:FFFFFFFF, store: :no, compression: :no, index: :no, term_vector: :no));
    fis.add(FieldInfo.new(:TFTTFTFF, store: :yes, compression: :no, index: :yes, term_vector: :yes));
    fis.add(FieldInfo.new(:TTTFFTTF, store: :yes, compression: :brotli, index: :untokenized, term_vector: :with_positions));
    fis.add(FieldInfo.new(:FFTTTTFT, store: :no, compression: :no, index: :omit_norms, term_vector: :with_offsets));
    fis.add(FieldInfo.new(:FFTFTTTT, store: :no, compression: :no, index: :untokenized_omit_norms, term_vector: :with_positions_offsets));

    fi = FieldInfo.new(:FFTFTTTT, store: :no, compression: :no, index: :untokenized_omit_norms, term_vector: :with_positions_offsets);
    begin
        asser_equal(fis, fis.add(fi));
    rescue ArgumentError
        arg_error = true;
    end
    assert_equal(true, arg_error);

    assert_equal(fis[:FFFFFFFF], fis[0]);
    assert_equal(fis[:TFTTFTFF], fis[1]);
    assert_equal(fis[:TTTFFTTF], fis[2]);
    assert_equal(fis[:FFTTTTFT], fis[3]);
    assert_equal(fis[:FFTFTTTT], fis[4]);

    assert_equal(0, fis[:FFFFFFFF].number);
    assert_equal(1, fis[:TFTTFTFF].number);
    assert_equal(2, fis[:TTTFFTTF].number);
    assert_equal(3, fis[:FFTTTTFT].number);
    assert_equal(4, fis[:FFTFTTTT].number);

    assert_equal(:FFFFFFFF, fis[0].name);
    assert_equal(:TFTTFTFF, fis[1].name);
    assert_equal(:TTTFFTTF, fis[2].name);
    assert_equal(:FFTTTTFT, fis[3].name);
    assert_equal(:FFTFTTTT, fis[4].name);

    fis[1].boost = 2.0;
    fis[2].boost = 3.0;
    fis[3].boost = 4.0;
    fis[4].boost = 5.0;

    field_prop_assertions(fis[0], :FFFFFFFF, 1.0, F, F, F, F, F, F, F, F);
    field_prop_assertions(fis[1], :TFTTFTFF, 2.0, T, F, T, T, F, T, F, F);
    field_prop_assertions(fis[2], :TTTFFTTF, 3.0, T, T, T, F, F, T, T, F);
    field_prop_assertions(fis[3], :FFTTTTFT, 4.0, F, F, T, T, T, T, F, T);
    field_prop_assertions(fis[4], :FFTFTTTT, 5.0, F, F, T, F, T, T, T, T);
  end
end
