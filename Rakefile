TEAMCITY_BUILD = !ENV['TEAMCITY_PROJECT_NAME'].nil?

require 'kinetic-ruby'
KineticRuby::Rake::load_tasks()

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

namespace :java_sim do

  $java_sim = nil

  def java_sim_start

    report_banner "Starting Kinetic Java Simulator"

    # Validate JAVA_HOME
    raise "JAVA_HOME must be set!" unless ENV["JAVA_HOME"]
    java = File.join(ENV['JAVA_HOME'], 'bin', 'java')

    # Find the java simulator jar
    sim_jars = Dir["vendor/kinetic-java/kinetic-simulator*.jar"]
    raise "No Kinetic Java simulator .jar files found!" if sim_jars.empty?

    # Configure the classpath
    ENV['CLASSPATH'] = '' unless ENV['CLASSPATH']
    jars = [File.join(ENV['JAVA_HOME'], 'lib', 'tools.jar')] + sim_jars
    jars.each do |jar|
      ENV['CLASSPATH'] += ':' + jar
    end

    sleep 2
    $java_sim = spawn("#{java} -classpath #{ENV['CLASSPATH']} com.seagate.kinetic.simulator.internal.SimulatorRunner")
    sleep 5
  end

  def java_sim_shutdown
    if $java_sim
      report_banner "Shutting down Kinetic Java Simulator"
      Process.kill("INT", $java_sim)
      Process.wait($java_sim)
      $java_sim = nil
    end
  end

  task :start do
    java_sim_start
  end

  task :shutdown do
    java_sim_shutdown
  end

end

namespace :ruby_sim do

  def start_ruby_server
    port = KineticRuby::DEFAULT_KINETIC_PORT
    # port = KineticRuby::TEST_KINETIC_PORT
    $kinetic_server ||= KineticRuby::Server.new(port)
    $kinetic_server.start
  end

  def shutdown_ruby_server
    $kinetic_server.shutdown unless $kinetic_server.nil?
    $kinetic_server = nil
  end

  task :start do
    start_ruby_server
  end

  task :shutdown do
    shutdown_ruby_server
  end
end

task 'test/integration/test_kinetic_socket.c' => ['ruby_sim:start']

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
  start_ruby_server
  Rake::Task['test:path'].reenable
  Rake::Task['test:path'].invoke('test/integration')
  shutdown_ruby_server

  report_banner "Running System Tests"
  java_sim_start
  Rake::Task['test:path'].reenable
  Rake::Task['test:path'].invoke('test/system')
  java_sim_shutdown

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

END {
  # Ensure java simlator is shutdown prior to rake exiting
  java_sim_shutdown
}
