compiler = ENV.fetch('CC', 'gcc')
compiler_location = `which #{compiler}`.strip
compiler_info = `#{compiler} --version 2>&1`.strip

SYSTEM_TEST_HOST = ENV.fetch('SYSTEM_TEST_HOST', "localhost")

task :report_toolchain do
  report_banner("Toolchain Configuration")
  report "" +
    "  compiler:\n" +
    "    location: #{compiler_location}\n" +
    "    info:\n" +
    "      " + compiler_info.gsub(/\n/, "\n      ") + "\n"
end

require 'ceedling'
Ceedling.load_project(config: './config/project.yml')

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
  report
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
    execute_command "./configure --disable-shared; make; make check; sudo make install"
  end

  report_banner "Building protobuf-c and installing protoc-c"
  cd PROTOBUF_C do
    execute_command "./autogen.sh && ./configure && make && sudo make install"
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

namespace :doxygen do

  DOCS_PATH = "./docs/api/"
  directory DOCS_PATH
  CLOBBER.include DOCS_PATH
  VERSION = File.read('./config/VERSION').strip

  task :checkout_github_pages => ['clobber', DOCS_PATH] do
    git "clone git@github.com:seagate/kinetic-c.git -b gh-pages #{DOCS_PATH}"
  end

  desc "Generate API docs"
  task :gen => [DOCS_PATH] do
    # Update API version in doxygen config
    doxyfile = "config/Doxyfile"
    content = File.read(doxyfile)
    content.sub!(/^PROJECT_NUMBER +=.*$/, "PROJECT_NUMBER           = \"v#{VERSION}\"")
    File.open(doxyfile, 'w').puts content

    # Generate the Doxygen API docs
    report_banner "Generating Doxygen API Docs (kinetic-c v#{VERSION})"
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

desc "Prepend license to source files"
task :apply_license do
  Dir['include/**/*.h', 'src/**/*.h', 'src/**/*.c', 'test/**/*.h', 'test/**/*.c'].each do |f|
    sh "config/apply_license.sh #{f}"
  end
end

namespace :tests do

  desc "Run unit tests"
  task :unit do
    report_banner "Running Unit Tests"
    Rake::Task['test:path'].reenable
    Rake::Task['test:path'].invoke('test/unit')
  end

  desc "Run integration tests"
  task :integration do
    report_banner "Running Integration Tests"
    Rake::Task['test:path'].reenable
    Rake::Task['test:path'].invoke('test/integration')
  end

end

task :test_all => ['report_toolchain', 'tests:unit', 'tests:integration']

task :default => ['test:delta']
