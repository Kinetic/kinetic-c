PROJECT_CEEDLING_ROOT = "vendor/ceedling"
load "#{PROJECT_CEEDLING_ROOT}/lib/ceedling/rakefile.rb"

task :default => ['test:all', 'release']

desc "Run the kinetic C test utility"
task :run do
  sh "./build/release/kinetic-c-client"
end

desc "Build all and run test utility"
task :all => ['default', 'run']

desc "Run full CI build"
task :ci => ['clobber', 'all']
