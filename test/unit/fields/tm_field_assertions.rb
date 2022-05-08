module FieldAssertions
  F = false
  T = true

  def field_prop_assertions(fi, name, boost, is_stored, is_compressed, is_indexed, is_tokenized, omit_norms, store_term_vector, store_positions, store_offsets)
    assert_equal(name, fi.name)
    assert_equal(boost, fi.boost)
    assert_equal(is_stored, fi.stored?)
    assert_equal(is_compressed, fi.compressed?)
    assert_equal(is_indexed, fi.indexed?)
    assert_equal(is_tokenized, fi.tokenized?)
    assert_equal(omit_norms, fi.omit_norms?)
    assert_equal(store_term_vector, fi.store_term_vector?)
    assert_equal(store_positions, fi.store_positions?)
    assert_equal(store_offsets, fi.store_offsets?)
  end

  def fis_get_or_add_field(fis, name)
    field = fis[name]
    unless field
      fis.add_field(name)
      field = fis[name]
    end
    field
  end
end
