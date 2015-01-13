require 'kinetic-ruby'

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

KineticRuby::Rake::load_tasks
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

  DOCS_PATH = "./docs/"
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

namespace :java_sim do
  JAVA_HOME = ENV.fetch('JAVA_HOME', '/usr')
  JAVA_BIN = File.join(JAVA_HOME, 'bin/java')
  $java_sim = nil

  def kinetic_device_listening?
    `netstat -an` =~ /[\.:]8123.+\s+LISTEN\s+/
  end

  def java_sim_start

    return if $java_sim

    report_banner "Starting Kinetic Java Simulator"

    java_sim_cleanup

    # Find the java simulator jar
    jars = Dir["vendor/kinetic-simulator/kinetic-simulator*.jar"]
    raise "No Kinetic Java simulator .jar files found!" if jars.empty?

    # Configure the classpath
    ENV['CLASSPATH'] = '' unless ENV['CLASSPATH']
    jars += [File.join(JAVA_HOME, 'lib/tools.jar')]
    jars.each {|jar| ENV['CLASSPATH'] += ':' + jar }
    $java_sim = spawn("#{JAVA_BIN} -classpath #{ENV['CLASSPATH']} com.seagate.kinetic.simulator.internal.SimulatorRunner")
    max_wait_secs = 20
    sleep_duration = 0.2
    timeout = false
    elapsed_secs = 0
    while !kinetic_device_listening? && !timeout do
      sleep(sleep_duration)
      elapsed_secs += sleep_duration
      timeout = (elapsed_secs > max_wait_secs)
    end
    raise "Kinetic Java simulator failed to start within #{max_wait_secs} seconds!" if timeout
  end

  def java_sim_shutdown
    if $java_sim
      report_banner "Shutting down Kinetic Java Simulator"
      Process.kill("INT", $java_sim)
      Process.wait($java_sim)
      $java_sim = nil
      sleep 0.5
      java_sim_cleanup
    end
  end

  def java_sim_erase_drive
    java_sim_start
    sh "\"#{JAVA_BIN}\" -classpath \"#{ENV['CLASSPATH']}\" com.seagate.kinetic.admin.cli.KineticAdminCLI -instanterase"
  end

  def java_sim_cleanup
    # Ensure stray simulators are not still running
    `ps -ef | grep kinetic-simulator`.each_line do |l|
      next if l =~ /grep kinetic-simulator/
      pid = l.match /^\s*\d+\s+(\d+)/
      if pid
        sh "kill -9 #{pid[1]}"
        report "Killed Java simulator with PID: #{pid[1]}"
      else
        report "Did not find any running Java Kinetic simulators"
      end
    end
  end

  task :start do
    java_sim_start
  end

  task :shutdown do
    java_sim_shutdown
  end

  desc "Erase Java simulator contents"
  task :erase do
    java_sim_erase_drive
  end

end

namespace :ruby_sim do

  def start_ruby_server
    return if $kinetic_server

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

# Setup ruby and java simulators for integration and system tests
Dir['test/integration/test_*.c'].each do |test_file|
  task test_file => ['java_sim:shutdown', 'ruby_sim:start']
end
Dir['test/system/test_*.c'].each do |test_file|
  task test_file => ['ruby_sim:shutdown', 'java_sim:start']
end

namespace :system do
  desc "Run system tests w/KineticRuby for message inspection"
  task :test_sniff do
    [
      'java_sim:shutdown',
      'ruby_sim:start',
    ].each do |task|
      Rake::Task[task].reenable
      Rake::Task[task].invoke
    end

    Dir['test/system/test_*.c'].each do |test_task|
      p test_task
      Rake::Task[test_task].clear_prerequisites
      Rake::Task[test_task].invoke
    end

  end
end

desc "Prepend license to source files"
task :apply_license do
  Dir['include/**/*.h', 'src/**/*.h', 'src/**/*.c', 'test/**/*.h', 'test/**/*.c'].each do |f|
    sh "config/apply_license.sh #{f}"
  end
end

desc "Enable verbose output"
task :verbose do
  Rake::Task[:verbosity].invoke(3) # Set verbosity to 3:semi-obnoxious for debugging
end

namespace :tests do

  desc "Run unit tests"
  task :unit do
    report_banner "Running Unit Tests"
    Rake::Task['test:path'].reenable
    Rake::Task['test:path'].invoke('test/unit')
  end

  desc "Run integration tests"
  task :integration => ['ruby_sim:start'] do
    report_banner "Running Integration Tests"
    java_sim_shutdown
    start_ruby_server
    Rake::Task['test:path'].reenable
    Rake::Task['test:path'].invoke('test/integration')
    shutdown_ruby_server
  end

  namespace :integration do
    task :noop do
      ####### ???
    end
  end

  desc "Run system tests"
  task :system => ['java_sim:start'] do
    report_banner "Running System Tests"
    shutdown_ruby_server
    java_sim_start
    Rake::Task['test:path'].reenable
    Rake::Task['test:path'].invoke('test/system')
    java_sim_shutdown
  end

  namespace :system do
    FileList['test/system/test_*.c'].each do |test|
      basename = File.basename(test, '.*')
      desc "Run system test #{basename}"
      task basename do
        shutdown_ruby_server
        java_sim_start
        Rake::Task[test].reenable
        Rake::Task[test].invoke
      end
    end
  end

  desc "Run Kinetic Client Utility tests"
  task :utility => ['ruby_sim:shutdown'] do
    sh "make run"
  end

  namespace :utility do

    def with_test_server(banner)
      report_banner(banner)
      shutdown_ruby_server
      java_sim_start
      cd "./build/artifacts/release/" do
        yield if block_given?
      end
    end

    task :noop => ['ruby_sim:shutdown'] do
      with_test_server("Testing NoOp Operation") do
        execute_command "./kinetic-c noop"
        execute_command "./kinetic-c --host localhost noop"
        execute_command "./kinetic-c --host 127.0.0.1 noop"
        execute_command "./kinetic-c --blocking --host 127.0.0.1 noop"
      end
    end

    task :put => ['ruby_sim:shutdown'] do
      with_test_server("Testing Put operation") do
        execute_command "./kinetic-c put"
      end
    end

    task :get => ['ruby_sim:shutdown'] do
      with_test_server("Testing Get operation") do
        execute_command "./kinetic-c put"
        execute_command "./kinetic-c get"
        execute_command "./kinetic-c --host localhost get"
      end
    end

    task :delete => ['release', 'ruby_sim:shutdown'] do
      with_test_server("Testing Get operation") do
        execute_command "./kinetic-c put"
        execute_command "./kinetic-c get"
        execute_command "./kinetic-c delete"
      end
    end

  end

end

task :test_all => ['report_toolchain', 'tests:unit', 'tests:integration', 'tests:system']

desc "Build all and run test utility"
task :all => ['test_all']

desc "Run full CI build"
task :ci => ['clobber', 'all']

task :default => ['test:delta']

END {
  # Ensure java simlator is shutdown prior to rake exiting
  java_sim_shutdown
  $stdout.flush
  $stderr.flush
}
