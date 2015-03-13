
kinetic_c_version = `cat config/VERSION`.strip
commit_hash = `git rev-parse HEAD`.strip
protocol_version = nil
proto_source = 'src/lib/kinetic.pb-c.c'
File.readlines(proto_source).each do |l|
    m = l.match /^char com__seagate__kinetic__proto__local__protocol_version__default_value\[\]\s+=\s+\"(.*)\"/
    if m
      protocol_version = m[1].strip
      break
    end
end

raise "Unable to parse protocol version from: #{proto_source}" if protocol_version.nil?

info_header_file = "src/lib/kinetic_version_info.h"

header_contents = []
header_contents << "#ifndef _KINETIC_VERSION_INFO_H\n"
header_contents << "#define _KINETIC_VERSION_INFO_H\n"
header_contents << "#define KINETIC_C_VERSION \"#{kinetic_c_version}\"\n"
header_contents << "#define KINETIC_C_PROTOCOL_VERSION \"#{protocol_version}\"\n"
header_contents << "#define KINETIC_C_REPO_HASH \"#{commit_hash}\"\n"
header_contents << "#endif\n\n"

current_contents = []
current_contents = File.readlines(info_header_file) if File.exist?(info_header_file)

if (current_contents != header_contents)
  File.open(info_header_file, "w+") do |f|
      f.write(header_contents.join(""))
  end
  puts "Generated version info for kinetic-c"
  puts "------------------------------------"
  puts "header file:       #{info_header_file}"
  puts "kinetic-c version: #{kinetic_c_version}"
  puts "protocol version:  #{protocol_version}"
  puts "commit_hash:       #{commit_hash}"
else
  puts "Generated version info for kinetic-c is up to date!"
end
