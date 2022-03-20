$:.unshift File.dirname(__FILE__)
$:.unshift File.join(File.dirname(__FILE__), '../lib')
$:.unshift File.join(File.dirname(__FILE__), '../ext')

class Float
  def approx_eql?(o)
    return (1 - self/o).abs < 0.0001
  end
  alias :=~ :approx_eql?
end

require 'test/unit'
require 'isomorfeus-ferret'
require 'unit/index/th_doc' if (defined?(IndexTestHelper).nil?)

def load_test_dir(dir)
  Dir[File.join(File.dirname(__FILE__), dir, "t[scm]*.rb")].each do |file|
    require file
  end
end
