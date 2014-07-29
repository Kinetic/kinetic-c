TEAMCITY_BUILD = !ENV['TEAMCITY_PROJECT_NAME'].nil?

require 'kinetic-ruby'
load 'kinetic-ruby.rake'

require 'ceedling'
Ceedling.load_project(config: './project.yml')

def report(message='')
  $stderr.flush
  $stdout.flush
  puts message
  $stderr.flush
  $stdout.flush
end

def report_banner(message)
  report "\n#{message}\n#{'='*message.length}\n\n"
end

def execute_command(cmd, banner=nil)
  report_banner banner unless banner.nil?
  report "Executing: #{cmd}"
  sh cmd
  report unless banner.nil?
end

def git(cmd)
  execute_command "git #{cmd}"
end

HERE = File.expand_path(File.dirname(__FILE__))
VENDOR_PATH = File.join(HERE, 'vendor')
PROTOBUF_CORE = File.join(VENDOR_PATH, 'protobuf-2.5.0')
PROTOBUF_C = File.join(VENDOR_PATH, 'protobuf-c')
PROTO_IN = File.join(VENDOR_PATH, 'kinetic-protocol')
BUILD_ARTIFACTS = File.join(HERE, 'build', 'artifacts', 'release')
TEST_ARTIFACTS = File.join(HERE, 'build', 'artifacts', 'test')
PROTO_OUT = TEST_ARTIFACTS
TEST_TEMP = File.join(HERE, 'build', 'test', 'temp')

directory PROTO_OUT
CLOBBER.include PROTO_OUT
directory TEST_TEMP
CLOBBER.include TEST_TEMP

task :test => ['test:delta']

desc "Generate protocol buffers"
task :proto => [PROTO_OUT] do

  report_banner "Building protobuf v2.5.0"
  cd PROTOBUF_CORE do
    execute_command "./configure --disable-shared; make; make check; make install"
  end

  report_banner "Building protobuf-c and installing protoc-c"
  cd PROTOBUF_C do
    execute_command "./autogen.sh && ./configure && make && make install"
    protoc_c = `which protoc-c`
    raise "Failed to find protoc-c utility" if protoc_c.strip.empty?
    versions = `protoc-c --version`
    version_match = versions.match /^protobuf-c (\d+\.\d+\.\d+-?r?c?\d*)\nlibprotoc (\d+\.\d+\.\d+-?r?c?\d*)$/mi
    raise "Failed to query protoc-c/libprotoc version info" if version_match.nil?
    protobuf_c_ver, libprotoc_ver = version_match[1..2]
    report_banner "Successfully built protobuf-c"
    report "protoc-c  v#{protobuf_c_ver}"
    report "libprotoc v#{libprotoc_ver}"
    report
  end

  report_banner "Generating Kinetic C protocol buffers"
  cd PROTO_OUT do
    rm Dir["*.h", "*.c", "*.proto"]
    cp "#{PROTO_IN}/kinetic.proto", "."
    execute_command "protoc-c --c_out=. kinetic.proto"
    report "Generated #{Dir['*.h', '*.c'].join(', ')}\n\n"
  end

end

desc "Analyze code w/CppCheck"
task :cppcheck do
  raise "CppCheck not found!" unless `cppcheck --version` =~ /cppcheck \d+.\d+/mi
  execute_command "cppcheck ./src ./test ./build/temp/proto", "Analyzing code w/CppCheck"
end

namespace :doxygen do

  DOCS_PATH = "./docs/"
  directory DOCS_PATH
  CLOBBER.include DOCS_PATH
  VERSION = File.read('VERSION').strip

  task :checkout_github_pages => ['clobber', DOCS_PATH] do
    git "clone git@github.com:atomicobject/kinetic-c.git -b gh-pages #{DOCS_PATH}"
  end

  desc "Generate API docs"
  task :gen => [DOCS_PATH] do
    # Update API version in doxygen config
    doxyfile = "config/Doxyfile"
    content = File.read(doxyfile)
    content.sub!(/^PROJECT_NUMBER +=.*$/, "PROJECT_NUMBER           = \"v#{VERSION}\"")
    File.open(doxyfile, 'w').puts content

    # Generate the Doxygen API docs
    report_banner "Generating Doxygen API Docs (kinetic-c v#{VERSION}"
    execute_command "doxygen #{doxyfile}"
  end

  desc "Generate and publish API docs"
  task :update_public_api => ['doxygen:checkout_github_pages', 'doxygen:gen'] do
    cd DOCS_PATH do
      git "add --all"
      git "status"
      git "commit -m 'Regenerated API docs for v#{VERSION}'"
      git "push"
      report_banner "Published updated API docs for v#{VERSION} to GitHub!"
    end
  end

end

task 'test/integration/test_kinetic_socket.c' => ['server:start']

desc "Run client test utility"
task :run do
  execute_command "./build/release/kinetic-c-client", "Running client test utility"
end

desc "Prepend license to source files"
task :apply_license do
  Dir['include/**/*.h', 'src/**/*.h', 'src/**/*.c', 'test/**/*.h', 'test/**/*.c'].each do |f|
    sh "config/apply_license.sh #{f}"
  end
end

desc "Validate .travis.yml config file"
namespace :travis do
  task :validate do
    execute_command "bundle exec travis lint --skip-version-check --skip-completion-check", "Validating Travis CI Configuration"
  end
end

desc "Enable verbose Ceedling output"
task :verbose do
  Rake::Task[:verbosity].invoke(4) # Set verbosity to 4-obnoxious for debugging
end

task :default => [
  'test:delta',
  'release'
]

task :test_all do
  report_banner "Running Unit Tests"
  Rake::Task['test:path'].reenable
  Rake::Task['test:path'].invoke('test/unit')
  report_banner "Running Integration Tests"
  Rake::Task['test:path'].reenable
  Rake::Task['test:path'].invoke('test/integration')
  report_banner "Running System Tests"
  Rake::Task['test:path'].reenable
  Rake::Task['test:path'].invoke('test/system')
  report_banner "Finished executing all test suites"
end

desc "Build all and run test utility"
task :all => [
  'cppcheck',
  'test_all',
  'release',
  'run',
  'travis:validate'
]

desc "Run full CI build"
task :ci => [
  'clobber',
  # 'verbose', # uncomment to enable verbose output for CI builds
  'all'
]
