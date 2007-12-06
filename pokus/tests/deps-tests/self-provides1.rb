#!/usr/bin/ruby


# Test addition of self-provides.


require 'zypptools/lib/package'
require 'zypptools/output/output'
include ZYppTools

require '../../src/testsuite'


packages = Array.new()

pkg = Package.new("Nuremberg")
pkg.arch = "i586"
pkg.rpmsize = 42
pkg.version = "1.2"
pkg.release = 1
pkg.summary = "A pure test package"
packages.push(pkg)

pkg = Package.new("Prague")
pkg.arch = "i586"
pkg.rpmsize = 42
pkg.version = "1.4"
pkg.release = 1
pkg.summary = "A pure test package"
pkg.deps :provides, "Prague == 1.2"
packages.push(pkg)


path = Testsuite::write_repo(:yum, packages)


Testsuite::set_arch("i586")
pool = Testsuite::read_repo("file://" + path)

Testsuite::haha2(pool).each do |res|
    puts "#{res.kind} #{res.name} #{res.edition.to_s} #{res.arch.to_s}"
    Testsuite::dump_deps(res, Dep.PROVIDES)
    puts
end

