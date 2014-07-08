PROJECT_CEEDLING_ROOT = "vendor/ceedling"
TEAMCITY_BUILD = !ENV['TEAMCITY_PROJECT_NAME'].nil?

load "#{PROJECT_CEEDLING_ROOT}/lib/ceedling/rakefile.rb"

def report(message='')
  puts message
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

task :clobber do
  report_banner "Cleaning out vendor directory"
  cd VENDOR_PATH do
    sh "git clean -f -d ."
  end
end

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

task :default => ['proto', 'cppcheck', 'test:all', 'release']

desc "Run client test utility"
task :run do
  execute_command "./build/release/kinetic-c-client", "Running client test utility"
end

desc "Build all and run test utility"
task :all => ['default', 'run']

desc "Run full CI build"
task :ci => ['clobber', 'all']
