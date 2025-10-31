# Define a "rubybuffer" command, bound to M-C-B, that
# runs the current buffer as a Ruby program.

require 'tempfile'
#require '/usr/local/share/pe/pe.rb'

def rubybuffer(n)
  file = Tempfile.new('perb')
  goto_line(1)
  keepgoing = true
  while keepgoing
    file.puts(E.line)
    keepgoing = forw_line == ETRUE
  end
  file.close
  load(file.path)
  file.unlink
  return ETRUE
end

ruby_command "rubybuffer"
E.bind "rubybuffer", metactrl('b')

# Run the event loop (only needed in RPC version of Ruby extensions).
#E.run
