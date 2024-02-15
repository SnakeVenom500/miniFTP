# Miniature File-Transfer-Protocol System

## Setup
1. Download the files from the git repository
2. Open a command prompt and type 'make'
3. (Optional) Move the server executable to a different directory than the client executable.
4. Open a new command prompt window and navigate to the server directory. Enter './server' in the command prompt.
5. In the initial command prompt window, enter './client localhost'. You should get a prompt to type in commands, and on the server window it should indicate a connection. Use the commands as defined below.

## User commands
The user commands are:
1. **exit:** terminate the client program after instructing the server’s child process to do the same.
2. **cd \<pathname\>:** change the current working directory of the client process to <pathname>. It is an error if \<pathname\> does not exist, or is not a readable directory.
3. **rcd \<pathname\>:** change the current working directory of the server child process to \<pathname\>. It is an error if \<pathname\> does not exist, or is not a readable directory.
4. **ls:** execute the “ls –l” command locally and display the output to standard output, 20 lines at a time, waiting for a space character on standard input before displaying the next 20 lines. See “rls” and “show”.
5. **rls:** execute the “ls –l” command on the remote server and display the output to the client’s standard output, 20 lines at a time.
6. **get \<pathname\>:** retrieve \<pathname\> from the server and store it locally in the client’s current working directory using the last component of \<pathname\> as the file name. It is an error if \<pathname\> does not exist, or is anything other than a regular file, or is not readable. It is also an error if the file cannot be opened/created in the client’s current working directory.
7. **show \<pathname\>:** retrieve the contents of the indicated remote \<pathname\> and write it to the client’s standard output, 20 lines at a time. Wait for the user to type a space character before displaying the next 20 lines. Note: the “more” command can be used to implement this feature. It is an error if \<pathname\> does not exist, or is anything except a regular file, or is not readable.
8. **put \<pathname\>:** transmit the contents of the local file \<pathname\> to the server and store the contents in the server process’ current working directory using the last component of \<pathname\> as the file name. It is an error if \<pathname\> does not exist locally or is anything other than a regular file. It is also an error if the file cannot be opened/created in the server’s child process’ current working directory.
