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
#include <pthread.h>
#include <sys/stat.h>
#include <ctype.h>

#include "url.h"
#include "wgetX.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
	fprintf(stderr, "Usage: %s <URL> [max_depth] [output_dir]\n", argv[0]);
	fprintf(stderr, "  URL:        The starting URL to download\n");
	fprintf(stderr, "  max_depth:  Maximum link depth to follow (default: 2)\n");
	fprintf(stderr, "  output_dir: Output directory (default: downloaded_site)\n");
	return 1;
    }

    char *url = argv[1];
    int max_depth = 2;
    const char *output_dir = "downloaded_site";

    // Get optional parameters
    if (argc > 2) {
	max_depth = atoi(argv[2]);
	if (max_depth < 0) max_depth = 0;
	if (max_depth > 10) {
	    fprintf(stderr, "Warning: max_depth limited to 10 to prevent downloading the entire Internet\n");
	    max_depth = 10;
	}
    }

    if (argc > 3) {
	output_dir = argv[3];
    }

    fprintf(stderr, "Starting multi-threaded crawler\n");
    fprintf(stderr, "Base URL: %s\n", url);
    fprintf(stderr, "Max depth: %d\n", max_depth);
    fprintf(stderr, "Output directory: %s\n", output_dir);

    // Create output directory
    mkdir(output_dir, 0755);

    // Initialize visited URLs set
    url_set visited;
    url_set_init(&visited);

    // Create initial download task
    download_task *initial_task = (download_task *)malloc(sizeof(download_task));
    initial_task->url = strdup(url);
    initial_task->base_url = strdup(url);
    initial_task->output_dir = strdup(output_dir);
    initial_task->depth = 0;
    initial_task->max_depth = max_depth;
    initial_task->visited = &visited;

    // Create and run the initial thread
    pthread_t initial_thread;
    if (pthread_create(&initial_thread, NULL, download_thread, initial_task) != 0) {
	fprintf(stderr, "Failed to create initial thread\n");
	free(initial_task->url);
	free(initial_task->base_url);
	free(initial_task->output_dir);
	free(initial_task);
	url_set_free(&visited);
	return 1;
    }

    // Wait for the initial thread to complete
    pthread_join(initial_thread, NULL);

    // Give some time for detached threads to finish
    fprintf(stderr, "Waiting for all download threads to complete...\n");
    sleep(5);

    fprintf(stderr, "Download complete! Total pages downloaded: %d\n", visited.count);
    fprintf(stderr, "Files saved in: %s\n", output_dir);

    // Cleanup
    url_set_free(&visited);

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

// ==================== URL Set Management (Thread-Safe) ====================

void url_set_init(url_set *set) {
    set->capacity = 100;
    set->count = 0;
    set->urls = (char **)malloc(set->capacity * sizeof(char *));
    pthread_mutex_init(&set->mutex, NULL);
}

int url_set_check_and_add(url_set *set, const char *url) {
    pthread_mutex_lock(&set->mutex);

    // Check if URL already exists
    for (int i = 0; i < set->count; i++) {
        if (strcmp(set->urls[i], url) == 0) {
            pthread_mutex_unlock(&set->mutex);
            return 1; // Already visited
        }
    }

    // Expand capacity if needed
    if (set->count >= set->capacity) {
        set->capacity *= 2;
        set->urls = (char **)realloc(set->urls, set->capacity * sizeof(char *));
    }

    // Add the URL
    set->urls[set->count] = strdup(url);
    set->count++;

    pthread_mutex_unlock(&set->mutex);
    return 0; // Newly added
}

void url_set_free(url_set *set) {
    for (int i = 0; i < set->count; i++) {
        free(set->urls[i]);
    }
    free(set->urls);
    pthread_mutex_destroy(&set->mutex);
}

// ==================== HTML Parsing ====================

int extract_urls_from_html(const char *html, int len, char ***urls) {
    int capacity = 50;
    int count = 0;
    *urls = (char **)malloc(capacity * sizeof(char *));

    const char *ptr = html;
    const char *end = html + len;

    while (ptr < end) {
        // Find <a href="
        const char *href_start = strcasestr(ptr, "<a ");
        if (href_start == NULL) break;

        const char *href_attr = strcasestr(href_start, "href=");
        if (href_attr == NULL || href_attr > end) {
            ptr = href_start + 3;
            continue;
        }

        href_attr += 5; // Skip "href="

        // Skip whitespace
        while (href_attr < end && isspace(*href_attr)) href_attr++;

        char quote_char = 0;
        if (*href_attr == '"' || *href_attr == '\'') {
            quote_char = *href_attr;
            href_attr++;
        }

        // Find the end of the URL
        const char *url_start = href_attr;
        const char *url_end = url_start;

        if (quote_char) {
            while (url_end < end && *url_end != quote_char) url_end++;
        } else {
            while (url_end < end && !isspace(*url_end) && *url_end != '>') url_end++;
        }

        int url_len = url_end - url_start;
        if (url_len > 0 && url_len < 2048) {
            // Skip anchor links and javascript
            if (url_start[0] != '#' && strncmp(url_start, "javascript:", 11) != 0 &&
                strncmp(url_start, "mailto:", 7) != 0) {

                // Expand capacity if needed
                if (count >= capacity) {
                    capacity *= 2;
                    *urls = (char **)realloc(*urls, capacity * sizeof(char *));
                }

                char *url = (char *)malloc(url_len + 1);
                memcpy(url, url_start, url_len);
                url[url_len] = '\0';
                (*urls)[count++] = url;
            }
        }

        ptr = url_end;
    }

    return count;
}

// ==================== URL Normalization ====================

char *normalize_url(const char *base_url, const char *relative_url) {
    // If it's already an absolute URL, return a copy
    if (strstr(relative_url, "://") != NULL) {
        return strdup(relative_url);
    }

    // Parse the base URL
    url_info base_info;
    char *base_copy = strdup(base_url);
    if (parse_url(base_copy, &base_info) != 0) {
        free(base_copy);
        return NULL;
    }

    char *result = (char *)malloc(4096);

    if (relative_url[0] == '/') {
        // Absolute path on same host
        snprintf(result, 4096, "%s://%s%s", base_info.protocol, base_info.host, relative_url);
    } else {
        // Relative path - append to base path
        char *last_slash = strrchr(base_info.path, '/');
        if (last_slash) {
            int base_path_len = last_slash - base_info.path + 1;
            char *base_path = (char *)malloc(base_path_len + 1);
            memcpy(base_path, base_info.path, base_path_len);
            base_path[base_path_len] = '\0';

            snprintf(result, 4096, "%s://%s/%s%s", base_info.protocol, base_info.host,
                     base_path, relative_url);
            free(base_path);
        } else {
            snprintf(result, 4096, "%s://%s/%s", base_info.protocol, base_info.host, relative_url);
        }
    }

    free(base_copy);
    return result;
}

// ==================== Domain Checking ====================

int is_same_domain(const char *url, const char *base_url) {
    url_info url_info1, url_info2;
    char *url_copy1 = strdup(url);
    char *url_copy2 = strdup(base_url);

    int ret1 = parse_url(url_copy1, &url_info1);
    int ret2 = parse_url(url_copy2, &url_info2);

    int result = 0;
    if (ret1 == 0 && ret2 == 0) {
        result = (strcmp(url_info1.host, url_info2.host) == 0);
    }

    free(url_copy1);
    free(url_copy2);
    return result;
}

// ==================== Filename Generation ====================

void create_directories(const char *path) {
    char *path_copy = strdup(path);
    char *ptr = path_copy;

    while (*ptr) {
        if (*ptr == '/') {
            *ptr = '\0';
            mkdir(path_copy, 0755);
            *ptr = '/';
        }
        ptr++;
    }

    free(path_copy);
}

char *url_to_filename(const char *url, const char *base_url, const char *output_dir) {
    url_info info;
    char *url_copy = strdup(url);

    if (parse_url(url_copy, &info) != 0) {
        free(url_copy);
        return NULL;
    }

    char *filename = (char *)malloc(4096);

    // Construct path: output_dir/host/path
    if (strlen(info.path) == 0 || info.path[strlen(info.path)-1] == '/') {
        snprintf(filename, 4096, "%s/%s/%sindex.html", output_dir, info.host, info.path);
    } else {
        snprintf(filename, 4096, "%s/%s/%s", output_dir, info.host, info.path);
    }

    // Create necessary directories
    create_directories(filename);

    free(url_copy);
    return filename;
}

// ==================== URL Rewriting ====================

char *rewrite_html_urls(const char *html, int len, const char *base_url, const char *output_dir) {
    // Allocate buffer for rewritten HTML (estimate 2x original size)
    int result_capacity = len * 2;
    char *result = (char *)malloc(result_capacity);
    int result_len = 0;

    const char *ptr = html;
    const char *end = html + len;

    while (ptr < end) {
        const char *href_start = strcasestr(ptr, "href=");

        if (href_start == NULL || href_start >= end) {
            // Copy the rest
            int remaining = end - ptr;
            if (result_len + remaining >= result_capacity) {
                result_capacity = result_len + remaining + 1000;
                result = (char *)realloc(result, result_capacity);
            }
            memcpy(result + result_len, ptr, remaining);
            result_len += remaining;
            break;
        }

        // Copy everything up to href=
        int copy_len = (href_start + 5) - ptr;
        if (result_len + copy_len >= result_capacity) {
            result_capacity *= 2;
            result = (char *)realloc(result, result_capacity);
        }
        memcpy(result + result_len, ptr, copy_len);
        result_len += copy_len;
        ptr = href_start + 5;

        // Skip whitespace and quote
        while (ptr < end && isspace(*ptr)) ptr++;

        char quote_char = 0;
        if (*ptr == '"' || *ptr == '\'') {
            quote_char = *ptr;
            if (result_len + 1 >= result_capacity) {
                result_capacity *= 2;
                result = (char *)realloc(result, result_capacity);
            }
            result[result_len++] = *ptr;
            ptr++;
        }

        // Extract the URL
        const char *url_start = ptr;
        const char *url_end = url_start;

        if (quote_char) {
            while (url_end < end && *url_end != quote_char) url_end++;
        } else {
            while (url_end < end && !isspace(*url_end) && *url_end != '>') url_end++;
        }

        int url_len = url_end - url_start;
        if (url_len > 0 && url_len < 2048) {
            char *old_url = (char *)malloc(url_len + 1);
            memcpy(old_url, url_start, url_len);
            old_url[url_len] = '\0';

            // Normalize and convert to local path
            char *absolute_url = normalize_url(base_url, old_url);
            if (absolute_url && is_same_domain(absolute_url, base_url)) {
                char *local_path = url_to_filename(absolute_url, base_url, output_dir);
                if (local_path) {
                    // Write relative path from output_dir
                    int path_len = strlen(local_path);
                    if (result_len + path_len >= result_capacity) {
                        result_capacity = result_len + path_len + 1000;
                        result = (char *)realloc(result, result_capacity);
                    }
                    memcpy(result + result_len, local_path, path_len);
                    result_len += path_len;
                    free(local_path);
                } else {
                    // Keep original URL
                    if (result_len + url_len >= result_capacity) {
                        result_capacity *= 2;
                        result = (char *)realloc(result, result_capacity);
                    }
                    memcpy(result + result_len, old_url, url_len);
                    result_len += url_len;
                }
                free(absolute_url);
            } else {
                // Keep original URL
                if (result_len + url_len >= result_capacity) {
                    result_capacity *= 2;
                    result = (char *)realloc(result, result_capacity);
                }
                memcpy(result + result_len, old_url, url_len);
                result_len += url_len;
            }

            free(old_url);
        }

        ptr = url_end;
    }

    result[result_len] = '\0';
    return result;
}

// ==================== Multi-threaded Download ====================

void *download_thread(void *arg) {
    download_task *task = (download_task *)arg;

    fprintf(stderr, "[Depth %d] Downloading: %s\n", task->depth, task->url);

    // Check if we've already visited this URL
    if (url_set_check_and_add(task->visited, task->url)) {
        fprintf(stderr, "[Depth %d] Already visited: %s\n", task->depth, task->url);
        free(task->url);
        free(task->base_url);
        free(task->output_dir);
        free(task);
        return NULL;
    }

    // Check if we're within the same domain
    if (!is_same_domain(task->url, task->base_url)) {
        fprintf(stderr, "[Depth %d] Skipping external URL: %s\n", task->depth, task->url);
        free(task->url);
        free(task->base_url);
        free(task->output_dir);
        free(task);
        return NULL;
    }

    // Parse URL
    url_info info;
    char *url_copy = strdup(task->url);
    if (parse_url(url_copy, &info) != 0) {
        fprintf(stderr, "[Depth %d] Could not parse URL: %s\n", task->depth, task->url);
        free(url_copy);
        free(task->url);
        free(task->base_url);
        free(task->output_dir);
        free(task);
        return NULL;
    }

    // Download the page
    struct http_reply reply;
    if (download_page(&info, &reply) != 0) {
        fprintf(stderr, "[Depth %d] Download failed: %s\n", task->depth, task->url);
        free(url_copy);
        free(task->url);
        free(task->base_url);
        free(task->output_dir);
        free(task);
        return NULL;
    }

    char *response = read_http_reply(&reply);
    if (response == NULL) {
        fprintf(stderr, "[Depth %d] Could not parse reply: %s\n", task->depth, task->url);
        free(reply.reply_buffer);
        free(url_copy);
        free(task->url);
        free(task->base_url);
        free(task->output_dir);
        free(task);
        return NULL;
    }

    int response_len = reply.reply_buffer + reply.reply_buffer_length - response;

    // Check if this is HTML content (simple check)
    int is_html = (strcasestr(response, "<html") != NULL ||
                   strcasestr(response, "<!doctype") != NULL ||
                   strcasestr(response, "<head") != NULL);

    // Generate filename
    char *filename = url_to_filename(task->url, task->base_url, task->output_dir);
    if (filename == NULL) {
        fprintf(stderr, "[Depth %d] Could not generate filename: %s\n", task->depth, task->url);
        free(reply.reply_buffer);
        free(url_copy);
        free(task->url);
        free(task->base_url);
        free(task->output_dir);
        free(task);
        return NULL;
    }

    // If HTML, rewrite URLs and extract links
    if (is_html) {
        char *rewritten_html = rewrite_html_urls(response, response_len, task->url, task->output_dir);
        write_data(filename, rewritten_html, strlen(rewritten_html));
        free(rewritten_html);

        // Extract URLs and spawn new threads (if not at max depth)
        if (task->depth < task->max_depth) {
            char **urls;
            int url_count = extract_urls_from_html(response, response_len, &urls);

            fprintf(stderr, "[Depth %d] Found %d URLs in %s\n", task->depth, url_count, task->url);

            pthread_t *threads = (pthread_t *)malloc(url_count * sizeof(pthread_t));
            int thread_count = 0;

            for (int i = 0; i < url_count; i++) {
                char *absolute_url = normalize_url(task->url, urls[i]);
                if (absolute_url) {
                    download_task *new_task = (download_task *)malloc(sizeof(download_task));
                    new_task->url = absolute_url;
                    new_task->base_url = strdup(task->base_url);
                    new_task->output_dir = strdup(task->output_dir);
                    new_task->depth = task->depth + 1;
                    new_task->max_depth = task->max_depth;
                    new_task->visited = task->visited;

                    if (pthread_create(&threads[thread_count], NULL, download_thread, new_task) == 0) {
                        pthread_detach(threads[thread_count]);
                        thread_count++;
                    } else {
                        free(new_task->url);
                        free(new_task->base_url);
                        free(new_task->output_dir);
                        free(new_task);
                    }
                }
                free(urls[i]);
            }

            free(threads);
            free(urls);
        }
    } else {
        // Not HTML, just save it
        write_data(filename, response, response_len);
    }

    fprintf(stderr, "[Depth %d] Saved to: %s\n", task->depth, filename);

    free(filename);
    free(reply.reply_buffer);
    free(url_copy);
    free(task->url);
    free(task->base_url);
    free(task->output_dir);
    free(task);

    return NULL;
}
