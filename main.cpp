#include<bits/stdc++.h>
#include<iostream>
#include<unistd.h>
#include<dirent.h>
#include<sys/stat.h>
#include<time.h>
#include<termios.h> //termios, tcgetattr, tcsetattr
#include<stdlib.h>
#include<sys/ioctl.h>  //winsize
#include<sys/wait.h> //wait
#include<fcntl.h>

using namespace std;

typedef unsigned long long int ulli;

/* --- Global variables --- */
//constants to convert bytes in GB, MB, KB
const int GB_IN_B = 1073741824;
const int MB_IN_B = 1048576;
const int KB_IN_B = 1024;

//variables to keep track of cursor position
int CURSOR_COL = 1;   
int CURSOR_ROW = 1;
size_t CURSOR_ROW_MAX;  //max row that cursor can occupy in normal mode
const int CURSOR_START_COL_FOR_COMMAND_MODE = 17;

size_t LAST_ROW;

//indices to print DIR_CONTENTS on screen. These should always be set by list_dir_contents
//k() and l() functions can modify them
int DIR_CONTENTS_S_IDX;
int DIR_CONTENTS_E_IDX;


//Present Working Directory variables
const size_t PWD_LEN = 1024;
char PWD[PWD_LEN];   //most functions use and return char array
string PWD_STR;      //string respresentation of PWD
vector<string> DIR_CONTENTS; //2nd element of pair is file type
int DIR_CONTENTS_IDX = 0;
string ROOT_PATH;
string HOME_PATH;
string USERNAME;

string VI_PATH = "/usr/bin/vi";

const int BUF_SIZE = 1024;

//stacks for navigating back and forward
stack<string> BACK_ST;    //back stack. used for going back
stack<string> FORWARD_ST;     //forward stack. used for going forward

//Going into raw mode changes terminal settings. This struct stores original terminal settings. 
struct termios ORIG_TERMIOS;

//Window size
struct winsize WIN_SIZE;

vector<string> COMMAND_TOKENS;

inline void initialize_globals()
{
    //store window sizes in WIN_SIZE
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &WIN_SIZE);
    //original attributes of terminal (whose file descriptor is STDIN_FILENO) is stored in ORIG_TERMIOS
    tcgetattr(STDIN_FILENO, &ORIG_TERMIOS);
    //PWD now contains present working directory
    getcwd(PWD, PWD_LEN); 
    PWD_STR = PWD;         //convert pwd from char array to string
    USERNAME = getlogin();
    HOME_PATH = "/home/" + USERNAME;
    //ROOT_PATH = PWD_STR;
    LAST_ROW = WIN_SIZE.ws_row - 4;
}

//positions cursor at top left corner (1,1)
inline void moveCursorToHome()
{
    std::cout<<"\033[1;1H";
    CURSOR_COL = 1;
    CURSOR_ROW = 1;
    //fflush(stdout);     
}

inline void moveCursorToColumn(int col)
{
    cout<<"\033["<<col<<"G";
    CURSOR_COL = col;
    //fflush(stdout);
}

inline void moveCursorUp()
{
    cout<<"\033[1A";
    CURSOR_ROW--;
    //fflush(stdout);  
}

inline void moveCursorDown()
{
    cout<<"\033[1B";
    CURSOR_ROW++;
    //fflush(stdout);  
}

inline void moveCursor(int row, int col)
{
    cout<<"\033["<<row<<";"<<col<<"H";
    CURSOR_COL = col;
    CURSOR_ROW = row;
    //fflush(stdout);  
}

inline void write_in_bold_cyan(string message)
{
    cout<<"\033[0;36m";  //set color cyan
    cout<<"\033[1m";     //set bold
    cout<<message;
    cout<<"\033[22m";    //reset color
    cout<<"\033[0m";     //reset bold
}

inline void print_status(string mode)
{
    moveCursor(WIN_SIZE.ws_row - 1, 1);
    cout<<"\033[0K";    //clear current line from cursor to end
    write_in_bold_cyan("Status: " + mode);
    moveCursorToHome();
}

inline void write_in_bold_green(string message)
{
    cout<<"\033[0;32m";  //set color green
    cout<<"\033[1m";     //set bold
    cout<<message;
    cout<<"\033[22m";    //reset color
    cout<<"\033[0m";     //reset bold
}

inline void write_in_bold_red(string message)
{
    cout<<"\033[0;31m";  //set color red
    cout<<"\033[1m";     //set bold
    cout<<message;
    cout<<"\033[22m";    //reset color
    cout<<"\033[0m";     //reset bold
}

inline void print_debug(string message)
{
    moveCursor(WIN_SIZE.ws_row - 4, 1);
    cout<<"debug: "<<message;
}

inline void clear_messages()
{
    moveCursor(WIN_SIZE.ws_row - 2, 1);
    cout<<"\033[0K";    //clear current line from cursor to end
}

inline void print_message(string message)
{
    clear_messages();
    write_in_bold_red("Messages: " + message);
    moveCursorToHome();
}

inline void print_enter_command()
{
    moveCursor(WIN_SIZE.ws_row, 1);
    cout<<"\033[0K";    //clear current line from cursor to end
    write_in_bold_green("Enter Command $");
    moveCursor(WIN_SIZE.ws_row, CURSOR_START_COL_FOR_COMMAND_MODE);
}

inline void setup_terminal()
{
    std::cout<<"\033[2J";       //clears terminal
    moveCursorToHome();         //positions cursor at top left corner (1,1)
}

inline void reset_DIR_CONTENTS()
{
    DIR_CONTENTS.clear();
    DIR_CONTENTS_IDX = 0;
}

//returns "abs" if path is absolute and "rel" if path is relative
inline string get_path_type(string path)
{
    if(path[0] == '/') return "abs";
    return "rel";
}

string convert_to_abs_path(string relPath)
{
    if(get_path_type(relPath) == "rel")
    {
        if(relPath == ".") return PWD_STR;
        else if (relPath == "..")
        {
            string absPath = PWD_STR;
            while(absPath.back() != '/') absPath.pop_back();
            if(absPath != "/") absPath.pop_back();
            return absPath;
        }
        else if (relPath.substr(0, 3) == "../")
        {
            string absPath = PWD_STR;
            while(absPath.back() != '/') absPath.pop_back();
            if(absPath != "/") absPath.pop_back();
            return absPath + relPath.substr(2);
        }
        else if (relPath[0] == '.') return PWD_STR + relPath.substr(1);
        else if (relPath[0] == '~') return HOME_PATH + relPath.substr(1);
        return PWD_STR + '/' + relPath;
    }
    return relPath;
}

//Normal mode
inline void enter_raw_mode()
{
    //copy original attributes in raw. Attributes will be changed here
    struct termios raw = ORIG_TERMIOS;
    /*turn off ECHO feature and canonical mode. ECHO and ICANON are bit flags stored in raw.c_lflag 
    (local flags)*/
    raw.c_lflag = raw.c_lflag & ~(ECHO | ICANON);
    //set new attributes for terminal
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    //clear line. removes text before prompt
}


void updatePWD(string path)
{
    chdir(path.c_str());
    getcwd(PWD, PWD_LEN);
    PWD_STR = PWD;
}

//prints DIR_CONTENTS vector from indices [start end]
void print_DIR_CONTENTS(int start, int end)
{
    setup_terminal();
    for(size_t i = start; i <= end; i++)
    {
        struct stat fileInfo;
        //load details of file with name d_name in fileInfo
        lstat(DIR_CONTENTS[i].c_str(), &fileInfo);

        //print file name. Truncate if too long
        string fileName = DIR_CONTENTS[i];
        if(fileName.size() <= 20) cout<<fileName<<"\t";
        else
        {
            string smallName = fileName.substr(0, 20);
            for(int i = 0; i < 3; i++) smallName.push_back('.');
            cout<<smallName<<"\t";
        }
        moveCursorToColumn(30);  //move the cursor to column 30
        
        /* print file size */
        off_t fileSize = fileInfo.st_size;
        if(fileSize >= GB_IN_B) cout<<fileSize / GB_IN_B<<"GB\t";
        else if (fileSize >= MB_IN_B) cout<<fileSize / MB_IN_B<<"MB\t";
        else if (fileSize >= KB_IN_B) cout<<fileSize / KB_IN_B<<"KB\t";
        else cout<<fileSize<<"B\t";
        moveCursorToColumn(40);  //move the cursor to column 40
        
        /* print file ownership and permissions */
        S_ISDIR(fileInfo.st_mode) ? cout<<"d" : cout<<"-";
        (S_IRUSR & fileInfo.st_mode) ? cout<<"r" : cout<<"-"; 
        (S_IWUSR & fileInfo.st_mode) ? cout<<"w" : cout<<"-"; 
        (S_IXUSR & fileInfo.st_mode) ? cout<<"x" : cout<<"-";
        (S_IRGRP & fileInfo.st_mode) ? cout<<"r" : cout<<"-"; 
        (S_IWGRP & fileInfo.st_mode) ? cout<<"w" : cout<<"-"; 
        (S_IXGRP & fileInfo.st_mode) ? cout<<"x" : cout<<"-";
        (S_IROTH & fileInfo.st_mode) ? cout<<"r" : cout<<"-"; 
        (S_IWOTH & fileInfo.st_mode) ? cout<<"w" : cout<<"-";
        (S_IXOTH & fileInfo.st_mode) ? cout<<"x" : cout<<"-";
        cout<<"\t";

        /*print file's last modified time */
        char* modTime = ctime(&fileInfo.st_mtime);
        // modTime[strlen(modTime) - 1] = '\0';  //from time.h
        cout<<modTime;
        //why is new line printed in terminal without \n or endl?
    }
    moveCursorToHome();
}

//warning: this function will first clear screen and then list contents
//returns 1 if operation is successful. Else returns 0
int list_dir_contents(string path)
{
    /*d is directory stream. It is an ordered sequence of all the directory entries in a 
    particular directory*/
    DIR* d = opendir(path.c_str());
    if(!d) return 0;  //INVALID PATH
    /* START: valid path. Update DIR_CONTENTS and print directory contents*/
    //ditem will hold an individual item of d at a time
    struct dirent* ditem;      
    reset_DIR_CONTENTS();
    while ((ditem = readdir(d)) != NULL) DIR_CONTENTS.push_back(string(ditem->d_name));
    CURSOR_ROW_MAX = min(DIR_CONTENTS.size(), LAST_ROW);
    DIR_CONTENTS_S_IDX = 0;
    DIR_CONTENTS_E_IDX = CURSOR_ROW_MAX - 1;
    print_DIR_CONTENTS(DIR_CONTENTS_S_IDX, DIR_CONTENTS_E_IDX);
    closedir(d);
    /* END: valid path. Update DIR_CONTENTS and print directory contents*/
    return 1;
}

void exit_raw_mode()
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &ORIG_TERMIOS);
}

//0 : user pressed esc key to quit
//1 : tokens are present in COMMAND_TOKENS
string get_command()
{
    char c;
    string command;
    while(1)
    {
        c = cin.get();
        if(c == 'q') 
        {
            setup_terminal();
            exit_raw_mode();
            exit(0);  //quit explorer
        }
        if(c == 27) return "esc";
        else if (c == 10) 
        {
            moveCursor(WIN_SIZE.ws_row, CURSOR_START_COL_FOR_COMMAND_MODE);
            cout<<"\033[0J";    //clears from cursor to end of screen
            return command;
        }
        else if(c == 127 && CURSOR_COL != CURSOR_START_COL_FOR_COMMAND_MODE)
        {
            command.pop_back();
            moveCursorToColumn(CURSOR_START_COL_FOR_COMMAND_MODE);
            cout<<"\033[0J";    //clears from cursor to end of screen
            cout<<command;
            --CURSOR_COL;
        }
        else if (c != 127)
        {
            cout<<c;
            command.push_back(c);
            ++CURSOR_COL;
        }
    }
}

void rename_wrapper(vector<string>& command_tokens)
{
    if(command_tokens.size() != 3)
    {
        print_message("Wrong number of arguments. Correct usage: 'rename old_name new_name'");
    }
    else
    {
        string old = convert_to_abs_path(command_tokens[1]);
        string nu = convert_to_abs_path(command_tokens[2]);
        int retCode = rename(old.c_str(), nu.c_str());
        if(retCode == 0) print_message("Rename succesful!");
        else print_message("Operation failed.");
    }
}



//returns file name from path. Doesnt matter if path is absolute, relative or just file name
string get_fileName_from_path(string path)
{
    /* use this snippet to debug from calling function
    vector<string> t;
    t.push_back(get_fileName_from_path(src));
    moveCursor(WIN_SIZE.ws_row - 3, 1);
    for(int i = 0; i < t.size(); i++) cout<<t[i]<<" ";
    */
    int c = 0;
    int path_idx = path.size() - 1;
    while(path_idx >= 0 && path[path_idx] != '/') 
    {
        ++c;
        --path_idx;
    }
    if(path_idx == 0) return path; //only file name was given
    else return path.substr(path_idx + 1, c); //path was given
}

//responsible for moving single file to destination. returns 0 if successful
int move_helper(string src, string dest_dir, string src_path_type)
{
    string fileName = get_fileName_from_path(src);
    string dest;
    if(src_path_type == "abs")
    {
        //dest_dir is system root
        if(dest_dir.size() == 1) dest = dest_dir + fileName;
        else dest = dest_dir + '/' + fileName;
        return rename(src.c_str(), dest.c_str());
    }
    else {}
    return 0;
}

int move_file(string src, string dest_dir)
{
    string src_path_type = get_path_type(src);
    string fileName = get_fileName_from_path(src);
    string dest;
    if(dest_dir.size() == 1) dest = dest_dir + fileName;  //dest_dir is root
    else dest = dest_dir + '/' + fileName;
    if(src_path_type != "abs") src = PWD_STR + '/' + fileName;
    return rename(src.c_str(), dest.c_str());
}

//moves all source files to destination. Takes help of move_helper function
void move(vector<string>& command_tokens)
{
    if(command_tokens.size() < 3)
    {
        print_message("Wrong number of arguments. Correct usage: move <source file(s)> <destination_directory>");
    }
    else
    {
        int fails = 0;
        string dest_dir = command_tokens.back();
        // print_debug(dest_dir);
        for(int i = 1; i < command_tokens.size() - 1; i++)
        {
            string fileName = get_fileName_from_path(command_tokens[i]);
            string dest;
            if(dest_dir == "/") dest = dest_dir + fileName;
            else if (dest_dir[0] == '~') dest = HOME_PATH + dest_dir.substr(1) + '/' + fileName;
            else dest = dest_dir + '/' + fileName;
            string src = command_tokens[i];
            if(src[0] == '~') src = convert_to_abs_path(src);
            int retCode = rename(src.c_str(), dest.c_str());
            if(retCode) ++fails;
        }
        if(fails == 0) print_message("Operation succesful!");
        else print_message("Failed to move " + to_string(fails) + " items");
    }
}

//searches for file or directory with name query recursively in directory given by path
//returns 0 if found. returns 1 otherwise
int search_helper(string path, string query)
{
    DIR* di = opendir(path.c_str());
    if(!di) return 1;
    updatePWD(path);
    struct stat fileInfo;
    struct dirent* ditem;
    while((ditem = readdir(di)))
    {
        lstat(ditem->d_name, &fileInfo);
        string fileName = ditem->d_name;
        if(fileName == query) return 0;
        if(S_ISDIR(fileInfo.st_mode))
        {
            if(fileName == "." || fileName == "..") continue;
            int retCode = search_helper(path + '/' + fileName, query);
            if(retCode == 0) return retCode;
        }
        
    }
    updatePWD("..");
    closedir(di);
    return 1;
}

void search(vector<string>& command_tokens)
{
    if(command_tokens.size() != 2)
    {
        print_message("Wrong number of arguments. Correct usage: 'search <file_name>' or 'search <directory_name>'");
    }
    else
    {
        string query = command_tokens[1];
        int retCode = search_helper(PWD_STR, query);
        if(retCode == 0) print_message("True");
        else print_message("False");
    }
}



//Prints contents of pwd and enters raw mode
void enter_normal_mode()
{
    
    list_dir_contents(PWD_STR);
    enter_raw_mode();
    print_status("Normal Mode");
    moveCursorToHome();   
}

void up()
{
    if(CURSOR_ROW > 1)
    {
        moveCursorUp();
        DIR_CONTENTS_IDX--;
    }
}

void down()
{
    if(CURSOR_ROW < CURSOR_ROW_MAX)
    {
        moveCursorDown();
        DIR_CONTENTS_IDX++;
    }
}

void left()
{
    if(BACK_ST.empty())
    {
        print_message("Cannot go back. You are back to where you started from.");
        return;
    }
    else
    {
        FORWARD_ST.push(PWD_STR);
        updatePWD(BACK_ST.top());
        BACK_ST.pop();
        list_dir_contents(PWD);
        print_status("Normal Mode");
        moveCursorToHome();
    }
}

void right()
{
    if(FORWARD_ST.empty())
    {
        print_message("You cannot go forward without going back");
        return;
    }
    else
    {
        BACK_ST.push(PWD_STR);
        updatePWD(FORWARD_ST.top());
        FORWARD_ST.pop();
        list_dir_contents(PWD);
        print_status("Normal mode");
        moveCursorToHome();
    }
}

void home()
{
    BACK_ST.push(PWD_STR);
    updatePWD(HOME_PATH); //now PWD_STR = /
    list_dir_contents(PWD_STR);
    print_message("You are now in system home");
    print_status("Normal mode");
    moveCursorToHome();
}



void goto_dir(vector<string>& command_tokens)
{
    if(command_tokens.size() != 2)
    {
        print_message("Wrong number of arguments. Correct usage: 'goto <directory_path>'");
    }
    else
    {
        string dest_dir = command_tokens[1];
        string dest_dir_path_type = get_path_type(dest_dir);
        if(dest_dir_path_type != "abs") dest_dir = convert_to_abs_path(dest_dir);
        int retCode = list_dir_contents(dest_dir);
        if(retCode) 
        {
            if(dest_dir.back() != '.') BACK_ST.push(PWD_STR);
            updatePWD(dest_dir); //non 0 return code means directory_path is valid
            print_message("Operation succesful!");
        }
        else print_message("Operation failed. Check if directory exists or if you have permissions");
    }
}

void create_file(vector<string>& command_tokens)
{
    if(command_tokens.size() != 3)
    {
        print_message("Wrong number of arguments. Correct usage: 'create_file <file_name> <destination_path>'");
    }
    else
    {
        string dest = command_tokens.back();
        string fileName = command_tokens[1];
        //string dest_path = convert_to_abs_path(dest) + '/' + fileName;
        string dest_path = convert_to_abs_path(dest);
        if(dest_path == "/") dest_path += fileName;
        else dest_path = dest_path + '/' + fileName;
        int fd = open(dest_path.c_str(), O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR);
        if(fd == -1) print_message("Failed to create file");
        else print_message ("File created successfully");
    }
}

void create_dir(vector<string>& command_tokens)
{
    if(command_tokens.size() != 3)
    {
        print_message("Wrong number of arguments. Correct usage: 'create_dir <directory_name> <destination_path>'");
    }
    else
    {
        string dest = command_tokens.back();
        string dirName = command_tokens[1];
        string dest_path = convert_to_abs_path(dest);
        if(dest_path == "/") dest_path += dirName;
        else dest_path = dest_path + '/' + dirName;
        int retCode = mkdir(dest_path.c_str(), S_IRUSR|S_IWUSR|S_IXUSR);
        if(retCode == -1) print_message("Failed to create directory");
        else print_message("Directory created successfully");
    }
}

int copy_file(string src, string dest_dir)
{
    char buf[BUF_SIZE];
    string dest;
    string fileName = get_fileName_from_path(src);
    if(dest_dir == "/") dest = dest_dir + fileName;
    else dest = dest_dir + "/" + fileName;
    struct stat infileInfo;
    lstat(src.c_str(), &infileInfo);
    int fin = open(src.c_str(), O_RDONLY);
    int fout = open(dest.c_str(), O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR);
    //uncomment below line if operation fails. -1 indicates that the file / folder could not be opened
    //print_debug("fin: " + to_string(fin) + ", fout: " + to_string(fout));
    if(fin == -1 || fout == -1) return 1;
    int bytesRead;
    while((bytesRead = read(fin, buf, BUF_SIZE)) > 0) write(fout, buf, bytesRead);
    close(fin);
    close(fout);
    chmod(dest.c_str(), infileInfo.st_mode);
    return 0;
}

int  copy_dir_helper(string src, string dest_dir, int fails)
{
    DIR* ds; //directory stream
    struct dirent* ent; //directory entry
    struct stat fileInfo;
    if(!(ds = opendir(src.c_str())))
    {
        print_message("Cannot open directory");
        ++fails;
        return fails;
    }
    updatePWD(src);
    //print_debug("src: " + PWD_STR + ", dest: " + dest_dir);
    while((ent = readdir(ds)))
    {
        lstat(ent->d_name, &fileInfo);
        string name = ent->d_name;
        //print_debug("name: " + name);
        if(S_ISDIR(fileInfo.st_mode))
        {
            if(name == "." || name == "..") continue;
            mkdir((dest_dir + '/' + name).c_str(), S_IRUSR|S_IWUSR|S_IXUSR);
            fails += copy_dir_helper("./" + name, dest_dir + '/' + name, fails);
        }
        else
        {
            //print_debug("copy_file(" + PWD_STR + '/' + name + ", " + dest_dir);
            fails += copy_file(PWD_STR + '/' + name, dest_dir);
        } 
    }
    updatePWD("..");
    closedir(ds);
    return fails;
}

int copy_dir(string src, string dest)
{
    int fails = 0;
    string dirName = get_fileName_from_path(src);
    string dest_path;
    if(dest == "/") dest_path = dest + dirName;
    else dest_path = dest + '/' + dirName;
    mkdir(dest_path.c_str(), S_IRUSR|S_IWUSR|S_IXUSR);
    return copy_dir_helper(src, dest_path, fails);
    //return 0;
}

void copy(vector<string>& command_tokens)
{
    if(command_tokens.size() < 3)
    {
        print_message("Wrong number of arguments. Correct usage: copy <source_file(s) / destination_directory(s)> <destination_directory");
    }
    else
    {
        int fails = 0;
        int retCode;
        string dest_dir = command_tokens.back();
        string dest_path = convert_to_abs_path(dest_dir);
        struct stat fileInfo;
        for(int i = 1; i < command_tokens.size() - 1; i++)
        {
            string src = convert_to_abs_path(command_tokens[i]);
            struct stat fileInfo;
            lstat(command_tokens[i].c_str(), &fileInfo);
            if(S_ISDIR(fileInfo.st_mode))
            {
                //updatePWD(command_tokens[i]);
                retCode = copy_dir(src, dest_path);
                fails += retCode;
            }
            else
            {
                retCode = copy_file(src, dest_path);
                if(retCode) ++fails;
            }
        }
        //list_dir_contents(PWD_STR);
        if(fails == 0) print_message("Operation succesful!");
        else print_message("Failed to move " + to_string(fails) + " items");
    }
}

void delete_file(vector<string>& command_tokens)
{
    if(command_tokens.size() != 2)
    {
        print_message("Wrong number of arguments. Correct usage: 'delete_file <file_path>'");
    }
    else
    {
        string path = convert_to_abs_path(command_tokens[1]);
        int retCode = unlink(path.c_str());
        if(retCode == 0) print_message("Operation successful!");
        else print_message("Operation failed. Check file permissions and existence");
    }
}

int delete_dir_helper(string path, int fails)
{
    DIR* ds;  //directory stream
    struct dirent* ent;  //directory entry
    struct stat fileInfo;
    if(!(ds = opendir(path.c_str())))
    {
        ++fails;
        return fails;
    }
    updatePWD(path);
    while((ent = readdir(ds)))
    {
        lstat(ent->d_name, &fileInfo);
        string name = ent->d_name;
        if(S_ISDIR(fileInfo.st_mode))
        {
            if(name == "." || name == "..") continue;
            fails += delete_dir_helper(name, fails);
            fails += abs(rmdir(ent->d_name));
        }
        else fails += abs(unlink(ent->d_name));
    }
    updatePWD("..");
    closedir(ds);
    return fails;
}

int delete_dir(vector<string>& command_tokens)
{
    if(command_tokens.size() != 2)
    {
        print_message("Wrong number of arguments. Correct usage: 'delete_dir <directory_path>'");
    }
    else
    {
        string path = convert_to_abs_path(command_tokens[1]);
        int fails = 0;
        fails = delete_dir_helper(path, fails);
        fails += abs(rmdir(path.c_str()));
        if (fails == 0) print_message("Operation successful");
        else print_message(to_string(fails) + " items could not be deleted");
    }
    return 0;
}

void execute_command(vector<string>& command_tokens)
{
    if(command_tokens.empty()) print_message("Please enter a command");
    else if (command_tokens[0] == "rename") rename_wrapper(command_tokens);
    else if (command_tokens[0] == "move") move(command_tokens);
    else if (command_tokens[0] == "goto") goto_dir(command_tokens);
    else if (command_tokens[0] == "search") search(command_tokens);
    else if (command_tokens[0] == "create_file") create_file(command_tokens);
    else if (command_tokens[0] == "create_dir") create_dir(command_tokens);
    else if (command_tokens[0] == "copy") copy(command_tokens);
    else if (command_tokens[0] == "delete_file") delete_file(command_tokens);
    else if (command_tokens[0] == "delete_dir") delete_dir(command_tokens);
    else print_message("'" + command_tokens[0] + "' command doesn't exist");
    print_status("Status mode");
    print_enter_command();
    moveCursor(WIN_SIZE.ws_row, CURSOR_START_COL_FOR_COMMAND_MODE);
}

void enter_command_mode()
{
    clear_messages();
    print_status("Command Mode");
    moveCursor(WIN_SIZE.ws_row, 1);
    print_enter_command();
    CURSOR_COL = CURSOR_START_COL_FOR_COMMAND_MODE;
    //exit_raw_mode();
    while(1)
    {
        string command = get_command();
        if(command == "esc") break;
        stringstream ss(command);
        vector<string> command_tokens;
        string token;
        while(ss>>token) command_tokens.push_back(token);
        execute_command(command_tokens);
    }
    enter_normal_mode();
}

void goto_parent_dir(string path)
{
    if(path == "/")
    {
        print_message("You are already in system root. Cannot go to parent directory");
        return;
    }
    string tempPWD = path;
    while(tempPWD.back() != '/') tempPWD.pop_back();
    if(tempPWD.size() > 2) tempPWD.pop_back();
    vector<string> params = {"goto", tempPWD};
    goto_dir(params);
    print_status("Normal mode");
}

void enter()
{
    struct stat fileInfo;
    lstat(DIR_CONTENTS[DIR_CONTENTS_IDX].c_str(), &fileInfo);
    if(S_ISDIR(fileInfo.st_mode))
    {
        string dirName = DIR_CONTENTS[DIR_CONTENTS_IDX];
        if(dirName == ".") return;
        if (dirName == "..") goto_parent_dir(PWD_STR);
        else
        {
            string tempPWD = PWD_STR + '/' + dirName;
            int retCode = list_dir_contents(tempPWD);
            if(retCode)
            {
                BACK_ST.push(PWD);
                updatePWD(tempPWD);
            }
        }
        print_status("Normal Mode");
        moveCursorToHome();
    }
    else
    {
        const char* fileName = DIR_CONTENTS[DIR_CONTENTS_IDX].c_str();
        pid_t pid = fork();
        if(pid == 0)
        {
            execl(VI_PATH.c_str(), "vi", fileName, NULL);
            exit(0);
        }
        else wait(NULL);
    }
}

void scroll_up()
{
    int current_cursor_row = CURSOR_ROW;
    if(DIR_CONTENTS_S_IDX > 0)
    {
        --DIR_CONTENTS_IDX;
        --DIR_CONTENTS_S_IDX;
        --DIR_CONTENTS_E_IDX;
        print_DIR_CONTENTS(DIR_CONTENTS_S_IDX, DIR_CONTENTS_E_IDX);
    }
    else print_message("No more items to show. Cannot scroll up");
    moveCursor(current_cursor_row, 1);   
}

void scroll_down()
{
    int current_cursor_row = CURSOR_ROW;
    if(DIR_CONTENTS_E_IDX < DIR_CONTENTS.size() - 1)
    {
        ++DIR_CONTENTS_IDX;
        ++DIR_CONTENTS_S_IDX;
        ++DIR_CONTENTS_E_IDX;
        print_DIR_CONTENTS(DIR_CONTENTS_S_IDX, DIR_CONTENTS_E_IDX);
    }
    else print_message("No more items to show. Cannot scroll down");
    moveCursor(current_cursor_row, 1);
}

/* ---Main--- */
int main()
{
    setup_terminal();
    initialize_globals();
    enter_normal_mode();
    char c;
    while(c != 'q')
    {
        c = cin.get();
        if(c == 65) up();
        else if (c == 66) down();
        else if (c == 67) right();
        else if (c == 68) left();
        else if (c == 10) enter();  
        else if (c == 127) goto_parent_dir(PWD_STR);
        else if (c == 104) home();
        else if (c == 'k') scroll_up();
        else if (c == 'l') scroll_down();
        else if (c == ':') enter_command_mode(); 
    }
    atexit(exit_raw_mode);
    return 0;
}