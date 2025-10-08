/**
 *  Jiazi Yi
 * LIX, Ecole Polytechnique
 * jiazi.yi@polytechnique.edu
 *
 * Updated by Pierre Pfister
 * Cisco Systems
 * ppfister@cisco.com
 *
 * Updated by Kevin Jiokeng
 * LIX, Ecole Polytechnique
 * kevin.jiokeng@polytechnique.edu
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <errno.h>

#include "url.h"
#include "wgetX.h"

int main(int argc, char* argv[]) {
    url_info info;
    const char * file_name = "received_page";
    if (argc < 2) {
	fprintf(stderr, "Missing argument. Please enter URL.\n");
	return 1;
    }

    char *url = argv[1];

    // Get optional file name
    if (argc > 2) {
	file_name = argv[2];
    }

    // Make a copy of the URL for potential redirects
    char *current_url = strdup(url);
    if (current_url == NULL) {
	fprintf(stderr, "Memory allocation error\n");
	return 1;
    }

    // Follow redirects (max 10 to prevent infinite loops)
    int redirect_count = 0;
    const int MAX_REDIRECTS = 10;
    char *response = NULL;
    struct http_reply reply;

    while (redirect_count < MAX_REDIRECTS) {
	// Parse the current URL
	int ret = parse_url(current_url, &info);
	if (ret) {
	    fprintf(stderr, "Could not parse URL '%s': %s\n", current_url, parse_url_errstr[ret]);
	    free(current_url);
	    return 2;
	}

	// Download the page
	ret = download_page(&info, &reply);
	if (ret) {
	    free(current_url);
	    return 3;
	}

	// Check for redirects
	char *redirect_url = check_redirect(&reply);
	if (redirect_url == NULL) {
	    // No redirect, parse the response
	    response = read_http_reply(&reply);
	    break;
	}

	// We have a redirect
	fprintf(stderr, "Redirecting to: %s\n", redirect_url);
	free(reply.reply_buffer);
	free(current_url);
	current_url = strdup(redirect_url);
	if (current_url == NULL) {
	    fprintf(stderr, "Memory allocation error\n");
	    return 1;
	}
	redirect_count++;
    }

    free(current_url);

    if (redirect_count >= MAX_REDIRECTS) {
	fprintf(stderr, "Too many redirects\n");
	if (reply.reply_buffer) free(reply.reply_buffer);
	return 4;
    }

    if (response == NULL) {
	fprintf(stderr, "Could not parse http reply\n");
	if (reply.reply_buffer) free(reply.reply_buffer);
	return 4;
    }

    // Write response to a file
    write_data(file_name, response, reply.reply_buffer + reply.reply_buffer_length - response);

    // Free allocated memory
    free(reply.reply_buffer);

    // Just tell the user where is the file
    fprintf(stderr, "the file is saved in %s.", file_name);
    return 0;
}

int download_page(url_info *info, http_reply *reply) {

    /*
     * To be completed:
     *   You will first need to resolve the hostname into an IP address.
     *
     *   Option 1: Simplistic
     *     Use gethostbyname function.
     *
     *   Option 2: Challenge
     *     Use getaddrinfo and implement a function that works for both IPv4 and IPv6.
     *
     */

    // Resolve hostname to IP address
    struct hostent *server = gethostbyname(info->host);
    if (server == NULL) {
        fprintf(stderr, "Error: no such host %s\n", info->host);
        return -1;
    }

    // Create socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "Error opening socket\n");
        return -1;
    }

    // Setup server address structure
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    server_addr.sin_port = htons(info->port);

    // Connect to server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "Error connecting to server\n");
        close(sockfd);
        return -1;
    }



    /*
     * To be completed:
     *   Next, you will need to send the HTTP request.
     *   Use the http_get_request function given to you below.
     *   It uses malloc to allocate memory, and snprintf to format the request as a string.
     *
     *   Use 'write' function to send the request into the socket.
     *
     *   Note: You do not need to send the end-of-string \0 character.
     *   Note2: It is good practice to test if the function returned an error or not.
     *   Note3: Call the shutdown function with SHUT_WR flag after sending the request
     *          to inform the server you have nothing left to send.
     *   Note4: Free the request buffer returned by http_get_request by calling the 'free' function.
     *
     */

    // Generate and send HTTP request
    char *request = http_get_request(info);
    int request_len = strlen(request);

    if (write(sockfd, request, request_len) < 0) {
        fprintf(stderr, "Error writing to socket\n");
        free(request);
        close(sockfd);
        return -1;
    }

    // Inform server we're done sending
    shutdown(sockfd, SHUT_WR);

    // Free request buffer
    free(request);



    /*
     * To be completed:
     *   Now you will need to read the response from the server.
     *   The response must be stored in a buffer allocated with malloc, and its address must be save in reply->reply_buffer.
     *   The length of the reply (not the length of the buffer), must be saved in reply->reply_buffer_length.
     *
     *   Important: calling recv only once might only give you a fragment of the response.
     *              in order to support large file transfers, you have to keep calling 'recv' until it returns 0.
     *
     *   Option 1: Simplistic
     *     Only call recv once and give up on receiving large files.
     *     BUT: Your program must still be able to store the beginning of the file and
     *          display an error message stating the response was truncated, if it was.
     *
     *   Option 2: Challenge
     *     Do it the proper way by calling recv multiple times.
     *     Whenever the allocated reply->reply_buffer is not large enough, use realloc to increase its size:
     *        reply->reply_buffer = realloc(reply->reply_buffer, new_size);
     *
     *
     */

    // Receive response - using Option 2 (proper way with multiple recv calls)
    int buffer_size = 4096;
    reply->reply_buffer = (char *)malloc(buffer_size);
    if (reply->reply_buffer == NULL) {
        fprintf(stderr, "Error allocating memory for reply\n");
        close(sockfd);
        return -1;
    }

    reply->reply_buffer_length = 0;
    int bytes_received;

    while ((bytes_received = recv(sockfd, reply->reply_buffer + reply->reply_buffer_length,
                                   buffer_size - reply->reply_buffer_length, 0)) > 0) {
        reply->reply_buffer_length += bytes_received;

        // If buffer is getting full (more than 75% full), expand it
        if (reply->reply_buffer_length * 4 >= buffer_size * 3) {
            buffer_size *= 2;
            char *new_buffer = realloc(reply->reply_buffer, buffer_size);
            if (new_buffer == NULL) {
                fprintf(stderr, "Error reallocating memory for reply\n");
                free(reply->reply_buffer);
                close(sockfd);
                return -1;
            }
            reply->reply_buffer = new_buffer;
        }
    }

    if (bytes_received < 0) {
        fprintf(stderr, "Error reading from socket\n");
        free(reply->reply_buffer);
        close(sockfd);
        return -1;
    }

    close(sockfd);
    return 0;
}

void write_data(const char *path, const char * data, int len) {
    /*
     * To be completed:
     *   Use fopen, fwrite and fclose functions.
     */
    FILE *file = fopen(path, "w");
    if (file == NULL) {
        fprintf(stderr, "Error opening file %s for writing\n", path);
        return;
    }

    if (fwrite(data, 1, len, file) != (size_t)len) {
        fprintf(stderr, "Error writing to file %s\n", path);
    }

    fclose(file);
}

char* http_get_request(url_info *info) {
    char * request_buffer = (char *) malloc(100 + strlen(info->path) + strlen(info->host));
    snprintf(request_buffer, 1024, "GET /%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
	    info->path, info->host);
    return request_buffer;
}

char *next_line(char *buff, int len) {
    if (len == 0) {
	return NULL;
    }

    char *last = buff + len - 1;
    while (buff != last) {
	if (*buff == '\r' && *(buff+1) == '\n') {
	    return buff;
	}
	buff++;
    }
    return NULL;
}

char *check_redirect(struct http_reply *reply) {
    // Let's first isolate the first line of the reply
    char *status_line = next_line(reply->reply_buffer, reply->reply_buffer_length);
    if (status_line == NULL) {
	return NULL;
    }

    // Create a temporary copy to parse without modifying the buffer
    char temp_line[1024];
    int line_len = status_line - reply->reply_buffer;
    if (line_len >= 1024) line_len = 1023;
    memcpy(temp_line, reply->reply_buffer, line_len);
    temp_line[line_len] = '\0';

    // Parse the status
    int status;
    double http_version;
    int rv = sscanf(temp_line, "HTTP/%lf %d", &http_version, &status);
    if (rv != 2) {
	return NULL;
    }

    // Check if it's a redirect
    if (status < 300 || status >= 400) {
	return NULL;
    }

    // Find the Location header
    char *buf = status_line + 2;
    while (1) {
	char *line_end = next_line(buf, reply->reply_buffer + reply->reply_buffer_length - buf);
	if (line_end == NULL || line_end == buf) {
	    break;
	}

	// Check if this line contains "Location:"
	if (strncasecmp(buf, "Location: ", 10) == 0) {
	    // Extract the location (need to copy before \r\n)
	    static char location[2048];
	    int loc_len = line_end - (buf + 10);
	    if (loc_len >= 2048) loc_len = 2047;
	    memcpy(location, buf + 10, loc_len);
	    location[loc_len] = '\0';
	    return location;
	}

	buf = line_end + 2;
    }

    return NULL;
}

char *read_http_reply(struct http_reply *reply) {

    // Let's first isolate the first line of the reply
    char *status_line = next_line(reply->reply_buffer, reply->reply_buffer_length);
    if (status_line == NULL) {
	fprintf(stderr, "Could not find status\n");
	return NULL;
    }
    *status_line = '\0'; // Make the first line is a null-terminated string

    // Now let's read the status (parsing the first line)
    int status;
    double http_version;
    int rv = sscanf(reply->reply_buffer, "HTTP/%lf %d", &http_version, &status);
    if (rv != 2) {
	fprintf(stderr, "Could not parse http response first line (rv=%d, %s)\n", rv, reply->reply_buffer);
	return NULL;
    }

    if (status != 200) {
	fprintf(stderr, "Server returned status %d (should be 200)\n", status);
	return NULL;
    }

    char *buf = status_line + 2;

    /*
     * To be completed:
     *   The previous code only detects and parses the first line of the reply.
     *   But servers typically send additional header lines:
     *     Date: Mon, 05 Aug 2019 12:54:36 GMT<CR><LF>
     *     Content-type: text/css<CR><LF>
     *     Content-Length: 684<CR><LF>
     *     Last-Modified: Mon, 03 Jun 2019 22:46:31 GMT<CR><LF>
     *     <CR><LF>
     *
     *   Keep calling next_line until you read an empty line, and return only what remains (without the empty line).
     *   Hint: Take a look at how end of lines are tested in next_line function declaration, to get inspiration
     *
     *   Difficul challenge:
     *     If you feel like having a real challenge, go on and implement HTTP redirect support for your client.
     *
     */

    // Skip all header lines until we find an empty line
    while (1) {
        char *line_end = next_line(buf, reply->reply_buffer + reply->reply_buffer_length - buf);
        if (line_end == NULL) {
            fprintf(stderr, "Could not find end of headers\n");
            return NULL;
        }

        // Check if this is an empty line (just \r\n)
        if (line_end == buf) {
            // Empty line found - skip the \r\n and return the rest
            return buf + 2;
        }

        // Move to the next line
        buf = line_end + 2;
    }

    return buf;
}
