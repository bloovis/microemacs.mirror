# Ruby Extensions

It is possible to extend MicroEMACS by writing commands in Ruby.
First, install the Ruby development packages (ruby-dev or ruby-devel).
Then specify the `--with-ruby` flag to `configure` when you
build MicroEMACS.  For example:

    mkdir objruby
    cd objruby
    ../configure --with-ruby
    make
    sudo make install # optional step

The resulting MicroEMACS is not linked directly with the Ruby runtime
library.  Instead, it loads the Ruby library dynamically as needed.
This allows you to copy the MicroEMACS executable (`pe`) to a system where Ruby is not
available, and it will still run, but without Ruby support.

A Ruby-enabled MicroEMACS will work only on a system that has the same
version of Ruby as the build system.  You will need to rebuild it for
each system that has a different version of Ruby.


## Initialization

In order for Ruby commands to run correctly, you will need to
copy a helper file called `pe.rb`, located in the `ruby` subdirectory
of the source code, to the directory `/usr/local/share/pe`. Running `sudo make install` after
building a Ruby-enabled MicroEMACS will perform the copy.
MicroEMACS will attempt to load the helper file when it starts.
If it cannot load the file, you will not be able to use the Ruby
extensions.

At startup, MicroEMACS will also attempt to load an initialization
script that you have written.  It will first look for a file called `.pe.rb`
(note the leading dot) in your home directory, and load that if found.  If it can't find `.pe.rb`
in your home directory, it will look in your current directory.  This
will allow you to define your own custom commands on a per-user or
per-project basis, as appropriate.  The `ruby` subdirectory
of the source code has examples that you can use or modify.

## An example

Before delving into details about how to write commands in Ruby,
let's look at an example.  Here is a file called `gccerr.rb`
that implements a command to parse gcc compiler errors and go to
the relevant lines of code.  This is essentially a rewrite of
the built-in **gcc-error** command:

```

def gccerr(n)
  keepgoing = true
  while keepgoing
    l = E.line
    if (l !~ /^In file included/ && l =~ /(.*):(\d+):(\d+): (.*)/)
      file = $1
      lno = $2
      col = $3
      err = $4
      if File.exist? file
	E.forw_line
	E.only_window
	E.split_window
	E.forw_window
	E.file_visit file
	E.goto_line lno.to_i
	E.forw_char col.to_i - 1
	E.echo "#{err}"
        return ETRUE
      else
	E.echo "File #{file} does not exist"
        return EFALSE
      end
    end
    keepgoing = E.forw_line == ETRUE
  end
  E.echo "No more gcc errors"
  return EFALSE
end

E.ruby_command "gccerr"
E.bind "gccerr", metactrl('e')
```

Some things to note about this example:

* It defines a new command called **gccerr**.

* Like all MicroEMACS commands written in Ruby, it takes a single parameter,
  which is the optional numeric argument.

* The new command invokes several built-in MicroEMACS functions, passing
  numeric parameters in some cases.

* It invokes built-in MicroEMACS commands, using their names with dashes replaced
  by underscores, and prefixed with "E.".  For example, it invokes the `file-visit` command by calling
  the `E.file_visit` method.  Use the "E." prefix for all MicroEMACS functions
  and Ruby-accessible variables.

* It uses the global variable `E.line` to get the contents of the current line.

* It checks the status of the **forw-line** command by comparing it with
  the constant `ETRUE`, which corresponds to the constant `TRUE` in the
  C source code of MicroEMACS.

* After the definition of the command, there is code to tell MicroEMACS about
  the new command, and to bind it to the `M-C-E` key.

* If you want MicroEMACS to load this script automatically when it starts,
  rename it to `.pe.rb` and copy it either to your current directory or
  your home directory.

## Ruby-related commands

MicroEMACS has several built-in commands related to Ruby extensions:

**F6** (**ruby-string**)

This command prompts you to enter a line of Ruby code.
MicroEMACS then passes the line to the Ruby interpreter.
One common use of this command is to load a file containing
Ruby code for a new command.  For example, to load the code for
the `gccerr` command described above, you could enter this command
to `ruby-string`:

    load 'PATH/gccerr.rb'

where you would replace PATH with the actual directory containing
`gccerr.rb`.

**[unbound]** (**ruby-command**)

This command prompts you to enter the name of a Ruby function
that implements a new command.  MicroEMACS then enters
the command into its symbol table but does not bind it to a key;
you can use the `bind-to-key` command for that.  The `gccerr`
example above shows a use of this command.

**[unbound]** (**ruby-load**)

This command prompts the user for the name of a Ruby script,
then loads that script.  This is shortcut that has the same
effect as using **ruby-string** and a `load` Ruby statement.

## Calling Built-in Commands from Ruby

Ruby code can call built-in MicroEMACS commands (written in C) by
invoking them as normal functions, but with the '-' characters
in the names replaced by '_', and with an "E." prefix.  For example, invoke the **forw-char**
command by calling `E.forw_char`.

You can pass an optional numeric parameter to a built-in command.
For example, to move the dot forward by 8 characters, use this code:

```
E.forw_char 8
```

Some commands prompt the user for one or more strings.  You can
supply these strings to a command by passing them as parameters.
For example, to replace all occurrences of `Windows` to `Linux`
in the current buffer, use this code:

```
E.replace_string "Windows", "Linux"
```

Some built-in commands prompt the user for a keystroke.  Two examples
are **help** and **bind-to-key**.  These commands will not work as
expected when invoked from Ruby, because as of this writing there is not
a way to pass keycodes as additional parameters to commands.

Microemacs provides a `bind` helper function to work around
the problem with the **bind-to-key** command.  For example, the `gccerr.rb`
code above used this helper to bind the **gccerr** command to
the **M-C-E** key:

    E.bind "gccerr", metactrl('e')

Commands return a trinary value indicating success, failure, or abort.
In Ruby, these values are:

`ETRUE` : The command succeeded.

`EFALSE` : The command failed.  For example, `forw_line` returns `EFALSE`
if the dot is already at the last line, as we can see in the
`gccerr` example above.

`EABORT` : The command was aborted by Control-G.

The **echo** command is useful when debugging Ruby code.  It displays
a string on the echo line, so you can use it to display debug
messages.  For example, this code displays the current line number:

    E.echo "line number is #{E.lineno}"

## Defining Commands in Ruby

You can create a new command in Ruby by first defining a function
that takes a single numeric parameter.  This parameter gives the
numeric argument that the user typed as a prefix (using **C-U**).  If the
user didn't specify a numeric argument, the parameter will be `nil`.
The function must return an EFALSE or ETRUE value to indicate
failure or success, respectively.

Then use **E.ruby_command** to inform
MicroEMACS of the new command.

Referring to the `gccerr.rb` example above, we can see that
the code first defines a new command function:

```
def gccerr(n)
  .. ruby code ...
  return ETRUE
  ...
  return EFALSE
end
```

Then it tells MicroEMACS about the new command:

```
E.ruby_command "gccerr"
```

Finally, it binds the new command to the **M-C-E** key:

```
E.bind "gccerr", metactrl('e')
```

### Helper Functions

MicroEMACS provides several helper functions
for use in Ruby commands.

`E.insert(string)`

This function inserts the value of the `string` parameter into the current
buffer at the dot. The string may contain newline characters, which are treated as
line breaks.

`E.setmode(name)`

This function deletes the current buffer's mode, if any.
It then creates a mode called `name`, with an empty key binding table, and attaches
it to the buffer.  See the [Modes](modes.md) section for
more information about modes.

`E.bind(name, key, mode=false)`

This function binds the command whose name is the string `name`
to the keycode `key`.  If the `mode` parameter is present, and is
`true`, the binding is attached to the current buffer's mode, if any.
Otherrwise, the binding is made global, i.e., available in
all buffers. See below for the helper functions
that provide keycodes.

`E.reply(string)`

This function prompts the user on the echo line with the specified
string, then reads an input line from the user.  It returns the input
line without a terminating newline, or nil if the user aborts
the input using Control-G.

`E.getkey`

This function waits for the user to enter a keystroke, then returns
a **Key** object describing the keystroke.  See the next section
for a description of the **Key** object.

`E.popup(string)`

This function creates a pop-up window, with the contents specified
by the `string` parameter.  The string may contain newline characters.
This function is useful for displaying error messages in a temporary
window.

### Keycodes

MicroEMACS also provides several helpers for encoding keycodes.
All built-in commands in MicroEMACS take a keycode parameter, which
contains the key that invoked the command.  You can specify the keycode
by passing it as a parameter when calling the command.
As of this writing, the only command that looks at the keycode is **ins-self**.
Given that fact, the following example inserts an 'x' character in to the current buffer:

``
E.ins_self key('x')
```

The `bind` helper function, described above, also takes a keycode parameter.

Keycodes can be specified using one of the following helper functions.
These helpers all take a single parameter, which is an ordinary ASCII character.

* `key`: specifies an ordinary, unmodified character.  For example,
  `key('c')` means the character 'c'.

* `ctrl`: specifies a control character.  For example,
  `ctrl('c')` means **C-C** (Control-C).

* `meta`: specifies a meta character. For example,
  `meta('c')` means **M-C** (Escape C or Alt-C).

* `ctlx`: specifies a character with the `C-X` prefix.  For example,
  `ctlx('c')` means **M-X C** (Control-X C).

* `metactrl`: specifies a combination of `meta` and `ctrl`.  For example,
  `metactrl('c')` means **M-C-C** (Escape Control-C).

* `ctlxctrl`: specifies a combination of `ctlx` and `ctrl`.  For example,
  `ctlxctrl('c')` means **C-X C-C** (Control-X Control-C).

These helpers all return an object of the class **Key**.  This object contains the raw keycode
as used internally by MicroEMACS, and provides methods for examining the keycode.
Here are the **Key** methods:

`ctrl?` : Returns true if the key is a control key.


`meta?` : Returns true if the key is a meta key (i.e., has an Escape prefix).


`ctlx?` : Returns true if the key is a Control-X key (i.e., has a Control-X prefix).


`normal?` : Returns true if the key is a "normal" key (i.e., is not a control, meta,
or Control-X key).

`to_i` : Returns the key's raw keycode.

`char` : Returns the normal character portion of the keycode, without
any control, meta, or Control-X flags.  As an example, the
`char` of the Control-G keycode is the character 'G'.

`to_s` : Returns a human readable string for the keycode.  As an example,
the `to_s` of the Control-G keycode is 'C-G'.

### Global variables

MicroEMACS provides several global virtual variables that may be both read
and written in Ruby code.

`E.line`

This variable contains the current line (the line containing the dot),
with a newline character appended if this is not the last line in the
buffer.  Writing to this variable causes the current line to be replaced
with the specified string.  A newline at the end of the string is removed,
but newlines at other positions in the string are left unchanged and cause
line breaks.

`E.char`

This variable contains the character at the dot.  Writing to this variable
replaces the character at the dot with the specified string (which can
be of any length).

`E.lineno`

This variable contains the line number of the line containing the dot.
The value is 1-based, for compatibility with the **goto-line** function.
Writing to this variable causes the dot to be moved to the specified line.

`E.offset`

This variable contains the offset into the current line of the dot.
The value is 0-based, so that it can be used as an index into `E.line`.
Writing to this variable moves the dot to the specified offset within
the current line.

`E.filename`

This variable contains the current buffer's filename.  Writing to this
variable changes the current buffer's filename.

`E.tabsize`

This variable contains the current tab width.  Writing to this variable
sets the tab width, as in the **set-tab-size** command.

`E.fillcol`

This variable contains the current fill column for paragraph justification.  Writing to this variable
sets the fill column, as in the **set-fill-column** command.

`E.bflag`

This variable contains the current buffer's flags, and can be read or written.
The flags are an OR of these values: BFCHG (buffer has changed), BFBAK (buffer
needs a backup), and BFRO (buffer is read-only).  For example, this code:

```
E.bflag &= ~BFCHG
```

turns off the "buffer changed" flag.  This is a dangerous operation, because
it could result in data loss.

## Exceptions

If an exception occurs in Ruby code, MicroEMACS will open a temporary
window containing the exception information, including a backtrace.

In the unlikely event that the Ruby interpreter crashes with a segfault,
it prints complete exception information to the terminal, but the output is difficult to
read because MicroEMACS puts the terminal into "raw" mode.
If you need to see the exception information, you can restart MicroEMACS
with stderr redirected to a file:

```
pe 2>ruby.log
```

Then, if you can reproduce the crash, the file `ruby.log` will contain
the exception information.

## Aborting Ruby Commands

If your Ruby code is taking too long to run, and you want to stop it,
you will need to send it a signal from another terminal window.  If you
are using the [RPC implementation](#rpc) of Ruby extensions (i.e., you configured
MicroEMACS with `--with-ruby=rpc`), find the ID of the Ruby server
process by using a command such as this:

```
ps -x | grep ruby
```

If you are using the original implementation of Ruby extensions
(i.e., you configured MicroEMACS with `--with-ruby` or
`--with-ruby=VERSION`, find the ID of the MicroEMACS process by using a
command such as this:

```
ps -x | grep pe
```

Then using the process ID that you discovered, kill the Ruby
code using:

```
kill -SIGINT <id>
```

The helper code in `pe.rb` and `server.rb` catches this signal and raises an exception that
aborts the errant Ruby code and return control to MicroEMACS.

<span id="rbenv">
## Using rbenv
</span>

If you are using a version of Ruby that you installed with
[rbenv](https://github.com/rbenv/rbenv#readme),
you can configure MicroEMACS to use that version
of Ruby.  As example, let's assume that you used rbenv to install Ruby 3.1.2.
To configure MicroEMACS to use this version of Ruby,
create an `obj` directory, move into it, and do this:

```
../configure --with-ruby=3.1.2
make
```

Replace 3.1.2 with the actual version of Ruby that
you installed with `rbenv`.

Examine the resulting `Makefile` to be sure that the correct
version of Ruby is used in compilation and linking.

<span id="rpc">
## Ruby RPC Implementation
</span>

There may be situations where you are unable to use the standard Ruby
extension implementation, either because the Ruby development packages
(ruby-dev or ruby-devel) are not available, or you choose not to use them.
In those cases, you can use the RPC implementation of Ruby extensions.  This
works by invoking Ruby as a separate process and communicating with it via pipes, using the
[JSON-RPC protocol](https://www.jsonrpc.org/specification) as the message format.  Although this RPC
implementation makes some extensions work much slower than in the C
API implementation, it's not so terrible as to be unuable, at least in
the extensions I use on a daily basis.

To build a Ruby with the RPC implementation, use a configure command like this:

```
../configure --with-ruby=rpc
make
```

Then as root, copy the file `ruby/server.rb` to the directory `/usr/local/share/pe`.
Create the directory using `mkdir -p` if it doesn't already exist.
