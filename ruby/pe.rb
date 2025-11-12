# This is the helper file for Ruby support in MicroEMACS.
# As root, copy it to /usr/local/share/pe/pe.rb
# (create the directory /usr/local/share/pe if necessary).

# MicroEMACS functions return a trinary value.  We have
# to use an "E" prefix to avoid conflicts with Ruby's
# builtin constants.

EFALSE = 0
ETRUE  = 1
EABORT = 2

# The variable $bflag (the current buffer's flags) is an OR
# of the following values.

BFCHG = 1	# changed
BFBAK = 2	# need backup
BFRO  = 4	# read-only

# Trap SIGINT so we can cleanly cause Ruby to
# exit instead of killing MicroEMACS.

Signal.trap('INT') do
 raise "Received SIGINT signal"
end

# Encapsulate a keystroke in its own type so that
# it can be distinguished from a numeric argument
# to cmd.

class Key

  CTRL = 0x10000000
  META = 0x20000000
  CTLX = 0x40000000
  CHARMASK = 0x10ffff
  KRANDOM = 0x80

  def initialize(c, f=0)
    if c.is_a?(String)
      if f != 0
	@key = c.upcase.ord | f
      else
	@key = c.ord
      end
    else
      @key = c | f
    end
  end

  def to_i
    @key
  end

  def char
    (@key & CHARMASK).chr('UTF-8')
  end

  def ctrl?
    (@key & CTRL) != 0
  end

  def meta?
    (@key & META) != 0
  end

  def ctlx?
    (@key & CTLX) != 0
  end

  def normal?
    (@key & (CTRL | META | CTLX)) == 0
  end

  def ==(k)
    self.to_i == k.to_i
  end

  def to_s
    s = ""
    if (@key & CTLX) != 0
      s << "C-X "
    end
    if (@key & META) != 0
      s << "M-"
    end
    if (@key & CTRL) != 0
      s << "C-"
    end
    if self.normal?
      s << self.char
    else
      s << self.char.upcase
    end
  end

end

def key(c)
  Key.new(c)
end

def ctrl(c)
  Key.new(c, Key::CTRL)
end

def meta(c)
  Key.new(c, Key::META)
end

def ctlx(c)
  Key.new(c, Key::CTLX)
end

def metactrl(c)
  Key.new(c, Key::META | Key::CTRL)
end

def ctlxctrl(c)
  Key.new(c, Key::CTLX | Key::CTRL)
end

# Editor class to encapsulate global variables and helper functions
# defined in ruby.c.

class E

  ###
  ### C helper function wrappers.
  ###

  # Run a MicroEMACS command.
  def self.cmd(c, f, n, k, s)
    e_cmd(c, f, n, k, s)
  end

  # Is c a MicroEMACS command?
  def self.iscmd(c)
    e_iscmd(c)
  end

  # Insert a string at the cursor.
  def self.insert(s)
    e_insert(s)
  end

  # Pop up a window showing a message.
  def self.popup(s)
    e_popup(s)
  end

  # We have to define our own equivalent of bindtokey
  # instead of using the command built into MicroEMACS,
  # because the latter prompts for a keystroke.
  # Parameters:
  #   name: string containing the function name
  #   key:  keycode
  #   mode: (optional) true if binding to mode, false if global (default)
  def self.bind(name, key, mode=false)
    e_cbind(name, key.to_i, mode)
  end

  # Prompt the user with a string, return the answer they types.
  def self.reply(s)
    e_reply(s)
  end

  # Get a keystroke from the user, return it packaged in a Key object.
  def self.getkey
    Key.new(e_cgetkey, 0)
  end

  # Set the current buffer's mode.
  def self.setmode(s)
    e_setmode(s)
  end

  # Get the current time as a Time object.
  def self.timenow
    e_timenow
  end

  # Convert a string to a symbol.
  def self.sym2str(s)
    e_sym2str(s)
  end

  # Run the event loop (only used in RPC version of Ruby extensions).
  def self.run
  end

  ###
  ### Global variable getters and setters.
  ###

  # Get current line number (1-based).
  def self.lineno
    $e_lineno
  end

  # Set line number (move to that line).
  def self.lineno=(n)
    $e_lineno = n
  end

  # Get current line.
  def self.line
    $e_line
  end

  # Set current line.
  def self.line=(s)
    $e_line = s
  end

  # Get current offset into line.
  def self.offset
    $e_offset
  end

  # Change current offset into line.
  def self.offset=(n)
    $e_offset = n
  end

  # Get character under cursor.
  def self.char
    $e_char
  end

  # Change character under cursor.
  def self.char=(c)
    $e_char = c
  end

  # Get current buffer's filename.
  def self.filename
    $e_filename
  end

  # Change current buffer's filename.
  def self.filename=(s)
    $e_filename = s
  end

  # Get current tab size.
  def self.tabsize
    $e_tabsize
  end

  # Save current tab size.
  def self.tabsize=(n)
    $e_tabsize = n
  end

  # Get current paragraph fill column.
  def self.fillcol
    $e_fillcol
  end

  # Set current paragraph fill column.
  def self.fillcol=(n)
    $e_fillcol = n
  end

  # Get current buffer's flags (see BF* constants).
  def self.bflag
    $e_bflag
  end

  # Set current buffer's flags.
  def self.bflag=(n)
    $e_bflag = n
  end

  # Get current buffer's name (NOT filename!).
  def self.bname
    $e_bname
  end

  # Set current buffer's name (NOT filename!).
  def self.bname=(s)
    $e_bname = s
  end

  # Check if an unknown method is MicroEMACS function.  If so,
  # marshall its various arguments and call it; otherwise pass
  # the exception on, which typically aborts the currently
  # running Ruby program.
  #
  # In order to invoke a MicroEMACS command, method_missing
  # uses two C helper functions:
  #
  # iscmd(c) - returns true if the name c is a MicroEMACS command
  # cmd(c, f, n, k, s) - invoke a MicroEMACS command:
  #   c = name of command
  #   f = argument flag (0 if no argument present, 1 if argument present)
  #   n = argument (integer)
  #   k = keystroke (only looked at by selfinsert)
  #   s = array of strings to be fed to eread

  def self.method_missing(m, *args, &block)
    #STDERR.puts "method_missing #{m}, has .name = #{m.respond_to?(:name)}"
    c = m.to_s # was sym2str(m)
    #STDERR.puts "c = #{c.inspect}"
    super(m, *args) unless E.iscmd(c)
    #STDERR.puts "Calling cmd #{m}"
    f = 0
    n = 1
    k = Key::KRANDOM
    s = []
    args.each do |arg|
      case arg
      when Integer
	# printf "arg = 0x%x\n", arg
	f = 1
	n = arg
      when Key
	# puts "key = #{arg.to_s}"
	k = arg.to_i
      when String
	s << arg
      else
	puts "unknown arg type: method = #{m}, arg = #{arg}, class = #{arg.class}"
      end
    end
    #STDERR.puts "calling cmd(#{c}, #{f}, #{n}, #{k})"
    self.cmd(c, f, n, k, s)
  end

end

# A mode is a record containing a name and a set of key bindings;
# this record is attached to a particular buffer.  Key bindings
# in a mode override those in the global key binding table.
#
# MicroEMACS calls initmode whenever a new buffer is opened.
# It uses the following two stratagies to determine the name
# of the mode for the buffer:
#
#   1. Look at the first non-blank line, and if it contains
#      the string -*-MODE-*-, MODE specifies the mode name
#   2. If strategy #1 fails, look at the filename, and see if
#      it matches any of the patterns in the $modetable below
#
# If a mode name is determined, run the function MODE_mode,
# if present, where MODE is the mode name.  The user must provide
# one or more of these functions to implement the desired modes.
#
# A mode function will typically perform any necessary buffer
# manipulation, then define one or more MicroEMACS functions,
# and finally bind some keys to that mode.  See dired.rb
# for an example.

# Table associating filename patterns to mode names.
$modetable = [
  [ /\.c$/,   "c" ],
  [ /\.rb$/,  "ruby" ],
  [ /\.cc$/,  "cplusplus" ],
  [ /\.cpp$/, "cplusplus" ],
  [ /\.cr$/,  "crystal" ],
  [ /\.sh$/,  "shell" ]
]

# This function is called whenever a file is read into a buffer.
# It attempts to determine the mode for the file, and then
# call the function associated with that mode.

def initmode(n)
  mode = nil
  f = E.filename

  # Try to determine the mode from the first non-blank line
  # in the file, which could contain a mode specification like this:
  #   -*-Mode-*-
  # The mode is lower-cased before checking for the hook function,
  lineno = E.lineno
  #linelen = E.line.length
  offset = E.offset
  E.goto_bob
  keepgoing = true
  while keepgoing
    l = E.line.chomp
    if l =~ /^\s*$/	# skip leading blank lines
      keepgoing = E.forw_line == ETRUE
    else
      keepgoing = false
      if l =~ /-\*-(\w+)-\*-/
	mode = $1.downcase
      end
    end
  end
  E.lineno = lineno
  E.offset = offset

  # Try to determine the mode from the filename pattern.
  if mode.nil?
    match = $modetable.find {|a| f =~ a[0] }
    if match
      mode = match[1]
    end
  end

  # If a mode hook function exists, call it
  if mode.nil?
    # echo "[unable to determine mode]"
    return
  end
  hook = mode + "_mode"
  if Object.respond_to?(hook, true)
    Object.send hook
  else
    # echo "[unknown mode hook #{hook}]"
  end
end

# Set the default encoding for strings.
Encoding.default_internal = 'UTF-8'
