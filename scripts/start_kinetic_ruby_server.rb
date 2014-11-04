require 'kinetic-ruby'


server = KineticRuby::Server.new(KineticRuby::DEFAULT_KINETIC_PORT)
puts "Kinetic Ruby Simulator waiting for a connection..."

trap("SIGINT") do
  puts
  puts "Shutting down..."
  server.shutdown
  exit 0
end

while(!server.connected) do
  sleep(0.25)
end
puts "Kinetic Ruby Simulator connected!"
while(server.connected)
  sleep(1)
end


