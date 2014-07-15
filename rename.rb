require 'rake'
# require 'fileutils'
# include FileUtils

Dir['./src/**/*.c', './src/**/*.h', './test/**/*.c'].each do |full_old|
  old = File.basename(full_old)
  old_dir = File.dirname(full_old)
  m = old.match(/^(t?e?s?t?)_?([A-Z][a-z0-9]+)([A-Z][A-Za-z0-9]+)(\.[ch])/)
  if m.nil?
    puts "Skipping #{old}!"
  else
    segs = []
    m[1..-2].each{|s| segs << s.downcase unless s.empty?}
    suffix = m[-1]
    renamed = segs.join('_') + suffix
    full_new = File.join(old_dir, renamed)
    sh "git mv #{full_old} #{full_new}"
    puts "old: #{full_old}\t\t=>\t\t#{full_new})" # (from #{segs})"
  end
end
