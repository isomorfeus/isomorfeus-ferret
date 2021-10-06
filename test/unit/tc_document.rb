require File.expand_path(File.join(File.dirname(__FILE__), "..", "test_helper.rb"))

class DocumentTest < Test::Unit::TestCase
  def test_field
    f = Isomorfeus::Ferret::Field.new
    assert_equal(0, f.size)
    assert_equal(1.0, f.boost)

    f2 = Isomorfeus::Ferret::Field.new
    assert_equal(f, f2)

    f << "section0"
    assert_equal(1, f.size)
    assert_equal(1.0, f.boost)
    assert_equal("section0", f[0])
    assert_not_equal(f, f2)

    f << "section1"
    assert_equal(2, f.size)
    assert_equal(1.0, f.boost)
    assert_equal("section0", f[0])
    assert_equal("section1", f[1])
    assert_equal('["section0", "section1"]', f.to_s)
    assert_not_equal(f, f2)
    f2 += f
    assert_equal(f, f2)

    f.boost = 4.0
    assert_not_equal(f, f2)
    assert_equal('["section0", "section1"]^4.0', f.to_s)

    f2.boost = 4.0
    assert_equal(f, f2)

    f3 = Isomorfeus::Ferret::Field.new(["section0", "section1"], 4.0)
    assert_equal(f, f3)
  end

  def test_document
    d = Isomorfeus::Ferret::Document.new

    d[:name] = Isomorfeus::Ferret::Field.new
    d[:name] << "section0"
    d[:name] << "section1"

    assert_equal(1, d.size)
    assert_equal(1.0, d.boost)
    assert_equal(%(
Document {
  :name => ["section0", "section1"]
}).strip, d.to_s)


    d.boost = 123.0
    d[:name] << "section2"
    d[:name].boost = 321.0
    assert_equal(123.0, d.boost)
    assert_equal(321.0, d[:name].boost)
    assert_equal(%(
Document {
  :name => ["section0", "section1", "section2"]^321.0
}^123.0).strip, d.to_s)

    d[:title] = "Shawshank Redemption"
    d[:actors] = ["Tim Robbins", "Morgan Freeman"]

    assert_equal(3, d.size)
    assert_equal(%(
Document {
  :actors => ["Tim Robbins", "Morgan Freeman"]
  :name => ["section0", "section1", "section2"]^321.0
  :title => "Shawshank Redemption"
}^123.0).strip, d.to_s)

    d2 = Isomorfeus::Ferret::Document.new(123.0)
    d2[:name] = Isomorfeus::Ferret::Field.new(["section0", "section1", "section2"], 321.0)
    d2[:title] = "Shawshank Redemption"
    d2[:actors] = ["Tim Robbins", "Morgan Freeman"]
    assert_equal(d, d2)
  end
end
