PROJECT_CEEDLING_ROOT = "vendor/ceedling"
TEAMCITY_BUILD = defined?(TEAMCITY_PROJECT_NAME)

load "#{PROJECT_CEEDLING_ROOT}/lib/ceedling/rakefile.rb"

def report(message='')
  puts message
  $stdout.flush
end

def report_banner(message)
  report "\n#{message}\n#{'='*message.length}"
end

desc "Analyze code w/CppCheck"
task :cppcheck do
  raise "CppCheck not found!" unless `cppcheck --version` =~ /cppcheck \d+.\d+/mi
  report_banner "Analyzing code w/CppCheck"
  sh "cppcheck ./src"
  report ''
end

task :default => %w|cppcheck test:all release|

desc "Run the kinetic C test utility"
task :run do
  sh "./build/release/kinetic-c-client"
end

desc "Build all and run test utility"
task :all => ['default', 'run']

desc "Run full CI build"
task :ci => ['clobber', 'all']
