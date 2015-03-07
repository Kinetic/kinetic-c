require 'cmock'

HERE = File.expand_path(File.join(File.dirname(__FILE__), '../'))
mock_out = File.join(HERE, 'build/test/mocks')
cmock = CMock.new(plugins: [:ignore, :return_thru_ptr], mock_path: mock_out)
headers_to_mock = Dir['src/lib/*.h']
puts "\nCreating mocks from header files...\n-----------------------------------"
cmock.setup_mocks(headers_to_mock)
puts "\nGenerated Mocks:\n----------------"
Dir["#{mock_out}/mock_*.h"].each{|mock| puts mock}
