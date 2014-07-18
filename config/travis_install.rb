require 'rake'

if ENV['TRAVIS_OS_NAME'] =~ /osx/i
  sh "brew install cppcheck"
else
  sh "sudo apt-get update"
  sh "sudo apt-get install -y cppcheck"
end
  
