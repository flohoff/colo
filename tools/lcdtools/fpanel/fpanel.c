/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include <stdlib.h>

#include <lua50/lua.h>
#include <lua50/lualib.h>
#include <lua50/lauxlib.h>

#include <readline/readline.h>
#include <readline/history.h>

int main(int argc, char *argv[])
{
	const char *line;
	lua_State *L;
	int indx;

	L = lua_open();
	luaopen_base(L);
	luaopen_table(L);
	luaopen_io(L);
	luaopen_string(L);
	luaopen_math(L);

	for(;;)
	{
		line = readline("> ");
		if(!line)
			break;

		for(indx = 0; line[indx] && line[indx] < ' '; ++indx)
			;

		if(line[indx])
		{
			add_history(line);

			if(luaL_loadbuffer(L, line, strlen(line), "line") || lua_pcall(L, 0, 0, 0))
			{
				fprintf(stderr, "%s\n", lua_tostring(L, -1));

				lua_pop(L, 1);
			}
		}

		free((void *) line);
	}

	return 0;
}

/* vi:set ts=3 sw=3 cin: */
