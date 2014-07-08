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
PROTO_IN = File.expand_path(File.join(HERE, 'vendor', 'kinetic-protocol'))
PROTO_OUT = File.join(HERE, 'build', 'temp', 'proto')
directory PROTO_OUT

desc "Generate protocol buffers"
task :proto => [PROTO_OUT] do
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
