/*
    Copyright (C) 2025 Mark Alexander

    This file is part of MicroEMACS, a small text editor.

    MicroEMACS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<unistd.h>
#include	<signal.h>
#include	<fcntl.h>
#include	<json-c/json.h>

#include	"def.h"

static int rubyinit_called;	/* TRUE if rubyinit has been called */
char rubyinit_error[100];	/* Buffer containing error message from rubyinit */

#define TEST 0			/* Enable test program. */
#define DEBUG 0			/* Enable debug log. */

/* JSON-RPC error codes
 */
#define ERROR_METHOD    -32601	/* Method not found */
#define ERROR_PARAMS    -32602	/* Invalid params */
#define ERROR_EXCEPTION -32000	/* Server error - exception */

/* Information about the server: its pipe handles
 * and the ID of the last message sent to it.
 */
struct
{
  const char *filename;
  FILE *input;
  FILE *output;
  int id;
} server;


/*
 * dprintf - print a line to the debug log.
 */
static void
dprintf(const char *fmt, ...)
{
#if DEBUG
  static FILE *logfile;		/* Log file handle. */
  va_list ap;

  if (logfile == NULL)
    {
      logfile = fopen("rubyrpc.log", "w");
      if (logfile == NULL)
	return;
    }
  va_start (ap, fmt);
  vfprintf (logfile, fmt, ap);
  fflush (logfile);
  va_end (ap);
#endif
}

/*
 * send_message - send a JSON object to the server
 *
 * The JSON object follows the JSON-RPC model, with a few
 * extra fields to support needed functionality.  Send
 * it to the server in two pieces:
 * - a line containing the size of the JSON payload in decimal
 * - the JSON payload itself
 */
void
send_message(json_object *root)
{
  const char *str;
  int nbytes;

  str = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
  nbytes = strlen(str);
  fprintf(server.output, "%d\n", nbytes);
  if (nbytes > 0)
    {
      fwrite(str, 1, nbytes, server.output);
      fflush(server.output);
    }
  dprintf("====\nSent %s\n", str);
  json_object_put(root);
}

/*
 * make_rpc_request - make a JSON request for a call to a Ruby command
 *
 * method: name of command
 * flag: 1 if command was preceded by a C-u numeric prefix
 * prefix: numeric prefix (undefined if flag is 0)
 * key: editor internal keycode that invoked the command
 * nstrings: number of strings in the strings array (could be 0)
 * strings: array of strings to pass to the command
 * id: unique request ID
 *
 * Example JSON:
 *   {"jsonrpc": "2.0",
 *     "method": "stuff",
 *      "params": {"flag": 1, "prefix": 42, "key": 9, "strings": ["a string"]  }, "id": 1}
 */
json_object *
make_rpc_request(
	const char *method,
	int flag,
	int prefix,
	int key,
	int nstrings,
	const char *strings[],
	int id)
{
  int i;
  json_object *root = json_object_new_object();
  if (!root)
     return NULL;

  // "jsonrpc": "2.0"
  json_object *version = json_object_new_string("2.0");
  json_object_object_add(root, "jsonrpc", version);

  // "method": "stuff"
  json_object *jmethod = json_object_new_string(method);
  json_object_object_add(root, "method", jmethod);

  // "params": ...
  json_object *jparams = json_object_new_object();
  json_object_object_add(root, "params", jparams);

  // "flag": 1
  json_object *jflag = json_object_new_int(flag);
  json_object_object_add(jparams, "flag", jflag);

  // "prefix": 42
  json_object *jprefix = json_object_new_int(prefix);
  json_object_object_add(jparams, "prefix", jprefix);

  // "key": 9
  json_object *jkey = json_object_new_int(key);
  json_object_object_add(jparams, "key", jkey);

  // "strings": [...]
  json_object *jstrings = json_object_new_array();
  json_object_object_add(jparams, "strings", jstrings);

  for (i = 0; i < nstrings; i++)
    {
      json_object *string1 = json_object_new_string(strings[i]);
      json_object_array_add(jstrings, string1);
    }

  // "id": 3
  json_object *jid = json_object_new_int(id);
  json_object_object_add(root, "id", jid);
  return root;
}

/*
 * make_normal_response - make a JSON object for a non-error response.
 *
 * result: result code
 * string: additional optional string result
 * id: unique request ID
 *
 * Example JSON:
 *   {"id":4,"result":0,"string":"success: id 4, method callback"}
 */
json_object *
make_normal_response(
	int result,
	const char *string,
	int id)
{
  json_object *root = json_object_new_object();
  if (!root)
     return NULL;

  // "jsonrpc": "2.0"
  json_object *version = json_object_new_string("2.0");
  json_object_object_add(root, "jsonrpc", version);

  // result
  json_object *jresult = json_object_new_int(result);
  json_object_object_add(root, "result", jresult);

  // "string": ...
  if (string == NULL)
    json_object_object_add(root, "string", NULL);
  else
    {
      json_object *jstring = json_object_new_string(string);
      json_object_object_add(root, "string", jstring);
    }

  // id
  json_object *jid = json_object_new_int(id);
  json_object_object_add(root, "id", jid);
  return root;
}

/*
 * make_error_response - make a JSON object for an error response.
 *
 * nstrings: number of strings in the strings array (could be 0)
 * strings: array of strings to pass to the caller
 * id: unique request ID
 *
 * Example JSON:
 *   {"id":4,"error":0,"strings":["success: id 4, method callback"]}
 */
json_object *
make_error_response(
	int code,
	const char *message,
	int id)
{
  json_object *root = json_object_new_object();
  if (!root)
     return NULL;

  // "jsonrpc": "2.0"
  json_object *version = json_object_new_string("2.0");
  json_object_object_add(root, "jsonrpc", version);

  // "error:" {...
  json_object *error = json_object_new_object();
  json_object_object_add(root, "error", error);

  // "code": ...
  json_object *jcode = json_object_new_int(code);
  json_object_object_add(error, "code", jcode);

  // "message": ...
  json_object *jmessage = json_object_new_string(message);
  json_object_object_add(error, "message", jmessage);

  // "id": 3
  json_object *jid = json_object_new_int(id);
  json_object_object_add(root, "id", jid);
  return root;
}

/*
 * init_server - intialize the Ruby server
 *
 * Open a pipe to the server executable (server.rb).
 * Return TRUE if successful, FALSE otherwise.
 */
int
init_server(const char *filename)
{
  const char *args[2];

  args[0] = filename;
  args[1] = NULL;
  server.id = 1;
  server.filename = strdup(filename);
  return openpipe (filename, args, &server.input, &server.output);
}


/*
 * read_rpc_message - read a JSON message from the server
 *
 * Read the JSON payload in two pieces:
 * - a line containing the size of the JSON payload in decimal
 * - the JSON payload itself
 * Return the JSON object representing it, or NULL if there's an error.
 */
json_object *
read_rpc_message(void)
{
  int nbytes, ret;
  char response[128];
  json_object *root;
  char *json = NULL;

  /* Read the line containing the size of the JSON.
   */
  if (fgets (response, sizeof (response), server.input) == NULL)
    {
      dprintf("Unable to read line from %s\n", server.filename);
      return NULL;
    }
  dprintf("====\nReceived size line:\n%s\n", response);
  if ((ret = sscanf(response, "%d", &nbytes)) == 1)
    {
      if (nbytes > 0)
	{
	  /* Read the JSON payload. */
	  json = (char *)malloc(nbytes + 1);
	  int nread;

	  if ((nread = fread(json, 1, nbytes, server.input)) != nbytes)
	    {
	      dprintf("Tried reading %d bytes, but read %d instead\n", nbytes, nread);
	      free(json);
	      return NULL;
	    }
	  else
	    {
	      json[nbytes] = '\0';
	      dprintf("Received %d bytes of json:\n%s\n", nbytes, json);
	    }
	}
      else
	{
	  dprintf("Unexpected zero-length response\n");
	  return NULL;
	}
    }
  else
    {
      dprintf("sscanf returned %d\n", ret);
      return NULL;
    }
  if (json == NULL)
    {
      dprintf("Unable to read JSON\n");
      return NULL;
    }

  /* Construct a JSON object representing the string we just read. */
  //dprintf("About to convert '%s' to json object\n", json);
  root = json_tokener_parse(json);
  //dprintf("Converted '%s' to json object, result is %p\n", json, root);
  free(json);
  return root;
}

/*
 * is_result - is the JSON object a result message?
 */
int
is_result(json_object *msg)
{
  json_object *value;
  return json_object_object_get_ex(msg, "result", &value) == 1;
}

/*
 * is_call - is the JSON object a method call message?
 */
int
is_call(json_object *msg)
{
  json_object *value;
  return json_object_object_get_ex(msg, "method", &value) == 1;
}

/*
 * is_error - is the JSON object an error message?
 */
int
is_error(json_object *msg)
{
  json_object *value;
  return json_object_object_get_ex(msg, "error", &value) == 1;
}

/*
 * get_string - get a string member from a JSON object
 */
const char *
get_string(json_object *jobj, const char *name)
{
  json_object *value;

  if (json_object_object_get_ex(jobj, name, &value) != 1)
    {
      dprintf("Unable to get %s from JSON\n", name);
      return NULL;
    }
  return json_object_get_string(value);
}

/*
 * get_int - get an integer member from a JSON object
 */
int
get_int(json_object *jobj, const char *name)
{
  json_object *value;

  if (json_object_object_get_ex(jobj, name, &value) != 1)
    {
      dprintf("Unable to get %s from JSON\n", name);
      return 0;
    }
  return json_object_get_int(value);
}

/*
 * get_nth_string - get the nth string member from a JSON array object
 */
const char *
get_nth_string(json_object *jobj, int i)
{
  json_object *element = json_object_array_get_idx(jobj, i);
  if (element == NULL)
    {
      dprintf("Unable to fetch %dth element of array\n", i);
    }
  return json_object_get_string(element);
}

/*
 * handle_cmd - process a request to run a MicroEMACS command
 */
int
handle_cmd( int id, json_object *params)
{
  const char *name;
  int flag, prefix, key, result;
  json_object *strings, *response;
  char buf[128];
  SYMBOL *sp;

  // name
  name = get_string(params, "name");
  dprintf("name: %s\n", name);

  // flag
  flag = get_int(params, "flag");
  dprintf("flag: %d\n", flag);

  // prefix
  prefix = get_int(params, "prefix");
  dprintf("prefix: %d\n", prefix);

  // key
  key = get_int(params, "key");
  dprintf("key: %d\n", key);

  // strings
  buf[0] = '\0';
  if (json_object_object_get_ex(params, "strings", &strings) != 1)
    dprintf("No strings in request.  That's OK.\n");
  else
    {
      int i;
      int n = json_object_array_length(strings);

      /* Put the strings in the reply queue, so that ereply
       * will pick them up without prompting.
       */
      for (i = 0; i < n; i++)
	{
	  const char *str = get_nth_string(strings, i);
	  replyq_put (str);
	}
    }

  /* Look up the command in the symbol table.
   */
  sp = symlookup (name);
  if (sp == NULL)
    {
      eprintf ("Unknown command %s", name);
      result = FALSE;
    }
  else
    {
      /* We found the command in the symbol table.
       * If it's a macro, replay the macro.
       * If it's a command, call it.
       */
      result = FALSE;
      if (sp->s_macro)
	{
	  if (kbdmip != NULL || kbdmop != NULL)
	    {
	      eprintf ("Not now");
	      result = FALSE;
	    }
	  else
	    result = domacro (sp->s_macro, 1);
	}
      else
	{
	  startsaveundo ();
	  dprintf("handle_cmd: running %s\n", name);
	  result = sp->s_funcp (flag, prefix, key);
	  endsaveundo ();
	}

      /* Send a response, unless it's not a notification (indicated
       * by a zero id) that expects no response.
       */
      if (id != 0)
	{
	  dprintf("handle_cmd: ran %s, result %d\n", name, result);
	  snprintf(buf, sizeof(buf) - 1, "handle_cmd: ran %s, result %d", name, result);
	  response = make_normal_response(result, buf, id);
	  send_message(response);
	}
    }
  return TRUE;	/* keep reading messages from the server. */
}

/*
 * popup - pop up a temporary MicroEMACS window
 *
 * Pop up the temporary window (the so-called "blist" or buffer list), and
 * write the message to it.  The message may contain multiple lines,
 * separate by newline characters (\n).
 */
static int
popup (const char *message)
{
  char *copy;
  char *start, *end;

  /* Clear the popup buffer.
   */
  blistp->b_flag &= ~BFCHG;
  if (bclear (blistp) != TRUE)
    return FALSE;
  strcpy (blistp->b_fname, "");

  /* Split the string into lines and write each one to the popup buffer.
   * In Ruby this would be: message.split("\n").each {|l| addline(l)}
   * Instead, we use a horrible kludge where we copy the string,
   * and work through it, changing each \n to a zero.
   */
  copy = strdup(message);
  if (copy == NULL)
    return FALSE;
  start = copy;
  end = start + strlen(copy);
  while (start < end)
    {
      char *newline = strchr (start, '\n');
      if (newline == NULL)
	newline = end;
      *newline = '\0';
      addline (start);
      start = newline + 1;
    }
  free (copy);

  /* Display the popup buffer.
   */
  return popblist ();
}
  
/*
 * set_* - set MicroEMACS variables
 *
 * These functions implement requests from the Ruby server to perform
 * "set" operations on virtual variables MicroEMACS.  Some of these
 * operations do more that setting a variable, e.g., inserting a
 * string
 */
json_object *
set_line (int id, json_object *params)
{
  const char *line = get_string(params, "string");
  ruby_setline (line);
  return make_normal_response(0, "", id);
}

json_object *
set_lineno (int id, json_object *params)
{
  int new_lineno = get_int(params, "int");
  gotoline (TRUE, new_lineno, KRANDOM);
  return make_normal_response(0, "", id);
}

json_object *
set_bind (int id, json_object *params)
{
  const char *str;
  int key;

  key = get_int(params, "int");
  str = get_string(params, "string");
  if (str == NULL)
    return make_error_response(ERROR_PARAMS, "missing command name", id);
  else
    {
      /* This is a hack: the name has a prefix "T" or "T", indicating
       * the value of mode.
       */
      int mode = str[0] == 'T';
      const char *cmd = &str[1];
      SYMBOL *sp;

      if ((sp = symlookup (cmd)) == NULL)
	{
	  eprintf ("%s: no such command", cmd);
	  return make_error_response(ERROR_METHOD, "no such command", id);
	}
      else
	{
	  if (mode)
	    setmodebinding (key, sp);
	  else
	    setbinding (key, sp);
	  return make_normal_response(0, "", id);
	}
    }
}

json_object *
set_bflag (int id, json_object *params)
{
  curbp->b_flag = get_int(params, "int");
  curwp->w_flag |= WFMODE;
  return make_normal_response(0, "", id);
}

json_object *
set_insert (int id, json_object *params)
{
  const char *str = get_string (params, "string");
  if (str == NULL)
    return make_error_response(ERROR_PARAMS, "missing insert string", id);
  else
    {
      int result = insertwithnl (str, strlen (str));
      return make_normal_response(result, "", id);
    }
}

json_object *
set_offset (int id, json_object *params)
{
  int offset = get_int(params, "int");
  if (offset > wllength (curwp->w_dot.p))
    eprintf ("Offset too large");
  else
    curwp->w_dot.o = offset;
  return make_normal_response(0, "", id);
}

json_object *
set_mode (int id, json_object *params)
{
  const char *str = get_string (params, "string");
  if (str == NULL)
    return make_error_response (ERROR_PARAMS, "missing mode string", id);
  else
    {
      createmode (str);
      return make_normal_response(0, "", id);
    }
}

json_object *
set_filename (int id, json_object *params)
{
  const char *str = get_string(params, "string");
  replyq_put (str);
  filename (FALSE, 0, KRANDOM);
  return make_normal_response(0, "", id);
}

json_object *
set_popup (int id, json_object *params)
{
  const char *str = get_string(params, "string");
  popup (str);
  return make_normal_response(0, "", id);
}

/* Table of setter functions
 */
typedef json_object *(*handler)(int id, json_object *params);

#define NSETTERS 9

struct
{
  const char *name;
  handler func;
} setters [NSETTERS] =
{
  { "line",     set_line },
  { "lineno",   set_lineno },
  { "bind",     set_bind },
  { "bflag",    set_bflag },
  { "insert",   set_insert },
  { "offset",   set_offset },
  { "mode",     set_mode },
  { "filename", set_filename },
  { "popup",    set_popup },
};

/*
 * handle_set - handle a set command
 *
 * The Ruby server uses a "set" message to ask MicroEMACS to perform
 * tasks that are not commands.  These can set variables
 * like the current line number or the current line, but
 * can also perform other actions, like prompting the
 * user for a replay, or inserting text.
 */
int
handle_set(int id, json_object *params)
{
  json_object *response = NULL;
  int i;
  const char *name = get_string(params, "name");

  dprintf("handle_set: name: %s\n", name);

  for (i = 0; i < NSETTERS; i++)
    {
      if (strcmp (setters[i].name, name) == 0)
	{
	  response = (setters[i].func)(id, params);
	  send_message (response);
	  return TRUE;
	}
    }

  response = make_error_response (ERROR_PARAMS, "no such variable", id);
  send_message(response);
  return TRUE;
}

/*
 * get_* - get MicroEMACS variables
 *
 * These functions implement requests from the Ruby server to
 * "get" virtual variables in MicroEMACS.  Some of these
 * operations do more that getting a variable, e.g., testing
 * that a particular MicroEMACS command exists.
 */
json_object *
get_line (int id, json_object *params)
{
  char *line = ruby_getline ();
  json_object *response = make_normal_response(0, line, id);
  free (line);
  return response;
}

json_object *
get_lineno (int id, json_object *params)
{
  return make_normal_response(lineno (curwp->w_dot.p) + 1, "", id);
}

json_object *
get_iscmd (int id, json_object *params)
{
  const char *cmd = get_string(params, "string");

  if (cmd != NULL && symlookup(cmd) != NULL)
    return make_normal_response(TRUE, "found", id);
  else
    return make_normal_response(FALSE, "not found", id);
}

json_object *
get_reply (int id, json_object *params)
{
  int result;
  char buf[NCOL];
  const char *prompt = get_string(params, "string");

  if (prompt != NULL)
    {
      result = ereply ("%s", buf, sizeof (buf), prompt);
      return make_normal_response(result, result == ABORT ? NULL : buf, id);
    }
  else
    return make_error_response(ERROR_PARAMS, "missing reply prompt", id);
}

json_object *
get_bflag (int id, json_object *params)
{
  return make_normal_response(curbp->b_flag, "", id);
}

json_object *
get_offset (int id, json_object *params)
{
  return make_normal_response(curwp->w_dot.o, "", id);
}

json_object *
get_filename (int id, json_object *params)
{
  return make_normal_response(0, curbp->b_fname, id);
}

json_object *
get_key (int id, json_object *params)
{
  return make_normal_response(getkey (), "", id);
}

#define NGETTERS 8

struct
{
  const char *name;
  handler func;
} getters [NGETTERS] =
{
  { "line",     get_line },
  { "lineno",   get_lineno },
  { "iscmd",    get_iscmd },
  { "reply",    get_reply },
  { "bflag",    get_bflag },
  { "offset",   get_offset },
  { "filename", get_filename },
  { "key",      get_key },
};

/*
 * handle_get - handle a set command
 *
 * The Ruby server uses a "get" message to ask MicroEMACS to return
 * some internal values
 */
int
handle_get(int id, json_object *params)
{
  json_object *response;
  int i;
  const char *name = get_string(params, "name");

  dprintf("handle_get: name: %s\n", name);

  for (i = 0; i < NGETTERS; i++)
    {
      if (strcmp (getters[i].name, name) == 0)
	{
	  response = (getters[i].func)(id, params);
	  send_message (response);
	  return TRUE;
	}
    }

  response = make_error_response(ERROR_METHOD, "no such variable", id);
  send_message(response);
  return TRUE;
}

/*
 * handle_call - handle an RPC method call from the Ruby server
 *
 * There are three types of method calls we can receive:
 * - cmd - run a MicroEMACS command
 * - set - set a MicroEMACS virtual variable
 * - get - get a MicroEMACS virtual variable
 */
int
handle_call(json_object *root)
{
  const char *method;
  json_object *params;
  int id;

  method = get_string(root, "method");
  if (method == NULL) {
    dprintf("Unable to get method from JSON\n");
    return FALSE;
  }
  id = get_int(root, "id");
  dprintf("handle_call: method %s, id %d\n", method, id);

  // params
  if (json_object_object_get_ex(root, "params", &params) != 1)
    {
      dprintf("Unable to get params from JSON\n");
      return FALSE;
    }

  if (strcmp(method, "cmd") == 0)
    return handle_cmd(id, params);
  else if (strcmp(method, "set") == 0)
    return handle_set(id, params);
  else if (strcmp(method, "get") == 0)
    return handle_get(id, params);
  else
    {
      json_object *response = make_normal_response(1, "Method not found" , id);
      send_message(response);
      return FALSE;
    }
}

/*
 * handle_result - parse a result object
 *
 * Parse a result object from the server, and store the result code to *resultp.
 *
 * Return a flag saying whether we should keep reading messages:
 *
 * Return true if we didn't see the expected result message (i.e, the
 * the ID didn't match the method call we just made), meaning we
 * should keep reading messages.
 *
 * Return false if we did see the expected result message, meaning
 * we can stop reading messages.
 */
int
handle_result(json_object *root, int expected_id, int *resultp)
{
  int id;
  const char *string;

  *resultp = get_int(root, "result");
  id = get_int(root, "id");

  string = get_string(root, "string");
  if (string == NULL)
    string = "<none>";
  dprintf("handle_result: id %d, expected id %d, result %d, string '%s'\n", id, expected_id, *resultp, string);

  /* If this result matches the method call we sent earlier,
   * return false, saying we're done and return to the editor.
   */
  return id != expected_id;
}

/*
 * handle_error - parse an error object
 *
 * Parse an error object from the server.
 *
 * Return false to tell the caller that we can stop reading messages.
 */
int
handle_error(json_object *root)
{
  int id, code;
  json_object *error;
  const char *message;

  dprintf("--- handle_error ---\n");
  id = get_int(root, "id");
  dprintf("id: %d\n", id);

  if (json_object_object_get_ex(root, "error", &error) != 1)
    {
      dprintf("Unable to get error from JSON\n");
      return FALSE;
    }
  code = get_int(error, "code");
  dprintf("code: %d\n", code);
  message = get_string(error, "message");
  dprintf("message: %s\n", message);
  if (code == ERROR_EXCEPTION)
    popup (message);
  return FALSE;
}

/*
 * call_server - call a method in the server
 *
 * Request the server to run a method, which must have the signature
 * of a MicroEMACS command as written in Ruby. Return the result code it sends back.
 */
int
call_server(const char *method, int flag, int prefix, int key, int nstrings,
            const char *strings[])
{
  json_object *root;
  int keep_going = TRUE;
  int result = FALSE;
  int id = server.id;

  server.id += 2;
  root = make_rpc_request(method, flag, prefix, key, nstrings, strings, id);
  send_message(root);

  /* Loop reading responses from the server.  There maybe one or more
   * method calls from the server before it sends a response for
   * the method call we just sent it.
   */
  while (keep_going == TRUE)
    {
      dprintf ("call_server: waiting for a message\n");
      json_object *msg = read_rpc_message();
      if (msg == NULL)
	{
	  dprintf("Couldn't read RPC message\n");
	  break;
	}

      /* It can be one of three types of messages:
       * - normal response
       * - error response
       * - method call
       */
      if (is_result(msg))
	keep_going = handle_result(msg, id, &result);
      else if (is_error(msg))
	keep_going = handle_error(msg);
      else if (is_call(msg))
	keep_going = handle_call(msg);
      else
	dprintf("Unrecognized message.\n");
    }

  dprintf("call_server: returning %d from method %s\n", result, method);
  return result;
}

/*
 *****
 *  Public functions provided by both Ruby implementations.
 *****
 */

/*
 * rubyinit - intialize the Ruby server
 *
 * Spawn the Ruby server and open a pipe to it.  Then load the optional
 * Ruby helpers (.pe.rb) if present.  Return TRUE if successful, FALSE otherwise.
 */
int
rubyinit (int quiet)
{
  static const char *filename = STRINGIFY(PREFIX) "/share/pe/server.rb";

  /* If we've been called before, return the status from that call.
   */
  if (rubyinit_called)
    return strlen(rubyerror()) == 0;
  rubyinit_called = TRUE;

  /* Load the server script.
   */
  if (access (filename, R_OK) != F_OK)
    {
      return rubyinit_set_error ("The file %s does not exist; cannot initialize Ruby",
				 filename);
      return FALSE;
    }
  if (init_server(filename) == FALSE)
    {
      rubyinit_set_error ("rubyinit unable to connect to server %s", filename);
      return FALSE;
    }

  /* Load ~/.pe.rb or ./pe.rb
   */
  return ruby_loadhelpers ();
}

/*
 * runruby - evaluate a line of Ruby code
 */
int
runruby (const char * line)
{
  const char *exec_strings[1];

  exec_strings[0] = line;
  return call_server("exec", 1, 42, 9, 1, exec_strings);
}

/*
 * rubyloadscript - load a Ruby script
 */
int
rubyloadscript (const char *path)
{
  char buf[1024];

  if (access (path, R_OK) != F_OK)
    {
      eprintf ("The file %s does not exist.", path);
      return FALSE;
    }
  snprintf (buf, sizeof(buf) - 1, "load '%s'", path);
  return runruby (buf);
}

/*
 * rubycall - run a Ruby function.
 */
int
rubycall (const char *name, int f, int n)
{
  const char *strings[1];
  int result;

  dprintf ("rubycall: calling %s\n", name);
  result = call_server(name, f, n, 0, 0, strings);
  return result;
}

#if TEST

int
main(int argc, const char *argv[])
{
  const char *filename;
  server p;
  static const char *req_strings[2] = {"string one", "string two"};
  static const char *exec_strings[1] = {"stuff(42) { ['exec strings parameter'] }"};
  static const char *exec_strings_no_str[1] = {"stuff(42)"};
  static const char *loadfile_strings[1] = {"../ruby/extra.rb"};

  if (argc < 2)
    {
      dprintf("usage: rpc server (e.g., server.rb)\n");
      exit(1);
    }
  filename = argv[1];
  if (init_server(&p, filename) == FALSE)
    return 1;


  /* Send some messages to the server to evince various
   * types of responses.
   */
  call_server(&p, "stuff",    1, 42, 9, 2, req_strings);
  call_server(&p, "stuff",    1, 42, 9, 0, req_strings);
  call_server(&p, "bad",      1, 42, 9, 2, req_strings);
  call_server(&p, "blorch",   1, 42, 9, 2, req_strings);
  call_server(&p, "blotz",    1, 42, 9, 2, req_strings);
  call_server(&p, "callback", 1, 42, 9, 0, req_strings);
  call_server(&p, "exec",     1, 42, 9, 1, exec_strings);
  call_server(&p, "exec",     1, 42, 9, 1, exec_strings_no_str);
  call_server(&p, "loadfile", 1, 42, 9, 1, loadfile_strings);
  call_server(&p, "extra",    1, 42, 9, 1, req_strings);
  return 0;
}

#endif	/* TEST */
