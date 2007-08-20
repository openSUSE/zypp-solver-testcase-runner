#!/usr/bin/ruby


# Generate YUM repository containing a package for all architectures.  Check
# for architectures that the correct architectures are loaded into the pool.


require 'zypptools/lib/arch'
require 'zypptools/lib/package'
require 'zypptools/lib/pattern'
require 'zypptools/lib/selection'
require 'zypptools/lib/product'
require 'zypptools/output/output'
include ZYppTools

require '../../src/testsuite'


path = "/ARVIN/zypp/trunk/repotools/tmp"


packages = Array.new()

Arch.all_archs.each do | arch |
    pkg = Package.new("test")
    pkg.arch = arch
    pkg.version = "1.2"
    pkg.release = 1
    pkg.summary = "A pure test package"
    packages.push(pkg)
end


output = OutputFactory(:yum, path)
output.write(packages)


Arch.all_archs.each do | arch |

    x = Arch.new(arch)

    puts "#{x.arch} -> #{x.compat.join(' ')}"

    Testsuite.set_arch(x.arch)

    t = Array.new()

    pool = Testsuite.read_repo("file://" + path)
    pool.each do | p |
        r = p.resolvable
        puts "#{r.kind_to_s} #{r.name} #{r.edition.to_s} #{r.arch.to_s}"
        t.push(r.arch.to_s)
    end

    if not x.compat.map{|x|x.to_s}.sort().eql?(t.sort())
        puts "Error: archs don't match"
        exit 1
    end

    puts

end

