/* terminal program written in C by @menascii
   
   UNLV CS 370 homework assignment by Ajoy Kumar Datta
   he was a graduate computer science professor who taught
   operating systems. his tests were tough and i appreciate 
   the knowledge he shared with us. rest in peace.
     
   compile:
      gcc terminal.c -o terminal

   run:
      ./terminal

  input:
      enter basic commands on terminal
      pipe commands are allowed

    examples
      /current/file/directory username$ > ls
      /current/file/directory username$ > ls -l | more
      /current/file/directory username$ > cat filename | more

   output:
      varies based upon command executed.  

   note: 
      not all commands work and there are minor bugs. this was 
      a simple two week homework assignment.
*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>

#define LSH_RL_BUFSIZE 1024

void get_command(void);
char *read_user_input(void);
char **parse_user_input(char *);
int executue_user_input(char **);
int execute_system_commands(char **);
int execute_process_commands(char **);
int execute_command(char **, int, int);
int single_command(char **);
int pipe_command(char **, int, int);
void runpipe(int[],char**, char**);
void join_command(char**);

int main(void) 
{ 
  static struct termios oldtio, newtio;
  tcgetattr(0, &oldtio);
  newtio = oldtio;
  newtio.c_lflag &= ~ICANON;
  newtio.c_lflag &= ~ECHO;
  tcsetattr(0, TCSANOW, &newtio);
  get_command();
  tcsetattr(0, TCSANOW, &oldtio);
  return 0;
}

void get_command(void)
{
  char *user;
  char *user_input;
  char cwd_buffer[256];
  char **parsed_data;
  int status;

 do
 {
   user = getenv("USER");
   if (getcwd(cwd_buffer, sizeof(cwd_buffer)) == NULL)
   {
     perror("getcwd() error");
   }
   else
   {
  	 printf ("%s %s$ > ", cwd_buffer, user);
  	 user_input = read_user_input();
  	 parsed_data = parse_user_input(user_input);
  	 status = executue_user_input(parsed_data);
  	 
  	 //	 status = 2;
  	 free(user_input);
  	 free(parsed_data);
   }
 } 
 while (status == 1);
 printf("\n");
}

char *read_user_input(void)
{
  char c;
  int buffsize = 1048;
  int position = 0;
  char *buffer = malloc(sizeof(char) * buffsize);

  while (1)
  {
    c = getchar();

    // check for escape sequence                                                                                   
    if (c == 27) 
    {
      // ignore next two characters if escape sequence                                              
      c = getchar();
      c = getchar();
      continue;
    }
    // check for backspace
    if (c == 0x7f)
    {
      // reset position
      if (position > 0 )
      {
        // go one char left                                                                                      
        printf("\b");
        // overwrite the char with whitespace                                                                    
        printf(" ");
        // go back to "now removed char position"                                                                
        printf("\b");
        position--;
        continue;
      }
      else
      {
        continue;
      }
    }

    // check for linefeed
    if (c == '\n' || c == EOF) 
    {
      buffer[position] = '\0';
      break;
    }
    // read user input into buffer
    else
    {
      buffer[position] = c;
    }
    position++;
    printf("%c", c);
  }
  return buffer;
}

char **parse_user_input(char *user_input)
{
  int parse_buffer = 128;
  int count;
  char **tokens = malloc(parse_buffer * sizeof(char*));
  char *input;

  if (!tokens)
    {
      fprintf(stderr, "lsh: allocation error\n");
      exit(EXIT_FAILURE);
    }

  else
  {
    input = strtok(user_input, " ");
    count = 0;
    do
    {
      tokens[count] = input;
      count++;

      if (count >= parse_buffer)
      {
        parse_buffer = parse_buffer + 128;
        tokens = realloc(tokens, parse_buffer * sizeof(char*));

        if (!tokens)
        {
          fprintf(stderr, "lsh: allocation error\n");
          exit(EXIT_FAILURE);
        }
      }

      input = strtok(NULL, " ");
    }
    while (input != NULL);

    tokens[count] = NULL;
    return tokens;
  }
}

int executue_user_input(char **parsed_data)
{
  char **system_commands[] =
  {
    "cd",
    "exit",
    "join"
  };
  int count;
  int length;

  if (parsed_data[0] == NULL)
  {
    return 1;
  }
  else
  {
    for (int i = 0; i < sizeof(system_commands) /sizeof(char *); i++)
    {
      if (strcmp(parsed_data[0], system_commands[i]) == 0)
      {
        return  execute_system_commands(parsed_data);
      }
    }
    return execute_process_commands(parsed_data);
  }
}

int execute_system_commands(char **parsed_data)
{
  if (strcmp(parsed_data[0], "cd") == 0)
  {
    if (parsed_data[1] == NULL)
    {
      printf("\n\nExpected arguments following cd command\n\n");
    }
    else
    {
      if (chdir(parsed_data[1]) != 0)
      {
        printf("\n\n-bash: ");
        for (int i = 0; parsed_data[i] != NULL; i++)
        {
          printf("%s ", parsed_data[i]);
        }
        printf(": No such file or directory\n\n");
      }
      else 
      {
        printf("\n\n");
      }
    }
    return 1;
  }

  if (strcmp(parsed_data[0], "exit") == 0)
  {
    printf("\n\nexit shell\n");
    return 0;
  }

  if (strcmp(parsed_data[0], "join") == 0)
  {
    join_command(parsed_data);
  }
  return 1;
}

int execute_process_commands(char **parsed_data)
{
  int pipe_position = -1;
  int last_token_position;
  for (int i = 0; parsed_data[i] != NULL; i++)
  {
    if (strcmp(parsed_data[i], "|") == 0)
    {
      pipe_position = i;
    }	
      last_token_position = i;
  }
  return execute_command(parsed_data, pipe_position, last_token_position);
}

int execute_command(char **parsed_data, int pipe_position, int last_token_position)
{
  if (pipe_position == -1)
  {  
    return single_command(parsed_data);
  }
  else
  {
    return pipe_command(parsed_data, pipe_position, last_token_position);
  }
}

int single_command(char **args)
{
  pid_t pid, wpid;
  int status;

  pid = fork();
  if (pid == 0) 
    {
      // Child process
      if (execvp(args[0], args) == -1) 
	{
	  printf("-bash");
	  for (int i = 0; args[i] != NULL; i++)
	    {
	      printf(" %s", args[i]);
	    }
	  printf(": command not found\n\n");
	}
      exit(EXIT_FAILURE);
    } 
  else if (pid < 0) 
    {
      // Error forking
      printf("-bash: error on fork\n\n");
    } 
  else 
    {
      // Parent process
      do 
	{
	  printf("\n");
	  wpid = waitpid(pid, &status, WUNTRACED);
	} 
      while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
  return 1;
}

int pipe_command(char **parsed_data, int pipe_position, int last_token_position)
{
  int parse_buffer = 248;
  char **command_one = malloc(parse_buffer * sizeof(char*));
  char **command_two = malloc(parse_buffer * sizeof(char*));
  int fd[2], pid, status;

  int i;
  for (i = 0; i < pipe_position; i++)
    {
      command_one[i] = parsed_data[i];
    }

  i++;
  command_one[i] = NULL;

  int j = 0;
  for ( i = pipe_position + 1; i <= last_token_position; i++)
    {
      command_two[j] = parsed_data[i];
      j++;
    }
  command_two[i] = NULL;

  pipe(fd);

  switch (pid = fork()) {

  case 0: /* child */
    runpipe(fd, command_one, command_two);
    exit(0);

  default: /* parent */
    while ((pid = wait(&status)) != -1)
      fprintf(stderr, "process %d exits with %d\n", pid, WEXITSTATUS(status));
    break;

  case -1:
    perror("fork");
    exit(1);
  }

  free(command_one);
  free(command_two);
  return 1;
}

void runpipe(int pfd[], char **cmd1, char **cmd2)
{
  int pid;

  switch (pid = fork()) {

  case 0: /* child */
    dup2(pfd[0], 0);
    close(pfd[1]);/* the child does not need this end of the pipe */
    execvp(cmd2[0], cmd2);
    perror(cmd2[0]);

  default: /* parent */
    dup2(pfd[1], 1);
    close(pfd[0]);/* the parent does not need this end of the pipe */
    execvp(cmd1[0], cmd1);
    perror(cmd1[0]);

  case -1:
    perror("fork");
    exit(1);
  }
}

void join_command(char **parsed_data)
{
  parsed_data[0] = "cat";
  parsed_data[4] = parsed_data[3];
  parsed_data[3] = ">";
  parsed_data[5] = NULL;
  
  single_command(parsed_data);  
}
