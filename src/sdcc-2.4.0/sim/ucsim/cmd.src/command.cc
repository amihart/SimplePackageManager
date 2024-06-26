/*
 * Simulator of microcontrollers (cmd.src/command.cc)
 *
 * Copyright (C) 2002,02 Drotos Daniel, Talker Bt.
 * 
 * To contact author send email to drdani@mazsola.iit.uni-miskolc.hu
 *
 */

/* This file is part of microcontroller simulator: ucsim.

UCSIM is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

UCSIM is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with UCSIM; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA. */
/*@1@*/

#include "ddconfig.h"

#include <stdlib.h>
#include "i_string.h"

// prj

// local, cmd
#include "commandcl.h"


/*
 * Command line
 *____________________________________________________________________________
 */

cl_cmdline::cl_cmdline(class cl_app *the_app,
		       char *acmd, class cl_console *acon):
  cl_base()
{
  app= the_app;
  cmd= strdup(acmd);
  params= new cl_list(2, 2);
  tokens= new cl_ustrings(2, 2);
  set_name(0);
  matched_syntax= 0;
  con= acon;
}

cl_cmdline::~cl_cmdline(void)
{
  if (cmd)
    free(cmd);
  delete params;
  delete tokens;
}

int
cl_cmdline::init(void)
{
  split();
  return(0);
}

char *
cl_cmdline::skip_delims(char *start)
{
  while (*start &&
	 strchr(" \t\v\r,", *start))
    start++;
  return(start);
}

int
cl_cmdline::split(void)
{
  //class cl_sim *sim;
  char *start= cmd;
  int i;
  class cl_cmd_arg *arg;

  //sim= app->get_sim();
  set_name(0);
  if (!cmd ||
      !*cmd)
    return(0);
  start+= strspn(start, " \t\v\r,");
  if (start &&
      *start == '\n')
    {
      char *n= (char*)malloc(2);
      strcpy(n, "\n");
      set_name(n);
      return(0);
    }
  if (!*start)
    return(0);
  i= strcspn(start, " \t\v\r,");
  if (i)
    {
      char *n= (char*)malloc(i+1);
      strncpy(n, start, i);
      n[i]= '\0';
      set_name(n);
    }
  start+= i;
  start= skip_delims(start);
  // skip delimiters
  while (*start)
    {
      char *end, *param_str;
      if (*start == '"')
	{
	  // string
	  start++;
	  end= start;
	  while (*end &&
		 *end != '"')
	    {
	      if (*end == '\\')
		{
		  end++;
		  if (*end)
		    end++;
		}
	      else
		end++;
	    }
	  if (*end == '"')
	    end--;
	  else
	    con->dd_printf("Unterminated string\n");
	  param_str= (char *)malloc(end-start+2);
	  strncpy(param_str, start, 1+end-start);
	  param_str[1+end-start]= '\0';
	  tokens->add(strdup(param_str));
	  params->add(arg= new cl_cmd_str_arg(param_str));
	  arg->init();
	  free(param_str);
	  if (*end)
	    end++;
	  if (*end == '"')
	    end++;
	}
      else
	{
	  char *dot;
	  i= strcspn(start, " \t\v\r,");
	  end= start+i;
	  param_str= (char *)malloc(i+1);
	  strncpy(param_str, start, i);
	  param_str[i]= '\0';
	  if (strchr(">", *start))
	    {
	      char *fn, mode[3]= "w\0";
	      switch (*start)
		{
		case '>':
		  if (i == 1 ||
		      (i == 2 &&
		       start[1] == '>')) {
		    con->dd_printf("Unspecified redirection\n");
		    break;
		  }
		  fn= start+1;
		  if (*fn == '>') {
		    fn++;
		    mode[0]= 'a';
		  }
		  con->redirect(fn, mode);
		  break;
		default:
		  break;
		}
	      free(param_str);
	      start= end;
	      start= skip_delims(start);
	      continue;
	    }
	  tokens->add(strdup(param_str));
	  if ((dot= strchr(param_str, '.')) != NULL)
	    {
	      // bit
	      class cl_cmd_arg *sfr, *bit;
	      *dot= '\0';
	      dot++;
	      if (strchr("0123456789", *param_str) != NULL)
		{
		  sfr= new cl_cmd_int_arg((long)strtol(param_str, 0, 0));
		  sfr->init();
		}
	      else
		{
		  sfr= new cl_cmd_sym_arg(param_str);
		  sfr->init();
		}
	      if (*dot == '\0')
		{
		  bit= 0;
		  con->dd_printf("Uncomplete bit address\n");
		  delete sfr;
		}
	      else
		{
		  if (strchr("0123456789", *dot) != NULL)
		    {
		      bit= new cl_cmd_int_arg((long)strtol(dot, 0, 0));
		      bit->init();
		    }
		  else
		    {
		      bit= new cl_cmd_sym_arg(dot);
		      bit->init();
		    }
		  params->add(arg= new cl_cmd_bit_arg(sfr, bit));
		  arg->init();
		}
	    }
	  else if ((dot= strchr(param_str, '[')) != NULL)
	    {
	      // array
	      class cl_cmd_arg *aname, *aindex;
	      *dot= '\0';
	      dot++;
	      if (strchr("0123456789", *param_str) != NULL)
		{
		  aname= new cl_cmd_int_arg((long)strtol(param_str, 0, 0));
		  aname->init();
		}
	      else
		{
		  aname= new cl_cmd_sym_arg(param_str);
		  aname->init();
		}
	      if (*dot == '\0')
		{
		  aname= 0;
		  con->dd_printf("Uncomplete array\n");
		}
	      else
		{
		  char *p;
		  p= dot + strlen(dot) - 1;
		  while (p > dot &&
			 *p != ']')
		    {
		      *p= '\0';
		      p--;
		    }
		  if (*p == ']')
		    *p= '\0';
		  if (strlen(dot) == 0)
		    {
		      con->dd_printf("Uncomplete array index\n");
		      delete aname;
		    }
		  else    
		    {
		      if (strchr("0123456789", *dot) != NULL)
			{
			  aindex= new cl_cmd_int_arg((long)strtol(dot, 0, 0));
			  aindex->init();
			}
		      else
			{
			  aindex= new cl_cmd_sym_arg(dot);
			  aindex->init();
			}
		      params->add(arg= new cl_cmd_array_arg(aname, aindex));
		      arg->init();
		    }
		}
	    }
	  else if (strchr("0123456789", *param_str) != NULL)
	    {
	      // number
	      params->add(arg= new cl_cmd_int_arg((long)
						  strtol(param_str, 0, 0)));
	      arg->init();
	    }
	  else
	    {
	      // symbol
	      params->add(arg= new cl_cmd_sym_arg(param_str));
	      arg->init();
	    }
	  free(param_str);
	}
      start= end;
      start= skip_delims(start);
    }
  return(0);
}

int
cl_cmdline::shift(void)
{
  char *s= skip_delims(cmd);

  set_name(0);
  if (s && *s)
    {
      while (*s &&
	     strchr(" \t\v\r,", *s) == NULL)
	s++;
      s= skip_delims(s);
      char *p= strdup(s);
      free(cmd);
      cmd= p;
      delete params;
      params= new cl_list(2, 2);
      split();
    }
  return(have_real_name());
}

int
cl_cmdline::repeat(void)
{
  char *n;
  return((n= get_name()) &&
	 *n == '\n');
}

class cl_cmd_arg *
cl_cmdline::param(int num)
{
  if (num >= params->count)
    return(0);
  return((class cl_cmd_arg *)(params->at(num)));
}

void
cl_cmdline::insert_param(int pos, class cl_cmd_arg *param)
{
  if (pos >= params->count)
    params->add(param);
  else
    params->add_at(pos, param);
}

bool
cl_cmdline::syntax_match(class cl_uc *uc, char *syntax)
{
  if (!syntax)
    return(DD_FALSE);
  if (!*syntax &&
      !params->count)
    {
      matched_syntax= syntax;
      return(DD_TRUE);
    }
  if (!params->count)
    return(DD_FALSE);
  //printf("syntax %s?\n",syntax);
  char *p= syntax;
  int iparam= 0;
  class cl_cmd_arg *parm= (class cl_cmd_arg *)(params->at(iparam));
  bool match;
  while (*p &&
	 parm)
    {
      //printf("Checking %s as %c\n",parm->get_svalue(),*p);
      if (uc)
	switch (*p)
	  {
	  case SY_ADDR:
	    match= parm->as_address(uc);
	    if (!match)
	      return(match);
	    //printf("ADDRESS match %lx\n",parm->value.address);
	    break;
	  case SY_MEMORY:
	    match= parm->as_memory(uc);
	    if (!match)
	      return(DD_FALSE);
	    //printf("MEMORY match %s\n",parm->value.memory->class_name);
	    break;
	  case SY_BIT:
	    if (!parm->as_bit(uc))
	      return(DD_FALSE);
	    break;
	  }
      switch (*p)
	{
	case SY_ADDR: case SY_MEMORY: case SY_BIT: break;
	case SY_NUMBER:
	  if (!parm->as_number())
	    return(DD_FALSE);
	  break;
	case SY_DATA:
	  if (!parm->as_data())
	    return(DD_FALSE);
	  break;
	case SY_HW:
	  if (!parm->as_hw(uc))
	    return(DD_FALSE);
	  break;
	case SY_STRING:
	  if (!parm->as_string())
	    return(DD_FALSE);
	  break;
	case SY_DATALIST:
	  if (!set_data_list(parm, &iparam))
	    return(DD_FALSE);
	  break;
	default:
	  return(DD_FALSE);
	}
      p++;
      iparam++;
      if (iparam < params->count)
	parm= (class cl_cmd_arg *)(params->at(iparam));
      else
	parm= 0;
    }
  if (!*p &&
      !parm)
    {
      matched_syntax= syntax;
      return(DD_TRUE);
    }
  return(DD_FALSE);
}

bool
cl_cmdline::set_data_list(class cl_cmd_arg *parm, int *iparm)
{
  class cl_cmd_arg *next_parm;
  int len, i, j;
  t_mem *array;

  len= 0;
  array= 0;
  for (i= *iparm, next_parm= param(i); next_parm; i++, next_parm= param(i))
    {
      if (next_parm->is_string())
	{
	  int l;
	  char *s;
	  //s= proc_escape(next_parm->get_svalue(), &l);
	  if (!next_parm->as_string())
	    continue;
	  s= next_parm->value.string.string;
	  l= next_parm->value.string.len;
	  if (!array)
	    array= (t_mem*)malloc(sizeof(t_mem)*l);
	  else
	    array= (t_mem*)realloc(array, sizeof(t_mem)*(l+len));
	  for (j= 0; j < l; j++)
	    {
	      array[len]= s[j];
	      len++;
	    }
	  //if (s)
	  //free(s);
	}
      else
	{
	  if (!next_parm->as_data())
	    {
	      if (array)
		free(array);
	      return(DD_FALSE);
	    }
	  if (!array)
	    array= (t_mem*)malloc(sizeof(t_mem));
	  else
	    array= (t_mem*)realloc(array, sizeof(t_mem)*(1+len));
	  array[len]= next_parm->value.data;
	  len++;
	}
    }
  *iparm= i;
  parm->value.data_list.array= array;
  parm->value.data_list.len= len;
  return(DD_TRUE);
}


/*
 * Command
 *____________________________________________________________________________
 */

cl_cmd::cl_cmd(enum cmd_operate_on op_on,
	       char *aname,
	       int can_rep,
	       char *short_hlp,
	       char *long_hlp):
  cl_base()
{
  operate_on= op_on;
  names= new cl_strings(1, 1);
  names->add(aname?strdup(aname):strdup("unknown"));
  can_repeat= can_rep;
  short_help= short_hlp?strdup(short_hlp):NULL;
  long_help= long_hlp?strdup(long_hlp):NULL;
}

/*cl_cmd::cl_cmd(class cl_sim *asim):
  cl_base()
{
  sim= asim;
  name= short_help= long_help= 0;
  can_repeat= 0;
}*/

cl_cmd::~cl_cmd(void)
{
  delete names;
  if (short_help)
    free(short_help);
  if (long_help)
    free(long_help);
}

void
cl_cmd::add_name(char *nam)
{
  if (nam)
    names->add(strdup(nam));
}

int
cl_cmd::name_match(char *aname, int strict)
{
  int i;
  
  if (names->count == 0 &&
      !aname)
    return(1);
  if (!aname)
    return(0);
  if (strict)
    {
      for (i= 0; i < names->count; i++)
	{
	  char *n= (char*)(names->at(i));
	  if (strcmp(aname, n) == 0)
	    return(1);
	}
    }
  else
    {
      for (i= 0; i < names->count; i++)
	{
	  char *n= (char*)(names->at(i));
	  if (strstr(n, aname) == n)
	    return(1);
	}
    }
  return(0);
}

int
cl_cmd::name_match(class cl_cmdline *cmdline, int strict)
{
  return(name_match(cmdline->get_name(), strict));
}

int
cl_cmd::syntax_ok(class cl_cmdline *cmdline)
{
  return(1);
}

int
cl_cmd::work(class cl_app *app,
	     class cl_cmdline *cmdline, class cl_console *con)
{
  if (!syntax_ok(cmdline))
    return(0);
  class cl_sim *sim= app->get_sim();
  class cl_uc *uc= 0;
  if (sim)
    uc= sim->uc;
  switch (operate_on)
    {
    case operate_on_app:
      if (!app)
	{
	  con->dd_printf("There is no application to work on!\n");
	  return(DD_TRUE);
	}
      return(do_work(app, cmdline, con));
    case operate_on_sim:
      if (!sim)
	{
	  con->dd_printf("There is no simulator to work on!\n");
	  return(DD_TRUE);
	}
      return(do_work(sim, cmdline, con));
    case operate_on_uc:
      if (!sim)
	{
	  con->dd_printf("There is no microcontroller to work on!\n");
	  return(DD_TRUE);
	}
      return(do_work(uc, cmdline, con));
    default:
      return(do_work(cmdline, con));
    }
}

int
cl_cmd::do_work(class cl_cmdline *cmdline, class cl_console *con)
{
  con->dd_printf("Command \"%s\" does nothing.\n",
		 (char*)(names->at(0)));
  return(0);
}

int
cl_cmd::do_work(class cl_app *app,
		class cl_cmdline *cmdline, class cl_console *con)
{
  con->dd_printf("Command \"%s\" does nothing on application.\n",
		 (char*)(names->at(0)));
  return(0);
}

int
cl_cmd::do_work(class cl_sim *sim,
		class cl_cmdline *cmdline, class cl_console *con)
{
  con->dd_printf("Command \"%s\" does nothing on simulator.\n",
		 (char*)(names->at(0)));
  return(0);
}

int
cl_cmd::do_work(class cl_uc *uc,
		class cl_cmdline *cmdline, class cl_console *con)
{
  con->dd_printf("Command \"%s\" does nothing on microcontroller.\n",
		 (char*)(names->at(0)));
  return(0);
}


/*
 * Set of commands
 *____________________________________________________________________________
 */

cl_cmdset::cl_cmdset(void):
  cl_list(5, 5)
{
  //sim= 0;
  last_command= 0;
}

/*cl_cmdset::cl_cmdset(class cl_sim *asim):
  cl_list(5, 5)
{
  sim= asim;
  last_command= 0;
}*/

class cl_cmd *
cl_cmdset::get_cmd(class cl_cmdline *cmdline)
{
  int i;

  if (cmdline->repeat())
    {
      if (last_command)
	return(last_command);
      else
	return(0);
    }
  // exact match
  for (i= 0; i < count; i++)
    {
      class cl_cmd *c= (class cl_cmd *)at(i);
      if (c->name_match(cmdline, 1))
	return(c);
    }
  // not exact match
  class cl_cmd *c_matched= 0;
  for (i= 0; i < count; i++)
    {
      class cl_cmd *c= (class cl_cmd *)at(i);
      if (c->name_match(cmdline, 0))
	{
	  if (!c_matched)
	    c_matched= c;
	  else
	    return(0);
	}
    }
  return(c_matched);
  //return(0);
}

class cl_cmd *
cl_cmdset::get_cmd(char *cmd_name)
{
  int i;

  for (i= 0; i < count; i++)
    {
      class cl_cmd *c= (class cl_cmd *)at(i);
      if (c->name_match(cmd_name, 1))
	return(c);
    }
  return(0);
}

void
cl_cmdset::del(char *nam)
{
  int i;

  if (!nam)
    return;
  for (i= 0; i < count; i++)
    {
      class cl_cmd *cmd= (class cl_cmd *)(at(i));
      if (cmd->name_match(nam, 1))
	free_at(i);
    }
}

void
cl_cmdset::replace(char *nam, class cl_cmd *cmd)
{
  int i;

  if (!nam)
    return;
  for (i= 0; i < count; i++)
    {
      class cl_cmd *c= (class cl_cmd *)(at(i));
      if (c->name_match(nam, 1))
	{
	  delete c;
	  put_at(i, cmd);
	}
    }
}


/*
 * Composed command: subset of commands
 *____________________________________________________________________________
 */

cl_super_cmd::cl_super_cmd(char *aname,
			   int  can_rep,
			   char *short_hlp,
			   char *long_hlp,
			   class cl_cmdset *acommands):
  cl_cmd(operate_on_none, aname, can_rep, short_hlp, long_hlp)
{
  commands= acommands;
}

cl_super_cmd::~cl_super_cmd(void)
{
  if (commands)
    delete commands;
}

int
cl_super_cmd::work(class cl_app *app,
		   class cl_cmdline *cmdline, class cl_console *con)
{
  class cl_cmd *cmd= 0;

  if (!commands)
    return(0);
  
  if (!cmdline->shift())
    {
      if ((cmd= commands->get_cmd("_no_parameters_")) != 0)
	return(cmd->work(app, cmdline, con));
      int i;
      con->dd_printf("\"%s\" must be followed by the name of a subcommand\n"
		     "List of subcommands:\n", (char*)(names->at(0)));
      for (i= 0; i < commands->count; i++)
	{
	  cmd= (class cl_cmd *)(commands->at(i));
	  con->dd_printf("%s\n", cmd->short_help);
	}
      return(0);
    }
  if ((cmd= commands->get_cmd(cmdline)) == NULL)
    {
      con->dd_printf("Undefined subcommand: \"%s\". Try \"help %s\".\n",
		     cmdline->get_name(), (char*)(names->at(0)));
      return(0);
    }
  return(cmd->work(app, cmdline, con));
}


/* End of cmd.src/command.cc */
