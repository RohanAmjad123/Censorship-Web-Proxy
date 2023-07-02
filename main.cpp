/*
Author: Rohan Amjad
Filename: main.cpp
Course: CPSC 441 - Computer Networks
Assignment: 1
Submission Date: Oct 1, 2021
*/

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <vector>
#include <unordered_map>
#include <string>
#include <algorithm>

using namespace std;

#define SERVER_PORT 8001
#define CLIENT_PORT 80
#define MAX_CLIENTS 3
#define BUFFER_SIZE 1024

vector<string> banned_words;

unordered_map<string, string> parse_HTTP(char *HTTP)
{
    vector<string> v;
    unordered_map<string, string> map;

    // break HTTP into vector of strings split by \n
    string str(HTTP);
    string temp = "";
    for (int i = 0; i < str.size(); i++)
    {
        if (str[i] == '\n')
        {
            temp.push_back(str[i]);
            v.push_back(temp);
            temp = "";
        }
        else
        {
            temp.push_back(str[i]);
        }
    }
    v.push_back(temp);

    // break each string in vector into key value pairs and insert into map
    for (int i = 0; i < v.size(); i++)
    {
        size_t pos = v[i].find(' ');
        string key = v[i].substr(0, pos);
        string value = "";
        if (pos < v[i].length())
        {
            value = v[i].substr(pos);
        }
        map.insert(make_pair(key, value));
    }

    return map;
}

bool is_banned(vector<string> banned, string s)
{
    if (banned.empty())
    {
        return false;
    }
    // convert s to lowercase
    transform(s.begin(), s.end(), s.begin(), ::tolower);
    puts(s.c_str());
    // check if any word from banned is found in s
    for (int i = 0; i < banned.size(); i++)
    {
        if (s.find(banned[i]) != string::npos)
        {
            return true;
        }
    }
    return false;
}

string modify_request(unordered_map<string, string> map)
{
    // modify map with url to error page
    if (map.find("GET") != map.end())
    {
        map.at("GET") = " https://pages.cpsc.ucalgary.ca/~carey/CPSC441/ass1/error.html HTTP/1.1\r\n";
    }
    // convert map to a string
    string s = "";
    for (auto &m : map)
    {
        s += m.first + m.second;
    }
    return s;
}

int main()
{
    struct sockaddr_in server_address;
    struct sockaddr_in client_address;
    int server_socket;
    int client_socket;

    fd_set read;
    int client_sockets[MAX_CLIENTS];

    // set all client sockets to 0
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        client_sockets[i] = 0;
    }

    // clear server and client addresse;
    memset(&server_address, 0, sizeof(server_address));
    memset(&client_address, 0, sizeof(client_address));

    // prepare sockaddr_in server_address struct
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_PORT);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    // prepare sockaddr_in client_address struct
    client_address.sin_family = AF_INET;
    client_address.sin_port = htons(CLIENT_PORT);

    // create server_socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        perror("\nCould not create server socket");
        exit(1);
    }

    // bind server_socket;
    int bind_status;
    if ((bind_status = bind(server_socket, (struct sockaddr *)&server_address, sizeof(struct sockaddr_in))) < 0)
    {
        perror("\nCould not bind server socket");
        close(server_socket);
        exit(1);
    }
    else
    {
        puts("\nBind complete");
    }

    // set server_socket to listening mode
    int listen_status;
    if ((listen_status = listen(server_socket, MAX_CLIENTS)) < 0)
    {
        perror("\nCould not listen for incoming connections");
        close(server_socket);
        exit(1);
    }
    else
    {
        puts("\n----------------------");
        puts("\nListening...");
    }

    char *buff[BUFFER_SIZE];
    char cli_snd_message[10000];
    char svr_rcv_message[10000];

    int max_socket_descr;
    int socket_descr;
    int activity;
    int new_socket;

    struct hostent *address;

    int bytes_rcv;
    int total_bytes_rcv = 0;

    while (1)
    {

        // clear socket set
        FD_ZERO(&read);
        total_bytes_rcv = 0;

        // add server_socket to socket set
        FD_SET(server_socket, &read);
        max_socket_descr = server_socket;

        // add client sockets to socket set
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            socket_descr = client_sockets[i];
            if (socket_descr > 0)
            {
                FD_SET(socket_descr, &read);
            }
            if (socket_descr > max_socket_descr)
            {
                max_socket_descr = socket_descr;
            }
        }

        activity = select(max_socket_descr + 1, &read, NULL, NULL, NULL);

        // if any activity occurs on server_socket incoming connection
        if (FD_ISSET(server_socket, &read))
        {
            // accept incoming connections
            if ((new_socket = accept(server_socket, NULL, NULL)) > 0)
            {
                puts("Connected!\n");
            }

            // add new socket to socket set
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (client_sockets[i] == 0)
                {
                    client_sockets[i] = new_socket;
                    break;
                }
            }
        }

        // clients
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            // clear send and recieve messages
            memset(&cli_snd_message, 0, sizeof(cli_snd_message));
            memset(&svr_rcv_message, 0, sizeof(svr_rcv_message));
            total_bytes_rcv = 0;

            socket_descr = client_sockets[i];

            if (FD_ISSET(socket_descr, &read))
            {
                // recieve message
                if (recv(socket_descr, svr_rcv_message, 10000, 0) < 0)
                {
                    perror("\nError recieving data from client");
                    close(socket_descr);
                    client_sockets[i] = 0;
                    continue;
                }
                strcpy(cli_snd_message, svr_rcv_message);

                // check if client wants to close the proxy
                char c[BUFFER_SIZE];
                sscanf(svr_rcv_message, "%s\r", c);
                string closing(c);
                if (closing == "close")
                {
                    puts("\nClosing...");
                    char cls_msg[BUFFER_SIZE] = "Closing...\n";
                    send(socket_descr, cls_msg, sizeof(cls_msg), 0);
                    // close current client socket and goto close proxy
                    close(socket_descr);
                    goto close_proxy;
                }

                if (strlen(svr_rcv_message) > 3)
                {
                    string t(svr_rcv_message);
                    string test = t.substr(0, 3);
                    transform(test.begin(), test.end(), test.begin(), ::tolower);

                    // if client wants to rmv a banned word
                    if (test == "rmv")
                    {
                        char word[BUFFER_SIZE];
                        memset(&word, 0, BUFFER_SIZE);
                        sscanf(svr_rcv_message, "rmv %s\r", word);
                        string t(word);
                        vector<string>::iterator r = find(banned_words.begin(), banned_words.end(), t);
                        if (r == banned_words.end())
                        {
                            char err_msg[BUFFER_SIZE] = "Word does not exist in banned words list\n";
                            send(socket_descr, err_msg, sizeof(err_msg), 0);
                        }
                        else
                        {
                            banned_words.erase(r);
                        }
                    }
                    // if clients wants to ban a word
                    else if (test == "add")
                    {
                        char word[BUFFER_SIZE];
                        memset(&word, 0, BUFFER_SIZE);
                        sscanf(svr_rcv_message, "add %s\r", word);
                        string t(word);
                        vector<string>::iterator r = find(banned_words.begin(), banned_words.end(), t);
                        if (r == banned_words.end())
                        {
                            banned_words.push_back(t);
                        }
                        else
                        {
                            char err_msg[BUFFER_SIZE] = "Word already exists in banned list\n";
                            send(socket_descr, err_msg, sizeof(err_msg), 0);
                        }
                    }
                    // if clients want list of banned words
                    else if (test == "lst")
                    {
                        char banned_list[BUFFER_SIZE];
                        memset(&banned_list, 0, BUFFER_SIZE);
                        strcat(banned_list, "Banned Words:\n");
                        for (auto &w : banned_words)
                        {
                            strcat(banned_list, w.c_str());
                            strcat(banned_list, "\n");
                        }
                        send(socket_descr, banned_list, sizeof(banned_list), 0);
                    }
                    // if client sends a get request
                    else if (test == "get")
                    {
                        unordered_map<string, string> parsed_HTTP = parse_HTTP(svr_rcv_message);

                        // check if url contains banned words
                        if (parsed_HTTP.find("GET") != parsed_HTTP.end())
                        {
                            if (is_banned(banned_words, parsed_HTTP.at("GET")))
                            {
                                printf("URL is banned\n");
                                string s = modify_request(parsed_HTTP);
                                strcpy(cli_snd_message, s.c_str());
                            }
                        }

                        // get host IP from host name and update client_address struct
                        address = gethostbyname("pages.cpsc.ucalgary.ca");
                        bcopy((char *)address->h_addr, (char *)&client_address.sin_addr.s_addr, address->h_length);

                        // create client_socket
                        if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
                        {
                            printf("Could not create client socket");
                            close(socket_descr);
                            client_sockets[i] = 0;
                            close(client_socket);
                            continue;
                        }

                        // connect to server
                        int status;
                        if ((status = connect(client_socket, (struct sockaddr *)&client_address, sizeof(struct sockaddr_in))) < 0)
                        {
                            printf("client connection failed");
                            close(socket_descr);
                            client_sockets[i] = 0;
                            close(client_socket);
                            continue;
                        }

                        // send request to server
                        send(client_socket, cli_snd_message, strlen(cli_snd_message), 0);

                        // clear buffer
                        memset(buff, 0, sizeof(buff));

                        // recieve data from server into buffer, and send buffer data to client
                        while ((bytes_rcv = recv(client_socket, buff, sizeof(buff), 0)) > 0)
                        {
                            total_bytes_rcv += bytes_rcv;
                            send(socket_descr, buff, bytes_rcv, 0);
                            memset(buff, 0, sizeof(buff));
                        }

                        // close client socket
                        close(client_socket);
                    }
                    else
                    {
                        char err_msg[BUFFER_SIZE] = "Sorry... request could not be handled by server\n";
                        send(socket_descr, err_msg, sizeof(err_msg), 0);
                    }
                }
                // close current client socket and remove from current clients
                puts("\nDisconnected");
                close(socket_descr);
                client_sockets[i] = 0;
            }
        }
    }

// close label
close_proxy:

    // close server socket
    close(server_socket);
    return 0;
}