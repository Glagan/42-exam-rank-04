#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#define SIDE_OUT	0
#define SIDE_IN		1

#define TYPE_END	0
#define TYPE_PIPE	1
#define TYPE_BREAK	2

#ifdef TEST_SH
# define TEST		1
#else
# define TEST		0
#endif

typedef struct	s_list
{
	char			**args;
	int				length;
	int				type;
	int				pipes[2];
	struct s_list	*previous;
	struct s_list	*next;
}				t_list;

int
	ft_strlen(char const *str)
{
	int	i;

	i = 0;
	while (str[i])
		i++;
	return (i);
}

int
	show_error(char const *str)
{
	if (str)
		write(STDERR_FILENO, str, ft_strlen(str));
	return (1);
}

int
	exit_fatal(void)
{
	show_error("error: fatal\n");
	exit(1);
	return (1);
}

char
	*ft_strdup(char const *str)
{
	char	*copy;
	int		i;

	if (!(copy = (char*)malloc(sizeof(*copy) * ft_strlen(str))))
	{
		exit_fatal();
		return (NULL);
	}
	i = 0;
	while (str[i])
	{
		copy[i] = str[i];
		i++;
	}
	copy[i] = 0;
	return (copy);
}

int
	add_arg(t_list *cmd, char *arg)
{
	char	**tmp;
	int		i;

	i = 0;
	tmp = NULL;
	if (!(tmp = (char**)malloc(sizeof(*tmp) * (cmd->length + 2))))
		return (exit_fatal());
	while (i < cmd->length)
	{
		tmp[i] = cmd->args[i];
		i++;
	}
	free(cmd->args);
	cmd->args = tmp;
	if (!(cmd->args[i++] = ft_strdup(arg)))
		return (exit_fatal());
	cmd->length++;
	cmd->args[i] = 0;
	return (0);
}

int
	list_push(t_list **list, char *arg)
{
	t_list	*new;

	if (!(new = (t_list*)malloc(sizeof(*new))))
		return (exit_fatal());
	if (!(new->args = (char**)malloc(sizeof(*new->args))))
		return (exit_fatal());
	new->args[0] = NULL;
	new->length = 0;
	new->type = TYPE_END;
	new->previous = NULL;
	new->next = NULL;
	if (*list)
	{
		(*list)->next = new;
		new->previous = *list;
	}
	*list = new;
	return (add_arg(new, arg));
}

int
	list_rewind(t_list **list)
{
	while (*list && (*list)->previous)
		*list = (*list)->previous;
	return (0);
}

int
	args_clear(t_list *cmd)
{
	int	i;

	i = 0;
	while (i < cmd->length)
		free(cmd->args[i++]);
	free(cmd->args);
	cmd->args = NULL;
	cmd->length = 0;
	return (0);
}

int
	list_clear(t_list **cmds)
{
	t_list	*tmp;

	list_rewind(cmds);
	while (*cmds)
	{
		tmp = (*cmds)->next;
		if ((*cmds)->length > 0)
			args_clear(*cmds);
		free(*cmds);
		*cmds = tmp;
	}
	*cmds = NULL;
	return (0);
}

int
	parse_arg(t_list **cmds, char *arg)
{
	int	is_break;

	is_break = (strcmp(";", arg) == 0);
	if (is_break && !*cmds)
		return (0);
	if ((!*cmds || (*cmds)->type > TYPE_END))
		return (list_push(cmds, arg));
	else
	{
		if (strcmp("|", arg) == 0)
			(*cmds)->type = TYPE_PIPE;
		else if (is_break)
		{
			if (*cmds)
				(*cmds)->type = TYPE_BREAK;
		}
		else
			return (add_arg(*cmds, arg));
	}
	return (0);
}

int
	exec_cmd(t_list *cmd, char **args, char * const *env)
{
	pid_t	pid;
	int		ret;
	int		status;
	int		pipe_open;

	pipe_open = 0;
	if (cmd->type == TYPE_PIPE || (cmd->previous && cmd->previous->type == TYPE_PIPE))
	{
		pipe_open = 1;
		if (pipe(cmd->pipes) < 0)
			return (exit_fatal());
	}
	pid = fork();
	if (pid == 0)
	{
		if (cmd->type == TYPE_PIPE
			&& dup2(cmd->pipes[SIDE_IN], STDOUT_FILENO) < 0)
			return (exit_fatal());
		if (cmd->previous
			&& cmd->previous->type == TYPE_PIPE
			&& dup2(cmd->previous->pipes[SIDE_OUT], STDIN_FILENO) < 0)
			return (exit_fatal());
		ret = execve(args[0], args, env);
		if (ret < 0)
		{
			show_error("error: cannot execute ");
			show_error(args[0]);
			show_error("\n");
		}
		if (pipe_open)
		{
			close(cmd->pipes[SIDE_OUT]);
			close(cmd->pipes[SIDE_IN]);
		}
		if (cmd->previous && cmd->previous->type == TYPE_PIPE)
			close(cmd->previous->pipes[SIDE_OUT]);
		exit(ret);
		return (ret);
	}
	else if (pid < 0)
		return (exit_fatal());
	else
	{
		waitpid(pid, &status, 0);
		if (pipe_open)
		{
			close(cmd->pipes[SIDE_IN]);
			if (!cmd->next || cmd->type == TYPE_BREAK
				|| !WIFEXITED(status) || WEXITSTATUS(status))
				close(cmd->pipes[SIDE_OUT]);
		}
		if (cmd->previous && cmd->previous->type == TYPE_PIPE)
			close(cmd->previous->pipes[SIDE_OUT]);
		if (WIFEXITED(status))
			return (WEXITSTATUS(status));
		return (1);
	}
	return (0);
}

int
	exec_cmds(t_list **cmds, char * const *env)
{
	t_list	*crt;
	int		last_ret;

	last_ret = 0;
	list_rewind(cmds);
	while (*cmds)
	{
		crt = *cmds;
		if (strcmp("cd", crt->args[0]) == 0)
		{
			if (crt->length != 2)
				return (show_error("error: cd: bad arguments\n"));
			if (chdir(crt->args[1]) != 0)
			{
				return (show_error("error: cd: cannot change directory to ")
					&& show_error(crt->args[1])
					&& show_error("\n"));
			}
		}
		else if (exec_cmd(crt, crt->args, env))
		{
			while ((*cmds)->next && (*cmds)->type == TYPE_PIPE)
				*cmds = (*cmds)->next;
		}
		if (!(*cmds)->next)
			break ;
		*cmds = (*cmds)->next;
	}
	return (last_ret);
}

int
	exit_error(t_list **cmds, char const *str)
{
	if (cmds)
		list_clear(cmds);
	if (str)
		show_error(str);
	return (1);
}

int
	main(int argc, char **argv, char * const *env)
{
	t_list	*cmds;
	int		i;
	int		ret;

	ret = 0;
	if (argc == 1)
	{
		write(STDOUT_FILENO, "\n", 1);
		return (ret);
	}
	cmds = NULL;
	i = 1;
	while (i < argc)
		if (parse_arg(&cmds, argv[i++]))
			return (exit_fatal());
	if (cmds && (ret = exec_cmds(&cmds, env)))
		return (list_clear(&cmds) | ret);
	list_clear(&cmds);
	if (TEST)
		while (1);
	return (ret);
}
