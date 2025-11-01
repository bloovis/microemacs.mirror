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

char rubyinit_error[100];	/* Buffer containing error message from rubyinit */

#define TEST 0

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
  printf("====\nSent %s\n", str);
  json_object_put(root);
}

/*
 * Make a JSON request for a call to an editor command.
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
 * Make a JSON object for a non-error response.
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
  json_object *jstring = json_object_new_string(string);
  json_object_object_add(root, "string", jstring);

  // id
  json_object *jid = json_object_new_int(id);
  json_object_object_add(root, "id", jid);
  return root;
}

/*
 * Make a JSON object for an error response.
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

int
init_server(const char *filename)
{
  const char *args[1];

  args[0] = NULL;
  server.id = 1;
  server.filename = strdup(filename);
  return openpipe (filename, args, &server.input, &server.output);
}


/*
 * Read a JSON message from the server, return the JSON
 * object representing it, or NULL if there's an error.
 */
json_object *
read_rpc_message(void)
{
  int nbytes, ret;
  char response[128];
  json_object *root;
  char *json = NULL;

  /* Read the response.
   */
  if (fgets (response, sizeof (response), server.input) == NULL)
    {
      printf("Unable to read line from %s\n", server.filename);
      return NULL;
    }
  if ((ret = sscanf(response, "%d", &nbytes)) == 1)
    {
      if (nbytes > 0)
	{
	  json = (char *)malloc(nbytes + 1);
	  int nread;

	  if ((nread = fread(json, 1, nbytes, server.input)) != nbytes)
	    {
	      printf("Tried reading %d bytes, but read %d instead\n", nbytes, nread);
	      free(json);
	      return NULL;
	    }
	  else
	    {
	      json[nbytes] = '\0';
	      printf("====\nReceived %s\n", json);
	    }
	}
      else
	{
	  printf("Unexpected zero-length response\n");
	  return NULL;
	}
    }
  else
    {
      printf("sscanf returned %d\n", ret);
      return NULL;
    }
  if (json == NULL)
    {
      printf("Unable to read JSON\n");
      return NULL;
    }

  /* Construct a JSON object representing the string we just read. */
  //printf("About to convert '%s' to json object\n", json);
  root = json_tokener_parse(json);
  //printf("Converted '%s' to json object, result is %p\n", json, root);
  free(json);
  return root;
}

int
is_result(json_object *msg)
{
  json_object *value;
  return json_object_object_get_ex(msg, "result", &value) == 1;
}

int
is_call(json_object *msg)
{
  json_object *value;
  return json_object_object_get_ex(msg, "method", &value) == 1;
}

int
is_error(json_object *msg)
{
  json_object *value;
  return json_object_object_get_ex(msg, "error", &value) == 1;
}

const char *
get_string(json_object *jobj, const char *name)
{
  json_object *value;

  if (json_object_object_get_ex(jobj, name, &value) != 1)
    {
      printf("Unable to get %s from JSON\n", name);
      return NULL;
    }
  return json_object_get_string(value);
}

int
get_int(json_object *jobj, const char *name)
{
  json_object *value;

  if (json_object_object_get_ex(jobj, name, &value) != 1)
    {
      printf("Unable to get %s from JSON\n", name);
      return 0;
    }
  return json_object_get_int(value);
}

const char *
get_nth_string(json_object *jobj, int i)
{
  json_object *element = json_object_array_get_idx(jobj, i);
  if (element == NULL)
    {
      printf("Unable to fetch %dth element of array\n", i);
    }
  return json_object_get_string(element);
}

int
handle_cmd( int id, json_object *params)
{
  const char *name;
  int flag, prefix, key;
  json_object *strings;
  char buf[128];

  // name
  name = get_string(params, "name");
  printf("name: %s\n", name);

  // flag
  flag = get_int(params, "flag");
  printf("flag: %d\n", flag);

  // prefix
  prefix = get_int(params, "prefix");
  printf("prefix: %d\n", prefix);

  // key
  key = get_int(params, "key");
  printf("key: %d\n", key);

  // strings
  buf[0] = '\0';
  if (json_object_object_get_ex(params, "strings", &strings) != 1)
    printf("No strings in request.  That's OK.\n");
  else
    {
      int i;
      int n = json_object_array_length(strings);

      for (i = 0; i < n; i++)
	{
	  const char *str = get_nth_string(strings, i);
	  replyq_put (str);
	}
    }
  /* Have to do something real here.  Let's pretend we did call the method.
   */
  if (strcmp (name, "echo") == 0)
    eecho(flag, prefix, key);

  /* Send a response if this is not a notification.
   */
  if (id == 0)
    printf("This is a notification, no response required.\n");
  else
    {
      json_object *response;

      /* The caller expects a response. Send it. */
      printf("Caller requires response.\n");
      snprintf(buf, sizeof(buf) - 1, "handle_command: ran %s", name);
      response = make_normal_response(0, buf, id);
      send_message(response);
    }
  return TRUE;
}

static struct
{
  int lineno;
  const char *line;
} vars = {42, "old line"};

int
handle_set(int id, json_object *params)
{
  json_object *response;

  // name
  const char *name = get_string(params, "name");
  printf("handle_set: name: %s\n", name);
  if (strcmp(name, "line") == 0)
    {
      vars.line = get_string(params, "string");
      response = make_normal_response(0, "", id);
      send_message(response);
      return TRUE;
    }
  else if (strcmp(name, "lineno") == 0)
    {
      vars.lineno = get_int(params, "int");
      response = make_normal_response(0, "", id);
      send_message(response);
      return TRUE;
    }
  else
    {
      response = make_error_response(-32602, "no such variable", id);
      send_message(response);
      return TRUE;
    }

  return FALSE;
}

int
handle_get(int id, json_object *params)
{
  json_object *response;

  // name
  const char *name = get_string(params, "name");
  printf("handle_get: name: %s\n", name);
  if (strcmp(name, "line") == 0)
    {
      response = make_normal_response(0, vars.line, id);
      send_message(response);
      return TRUE;
    }
  else if (strcmp(name, "lineno") == 0)
    {
      response = make_normal_response(vars.lineno, "", id);
      send_message(response);
      return TRUE;
    }
  else if (strcmp(name, "iscmd") == 0)
    {
      const char *cmd = get_string(params, "string");
      if (cmd != NULL && (strcmp(cmd, "goto-line") == 0 || strcmp(cmd, "echo") == 0))
	response = make_normal_response(TRUE, "found", id);
      else
	response = make_normal_response(FALSE, "not found", id);
      send_message(response);
      return TRUE;
    }
  else
    {
      response = make_error_response(-32602, "no such variable", id);
      send_message(response);
      return TRUE;
    }
}

int
handle_call(json_object *root)
{
  const char *method;
  json_object *params;
  int id;

  method = get_string(root, "method");
  if (method == NULL) {
    printf("Unable to get method from JSON\n");
    return FALSE;
  }
  id = get_int(root, "id");
  printf("handle_call: method %s, id %d\n", method, id);

  // params
  if (json_object_object_get_ex(root, "params", &params) != 1)
    {
      printf("Unable to get params from JSON\n");
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
 * Parse a result object, store the result code to *resultp.
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
handle_result(json_object *root, int *resultp)
{
  int id;
  const char *string;

  *resultp = get_int(root, "result");
  id = get_int(root, "id");

  string = get_string(root, "string");
  if (string == NULL)
    string = "<none>";
  printf("handle_result: id %d, result %d, string '%s'\n", id, *resultp, string);

  /* If this result matches the method call we sent earlier,
   * return false, saying we're done and return to the editor.
   */
  return id != server.id;
}

int
handle_error(json_object *root)
{
  int id, code;
  json_object *error;
  const char *message;

  printf("--- handle_error ---\n");
  id = get_int(root, "id");
  printf("id: %d\n", id);

  if (json_object_object_get_ex(root, "error", &error) != 1)
    {
      printf("Unable to get error from JSON\n");
      return FALSE;
    }
  code = get_int(error, "code");
  printf("code: %d\n", code);
  message = get_string(error, "message");
  printf("message: %s\n", message);
  return FALSE;
}

/*
 * Call a method in the server, and return the result code it sends back.
 */
int
call_server(const char *method, int flag, int prefix, int key, int nstrings,
            const char *strings[])
{
  json_object *root;
  int keep_going = TRUE;
  int result = FALSE;

  server.id += 2;
  root = make_rpc_request(method, flag, prefix, key, nstrings, strings, server.id); /* was "stuff" */
  send_message(root);

  /* Loop reading responses from the server.  There maybe one or more
   * method calls from the server before it sends a response for
   * the method call we just sent it.
   */
  while (keep_going == TRUE)
    {
      json_object *msg = read_rpc_message();
      if (msg == NULL)
	{
	  printf("Couldn't read RPC message\n");
	  break;
	}

      /* It can be one of three types of messages:
       * - normal response
       * - error response
       * - method call
       */
      if (is_result(msg))
	keep_going = handle_result(msg, &result);
      else if (is_error(msg))
	keep_going = handle_error(msg);
      else if (is_call(msg))
	keep_going = handle_call(msg);
      else
	printf("Unrecognized message.\n");
    }
  printf("call_server: returning %d\n", result);
  return result;
}

int
rubyinit (int quiet)
{
  static const char *filename = STRINGIFY(PREFIX) "/share/pe/server.rb";

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
  else
    return TRUE;
}

int
rubyloadscript (const char *path)
{
  eprintf ("[rubyloadscript for RPC not implemented]");
  return FALSE;
}

int
runruby (const char * line)
{
  eprintf ("[runruby for RPC not implemented]");
  return FALSE;
}

int
rubycall (const char *name, int f, int n)
{
  eprintf ("[rubycall for RPC not implemented]");
  return FALSE;
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
      printf("usage: rpc server (e.g., server.rb)\n");
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
