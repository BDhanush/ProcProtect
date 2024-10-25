#include <string>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <set>
#include <chrono>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>

namespace fs = std::filesystem;
using namespace std;

set<string> pathsToSkip={
    // "/proc/keys","/proc/kmsg","/proc/kcore","/proc/1","/proc/2"
    "/proc/cpuinfo",
    "/proc/meminfo",
    "/proc/interrupts",
    "/proc/iomem",
    "/proc/modules",
    // "net",

};

bool is_readable(const fs::path& path) {
    try {
        auto perms = fs::status(path).permissions();
        if ((perms & fs::perms::owner_read) != fs::perms::none ||
            (perms & fs::perms::group_read) != fs::perms::none ||
            (perms & fs::perms::others_read) != fs::perms::none) {
            return true;  // Path has read permissions
        }
    } catch (const fs::filesystem_error& e) {
        cerr << "Error checking permissions: " << e.what() << '\n';
    }
    return false;  // Path is not readable or an error occurred
}

void printDepth(int depth,string s,ofstream& jsonData)
{
    jsonData<<string(depth, '\t');
    jsonData<<s;
}

void readFile(string& path,int depth,ofstream& jsonData)
{    
    int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
    
    char buffer[10000];
    ssize_t bytesRead;

    while (true) {
        bytesRead = read(fd, buffer, sizeof(buffer) - 1);
        if (bytesRead <= 0 || buffer[0]=='\0') {
            // End of file
            break;
        }
        buffer[bytesRead] = '\0'; // Null-terminate the string
        for(int i=0;i<sizeof(buffer) && buffer[i]!='\0';i++)
        {
            if(i==0 || buffer[i]=='\n')
            {
                printDepth(depth,"",jsonData);
            }
            jsonData<<buffer[i];
        }
    }

    close(fd);

}

string lastDir(string& path)
{
    string s="";
    for(int i=path.length()-1;i>=0;i--)
    {
        if(path[i]=='/')
        {
            break;
        }
        s+=path[i];
    }
    reverse(s.begin(),s.end());
    return s;
}

void crawl(string path,int depth,ofstream& jsonData)
{
    printDepth(depth,"\"",jsonData);
    jsonData<<path;
    jsonData<<"\": ";
    if(!fs::is_directory(path))
    {
        jsonData<<"\"\n";
        readFile(path,depth+1,jsonData);
        printDepth(depth,"\"\n",jsonData);
        return;
    }else{
        jsonData<<"{\n";
    }
    bool first=true;
    for (const auto & entry : fs::directory_iterator(path)){
        string newPath=entry.path();
        if(!fs::is_symlink(newPath) && is_readable(newPath)){
            if(pathsToSkip.find(newPath)==pathsToSkip.end() && pathsToSkip.find(lastDir(newPath))==pathsToSkip.end())
            {
                if(!first){
                    printDepth(depth+1,",\n",jsonData);
                }
                crawl(newPath,depth+1,jsonData);
                first=false;
            }
        }
    }
    printDepth(depth,"}\n",jsonData);
    
}

int main()
{
    ofstream jsonData("ProcJson.json");
    std::string path = "/proc/";
    jsonData<<"{\n";
    crawl(path,1,jsonData);
    jsonData<<"}";
    return 0;
    
}