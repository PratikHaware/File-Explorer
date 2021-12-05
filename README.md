# AOS Assignment 1 (File Explorer)

## Steps to run
1. Open terminal from this same folder
2. Run the following commands in given order
-  g++ main.cpp -o main
- ./main

## How to use
- This application has 2 modes
1. Normal mode (to explore the file system)
2. Command mode (to perform actions on the file system)

### Normal mode

- The application opens by default in this mode. 
- Use **up** and **down** arrow keys to traverse the list. 
- To scroll up and down, use **k** and **l** keys respectively. 
- Press **Enter** to open the file / directory on which the cursor is. Files are opened in vi editor.
- Press **left** arrow key to go back to previously visited directory and **right** arrow key to go forward. 
- Press **backspace** to go to parent directory and **h** to go to system home directory
- Press **:** to enter command mode

### Command mode
Following are the commands available
- Copy - ‘copy <source_file(s)> <destination_directory>’
- Move - ‘move <source_file(s)> <destination_directory>’
- Rename - ‘rename <old_filename> <new_filename>’
- Create File - ‘create_file <file_name> <destination_path>’
- Create Directory - ‘create_dir <dir_name> <destination_path>’
- Delete File - ‘delete_file <file_path>’
- Delete Directory - ‘delete_dir <dir_path>’
- Goto - ‘goto <location>’
- Search - ‘search <file_name>’ or ‘search <directory_name>’ (Prints True in message if item exists. Prints false otherwise)

>Press **esc** to exit command mode. You will reach in Normal mode

### General details

1. Absolute and relative, both paths are accepted
2. Current status is shown in status bar (in blue)
3. Changes made in command mode can be seen in normal mode. No need to restart the application.
4. Messages about the execution of commands is given in Message bar (in red)
5. Pressing **q** will exit the application immediately
6. Relative path is calculated from current working directory

### This application was developed and tested on Ubuntu 20.04





