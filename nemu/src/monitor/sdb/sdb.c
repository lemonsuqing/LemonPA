/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "sdb.h"
#include "memory/paddr.h"

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}


static int cmd_q(char *args) {
  nemu_state.state = NEMU_QUIT;
  return -1;
}

static int cmd_si(char *args){
  int num = 1;
  if(args != NULL){
    sscanf(args, "%d", &num);
  }
  cpu_exec(num);
  return 0;
}

static int cmd_info(char *args) {
  int len = strlen(args);
  if(len > 1){
    printf("You cannot iutput strings longer than 1.\nYou can only type 'r' or 'w'\n");
  }else{
    if(strcmp(args, "r") == 0){
      isa_reg_display();
    }else if (strcmp(args, "w") == 0){
      printf("display monitor\n");
    }
    
  }
  return 0;
}

extern const char* regs[];
extern const int reg_len;
bool contains_register(const char* input) {
    for (size_t i = 0; i < reg_len; ++i) {
        if (strstr(input, regs[i]) != NULL) {
            return true;
        }
    }
     if(strstr(input, "pc") != NULL){
       return true;
     }
    return false;
}

static int cmd_p(char* args) {
  bool success = true;
  init_regex();
  int res = expr(args, &success);
  if (!success) {
    puts("invalid expression!");
  } else {
    if(contains_register(args))
      printf("%s = 0x%08X\n", args, res);
    else
      printf("%s = %d\n", args, res);
  }
  return 0;
}

static int cmd_x(char *args){
  char* first = strtok(args," ");
  char* second = strtok(NULL," ");
  int len = 0;
  paddr_t addr = 0;
  sscanf(first,"%d",&len);
  sscanf(second,"%x",&addr);
  for(int i = 0;i<len;i++){
    printf("0x%08X: 0x%08X\n",addr,paddr_read(addr,4));
    addr += 4;
  }
  return 0;
}

static int cmd_help(char *args);

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },

  /* TODO: Add more commands */
  { "si", "cpu_exec N", cmd_si },
  { "info", "r display reg\n     - w display monitor", cmd_info },
  { "p", "cpu_exec N", cmd_p },
  { "x", "Memory scanning",cmd_x},
};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
