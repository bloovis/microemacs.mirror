#!/usr/bin/env ruby

require 'json'


# Logging during debugging.

DEBUG = true
$logfile = nil

def dprint(s)
  if DEBUG
    unless $logfile
      $logfile = File.open("server.log", "w")
    end
    $logfile.puts s
    $logfile.flush
  end
end

# MicroEMACS functions return a trinary value.  We have
# to use an "E" prefix to avoid conflicts with Ruby's
# builtin constants.

EFALSE = 0
ETRUE  = 1
EABORT = 2


###
### Key -
### Encapsulate a keystroke in its own type so that
### it can be distinguished from a numeric argument
### to cmd.
###

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

###
### E - singleton class that provides an interface to MicroEMACS.
###

class E
  # The id that we supply with each message we send to the editor.
  # We increment it by 2 with each use, to avoid confusion with
  # ids from the editor, which are odd numbers.
  @@id = 2

  # Hash of name => bool, saying whether that name is
  # is valid MicroEMACS command.
  @@iscmd = {}

private

  # Trap an unknown method call for the E class.  We assume this is
  # a call to a MicroEMACS command.  First, replace underscores with
  # dashes in the method name.  Then query MicroEMACS to
  # see if this is a valid command.  If it is, send a method
  # call to MicroEMACS to run the command.
  def self.method_missing(m, *args, &block)
    #dprint "method_missing #{m}, has .name = #{m.respond_to?(:name)}"
    c = m.to_s.gsub('_','-')
    dprint "m = #{m.to_s}, c = #{c}"
    super(m, *args) unless E.iscmd(c)
    #dprint "Calling cmd #{m}"

    # Marshall the arguments.  Any or none can be specified, but
    # we assume there is only one of each type:
    # - int (numeric prefix),
    # - key object
    # - string
    f = 0
    n = 1
    k = Key::KRANDOM
    s = []
    args.each do |arg|
      #dprint "arg: #{arg} (#{arg.class})"
      case arg
      when Integer
	dprint "int arg = #{arg}"
	f = 1
	n = arg
      when Key
	dprint "key = #{arg.to_s}"
	k = arg.to_i
      when String
	#dprint "string = #{arg}"
	s << arg
      else
	#dprint "unknown arg type: method = #{m}, arg = #{arg}, class = #{arg.class}"
      end
    end
    #dprint "calling cmd(#{c}, #{f}, #{n}, #{k})"
    self.cmd(c, f, n, k, s)
  end

  # Send a hash as RPC message.
  def self.send_message(hash)
    hash["jsonrpc"] = "2.0"	# not actually needed, but insert it for completeness
    json = JSON.generate(hash);
    puts "#{json.length}"
    print json
    STDOUT.flush
    dprint "===\nSent #{json}"
  end

  # Read an RPC message, return it as a hash, or nil
  # if unrecoverable error occurred.
  def self.read_message
    # Read the line containing the size of the JSON payload,
    # then read the JSON.
    str = STDIN.gets
    unless str
      return nil
    end
    nbytes = str.to_i
    if nbytes > 0
      str = STDIN.read(nbytes)
    else
      # Empty response.  Maybe the pipe has been closed?
      return nil
    end
    dprint "===\nReceived #{str}"
    return JSON.parse(str)
  end

  # Read a JSON response, and if the ID matches, return the hash
  # representing the JSON.  If the ID doesn't match, or if
  # there's another error, return nil.
  def self.read_response
    # Read the JSON string and convert it to a hash.
    json = read_message
    unless json
      return nil
    end

    # Parse the JSON and determine if it indicates success or error.
    if error = json["error"]
      error_message = error["message"]
      dprint "error response #{error_message}!\n"
      return nil
    else
      result = json["result"].to_i
      id = json["id"].to_i
      if id != @@id
	dprint "response ID was #{id}, expected #{@@id}"
	return nil
      else
	dprint "Got expected response id #{id}"
	return json
      end
    end
  end

  # Process an incoming command, return true if successful,
  # false if unrecoverable error.
  def self.process_command
    # Read JSON message.
    json = read_message
    unless json
      return false
    end

    # Parse the JSON and extract the parameters.
    method = json["method"]
    unless params = json["params"]
      return false
    end
    flagstr = params["flag"]
    if flagstr
      flag = flagstr.to_i
    else
      flag = 0
    end
    prefixstr = params["prefix"]
    if prefixstr
      prefix = prefixstr.to_i
    else
      prefix = 0
    end
    keystr = params["key"]
    if keystr
      key = keystr.to_i
    else
      key = 0
    end
    id = json["id"].to_i
    strs = params["strings"]
    if strs
      strings = "[" + strs.join(", ") + "]"
    else
      strings = "[]"
      strs = []
    end
    dprint "command: id #{id}, method #{method}, flag #{flag}, prefix #{prefix}, key #{key}, strings #{strings}"

    # If there is a Ruby method that has the name of the commmand, call it
    # and return a normal response.  Otherwise return an error response.
    # The command method can send calls to the editor before it returns,
    # but those calls must not be reentrant, i.e., they must not call
    # back into the server.
    if respond_to?(method,true)
      exc = nil
      begin
        # Command can fetch the optionsl string array (strs) with yield.
	# We ignore the key parameter for now.  Maybe pass it to yield?
        ret = send(method, flag == 1 ? prefix : nil) { strs }
      rescue => x
        exc = x.message + "\n" + x.backtrace.join("\n")
      end
      if exc
	send_message({id: id, error: {code: -32000, message: "Exception:\n#{exc}"}})
	exc = nil
      else
	if ret.is_a?(Array)
	  result = ret[0]
	  msg = ret[1]
	else
	  result = ret
	  msg = ""
	end
	if result != ETRUE
	  send_message({id: id, error: {code: result, message: msg}})
	else
	  send_message({id: id, result: ETRUE, string: "success: id #{id}, method #{method}, message '#{msg}'"})
	end
      end
    else
      send_message({id: id, error: {code: -32601, message: "Method not found"}})
    end
    return true
  end

  # Get a string from the editor.
  # - name: the name of the string
  # - string: additional string parameter (could be nil)
  def self.get_string(name, string)
    send_message({method: "get", params: {name: name}, string: string, id: @@id})
    json = read_response
    @@id += 2
    return json.nil? ? "" : json["string"]
  end

  # Get an integer from the editor.
  # - name: the name of the string
  # - string: additional string parameter (could be empty)
  def self.get_int(name, string)
    send_message({method: "get", params: {name: name, string: string}, id: @@id})
    json = read_response
    @@id += 2
    return json.nil? ? 0 : json["result"]
  end

  def self.set(name, int, string)
    send_message({method: "set", params: {name: name, int: int, string: string}, id: @@id})
    read_response
    @@id += 2
  end

public

  # Getters and setters for objects inside the editor.
  # Getters can take an additional string parameter,
  # and setters can take an integer and a string parameter.
  # iscmd   -> get "iscmd", cmdname
  # reply   -> get "reply", string
  # getkey  -> get "getkey"
  # insert  -> set "insert, string
  # popup   -> set "popup", string
  # cbind   -> set "cbind", key, name
  # setmode -> set "setmode", int
  # bflag   -> set "bflag", int

  def self.line
    return get_string("line", "")
  end

  def self.line=(val)
    set("line", 0, val)
    @@id += 2
  end

  def self.lineno
    return get_int("lineno", "")
  end

  def self.lineno=(val)
    set("lineno", val, "")
  end

  ###
  ### Methods for running commands built into MicroEMACS.
  ###

  # Does the specified command exist in MicroEMACS?
  def self.iscmd(cmd)
    # Check the hash table first.  If nothing there,
    # send a message asking if this is a real command.
    result = @@iscmd[cmd]
    if result
      dprint "iscmd: got result #{result} from hash"
    else
      result = get_int("iscmd", cmd)
      @@iscmd[cmd] = result
      dprint "iscmd: stored result #{result} in hash"
    end
    return result == 1
  end

  # Run a MicroEMACS command, return true if successful.
  def self.cmd(name, flag, prefix, key, s)
    return unless self.iscmd(name)
    send_message({method: "cmd",
	params: {name: name,
		 flag: flag,
		 prefix: prefix,
		 key: key,
		 strings: s}, id: @@id})
    json = read_response
    dprint "self.cmd: response = #{json}"
    @@id += 2
    return !json.nil?
  end

  # Helper function that fetches the optional string parameter for a command.
  # Commands that use getstr must declare the block as a second parameter
  # named &b, then pass &b to getstr.
  def self.getstr
    if block_given?
      strs = yield
      if strs.length > 0
	return strs[0]
      end
    end
    return nil
  end

  # Main server loop.  Should run forever unless a fatal error occurs.
  def self.server_loop
    while process_command
    end
  end
end

# Non-command functions needed by MicroEMACS to run
# ruby "eval" and "load" statements.

def exec(n, &b)
  cmd = E.getstr(&b)
  if cmd
    eval(cmd)
    return [ETRUE, "exec ran '#{cmd}'"]
  else
    return [EFALSE, "exec called without a string"]
  end
end

def loadfile(n, &b)
  filename = E.getstr(&b)
  if filename
    load(filename)
    return [ETRUE, "loadfile loaded #{filename}"]
  else
    return [EFALSE, "loadfile called without a string"]
  end
end

# Some example MicroEMACS commands written in Ruby.  A command
# returns either a result code, or a tuple containing:
# - result code: ETRUE if successful, error code otherwise
# - message to display on status line (could be empty string)

def blorch(n)
  STDERR.puts "blorch: n #{n}}"
  return [EFALSE, "Blorch is unhappy about something"]
end

def stuff(n, &b)
  str = E.getstr(&b)
  if str
    E.echo "Stuff received #{str}"
    return [ETRUE, "stuff: str #{str}"]
  else
    return ETRUE
  end
end

def bad(n)
  # Try raising an exception.
  len = nil.length

  return [ETRUE, "bad"]
end

def callback(n)
  # Try an echo.
  E.echo("This is an echo test")

  result = E.goto_line 42
  dprint "goto_line returned #{result}"

  # Try getting and setting a string variable using E class methods.
  line = E.line
  dprint "line = '#{line}'"
  E.line = "New line!"
  line = E.line
  dprint "line after set = '#{line}'"

  # Try getting and setting an integer variable using E class methods.
  lineno = E.lineno
  dprint "lineno = #{lineno}"
  E.lineno = lineno + 10
  lineno = E.lineno
  dprint "lineno after set = #{lineno}"

  # We're done in this MicroEMACS function.  Return a success response.
  return [ETRUE, "callback"]
end

E.server_loop
