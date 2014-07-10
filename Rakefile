TEAMCITY_BUILD = !ENV['TEAMCITY_PROJECT_NAME'].nil?

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

HERE = File.expand_path(File.dirname(__FILE__))
VENDOR_PATH = File.join(HERE, 'vendor')
PROTOBUF_CORE = File.join(VENDOR_PATH, 'protobuf-2.5.0')
PROTOBUF_C = File.join(VENDOR_PATH, 'protobuf-c')
PROTO_IN = File.join(VENDOR_PATH, 'kinetic-protocol')
PROTO_OUT = File.join(HERE, 'build', 'temp', 'proto')

directory PROTO_OUT

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
  execute_command "cppcheck ./src ./build/temp/proto", "Analyzing code w/CppCheck"
end

namespace :test_server do

  require "webrick"

  $test_server = nil

  # WEBrick is a Ruby library that makes it easy to build an HTTP server with Ruby.
  # It comes with most installations of Ruby by default (it’s part of the standard library),
  # so you can usually create a basic web/HTTP server with only several lines of code.
  #
  # The following code creates a generic WEBrick server on the local machine on port 1234
  class KineticServlet < WEBrick::HTTPServlet::AbstractServlet
    def do_GET (request, response)
      # a = request.query["a"]
      response.status = 200
      response.content_type = "text/plain"
      response.body = "Kinetic Fake Test Server"

      case request.path
      when "/admin"
        response.body += " - admin mode"
      when "/config"
        response.body += " - config mode"
      else
        response.body += " - normal mode"
      end
    end
  end

  class KineticTestServer

    def initialize(port=8213)
      @server = WEBrick::HTTPServer.new(:Port => port)
      @server.mount "/", KineticServlet
      trap("INT") do
        report "INT triggered Kintic Test Server shutdown"
        shutdown
      end
      @abort = false
      @worker = Thread.new do
        @server.start
        while !@abort do
          puts 'X'
        end
      end
    end

    def shutdown
      report_banner "Kinetic Test Server shutting down..."
      @abort = true
      @worker.join(2)
      @server.shutdown
      sleep(0.2)
      report "Kinetic Test Server shutdown complete"
    end

  end

  task :start do
    $test_server ||= KineticTestServer.new(8999)
  end

  task :shutdown do
    $test_server.shutdown unless $test_server.nil?
    $test_server = nil
  end

end

task :default => [
  'test_server:start',
  'test:all',
  'test_server:shutdown',
  'release'
]

desc "Run client test utility"
task :run do
  execute_command "./build/release/kinetic-c-client", "Running client test utility"
end

desc "Build all and run test utility"
task :all => [
  'cppcheck',
  'default',
  'run'
]

desc "Run full CI build"
task :ci => [
  'clobber',
  'all'
]


# This block of code will be run prior to Rake instance terminating
END {
  # Ensure test server is shutdown, so we can terminate cleanly
  $test_server.shutdown unless $test_server.nil?
  $test_server = nil
}
