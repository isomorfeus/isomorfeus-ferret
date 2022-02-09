# encoding: utf-8
require File.expand_path(File.join(File.dirname(__FILE__), "..", "..", "test_helper.rb"))

class AnalyzerTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Analysis

  def test_analyzer
    input = 'DBalmain@gmail.com is My E-Mail 523@#$ ADDRESS. 23#!$'
    a = Analyzer.new
    t = a.token_stream("fieldname", input)
    t2 = a.token_stream("fieldname", input)
    assert_equal(Token.new("dbalmain", 0, 8), t.next)
    assert_equal(Token.new("gmail", 9, 14), t.next)
    assert_equal(Token.new("com", 15, 18), t.next)
    assert_equal(Token.new("is", 19, 21), t.next)
    assert_equal(Token.new("my", 22, 24), t.next)
    assert_equal(Token.new("e", 25, 26), t.next)
    assert_equal(Token.new("mail", 27, 31), t.next)
    assert_equal(Token.new("address", 39, 46), t.next)
    assert(! t.next)
    assert_equal(Token.new("dbalmain", 0, 8), t2.next)
    assert_equal(Token.new("gmail", 9, 14), t2.next)
    assert_equal(Token.new("com", 15, 18), t2.next)
    assert_equal(Token.new("is", 19, 21), t2.next)
    assert_equal(Token.new("my", 22, 24), t2.next)
    assert_equal(Token.new("e", 25, 26), t2.next)
    assert_equal(Token.new("mail", 27, 31), t2.next)
    assert_equal(Token.new("address", 39, 46), t2.next)
    assert(! t2.next)
    a = Analyzer.new(false)
    t = a.token_stream("fieldname", input)
    assert_equal(Token.new("DBalmain", 0, 8), t.next)
    assert_equal(Token.new("gmail", 9, 14), t.next)
    assert_equal(Token.new("com", 15, 18), t.next)
    assert_equal(Token.new("is", 19, 21), t.next)
    assert_equal(Token.new("My", 22, 24), t.next)
    assert_equal(Token.new("E", 25, 26), t.next)
    assert_equal(Token.new("Mail", 27, 31), t.next)
    assert_equal(Token.new("ADDRESS", 39, 46), t.next)
    assert(! t.next)
  end
end if (/utf-8/i =~ Isomorfeus::Ferret.locale)

class AsciiLetterAnalyzerTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Analysis

  def test_letter_analyzer
    input = 'DBalmain@gmail.com is My E-Mail 523@#$ ADDRESS. 23#!$'
    a = AsciiLetterAnalyzer.new
    t = a.token_stream("fieldname", input)
    t2 = a.token_stream("fieldname", input)
    assert_equal(Token.new("dbalmain", 0, 8), t.next)
    assert_equal(Token.new("gmail", 9, 14), t.next)
    assert_equal(Token.new("com", 15, 18), t.next)
    assert_equal(Token.new("is", 19, 21), t.next)
    assert_equal(Token.new("my", 22, 24), t.next)
    assert_equal(Token.new("e", 25, 26), t.next)
    assert_equal(Token.new("mail", 27, 31), t.next)
    assert_equal(Token.new("address", 39, 46), t.next)
    assert(! t.next)
    assert_equal(Token.new("dbalmain", 0, 8), t2.next)
    assert_equal(Token.new("gmail", 9, 14), t2.next)
    assert_equal(Token.new("com", 15, 18), t2.next)
    assert_equal(Token.new("is", 19, 21), t2.next)
    assert_equal(Token.new("my", 22, 24), t2.next)
    assert_equal(Token.new("e", 25, 26), t2.next)
    assert_equal(Token.new("mail", 27, 31), t2.next)
    assert_equal(Token.new("address", 39, 46), t2.next)
    assert(! t2.next)
    a = AsciiLetterAnalyzer.new(false)
    t = a.token_stream("fieldname", input)
    assert_equal(Token.new("DBalmain", 0, 8), t.next)
    assert_equal(Token.new("gmail", 9, 14), t.next)
    assert_equal(Token.new("com", 15, 18), t.next)
    assert_equal(Token.new("is", 19, 21), t.next)
    assert_equal(Token.new("My", 22, 24), t.next)
    assert_equal(Token.new("E", 25, 26), t.next)
    assert_equal(Token.new("Mail", 27, 31), t.next)
    assert_equal(Token.new("ADDRESS", 39, 46), t.next)
    assert(! t.next)
  end
end

class LetterAnalyzerTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Analysis

  def test_letter_analyzer
    Isomorfeus::Ferret.locale = ""
    input = 'DBalmän@gmail.com is My e-mail 52   #$ address. 23#!$ ÁÄGÇ®ÊËÌ¯ÚØÃ¬ÖÎÍ'
    a = LetterAnalyzer.new(false)
    t = a.token_stream("fieldname", input)
    t2 = a.token_stream("fieldname", input)
    assert_equal(Token.new("DBalmän", 0, 8), t.next)
    assert_equal(Token.new("gmail", 9, 14), t.next)
    assert_equal(Token.new("com", 15, 18), t.next)
    assert_equal(Token.new("is", 19, 21), t.next)
    assert_equal(Token.new("My", 22, 24), t.next)
    assert_equal(Token.new("e", 25, 26), t.next)
    assert_equal(Token.new("mail", 27, 31), t.next)
    assert_equal(Token.new("address", 40, 47), t.next)
    assert_equal(Token.new("ÁÄGÇ", 55, 62), t.next)
    assert_equal(Token.new("ÊËÌ", 64, 70), t.next)
    assert_equal(Token.new("ÚØÃ", 72, 78), t.next)
    assert_equal(Token.new("ÖÎÍ", 80, 86), t.next)
    assert(! t.next)
    assert_equal(Token.new("DBalmän", 0, 8), t2.next)
    assert_equal(Token.new("gmail", 9, 14), t2.next)
    assert_equal(Token.new("com", 15, 18), t2.next)
    assert_equal(Token.new("is", 19, 21), t2.next)
    assert_equal(Token.new("My", 22, 24), t2.next)
    assert_equal(Token.new("e", 25, 26), t2.next)
    assert_equal(Token.new("mail", 27, 31), t2.next)
    assert_equal(Token.new("address", 40, 47), t2.next)
    assert_equal(Token.new("ÁÄGÇ", 55, 62), t2.next)
    assert_equal(Token.new("ÊËÌ", 64, 70), t2.next)
    assert_equal(Token.new("ÚØÃ", 72, 78), t2.next)
    assert_equal(Token.new("ÖÎÍ", 80, 86), t2.next)
    assert(! t2.next)
    a = LetterAnalyzer.new
    t = a.token_stream("fieldname", input)
    assert_equal(Token.new("dbalmän", 0, 8), t.next)
    assert_equal(Token.new("gmail", 9, 14), t.next)
    assert_equal(Token.new("com", 15, 18), t.next)
    assert_equal(Token.new("is", 19, 21), t.next)
    assert_equal(Token.new("my", 22, 24), t.next)
    assert_equal(Token.new("e", 25, 26), t.next)
    assert_equal(Token.new("mail", 27, 31), t.next)
    assert_equal(Token.new("address", 40, 47), t.next)
    assert_equal(Token.new("áägç", 55, 62), t.next)
    assert_equal(Token.new("êëì", 64, 70), t.next)
    assert_equal(Token.new("úøã", 72, 78), t.next)
    assert_equal(Token.new("öîí", 80, 86), t.next)
    assert(! t.next)
  end
end if (/utf-8/i =~ Isomorfeus::Ferret.locale)

class AsciiWhiteSpaceAnalyzerTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Analysis

  def test_white_space_analyzer
    input = 'DBalmain@gmail.com is My E-Mail 52   #$ ADDRESS. 23#!$'
    a = AsciiWhiteSpaceAnalyzer.new
    t = a.token_stream("fieldname", input)
    t2 = a.token_stream("fieldname", input)
    assert_equal(Token.new('DBalmain@gmail.com', 0, 18), t.next)
    assert_equal(Token.new('is', 19, 21), t.next)
    assert_equal(Token.new('My', 22, 24), t.next)
    assert_equal(Token.new('E-Mail', 25, 31), t.next)
    assert_equal(Token.new('52', 32, 34), t.next)
    assert_equal(Token.new('#$', 37, 39), t.next)
    assert_equal(Token.new('ADDRESS.', 40, 48), t.next)
    assert_equal(Token.new('23#!$', 49, 54), t.next)
    assert(! t.next)
    assert_equal(Token.new('DBalmain@gmail.com', 0, 18), t2.next)
    assert_equal(Token.new('is', 19, 21), t2.next)
    assert_equal(Token.new('My', 22, 24), t2.next)
    assert_equal(Token.new('E-Mail', 25, 31), t2.next)
    assert_equal(Token.new('52', 32, 34), t2.next)
    assert_equal(Token.new('#$', 37, 39), t2.next)
    assert_equal(Token.new('ADDRESS.', 40, 48), t2.next)
    assert_equal(Token.new('23#!$', 49, 54), t2.next)
    assert(! t2.next)
    a = AsciiWhiteSpaceAnalyzer.new(true)
    t = a.token_stream("fieldname", input)
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

class WhiteSpaceAnalyzerTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Analysis

  def test_white_space_analyzer
    input = 'DBalmän@gmail.com is My e-mail 52   #$ address. 23#!$ ÁÄGÇ®ÊËÌ¯ÚØÃ¬ÖÎÍ'
    a = WhiteSpaceAnalyzer.new
    t = a.token_stream("fieldname", input)
    t2 = a.token_stream("fieldname", input)
    assert_equal(Token.new('DBalmän@gmail.com', 0, 18), t.next)
    assert_equal(Token.new('is', 19, 21), t.next)
    assert_equal(Token.new('My', 22, 24), t.next)
    assert_equal(Token.new('e-mail', 25, 31), t.next)
    assert_equal(Token.new('52', 32, 34), t.next)
    assert_equal(Token.new('#$', 37, 39), t.next)
    assert_equal(Token.new('address.', 40, 48), t.next)
    assert_equal(Token.new('23#!$', 49, 54), t.next)
    assert_equal(Token.new('ÁÄGÇ®ÊËÌ¯ÚØÃ¬ÖÎÍ', 55, 86), t.next)
    assert(! t.next)
    assert_equal(Token.new('DBalmän@gmail.com', 0, 18), t2.next)
    assert_equal(Token.new('is', 19, 21), t2.next)
    assert_equal(Token.new('My', 22, 24), t2.next)
    assert_equal(Token.new('e-mail', 25, 31), t2.next)
    assert_equal(Token.new('52', 32, 34), t2.next)
    assert_equal(Token.new('#$', 37, 39), t2.next)
    assert_equal(Token.new('address.', 40, 48), t2.next)
    assert_equal(Token.new('23#!$', 49, 54), t2.next)
    assert_equal(Token.new('ÁÄGÇ®ÊËÌ¯ÚØÃ¬ÖÎÍ', 55, 86), t2.next)
    assert(! t2.next)
    a = WhiteSpaceAnalyzer.new(true)
    t = a.token_stream("fieldname", input)
    assert_equal(Token.new('dbalmän@gmail.com', 0, 18), t.next)
    assert_equal(Token.new('is', 19, 21), t.next)
    assert_equal(Token.new('my', 22, 24), t.next)
    assert_equal(Token.new('e-mail', 25, 31), t.next)
    assert_equal(Token.new('52', 32, 34), t.next)
    assert_equal(Token.new('#$', 37, 39), t.next)
    assert_equal(Token.new('address.', 40, 48), t.next)
    assert_equal(Token.new('23#!$', 49, 54), t.next)
    assert_equal(Token.new('áägç®êëì¯úøã¬öîí', 55, 86), t.next)
    assert(! t.next)
  end
end if (/utf-8/i =~ Isomorfeus::Ferret.locale)

class AsciiStandardAnalyzerTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Analysis

  def test_standard_analyzer
    input = 'DBalmain@gmail.com is My e-mail 52   #$ Address. 23#!$ http://www.google.com/results/ T.N.T. 123-1235-ASD-1234'
    a = AsciiStandardAnalyzer.new
    t = a.token_stream("fieldname", input)
    t2 = a.token_stream("fieldname", input)
    assert_equal(Token.new('dbalmain@gmail.com', 0, 18), t.next)
    assert_equal(Token.new('email', 25, 31, 3), t.next)
    assert_equal(Token.new('e', 25, 26, 0), t.next)
    assert_equal(Token.new('mail', 27, 31), t.next)
    assert_equal(Token.new('52', 32, 34), t.next)
    assert_equal(Token.new('address', 40, 47), t.next)
    assert_equal(Token.new('23', 49, 51), t.next)
    assert_equal(Token.new('www.google.com/results', 55, 85), t.next)
    assert_equal(Token.new('tnt', 86, 91), t.next)
    assert_equal(Token.new('123-1235-asd-1234', 93, 110), t.next)
    assert(! t.next)
    assert_equal(Token.new('dbalmain@gmail.com', 0, 18), t2.next)
    assert_equal(Token.new('email', 25, 31, 3), t2.next)
    assert_equal(Token.new('e', 25, 26, 0), t2.next)
    assert_equal(Token.new('mail', 27, 31), t2.next)
    assert_equal(Token.new('52', 32, 34), t2.next)
    assert_equal(Token.new('address', 40, 47), t2.next)
    assert_equal(Token.new('23', 49, 51), t2.next)
    assert_equal(Token.new('www.google.com/results', 55, 85), t2.next)
    assert_equal(Token.new('tnt', 86, 91), t2.next)
    assert_equal(Token.new('123-1235-asd-1234', 93, 110), t2.next)
    assert(! t2.next)
    a = AsciiStandardAnalyzer.new(ENGLISH_STOP_WORDS, false)
    t = a.token_stream("fieldname", input)
    t2 = a.token_stream("fieldname", input)
    assert_equal(Token.new('DBalmain@gmail.com', 0, 18), t.next)
    assert_equal(Token.new('My', 22, 24), t.next)
    assert_equal(Token.new('email', 25, 31, 3), t.next)
    assert_equal(Token.new('e', 25, 26, 0), t.next)
    assert_equal(Token.new('mail', 27, 31), t.next)
    assert_equal(Token.new('52', 32, 34), t.next)
    assert_equal(Token.new('Address', 40, 47), t.next)
    assert_equal(Token.new('23', 49, 51), t.next)
    assert_equal(Token.new('www.google.com/results', 55, 85), t.next)
    assert_equal(Token.new('TNT', 86, 91), t.next)
    assert_equal(Token.new('123-1235-ASD-1234', 93, 110), t.next)
    assert(! t.next)
  end
end

class StandardAnalyzerTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Analysis

  def test_standard_analyzer
    input = 'DBalmán@gmail.com is My e-mail and the Address. 23#!$ http://www.google.com/results/ T.N.T. 123-1235-ASD-1234 23#!$ ÁÄGÇ®ÊËÌ¯ÚØÃ¬ÖÎÍ'
    a = StandardAnalyzer.new
    t = a.token_stream("fieldname", input)
    t2 = a.token_stream("fieldname", input)
    assert_equal(Token.new('dbalmán@gmail.com', 0, 18), t.next)
    assert_equal(Token.new('email', 25, 31, 3), t.next)
    assert_equal(Token.new('e', 25, 26, 0), t.next)
    assert_equal(Token.new('mail', 27, 31), t.next)
    assert_equal(Token.new('address', 40, 47), t.next)
    assert_equal(Token.new('23', 49, 51), t.next)
    assert_equal(Token.new('www.google.com/results', 55, 85), t.next)
    assert_equal(Token.new('tnt', 86, 91), t.next)
    assert_equal(Token.new('123-1235-asd-1234', 93, 110), t.next)
    assert_equal(Token.new('23', 111, 113), t.next)
    assert_equal(Token.new('áägç', 117, 124), t.next)
    assert_equal(Token.new('êëì', 126, 132), t.next)
    assert_equal(Token.new('úøã', 134, 140), t.next)
    assert_equal(Token.new('öîí', 142, 148), t.next)
    assert(! t.next)
    assert_equal(Token.new('dbalmán@gmail.com', 0, 18), t2.next)
    assert_equal(Token.new('email', 25, 31, 3), t2.next)
    assert_equal(Token.new('e', 25, 26, 0), t2.next)
    assert_equal(Token.new('mail', 27, 31), t2.next)
    assert_equal(Token.new('address', 40, 47), t2.next)
    assert_equal(Token.new('23', 49, 51), t2.next)
    assert_equal(Token.new('www.google.com/results', 55, 85), t2.next)
    assert_equal(Token.new('tnt', 86, 91), t2.next)
    assert_equal(Token.new('123-1235-asd-1234', 93, 110), t2.next)
    assert_equal(Token.new('23', 111, 113), t2.next)
    assert_equal(Token.new('áägç', 117, 124), t2.next)
    assert_equal(Token.new('êëì', 126, 132), t2.next)
    assert_equal(Token.new('úøã', 134, 140), t2.next)
    assert_equal(Token.new('öîí', 142, 148), t2.next)
    assert(! t2.next)
    a = StandardAnalyzer.new(nil, false)
    t = a.token_stream("fieldname", input)
    assert_equal(Token.new('DBalmán@gmail.com', 0, 18), t.next)
    assert_equal(Token.new('My', 22, 24), t.next)
    assert_equal(Token.new('email', 25, 31, 3), t.next)
    assert_equal(Token.new('e', 25, 26, 0), t.next)
    assert_equal(Token.new('mail', 27, 31), t.next)
    assert_equal(Token.new('Address', 40, 47), t.next)
    assert_equal(Token.new('23', 49, 51), t.next)
    assert_equal(Token.new('www.google.com/results', 55, 85), t.next)
    assert_equal(Token.new('TNT', 86, 91), t.next)
    assert_equal(Token.new('123-1235-ASD-1234', 93, 110), t.next)
    assert_equal(Token.new('23', 111, 113), t.next)
    assert_equal(Token.new('ÁÄGÇ', 117, 124), t.next)
    assert_equal(Token.new('ÊËÌ', 126, 132), t.next)
    assert_equal(Token.new('ÚØÃ', 134, 140), t.next)
    assert_equal(Token.new('ÖÎÍ', 142, 148), t.next)
    assert(! t.next)
    a = StandardAnalyzer.new(["e-mail", "23", "tnt"])
    t = a.token_stream("fieldname", input)
    t2 = a.token_stream("fieldname", input)
    assert_equal(Token.new('dbalmán@gmail.com', 0, 18), t.next)
    assert_equal(Token.new('is', 19, 21), t.next)
    assert_equal(Token.new('my', 22, 24), t.next)
    assert_equal(Token.new('and', 32, 35), t.next)
    assert_equal(Token.new('the', 36, 39), t.next)
    assert_equal(Token.new('address', 40, 47), t.next)
    assert_equal(Token.new('www.google.com/results', 55, 85), t.next)
    assert_equal(Token.new('123-1235-asd-1234', 93, 110), t.next)
    assert_equal(Token.new('áägç', 117, 124), t.next)
    assert_equal(Token.new('êëì', 126, 132), t.next)
    assert_equal(Token.new('úøã', 134, 140), t.next)
    assert_equal(Token.new('öîí', 142, 148), t.next)
    assert(! t.next)
    assert_equal(Token.new('dbalmán@gmail.com', 0, 18), t2.next)
    assert_equal(Token.new('is', 19, 21), t2.next)
    assert_equal(Token.new('my', 22, 24), t2.next)
    assert_equal(Token.new('and', 32, 35), t2.next)
    assert_equal(Token.new('the', 36, 39), t2.next)
    assert_equal(Token.new('address', 40, 47), t2.next)
    assert_equal(Token.new('www.google.com/results', 55, 85), t2.next)
    assert_equal(Token.new('123-1235-asd-1234', 93, 110), t2.next)
    assert_equal(Token.new('áägç', 117, 124), t2.next)
    assert_equal(Token.new('êëì', 126, 132), t2.next)
    assert_equal(Token.new('úøã', 134, 140), t2.next)
    assert_equal(Token.new('öîí', 142, 148), t2.next)
    assert(! t2.next)
  end
end if (/utf-8/i =~ Isomorfeus::Ferret.locale)

class PerFieldAnalyzerTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Analysis
  def test_per_field_analyzer
    input = 'DBalmain@gmail.com is My e-mail 52   #$ address. 23#!$'
    pfa = PerFieldAnalyzer.new(StandardAnalyzer.new)
    pfa['white'] = WhiteSpaceAnalyzer.new(false)
    pfa['white_l'] = WhiteSpaceAnalyzer.new(true)
    pfa['letter'] = LetterAnalyzer.new(false)
    pfa.add_field('letter', LetterAnalyzer.new(true))
    pfa.add_field('letter_u', LetterAnalyzer.new(false))
    t = pfa.token_stream('white', input)
    assert_equal(Token.new('DBalmain@gmail.com', 0, 18), t.next)
    assert_equal(Token.new('is', 19, 21), t.next)
    assert_equal(Token.new('My', 22, 24), t.next)
    assert_equal(Token.new('e-mail', 25, 31), t.next)
    assert_equal(Token.new('52', 32, 34), t.next)
    assert_equal(Token.new('#$', 37, 39), t.next)
    assert_equal(Token.new('address.', 40, 48), t.next)
    assert_equal(Token.new('23#!$', 49, 54), t.next)
    assert(! t.next)
    t = pfa.token_stream('white_l', input)
    assert_equal(Token.new('dbalmain@gmail.com', 0, 18), t.next)
    assert_equal(Token.new('is', 19, 21), t.next)
    assert_equal(Token.new('my', 22, 24), t.next)
    assert_equal(Token.new('e-mail', 25, 31), t.next)
    assert_equal(Token.new('52', 32, 34), t.next)
    assert_equal(Token.new('#$', 37, 39), t.next)
    assert_equal(Token.new('address.', 40, 48), t.next)
    assert_equal(Token.new('23#!$', 49, 54), t.next)
    assert(! t.next)
    t = pfa.token_stream('letter_u', input)
    assert_equal(Token.new('DBalmain', 0, 8), t.next)
    assert_equal(Token.new('gmail', 9, 14), t.next)
    assert_equal(Token.new('com', 15, 18), t.next)
    assert_equal(Token.new('is', 19, 21), t.next)
    assert_equal(Token.new('My', 22, 24), t.next)
    assert_equal(Token.new('e', 25, 26), t.next)
    assert_equal(Token.new('mail', 27, 31), t.next)
    assert_equal(Token.new('address', 40, 47), t.next)
    assert(! t.next)
    t = pfa.token_stream('letter', input)
    assert_equal(Token.new('dbalmain', 0, 8), t.next)
    assert_equal(Token.new('gmail', 9, 14), t.next)
    assert_equal(Token.new('com', 15, 18), t.next)
    assert_equal(Token.new('is', 19, 21), t.next)
    assert_equal(Token.new('my', 22, 24), t.next)
    assert_equal(Token.new('e', 25, 26), t.next)
    assert_equal(Token.new('mail', 27, 31), t.next)
    assert_equal(Token.new('address', 40, 47), t.next)
    assert(! t.next)
    t = pfa.token_stream('XXX', input) # should use default StandardAnalzyer
    assert_equal(Token.new('dbalmain@gmail.com', 0, 18), t.next)
    assert_equal(Token.new('email', 25, 31, 3), t.next)
    assert_equal(Token.new('e', 25, 26, 0), t.next)
    assert_equal(Token.new('mail', 27, 31), t.next)
    assert_equal(Token.new('52', 32, 34), t.next)
    assert_equal(Token.new('address', 40, 47), t.next)
    assert_equal(Token.new('23', 49, 51), t.next)
    assert(! t.next)
  end
end

class RegExpAnalyzerTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Analysis

  def test_reg_exp_analyzer
    input = 'DBalmain@gmail.com is My e-mail 52   #$ Address. 23#!$ http://www.google.com/RESULT_3.html T.N.T. 123-1235-ASD-1234 23 Rob\'s'
    a = RegExpAnalyzer.new
    t = a.token_stream('XXX', input)
    t2 = a.token_stream('XXX', "one_Two three")
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
    t = t2
    assert_equal(Token.new("one_two", 0, 7), t.next)
    assert_equal(Token.new("three", 8, 13), t.next)
    assert(! t.next)
    a = RegExpAnalyzer.new(/\w{2,}/, false)
    t = a.token_stream('XXX', input)
    t2 = a.token_stream('XXX', "one Two three")
    assert_equal(Token.new('DBalmain', 0, 8), t.next)
    assert_equal(Token.new('gmail', 9, 14), t.next)
    assert_equal(Token.new('com', 15, 18), t.next)
    assert_equal(Token.new('is', 19, 21), t.next)
    assert_equal(Token.new('My', 22, 24), t.next)
    assert_equal(Token.new('mail', 27, 31), t.next)
    assert_equal(Token.new('52', 32, 34), t.next)
    assert_equal(Token.new('Address', 40, 47), t.next)
    assert_equal(Token.new('23', 49, 51), t.next)
    assert_equal(Token.new('http', 55, 59), t.next)
    assert_equal(Token.new('www', 62, 65), t.next)
    assert_equal(Token.new('google', 66, 72), t.next)
    assert_equal(Token.new('com', 73, 76), t.next)
    assert_equal(Token.new('RESULT_3', 77, 85), t.next)
    assert_equal(Token.new('html', 86, 90), t.next)
    assert_equal(Token.new('123', 98, 101), t.next)
    assert_equal(Token.new('1235', 102, 106), t.next)
    assert_equal(Token.new('ASD', 107, 110), t.next)
    assert_equal(Token.new('1234', 111, 115), t.next)
    assert_equal(Token.new('23', 116, 118), t.next)
    assert_equal(Token.new('Rob', 119, 122), t.next)
    assert(! t.next)
    assert_equal(Token.new("one", 0, 3), t2.next)
    assert_equal(Token.new("Two", 4, 7), t2.next)
    assert_equal(Token.new("three", 8, 13), t2.next)
    assert(! t2.next)
    a = RegExpAnalyzer.new do |str|
      if str =~ /^[[:alpha:]]\.([[:alpha:]]\.)+$/
        str.gsub!(/\./, '')
      elsif str =~ /'[sS]$/
        str.gsub!(/'[sS]$/, '')
      end
      str
    end
    t = a.token_stream('XXX', input)
    t2 = a.token_stream('XXX', "one's don't T.N.T.")
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
    assert_equal(Token.new("one", 0, 5), t2.next)
    assert_equal(Token.new("don't", 6, 11), t2.next)
    assert_equal(Token.new("tnt", 12, 18), t2.next)
    assert(! t2.next)
  end
end

module Isomorfeus::Ferret::Analysis
  class StemmingStandardAnalyzer < StandardAnalyzer
    def token_stream(field, text)
      StemFilter.new(super)
    end
  end
end

class CustomAnalyzerTest < Test::Unit::TestCase
  include Isomorfeus::Ferret::Analysis

  def test_custom_filter
    input = 'DBalmán@gmail.com is My e-mail and the Address. 23#!$ http://www.google.com/results/ T.N.T. 123-1235-ASD-1234 23#!$ ÁÄGÇ®ÊËÌ¯ÚØÃ¬ÖÎÍ'
    a = StemmingStandardAnalyzer.new
    t = a.token_stream("fieldname", input)
    assert_equal(Token.new('dbalmán@gmail.com', 0, 18), t.next)
    assert_equal(Token.new('email', 25, 31, 3), t.next)
    assert_equal(Token.new('e', 25, 26, 0), t.next)
    assert_equal(Token.new('mail', 27, 31), t.next)
    assert_equal(Token.new('address', 40, 47), t.next)
    assert_equal(Token.new('23', 49, 51), t.next)
    assert_equal(Token.new('www.google.com/result', 55, 85), t.next)
    assert_equal(Token.new('tnt', 86, 91), t.next)
    assert_equal(Token.new('123-1235-asd-1234', 93, 110), t.next)
    assert_equal(Token.new('23', 111, 113), t.next)
    assert_equal(Token.new('áägç', 117, 124), t.next)
    assert_equal(Token.new('êëì', 126, 132), t.next)
    assert_equal(Token.new('úøã', 134, 140), t.next)
    assert_equal(Token.new('öîí', 142, 148), t.next)
    assert(! t.next)
    input = "Debate Debates DEBATED DEBating Debater";
    t = a.token_stream("fieldname", input)
    assert_equal(Token.new("debat", 0, 6), t.next)
    assert_equal(Token.new("debat", 7, 14), t.next)
    assert_equal(Token.new("debat", 15, 22), t.next)
    assert_equal(Token.new("debat", 23, 31), t.next)
    assert_equal(Token.new("debat", 32, 39), t.next)
    assert(! t.next)
    input = "Dêbate dêbates DÊBATED DÊBATing dêbater";
    t = StemFilter.new(LowerCaseFilter.new(LetterTokenizer.new(input)), :english)
    assert_equal(Token.new("dêbate", 0, 7), t.next)
    assert_equal(Token.new("dêbate", 8, 16), t.next)
    assert_equal(Token.new("dêbate", 17, 25), t.next)
    assert_equal(Token.new("dêbate", 26, 35), t.next)
    assert_equal(Token.new("dêbater", 36, 44), t.next)
    assert(! t.next)
  end
end if (/utf-8/i =~ Isomorfeus::Ferret.locale)
