# encoding: utf-8
require File.expand_path(File.join(File.dirname(__FILE__), "..", "..", "test_helper.rb"))

class TokenTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Analysis
  def test_token
    t = Token.new("text", 1, 2, 3)
    assert_equal("text", t.text)
    assert_equal(1, t.start)
    assert_equal(2, t.end)
    assert_equal(3, t.pos_inc)
    t.text    = "yada yada yada"
    t.start   = 11
    t.end     = 12
    t.pos_inc = 13
    assert_equal("yada yada yada", t.text)
    assert_equal(11, t.start)
    assert_equal(12, t.end)
    assert_equal(13, t.pos_inc)

    t = Token.new("text", 1, 2)
    assert_equal(1, t.pos_inc)
  end
end

class AsciiLetterTokenizerTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Analysis

  def test_letter_tokenizer
    input = 'DBalmain@gmail.com is My e-mail 523@#$ ADDRESS. 23#!$'
    input.encode!("ASCII-8BIT")
    t = LetterTokenizer.new(input)
    assert_equal(Token.new("DBalmain", 0, 8), t.next)
    assert_equal(Token.new("gmail", 9, 14), t.next)
    assert_equal(Token.new("com", 15, 18), t.next)
    assert_equal(Token.new("is", 19, 21), t.next)
    assert_equal(Token.new("My", 22, 24), t.next)
    assert_equal(Token.new("e", 25, 26), t.next)
    assert_equal(Token.new("mail", 27, 31), t.next)
    assert_equal(Token.new("ADDRESS", 39, 46), t.next)
    assert(! t.next)
    t.text = "one_two three".encode!("ASCII-8BIT")
    assert_equal(Token.new("one", 0, 3), t.next)
    assert_equal(Token.new("two", 4, 7), t.next)
    assert_equal(Token.new("three", 8, 13), t.next)
    assert(! t.next)
    t = LowerCaseFilter.new(LetterTokenizer.new(input))
    assert_equal(Token.new("dbalmain", 0, 8), t.next)
    assert_equal(Token.new("gmail", 9, 14), t.next)
    assert_equal(Token.new("com", 15, 18), t.next)
    assert_equal(Token.new("is", 19, 21), t.next)
    assert_equal(Token.new("my", 22, 24), t.next)
    assert_equal(Token.new("e", 25, 26), t.next)
    assert_equal(Token.new("mail", 27, 31), t.next)
    assert_equal(Token.new("address", 39, 46), t.next)
    assert(! t.next)
  end
end

class LetterTokenizerTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Analysis

  def test_letter_tokenizer
    input = 'DBalm??n@gmail.com is My e-mail 52   #$ address. 23#!$ ????G??????????????????????????'
    t = LetterTokenizer.new(input)
    assert_equal(Token.new('DBalm??n', 0, 8), t.next)
    assert_equal(Token.new('gmail', 9, 14), t.next)
    assert_equal(Token.new('com', 15, 18), t.next)
    assert_equal(Token.new('is', 19, 21), t.next)
    assert_equal(Token.new('My', 22, 24), t.next)
    assert_equal(Token.new('e', 25, 26), t.next)
    assert_equal(Token.new('mail', 27, 31), t.next)
    assert_equal(Token.new('address', 40, 47), t.next)
    assert_equal(Token.new('????G??', 55, 62), t.next)
    assert_equal(Token.new('??????', 64, 70), t.next)
    assert_equal(Token.new('??????', 72, 78), t.next)
    assert_equal(Token.new('??????', 80, 86), t.next)
    assert(! t.next)
    t.text = "one_two three"
    assert_equal(Token.new("one", 0, 3), t.next)
    assert_equal(Token.new("two", 4, 7), t.next)
    assert_equal(Token.new("three", 8, 13), t.next)
    assert(! t.next)
    t = LowerCaseFilter.new(LetterTokenizer.new(input))
    assert_equal(Token.new('dbalm??n', 0, 8), t.next)
    assert_equal(Token.new('gmail', 9, 14), t.next)
    assert_equal(Token.new('com', 15, 18), t.next)
    assert_equal(Token.new('is', 19, 21), t.next)
    assert_equal(Token.new('my', 22, 24), t.next)
    assert_equal(Token.new('e', 25, 26), t.next)
    assert_equal(Token.new('mail', 27, 31), t.next)
    assert_equal(Token.new('address', 40, 47), t.next)
    assert_equal(Token.new('????g??', 55, 62), t.next)
    assert_equal(Token.new('??????', 64, 70), t.next)
    assert_equal(Token.new('??????', 72, 78), t.next)
    assert_equal(Token.new('??????', 80, 86), t.next)
    assert(! t.next)
  end
end

class AsciiWhiteSpaceTokenizerTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Analysis

  def test_whitespace_tokenizer
    input = 'DBalmain@gmail.com is My e-mail 52   #$ ADDRESS. 23#!$'
    input.encode!("ASCII-8BIT")
    t = WhiteSpaceTokenizer.new(input)
    assert_equal(Token.new('DBalmain@gmail.com', 0, 18), t.next)
    assert_equal(Token.new('is', 19, 21), t.next)
    assert_equal(Token.new('My', 22, 24), t.next)
    assert_equal(Token.new('e-mail', 25, 31), t.next)
    assert_equal(Token.new('52', 32, 34), t.next)
    assert_equal(Token.new('#$', 37, 39), t.next)
    assert_equal(Token.new('ADDRESS.', 40, 48), t.next)
    assert_equal(Token.new('23#!$', 49, 54), t.next)
    assert(! t.next)
    t.text = "one_two three".encode!("ASCII-8BIT")
    assert_equal(Token.new("one_two", 0, 7), t.next)
    assert_equal(Token.new("three", 8, 13), t.next)
    assert(! t.next)
    t = LowerCaseFilter.new(WhiteSpaceTokenizer.new(input))
    assert_equal(Token.new('dbalmain@gmail.com', 0, 18), t.next)
    assert_equal(Token.new('is', 19, 21), t.next)
    assert_equal(Token.new('my', 22, 24), t.next)
    assert_equal(Token.new('e-mail', 25, 31), t.next)
    assert_equal(Token.new('52', 32, 34), t.next)
    assert_equal(Token.new('#$', 37, 39), t.next)
    assert_equal(Token.new('address.', 40, 48), t.next)
    assert_equal(Token.new('23#!$', 49, 54), t.next)
    assert(! t.next)
  end
end

class WhiteSpaceTokenizerTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Analysis

  def test_whitespace_tokenizer
    input = 'DBalm??n@gmail.com is My e-mail 52   #$ address. 23#!$ ????G??????????????????????????'
    t = WhiteSpaceTokenizer.new(input)
    assert_equal(Token.new('DBalm??n@gmail.com', 0, 18), t.next)
    assert_equal(Token.new('is', 19, 21), t.next)
    assert_equal(Token.new('My', 22, 24), t.next)
    assert_equal(Token.new('e-mail', 25, 31), t.next)
    assert_equal(Token.new('52', 32, 34), t.next)
    assert_equal(Token.new('#$', 37, 39), t.next)
    assert_equal(Token.new('address.', 40, 48), t.next)
    assert_equal(Token.new('23#!$', 49, 54), t.next)
    assert_equal(Token.new('????G??????????????????????????', 55, 86), t.next)
    assert(! t.next)
    t.text = "one_two three"
    assert_equal(Token.new("one_two", 0, 7), t.next)
    assert_equal(Token.new("three", 8, 13), t.next)
    assert(! t.next)
    t = LowerCaseFilter.new(WhiteSpaceTokenizer.new(input))
    assert_equal(Token.new('dbalm??n@gmail.com', 0, 18), t.next)
    assert_equal(Token.new('is', 19, 21), t.next)
    assert_equal(Token.new('my', 22, 24), t.next)
    assert_equal(Token.new('e-mail', 25, 31), t.next)
    assert_equal(Token.new('52', 32, 34), t.next)
    assert_equal(Token.new('#$', 37, 39), t.next)
    assert_equal(Token.new('address.', 40, 48), t.next)
    assert_equal(Token.new('23#!$', 49, 54), t.next)
    assert_equal(Token.new('????g??????????????????????????', 55, 86), t.next)
    assert(! t.next)
  end
end

class AsciiStandardTokenizerTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Analysis

  def test_standard_tokenizer
    input = 'DBalmain@gmail.com is My e-mail 52   #$ Address. 23#!$ http://www.google.com/results/ T.N.T. 123-1235-ASD-1234'
    t = StandardTokenizer.new(input)
    assert_equal(Token.new('DBalmain@gmail.com', 0, 18), t.next)
    assert_equal(Token.new('is', 19, 21), t.next)
    assert_equal(Token.new('My', 22, 24), t.next)
    assert_equal(Token.new('e-mail', 25, 31), t.next)
    assert_equal(Token.new('52', 32, 34), t.next)
    assert_equal(Token.new('Address', 40, 47), t.next)
    assert_equal(Token.new('23', 49, 51), t.next)
    assert_equal(Token.new('www.google.com/results', 55, 85), t.next)
    assert_equal(Token.new('TNT', 86, 91), t.next)
    assert_equal(Token.new('123-1235-ASD-1234', 93, 110), t.next)
    assert(! t.next)
    t.text = "one_two three"
    assert_equal(Token.new("one_two", 0, 7), t.next)
    assert_equal(Token.new("three", 8, 13), t.next)
    assert(! t.next)
    t = LowerCaseFilter.new(StandardTokenizer.new(input))
    assert_equal(Token.new('dbalmain@gmail.com', 0, 18), t.next)
    assert_equal(Token.new('is', 19, 21), t.next)
    assert_equal(Token.new('my', 22, 24), t.next)
    assert_equal(Token.new('e-mail', 25, 31), t.next)
    assert_equal(Token.new('52', 32, 34), t.next)
    assert_equal(Token.new('address', 40, 47), t.next)
    assert_equal(Token.new('23', 49, 51), t.next)
    assert_equal(Token.new('www.google.com/results', 55, 85), t.next)
    assert_equal(Token.new('tnt', 86, 91), t.next)
    assert_equal(Token.new('123-1235-asd-1234', 93, 110), t.next)
    assert(! t.next)
  end
end

class StandardTokenizerTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Analysis

  def test_standard_tokenizer
    input = 'DBalm??n@gmail.com is My e-mail 52   #$ Address. 23#!$ http://www.google.com/res_345/ T.N.T. 123-1235-ASD-1234 23#!$ ????G??????????????????????????'
    t = StandardTokenizer.new(input)
    assert_equal(Token.new('DBalm??n@gmail.com', 0, 18), t.next)
    assert_equal(Token.new('is', 19, 21), t.next)
    assert_equal(Token.new('My', 22, 24), t.next)
    assert_equal(Token.new('e-mail', 25, 31), t.next)
    assert_equal(Token.new('52', 32, 34), t.next)
    assert_equal(Token.new('Address', 40, 47), t.next)
    assert_equal(Token.new('23', 49, 51), t.next)
    assert_equal(Token.new('www.google.com/res_345', 55, 85), t.next)
    assert_equal(Token.new('TNT', 86, 91), t.next)
    assert_equal(Token.new('123-1235-ASD-1234', 93, 110), t.next)
    assert_equal(Token.new('23', 111, 113), t.next)
    assert_equal(Token.new('????G??', 117, 124), t.next)
    assert_equal(Token.new('??????', 126, 132), t.next)
    assert_equal(Token.new('??????', 134, 140), t.next)
    assert_equal(Token.new('??????', 142, 148), t.next)
    assert(! t.next)
    t.text = "one_two three"
    assert_equal(Token.new("one_two", 0, 7), t.next)
    assert_equal(Token.new("three", 8, 13), t.next)
    assert(! t.next)
    t = LowerCaseFilter.new(StandardTokenizer.new(input))
    assert_equal(Token.new('dbalm??n@gmail.com', 0, 18), t.next)
    assert_equal(Token.new('is', 19, 21), t.next)
    assert_equal(Token.new('my', 22, 24), t.next)
    assert_equal(Token.new('e-mail', 25, 31), t.next)
    assert_equal(Token.new('52', 32, 34), t.next)
    assert_equal(Token.new('address', 40, 47), t.next)
    assert_equal(Token.new('23', 49, 51), t.next)
    assert_equal(Token.new('www.google.com/res_345', 55, 85), t.next)
    assert_equal(Token.new('tnt', 86, 91), t.next)
    assert_equal(Token.new('123-1235-asd-1234', 93, 110), t.next)
    assert_equal(Token.new('23', 111, 113), t.next)
    assert_equal(Token.new('????g??', 117, 124), t.next)
    assert_equal(Token.new('??????', 126, 132), t.next)
    assert_equal(Token.new('??????', 134, 140), t.next)
    assert_equal(Token.new('??????', 142, 148), t.next)
    input = "e-mail 123-1235-asd-1234 http://www.davebalmain.com/trac-site/"
    t = HyphenFilter.new(StandardTokenizer.new(input))
    assert_equal(Token.new('email', 0, 6), t.next)
    assert_equal(Token.new('e', 0, 1, 0), t.next)
    assert_equal(Token.new('mail', 2, 6, 1), t.next)
    assert_equal(Token.new('123-1235-asd-1234', 7, 24), t.next)
    assert_equal(Token.new('www.davebalmain.com/trac-site', 25, 62), t.next)
    assert(! t.next)
  end
end

class RegExpTokenizerTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Analysis

  ALPHA      = /[[:alpha:]_-]+/
  APOSTROPHE = /#{ALPHA}('#{ALPHA})+/
  ACRONYM    = /#{ALPHA}\.(#{ALPHA}\.)+/
  ACRONYM_WORD    = /^#{ACRONYM}$/
  APOSTROPHE_WORD = /^#{APOSTROPHE}$/

  def test_reg_exp_tokenizer
    input = 'DBalmain@gmail.com is My e-mail 52   #$ Address. 23#!$ http://www.google.com/RESULT_3.html T.N.T. 123-1235-ASD-1234 23 Rob\'s'
    t = RegExpTokenizer.new(input)
    assert_equal(Token.new('DBalmain@gmail.com', 0, 18), t.next)
    assert_equal(Token.new('is', 19, 21), t.next)
    assert_equal(Token.new('My', 22, 24), t.next)
    assert_equal(Token.new('e-mail', 25, 31), t.next)
    assert_equal(Token.new('52', 32, 34), t.next)
    assert_equal(Token.new('Address', 40, 47), t.next)
    assert_equal(Token.new('23', 49, 51), t.next)
    assert_equal(Token.new('http://www.google.com/RESULT_3.html', 55, 90), t.next)
    assert_equal(Token.new('T.N.T.', 91, 97), t.next)
    assert_equal(Token.new('123-1235-ASD-1234', 98, 115), t.next)
    assert_equal(Token.new('23', 116, 118), t.next)
    assert_equal(Token.new('Rob\'s', 119, 124), t.next)
    assert(! t.next)
    t.text = "one_two three"
    assert_equal(Token.new("one_two", 0, 7), t.next)
    assert_equal(Token.new("three", 8, 13), t.next)
    assert(! t.next)
    t = LowerCaseFilter.new(RegExpTokenizer.new(input))
    t2 = LowerCaseFilter.new(RegExpTokenizer.new(input, /\w{2,}/))
    assert_equal(Token.new('dbalmain@gmail.com', 0, 18), t.next)
    assert_equal(Token.new('is', 19, 21), t.next)
    assert_equal(Token.new('my', 22, 24), t.next)
    assert_equal(Token.new('e-mail', 25, 31), t.next)
    assert_equal(Token.new('52', 32, 34), t.next)
    assert_equal(Token.new('address', 40, 47), t.next)
    assert_equal(Token.new('23', 49, 51), t.next)
    assert_equal(Token.new('http://www.google.com/result_3.html', 55, 90), t.next)
    assert_equal(Token.new('t.n.t.', 91, 97), t.next)
    assert_equal(Token.new('123-1235-asd-1234', 98, 115), t.next)
    assert_equal(Token.new('23', 116, 118), t.next)
    assert_equal(Token.new('rob\'s', 119, 124), t.next)
    assert(! t.next)
    assert_equal(Token.new('dbalmain', 0, 8), t2.next)
    assert_equal(Token.new('gmail', 9, 14), t2.next)
    assert_equal(Token.new('com', 15, 18), t2.next)
    assert_equal(Token.new('is', 19, 21), t2.next)
    assert_equal(Token.new('my', 22, 24), t2.next)
    assert_equal(Token.new('mail', 27, 31), t2.next)
    assert_equal(Token.new('52', 32, 34), t2.next)
    assert_equal(Token.new('address', 40, 47), t2.next)
    assert_equal(Token.new('23', 49, 51), t2.next)
    assert_equal(Token.new('http', 55, 59), t2.next)
    assert_equal(Token.new('www', 62, 65), t2.next)
    assert_equal(Token.new('google', 66, 72), t2.next)
    assert_equal(Token.new('com', 73, 76), t2.next)
    assert_equal(Token.new('result_3', 77, 85), t2.next)
    assert_equal(Token.new('html', 86, 90), t2.next)
    assert_equal(Token.new('123', 98, 101), t2.next)
    assert_equal(Token.new('1235', 102, 106), t2.next)
    assert_equal(Token.new('asd', 107, 110), t2.next)
    assert_equal(Token.new('1234', 111, 115), t2.next)
    assert_equal(Token.new('23', 116, 118), t2.next)
    assert_equal(Token.new('rob', 119, 122), t2.next)
    assert(! t2.next)
    t = RegExpTokenizer.new(input) do |str|
      if str =~ ACRONYM_WORD
        str.gsub!(/\./, '')
      elsif str =~ APOSTROPHE_WORD
        str.gsub!(/'[sS]$/, '')
      end
      str
    end
    t = LowerCaseFilter.new(t)
    assert_equal(Token.new('dbalmain@gmail.com', 0, 18), t.next)
    assert_equal(Token.new('is', 19, 21), t.next)
    assert_equal(Token.new('my', 22, 24), t.next)
    assert_equal(Token.new('e-mail', 25, 31), t.next)
    assert_equal(Token.new('52', 32, 34), t.next)
    assert_equal(Token.new('address', 40, 47), t.next)
    assert_equal(Token.new('23', 49, 51), t.next)
    assert_equal(Token.new('http://www.google.com/result_3.html', 55, 90), t.next)
    assert_equal(Token.new('tnt', 91, 97), t.next)
    assert_equal(Token.new('123-1235-asd-1234', 98, 115), t.next)
    assert_equal(Token.new('23', 116, 118), t.next)
    assert_equal(Token.new('rob', 119, 124), t.next)
    assert(! t.next)
  end
end

class MappingFilterTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Analysis

  def test_mapping_filter
    mapping = {
      ['??','??','??','??','??','??','??','??']         => 'a',
      '??'                                       => 'ae',
      ['??','??']                                 => 'd',
      ['??','??','??','??','??']                     => 'c',
      ['??','??','??','??','??','??','??','??','??',]    => 'e',
      ['??']                                     => 'f',
      ['??','??','??','??']                         => 'g',
      ['??','??']                                 => 'h',
      ['??','??','??','??','??','??','??','??']         => 'i',
      ['??','??','??','??']                         => 'j',
      ['??','??']                                 => 'k',
      ['??','??','??','??','??']                     => 'l',
      ['??','??','??','??','??','??']                 => 'n',
      ['??','??','??','??','??','??','??','??','??','??'] => 'o',
      '??'                                       => 'oek',
      '??'                                       => 'q',
      ['??','??','??']                             => 'r',
      ['??','??','??','??','??']                     => 's',
      ['??','??','??','??']                         => 't',
      ['??','??','??','??','??','??','??','??','??','??'] => 'u',
      '??'                                       => 'w',
      ['??','??','??']                             => 'y',
      ['??','??','??']                             => 'z'
    }
    input = <<END
a????????????????b c??d e????f g??????????h i??????????????????j k??l m????????n o????p q????????????????r s????????t u????v
w??????????x y????????????z a????????????????????b c??d e??f g??????h i??????????j k????????l m????????????????????n o??p
q??????r s??????t
END
    t = MappingFilter.new(LetterTokenizer.new(input), mapping)
    assert_equal(Token.new('aaaaaaaaab', 0, 18), t.next)
    assert_equal(Token.new('caed', 19, 23), t.next)
    assert_equal(Token.new('eddf', 24, 30), t.next)
    assert_equal(Token.new('gccccch', 31, 43), t.next)
    assert_equal(Token.new('ieeeeeeeeej', 44, 64), t.next)
    assert_equal(Token.new('kfl', 65, 69), t.next)
    assert_equal(Token.new('mggggn', 70, 80), t.next)
    assert_equal(Token.new('ohhp', 81, 87), t.next)
    assert_equal(Token.new('qiiiiiiiir', 88, 106), t.next)
    assert_equal(Token.new('sjjjjt', 107, 117), t.next)
    assert_equal(Token.new('ukkv', 118, 124), t.next)
    assert_equal(Token.new('wlllllx', 125, 137), t.next)
    assert_equal(Token.new('ynnnnnnz', 138, 152), t.next)
    assert_equal(Token.new('aoooooooooob', 153, 175), t.next)
    assert_equal(Token.new('coekd', 176, 180), t.next)
    assert_equal(Token.new('eqf', 181, 185), t.next)
    assert_equal(Token.new('grrrh', 186, 194), t.next)
    assert_equal(Token.new('isssssj', 195, 207), t.next)
    assert_equal(Token.new('kttttl', 208, 218), t.next)
    assert_equal(Token.new('muuuuuuuuuun', 219, 241), t.next)
    assert_equal(Token.new('owp', 242, 246), t.next)
    assert_equal(Token.new('qyyyr', 247, 255), t.next)
    assert_equal(Token.new('szzzt', 256, 264), t.next)
    assert(! t.next)
  end
end

class StopFilterTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Analysis

  def test_stop_filter
    words = ["one", "four", "five", "seven"]
    input = "one, two, three, four, five, six, seven, eight, nine, ten."
    t = StopFilter.new(LetterTokenizer.new(input), words)
    assert_equal(Token.new('two', 5, 8, 2), t.next)
    assert_equal(Token.new('three', 10, 15, 1), t.next)
    assert_equal(Token.new('six', 29, 32, 3), t.next)
    assert_equal(Token.new('eight', 41, 46, 2), t.next)
    assert_equal(Token.new('nine', 48, 52, 1), t.next)
    assert_equal(Token.new('ten', 54, 57, 1), t.next)
    assert(! t.next)
  end
end

class StemFilterTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Analysis

  def test_stop_filter
    input = "Debate Debates DEBATED DEBating Debater";
    t = StemFilter.new(LowerCaseFilter.new(LetterTokenizer.new(input)),
                       "english")
    assert_equal(Token.new("debat", 0, 6), t.next)
    assert_equal(Token.new("debat", 7, 14), t.next)
    assert_equal(Token.new("debat", 15, 22), t.next)
    assert_equal(Token.new("debat", 23, 31), t.next)
    assert_equal(Token.new("debat", 32, 39), t.next)
    assert(! t.next)
    t = StemFilter.new(LetterTokenizer.new(input), :english)
    assert_equal(Token.new("Debat", 0, 6), t.next)
    assert_equal(Token.new("Debat", 7, 14), t.next)
    assert_equal(Token.new("DEBATED", 15, 22), t.next)
    assert_equal(Token.new("DEBate", 23, 31), t.next)
    assert_equal(Token.new("Debat", 32, 39), t.next)

    input = "D??bate d??bates D??BATED D??BATing d??bater";
    t = StemFilter.new(LowerCaseFilter.new(LetterTokenizer.new(input)), :english)
    assert_equal(Token.new("d??bate", 0, 7), t.next)
    assert_equal(Token.new("d??bate", 8, 16), t.next)
    assert_equal(Token.new("d??bate", 17, 25), t.next)
    assert_equal(Token.new("d??bate", 26, 35), t.next)
    assert_equal(Token.new("d??bater", 36, 44), t.next)
    t = StemFilter.new(LetterTokenizer.new(input), :english)
    assert_equal(Token.new("D??bate", 0, 7), t.next)
    assert_equal(Token.new("d??bate", 8, 16), t.next)
    assert_equal(Token.new("D??BATED", 17, 25), t.next)
    assert_equal(Token.new("D??BATing", 26, 35), t.next)
    assert_equal(Token.new("d??bater", 36, 44), t.next)
    assert(! t.next)

    tz = LetterTokenizer.new(input)
    assert_not_nil(StemFilter.new(tz,'HunGarIaN'))
    assert_not_nil(StemFilter.new(tz,'romanIAN'))
    assert_raises(ArgumentError) {StemFilter.new(tz, 'Jibberish')}
  end
end

require 'strscan'
module Isomorfeus::Ferret::Analysis

  class MyRegExpTokenizer < TokenStream

    def initialize(input)
      @ss = StringScanner.new(input)
    end

    # Returns the next token in the stream, or null at EOS.
    def next
      if @ss.scan_until(token_re)
        term = @ss.matched
        term_end = @ss.pos
        term_start = term_end - term.size
      else
        return nil
      end

      return Token.new(normalize(term), term_start, term_end)
    end

    def text=(text)
      @ss = StringScanner.new(text)
    end


    protected
      # returns the regular expression used to find the next token
      TOKEN_RE = /[[:alpha:]]+/
      def token_re
        TOKEN_RE
      end

      # Called on each token to normalize it before it is added to the
      # token.  The default implementation does nothing.  Subclasses may
      # use this to, e.g., lowercase tokens.
      def normalize(str) return str end
  end

  class MyReverseTokenFilter < TokenStream
    def initialize(token_stream)
      @token_stream = token_stream
    end

    def text=(text)
      @token_stream.text = text
    end

    def next
      if token = @token_stream.next
        token.text = token.text.reverse
      end
      token
    end
  end

  class MyCSVTokenizer < MyRegExpTokenizer
    protected
      # returns the regular expression used to find the next token
      TOKEN_RE = /[^,]+/
      def token_re
        TOKEN_RE
      end

      # Called on each token to normalize it before it is added to the
      # token.  The default implementation does nothing.  Subclasses may
      # use this to, e.g., lowercase tokens.
      def normalize(str) return str.upcase end
  end
end

class CustomTokenizerTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Analysis

  def test_custom_tokenizer
    input = "First Field,2nd Field,  P a d d e d  F i e l d  "
    t = MyCSVTokenizer.new(input)
    assert_equal(Token.new("FIRST FIELD", 0, 11), t.next)
    assert_equal(Token.new("2ND FIELD", 12, 21), t.next)
    assert_equal(Token.new("  P A D D E D  F I E L D  ", 22, 48), t.next)
    assert(! t.next)
    t = LowerCaseFilter.new(MyCSVTokenizer.new(input))
    assert_equal(Token.new("first field", 0, 11), t.next)
    assert_equal(Token.new("2nd field", 12, 21), t.next)
    assert_equal(Token.new("  p a d d e d  f i e l d  ", 22, 48), t.next)
    assert(! t.next)
    t = MyReverseTokenFilter.new(
          LowerCaseFilter.new(MyCSVTokenizer.new(input)))
    assert_equal(Token.new("dleif tsrif", 0, 11), t.next)
    assert_equal(Token.new("dleif dn2", 12, 21), t.next)
    assert_equal(Token.new("  d l e i f  d e d d a p  ", 22, 48), t.next)
    t.text = "one,TWO,three"
    assert_equal(Token.new("eno", 0, 3), t.next)
    assert_equal(Token.new("owt", 4, 7), t.next)
    assert_equal(Token.new("eerht", 8, 13), t.next)
    t = LowerCaseFilter.new(
          MyReverseTokenFilter.new(MyCSVTokenizer.new(input)))
    assert_equal(Token.new("dleif tsrif", 0, 11), t.next)
    assert_equal(Token.new("dleif dn2", 12, 21), t.next)
    assert_equal(Token.new("  d l e i f  d e d d a p  ", 22, 48), t.next)
    t.text = "one,TWO,three"
    assert_equal(Token.new("eno", 0, 3), t.next)
    assert_equal(Token.new("owt", 4, 7), t.next)
    assert_equal(Token.new("eerht", 8, 13), t.next)
  end
end

module Isomorfeus::Ferret::Analysis
  class TokenFilter < TokenStream
    protected
      # Construct a token stream filtering the given input.
      def initialize(input)
        @input = input
      end
  end

  # Normalizes token text to lower case.
  class CapitalizeFilter < TokenFilter
    def next
      t = @input.next

      return nil if (t.nil?)

      t.text = t.text.capitalize

      return t
    end
  end
end

class CustomFilterTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Analysis

  def test_custom_filter
    input = "This text SHOULD be capitalized ... I hope. :-S"
    t = CapitalizeFilter.new(LetterTokenizer.new(input))
    assert_equal(Token.new("This", 0, 4), t.next)
    assert_equal(Token.new("Text", 5, 9), t.next)
    assert_equal(Token.new("Should", 10, 16), t.next)
    assert_equal(Token.new("Be", 17, 19), t.next)
    assert_equal(Token.new("Capitalized", 20, 31), t.next)
    assert_equal(Token.new("I", 36, 37), t.next)
    assert_equal(Token.new("Hope", 38, 42), t.next)
    assert_equal(Token.new("S", 46, 47), t.next)
    assert(! t.next)
    t = StemFilter.new(CapitalizeFilter.new(LetterTokenizer.new(input)))
    assert_equal(Token.new("This", 0, 4), t.next)
    assert_equal(Token.new("Text", 5, 9), t.next)
    assert_equal(Token.new("Should", 10, 16), t.next)
    assert_equal(Token.new("Be", 17, 19), t.next)
    assert_equal(Token.new("Capit", 20, 31), t.next)
    assert_equal(Token.new("I", 36, 37), t.next)
    assert_equal(Token.new("Hope", 38, 42), t.next)
    assert_equal(Token.new("S", 46, 47), t.next)
    assert(! t.next)
  end
end
