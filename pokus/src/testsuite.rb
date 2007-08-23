
# aschnell@suse.de

require 'zypp'
include Zypp


module Testsuite

    def Testsuite.set_arch(arch)

        arch = Zypp::Arch.new(arch.to_s)
        zypp = ZYppFactory::instance.get_zypp()
        zypp.set_architecture(arch)

    end


    def Testsuite.write_repo(type, packages)

        # ruby does *not* have a mkdtemp function!
        path = '/tmp/no-fun'
        IO.popen('mktemp -d') {|pipe| path = pipe.gets.chomp }

        output = OutputFactory(type, path)
        output.write(packages)

        return path

    end


    # reads a single repository and returns the pool
    def Testsuite.read_repo(url)

        tmp_cache_path = TmpDir.new()
        tmp_raw_cache_path = TmpDir.new()
        tmp_known_repos_path = TmpDir.new()

        opts = RepoManagerOptions.new()
        opts.repoCachePath = tmp_cache_path.path()
        opts.repoRawCachePath = tmp_raw_cache_path.path()
        opts.knownReposPath = tmp_known_repos_path.path()

        repo_manager = RepoManager.new(opts)
        res_pool_manager = ResPoolManager.new()
        repo_info = RepoInfo.new()

        repo_info.set_alias("testsuite")
        repo_info.set_name("Testsuite Repo")
        repo_info.set_enabled(true)
        repo_info.set_autorefresh(false)
        repo_info.add_base_url(url)

        repo_manager.add_repository(repo_info)

        repos = repo_manager.known_repositories()
        repos.each do | repo |
            repo_manager.refresh_metadata(repo)
            repo_manager.build_cache(repo)
            rep = repo_manager.create_from_cache(repo)
            store = rep.resolvables()
            res_pool_manager.insert(store)
        end

        pool = res_pool_manager.accessor()
        return pool

    end


    # TODO: can't this be done more elegant?
    def Testsuite.haha1(x)
        y = Array.new
        x.each do |z|
            y << z
        end
        return y.sort
    end

    def Testsuite.haha2(x)
        y = Array.new
        x.each do |z|
           y << z
        end
        y.map! {|a| a.resolvable}
        return y.sort
    end


    def Testsuite.dump_deps(res, kind)
        deps = res.dep(kind)
        Testsuite.haha1(deps).each do |dep|
            case kind.in_switch
                when Dep.PROVIDES.in_switch : puts "  Provides: #{dep.to_s}"
                when Dep.PREREQUIRES.in_switch : puts "  Prerequires: #{dep.to_s}"
                when Dep.REQUIRES.in_switch : puts "  Requires: #{dep.to_s}"
                when Dep.CONFLICTS.in_switch : puts "  Conflicts: #{dep.to_s}"
                when Dep.OBSOLETES.in_switch : puts "  Obsoletes: #{dep.to_s}"
                when Dep.RECOMMENDS.in_switch : puts "  Recommends: #{dep.to_s}"
                when Dep.SUGGESTS.in_switch : puts "  Suggests: #{dep.to_s}"
                when Dep.FRESHENS.in_switch : puts "  Freshens: #{dep.to_s}"
                when Dep.ENHANCES.in_switch : puts "  Enhances: #{dep.to_s}"
                when Dep.SUPPLEMENTS.in_switch : puts "  Supplements: #{dep.to_s}"
            else
                puts "  Unknown: #{dep.to_s}"
            end
        end
    end


end

