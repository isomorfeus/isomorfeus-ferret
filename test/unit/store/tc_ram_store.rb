require File.expand_path(File.join(File.dirname(__FILE__), "..", "..", "test_helper.rb"))
require File.expand_path(File.join(File.dirname(__FILE__), "tm_store"))
require File.expand_path(File.join(File.dirname(__FILE__), "tm_store_lock"))

class RAMStoreTest < Test::Unit::TestCase
  include StoreTest
  include StoreLockTest
  def setup
    @dir = Isomorfeus::Ferret::Store::RAMDirectory.new
  end

  def teardown
    @dir.close()
  end

  def test_ramlock
    name = "lfile"
    lfile = Isomorfeus::Ferret::Store::Directory::LOCK_PREFIX + name + ".lck"
    assert(! @dir.exists?(lfile),
           "There should be no lock file")
    lock = @dir.make_lock(name)
    assert(! @dir.exists?(lfile),
           "There should still be no lock file")
    assert(! @dir.exists?(lfile),
           "The lock should be hidden by the FSDirectories directory scan")
    assert(! lock.locked?, "lock shouldn't be locked yet")
    lock.obtain
    assert(lock.locked?, "lock should now be locked")
    assert(@dir.exists?(lfile), "A lock file should have been created")
    lock.release
    assert(! lock.locked?, "lock should be freed again")
    assert(! @dir.exists?(lfile),
           "The lock file should have been deleted")
  end
end
