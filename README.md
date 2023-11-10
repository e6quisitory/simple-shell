# Simple Shell
A shell like bash or zsh, albiet very stripped down in terms of features compared to those.

Coded on and for Linux, but should work on any POSIX-compliant OS (ex. macOS).

This is a project I completed as part of a class at _Simon Fraser University_ : CMPT 201 - Systems Programming.

## Features
- Enter commands like in any other shell and execute them

  ```
  /Users/exquisitory/42$ ls -al
  total 8
  drwxr-xr-x   3 exquisitory  staff    96  9 Nov 20:54 .
  drwxr-x---+ 86 exquisitory  staff  2752  9 Nov 20:54 ..
  -rw-r--r--   1 exquisitory  staff    29  9 Nov 20:54 meaning_of_life.txt
  /Users/exquisitory/42$ cat meaning_of_life.txt
  The meaning of life is...
  42
  ```
- Putting `&` at the end of the command executes it in the background (ex. `/Users/exquisitory$ sleep 10 &`)
- Pressing `CTRL+C` displays the same help message as the internal command `help`
- Shell internal commands:
    - `cd` : change directory
    - `pwd` : print working directory
    - `help` : displays all internal commands and what they do
    - `exit` : exits the shell
    - `history` : displays up to the last 10 entered commands ; past command `n` can be run through `!n`
      
      ```
      /Users/exquisitory/42$ cat meaning_of_life.txt
      The meaning of life is...
      42
      /Users/exquisitory/42$ history
      3	history
      2	cat meaning_of_life.txt
      1	ls -al
      0	cd /Users/exquisitory/42
      /Users/exquisitory/42$ !2
      cat meaning_of_life.txt
      The meaning of life is...
      42
      ```

## Technical Details
- Entered commands are executed in a new process through the `fork()` syscall, then `exec()`, and finally `wait()` in the parent process
- `CTRL+C` printing out the help message is made possible through a custom signal handler for `SIGINT`, setup using `sigaction()`
- All code is signal-safe ; only functions listed in the `signal-safe` man page are used

## Build Instructions
Clone this repository, then, from its root, run the following commands.
```
mkdir build
cd build
cmake ..
make
./simple-shell
```
