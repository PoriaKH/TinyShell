# TinyShell
A tiny little shell :)

It searches the given command through the $PATH variable using execv() function and also it supports rediraction (`<`,`>`).
tinyshell is able to handle signals such as (SIGINT, SIGTSTP, ...).
and also you can put procceses in background and bring them back to foreground.


### Usage
Use command `?` when using tinyshell to show the help menu:
```
? - show this help menu
quit - quit the command shell
cd - change directory
pwd - show current path
wait - wait till end of each child process
```
### Build
```
make
```
It generates the executable `tinyshell` in the `./build` directory.
### Contribute
Feel free to add anything. I will take care of the PRs.

