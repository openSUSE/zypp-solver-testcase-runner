#!/usr/bin/ruby


# Create two simple YUM repositories and check that adding and removing
# yields the correct packages in the pool.


require 'zypp'
require 'zypptools/lib/arch'
require 'zypptools/lib/package'
require 'zypptools/lib/pattern'
require 'zypptools/lib/selection'
require 'zypptools/lib/product'
require 'zypptools/output/output'
include ZYppTools

require '../../src/testsuite'


packages1 = Array.new()
pkg = Package.new("hello")
pkg.arch = "i586"
pkg.version = "1.2"
pkg.release = 1
pkg.summary = "A pure test package"
packages1.push(pkg)
path1 = Testsuite::write_repo(:yum, packages1)


packages2 = Array.new()
pkg = Package.new("world")
pkg.arch = "i586"
pkg.version = "1.4"
pkg.release = 2
pkg.summary = "A pure test package"
packages2.push(pkg)
path2 = Testsuite::write_repo(:yum, packages2)


tmp_cache_path = Zypp::TmpDir.new()
tmp_raw_cache_path = Zypp::TmpDir.new()
tmp_known_repos_path = Zypp::TmpDir.new()

opts = Zypp::RepoManagerOptions.new()
opts.repoCachePath = tmp_cache_path.path()
opts.repoRawCachePath = tmp_raw_cache_path.path()
opts.knownReposPath = tmp_known_repos_path.path()

repo_manager = Zypp::RepoManager.new(opts)

repo_info1 = Zypp::RepoInfo.new()
repo_info1.set_alias("factorytest1")
repo_info1.set_name("Test Repo for Factory.")
repo_info1.set_enabled(true)
repo_info1.set_autorefresh(false)
repo_info1.add_base_url("file://" + path1)
repo_manager.add_repository(repo_info1)
repo_manager.refresh_metadata(repo_info1)
repo_manager.build_cache(repo_info1)
rep1 = repo_manager.create_from_cache(repo_info1)

repo_info2 = Zypp::RepoInfo.new()
repo_info2.set_alias("factorytest2")
repo_info2.set_name("Test Repo for Factory.")
repo_info2.set_enabled(true)
repo_info2.set_autorefresh(false)
repo_info2.add_base_url("file://" + path2)
repo_manager.add_repository(repo_info2)
repo_manager.refresh_metadata(repo_info2)
repo_manager.build_cache(repo_info2)
rep2 = repo_manager.create_from_cache(repo_info2)


z = Zypp::ZYppFactory::instance.get_zypp()
pool = z.pool()


def dump(pool)
    Testsuite::haha2(pool).each do |res|
        puts "#{res.kind_to_s} #{res.name} #{res.edition.to_s} #{res.arch.to_s}"
	Testsuite::dump_deps(res, Zypp::Dep.PROVIDES)
        puts
    end
end


puts "=== step 1 ==="
dump(pool)

z.add_resolvables(rep1.resolvables())

puts "=== step 2 ==="
dump(pool)

z.add_resolvables(rep2.resolvables())

puts "=== step 3 ==="
dump(pool)

z.remove_resolvables(rep1.resolvables())

puts "=== step 4 ==="
dump(pool)

z.remove_resolvables(rep2.resolvables())

puts "=== step 5 ==="
dump(pool)

