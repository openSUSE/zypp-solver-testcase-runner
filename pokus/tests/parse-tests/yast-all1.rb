#!/usr/bin/ruby


# Check handling of various package data.


require 'zypptools/lib/package'
require 'zypptools/output/output'
include ZYppTools

require '../../src/testsuite'


packages = Array.new()

pkg = Package.new("test")
pkg.arch = "i586"
pkg.version = "1.2"
pkg.release = 1
pkg.installsize = 84000
pkg.rpmsize = 42000
pkg.summary = "A pure test package"
packages.push(pkg)


path = Testsuite::write_repo(:yast, packages)


Testsuite.set_arch("i586")
pool = Testsuite.read_repo("file://" + path)


Testsuite::haha2(pool).each do |res|
    puts "#{res.kind} #{res.name} #{res.edition.to_s} #{res.arch.to_s}"
    puts "  Summary: #{res.summary}"
    puts "  Size: #{res.size} / #{res.download_size}"
    Testsuite::dump_all_deps(res)
    puts
end

