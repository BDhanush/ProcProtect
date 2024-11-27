#include <string>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <set>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include <nlohmann/json.hpp>
#include <curl/curl.h>

namespace fs = std::filesystem;
using namespace std;
using json = nlohmann::json;

std::ofstream antiVirus("antivirus.txt"); 

string MODEL="llama3.2";
string URL="http://127.0.0.1:11434/api/generate";

int CONTEXT_LENGTH=128000;

set<string> pathsToSearch={
    "cmdline",
    "exe",
    "fd",
    "maps",
    "status",
    "net"

};

string to_json_value_format(const string& raw_string) {
    // Use nlohmann::json to escape the string as a JSON value (without a key)
    json json_value = raw_string; // Directly assign raw_string to a JSON value
    return json_value.dump();               // Serialize the JSON value to a string
}

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

void printEval(string& fileName,string& info)
{
    int i=0;
    while(i<info.length() && !((info[i]>='a' && info[i]<='z') || info[i]>='A' && info[i]<='Z'))
    {
        i++;
    }
    string firstWord="";
    for(;i<info.length() && ((info[i]>='a' && info[i]<='z') || info[i]>='A' && info[i]<='Z');i++)
    {
        firstWord+=info[i];
        if(firstWord.back()<='a')
        {
            firstWord.back()+=32;
        }
    }
    set<string> neg={"empty","no","not","nothing","none"};
    if(neg.find(firstWord)==neg.end())
        antiVirus << "\n" << fileName << ":\n" << info << std::endl;

}

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* response) {
    size_t totalSize = size * nmemb;
    response->append((char*)contents, totalSize);
    return totalSize;
}

int ollama_get(std::string fileName, std::string contents) {
    CURL* curl;
    CURLcode res;
    std::string response;

    curl = curl_easy_init();
    if (curl) {
        json payload = {
            {"model", MODEL},
            {"system", "You are an antivirus that takes input the contents of a file from the proc file system and tells if the process is a malware. ONLY RESPOND IF YOU THINK THE PROCESS IS A MALWARE ELSE GIVE EMPTY OUTPUT."},
            {"prompt", "file name:"+fileName+"\n"+contents},
            {"stream", false},
            {"options", {
                    {"num_ctx", CONTEXT_LENGTH}
                }
            }
        };
        
        std::string jsonPayload = payload.dump();
        curl_easy_setopt(curl, CURLOPT_URL, URL.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonPayload.c_str());
        
        // Set headers, especially Content-Type
        struct curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            // std::cout << response << std::endl;

            try {
                json jsonResponse = json::parse(response);
                if (jsonResponse.contains("response")) {
                    string info=jsonResponse["response"];
                    printEval(fileName,info);
                } else {
                    std::cerr << "Response field not found in the JSON response." << std::endl;
                }
            } catch (const json::exception& e) {
                std::cerr << "Error parsing JSON response: " << e.what() << std::endl;
            }
        }

        // Cleanup
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }

    return 0;
}


void sendChunks(string& path,string& contents)
{
    string chunk="";
    for(int i=0;i<contents.length();i++)
    {
        chunk+=contents[i];
        if(chunk.length()==CONTEXT_LENGTH-100)
        {
            ollama_get(path,chunk);
            chunk="";
        }
    }
    if(chunk.length())
    {
        ollama_get(path,chunk);
    }
}

void readFile(string& path,ofstream& jsonData)
{    
    int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
    
    char buffer[10000];
    ssize_t bytesRead;

    string value="";

    while (true) {
        bytesRead = read(fd, buffer, sizeof(buffer) - 1);
        if (bytesRead <= 0 || buffer[0]=='\0') {
            // End of file
            break;
        }
        buffer[bytesRead] = '\0'; // Null-terminate the string
        for(int i=0;i<sizeof(buffer) && buffer[i]!='\0';i++)
        {
            // if(i==0 || buffer[i]=='\n')
            // {
            //     printDepth(depth,"",jsonData);
            // }
            // jsonData<<buffer[i];
            value+=buffer[i];
        }
    }
    try{
        string valueJson=to_json_value_format(value);
        jsonData<<valueJson;
        sendChunks(path,valueJson);
    }catch(exception e){
        value="";
        jsonData<<to_json_value_format(value);

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

int getPid(string& path)
{
    int i=0;
    int slashCount=0;
    while(i<path.length() && slashCount<2)
    {
        slashCount+=path[i]=='/';
        i++;
    }
    int ret=0;
    for(;i<path.length() && path[i]!='/';i++)
    {
        if(!(path[i]>='0' && path[i]<='9'))
        {
            return -1;
        }
        ret*=10;
        ret+=path[i]-'0';
    }
    return ret;
}

void crawl(string path,ofstream& jsonData)
{
    // printDepth(depth,"\"",jsonData);
    if(!fs::is_directory(path))
    {
        if(pathsToSearch.find(lastDir(path))!=pathsToSearch.end() && getPid(path)>100000)
        {
            // cout<<path<<" "<<getPid(path)<<endl;
            // jsonData<<"\"\n";
            jsonData<<"\"";
            jsonData<<path;
            jsonData<<"\": ";
            readFile(path,jsonData);
            jsonData<<"\n,\n";
            // printDepth(depth,"\"\n",jsonData);
        }
        return;
    }
    for (const auto & entry : fs::directory_iterator(path)){
        string newPath=entry.path();
        if(!fs::is_symlink(newPath) && is_readable(newPath)){
            crawl(newPath,jsonData);
        }
    }
    
}

void daemonize() {
    pid_t pid = fork(); // Create a child process
    
    if (pid < 0) {
        std::cerr << "Fork failed!\n";
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        // Parent process exits
        exit(EXIT_SUCCESS);
    }

    // Child process continues
    if (setsid() < 0) {
        std::cerr << "Failed to create new session.\n";
        exit(EXIT_FAILURE);
    }

    // Redirect standard file descriptors (optional but useful)
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    open("/dev/null", O_RDONLY);   // Redirect stdin to /dev/null
    open("/dev/null", O_RDWR);     // Redirect stdout to /dev/null
    open("/dev/null", O_RDWR);     // Redirect stderr to /dev/null
}

int main()
{
    daemonize();
    ofstream jsonData("ProcJson.json");
    std::string path = "/proc/";
    jsonData<<"{\n";
    crawl(path,jsonData);
    // ollama_get("","");
    jsonData<<"}";
    return 0;
    
}