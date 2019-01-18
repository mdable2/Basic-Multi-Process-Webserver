#include <fnmatch.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <dirent.h>

#define BACKLOG (10)
const int NUM_THREADS = 1;

struct threadArgument {
    int socket;
};

void *threadFunction(void*);

void serve_request(int);

// MIME types
char * mime_ico = "HTTP/1.0 200 OK\r\n"
        "Content-type: image/x-icon; charset=UTF-8\r\n\r\n";
char * mime_gif = "HTTP/1.0 200 OK\r\n"
        "Content-type: image/gif; charset=UTF-8\r\n\r\n";
char * mime_html = "HTTP/1.0 200 OK\r\n"
        "Content-type: text/html; charset=UTF-8\r\n\r\n";
char * mime_jpg = "HTTP/1.0 200 OK\r\n"
        "Content-type: image/jpeg; charset=UTF-8\r\n\r\n";
char * mime_png = "HTTP/1.0 200 OK\r\n"
        "Content-type: image/png; charset=UTF-8\r\n\r\n";
char * mime_pdf = "HTTP/1.0 200 OK\r\n"
        "Content-type: application/pdf; charset=UTF-8\r\n\r\n";

// 404 page
char * error_404 = "HTTP/1.0 404 Not Found\r\n"
        "Content-type: text/html; charset=UTF-8\r\n\r\n"
        "<html><head><title>Page Not Found</title></head>"
        "<body>Error 404! This page does not exist.</body></html>";

char * index_hdr = "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\"><html>"
        "<title>Directory listing for %s</title>"
"<body>"
"<h2>Directory listing for %s</h2><hr><ul>";

// snprintf(output_buffer,4096,index_hdr,filename,filename);

char * index_body = "<li><a href=\"%s\">%s</a>";

char * index_ftr = "</ul><hr></body></html>";

/* char* parseRequest(char* request)
 * Args: HTTP request of the form "GET /path/to/resource HTTP/1.X" 
 *
 * Return: the resource requested "/path/to/resource"
 *         0 if the request is not a valid HTTP request 
 * 
 * Does not modify the given request string. 
 * The returned resource should be free'd by the caller function. 
 */
char* parseRequest(char* request) {
  //assume file paths are no more than 256 bytes + 1 for null. 
  char *buffer = malloc(sizeof(char)*257);
  memset(buffer, 0, 257);
  
  if(fnmatch("GET * HTTP/1.*",  request, 0)) return 0; 

  sscanf(request, "GET %s HTTP/1.", buffer);
  return buffer; 
}

int isDir(const char *path) {
    struct stat pathS;
    stat(path, &pathS);
    return S_ISDIR(pathS.st_mode);
}

int isFile(const char *path) {
    struct stat pathS;
    stat(path, &pathS);
    return S_ISREG(pathS.st_mode);
}

void serve_request(int client_fd){
    int read_fd;
    int check;
    int bytes_read;
    int file_offset = 0;
    char client_buf[4096];
    char send_buf[4096];
    char out_buf[4096];
    char filename[4096];
    char mime_type[4096];
    char * requested_file;
    memset(client_buf,0,4096);
    memset(filename,0,4096);
    while(1){

        file_offset += recv(client_fd,&client_buf[file_offset],4096,0);
        if(strstr(client_buf,"\r\n\r\n"))
        break;
    }
    requested_file = parseRequest(client_buf);

    // take requested_file, add directory path to beginning, open that file
    filename[0] = '.';
    strncpy(&filename[1],requested_file,4095);

    // Check if path is directory
    if (isDir(filename)) {
        char temp[4096];
        strcpy(temp, filename);
        strcat(temp, "/index.html");

        if (isFile(temp)) {
            strcpy(mime_type, mime_html);
            strcpy(filename, temp);
        }
        else {
            check = send(client_fd,mime_html,strlen(mime_html),0);
            if (check < 0) {
                perror("Sending out_buf error failed.[1]\n");
                exit(1);
            }
            char * directory_listing = NULL;
            snprintf(out_buf, 4096, index_hdr, requested_file, requested_file);
            check = send(client_fd,out_buf,strlen(out_buf),0);
            if (check < 0) {
                perror("Sending out_buf error failed.[1]\n");
                exit(1);
            }
            DIR* path = opendir(filename);
            printf("filename: %s\n", filename);

            if (path != NULL) {
                directory_listing = (char*) malloc(sizeof(char)*1013);
                directory_listing[0] = '\0';

                // Stores underlying info of files and sub_directories of directory_path
                struct dirent* underlying_file = NULL;

                // Iterate through all of the underlying files of directory_path
                while((underlying_file = readdir(path)) != NULL) {
                    printf("underlying_file: %s\n", underlying_file->d_name);
                    snprintf(out_buf, 4096, index_body, underlying_file->d_name, underlying_file->d_name);
                    check = send(client_fd,out_buf,strlen(out_buf),0);
                    if (check < 0) {
                        perror("Sending out_buf error failed.[1]\n");
                        exit(1);
                    }
                }
                snprintf(out_buf, 4096, index_ftr);
                check = send(client_fd,out_buf,strlen(out_buf),0);
                if (check < 0) {
                    perror("Sending out_buf error failed.[1]\n");
                    exit(1);
                }
                closedir(path);
            }
            close(client_fd);
            return;
        }
    }
    // This is a file
    else if (isFile(filename)) {
        if (strstr(filename, ".html")) strcpy(mime_type, mime_html);
        else if (strstr(filename, ".gif")) strcpy(mime_type, mime_gif);
        else if (strstr(filename, ".ico")) strcpy(mime_type, mime_ico);
        else if (strstr(filename, ".jpg")) strcpy(mime_type, mime_jpg);
        else if (strstr(filename, ".png")) strcpy(mime_type, mime_png);
        else if (strstr(filename, ".pdf")) strcpy(mime_type, mime_pdf);
    }

    // This is a 404 error
    else {
        check = send(client_fd,error_404,strlen(error_404),0);
        if (check < 0) {
            perror("Sending in 404 error failed.");
            exit(1);
        }
        close(client_fd);
        return;
    }

    check = send(client_fd,mime_type,strlen(mime_type),0);
    if (check < 0) {
        perror("Sending mime_type error failed.");
        exit(1);
    }
    read_fd = open(filename,0,0);
    while(1){
        bytes_read = read(read_fd,send_buf,4096);
        if(bytes_read == 0)
        break;

        check = send(client_fd,send_buf,bytes_read,0);
        if (check < 0) {
            perror("Sending bytes_read error failed.");
            exit(1);
        }
    }
    close(read_fd);
    close(client_fd);
    return;
}

void *threadFunction(void* args) {
    // get the casted args
    struct threadArgument* myArg = (struct threadArgument*) args;

    // serve the request on this thread and pass in the socket from the arg struct
    serve_request(myArg->socket);

    // close the socket
    close(myArg->socket);

    return NULL;
}

/* Your program should take two arguments:
 * 1) The port number on which to bind and listen for connections, and
 * 2) The directory out of which to serve files.
 */
int main(int argc, char** argv) {
    /* For checking return values. */
    int retval;

    /* Read the port number from the first command line argument. */
    int port = atoi(argv[1]);
    chdir(argv[2]);

    /* Create a socket to which clients will connect. */
    int server_sock = socket(AF_INET6, SOCK_STREAM, 0);
    if(server_sock < 0) {
        perror("Creating socket failed");
        exit(1);
    }

    /* A server socket is bound to a port, which it will listen on for incoming
     * connections.  By default, when a bound socket is closed, the OS waits a
     * couple of minutes before allowing the port to be re-used.  This is
     * inconvenient when you're developing an application, since it means that
     * you have to wait a minute or two after you run to try things again, so
     * we can disable the wait time by setting a socket option called
     * SO_REUSEADDR, which tells the OS that we want to be able to immediately
     * re-bind to that same port. See the socket(7) man page ("man 7 socket")
     * and setsockopt(2) pages for more details about socket options. */
    int reuse_true = 1;
    retval = setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &reuse_true,
                        sizeof(reuse_true));
    if (retval < 0) {
        perror("Setting socket option failed");
        exit(1);
    }

    /* Create an address structure.  This is very similar to what we saw on the
     * client side, only this time, we're not telling the OS where to connect,
     * we're telling it to bind to a particular address and port to receive
     * incoming connections.  Like the client side, we must use htons() to put
     * the port number in network byte order.  When specifying the IP address,
     * we use a special constant, INADDR_ANY, which tells the OS to bind to all
     * of the system's addresses.  If your machine has multiple network
     * interfaces, and you only wanted to accept connections from one of them,
     * you could supply the address of the interface you wanted to use here. */
    
   
    struct sockaddr_in6 addr;   // internet socket address data structure
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port); // byte order is significant
    addr.sin6_addr = in6addr_any; // listen to all interfaces

    
    /* As its name implies, this system call asks the OS to bind the socket to
     * address and port specified above. */
    retval = bind(server_sock, (struct sockaddr*)&addr, sizeof(addr));
    if(retval < 0) {
        perror("Error binding to port");
        exit(1);
    }

    /* Now that we've bound to an address and port, we tell the OS that we're
     * ready to start listening for client connections.  This effectively
     * activates the server socket.  BACKLOG (#defined above) tells the OS how
     * much space to reserve for incoming connections that have not yet been
     * accepted. */
    retval = listen(server_sock, BACKLOG);
    if(retval < 0) {
        perror("Error listening for connections");
        exit(1);
    }

    while(1) {
        /* Declare a socket for the client connection. */
        int sock;
        char buffer[256];

        /* Another address structure.  This time, the system will automatically
         * fill it in, when we accept a connection, to tell us where the
         * connection came from. */
        struct sockaddr_in remote_addr;
        unsigned int socklen = sizeof(remote_addr); 

        /* Accept the first waiting connection from the server socket and
         * populate the address information.  The result (sock) is a socket
         * descriptor for the conversation with the newly connected client.  If
         * there are no pending connections in the back log, this function will
         * block indefinitely while waiting for a client connection to be made.
         * */
        sock = accept(server_sock, (struct sockaddr*) &remote_addr, &socklen);
        if(sock < 0) {
            perror("Error accepting connection");
            exit(1);
        }

        pthread_t* thread = malloc(NUM_THREADS * sizeof(pthread_t));
        struct threadArgument* args = malloc(NUM_THREADS * sizeof(struct threadArgument));
        args->socket = sock;
        pthread_create(thread, NULL, threadFunction, args);

        /* At this point, you have a connected socket (named sock) that you can
         * use to send() and recv(). */

        /* ALWAYS check the return value of send().  Also, don't hardcode
         * values.  This is just an example.  Do as I say, not as I do, etc. */
        //serve_request(sock);

        /* Tell the OS to clean up the resources associated with that client
         * connection, now that we're done with it. */
        //close(sock);
    }

    close(server_sock);
}
